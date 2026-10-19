// CraftItemWindow: the crafting window (Window_item_craft.xml), opened by the belt's Crafting button.
// Tab 0 lists the professions (with their levels and the recipes each level unlocks); tabs 1..5 list
// the recipes (Collections of type 1) of a profession, with what they need and make.
// Port of CraftItemWindow (libkingdom.so 5.11), 0x29c190..0x2a38fc. Statics at 0x61c8d8.
//
// PORT (user 2026-10-19): the crystal "Buy now" buttons of the missing parts are not offered (as in
// CraftingWindow); OnBuy does nothing.
// Milestone 5o-5: the window opens and closes and its parts are laid out. STUBS (empty, no @0x so
// inventory still lists them): UpdateContents (5o-6), OnCraft, OnFind (5o-7).
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "engine/IconManager.h"
#include "engine/Render.h"
#include "engine/StlSort.h"
#include "engine/Text.h"
#include "game/BuildingHovers.h"
#include "game/Collections.h"
#include "game/EntityManager.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/MetaExpression.h"
#include "game/Professions.h"
#include "game/Setting.h"
#include "game/StringTable.h"
#include "game/Tasks.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/ContentScroller.h"
#include "hud/HUD.h"
#include "hud/ItemHoverWindow.h"
#include "hud/Shared.h"
#include "windows/Windows.h"

namespace CraftItemWindow {
namespace {

using GUI::Textfield;
using GUI::Window;

void UpdateContents();
void SelectListItem(unsigned index);
void OnFind(unsigned part);
void OnBuy(unsigned part, bool confirmed);
void OnCraft();
void OnTweenIn();
void OnLockedFlag();
void OnResult();
void OnImage(unsigned part);
void OnProfessionLevelItem(unsigned node, unsigned slot);
void ScrollProfessionLevelIntoView(unsigned level);

// @0x2a0b78 ItemInfo (0x48 bytes): one item slot (Inventory_item_icon).
struct ItemInfo {
    Window* root = nullptr;           // +0x00 "%s"
    Window* selected = nullptr;       // +0x04 selected_item
    Window* locked = nullptr;         // +0x08 inventory_item_icon_holder.item_class_locked
    Window* itemClass[5] = {};        // +0x0c ..item_class_01..05
    Window* icon = nullptr;           // +0x20 ..item_icon_holder_item (takes a depth slot)
    Window* lock = nullptr;           // +0x24 ..lock
    Textfield* lockLevel = nullptr;   // +0x28 ..lock.text_level_req
    Window* redX = nullptr;           // +0x2c ..icon_red_x (icon "red_x")
    Window* goldOwned = nullptr;      // +0x30 ..icon_gold_owned (icon "gold_confirm")
    Window* recommended = nullptr;    // +0x34 ..recomended_item
    Window* lockTiny = nullptr;       // +0x38 icon_lock_tiny (takes a depth slot)
    Textfield* quantity = nullptr;    // +0x3c text_quantity
    Textfield* quantityRed = nullptr; // +0x40 text_quantity_red
    int item = 0;                     // +0x44

    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        selected = GUI::GetWindowTypedF<Window>(w, "%s.selected_item", p);
        locked = GUI::GetWindowTypedF<Window>(w, "%s.inventory_item_icon_holder.item_class_locked", p);
        for (int i = 0; i != 5; ++i)
            itemClass[i] = GUI::GetWindowTypedF<Window>(w, "%s.inventory_item_icon_holder.item_class_%02d", p, i + 1);
        icon = GUI::GetWindowTypedF<Window>(w, "%s.inventory_item_icon_holder.item_icon_holder_item", p);
        icon->takesZ = true;
        lock = GUI::GetWindowTypedF<Window>(w, "%s.inventory_item_icon_holder.lock", p);
        lockLevel = GUI::GetWindowTypedF<Textfield>(w, "%s.inventory_item_icon_holder.lock.text_level_req", p);
        redX = GUI::GetWindowTypedF<Window>(w, "%s.inventory_item_icon_holder.icon_red_x", p);
        redX->SetTexture(IconManager::GetIcon("red_x", false));
        goldOwned = GUI::GetWindowTypedF<Window>(w, "%s.inventory_item_icon_holder.icon_gold_owned", p);
        goldOwned->SetTexture(IconManager::GetIcon("gold_confirm", false));
        recommended = GUI::GetWindowTypedF<Window>(w, "%s.inventory_item_icon_holder.recomended_item", p);
        lockTiny = GUI::GetWindowTypedF<Window>(w, "%s.icon_lock_tiny", p);
        lockTiny->takesZ = true;
        quantity = GUI::GetWindowTypedF<Textfield>(w, "%s.text_quantity", p);
        quantityRed = GUI::GetWindowTypedF<Textfield>(w, "%s.text_quantity_red", p);
        item = 0;
    }
};

// @0x2a0a94 ProfessionInfo (0x1c bytes): a profession in the list of tab 0.
struct ProfessionInfo {
    Window* root = nullptr;           // +0x00 "%s"
    Window* selectedBg = nullptr;     // +0x04 selected_item_bg_9s
    Window* slotStatic = nullptr;     // +0x08 slot_static_bg
    Window* slotLocked = nullptr;     // +0x0c slot_locked_bg
    Window* active = nullptr;         // +0x10 profession_active (takes a depth slot)
    Textfield* name = nullptr;        // +0x14 text_item_class_01
    Window* lock = nullptr;           // +0x18 profession_lock (takes a depth slot)

    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        selectedBg = GUI::GetWindowTypedF<Window>(w, "%s.selected_item_bg_9s", p);
        slotStatic = GUI::GetWindowTypedF<Window>(w, "%s.slot_static_bg", p);
        slotLocked = GUI::GetWindowTypedF<Window>(w, "%s.slot_locked_bg", p);
        active = GUI::GetWindowTypedF<Window>(w, "%s.profession_active", p);
        active->takesZ = true;
        name = GUI::GetWindowTypedF<Textfield>(w, "%s.text_item_class_01", p);
        lock = GUI::GetWindowTypedF<Window>(w, "%s.profession_lock", p);
        lock->takesZ = true;
    }
};

// @0x2a0f44 CraftItemInfo (0x6c bytes): a recipe in the list.
struct CraftItemInfo {
    Window* root = nullptr;           // +0x00 "%s"
    Window* selectedBg = nullptr;     // +0x04 selected_item_bg_9s
    Window* slotStatic = nullptr;     // +0x08 slot_static_bg
    Window* slotLocked = nullptr;     // +0x0c slot_locked_bg
    Textfield* name[5] = {};          // +0x10 texts_shop.text_item_class_01..05
    ItemInfo item;                    // +0x24 inventory_slot_holder_01.inventory_item_icon

    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        selectedBg = GUI::GetWindowTypedF<Window>(w, "%s.selected_item_bg_9s", p);
        slotStatic = GUI::GetWindowTypedF<Window>(w, "%s.slot_static_bg", p);
        slotLocked = GUI::GetWindowTypedF<Window>(w, "%s.slot_locked_bg", p);
        for (int i = 0; i != 5; ++i)
            name[i] = GUI::GetWindowTypedF<Textfield>(w, "%s.texts_shop.text_item_class_%02d", p, i + 1);
        char sub[0x100];
        std::snprintf(sub, sizeof sub, "%s.inventory_slot_holder_01.inventory_item_icon", p);
        item.LoadFrom(w, sub);
    }
};

// @0x2a1068 RequiredItemInfo (0x7c bytes): an item the recipe needs (or makes).
struct RequiredItemInfo {
    Window* root = nullptr;           // +0x00 "%s"
    ItemInfo item;                    // +0x04 inventory_slot_holder_01.inventory_item_icon
    Shared::ButtonCraftBuyNow buyNow; // +0x4c button_craft_buy_now
    Window* find = nullptr;           // +0x6c button_crafting_find
    Textfield* findText = nullptr;    // +0x70 button_crafting_find.text
    Window* crafted = nullptr;        // +0x74 item_crafted_found
    Window* confirm = nullptr;        // +0x78 item_crafted_found.confim (icon "gold_confirm")

    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        char sub[0x100];
        std::snprintf(sub, sizeof sub, "%s.inventory_slot_holder_01.inventory_item_icon", p);
        item.LoadFrom(w, sub);
        std::snprintf(sub, sizeof sub, "%s.button_craft_buy_now", p);
        buyNow.LoadFrom(w, sub);
        find = GUI::GetWindowTypedF<Window>(w, "%s.button_crafting_find", p);
        findText = GUI::GetWindowTypedF<Textfield>(w, "%s.button_crafting_find.text", p);
        crafted = GUI::GetWindowTypedF<Window>(w, "%s.item_crafted_found", p);
        confirm = GUI::GetWindowTypedF<Window>(w, "%s.item_crafted_found.confim", p);
        confirm->SetTexture(IconManager::GetIcon("gold_confirm", false));
    }
};

