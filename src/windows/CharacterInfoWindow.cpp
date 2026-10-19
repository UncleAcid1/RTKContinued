// CharacterInfoWindow: the hero's equipment and inventory window (the belt's inventory button, the
// portrait). The dressed figure with its eleven equipment slots, the tabs of the inventory and its
// scrolling grid of items, the stats, the set panel and the customisation strip. Port of
// CharacterInfoWindow (libkingdom.so 5.11, @0x27aea0..0x2885e0, Menu_character.xml; statics
// 0x619378..0x61ac0c and 0x60f17c..0x60f1a8).
//
// 5l-1: the window opens and closes (Init, Show, Hide, the tweens, Back, the tabs). Its contents
// (UpdateContents' window part) are 5l-2, the item taps and moves (Click, Move, OnItem, OnEquip ...)
// 5l-3.
#include <cstdio>
#include <set>

#include "engine/IconManager.h"
#include "engine/Render.h"
#include "engine/Resources.h"
#include "game/BuildingHovers.h"
#include "game/Entity.h"
#include "game/EntityManager.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/MetaExpression.h"
#include "game/Squad.h"
#include "game/StringTable.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/ContentScroller.h"
#include "hud/HUD.h"
#include "hud/ItemHoverWindow.h"
#include "hud/Shared.h"
#include "windows/Windows.h"

namespace CharacterInfoWindow {
namespace {

using GUI::Textfield;
using GUI::Window;

// CharacterInfoWindow::StatisticsHolder (0x7c bytes): three stat lines, each in red / white /
// green, with a before and an after value (the compare view) and an icon.
struct StatisticsHolder {
    Window* root = nullptr;                // +0x00 "%s"
    Textfield* red[3] = {};                // +0x04 text_param_%02d_red
    Textfield* white[3] = {};              // +0x10 text_param_%02d_white
    Textfield* green[3] = {};              // +0x1c text_param_%02d_green
    Textfield* beforeRed[3] = {};          // +0x28 text_stats_before_%02d_red
    Textfield* beforeWhite[3] = {};        // +0x34
    Textfield* beforeGreen[3] = {};        // +0x40
    Textfield* afterRed[3] = {};           // +0x4c text_stats_after_%02d_red
    Textfield* afterWhite[3] = {};         // +0x58
    Textfield* afterGreen[3] = {};         // +0x64
    Window* icon[3] = {};                  // +0x70 icon_stats_%02d

    // @0x27c42c
    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        for (int i = 0; i != 3; ++i) {
            red[i] = GUI::GetWindowTypedF<Textfield>(w, "%s.text_param_%02d_red", p, i + 1);
            red[i]->lineSpacing = -3;
        }
        for (int i = 0; i != 3; ++i) {
            white[i] = GUI::GetWindowTypedF<Textfield>(w, "%s.text_param_%02d_white", p, i + 1);
            white[i]->lineSpacing = -3;
        }
        for (int i = 0; i != 3; ++i) {
            green[i] = GUI::GetWindowTypedF<Textfield>(w, "%s.text_param_%02d_green", p, i + 1);
            green[i]->lineSpacing = -3;
        }
        for (int i = 0; i != 3; ++i)
            beforeRed[i] = GUI::GetWindowTypedF<Textfield>(w, "%s.text_stats_before_%02d_red", p, i + 1);
        for (int i = 0; i != 3; ++i)
            beforeWhite[i] = GUI::GetWindowTypedF<Textfield>(w, "%s.text_stats_before_%02d_white", p, i + 1);
        for (int i = 0; i != 3; ++i)
            beforeGreen[i] = GUI::GetWindowTypedF<Textfield>(w, "%s.text_stats_before_%02d_green", p, i + 1);
        for (int i = 0; i != 3; ++i)
            afterRed[i] = GUI::GetWindowTypedF<Textfield>(w, "%s.text_stats_after_%02d_red", p, i + 1);
        for (int i = 0; i != 3; ++i)
            afterWhite[i] = GUI::GetWindowTypedF<Textfield>(w, "%s.text_stats_after_%02d_white", p, i + 1);
        for (int i = 0; i != 3; ++i)
            afterGreen[i] = GUI::GetWindowTypedF<Textfield>(w, "%s.text_stats_after_%02d_green", p, i + 1);
        for (int i = 0; i != 3; ++i) icon[i] = GUI::GetWindowTypedF<Window>(w, "%s.icon_stats_%02d", p, i + 1);
    }
};

// CharacterInfoWindow::CustomItemHolder (0x3a4 bytes): the strip of five customisation pieces
// (hair, eyes ...) that scrolls sideways.
struct CustomItemHolder {
    Window* root = nullptr;                // +0x000 "%s"
    Shared::ItemHolder slots[5];           // +0x004 custom_hold.slot_%02d.inventory_item_icon
    Shared::ContentScroller scroller;      // +0x2ac
    Window* barContainer = nullptr;        // +0x324 (scroller +0x78) scroll_bar_custom_hold

