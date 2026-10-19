// ItemShopWindow: the item shop: items sold for gold, crystals or another item (cost / cost2 /
// cost_items), in nine tabs.
// Port of ItemShopWindow (libkingdom.so 5.11), 0x2dd098..0x2e72c8. Statics at 0x622cc0 and
// 0x60f2c4.
//
// DECIDED (user 2026-10-18): gold and crystal prices only; the real-money payment
// (OnPaymentDataReceived, Billing) is not ported. Chest keys are not sold (5p-6).
// DECIDED (user 2026-10-19): the premium items stay (all crystal-priced); the item packs
// (item_packs.xml) are dropped: never listed, so never sold; the Halloween pumpkin cost stays.
// Discounts (ItemInfo +0x7c..) are the server's: dormant offline.
//
// Window_shop.xml: nine tabs, a grid of twelve cells (three a line, scrolled), the selected item's
// card with its Buy buttons.
// 5q-1: the item lists; 5q-2: the window opens and closes; UpdateContents is 5q-3, the actions 5q-4.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "engine/IconManager.h"
#include "engine/Render.h"
#include "game/BuildingHovers.h"
#include "game/Chests.h"
#include "game/GameState.h"
#include "game/Items.h"
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

namespace ItemShopWindow {
namespace {

// A cell of the shop's list (8 bytes).
struct MixedItem {
    uint32_t f0 = 0;                   // +0x00 (0 for every listed item)
    Items::ItemInfo* info = nullptr;   // +0x04
};

std::vector<MixedItem> g_items;        // 0x622cd0 (data +0x10, inline buffer +0x14, cap +0x1c, count +0x20)
unsigned g_tab = 3;                    // 0x60f2c4 the tab shown

// @0x2dd098 premium first, then by type, then by sort_id.
bool SortItemsByTypeAndPriority(const MixedItem& a, const MixedItem& b) {
    if (a.info->premium != b.info->premium) return a.info->premium > b.info->premium;
    if (a.info->type != b.info->type) return a.info->type < b.info->type;
    return a.info->sortId < b.info->sortId;
}

// @0x2dd0f0
bool SortItemsByPriority(const MixedItem& a, const MixedItem& b) {
    return a.info->sortId < b.info->sortId;
}

// @0x2df880 the list's push (STLport vector growth: size * 1.5, at least size + 0x20).
void PushItem(const MixedItem& item) {
    g_items.push_back(item);
}


using GUI::Textfield;
using GUI::Window;

// @0x2de5f0 ItemShopUnlock (0xc bytes): the "unlocked by" line of an item's card.
struct ItemShopUnlock {
    Window* root = nullptr;           // +0x00 "%s"
    Window* icon = nullptr;           // +0x04 icon_reputation (takes a depth slot)
    Textfield* text = nullptr;        // +0x08 text_unlock

    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        icon = GUI::GetWindowTypedF<Window>(w, "%s.icon_reputation", p);
        icon->takesZ = true;
        text = GUI::GetWindowTypedF<Textfield>(w, "%s.text_unlock", p);
    }
};

// @0x2de660 ItemCounter (0x10 bytes): the count to buy, with - and +.
struct ItemCounter {
    Window* root = nullptr;           // +0x00 "%s"
    Window* less = nullptr;           // +0x04 button_less_money
    Window* more = nullptr;           // +0x08 button_more_money
    Textfield* sum = nullptr;         // +0x0c text_summ

    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        less = GUI::GetWindowTypedF<Window>(w, "%s.button_less_money", p);
        more = GUI::GetWindowTypedF<Window>(w, "%s.button_more_money", p);
        sum = GUI::GetWindowTypedF<Textfield>(w, "%s.text_summ", p);
    }
};

// @0x2de6e0 CostButton (0x28 bytes): a Buy button in its gold (active_gold) or crystal (buy_green)
// look, with its price.
struct CostButton {
    Window* root = nullptr;           // +0x00 "%s"
    Window* active = nullptr;         // +0x04 button_active_gold (shown in the crystal look)
    Window* green = nullptr;          // +0x08 button_buy_green
    Textfield* buyCrystal = nullptr;  // +0x0c text_buy_crystal
    Textfield* buyGold = nullptr;     // +0x10 text_buy_gold
    Window* crystalIcon = nullptr;    // +0x14 icon_35_crystal
    Window* goldIcon = nullptr;       // +0x18 icon_35_gold_coin (takes a depth slot)
    Textfield* priceCrystal = nullptr;   // +0x1c text_price_crystal
    Textfield* priceGold = nullptr;   // +0x20 text_price_gold
    bool crystals = false;            // +0x24 the crystal look (OnBuy's argument)

    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        active = GUI::GetWindowTypedF<Window>(w, "%s.button_active_gold", p);
        green = GUI::GetWindowTypedF<Window>(w, "%s.button_buy_green", p);
        buyCrystal = GUI::GetWindowTypedF<Textfield>(w, "%s.text_buy_crystal", p);
        buyGold = GUI::GetWindowTypedF<Textfield>(w, "%s.text_buy_gold", p);
        crystalIcon = GUI::GetWindowTypedF<Window>(w, "%s.icon_35_crystal", p);
        goldIcon = GUI::GetWindowTypedF<Window>(w, "%s.icon_35_gold_coin", p);
        goldIcon->takesZ = true;
        priceCrystal = GUI::GetWindowTypedF<Textfield>(w, "%s.text_price_crystal", p);
        priceGold = GUI::GetWindowTypedF<Textfield>(w, "%s.text_price_gold", p);
        crystals = false;
    }
    // @0x2ddf94: the green look with a crystal price.
    void SetCrystalCost(int cost) {
        active->SetVisibility(true);
        green->SetVisibility(false);
        buyCrystal->SetVisibility(true);
        buyGold->SetVisibility(false);
        crystalIcon->SetVisibility(true);
        goldIcon->SetVisibility(false);
        priceCrystal->SetVisibility(true);
        priceGold->SetVisibility(false);
        buyCrystal->SetText(StringTable::GetString("ITEM_SHOP_BUY"));
        buyGold->SetText(StringTable::GetString("ITEM_SHOP_BUY"));
        priceCrystal->SetText(ToWideString(cost));
        priceGold->SetText(ToWideString(cost));
        crystals = true;
    }
    // @0x2de120: the gold look with a gold price.
    void SetGoldCost(int cost) {
        active->SetVisibility(false);
        green->SetVisibility(true);
        buyCrystal->SetVisibility(false);
        buyGold->SetVisibility(true);
        crystalIcon->SetVisibility(false);
        goldIcon->SetVisibility(true);
        goldIcon->SetTexture(IconManager::GetIcon("gold_buy", false), true);
        priceCrystal->SetVisibility(false);
        priceGold->SetVisibility(true);
        buyCrystal->SetText(StringTable::GetString("ITEM_SHOP_BUY"));
        buyGold->SetText(StringTable::GetString("ITEM_SHOP_BUY"));
        priceCrystal->SetText(ToWideString(cost));
        priceGold->SetText(ToWideString(cost));
        crystals = false;
    }
    // @0x2de2f4: the gold look with the cost item's icon (none: the gold coin).
    void SetItemCost(int cost, Items::ItemInfo* item) {
        SetGoldCost(cost);
        if (item) goldIcon->SetTexture(item->GetIcon(), false);
    }
};

