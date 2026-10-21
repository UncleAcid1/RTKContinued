// BossOutroWindow: "Boss defeated!" (Endscreen_boss_end.xml) when a boss fight is won, and the end of
// an arena event: the banner slides down over a black band, the squad cheers, and the loot strip
// (FoundItemsWindow, what was picked up on the map) rises. Closing the strip slides the banner away
// and the map the fight started from is loaded again (OnTweenOut).
// Port of BossOutroWindow (libkingdom.so 5.11), 0x253a54..0x255080. Statics at 0x617f9c.
//
#include <algorithm>
#include <cstdio>
#include <string>

#include "engine/SoundsManager.h"
#include "engine/Render.h"
#include "engine/Resources.h"
#include "engine/Text.h"
#include "game/AIState.h"
#include "game/Combat.h"
#include "game/Entity.h"
#include "game/EntityManager.h"
#include "game/GameState.h"
#include "game/Map.h"
#include "game/Professions.h"
#include "game/Rand48.h"
#include "game/Squad.h"
#include "game/StringTable.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/HUD.h"
#include "hud/Shared.h"
#include "windows/Windows.h"

namespace BossOutroWindow {
namespace {

using GUI::Textfield;
using GUI::Window;

WindowManager::FunctionalWindow* g_queue = nullptr;   // BossOutroWindow::wnd 0x618000
Textfield* g_text2 = nullptr;         // 0x617f9c text_02: the boss's name (the arena: the victory text)
uint32_t g_boss = 0;                  // 0x617fa0 the boss's entity id (the loot strip's share)
Shared::EndscreenProgressBar g_bar;   // 0x617fa4 the arena's reputation bar
Window* g_root = nullptr;             // 0x617fd4
Render::Sprite* g_black = nullptr;    // 0x617fd8 the black band (alpha 0.6)
Render::Sprite* g_goldline = nullptr; // 0x617fdc Endscreen_goldline_scaler.png under the band
GUI::MovementEffect* g_effect = nullptr;   // 0x617fe0
Textfield* g_text1 = nullptr;         // 0x617fe4 text_01: the header
Render::Texture* g_blackTex = nullptr;   // 0x617fe8
float g_time = 0.f;                   // 0x617fec (the arena's bar: only counts while one window is up)
// The arena's reputation bar: profession 1 (the Bloodletters) from its points before to those plus
// the map's reputation items (0x1ba).
Professions::Profession* g_profession = nullptr;   // 0x617ff0
uint32_t g_startPoints = 0;           // 0x617ff4
uint32_t g_endPoints = 0;             // 0x617ff8
int g_bandY = 0;                      // 0x617ffc small screens: the slid-down window's y
bool g_tablet = false;                // 0x60f0ec
float g_rootScale = 1.f;              // 0x60f0e8

// @0x253b6c: the banner is gone. After a boss fight the map it started from loads again (and the
// next arena wave starts, or the arena ends); the battle bar returns on a campaign map.
void OnTweenOut() {
    g_root->SetVisibility(false);
    Render::SetVisibility(g_black, false);
    Render::SetVisibility(g_goldline, false);
    if (BaseCombat* combat = GameState::GetActiveCombat()) {
        if (combat->GetType() == 5 || combat->GetType() == 6) {
            if (GameState::GetActiveCombat()->GetCurrentWave() != GameState::GetActiveCombat()->GetWaveCount()) {
                GameState::GetActiveCombat()->Continue(7);
                if (GameState::GetCurrentMapID() != 0 && GameState::GetCurrentMapID() != 0xb) BottomBattleWindow::Show();
                return;
            }
            Map::EnqueueLoadMap((int)GameState::mReturnMapID, false, false, 0.f);
            GameState::GetActiveCombat()->PostCombat();
        } else {
            Map::EnqueueLoadMap((int)GameState::mReturnMapID, false, false, 0.f);
        }
    }
    if (GameState::GetCurrentMapID() != 0 && GameState::GetCurrentMapID() != 0xb) BottomBattleWindow::Show();
}

// @0x253ca8: the band settles.
void OnTweenIn() {
    const float y = g_effect->hideAtEnd ? 0.f : g_black->h;
    Render::SetPosition(g_black, 0.f, y, 0.02f);
    Render::SetPosition(g_goldline, 0.f, g_black->y + 5.f, 0.0199f);
}

// @0x253d2c: the band follows the banner.
void OnTweenMove() {
    if (!g_effect->hideAtEnd) Render::SetPosition(g_black, 0.f, g_black->h * g_effect->t, 0.02f);
    else Render::SetPosition(g_black, 0.f, (1.f - g_effect->t) * g_black->h, 0.02f);
    Render::SetPosition(g_goldline, 0.f, g_black->y + 5.f, 0.0199f);
    Render::SortRenderLayer(0xe, 1);
}

// @0x253dec: after a boss only the header; in an arena "waves slain" and the victory text, with the
// reputation bar.
void UpdateContents() {
    const int waves = BossIntroWindow::GetTotalWaves();
    if (!GameState::IsArenaCombatActive()) {
        g_text1->SetText(StringTable::GetString("BOSS_OUTRO_HEADER"));
    } else {
        g_text1->SetText(
            SWPrintf(0x100, StringTable::GetString("ARENA_WAVE_SLAIN"), {ToWideString(waves), ToWideString(waves)}).c_str());
        g_text2->SetText(StringTable::GetString("ARENA_WAVE_VICTORY"));
    }
    g_bar.root->SetVisibility(GameState::IsArenaCombatActive());
}

}  // namespace

// @0x2540f0: the band's texture comes back after a context loss. In an arena the reputation bar
// fills in 2 s (after half a second) towards the next level's points, with "gained x / y" (or "next
// rank" once they are reached).
void Update(float dt) {
    if (!g_queue->shown || !g_black) return;
    if (!g_blackTex || g_blackTex->glId == 0) {
        g_blackTex = Render::CreateSolidFillTexture(0.f, 0.f, 0.f, 1.f);
        Render::SetTexture(g_black, g_blackTex);
    }
    if (WindowManager::GetShownWindowCount() < 2) g_time += dt;
    if (!g_profession) return;
    float t = g_time * 0.5f;
    t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
    const uint32_t level = Professions::GetPlayerProfessionLevel(g_profession->id);
    Professions::ProfessionLevel* next = Professions::GetProfessionLevel(g_profession->id, level + 1);
    if (!next) next = Professions::GetProfessionLevel(g_profession->id, level);
    if (!next) return;
    const float from = (float)g_startPoints;
    const float value = from + t * ((float)std::min(next->points, g_endPoints) - from);
    Professions::ProfessionLevel* cur = Professions::GetProfessionLevel(g_profession->id, level);
    const int shown = (int)(value + 0.5f);
    const int prev = cur ? (int)cur->points : 0;
    std::u32string text;
    if ((int)next->points <= shown)
        text = SWPrintf(0x100, StringTable::GetString("ARENA_WAVE_GAINED_NEXT"), {g_profession->title});
    else
        text = SWPrintf(0x100, StringTable::GetString("ARENA_WAVE_GAINED"),
                        {g_profession->title60, ToWideString(shown), ToWideString((int)next->points)});
    g_bar.textUnder->SetText(text.c_str());
    g_bar.textOver->SetText(text.c_str());
    if (prev != (int)next->points) {
        g_bar.SetProgress((value - (float)(uint32_t)prev) / (float)(uint32_t)((int)next->points - prev));
        g_root->UpdatePosition();
    }
}

// @0x2547a4: the arena's music stops; the loot strip rises (the map's pick-ups, "BOSS_OUTRO_FOUND"),
// the banner slides down, the squad cheers and the camera glides to the hero.
void Show() {
    if (g_queue->shown) return;
    if (ArenaTurnWindow::IsVisible()) ArenaTurnWindow::Hide();
    if (GameState::IsArenaCombatActive()) {
        g_profession = Professions::GetProfession(1);
        g_startPoints = GameState::GetProfessionPoints(g_profession->id);
        const int found = GameState::GetMapItemCollectionInfo(GameState::GetCurrentMapID(), 0x1ba);
        g_time = -0.5f;
        g_endPoints = (uint32_t)found + g_startPoints;
        SoundsManager::PlaySound("arena_win", 1, false);
        SoundsManager::StopMusic();
    }
    FoundItemsWindow::SetParameters((uint32_t)GameState::GetCurrentMapID(), g_boss,
                                    StringTable::GetString(GameState::IsArenaCombatActive() ? "ARENA_WAVE_FOUND" : "BOSS_OUTRO_FOUND"));
    FoundItemsWindow::Show();
    g_queue->shown = true;
    g_queue->MoveWindowOnTop(true);
    WindowManager::WindowShow(false);
    const int x = g_effect->GetCenteredX();
    g_effect->Animate(x, -g_root->h, g_effect->GetCenteredX(), g_bandY, true);
    g_root->SetPosition(g_effect->GetCenteredX(), -g_root->h);
    g_root->SetVisibility(true);
    Render::SetVisibility(g_black, true);
    g_black->w = (float)GUI::ScreenWidth();
    g_black->h = (float)GUI::ScreenHeight() * 0.37f;
    Render::SetPosition(g_black, 0.f, 0.f, 0.02f);
    Render::SetVisibility(g_goldline, true);
    g_goldline->w = (float)GUI::ScreenWidth();
    Render::SetPosition(g_goldline, 0.f, 0.f, 0.0199f);
    if (Entity* player = EntityManager::GetPlayer())
        if (BaseSquad* squad = player->GetSquad())
            for (unsigned i = 0; i < (unsigned)squad->GetSoldierCount(); ++i) {
                Entity* e = squad->GetSoldier(i);
                if (!e) continue;
                e->SetDirection((Rand48::lrand48() & 1) == 0 ? 1 : 7);
                e->SetAnimationP(e->player ? "bat_vict" : "victory", true, false, false);
                if (e->player) Render::CenterOn(e->worldX, e->worldY - e->GetIdleHeight(), true, 0.4f, 0, false);
            }
    UpdateContents();
    g_root->PreloadTextures();   // GUI::PreloadWindow
    GUI::SetInteractionLock(false);
}

// @0x254b5c: the banner slides back up and the window leaves the queue at once.
void Hide() {
    if (g_root->visibleSelf) {
        const int x = g_effect->GetCenteredX();
        g_effect->Animate(x, g_bandY, g_effect->GetCenteredX(), -g_root->h, false);
    }
    WindowManager::WindowHide(false);
    g_queue->shown = false;
    g_queue->MoveWindowDown(true);
}

// @0x254c0c
bool OnBack() {
    std::puts("BossOutroWindow::OnBack");
    if (!g_queue->shown) return false;
    Hide();
    return true;
}

// @0x254c44
void SetZ(float z) {
    g_root->SetZ(z);
    g_root->SetPosition(g_effect->GetCenteredX(), g_bandY);
    if (g_effect->active) return;
    if (!g_queue->shown) return;
    g_black->w = (float)GUI::ScreenWidth();
    g_black->h = (float)GUI::ScreenHeight() * 0.37f;
    Render::SetPosition(g_black, 0.f, g_black->h, 0.02f);
    g_goldline->w = (float)GUI::ScreenWidth();
    Render::SetPosition(g_goldline, 0.f, g_black->y + 5.f, 0.0199f);
    g_bar.Rescale(g_rootScale);
}

// @0x254d74: the taps go to the loot strip.
bool Click(int x, int y, bool pressed) {
    FoundItemsWindow::Click(x, y, pressed);
    return g_queue->shown;
}

// @0x254d90
void Deinit() {
    GUI::RemoveMovementEffect(g_effect);
    g_effect = nullptr;
    delete g_root;
    g_black = Render::RemoveSprite(g_black);
    g_goldline = Render::RemoveSprite(g_goldline);
}

WindowManager::FunctionalWindow* Queue() {
    if (!g_queue) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.deinit = Deinit;
        f.click = Click;
        f.setZ = SetZ;
        f.hide = Hide;
        g_queue = new WindowManager::FunctionalWindow("BossOutroWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_queue->shown; }   // @0x253a54

// @0x253a68: the boss's name under the header and its entity id (for the loot strip).
void SetBossName(const char32_t* name, uint32_t id) {
    g_text2->SetText(name);
    g_boss = id;
}

// @0x254df4
void Init() {
    g_blackTex = nullptr;
    g_black = nullptr;
    g_goldline = nullptr;
    g_tablet = GUI::IsTabletVersion();
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Endscreen_boss_end.xml", "Endscreen_boss_end.png",
                             GUI::GetHoverScaleFactor(1.f, 1.f), 0, 0, 0, 0, false, 1.f);
    g_queue->RegisterTopWindow(g_root);
    g_rootScale = g_root->scale;
    g_root->SetVisibility(false);
    g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::GetVerticalCenter(g_root->h));
    int h = GUI::ScreenHeight();
    if (h < 0x300) {
        if (h < 600) h = 600;
        g_bandY = -(int)((float)(0x300 - h) * 0.45f);
    }
    g_effect = GUI::CreateMovementEffect(g_root, OnTweenMove, OnTweenIn, OnTweenOut);
    g_text1 = GUI::GetWindowTyped<Textfield>(g_root, "text_01");
    g_text2 = GUI::GetWindowTyped<Textfield>(g_root, "text_02");
    g_bar.LoadFrom(g_root, "endscreen_progress_bar", "progress_bar_end_orange", "progress_bar_large_orange");
    g_bar.Rescale(g_rootScale);
    g_queue->fn.zRange = WindowManager::ReturnSmallZRange;
    g_queue->fn.back = OnBack;
    g_queue->fn.update = Update;
    g_queue->fn.show = Show;
    g_blackTex = Render::CreateSolidFillTexture(0.f, 0.f, 0.f, 1.f);
    g_black = Render::CreateSprite(g_blackTex, 0xe, false, false);
    g_black->screenSpace = true;
    Render::SetShaderType(g_black, 1);
    Render::SetVisibility(g_black, false);
    Render::SetAlpha(g_black, 0.6f);
    g_goldline = Render::CreateSprite(Resources::GetUIImage("Endscreen_goldline_scaler.png", false, false), 0xe, false, false);
    g_goldline->screenSpace = true;
    Render::SetVisibility(g_goldline, false);
}

}  // namespace BossOutroWindow