    // @0x27c794
    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        char name[0x80];
        for (int i = 0; i != 5; ++i) {
            std::snprintf(name, sizeof name, "%s.custom_hold.slot_%02d.inventory_item_icon", p, i + 1);
            slots[i].LoadFrom(w, name, false);
        }
        scroller.Init();
        Window* mask = GUI::GetWindowTypedF<Window>(w, "%s.custom_hold.mask_universal", p);
        scroller.viewport = {mask->x, mask->y, mask->x + mask->w, mask->y + mask->h};
        scroller.track = mask;
        for (Shared::ItemHolder& s : slots) {
            s.root->SetClipRect(&scroller.viewport);
            scroller.windows.push_back(s.root);
        }
        barContainer = GUI::GetWindowTypedF<Window>(w, "%s.scroll_bar_custom_hold", p);
        scroller.scrollbar = GUI::GetWindowTypedF<Window>(w, "%s.scroll_bar_custom_hold.scroll_bar_tiny", p);
        scroller.scrollbar->SetTexture(Resources::GetDirectImage("images/scroll.png"), false);
        barContainer->MoveWindow(0, (int)(w->scale * -4.f));
        scroller.horizontal = true;
        scroller.itemSize = slots[1].root->x - slots[0].root->x;
        scroller.perLine = 1;
        scroller.unkA0 = 5;
        scroller.onScroll = UpdateContents;
    }
};

// CharacterInfoWindow::ButtonCustomBuy (0x10 bytes): the crystal button that buys the previewed
// customisation.
struct ButtonCustomBuy {
    Window* root = nullptr;                // +0x00 "%s"
    Textfield* text = nullptr;             // +0x04 text_buy_crystal
    Textfield* price = nullptr;            // +0x08 text_price_crystal
    Window* crystal = nullptr;             // +0x0c icon_35_crystal (takes a depth slot)

    // @0x27c3a4
    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        text = GUI::GetWindowTypedF<Textfield>(w, "%s.text_buy_crystal", p);
        price = GUI::GetWindowTypedF<Textfield>(w, "%s.text_price_crystal", p);
        crystal = GUI::GetWindowTypedF<Window>(w, "%s.icon_35_crystal", p);
        crystal->takesZ = true;
    }
};

// One of the figure's eleven equipment slots (0x28 bytes from 0x619474).
struct EquipSlot {
    Window* root = nullptr;                // +0x00 "%s"
    Window* bg = nullptr;                  // +0x04 item_icon_holder_bg (takes a depth slot)
    GUI::Button* click = nullptr;          // +0x08 clickArea
    Window* selector = nullptr;            // +0x0c ...item_class_character_selector
    Window* broken = nullptr;              // +0x10 ...item_class_character_broken
    Window* itemClass[5] = {};             // +0x14 ...item_class_character_01..05
};

// One cell of the inventory grid (0xd8 bytes from 0x619630): an item with its belt button, its
// shop price and its texts.
struct Cell {
    Window* root = nullptr;                // +0x00 shop_inv_item_holder.shop_item_%02d
    Window* beltButton = nullptr;          // +0x04 button_belt_blue
    Window* boughtBg = nullptr;            // +0x08 slot_locked_bg_bought
    Textfield* price = nullptr;            // +0x0c shop_price_holder.price_no_discount.shop_price
    Textfield* notEnough = nullptr;        // +0x10 ...shop_not_enough
    Window* costIcon = nullptr;            // +0x14 ...icon_cost (takes a depth slot)
    Textfield* requiresLevel = nullptr;    // +0x18 shop_price_holder.shop_requires_level
    Window* confirm = nullptr;             // +0x1c shop_price_holder.icon_gold_confirm (takes a depth slot)
    Textfield* shopText[5] = {};           // +0x20 texts_shop.text_item_class_%02d
    Textfield* bagText[5] = {};            // +0x34 texts_bag.text_item_class_%02d
    Window* bg = nullptr;                  // +0x48 shop_item_bg
    Shared::ItemHolder holder;             // +0x4c inventory_slot_holder_01.inventory_item_icon
};