// @0x2de7fc DetailedItemInfo (0x7c bytes): an item's icon with its quality frame, marks and
// durability bar.
struct DetailedItemInfo {
    Window* root = nullptr;           // +0x00 "%s"
    Window* broken = nullptr;         // +0x04 inventory_item_icon_holder.item_class_broken
    Window* locked = nullptr;         // +0x08 .item_class_locked
    Window* repOverlay = nullptr;     // +0x0c .shop_rep_item_overlay
    Window* itemClass[5] = {};        // +0x10 .item_class_01..05
    Window* icon = nullptr;           // +0x24 .item_icon_holder_item (takes a depth slot)
    Window* lock = nullptr;           // +0x28 .lock
    Textfield* levelReq = nullptr;    // +0x2c .text_level_req
    Window* recommended = nullptr;    // +0x30 .recomended_item
    Window* owned = nullptr;          // +0x34 .icon_gold_owned
    Window* discount = nullptr;       // +0x38 .discount_label_holder
    Textfield* quantity = nullptr;    // +0x3c text_quantity
    Window* nameBar = nullptr;        // +0x40 holy_fuck_its_a_black_bar_name_whoop
    Window* darkRed = nullptr;        // +0x44 durability_bar_dark_red
    Textfield* darkRedText = nullptr; // +0x48 text_bar_dark_red
    Window* red = nullptr;            // +0x4c durability_bar_red
    Textfield* redText = nullptr;     // +0x50 text_bar_red
    GUI::ClipRect redClip = {0, 0, 0, 0};     // +0x54
    Window* green = nullptr;          // +0x64 durability_bar_green
    Textfield* greenText = nullptr;   // +0x68 text_bar_green
    GUI::ClipRect greenClip = {0, 0, 0, 0};   // +0x6c

    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        broken = GUI::GetWindowTypedF<Window>(w, "%s.inventory_item_icon_holder.item_class_broken", p);
        locked = GUI::GetWindowTypedF<Window>(w, "%s.inventory_item_icon_holder.item_class_locked", p);
        repOverlay = GUI::GetWindowTypedF<Window>(w, "%s.inventory_item_icon_holder.shop_rep_item_overlay", p);
        for (int i = 0; i != 5; ++i)
            itemClass[i] = GUI::GetWindowTypedF<Window>(w, "%s.inventory_item_icon_holder.item_class_%02d", p, i + 1);
        icon = GUI::GetWindowTypedF<Window>(w, "%s.inventory_item_icon_holder.item_icon_holder_item", p);
        icon->takesZ = true;
        lock = GUI::GetWindowTypedF<Window>(w, "%s.inventory_item_icon_holder.lock", p);
        levelReq = GUI::GetWindowTypedF<Textfield>(w, "%s.inventory_item_icon_holder.text_level_req", p);
        recommended = GUI::GetWindowTypedF<Window>(w, "%s.inventory_item_icon_holder.recomended_item", p);
        owned = GUI::GetWindowTypedF<Window>(w, "%s.inventory_item_icon_holder.icon_gold_owned", p);
        quantity = GUI::GetWindowTypedF<Textfield>(w, "%s.text_quantity", p);
        discount = GUI::GetWindowTypedF<Window>(w, "%s.inventory_item_icon_holder.discount_label_holder", p);
        nameBar = GUI::GetWindowTypedF<Window>(w, "%s.holy_fuck_its_a_black_bar_name_whoop", p);
        darkRed = GUI::GetWindowTypedF<Window>(w, "%s.durability_bar_dark_red", p);
        darkRedText = GUI::GetWindowTypedF<Textfield>(w, "%s.text_bar_dark_red", p);
        red = GUI::GetWindowTypedF<Window>(w, "%s.durability_bar_red", p);
        redText = GUI::GetWindowTypedF<Textfield>(w, "%s.text_bar_red", p);
        redClip = {red->x, 0, red->x + red->w, 0x400};
        red->SetClipRect(&redClip);
        redText->SetClipRect(&redClip);
        green = GUI::GetWindowTypedF<Window>(w, "%s.durability_bar_green", p);
        greenText = GUI::GetWindowTypedF<Textfield>(w, "%s.text_bar_green", p);
        // (As the original: the green part's clip is made from the red bar's place.)
        greenClip = {red->x, 0, red->x + red->w, 0x400};
        green->SetClipRect(&greenClip);
        greenText->SetClipRect(&greenClip);
    }
};

// @0x2deaf0 UserItemInfo (0xe4 bytes): the owned item the card compares with.
struct UserItemInfo {
    Window* root = nullptr;           // +0x00 "%s"
    Window* shading3 = nullptr;       // +0x04 quest_shading_bg_3_lines
    Window* shading2 = nullptr;       // +0x08 quest_shading_bg_2_lines
    Shared::EffectDescriptionLine lines[3];   // +0x0c info_box_info_line_01..03
    Textfield* name[5] = {};          // +0x54 shop_item_class_holder_1_line.text_item_class_01..05
    DetailedItemInfo item;            // +0x68 info_box_detailed_item_info

    void LoadFrom(Window* w, const char* p) {
        char sub[0x80];
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        shading3 = GUI::GetWindowTypedF<Window>(w, "%s.quest_shading_bg_3_lines", p);
        shading2 = GUI::GetWindowTypedF<Window>(w, "%s.quest_shading_bg_2_lines", p);
        for (int i = 0; i != 3; ++i) {
            std::snprintf(sub, sizeof sub, "%s.info_box_info_line_%02d", p, i + 1);
            lines[i].LoadFrom(w, sub);
        }
        for (int i = 0; i != 5; ++i)
            name[i] = GUI::GetWindowTypedF<Textfield>(w, "%s.shop_item_class_holder_1_line.text_item_class_%02d", p, i + 1);
        std::snprintf(sub, sizeof sub, "%s.info_box_detailed_item_info", p);
        item.LoadFrom(w, sub);
    }
};