// @0x2a0db4 LockedCraftInfo (0xf8 bytes): the card of a recipe whose profession level is not
// reached: the level, the items that level unlocks and what is still required.
struct LockedCraftInfo {
    Window* root = nullptr;           // +0x00 "%s"
    Window* flagDisabled = nullptr;   // +0x04 profession_flag_disabled
    Textfield* noItem = nullptr;      // +0x08 text_no_item
    Textfield* day1 = nullptr;        // +0x0c text_day_01
    Textfield* day2 = nullptr;        // +0x10 text_day_02
    Textfield* achieved = nullptr;    // +0x14 profession_achieved
    Window* icon = nullptr;           // +0x18 icon_profession (takes a depth slot)
    ItemInfo items[3];                // +0x1c profession_unlocked_holder_3_item.inventory_slot_holder_NN.inventory_item_icon
    Textfield* required = nullptr;    // +0xf4 required_items

    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        flagDisabled = GUI::GetWindowTypedF<Window>(w, "%s.profession_flag_disabled", p);
        noItem = GUI::GetWindowTypedF<Textfield>(w, "%s.text_no_item", p);
        day1 = GUI::GetWindowTypedF<Textfield>(w, "%s.text_day_01", p);
        day2 = GUI::GetWindowTypedF<Textfield>(w, "%s.text_day_02", p);
        achieved = GUI::GetWindowTypedF<Textfield>(w, "%s.profession_achieved", p);
        icon = GUI::GetWindowTypedF<Window>(w, "%s.icon_profession", p);
        icon->takesZ = true;
        for (int i = 0; i != 3; ++i) {
            char sub[0x100];
            std::snprintf(sub, sizeof sub,
                          "%s.profession_unlocked_holder_3_item.inventory_slot_holder_%02d.inventory_item_icon", p, i + 1);
            items[i].LoadFrom(w, sub);
        }
        required = GUI::GetWindowTypedF<Textfield>(w, "%s.required_items", p);
    }
};

// Shared::EffectDescriptionLine (0x18 bytes), LoadFrom @0x343cd0: one effect line of the item's card.
struct EffectDescriptionLine {
    Window* root = nullptr;           // +0x00 "%s"
    Textfield* text = nullptr;        // +0x04 text_desc
    Textfield* broken = nullptr;      // +0x08 text_desc_broken
    Textfield* green = nullptr;       // +0x0c text_desc_green
    Window* icon = nullptr;           // +0x10 icon (takes a depth slot)
    int f14 = 0;                      // +0x14

    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        text = GUI::GetWindowTypedF<Textfield>(w, "%s.text_desc", p);
        broken = GUI::GetWindowTypedF<Textfield>(w, "%s.text_desc_broken", p);
        green = GUI::GetWindowTypedF<Textfield>(w, "%s.text_desc_green", p);
        icon = GUI::GetWindowTypedF<Window>(w, "%s.icon", p);
        icon->takesZ = true;
        f14 = 0;
    }
};

void OnSkillLevelHolderDown();
void OnSkillLevelHolderUp();

// @0x2a180c ProfessionSkillInfo (0x3dc bytes): one profession level of the profession tab's
// column: its flag, days, progress bar and the (up to eight) items it unlocks in a sideways strip.
struct ProfessionSkillInfo {
    Window* root = nullptr;           // +0x00 "%s"
    Window* flag = nullptr;           // +0x04 profession_flag
    Textfield* classActive = nullptr; // +0x08 profession_class_active
    Textfield* day1 = nullptr;        // +0x0c text_day_01
    Textfield* day2 = nullptr;        // +0x10 text_day_02
    Window* icon = nullptr;           // +0x14 icon_profession (takes a depth slot)
    Textfield* achieved = nullptr;    // +0x18 profession_achieved
    Window* checkbox = nullptr;       // +0x1c profession_achieved_checkbox
    Shared::ProgressBarLarge bar;     // +0x20 profession_progress_bar
    Textfield* explained = nullptr;   // +0x44 profession_explained
    Window* slots[8] = {};            // +0x48 prof_unl_h.prof_item_h.inventory_slot_holder_NN
    ItemInfo items[8];                // +0x68 ...inventory_slot_holder_NN.inventory_item_icon
    Window* arrowLeft = nullptr;      // +0x2a8 prof_unl_h.arrow_left
    Window* arrowRight = nullptr;     // +0x2ac prof_unl_h.arrow_right
    Shared::ContentScroller scroller; // +0x2c0 the strip (scrollbar: ...scroll_bar_profession_items_horisontal.scroll_bar_tiny)
    Window* mask = nullptr;           // +0x340 prof_unl_h.prof_item_h.mask_universal
    GUI::ClipRect clip{0, 0, 0, 0};   // +0x344 the mask's rectangle
    // (the profession level's recipes sit in a sgl::vector at +0x2b0, the strip's shifts at +0x3d0)
    std::vector<Collections::Collection*> recipes;   // the recipes this level unlocks
    int shiftX[2] = {0, 0};           // +0x3d0 +0x3d4 how far the even / odd slots were moved to centre the strip
    int shiftY[2] = {0, 0};           // +0x3d8 +0x3dc

    void LoadFrom(Window* w, const char* p) {
        scroller.Init();
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        flag = GUI::GetWindowTypedF<Window>(w, "%s.profession_flag", p);
        classActive = GUI::GetWindowTypedF<Textfield>(w, "%s.profession_class_active", p);
        day1 = GUI::GetWindowTypedF<Textfield>(w, "%s.text_day_01", p);
        day2 = GUI::GetWindowTypedF<Textfield>(w, "%s.text_day_02", p);
        icon = GUI::GetWindowTypedF<Window>(w, "%s.icon_profession", p);
        icon->takesZ = true;
        achieved = GUI::GetWindowTypedF<Textfield>(w, "%s.profession_achieved", p);
        checkbox = GUI::GetWindowTypedF<Window>(w, "%s.profession_achieved_checkbox", p);
        char sub[0x100];
        std::snprintf(sub, sizeof sub, "%s.profession_progress_bar", p);
        bar.LoadFrom(w, sub);
        explained = GUI::GetWindowTypedF<Textfield>(w, "%s.profession_explained", p);
        // The slots are laid out in two rows: the strip takes them in this order.
        static const unsigned kOrder[8] = {1, 5, 2, 6, 3, 7, 4, 8};   // 0x5941bc
        for (int i = 0; i != 8; ++i) {
            slots[i] = GUI::GetWindowTypedF<Window>(w, "%s.prof_unl_h.prof_item_h.inventory_slot_holder_%02d", p, kOrder[i]);
            std::snprintf(sub, sizeof sub, "%s.prof_unl_h.prof_item_h.inventory_slot_holder_%02d.inventory_item_icon", p,
                          kOrder[i]);
            items[i].LoadFrom(w, sub);
        }
        // The last two items start one column further right.
        items[6].root->MoveWindow(items[2].root->x - items[0].root->x, 0);   // 0x218
        items[7].root->MoveWindow(items[2].root->x - items[0].root->x, 0);   // 0x260
        arrowLeft = GUI::GetWindowTypedF<Window>(w, "%s.prof_unl_h.arrow_left", p);
        arrowRight = GUI::GetWindowTypedF<Window>(w, "%s.prof_unl_h.arrow_right", p);
        mask = GUI::GetWindowTypedF<Window>(w, "%s.prof_unl_h.prof_item_h.mask_universal", p);
        clip = {mask->x, mask->y, mask->x + mask->w, mask->y + mask->h};
        for (ItemInfo& it : items) {
            it.root->SetClipRect(&clip);
            scroller.windows.push_back(it.root);
        }
        scroller.scrollbar = GUI::GetWindowTypedF<Window>(
            w, "%s.prof_unl_h.scroll_bar_profession_items_horisontal.scroll_bar_tiny", p);
        scroller.itemSize = items[2].root->x - items[0].root->x;   // two items (rows) per column
        scroller.horizontal = true;
        scroller.unkA0 = 8;
        scroller.perLine = 2;
        scroller.onScroll = nullptr;
        scroller.onNext = nullptr;
        scroller.onPrev = nullptr;
        scroller.onScroll = UpdateContents;
    }
};

