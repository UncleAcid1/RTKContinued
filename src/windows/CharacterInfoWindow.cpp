// CharacterInfoWindow: only the stat part of UpdateContents (see Windows.h).
#include "game/Entity.h"
#include "game/EntityManager.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/MetaExpression.h"
#include "game/Squad.h"
#include "windows/Windows.h"
#include "hud/HUD.h"

namespace CharacterInfoWindow {

namespace {
void ApplyEquipment(GameState::PlayerItem* it, Entity* player) {
    if (it && it->info && it->info->meta && !it->IsBroken()) it->info->meta->OnApplyItemEffect(player);
}
}  // namespace

// UNVERIFIED (milestone 5): the window's contents (tabs, item holders, the player picture, the
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
