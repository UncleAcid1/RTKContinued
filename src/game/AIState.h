// The entity AI classes: BaseAI (speed, registry) -> TargetedAI (path search on the waypoint graph)
// -> AIBaseState (walking, states, the virtual interface every behaviour overrides) -> AIWorker
// (city workers). Port of BaseAI/TargetedAI (@0xe1a6c..0xe7600), AIBaseState (@0xe81ec..0xea0f8),
// AIWorker (@0xf7d28..0xf8a60) and AIStateFactory::CreateNewState.
//
// Virtual functions are in the original's vtable order (slot offsets in the comments). Slots
// +0x88..+0xc0 (attacks, hits, magic) are the combat interface (milestone 4g) and are not declared.
#pragma once
#include <string>
#include <vector>

#include "game/AI.h"
#include "game/Contracts.h"

namespace GameState { struct Order; }
namespace Render { struct Sprite; struct Texture; }

class Entity;
class HiddenObjects;
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

class AIPatch;

// PatchAnimationController (0x14 bytes): plays the growth powder's frames on its patch's dust
// sprite and removes the sprite after the last frame. Port of @0x1e3e04..0x1e3fdc.
struct PatchAnimationController {
    explicit PatchAnimationController(AIPatch* p) : patch(p) {}   // @0x1e3e04
    void Pause(bool on) { paused = on; }                          // @0x1e3e54
    void SetAnim(Render::Texture* t) { acc = 0.f; frame = 0; tex = t; }   // @0x1e3e5c
    void Update(float dt);                                        // @0x1e3e78

    int frame = 0;                  // +0x00
    Render::Texture* tex = nullptr; // +0x04
    AIPatch* patch = nullptr;       // +0x08
    float acc = 0.f;                // +0x0c
    bool paused = false;            // +0x10
};

// A farm view's soil patch (class 6: entities 10, 0xe8, 0xe0): its state is the patch state
// (0x12 empty, 0x13 dirty, 0x14 locked, 0x15..0x18 growing, 0x19 ready, 0x1a rotten, 0x1b for sale,
// 0x1c bought with an order waiting); its crop is a separate entity whose frame is the growth stage.
class AIPatch : public AIBaseState {
public:
    explicit AIPatch(Entity* e);                      // @0xed688
    ~AIPatch() override;                              // @0xed5f8
    bool HasEntity(Entity* e) override;               // +0x10 @0xed0f4
    void Update(float dt) override;                   // +0x4c @0xed8a0
    void UpdateLastTarget() override;                 // +0x68 @0xecff4
    void ChangeState(int s) override;                 // +0xc4 @0xed14c
    void Clean() override;                            // +0xf4 @0xed5ac
    // +0xf8 @0xedac0: -2 ends the current growth stage now, -1 starts the powder speed-up (a stage
    // per second until ready), n > 0 moves the patch's timers n seconds earlier.
    void SpeedUp(int n) override;
    bool SpeedUpProcess() override { return speedingUp; }   // +0xfc @0xecfd8
    // +0x120 @0xedd30: the crop of order `contract` (0-based) of delivery list `deliveryId`, as the
    // farm's order on `patch`; it is planted if the patch is empty.
    void SetItem(int deliveryId, int contract, int patch) override;
    int GetItem() override { return crop ? 1 : 0; }   // +0x124 @0xecfd0 (the crop entity)
    void Revive() override;                           // +0x128 @0xed00c
    void CleanFarm() override;                        // +0x12c @0xed088
    int GetFarmPatchNum() override { return patchNum; }          // +0x130 @0xecfb0
    void SetFarmPatchNum(unsigned n) override { patchNum = (int)n; }   // +0x134 @0xecfb8
    Map::Building* GetFarm() const { return farmBuilding; }   // +0x158 @0xecfc0
    Render::Sprite* GetDust() const { return dust; }  // +0x15c @0xecfc8
    void StopDust();                                  // +0x160 @0xed134