WindowManager::FunctionalWindow* g_queue = nullptr;   // 0x61ac0c CharacterInfoWindow::wnd
Window* g_root = nullptr;                 // 0x619378
unsigned g_tab = 0;                       // 0x61937c the tab shown (7: another player's equipment)
// The pieces tried on (previewed) on the figure, per equipment binding and per customisation
// binding (RemovePreviewItems clears them). UNVERIFIED (5l-2): their use.
uint32_t g_previewEquip[10] = {};         // 0x619398
uint32_t g_previewCustom[32] = {};        // 0x6193c0
EquipSlot g_equip[11];                    // 0x619474
Shared::BuffHolderManager* g_buffs = nullptr;   // 0x61946c the running potions, three to a line at buff_holder_01
ItemHoverWindow* g_hover = nullptr;       // 0x61962c the card of the tapped item
Cell g_cells[12];                         // 0x619630
Shared::ContentScroller g_scroller;       // 0x61a050 the inventory grid, two cells to a line
Window* g_scrollContainer = nullptr;      // 0x61a0c8 (scroller +0x78) scroll_bar_tiny_container_inventory
CustomItemHolder g_custom;                // 0x61a15c
GUI::TweenEffect* g_tween = nullptr;      // 0x61a514
Window* g_buffHolder = nullptr;           // 0x61a518 buff_holder_01
Window* g_itemHolder = nullptr;           // 0x61a51c Character_item_holder.xml (the figure's pieces)
ItemHoverWindow* g_hover2 = nullptr;      // 0x61a520 the second card. UNVERIFIED (5l-3): the compare card
// The items shown with "NEW" (got less than 15 minutes ago) in this opening of the window, and those
// seen in an earlier one (Hide moves them over), which no longer get it.
std::set<uint32_t> g_newShown;            // 0x61a538
std::set<uint32_t> g_newSeen;             // 0x61a550
// The binding names: the slots' windows, their empty pictures, the customisations' default pieces.
const char* g_slotNames[11] = {           // 0x61a56c
    "inventory_item_icon_head",   "inventory_item_icon_left_hand", "inventory_item_icon_pants",
    "inventory_item_icon_foot",   "inventory_item_icon_jewels",    "inventory_item_icon_neck",
    "inventory_item_icon_right_hand", "inventory_item_icon_armor", "inventory_item_icon_ring1",
    "inventory_item_icon_ring2",  "inventory_item_icon_tool"};
[[maybe_unused]] const char* g_slotBackgrounds[11] = {     // 0x61a598
    "inv_bg_head", "inv_bg_sword", "inv_bg_torso", "inv_bg_foot",  "inv_bg_ring", "inv_bg_amulet",
    "inv_bg_armor", "inv_bg_glove", "inv_bg_belt", "inv_bg_ring", ""};
[[maybe_unused]] const char* g_customNames[32] = {         // 0x61a5c4
    "Customisation_head", "Customisation_eyes", "Customisation_lips",        "Customisation_back",
    "Customisation_none", "Customisation_hair", "Customisation_nose",        "Customisation_facial_hair",
    "Customisation_none", "Customisation_none", "",  "",  "",  "",  "",  "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", ""};
// 0x61a644. UNVERIFIED (5l-2): an order of the customisation bindings.
[[maybe_unused]] unsigned g_customOrder[11] = {1, 3, 5, 7, 10, 2, 4, 6, 8, 10, 0};
Shared::TabHolder g_tabs[8];              // 0x61a670 tab_01..08
Shared::PlayerHolder g_figure;            // 0x61a790 character_item_holder
Textfield* g_setTexts[5] = {};            // 0x61a94c inv_set_info_holder.text_item_class_%02d
GUI::ClipRect g_setClip{};                // 0x61a960 the set's progress bar
StatisticsHolder g_statsLong[2];          // 0x61a970 0x61a9ec stats_holder_long.char_stats_holder_%02d
StatisticsHolder g_statsShort[2];         // 0x61aa68 0x61aae4 stats_holder_short.char_stats_holder_short_%02d
int g_keptHp = 0;                         // 0x61ab78 the HP shown while the window is open
bool g_keepHp = false;                    // 0x61ab7c
bool g_itemFlying = false;                // 0x61ab7d a dragged piece glides back (Update, 5l-3)
float g_itemFlyTime = 0.f;                // 0x61ab80
Window* g_close = nullptr;                // 0x61ab84 x_button.clickArea
Textfield* g_name = nullptr;              // 0x61ab88 text_player_name
Window* g_headerBack = nullptr;           // 0x61ab8c header_back
Textfield* g_header = nullptr;            // 0x61ab90 header_text
Window* g_background = nullptr;           // 0x61ab94 inventory_background
Textfield* g_tabName = nullptr;           // 0x61ab98 text_tab_name
Textfield* g_noItems = nullptr;           // 0x61ab9c text_no_items_in_cat
Window* g_shop = nullptr;                 // 0x61aba0 button_open_shop
Textfield* g_shopText = nullptr;          // 0x61aba4 button_open_shop.text
Window* g_setBar = nullptr;               // 0x61aba8 inv_set_info_holder.progress_bar_set
Window* g_setInfo = nullptr;              // 0x61abac inv_set_info_holder.button_set_info_2
Textfield* g_setInfoText = nullptr;       // 0x61abb0
Window* g_paramBack = nullptr;            // 0x61abb4 param_back_01
Textfield* g_chance = nullptr;            // 0x61abb8 text_chance
ButtonCustomBuy g_customBuy;              // 0x61abbc button_custom_buy
// 0x61abcc: the card to explain once the window is in (OnTweenIn): 1 equip, 4 drink, 2 / 3 kept
// for later. UNVERIFIED (5l-3): ShowInventoryItem sets it.
unsigned g_help = 0;
int g_selectedItem = -1;                  // 0x60f17c the selected cell's item index (-1: none)
int g_selectedEquip = -1;                 // 0x60f180 the selected equipment slot (-1: none)
float g_itemFlyDuration = 1.f;            // 0x60f184
bool g_tablet = false;                    // 0x60f188
// The arrow to put on a cell / an equipment slot once the window is in (ShowInventoryItem, 5l-3),
// and the one it is on.
int g_arrowCell = -1, g_arrowCellShown = -1;     // 0x60f19c 0x60f190
int g_arrowEquip = -1, g_arrowEquipShown = -1;   // 0x60f1a0 0x60f198