void OnSetHolder(unsigned which);
void OnBuy(bool crystals);

// @0x2dec4c GenericItemInfo (0x30c bytes): the selected item's card (inventory_item_info_box): its
// name, the owned item to compare, the description and stats, type / set / pack lines, and the
// buy buttons (one; three with a count; two with a count; two).
struct GenericItemInfo {
    Window* root = nullptr;           // +0x000 "%s"
    Textfield* name[5] = {};          // +0x004 item_name_shop_text_holder.text_item_class_01..05
    UserItemInfo user;                // +0x018 shop_comaparble_holder
    Window* notComparable = nullptr;  // +0x0fc shop_item_not_comparable
    Textfield* noSuchItem = nullptr;  // +0x100 its text_no_such_item
    Textfield* descLarge = nullptr;   // +0x104 text_desc_large
    Textfield* descLargeHave = nullptr;   // +0x108 text_desc_large_have
    Textfield* youHave = nullptr;     // +0x10c text_you_have
    Window* equip = nullptr;          // +0x110 button_shop_equip (OnUseItem)
    Window* equipIcon = nullptr;      // +0x114 its icon_placeholder_35 (takes a depth slot)
    Textfield* equipText = nullptr;   // +0x118 its text
    Shared::EffectDescriptionLine lines[4];   // +0x11c stat_block_shop.stats_text_holder_04.info_box_info_line_01..04
    Shared::EffectDescriptionLine huge[2];    // +0x17c stat_block_shop.stats_text_holder_02.info_box_info_line_huge_01..02
    Window* typeHolder = nullptr;     // +0x1ac shop_item_type_holder
    Window* typeIcon = nullptr;       // +0x1b0 its item_type_icon (takes a depth slot)
    Textfield* typeText = nullptr;    // +0x1b4 its text_item_type
    Window* setHolder = nullptr;      // +0x1b8 shop_item_set_holder (OnSetHolder(0))
    Window* setIcon = nullptr;        // +0x1bc its item_set_icon (takes a depth slot)
    Textfield* setText = nullptr;     // +0x1c0 its text_item_set
    Window* typeHolderLarge = nullptr;    // +0x1c4 shop_item_type_holder_large
    Window* typeIconLarge = nullptr;  // +0x1c8 its item_type_icon (takes a depth slot)
    Textfield* typeTextLarge = nullptr;   // +0x1cc its text_item_type
    Window* packHolder = nullptr;     // +0x1d0 item_pack_info_holder (packs are not sold)
    Window* packIcon = nullptr;       // +0x1d4 its icon_pack_container (takes a depth slot)
    Textfield* packCount = nullptr;   // +0x1d8 its text_item_count
    Textfield* packDesc = nullptr;    // +0x1dc its text_desc
    Textfield* packSave = nullptr;    // +0x1e0 its text_save
    CostButton buySingle;             // +0x1e4 shop_button_holder.button_buy_single
    Window* three = nullptr;          // +0x20c shop_button_holder.shop_3_buttons
    ItemCounter threeCount;           // +0x210 its count_holder
    CostButton threeBuy1;             // +0x220 its button_buy_01
    CostButton threeBuy2;             // +0x248 its button_buy_02
    Window* two = nullptr;            // +0x270 shop_button_holder.shop_2_buttons
    ItemCounter twoCount;             // +0x274 its count_holder
    CostButton twoBuy;                // +0x284 its button_buy_01
    Window* two2 = nullptr;           // +0x2ac shop_button_holder.shop_2_buttons_02
    CostButton two2Buy2;              // +0x2b0 its button_buy_02 (OnBuy(false))
    CostButton two2Buy1;              // +0x2d8 its button_buy_01 (OnBuy(true))
    ItemShopUnlock unlock;            // +0x300 shop_button_holder.shop_unlock_item.button_shop_unlock