// @0x2a1c58 ProfessionSkillZone (0xc60 bytes): the profession tab's column of three levels.
struct ProfessionSkillZone {
    Window* root = nullptr;           // +0x00 "%s"
    ProfessionSkillInfo node[3];      // +0x04 node_NN
    Shared::ContentScroller scroller; // +0xba8 the column (scrollbar: scroll_bar_professions.scroll_bar_tiny)
    Window* mask = nullptr;           // +0xc28 mask_universal
    GUI::ClipRect clip{0, 0, 0, 0};   // +0xc2c

    void LoadFrom(Window* w, const char* p) {
        scroller.Init();
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        for (int i = 0; i != 3; ++i) {
            char sub[0x100];
            std::snprintf(sub, sizeof sub, "%s.node_%02d", p, i + 1);
            node[i].LoadFrom(w, sub);
        }
        // The second and third level hang below the first one.
        const int dy = node[0].root->h - (int)(w->scale * 20.f);
        node[1].root->MoveWindow(0, dy);
        node[1].clip.top += dy;
        node[1].clip.bottom += dy;
        node[2].root->MoveWindow(0, dy * 2);
        node[2].clip.top += dy * 2;
        node[2].clip.bottom += dy * 2;
        mask = GUI::GetWindowTypedF<Window>(w, "%s.mask_universal", p);
        clip = {mask->x, mask->y, mask->x + mask->w, mask->y + mask->h};
        for (ProfessionSkillInfo& n : node) {
            n.root->SetClipRect(&clip);
            scroller.windows.push_back(n.root);
            scroller.rects.push_back(&n.clip);
        }
        scroller.scrollbar = GUI::GetWindowTypedF<Window>(w, "%s.scroll_bar_professions.scroll_bar_tiny", p);
        scroller.itemSize = node[1].root->y - node[0].root->y;
        scroller.onScroll = UpdateContents;
        scroller.onNext = OnSkillLevelHolderUp;
        scroller.onPrev = OnSkillLevelHolderDown;
        scroller.horizontal = false;
        scroller.unkA0 = 3;
        scroller.perLine = 1;
    }
};

WindowManager::FunctionalWindow* g_queue = nullptr;   // CraftItemWindow::wnd 0x61df78
Window* g_root = nullptr;                     // 0x61c8d8 Window_item_craft.xml
GUI::TweenEffect* g_tween = nullptr;          // 0x61c8dc
ProfessionSkillZone g_zone;                   // 0x61c8e0 scroller_prof_levels (the professions' levels)
Shared::ContentScroller g_recipes;            // 0x61d594 the list of recipes / professions
Window* g_scrollContainer = nullptr;          // 0x61d60c scroll_bar_tiny_container_crafting
Shared::TabHolder g_tabs[6];                  // 0x61d6a0 tab_01..06 (0: professions)
unsigned g_tab = 0;                           // 0x61d778 the tab shown
Textfield* g_title = nullptr;                 // 0x61d77c text_title
unsigned g_selected = 0;                      // 0x61d780 the recipe (or profession) selected
std::vector<Professions::Profession*> g_professions;   // 0x61d784 per tab (1..5) its profession
Textfield* g_description = nullptr;           // 0x61d794 text_craftable_item_desc
Window* g_limitLine = nullptr;                // 0x61d798 scaler_limit_underline_2px_02
Window* g_craftButton = nullptr;              // 0x61d79c button_craft_items_craft
Textfield* g_tabName = nullptr;               // 0x61d7a0 text_tab_name
Window* g_requiredBg = nullptr;               // 0x61d7a4 bg_item_holder_01
RequiredItemInfo g_required[3];               // 0x61d7a8 the items the recipe needs
Window* g_plus1 = nullptr;                    // 0x61d91c icon_add_01
Window* g_plus2 = nullptr;                    // 0x61d920 icon_add_02
Window* g_arrow = nullptr;                    // 0x61d924 craft_arrow
Window* g_resultBg = nullptr;                 // 0x61d928 bg_item_holder_02
RequiredItemInfo g_result;                    // 0x61d92c the item the recipe makes
CraftItemInfo g_list[7];                      // 0x61d9a8 the recipes in view
LockedCraftInfo g_locked;                     // 0x61dc9c crafting_item_locked
Textfield* g_tabNameTop = nullptr;            // 0x61de24 text_tab_name_top
Textfield* g_professionExplained = nullptr;   // 0x61de28 profession_explained
Window* g_professionScaler = nullptr;         // 0x61de2c scaler_prof_01
ProfessionInfo g_professionList[7];           // 0x61de30 professions_holder_masked.profession_list_NN
std::vector<Collections::Collection*> g_collections;   // 0x61def4 the recipes of the tab, by "sort"
Window* g_craftableHolder = nullptr;          // 0x61df04 bg_craftable_holder
ItemInfo g_craftableItem;                     // 0x61df08 inventory_slot_holder_01.inventory_item_icon: what is made
Textfield* g_classText[5] = {};               // 0x61df4c text_item_class_01..05
Textfield* g_craftText = nullptr;             // 0x61df64 button_craft_items_craft.text
bool g_crafting = false;                      // 0x61df68 the second after Craft
float g_craftTime = 0.f;                      // 0x61df6c
ItemHoverWindow* g_hover = nullptr;           // 0x61df70 the card of a tapped item
GUI::Button* g_close = nullptr;               // 0x61df74 x_button.clickArea
EffectDescriptionLine g_effects[4];           // 0x61dd94 stat_block_shop.stats_text_holder_04.info_box_info_line_NN
EffectDescriptionLine g_effectsHuge[2];       // 0x61ddf4 ...stats_text_holder_02.info_box_info_line_huge_NN
bool g_tablet = false;                        // 0x60f1ec
float g_scale = 1.f;                          // 0x60f1f0
int g_pendingList = -1;                       // 0x60f1e4 the recipe the next opening points the arrow at
int g_pendingItem = -1;                       // 0x60f1e8 its id, for OnListItem's craft arrow

// @0x29c190 CollectionSort: the recipes of a tab by "sort". OnTab sorts them with StlSort::Sort, the std::sort
// of the original (its inlined FUN_002a1678 @0x2a1678 introsort loop, FUN_002a158c @0x2a158c heap adjust and
// FUN_002a14ec @0x2a14ec insertion sort).
bool CollectionSort(const Collections::Collection* a, const Collections::Collection* b) { return a->sort < b->sort; }

// @0x29c2a8
void OnTweenOut() {
    WindowManager::WindowHide(false);
    g_queue->shown = false;
    g_queue->MoveWindowDown(true);
    GameState::DropGamePauseState();
}

// The scroll state of the three levels' sideways strips, saved while they hand it over. The original
// keeps it in spare scroller fields (+0x9c / +0xa8 / +0xb8).
struct ScrollerState { unsigned itemCount; unsigned firstIndex; int offset; };
ScrollerState g_saved[3];

void SaveNodeScrollers() {
    for (unsigned k = 0; k != 3; ++k) {
        const Shared::ContentScroller& s = g_zone.node[k].scroller;
        g_saved[k] = {s.itemCount, s.firstIndex, s.offset};
    }
}

// Level `to` takes the saved scroll state of level `from`.
void TakeNodeScroll(unsigned to, unsigned from) {
    Shared::ContentScroller& s = g_zone.node[to].scroller;
    s.itemCount = g_saved[from].itemCount;
    s.firstIndex = g_saved[from].firstIndex;
    s.HandleMove(g_saved[from].offset - g_saved[to].offset);
}

