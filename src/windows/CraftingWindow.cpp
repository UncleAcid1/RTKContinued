// CraftingWindow: the item improvement window (Window_crafting.xml): an item of a collection chain
// (Collections, type 0) and what improving it to the next one needs, in four tabs by item level.
// Port of CraftingWindow (libkingdom.so 5.11), 0x2a50ac..0x2a8c10 (static init FUN_002a512c). Statics at 0x61e0d8.
//
// Milestone 5o-3: the window opens and closes and its parts are laid out. STUBS (empty, no @0x so
// inventory still lists them): UpdateContents, OnCraft and OnBuy (5o-4, where the crystal prices
// are asked about first).
// UNVERIFIED (milestone 5q): OnFind's item shop (ItemShopWindow::Show / ShowShopItem).
#include <cstdio>

#include "engine/IconManager.h"
#include "game/BuildingHovers.h"
#include "game/Collections.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/Locations.h"
#include "game/Setting.h"
#include "game/StringTable.h"
#include "game/Tasks.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/HUD.h"
#include "hud/Shared.h"
#include "windows/Windows.h"

namespace CraftingWindow {
namespace {

using GUI::Textfield;
using GUI::Window;

// @0x2a7e54 CraftStatisticsHolder (0xa4 bytes): the item's four statistics before and after.
struct CraftStatisticsHolder {
    Window* root = nullptr;           // +0x00 "%s"
    Textfield* paramRed[4] = {};      // +0x04 "%s.text_param_%02d_red"
    Textfield* paramWhite[4] = {};    // +0x14 ..._white
    Textfield* paramGreen[4] = {};    // +0x24 ..._green
    // The original loads the same three names again for the next three arrays.
    Textfield* param2Red[4] = {};     // +0x34
    Textfield* param2White[4] = {};   // +0x44
    Textfield* param2Green[4] = {};   // +0x54
    Textfield* afterRed[4] = {};      // +0x64 "%s.text_stats_after_%02d_red"
    Textfield* afterWhite[4] = {};    // +0x74
    Textfield* afterGreen[4] = {};    // +0x84
    Window* icon[4] = {};             // +0x94 "%s.icon_stats_%02d" (take a depth slot)

    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        for (int i = 0; i < 4; ++i) paramRed[i] = GUI::GetWindowTypedF<Textfield>(w, "%s.text_param_%02d_red", p, i + 1);
        for (int i = 0; i < 4; ++i) paramWhite[i] = GUI::GetWindowTypedF<Textfield>(w, "%s.text_param_%02d_white", p, i + 1);
        for (int i = 0; i < 4; ++i) paramGreen[i] = GUI::GetWindowTypedF<Textfield>(w, "%s.text_param_%02d_green", p, i + 1);
        for (int i = 0; i < 4; ++i) param2Red[i] = GUI::GetWindowTypedF<Textfield>(w, "%s.text_param_%02d_red", p, i + 1);
        for (int i = 0; i < 4; ++i) param2White[i] = GUI::GetWindowTypedF<Textfield>(w, "%s.text_param_%02d_white", p, i + 1);
        for (int i = 0; i < 4; ++i) param2Green[i] = GUI::GetWindowTypedF<Textfield>(w, "%s.text_param_%02d_green", p, i + 1);
        for (int i = 0; i < 4; ++i) afterRed[i] = GUI::GetWindowTypedF<Textfield>(w, "%s.text_stats_after_%02d_red", p, i + 1);
        for (int i = 0; i < 4; ++i) afterWhite[i] = GUI::GetWindowTypedF<Textfield>(w, "%s.text_stats_after_%02d_white", p, i + 1);
        for (int i = 0; i < 4; ++i) afterGreen[i] = GUI::GetWindowTypedF<Textfield>(w, "%s.text_stats_after_%02d_green", p, i + 1);
        for (int i = 0; i < 4; ++i) {
            icon[i] = GUI::GetWindowTypedF<Window>(w, "%s.icon_stats_%02d", p, i + 1);
            icon[i]->takesZ = true;
        }
    }
};

// @0x2a80b8 CraftTabInfo (0x10 bytes): a tab and its icon with their laid-out y.
struct CraftTabInfo {
    Window* root = nullptr;           // +0x00 "%s"
    int y = 0;                        // +0x04 its y
    Window* icon = nullptr;           // +0x08 "%s.icon_place_holder" (takes a depth slot)
    int iconY = 0;                    // +0x0c its y

    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        y = root->y;
        icon = GUI::GetWindowTypedF<Window>(w, "%s.icon_place_holder", p);
        icon->takesZ = true;
        iconY = icon->y;
    }
};

