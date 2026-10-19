// CharacterInfoWindow: the hero's equipment and inventory window (the belt's inventory button, the
// portrait). The dressed figure with its eleven equipment slots, the tabs of the inventory and its
// scrolling grid of items, the stats, the set panel and the customisation strip. Port of
// CharacterInfoWindow (libkingdom.so 5.11, @0x27aea0..0x2885e0, Menu_character.xml; statics
// 0x619378..0x61ac0c and 0x60f17c..0x60f1a8).
//
// The tabs: 0 everything (the newest first, at most 32), 1 weapons, 2 armour, 3 potions and orbs,
// 4 gems, decorations and belts, 5 quest items, 6 crafting, 7 the customisations (the figure's face,
// hair ...: the slots show the customisation bindings and the grid the pieces on sale).
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <vector>

#include "engine/IconManager.h"
#include "engine/Render.h"
#include "engine/Resources.h"
#include "engine/Timer.h"
#include "game/BuildingHovers.h"
#include "game/Entity.h"
#include "game/EntityManager.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/MetaData.h"
#include "game/MetaExpression.h"
#include "game/PlayerEquipmentInfo.h"
#include "game/Professions.h"
#include "game/Rand48.h"
#include "game/Collections.h"
#include "game/Sets.h"
#include "game/Setting.h"
#include "game/Squad.h"
#include "game/StringTable.h"
#include "game/Tasks.h"
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
    uint32_t id = 0;                       // +0xd4 the item shown
};

WindowManager::FunctionalWindow* g_queue = nullptr;   // 0x61ac0c CharacterInfoWindow::wnd
Window* g_root = nullptr;                 // 0x619378
unsigned g_tab = 0;                       // 0x61937c the tab shown (7: the customisations)
// CharacterInfoWindow::MixedItem (0xc bytes): one grid entry, an owned item or (data null) a piece
// that is not owned (an active buff, a recent item of the shop, a customisation on sale).
struct MixedItem {
    GameState::PlayerItem* data = nullptr;   // +0x00
    Items::ItemInfo* info = nullptr;         // +0x04
    uint32_t time = 0;                       // +0x08 tab 0's sort key (GetItemRecentTime)
};
std::vector<MixedItem> g_items;           // 0x619380 the tab's grid (GetItemsForTabAndMode)
// The pieces tried on (previewed) on the figure in the customisation tab, per equipment binding and
// per customisation binding (RemovePreviewItems clears them).
Items::ItemInfo* g_previewEquip[10] = {};     // 0x619398
Items::ItemInfo* g_previewCustom[32] = {};    // 0x6193c0
// What each slot shows: the worn item of each binding (10: the profession tool), or in the
// customisation tab the customisation of g_customOrder[i].
GameState::PlayerItem* g_equipped[11] = {};   // 0x619440
uint32_t g_shownSet = 0;                  // 0x619470 the set of the set panel (OnSetInfo)
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
std::map<uint32_t, uint32_t> g_buffTimes; // 0x61a524 each buff item's seconds left (UpdateItemTimers)
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
const char* g_slotBackgrounds[11] = {     // 0x61a598
    "inv_bg_head", "inv_bg_sword", "inv_bg_torso", "inv_bg_foot",  "inv_bg_ring", "inv_bg_amulet",
    "inv_bg_armor", "inv_bg_glove", "inv_bg_belt", "inv_bg_ring", ""};
const char* g_customNames[32] = {         // 0x61a5c4
    "Customisation_head", "Customisation_eyes", "Customisation_lips",        "Customisation_back",
    "Customisation_none", "Customisation_hair", "Customisation_nose",        "Customisation_facial_hair",
    "Customisation_none", "Customisation_none", "",  "",  "",  "",  "",  "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", ""};
// 0x61a644: the customisation binding each slot shows in the customisation tab.
unsigned g_customOrder[11] = {1, 3, 5, 7, 10, 2, 4, 6, 8, 10, 0};
Shared::TabHolder g_tabs[8];              // 0x61a670 tab_01..08
Shared::PlayerHolder g_figure;            // 0x61a790 character_item_holder
Textfield* g_setTexts[5] = {};            // 0x61a94c inv_set_info_holder.text_item_class_%02d
GUI::ClipRect g_setClip{};                // 0x61a960 the set's progress bar
StatisticsHolder g_statsLong[2];          // 0x61a970 0x61a9ec stats_holder_long.char_stats_holder_%02d
StatisticsHolder g_statsShort[2];         // 0x61aa68 0x61aae4 stats_holder_short.char_stats_holder_short_%02d
int g_keptHp = 0;                         // 0x61ab78 the HP shown while the window is open
bool g_keepHp = false;                    // 0x61ab7c
bool g_itemFlying = false;                // 0x61ab7d the dragged / tapped piece glides (Update)
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
// The piece that glides (the dragged one, or one moving between the grid, a slot and the belt), and
// the glow that turns behind it.
Render::Sprite* g_flySprite = nullptr;    // 0x61abd0
float g_flyFromX = 0, g_flyFromY = 0;     // 0x61abd4 0x61abd8
float g_flyToX = 0, g_flyToY = 0;         // 0x61abdc 0x61abe0
float g_time = 0.f;                       // 0x61abe4 the pulses' clock
bool g_glowWanted = false;                // 0x61abe8
Render::Sprite* g_glow = nullptr;         // 0x61abec
int g_lastSecond = 0;                     // 0x61abf0
// The drag: a press on a filled slot or on a cell, then a move past the threshold.
bool g_draggingSlot = false;              // 0x61abf4
bool g_draggingGrid = false;              // 0x61abf5
bool g_pressedGrid = false;               // 0x61abf6
int g_pressX = 0;                         // 0x61abf8
bool g_scrollMoved = false;               // 0x61abfc the press became a scroll: the release does nothing
bool g_pressOnIcon = false;               // 0x61abfd the press was on the cell's picture
int g_pressY = 0, g_pressY2 = 0;          // 0x61ac00 0x61ac04
bool g_pressedSlot = false;               // 0x61ac08
int g_dragCell = -1;                      // 0x60f194 the cell whose picture is hidden while a piece glides to it
int g_dragSlot = -1;                      // 0x60f1a4 the slot likewise
int g_pressedCell = -1;                   // 0x60f1a8 the grid index pressed
// 0x61ab60: the tool chosen per profession (OnEquip; GetToolOverride drops one no longer owned).
std::map<uint32_t, uint32_t> g_toolOverrides;
// 0x61abcc: the card to explain once the window is in (OnTweenIn): 1 equip, 4 drink, 2 / 3 kept
// for later. UNVERIFIED (5l-3): ShowInventoryItem sets it.
unsigned g_help = 0;
uint32_t g_lastProfession = 3;            // 0x60f178 the profession whose tool the tool slot shows
int g_selectedItem = -1;                  // 0x60f17c the selected cell's item index (-1: none)
int g_selectedEquip = -1;                 // 0x60f180 the selected equipment slot (-1: none)
float g_itemFlyDuration = 1.f;            // 0x60f184
bool g_tablet = false;                    // 0x60f188
// The arrow to put on a cell / an equipment slot once the window is in (ShowInventoryItem, 5l-3),
// and the one it is on.
int g_arrowCell = -1, g_arrowCellShown = -1;     // 0x60f19c 0x60f190
int g_arrowEquip = -1, g_arrowEquipShown = -1;   // 0x60f1a0 0x60f198

// @0x27ae4c: tab 0's order, the newest first.
bool SortByTime(const MixedItem& a, const MixedItem& b) { return a.time > b.time; }
// @0x27ae64: the customisations by binding, then by sort id.
bool SortBySlot(const MixedItem& a, const MixedItem& b) {
    if (a.info->slot != b.info->slot) return a.info->slot < b.info->slot;
    return a.info->sortId < b.info->sortId;
}

// A piece's picture as a sprite at window w (its bottom left, screen space), to glide.
Render::Sprite* MakeFlySprite(Render::Texture* tex, const Window* w) {
    Render::Sprite* s = Render::CreateSprite(tex, 0xf, false, false);
    s->screenSpace = true;
    Render::SetShaderType(s, 1);
    Render::SetPosition(s, (float)(g_root->x + w->x), (float)(w->h + w->y + g_root->y), 0.001f);
    return s;
}

// The glide's start where the sprite is now.
void FlyFromHere(float duration) {
    g_itemFlyTime = 0.f;
    g_itemFlying = true;
    g_itemFlyDuration = duration;
    g_flyFromX = g_flySprite->x;
    g_flyFromY = g_flySprite->y;
}

// The pictures hidden for a glide come back, and the glide ends.
void ShowHiddenPictures() {
    if (g_dragSlot != -1) g_equip[g_dragSlot].bg->SetVisibility(true);
    g_dragSlot = -1;
    if (g_dragCell != -1) g_cells[g_dragCell].holder.icon.icon->SetVisibility(true);
    g_dragCell = -1;
}

// @0x2840a8: the customisation tab's random looks: each binding 2..9 but 7 (not the head) gets a
// random piece of the grid or none (on binding 5 never 0x2ef / 0x2f0); an owned piece is put on,
// one not owned is tried on.
void OnSelectRandomCustomizations() {
    if (g_tab != 7) return;
    g_previewCustom[1] = nullptr;
    std::vector<Items::ItemInfo*> cands;
    for (unsigned b = 2; b != 10; ++b) {
        if (b == 7) continue;
        cands.clear();
        for (const MixedItem& m : g_items)
            if (m.info->slot == b) cands.push_back(m.info);
        if (cands.empty()) continue;
        const unsigned r = (unsigned)Rand48::lrand48() % (unsigned)(cands.size() + 1);
        if (r == cands.size() || (b == 5 && (cands[r]->id == 0x2ef || cands[r]->id == 0x2f0))) {
            g_previewCustom[b] = nullptr;
            continue;
        }
        Items::ItemInfo* p = cands[r];
        if (GameState::GetItemAmount(p->id, false) != 0) {
            GameState::BindCustomizationTo(p->slot, GameState::GetFirstItemByID(p->id, false));
            g_previewCustom[p->slot] = nullptr;
            continue;
        }
        g_previewCustom[p->slot] = p;
    }
    UpdateContents();
}