// @0x29c2dc: the column went one level down: each level's strip follows its level to the next
// one, the first level starts at the left.
void OnSkillLevelHolderDown() {
    SaveNodeScrollers();
    TakeNodeScroll(1, 0);
    TakeNodeScroll(2, 1);
    Shared::ContentScroller& s = g_zone.node[0].scroller;
    s.firstIndex = 0;
    s.HandleMove(-g_saved[0].offset);
}

// @0x29c390: the column went one level up: each level takes the strip of the one below it, the
// last one starts at the left.
void OnSkillLevelHolderUp() {
    SaveNodeScrollers();
    TakeNodeScroll(0, 1);
    TakeNodeScroll(1, 2);
    Shared::ContentScroller& s = g_zone.node[2].scroller;
    s.firstIndex = 0;
    s.HandleMove(-s.offset);
}

// @0x29c444: the recipe index comes into view: the list starts at the page it is on (the list
// scrolls by whole pages of perLine, the last page shows the last seven).
void ScrollRecipeIntoView(unsigned index) {
    g_recipes.firstIndex = 0;
    const unsigned per = g_recipes.perLine;
    if (7 - per <= index) {
        const unsigned first = per * (index / per);
        g_recipes.firstIndex = first;
        const int left = ((int)g_recipes.itemCount - 1) - (int)first;
        if (left < 5) g_recipes.firstIndex = (unsigned)((int)first - 5 + left);
    }
    g_recipes.HandleMove(-g_recipes.offset);
    if (4 < (int)(index - g_recipes.firstIndex)) g_recipes.HandleMove((int)((float)-g_recipes.itemSize * 0.8f));
}

// @0x29f680: a profession level comes into view in the profession tab (the column starts at it).
void ScrollProfessionLevelIntoView(unsigned level) {
    if (level != 0) level -= 1;
    g_zone.scroller.firstIndex = level;
    g_zone.scroller.HandleMove(-g_zone.scroller.offset);
    UpdateContents();
}

// @0x29f98c: the item card over a slot.
void ShowItemHover(unsigned id, Window* w) {
    Items::ItemInfo* info = Items::GetItemInfo(id);
    if (!info) return;
    // UNVERIFIED (milestone 6): SoundsManager::PlaySound("ui_click", 1, false).
    g_hover->Show();
    g_hover->SetItem(nullptr, nullptr, info, false, true, nullptr, false, false, false, false);
    const int x = g_root->x + w->x, y = g_root->y + w->y;
    g_hover->SetTargetRect(x, y, x + w->w, y + w->h, ItemHoverWindow::kS);
    g_hover->RecomputePosition();
    g_hover->SetZ(0.001f);
    Render::SortRenderLayer(0xe, 1);
}

// The profession tab (0): the professions in the list and, for the selected one, its description
// and three levels with their progress and the recipes each unlocks.
void UpdateProfessionTab(Professions::Profession* p) {
    for (Window* w : {g_craftableHolder, g_craftableItem.root}) w->SetVisibility(false);
    for (Textfield* t : g_classText) t->SetVisibility(false);
    for (Window* w : {(Window*)g_description, g_limitLine, g_craftButton, (Window*)g_tabName, g_requiredBg})
        w->SetVisibility(false);
    for (RequiredItemInfo& r : g_required) r.root->SetVisibility(false);
    for (Window* w : {g_plus1, g_plus2, g_arrow, g_resultBg, g_result.root}) w->SetVisibility(false);
    for (CraftItemInfo& l : g_list) l.root->SetVisibility(false);
    g_locked.root->SetVisibility(false);
    for (EffectDescriptionLine& e : g_effects) e.root->SetVisibility(false);
    for (EffectDescriptionLine& e : g_effectsHuge) e.root->SetVisibility(false);
    g_tabNameTop->SetText(StringTable::GetString("CRAFT_ITEM_PROFESSIONS"));
    g_professionExplained->SetVisibility(true);
    g_professionExplained->SetText(p->desc);
    g_professionScaler->SetVisibility(true);
    bool unlocked0 = p->IsUnlocked();
    // PORT: the zone keeps a copy of the profession's levels (a vector in the original).
    const std::vector<Professions::ProfessionLevel*>& levels = p->levels;
    g_zone.scroller.itemCount = (unsigned)levels.size();
    g_zone.scroller.UpdateScroll();
    g_zone.root->SetVisibility(true);
    for (unsigned n = 0; n < 3; ++n) {
        ProfessionSkillInfo& nd = g_zone.node[n];
        nd.root->SetVisibility(n + g_zone.scroller.firstIndex < levels.size());
        if (nd.root->visibleSelf) {
            Professions::ProfessionLevel* level = levels[n + g_zone.scroller.firstIndex];
            const uint32_t points = GameState::GetProfessionPoints(p->id);
            const bool reached = level->level <= Professions::GetPlayerProfessionLevel(p->id);
            bool unlocked = unlocked0, locked;
            if ((int)(level->level - Professions::GetPlayerProfessionLevel(p->id)) < 2) {
                locked = !unlocked;
            } else {   // a level more than one above the player's stays locked
                unlocked = false;
                locked = true;
            }
            nd.recipes.clear();
            for (unsigned i = 0; Collections::Collection* c = Collections::EnumCollection(i); ++i)
                if (c->reputationId == p->id && c->reputationLevel == level->level) nd.recipes.push_back(c);
            nd.scroller.itemCount = (unsigned)nd.recipes.size();
            nd.root->SetEnabled(true);
            nd.flag->SetEnabled(unlocked);
            nd.bar.root->SetEnabled(unlocked);
            nd.icon->SetEnabled(unlocked);
            nd.classActive->SetText(level->name);
            nd.day1->SetText(ToWideString((int)level->level));
            nd.day2->SetText(ToWideString((int)level->level));
            nd.day1->SetEnabled(unlocked);
            nd.day2->SetEnabled(unlocked);
            nd.achieved->SetText(StringTable::GetString("CRAFT_ITEM_ACHIEVED"));
            nd.achieved->SetVisibility(reached);
            nd.checkbox->SetVisibility(reached);
            nd.arrowLeft->SetVisibility(false);
            nd.arrowRight->SetVisibility(false);
            nd.icon->SetTexture(p->iconLarge, true);
            nd.explained->SetText(
                SWPrintf(0x100, StringTable::GetString("CRAFT_ITEM_EXPLAINED"), {level->name, p->title}).c_str());
            std::u32string text;
            if (!reached) {
                if (!unlocked)
                    text = SWPrintf(0x100, U"%s - %d %s",
                                    {level->name, (int)level->points, StringTable::GetString("CRAFT_ITEM_REQUIRED")});
                else
                    text = SWPrintf(0x100, U"%s - %d/%d", {level->name, (int)points, (int)level->points});
            } else {
                text = SWPrintf(0x100, U"%s - %d/%d - %s",
                                {level->name, (int)level->points, (int)level->points,
                                 StringTable::GetString("CRAFT_ITEM_ACHIEVED")});
            }
            nd.bar.textUnder->SetText(text.c_str());
            nd.bar.textOver->SetText(text.c_str());
            nd.bar.lock->SetVisibility(locked);
            nd.bar.SetProgress(points >= level->points ? 1.f : (float)points / (float)level->points);
            // The strip: the level's recipes (up to eight), their icons.
            for (unsigned k = 0; k < 8; ++k) {
                nd.slots[k]->MoveWindow(-nd.shiftX[k & 1], -nd.shiftY[k & 1]);
                ItemInfo& it = nd.items[k];
                it.root->SetVisibility(k + nd.scroller.firstIndex < nd.recipes.size());
                it.root->SetEnabled(true);
                if (!it.root->visibleSelf) continue;
                Collections::Collection* c = nd.recipes[k + nd.scroller.firstIndex];
                it.goldOwned->SetVisibility(false);
                it.redX->SetVisibility(false);
                it.lock->SetVisibility(false);
                it.recommended->SetVisibility(false);
                if (Items::ItemInfo* info = Items::GetItemInfo(c->to)) it.icon->SetTexture(info->GetIcon(), true);
                it.item = (int)c->to;
            }
            // Few recipes sit in the middle of the strip.
            const unsigned count = (unsigned)nd.recipes.size();
            nd.shiftX[0] = nd.shiftX[1] = nd.shiftY[0] = nd.shiftY[1] = 0;
            const int d = nd.slots[2]->x - nd.slots[0]->x;
            if (count < 2) nd.shiftY[0] = (nd.slots[1]->y - nd.slots[0]->y) / 2;
            int x = nd.shiftX[0];
            if (count < 3) {
                x += d / 2;
                nd.shiftX[0] = x;
            }
            if (count < 5) x += d / 2;
            const int y = nd.shiftY[0];
            if (count < 5) nd.shiftX[0] = x;
            nd.shiftX[1] = x;
            nd.shiftY[1] = y;
            if (count < 6 && (count & 1)) nd.shiftX[1] = x + d / 2;
            for (unsigned k = 0; k < 8; ++k) nd.slots[k]->MoveWindow(nd.shiftX[k & 1], nd.shiftY[k & 1]);
        }
        nd.scroller.UpdateScroll();
    }
    // The professions in the list.
    g_recipes.itemCount = (unsigned)g_professions.size();
    for (unsigned i = 0; i < 7; ++i) {
        ProfessionInfo& pi = g_professionList[i];
        const unsigned index = i + g_recipes.firstIndex;
        pi.root->SetVisibility(index < g_professions.size());
        if (!pi.root->visibleSelf) continue;
        Professions::Profession* pr = g_professions[index];
        const bool unlocked = pr->IsUnlocked();
        pi.selectedBg->SetVisibility(index == g_selected);
        pi.slotStatic->SetVisibility(index == g_selected ? false : unlocked);
        pi.slotLocked->SetVisibility(index == g_selected ? false : !unlocked);
        pi.active->SetTexture(pr->icon, true);
        pi.name->SetText(pr->title);
        pi.lock->SetVisibility(!unlocked);
        pi.lock->SetTexture(IconManager::GetIcon("gold_16_lock", false), true);
        pi.selectedBg->SetEnabled(unlocked);
        pi.active->SetEnabled(unlocked);
        pi.root->SetEnabled(true);
    }
}

