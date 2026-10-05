#include "game/EntityManager.h"

#include <algorithm>
#include <cstdio>
#include <vector>

#include "engine/Timer.h"
#include "game/AIState.h"
#include "game/Entity.h"
#include "game/EntityData.h"
#include "game/GameState.h"

namespace EntityManager {
namespace {
std::vector<Entity*> g_entities;   // +0x00
Entity* g_player = nullptr;        // +0x1c
}  // namespace

Entity* CreateEntity(int id, bool noPlayer, bool temporary) {
    EntityData* d = EntityFactory::GetEntityByID(id);
    if (!d) {
        std::fprintf(stderr, "ERROR: EntityManager::CreateEntity() Entity %d doesn't exist\n", id);
        d = EntityFactory::GetEntityByID(0x135);
    }
    Entity* e = new Entity(d, 0, 0);
    e->temporary = temporary;
    if (d->clas == 0 || d->clas == 2) {
        int farmer = d->clas == 2 ? 1 : 0;
        e->firstNameIdx = EntityFactory::GetRandomNameIdx(d->female, farmer);
        e->surnameIdx = EntityFactory::GetRandomSurnameIdx(d->female, farmer);
        e->firstName = EntityFactory::GetNameByIdx(d->female, farmer, e->firstNameIdx);
        e->surname = EntityFactory::GetSurnameByIdx(d->female, farmer, e->surnameIdx);
    }
    if (d->clas == 5 && !noPlayer) g_player = e;
    g_entities.push_back(e);
    return e;
}

Entity* SpawnEntityAt(Entity* e, unsigned x, unsigned y, bool appear, bool glow) {
    e->SetPos((int)x, (int)y);
    e->SetCurrentMap((int)GameState::GetCurrentMapID());
    // UNVERIFIED (milestone 3e): BuildingHovers::SafeRegisterEntity.
    e->SetActive(true, true);
    e->UpdateGraphics();
    if (e->IsDead() && !e->player) e->SetActive(false, false);
    if (e->data->clas == 5) {
        if (appear) e->Appear(glow, true);
        g_player = e;
    } else if (appear) {
        e->Appear(glow, true);
    }
    return e;
}

Entity* SpawnEntityAt(int id, unsigned x, unsigned y, bool appear, bool glow) {
    Entity* e = CreateEntity(id, false, true);
    e->SetPos((int)x, (int)y);
    e->SetCurrentMap((int)GameState::GetCurrentMapID());
    e->SetActive(true, true);
    e->UpdateGraphics();
    if (e->data->clas == 5) g_player = e;
    else if (appear) e->Appear(glow, true);
    return e;
}

Entity* SpawnEntityAt(const std::string& name, unsigned x, unsigned y, bool appear, bool glow) {
    int id = EntityFactory::GetEntityIDByName(name);
    if (id == -1) return nullptr;
    return SpawnEntityAt(id, x, y, appear, glow);
}

void AddEntity(Entity* e) { g_entities.push_back(e); }

void RemoveEntity(Entity* e, bool any) {
    auto it = std::find_if(g_entities.begin(), g_entities.end(),
                           [&](Entity* x) { return x == e && (any || e->temporary); });
    if (it == g_entities.end()) return;
    g_entities.erase(it);
    DestroyEntity(e);
}

void DestroyEntity(Entity* e) {
    // UNVERIFIED (milestone 4): class 10 soldiers leave SoldierSlots and the player's squad.
    // UNVERIFIED (milestone 3e): BuildingHovers::UnregisterEntity.
    delete e;
}

void Clean(bool keepPlayer) {
    if (!keepPlayer) {
        for (size_t i = 0; i < g_entities.size();) {
            Entity* e = g_entities[i];
            if (!e->player && e->temporary) {
                g_entities.erase(g_entities.begin() + (long)i);
                DestroyEntity(e);
            } else {
                ++i;
            }
        }
        return;
    }
    for (size_t i = 0; i < g_entities.size();) {
        Entity* e = g_entities[i];
        if (!e->player) {
            g_entities.erase(g_entities.begin() + (long)i);
            DestroyEntity(e);
        } else {
            ++i;
        }
    }
    if (g_player) {
        DestroyEntity(g_player);
        auto it = std::find(g_entities.begin(), g_entities.end(), g_player);
        if (it != g_entities.end()) g_entities.erase(it);
        g_player = nullptr;
    }
}

void Update(float dt) {
    // UNVERIFIED (milestone 4): the active combat's update (+0x88) runs first.
    for (size_t i = 0; i < g_entities.size(); ++i)
        if (g_entities[i]->IsActive()) g_entities[i]->Update(dt);
    if (!GameState::IsPaused()) {
        for (size_t i = 0; i < g_entities.size(); ++i) {
            Entity* e = g_entities[i];
            if (e->IsActive() && e->GetAI()) e->GetAI()->CheckAggro();
        }
    }
    // UNVERIFIED (milestone 4): Entity::UpdateSafePos on the player.
    for (size_t i = 0; i < g_entities.size();) {
        Entity* e = g_entities[i];
        if (e->NeedRemove()) {
            g_entities.erase(g_entities.begin() + (long)i);
            DestroyEntity(e);
        } else {
            ++i;
        }
    }
}

void UpdateGraphics() {
    for (Entity* e : g_entities)
        if (e->IsActive()) e->UpdateGraphics();
}

Entity* EnumEntities(unsigned i) { return i < g_entities.size() ? g_entities[i] : nullptr; }

int GetEntityCount(bool permanentOnly) {
    if (!permanentOnly) return (int)g_entities.size();
    int n = 0;
    for (Entity* e : g_entities)
        if (!e->temporary) ++n;
    return n;
}

Entity* GetPlayer() { return g_player; }

Entity* GetEntityAtXY(int x, int y) {
    for (Entity* e : g_entities)
        if (e->IsActive() && !e->IsDead() && e->tileX == x && e->tileY == y) return e;
    return nullptr;
}

int GetEntityCountAtXY(int x, int y) {
    int n = 0;
    for (Entity* e : g_entities)
        if (e->IsActive() && !e->IsDead() && e->tileX == x && e->tileY == y) ++n;
    return n;
}

Entity* GetEntityWithTargetPositionAtXY(int x, int y) {
    for (Entity* e : g_entities) {
        if (e->player || !e->IsActive() || !e->GetAI() || e->IsDead()) continue;
        AI::Waypoint* t = e->GetAI()->GetLastTarget();
        if (t && t->x == x && t->y == y) return e;
        // UNVERIFIED (milestone 4): Entity::PartolEndsOn (patrols are not ported).
    }
    return nullptr;
}

unsigned GetEntityCountByID(unsigned id) {
    unsigned n = 0;
    for (Entity* e : g_entities)
        if (e->data->id == id) ++n;
    return n;
}

}  // namespace EntityManager