// @0x2849ec: a tap on a cell: the item is selected and (show) its card opens beside it (with the
// worn piece of its binding beside that, to compare); a cell half out of the view scrolls in
// instead. Special items: the new hero (0x2f8 / 0x2f9: SelectHQWindow), the random looks (0x2fe);
// in the customisation tab the piece is tried on or off (owned: put on or off); a profession point
// item turns into points.
void OnItem(unsigned cell, bool show) {
    g_selectedEquip = -1;
    g_selectedItem = (int)(cell + g_scroller.firstIndex);
    UpdateContents();
    if (!show) return;
    const Window* r = g_cells[cell].root;
    const int h3 = r->h / 3;
    if (r->h + r->y > h3 + g_scroller.viewport.bottom) {
        if (r->y < g_scroller.viewport.top - h3) g_scroller.bounce += (float)g_scroller.itemSize;
        else g_scroller.bounce -= (float)g_scroller.itemSize;
        return;
    }
    if (r->y < g_scroller.viewport.top - h3) {
        g_scroller.bounce += (float)g_scroller.itemSize;
        return;
    }
    const SelectedItem sel = GetSelectedItem();
    Items::ItemInfo* info = sel.info;
    if (!info) return;   // PORT: the original reads through it
    const uint32_t id = info->id;
    if (id == 0x2fe) {
        OnSelectRandomCustomizations();
        return;
    }
    if (id - 0x2f8 <= 1) {
        SelectHQWindow::SetMode(id == 0x2f8 ? 1 : 2);
        SelectHQWindow::Show();
        return;
    }
    if (g_tab == 7) {
        // (PORT: bounds on the binding, which indexes the preview lists.)
        if (info->type == 0x11) {
            if (info->slot < 32) {
                if (GameState::GetItemAmount(id, false) == 0) {
                    g_previewCustom[info->slot] = g_previewCustom[info->slot] == info ? nullptr : info;
                } else {
                    GameState::PlayerItem* first = GameState::GetFirstItemByID(id, false);
                    GameState::BindCustomizationTo(info->slot,
                                                   GameState::GetCustomizationAt(info->slot) == first ? nullptr : first);
                    g_previewCustom[info->slot] = nullptr;
                }
            }
            // Only pieces of one price currency are tried on together.
            for (Items::ItemInfo*& p : g_previewCustom)
                if (p && p->costItem != info->costItem) p = nullptr;
        } else {
            if (info->slot < 10) {
                if (GameState::GetItemAmount(id, false) == 0) {
                    g_previewEquip[info->slot] = g_previewEquip[info->slot] == info ? nullptr : info;
                } else {
                    GameState::PlayerItem* first = GameState::GetFirstItemByID(id, false);
                    GameState::BindItemTo(info->slot, GameState::GetItemAt(info->slot) == first ? nullptr : first);
                    g_previewEquip[info->slot] = nullptr;
                }
            }
            for (Items::ItemInfo*& p : g_previewEquip)
                if (p && p->costItem != info->costItem) p = nullptr;
        }
        g_selectedItem = -1;
        UpdateContents();
        return;
    }
    if (Professions::Profession* p = Professions::GetProfessionForPointItem(id)) {
        // A profession's point item: all of them become its points.
        GameState::AddProfessionPoints(p->id, GameState::GetItemAmount(id, false));
        GameState::RemoveItem(id, GameState::GetItemAmount(id, false));
        g_selectedItem = -1;
        UpdateContents();
        return;
    }
    GameState::PlayerItem* data = sel.data;
    GameState::PlayerItem* worn = nullptr;
    bool compare = false;
    if (info->slot != 0) {
        worn = g_equipped[GameState::ExternalItemBindingToInternal(info->slot)];
        compare = worn && data != worn;
    }
    g_hover->Show();
    g_hover->SetItem(StringTable::GetString("INVENTORY_UNEQUIPPED_ITEM"), data, info, data != nullptr, true, nullptr,
                     false, false, false, false);
    const Window* h = g_cells[g_selectedItem - (int)g_scroller.firstIndex].holder.root;
    const int x = g_root->x + h->x, y = g_root->y + h->y, w3 = h->w / 3, hh3 = h->h / 3;
    if (compare) {
        // (The original takes a third of the width for the top too.)
        g_hover->SetTargetRect(x + w3, y + w3, x + 2 * w3, y + 2 * hh3, ItemHoverWindow::kNW);
        g_hover->RecomputePosition();
        g_hover->SetZ(0.001f);
        g_hover2->Show();
        g_hover2->SetItem(StringTable::GetString("INVENTORY_EQUIPPED_ITEM"), worn, worn->info, true, true, nullptr, false,
                          false, false, false);
        const Window* e = g_equip[GameState::ExternalItemBindingToInternal(info->slot)].root;
        const int ex = g_root->x + e->x, ey = g_root->y + e->y, ew3 = e->w / 3, eh3 = e->h / 3;
        g_hover2->SetTargetRect(ex + ew3, ey + ew3, ex + 2 * ew3, ey + 2 * eh3, ItemHoverWindow::kSE);
        g_hover2->RecomputePosition();
        g_hover2->SetZ(0.001f);
    } else {
        g_hover->SetTargetRect(x + w3, y + w3, x + 2 * w3, y + 2 * hh3, ItemHoverWindow::kE);
        g_hover->RecomputePosition();
        g_hover->SetZ(0.001f);
    }
    Render::SortRenderLayer(0xe, 1);
    if (g_help == 2 && g_arrowCellShown == (int)cell) {
        g_hover->HelpImprove(false);
        g_help = 0;
    }
}

// @0x28526c: a tap on a slot: it is selected and the worn piece's card opens.
void OnItemEquipped(unsigned slot, bool show) {
    g_selectedItem = -1;
    g_selectedEquip = (int)slot;
    UpdateContents();
    if (!show) return;
    const SelectedItem sel = GetSelectedItem();
    if (sel.data) {
        g_hover->Show();
        g_hover->SetItem(StringTable::GetString("INVENTORY_EQUIPPED_ITEM"), sel.data, sel.data->info, true, true, nullptr,
                         false, false, false, false);
        const Window* e = g_equip[slot].root;
        const int x = g_root->x + e->x, y = g_root->y + e->y;
        g_hover->SetTargetRect(x, y, x + e->w, y + e->h, ItemHoverWindow::kSW);
        g_hover->RecomputePosition();
        g_hover->SetZ(0.001f);
        Render::SortRenderLayer(0xe, 1);
    }
    if (g_help == 2 && g_arrowEquipShown == (int)slot) {
        g_hover->HelpImprove(false);
        g_help = 0;
    } else if (g_help == 3 && g_arrowEquipShown == (int)slot) {
        g_hover->HelpRepair(false);
        g_help = 0;
    }
}

// @0x27bc2c: a tap on the n-th piece of the customisation strip: its card.
void OnPreviewItem(unsigned n) {
    for (unsigned k = 0, shown = 0; shown <= 4 && k <= 31; ++k) {
        Items::ItemInfo* p = g_previewCustom[k];
        if (!p) continue;
        if (n == 0) {
            g_hover->Show();
            g_hover->SetItem(StringTable::GetString("INVENTORY_EQUIPPED_ITEM"), nullptr, p, false, false, nullptr, true,
                             false, false, false);
            const Window* s = g_custom.slots[shown].root;
            const int x = g_root->x + s->x, y = g_root->y + s->y;
            g_hover->SetTargetRect(x, y, x + s->w, y + s->h, ItemHoverWindow::kS);
            g_hover->RecomputePosition();
            g_hover->SetZ(0.001f);
            Render::SortRenderLayer(0xe, 1);
            return;
        }
        ++shown;
        --n;
    }
}

// @0x284488: the crystal button buys every piece tried on (all in one currency: crystals, asked
// for first, or an item), and puts them on. Without enough, NotEnoughWindow offers to get it.
// (Billing::LogCBPurchase / FileManager::OnItemBought: online.)
void OnPreviewBuy(bool confirmed) {
    int cost = 0;
    uint32_t costItem = 0;
    auto add = [&](Items::ItemInfo* p) {
        if (!p) return;
        cost += (int)(p->GetCostItem(false) ? p->GetCostItem(false) : p->GetCostCrystals(false));
        costItem = p->costItem;
    };
    for (Items::ItemInfo* p : g_previewEquip) add(p);
    for (Items::ItemInfo* p : g_previewCustom) add(p);
    NotEnoughWindow::ResetRequirements();
    NotEnoughWindow::SetDescriptionText(
        ToWideString(StringTable::GetString("REQUIREMENT_ITEM"), {StringTable::GetString("INV_SUBTAB_03")}), nullptr);
    bool ok;
    if (costItem == 0) {
        NotEnoughWindow::AddRequirement(GameState::kCrystal, (unsigned)cost);
        ok = NotEnoughWindow::CheckRequirements();
    } else {
        NotEnoughWindow::AddItemRequirement(costItem, (unsigned)cost);
        ok = NotEnoughWindow::CheckRequirements();
    }
    if (!ok) {
        NotEnoughWindow::SetActionCallback([] { OnPreviewBuy(false); }, StringTable::GetString("ITEM_SHOP_BUY"), true);
        NotEnoughWindow::Show();
        return;
    }
    NotEnoughWindow::Hide();
    if (costItem == 0) {
        if (!confirmed) {
            ConfirmPurchaseWindow::SetParameters([] { OnPreviewBuy(true); }, (unsigned)cost, true);
            ConfirmPurchaseWindow::Show();
            return;
        }
        // SoundsManager::PlaySound("ui_buy_with_crystals"): milestone 6.
        GameState::ChangeResourceAmount(GameState::kCrystal, -cost);
    } else {
        // SoundsManager::PlaySound("ui_buy_with_crystals"): milestone 6.
        GameState::RemoveItem(costItem, cost);
    }
    auto buy = [](Items::ItemInfo* p) {
        if (!p) return;
        GameState::PlayerItem* it = GameState::AddItem(p->id, 1, false, false, false);
        GameState::BindCustomizationTo(p->slot, it);
    };
    for (Items::ItemInfo* p : g_previewEquip) buy(p);
    for (Items::ItemInfo* p : g_previewCustom) buy(p);
    RemovePreviewItems();
    UpdateContents();
}


// @0x27dc88. UNVERIFIED (milestone 5, 5q): ItemShopWindow::Show.
void OnShop() { Hide(); }

// @0x27ba44: the set window on the set panel's set (else the first set).
void OnSetInfo() {
    SetInfoWindow::SelectSet(g_shownSet ? g_shownSet : 1);
    SetInfoWindow::Show();
}

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