// @0x2a811c CraftItemHolder (0x84 bytes): an item the improvement makes or needs.
struct CraftItemHolder {
    Window* root = nullptr;           // +0x00 "%s"
    Window* noGrade = nullptr;        // +0x04 "%s.craft_backdrop_element_bg.craft_backdrop_no_grade"
    Window* complete = nullptr;       // +0x08 ..._complete
    Window* grey = nullptr;           // +0x0c ..._grey
    Window* green = nullptr;          // +0x10 ..._green
    Window* blue = nullptr;           // +0x14 ..._blue
    Window* orange = nullptr;         // +0x18 ..._orange
    Window* purple = nullptr;         // +0x1c ..._purple
    Textfield* name[5] = {};          // +0x20 "%s.crafting_item_name_gem.text_item_class_%02d"
    Window* gem = nullptr;            // +0x34 "%s.icon_gem" (takes a depth slot)
    Window* goldPlus = nullptr;       // +0x38 icon_gold_plus
    Window* owned = nullptr;          // +0x3c icon_owned
    Window* findTiny = nullptr;       // +0x40 icon_find_tiny
    Textfield* textFind = nullptr;    // +0x44 text_find
    Textfield* count = nullptr;       // +0x48 text_count
    Textfield* countComplete = nullptr;   // +0x4c text_count_complete
    Shared::ButtonCraftBuyNow buyNow; // +0x50 button_craft_buy_now
    Window* find = nullptr;           // +0x70 button_crafting_find
    Textfield* findText = nullptr;    // +0x74 button_crafting_find.text
    Window* crafted = nullptr;        // +0x78 item_crafted_found
    Window* locked = nullptr;         // +0x7c locked
    Textfield* levelReq = nullptr;    // +0x80 locked.text_level_req

    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        noGrade = GUI::GetWindowTypedF<Window>(w, "%s.craft_backdrop_element_bg.craft_backdrop_no_grade", p);
        complete = GUI::GetWindowTypedF<Window>(w, "%s.craft_backdrop_element_bg.craft_backdrop_complete", p);
        grey = GUI::GetWindowTypedF<Window>(w, "%s.craft_backdrop_element_bg.craft_backdrop_grey", p);
        green = GUI::GetWindowTypedF<Window>(w, "%s.craft_backdrop_element_bg.craft_backdrop_green", p);
        blue = GUI::GetWindowTypedF<Window>(w, "%s.craft_backdrop_element_bg.craft_backdrop_blue", p);
        orange = GUI::GetWindowTypedF<Window>(w, "%s.craft_backdrop_element_bg.craft_backdrop_orange", p);
        purple = GUI::GetWindowTypedF<Window>(w, "%s.craft_backdrop_element_bg.craft_backdrop_purple", p);
        for (int i = 0; i < 5; ++i)
            name[i] = GUI::GetWindowTypedF<Textfield>(w, "%s.crafting_item_name_gem.text_item_class_%02d", p, i + 1);
        gem = GUI::GetWindowTypedF<Window>(w, "%s.icon_gem", p);
        gem->takesZ = true;
        goldPlus = GUI::GetWindowTypedF<Window>(w, "%s.icon_gold_plus", p);
        owned = GUI::GetWindowTypedF<Window>(w, "%s.icon_owned", p);
        findTiny = GUI::GetWindowTypedF<Window>(w, "%s.icon_find_tiny", p);
        textFind = GUI::GetWindowTypedF<Textfield>(w, "%s.text_find", p);
        count = GUI::GetWindowTypedF<Textfield>(w, "%s.text_count", p);
        countComplete = GUI::GetWindowTypedF<Textfield>(w, "%s.text_count_complete", p);
        char buy[0x100];
        std::snprintf(buy, sizeof buy, "%s.button_craft_buy_now", p);
        buyNow.LoadFrom(w, buy);
        find = GUI::GetWindowTypedF<Window>(w, "%s.button_crafting_find", p);
        findText = GUI::GetWindowTypedF<Textfield>(w, "%s.button_crafting_find.text", p);
        crafted = GUI::GetWindowTypedF<Window>(w, "%s.item_crafted_found", p);
        locked = GUI::GetWindowTypedF<Window>(w, "%s.locked", p);
        levelReq = GUI::GetWindowTypedF<Textfield>(w, "%s.locked.text_level_req", p);
    }
};

