// CastleTopWindow: the navigation panel at the top right (Friends, Shop, world map / attack, PvP,
// settings). Port of CastleTopWindow (libkingdom.so 5.11), 0x276934..0x279f6c.
#include <cmath>
#include <cstdio>
#include <vector>

#include "engine/SoundsManager.h"
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
#include "game/Tasks.h"
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
bool g_screenshotHelp = false;         // 0x619218 HelpWithScreenshot: the settings open with the screenshot help
int g_friendRequests = 0;              // 0x61924c (online: always 0)
int g_lastSecond = 0;                  // 0x619250
uint32_t g_lastActivate = 0;           // 0x619254 ActivateFeatureHolders' last time (every 300 s)
float g_scale = 1.f;                   // 0x60f168 the panel's layout scale

// CastleTopWindow::FeaturedQuestHolder (0x40 bytes, @0x27823c; ~ @0x276934 deletes the root):
// Quest_featured_holder.xml, the round button below the panel that leads to a featured quest. Two
// of them share one place and cross-fade when the featured quests take turns.
struct FeaturedQuestHolder {
    Window* root = nullptr;        // +0x00
    Window* glows[5] = {};         // +0x04 pickup_item_glow_green, _blue, _purple, _common, glow
    Window* icon = nullptr;        // +0x18 featured_icon_holder.icon_60 (takesZ)
    Textfield* time = nullptr;     // +0x1c featured_icon_holder.text_time
    Button* clickArea = nullptr;   // +0x20
    bool active = false;           // +0x24 it shows a quest
    uint32_t task = 0;             // +0x28
    float shownTime = 0.f;         // +0x2c seconds since it took the quest
    bool fading = false;           // +0x30 the alpha moves to targetAlpha
    float targetAlpha = 0.f;       // +0x34
    float spin = 0.f;              // +0x38 the glow's rotation and pulse phase
    float pulse = 0.f;             // +0x3c seconds of spinning left (ActivateFeatureHolders: 5)

    FeaturedQuestHolder() {
        root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Quest_featured_holder.xml", "Quest_featured_holder.png",
                               GUI::GetHudScaleFactor(), 0, 0, 0, 0, false, 1.f);
        static const char* const names[5] = {
            "item_pickup_holder.pickup_item_glow_green", "item_pickup_holder.pickup_item_glow_blue",
            "item_pickup_holder.pickup_item_glow_purple", "item_pickup_holder.pickup_item_glow_common",
            "item_pickup_holder.pickup_item_glow"};
        for (int i = 0; i < 5; ++i) glows[i] = GUI::GetWindowTyped<Window>(root, names[i]);
        icon = GUI::GetWindowTyped<Window>(root, "featured_icon_holder.icon_60");
        icon->takesZ = true;
        time = GUI::GetWindowTyped<Textfield>(root, "featured_icon_holder.text_time");
        clickArea = GUI::GetWindowTyped<Button>(root, "clickArea");
    }
    ~FeaturedQuestHolder() { delete root; }

    // @0x27698c: only glow `type` shows (4: the plain glow).
    void SetBackground(int type) {
        for (int i = 0; i < 5; ++i) glows[i]->SetVisibility(i == type);
    }
    // It fades out and gives up its quest (Hide, Update's turns, outside the city).
    void FadeOut() {
        targetAlpha = 0.f;
        active = false;
        fading = true;
    }
    // It fades in with `id` (the plain glow and the quest's icon).
    void TakeQuest(uint32_t id) {
        root->SetVisibility(true);
        root->SetAlpha(0.f, true);
        SetBackground(4);
        if (Tasks::TaskInfo* t = Tasks::GetTask(id)) icon->SetTexture(t->GetIcon(), true);
        targetAlpha = 1.f;
        shownTime = 0.f;
        fading = true;
        active = true;
        task = id;
    }
};

FeaturedQuestHolder* g_holders[2] = {};   // 0x61919c
std::vector<uint32_t> g_featured;         // 0x619204 (count 0x619210) this second's featured quests
unsigned g_featuredIndex = 0;             // 0x619234 the one shown

