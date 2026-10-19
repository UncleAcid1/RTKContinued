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
// Step 5q-1: the item lists. The window itself is 5q-2 .. 5q-4.
#include <algorithm>
#include <vector>

#include "game/Chests.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/Professions.h"
#include "game/Setting.h"
#include "game/Tasks.h"
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

}  // namespace ItemShopWindow