    void LoadFrom(Window* w, const char* p) {
        char sub[0x80];
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        for (int i = 0; i != 5; ++i)
            name[i] = GUI::GetWindowTypedF<Textfield>(w, "%s.item_name_shop_text_holder.text_item_class_%02d", p, i + 1);
        std::snprintf(sub, sizeof sub, "%s.shop_comaparble_holder", p);
        user.LoadFrom(w, sub);
        notComparable = GUI::GetWindowTypedF<Window>(w, "%s.shop_item_not_comparable", p);
        noSuchItem = GUI::GetWindowTypedF<Textfield>(w, "%s.shop_item_not_comparable.text_no_such_item", p);
        descLarge = GUI::GetWindowTypedF<Textfield>(w, "%s.text_desc_large", p);
        descLargeHave = GUI::GetWindowTypedF<Textfield>(w, "%s.text_desc_large_have", p);
        youHave = GUI::GetWindowTypedF<Textfield>(w, "%s.text_you_have", p);
        equip = GUI::GetWindowTypedF<Window>(w, "%s.button_shop_equip", p);
        equipIcon = GUI::GetWindowTypedF<Window>(w, "%s.button_shop_equip.icon_placeholder_35", p);
        equipIcon->takesZ = true;
        equipText = GUI::GetWindowTypedF<Textfield>(w, "%s.button_shop_equip.text", p);
        for (int i = 0; i != 4; ++i) {
            std::snprintf(sub, sizeof sub, "%s.stat_block_shop.stats_text_holder_04.info_box_info_line_%02d", p, i + 1);
            lines[i].LoadFrom(w, sub);
        }
        for (int i = 0; i != 2; ++i) {
            std::snprintf(sub, sizeof sub, "%s.stat_block_shop.stats_text_holder_02.info_box_info_line_huge_%02d", p, i + 1);
            huge[i].LoadFrom(w, sub);
        }
        typeHolder = GUI::GetWindowTypedF<Window>(w, "%s.shop_item_type_holder", p);
        typeIcon = GUI::GetWindowTypedF<Window>(w, "%s.shop_item_type_holder.item_type_icon", p);
        typeIcon->takesZ = true;
        typeText = GUI::GetWindowTypedF<Textfield>(w, "%s.shop_item_type_holder.text_item_type", p);
        setHolder = GUI::GetWindowTypedF<Window>(w, "%s.shop_item_set_holder", p);
        setHolder->SetOnClick([] { OnSetHolder(0); });
        setIcon = GUI::GetWindowTypedF<Window>(w, "%s.shop_item_set_holder.item_set_icon", p);
        setIcon->takesZ = true;
        setText = GUI::GetWindowTypedF<Textfield>(w, "%s.shop_item_set_holder.text_item_set", p);
        typeHolderLarge = GUI::GetWindowTypedF<Window>(w, "%s.shop_item_type_holder_large", p);
        typeIconLarge = GUI::GetWindowTypedF<Window>(w, "%s.shop_item_type_holder_large.item_type_icon", p);
        typeIconLarge->takesZ = true;
        typeTextLarge = GUI::GetWindowTypedF<Textfield>(w, "%s.shop_item_type_holder_large.text_item_type", p);
        packHolder = GUI::GetWindowTypedF<Window>(w, "%s.item_pack_info_holder", p);
        packIcon = GUI::GetWindowTypedF<Window>(w, "%s.item_pack_info_holder.icon_pack_container", p);
        packIcon->takesZ = true;
        packCount = GUI::GetWindowTypedF<Textfield>(w, "%s.item_pack_info_holder.text_item_count", p);
        packDesc = GUI::GetWindowTypedF<Textfield>(w, "%s.item_pack_info_holder.text_desc", p);
        packSave = GUI::GetWindowTypedF<Textfield>(w, "%s.item_pack_info_holder.text_save", p);
        std::snprintf(sub, sizeof sub, "%s.shop_button_holder.button_buy_single", p);
        buySingle.LoadFrom(w, sub);
        three = GUI::GetWindowTypedF<Window>(w, "%s.shop_button_holder.shop_3_buttons", p);
        std::snprintf(sub, sizeof sub, "%s.shop_button_holder.shop_3_buttons.count_holder", p);
        threeCount.LoadFrom(w, sub);
        std::snprintf(sub, sizeof sub, "%s.shop_button_holder.shop_3_buttons.button_buy_01", p);
        threeBuy1.LoadFrom(w, sub);
        std::snprintf(sub, sizeof sub, "%s.shop_button_holder.shop_3_buttons.button_buy_02", p);
        threeBuy2.LoadFrom(w, sub);
        two = GUI::GetWindowTypedF<Window>(w, "%s.shop_button_holder.shop_2_buttons", p);
        std::snprintf(sub, sizeof sub, "%s.shop_button_holder.shop_2_buttons.count_holder", p);
        twoCount.LoadFrom(w, sub);
        std::snprintf(sub, sizeof sub, "%s.shop_button_holder.shop_2_buttons.button_buy_01", p);
        twoBuy.LoadFrom(w, sub);
        two2 = GUI::GetWindowTypedF<Window>(w, "%s.shop_button_holder.shop_2_buttons_02", p);
        std::snprintf(sub, sizeof sub, "%s.shop_button_holder.shop_2_buttons_02.button_buy_02", p);
        two2Buy2.LoadFrom(w, sub);
        two2Buy2.root->SetOnClick([] { OnBuy(false); });
        std::snprintf(sub, sizeof sub, "%s.shop_button_holder.shop_2_buttons_02.button_buy_01", p);
        two2Buy1.LoadFrom(w, sub);
        two2Buy1.root->SetOnClick([] { OnBuy(true); });
        std::snprintf(sub, sizeof sub, "%s.shop_button_holder.shop_unlock_item.button_shop_unlock", p);
        unlock.LoadFrom(w, sub);
    }
};

// @0x2df2a0 PriceInfo (0x40 bytes): a cell's price, without or with a discount.
struct PriceInfo {
    Window* root = nullptr;           // +0x00 "%s"
    Textfield* requiresLevel = nullptr;   // +0x04 shop_requires_level
    Window* confirm = nullptr;        // +0x08 icon_gold_confirm
    Window* plain = nullptr;          // +0x0c price_no_discount
    Textfield* price = nullptr;       // +0x10 its shop_price
    Textfield* priceCrystal = nullptr;    // +0x14 its shop_price_cry
    Textfield* notEnough = nullptr;   // +0x18 its shop_not_enough
    Window* icon = nullptr;           // +0x1c its icon_cost (takes a depth slot)
    Window* discounted = nullptr;     // +0x20 price_discount
    Textfield* realPrice = nullptr;   // +0x24 its shop_price_real
    Textfield* realPriceCrystal = nullptr;    // +0x28 its shop_price_real_cry
    Textfield* realNotEnough = nullptr;   // +0x2c its shop_not_enough_real
    Textfield* discPrice = nullptr;   // +0x30 its shop_price
    Textfield* discPriceCrystal = nullptr;    // +0x34 its shop_price_cry
    Textfield* discNotEnough = nullptr;   // +0x38 its shop_not_enough
    Window* discIcon = nullptr;       // +0x3c its icon_cost (takes a depth slot)

    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        requiresLevel = GUI::GetWindowTypedF<Textfield>(w, "%s.shop_requires_level", p);
        confirm = GUI::GetWindowTypedF<Window>(w, "%s.icon_gold_confirm", p);
        plain = GUI::GetWindowTypedF<Window>(w, "%s.price_no_discount", p);
        price = GUI::GetWindowTypedF<Textfield>(w, "%s.price_no_discount.shop_price", p);
        priceCrystal = GUI::GetWindowTypedF<Textfield>(w, "%s.price_no_discount.shop_price_cry", p);
        notEnough = GUI::GetWindowTypedF<Textfield>(w, "%s.price_no_discount.shop_not_enough", p);
        icon = GUI::GetWindowTypedF<Window>(w, "%s.price_no_discount.icon_cost", p);
        icon->takesZ = true;
        discounted = GUI::GetWindowTypedF<Window>(w, "%s.price_discount", p);
        realPrice = GUI::GetWindowTypedF<Textfield>(w, "%s.price_discount.shop_price_real", p);
        realPriceCrystal = GUI::GetWindowTypedF<Textfield>(w, "%s.price_discount.shop_price_real_cry", p);
        realNotEnough = GUI::GetWindowTypedF<Textfield>(w, "%s.price_discount.shop_not_enough_real", p);
        discPrice = GUI::GetWindowTypedF<Textfield>(w, "%s.price_discount.shop_price", p);
        discPriceCrystal = GUI::GetWindowTypedF<Textfield>(w, "%s.price_discount.shop_price_cry", p);
        discNotEnough = GUI::GetWindowTypedF<Textfield>(w, "%s.price_discount.shop_not_enough", p);
        discIcon = GUI::GetWindowTypedF<Window>(w, "%s.price_discount.icon_cost", p);
        discIcon->takesZ = true;
    }
};