// The recipe tabs (1..5): the recipe list and the selected recipe: what it makes, what it needs and
// whether it can be made (or its profession level is missing: the locked card).
bool UpdateRecipeTab() {
    g_professionExplained->SetVisibility(false);
    g_professionScaler->SetVisibility(false);
    g_zone.root->SetVisibility(false);
    for (ProfessionInfo& pi : g_professionList) pi.root->SetVisibility(false);
    g_tabNameTop->SetText(StringTable::GetString("CRAFT_ITEM_RECIPES"));
    if (g_collections.size() <= g_selected) return false;
    Collections::Collection* c = g_collections[g_selected];
    if (!c) return false;
    Items::ItemInfo* info = Items::GetItemInfo(c->to);
    if (!info) return false;
    const bool locked = Professions::GetPlayerProfessionLevel(c->reputationId) < c->reputationLevel;   // local_974
    // What it makes (the card at the top).
    g_craftableHolder->SetVisibility(true);
    g_craftableItem.root->SetVisibility(true);
    g_craftableItem.goldOwned->SetVisibility(false);
    g_craftableItem.redX->SetVisibility(false);
    g_craftableItem.lock->SetVisibility(false);
    g_craftableItem.recommended->SetVisibility(false);
    g_craftableItem.locked->SetVisibility(false);
    for (unsigned k = 0; k < 5; ++k) g_craftableItem.itemClass[k]->SetVisibility(info->itemLevel == k);
    g_craftableItem.icon->SetTexture(info->GetIcon(), true);
    g_craftableItem.quantity->SetText(ToWideString(GameState::GetItemAmount(info->id, false)));
    for (unsigned k = 0; k < 5; ++k) g_classText[k]->SetVisibility(info->itemLevel == k);
    g_classText[info->itemLevel]->SetText(info->title);
    // Its description, or its effects.
    char key[0x40];
    std::snprintf(key, sizeof key, "%s_DESC", info->name);
    g_description->SetVisibility(StringTable::StringNotEmpty(key) && info->type != 8 && info->type != 3 && !info->meta);
    if (g_description->visibleSelf) g_description->SetText(StringTable::GetString(key));
    for (EffectDescriptionLine& e : g_effects) {
        e.root->SetVisibility(false);
        e.root->MoveWindow(0, -e.f14);
        e.f14 = 0;
    }
    for (EffectDescriptionLine& e : g_effectsHuge) {
        e.root->SetVisibility(false);
        e.root->MoveWindow(0, -e.f14);
        e.f14 = 0;
    }
    if (!g_description->visibleSelf && info->meta) {
        unsigned count = 0;
        for (MetaExpression* m = info->meta; m && count < 4; m = m->GetNext()) ++count;
        auto show = [&](EffectDescriptionLine* lines, unsigned max) {
            unsigned i = 0;
            for (MetaExpression* m = info->meta; m && i < max; m = m->GetNext(), ++i) {
                std::u32string desc;
                m->GenerateDescription(desc, 0x80, EntityManager::GetPlayer());
                lines[i].root->SetVisibility(true);
                lines[i].text->SetVisibility(true);
                lines[i].text->SetText(desc.c_str());
                lines[i].icon->SetTexture(IconManager::GetIcon(m->GetBonusIcon(info), false), true);
            }
        };
        if (count < 3) {   // up to two big lines
            show(g_effectsHuge, 2);
            if (count == 1) {
                g_effectsHuge[0].f14 = (g_effectsHuge[1].root->y - g_effectsHuge[0].root->y) / 2;
                g_effectsHuge[0].root->MoveWindow(0, g_effectsHuge[0].f14);
            }
        } else {           // up to four lines
            show(g_effects, 4);
            if (count == 3) {
                g_effects[2].f14 = (g_effects[1].root->y - g_effects[0].root->y) / 2;
                g_effects[2].root->MoveWindow(0, g_effects[2].f14);
            }
        }
    }
    // What it needs.
    g_limitLine->SetVisibility(true);
    g_tabName->SetVisibility(!locked);
    g_tabName->SetText(StringTable::GetString("CRAFT_ITEM_REQUIRED_TAP_TO_FIND"));
    g_requiredBg->SetVisibility(!locked);
    bool allParts = true;    // local_978
    bool buyable = true;     // local_960
    for (unsigned i = 0; i < 3; ++i) {
        RequiredItemInfo& r = g_required[i];
        Items::ItemInfo* part = Items::GetItemInfo(c->parts[i].item);
        const uint32_t need = c->parts[i].amount;
        r.root->SetVisibility(locked ? false : part != nullptr);
        r.item.goldOwned->SetVisibility(false);
        r.item.redX->SetVisibility(false);
        r.item.lock->SetVisibility(false);
        r.item.recommended->SetVisibility(false);
        if (!part) continue;
        for (unsigned k = 0; k < 5; ++k) r.item.itemClass[k]->SetVisibility(part->itemLevel == k);
        r.item.icon->SetTexture(part->GetIcon(), true);
        const uint32_t have = (uint32_t)GameState::GetItemAmount(part->id, false);
        if (have < need) allParts = false;
        const bool enough = have >= need;
        // PORT: the crystal "Buy now" buttons are not offered, so a missing part gets its Find button.
        r.find->SetVisibility(!enough);
        r.findText->SetVisibility(!enough);
        r.findText->SetText(StringTable::GetString("CRAFTING_FIND"));
        r.crafted->SetVisibility(enough);
        const std::u32string count =
            SWPrintf(0x100, U"%d/%d", {(int)(have < need ? have : need), (int)need});
        r.item.quantityRed->SetVisibility(!enough);
        if (r.item.quantityRed->visibleSelf) r.item.quantityRed->SetText(count.c_str());
        r.item.quantity->SetVisibility(enough);
        if (r.item.quantity->visibleSelf) r.item.quantity->SetText(count.c_str());
        if (!enough) buyable = false;
        r.buyNow.root->SetVisibility(false);
    }
    // What it makes (the result at the right).
    for (Window* w : {g_plus1, g_plus2, g_arrow, g_resultBg, g_result.root}) w->SetVisibility(!locked);
    g_result.item.goldOwned->SetVisibility(false);
    g_result.item.redX->SetVisibility(false);
    g_result.item.lock->SetVisibility(false);
    g_result.item.recommended->SetVisibility(false);
    g_result.item.locked->SetVisibility(false);
    for (unsigned k = 0; k < 5; ++k) g_result.item.itemClass[k]->SetVisibility(info->itemLevel == k);
    g_result.item.icon->SetTexture(info->GetIcon(), true);
    g_result.find->SetVisibility(false);
    g_result.crafted->SetVisibility(true);
    g_result.confirm->SetTexture(IconManager::GetIcon("gold_repair", false), true);
    g_result.buyNow.root->SetVisibility(false);
    g_craftButton->SetVisibility(!locked);
    g_craftButton->SetEnabled(allParts || buyable);
    g_craftText->SetText(StringTable::GetString("CRAFT_ITEM_CREATE"));
    // The recipes in the list.
    g_recipes.itemCount = (unsigned)g_collections.size();
    for (unsigned i = 0; i < 7; ++i) {
        CraftItemInfo& l = g_list[i];
        const unsigned index = i + g_recipes.firstIndex;
        l.root->SetVisibility(index < g_collections.size());
        if (!l.root->visibleSelf) continue;
        Collections::Collection* rc = g_collections[index];
        const bool rLocked = Professions::GetPlayerProfessionLevel(rc->reputationId) < rc->reputationLevel;
        l.selectedBg->SetVisibility(index == g_selected);
        l.slotStatic->SetVisibility(index != g_selected && !rLocked);
        l.slotLocked->SetVisibility(index != g_selected && rLocked);
        l.item.goldOwned->SetVisibility(false);
        l.item.redX->SetVisibility(false);
        l.item.lock->SetVisibility(false);
        l.item.recommended->SetVisibility(false);
        if (Items::ItemInfo* made = Items::GetItemInfo(rc->to)) {
            l.item.locked->SetVisibility(rLocked);
            for (unsigned k = 0; k < 5; ++k) l.item.itemClass[k]->SetVisibility(rLocked ? false : made->itemLevel == k);
            l.item.icon->SetTexture(made->GetIcon(), true);
            l.item.icon->SetEnabled(!rLocked);
            for (unsigned k = 0; k < 5; ++k) l.name[k]->SetVisibility(made->itemLevel == k);
            l.name[made->itemLevel]->SetText(made->title);
        }
        l.item.lockTiny->SetTexture(IconManager::GetIcon("gold_16_lock", false), true);
        l.item.lockTiny->SetVisibility(rLocked);
    }
    // The locked card: the profession level the recipe needs.
    g_locked.root->SetVisibility(locked);
    Professions::Profession* prof = Professions::GetProfession(c->reputationId);
    Professions::ProfessionLevel* level = Professions::GetProfessionLevel(c->reputationId, c->reputationLevel);
    if (prof && level) {
        g_title->SetText(prof->title);
        g_locked.noItem->SetText(
            SWPrintf(0x100, StringTable::GetString("CRAFT_ITEM_REQUIRED_PROFESSION"), {level->name}).c_str());
        g_locked.day1->SetText(ToWideString((int)level->level));
        g_locked.day2->SetText(ToWideString((int)level->level));
        g_locked.day1->SetEnabled(false);
        g_locked.day2->SetEnabled(false);
        g_locked.achieved->SetText(prof->title);
    }
    if (prof) g_locked.icon->SetTexture(prof->iconLarge, true);
    g_locked.icon->SetEnabled(false);
    g_locked.required->SetText(StringTable::GetString(locked ? "CRAFT_ITEM_LOCKED" : "CRAFT_ITEM_REQUIRED_TAP_TO_FIND"));
    for (unsigned i = 0; i < 3; ++i) {
        ItemInfo& it = g_locked.items[i];
        Items::ItemInfo* part = Items::GetItemInfo(c->parts[i].item);
        it.root->SetVisibility(part != nullptr);
        it.goldOwned->SetVisibility(false);
        it.redX->SetVisibility(false);
        it.lock->SetVisibility(false);
        it.recommended->SetVisibility(false);
        if (!part) continue;
        for (unsigned k = 0; k < 5; ++k) it.itemClass[k]->SetVisibility(part->itemLevel == k);
        it.icon->SetTexture(part->GetIcon(), true);
        it.quantity->SetText(ToWideString((int)c->parts[i].amount));
    }
    return true;
}

