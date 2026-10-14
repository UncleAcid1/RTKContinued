// PvP::PlayerEquipmentInfo (0x348 bytes): what a hero wears, as the figure drawings need it: the
// ten equipment bindings and the 32 customisation bindings (face, hair, ...), each as a record
// of the item's unique id, its id, two of the owned item's fields and "broken". The player's own
// is read from GameState (GetPlayerEquipment); another player's comes from the server (online).
#pragma once
#include <cstdint>

namespace Items { struct ItemInfo; }
namespace GameState { struct PlayerItem; }

namespace PvP {

struct PlayerEquipmentInfo {
    struct ItemInfo {               // 0x14 bytes (constructor @0x1eb5d0, also @0x1eb5ec)
        uint32_t uniqueId = 0;      // +0x00
        uint32_t id = 0;            // +0x04
        uint32_t f08 = 0;           // +0x08 the owned item's +0x0c (durability left)
        uint32_t f0c = 0;           // +0x0c the owned item's +0x10
        bool broken = false;        // +0x10
    };
    ItemInfo items[10];             // +0x00
    ItemInfo customs[32];           // +0xc8

    void GetPlayerEquipment();      // @0x1ec8f8 (the original constructs the result with it)
    bool IsItemBrokenAt(int b) const { return items[b].broken; }             // @0x1eb608
    bool IsCustomizationBrokenAt(int b) const { return customs[b].broken; }  // @0x1eb618
    Items::ItemInfo* GetItemAt(int b) const;                                 // @0x1ebd10
    // @0x1ec154: the customisation bound there, else the one a worn piece brings (ADD_CUSTOM).
    Items::ItemInfo* GetCustomizationAt(int b) const;
    // @0x1ec240 / @0x1ec050: false when a worn piece or a customisation (1..9) hides that
    // binding (HIDE_BIND, external numbering) or that customisation (HIDE_CUSTOM).
    bool IsItemVisibleAt(int b) const;
    bool IsCustomizationVisibleAt(int b) const;
    GameState::PlayerItem* GetUniqueItemAt(int b) const;            // @0x1ec35c
    GameState::PlayerItem* GetUniqueCustomizationAt(int b) const;   // @0x1ec34c
};

}  // namespace PvP