// UNVERIFIED (5l-3): the taps on a cell, an equipment slot, a customisation piece and the crystal
// button (OnItem 0x2849ec, OnItemEquipped 0x28526c, OnPreviewItem 0x27bc2c, OnPreviewBuy 0x284488).
void OnItem(unsigned, bool) {}
void OnItemEquipped(unsigned, bool) {}
void OnPreviewItem(unsigned) {}
void OnPreviewBuy(bool) {}

// @0x27dc88. UNVERIFIED (milestone 5, 5q): ItemShopWindow::Show.
void OnShop() { Hide(); }

// @0x27ba44. UNVERIFIED (milestone 5, 5m): SetInfoWindow::SelectSet(the shown set, else 1) and
// SetInfoWindow::Show.
void OnSetInfo() {}

// @0x2857bc
void OnTab(unsigned tab) {
    if (g_tab != tab) RemovePreviewItems();
    g_selectedEquip = -1;
    g_tab = tab;
    g_scroller.firstIndex = 0;
    g_selectedItem = -1;
    g_scroller.HandleMove(-g_scroller.offset);
    UpdateContents();
    BeltBarWindow::SetZ(HUDWindow::GetZ());
    Render::SortRenderLayer(0xe, 1);
}

// @0x28547c
void OnTweenIn() {
    // SoundsManager::PlaySound("ui_inventory_opened"): milestone 6.
    if ((unsigned)(GameState::tutorial - 0x14) < 2) {
        BuildingHovers::CollectAll();
        g_queue->MoveWindowOnTop(true);
    }
    // The arrow asked for by ShowInventoryItem, at a cell or at an equipment slot.
    auto arrowAt = [](const Window* w) {
        TaskInfoWindow::BlockArrowLimitChangeOnce();
        BuildingHovers::SetArrowVisibleWindowLimit(2);
        BuildingHovers::ArrowAt((float)(g_root->x + w->x + w->w / 2), (float)(g_root->y + w->y), false, false, false,
                                false, true, false, false);
    };
    if (g_arrowCell != -1) {
        arrowAt(g_cells[g_arrowCell].root);
        const unsigned i = (unsigned)g_arrowCell;
        BuildingHovers::SetArrowClickCallback(new GUI::Callback([i] { OnItem(i, true); }));
        g_arrowCellShown = g_arrowCell;
        g_arrowCell = -1;
    }
    if (g_arrowEquip != -1) {
        arrowAt(g_equip[g_arrowEquip].root);
        const unsigned i = (unsigned)g_arrowEquip;
        BuildingHovers::SetArrowClickCallback(new GUI::Callback([i] { OnItemEquipped(i, true); }));
        g_arrowEquipShown = g_arrowEquip;
        g_arrowEquip = -1;
    }
    if (g_help == 1 || g_help == 4) {
        // The selection's card opens again, and explains equipping (1) or drinking (4).
        if (g_selectedItem != -1) OnItem((unsigned)(g_selectedItem - (int)g_scroller.firstIndex), true);
        else if (g_selectedEquip != -1) OnItemEquipped((unsigned)g_selectedEquip, true);
        if (g_help == 1) {
            const int st = GameState::secondTutorial;
            g_hover->HelpEquip(st == 0x88 || st == 0xa4 || (unsigned)(GameState::tutorial - 0x14) < 2);
        }
        if (g_help == 4) g_hover->HelpDrink(false);
    }
    if (g_help - 2 > 1) g_help = 0;
}

