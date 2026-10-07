#include "game/Squad.h"

#include <cstdio>
#include <cstdlib>

#include "engine/Render.h"
#include "game/AI.h"
#include "game/AIState.h"
#include "game/Combat.h"
#include "game/Entity.h"
#include "game/EntityData.h"
#include "game/EntityManager.h"
#include "game/GameState.h"
#include "game/Map.h"
#include "game/Rand48.h"
#include "game/SoldierSlots.h"

namespace {

// The formations (0x579d8c), 3x3 cells row by row; GetFormation picks one by direction.
const char kFormations[0x5d] = {
    0, 1, 0, 1, 3, 1, 0, 2, 0,   // +0x00 combat, directions 3 and 4
    0, 1, 0, 2, 3, 1, 0, 1, 0,   // +0x09 combat, 1 and 2
    0, 2, 0, 1, 3, 1, 0, 1, 0,   // +0x12 combat, 0 and 7
    0, 1, 0, 1, 3, 2, 0, 1, 0,   // +0x1b combat, 5 and 6
    0, 0, 0, 1, 2, 1, 0, 1, 0,   // +0x24 3 and 4
    0, 1, 0, 1, 2, 0, 0, 1, 0,   // +0x2d 1 and 2
    0, 1, 0, 1, 2, 1, 0, 0, 0,   // +0x36 0 and 7
    0, 1, 0, 0, 2, 1, 0, 1, 0,   // +0x3f 5 and 6
    0, 0, 0, 0, 2, 0, 1, 0, 1,   // +0x48 0 and 7 in the tutorial step 0x89
    0, 0, 0,                     // (+0x51 unused)
    0, 1, 0, 1, 2, 1, 0, 0, 0,   // +0x54 0 and 7, an enemy squad
};

// The formation's top corner from the owner's cell: `col` steps up-left, then `row` steps up-right
// (the loops the original inlines wherever it lays a formation out).
void StepToCorner(int& x, int& y, int col, int row) {
    if (col != 0) {
        int end = y - col;
        do {
            if (std::abs(y) & 1) ++x;
            --y;
            --x;
        } while (y != end);
    }
    if (row != 0) {
        int end = y - row;
        do {
            if (y % 2 == 1) ++x;
            --y;
        } while (y != end);
    }
}

// The active combat's type (vtable +0x50). PORT: no combat exists until milestone 4g; some callers
// read it without checking IsCombatActive on the original.
int CombatType() {
    BaseCombat* c = GameState::GetActiveCombat();
    return c ? c->GetType() : 0;
}

}  // namespace

// ------------------------------------------------------------------------------------- BaseSquad

BaseSquad::BaseSquad(Entity* o) : owner(o) {
    BaseSquad::AddSoldier(o);
    BaseSquad::ChangeState(0);
}

BaseSquad::~BaseSquad() {
    while (!soldiers.empty()) {
        soldiers.front()->OnRemovedFromSquad();
        soldiers.erase(soldiers.begin());
    }
    target = nullptr;
    attackWP = nullptr;
    combat = nullptr;
    ready.clear();
    BaseSquad::ChangeState(0);
}

int BaseSquad::GetAliveSoldierCount() {
    int n = 0;
    for (Entity* e : soldiers)
        if (!e->IsDead() && !e->IsDying()) ++n;
    return n;
}

void BaseSquad::RemoveSoldier(Entity* e) {
    for (size_t i = 0; i < soldiers.size(); ++i) {
        if (soldiers[i] != e) continue;
        soldiers.erase(soldiers.begin() + (long)i);
        ready.erase(ready.begin() + (long)i);
        e->OnRemovedFromSquad();
        return;
    }
}

void BaseSquad::RemoveAllSoldiers() {
    while (!soldiers.empty()) {
        Entity* e = soldiers.front();
        soldiers.erase(soldiers.begin());
        e->OnRemovedFromSquad();
        e->SetActive(false, false);
        e->UpdateRing();
        EntityManager::RemoveEntity(e, true);
    }
    ready.clear();
}

void BaseSquad::AddSoldier(Entity* e) {
    soldiers.push_back(e);
    if (owner != e) e->OnAddedToSquad(this);
    ready.push_back(false);
}