    int cropId = 0;               // +0xe0 the crop entity's id
    Entity* crop = nullptr;       // +0xe4
    Map::Building* farmBuilding = nullptr;  // +0xe8 Map::GetCurrentFarm() at creation
    int type = 0;                 // +0xec 1 vegetable farm (0x13), 3 oil farm (0x72), 4 animal farm (0x3ee)
    Contracts::ContractMission mission;   // +0xf0 a copy of the order (growTimes at +0x140)
    Render::Sprite* sign = nullptr;       // +0x14c the locked / for-sale sign (vegetable farm)
    Render::Texture* lockedTex = nullptr; // +0x150 images/Farm/soil_patch_locked2
    Render::Texture* buyTex = nullptr;    // +0x154 images/Farm/soil_patch_buy2
    Render::Sprite* dust = nullptr;       // +0x158 the speed-up powder
    PatchAnimationController* dustAnim = nullptr;   // +0x15c
    int patchNum = -1;            // +0x160 index in the farm's patch list
    bool force = true;            // +0x164 the next ChangeState runs even for the same state
    bool speedingUp = false;      // +0x165
};

// The farm view's farmer (class 5: entities 0xe, 0xdf): walks to the patches, plants, waters,
// harvests and cleans them; Farm queues the orders it is given.
class AIFarmerBig : public AIBaseState {
public:
    struct FarmActionQueue { int patch, contract, flag; };   // 0xc bytes

    explicit AIFarmerBig(Entity* e);                  // @0xeb9c4
    ~AIFarmerBig() override;                          // @0xeb7dc
    void Update(float dt) override;                   // +0x4c @0xeb1b4
    void UpdateLastTarget() override;                 // +0x68 @0xeaf38
    void AnimEnded() override;                        // +0xc8 @0xeace4
    void WalkCompleted() override {}                  // +0xd8 @0xea8b8
    // +0x100 @0xeb4a0: queue order `contract` on `patch` (plant: launch it first); 999 marks a
    // harvest. A patch already queued with a real order is ignored.
    void Farm(int contract, int patch, bool plant) override;
    void SetCurrentPatch(unsigned p) override { patch = (int)p; }   // +0x11c @0xea8b0
    void StartHarvesting();                           // @0xea8e4
    void StartCleaning();                             // @0xeaa1c
    void StartWatering();                             // @0xeaafc
    void StartSeeding();                              // @0xeac04
    void Move(bool plant, int p);                     // @0xeaecc to patch p
    void ChooseAction();                              // @0xeaf78 at the patch: the work it needs
    // @0xeb0a4: one of the first n patch entities, n = the patches neither locked nor for sale (all:
    // empty ones count too).
    Entity* GetRandomPatch(bool all);

    AI::Waypoint* moveTarget = nullptr;   // +0x64 the patch waypoint being walked to
    float idleTime = 0.f;         // +0xe0
    float actionLeft = 0.f;       // +0xe4
    float actionTime = 0.f;       // +0xe8
    bool water = true;            // +0xec the current patch wants watering
    bool idle = true;             // +0xed no queued work running
    int patch = -1;               // +0xf0
    int loops = 1;                // +0xf4 replays of the work animation left
    std::vector<FarmActionQueue> queue;   // +0xf8
};

// The farmer at a farm on the city map (class 2: entities 6, 7): stands at the farm's parking spot
// and, from the first patch's state, seeds, waters or rests for a while, then idles 5-9 s.
class AIFarmerSmall : public AIBaseState {
public:
    explicit AIFarmerSmall(Entity* e) : AIBaseState(e) {}   // @0xec354
    void Update(float dt) override;                   // +0x4c @0xec1e0
    void WalkCompleted() override;                    // +0xd8 @0xebf18 (tutorial steps)
    void AssignToJob(Map::Building* b) override;      // +0x104 @0xebfa4
    void StartResting();                              // @0xebd3c state 0x11
    void StartWatering();                             // @0xec018 state 0x10
    void StartSeeding();                              // @0xec0ac state 0xf
    void ChooseAction();                              // @0xec100

    float idleTime = 0.f;         // +0xe0 until the next ChooseAction (state 0)
    float actionLeft = 0.f;       // +0xe4 of the current action
};

