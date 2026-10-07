// Squads: a leader entity (the owner, also soldiers[0]) and the soldiers that follow it in a 3x3
// formation. The hero has a PlayerSquad (its soldiers come from SoldierSlots); other leaders
// (Entity::CreateSquad for a non-player) get an EnemySquad. Port of BaseSquad (@0x110e08..0x112b68,
// 0x44 bytes), PlayerSquad (@0x1e50a8..0x1e5ac4, vtable 0x608a28) and EnemySquad (@0x14fc08..0x14ff4c,
// vtable 0x608400). Virtual functions are in the original's vtable order.
//
// Combat parts (the active combat's type and state, AddToCombat, active soldiers, turns) only run
// while GameState::IsCombatActive, which is false until milestone 4g.
#pragma once
#include <vector>

class Entity;
class BaseCombat;
namespace AI { struct Waypoint; }

class BaseSquad {
public:
    explicit BaseSquad(Entity* owner);                    // @0x112478 (the owner is soldiers[0])
    virtual ~BaseSquad();                                 // +0x00 @0x112614
    virtual void Update(float) {}                         // +0x08 @0x110f30
    virtual int GetSoldierCount() { return (int)soldiers.size(); }   // +0x0c @0x111674
    virtual int GetAliveSoldierCount();                   // +0x10 @0x111270
    virtual std::vector<Entity*>& GetSoldiers() { return soldiers; }   // +0x14 @0x110ef4
    virtual Entity* GetSoldier(unsigned i) { return soldiers[i]; }     // +0x18 @0x110efc
    virtual Entity* GetActiveSoldier() { return nullptr; }             // +0x1c @0x110e08
    virtual void SetActiveSoldier(Entity*) {}             // +0x20 @0x110e10
    virtual void HealSquad() {}                           // +0x24 @0x110e14
    virtual bool HasSoldiersToHeal() { return false; }    // +0x28 @0x110e18
    virtual int GetSoldierHealCost() { return 0; }        // +0x2c @0x110e20
    virtual void RearrangeSoldiers() {}                   // +0x30 @0x110e28
    virtual void RemoveSoldierFromSquad(Entity*) {}       // +0x34 @0x110e2c
    virtual void RemoveSoldier(Entity* e);                // +0x38 @0x1129c4
    virtual void RemoveAllSoldiers();                     // +0x3c @0x112578 (and removes the entities)
    virtual void AddSoldier(Entity* e);                   // +0x40 @0x111cec
    virtual void SetReady(Entity* e);                     // +0x44 @0x111fd0 e reached its place
    virtual void MoveTo(AI::Waypoint* wp);                // +0x48 @0x112750 the owner walks, then the rest
    virtual void MoveSquadTo(AI::Waypoint* wp);           // +0x4c @0x112b68 the soldiers take formation at wp
    virtual void SpawnAt(AI::Waypoint* wp);               // +0x50 @0x111184
    virtual void SpawnSquadAt(AI::Waypoint* wp);          // +0x54 @0x111688 the soldiers around the owner
    virtual bool IsInPosition(AI::Waypoint* wp, bool all);   // +0x58 @0x1115c8 (wp unused)
    virtual Entity* GetOwner() { return owner; }          // +0x5c @0x110eec
    virtual Entity* GetRandomAliveSolider();              // +0x60 @0x11216c (the original's spelling)
    virtual void OnTurnStarted() {}                       // +0x64 @0x110f10
    virtual void OnTurnEnded() {}                         // +0x68 @0x110f14
    virtual void OnBeginCombat(BaseCombat* c) { combat = c; }          // +0x6c @0x110f18
    virtual void OnEndCombat() { attackWP = nullptr; target = nullptr; }   // +0x70 @0x110f20
    virtual void OnAggro() {}                             // +0x74 @0x110e30
    virtual void OnEnemyDied() {}                         // +0x78 @0x110e34
    virtual void SetCurrentTarget(Entity* e) { target = e; }           // +0x7c @0x110e40
    virtual void SetPendingDirection(int d) { pendingDir = d; }        // +0x80 @0x110e48
    virtual void SetAttackWP(AI::Waypoint* wp) { attackWP = wp; }      // +0x84 @0x1110e0
    virtual void UpdateAttackers() {}                     // +0x88 @0x110e38
    virtual bool HasEntity(Entity* e);                    // +0x8c @0x1112f4
    virtual bool IsAnyoneInCombat();                      // +0x90 @0x11121c
    virtual void RemoveAllSoldiersByID(int id);           // +0x94 @0x11280c
    virtual void DisableSpeedRun();                       // +0x98 @0x111410
    virtual void EnableSpeedRun();                        // +0x9c @0x111394
    virtual int GetDeadSoldierCount(int) { return 0; }    // +0xa0 @0x1110e8
    virtual void ClearDeadSoldiers();                     // +0xa4 @0x11148c (and removes the entities)
    virtual void ResetReady();                            // +0xa8 @0x111530
    virtual void ResetRangedWPs() {}                      // +0xac @0x1110f0
    // +0xb0 @0x110e50: the direction (1, 3, 5, 7 or -1) of the step from waypoint a to its neighbour b.
    virtual int GetDir(AI::Waypoint* a, AI::Waypoint* b);
    // +0xb4 @0x110f34: the column and row of the owner's cell (2) in a formation.
    virtual void FindOwnerPos(const char* f, int& col, int& row);
    // +0xb8 @0x110f90: the 3x3 formation for a direction (0 empty, 1 soldier, 2 owner; combat: 3 owner,
    // 2 the front).
    virtual const char* GetFormation(int dir, bool combat);
    virtual void ChangeState(int s) { state = s; }        // +0xbc @0x110f08
    virtual void AddToCombat(Entity*) {}                  // +0xc0 @0x110e3c

