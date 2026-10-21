// The scripted camera of a map: a list of points the view visits one after another (opening the
// fog there, pointing an arrow, completing a task), the camera shake and the cinematic bars.
// Port of Map::AddCameraPoint / AddCameraShake / AddCameraCinematic / ClearCameraPoints and their
// part of Map::Update (@0x1c4dd8).
#include "engine/SoundsManager.h"
#include "game/Map.h"

#include <cmath>
#include <cstdlib>
#include <vector>

#include "engine/Render.h"
#include "game/AIState.h"
#include "game/BuildingHovers.h"
#include "game/Entity.h"
#include "game/EntityManager.h"
#include "game/Fog.h"
#include "game/Combat.h"
#include "game/GameState.h"
#include "game/MetaData.h"
#include "game/Setting.h"
#include "game/Tasks.h"
#include "gui/GUI.h"

namespace Map {

namespace {

struct CameraPoint {        // 0x30 bytes
    int x = 0, y = 0;       // +0x00 +0x04 tile
    float wait = 0.f;       // +0x08 seconds the view stays
    float travel = 0.f;     // +0x0c seconds on the way there
    bool keepHero = false;  // +0x10 the hero is not stopped
    int arrowX = 0, arrowY = 0;   // +0x14 +0x18 world point of the help arrow (0, 0: none)
    int f1c = 0;            // +0x1c the glide's mode (Render::CenterOn: 1 linear)
    float zoom = 1.f;       // +0x20 the zoom factor on the way
    bool returnToPlayer = false;  // +0x24 after the last point the view goes back to the hero
    float fogRadius = 0.f;  // +0x28 added to the opened fog's radius
    uint32_t task = 0;      // +0x2c completed when the view leaves
};

std::vector<CameraPoint> g_points;      // 0x613854
int g_state = 0;                        // 0x61373c 0 start, 1 on the way, 2 staying
float g_timer = 0.f;                    // 0x613888
float g_shakeLeft = 0.f;                // 0x6136c4
float g_shakeTime = 0.f;                // 0x6136c8
float g_shakeStrength = 0.f;            // 0x6136cc
float g_shakeX = 0.f, g_shakeY = 0.f;   // 0x6136d0 0x6136d4 the view before the shake
bool g_cinematic = false;               // 0x61372c the bars are in use
bool g_cinematicIn = false;             // 0x61372d sliding in (else out)
float g_cinematicTime = 0.f;            // 0x613730
Render::Sprite* g_barTop = nullptr;     // 0x613734
Render::Sprite* g_barBottom = nullptr;  // 0x613738

float Clamp01(float t) { return t < 0.f ? 0.f : t >= 1.f ? 1.f : t; }

}  // namespace

// @0x1c08cc Map::AddCameraPoint
void AddCameraPoint(int x, int y, int wait, int travel, int keepHero, int arrowX, int arrowY, int f1c, float zoom,
                    bool returnToPlayer, float fogRadius, uint32_t task) {
    CameraPoint p;
    p.x = x;
    p.y = y;
    p.wait = (float)wait;
    p.travel = (float)travel;
    p.keepHero = keepHero != 0;
    p.arrowX = arrowX;
    p.arrowY = arrowY;
    p.f1c = f1c;
    p.zoom = zoom;
    p.returnToPlayer = returnToPlayer;
    p.fogRadius = fogRadius;
    p.task = task;
    g_points.push_back(p);
}

void AddCameraPath(const MetaData* list, unsigned first) {
    AddCameraCinematic();
    for (unsigned i = first; i < list->GetChildrenCount(); ++i) {
        const MetaData* e = list->GetChild(i);
        unsigned n = e->GetChildrenCount();
        AddCameraPoint(e->GetChild(0)->GetInt(), e->GetChild(1)->GetInt(), e->GetChild(2)->GetInt(),
                       n < 4 ? 1 : e->GetChild(3)->GetInt(), n < 5 ? 0 : e->GetChild(4)->GetInt(), 0, 0,
                       n < 6 ? 0 : e->GetChild(5)->GetInt(), n < 7 ? 1.f : (float)e->GetChild(6)->GetInt() * 0.01f, true,
                       n < 8 ? 0.f : (float)e->GetChild(7)->GetInt(), n < 9 ? 0u : (uint32_t)e->GetChild(8)->GetInt());
    }
}

// @0x1bf6d4 Map::InterruptCamera
void InterruptCamera() {
    if (g_points.empty() || !g_points.front().keepHero) return;
    g_cinematicIn = false;
    g_cinematicTime = 0.f;
    ClearCameraPoints();
    Render::CenterOn(-Render::offsetX, Render::offsetY);
}

// @0x1b82c4 Map::IsCameraMoving
bool IsCameraMoving() {
    if (!GameState::IsPaused()) return g_state != 0;
    GUI::SetInteractionLock(false);
    return false;
}

// @0x1bf69c Map::ClearCameraPoints
void ClearCameraPoints() {
    g_timer = 0.f;
    g_state = 0;
    g_points.clear();
    GUI::SetInteractionLock(false);
}

// @0x1b7438 Map::AddCameraShake: the view jitters by up to half of strength for time seconds.
void AddCameraShake(float time, float strength) {
    g_shakeX = Render::offsetX;
    g_shakeTime = time;
    g_shakeStrength = strength;
    g_shakeLeft = time;
    g_shakeY = Render::offsetY;
}

// @0x1b81a8 Map::AddCameraCinematic: the black bars slide in (if "cinematic_camera" is set).
void AddCameraCinematic() {
    g_cinematic = Setting("cinematic_camera").GetInt() != 0;
    g_cinematicIn = true;
    g_cinematicTime = 0.f;
    if (g_barTop) return;
    Render::Texture* black = Render::CreateSolidFillTexture(0.f, 0.f, 0.f, 0.85f);
    for (Render::Sprite** bar : {&g_barTop, &g_barBottom}) {
        Render::Sprite* s = *bar = Render::CreateSprite(black, 0xf, false, false);
        s->screenSpace = true;
        s->h = (float)GUI::ScreenHeight() * 0.16f;
        s->w = (float)GUI::ScreenWidth();
    }
}

// Map::Update's camera part.
void UpdateCamera(float dt) {
    if (!g_points.empty()) {
        CameraPoint& p = g_points.front();
        if (g_state == 1) {   // on the way
            g_timer -= dt;
            if (g_timer < 0.f) {
                int wx = p.x, wy = p.y;
                g_timer = p.wait;
                TileCoordinatesToWorld(wx, wy);
                if (p.arrowX == 0 && p.arrowY == 0) {
                    Fog::Open((float)wx, (float)wy, p.fogRadius + 240.f, 0.5f);
                } else {
                    Fog::Open((float)wx, (float)wy, p.fogRadius + 200.f, 0.3f);
                    BuildingHovers::ArrowAt((float)p.arrowX, (float)p.arrowY, false, false, false, false, false, false,
                                            false);
                }
                if (g_points.size() == 1) SoundsManager::PlaySound("camera_reveal", 1, false);   // the last point
                g_state = 2;
                Render::zoom = Render::GetDefaultZoom();
            } else {
                float c = std::cos(((p.travel - g_timer) / p.travel) * 3.1415925f);
                Render::zoom = Render::GetDefaultZoom() * (1.f + (1.f - std::fabs(c)) * (p.zoom - 1.f));
            }
        } else if (g_state == 2) {   // staying
            g_timer -= dt;
            if (g_timer < 0.f) {
                if (p.arrowX != 0 || p.arrowY != 0) BuildingHovers::HideArrow();
                if (p.task != 0) Tasks::CompleteTask(p.task, true, true, false);
                // (CompleteTask may add points: the first is read again)
                const bool back = g_points.front().returnToPlayer;
                g_points.erase(g_points.begin());
                g_state = 0;
                if (g_points.empty()) {
                    g_cinematicIn = false;
                    g_cinematicTime = 0.f;
                    if (back) {
                        if (GameState::tutorial == 0x17 && !GameState::TaskCompleted(0x76e))
                            GameState::CompleteTask(0x76e, false);
                        Entity* player = EntityManager::GetPlayer();
                        if (player && player->GetSprite()) {
                            Render::Sprite* s = player->GetSprite();
                            Render::CenterOn(s->x + s->w * 0.5f, s->y - s->h, true, 1.f);
                        }
                    }
                }
                if (!GameState::IsPvPTutorial()) GUI::SetInteractionLock(false);
            }
        } else if (g_state == 0) {   // setting off
            int wx = p.x, wy = p.y;
            g_timer = p.travel;
            TileCoordinatesToWorld(wx, wy);
            // (the glide leaves the zoom to the travel's cosine zoom when p.zoom is not 1)
            Render::CenterOn((float)wx, (float)wy, true, p.travel, p.f1c, p.zoom != 1.f);
            if (!GameState::IsPvPTutorial()) GUI::SetInteractionLock(true);
            if (!p.keepHero) {
                Entity* player = EntityManager::GetPlayer();
                if (player && !player->IsOnPatrol()) {
                    player->GetAI()->StopMovement();
                    if (GameState::IsCombatActive()) GameState::GetActiveCombat()->EndCombat();
                }
            }
            g_state = 1;
        }
    }
    if (g_shakeLeft > 0.f) {
        float c = std::cos((g_shakeLeft / g_shakeTime) * 3.1415925f);
        Render::offsetX = g_shakeX + ((float)(lrand48() % 1000) / 1000.f - 0.5f) * (1.f - std::fabs(c)) * g_shakeStrength;
        long r = lrand48();
        g_shakeLeft -= dt;
        Render::offsetY = g_shakeY + ((float)(r % 1000) / 1000.f - 0.5f) * (1.f - std::fabs(c)) * g_shakeStrength;
        if (!(g_shakeLeft > 0.f)) {
            Render::offsetY = g_shakeY;
            Render::offsetX = g_shakeX;
        }
    }
    if (g_cinematic) {
        const float H = (float)GUI::ScreenHeight();
        const float base = H + g_barBottom->h;
        if (!g_cinematicIn) {
            float t = Clamp01(g_cinematicTime);
            Render::SetPosition(g_barTop, 0.f, 0.f + (1.f - t) * g_barTop->h, 0.01f);
            Render::SetPosition(g_barBottom, 0.f, base + (1.f - t) * (H - base), 0.01f);
            if (g_cinematicTime >= 1.f) {
                Render::SetVisibility(g_barTop, false);
                Render::SetVisibility(g_barBottom, false);
                g_cinematic = false;
            }
        } else {
            Render::SetVisibility(g_barTop, true);
            Render::SetVisibility(g_barBottom, true);
            float t = Clamp01(g_cinematicTime);
            Render::SetPosition(g_barTop, 0.f, 0.f + t * g_barTop->h, 0.01f);
            Render::SetPosition(g_barBottom, 0.f, base + t * (H - base), 0.01f);
        }
        g_cinematicTime += dt + dt;
    }
}

}  // namespace Map
