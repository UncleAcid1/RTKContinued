#include "game/SoldierSlots.h"

#include <algorithm>
#include <cstdio>
#include <vector>

#include "game/Entity.h"
#include "game/EntityData.h"
#include "game/EntityManager.h"
#include "game/Squad.h"

namespace SoldierSlots {
namespace {
unsigned g_max = 0;              // 0x615578
unsigned g_free = 0;             // 0x61557c
std::vector<Entity*> g_slots;    // 0x615580
}  // namespace

bool Init() {
    g_max = 3;
    g_slots.clear();
    g_free = 1;
    return true;
}

void Deinit() { g_slots.clear(); }
unsigned GetTotalSlotCount() { return g_max; }
unsigned GetTotalSlotsFree() { return g_free; }
void SetMaxSlotsCount(unsigned n) { g_max = n; }
void SetFreeSlotsCount(unsigned n) { g_free = n; }

bool HasSoldier(Entity* e) { return std::find(g_slots.begin(), g_slots.end(), e) != g_slots.end(); }

Entity* GetFirstDeadSoldier() {
    for (Entity* e : g_slots)
        if (e && e->IsDead()) return e;
    return nullptr;
}

int RemoveSoldier(Entity* e, bool) {
    auto it = std::find(g_slots.begin(), g_slots.end(), e);
    if (it == g_slots.end()) return -1;
    int index = (int)(it - g_slots.begin());
    g_slots.erase(it);
    e->f3b = false;
    Entity* player = EntityManager::GetPlayer();
    if (player && player->GetSquad()) player->GetSquad()->RemoveAllSoldiersByID((int)e->GetEntityData()->id);
    return index;
}

int GetSoldiersCountByID(int id) {
    int n = 0;
    for (Entity* e : g_slots)
        if (e && (int)e->GetEntityData()->id == id) ++n;
    return n;
}

void RemoveAllSodiersByID(int id) {
    for (size_t i = 0; i < g_slots.size(); ++i) {
        if ((int)g_slots[i]->GetEntityData()->id != id) continue;
        EntityManager::GetPlayer()->GetSquad()->RemoveSoldierFromSquad(g_slots[i]);
        g_slots.erase(g_slots.begin() + (long)i);
        // (the next entry slides into i and the loop goes on at i + 1: it is skipped, as on the original)
    }
}

unsigned GetOccupiedSlotCount() { return (unsigned)g_slots.size(); }
Entity* GetFirstSoldier() { return g_slots.empty() ? nullptr : g_slots[0]; }
Entity* GetSoldierBySlot(unsigned i) { return i < g_slots.size() ? g_slots[i] : nullptr; }

Entity* GetFirstSoldierByID(unsigned id) {
    for (Entity* e : g_slots)
        if (e->GetEntityData()->id == id) return e;
    return nullptr;
}

bool GotFreeSlot(unsigned id) {
    if (GetFirstSoldierByID(id)) return false;
    return g_slots.size() < g_free;
}

// The type check below only reports; the soldier is added either way.
static void ReportSameType(Entity* e, const char* fn) {
    for (Entity* s : g_slots)
        if (s && s->GetEntityData() && s->GetEntityData()->id == e->GetEntityData()->id)
            std::fprintf(stderr, "ERROR: SoldierSlots::%s() Soldier slots already have an entity with ID %d\n", fn,
                         (int)e->GetEntityData()->id);
}

int AddSoldier(Entity* e) {
    if (g_free <= g_slots.size())
        std::fprintf(stderr,
                     "ERROR: SoldierSlots::AddSoldier() Soldier pool size %d is larger than total free slot count %d\n",
                     (int)g_slots.size(), (int)g_free);
    ReportSameType(e, "AddSoldier");
    g_slots.push_back(e);
    e->f3b = true;
    return (int)g_slots.size() - 1;
}

unsigned AddSoldierToSlot(Entity* e, unsigned slot) {
    ReportSameType(e, "AddSoldierToSlot");
    g_slots.insert(g_slots.begin() + slot, e);
    return slot;
}

}  // namespace SoldierSlots

namespace SoldierPool {
namespace {
unsigned g_max = 0;              // 0x615564
unsigned g_free = 0;             // 0x615568
std::vector<Entity*> g_pool;     // 0x61556c
}  // namespace

bool Init() {
    g_max = 6;
    g_pool.clear();
    g_free = 3;
    return true;
}

void Deinit() { g_pool.clear(); }
unsigned GetTotalSlotCount() { return g_max; }
unsigned GetTotalSlotsFree() { return g_free; }
void SetMaxSlotsCount(unsigned n) { g_max = n; }
void SetFreeSlotsCount(unsigned n) { g_free = n; }

Entity* GetFirstSoldierExcludingSoldierSlots() {
    for (Entity* e : g_pool) {
        if (!e || e->IsDead()) continue;
        bool inSlots = false;
        for (unsigned i = 0; i < SoldierSlots::GetOccupiedSlotCount(); ++i) {
            Entity* s = SoldierSlots::GetSoldierBySlot(i);
            if (s && s->GetEntityData()->id == e->GetEntityData()->id) {
                inSlots = true;
                break;
            }
        }
        if (!inSlots) return e;
    }
    return nullptr;
}

bool HasSoldier(Entity* e) { return std::find(g_pool.begin(), g_pool.end(), e) != g_pool.end(); }

int RemoveSoldier(Entity* e) {
    auto it = std::find(g_pool.begin(), g_pool.end(), e);
    if (it == g_pool.end()) return -1;
    int index = (int)(it - g_pool.begin());
    g_pool.erase(it);
    return index;
}

int GetSoldiersCountByID(int id) {
    int n = 0;
    for (Entity* e : g_pool)
        if (e && (int)e->GetEntityData()->id == id) ++n;
    return n;
}

void DeactivateDeadSoldiers() {
    for (Entity* e : g_pool)
        if (e && e->IsActive()) e->SetActive(false, false);
}

void RemoveAllSodiersByID(int id) {
    for (size_t i = 0; i < g_pool.size(); ++i)
        if ((int)g_pool[i]->GetEntityData()->id == id) g_pool.erase(g_pool.begin() + (long)i);   // (skips the next, as above)
}

unsigned GetOccupiedSlotCount() { return (unsigned)g_pool.size(); }
bool GotFreeSlot(unsigned) { return g_pool.size() < g_free; }
Entity* GetSoldierBySlot(unsigned i) { return i < g_pool.size() ? g_pool[i] : nullptr; }

Entity* GetFirstSoldier(int id) {
    if (g_pool.empty()) return nullptr;
    if (id == -1) return g_pool[0];
    for (Entity* e : g_pool)
        if ((int)e->GetEntityData()->id != id) return e;
    return nullptr;
}

Entity* GetFirstSoldierByID(unsigned id) {
    for (Entity* e : g_pool)
        if (!e->IsDead() && e->GetEntityData()->id == id) return e;
    return nullptr;
}

int AddSoldier(Entity* e) {
    g_pool.push_back(e);
    return (int)g_pool.size() - 1;
}

}  // namespace SoldierPool