// @0x29c53c: the tabs, then the professions tab or a recipe tab.
void UpdateContents() {
    for (unsigned i = 0; i < 6; ++i) {
        g_tabs[i].SetMode(g_tab == i, false);
        g_tabs[i].root->SetEnabled(g_tabs[i].unk20 != 0);
        g_tabs[i].root->SetVisibility(i == 0 ? true : Professions::GetProfessionForTab(i, Professions::kProfession) != nullptr);
    }
    if (g_tab == 0) {
        g_title->SetText(StringTable::GetString("CRAFT_ITEM_TITLE"));
        if (g_professions.size() <= g_selected || !g_professions[g_selected]) return;
        UpdateProfessionTab(g_professions[g_selected]);
    } else {
        if (!UpdateRecipeTab()) return;
    }
    g_recipes.UpdateScroll();
    g_root->UpdatePosition();
}

// @0x29fabc: an item the selected recipe needs.
void OnImage(unsigned part) {
    if (g_collections.size() <= g_selected) return;
    Collections::Collection* c = g_collections[g_selected];
    if (!c) return;
    ShowItemHover(c->parts[part].item, g_required[part].root);
}

// @0x29fb10: what the selected recipe makes.
void OnResult() {
    if (g_collections.size() <= g_selected) return;
    Collections::Collection* c = g_collections[g_selected];
    if (!c) return;
    ShowItemHover(c->to, g_craftableItem.root);
}

// STUBS (5o-7).
void OnCraft() {}                         // 0x29fb54
void OnFind(unsigned) {}                  // 0x2a03ec
// @0x29ff94 OnBuy: PORT (crystal purchases not offered, see the top).
void OnBuy(unsigned, bool) {}

// @0x2a0100 OnListItem: a recipe (or profession) of the list is selected.
void SelectListItem(unsigned index) {
    g_selected = index + g_recipes.firstIndex;
    if (g_tab == 0) {
        // The profession tab: the column of levels starts at the profession's level.
        g_zone.scroller.firstIndex = 0;
        g_zone.scroller.HandleMove(-g_zone.scroller.offset);
        if (g_selected < g_professions.size() && g_professions[g_selected])
            g_zone.scroller.ScrollIntoView(Professions::GetPlayerProfessionLevel(g_professions[g_selected]->id));
    }
    UpdateContents();
    if (g_pendingItem != -1 && g_selected < g_collections.size() && g_collections[g_selected] &&
        g_collections[g_selected]->reputationLevel <=
            Professions::GetPlayerProfessionLevel(g_collections[g_selected]->reputationId)) {
        // The recipe can be made: the arrow points at the Craft button.
        Window* b = g_craftButton;
        BuildingHovers::ArrowAt((float)(g_root->x + b->x + b->w / 2), (float)(g_root->y + b->y), false, false, false, false,
                                true, false, false);
        BuildingHovers::SetArrowClickCallback(new GUI::Callback(OnCraft));   // FUN_0029f948
    }
    g_pendingItem = -1;
}

void Update(float dt);   // below

// @0x29f6b4: the second after Craft the recipe's result is collected; the scrollers run; once the
// window is in, the pending arrow points at the recipe in the list.
void Update(float dt) {
    if (g_crafting) {
        g_craftTime += dt;
        if (g_craftTime > 1.f) {
            g_crafting = false;
            GUI::SetInteractionLock(false);
            Collections::Collection* c = g_selected < g_collections.size() ? g_collections[g_selected] : nullptr;
            if (!c) return;
            Items::ItemInfo* info = Items::GetItemInfo(c->to);
            if (!info) return;
            BuildingHovers::OnCollect(info->id, g_result.item.icon->sprite, true, true, 0xb, false);
            UpdateContents();
            Render::SortRenderLayer(0xe, 1);
        }
    }
    if (!g_queue->shown) return;
    g_recipes.Update(dt);
    g_zone.scroller.Update(dt);
    if (g_tween->active) return;
    if (g_pendingList == -1 || g_pendingList > 6) return;
    Window* row = g_list[g_pendingList].root;
    TaskInfoWindow::BlockArrowLimitChangeOnce();
    BuildingHovers::SetArrowVisibleWindowLimit(2);
    BuildingHovers::ArrowAt((float)(g_root->x + row->x + row->w / 2), (float)(g_root->y + row->y), false, false, false,
                            false, true, false, false);
    const unsigned at = (unsigned)g_pendingList;
    BuildingHovers::SetArrowClickCallback(new GUI::Callback([at] { SelectListItem(at); }));
    if (g_tab != 0) {
        const unsigned index = at + g_recipes.firstIndex;
        if (index < g_collections.size()) g_pendingItem = (int)g_collections[index]->id;
    }
    g_pendingList = -1;
}