// @0x2df480 ItemInfo (0xe4 bytes): a cell of the shop's grid (shop_item_holder.shop_item_01..12).
struct ItemInfo {
#define RTK_ICON "%s.inventory_slot_holder_01.inventory_item_icon"
#define RTK_HOLDER RTK_ICON ".inventory_item_icon_holder"
    Window* root = nullptr;           // +0x00 "%s" (OnItem)
    Window* boughtBg = nullptr;       // +0x04 slot_locked_bg_bought
    PriceInfo price;                  // +0x08 shop_price_holder
    Textfield* shopName[5] = {};      // +0x48 texts_shop.text_item_class_01..05
    Textfield* bagName[5] = {};       // +0x5c texts_bag.text_item_class_01..05
    Window* bg = nullptr;             // +0x70 shop_item_bg
    Window* selected = nullptr;       // +0x74 <icon>.selected_item
    Window* emptyBg = nullptr;        // +0x78 <icon>.item_icon_inventory_empty_bg
    Window* broken = nullptr;         // +0x7c <holder>.item_class_broken
    Window* locked = nullptr;         // +0x80 <holder>.item_class_locked
    Window* repOverlay = nullptr;     // +0x84 <holder>.shop_rep_item_overlay
    Window* itemClass[5] = {};        // +0x88 <holder>.item_class_01..05
    Window* icon = nullptr;           // +0x9c <holder>.item_icon_holder_item (takes a depth slot)
    Window* lock = nullptr;           // +0xa0 <holder>.lock
    Window* lockSmall = nullptr;      // +0xa4 <holder>.lock_small ("gold_16_lock")
    Textfield* levelReq = nullptr;    // +0xa8 <holder>.text_level_req
    Window* recommended = nullptr;    // +0xac <holder>.recomended_item
    Window* owned = nullptr;          // +0xb0 <holder>.icon_gold_owned
    Window* repProf = nullptr;        // +0xb4 <holder>.icon_16_rep_prof (takes a depth slot)
    Window* discountHolder = nullptr; // +0xb8 <holder>.discount_label_holder
    Shared::DiscountInfo discount[4]; // +0xbc its discount_label_blue, _green, _purple, _red
    Textfield* quantity = nullptr;    // +0xdc <icon>.text_quantity
    // (+0xe0: not set by LoadFrom.)

    void LoadFrom(Window* w, const char* p) {
        char sub[0x100];
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        boughtBg = GUI::GetWindowTypedF<Window>(w, "%s.slot_locked_bg_bought", p);
        std::snprintf(sub, sizeof sub, "%s.shop_price_holder", p);
        price.LoadFrom(w, sub);
        for (int i = 0; i != 5; ++i)
            shopName[i] = GUI::GetWindowTypedF<Textfield>(w, "%s.texts_shop.text_item_class_%02d", p, i + 1);
        for (int i = 0; i != 5; ++i)
            bagName[i] = GUI::GetWindowTypedF<Textfield>(w, "%s.texts_bag.text_item_class_%02d", p, i + 1);
        bg = GUI::GetWindowTypedF<Window>(w, "%s.shop_item_bg", p);
        selected = GUI::GetWindowTypedF<Window>(w, RTK_ICON ".selected_item", p);
        emptyBg = GUI::GetWindowTypedF<Window>(w, RTK_ICON ".item_icon_inventory_empty_bg", p);
        broken = GUI::GetWindowTypedF<Window>(w, RTK_HOLDER ".item_class_broken", p);
        locked = GUI::GetWindowTypedF<Window>(w, RTK_HOLDER ".item_class_locked", p);
        repOverlay = GUI::GetWindowTypedF<Window>(w, RTK_HOLDER ".shop_rep_item_overlay", p);
        for (int i = 0; i != 5; ++i)
            itemClass[i] = GUI::GetWindowTypedF<Window>(w, RTK_HOLDER ".item_class_%02d", p, i + 1);
        icon = GUI::GetWindowTypedF<Window>(w, RTK_HOLDER ".item_icon_holder_item", p);
        icon->takesZ = true;
        lock = GUI::GetWindowTypedF<Window>(w, RTK_HOLDER ".lock", p);
        levelReq = GUI::GetWindowTypedF<Textfield>(w, RTK_HOLDER ".text_level_req", p);
        lockSmall = GUI::GetWindowTypedF<Window>(w, RTK_HOLDER ".lock_small", p);
        lockSmall->SetTexture(IconManager::GetIcon("gold_16_lock", false), true);
        recommended = GUI::GetWindowTypedF<Window>(w, RTK_HOLDER ".recomended_item", p);
        owned = GUI::GetWindowTypedF<Window>(w, RTK_HOLDER ".icon_gold_owned", p);
        repProf = GUI::GetWindowTypedF<Window>(w, RTK_HOLDER ".icon_16_rep_prof", p);
        repProf->takesZ = true;
        discountHolder = GUI::GetWindowTypedF<Window>(w, RTK_HOLDER ".discount_label_holder", p);
        static const char* const kLabels[4] = {"blue", "green", "purple", "red"};
        for (int i = 0; i != 4; ++i) {
            std::snprintf(sub, sizeof sub, RTK_HOLDER ".discount_label_holder.discount_label_%s", p, kLabels[i]);
            discount[i].LoadFrom(w, sub);
        }
        quantity = GUI::GetWindowTypedF<Textfield>(w, RTK_ICON ".text_quantity", p);
    }
#undef RTK_HOLDER
#undef RTK_ICON
};

