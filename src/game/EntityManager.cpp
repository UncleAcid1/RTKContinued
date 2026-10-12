#include "game/EntityManager.h"
#include "game/SpawnPoint.h"

#include <algorithm>
#include <cstdio>
#include <vector>

#include "engine/Render.h"
#include "engine/Timer.h"
#include "game/AIState.h"
#include "game/BuildingHovers.h"
#include "game/Entity.h"
#include "game/EntityData.h"
#include "game/GameState.h"
#include "game/Map.h"
#include "game/MetaData.h"
#include "game/MetaExpression.h"
#include "game/SoldierSlots.h"
#include "game/Squad.h"
#include "game/Tasks.h"

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
    BuildingHovers::SafeRegisterEntity(e);
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

Entity* SpawnEntityAt(SpawnPoint* sp, bool appear, bool glow) {
    Entity* e = CreateEntity((int)sp->entityId, false, true);
    e->SetSpawnPoint(sp);
    e->SetPos(sp->x, sp->y);
    e->SetCurrentMap((int)GameState::GetCurrentMapID());
    BuildingHovers::SafeRegisterEntity(e);
    e->SetActive(true, true);
    e->UpdateGraphics();
    if (e->data->clas == 5) g_player = e;
    else if (appear) e->Appear(glow, true);
    return e;
}

void UpdateAggroZone() {
    for (Entity* e : g_entities)
        if (e->GetAI()) e->GetAI()->UpdateAggroZone();
}

Entity* GetEntityBySpawnPoint(uint32_t id) {
    for (Entity* e : g_entities)
        if (e->GetSpawnPointID() == id) return e;
    return nullptr;
}

Entity* GetEntityWithGUID(uint32_t guid) {
    for (Entity* e : g_entities)
        if (e->GetGUID() == guid) return e;
    return nullptr;
}

Entity* GetEntityWithTalkTask() {
    for (Entity* e : g_entities) {
        uint32_t task = e->GetTalkTask();
        if (task == 0) continue;
        Tasks::TaskInfo* t = Tasks::GetTask(task);
        if (t && !t->repeatTask) return e;
    }
    return nullptr;
}

Entity* GetEntityWithDrop(uint32_t item) {
    Entity* player = GetPlayer();
    Entity* best = nullptr;
    // The nearer of the best so far and e (squared world distance to the player; a tie keeps the
    // earlier one).
    auto nearer = [&](Entity* e) {
        if (!best) return e;
        float bx = best->worldX - player->worldX, by = best->worldY - player->worldY;
        float ex = e->worldX - player->worldX, ey = e->worldY - player->worldY;
        return by * by + bx * bx <= ey * ey + ex * ex ? best : e;
    };
    for (Entity* e : g_entities) {
        if (e->IsActive() && !e->IsDead()) {
            if (const MetaData* drops = e->data->itemsDrop) {
                for (unsigned i = 0; i < drops->GetChildrenCount(); ++i) {
                    if ((uint32_t)drops->GetChild(i)->GetChild(0)->GetInt() != item) continue;
                    if (drops->GetChild(i)->GetChild(1)->GetChild(0)->GetInt() < 1) continue;
                    best = nearer(e);
                }
            }
            if (MetaExpression* meta = e->GetMetaExpression()) {
                bool found = false;
                if (const MetaData* d = meta->FindChildData(kExpDropAfterQuest)) {
                    for (unsigned i = 0; i < d->GetChildrenCount(); ++i) {
                        const MetaData* c = d->GetChild(i);
                        if ((uint32_t)c->GetChild(1)->GetInt() != item) continue;
                        if (!GameState::TaskCompleted((unsigned)c->GetChild(0)->GetInt())) continue;
                        found = true;
                        best = nearer(e);
                    }
                }
                if (!found) {
                    if (const MetaData* d = meta->FindChildData(kExpDropBeforeQuest)) {
                        for (unsigned i = 0; i < d->GetChildrenCount(); ++i) {
                            const MetaData* c = d->GetChild(i);
                            if ((uint32_t)c->GetChild(1)->GetInt() != item) continue;
                            if (GameState::TaskCompleted((unsigned)c->GetChild(0)->GetInt())) continue;
                            found = true;
                            best = nearer(e);
                        }
                    }
                    if (!found) {
                        if (const MetaData* d = meta->FindChildData(kExpDrop)) {
                            for (unsigned i = 0; i < d->GetChildrenCount(); ++i) {
                                const MetaData* c = d->GetChild(i);
                                if (c->GetChild(0)->type != MetaData::kNumber) continue;
                                if ((uint32_t)c->GetChild(0)->GetInt() != item) continue;
                                best = nearer(e);
                            }
                        }
                    }
                }
            }
        }
        if (e->IsActive() && e->IsDead() && e->HasCorpseItem() && (uint32_t)e->data->corpseDropId == item) return e;
    }
    return best;
}

void RemoveEntitiesByID(uint32_t id, bool all) {
    for (size_t i = 0; i < g_entities.size();) {
        Entity* e = g_entities[i];
        if (e->data->id == id && (all || e->temporary)) {
            g_entities.erase(g_entities.begin() + (long)i);
            DestroyEntity(e);
        } else {
            ++i;
        }
    }
}

