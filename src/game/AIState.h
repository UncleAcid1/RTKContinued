// The entity AI classes: BaseAI (speed, registry) -> TargetedAI (path search on the waypoint graph)
// -> AIBaseState (walking, states, the virtual interface every behaviour overrides) -> AIWorker
// (city workers). Port of BaseAI/TargetedAI (@0xe1a6c..0xe7600), AIBaseState (@0xe81ec..0xea0f8),
// AIWorker (@0xf7d28..0xf8a60) and AIStateFactory::CreateNewState.
//
// Virtual functions are in the original's vtable order (slot offsets in the comments). Slots
// +0x88..+0xc0 (attacks, hits, magic) are the combat interface (milestone 4) and are not declared.
#pragma once
#include <string>
#include <vector>

#include "game/AI.h"

namespace GameState { struct Order; }

class Entity;
namespace Map { struct Building; struct Decor; }

class BaseAI {
public:
    BaseAI();                                         // @0xe3790
    virtual ~BaseAI();                                // +0x00 @0xe1a6c
    virtual void OnWaypointRemove(AI::Waypoint* wp);  // +0x08 @0xe2298
    virtual void OnMapRemove() {}                     // +0x0c @0xe1b34
    virtual bool HasEntity(Entity* e);                // +0x10 @0xe1b38
    virtual void SetOverrideSpeed(float s) { overrideSpeed = s; }   // +0x14 @0xe1b4c
    virtual void SetSpeed(float s);                   // +0x18 @0xe22a0
    virtual float GetSpeed() { return speed; }        // +0x1c @0xe1b54
    virtual float GetBaseSpeed() { return baseSpeed; }  // +0x20 @0xe1b5c

    // Every live AI (0x610b28), for AI::RemoveMapWaypoint and AI::FreeWaypoints.
    static const std::vector<BaseAI*>& All();

    bool farm = false;            // +0x04 walks on the farm grid
    unsigned regIndex = 0;        // +0x08 slot in All()
    Entity* entity = nullptr;     // +0x0c
    float x = 0.f, y = 0.f;       // +0x10 +0x14 world position while walking
    float speed = 12.f;           // +0x18
    float baseSpeed = 12.f;       // +0x1c
    float overrideSpeed = 0.f;    // +0x20
};

class TargetedAI : public BaseAI {
public:
    explicit TargetedAI(Entity* e);                   // @0xe6750
    void OnWaypointRemove(AI::Waypoint* wp) override; // +0x08 @0xe61e4
    void OnMapRemove() override;                      // +0x0c @0xe1b64
    // +0x24 @0xe69c0: a cheapest path from the current waypoint to wp (same part only) into `path`,
    // target first. With keepCopy false the path is also copied into pathCopy. False if none.
    virtual bool SetTarget(AI::Waypoint* wp, bool noCopy);
    virtual AI::Waypoint* SetTarget(Map::Building* b);  // +0x28 @0xe4b74
    virtual AI::Waypoint* SetTarget(Map::Decor* d);     // +0x2c @0xe4c8c
    virtual AI::Waypoint* GetLastTarget() { return current; }   // +0x38 @0xe1c18
    // +0x3c @0xe711c: the waypoint `steps` steps before wp on the cheapest path to it.
    virtual AI::Waypoint* GetClosestReachableWP(AI::Waypoint* wp, int steps);
    // (+0x30 GetDistanceToTarget, +0x34 SetBlockedPathTarget, +0x40..+0x48 the
    //  GetClosestReachableWPTo* family are not ported yet.)

    std::vector<AI::Waypoint*> pathCopy;   // +0x24
    std::vector<AI::Waypoint*> path;       // +0x34 target first; path.back() is the next step
    std::vector<AI::Waypoint*> reach;      // +0x44 GetClosestReachableWP's trail
    int stepDir = 0;                       // +0x54 the last step's direction code (1..8)
    AI::Waypoint* current = nullptr;       // +0x58
    AI::Waypoint* next = nullptr;          // +0x5c
    bool repathing = false;                // +0x60 a new target was set while walking
};

