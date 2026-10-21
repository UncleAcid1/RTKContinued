// RaidCompleteWindow: "Raid complete!" at a mission's end (Endscreen_raid_complete.xml): the banner
// slides down over a black band, the squad cheers, and the loot strip (FoundItemsWindow, "You
// found") rises from the bottom; the first completion adds the "farm_complete_cb" crystals to it.
// Closing the strip slides the banner away and opens the mission's brief (RaidInfoWindow) as after
// its end, with the new star's animation; the brief's button then resets the mission (ResetRaid):
// its quests, items and map start over and the completion counts.
// Port of RaidCompleteWindow (libkingdom.so 5.11), 0x32df5c..0x32f150. Statics at 0x62b134.
//
// PORT (online removed): ResetRaid's star statistics request (OG::MakeRequest(0xd, 9, ...)) and
// OnTweenIn's SaveManager::QueueSendSave.
#include <cstdio>

#include "engine/SoundsManager.h"
#include "engine/Render.h"
#include "engine/Resources.h"
#include "engine/Timer.h"
#include "game/AIState.h"
#include "game/BuildingHovers.h"
#include "game/Entity.h"
#include "game/EntityManager.h"
#include "game/GameState.h"
#include "game/Locations.h"
#include "game/Map.h"
#include "game/Rand48.h"
#include "game/SaveManager.h"
#include "game/Setting.h"
#include "game/Squad.h"
#include "game/StringTable.h"
#include "game/Tasks.h"
#include "engine/Text.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/HUD.h"
#include "windows/Windows.h"

