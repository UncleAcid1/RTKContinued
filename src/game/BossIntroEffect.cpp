// BossIntroEffect: the transition into a boss fight (Map::Update's queued load). A blocking loop of
// frames, 3.85 s in all, run in two halves around the map's load: the view zooms to 1.5 times on the
// hero (0.75 s), the picture is blurred and brightens (0.2 s, 0.4 s), the blur spreads (1 s) and
// collapses back (0.5 s); then it turns white until the map is loaded (Run returns, the map loads,
// Continue and Run go on) and the white fades out (1 s) over the new map.
// Port of BossIntroEffect (libkingdom.so 5.11), 0x1b7674..0x1be000; statics at 0x6038f8..0x603928.
// FastBossIntroEffect (slow devices) does nothing in 5.11.
//
// PORT: the screen capture is not upside down here (Render::CreateScreenTexture flips its rows), so the
// sprite is made without the flip the original asks for.
#include <cmath>
#include <cstdio>

#include "engine/Render.h"
#include "engine/Timer.h"
#include "game/Entity.h"
#include "game/EntityManager.h"
#include "game/Game.h"
#include "game/Map.h"
#include "gui/GUI.h"
#include "hud/HUD.h"
#include "windows/Windows.h"

namespace BossIntroEffect {
namespace {

double time = 0.0;              // 0x603900
double lastTime = 0.0;          // 0x603908
float zoom = 0.f;               // 0x603910 the view when it began
float offsetX = 0.f;            // 0x603914
float offsetY = 0.f;            // 0x603918
Entity* player = nullptr;       // 0x60391c
Render::Texture* screen = nullptr;     // 0x603920 the blurred capture, then a white pixel
Render::Sprite* sprite = nullptr;      // 0x603924 over the whole screen, shader 6: color = (scale, spread, add)
int frames = 0;                 // 0x6038f8
bool mapLoaded = false;         // 0x603928

float Arc(float k) { return std::sqrt(1.f - k * k); }

}  // namespace

// @0x1b78b8
void Prepare() {
    time = 0.0;
    lastTime = Timer::GetTime();
    zoom = Render::zoom;
    offsetX = Render::offsetX;
    offsetY = Render::offsetY;
    player = EntityManager::GetPlayer();
    screen = nullptr;
    frames = 0;
    sprite = nullptr;
    mapLoaded = false;
}

// @0x1b7674: the map is loaded; the time it took does not count.
void Continue() { lastTime = Timer::GetTime(); }

// @0x1bdbd0
void Run() {
    if (GlobalMapWindow::IsVisible() && time < 0.75) time = 0.75;
    while (time < 3.85 && !renderSuspendEvent) {
        double dt = (Timer::GetTime() - lastTime) * (double)HUDWindow::GetDeltaTimeMultiplier();
        time += dt;
        lastTime = Timer::GetTime();
        float t = (float)time;
        if (t > 0.f && t < 0.75f) {   // the view glides to the hero, zoomed in
            const float s = Arc(t / 0.75f - 1.f);
            Render::zoom = zoom + (zoom * 1.5f - zoom) * s;
            Render::offsetX = offsetX + (-player->worldX - offsetX) * s;
            Render::offsetY = offsetY + ((player->worldY - player->GetIdleHeight()) - offsetY) * s;
        }
        t -= 0.75f;
        if (t > 0.f && t < 0.2f) {   // the picture is captured, blurred and fades in over the world
            if (!screen) {
                screen = Render::CreateScreenTexture();
                Render::BlurTexture(screen, 0);
                sprite = Render::CreateSprite(screen, 0xf, false, false);
                sprite->screenSpace = true;
                sprite->color[0] = 1.f;
                sprite->color[2] = 0.f;
                sprite->color[1] = 0.f;
                Render::SetShaderType(sprite, 1);
                Render::SetPosition(sprite, 0.f, (float)GUI::ScreenHeight(), 0.5f);
                lastTime = Timer::GetTime();
            }
            Render::SetAlpha(sprite, Arc(t / 0.2f - 1.f));
        }
        t -= 0.2f;
        if (t > 0.f && t < 0.4f) {   // only the sprite's layer is drawn; it brightens
            Render::minLayer = 0xf;
            if (sprite) {
                Render::SetShaderType(sprite, 6);
                sprite->color[0] = 1.f + Arc(t / 0.4f - 1.f) * 0.5f;
                Render::MarkDirty(sprite);
            }
        }
        t -= 0.4f;
        if (t > 0.f && t < 1.f) {   // the blur spreads
            if (sprite) {
                sprite->color[1] = 0.f + t * (40.f / (float)GUI::ScreenWidth());
                Render::MarkDirty(sprite);
            }
            ++frames;
        }
        t -= 1.f;
        if (t > 0.f && t < 0.5f && sprite) {   // and comes back together
            const float spread = 40.f / (float)GUI::ScreenWidth();
            sprite->color[0] = 1.5f + Arc(t / 0.4f - 1.f) * -1.5f;
            sprite->color[1] = spread + (t + t) * (0.f - spread);
            sprite->color[2] = Arc((t + t) - 1.f) + 0.f;
            Render::MarkDirty(sprite);
        }
        t -= 0.5f;
        if (t > 0.f) {   // white, until the map is there, then it fades out
            if (t < 1.f && mapLoaded) {
                EntityManager::UpdateGraphics();
                if (sprite) Render::SetAlpha(sprite, Arc(t));
            } else if (!mapLoaded) {
                mapLoaded = true;
                Render::Texture* white = Render::CreateSolidFillTexture(1.f, 1.f, 1.f, 1.f);
                if (!sprite) return;
                Render::SetShaderType(sprite, 1);
                Render::SetTexture(sprite, white);
                return;
            }
        }
        Render::SortRenderLayer(0xf, 1);
        Render::Update();
    }
    std::printf("BossIntroEffect::Run() %d frames in %f seconds\n", frames, 1.0);
    sprite = Render::RemoveSprite(sprite);
    if (screen) Render::RemoveTexture(screen);
    screen = nullptr;
}

}  // namespace BossIntroEffect

// @0x1b6748 .. 0x1b6750: nothing in 5.11
namespace FastBossIntroEffect {
void Prepare() {}
void Continue() {}
void Run() {}
}  // namespace FastBossIntroEffect