class AIBaseState : public TargetedAI {
public:
    explicit AIBaseState(Entity* e);                  // @0xe959c
    ~AIBaseState() override = default;                // @0xe9684
    bool HasEntity(Entity* e) override;               // +0x10 @0xe83a0
    virtual void Update(float dt);                    // +0x4c @0xe9010
    virtual void ClickedEntity(Entity*) {}            // +0x50
    virtual void ClickedTile(int, int, int, int) {}   // +0x54
    virtual void ClickedPortal() {}                   // +0x58
    virtual void ClickedDecoration(Map::Decor*) {}    // +0x5c
    virtual void SearchDecoration() {}                // +0x60
    virtual void UpdateAggroZone() {}                 // +0x64
    virtual void UpdateLastTarget();                  // +0x68 @0xe8880
    virtual void WalkTo(int x, int y);                // +0x6c @0xe90f4
    virtual void WalkTo(AI::Waypoint* wp);            // +0x70 @0xe8954
    virtual AI::Waypoint* WalkToBuilding(Map::Building* b);   // +0x74 @0xe87d4
    virtual AI::Waypoint* WalkToDecoration(Map::Decor* d);    // +0x78 @0xe8728
    virtual void TurnTo(AI::Waypoint* wp);            // +0x7c @0xe8498
    virtual void TurnTo(int x, int y, int tx, int ty);  // +0x80 @0xe83d0
    virtual void TurnTo(int x, int y, int bx, int by, int w, int h);  // +0x84 @0xe97f8
    virtual void ChangeState(int s) { state = s; }    // +0xc4 @0xe8384
    virtual void AnimEnded() {}                       // +0xc8
    virtual void ClearPath() { path.clear(); }        // +0xd0 @0xe8394
    virtual void Reset(bool idle);                    // +0xd4 @0xe898c
    virtual void WalkCompleted() {}                   // +0xd8
    virtual void PositionChanged() {}                 // +0xdc
    virtual void StartTraining() {}                   // +0xe0
    virtual void UnableToWalk(int, int) {}            // +0xe4
    virtual void Retreat() {}                         // +0xe8
    virtual int GetFutureDirection();                 // +0xec @0xe82d8
    virtual int GetState() { return state; }          // +0xf0 @0xe838c
    virtual void Clean() {}                           // +0xf4
    virtual void SpeedUp(int) {}                      // +0xf8 @0xe8260
    virtual bool SpeedUpProcess() { return false; }   // +0xfc @0xe8264
    // +0x100 @0xe826c: a farmer works order `contract` (0-based) on soil patch `patch`.
    virtual void Farm(int contract, int patch, bool plant) {}
    virtual void AssignToJob(Map::Building*) {}       // +0x104
    virtual bool AssignToJob(Map::Decor*) { return false; }   // +0x108
    virtual void RemoveFromJob() {}                   // +0x10c
    virtual bool TryToInterruptJob() { return false; }   // +0x110
    virtual bool IsWorking() { return false; }        // +0x114
    virtual void CancelWork() {}                      // +0x118
    virtual void SetCurrentPatch(unsigned) {}         // +0x11c @0xe8294
    virtual void SetItem(int, int, int) {}            // +0x120 @0xe8298
    virtual int GetItem() { return 0; }               // +0x124 @0xe829c
    virtual void Revive() {}                          // +0x128 @0xe82a4
    virtual void CleanFarm() {}                       // +0x12c @0xe82a8
    virtual int GetFarmPatchNum() { return 0; }       // +0x130 @0xe82ac
    virtual void SetFarmPatchNum(unsigned) {}         // +0x134 @0xe82b4
    virtual void ResetOrder(Map::Building*) {}        // +0x138
    virtual void RemoveActionMarker() {}              // +0x13c
    virtual void StopMovement() {}                    // +0x140
    virtual void AutoInteraction() {}                 // +0x144
    virtual void CheckAggro() {}                      // +0x148
    virtual void OnMapUnload() {}                     // +0x150
    virtual void UpdateWalking(float dt, int depth);  // +0x154 @0xe8a24