void BaseSquad::SetReady(Entity* e) {
    for (size_t i = 0; i < soldiers.size(); ++i) {
        if (soldiers[i] != e) continue;
        if (attackWP == nullptr || type != 0) {
            if (GameState::GetActiveCombat() && CombatType() != 5 && CombatType() != 6)
                e->SetDirection(owner->GetDirection());
        } else {
            e->GetAI()->TurnTo(attackWP);
        }
        ready[i] = true;
        e->SetReportReadiness(false);
        if (!GameState::IsCombatActive() || GameState::GetActiveCombat()->GetState() != 0x10) {
            if (!e->f36) AddToCombat(e);
        }
    }
}

void BaseSquad::MoveTo(AI::Waypoint* wp) {
    if (!wp) return;
    if (!ready.empty()) ready[0] = false;
    if (attackWP) owner->SetWorkplaceDecoration(nullptr);
    if (owner->f75) {
        owner->SetReportReadiness(true);
        owner->GetAI()->WalkTo(wp);
    }
    MoveSquadTo(wp);
}

void BaseSquad::MoveSquadTo(AI::Waypoint* wp) {
    if (soldiers.size() < 2) return;
    int dir = pendingDir;
    if (dir < 0) {
        if (attackWP == nullptr) dir = owner->GetAI()->GetFutureDirection();
        else dir = GetDir(AI::GetWaypoint(attackWP->x, attackWP->y, false), wp);
        if (dir < 0) dir = owner->GetDirection();
    } else {
        pendingDir = -1;
    }
    Entity* tgt = target;
    const char* f = GetFormation(dir, tgt != nullptr);
    int col = 0, row = 0;
    FindOwnerPos(f, col, row);
    int x = wp->x, y = wp->y;
    StepToCorner(x, y, col, row);

    // The formation's cells sorted into the front (1 in a combat formation), the cells kept free (4,
    // unused afterwards, as on the original) and the rest.
    std::vector<AI::Waypoint*> front, kept, rest;
    for (int r = 0; r < 3; ++r) {
        int cx = x, cy = y;
        for (int c = 0; c < 3; ++c) {
            char cell = f[r * 3 + c];
            AI::Waypoint* p = AI::GetWaypoint(cx, cy, false);
            Entity* there = EntityManager::GetEntityAtXY(cx, cy);
            if (p) {
                if (!tgt) {
                    if (type == 1) {
                        if (!there && cell == 1 && p != wp) rest.push_back(p);
                    } else if (!there && p != wp &&
                               ((GameState::GetCurrentMapID() == 0 && p->weight <= 1.f) ||
                                GameState::GetCurrentMapID() != 0)) {
                        rest.push_back(p);
                    }
                } else {
                    // A cell holding the target at the attack waypoint, or any monster (class 9), is
                    // not a place to stand.
                    auto blocked = [&]() {
                        if (!there || !target || !attackWP) return false;
                        if (there == target && AI::GetWaypoint(there->tileX, there->tileY, false) == attackWP) p = nullptr;
                        return there->GetEntityData()->clas == 9;
                    };
                    if (cell == 1) {
                        if (!there || !target || !attackWP) {
                            front.push_back(p);
                        } else if (!blocked() && p) {
                            front.push_back(p);
                        }
                    } else if (cell == 4) {
                        if (!blocked() && p) kept.push_back(p);
                    } else if (cell == 0) {
                        if (!blocked() && p) rest.push_back(p);
                    }
                }
            }
            if (cy % 2 == 1) ++cx;
            ++cy;
        }
        if (std::abs(y) & 1) ++x;
        --x;
        ++y;
    }
    for (size_t i = 1; i < ready.size(); ++i)
        if (!soldiers[i]->f36) ready[i] = false;

    size_t next = 1;   // the first soldier not yet sent
    if (!front.empty()) {
        for (size_t k = 0;; ++k) {
            size_t i = k + 1;
            Entity* s = soldiers[i];
            if (!s->IsDead() && s->f75) {
                // (the original compares the soldier's tileX with both of wp's coordinates)
                bool there = s->tileX == wp->x && s->tileX == wp->y;
                if (type == 1 && CombatType() > 2) {
                    s->SetReportReadiness(true);
                    if (there) SetReady(s);
                    else if (i == soldiers.size()) return;
                    else s->GetAI()->WalkTo(wp->x, wp->y);
                } else {
                    s->SetReportReadiness(true);
                    if (there) SetReady(s);
                    else if (i == soldiers.size()) return;
                    else s->GetAI()->WalkTo(front[k]->x, front[k]->y);
                }
            }
            if (i + 1 == soldiers.size()) return;
            if (k + 1 == front.size()) {
                next = i + 1;
                break;
            }
        }
        // The soldiers left over walk to random places near wp; the original sends the same soldier
        // (the first left over) each time.
        for (size_t j = next; j < soldiers.size(); ++j)
            soldiers[next]->GetAI()->WalkTo(AI::GetRandomWaypointInRange(wp->x, wp->y, 3, 1, false, nullptr, false));
    }
    if (next < soldiers.size() && !tgt && !rest.empty()) {
        for (size_t j = 0; j < rest.size(); ++j) {
            Entity* s = soldiers[next + j];
            if (!s->f36 && !s->IsDead() && s->f75) s->GetAI()->WalkTo(rest[j]->x, rest[j]->y);
            if (next + j + 1 == soldiers.size()) return;
        }
    }
}