WindowManager::FunctionalWindow* g_queue = nullptr;   // CraftingWindow::wnd 0x61e570
Collections::Collection* g_collection = nullptr;      // 0x61e0d8 the chain the window shows (SetParameters)
bool g_help = false;                          // 0x61e0dc the improve help: the arrow at the Craft button
CraftTabInfo g_tabs[4];                       // 0x61e0e0
Textfield* g_before[5] = {};                  // 0x61e120 "crafting_item_name_before.text_item_class_%02d"
Textfield* g_after[5] = {};                   // 0x61e134 "crafting_item_name_after.text_item_class_%02d"
CraftItemHolder g_from;                       // 0x61e148 craft_required_item_01: the item as it is
CraftItemHolder g_parts[3];                   // 0x61e1cc "crafting_required_items_holder.crafting_req_items_3.craft_required_item_%02d"
CraftItemHolder g_to;                         // 0x61e358 craft_required_item_02: the result
CraftStatisticsHolder g_statsFrom;            // 0x61e3dc craft_item_stats_01
CraftStatisticsHolder g_statsTo;              // 0x61e480 craft_item_stats_02
Window* g_craft = nullptr;                    // 0x61e524 button_crafting_craft
Window* g_root = nullptr;                     // 0x61e528 Window_crafting.xml
Textfield* g_title = nullptr;                 // 0x61e52c text_title
unsigned g_tabLevel = 0;                      // 0x61e530 the tab (item level) shown
Collections::Collection* g_current = nullptr; // 0x61e534 the step of the chain shown
Textfield* g_craftText = nullptr;             // 0x61e53c button_crafting_craft.text
Window* g_craftedNotice = nullptr;            // 0x61e540 item_crafted_notice
Window* g_reqNotice = nullptr;                // 0x61e544 item_craft_req_notice
Window* g_reqIcon = nullptr;                  // 0x61e548 ...icon_req (takes a depth slot)
Textfield* g_reqDesc = nullptr;               // 0x61e54c ...text_desc
Textfield* g_textBefore = nullptr;            // 0x61e550 text_before
Textfield* g_textAfter = nullptr;             // 0x61e554 text_after
GameState::PlayerItem* g_item = nullptr;      // 0x61e558 the player's item (SelectTabFor)
GameState::PlayerItem* g_crafted = nullptr;   // 0x61e55c the item just made (shown by CraftSuccessWindow)
bool g_crafting = false;                      // 0x61e560 the second after Craft
float g_craftTime = 0.f;                      // 0x61e564
GUI::TweenEffect* g_tween = nullptr;          // 0x61e568
GUI::Button* g_close = nullptr;               // 0x61e56c x_button.clickArea
bool g_tablet = false;                        // 0x60f200
float g_scale = 1.f;                          // 0x60f1fc

// @0x2a529c
void OnTweenOut() {
    WindowManager::WindowHide(false);
    g_queue->shown = false;
    g_queue->MoveWindowDown(true);
    GameState::DropGamePauseState();
}

// STUBS (5o-4).
void UpdateContents() {}
void OnCraft() {}                  // 0x2a71d8
void OnBuy(unsigned, bool) {}      // 0x2a7804

// @0x2a5314: with the improve help (HelpImprove) the arrow points at the Craft button.
void OnTweenIn() {
    if (g_help) {
        BuildingHovers::SetArrowVisibleWindowLimit(2);
        BuildingHovers::ArrowAt((float)(g_root->x + g_craft->x + g_craft->w / 2), (float)(g_root->y + g_craft->y), false,
                                false, false, false, true, false, false);
        BuildingHovers::SetArrowClickCallback(new GUI::Callback(OnCraft));   // FUN_002a52d0
    }
    g_help = false;
}

// @0x2a717c: the result's item level is not reached: what is missing.
void OnResult() {
    if (!g_current) return;
    Items::ItemInfo* info = Items::GetItemInfo(g_current->to);
    if (!info) return;
    if (info->IsLockedByReputation(false)) return;
    NotEnoughWindow::ResetRequirements();
    NotEnoughWindow::AddLevelRequirement(info->level);
    if (NotEnoughWindow::CheckRequirements()) return;
    NotEnoughWindow::Show();
}