// The soldier (class 10) and the base of the hero's AI: walks with the squad ("run", 239.4 for
// soldiers, 180 for others), reports to its squad when it arrives, trains at a city building.
// Port of AIWarrior (@0xf208c..0xf4b78, vtable 0x6072a0). Its combat slots (+0x88..+0xcc: attacks,
// hits, blocks, deaths, projectiles) and the combat timers in Update come with milestone 4g.
class AIWarrior : public AIBaseState {
public:
    explicit AIWarrior(Entity* e);                    // @0xf39dc
    using AIBaseState::WalkTo;
    void Update(float dt) override;                   // +0x4c @0xf3660
    void WalkTo(int x, int y) override;               // +0x6c @0xf2328
    void AnimEnded() override;                        // +0xc8 @0xf48c4
    void WalkCompleted() override;                    // +0xd8 @0xf240c
    void StartTraining() override;                    // +0xe0 @0xf2550 swings at its workplace (state 0x1e)
    void AssignToJob(Map::Building* b) override;      // +0x104 @0xf26cc walks to the work tile (state 0x1d)
    void RemoveFromJob() override;                    // +0x10c @0xf2604
    bool IsWorking() override { return GetState() == 0x1e; }   // +0x114 @0xf20dc
    void StopMovement() override;                     // +0x140 @0xf2244

    float f74 = 1.f;              // +0x74 (magic hit argument)
    bool f78 = true, f79 = false; // +0x78 +0x79
    int f7c = 0;                  // +0x7c
    float f8c = 0.f, f90 = 0.f, f94 = 0.f;   // +0x8c +0x90 +0x94 (combat timers with +0x80..+0x98)
};

// The hero (class 5). Taps on the map make the squad walk there (a "goto" marker shows the target,
// a "blocked" one an unreachable tap); taps on characters start the matching action, which runs
// when the hero arrives (ProcessAction). Port of AIPlayer (@0xee378..0xf19ac, vtable 0x606c88).
// Actions (+0xe4): 1 portal, 2 talk, 3 decoration job, 4 blocked decoration, 5 decoration action,
// 6/7 search a corpse, 8 a PvP shadow.
class AIPlayer : public AIWarrior {
public:
    explicit AIPlayer(Entity* e);                     // @0xf188c
    ~AIPlayer() override;                             // @0xf1788
    void Update(float dt) override;                   // +0x4c @0xf14e0
    void ClickedEntity(Entity* e) override;           // +0x50 @0xf05a4
    void ClickedTile(int x, int y, int wx, int wy) override;   // +0x54 @0xf0eec
    void AnimEnded() override;                        // +0xc8 @0xf03f0
    void Reset(bool idle) override;                   // +0xd4 @0xeed48 (always to idle)
    void WalkCompleted() override;                    // +0xd8 @0xf01fc
    void UnableToWalk(int x, int y) override;         // +0xe4 @0xeec6c
    bool TryToInterruptJob() override;                // +0x110 @0xef578
    bool IsWorking() override { return GetState() == 0x21; }   // +0x114 @0xee378
    void RemoveActionMarker() override;               // +0x13c @0xeeb80
    void StopMovement() override;                     // +0x140 @0xeed68
    void AutoInteraction() override { autoInteraction = true; }   // +0x144 @0xee398
    void OnMapUnload() override;                      // +0x150 @0xee644
    void ProcessAction();                             // @0xef6e0 the action of the tap, on arrival

    AI::Waypoint* actionWP = nullptr;   // +0x64 where the action is done
    void* actionTarget = nullptr;       // +0xe0 the tapped entity, decoration or portal
    int action = 0;                     // +0xe4
    int actionArg = 0;                  // +0xe8
    int arrivalDir = -1;                // +0xec the direction to face on arrival (-1: keep)
    bool autoInteraction = false;       // +0xf0
    HiddenObjects* hidden = nullptr;    // +0xf4 decorations in front of the hero (faded in combat)
    bool movingSquad = false;           // +0xf8 a tap is moving the squad (UnableToWalk retries)
};

// AIStateFactory::CreateNewState: 0 AIBaseState, 1 AIWarrior, 2 AIPlayer, 3/4 AIWorker,
// 5 AIFarmerBig, 6 AIPatch, 7 AIGoblin, 8 AIFarmerSmall, 9 AIEnemy, 10 AISpell, 11 AIPlayerBot.
AIBaseState* CreateAIState(int state, Entity* e);