void BaseSquad::SpawnAt(AI::Waypoint* wp) {
    if (!wp) {
        std::puts("BaseSquad::SpawnAt() cannot spawn - no waypoint");
        return;
    }
    if (GameState::GetCurrentMapID() == 0xb)
        EntityManager::SpawnEntityAt(owner, (unsigned)wp->x, (unsigned)wp->y, true, true);
    else
        EntityManager::SpawnEntityAt(owner, (unsigned)wp->x, (unsigned)wp->y, false, false);
    SpawnSquadAt(wp);
}

void BaseSquad::SpawnSquadAt(AI::Waypoint* wp) {
    size_t i = 1;
    auto place = [&](int x, int y) {
        Entity* e = EntityManager::SpawnEntityAt(soldiers[i++], (unsigned)x, (unsigned)y, true, false);
        e->GetAI()->ClearPath();
        e->SetAnimationP("idle_1", true, false, false);
        e->SetDirection(owner->GetDirection());
    };
    if (soldiers.size() > 1) {
        if (Map::GetMapID() == 0) {
            // The city: each soldier on a random free tile near the owner, facing down-right.
            for (size_t k = 1; k < soldiers.size(); ++k) {
                AI::Waypoint* p = AI::GetRandomWaypointInRange(owner->tileX, owner->tileY, 2, 1, false, nullptr, false);
                int x = p ? p->x : owner->tileX, y = p ? p->y : owner->tileY;
                EntityManager::SpawnEntityAt(soldiers[k], (unsigned)x, (unsigned)y, false, false)->SetDirection(1);
            }
            return;
        }
        const char* f = GetFormation(owner->GetDirection(), false);
        // First the formation's soldier cells, then its empty ones (where the original only moves on
        // to the next cell of a row while the tile has a waypoint).
        for (int pass = 0; pass < 2 && i != soldiers.size(); ++pass) {
            int col = 0, row = 0;
            FindOwnerPos(f, col, row);
            int x = owner->tileX, y = owner->tileY;
            StepToCorner(x, y, col, row);
            for (int r = 0; r < 3; ++r) {
                int cx = x, cy = y;
                for (int c = 0; c < 3; ++c) {
                    AI::Waypoint* p = AI::GetWaypoint(cx, cy, false);
                    if (pass == 0) {
                        if (f[r * 3 + c] == 1 && p) {
                            if (soldiers.size() <= i) break;
                            place(cx, cy);
                        }
                        if (cy % 2 == 1) ++cx;
                        ++cy;
                    } else if (p) {
                        if (f[r * 3 + c] == 0) {
                            if (soldiers.size() <= i) break;
                            place(cx, cy);
                        }
                        if (cy % 2 == 1) ++cx;
                        ++cy;
                    }
                }
                if (std::abs(y) & 1) ++x;
                if (r == 2) break;
                --x;
                ++y;
            }
        }
    }
    // The rest: up to 10 tries at random free tiles near wp.
    for (int tries = 10; i < soldiers.size() && tries != 0; --tries) {
        AI::Waypoint* p = AI::GetRandomWaypointInRange(wp->x, wp->y, 2, 1, false, nullptr, false);
        if (p) place(p->x, p->y);
    }
}

bool BaseSquad::IsInPosition(AI::Waypoint*, bool all) {
    size_t n = 0;
    for (bool b : ready) n += b;
    return all ? n == ready.size() : n != 0;
}