// @0x27b9d0
void OnTweenOut() {
    WindowManager::WindowHide(false);
    g_queue->shown = false;
    g_queue->MoveWindowDown(true);
    g_buffs->SetVisibility(false);
    PlayerTopWindow::RefreshHP();
    GameState::DropGamePauseState();
    if ((unsigned)(GameState::tutorial - 0x14) < 2) GameState::tutorial = 0x16;
    RemovePreviewItems();
}

// @0x27dbcc: Back closes an open card first.
bool OnBack() {
    if (!g_queue->shown) return false;
    if (!g_hover->shownHover) {
        if (g_itemFlying) g_itemFlyTime = g_itemFlyDuration;
        Hide();
        return true;
    }
    g_hover->Hide();
    if (g_hover2->shownHover) g_hover2->Hide();
    return true;
}

// @0x27aef4
float ZRange() { return g_queue->shown ? 0.12f : 0.001f; }

// (0x286a64) PORT (5l-3 replaces it with the original's Click: the scrollers, dragging pieces between
// the grid, the figure and the belt): the cards, then the window's own buttons.
bool Click(int x, int y, bool pressed) {
    if (g_hover->Click(x, y, pressed)) {
        if (g_hover2->shownHover) g_hover2->Hide();
        return true;
    }
    if (g_hover2->Click(x, y, pressed)) return true;
    if (!g_root->visibleSelf) {
        g_root->Click(x, y, pressed, false);
        return false;
    }
    if (BuildingHovers::ClickOnArrow(x, y, pressed)) return true;
    return g_root->Click(x, y, pressed, false);
}

// @0x27c158
void SetZ(float z) {
    if (!g_queue->shown) return;
    g_root->SetZ(z);
    g_root->SetPosition(g_tween->GetCenteredX(), g_tween->GetCenteredY());
    g_buffs->SetZ(g_buffHolder->z);
}

}  // namespace