// @0x285a1c: the card's depth, the buff row, the scrollers, the gliding piece (ease-out-back, with
// its turning glow when asked), the figure's pieces (a just-equipped piece flashes, a broken one
// pulses red, a mended one loses its red), the pulsing slot of the selected item, and once a
// second the buffs (a buff with more than its BUFF_TIME left is cut to half: UNVERIFIED why).
void Update(float dt) {
    g_time += dt;
    if (g_hover->shownHover) {
        auto wanted = [] {
            return PopupWindow::IsVisible() ? g_root->z - g_queue->ZRange() + 0.001f : 0.001f;
        };
        if (g_hover->GetZ() != wanted()) {
            g_hover->SetZ(wanted());
            Render::SortRenderLayer(0xe, 1);
        }
    }
    if (g_queue->shown) {
        g_buffs->SetPosition(g_buffHolder->x + g_root->x, g_buffHolder->y + g_root->y);
        g_buffs->Update(dt);
        g_scroller.Update(dt);
        g_custom.scroller.Update(dt);
    }
    const bool slowTutorial = GameState::secondTutorial == 0xa4 || (unsigned)(GameState::tutorial - 0x14) < 2;
    if (g_itemFlying && g_flySprite) {
        if (g_glowWanted && !g_glow) {
            g_glow = Render::CreateSprite(IconManager::GetIcon("Pickup_item_glow", false), 0xf, false, false);
            g_glow->screenSpace = true;
            Render::SetShaderType(g_glow, 8);
            Render::SetPosition(g_glow, 0.f, 0.f, 0.02f);
            g_glow->w *= g_root->scale;
            g_glow->h *= g_root->scale;
            Render::SortRenderLayer(0xf, 1);
        }
        float t = g_itemFlyTime / g_itemFlyDuration;
        if (t >= 1.f) {
            g_glow = Render::RemoveSprite(g_glow);
            g_flySprite = Render::RemoveSprite(g_flySprite);
            ShowHiddenPictures();
            if (GameState::secondTutorial == 0xa4) {
                // The second tutorial's equip lesson is over (the short tutorial goes on at 0x37).
                if (GameState::GetTutorialType() == 1) {
                    GameState::tutorial = 0x37;
                } else {
                    GameState::secondTutorial = 0x100;
                    TutorialSender::AddTutorialStep(0x100);
                    GameState::lastSentStep = 0x100;
                    // (FileManager::SendTutorialFinished2: online.)
                }
                Hide();
            }
            if ((unsigned)(GameState::tutorial - 0x14) < 2) {
                BeltBarWindow::Show();
                Hide();
            }
            g_itemFlying = false;
            g_glowWanted = false;
        } else {
            // The tutorials' glide lasts a second less (and waits at the end).
            if (slowTutorial) {
                t = g_itemFlyTime / (g_itemFlyDuration - 1.f);
                if (t > 1.f) t = 1.f;
            }
            const float u = t - 1.f;
            const float e = 1.f + u * u * (1.70158f + u * 2.70158f);
            Render::SetPosition(g_flySprite, g_flyFromX + (g_flyToX - g_flyFromX) * e,
                                g_flyFromY + e * (g_flyToY - g_flyFromY), 0.001f);
            if (slowTutorial) t = g_itemFlyTime / g_itemFlyDuration;
            if (g_glow) {
                Render::SetPosition(g_glow, g_flySprite->x + (g_glow->w - g_flySprite->w) * -0.5f,
                                    g_flySprite->y + (g_glow->h - g_flySprite->h) * 0.5f, 0.02f);
                // (the rotation shader's centre and angle)
                g_glow->color[0] = g_glow->x + g_glow->w * 0.5f;
                g_glow->color[1] = g_glow->h * 0.5f - g_glow->y;
                g_glow->color[2] = -t;
                if (t < 0.2f) Render::SetAlpha(g_glow, t / 0.2f);
                else if (t >= 0.8f) Render::SetAlpha(g_glow, (t - 0.8f) / -0.2f + 1.f);
                else Render::SetAlpha(g_glow, 1.f);
            }
        }
        g_itemFlyTime += dt;
    }
    for (unsigned b = 0; b != 11 && g_queue->shown; ++b) {
        if (GameState::PlayerItem* item = GetItemAt(b)) {
            Shared::PlayerHolder::Layer& l = g_figure.layers[b];
            Window* w = GameState::IsMalePlayer() ? l.male : l.femaleWindow;
            const float c = std::cos(g_time * 6.f);
            if (l.f10 > 0.f) {
                l.f10 += dt * -0.75f;
                const float v = std::sin((1.f - l.f10) * 3.14159274f) * 0.3f;
                w->SetCustomShader(12, false);
                w->SetColor(v, v, v, false);
            } else {
                const float v = 0.15f + c * 0.15f;
                if (item->IsBroken()) {
                    w->SetCustomShader(12, false);
                    w->SetColor(v, 0.f, 0.f, false);
                } else if (w->sprite && v < w->sprite->color[0]) {
                    w->SetColor(v, 0.f, 0.f, false);
                }
            }
        }
        if (g_selectedItem != -1) {
            const SelectedItem sel = GetSelectedItem();
            if (sel.info && ItemHasSlot(sel.info) && GetItemSlot(sel.info) == b) {
                g_equip[b].selector->SetVisibility(true);
                g_equip[b].selector->SetAlpha(0.5f + std::cos(g_time * 6.f) * 0.5f, true);
            }
        }
    }
    if (Timer::GetGlobalTime() == g_lastSecond) return;
    g_lastSecond = Timer::GetGlobalTime();
    bool ended = false;
    for (auto& [id, left] : g_buffTimes) {
        Items::ItemInfo* info = Items::GetItemInfo(id);
        if (info && info->meta)
            if (const MetaData* d = info->meta->FindChildData(kExpBuffTime))
                if ((uint32_t)(d->GetInt() + 1) < GameState::GetItemBuffRemainingTime(id))
                    GameState::ActivateItemBuff(id, (uint32_t)(d->GetInt() / 2));
        if (left != 0 && GameState::GetItemBuffRemainingTime(id) == 0) ended = true;
    }
    if (ended) UpdateContents();
    UpdateItemTimers();
}

// @0x286a64: the cards first; then (a press) the scrollers, a press on a cell (pieces, orbs and tools
// can be dragged) or on a worn piece; (a release) the end of a drag: a worn piece dropped off the
// figure comes off (and glides to its cell), dropped on it glides back; a grid piece dropped on the
// figure is put on (glides to its slot), an orb on the belt goes there, else it glides back.
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
    const int st = GameState::secondTutorial;
    if ((st == 0xa4 || (unsigned)(GameState::tutorial - 0x14) < 2) && g_itemFlying) return true;
    if (BuildingHovers::ClickOnArrow(x, y, pressed)) return true;
    bool scrolled;
    if (pressed) {
        const bool a = g_scroller.Click(x, y, true, g_root);
        const bool b = g_custom.scroller.Click(x, y, true, g_root);
        scrolled = a || b;
        const GUI::ClipRect& vp = g_scroller.viewport;
        if (g_root->x + vp.left <= x && x <= g_root->x + vp.right && g_root->y + vp.top <= y &&
            y <= g_root->y + vp.bottom) {
            if (st == 0x88) return true;
            g_pressY = g_pressY2 = y;
            g_pressedGrid = true;
            g_pressX = x;
            g_pressedCell = -1;
            const int rx = x - g_root->x, ry = y - g_root->y;
            for (unsigned i = 0; i != 12; ++i) {
                Cell& c = g_cells[i];
                Items::ItemInfo* info = Items::GetItemInfo(c.id);
                if (info && info->type != 3 && info->type != 8 && !info->IsProfessionTool()) continue;
                if (!c.root->visibleSelf) continue;
                const Window* r = c.root;
                if (rx < r->x || rx > r->w + r->x || ry < r->y || ry > r->h + r->y) continue;
                const Window* ic = c.holder.icon.icon;
                g_pressOnIcon = ic->x <= rx && rx <= ic->w + ic->x && ic->y <= ry && ry <= ic->h + ic->y;
                g_pressedCell = (int)(i + g_scroller.firstIndex);
            }
        }
        for (unsigned b = 0; b <= 10; ++b) {
            if (b == 8) continue;   // (the belt is not dragged)
            if (!g_figure.layers[b].item) continue;
            const Window* e = g_equip[b].root;
            const int ex = g_root->x + e->x, ey = g_root->y + e->y;
            if (x < ex || x > ex + e->w || y < ey || y > ey + e->h) continue;
            if (g_flySprite) {
                ShowHiddenPictures();
                g_flySprite = Render::RemoveSprite(g_flySprite);
                g_itemFlying = false;
                g_glow = Render::RemoveSprite(g_glow);
                g_glowWanted = false;
            }
            g_pressedSlot = true;
            g_dragSlot = (int)b;
            g_pressX = x;
            g_pressY = y;
            break;
        }
    } else {
        if (st != 0x88 && st != 0xa4 && GameState::tutorial != 0x14) BuildingHovers::HideArrow();
        const bool a = g_scroller.Click(x, y, false, g_root);
        const bool b = g_custom.scroller.Click(x, y, false, g_root);
        scrolled = a || b;
        g_pressedGrid = false;
        if (g_scrollMoved) {
            g_scrollMoved = false;
            return true;
        }
        g_pressedSlot = false;
        if (g_draggingSlot) {
            g_draggingSlot = false;
            const Window* f = g_figure.root;
            const int fx = g_root->x + f->x, fy = g_root->y + f->y;
            const Render::Sprite* s = g_flySprite;
            const bool off = (int)(s->x + s->w) < fx || (int)s->x > fx + f->w || (int)s->y < fy ||
                             (int)(s->y - s->h) > fy + f->h;
            if (!off) {
                FlyFromHere(1.f);   // back to its slot
                return true;
            }
            // Taken off: to its cell.
            unsigned b = (unsigned)g_dragSlot;
            GameState::PlayerItem* item = GameState::GetItemAt(b);
            Items::ItemInfo* info = Items::GetItemInfo(g_figure.layers[b].item);
            // SoundsManager::PlaySound("ui_unequip"): milestone 6.
            if (b == 10) {
                if (info) g_toolOverrides[info->reputationId] = info->id;
            } else {
                if (b == 4 && item != GameState::GetItemAt(4)) b = item == GameState::GetItemAt(9) ? 9 : 4;
                GameState::BindItemTo(b, nullptr);
            }
            UpdateContents();
            g_selectedItem = GetItemPositionInCurrentTab(nullptr, info);
            if (g_selectedItem == -1) {
                g_flySprite = Render::RemoveSprite(g_flySprite);
                if (g_dragSlot != -1) g_equip[g_dragSlot].bg->SetVisibility(true);
                g_dragSlot = -1;
                return true;
            }
            if (g_dragSlot != -1) g_equip[g_dragSlot].bg->SetVisibility(true);
            g_selectedEquip = -1;
            g_scroller.ScrollIntoView((unsigned)g_selectedItem);
            const unsigned cell = (unsigned)(g_selectedItem - (int)g_scroller.firstIndex);
            FlyFromHere(1.f);
            OnItem(cell, false);
            Window* ic = g_cells[cell].holder.icon.icon;
            g_flyToX = (float)(g_root->x + ic->x);
            g_flyToY = (float)(g_root->y + ic->y + ic->h);
            ic->SetVisibility(false);
            g_dragCell = (int)cell;
            return true;
        }
        if (g_draggingGrid) {
            g_draggingGrid = false;
            Items::ItemInfo* info = nullptr;
            GameState::PlayerItem* data = nullptr;
            if ((unsigned)g_pressedCell < g_items.size()) {
                data = g_items[g_pressedCell].data;
                info = g_items[g_pressedCell].info;
            }
            if (info) {
                const Render::Sprite* s = g_flySprite;
                const float cx = s->x + s->w * 0.5f - (float)g_root->x;
                const float cy = s->y + s->h * -0.5f - (float)g_root->y;
                const Window* bg = g_background;
                if ((info->type == 8 || info->IsProfessionTool()) && CanTakeOnItem(data) && (float)bg->x < cx &&
                    cx < (float)(bg->w + bg->x) && (float)bg->y < cy && cy < (float)(bg->h + bg->y)) {
                    // Dropped on the figure: put on.
                    if (st == 0xa4 || (unsigned)(GameState::tutorial - 0x14) < 2) {
                        BuildingHovers::HideArrow();
                        GUI::SetInteractionLock(false);
                        GameState::secondTutorial = 0x100;
                        TutorialSender::AddTutorialStep(0x100);
                        GameState::lastSentStep = 0x100;
                        // (FileManager::SendTutorialFinished2: online.)
                    }
                    unsigned b;
                    // SoundsManager::PlaySound("ui_equip"): milestone 6.
                    if (info->IsProfessionTool()) {
                        b = 10;
                        g_toolOverrides[info->reputationId] = info->id;
                    } else {
                        b = GameState::ExternalItemBindingToInternal(info->slot);
                        if (b == 4 && GameState::GetItemAt(4) && !GameState::GetItemAt(9)) b = 9;
                        if (b == 10) {
                            g_toolOverrides[info->reputationId] = info->id;
                        } else {
                            GameState::BindItemTo(b, g_items[g_pressedCell].data);
                            g_figure.layers[b].f10 = 1.f;
                        }
                    }
                    CompleteEquipTaskFor(info->id);
                    g_selectedItem = -1;
                    g_selectedEquip = (int)b;
                    UpdateContents();
                    FlyFromHere(1.f);
                    Window* e = g_equip[b].bg;
                    g_flyToX = (float)(g_root->x + e->x);
                    g_flyToY = (float)(g_root->y + e->y + e->h);
                    e->SetVisibility(false);
                    g_dragSlot = (int)b;
                    return true;
                }
                if (info->type == 3 && CanTakeOnItem(data) &&
                    BeltBarWindow::HasMovedIntoBelt((int)((float)g_root->x + cx), (int)((float)g_root->y + cy),
                                                   info->id)) {
                    BeltBarWindow::MoveIntoBelt(g_flySprite, info->id, (unsigned)-1);
                    g_flySprite = nullptr;
                    UpdateContents();
                    return true;
                }
            }
            FlyFromHere(1.f);   // back
            return true;
        }
    }
    if (scrolled) return true;
    BeltBarWindow::Click(x, y, pressed);
    const bool visible = g_root->visibleSelf;
    g_root->Click(x, y, pressed, false);
    return visible;
}