// @0x276e60: the featured button's tap: the quest's window (the pyramid exchange's crystals tab, the
// chests, the PvP rivals, the item shop's sale tabs), else its first step's help for a quick quest.
void OnFeatured() {
    FeaturedQuestHolder* h = g_holders[0]->active ? g_holders[0] : g_holders[1];
    if (!h->active) return;
    const uint32_t id = h->task;
    if (id == 0x4d8 || id == 0x4da || id == 0x4db) {
        ExchangeWindow::Show();
        ExchangeWindow::OnTab(1, nullptr);
        return;
    }
    const bool cityView = !Map::GetCurrentFarm() && !GlobalMapWindow::IsVisible();
    if (id == 0x6b4 && cityView) {
        ChestsWindow::Show();
        return;
    }
    if (id == 0x75b && cityView) {
        ChestsWindow::Show();
        ChestsWindow::SetStarterChestMode(false, Setting("starter_chest_b").GetInt() != 0 ? 0x3f : 0x35);
        return;
    }
    if (id == 0x77b && !Map::GetCurrentFarm()) {
        OnPvP(0x317);
        return;
    }
    if (id - 0x77cu < 5) {
        ItemShopWindow::Show();
        switch (id) {
        case 0x77c: ItemShopWindow::ShowShopTab(1); break;
        case 0x77d: ItemShopWindow::ShowShopItem(0xaf, 0, false, 0xffffffffu); break;
        case 0x77e: ItemShopWindow::ShowShopTab(2); break;
        case 0x77f: ItemShopWindow::ShowShopPack(2, false); break;
        default: ItemShopWindow::ShowShopTab(3); break;
        }
        return;
    }
    Tasks::TaskInfo* t = Tasks::GetTask(id);
    if (t && t->quickTask) t->subtasks[0].OnHelp(true);
}

// @0x277f5c: the panel takes clicks even on disabled buttons (force), and the tutorial arrow at the
// compass first. While a building is edited or placed, the shop is open or at tutorial step 0x17 it
// only absorbs the clicks that land on it (while editing, only presses); so in the screenshot mode.
// The featured-quest holders take the clicks the panel leaves.
bool Click(int x, int y, bool pressed) {
    if (!g_root) return false;
    const bool editing = BuildingMovement::Activated() || BuildingPlacement::Activated();
    if (editing || ShopWindow::IsVisible() || BottomScreenshotWindow::IsVisible() || GameState::tutorial == 0x17) {
        bool r = g_root->GetWindowAtPosition(x, y, false) != nullptr;
        if (editing && !pressed) r = false;
        return r;
    }
    if (BuildingHovers::GetArrowClickCallbackID() == g_arrowCallback && BuildingHovers::ClickOnArrow(x, y, pressed))
        return true;
    return g_root->Click(x, y, pressed, true) || g_holders[0]->root->Click(x, y, pressed, true) ||
           g_holders[1]->root->Click(x, y, pressed, true);
}

// @0x276d0c: with the screenshot help pending its arrow goes and the settings get the help.
void OnSettings() {
    if (g_screenshotHelp) {
        if (BuildingHovers::GetArrowClickCallbackID() == g_arrowCallback) BuildingHovers::HideArrow();
        SettingsWindow::HelpWithScreenShot();
        g_screenshotHelp = false;
    }
    SettingsWindow::Show();
}

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
            SoundsManager::PlaySound("ui_map_opened", 1, false);
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

// @0x277188: the screenshot quest's help: the arrow at the settings button (over the windows shown
// now); its tap opens the settings, which point at the screenshot mode.
void HelpWithScreenshot() {
    g_screenshotHelp = true;
    g_shownWindows = WindowManager::GetShownWindowCount();
    BuildingHovers::SetArrowVisibleWindowLimit(g_shownWindows);
    g_arrowCallback = BuildingHovers::SetArrowClickCallback(new GUI::Callback(OnSettings));
    BuildingHovers::ArrowAt((float)(g_root->x + g_settings->x), (float)(g_root->y + g_settings->y + g_settings->h / 2),
                            false, true, true, true, true, false, false);
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
        f.deinit = Deinit;
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
    g_scale = g_root->scale;
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
    g_offsetY = (int)((float)g_offsetY * g_scale);
    g_offsetX = (int)((float)g_offsetX * g_scale);
    for (FeaturedQuestHolder*& h : g_holders) {
        h = new FeaturedQuestHolder();
        h->root->SetVisibility(false);
        h->clickArea->SetOnClick(OnFeatured);
    }
    g_featuredIndex = 0;
}