WindowManager::FunctionalWindow* Queue() {
    if (!g_queue) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.click = Click;
        f.setZ = SetZ;
        f.hide = Hide;
        // UNVERIFIED (5l-3): Move (0x286444), the drag of a piece.
        g_queue = new WindowManager::FunctionalWindow("CharacterInfo", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_root && g_root->visibleSelf; }
float GetZ() { return g_root->z; }
bool CanRaiseBelt() { return g_root && g_root->visibleSelf && g_tab != 7; }

// @0x27afc4
void RemovePreviewItems() {
    for (uint32_t& p : g_previewEquip) p = 0;
    for (uint32_t& p : g_previewCustom) p = 0;
}

// @0x28407c
void ShowInventoryTab(unsigned tab) {
    g_tab = tab;
    g_selectedEquip = -1;
    g_selectedItem = -1;
    UpdateContents();
}

// @0x27bae4
void ShowBuffAnimation(ItemHoverWindow* hover) {
    if (hover != g_hover || g_selectedItem == -1) return;
    Render::Sprite* s = g_cells[g_selectedItem - (int)g_scroller.firstIndex].holder.icon.icon->sprite;
    if (!s) return;
    int x, y;
    BeltBarWindow::GetAdjustedBuffZoneLocation(x, y, (int)s->w, (int)s->h);
    BuildingHovers::AddItemMovement(s, x, y, true, 1.5f, -1, -1, false, true);
}

// @0x27dc98
void Init() {
    g_tablet = GUI::IsTabletVersion();
    const float hdpi = GUI::GetHighDPIScaleFactor();
    GUI::SetHighDPIScaleFactor(hdpi * 0.96f);
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Menu_character.xml", "Menu_character.png", -1.f,
                             GUI::IsHighDPIVersion() ? 0x28 : 0, 0, 0, 0, false, 1.f);
    Queue()->RegisterTopWindow(g_root);
    GUI::SetHighDPIScaleFactor(hdpi);
    const float scale = g_root->scale;   // 0x60f18c
    g_root->SetVisibility(false);
    g_tween = GUI::CreateTweenEffect(g_root, nullptr, OnTweenIn, OnTweenOut);
    g_tween->swingIn.clear();   // (SetSwingInSound(nullptr))
    g_tween->CenterWith(0, (int)(scale * -50.f), (int)(scale * (GUI::IsSmallScreenVersion() ? -100.f : -22.f)), 0);
    g_close = GUI::GetWindowTyped<GUI::Button>(g_root, "x_button.clickArea");
    g_close->SetOnClick(Hide);
    g_close->SetActionSound("", 1);
    g_name = GUI::GetWindowTyped<Textfield>(g_root, "text_player_name");
    g_headerBack = GUI::GetWindowTyped<Window>(g_root, "header_back");
    g_header = GUI::GetWindowTyped<Textfield>(g_root, "header_text");
    g_background = GUI::GetWindowTyped<Window>(g_root, "inventory_background");
    g_buffHolder = GUI::GetWindowTypedF<Window>(g_root, "buff_holder_01");
    g_buffHolder->takesZ = true;
    g_figure.LoadFrom(g_root, "character_item_holder");
    char name[0x100];
    for (unsigned i = 0; i != 8; ++i) {
        std::snprintf(name, sizeof name, "tab_%02d", i + 1);
        g_tabs[i].LoadFrom(g_root, name, false);
        g_tabs[i].root->SetOnClick([i] { OnTab(i); });
        g_tabs[i].root->SetActionSound("ui_tab", 1);
    }
    g_tabName = GUI::GetWindowTyped<Textfield>(g_root, "text_tab_name");
    for (unsigned i = 0; i != 11; ++i) {
        EquipSlot& e = g_equip[i];
        const char* n = g_slotNames[i];
        e.root = GUI::GetWindowTypedF<Window>(g_root, "%s", n);
        e.bg = GUI::GetWindowTypedF<Window>(g_root, "%s.item_icon_holder_bg", n);
        e.bg->takesZ = true;
        e.selector = GUI::GetWindowTypedF<Window>(
            g_root, "%s.inventory_item_character_icon_holder.item_class_character_selector", n);
        e.broken = GUI::GetWindowTypedF<Window>(
            g_root, "%s.inventory_item_character_icon_holder.item_class_character_broken", n);
        for (int k = 0; k != 5; ++k)
            e.itemClass[k] = GUI::GetWindowTypedF<Window>(
                g_root, "%s.inventory_item_character_icon_holder.item_class_character_%02d", n, k + 1);
        e.click = GUI::GetWindowTypedF<GUI::Button>(g_root, "%s.clickArea", n);
        e.click->SetOnClick([i] { OnItemEquipped(i, true); });
        e.click->SetActionSound("ui_click", 1);
    }
    for (unsigned i = 0; i != 12; ++i) {
        Cell& c = g_cells[i];
        const unsigned n = i + 1;
        c.root = GUI::GetWindowTypedF<Window>(g_root, "shop_inv_item_holder.shop_item_%02d", n);
        c.root->SetOnClick([i] { OnItem(i, true); });
        c.root->SetActionSound("ui_click", 1);
        c.beltButton = GUI::GetWindowTypedF<Window>(g_root, "shop_inv_item_holder.shop_item_%02d.button_belt_blue", n);
        c.boughtBg = GUI::GetWindowTypedF<Window>(g_root, "shop_inv_item_holder.shop_item_%02d.slot_locked_bg_bought", n);
        c.price = GUI::GetWindowTypedF<Textfield>(
            g_root, "shop_inv_item_holder.shop_item_%02d.shop_price_holder.price_no_discount.shop_price", n);
        c.notEnough = GUI::GetWindowTypedF<Textfield>(
            g_root, "shop_inv_item_holder.shop_item_%02d.shop_price_holder.price_no_discount.shop_not_enough", n);
        c.costIcon = GUI::GetWindowTypedF<Window>(
            g_root, "shop_inv_item_holder.shop_item_%02d.shop_price_holder.price_no_discount.icon_cost", n);
        c.costIcon->takesZ = true;
        c.requiresLevel = GUI::GetWindowTypedF<Textfield>(
            g_root, "shop_inv_item_holder.shop_item_%02d.shop_price_holder.shop_requires_level", n);
        c.confirm = GUI::GetWindowTypedF<Window>(
            g_root, "shop_inv_item_holder.shop_item_%02d.shop_price_holder.icon_gold_confirm", n);
        c.confirm->takesZ = true;
        for (int k = 0; k != 5; ++k)
            c.shopText[k] = GUI::GetWindowTypedF<Textfield>(
                g_root, "shop_inv_item_holder.shop_item_%02d.texts_shop.text_item_class_%02d", n, k + 1);
        for (int k = 0; k != 5; ++k)
            c.bagText[k] = GUI::GetWindowTypedF<Textfield>(
                g_root, "shop_inv_item_holder.shop_item_%02d.texts_bag.text_item_class_%02d", n, k + 1);
        c.bg = GUI::GetWindowTypedF<Window>(g_root, "shop_inv_item_holder.shop_item_%02d.shop_item_bg", n);
        std::snprintf(name, sizeof name,
                      "shop_inv_item_holder.shop_item_%02d.inventory_slot_holder_01.inventory_item_icon", n);
        c.holder.LoadFrom(g_root, name, false);
        c.holder.icon.lockSmall->SetTexture(IconManager::GetIcon("gold_16_lock", false), true);
    }
    g_noItems = GUI::GetWindowTyped<Textfield>(g_root, "text_no_items_in_cat");
    g_shop = GUI::GetWindowTyped<Window>(g_root, "button_open_shop");
    g_shop->SetOnClick(OnShop);
    GUI::GetWindowTyped<Window>(g_root, "button_open_shop.icon_placeholder_35")
        ->SetTexture(IconManager::GetIcon("profession_hauler", false), true);
    g_shopText = GUI::GetWindowTyped<Textfield>(g_root, "button_open_shop.text");
    g_setBar = GUI::GetWindowTyped<Window>(g_root, "inv_set_info_holder.progress_bar_set");
    g_setBar->SetFrame(100, false);
    for (int k = 0; k != 5; ++k)
        g_setTexts[k] = GUI::GetWindowTypedF<Textfield>(g_root, "inv_set_info_holder.text_item_class_%02d", k + 1);
    g_setInfo = GUI::GetWindowTyped<Window>(g_root, "inv_set_info_holder.button_set_info_2");
    g_setInfo->SetOnClick(OnSetInfo);
    g_setInfoText = GUI::GetWindowTyped<Textfield>(g_root, "inv_set_info_holder.button_set_info_2.text");
    g_setClip = {g_setBar->x, 0, g_setBar->x + g_setBar->w, 0x400};
    g_setBar->SetClipRect(&g_setClip);
    for (int k = 0; k != 2; ++k) {
        std::snprintf(name, sizeof name, "stats_holder_long.char_stats_holder_%02d", k + 1);
        g_statsLong[k].LoadFrom(g_root, name);
    }
    for (int k = 0; k != 2; ++k) {
        std::snprintf(name, sizeof name, "stats_holder_short.char_stats_holder_short_%02d", k + 1);
        g_statsShort[k].LoadFrom(g_root, name);
    }
    for (int k = 0; k != 2; ++k) {
        StatisticsHolder& a = k == 0 ? g_statsLong[0] : g_statsShort[0];
        a.icon[0]->SetTexture(IconManager::GetIcon("gold_16_health", false), true);
        a.icon[1]->SetTexture(IconManager::GetIcon("gold_16_attack", false), true);
        a.icon[2]->SetTexture(IconManager::GetIcon("gold_16_leadership", false), true);
        StatisticsHolder& b = k == 0 ? g_statsLong[1] : g_statsShort[1];
        b.icon[0]->SetTexture(IconManager::GetIcon("gold_16_initiative", false), true);
        b.icon[1]->SetTexture(IconManager::GetIcon("gold_16_defence_melee", false), true);
        b.icon[2]->SetTexture(IconManager::GetIcon("gold_16_defence", false), true);
    }
    g_paramBack = GUI::GetWindowTyped<Window>(g_root, "param_back_01");
    g_chance = GUI::GetWindowTyped<Textfield>(g_root, "text_chance");
    g_custom.LoadFrom(g_root, "custom_item_holder");
    for (unsigned i = 0; i != 5; ++i) g_custom.slots[i].root->SetOnClick([i] { OnPreviewItem(i); });
    g_customBuy.LoadFrom(g_root, "button_custom_buy");
    g_customBuy.root->SetOnClick([] { OnPreviewBuy(false); });
    g_itemHolder = GUI::RegisterUI("../resource/kingdom_icons/1Original/Character_item_holder.xml", "", 1.f, 0, 0, 0, 0,
                                   false, 1.f);
    g_scroller.Init();
    Window* mask = GUI::GetWindowTyped<Window>(g_root, "shop_inv_item_holder.mask_inventory_items");
    g_scroller.viewport = {mask->x, mask->y, mask->x + mask->w, mask->y + mask->h};
    g_scroller.track = mask;
    for (Cell& c : g_cells) {
        c.root->SetClipRect(&g_scroller.viewport);
        g_scroller.windows.push_back(c.root);
    }
    g_scrollContainer = GUI::GetWindowTyped<Window>(g_root, "scroll_bar_tiny_container_inventory");
    g_scroller.scrollbar = GUI::GetWindowTyped<Window>(g_root, "scroll_bar_tiny_container_inventory.scroll_bar_tiny");
    g_scroller.horizontal = false;
    g_scroller.onScroll = UpdateContents;
    g_scroller.itemSize = g_cells[2].root->y - g_cells[0].root->y;
    g_scroller.unkA0 = 0xc;
    g_scroller.perLine = 2;
    g_scroller.padEnd = (int)(scale * 20.f);
    RemovePreviewItems();
    // UNVERIFIED (5l-2 / 5l-3): fn.update = Update (0x285a1c): the item timers, the dragged piece,
    // the buff row's place and its Update.
    g_queue->fn.zRange = ZRange;
    g_queue->fn.back = OnBack;
    g_queue->fn.show = Show;
    g_queue->hideOnOuterClick = true;
    g_hover = new ItemHoverWindow();
    g_hover2 = new ItemHoverWindow();
    g_buffs = new Shared::BuffHolderManager(0x18, 3, 0, 1);
}

