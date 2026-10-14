// SetInfoWindow: the item set window (milestone 5). Only its item lookup is used before then.
#include "game/GameState.h"
#include "windows/Windows.h"

namespace SetInfoWindow {

// @0x33b3d4: the player's item `id`, or a better one of its improvement chain: worn (bindings
// 0..9), else the first in the bag. UNVERIFIED (milestone 5): the chain
// (Collections::GetCollectionWithSource, each collection's result +0xc) comes with the
// collections; until then only the item itself.
GameState::PlayerItem* GetPlayerItemOrBetter(uint32_t id) {
    for (unsigned i = 0; i < 10; ++i) {
        GameState::PlayerItem* it = GameState::GetItemAt(i);
        if (it && it->id == id) return it;
    }
    if (GameState::GetItemAmount(id, false) != 0) return GameState::GetFirstItemByID(id, false);
    return nullptr;
}

}  // namespace SetInfoWindow