Entity* SpawnEntityAt(int id, unsigned x, unsigned y, bool appear, bool glow) {
    Entity* e = CreateEntity(id, false, true);
    e->SetPos((int)x, (int)y);
    e->SetCurrentMap((int)GameState::GetCurrentMapID());
    BuildingHovers::SafeRegisterEntity(e);
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

void ResetOrders(Map::Building* b) {
    for (Entity* e : g_entities)
        if (e->GetAI()) e->GetAI()->ResetOrder(b);
}

void RemoveEntity(Entity* e, bool any) {
    auto it = std::find_if(g_entities.begin(), g_entities.end(),
                           [&](Entity* x) { return x == e && (any || e->temporary); });
    if (it == g_entities.end()) return;
    g_entities.erase(it);
    DestroyEntity(e);
}

void DestroyEntity(Entity* e) {
    if (e->GetEntityData()->clas == 10) {
        SoldierSlots::RemoveSoldier(e, false);
        if (GetPlayer() && GetPlayer()->GetSquad()) GetPlayer()->GetSquad()->RemoveSoldierFromSquad(e);
    }
    BuildingHovers::UnregisterEntity(e);
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

// The list is read again each time round, as the original does: a reaction may add entities.
// @0x16664c EntityManager::OnTagUpdate
void OnTagUpdate(Tags::Tag* tag) {
    for (size_t i = 0; i < g_entities.size(); ++i) g_entities[i]->OnTagUpdate(tag);
}
// @0x166698 EntityManager::OnTaskStarted
void OnTaskStarted(uint32_t task) {
    for (size_t i = 0; i < g_entities.size(); ++i) g_entities[i]->OnTaskStarted(task);
}
// @0x1666e4 EntityManager::OnInteract
void OnInteract(uint32_t object) {
    for (size_t i = 0; i < g_entities.size(); ++i) g_entities[i]->OnInteract(object);
}
// @0x166730 EntityManager::OnSubTaskCompleted
void OnSubTaskCompleted(uint32_t subtask) {
    for (size_t i = 0; i < g_entities.size(); ++i) g_entities[i]->OnSubTaskCompleted(subtask);
}
// @0x16677c EntityManager::OnTaskCompleted
void OnTaskCompleted(uint32_t task) {
    for (size_t i = 0; i < g_entities.size(); ++i) g_entities[i]->OnTaskCompleted(task);
}
// @0x166820 EntityManager::ExecuteStaticMeta
void ExecuteStaticMeta() {
    Entity::PrepareForSetup();
    for (size_t i = 0; i < g_entities.size(); ++i) g_entities[i]->OnSetup();
}

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

Entity* GetEntityByEnemyClassID(uint32_t enemyClass) {
    for (Entity* e : g_entities)
        if (e->IsActive() && !e->IsDead() && (uint32_t)e->data->enemyClass == enemyClass &&
            (!e->GetSpawnPoint() || !e->GetSpawnPoint()->npc))
            return e;
    return nullptr;
}

Entity* GetNonPlayerEntityAtXY(int x, int y) {
    for (Entity* e : g_entities)
        if (!e->player && e->IsActive() && !e->IsDead() && e->tileX == x && e->tileY == y) return e;
    return nullptr;
}

void OnClick(int x, int y) {
    if (!g_player || !g_player->GetAI()) return;
    if (Entity* e = GetEntityAtWorldXY(x, y)) {
        // UNVERIFIED (milestones 4b/4g): a dead entity with a corpse item (Entity::HasCorpseItem) can
        // be tapped too; (4c) its meta expression is printed.
        if (e->IsActive() && !e->IsDead()) {
            g_player->GetAI()->ClickedEntity(e);
            return;
        }
        std::printf("EntityManager::OnClick() Ignoring click on inactive entity (active = %d, dead = %d) (spawn ID = %d)\n",
                    e->IsActive(), e->IsDead(), 0);   // (spawn points: milestone 4f)
    }
    int tx = x, ty = y;
    Map::WorldCoordinatesToTile(tx, ty);
    if (tx > -1 && tx <= Map::GetGridWidth() && ty > -1 && ty <= Map::GetGridHeight())
        g_player->GetAI()->ClickedTile(tx, ty, x, y);
}

Entity* GetEntityAtWorldXY(int x, int y) {
    // UNVERIFIED (milestone 4): the first pass also skips NPCs (spawn point +0x10); spawn points
    // are not ported, so it is the same as the second.
    Entity* best = nullptr;
    float z = 100000.f;
    for (int pass = 0; pass < 3 && !best; ++pass) {
        for (Entity* e : g_entities) {
            if (!e->IsActive() || (pass < 2 && e->IsDead()) || !e->Contains(x, y)) continue;
            Render::Sprite* s = e->GetSprite();
            if (!s || z <= s->z) continue;
            best = e;
            z = s->z;
        }
    }
    return best;
}

Entity* GetFreeWorker() {
    for (Entity* e : g_entities) {
        // UNVERIFIED (milestone 4): IsNPC (spawn points) is always false.
        if (e->GetEntityData()->clas == 0 && !e->GetWorkplace() && !e->GetWorkplaceDecoration() && e->IsActive())
            return e;
    }
    return nullptr;
}

Entity* GetFirstBusyWorker() {
    for (Entity* e : g_entities)
        if (e->GetEntityData()->clas == 0 && e->GetWorkplace() && !e->GetWorkplaceDecoration()) return e;
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

namespace EntityManager {

void AssignSoldiersToPlayerSquad() {
    for (Entity* e : g_entities) {
        if (e->GetEntityData()->clas != 10) continue;
        if (!e->f3b) {
            e->SetActive(false, false);
            SoldierPool::AddSoldier(e);
        } else {
            GetPlayer()->GetSquad()->AddSoldier(e);
            SoldierSlots::AddSoldier(e);
        }
    }
}

void GetPlayerAndSoldiers(std::vector<Entity*>& out) {
    for (Entity* e : g_entities)
        if (e->GetEntityData()->clas == 10 || e->GetEntityData()->clas == 5) out.push_back(e);
}

}  // namespace EntityManager