// @0x286444: past 30 pixels sideways a pressed cell's piece is picked up (past 10 up or down the
// press is a scroll); past 10 pixels a pressed worn piece is; the picked-up piece follows.
void Move(int x, int y) {
    g_custom.scroller.Move(x, y);
    if (!g_draggingSlot && !g_draggingGrid) g_scroller.Move(x, y);
    if (g_pressedGrid) {
        if (std::abs(x - g_pressX) > 30 && g_pressedCell != -1 && !g_draggingGrid) {
            g_scrollMoved = false;
            g_draggingGrid = true;
            // (PORT: bounds; the original reads the entry as is.)
            Items::ItemInfo* info = (unsigned)g_pressedCell < g_items.size() ? g_items[g_pressedCell].info : nullptr;
            if (info) {
                if (g_pressedCell != g_selectedItem) {
                    g_selectedItem = g_pressedCell;
                    UpdateContents();
                }
                g_flySprite = Render::RemoveSprite(g_flySprite);
                ShowHiddenPictures();
                const unsigned cell = (unsigned)(g_pressedCell - (int)g_scroller.firstIndex);
                if (cell > 11) {
                    g_draggingGrid = false;
                } else {
                    g_dragCell = (int)cell;
                    g_glow = Render::RemoveSprite(g_glow);
                    g_glowWanted = false;
                    Window* ic = g_cells[cell].holder.icon.icon;
                    g_flySprite = MakeFlySprite(info->GetIcon(), ic);
                    g_itemFlying = false;
                    g_flySprite->w = (float)ic->w;
                    g_flySprite->h = (float)ic->h;
                    g_flyToX = g_flySprite->x;
                    g_flyToY = g_flySprite->y;
                    if (!g_pressOnIcon) {
                        // Picked up away from the picture: it jumps under the finger.
                        g_pressY2 = y;
                        Render::SetPosition(g_flySprite, (float)(x - ic->w / 2), (float)(y + ic->h / 2), 0.001f);
                        g_pressX = x;
                        g_pressY = y;
                    }
                    ic->SetVisibility(false);
                }
            }
        }
        if (std::abs(y - g_pressY) > 10 && !g_draggingGrid) g_scrollMoved = true;
    }
    if (g_pressedSlot && !g_draggingSlot && std::abs(y - g_pressY) + std::abs(x - g_pressX) > 10) {
        g_draggingSlot = true;
        if (Items::ItemInfo* info = Items::GetItemInfo(g_figure.layers[g_dragSlot].item)) {
            g_glow = Render::RemoveSprite(g_glow);
            g_glowWanted = false;
            g_flySprite = Render::RemoveSprite(g_flySprite);
            Window* bg = g_equip[g_dragSlot].bg;
            g_flySprite = MakeFlySprite(info->GetIcon(), bg);
            g_itemFlying = false;
            g_flySprite->w = (float)bg->w;
            g_flySprite->h = (float)bg->h;
            g_flyToX = g_flySprite->x;
            g_flyToY = g_flySprite->y;
            bg->SetVisibility(false);
        }
    }
    if (g_draggingSlot) {
        Render::SetPosition(g_flySprite, (float)(x - g_pressX) + g_flySprite->x, (float)(y - g_pressY) + g_flySprite->y,
                            0.001f);
        g_pressY = y;
        g_pressX = x;
    }
    if (g_draggingGrid) {
        Render::SetPosition(g_flySprite, (float)(x - g_pressX) + g_flySprite->x,
                            (float)(y - g_pressY2) + g_flySprite->y, 0.001f);
        g_pressY2 = y;
        g_pressX = x;
    }
    BeltBarWindow::Move(x, y);
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
        f.move = Move;
        g_queue = new WindowManager::FunctionalWindow("CharacterInfo", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_root && g_root->visibleSelf; }
float GetZ() { return g_root->z; }
bool CanRaiseBelt() { return g_root && g_root->visibleSelf && g_tab != 7; }

// @0x27afc4
void RemovePreviewItems() {
    for (Items::ItemInfo*& p : g_previewEquip) p = nullptr;
    for (Items::ItemInfo*& p : g_previewCustom) p = nullptr;
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

// @0x27b00c
void SetLastProfession(uint32_t profession) { g_lastProfession = profession; }

// @0x27f2f4: the tool chosen for the profession while the player still has one.
uint32_t GetToolOverride(uint32_t profession) {
    auto it = g_toolOverrides.find(profession);
    if (it == g_toolOverrides.end()) return 0;
    if (GameState::GetItemAmount(it->second, false) != 0) return it->second;
    g_toolOverrides.erase(it);
    return 0;
}

// @0x27d60c: the worn item of binding b; 10 is the last profession's chosen tool, else its best one.
GameState::PlayerItem* GetItemAt(unsigned b) {
    if (b != 10) return GameState::GetItemAt(b);
    auto it = g_toolOverrides.find(g_lastProfession);
    if (it != g_toolOverrides.end()) {
        if (GameState::PlayerItem* p = GameState::GetFirstItemByID(it->second, false)) return p;
        g_toolOverrides.erase(it);   // (FUN_0027ce8c)
    }
    GameState::PlayerItem* item = nullptr;
    Professions::GetProfessionTool(g_lastProfession, true, &item, false);
    return item;
}

// @0x27d710: the same from an equipment record.
GameState::PlayerItem* GetUniqueItemAt(const PvP::PlayerEquipmentInfo& info, unsigned b) {
    if (b != 10) return info.GetUniqueItemAt((int)b);
    auto it = g_toolOverrides.find(g_lastProfession);
    if (it != g_toolOverrides.end()) {
        if (GameState::PlayerItem* p = GameState::GetFirstItemByID(it->second, false)) return p;
        g_toolOverrides.erase(it);   // (FUN_0027ce8c)
    }
    GameState::PlayerItem* item = nullptr;
    Professions::GetProfessionTool(g_lastProfession, true, &item, false);
    return item;
}

// @0x27b020: the selected cell's item (data null for a piece not owned), else the selected slot's.
SelectedItem GetSelectedItem() {
    if (g_selectedItem == -1) {
        if (g_selectedEquip == -1) return {};
        GameState::PlayerItem* p = g_equipped[g_selectedEquip];
        return p ? SelectedItem{p, p->info} : SelectedItem{};
    }
    if (g_selectedItem >= (int)g_items.size()) return {};
    const MixedItem& m = g_items[g_selectedItem];
    return m.data ? SelectedItem{m.data, m.data->info} : SelectedItem{nullptr, m.info};
}

// @0x27b0e0: a piece with this id in a slot.
bool IsItemEquipped(const Items::ItemInfo* info) {
    if (!info) return false;
    bool on = false;
    for (GameState::PlayerItem* p : g_equipped)
        if (p && p->id == info->id) on = true;
    return on;
}

// @0x27b958: the item in a slot, or its id on the belt.
bool IsItemEquipped(const GameState::PlayerItem* item) {
    if (!item) return false;
    bool on = false;
    for (GameState::PlayerItem* p : g_equipped)
        if (p && p == item) on = true;
    for (unsigned i = 0; i < GameState::GetBeltSlotCount(); ++i)
        if (GameState::GetBeltItemAt(i) == item->id) on = true;
    return on;
}

// @0x27b878: the binding the item goes to (10 a profession tool, 0 none).
unsigned GetItemSlot(const Items::ItemInfo* info) {
    if (info->slot != 0) return GameState::ExternalItemBindingToInternal(info->slot);
    return info->IsProfessionTool() ? 10 : 0;
}

// @0x27b8a8
bool ItemHasSlot(const Items::ItemInfo* info) {
    if (!info) return false;
    return info->slot != 0 || info->IsProfessionTool();
}

// @0x27af24: the grid index of the owned item (or, data null, of the piece), else -1.
int GetItemPositionInCurrentTab(const GameState::PlayerItem* data, const Items::ItemInfo* info) {
    for (size_t i = 0; i < g_items.size(); ++i)
        if (data ? g_items[i].data == data : g_items[i].info == info) return (int)i;
    return -1;
}

// @0x27bd98: b is a or one of a's improvement chain.
bool CheckIfItemImprovementLineHasItem(uint32_t a, uint32_t b) {
    if (a == b) return true;
    Collections::Collection* c = Collections::GetCollectionStart(a);
    if (!c) return false;
    if (c->from == b) return true;
    for (; c; c = Collections::GetCollectionWithSource(c->to))
        if (c->to == b) return true;
    return false;
}

// @0x27bdec: equipping an item completes the "equip" subtasks (0x26) of its whole improvement chain.
void CompleteEquipTaskFor(uint32_t id) {
    Collections::Collection* c = Collections::GetCollectionStart(id);
    if (!c) {
        Tasks::CompleteSubtask(0x26, id, 1);
        return;
    }
    Tasks::CompleteSubtask(0x26, c->from, 1);
    for (; c; c = Collections::GetCollectionWithSource(c->to)) Tasks::CompleteSubtask(0x26, c->to, 1);
}

// @0x27ba6c: the screen point of the worn belt's slot, when a belt is worn and the window is open.
// UNVERIFIED (milestone 5, 5q): not while the item shop is open (ItemShopWindow::IsVisible).
bool CanMoveIntoEquippedBelt(int& x, int& y) {
    if (!g_root->visibleSelf || !g_equipped[8]) return false;
    x = g_root->x + g_equip[8].bg->x;
    y = g_root->y + g_equip[8].bg->y;
    return true;
}

// @0x27bba8: the screen point is over the grid.
bool HasMovedIntoInventory(int x, int y) {
    Window* mask = GUI::GetWindowTyped<Window>(g_root, "shop_inv_item_holder.mask_inventory_items");
    const int rx = x - g_root->x, ry = y - g_root->y;
    return mask->x < rx && rx < mask->w + mask->x && mask->y < ry && ry < mask->h + mask->y;
}

// @0x27be48: whether an item (owned: data, else only its info) belongs on the tab. Worn pieces are
// not listed, nor a second entry of a non-durable item (a profession tool per durability).
bool IsValidItemForTabAndMode(const GameState::PlayerItem* data, const Items::ItemInfo* info, unsigned tab) {
    if (GameState::GetServerSetting(179) && info->id - 56u <= 4) return false;
    const unsigned b = info->slot == 0 ? 10 : GameState::ExternalItemBindingToInternal(info->slot);
    if (data && info->slot != 0)
        for (unsigned i = 0; i < 10; ++i)
            if (data == GameState::GetItemAt(i)) return false;
    bool common = true;   // the craft and customisation checks
    switch (tab) {
    case 1:   // weapons
        if (info->type != 8 || (b != 6 && b != 1)) return false;
        break;
    case 2:   // armour
        if (info->type != 8 || b == 1 || b == 6 || b == 4 || b == 5 || b == 9) return false;
        break;
    case 3:   // potions and orbs
        if (info->type != 14 && info->type - 3 > 2) return false;
        break;
    case 4:   // gems, decorations, belts, rings
        if (info->type == 0 || info->type == 0x81 || info->type == 0x82 || info->type == 0xf || info->type == 7) break;
        if (info->type != 8 || (b != 9 && b - 4 > 1)) return false;
        break;
    case 5:   // quest items
        if (info->type != 9 && info->type != 1) return false;
        break;
    case 6:   // crafting: crafted pieces, materials
        if (info->isCraft) {
            common = false;
            if (info->type == 0x11) return false;
            break;
        }
        if (info->type != 2 && info->type != 6) return false;
        common = false;
        break;
    default: break;
    }
    if (common) {
        if (info->isCraft || tab == 7 || info->type == 0x11) return false;
    }
    if (info->durability == 0 && !g_items.empty()) {
        for (const MixedItem& m : g_items)
            if (m.info->id == info->id) return false;
    }
    if (!data || !Professions::IsProfessionTool(info->id) || data->f0c != info->durability || g_items.empty()) return true;
    // A profession tool once per durability left. (PORT: an entry without data is skipped; the
    // original reads through it.)
    for (const MixedItem& m : g_items)
        if (m.info->id == info->id && m.data && data->f0c == m.data->f0c) return false;
    return true;
}

// @0x27fda8: the tab's grid: the owned items, then (not for weapons) the pieces not owned that
// are worth showing (a running buff, a "remains" item of the player's level range seen lately),
// then the customisations on sale (tab 7, sorted by binding) or the newest first (tab 0, at most 32).
// The opening tutorial (0x14 / 0x15) lists only item 0x5b.
void GetItemsForTabAndMode(unsigned tab) {
    if (tab == 7) {
        for (unsigned i = 0; i < 10; ++i) g_equipped[i] = GameState::GetCustomizationAt(g_customOrder[i]);
    } else {
        for (unsigned i = 0; i < 11; ++i) g_equipped[i] = GetItemAt(i);
    }
    g_items.clear();
    for (unsigned k = 0; GameState::PlayerItem* it = GameState::EnumItems(k); ++k) {
        if (!it->info) {
            std::printf("CharacterInfoWindow::GetItemsForTabAndMode() Item %d does not exist in the configuration\n",
                        it->id);
            continue;
        }
        if (!IsValidItemForTabAndMode(it, it->info, tab)) continue;
        if ((unsigned)(GameState::tutorial - 0x14) < 2 && it->id != 0x5b) continue;
        g_items.push_back({it, it->info, 0});
    }
    if (tab != 1) {
        const unsigned level = (unsigned)GameState::GetLevel();
        for (unsigned k = 0; Items::ItemInfo* info = Items::EnumItemInfo(k); ++k) {
            const bool buff = info->type == 0xe && GameState::GetItemBuffRemainingTime(info->id) != 0;
            if (!info->remains && !buff) continue;
            if ((level < (uint16_t)info->levelLimitMin || (uint16_t)info->levelLimitMax < level) && !buff) continue;
            if (GameState::GetItemRecentTime(info->id) == 0) continue;
            if (!IsValidItemForTabAndMode(nullptr, info, tab)) continue;
            g_items.push_back({nullptr, info, 0});
        }
        if (tab == 7) {
            const bool male = GameState::IsMalePlayer();
            for (unsigned k = 0; Items::ItemInfo* info = Items::EnumItemInfo(k); ++k) {
                if (info->type != 0x11) continue;
                if (!info->IsOnSale() && GameState::GetItemAmount(info->id, false) == 0) continue;
                const bool forMale = std::strncmp(info->name, "male_", 5) == 0;
                const bool forFemale = std::strncmp(info->name, "female_", 7) == 0;
                if ((forMale && !male) || (forFemale && male)) continue;
                g_items.push_back({nullptr, info, 0});
            }
            std::stable_sort(g_items.begin(), g_items.end(), SortBySlot);
        } else if (tab == 0) {
            for (MixedItem& m : g_items) m.time = GameState::GetItemRecentTime(m.info->id);
            std::stable_sort(g_items.begin(), g_items.end(), SortByTime);
            if (g_items.size() > 0x20) g_items.resize(0x20);
        }
    }
    if (g_selectedItem != -1 && (int)g_items.size() <= g_selectedItem) g_selectedItem = -1;
    if (g_tab != 7)
        for (unsigned i = 0; i < 11; ++i) g_equipped[i] = GetItemAt(i);
}

// @0x283e4c: the time left on the visible cells: a running buff, the starter chest (item 0x2b6,
// which goes away after "starter_chest_time" seconds and resets task 0x75b).
void UpdateItemTimers() {
    for (unsigned i = 0; i < 12; ++i) {
        const unsigned idx = g_scroller.firstIndex + i;
        if (idx >= g_items.size()) continue;
        Items::ItemInfo* info = g_items[idx].info;
        if (info->id != 0x2b6 && g_buffTimes[info->id] == 0) continue;
        Textfield* t = g_cells[i].holder.icon.levelReq;
        t->SetVisibility(true);
        if (info->id == 0x2b6) {
            const int now = Timer::GetGlobalTime();
            const int left = Setting("starter_chest_time").GetInt() - (now - (int)GameState::GetItemRecentTime(info->id));
            t->SetText(StringTable::GetNumericTimeString(left < 0 ? 0 : left, true).c_str());
            if (left < 1 && GameState::GetItemAmount(info->id, false) != 0) {
                GameState::RemoveItem(info->id, 1);
                GameState::ResetTask(0x75b);
                if (Tasks::TaskInfo* task = Tasks::GetTask(0x75b)) task->dnShow = task->dnShowDefault;
                HUDWindow::UpdateTasks();
                if (g_hover->info == info) g_hover->Hide();
                UpdateContents();
            }
        } else if (int left = (int)GameState::GetItemBuffRemainingTime(info->id)) {
            t->SetText(StringTable::GetNumericTimeString(left, true).c_str());
        }
    }
}

// @0x27b8c8: not locked by level, and a binding, a profession tool, or a free (or used-up) belt slot.
bool CanTakeOnItem(const GameState::PlayerItem* it) {
    if (!it || it->info->IsLockedByLevel()) return false;
    if (it->info->slot != 0 || it->info->IsProfessionTool()) return true;
    for (unsigned i = 0; i < GameState::GetBeltSlotCount(); ++i) {
        if (GameState::GetBeltItemAt(i) == 0) return true;
        if (GameState::GetItemAmount(GameState::GetBeltItemAt(i), false) == 0) return true;
    }
    return false;
}

// @0x27b134: everything but the belt (binding 8).
bool CanTakeOffItem(const GameState::PlayerItem* it) { return it && it->info->slot != 8; }

// @0x287920: the card's Equip (equip true) / Unequip button for the selected item. A worn piece's
// binding takes it (a second ring the free ring binding) and it glides there from its cell; an orb
// goes to the first free belt slot (gliding into the belt). Unequip: the piece glides to its cell,
// a belt orb from the belt into the grid.
void OnEquip(bool equip) {
    const int st = GameState::secondTutorial;
    if (st == 0xa4 || (unsigned)(GameState::tutorial - 0x14) < 2) {
        BuildingHovers::HideArrow();
        GUI::SetInteractionLock(false);
    }
    if (GameState::secondTutorial == 0x88) {
        BuildingHovers::HideArrow();
        GUI::SetInteractionLock(true);
    }
    const SelectedItem sel = GetSelectedItem();
    GameState::PlayerItem* item = sel.data;
    if (!item) {
        UpdateContents();
        return;
    }
    if (!IsItemEquipped(item)) {
        if (!equip) return;
        if (item->info->slot != 0) {
            // SoundsManager::PlaySound("ui_equip"): milestone 6.
            unsigned b = GameState::ExternalItemBindingToInternal(item->info->slot);
            if (b == 4 && GameState::GetItemAt(4)) b = GameState::GetItemAt(9) ? 4 : 9;
            GameState::BindItemTo(b, item);
            g_figure.layers[b].f10 = 1.f;
            CompleteEquipTaskFor(item->id);
            Items::ItemInfo* info = item->info;
            g_flySprite = Render::RemoveSprite(g_flySprite);
            if (g_dragSlot != -1) g_equip[g_dragSlot].bg->SetVisibility(true);
            g_dragSlot = -1;
            if (g_dragCell != -1) g_cells[g_dragCell].holder.icon.icon->SetVisibility(true);
            g_dragCell = -1;
            const unsigned cell = (unsigned)(g_selectedItem - (int)g_scroller.firstIndex);
            g_pressedCell = g_selectedItem;
            if (cell < 12) g_dragCell = (int)cell;
            if (cell < 12 && g_queue->shown) {
                g_glow = Render::RemoveSprite(g_glow);
                g_glowWanted = false;
                Window* ic = g_cells[cell].holder.icon.icon;
                g_flySprite = MakeFlySprite(info->GetIcon(), ic);
                g_itemFlying = false;
                g_flySprite->w = (float)ic->w;
                g_flySprite->h = (float)ic->h;
                g_flyToX = g_flySprite->x;
                g_flyToY = g_flySprite->y;
                ic->SetVisibility(false);
                g_selectedItem = -1;
                g_selectedEquip = (int)b;
                UpdateContents();
                Window* e = g_equip[b].bg;
                g_itemFlyTime = 0.f;
                g_glowWanted = true;
                g_itemFlying = true;
                // The second tutorial's piece glides slower.
                g_itemFlyDuration = GameState::secondTutorial == 0xa4 ? 2.5f : 1.f;
                g_flyFromX = g_flySprite->x;
                g_flyFromY = g_flySprite->y;
                g_flyToY = (float)(g_root->y + e->y + e->h);
                g_flyToX = (float)(g_root->x + e->x);
                e->SetVisibility(false);
                g_dragSlot = (int)b;
                return;
            }
            g_selectedItem = -1;
            g_selectedEquip = (int)b;
            UpdateContents();
            return;
        }
        // An orb: to the belt (not twice).
        bool onBelt = false;
        for (unsigned k = 0; k < GameState::GetBeltSlotCount(); ++k)
            if (GameState::GetBeltItemAt(k) == item->id) onBelt = true;
        if (!onBelt) {
            for (unsigned k = 0; k < GameState::GetBeltSlotCount(); ++k) {
                if (GameState::GetBeltItemAt(k) != 0 &&
                    GameState::GetItemAmount(GameState::GetBeltItemAt(k), false) != 0)
                    continue;
                Items::ItemInfo* info = item->info;
                const unsigned cell = (unsigned)(g_selectedItem - (int)g_scroller.firstIndex);
                g_pressedCell = g_selectedItem;
                if (cell < 12) {
                    g_dragCell = (int)cell;
                } else if (g_dragCell == -1) {
                    // Not in view: straight onto the belt.
                    // SoundsManager::PlaySound("ui_equip_belt"): milestone 6.
                    GameState::BindBeltItemTo(k, item->id);
                    if (item->info->CanBeActivated()) {
                        GameState::SetBeltItemActivationTimeAt(k, (uint32_t)Timer::GetGlobalTime());
                        BeltBarWindow::UpdateContents();
                        UpdateContents();
                        return;
                    }
                    break;
                }
                Render::RemoveSprite(g_flySprite);
                Window* ic = g_cells[g_dragCell].holder.icon.icon;
                g_flySprite = MakeFlySprite(info->GetIcon(), ic);
                g_flySprite->h = (float)ic->h;
                g_flySprite->w = (float)ic->w;
                BeltBarWindow::MoveIntoBelt(g_flySprite, info->id, k);
                g_flySprite = nullptr;
                UpdateContents();
                BeltBarWindow::UpdateContents();
                UpdateContents();
                return;
            }
        }
        BeltBarWindow::UpdateContents();
        UpdateContents();
        return;
    }
    if (equip) return;
    if (item->info->slot != 0) {
        // SoundsManager::PlaySound("ui_unequip"): milestone 6.
        unsigned b = GameState::ExternalItemBindingToInternal(item->info->slot);
        if (b == 4 && item != GameState::GetItemAt(4)) b = item == GameState::GetItemAt(9) ? 9 : 4;
        GameState::BindItemTo(b, nullptr);
        GetItemsForTabAndMode(g_tab);
        Items::ItemInfo* info = item->info;
        g_dragSlot = (int)b;
        g_dragCell = GetItemPositionInCurrentTab(item, info);
        if (g_dragCell == -1 || !g_queue->shown) {
            g_selectedEquip = -1;
            g_selectedItem = -1;
            UpdateContents();
            return;
        }
        g_glow = Render::RemoveSprite(g_glow);
        g_glowWanted = false;
        g_flySprite = Render::RemoveSprite(g_flySprite);
        Window* e = g_equip[b].bg;
        g_flySprite = MakeFlySprite(info->GetIcon(), e);
        g_itemFlying = false;
        g_flySprite->w = (float)e->w;
        g_flySprite->h = (float)e->h;
        g_flyToX = g_flySprite->x;
        g_flyToY = g_flySprite->y;
        e->SetVisibility(false);
        UpdateContents();
        if (g_dragSlot != -1) g_equip[g_dragSlot].bg->SetVisibility(true);
        g_selectedEquip = -1;
        g_selectedItem = g_dragCell;
        g_scroller.ScrollIntoView((unsigned)g_selectedItem);
        const unsigned cell = (unsigned)(g_selectedItem - (int)g_scroller.firstIndex);
        FlyFromHere(1.f);
        OnItem(cell, false);
        Window* ic = g_cells[cell].holder.icon.icon;
        g_flyToX = (float)(g_root->x + ic->x);
        g_flyToY = (float)(g_root->y + ic->y + ic->h);
        ic->SetVisibility(false);
        g_dragCell = (int)cell;
        return;
    }
    // An orb off the belt: it glides from its slot into the grid.
    for (unsigned k = 0; k < GameState::GetBeltSlotCount(); ++k) {
        if (GameState::GetBeltItemAt(k) != item->id) continue;
        Items::ItemInfo* info = item->info;
        // SoundsManager::PlaySound("ui_unequip_belt"): milestone 6.
        GameState::BindBeltItemTo(k, 0);
        BeltBarWindow::UpdateContents();
        if (IsVisible()) {
            int bx, by;
            BeltBarWindow::GetBeltItemLocation(bx, by, (int)k);
            Render::RemoveSprite(g_flySprite);
            g_flySprite = Render::CreateSprite(info->GetIcon(), 0xf, false, false);
            g_flySprite->screenSpace = true;
            Render::SetShaderType(g_flySprite, 1);
            Render::SetPosition(g_flySprite, (float)bx, (float)by, 0.001f);
            MoveIntoInventory(g_flySprite, info->id);
        }
    }
    BeltBarWindow::UpdateContents();
    UpdateContents();
}

// @0x283ddc: the selected piece to the repair window.
void OnRepair() {
    const SelectedItem sel = GetSelectedItem();
    if (sel.data) {
        RepairWindow::SetParameters(sel.data);
        RepairWindow::Show();
        return;
    }
    UpdateContents();
}

// @0x283e14: one of the selected piece thrown away. UNVERIFIED (milestone 5, 5q):
// ItemShopWindow::UpdateContents.
void OnRemove() {
    const SelectedItem sel = GetSelectedItem();
    if (sel.data) GameState::RemoveItem(sel.data->id, 1);
    UpdateContents();
}

// @0x2859b8: the item's improvement (from the start of its chain) in the crafting window.
// UNVERIFIED (milestone 5, 5o): CraftingWindow::SetParameters / SelectTabFor / Show, and this
// window's Hide.
void OnImprove(GameState::PlayerItem* item) {
    if (!item) return;
    UpdateContents();
}

// @0x284318 / 0x284394 / 0x2843d8: the admin tools on the selected cell: more of it, break it,
// remove one or all.
void OnAddSelectedItem(unsigned n) {
    if ((unsigned)g_selectedItem >= g_items.size()) return;
    if (uint32_t id = g_items[g_selectedItem].info->id) {
        GameState::AddItem(id, (int)n, true, false, false);
        UpdateContents();
    }
}
void OnBreakSelectedItem() {
    if ((unsigned)g_selectedItem >= g_items.size()) return;
    if (GameState::PlayerItem* p = g_items[g_selectedItem].data) p->f0c = 0;
    UpdateContents();
}
void OnRemoveSelectedItem(bool all) {
    if ((unsigned)g_selectedItem >= g_items.size()) return;
    const uint32_t id = g_items[g_selectedItem].info->id;
    if (!id) return;
    if (GameState::GetItemAmount(id, false) != 0)
        GameState::RemoveItem(id, all ? GameState::GetItemAmount(id, false) : 1);
    if (GameState::GetItemAmount(id, false) == 0) g_selectedItem = 0;
    UpdateContents();
}

// @0x2850e0: a picture (from the belt) glides into the item's cell, which is selected; without a
// cell it goes.
void MoveIntoInventory(Render::Sprite* sprite, uint32_t id) {
    if (g_flySprite && g_flySprite != sprite) g_flySprite = Render::RemoveSprite(g_flySprite);
    g_selectedItem = GetItemPositionInCurrentTab(nullptr, Items::GetItemInfo(id));
    if (g_selectedItem == -1) {
        Render::RemoveSprite(sprite);
        return;
    }
    g_selectedEquip = -1;
    g_flySprite = sprite;
    g_scroller.itemCount = (unsigned)g_items.size();
    g_scroller.ScrollIntoView((unsigned)g_selectedItem);
    const unsigned cell = (unsigned)(g_selectedItem - (int)g_scroller.firstIndex);
    g_itemFlying = true;
    g_itemFlyTime = 0.f;
    g_itemFlyDuration = 1.f;
    g_flyFromX = g_flySprite->x;
    g_flyFromY = g_flySprite->y;
    OnItem(cell, false);
    Window* ic = g_cells[cell].holder.icon.icon;
    g_flySprite->h = (float)ic->h;
    g_flySprite->w = (float)ic->w;
    g_flyToX = (float)(g_root->x + ic->x);
    g_flyToY = (float)(g_root->y + ic->y + ic->h);
    ic->SetVisibility(false);
    g_dragCell = (int)cell;
}

// @0x2883b0: the window shows the item (or one of its improvement chain): its slot if worn, else
// its cell in the first tab that lists it (data: that very item); help is the card's lesson
// (0 / 2 / 3 put the arrow on it; 1 equip, 4 drink; see OnTweenIn).
void ShowInventoryItem(uint32_t id, unsigned help, GameState::PlayerItem* data) {
    for (unsigned b = 0; b != 11; ++b) {
        GameState::PlayerItem* worn = GetItemAt(b);
        if (!worn || !CheckIfItemImprovementLineHasItem(worn->id, id)) continue;
        g_help = help;
        if (help == 0 || help == 2 || help == 3) g_arrowEquip = (int)b;
        g_selectedItem = -1;
        g_selectedEquip = (int)b;
        UpdateContents();
        if (!g_tween->active) OnTweenIn();
        return;
    }
    for (unsigned tab = 0; tab != 8; ++tab) {
        GetItemsForTabAndMode(tab);
        for (unsigned i = 0; i < g_items.size(); ++i) {
            const bool hit = data ? g_items[i].data == data : CheckIfItemImprovementLineHasItem(g_items[i].info->id, id);
            if (!hit) continue;
            g_help = help;
            g_tab = tab;
            g_selectedEquip = -1;
            UpdateContents();
            g_scroller.ScrollIntoView(i);
            g_selectedItem = (int)i;
            if (help == 0 || help == 2) g_arrowCell = (int)(i - g_scroller.firstIndex);
            UpdateContents();
            Render::SortRenderLayer(0xe, 1);
            if (!g_tween->active) OnTweenIn();
            return;
        }
    }
    GetItemsForTabAndMode(0);
    UpdateContents();
}

// @0x2885e0: the inventory's first lesson: a broken worn piece (to repair), else an empty binding
// with a piece for it (to equip). True when there was one.
bool ExecuteFirstHelp() {
    if (g_tween->active) return true;
    for (unsigned b = 0; b != 10; ++b) {
        GameState::PlayerItem* it = GameState::GetItemAt(b);
        if (it && it->IsBroken()) {
            Show();
            ShowInventoryTab(0);
            ShowInventoryItem(it->id, 3, it);
            return true;
        }
    }
    for (unsigned b = 0; b != 10; ++b) {
        if (GameState::GetItemAt(b)) continue;
        for (unsigned k = 0; GameState::PlayerItem* it = GameState::EnumItems(k); ++k) {
            if (!it->info || it->info->slot == 0) continue;
            if (GameState::ExternalItemBindingToInternal(it->info->slot) != b) continue;
            Show();
            ShowInventoryTab(0);
            ShowInventoryItem(it->id, 1, it);
            return true;
        }
    }
    return false;
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
    g_buffHolder = GUI::GetWindowTyped<Window>(g_root, "buff_holder_01");
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
    g_queue->fn.update = Update;
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

namespace CharacterInfoWindow {

namespace {
void ApplyEquipment(GameState::PlayerItem* it, Entity* player) {
    if (it && it->info && it->info->meta && !it->IsBroken()) it->info->meta->OnApplyItemEffect(player);
}

// The complete set bonuses (every slot of the bonus covered).
template <class F> void ForEachCompleteBonus(F f) {
    for (unsigned i = 0; Sets::Set* s = Sets::EnumSet(i); ++i)
        for (int k = 0; k != 3; ++k)
            if (s->bonusSlots[k] && s->covered[k] == s->bonusSlots[k]->GetChildrenCount()) f(s->bonus[k]);
}

// One cell of the grid (UpdateContents' loop over the 12 cells).
void UpdateCell(unsigned i, uint32_t now) {
    Cell& c = g_cells[i];
    const unsigned idx = g_scroller.firstIndex + i;
    if (idx >= g_items.size()) {
        c.root->SetVisibility(false);
        c.id = 0;
        return;
    }
    GameState::PlayerItem* data = g_items[idx].data;
    Items::ItemInfo* info = g_items[idx].info;
    const bool broken = data && data->IsBroken();
    const bool locked = info->IsLockedByLevel();
    const bool isPack = info->packId != 0;
    const bool isRandom = info->id == 0x2fe;
    bool equipped = false;
    if (data)
        for (GameState::PlayerItem* p : g_equipped)
            if (p == data) equipped = true;
    int amount = GameState::GetItemAmount(info->id, false);
    if (info->type == 8 && IsItemEquipped(data)) --amount;
    // A durable item counts as one (a profession tool only while it is worn out to its durability).
    if (info->durability != 0 && !(Professions::IsProfessionTool(info->id) && data->f0c == info->durability)) amount = 1;
    if (isPack && info->packItems && info->packItems->GetChildrenCount())
        amount = info->packItems->GetChild(0)->GetChild(1)->GetInt();
    c.root->SetVisibility(true);
    c.holder.root->SetVisibility(true);
    bool multi = (unsigned)amount > 1;
    bool bought;   // the "bought" background and the status text
    bool owned;    // the shop texts (else the bag texts)
    if (equipped) {
        bought = owned = true;
    } else {
        const bool m = multi || info->type == 3;
        bought = owned = m;
    }
    const bool tab7 = g_tab == 7;
    bool hasPrice = false;
    if (tab7) {
        hasPrice = info->GetCostCrystals(false) != 0 || info->GetCostItem(false) != 0;
        bought = owned = hasPrice;
        GameState::PlayerItem* cur = GameState::GetCustomizationAt(info->slot);
        if (cur && cur->id == info->id) equipped = true;
    }
    c.beltButton->SetVisibility(isRandom);
    c.boughtBg->SetVisibility(bought);
    const bool buyable = tab7 && amount == 0;
    c.costIcon->SetVisibility(buyable && hasPrice);
    if (Items::ItemInfo* ci = Items::GetItemInfo(info->costItem)) c.costIcon->SetTexture(ci->GetIcon(), false);
    else c.costIcon->SetTexture(IconManager::GetIcon("35_crystal", false), true);
    c.price->SetVisibility(buyable && hasPrice);
    const uint32_t cost = info->GetCostItem(false) ? info->GetCostItem(false) : info->GetCostCrystals(false);
    c.price->SetText(ToWideString((int)cost));
    c.requiresLevel->SetVisibility(bought);
    c.confirm->SetTexture(IconManager::GetIcon(buyable ? "gold_add" : "gold_confirm", false), false);
    c.confirm->SetVisibility(equipped || (tab7 && g_previewCustom[info->slot] == info));
    const unsigned level = info->itemLevel;
    bool anyShop = false;
    for (unsigned k = 0; k != 5; ++k) {
        c.shopText[k]->SetVisibility(owned && level == k);
        if (c.shopText[k]->visibleSelf) anyShop = true;
    }
    for (unsigned k = 0; k != 5; ++k) c.bagText[k]->SetVisibility(!anyShop && level == k);
    if (!(multi || info->type == 3)) {
        const char* key = !tab7 || equipped ? "INVENTORY_SHOP_EQUIPPED" : amount == 0 ? "SHOP_BUY" : "INVENTORY_SHOP_OWNED";
        c.requiresLevel->SetText(StringTable::GetString(key));
    } else if (amount == 0) {
        c.requiresLevel->SetText(StringTable::GetString("ITEM_GET_MORE"));
    } else {
        c.requiresLevel->SetText(
            SWPrintf(0x100, U"%s: %d", {StringTable::GetString("INVENTORY_SHOP_OWNED"), amount}).c_str());
    }
    // The name in its quality colour ("<name> is empty" for a used-up orb outside the customisations).
    const char32_t* title = info->title ? info->title : U"";
    std::u32string name = amount == 0 && !tab7 && info->type != 0xe
                              ? SWPrintf(0x100, StringTable::GetString("EMPTY_ORB"), {title})
                              : std::u32string(title);
    if (level < 5 && c.shopText[level]->visibleSelf) c.shopText[level]->SetText(name.c_str());
    if (level < 5 && c.bagText[level]->visibleSelf) c.bagText[level]->SetText(name.c_str());
    c.id = info->id;
    Shared::ItemIconHolder& icon = c.holder.icon;
    info->PutImageInHolder(icon.icon);
    icon.icon->SetVisibility(info->GetIcon() != nullptr);
    icon.icon->SetEnabled(tab7 || amount != 0);
    c.holder.quantity->SetVisibility(multi && isPack);
    c.holder.quantity->SetText(SWPrintf(0x80, U"x%d", {amount}).c_str());
    bool selected = (g_selectedEquip != -1 && data && g_equipped[g_selectedEquip] == data) || (int)idx == g_selectedItem;
    c.bg->SetVisibility(selected && !isRandom);
    c.holder.selected->SetVisibility(false);
    c.holder.emptyBg->SetVisibility(false);
    icon.broken->SetVisibility(broken);
    icon.locked->SetVisibility(locked);
    for (unsigned k = 0; k != 5; ++k) icon.itemClass[k]->SetVisibility(!locked && !broken && level == k);
    icon.lock->SetVisibility(false);
    icon.lockSmall->SetVisibility(locked);
    icon.levelReq->SetVisibility(locked);
    if (icon.levelReq->visibleSelf)
        icon.levelReq->SetText(
            SWPrintf(0x100, StringTable::GetString("LVL_HEAD"), {ToWideString((int)info->level)}).c_str());
    // A profession item's profession icon (shown for the reputations and for profession 6, the tamer).
    if (Professions::Profession* p = Professions::GetProfession(info->reputationId)) {
        icon.repProf->SetVisibility(p->type == Professions::kReputation || p->id == 6);
        icon.repProf->SetTexture(p->smallIcon, p->icon != p->smallIcon);
    } else {
        icon.repProf->SetVisibility(false);
    }
    icon.recommended->SetVisibility(false);
    icon.owned->SetVisibility(false);
    icon.redX->SetVisibility(false);
    // "NEW": got in the last 15 minutes and not shown in an earlier opening.
    g_newShown.insert(info->id);   // (FUN_0027d814)
    icon.discount[1].root->SetVisibility(now - GameState::GetItemAcquirementTime(info->id) < 900 &&
                                         !g_newSeen.count(info->id));
    icon.discount[1].text->SetText(StringTable::GetString("NEW"));
}

// The set panel: the set with the most bonus slots covered, its name in its quality colour and how
// far it is.
void UpdateSetPanel() {
    Sets::Set* best = nullptr;
    unsigned bestCovered = 0, total = 0;
    for (unsigned i = 0; Sets::Set* s = Sets::EnumSet(i); ++i) {
        unsigned covered = 0, count = 0;
        for (int k = 0; k != 3; ++k) {
            covered += s->covered[k];
            count += s->bonusSlots[k] ? s->bonusSlots[k]->GetChildrenCount() : 0;
        }
        if (bestCovered < covered) {
            total = count;
            bestCovered = covered;
            best = s;
        }
    }
    unsigned level = 0;
    g_shownSet = best ? best->id : 0;
    if (best && best->slots[0])
        if (Items::ItemInfo* info = Items::GetItemInfo((uint32_t)best->slots[0]->GetChild(0)->GetInt()))
            level = info->itemLevel;
    for (unsigned k = 0; k != 5; ++k) g_setTexts[k]->SetVisibility(k == level);
    if (best) g_setTexts[level]->SetText(best->name);
    else g_setTexts[level]->SetText(StringTable::GetString("INVENTORY_NO_SET"));
    g_setInfoText->SetText(StringTable::GetString("INVENTORY_SET_INFO"));
    if (best)
        g_setClip.right = (int)((float)g_setClip.left + (float)g_setBar->w * ((float)bestCovered / (float)total));
    else
        g_setClip.right = g_setClip.left;
}

// The stats: now and with the selected piece worn instead (red worse, green better). Two holders of
// three stats each: the long ones, or the short ones on a small screen while the belt is raised.
void UpdateStats(Entity* player, const SelectedItem& sel, int previews) {
    int now[11] = {}, with[11] = {};
    for (int* s : {now, with}) {
        s[0] = player->GetHpMax();
        s[1] = player->GetAttackMelee();
        s[2] = (int)GameState::GetSoldierSlotCount();
        s[3] = player->GetInitiative(0);
        s[4] = player->GetDefenseMelee();
        s[5] = player->GetDefenseRanged();
    }
    if (sel.info && sel.info->type == 8 && !(sel.data && sel.data->IsBroken())) {
        unsigned b = 10;
        if (sel.info->slot != 0) {
            b = GameState::ExternalItemBindingToInternal(sel.info->slot);
            // A second ring goes to the free ring binding.
            if (b == 4 && GameState::GetItemAt(4) && !GameState::GetItemAt(9) &&
                !(sel.data && GameState::GetItemAt(4) == sel.data))
                b = 9;
        }
        if (sel.data && GameState::GetItemAt(9) == sel.data) b = 9;
        ForEachCompleteBonus([&](MetaExpression* e) { if (e) e->OnApplyItemEffect(with, false); });
        if (GameState::PlayerItem* old = g_equipped[b])
            if (old->info->meta) {
                old->info->meta->OnApplyItemEffect(with, false);
                Sets::RemoveItem(old->id);
            }
        if (sel.info->meta) {
            sel.info->meta->OnApplyItemEffect(with, true);
            Sets::AddItem(sel.info->id);
        }
        Sets::CommitChanges();
        ForEachCompleteBonus([&](MetaExpression* e) { if (e) e->OnApplyItemEffect(with, true); });
    }
    for (StatisticsHolder* group : {g_statsLong, g_statsShort})
        for (int h = 0; h != 2; ++h) {
            StatisticsHolder& sh = group[h];
            sh.root->SetVisibility(false);
            for (int k = 0; k != 9; ++k) {
                (&sh.red[0])[k]->SetVisibility(false);
                (&sh.beforeRed[0])[k]->SetVisibility(false);
                (&sh.afterRed[0])[k]->SetVisibility(false);
            }
        }
    const float scale = g_root->scale;
    bool useShort = false;
    if ((int)(scale * 1100.f) > GUI::ScreenWidth() && (int)(scale * 740.f) >= GUI::ScreenHeight() &&
        !(GUI::IsHighDPIVersion() && GUI::ScreenHeight() >= 600))
        useShort = true;
    if (!CanRaiseBelt()) useShort = false;
    for (int h = 0; h != 2; ++h) {
        StatisticsHolder& sh = useShort ? g_statsShort[h] : g_statsLong[h];
        sh.root->SetVisibility(previews == 0 || g_tab != 7);
        for (int j = 0; j != 3; ++j) {
            const int stat = h * 3 + j;
            const int color = with[stat] < now[stat] ? 0 : with[stat] > now[stat] ? 2 : 1;
            Textfield* label = (&sh.red[0])[color * 3 + j];
            label->SetVisibility(true);
            label->SetText(StringTable::GetString(MetaExpression::GetItemEffectName((unsigned)stat)));
            const char32_t* fmt = stat >= 4 ? U"%d%%" : U"%d";
            sh.beforeWhite[j]->SetVisibility(true);
            sh.beforeWhite[j]->SetText(SWPrintf(0x20, fmt, {now[stat]}).c_str());
            Textfield* after = (&sh.afterRed[0])[color * 3 + j];
            after->SetVisibility(true);
            after->SetText(SWPrintf(0x20, fmt, {with[stat]}).c_str());
        }
    }
}

// The customisation tab's strip of the previewed pieces and their price.
void UpdateCustomStrip(int previews) {
    g_custom.scroller.itemCount = (unsigned)previews;
    g_custom.scroller.UpdateScroll();
    const bool tab7 = g_tab == 7;
    g_paramBack->SetVisibility(tab7);
    g_chance->SetVisibility(tab7);
    g_custom.root->SetVisibility(tab7);
    g_customBuy.root->SetVisibility(tab7);
    g_chance->SetText(StringTable::GetString("WIN_CUSTOM_PREVIEW"));
    int cost = 0;
    uint32_t costItem = 0;
    for (unsigned k = 0, slot = 0; slot <= 4 && k <= 31; ++k) {
        Items::ItemInfo* p = g_previewCustom[k];
        if (!p) continue;
        Shared::ItemHolder& h = g_custom.slots[slot];
        h.root->SetVisibility(true);
        if (h.root->visibleSelf) {
            h.icon.discount[1].root->SetVisibility(false);
            h.icon.broken->SetVisibility(false);
            h.icon.locked->SetVisibility(false);
            for (unsigned c = 0; c != 5; ++c) h.icon.itemClass[c]->SetVisibility(c == 0);
            p->PutImageInHolder(h.icon.icon);
            h.emptyBg->SetVisibility(false);
            h.selected->SetVisibility(false);
            h.icon.redX->SetVisibility(false);
            h.icon.owned->SetVisibility(false);
            h.icon.recommended->SetVisibility(false);
            h.icon.lock->SetVisibility(false);
            cost += (int)(p->GetCostItem(false) ? p->GetCostItem(false) : p->GetCostCrystals(false));
            costItem = p->costItem;
        }
        ++slot;
    }
    const unsigned shown = (unsigned)previews - g_custom.scroller.firstIndex;
    if (shown <= 4)
        for (unsigned s = shown; s != 5; ++s) g_custom.slots[s].root->SetVisibility(false);
    g_customBuy.text->SetText(StringTable::GetString("SHOP_BUY"));
    g_customBuy.price->SetText(ToWideString(cost));
    if (Items::ItemInfo* ci = Items::GetItemInfo(costItem)) g_customBuy.crystal->SetTexture(ci->GetIcon(), false);
    else g_customBuy.crystal->SetTexture(IconManager::GetIcon("35_crystal", false), true);
}
}  // namespace

// @0x28047c: the player's stats again (its level, the soldiers', the worn pieces, the complete sets,
// the running buffs: the HP bar too), the belt, then the window: tabs, slots, figure, grid, set
// panel, stats and the customisation strip. Map::Load and the item windows run it too.
void UpdateContents() {
    Entity* player = EntityManager::GetPlayer();
    const uint32_t now = (uint32_t)Timer::GetGlobalTime();
    if (g_keepHp && player->GetHP() > g_keptHp) g_keptHp = player->GetHP();
    int previews = 0;
    for (Items::ItemInfo* p : g_previewEquip)
        if (p) ++previews;
    for (Items::ItemInfo* p : g_previewCustom)
        if (p) ++previews;
    for (unsigned i = 0; i != 8; ++i) g_tabs[i].SetMode(i == g_tab, false);
    static const char* const kTabIcons[8] = {"tab_inv_all_items", "tab_inv_gear",      "tab_shop_armor",
                                             "tab_inv_cons",      "tab_inv_acc_gems",  "tab_inv_quest",
                                             "tab_inv_acc_craft", "customisation_35"};
    for (unsigned i = 0; i != 8; ++i) g_tabs[i].icon->SetTexture(IconManager::GetIcon(kTabIcons[i], false), true);
    g_name->SetText(StringTable::GetString("SQ_PLAYER"));
    g_headerBack->SetVisibility(previews == 0 || g_tab != 7);
    g_header->SetVisibility(previews == 0 || g_tab != 7);
    g_header->SetText(GameState::GetPlayerName());
    g_tabName->SetText(StringTable::GetStringF("INVENTORY_CAT_%02d", g_tab + 1));
    g_figure.body->SetVisibility(GameState::IsMalePlayer());
    g_figure.bodyFemale->SetVisibility(!GameState::IsMalePlayer());
    player->ResetStats(true);
    if (BaseSquad* s = player->GetSquad())
        for (unsigned i = 1; i < (unsigned)s->GetSoldierCount(); ++i)
            if (Entity* m = s->GetSoldier(i)) m->ResetStats(false);
    Sets::FindActiveSetState();
    PvP::PlayerEquipmentInfo equip;
    equip.GetPlayerEquipment();
    for (unsigned i = 0; i != 11; ++i) {
        GameState::PlayerItem* item = GetUniqueItemAt(equip, i);
        ApplyEquipment(item, player);
        if (g_tab == 7) item = GameState::GetCustomizationAt(g_customOrder[i]);
        EquipSlot& e = g_equip[i];
        if (item) {
            Items::ItemInfo* info = item->info;
            if (!info) continue;
            info->PutImageInHolder(e.bg);
            bool selected = g_selectedEquip == (int)i;
            if (g_selectedItem != -1 && g_items[g_selectedItem].data && item == g_items[g_selectedItem].data)
                selected = true;
            e.selector->SetVisibility(selected);
            e.broken->SetVisibility(item->IsBroken());
            for (unsigned k = 0; k != 5; ++k) e.itemClass[k]->SetVisibility(info->itemLevel == k && !item->IsBroken());
        } else {
            e.bg->SetTexture(IconManager::GetIcon(g_tab == 7 ? g_customNames[i] : g_slotBackgrounds[i], false), false);
            e.selector->SetVisibility(false);
            e.broken->SetVisibility(false);
            for (Window* w : e.itemClass) w->SetVisibility(false);
        }
    }
    for (unsigned i = 1; i != 10; ++i) ApplyEquipment(equip.GetUniqueCustomizationAt((int)i), player);
    if (g_tab == 7) {
        // The figure wears only what is tried on, over the customisations.
        for (auto& it : equip.items) it = PvP::PlayerEquipmentInfo::ItemInfo();
        for (unsigned b = 0; b != 10; ++b)
            if (g_previewEquip[b]) equip.items[b].id = g_previewEquip[b]->id;
        for (unsigned b = 1; b != 10; ++b)
            if (g_previewCustom[b]) equip.customs[b].id = g_previewCustom[b]->id;
    }
    g_figure.PlacePlayerEquipment(equip, (float)g_figure.root->x, (float)g_figure.root->y, g_itemHolder,
                                  GameState::IsMalePlayer(), 1.f, 0.f, 0.f, 0.f, true);
    ForEachCompleteBonus([&](MetaExpression* e) { if (e) e->OnApplyItemEffect(player); });
    for (unsigned i = 0; Items::ItemInfo* info = Items::EnumItemInfo(i); ++i) {
        if (info->type != 0xe) continue;
        const uint32_t left = GameState::GetItemBuffRemainingTime(info->id);
        g_buffTimes[info->id] = left;
        if (left == 0 || !info->meta) continue;
        info->meta->OnApplyItemEffect(player);
        info->meta->OnApplyItemBuffEffect(player);
    }
    BeltBarWindow::UpdateContents();
    if (!GameState::IsArenaCombatActive()) BeltBarWindow::UpdateBeltItems();

    GetItemsForTabAndMode(g_tab);
    g_scroller.itemCount = (unsigned)g_items.size();
    g_scroller.UpdateScroll();
    for (unsigned i = 0; i != 12; ++i) UpdateCell(i, now);
    UpdateItemTimers();
    if (g_keepHp) player->SetHP(g_keptHp);
    if (player->GetHP() > player->GetHpMax()) player->SetHP(player->GetHpMax());
    const bool shopOpen = GameState::TaskCompleted((unsigned)Setting("shop_unlock_task").GetInt());
    g_noItems->SetVisibility(g_items.empty());
    g_noItems->SetText(StringTable::GetString(shopOpen ? "INVENTORY_NO_ITEMS_IN_TAB" : "INVENTORY_NO_ITEMS_IN_TAB_LOCKED"));
    g_shop->SetVisibility(g_items.empty() &&
                          GameState::TaskCompleted((unsigned)Setting("shop_unlock_task").GetInt()));
    g_shopText->SetText(StringTable::GetString("INVENTORY_SHOP_TO_SHOP"));
    const SelectedItem sel = GetSelectedItem();
    UpdateSetPanel();
    UpdateStats(player, sel, previews);
    UpdateCustomStrip(previews);
    g_root->UpdatePosition();
}

}  // namespace CharacterInfoWindow