namespace RaidCompleteWindow {
namespace {

using GUI::Textfield;
using GUI::Window;

WindowManager::FunctionalWindow* g_queue = nullptr;   // RaidCompleteWindow::wnd 0x62b154
uint32_t g_location = 0;              // 0x62b134 the mission (location id)
Window* g_root = nullptr;             // 0x62b138
GUI::MovementEffect* g_effect = nullptr;   // 0x62b13c
int g_bandY = 0;                      // 0x62b140 the banner's y (small screens: slid up)
Render::Sprite* g_black = nullptr;    // 0x62b144 the black band (alpha 0.6)
Render::Sprite* g_goldline = nullptr; // 0x62b148 Endscreen_goldline_scaler.png under the band
Textfield* g_text = nullptr;          // 0x62b14c "text": "<mission> completed"
Render::Texture* g_blackTex = nullptr;   // 0x62b150
bool g_tablet = false;                // 0x60f360
float g_rootScale = 1.f;              // 0x60f364

// @0x32e138: the band settles; the game is saved (not in the city tutorial).
void OnTweenIn() {
    const float y = g_effect->hideAtEnd ? 0.f : g_black->h;
    Render::SetPosition(g_black, 0.f, y, 0.02f);
    Render::SetPosition(g_goldline, 0.f, g_black->y + 5.f, 0.0199f);
    SoundsManager::PlaySound("ui_campaign_completed", 1, false);
    if (!g_root->visibleSelf) return;
    if (GameState::IsCityTutorial()) return;
    Map::Save(0);
}

// @0x32e1fc: the band follows the banner.
void OnTweenMove() {
    if (!g_effect->hideAtEnd) Render::SetPosition(g_black, 0.f, g_black->h * g_effect->t, 0.02f);
    else Render::SetPosition(g_black, 0.f, (1.f - g_effect->t) * g_black->h, 0.02f);
    Render::SetPosition(g_goldline, 0.f, g_black->y + 5.f, 0.0199f);
    Render::SortRenderLayer(0xe, 1);
}

// @0x32e89c: the banner is gone; the mission's brief opens as after its end (its new star
// animated), or, without the mission, it is reset at once.
void OnTweenOut() {
    WindowManager::WindowHide(false);
    g_queue->shown = false;
    g_queue->MoveWindowDown(true);
    Render::SetVisibility(g_black, false);
    Render::SetVisibility(g_goldline, false);
    g_root->SetVisibility(false);
    Locations::LocationInfo* info = Locations::GetLocationInfo(g_location);
    if (!info) {
        ResetRaid(false);
        return;
    }
    RaidInfoWindow::SetParameters(info, info->startMap, true);
    RaidInfoWindow::Show();
    if (Locations::GetMissionStarCount(info->startMap, 0) == 3) return;
    RaidInfoWindow::RunCompletionAnimation();
}

// @0x32e2bc
void UpdateContents() {
    Locations::LocationInfo* info = Locations::GetLocationInfo(g_location);
    if (!info) return;
    g_text->SetText(SWPrintf(0x100, StringTable::GetString("RAID_COMPLETED"), {info->name}).c_str());
}

// @0x32e518: the band's texture comes back after a context loss; alone on screen it returns to
// the top with the loot strip.
void Update(float) {
    if (!g_queue->shown) return;
    if (!g_black) return;
    if (!g_blackTex || g_blackTex->glId == 0) {
        g_blackTex = Render::CreateSolidFillTexture(0.f, 0.f, 0.f, 1.f);
        Render::SetTexture(g_black, g_blackTex);
    }
    if (WindowManager::GetShownWindowCount() != 1) return;
    if (WindowManager::Head() == g_queue) return;
    FoundItemsWindow::Show();
    g_queue->MoveWindowOnTop(true);
}

// @0x32e030: the band is 37% of the screen.
void SetZ(float z) {
    if (!g_queue->shown) return;
    g_root->SetZ(z);
    g_root->SetPosition(g_effect->GetCenteredX(), g_bandY);
    g_black->w = (float)GUI::ScreenWidth();
    g_black->h = (float)GUI::ScreenHeight() * 0.37f;
    Render::SetPosition(g_black, 0.f, g_black->h, 0.02f);
    g_goldline->w = (float)GUI::ScreenWidth();
    Render::SetPosition(g_goldline, 0.f, g_black->y + 5.f, 0.0199f);
}

// @0x32edac: the taps go to the loot strip.
bool Click(int x, int y, bool pressed) {
    FoundItemsWindow::Click(x, y, pressed);
    return g_queue->shown;
}

// @0x32edc8
void Deinit() {
    GUI::RemoveMovementEffect(g_effect);
    g_effect = nullptr;
    delete g_root;
    g_root = nullptr;
    g_black = Render::RemoveSprite(g_black);
    g_goldline = Render::RemoveSprite(g_goldline);
}

}  // namespace

WindowManager::FunctionalWindow* Queue() {
    if (!g_queue) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.deinit = Deinit;
        f.click = Click;
        f.setZ = SetZ;
        f.hide = Hide;
        g_queue = new WindowManager::FunctionalWindow("RaidCompleteWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_queue->shown; }              // @0x32df5c
void SetParameters(uint32_t location) { g_location = location; }   // @0x32df70

// @0x32e5e0: the mission starts over: its quests are reset (and hidden), its items to remove go,
// its map is reset and loaded again, and the completion counts: the first one pays "farm_complete_cb"
// crystals (at most 5), a new star completes the step (0x29, star + map * 10 + 1). Unless silent,
// the world map opens with the "where to next" window.
void ResetRaid(bool silent) {
    Locations::LocationInfo* info = Locations::GetLocationInfo(g_location);
    if (!info) return;
    for (int id : info->clearTasks) {
        GameState::ResetTask((unsigned)id);
        if (Tasks::TaskInfo* t = Tasks::GetTask((uint32_t)id)) t->dnShow = true;
    }
    for (int id : info->removeItems)
        if (id != 0) GameState::RemoveItem((uint32_t)id, GameState::GetItemAmount((uint32_t)id, false));
    const uint32_t map = info->maps[0].id;
    SaveManager::GetMainSave()->ResetMap(map);
    GameState::ResetBossFightsOnMap(map);
    GameState::ResetMapCollectionInfo(map);
    GameState::SetMapStartTime(map, 0);
    if (SaveManager::SaveBlock* block = SaveManager::GetMapData(map)) {
        GameState::SetCurrentMapID(map);
        Map::Load(block, (uint32_t)Rand48::lrand48(), false, false);
    } else {
        std::printf("ERROR: RaidCompleteWindow::ResetRaid() Map %d cannot be found\n", (int)map);
    }
    HUDWindow::HideMapName();
    const unsigned count = (unsigned)GameState::GetMapCompletionCount(map);
    unsigned star = info->starsCount[0] <= count ? 1 : 0;
    if (info->starsCount[1] <= count) star = 2;
    if (info->starsCount[2] <= count) star = 3;
    if (info->mission && count == 0) {
        int crystals = Setting("farm_complete_cb").GetInt();
        if (crystals >= 5) crystals = 5;
        crystals = GameState::CrystalReward(crystals);   // PORT (balance, user 2026-10-20)
        std::printf("Adding %d crystals for completing the farm map for the first time\n", crystals);
        GameState::AddCrystals(crystals);
    }
    GameState::AddMapCompletion(map);
    if (star != 3 && info->starsCount[star] <= count + 1) {
        Tasks::CompleteSubtask(0x29, star + map * 10 + 1, 1);
        // (OG::MakeRequest(0xd, 9, location, -1, 0): online)
    }
    GameState::SetMapStartTime(map, 0);
    GameState::SetMapEndTime(map, (uint32_t)Timer::GetGlobalTime());
    if (silent) return;
    GlobalMapWindow::Show();
    Map::SafeSave();
    ChoosePathWindow::Show();
    TaskHolderWindow::UpdateTasks();
}

// @0x32e948 (exit, reload): closed at once, the mission reset silently.
void ForceHide() {
    if (!g_queue->shown) return;
    FoundItemsWindow::Hide();
    WindowManager::WindowHide(false);
    g_queue->shown = false;
    g_queue->MoveWindowDown(true);
    Render::SetVisibility(g_black, false);
    Render::SetVisibility(g_goldline, false);
    g_root->SetVisibility(false);
    ResetRaid(true);
}

// @0x32e9cc (FoundItemsWindow's close): the banner slides back up.
void Hide() {
    if (!g_root->visibleSelf) return;
    const int x = g_effect->GetCenteredX();
    g_effect->Animate(x, g_bandY, g_effect->GetCenteredX(), -g_root->h, false);
}

// @0x32ea54: the banner slides down, the drops are collected, the loot strip rises (with the first
// completion's crystals), the squad cheers and the camera glides to the hero.
void Show() {
    if (g_queue->shown) return;
    g_queue->shown = true;
    g_queue->MoveWindowOnTop(true);
    WindowManager::WindowShow(false);
    const int x = g_effect->GetCenteredX();
    g_effect->Animate(x, -g_root->h, g_effect->GetCenteredX(), g_bandY, true);
    g_root->SetPosition(g_effect->GetCenteredX(), -g_root->h);
    g_root->SetVisibility(true);
    BuildingHovers::CollectAll();
    // PORT (DECIDED user 2026-10-18): a map holding a chest key gives it with a 1/4 chance.
    if (const uint32_t key = Map::RollHeldChestKey(4, false)) {
        GameState::AddItem(key, 1, false, false, false);
        GameState::AddMapItemCollectionInfo((uint32_t)GameState::GetCurrentMapID(), key, 1);
    }
    unsigned crystals = 0;
    if (Locations::LocationInfo* info = Locations::GetLocationInfo(g_location)) {
        const int count = GameState::GetMapCompletionCount(info->maps[0].id);
        if (info->mission && count == 0) crystals = (unsigned)GameState::CrystalReward(Setting("farm_complete_cb").GetInt());
    }
    FoundItemsWindow::SetParameters(Locations::GetLocationInfo(g_location), StringTable::GetString("RAID_YOU_FOUND"),
                                    crystals);
    FoundItemsWindow::Show();
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
                e->GetAI()->StopMovement();
                e->SetDirection((Rand48::lrand48() & 1) == 0 ? 1 : 7);
                e->SetAnimationP(e->player ? "bat_vict" : "victory", true, false, false);
                if (e->player) Render::CenterOn(e->worldX, e->worldY - e->GetIdleHeight(), true, 0.4f, 0, false);
            }
    UpdateContents();
    g_root->PreloadTextures();   // GUI::PreloadWindow
}

// @0x32ee2c
void Init() {
    g_blackTex = nullptr;
    g_black = nullptr;
    g_goldline = nullptr;
    g_tablet = GUI::IsTabletVersion();
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Endscreen_raid_complete.xml", "Endscreen_raid_complete.png",
                             GUI::GetHoverScaleFactor(1.f, 1.f), 0, 0, 0, 0, false, 1.f);
    g_queue->RegisterTopWindow(g_root);
    g_rootScale = g_root->scale;
    g_root->SetVisibility(false);
    g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::GetVerticalCenter(g_root->h));
    int h = GUI::ScreenHeight();
    if (h < 0x300) {
        if (h < 600) h = 600;
        g_bandY = -(int)((float)(0x300 - h) * 0.37f);
    }
    g_effect = GUI::CreateMovementEffect(g_root, OnTweenMove, OnTweenIn, OnTweenOut);
    g_text = GUI::GetWindowTyped<Textfield>(g_root, "text");
    g_queue->fn.show = Show;
    g_queue->fn.zRange = WindowManager::ReturnSmallZRange;
    g_queue->fn.update = Update;
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

}  // namespace RaidCompleteWindow