WindowManager::FunctionalWindow* g_queue = nullptr;   // ItemShopWindow::wnd 0x623d68
Window* g_root = nullptr;              // 0x622cc0 Window_shop.xml
GUI::TweenEffect* g_tween = nullptr;   // 0x622cc4
bool g_continue = false;               // 0x622cc8 the continuation runs when the shop closes
GUI::Callback* g_continuation = nullptr;   // 0x622ccc (owned) SetContinuationAfterTargetIsPurchased
Shared::TabHolder g_tabs[9];           // 0x622ce4 tab_01..09 (OnTab)
ItemInfo g_cells[12];                  // 0x622e28 shop_item_holder.shop_item_01..12 (OnItem)
GenericItemInfo g_info;                // 0x6238d8 inventory_item_info_box
Shared::ContentScroller g_scroller;    // 0x623be4 the grid, three cells a line
Window* g_scrollContainer = nullptr;   // 0x623c5c (the scroller's +0x78) scroll_bar_tiny_container_shop
bool g_arrowOnBuy = false;             // 0x623cf0 OnTweenIn points the helper arrow at the Buy button
ItemHoverWindow* g_hover = nullptr;    // 0x623cf4
int g_moveX = 0;                       // 0x623cf8 Move's last x
int g_pressX = 0;                      // 0x623cfc Click's last press x
Window* g_close = nullptr;             // 0x623d00 x_button.clickArea
Textfield* g_playerName = nullptr;     // 0x623d04 text_player_name
Textfield* g_tabName = nullptr;        // 0x623d08 text_tab_name
Textfield* g_noSelected = nullptr;     // 0x623d0c text_no_selected_item
int g_d10 = 0;                         // 0x623d10 (Show: 0; 5q-3)
float g_holdTime = 0.f;                // 0x623d14 a held count button's time
Window* g_holdButton = nullptr;        // 0x623d18 the count button held
int g_d64 = 0;                         // 0x623d64 (Show: 0; 5q-3)
int g_f2b4 = -1;                       // 0x60f2b4 (Show: -1; 5q-3)
int g_arrowItem = -1;                  // 0x60f2b8 OnTweenIn points the arrow at this cell
bool g_tablet = true;                  // 0x60f2bc
float g_scale = 1.f;                   // 0x60f2c0 the root's scale
}  // namespace

// @0x2e0fc0 the items of a tab into the list. An item is listed when it has a price (gold,
// crystals or an item) and is on sale (Items::ItemInfo::IsOnSale), and boosts (type 0x10) only with
// the setting "item_boosts". Tab 0: premium items, shop_tab 1, type 7 and packs, sorted premium
// first; tab 7: reputation_id 7 (a profession's items); another tab: the items whose shop_tab maps
// to it (0x599ba8: shop_tab 1..9 -> 0, 1, 2, 3, 4, 0, 0, 5, 6), or with no shop_tab by type and
// slot (internal slot, 10 without one): 1 equipment in slots 1 / 6, 2 equipment not in slots 1, 4,
// 5, 6, 9, 3 types 3..5, 4 equipment in slots 4, 5, 9, 5 crafted items, profession tools and type
// 9, 8 the rest (tabs 1..4 and 8: not crafted, no profession tool; tab 6: nothing).
void GetItemsForTabAndMode(unsigned tab) {
    static const unsigned kTabOfShopTab[9] = {0, 1, 2, 3, 4, 0, 0, 5, 6};   // 0x599ba8
    g_items.clear();
    for (unsigned i = 0; Items::ItemInfo* info = Items::EnumItemInfo(i); i++) {
        if (!info->cost && !info->cost2 && !info->costItem) continue;
        if (!info->IsOnSale()) continue;
        // PORT (user 2026-10-19): the item packs (type 0x80; tab 0 and, with no shop_tab, tab 8)
        // are not sold. PORT (DECIDED user 2026-10-18, 5p-6): nor are the chest keys.
        if (info->type == 0x80 || Chests::IsChestKey(info->id)) continue;
        const uint32_t slot = info->slot ? GameState::ExternalItemBindingToInternal(info->slot) : 10;
        if (info->type == 0x10 && !Setting("item_boosts").GetInt()) continue;
        if (tab == 0) {
            if (info->premium || info->shopTab == 1 || info->type == 7 || info->type == 0x80)
                PushItem({0, info});
            continue;
        }
        if (tab == 7) {
            if (info->reputationId == 7) PushItem({0, info});
            continue;
        }
        if (info->shopTab) {
            if (info->shopTab - 1 <= 8 && kTabOfShopTab[info->shopTab - 1] == tab) PushItem({0, info});
            continue;
        }
        bool rest = false;   // 0x2e12bc: not crafted, no profession tool, not tab 6
        switch (tab) {
        case 1:
            rest = info->type == 8 && (slot == 6 || slot == 1);
            break;
        case 2:
            rest = info->type == 8 && slot != 1 && slot != 6 && slot != 4 && slot != 5 && slot != 9;
            break;
        case 3:
            rest = info->type - 3 <= 2;
            break;
        case 4:
            rest = info->type == 8 && (slot == 9 || slot - 4 <= 1);
            break;
        case 5:
            if (info->isCraft || Professions::IsProfessionTool(info->id) || info->type == 9)
                PushItem({0, info});
            continue;
        default:
            rest = true;
            break;
        }
        if (rest && !info->isCraft && !Professions::IsProfessionTool(info->id) && tab != 6)
            PushItem({0, info});
    }
    std::stable_sort(g_items.begin(), g_items.end(),
                     tab ? SortItemsByPriority : SortItemsByTypeAndPriority);
}

// @0x2e13f4 whether the shop sells item id (pack 0) or pack (id 0), or either: the tabs' lists are
// searched (no chest key or pack is in them) (as the original, a match leaves the list at that tab; else the shown tab's list is
// made again).
bool ItemCanBeBought(uint32_t id, uint32_t pack) {
    for (unsigned tab = 0; tab < 9; tab++) {
        GetItemsForTabAndMode(tab);
        if (id == 0 && pack == 0) continue;
        for (const MixedItem& m : g_items) {
            if (id && m.info->id == id) return true;
            if (pack && m.info->type == 0x80 && m.info->packId == pack) return true;
        }
    }
    GetItemsForTabAndMode(g_tab);
    return false;
}

// @0x2e152c (no pack is listed: user 2026-10-19)
bool PackCanBeBought(uint32_t pack) {
    return ItemCanBeBought(0, pack);
}

// @0x2dd75c the Halloween prices: while task 0x77b runs, the items 0x2d2, 0x2d3, 0x2ef, 0x2f0 and
// 0x2ff are sold for pumpkins (item 0x300), cost2 x the setting "pumkins_for_crystal" (rounded);
// otherwise those pumpkin prices are taken away again.
void PrepareSpecialEventCost() {
    auto special = [](uint32_t id) {
        return id == 0x2ef || id == 0x2d2 || id == 0x2f0 || id == 0x2ff || id == 0x2d3;
    };
    if (Tasks::IsTaskActive(0x77b, true)) {
        for (unsigned i = 0; Items::ItemInfo* info = Items::EnumItemInfo(i); i++) {
            if (!special(info->id)) continue;
            info->sellable = true;
            info->costItem = 0x300;
            info->costItemCount = (uint32_t)(int)(0.5f + (float)info->cost2 *
                                                  Setting("pumkins_for_crystal").GetFloat());
        }
    } else {
        for (unsigned i = 0; Items::ItemInfo* info = Items::EnumItemInfo(i); i++) {
            if (special(info->id) && info->costItem == 0x300) {
                info->costItemCount = 0;
                info->sellable = false;
                info->costItem = 0;
            }
        }
    }
}