    void SetCustomWalkAnimation(const char* name) { walkAnim = name; }   // @0xe9b88
    void SetCustomIdleAnimation(const char* name, int first, int last);  // @0xe9df0

    Entity* linked = nullptr;     // +0x68 (HasEntity)
    bool arrived = false;         // +0x70 reached a waypoint this update
    float f80 = 0.f, f84 = 0.f, f88 = 0.f, f98 = 0.f;   // +0x80 +0x84 +0x88 +0x98
    int f9c = 1;                  // +0x9c
    int state = 0;                // +0xa0
    float fa4 = 1.f;              // +0xa4
    std::string walkAnim;         // +0xa8 custom walk animation ("walk" when empty)
    std::string idleAnim;         // +0xc0 custom idle animation ("idle_1" when empty)
    int sprintSteps = 0;          // +0xd8
    float runTime = 0.f;          // +0xdc time the "run" animation has played
};

class AIWorker : public AIBaseState {
public:
    explicit AIWorker(Entity* e);                     // @0xf870c
    void Update(float dt) override;                   // +0x4c @0xf88b4
    void WalkCompleted() override;                    // +0xd8 @0xf8070
    void AssignToJob(Map::Building* b) override;      // +0x104 @0xf852c
    bool AssignToJob(Map::Decor* d) override;         // +0x108 @0xf8428
    void RemoveFromJob() override;                    // +0x10c @0xf8140
    bool IsWorking() override { return state == 6; }  // +0x114 @0xf7d28
    void StartWorking();                              // @0xf7d3c

    int job = 0;                  // +0xe0 1 build/decoration, 2 lumber, 4 stone
    float wanderTime = 0.f;       // +0xe4 seconds to the next stroll (0..29)
    float jobTime = 0.f;          // +0xe8 seconds to the next idle-workplace check (1..6)
    AI::Waypoint* strollTarget = nullptr;   // +0xec
};

// The storage goblin (class 0x10): takes delivery orders (GameState::GetTopOrder), walks to the
// pile, carries it to the storage and adds it to the player's resources.
class AIGoblin : public AIBaseState {
public:
    explicit AIGoblin(Entity* e);                     // @0xecef8
    void Update(float dt) override;                   // +0x4c @0xece1c
    void Reset(bool idle) override;                   // +0xd4 @0xec480
    void WalkCompleted() override;                    // +0xd8 @0xec518
    bool AssignToJob(Map::Decor* d) override;         // +0x108 @0xecd3c
    void RemoveFromJob() override;                    // +0x10c @0xeccb8
    bool IsWorking() override { return GetState() == 8; }   // +0x114 @0xec3dc
    void CancelWork() override;                       // +0x118 @0xec3fc
    void ResetOrder(Map::Building* b) override;       // +0x138 @0xec460
    void GetOrder();                                  // @0xecc5c
    void GotoTarget();                                // @0xecbc0 to the order's pile
    void GotoDestination();                           // @0xec4b0 to the order's storage

    float orderTime = 0.f;        // +0xe0 seconds to the next GetOrder
    GameState::Order* order = nullptr;   // +0xe4
    int step = 0;                 // +0xe8 0 free, 1/2 carrying, 3/4 to the pile, 5 decoration job
};

// AIStateFactory::CreateNewState: 0 AIBaseState, 1 AIWarrior, 2 AIPlayer, 3/4 AIWorker,
// 5 AIFarmerBig, 6 AIPatch, 7 AIGoblin, 8 AIFarmerSmall, 9 AIEnemy, 10 AISpell, 11 AIPlayerBot.
AIBaseState* CreateAIState(int state, Entity* e);