// @0x2a7ae8: where to get the item of the chain (param 0) or of part `param`: a drop on the map
// (the camera goes to it and the arrow points at it), the mission that gives it, or the item shop.
void OnFind(unsigned param) {
    if (GameState::IsBossCombatActive() || GameState::IsArenaCombatActive()) {
        PopupWindow::Show(StringTable::GetString("CAN_NOT_TRAVEL"), PopupWindow::Hide, nullptr, nullptr);
        return;
    }
    if (!g_current) return;
    uint32_t item;
    if (param == 0) {
        if (g_from.owned->visibleSelf) return;
        item = g_current->from;
    } else {
        if (g_parts[param - 1].crafted->visibleSelf) return;
        item = g_current->parts[param - 1].item;
    }
    Items::ItemInfo* info = Items::GetItemInfo(item);
    if (!info) return;
    float x, y;
    if (BuildingHovers::HasDroppedItem(info->id, x, y)) {
        Hide();
        CharacterInfoWindow::Hide();
        BuildingHovers::SetArrowClickCallback(new GUI::Callback(BuildingHovers::CollectAll));
        Tasks::HelpWithLocation((int)x, (int)y, 0, 0);
        return;
    }
    if (uint32_t map = GlobalMapWindow::GetMissionMapForItem(info->id, 0)) {
        Hide();
        CharacterInfoWindow::Hide();
        GlobalMapWindow::Show();
        GlobalMapWindow::CenterOnMap(map);
        if (Locations::LocationInfo* location = Locations::GetLocationWithMap(map)) {
            RaidInfoWindow::SetParameters(location, map, false);
            RaidInfoWindow::Show();
            RaidInfoWindow::SetAnimationOnce();
            RaidInfoWindow::ShowItem(info->id);
        }
        return;
    }
    // UNVERIFIED (milestone 5q): with ItemShopWindow::ItemCanBeBought(info->id, 0) and the task
    // "shop_unlock_task" done: Hide(), ItemShopWindow::Show() and ShowShopItem(info->id, 0, true, -1).
    std::printf("ERROR: CraftingWindow::OnFind() Cannot find a place with item %d\n", (int)info->id);
}

// @0x2a7a38: a second after Craft the new item is shown and the window closes.
void Update(float dt) {
    if (!g_crafting) return;
    g_craftTime += dt;
    if (g_craftTime <= 1.f) return;
    g_crafting = false;
    GUI::SetInteractionLock(false);
    if (g_crafted) {
        CraftSuccessWindow::SetParameters(g_crafted);
        CraftSuccessWindow::Show();
    }
    g_crafted = nullptr;
    Hide();
}

// @0x2a7ac0
bool OnBack() {
    if (!g_queue->shown) return false;
    Hide();
    return true;
}

// @0x2a7ce0
void SetZ(float z) {
    if (!g_queue->shown) return;
    g_root->SetZ(z);
    g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::GetVerticalCenter(g_root->h));
}

// @0x2a7d68
bool Click(int x, int y, bool pressed) {
    if (g_root->visibleSelf) {
        if (BuildingHovers::ClickOnArrow(x, y, pressed)) return true;
        if (!pressed) BuildingHovers::HideArrow();
    }
    if (TopCityWindow::Click(x, y, pressed)) return true;
    const bool visible = g_root->visibleSelf;
    g_root->Click(x, y, pressed, false);
    return visible;
}

// @0x2a7e14
void Deinit() {
    GUI::RemoveTweenEffect(g_tween);
    g_tween = nullptr;
    delete g_root;
    g_root = nullptr;
}

// @0x2a50d4
float ZRange() { return g_queue->shown ? 0.028f : 0.001f; }

}  // namespace

