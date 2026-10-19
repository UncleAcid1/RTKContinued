// Sets: the equipment sets (sets.xml). A set has up to nine slots, each a list of items, and three
// bonuses, each unlocked by owning (or wearing) an item of every slot of its list. Port of Sets
// (libkingdom.so 5.11, @0x21a154..0x21c04c; statics 0x615530..0x61555c).
//
// The "state" is a set of item ids (the worn pieces, the inventory, or another player's pieces);
// CommitChanges counts, per set and bonus, the slots that state covers.
#pragma once
#include <cstdint>

class MetaData;
class MetaExpression;
namespace PvP { struct PlayerEquipmentInfo; }

namespace Sets {

struct Set {                               // 0x58 bytes
    uint32_t id = 0;                       // +0x00 "id"
    const char32_t* name = nullptr;        // +0x04 StringTable "name"
    bool active = false;                   // +0x08 "active"
    uint32_t sortId = 0;                   // +0x0c "sort_id" (a newer set replaces the one with its sort id)
    MetaData* slots[9] = {};               // +0x10 slot 1..9: <Item item_id=".." slot="n"/> (a list)
    MetaData* bonusSlots[3] = {};          // +0x34 "bonus1_slots".."bonus3_slots": the slots each bonus needs
    MetaExpression* bonus[3] = {};         // +0x40 "bonus", "bonus2", "bonus3"
    unsigned covered[3] = {};              // +0x4c CommitChanges: the slots of each bonus covered
};

bool Init(const char* file);               // @0x21ac70
bool SortSet(const Set* a, const Set* b);  // @0x21a154 by sort id
Set* GetSet(uint32_t id);                  // @0x21a16c
Set* EnumSet(unsigned i);                  // @0x21a1d0 (sorted by sort id)
Set* GetSetThatWasReplacedByNewSet(uint32_t id);   // @0x21a1f4 another set with its sort id
bool HasItemOrBetter(uint32_t id);         // @0x21a34c the state holds the item or one it improves into
void CommitChanges();                      // @0x21a488
Set* GetSetForItem(uint32_t id);           // @0x21a6c4
void RemoveItem(uint32_t id);              // @0x21b558
void AddItem(uint32_t id);                 // @0x21bdac
void FindPlayerSetState(const PvP::PlayerEquipmentInfo& info);   // @0x21bed4 another player's pieces
void FindInventorySetState();              // @0x21bf54 every item the player owns
void FindActiveSetState();                 // @0x21bfc4 the worn, unbroken pieces

}  // namespace Sets