Entity* BaseSquad::GetRandomAliveSolider() {
    if (soldiers.empty()) return nullptr;
    std::vector<Entity*> alive;
    for (Entity* e : soldiers)
        if (!e->IsDead()) alive.push_back(e);
    if (alive.empty()) return nullptr;
    return alive[(size_t)((unsigned long)Rand48::lrand48() % alive.size())];
}

bool BaseSquad::HasEntity(Entity* e) {
    if (target == e || owner == e) return true;
    for (Entity* s : soldiers) {
        if (s == e) return true;
        if (s && s->HasEntity(e)) return true;
    }
    return false;
}

bool BaseSquad::IsAnyoneInCombat() {
    for (Entity* e : soldiers)
        if (e->f36) return true;
    return false;
}

void BaseSquad::RemoveAllSoldiersByID(int id) {
    for (size_t i = 0; i < soldiers.size();) {
        Entity* e = soldiers[i];
        if ((int)e->GetEntityData()->id != id) {
            ++i;
            continue;
        }
        soldiers.erase(soldiers.begin() + (long)i);
        ready.erase(ready.begin() + (long)i);
        e->OnRemovedFromSquad();
        e->SetActive(false, false);
        e->UpdateRing();
    }
}

void BaseSquad::DisableSpeedRun() {
    if (type != 0) return;
    for (Entity* e : soldiers)
        if (!e->IsDead()) e->DisableSpeedRun();
}

void BaseSquad::EnableSpeedRun() {
    if (type != 0) return;
    for (Entity* e : soldiers)
        if (!e->IsDead()) e->EnableSpeedRun();
}

void BaseSquad::ClearDeadSoldiers() {
    // (the ready flags are left as they were)
    for (size_t i = 0; i < soldiers.size();) {
        Entity* e = soldiers[i];
        if (!e->IsDead()) {
            ++i;
            continue;
        }
        soldiers.erase(soldiers.begin() + (long)i);
        e->OnRemovedFromSquad();
        e->SetActive(false, false);
        e->UpdateRing();
        EntityManager::RemoveEntity(e, true);
    }
}

void BaseSquad::ResetReady() {
    for (size_t i = 0; i < ready.size(); ++i) ready[i] = false;
}

int BaseSquad::GetDir(AI::Waypoint* a, AI::Waypoint* b) {
    int r = (b == a->n[1] || b == a->n[6]) ? 5 : -1;
    if (b == a->n[0] || b == a->n[7] || b == a->n[4]) r = 1;
    if (b == a->n[2] || b == a->n[5]) return b == a->n[3] ? 7 : 3;
    if (b == a->n[3]) r = 7;
    return r;
}

void BaseSquad::FindOwnerPos(const char* f, int& col, int& row) {
    for (int r = 0; r < 3; ++r, f += 3) {
        for (int c = 0; c < 3; ++c) {
            if (f[c] == 2) {
                col = c;
                row = r;
                return;
            }
        }
    }
}

const char* BaseSquad::GetFormation(int dir, bool fighting) {
    if (fighting) {
        switch (dir) {
        case 0: case 7: return kFormations + 0x12;
        case 1: case 2: return kFormations + 0x09;
        case 3: case 4: return kFormations + 0x00;
        case 5: case 6: return kFormations + 0x1b;
        default: return nullptr;
        }
    }
    switch (dir) {
    case 0: case 7:
        if (GameState::secondTutorial == 0x89) return kFormations + 0x48;
        return type == 1 ? kFormations + 0x54 : kFormations + 0x36;
    case 1: case 2: return kFormations + 0x2d;
    case 3: case 4: return kFormations + 0x24;
    case 5: case 6: return kFormations + 0x3f;
    default: return nullptr;
    }
}

// ----------------------------------------------------------------------------------- PlayerSquad

void PlayerSquad::HealSquad() {
    for (Entity* e : soldiers) {
        if (e->IsDead()) e->SetAnimationP("idle_1", true, false, false);
        e->SetHP(e->GetHpMax());
        Render::ChangeLayer(e->GetSprite(), 8);
    }
    for (unsigned i = 0; i < SoldierSlots::GetOccupiedSlotCount(); ++i) {
        Entity* e = SoldierSlots::GetSoldierBySlot(i);
        if (e->IsDead()) {
            e->SetAnimationP("idle_1", true, false, false);
            Render::ChangeLayer(e->GetSprite(), 8);
            AI::Waypoint* p = AI::GetRandomWaypointInRange(owner->tileX, owner->tileY, 1, 0, false, nullptr, false);
            EntityManager::SpawnEntityAt(e, (unsigned)p->x, (unsigned)p->y, true, true);
            AddSoldier(e);
        }
        e->SetHP(e->GetHpMax());
    }
}