// @0x29f940: the tween's end calls Update (the first frame's arrow).
void OnTweenIn() { Update(0.f); }

// @0x2a0390: back closes the item card first, then the window.
bool OnBack() {
    if (!g_queue->shown) return false;
    if (!g_hover->shownHover) {
        Hide();
        return true;
    }
    g_hover->Hide();
    return true;
}

// @0x29c1a8
void SetZ(float z) {
    if (!g_queue->shown) return;
    g_root->SetZ(z);
    if (g_tween->active) return;
    g_root->SetPosition(g_tween->GetCenteredX(), g_tween->GetCenteredY());
}

// @0x29c254
float ZRange() { return g_queue->shown ? 0.12f : 0.01f; }

// @0x2a0730: a drag moves the scrollers; the three levels' strips follow the pointer on their own
// (once it has moved more than ten pixels).
void Move(int x, int y) {
    g_recipes.Move(x, y);
    g_zone.scroller.Move(x, y);
    for (ProfessionSkillInfo& n : g_zone.node) {
        Shared::ContentScroller& s = n.scroller;
        if (!s.pressed || std::abs(x - s.lastPos) < 11) {
            if (!s.dragging) continue;
        } else {
            s.dragging = true;
        }
        s.HandleMove(x - s.lastPos);
        s.lastPos = x;
    }
}

// @0x2a07f8
bool Click(int x, int y, bool pressed) {
    if (g_root->visibleSelf) {
        if (g_hover->Click(x, y, pressed)) return true;
        if (BuildingHovers::ClickOnArrow(x, y, pressed)) return true;
        if (!pressed) BuildingHovers::HideArrow();
        if (g_recipes.Click(x, y, pressed, g_root)) return true;
        if (g_zone.scroller.Click(x, y, pressed, g_root)) return true;
        if (pressed) {
            // A press inside a level's strip starts its drag.
            for (ProfessionSkillInfo& n : g_zone.node) {
                if (n.root->visibleSelf && g_root->x + n.clip.left <= x && x <= g_root->x + n.clip.right &&
                    g_root->y + n.clip.top <= y && y <= g_root->y + n.clip.bottom) {
                    n.scroller.lastPos = x;
                    n.scroller.pressed = true;
                }
            }
        } else {
            // The release ends the presses; a drag that was running swallows it.
            ProfessionSkillInfo* n = g_zone.node;
            n[0].scroller.pressed = false;
            const bool drag0 = n[0].scroller.dragging;
            if (drag0) n[0].scroller.dragging = false;
            n[1].scroller.pressed = false;
            const bool drag1 = n[1].scroller.dragging;
            if (drag1) n[1].scroller.dragging = false;
            n[2].scroller.pressed = false;
            if (n[2].scroller.dragging) {
                n[2].scroller.dragging = false;
                return true;
            }
            if (drag1 || drag0) return true;
        }
    }
    if (TopCityWindow::Click(x, y, pressed)) return true;
    const bool visible = g_root->visibleSelf;
    g_root->Click(x, y, pressed, false);
    return visible;
}

// @0x2a2008 OnTab: the tab: the recipes of the profession tab (Collections with that "tab", by "sort").
void SelectTab(unsigned tab) {
    g_recipes.firstIndex = 0;
    g_recipes.HandleMove(-g_recipes.offset);
    g_selected = 0;
    g_tab = tab;
    g_collections.clear();
    if (tab != 0) {
        for (unsigned i = 0; Collections::Collection* c = Collections::EnumCollection(i); ++i)
            if (c->tab == tab) g_collections.push_back(c);
        StlSort::Sort(g_collections.data(), g_collections.data() + g_collections.size(), CollectionSort);
    }
    UpdateContents();
}

// @0x2a2208: the "locked" card: back to the profession's recipes, at the level it unlocks.
void OnLockedFlag() {
    if (g_collections.size() <= g_selected) return;
    Collections::Collection* c = g_collections[g_selected];
    if (!c) return;
    const unsigned tab = g_tab;
    SelectTab(0);
    SelectListItem(tab - 1);
    ScrollProfessionLevelIntoView(c->reputationLevel);
}

// @0x2a2358: an item of a profession level's strip: its recipe is shown.
void OnProfessionLevelItem(unsigned node, unsigned slot) {
    Items::ItemInfo* info = Items::GetItemInfo((uint32_t)g_zone.node[node].items[slot].item);
    if (!info) return;
    ShowItem(info->id);
}

// @0x29c444 ...
}  // namespace

