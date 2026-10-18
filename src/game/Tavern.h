// Tavern: the warriors HireTroopsWindow offers, one list for all its tabs (tavern.xml's <Warrior>
// entries, plus the storage's goblins made from the "goblin_cost" setting). Port of Tavern
// (libkingdom.so 5.11), 0x23d86c..0x23e138; the list is the vector 0x6169f0 (entries from a
// 0x80-entry pool at 0x6169e0).
// Not ported: ChangeWarriorAttribute (the server's changes to an entry; online only).
#pragma once
#include <cstdint>

namespace Tavern {

struct TavernInfo {          // 0x30 bytes
    uint32_t id = 0;         // +0x00 "id" (goblins: 0x400 + n)
    uint32_t entity = 0;     // +0x04 "car_id": the warrior's EntityData id
    uint32_t probability = 0;   // +0x08 "probability"
    int hpMin = 0, hpMax = 0;   // +0x0c +0x10 "hp_range" ("a-b", or one number for both)
    uint32_t type = 0;       // +0x14 "type": the HireTroopsWindow tab it shows on (5 the goblins)
    uint32_t slot = 0;       // +0x18 "slot": its place on the tab
    uint32_t attack = 0;     // +0x1c the hired unit's attack (0: the unit's; Entity::SetOverrideAttack; 1 for the goblins)
    uint32_t defense = 0;    // +0x20 its defense (Entity::SetOverrideDefense)
    uint32_t gold = 0;       // +0x24 its own gold price (0: the unit's; EntityData::GetCostGold)
    uint32_t crystals = 0;   // +0x28 its own crystal price (0: the unit's)
    uint32_t level = 0;      // +0x2c the player level it needs (the goblin_cost entry's third number)
    int GenerateHP() const;  // @0x23d86c
};

bool Init(const char* file);              // @0x23e138 tavern.xml
void InitSpecial();                       // @0x23ddf0 the goblins
TavernInfo* EnumTavernInfo(unsigned i);   // @0x23ddc8 the i-th entry, null past the end
TavernInfo* GetTavernInfo(uint32_t id);   // @0x23d910
TavernInfo* GetForEntity(uint32_t entity);   // @0x23d8a8 the first entry for that unit

}  // namespace Tavern