// @0x2780f4
void Deinit() {
    for (FeaturedQuestHolder*& h : g_holders) {
        delete h;
        h = nullptr;
    }
    GUI::RemoveMovementEffect(g_effect);
    g_effect = nullptr;
    delete g_root;
    g_root = nullptr;
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

// @0x277b6c: slides out; the featured holders fade out at once.
void Hide() {
    if (!g_root) return;
    if (g_root->visibleSelf) {
        int W = GUI::ScreenWidth();
        g_effect->Animate((W + g_offsetX) - g_root->w, g_offsetY, W, g_offsetY, false);
    }
    g_featuredIndex = 0;
    for (FeaturedQuestHolder* h : g_holders) {
        h->root->SetVisibility(false);
        h->FadeOut();
    }
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
    // The featured holders just above the panel's depth, (50 * scale) right of and below its corner.
    const int d = (int)(g_scale * 50.f);
    for (FeaturedQuestHolder* h : g_holders) {
        h->root->SetZ(HUDWindow::GetZ() + 0.001f);
        h->root->SetPosition((g_offsetX + GUI::ScreenWidth() + d) - g_root->w, d);
    }
}

// @0x276ac4: the holders' glow spins for 5 seconds (every 300 s, on resume, back in the city).
void ActivateFeatureHolders() {
    for (FeaturedQuestHolder* h : g_holders) h->pulse = 5.f;
}

// @0x2769d0: how far the shown holder reaches left of its place (half its width, by its alpha while
// both are still).
int GetFeaturedOffset() {
    int shown;
    unsigned moving;
    if (g_holders[0]->active) {
        shown = 0;
        moving = g_holders[0]->fading + g_holders[1]->fading + 1 + g_holders[1]->active;
    } else {
        shown = g_holders[1]->active ? 1 : -1;
        moving = g_holders[0]->fading + g_holders[1]->fading + g_holders[1]->active;
    }
    if (shown == -1) return 0;
    Window* r = g_holders[shown]->root;
    const float f = moving == 0 ? r->alpha : 1.f;
    return (int)(f * (float)r->w * 0.5f);
}

// @0x278e40: the buttons' states, the tutorial's arrows at the compass (its tap is the compass's),
// the featured holders' fades, glow and timers, the compass blink and, once a second, the shop's
// state and the featured quests' turns.
void Update(float dt) {
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
    // PORT (user's bug report 2026-10-26): "To Map" (the game's own unused WORLD_NAV_TO_MAP) in place
    // of "Attack" (CONTROL_BATTLE_ATTACK), since the compass opens the world map.
    g_compassText->SetText(StringTable::GetString(toCastle ? "WORLD_NAV_TO_CASTLE" : "WORLD_NAV_TO_MAP"));
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
    if (!GameState::IsPlayerCity()) {
        g_featuredIndex = 0;
        g_shop->SetEnabled(false);
        for (FeaturedQuestHolder* h : g_holders) {
            h->root->SetVisibility(false);
            h->FadeOut();
        }
        return;
    }
    for (FeaturedQuestHolder* h : g_holders) {
        if (h->fading) {
            // Fading in at 1 per second, out at 3; gone at 0.
            float a = h->root->alpha;
            const float speed = h->targetAlpha == 0.f ? 3.f : 1.f;
            if (a < h->targetAlpha) a = std::min(a + speed * dt, h->targetAlpha);
            else if (a > h->targetAlpha) a = std::max(a - speed * dt, h->targetAlpha);
            h->root->SetAlpha(a, true);
            if (h->root->alpha == h->targetAlpha && h->targetAlpha == 0.f) {
                h->root->SetVisibility(false);
                h->fading = false;
            }
            // The glow spins while pulse lasts (at most 1 radian a second) and breathes with it.
            if (h->pulse > 0.f) {
                h->pulse -= dt;
                h->spin += std::min(h->pulse, 1.f) * dt;
            }
            h->glows[4]->SetAlpha(h->root->alpha + std::cos(h->spin * 8.f) * 0.2f, false);
            h->glows[4]->SetRotation(h->spin);
        }
        if (h->active) {
            Tasks::TaskInfo* t = Tasks::GetTask(h->task);
            h->shownTime += dt;
            if (t && t->HasTimeout()) {
                h->time->SetVisibility(true);
                int left = t->GetRemainingTime();
                // The starter chest's offer runs from the key's arrival (item 0x2b6).
                if (t->id == 0x75b && GameState::GetItemAmount(0x2b6, false) != 0)
                    left = Setting("starter_chest_time").GetInt() -
                           (int)((uint32_t)Timer::GetGlobalTime() - GameState::GetItemRecentTime(0x2b6));
                h->time->SetText(StringTable::GetNumericTimeString(left < 0 ? 0 : left, true).c_str());
            } else {
                h->time->SetVisibility(false);
            }
        }
    }
    // BlinkCompass's map: the compass pulses until the map changes.
    if ((int)GameState::GetCurrentMapID() == g_blinkMap) {
        if (g_blinkMap != 0) g_compass->SetAlpha(1.15f + std::cos((float)Timer::GetTime() * 8.f) * 0.15f, true);
    } else {
        g_compass->SetAlpha(1.f, true);
        g_blinkMap = 0;
    }
    if (Timer::GetGlobalTime() == g_lastSecond) return;
    g_lastSecond = Timer::GetGlobalTime();
    if ((uint32_t)g_lastSecond - g_lastActivate > 300) {
        g_lastActivate = (uint32_t)g_lastSecond;
        ActivateFeatureHolders();
    }
    g_shop->SetEnabled(!GameState::IsPvPCombatActive() && GameState::IsPlayerCity());
    const bool shopOn = GameState::TaskCompleted((unsigned)Setting("shop_unlock_task").GetInt());
    g_shopBg->SetEnabled(shopOn);
    g_shopIcon->SetEnabled(shopOn);
    g_shopText->SetEnabled(shopOn);

    // This second's featured quests: the promotions first (the pyramid exchange's sales, the chest
    // offers in the city view, the PvP and item-shop events), then every active "featured" quest.
    // The server starts the promotions; offline they never start (DECIDED 2026-10-20: ported as is).
    g_featured.clear();
    const bool farm = Map::GetCurrentFarm() != nullptr;
    if (Tasks::IsTaskActive(0x4d8, false)) g_featured.push_back(0x4d8);
    if (Tasks::IsTaskActive(0x4da, false)) g_featured.push_back(0x4da);
    if (Tasks::IsTaskActive(0x4db, true)) g_featured.push_back(0x4db);
    if (Tasks::IsTaskActive(0x6b4, true) && !farm && !GlobalMapWindow::IsVisible()) g_featured.push_back(0x6b4);
    if (Tasks::IsTaskActive(0x75b, false) && GameState::GetItemAmount(0x2b6, false) != 0 && !farm &&
        !GlobalMapWindow::IsVisible())
        g_featured.push_back(0x75b);
    if (Tasks::IsTaskActive(0x77b, true) && !farm) g_featured.push_back(0x77b);
    if (Tasks::IsTaskActive(0x791, false) && !farm) g_featured.push_back(0x791);
    for (unsigned i = 0; Tasks::TaskInfo* t = Tasks::EnumTask(i); ++i)
        if (t->featured && Tasks::IsTaskActive(t->id, t->dnShowDefault)) g_featured.push_back(t->id);
    // None during the second tutorial or a boss fight.
    if (GameState::secondTutorial != 0x100 || GameState::IsBossCombatActive()) g_featured.clear();
    const unsigned n = (unsigned)g_featured.size();

    // One holder shows a quest at a time; with several, the next takes over (the other holder fades
    // in) after 15 s; a quest that left the list makes way at once.
    int cur, other;
    if (g_holders[0]->active) {
        cur = 0;
        other = 1;
    } else if (g_holders[1]->active) {
        cur = 1;
        other = 0;
    } else {
        if (n != 0) {
            g_featuredIndex = 0;
            g_holders[0]->TakeQuest(g_featured[0]);
        }
        return;
    }
    if (n != 0) {
        bool found = false;
        for (uint32_t id : g_featured)
            if (id == g_holders[cur]->task) found = true;
        if (n > 1 && (g_holders[cur]->shownTime > 15.f || !found)) {
            if (++g_featuredIndex >= n) g_featuredIndex = 0;
            g_holders[cur]->FadeOut();
            g_holders[other]->TakeQuest(g_featured[g_featuredIndex]);
            return;
        }
        if (n != 1 || found) return;
    }
    g_featuredIndex = 0;
    g_holders[cur]->FadeOut();
}

}  // namespace CastleTopWindow
