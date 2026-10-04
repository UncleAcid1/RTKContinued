// CastleTopWindow: the navigation panel at the top right (Friends, Shop, world map / attack, PvP,
// settings). Port of CastleTopWindow (libkingdom.so 5.11), 0x276934..0x279f6c.
#include "engine/IconManager.h"
#include "engine/Render.h"
#include "game/GameState.h"
#include "game/StringTable.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/HUD.h"

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
bool g_campaignToCityDirect = false;   // Setting "campaign_to_city_direct" == 1

}  // namespace

WindowManager::FunctionalWindow* Queue() {
    if (!g_queue) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.setZ = SetZ;
        f.hide = Hide;
        f.click = [](int, int, bool) { return false; };   // Click @0x277f5c: milestone 2d
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

    // UNVERIFIED (milestone 2d and later): OnFriends, OnShop, OnReturn, OnPvP, OnSettings open
    // windows that are not ported; the callbacks are installed as no-ops.
    g_friends = GUI::GetWindowTyped<Window>(g_root, "button_hud_nav_blue_01");
    g_friends->SetOnClick([] {});
    g_friendsBg = GUI::GetWindowTyped<Window>(g_root, "button_hud_nav_blue_01.button_large_blue_bg");
    g_friendsIcon = GUI::GetWindowTyped<Window>(g_root, "button_hud_nav_blue_01.icon_60");
    g_friendsIcon->SetTexture(IconManager::GetIcon("friend_list_60", false), true);
    g_friendsText = GUI::GetWindowTyped<Textfield>(g_root, "hud_vanigation_name_overlay.text_01");
    g_friendsText->SetText(StringTable::GetString("FRIENDS_BUTTON"));

    g_shop = GUI::GetWindowTyped<Window>(g_root, "button_hud_nav_blue_02");
    g_shop->SetOnClick([] {});
    g_shopBg = GUI::GetWindowTyped<Window>(g_root, "button_hud_nav_blue_02.button_large_blue_bg");
    g_shopIcon = GUI::GetWindowTyped<Window>(g_root, "button_hud_nav_blue_02.icon_60");
    g_shopIcon->SetTexture(IconManager::GetIcon("Icon_bottom_bar_shop", false), true);
    g_shopText = GUI::GetWindowTyped<Textfield>(g_root, "hud_vanigation_name_overlay.text_02");
    g_shopText->SetText(StringTable::GetString("main_menu_button_shop"));

    g_compass = GUI::GetWindowTyped<Window>(g_root, "button_world_map_compass");
    g_compass->SetOnClick([] {});
    g_compassBg = GUI::GetWindowTyped<Window>(g_root, "button_world_map_compass.button_nav_world_map_bg");
    g_compassText = GUI::GetWindowTyped<Textfield>(g_root, "hud_vanigation_name_overlay.text_03");
    g_compassText->SetText(StringTable::GetString("main_menu_button_shop"));   // replaced by Update

    g_pvp = GUI::GetWindowTyped<Window>(g_root, "button_hud_nav_blue_03");
    g_pvp->SetOnClick([] {});
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
    g_settingsClick->SetOnClick([] {});
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
    // the quest system (milestone 4).
}

// @0x277c60: slide in from the right. (The original also skips this while a second global is in
// 0x91..0x9d; that state belongs to later milestones.)
void Show() {
    if (!g_root || !GameState::IsPlayerCity() || GameState::TutorialStep() <= 0x36) return;
    if (g_effect->active && g_effect->hideAtEnd) g_root->SetVisibility(false);
    Queue()->MoveWindowOnTop(true);
    int W = GUI::ScreenWidth();
    if (!g_root->visibleSelf) g_effect->Animate(W, g_offsetY, (W + g_offsetX) - g_root->w, g_offsetY, true);
    g_root->SetVisibility(true);
}

// @0x277b6c (featured holders: milestone 4)
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

// @0x278e40, the parts whose inputs exist so far. UNVERIFIED stand-ins: TaskCompleted(...) is
// true (a finished tutorial), no combat, no farm, no notifications; tutorial arrows, the starter
// chest timer and the PvP state follow with quests/combat (milestones 3-4).
void Update(float) {
    if (!g_root || !g_root->visibleSelf) return;
    bool arena = GameState::GetCurrentLocation() == 3;   // || (campaign_to_city_direct && a campaign map)
    bool cityIdle = GameState::IsPlayerCity();          // && !combat && !farm
    bool friendsUnlocked = true;                        // TaskCompleted(Setting "task_friends_enable")
    g_friends->SetEnabled(cityIdle);
    g_friendsBg->SetEnabled(cityIdle && friendsUnlocked);
    g_friendsIcon->SetEnabled(cityIdle && friendsUnlocked);
    g_friendsText->SetEnabled(cityIdle && friendsUnlocked);
    g_friendsNotify->SetVisibility(false);              // pending friend requests (online)
    g_compass->SetEnabled(true);                        // not in a late combat phase
    g_compassText->SetText(StringTable::GetString(arena ? "WORLD_NAV_TO_CASTLE" : "CONTROL_BATTLE_ATTACK"));
}

}  // namespace CastleTopWindow