WindowManager::FunctionalWindow* Queue() {
    if (!g_queue) {   // FUN_002a11cc
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.deinit = Deinit;
        f.click = Click;
        f.setZ = SetZ;
        f.hide = Hide;
        f.move = Move;
        g_queue = new WindowManager::FunctionalWindow("CraftItemWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_queue->shown; }   // @0x29c240

// @0x2a2008, @0x2a0100
void OnTab(unsigned tab) { SelectTab(tab); }
void OnListItem(unsigned index) { SelectListItem(index); }

// @0x2a0284
void Show() {
    if (GameState::IsArenaCombatActive()) {
        PopupWindow::Show(StringTable::GetString("CAN_NOT_OPEN_IN_ARENA"), PopupWindow::Hide, nullptr, nullptr);
        return;
    }
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

// @0x2a036c
void Hide() {
    if (!g_root->visibleSelf) return;
    g_tween->AnimateOut();
}

// @0x2a2264: the recipe `id`: its tab, scrolled into view and selected. (The original repeats its
// scan five times with nothing changing between the rounds.)
void ShowCollection(uint32_t id) {
    for (unsigned i = 0; Collections::Collection* c = Collections::EnumCollection(i); ++i) {
        if (c->id != id || c->tab == 0) continue;
        SelectTab(c->tab);
        unsigned index = 0;
        for (unsigned k = 0; k < g_collections.size(); ++k)
            if (g_collections[k]->id == id) index = k;
        ScrollRecipeIntoView(index);
        SelectListItem(index - g_recipes.firstIndex);
        g_pendingList = (int)(index - g_recipes.firstIndex);
        OnTweenIn();
        return;
    }
}

// @0x2a233c: the recipe that makes `item`.
void ShowItem(uint32_t item) {
    if (Collections::Collection* c = Collections::GetCollectionWithResult(item)) ShowCollection(c->id);
}

// @0x2a0a2c
void Deinit() {
    GUI::RemoveTweenEffect(g_tween);
    g_tween = nullptr;
    delete g_root;
    g_root = nullptr;
    delete g_hover;
}

// @0x2a2398
void Init() {
    g_tab = 0;
    g_selected = 0;
    g_tablet = GUI::IsTabletVersion();
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Window_item_craft.xml", "Window_item_craft.png", -1.f, 0, 0,
                             0, 0, false, 1.f);
    g_queue->RegisterTopWindow(g_root);
    g_scale = g_root->scale;
    g_root->SetVisibility(false);
    g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::GetVerticalCenter(g_root->h));
    g_tween = GUI::CreateTweenEffect(g_root, nullptr, OnTweenIn, OnTweenOut);
    g_tween->CenterWith(0, (int)(g_scale * -50.f), 0, 0);
    g_close = GUI::GetWindowTyped<GUI::Button>(g_root, "x_button.clickArea");
    g_close->SetOnClick(Hide);
    g_close->SetActionSound("ui_close", 1);
    for (unsigned i = 0; i < 6; ++i) {
        char name[0x80];
        std::snprintf(name, sizeof name, "tab_%02d", i + 1);
        g_tabs[i].LoadFrom(g_root, name, false);
        g_tabs[i].root->SetOnClick([i] { SelectTab(i); });
        g_tabs[i].root->SetActionSound("ui_tab", 1);
    }
    g_tabs[0].icon->SetTexture(IconManager::GetIcon("professions_60", false), true, (unsigned)g_tabs[0].icon->origW + 0xf, 0,
                               false, 0);
    g_title = GUI::GetWindowTyped<Textfield>(g_root, "text_title");
    g_tabNameTop = GUI::GetWindowTyped<Textfield>(g_root, "text_tab_name_top");
    for (int i = 0; i < 5; ++i) g_classText[i] = GUI::GetWindowTypedF<Textfield>(g_root, "text_item_class_%02d", i + 1);
    g_tabName = GUI::GetWindowTyped<Textfield>(g_root, "text_tab_name");
    g_craftableHolder = GUI::GetWindowTyped<Window>(g_root, "bg_craftable_holder");
    g_craftableItem.LoadFrom(g_root, "inventory_slot_holder_01.inventory_item_icon");
    g_craftableItem.icon->SetOnClick(OnResult);
    g_description = GUI::GetWindowTyped<Textfield>(g_root, "text_craftable_item_desc");
    for (int i = 0; i < 4; ++i) {
        char name[0x100];
        std::snprintf(name, sizeof name, "stat_block_shop.stats_text_holder_04.info_box_info_line_%02d", i + 1);
        g_effects[i].LoadFrom(g_root, name);
    }
    for (int i = 0; i < 2; ++i) {
        char name[0x100];
        std::snprintf(name, sizeof name, "stat_block_shop.stats_text_holder_02.info_box_info_line_huge_%02d", i + 1);
        g_effectsHuge[i].LoadFrom(g_root, name);
        Textfield* t = g_effectsHuge[i].text;
        t->SetSize((unsigned)t->w + 200, (unsigned)t->h);
    }
    g_limitLine = GUI::GetWindowTyped<Window>(g_root, "scaler_limit_underline_2px_02");
    g_craftButton = GUI::GetWindowTyped<Window>(g_root, "button_craft_items_craft");
    g_craftButton->SetOnClick(OnCraft);
    g_craftText = GUI::GetWindowTyped<Textfield>(g_root, "button_craft_items_craft.text");
    g_requiredBg = GUI::GetWindowTyped<Window>(g_root, "bg_item_holder_01");
    for (unsigned i = 0; i < 3; ++i) {
        char name[0x100];
        std::snprintf(name, sizeof name,
                      "item_craft_items_required_holder.craft_items_required_3.item_craft_required_item_%02d", i + 1);
        g_required[i].LoadFrom(g_root, name);
        g_required[i].item.icon->SetOnClick([i] { OnImage(i); });
        g_required[i].find->SetOnClick([i] { OnFind(i); });
        g_required[i].buyNow.root->SetOnClick([i] { OnBuy(i, false); });
    }
    g_plus1 = GUI::GetWindowTypedF<Window>(g_root, "item_craft_items_required_holder.craft_items_required_3.icon_add_%02d", 1);
    g_plus2 = GUI::GetWindowTypedF<Window>(g_root, "item_craft_items_required_holder.craft_items_required_3.icon_add_%02d", 2);
    g_arrow = GUI::GetWindowTyped<Window>(g_root, "craft_arrow");
    g_resultBg = GUI::GetWindowTyped<Window>(g_root, "bg_item_holder_02");
    g_result.LoadFrom(g_root, "item_craft_items_required_holder.craft_items_crafted_01.item_craft_required_item_01");
    for (unsigned i = 0; i < 7; ++i) {
        char name[0x100];
        std::snprintf(name, sizeof name, "craftable_items_holder_masked.crafting_list_item_holder_%02d", i + 1);
        g_list[i].LoadFrom(g_root, name);
        g_list[i].root->SetOnClick([i] { SelectListItem(i); });
        g_list[i].root->SetActionSound("ui_click", 1);
    }
    g_list[6].root->MoveWindow(0, g_list[1].root->y - g_list[0].root->y);
    g_locked.LoadFrom(g_root, "crafting_item_locked");
    g_locked.flagDisabled->SetOnClick(OnLockedFlag);
    for (unsigned i = 0; i < 3; ++i) g_locked.items[i].icon->SetOnClick([i] { OnFind(i); });
    g_professionExplained = GUI::GetWindowTyped<Textfield>(g_root, "profession_explained");
    g_professionScaler = GUI::GetWindowTyped<Window>(g_root, "scaler_prof_01");
    g_zone.LoadFrom(g_root, "scroller_prof_levels");
    for (unsigned n = 0; n < 3; ++n)
        for (unsigned k = 0; k < 8; ++k) {
            g_zone.node[n].items[k].root->SetOnClick([n, k] { OnProfessionLevelItem(n, k); });
            g_zone.node[n].items[k].root->SetActionSound("ui_click", 1);
        }
    for (unsigned i = 0; i < 7; ++i) {
        char name[0x100];
        std::snprintf(name, sizeof name, "professions_holder_masked.profession_list_%02d", i + 1);
        g_professionList[i].LoadFrom(g_root, name);
        g_professionList[i].root->SetOnClick([i] { SelectListItem(i); });
        g_professionList[i].root->SetActionSound("ui_click", 1);
    }
    g_professionList[6].root->MoveWindow(0, g_professionList[1].root->y - g_professionList[0].root->y);
    // The list's scroller (the recipes and the professions share it).
    g_recipes.Init();
    g_scrollContainer = GUI::GetWindowTyped<Window>(g_root, "scroll_bar_tiny_container_crafting");
    g_recipes.scrollbar = GUI::GetWindowTyped<Window>(g_root, "scroll_bar_tiny_container_crafting.scroll_bar_tiny");
    for (ProfessionSkillInfo& n : g_zone.node) n.scroller.scrollbar->SetTexture(g_recipes.scrollbar->texture);
    Window* scaler = GUI::GetWindowTyped<Window>(g_root, "scaler_transparent");
    g_recipes.unkA0 = 7;
    g_recipes.onScroll = UpdateContents;
    g_recipes.itemSize = g_list[1].root->y - g_list[0].root->y;
    g_recipes.perLine = 1;
    g_recipes.horizontal = false;
    g_recipes.track = scaler;
    const int edge = (int)(g_scale * 8.f);
    g_recipes.viewport.right = scaler->w + scaler->x;
    g_recipes.viewport.bottom = (scaler->h + scaler->y) - edge;
    g_recipes.viewport.top = edge + scaler->y;
    g_recipes.viewport.left = scaler->x - (int)(g_scale * 5.f);
    for (CraftItemInfo& l : g_list) {
        l.root->SetClipRect(&g_recipes.viewport);
        g_recipes.windows.push_back(l.root);
    }
    for (ProfessionInfo& pr : g_professionList) {
        pr.root->SetClipRect(&g_recipes.viewport);
        g_recipes.windows.push_back(pr.root);
    }
    g_queue->hideOnOuterClick = true;
    g_queue->fn.update = Update;
    g_queue->fn.zRange = ZRange;
    g_queue->fn.back = OnBack;
    g_queue->fn.show = Show;
    g_hover = new ItemHoverWindow();
    // How many recipes each tab has, and the profession each tab stands for.
    for (Shared::TabHolder& t : g_tabs) t.unk20 = 0;
    g_professions.clear();
    for (unsigned i = 0; Collections::Collection* c = Collections::EnumCollection(i); ++i)
        if (c->tab <= 5) g_tabs[c->tab].unk20 += 1;
    for (unsigned i = 0; Professions::Profession* p = Professions::EnumProfession(i); ++i) {
        if (p->type != Professions::kProfession) continue;
        while (g_professions.size() < p->tab) g_professions.push_back(nullptr);
        g_professions[p->tab - 1] = p;
        if (p->tab > 5) continue;
        Window* icon = g_tabs[p->tab].icon;
        icon->SetTexture(p->icon, true, (unsigned)icon->origW + 0xf, 0, false, 0);
    }
}

}  // namespace CraftItemWindow
