// SoldierSlots: the soldiers in the player's squad (the hired army that walks with the hero), and
// SoldierPool: the reserve, soldiers kept out of the squad. Both are global lists of entities with a
// slot limit. Port of SoldierSlots (@0x21e1d0..0x21ec28) and SoldierPool (@0x21da40..0x21e1cc).
#pragma once

class Entity;

namespace SoldierSlots {

bool Init();                                     // @0x21e258 max 3, free 1
void Deinit();                                   // @0x21e5e0 empties the list
unsigned GetTotalSlotCount();                    // @0x21e1d0 (0x615578)
unsigned GetTotalSlotsFree();                    // @0x21e1e0 (0x61557c) the usable slots
void SetMaxSlotsCount(unsigned n);               // @0x21e1f4
void SetFreeSlotsCount(unsigned n);              // @0x21e208
bool HasSoldier(Entity* e);                      // @0x21e28c
Entity* GetFirstDeadSoldier();                   // @0x21e2e8
// @0x21e370: -1 if e is not in a slot. Clears e's +0x3b and makes the player's squad drop every
// soldier of e's type (RemoveAllSoldiersByID). The flag is unused.
int RemoveSoldier(Entity* e, bool flag);
int GetSoldiersCountByID(int id);                // @0x21e480
void RemoveAllSodiersByID(int id);               // @0x21e4ec (the original's spelling)
unsigned GetOccupiedSlotCount();                 // @0x21e600
Entity* GetFirstSoldier();                       // @0x21e620
Entity* GetSoldierBySlot(unsigned i);            // @0x21e644
Entity* GetFirstSoldierByID(unsigned id);        // @0x21e66c
bool GotFreeSlot(unsigned id);                   // @0x21e6f8 no soldier of that type and a free slot
int AddSoldier(Entity* e);                       // @0x21e894 its slot; sets e's +0x3b
unsigned AddSoldierToSlot(Entity* e, unsigned slot);   // @0x21eb24 inserted before `slot`

}  // namespace SoldierSlots

namespace SoldierPool {

bool Init();                                     // @0x21db08 max 6, free 3
void Deinit();                                   // @0x21deb0
unsigned GetTotalSlotCount();                    // @0x21da40 (0x615564)
unsigned GetTotalSlotsFree();                    // @0x21da50 (0x615568)
void SetMaxSlotsCount(unsigned n);               // @0x21da64
void SetFreeSlotsCount(unsigned n);              // @0x21da78
// @0x21db3c: the first living soldier whose type has no soldier in SoldierSlots.
Entity* GetFirstSoldierExcludingSoldierSlots();
bool HasSoldier(Entity* e);                      // @0x21dbec
int RemoveSoldier(Entity* e);                    // @0x21dc48 its index, -1 if none
int GetSoldiersCountByID(int id);                // @0x21dcf4
void DeactivateDeadSoldiers();                   // @0x21dd60 (deactivates every active one)
void RemoveAllSodiersByID(int id);               // @0x21dde0
unsigned GetOccupiedSlotCount();                 // @0x21ded0
bool GotFreeSlot(unsigned id);                   // @0x21def0 (the id is unused)
Entity* GetSoldierBySlot(unsigned i);            // @0x21df1c
// @0x21df44: the first soldier, or with id != -1 the first one of another type.
Entity* GetFirstSoldier(int id);
Entity* GetFirstSoldierByID(unsigned id);        // @0x21dfdc a living one
int AddSoldier(Entity* e);                       // @0x21e07c its index

}  // namespace SoldierPool
