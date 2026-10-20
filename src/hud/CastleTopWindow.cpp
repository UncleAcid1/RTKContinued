// CastleTopWindow: the navigation panel at the top right (Friends, Shop, world map / attack, PvP,
// settings). Port of CastleTopWindow (libkingdom.so 5.11), 0x276934..0x279f6c.
#include <cstdio>

#include "engine/IconManager.h"
#include "engine/Render.h"
#include "engine/Timer.h"
#include "game/Building.h"
#include "game/BuildingHovers.h"
#include "game/BuildingMovement.h"
#include "game/BuildingPlacement.h"
#include "game/Combat.h"
#include "game/Entity.h"
#include "game/EntityManager.h"
#include "game/GameState.h"
#include "game/Locations.h"
#include "game/Map.h"
#include "game/Rand48.h"
#include "game/SaveManager.h"
#include "game/Setting.h"
#include "game/StringTable.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/HUD.h"
#include "windows/Windows.h"

namespace CastleTopWindow {
namespace {

using GUI::Button;
using GUI::Textfield;
using GUI::Window;

WindowManager::FunctionalWindow* g_queue = nullptr;
int g_offsetX = 3, g_offsetY = -2;   // 0x60f164 / 0x60f160 (Init overwrites the data values)
bool g_tablet = false;
Window* g_root = nullptr;            // "Hud_navigation_map"
GUI::MovementEffect* g_effect = nullptr;
Window* g_friends = nullptr;         // button_hud_nav_blue_01
Window* g_friendsBg = nullptr;
Window* g_friendsIcon = nullptr;
Textfield* g_friendsText = nullptr;
Window* g_shop = nullptr;            // button_hud_nav_blue_02
Window* g_shopBg = nullptr;
Window* g_shopIcon = nullptr;
Textfield* g_shopText = nullptr;
Window* g_compass = nullptr;         // button_world_map_compass
Window* g_compassBg = nullptr;
Textfield* g_compassText = nullptr;
Window* g_pvp = nullptr;             // button_hud_nav_blue_03
Window* g_pvpBg = nullptr;
Window* g_pvpIcon = nullptr;
Textfield* g_pvpText = nullptr;
Window* g_giPlay = nullptr;          // button_nav_dangle_gi_play (online: Game Insight portal)
Window* g_giPlayIcon = nullptr;
Button* g_giPlayClick = nullptr;
Window* g_settings = nullptr;        // button_nav_dangle_settings
Window* g_settingsIcon = nullptr;
Button* g_settingsClick = nullptr;
Window* g_giPlayButton = nullptr;
Window* g_moreGames = nullptr;
Window* g_friendsNotify = nullptr;
Textfield* g_friendsNotifyText = nullptr;
Window* g_giNotify = nullptr;
Textfield* g_giNotifyText = nullptr;
bool g_campaignToCityDirect = false;   // 0x61922c Setting "campaign_to_city_direct" == 1
int g_blinkMap = 0;                    // 0x619214 BlinkCompass's map
uint32_t g_buyItem = 0;                // 0x619228 OnBuyItem's item: the shop opens on it once
int g_arrowCallback = 0;               // 0x61921c the id of the tutorial arrow's callback
unsigned g_shownWindows = 0;           // 0x619220 the fewest windows shown since the arrow went up
int g_friendRequests = 0;              // 0x61924c (online: always 0)
int g_lastSecond = 0;                  // 0x619250

// @0x277f5c: the panel takes clicks even on disabled buttons (force), and the tutorial arrow at the
// compass first. While a building is edited or placed, the shop is open or at tutorial step 0x17 it
// only absorbs the clicks that land on it (while editing, only presses). (BottomScreenshotWindow is
// milestone 5 and never open; the featured-quest holders, which take clicks after the root, too.)
bool Click(int x, int y, bool pressed) {
    if (!g_root) return false;
    const bool editing = BuildingMovement::Activated() || BuildingPlacement::Activated();
    if (editing || ShopWindow::IsVisible() || GameState::tutorial == 0x17) {
        bool r = g_root->GetWindowAtPosition(x, y, false) != nullptr;
        if (editing && !pressed) r = false;
        return r;
    }
    if (BuildingHovers::GetArrowClickCallbackID() == g_arrowCallback && BuildingHovers::ClickOnArrow(x, y, pressed))
        return true;
    return g_root->Click(x, y, pressed, true);
}

// @0x276d0c. UNVERIFIED (milestone 5, screenshot help): while the screenshot help is pending (a flag set with the
// tutorial arrows) it first hides its arrow and calls SettingsWindow::HelpWithScreenShot.
void OnSettings() { SettingsWindow::Show(); }

}  // namespace

// @0x27738c the compass: to the world map (from the city or a farm), or back to the castle from a
// campaign map's fight or the arena; not while editing, shopping or in a late fight phase.
void OnReturn() {
    std::printf("saint -> castletop\n");
    if (BuildingMovement::Activated() || BuildingPlacement::Activated() || ShopWindow::IsVisible()) return;
    if (GameState::IsCombatActive() && GameState::GetActiveCombat()->GetType() > 2) return;
    if (GameState::IsCombatActive()) {
        PopupWindow::Show(StringTable::GetString("CAN_NOT_TRAVEL"), PopupWindow::Hide, nullptr, nullptr);
        return;
    }
    const int step = GameState::tutorial;
    const bool firstTravel = GameState::GetTutorialType() == 1 && GameState::GetCurrentMapID() == 0 && step == 0x37 &&
                             !GameState::TaskCompleted(0x237);
    if (GameState::GetCurrentLocation() == 0 && step <= 0x66 && !firstTravel) {
        PopupWindow::Show(StringTable::GetString("FINISH_TUTORIAL_TO_TRAVEL"), PopupWindow::Hide, nullptr, nullptr);
        return;
    }
    if (!GameState::IsPlayerCity()) return;   // GoHomeFromFriend: a friend's city (online)
    const int location = GameState::GetCurrentLocation();
    bool choosePath = false;
    if (location == 1) {   // a farm
        GlobalMapWindow::CenterOnMap(GameState::GetCurrentMapID());
        GlobalMapWindow::Show();
        TaskHolderWindow::UpdateTasks();
        BuildingHovers::HideArrow();
        HUDWindow::ExitFarm();
        EntityManager::Update(0.f);
        BuildingHovers::Update(0.0, true);
        choosePath = GameState::TaskCompleted(0x43f);
    } else {
        if (location == 2 && g_campaignToCityDirect && Locations::GetCampaignForMap(GameState::GetCurrentMapID())) {
            GlobalMapWindow::SwitchMap(0, false, true);
            return;
        }
        if (location != 2 && location != 0) {
            // The arena (location 3, after tutorial step 0x7f): saved, straight back to the city.
            if (location != 3 || GameState::tutorial <= 0x7f) return;
            // UNVERIFIED (milestone 6): SoundsManager::PlaySound("ui_map_opened", 1, false).
            if (GameState::GetCurrentMapID() != 0) {
                Map::Save(0);
                const bool cached = SaveManager::GetMainSave()->HasMapInCache(0);
                SaveManager::SaveBlock* block = SaveManager::GetMapData(0);
                GameState::SetCurrentMapID(0);
                Map::Load(block, (uint32_t)Rand48::lrand48(), false, cached);
                Map::UpdateOfflineResources();
            }
            GlobalMapWindow::Hide();
            TaskHolderWindow::UpdateTasks();
            return;
        }
        GlobalMapWindow::CenterOnMap(GameState::GetCurrentMapID());
        GlobalMapWindow::Show();
        TaskHolderWindow::UpdateTasks();
        if (step == 0x80 && GameState::secondTutorial == 0x81 && GameState::GetCurrentMapID() == 0)
            GlobalMapWindow::ArrowAtMap(0xf, false, true);
        if (firstTravel) GlobalMapWindow::ArrowAtMap(0xf, false, true);
        const uint32_t now = (uint32_t)Timer::GetGlobalTime();
        if (GameState::GetEventStartTime(0x5dc) == 0) GameState::SetEventStartTime(0x5dc, now);
        if (GameState::GetEventStartTime(2000) == 0) {
            GameState::SetEventStartTime(2000, now);
            GameState::SetEventStartTime(0x7d2, (uint32_t)GameState::GetGoldEarned());
            GameState::SetEventStartTime(0x7d1, (uint32_t)GameState::GetGoldEarned());
        }
        if (GameState::GetTaskBeginTime(0x43f) == 0) return;
        if (GameState::GetTaskBeginTime(0x5c9) != 0 && !GameState::TaskCompleted(0x5c9) &&
            GameState::GetEventStartTime(0x5dd) == 0) {
            GameState::SetEventStartTime(0x5dc, now);
            GlobalMapWindow::CenterOnSultan();
            SultanArrivingWindow::Show();
            return;
        }
        if (0x14370u < now - GameState::GetEventStartTime(2000)) {
            // The "easy money" offer (StartRaidingWindow) when the gold earned since the last check
            // (plus "easy_money_delta") grew less than in the check before and the gold is under
            // "easy_money_threshold". GetGoldEarned is always 0 in 5.11, so it never shows.
            const uint32_t earned = (uint32_t)GameState::GetGoldEarned();
            const uint32_t last = GameState::GetEventStartTime(0x7d2);
            const uint32_t before = GameState::GetEventStartTime(0x7d1);
            const uint32_t delta = (uint32_t)Setting("easy_money_delta").GetInt();
            if (earned + delta - last < last - before &&
                (int)GameState::GetResourceAmount(GameState::kGold) < Setting("easy_money_threshold").GetInt())
                StartRaidingWindow::Show();
            GameState::SetEventStartTime(2000, (uint32_t)Timer::GetGlobalTime());
            GameState::SetEventStartTime(0x7d2, earned);
            GameState::SetEventStartTime(0x7d1, last);
            if (StartRaidingWindow::IsVisible()) return;
        }
        choosePath = GameState::TaskCompleted(0x43f);
    }
    if (choosePath) ChoosePathWindow::Show();
}

// @0x2778b0 the Shop button: the help with the shop's quest while it is not done, else the item
// shop (on OnBuyItem's item, once).
void OnShop() {
    if (GameState::TryToHelpWithTask((unsigned)Setting("shop_unlock_task").GetInt())) return;
    ItemShopWindow::Show();
    if (g_buyItem != 0) {
        ItemShopWindow::ShowShopItem(g_buyItem, 0, true, (unsigned)-1);
        g_buyItem = 0;
    }
}

// @0x27723c: the arrow at the Shop button; its tap (or the button's) opens the shop on item.
void OnBuyItem(uint32_t item) {
    g_buyItem = item;
    const int id = BuildingHovers::SetArrowClickCallback(new GUI::Callback(OnShop));
    g_arrowCallback = id;
    BuildingHovers::ArrowAt((float)(g_root->x + g_shop->x + g_shop->w / 2), (float)(g_root->y + g_shop->y + g_shop->h),
                            false, false, false, true, true, false, false);
    BuildingHovers::PreventArrowAutoHide();
}

// @0x276d64 the PvP button (help: a help page, HelpWindow online): only in the city while nothing is being
// placed and no fight beyond an aggro one runs; before the PvP quest 0x74d is done its help.
void OnPvP(unsigned help) {
    if (!GameState::IsPlayerCity() || BuildingMovement::Activated() || BuildingPlacement::Activated() ||
        ShopWindow::IsVisible())
        return;
    if (GameState::IsCombatActive() && GameState::GetActiveCombat()->GetType() > 2) return;
    if (GameState::IsCombatActive()) {
        PopupWindow::Show(StringTable::GetString("CAN_NOT_TRAVEL"), PopupWindow::Hide, nullptr, nullptr);
        return;
    }
    if (!GameState::TaskCompleted(0x74d)) {
        LevelInfoWindow::HelpWithPvP(false);
        return;
    }
    if (help != 0xffffffffu) return;   // (HelpWindow::Show(help): the platform's help web view, online)
    PlayerCompetitionWindow::Show();
    PlayerCompetitionWindow::OnTab(4);
}

// @0x277048: the arrow at the PvP button (over the windows shown now); its tap is the button's.
void HelpWithPvP() {
    g_shownWindows = WindowManager::GetShownWindowCount();
    BuildingHovers::SetArrowVisibleWindowLimit(g_shownWindows);
    g_arrowCallback = BuildingHovers::SetArrowClickCallback(new GUI::Callback([] { OnPvP(0xffffffffu); }));
    BuildingHovers::ArrowAt((float)(g_root->x + g_pvp->x), (float)(g_root->y + g_pvp->y + g_pvp->h / 2), false, true,
                            true, true, true, false, false);
    BuildingHovers::PreventArrowAutoHide();
}

// @0x276c50
void BlinkCompass() { g_blinkMap = (int)GameState::GetCurrentMapID(); }

// @0x2772e8 (the HUD's player locator): the camera glides to the hero in a fight, else to the castle.
void OnFindPlayer() {
    if (GameState::GetCurrentLocation() == 2) {
        if (Entity* p = EntityManager::GetPlayer()) Render::CenterOn(p->worldX, p->worldY, true);
    } else if (Map::Building* hq = Map::GetHQ()) {
        Render::CenterOn((hq->minX + hq->maxX) * 0.5f, hq->maxY, true);
    }
}

WindowManager::FunctionalWindow* Queue() {
    if (!g_queue) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.setZ = SetZ;
        f.hide = Hide;
        f.click = Click;
        g_queue = new WindowManager::FunctionalWindow("CastleTopWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_root && g_root->visibleSelf; }

// @0x278568
void Init() {
    g_offsetX = 3;
    Queue()->usesZRange = false;
    g_offsetY = -2;
    g_tablet = GUI::IsTabletVersion();
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Hud_navigation_map.xml", "Hud_navigation_map.png",
                             GUI::GetHudScaleFactor(), 0, 0, 0, 0, false, 1.f);
    if (!g_root) return;
    g_root->SetVisibility(false);
    g_root->SetPosition(GUI::ScreenWidth(), 0);
    g_effect = GUI::CreateMovementEffect(g_root);
    GUI::GetWindowTyped<Window>(g_root, "hud_navigation_main_bg")->noInput = true;

    // OnFriends opens the friends' tabs of PlayerCompetitionWindow (online, dormant): a no-op.
    g_friends = GUI::GetWindowTyped<Window>(g_root, "button_hud_nav_blue_01");
    g_friends->SetOnClick([] {});
    g_friendsBg = GUI::GetWindowTyped<Window>(g_root, "button_hud_nav_blue_01.button_large_blue_bg");
    g_friendsIcon = GUI::GetWindowTyped<Window>(g_root, "button_hud_nav_blue_01.icon_60");
    g_friendsIcon->SetTexture(IconManager::GetIcon("friend_list_60", false), true);
    g_friendsText = GUI::GetWindowTyped<Textfield>(g_root, "hud_vanigation_name_overlay.text_01");
    g_friendsText->SetText(StringTable::GetString("FRIENDS_BUTTON"));

    g_shop = GUI::GetWindowTyped<Window>(g_root, "button_hud_nav_blue_02");
    g_shop->SetOnClick(OnShop);
    g_shopBg = GUI::GetWindowTyped<Window>(g_root, "button_hud_nav_blue_02.button_large_blue_bg");
    g_shopIcon = GUI::GetWindowTyped<Window>(g_root, "button_hud_nav_blue_02.icon_60");
    g_shopIcon->SetTexture(IconManager::GetIcon("Icon_bottom_bar_shop", false), true);
    g_shopText = GUI::GetWindowTyped<Textfield>(g_root, "hud_vanigation_name_overlay.text_02");
    g_shopText->SetText(StringTable::GetString("main_menu_button_shop"));

    g_compass = GUI::GetWindowTyped<Window>(g_root, "button_world_map_compass");
    g_compass->SetOnClick(OnReturn);
    g_compassBg = GUI::GetWindowTyped<Window>(g_root, "button_world_map_compass.button_nav_world_map_bg");
    g_compassText = GUI::GetWindowTyped<Textfield>(g_root, "hud_vanigation_name_overlay.text_03");
    g_compassText->SetText(StringTable::GetString("main_menu_button_shop"));   // replaced by Update

    g_pvp = GUI::GetWindowTyped<Window>(g_root, "button_hud_nav_blue_03");
    g_pvp->SetOnClick([] { OnPvP(0xffffffffu); });
    g_pvpBg = GUI::GetWindowTyped<Window>(g_root, "button_hud_nav_blue_03.button_large_red_bg");
    g_pvpIcon = GUI::GetWindowTyped<Window>(g_root, "button_hud_nav_blue_03.icon_60");
    g_pvpText = GUI::GetWindowTyped<Textfield>(g_root, "text_pvp");
    g_pvpIcon->SetTexture(IconManager::GetIcon("rep_pvp_valor_60", false), true);
    g_pvpText->SetText(StringTable::GetString("PVP"));

    g_giPlay = GUI::GetWindowTyped<Window>(g_root, "button_nav_dangle_gi_play");
    g_giPlayIcon = GUI::GetWindowTyped<Window>(g_root, "button_nav_dangle_gi_play.icon_chained");
    g_giPlayClick = GUI::GetWindowTyped<Button>(g_root, "button_nav_dangle_gi_play.clickArea");
    g_giPlayClick->SetOnClick([] {});
    g_settings = GUI::GetWindowTyped<Window>(g_root, "button_nav_dangle_settings");
    g_settingsIcon = GUI::GetWindowTyped<Window>(g_root, "button_nav_dangle_settings.icon_chained");
    g_settingsIcon->SetTexture(IconManager::GetIcon("gold_settings", false), true);
    g_settingsClick = GUI::GetWindowTyped<Button>(g_root, "button_nav_dangle_settings.clickArea");
    g_settingsClick->SetOnClick(OnSettings);
    g_giPlayButton = GUI::GetWindowTyped<Window>(g_root, "button_nav_dangle_gi_play.button_gi_play");
    g_moreGames = GUI::GetWindowTyped<Window>(g_root, "button_nav_dangle_gi_play.button_more_games");
    g_friendsNotify = GUI::GetWindowTyped<Window>(g_root, "friendsi_notification_holder");
    g_friendsNotifyText = GUI::GetWindowTyped<Textfield>(g_root, "friendsi_notification_holder.text_notification");
    g_giNotify = GUI::GetWindowTyped<Window>(g_root, "gi_notification_holder");
    g_giNotifyText = GUI::GetWindowTyped<Textfield>(g_root, "gi_notification_holder.text_notification");
    g_giPlayButton->SetVisibility(false);
    g_moreGames->SetVisibility(true);
    g_giNotify->SetVisibility(false);

    Queue()->fn.update = Update;
    Queue()->fn.show = Show;
    Queue()->fn.back = [] { return false; };   // OnBack @0x277894
    // UNVERIFIED: Setting("campaign_to_city_direct") comes from dynamic_config (milestone 3).
    g_campaignToCityDirect = false;
    // The two featured-quest holders (Quest_featured_holder.xml) start hidden; they follow with
    // the featured quests (milestone 5).
}

// @0x277c60: slide in from the right, in the city after the tutorial's first level (at step 0x34
// for tutorial types 1 and 2), not during the second tutorial's steps 0x91..0x9d.
// (GUI::PreloadWindow: the port's textures are loaded already.)
void Show() {
    if (!g_root || !GameState::IsPlayerCity()) return;
    const int type = GameState::GetTutorialType();
    if (!(GameState::tutorial > 0x36 || ((type == 1 || type == 2) && GameState::tutorial == 0x34))) return;
    if ((unsigned)(GameState::secondTutorial - 0x91) <= 0xc) return;
    if (g_effect->active && g_effect->hideAtEnd) g_root->SetVisibility(false);
    Queue()->MoveWindowOnTop(true);
    int W = GUI::ScreenWidth();
    if (!g_root->visibleSelf) g_effect->Animate(W, g_offsetY, (W + g_offsetX) - g_root->w, g_offsetY, true);
    g_root->SetVisibility(true);
}

// @0x277b6c (the featured holders' reset: milestone 5)
void Hide() {
    if (!g_root || !g_root->visibleSelf) return;
    int W = GUI::ScreenWidth();
    g_effect->Animate((W + g_offsetX) - g_root->w, g_offsetY, W, g_offsetY, false);
}

// @0x277ddc
void SetZ(float) {
    if (!g_root) return;
    g_root->SetZ(HUDWindow::GetZ());
    if (!g_effect->active) {
        int W = GUI::ScreenWidth();
        if (!g_root->visibleSelf) g_root->SetPosition(W, g_offsetY);
        else g_root->SetPosition((g_offsetX + W) - g_root->w, g_offsetY);
    }
}

// @0x278e40: the buttons' states, the tutorial's arrows at the compass (its tap is the compass's)
// and, once a second, the shop's. UNVERIFIED (milestone 5): the two featured-quest holders (their
// slide, pulse and timers, ActivateFeatureHolders every 300 s, the featured tasks list) and the
// compass blink (BlinkCompass).
void Update(float) {
    if (!g_root || !g_root->visibleSelf) return;
    const unsigned shown = WindowManager::GetShownWindowCount();
    const unsigned before = g_shownWindows;
    if (shown < g_shownWindows) g_shownWindows = shown;
    if (BuildingHovers::GetArrowClickCallbackID() == g_arrowCallback && before < shown && GameState::tutorial != 0x5b)
        BuildingHovers::HideArrow();
    const bool toCastle = GameState::GetCurrentLocation() == 3 ||
                          (g_campaignToCityDirect && Locations::GetCampaignForMap(GameState::GetCurrentMapID()));
    const bool cityIdle = !GameState::IsCombatActive() && GameState::IsPlayerCity() && !Map::GetCurrentFarm();
    const bool canTravel = !GameState::IsCombatActive() || GameState::GetActiveCombat()->GetType() < 3;
    const bool friendsOn = GameState::TaskCompleted((unsigned)Setting("task_friends_enable").GetInt());
    g_friends->SetEnabled(cityIdle);
    g_friendsBg->SetEnabled(cityIdle && friendsOn);
    g_friendsIcon->SetEnabled(cityIdle && friendsOn);
    g_friendsText->SetEnabled(cityIdle && friendsOn);
    g_friendsNotify->SetVisibility(g_friendRequests != 0 && cityIdle && friendsOn);
    g_friendsNotifyText->SetText(ToWideString(g_friendRequests));
    g_compass->SetEnabled(canTravel);
    g_compassText->SetText(StringTable::GetString(toCastle ? "WORLD_NAV_TO_CASTLE" : "CONTROL_BATTLE_ATTACK"));
    const bool pvpOn = canTravel && GameState::TaskCompleted(0x74d) && !Map::GetCurrentFarm();
    g_pvpBg->SetEnabled(pvpOn);
    g_pvpIcon->SetEnabled(pvpOn);
    g_pvpText->SetEnabled(pvpOn);

    // The tutorial's way to the world map: an arrow under the compass.
    const int step = GameState::tutorial;
    const bool firstTravel = GameState::GetTutorialType() == 1 && GameState::GetCurrentMapID() == 0 && step == 0x37 &&
                             !GameState::TaskCompleted(0x237);
    const bool arrow = (step == 0x67 && WindowManager::GetShownWindowCount() == 0) ||
                       (step == 0x80 && GameState::secondTutorial == 0x81 && GameState::GetCurrentMapID() == 0 &&
                        WindowManager::GetShownWindowCount() == 0) ||
                       (firstTravel && WindowManager::GetShownWindowCount() == 0);
    if (arrow) {
        if (BuildingHovers::GetArrowClickCallbackID() != g_arrowCallback)
            g_arrowCallback = BuildingHovers::SetArrowClickCallback(new GUI::Callback(OnReturn));
        if (step == 0x67) GUI::SetInteractionObjectLock(g_compass, nullptr);
        BuildingHovers::ArrowAt((float)(g_compass->x + g_root->x + g_compass->w / 2),
                                (float)(g_compass->y + g_root->y + g_compass->h), false, false, false, true, true, false,
                                false);
    }
    if (!GameState::IsPlayerCity()) return;   // (the featured holders are reset: milestone 5)
    if (GameState::GetCurrentMapID() != g_blinkMap) {
        g_compass->SetAlpha(1.f, true);
        g_blinkMap = 0;
    }
    if (Timer::GetGlobalTime() == g_lastSecond) return;
    g_lastSecond = Timer::GetGlobalTime();
    g_shop->SetEnabled(!GameState::IsPvPCombatActive() && GameState::IsPlayerCity());
    const bool shopOn = GameState::TaskCompleted((unsigned)Setting("shop_unlock_task").GetInt());
    g_shopBg->SetEnabled(shopOn);
    g_shopIcon->SetEnabled(shopOn);
    g_shopText->SetEnabled(shopOn);
}

}  // namespace CastleTopWindow
