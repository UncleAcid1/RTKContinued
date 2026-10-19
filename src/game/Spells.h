// Spells: spells.xml, the hero's spells (fire, lightning, arcane; three grades each).
// Port of Spells (libkingdom.so 5.11, @0x2251b4..0x225768).
//
// Every SpellInfo is in one list (0x6156ac, file order; EnumSpellInfo) and found by id.
#pragma once
#include <cstdint>

namespace Render { struct Texture; }

namespace Spells {

struct SpellInfo {                     // 0x30 bytes
    uint32_t id = 0;                   // +0x00 "spell_id"
    const char* name = nullptr;        // +0x04 "name" (also the StringTable key)
    Render::Texture* icon = nullptr;   // +0x08 "icon"
    Render::Texture* markerOn = nullptr;    // +0x0c "marker" with its "_off" turned to "_on"
    Render::Texture* marker = nullptr;      // +0x10 "marker"
    const char* animation = nullptr;   // +0x14 "animation": 'b' fire, 'l' lightning, 'i' ice
    uint32_t damage = 0;               // +0x18 "damage"
    uint32_t range = 0;                // +0x1c "range" (the area: 0 a single target)
    uint32_t slot = 0;                 // +0x20 "slot": 1 fire, 2 lightning, 3 arcane
    uint32_t level = 0;                // +0x24 "level" the player level needed (99: not yet)
    uint32_t unlockQuest = 0;          // +0x28 "unlock_quest" (0: by level alone)
    uint32_t cost = 0;                 // +0x2c "cost" mana
};

bool Init(const char* file);                       // @0x225768
SpellInfo* GetSpellInfo(uint32_t id);              // @0x2253e8
SpellInfo* EnumSpellInfo(unsigned i);              // @0x22573c
SpellInfo* GetSpellLockedByTask(uint32_t task);    // @0x225380 the spell a quest unlocks
// @0x2254f8: the dearest spell of `slot` that costs at most `mana`. With `unlocked` only one the
// hero has (its quest done and its level reached), else any.
SpellInfo* GetBestSpellForSlot(uint32_t slot, uint32_t mana, bool unlocked);

}  // namespace Spells