WindowManager::FunctionalWindow* Queue() {
    if (!g_queue) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.deinit = Deinit;
        f.click = Click;
        f.setZ = SetZ;
        f.hide = Hide;
        g_queue = new WindowManager::FunctionalWindow("CraftingWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_queue->shown; }   // @0x2a50ac

// @0x2a50c0: the chain (its first step) the window shows.
void SetParameters(Collections::Collection* collection) { g_collection = collection; }

// @0x2a5104: the next opening points at the Craft button.
void HelpImprove() { g_help = true; }

// @0x2a70a4: the tab of an item level shows the first step of the chain that starts at that level.
void OnTab(unsigned level) {
    g_tabLevel = level;
    Collections::Collection* c = g_collection;
    while (c) {
        Items::ItemInfo* info = Items::GetItemInfo(c->from);
        if (info && info->itemLevel == level) break;
        c = c->next;
    }
    g_current = c;
    UpdateContents();
}

// @0x2a7108: the tab and step of `info` (an item of the chain), with the player's item `data`.
void SelectTabFor(GameState::PlayerItem* data, Items::ItemInfo* info) {
    if (!g_collection || !info) return;
    Collections::Collection* c = g_collection;
    while (c && c->from != info->id) c = c->next;
    if (c) {
        g_current = c;
        g_tabLevel = info->itemLevel;
    }
    g_item = data;
    UpdateContents();
}

// @0x2a797c
void Show() {
    g_queue->shown = true;
    g_queue->MoveWindowOnTop(true);
    if (!g_root->visibleSelf) {
        WindowManager::WindowShow(false);
        g_tween->AnimateIn();
        if (GameState::GetCurrentLocation() == 2) GameState::RaiseGamePauseState();
    }
    g_root->SetVisibility(true);
    UpdateContents();
    g_root->PreloadTextures();   // GUI::PreloadWindow
}

// @0x2a7a14
void Hide() {
    if (!g_root->visibleSelf) return;
    g_tween->AnimateOut();
}

// @0x2a8408
void Init() {
    g_help = false;
    g_tablet = GUI::IsTabletVersion();
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Window_crafting.xml", "Window_crafting.png", -1.f, 0, 0, 0, 0,
                             false, 1.f);
    g_queue->RegisterTopWindow(g_root);
    g_scale = g_root->scale;
    g_root->SetVisibility(false);
    g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::GetVerticalCenter(g_root->h));
    g_tween = GUI::CreateTweenEffect(g_root, nullptr, OnTweenIn, OnTweenOut);
    g_close = GUI::GetWindowTyped<GUI::Button>(g_root, "x_button.clickArea");
    g_close->SetOnClick(Hide);
    g_close->SetActionSound("ui_close", 1);
    for (int i = 0; i < 4; ++i) {
        char name[0x80];
        std::snprintf(name, sizeof name, "tab_%02d", i + 1);
        g_tabs[i].LoadFrom(g_root, name);
        g_tabs[i].root->SetOnClick([i] { OnTab((unsigned)i); });
        g_tabs[i].root->SetActionSound("ui_tab", 1);
        std::snprintf(name, sizeof name, "upgrade_%02d", i + 1);
        g_tabs[i].icon->SetTexture(IconManager::GetIcon(name, false), true);
    }
    g_title = GUI::GetWindowTyped<Textfield>(g_root, "text_title");
    for (int i = 0; i < 5; ++i)
        g_before[i] = GUI::GetWindowTypedF<Textfield>(g_root, "crafting_item_name_before.text_item_class_%02d", i + 1);
    for (int i = 0; i < 5; ++i)
        g_after[i] = GUI::GetWindowTypedF<Textfield>(g_root, "crafting_item_name_after.text_item_class_%02d", i + 1);
    g_from.LoadFrom(g_root, "craft_required_item_01");
    g_from.gem->SetOnClick([] { OnFind(0); });
    for (int i = 0; i < 3; ++i) {
        char name[0x100];
        std::snprintf(name, sizeof name,
                      "crafting_required_items_holder.crafting_req_items_3.craft_required_item_%02d", i + 1);
        g_parts[i].LoadFrom(g_root, name);
        g_parts[i].gem->SetOnClick([i] { OnFind((unsigned)i + 1); });
        g_parts[i].buyNow.root->SetOnClick([i] { OnBuy((unsigned)i + 1, false); });
    }
    g_to.LoadFrom(g_root, "craft_required_item_02");
    g_to.root->SetOnClick(OnResult);
    g_craft = GUI::GetWindowTyped<Window>(g_root, "button_crafting_craft");
    g_craft->SetOnClick(OnCraft);
    g_craftText = GUI::GetWindowTyped<Textfield>(g_root, "button_crafting_craft.text");
    g_craftedNotice = GUI::GetWindowTyped<Window>(g_root, "item_crafted_notice");
    g_reqNotice = GUI::GetWindowTyped<Window>(g_root, "item_craft_req_notice");
    g_reqIcon = GUI::GetWindowTyped<Window>(g_root, "item_craft_req_notice.icon_req");
    g_reqIcon->takesZ = true;
    g_reqDesc = GUI::GetWindowTyped<Textfield>(g_root, "item_craft_req_notice.text_desc");
    g_textBefore = GUI::GetWindowTyped<Textfield>(g_root, "text_before");
    g_textAfter = GUI::GetWindowTyped<Textfield>(g_root, "text_after");
    g_statsFrom.LoadFrom(g_root, "craft_item_stats_01");
    g_statsTo.LoadFrom(g_root, "craft_item_stats_02");
    g_queue->hideOnOuterClick = true;
    g_queue->fn.update = Update;
    g_queue->fn.zRange = ZRange;
    g_queue->fn.back = OnBack;
    g_queue->fn.show = Show;
}

}  // namespace CraftingWindow