bool PlayerSquad::HasSoldiersToHeal() {
    for (unsigned i = 0; i < SoldierSlots::GetOccupiedSlotCount(); ++i) {
        Entity* e = SoldierSlots::GetSoldierBySlot(i);
        if (!e->player && e->GetHP() != e->GetHpMax()) return true;
    }
    return false;
}

int PlayerSquad::GetSoldierHealCost() {
    int cost = 0;
    for (unsigned i = 0; i < SoldierSlots::GetOccupiedSlotCount(); ++i) {
        Entity* e = SoldierSlots::GetSoldierBySlot(i);
        if (!e->player && e->GetHP() != e->GetHpMax()) {
            int c = (int)((float)(e->GetHpMax() - e->GetHP()) / (float)e->GetHpMax() * (float)e->GetEntityData()->reviveCost);
            cost += c == 0 ? 1 : c;
        }
        cost += e->GetEntityData()->reviveCost;
    }
    return cost;
}

void PlayerSquad::RearrangeSoldiers() {
    std::vector<Entity*> replacements;
    for (size_t i = 0; i < soldiers.size(); ++i) {
        Entity* e = soldiers[i];
        if (!e->IsDead() || e->IsEliteSoldier()) continue;
        SoldierSlots::RemoveSoldier(e, true);
        if (!SoldierPool::GetFirstSoldierByID(e->GetEntityData()->id)) continue;
        SoldierSlots::RemoveSoldier(e, true);
        Entity* r = SoldierPool::GetFirstSoldierByID(e->GetEntityData()->id);
        replacements.push_back(r);
        SoldierPool::RemoveSoldier(r);
        SoldierSlots::AddSoldier(r);
        // UNVERIFIED (milestone 4g): CharacterInfoWindow::UpdateContents.
    }
    ClearDeadSoldiers();
    for (Entity* r : replacements) {
        AI::Waypoint* p = AI::GetRandomWaypointInRange(owner->tileX, owner->tileY, 1, 0, false, nullptr, false);
        EntityManager::SpawnEntityAt(r, (unsigned)p->x, (unsigned)p->y, true, false);
        AddSoldier(r);
        // UNVERIFIED (milestone 4g): CharacterInfoWindow::UpdateContents.
    }
}

void PlayerSquad::RemoveSoldierFromSquad(Entity* e) { BaseSquad::RemoveSoldier(e); }

void PlayerSquad::RemoveSoldier(Entity* e) {
    BaseSquad::RemoveSoldier(e);
    if (!e->IsEliteSoldier()) SoldierSlots::RemoveSoldier(e, true);
    if (!SoldierPool::GetFirstSoldierByID(e->GetEntityData()->id)) {
        // A reserve soldier of another type takes the place: a free slot, else a dead soldier's.
        if (SoldierPool::GetOccupiedSlotCount() == 0) return;
        Entity* r = SoldierPool::GetFirstSoldierExcludingSoldierSlots();
        if (!r) return;
        if (SoldierSlots::GetOccupiedSlotCount() < SoldierSlots::GetTotalSlotsFree()) {
            SoldierSlots::AddSoldier(r);
            SoldierPool::RemoveSoldier(r);
        } else if (Entity* dead = SoldierSlots::GetFirstDeadSoldier()) {
            SoldierSlots::RemoveSoldier(dead, true);
            SoldierSlots::AddSoldier(r);
            SoldierPool::RemoveSoldier(r);
            if (dead->IsEliteSoldier()) SoldierPool::AddSoldier(dead);
        }
        if (AI::Waypoint* p = AI::GetRandomWaypointInRange(e->tileX, e->tileY, 2, 1, false, owner, true)) {
            EntityManager::SpawnEntityAt(r, (unsigned)p->x, (unsigned)p->y, true, true);
            AddSoldier(r);
        }
        return;
    }
    // A reserve soldier of the same type takes the place.
    SoldierSlots::RemoveSoldier(e, true);
    Entity* r = SoldierPool::GetFirstSoldierByID(e->GetEntityData()->id);
    AI::Waypoint* p = AI::GetRandomWaypointInRange(e->tileX, e->tileY, 2, 1, false, owner, true);
    if (!p) return;
    EntityManager::SpawnEntityAt(r, (unsigned)p->x, (unsigned)p->y, true, true);
    AddSoldier(r);
    SoldierPool::RemoveSoldier(r);
    SoldierSlots::AddSoldier(r);
    if (e->IsEliteSoldier()) SoldierPool::AddSoldier(e);
    // UNVERIFIED (milestone 4g): CharacterInfoWindow::UpdateContents.
}