// @0x285840: in the city only (in the tutorial only at its step 0x15), never in an arena.
void Show() {
    if (!GameState::IsPlayerCity()) return;
    if (GameState::IsTutorial() && GameState::tutorial != 0x15) return;
    if (GameState::IsArenaCombatActive()) {
        PopupWindow::Show(StringTable::GetString("CAN_NOT_OPEN_IN_ARENA"), PopupWindow::Hide, nullptr, nullptr, nullptr,
                          nullptr);
        return;
    }
    const bool wasShown = g_root->visibleSelf;
    g_root->SetVisibility(true);
    g_buffs->SetVisibility(true);
    g_queue->shown = true;
    g_queue->MoveWindowOnTop(true);
    if (!wasShown) {
        WindowManager::WindowShow(false);
        g_tween->AnimateIn();
        GameState::RaiseGamePauseState();
    }
    g_keptHp = 0;
    g_selectedEquip = -1;
    g_selectedItem = -1;
    g_scroller.firstIndex = 0;
    g_keepHp = true;
    g_scroller.HandleMove(-g_scroller.offset);
    // UNVERIFIED (milestone 5, 5q): ItemShopWindow::PrepareSpecialEventCost.
    UpdateContents();
    g_root->PreloadTextures();   // GUI::PreloadWindow
}

