#include "game/PlayerEquipmentInfo.h"

#include "game/GameState.h"
#include "game/Items.h"
#include "game/MetaData.h"
#include "game/MetaExpression.h"

namespace PvP {

void PlayerEquipmentInfo::GetPlayerEquipment() {
    for (ItemInfo& i : items) i = ItemInfo();
    for (ItemInfo& c : customs) c = ItemInfo();
    for (unsigned b = 0; b < 10; ++b) {
        const GameState::PlayerItem* it = GameState::GetItemAt(b);
        if (!it) continue;
        items[b].id = it->id;
        items[b].uniqueId = it->uniqueId;
        items[b].broken = it->IsBroken();
        items[b].f08 = it->f0c;
        items[b].f0c = it->f10;
    }
    for (unsigned b = 1; b < 10; ++b) {
        const GameState::PlayerItem* it = GameState::GetCustomizationAt(b);
        if (!it) continue;
        customs[b].id = it->id;
        customs[b].uniqueId = it->uniqueId;
    }
}

Items::ItemInfo* PlayerEquipmentInfo::GetItemAt(int b) const { return Items::GetItemInfo(items[b].id); }

Items::ItemInfo* PlayerEquipmentInfo::GetCustomizationAt(int b) const {
    if (Items::ItemInfo* own = Items::GetItemInfo(customs[b].id)) return own;
    Items::ItemInfo* found = nullptr;
    for (const ItemInfo& i : items) {
        const Items::ItemInfo* info = Items::GetItemInfo(i.id);
        const MetaData* add = info && info->meta ? info->meta->FindChildData(kExpAddCustom) : nullptr;
        if (!add) continue;
        for (unsigned k = 0; k < add->GetChildrenCount(); ++k)
            if (add->GetChild(k)->GetChild(0)->GetInt() == b)
                found = Items::GetItemInfo((uint32_t)add->GetChild(k)->GetChild(1)->GetInt());
    }
    return found;
}

namespace {
// One of `info`'s list `exp` names binding `b` (external equipment numbers are converted).
bool Hides(uint32_t id, int exp, int b, bool external) {
    const Items::ItemInfo* info = Items::GetItemInfo(id);
    const MetaData* list = info && info->meta ? info->meta->FindChildData(exp) : nullptr;
    if (!list) return false;
    for (unsigned k = 0; k < list->GetChildrenCount(); ++k) {
        int v = list->GetChild(k)->GetInt();
        if (external) v = GameState::ExternalItemBindingToInternal((unsigned)v);
        if (v == b) return true;
    }
    return false;
}
}  // namespace

bool PlayerEquipmentInfo::IsItemVisibleAt(int b) const {
    for (const ItemInfo& i : items)
        if (Hides(i.id, kExpHideBind, b, true)) return false;
    for (int c = 1; c < 10; ++c)
        if (Hides(customs[c].id, kExpHideBind, b, true)) return false;
    return true;
}

bool PlayerEquipmentInfo::IsCustomizationVisibleAt(int b) const {
    for (const ItemInfo& i : items)
        if (Hides(i.id, kExpHideCustom, b, false)) return false;
    for (int c = 1; c < 10; ++c)
        if (Hides(customs[c].id, kExpHideCustom, b, false)) return false;
    return true;
}

GameState::PlayerItem* PlayerEquipmentInfo::GetUniqueItemAt(int b) const {
    return GameState::GetItemByUniqueID(items[b].uniqueId);
}

GameState::PlayerItem* PlayerEquipmentInfo::GetUniqueCustomizationAt(int b) const {
    return GameState::GetItemByUniqueID(customs[b].uniqueId);
}

}  // namespace PvP
