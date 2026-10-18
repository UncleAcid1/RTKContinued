// Tavern: the warriors the tavern and the barracks offer (tavern.xml's <Warrior> entries).
// Port of Tavern (libkingdom.so 5.11), 0x23d86c..0x23e138.
//
// UNVERIFIED (milestone 5b/5r): only the fields HireTroopsWindow reads so far; Tavern::Init (the
// list), EnumTavernInfo, GetTavernInfo, GenerateHP and the rest are ported with the window's
// contents.
#pragma once
#include <cstdint>

namespace Tavern {

struct TavernInfo {          // 0x30 bytes (Tavern::Init's pool)
    uint32_t id = 0;         // +0x00 "id"
    uint32_t entity = 0;     // +0x04 "car_id": the warrior's EntityData id
    uint32_t unk08 = 0;      // +0x08
    int hpMin = 0, hpMax = 0;   // +0x0c +0x10 (GenerateHP)
    int fixedHp = 0;         // +0x14 1: always hpMin
    uint32_t slot = 0;       // +0x18 the order on the window (SortTavernInfoBySlot)
};

}  // namespace Tavern