// @0x27d94c: the items shown as new this time are new no more.
void Hide() {
    if (g_root->visibleSelf) g_tween->AnimateOut();
    if (Entity* player = EntityManager::GetPlayer())
        if (player->GetHP() < player->GetHpMax()) GameState::UpdatePlayerRegenerationState();
    g_keepHp = false;
    g_keptHp = 0;
    g_newSeen.insert(g_newShown.begin(), g_newShown.end());   // (FUN_0027d814: the set's insert)
    g_hover->Hide();
}

}  // namespace CharacterInfoWindow

// The stat part of UpdateContents (5l-2 adds the window's).
namespace CharacterInfoWindow {
namespace {
void ApplyEquipment(GameState::PlayerItem* it, Entity* player) {
    if (it && it->info && it->info->meta && !it->IsBroken()) it->info->meta->OnApplyItemEffect(player);
}
}  // namespace

// UNVERIFIED (milestone 5, 5l-2): the window's contents (tabs, item holders, the player picture, the
// stat texts) and its mode 7 (another player's equipment); Sets::FindActiveSetState and the
// complete sets' effects (no sets yet).
void UpdateContents() {
    Entity* player = EntityManager::GetPlayer();
    player->ResetStats(true);
    if (BaseSquad* s = player->GetSquad())
        for (unsigned i = 1; i < (unsigned)s->GetSoldierCount(); ++i)
            if (Entity* m = s->GetSoldier(i)) m->ResetStats(false);
    // (PvP::PlayerEquipmentInfo::GetPlayerEquipment copies the bindings; its records are found again
    // by unique id.)
    for (unsigned i = 0; i < 10; ++i) {
        GameState::PlayerItem* it = GameState::GetItemAt(i);
        ApplyEquipment(it ? GameState::GetItemByUniqueID(it->uniqueId) : nullptr, player);
    }
    for (unsigned i = 1; i < 10; ++i) {
        GameState::PlayerItem* it = GameState::GetCustomizationAt(i);
        ApplyEquipment(it ? GameState::GetItemByUniqueID(it->uniqueId) : nullptr, player);
    }
    for (unsigned i = 0; Items::ItemInfo* info = Items::EnumItemInfo(i); ++i) {
        if (info->type != 0xe) continue;
        // (the window keeps each buff's remaining time for its list)
        if (GameState::GetItemBuffRemainingTime(info->id) == 0 || !info->meta) continue;
        info->meta->OnApplyItemEffect(player);
        info->meta->OnApplyItemBuffEffect(player);
    }
    BeltBarWindow::UpdateContents();
    if (!GameState::IsArenaCombatActive()) BeltBarWindow::UpdateBeltItems();
}

}  // namespace CharacterInfoWindow