namespace {

void UpdateContents() {}               // STUB (5q-3)
void OnTab(unsigned) {}                // STUB (5q-4)
void OnItem(unsigned) {}               // STUB (5q-4)
void OnUserItem() {}                   // STUB (5q-4)
void OnSetHolder(unsigned) {}          // STUB (5q-4)
void OnCountChange(int) {}             // STUB (5q-4)
void OnUseItem() {}                    // STUB (5q-4)
void OnBuy(bool) {}                    // STUB (5q-4)

// @0x2dd8b8: the continuation (SetContinuationAfterTargetIsPurchased) runs once the shop is closed,
// when it was armed.
void OnTweenOut() {
    WindowManager::WindowHide(false);
    g_queue->shown = false;
    g_queue->MoveWindowDown(true);
    GameState::DropGamePauseState();
    if (g_continue && g_continuation) (*g_continuation)();
}

// @0x2dd96c: the helper arrow on the cell g_arrowItem (tapping it selects the cell), or with
// g_arrowOnBuy on the first Buy button shown (tapping it buys).
// UNVERIFIED (milestone 6): SoundsManager::PlaySound("ui_shop_opened", 1, false).
void OnTweenIn() {
    if (g_arrowItem != -1) {
        Window* cell = g_cells[g_arrowItem].root;
        TaskInfoWindow::BlockArrowLimitChangeOnce();
        BuildingHovers::SetArrowVisibleWindowLimit(2);
        BuildingHovers::ArrowAt((float)(g_root->x + cell->x + cell->w / 2), (float)(g_root->y + cell->y), false,
                                false, false, false, true, false, false);
        const unsigned item = (unsigned)g_arrowItem;
        BuildingHovers::SetArrowClickCallback(new GUI::Callback([item] { OnItem(item); }));
        g_arrowItem = -1;
    }
    if (g_arrowOnBuy) {
        TaskInfoWindow::BlockArrowLimitChangeOnce();
        BuildingHovers::SetArrowVisibleWindowLimit(2);
        CostButton* const order[6] = {&g_info.threeBuy1, &g_info.two2Buy2, &g_info.threeBuy2,
                                      &g_info.twoBuy,    &g_info.two2Buy1, &g_info.buySingle};
        for (CostButton* b : order) {
            if (!b->root->visible) continue;
            const bool crystals = b->crystals;
            BuildingHovers::SetArrowClickCallback(new GUI::Callback([crystals] { OnBuy(crystals); }));
            BuildingHovers::ArrowAt((float)(g_root->x + b->root->x + b->root->w / 2), (float)(g_root->y + b->root->y),
                                    false, false, false, false, true, false, false);
            break;
        }
    }
    g_arrowOnBuy = false;
}

// @0x2dd110
void SetZ(float z) {
    if (!g_queue->shown) return;
    g_root->SetZ(z);
    if (g_tween->active) return;
    g_root->SetPosition(g_tween->GetCenteredX(), g_tween->GetCenteredY());
}

// @0x2dd21c
float ZRange() { return g_queue->shown ? 0.098f : 0.001f; }

// @0x2de370: Back closes an open card first.
bool OnBack() {
    if (!g_queue->shown) return false;
    if (g_hover->shownHover) {
        g_hover->Hide();
        return true;
    }
    BuildingHovers::HideArrow();
    Hide();
    return true;
}

// @0x2de3d0
void Move(int x, int y) {
    g_moveX = x;
    g_scroller.Move(x, y);
    BeltBarWindow::Move(x, y);
}

// @0x2de41c
bool Click(int x, int y, bool pressed) {
    if (pressed) g_pressX = x;
    if (g_hover->Click(x, y, pressed)) return true;
    if (g_root->visibleSelf) {
        if (BuildingHovers::ClickOnArrow(x, y, pressed)) return true;
        if (!pressed) BuildingHovers::HideArrow();
        if (g_scroller.Click(x, y, pressed, g_root)) return true;
    }
    if (TopCityWindow::Click(x, y, pressed)) return true;
    const bool visible = g_root->visibleSelf;
    if (!g_root->Click(x, y, pressed, false)) BeltBarWindow::Click(x, y, pressed);
    return visible;
}

// @0x2e6240: the card kept in front; the grid scrolls; a held - / + button repeats, faster the
// farther the finger has moved since the press (every 0.3 s / (distance / 15), 1..16).
void Update(float dt) {
    if (g_hover->shownHover && g_hover->GetZ() != 0.001f) {
        g_hover->SetZ(0.001f);
        Render::SortRenderLayer(0xe, 1);
    }
    if (!g_root->visibleSelf) return;
    g_scroller.Update(dt);
    if (!GUI::GetCapture()) {
        g_holdButton = nullptr;
        g_holdTime = 0.f;
    }
    g_holdTime += dt;
    Window* button = nullptr;
    bool more = false;
    Window* const capture = GUI::GetCapture();
    if (capture == g_info.twoCount.less) button = capture;
    if (capture == g_info.twoCount.more) { button = capture; more = true; }
    if (capture == g_info.threeCount.less) button = capture;
    if (capture == g_info.threeCount.more) { button = capture; more = true; }
    if (!button) return;
    g_holdButton = button;
    int steps = std::abs(g_moveX - g_pressX) / 15;
    if (steps <= 0) steps = 1;
    else if (steps >= 16) steps = 16;
    if (0.3f / (float)steps >= g_holdTime) return;
    g_holdTime = 0.f;
    OnCountChange(more ? 1 : -1);
    GUI::SetCapture(button);
}

// @0x2de55c
void Deinit() {
    delete g_continuation;
    g_continuation = nullptr;
    GUI::RemoveTweenEffect(g_tween);
    g_tween = nullptr;
    delete g_root;
    g_root = nullptr;
    delete g_hover;
    g_hover = nullptr;
}

// @0x2dff54
void Init() {
    g_tablet = GUI::IsTabletVersion();
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Window_shop.xml", "Window_shop.png", -1.f, 0, 0, 0, 0,
                             false, 1.f);
    g_queue->RegisterTopWindow(g_root);
    g_scale = g_root->scale;
    g_root->SetVisibility(false);
    g_root->SetPosition((GUI::ScreenWidth() - 0x16 - g_root->w) / 2, GUI::GetVerticalCenter(g_root->h));
    g_tween = GUI::CreateTweenEffect(g_root, nullptr, OnTweenIn, OnTweenOut);
    g_tween->swingIn.clear();   // (SetSwingInSound(nullptr))
    g_tween->CenterWith(0, (int)(g_scale * -50.f), (int)(g_scale * -10.f), 0);
    g_close = GUI::GetWindowTyped<GUI::Button>(g_root, "x_button.clickArea");
    g_close->SetOnClick(Hide);
    g_close->SetActionSound("", 1);
    g_playerName = GUI::GetWindowTyped<Textfield>(g_root, "text_player_name");
    for (unsigned i = 0; i != 9; ++i) {
        char name[0x20];
        std::snprintf(name, sizeof name, "tab_%02d", i + 1);
        g_tabs[i].LoadFrom(g_root, name, false);
        g_tabs[i].root->SetOnClick([i] { OnTab(i); });
        g_tabs[i].root->SetActionSound("ui_tab", 1);
    }
    g_tabName = GUI::GetWindowTyped<Textfield>(g_root, "text_tab_name");
    for (unsigned i = 0; i != 12; ++i) {
        char name[0x40];
        std::snprintf(name, sizeof name, "shop_item_holder.shop_item_%02d", i + 1);
        g_cells[i].LoadFrom(g_root, name);
        g_cells[i].root->SetOnClick([i] { OnItem(i); });
        g_cells[i].root->SetActionSound("ui_click", 1);
    }
    g_info.LoadFrom(g_root, "inventory_item_info_box");
    g_info.equip->SetOnClick(OnUseItem);
    g_info.threeCount.less->SetOnClick([] { OnCountChange(-1); });
    g_info.threeCount.more->SetOnClick([] { OnCountChange(1); });
    g_info.twoCount.less->SetOnClick([] { OnCountChange(-1); });
    g_info.twoCount.more->SetOnClick([] { OnCountChange(1); });
    g_info.unlock.root->SetOnClick([] { OnSetHolder(1); });
    g_info.unlock.root->SetVisibility(false);
    g_info.user.root->SetOnClick(OnUserItem);
    g_noSelected = GUI::GetWindowTyped<Textfield>(g_root, "text_no_selected_item");
    g_scroller.Init();
    Window* mask = GUI::GetWindowTyped<Window>(g_root, "shop_item_holder.mask_inventory_items");
    g_scroller.viewport = {mask->x, mask->y, mask->x + mask->w, mask->y + mask->h};
    g_scroller.track = mask;
    for (ItemInfo& c : g_cells) {
        c.root->SetClipRect(&g_scroller.viewport);
        g_scroller.windows.push_back(c.root);
    }
    g_scrollContainer = GUI::GetWindowTyped<Window>(g_root, "scroll_bar_tiny_container_shop");
    g_scroller.scrollbar = GUI::GetWindowTyped<Window>(g_root, "scroll_bar_tiny_container_shop.scroll_bar_tiny");
    g_scroller.onScroll = UpdateContents;
    g_scroller.horizontal = false;
    g_scroller.unkA0 = 12;
    g_scroller.itemSize = g_cells[3].root->y - g_cells[0].root->y;
    g_scroller.perLine = 3;
    g_scroller.padStart = g_scroller.padEnd = (int)(g_scale * 10.f);
    g_scroller.viewport.top += (int)(g_scale * 7.f);
    g_queue->hideOnOuterClick = true;
    g_queue->fn.zRange = ZRange;
    g_queue->fn.update = Update;
    g_queue->fn.back = OnBack;
    g_queue->fn.show = Show;
    g_hover = new ItemHoverWindow();
}

}  // namespace