void PlayerSquad::AddSoldier(Entity* e) {
    if (GameState::IsCombatActive() && (CombatType() == 5 || CombatType() == 6))
        GameState::GetActiveCombat()->AddGoodGuy(e);
    if (owner->IsSpeedRunning()) e->EnableSpeedRun();
    BaseSquad::AddSoldier(e);
    // UNVERIFIED (milestone 4g): CharacterInfoWindow::UpdateContents.
}

void PlayerSquad::OnAggro() {
    owner->GetAI()->Reset(true);
    for (size_t i = 1; i < soldiers.size(); ++i) soldiers[i]->GetAI()->Reset(true);
}

void PlayerSquad::OnEnemyDied() {
    // (the original calls Entity::OnRemovedFromCombat, an empty function, once per soldier)
}

int PlayerSquad::GetDeadSoldierCount(int) {
    int n = 0;
    for (unsigned i = 0; i < SoldierSlots::GetOccupiedSlotCount(); ++i)
        if (SoldierSlots::GetSoldierBySlot(i)->IsDead()) ++n;
    return n;
}

void PlayerSquad::ClearDeadSoldiers() {
    for (size_t i = 0; i < soldiers.size();) {
        Entity* e = soldiers[i];
        if (!e->IsDead() || e->player || e->IsEliteSoldier()) {
            ++i;
            continue;
        }
        soldiers.erase(soldiers.begin() + (long)i);
        e->OnRemovedFromSquad();
        e->SetActive(false, false);
        e->UpdateRing();
        EntityManager::RemoveEntity(e, true);
    }
    ready.assign(soldiers.size(), false);
}

void PlayerSquad::ResetRangedWPs() {
    // UNVERIFIED (milestone 4g): each soldier's +0x5c (its ranged-attack waypoint) is cleared; the
    // port's Entity has no such field yet (+0x54/+0x58 are the spawn tile).
}

void PlayerSquad::AddToCombat(Entity* e) {
    if (GameState::IsCombatActive()) GameState::GetActiveCombat()->AddGoodGuy(e);
}

// ------------------------------------------------------------------------------------ EnemySquad

Entity* EnemySquad::GetActiveSoldier() {
    if (!GameState::IsCombatActive()) return nullptr;
    int t = CombatType();
    if (t == 5 || t == 1 || t == 6) return activeSoldier;
    for (Entity* e : soldiers)
        if (e->f75) return e;
    return nullptr;
}

void EnemySquad::SetActiveSoldier(Entity* e) {
    if (!GameState::IsCombatActive()) return;
    int t = CombatType();
    if (t == 5 || t == 6) {
        activeSoldier = e;
        return;
    }
    if (soldiers.empty()) return;
    for (Entity* s : soldiers) s->f75 = false;
    for (size_t i = soldiers.size(); i-- > 0;) {
        if (!soldiers[i]->IsDead() && soldiers[i] == e) {
            e->f75 = true;
            return;
        }
    }
}

void EnemySquad::OnTurnEnded() {
    if (GameState::GetActiveCombat()->GetBadiesCount() != 0) return;
    if (soldiers.empty()) return;
    UpdateAttackers();
}

void EnemySquad::UpdateAttackers() {
    if (!GameState::IsCombatActive()) return;
    for (Entity* s : soldiers) s->f75 = false;
    for (size_t i = soldiers.size(); i-- > 0;) {
        if (!soldiers[i]->IsDead()) {
            soldiers[i]->f75 = true;
            return;
        }
    }
}

void EnemySquad::AddToCombat(Entity* e) {
    if (GameState::IsCombatActive()) GameState::GetActiveCombat()->AddBadGuy(e);
}