    AI::Waypoint* attackWP = nullptr;   // +0x04
    int pendingDir = -1;                // +0x08 the next formation direction (< 0: the walk's)
    Entity* target = nullptr;           // +0x0c the current target
    Entity* activeSoldier = nullptr;    // +0x10 (EnemySquad, arena combats)
    std::vector<Entity*> soldiers;      // +0x14 the owner first
    std::vector<bool> ready;            // +0x20 per soldier: in place
    Entity* owner = nullptr;            // +0x34
    BaseCombat* combat = nullptr;       // +0x38
    int state = 0;                      // +0x3c
    int type = 0;                       // +0x40 0 player squad, 1 enemy squad
};

class PlayerSquad : public BaseSquad {
public:
    explicit PlayerSquad(Entity* owner) : BaseSquad(owner) { type = 0; }   // @0x1e5540
    void HealSquad() override;                            // +0x24 @0x1e56a0 also the dead ones in SoldierSlots
    bool HasSoldiersToHeal() override;                    // +0x28 @0x1e51b8
    int GetSoldierHealCost() override;                    // +0x2c @0x1e50e4
    void RearrangeSoldiers() override;                    // +0x30 @0x1e5838 dead ones swapped for reserves
    void RemoveSoldierFromSquad(Entity* e) override;      // +0x34 @0x1e52c8
    void RemoveSoldier(Entity* e) override;               // +0x38 @0x1e52cc a reserve takes its place
    void AddSoldier(Entity* e) override;                  // +0x40 @0x1e523c
    void OnAggro() override;                              // +0x74 @0x1e5630
    void OnEnemyDied() override;                          // +0x78 @0x1e55ec
    int GetDeadSoldierCount(int) override;                // +0xa0 @0x1e50a8 (in SoldierSlots)
    void ClearDeadSoldiers() override;                    // +0xa4 @0x1e5ac4 (elite ones stay)
    void ResetRangedWPs() override;                       // +0xac @0x1e55b8
    void AddToCombat(Entity* e) override;                 // +0xc0 @0x1e5210
};

class EnemySquad : public BaseSquad {
public:
    explicit EnemySquad(Entity* owner) : BaseSquad(owner) { type = 1; }    // @0x14fcbc
    Entity* GetActiveSoldier() override;                  // +0x1c @0x14fe98
    void SetActiveSoldier(Entity* e) override;            // +0x20 @0x14fd34
    void OnTurnEnded() override;                          // +0x68 @0x14ff4c
    void OnEndCombat() override { attackWP = nullptr; target = nullptr; }   // +0x70 @0x14fc34
    void UpdateAttackers() override;                      // +0x88 @0x14fe08
    void AddToCombat(Entity* e) override;                 // +0xc0 @0x14fc08
};