WindowManager::FunctionalWindow* Queue() {
    if (!g_queue) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.deinit = Deinit;
        f.click = Click;
        f.move = Move;
        f.setZ = SetZ;
        f.hide = Hide;
        g_queue = new WindowManager::FunctionalWindow("ItemShopWindow", std::move(f));
    }
    return g_queue;
}

float GetZ() { return g_root->z; }                         // @0x2dd1a8
bool IsVisible() { return g_root->visibleSelf; }           // @0x2dd1bc

// @0x2dd1d0: cb (owned) runs when the shop closes once armed (5q-4); Show drops it, so it is set
// after Show.
void SetContinuationAfterTargetIsPurchased(GUI::Callback* cb) {
    g_continue = false;
    delete g_continuation;
    g_continuation = cb;
}

// @0x2de34c
void Hide() {
    if (!g_root->visibleSelf) return;
    g_tween->AnimateOut();
}

// @0x2e6858: only after the city tutorial (secondTutorial 0x100), not in an arena fight (unless
// its in / out window is up; "CAN_NOT_OPEN_IN_ARENA"); from a boss fight the city's top bar comes
// back. The grid scrolls back to the top; the Halloween prices are brought up to date.
void Show() {
    if (GameState::IsTutorial() || GameState::secondTutorial != 0x100) return;
    if (GameState::IsArenaCombatActive() && !PlayerBattleInOutWindow::IsVisible()) {
        PopupWindow::Show(StringTable::GetString("CAN_NOT_OPEN_IN_ARENA"), PopupWindow::Hide, nullptr, nullptr, nullptr,
                          nullptr);
        return;
    }
    if (GameState::IsBossCombatActive()) {
        ArenaTurnWindow::Hide();
        TopCityWindow::Show();
    }
    const bool wasVisible = g_root->visibleSelf;
    g_root->SetVisibility(true);
    g_queue->shown = true;
    g_queue->MoveWindowOnTop(true);
    if (!wasVisible) {
        WindowManager::WindowShow(false);
        g_tween->AnimateIn();
        if (GameState::GetCurrentLocation() == 2) GameState::RaiseGamePauseState();
    }
    g_f2b4 = -1;
    g_d10 = 0;
    g_scroller.firstIndex = 0;
    g_scroller.HandleMove(-g_scroller.offset);
    PrepareSpecialEventCost();
    UpdateContents();
    g_root->PreloadTextures();   // GUI::PreloadWindow
    g_d64 = 0;
    g_continue = false;
    delete g_continuation;
    g_continuation = nullptr;
}

}  // namespace ItemShopWindow
