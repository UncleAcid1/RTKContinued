// Entity: a character on the map (workers, the hero, soldiers, monsters, farm animals, storage
// piles): its tile and world position, sprite and animation, appear/disappear fades, stats and
// its AI (AIState.h). Port of Entity (libkingdom.so 5.11, 0x1c8 bytes; constructor @0x15ca70).
// Offsets are the original's.
//
// The fight side (damage, death, drops, aggro) is in EntityCombat.cpp. Not ported yet (marked
// UNVERIFIED where they would run): projectiles, spells, particles, glow effects and sounds.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace Render { struct Sprite; }
namespace Map { struct Building; struct Decor; }
namespace AI { struct Waypoint; }
struct EntityData;
struct AnimationController;
struct Animation;
class AIBaseState;
class BaseSquad;
struct SpawnPoint;
class MetaExpression;
class MetaData;
class Projectile;
namespace Tags { struct Tag; }

class Entity {
public:
    Entity(EntityData* data, int x, int y);      // @0x15ca70
    ~Entity();                                   // @0x156b9c

    // Tile and world position. SetPos also updates the waypoint under the entity.
    void SetPos(int x, int y);                   // @0x155c4c
    void GetPos(int& x, int& y) const { x = tileX; y = tileY; }       // @0x14ffdc
    void SetWorldPos(float x, float y) { worldX = x; worldY = y; }    // @0x14fff0
    void AdjustWorldPos(float dx, float dy) { worldX += dx; worldY += dy; }   // @0x14fffc
    void GetWorldPos(float& x, float& y) const { x = worldX; y = worldY; }    // @0x150020
    void SetDirection(int dir);                  // @0x1550e0 (5, 6, 7 are drawn mirrored)
    int GetDirection() const { return direction; }  // @0x14ffc4
    void TurnTo(int wx, int wy);                 // @0x155128 face a world point
    void TurnTo(const AI::Waypoint* wp);         // @0x1553a0

    // Animation. SetAnimation(name, playCount, randomStart, paused, holdLast, notify, bounce) returns
    // false if this entity has no such animation (it has animations at all).
    bool SetAnimation(const char* name, int playCount, bool randomStart, bool paused, bool holdLast,
                      bool notify, bool bounce);  // @0x156a24
    bool SetAnimationP(const char* name, bool randomStart, bool paused, bool bounce);   // @0x159420 (forever; the std::string overload @0x1594d4)
    bool SetAnimationOnce(const char* name, bool randomStart, bool paused, bool holdLast, bool notify);  // @0x158ba0 (std::string overload @0x159044)
    bool SetAnimationMult(const char* name, int count, bool randomStart, bool paused, bool notify);     // @0x15d0e0
    void EnqueueAnimation(const char* name, float time);   // @0x158c60
    void SetAnimationFrame(int f);               // @0x155410
    bool AnimExists(const char* name) const;     // @0x155408
    void AnimationEnded();                       // @0x15293c
    void OnMapUnload();                          // @0x150764 drops its links into the map
    AnimationController* GetAnimController() const { return anim; }   // @0x14ffcc
    Animation* GetCurrentAnimation() const;      // @0x151158 its controller's active animation
    int GetAnimationFrameCount() const;          // @0x151168 the active animation's frames (0 without one)
    Render::Sprite* GetSprite() const { return sprite; }             // @0x14ffd4

    void Update(float dt);                       // @0x15ba40
    void UpdateGraphics();                       // @0x155504
    void UpdateRing();                           // @0x152380
    void CreateUnderlay(int style);              // @0x1517c4 (Rings: 1 npc .. 7 boss idle)

    void Appear(bool glow, bool sound);          // @0x152dc4 fade in
    void Disappear() { appearing = false; disappearing = true; }   // @0x1506cc fade out
    bool IsAppeared() const { return alpha >= 1.f; }   // @0x1506ac
    bool IsAppearing() const { return appearing; }     // @0x15069c
    bool IsDisappearing() const { return disappearing; }   // @0x1506a4
    void OnAppeared() {}                         // @0x150924
    void OnDisappeared();                        // @0x152724
    // @0x152654 SetActive(active, idle): activating resets the AI (AIBaseState::Reset(idle)).
    void SetActive(bool active, bool idle);
    bool IsActive() const { return active; }     // @0x150478
    // @0x150898: while `w` is BuildingHovers' current hover it stays over the entity's head.
    void SetHoverWindowPositionHandling(void* w) { hoverWindow = w; }
    float GetIdleHeight();                       // @0x150654 (frame height of its sprite's sheet)
    void UpdateIdleInfo();                       // @0x1505cc
    void SetAlpha(float a) { alpha = a; }        // @0x1506e0
    void SetCustomZ(float z) { customZ = z; }    // @0x150044
    void RemoveGlow();                           // @0x1516d4

    void CreateAnims();                          // @0x15ca08
    void CreateAI();                             // @0x154ff4
    void ChangeAIState(int state);               // @0x154fb0 (AIStateFactory, see AIState.h)
    AIBaseState* GetAI() const { return ai; }    // @0x15004c
    void ChangeState(int s) { entityState = s; } // @0x150a00
    int GetState() const { return entityState; } // @0x1509f8

    // AI callbacks
    void OnMoved();                              // @0x155bb8
    void OnWalkComplete();                       // @0x1553bc
    void OnUnableToWalk(int x, int y);           // @0x151f50
    void OnWaypointRemove(AI::Waypoint* wp);     // @0x150800
    void OnStartedToWork();                      // @0x151260
    void OnClick();                              // @0x151f7c
    // @0x1519a4: the world point is within the sprite's width around worldX (a third of it for a
    // boss) and its height above worldY.
    bool Contains(int x, int y) const;

    void SetWorkplace(Map::Building* b);         // @0x1500e8
    Map::Building* GetWorkplace() const { return workplace; }        // @0x1500b8
    void SetWorkplaceDecoration(Map::Decor* d);  // @0x15011c
    Map::Decor* GetWorkplaceDecoration() const { return workplaceDecor; }   // @0x1500c0
    bool IsDecorInRange(const Map::Decor* d, int range) const;   // @0x150b50
    void SetEndWorkTS(int t) { f78 = t; }        // @0x150888 when a goblin's job ends
    void SetHome(Map::Building* b);              // @0x1500c8
    Map::Building* GetHome() const { return home; }                  // @0x1500b0
    void SetOfflineMode(bool on) { offline = on; }   // @0x15068c
    bool GetOfflineMode() const { return offline; }  // @0x150694
    void SetCurrentMap(int id) { currentMap = id; }  // @0x150470
    int GetCurrentMap() const { return currentMap; } // @0x150468
    static void SetCurrentTime(uint32_t t);      // @0x1508e8 (0x6119b0)
    static uint32_t GetCurrentTime();            // PORT: reads it

    EntityData* GetEntityData() const { return data; }   // @0x150034
    bool IsPlayer() const { return player; }     // @0x1505b0
    // @0x15058c: spawned by a spawn point flagged NPC (+0x10). UNVERIFIED (milestone 4): spawn
    // points are not ported, so no entity is an NPC yet.
    bool IsNPC() const { return false; }
    bool IsEnemy() const;                        // @0x150534 class 9, not of an NPC spawn point
    uint32_t GetGUID() const { return guid; }    // @0x1508bc
    void SetGUID(uint32_t g) { guid = g; }       // @0x1508b4

    // ---- quest behaviour (EntityMeta.cpp): the entity's MetaExpression is its spawn point's,
    // unless a foreign one is set (the boss combat).
    MetaExpression* GetMetaExpression() const;                        // @0x15014c
    void SetForeignMetaExpression(MetaExpression* m) { foreignMeta = m; }   // @0x150170
    static void PrepareForSetup();               // @0x151230 seeds the RANDOM places' generator
    void OnSetup();                              // @0x15a938 its state from the quests' state now
    void OnTaskStarted(uint32_t task);           // @0x159518
    void OnTaskCompleted(uint32_t task);         // @0x15a12c
    void OnSubTaskCompleted(uint32_t subtask);   // @0x159a4c
    void OnInteract(uint32_t object);            // @0x152fd0
    void OnTagUpdate(Tags::Tag* tag);            // @0x15b988
    // @0x15854c: one more patrol point (the walk starts over).
    void AddPatrolPoint(const std::string& name, int x, int y, int wait, int direction, const std::string& animation,
                        int times, int pause);
    bool IsMale() const;                         // @0x152b08
    bool IsFat() const;                          // @0x152c50
    bool IsSpeedRunning() const { return speedRunning; }   // @0x150890
    float GetBaseSpeedMultiplier() const;        // @0x151434
    bool IsDead() const { return hp < 1; }       // @0x150064
    bool IsDying() const { return dying; }       // @0x150054
    bool IsActuallyDying() const { return actuallyDying; }   // @0x15005c
    int GetHP() const { return hp; }             // @0x150078
    int GetHpMax() const { return hpMax; }       // @0x150298
    int GetAP() const { return f100; }           // @0x150080
    int GetApMax() const { return f104; }        // @0x1502a0
    void SetAP(int v);                           // @0x1502a8 (capped at GetApMax)
    void AddAP(int v);                           // @0x1502d4 (0..GetApMax)
    void SetHpMax(int v) { hpMax = v; }          // @0x1503b8
    // The item and buff stat changes (MetaExpression::OnApplyItemEffect). Each stays >= 0 (HpMax >= 1).
    void AddHpMax(int v);                        // @0x150180
    void AddAttackMelee(int v);                  // @0x15019c
    void AddAttackRanged(int v);                 // @0x1501b8
    void AddAttackMagic(int v);                  // @0x1501d4
    void AddDefenseMelee(int v);                 // @0x1501f0
    void AddDefenseRanged(int v);                // @0x15020c
    void AddDefenseMagic(int v);                 // @0x150228
    void AddAbsorbMelee(int v);                  // @0x150244
    void AddAbsorbRanged(int v);                 // @0x150260
    void AddAbsorbMagic(int v);                  // @0x15027c
    void AddCritChance(int v);                   // @0x1506f0
    void AddFuryBonus(int v);                    // @0x15070c
    void AddInitiative(int v);                   // @0x150748
    // (Attack and defense also count the combat additions +0x88 / +0x84; an attack of 0 stays 0.)
    int GetAttackMelee() const { return attackMelee == 0 ? 0 : attackMelee + f88; }   // @0x150318
    int GetAttackRanged() const;                 // @0x151e88 0 without ranged damage at this level
    int GetAttackMagic() const;                  // @0x151e2c
    bool HasMeleeDamage() const { return true; } // @0x150178
    bool HasRangedDamage() const;                // @0x151e60 the level's ranged attack > 0
    bool HasMagicDamage() const;                 // @0x151e04
    int GetDefenseMelee() const { return f84 + defenseMelee; }    // @0x150358
    int GetDefenseRanged() const { return f84 + defenseRanged; }  // @0x150368
    int GetDefenseMagic() const { return defenseMagic; }          // @0x150380
    int GetAbsorbMelee() const { return absorbMelee; }            // @0x1503a0
    int GetAbsorbRanged() const { return absorbRanged; }          // @0x1503a8
    int GetAbsorbMagic() const { return absorbMagic; }            // @0x1503b0
    int GetFuryBonus() const { return furyBonus; }                // @0x150730
    int GetHPRegeneration() const { return hpRate ? hpRate : 0xb4; }   // @0x150088 seconds per HP
    void SetHPRegeneration(int v) { hpRate = v; }                 // @0x150098
    int GetLuck() const { return luck; }                          // @0x1500a0
    void SetLuck(int v) { luck = v; }                             // @0x1500a8
    void SetOverrideAttack(int v) { overrideAttack = v; }     // @0x150338
    void SetOverrideDefense(int v) { overrideDefense = v; }   // @0x150390
    // @0x1520ac: HP above HpMax is cut to HpMax; for the player while the over-limit setup's first
    // flag is on, the excess goes to the over-limit (+0xfc, capped by "hp_gift_overlimit").
    void AddHP(int v);
    void ConsumeHpOverlimit(bool addHp);         // @0x152224
    // @0x1508fc: the player's HP over-limit handling (0x6119b4: AddHP keeps the excess, 0x6119b5:
    // ConsumeHpOverlimit may use it).
    static void SetupPlayerHPOverlimit(bool keepExcess, bool consume);
    int GetHpOverlimit();                        // @0x151c30 (the player's, capped by "hp_gift_overlimit")
    int GetOverrideAttack() const { return overrideAttack; }     // @0x150340
    int GetOverrideDefense() const { return overrideDefense; }   // @0x150398
    uint32_t GetUniqueID() const { return uniqueId; }            // @0x1508cc
    void SetUniqueID(uint32_t id) { uniqueId = id; }             // @0x1508c4
    void SetHP(int v);                           // @0x150b74
    void ResetStats(bool hpToo);                 // @0x154d84
    bool NeedRemove() const { return needRemove; }   // @0x150878

    // Squads (Squad.h). The hero leads a PlayerSquad, other leaders an EnemySquad.
    BaseSquad* GetSquad() const { return squad; }    // @0x14ffbc
    void CreateSquad();                              // @0x155418 (replaces the old one)
    void OnAddedToSquad(BaseSquad* s);               // @0x150480 (the player also gets +0x3b)
    void OnRemovedFromSquad();                       // @0x151f18
    bool HasSquad(BaseSquad* s) const { return squad && squad == s; }   // @0x15084c
    bool HasEntity(Entity* e);                       // @0x150824 its AI's HasEntity
    bool ReportReadiness() const { return fae; }     // @0x150868
    void SetReportReadiness(bool on) { fae = on; }   // @0x150870
    void SetRetreating(bool on) { fad = on; }        // @0x150914
    bool IsRetreating() const { return fad; }        // @0x15091c
    // @0x151ca4: by "healable_soldiers": 1 a soldier bought with the second currency (except type
    // 0x11), 2 any once the city tutorial is done, 3 a second-currency one, 4 like 2.
    bool IsEliteSoldier() const;
    void EnableSpeedRun();                           // @0x151500 twice the base speed
    void DisableSpeedRun();                          // @0x1514ac
    // @0x150498: the previous tile becomes the spawn/safe tile (+0x54/+0x58) unless in combat (+0x36)
    // or +0x38 is set.
    static void UpdateSafePos(Entity* e);
    SpawnPoint* GetSpawnPoint() const { return spawnPoint; }   // @0x1504e4
    // @0x151d88: a non-player takes its HP (an NPC 0x400, else its level's; the saved HP when the
    // spawn point has it), HpMax and initiative from the spawn point.
    void SetSpawnPoint(SpawnPoint* sp);
    uint32_t GetSpawnPointID() const;            // @0x14ffa4 (0 without one)
    bool IsOnPatrol() const { return !patrol.empty(); }                      // @0x1503c0
    bool IsWalkingOnPatrol() const { return !patrol.empty() && patrolStep == 1; }   // @0x1503d4
    bool PartolEndsOn(int x, int y) const;       // @0x1503fc
    void RemovePatrol();                         // @0x15d1a0
    void AssignTalkTask(uint32_t task) { talkTask = task; }      // @0x1504c4
    uint32_t GetTalkTask() const { return talkTask; }            // @0x1504cc
    void AssignSpecialTalk(uint32_t id) { specialTalk = id; }    // @0x1504d4
    uint32_t GetSpecialTalk() const { return specialTalk; }      // @0x1504dc
    // @0x1577c0: the hero has reached it. Its talk task's step is done; without one its special talk
    // opens a window (0x1d7 the travel choice, 0x578 task 0x5b7's window, the arena's: milestone 5).
    void OnTalkTo();
    AI::Waypoint* GetCurentWP() const { return waypoint; }       // @0x150a08
    bool HasFixedLocation() const;               // @0x1518c8
    bool HasRandomLocation() const;              // @0x151914
    bool IsPlayerShadow() const;                 // @0x151944 a PvP opponent's stand-in (PVP_SPAWN)
    bool IsWanderingPlayerShadow() const;        // @0x151b48
    const char32_t* GetDeadHint() const;         // @0x151ba8 (nullptr without one)
    const char32_t* GetHint() const;             // @0x151bec
    bool IsEmperor() const;                      // @0x152bf0
    bool HasCorpseItem() const;                  // @0x1504ec its corpse still holds its "corpse_drop" item

    // ---- fights (EntityCombat.cpp) ----
    void SetUpdateSpeedScale(float s) { speedScale = s; }        // @0x14ffb4
    void SetAttackMelee(int v) { attackMelee = v; }              // @0x150330
    void SetAttackRanged(int v) { attackRanged = v; }            // @0x150348
    void SetDefenseMelee(int v) { defenseMelee = v; }            // @0x150350
    void SetDefenseRanged(int v) { defenseRanged = v; }          // @0x150378
    void SetCritChance(int v) { critChance = v; }                // @0x150738
    void SetInitiative(int v) { initiative = v; }                // @0x150740
    void SetAdditiveDefence(int v) { f84 = v; }                  // @0x150938
    void SetAdditiveCrit(int v) { f8c = v; }                     // @0x150948
    void SetAdditiveFuryGain(float v) { f90 = v; }               // @0x150950
    float GetAdditiveFuryGain() const { return f90; }            // @0x150958
    void SetWarriorCost(int v);                                  // @0x150974 its gold (its data's)
    void SetItemDrop(MetaData* drop);                            // @0x150988 replaces its data's "items_drop"
    int GetDamage() const;                                       // @0x1509c0 magic, else ranged, else melee
    int GetDefense() const { return defenseMagic + defenseMelee + defenseRanged; }   // @0x1509e0
    void SetCrit(bool on) { f15c = on; }                         // @0x15081c
    bool CriticalHit() const { return f15c; }                    // @0x150814 the blow it deals or takes is critical
    void SetCurrentTarget(Entity* e) { currentTarget = e; }      // @0x150928
    Entity* GetCurrentTarget() const { return currentTarget; }   // @0x150930
    void OnAttack() {}                                           // @0x150448
    void OnDefend() {}                                           // @0x150450
    void OnRetreat() {}                                          // @0x1504c0
    void CheckAggro() {}                                         // @0x150af8
    void MoveAwayAfterAnimation() { walkAwayOnEnd = true; }      // @0x1508a8
    static bool IsDeathAnimationRunning();                       // @0x1508d4 a boss is dying
    void SetAggro(int range);                    // @0x151558 its spawn point's range (one is made if needed)
    // @0x1515a0: against an opponent of `level` (0: as it is). Three or more levels apart the hero's
    // falls (x0.75 / 0.5 / 0.3 at 2 / 3 / more) and a monster's rises (x1.25 / 1.5 / 1.75).
    int GetInitiative(int level) const;
    void ShowGlowAnimation();                    // @0x1516f8
    bool IsBoss() const;                         // @0x151974 its meta expression has BOSS (0x86)
    void OnAggro();                              // @0x151fa8 (the hero) a monster noticed him
    void OnAddedToCombat();                      // @0x15d248
    void OnRemovedFromCombat();                  // @0x152908
    bool OrbsCanBeUsed();                        // @0x152308 the hero (his squad too while its flag is on)
    // The belt's active orbs (OrbBonus in EntityCombat.cpp); `use` spends a charge of each.
    int GetItemAttackHealthReturn(bool use);     // @0x1531b4
    int GetItemCriticalChanceBonus(bool use);    // @0x153204
    int GetItemDamageAbsorption(bool use);       // @0x153278
    int GetItemDamageBoost(bool use);            // @0x1532c8
    int GetCritChance();                         // @0x153254 percent (spends the crit orbs' charges)
    bool WillDie(Entity* attacker);              // @0x154bfc the attacker's blow would kill it
    void OnBlock(Entity* attacker);              // @0x154b58
    // @0x15e7ac: takes the attacker's blow (never the last HP: Death does that); returns the damage
    // and what it absorbed.
    int OnHit(Entity* attacker, int* absorbed);
    void Hit();                                  // @0x157ce8 the HUD and the spilled gold / experience
    void OnDying(Entity* attacker);              // @0x15d28c the death animation starts
    void OnDeath(Entity* attacker);              // @0x153388 the death animation ended
    void OnProjectileFired(Entity* target, int kind);   // @0x15da30
    void ProjectileLanded();                     // @0x150c70
    // PORT: the next value (0..0x7fff) of the generator PrepareForSetup seeds (0x6119c8); the drops
    // and the broken gear draw from it too.
    static unsigned NextSetupRandom();
    static bool SquadUsesOrbs();                 // PORT: reads 0x6119cd
    static bool HpOverlimitConsumable();         // PORT: reads 0x6119b5

    struct QueuedAnim { std::string name; float time; };   // 0x1c bytes

    std::vector<Entity*> observers;  // +0x00 told OnMoved (their AI's +0x178)
    AI::Waypoint* waypoint = nullptr;   // +0x0c the waypoint under the entity
    int entityState = 0;             // +0x10
    // +0x14 EventManager<Entity> (classes 9 and 10)  UNVERIFIED: not ported
    int linearX = 0, linearY = 0;    // +0x20 +0x24 (Map::TileCoordinatesToLinear)
    bool player = false;             // +0x34
    bool onFarm = false;             // +0x35 classes 3, 4, 6, 7, 0xf (farm coordinates)
    bool f36 = false;                // +0x36 in combat
    bool f37 = false, f38 = false;   // +0x37 +0x38
    bool f39 = true;                 // +0x39
    bool temporary = true;           // +0x3a removed by EntityManager::Clean (CreateEntity's arg)
    bool f3b = false;                // +0x3b in SoldierSlots / the player's squad
    float worldX = 0.f, worldY = 0.f;   // +0x3c +0x40
    int tileX = 0, tileY = 0;        // +0x44 +0x48
    int prevX = 0, prevY = 0;        // +0x4c +0x50
    int spawnX = 0, spawnY = 0;      // +0x54 +0x58 (constructor tile)
    int f5c = 0;                     // +0x5c cleared by PlayerSquad::ResetRangedWPs; nothing reads it in 5.11
    int homeX = 0, homeY = 0;        // +0x60 +0x64
    int workX = 0, workY = 0;        // +0x68 +0x6c
    int workType = 0;                // +0x70 1 building, 2 decoration
    bool hasHome = false;            // +0x74
    bool f75 = true;                 // +0x75 may act (combat turns; squads move only such members)
    int f78 = 0;                     // +0x78 when its job ends (SetEndWorkTS)
    Render::Sprite* underlay = nullptr;  // +0x7c
    Render::Sprite* debugText = nullptr; // +0x80 (Map::debugEntity overlay, off)
    int f84 = 0, f88 = 0, f8c = 0;   // +0x84 defense, +0x88 attack, +0x8c crit chance (combat additions)
    float f90 = 1.f;                 // +0x90
    bool f94 = false, f95 = false, f96 = false;   // +0x94..+0x96
    Entity* nextParticle = nullptr;  // +0x98 the next particle entity of its decoration
    bool f9c = true;                 // +0x9c
    Render::Sprite* glow = nullptr;  // +0xa0
    float glowTime = 0.f, glowAlpha = 0.f;   // +0xa4 +0xa8
    bool disappearOnArrival = false; // +0xac
    bool fad = false;                // +0xad retreating
    bool fae = false;                // +0xae reports readiness to its squad
    bool speedRunning = false;       // +0xaf
    bool offline = false;            // +0xb0
    int currentMap = -1;             // +0xb4
    EntityData* data = nullptr;      // +0xb8
    bool ownsData = false;           // +0xbc
    AnimationController* anim = nullptr;   // +0xc0
    Render::Sprite* sprite = nullptr;      // +0xc4
    Render::Sprite* ring = nullptr;        // +0xc8 the underlay ring
    BaseSquad* squad = nullptr;            // +0xcc
    SpawnPoint* spawnPoint = nullptr;      // +0xd0
    std::vector<QueuedAnim> animQueue;     // +0xd4
    struct PatrolPoint {             // 0x50 bytes
        std::string name;            // +0x00
        int x = 0, y = 0;            // +0x18 +0x1c tile
        int wait = 0;                // +0x20
        int direction = 0;           // +0x24 (-1: none)
        std::string animation;       // +0x28
        int times = 0, pause = 0;    // +0x40 +0x44
        float waitLeft = 0.f, pauseLeft = 0.f;   // +0x48 +0x4c (wait and pause as floats)
    };
    std::vector<PatrolPoint> patrol; // +0xe0
    int patrolIndex = 0, patrolStep = 0;   // +0xec +0xf0
    MetaExpression* foreignMeta = nullptr; // +0x1a4
    uint32_t guid = 0;               // +0x1b8
    int hp = 0;                      // +0xf4
    int hpMax = 0;                   // +0xf8
    int ffc = 0, f100 = 0;           // +0xfc +0x100
    int f104 = 900;                  // +0x104
    int attackMelee = 0, attackRanged = 0, attackMagic = 0;      // +0x108 +0x10c +0x110
    int defenseMelee = 0, defenseRanged = 0, defenseMagic = 0;   // +0x114 +0x118 +0x11c
    int absorbMelee = 0, absorbRanged = 0, absorbMagic = 0;      // +0x120 +0x124 +0x128
    int critChance = 10;             // +0x12c
    int initiative = 0;              // +0x130 (50 for the player)
    int hpRate = 0;                  // +0x134
    int luck = 0, furyBonus = 0;     // +0x138 +0x13c
    int direction = 1;               // +0x140
    AIBaseState* ai = nullptr;       // +0x144
    Map::Building* home = nullptr;   // +0x148
    Map::Building* workplace = nullptr;  // +0x14c
    Map::Decor* workplaceDecor = nullptr;  // +0x150
    Entity* linked = nullptr;        // +0x154
    Entity* currentTarget = nullptr; // +0x158 whom it attacks this turn (BaseCombat::EnemyTurn)
    bool f15c = false;               // +0x15c the blow is critical (SetCrit)
    bool active = false;             // +0x15d
    const char32_t* firstName = U""; // +0x160
    const char32_t* surname = U"";   // +0x164
    int firstNameIdx = 0, surnameIdx = 0;   // +0x168 +0x16c
    float customZ = 0.f;             // +0x170 (<= 0: from the position; GetCustomZ @0x15003c)
    uint32_t talkTask = 0, specialTalk = 0;   // +0x174 +0x178 (ring style 1 while set)
    bool appearing = false, disappearing = false;   // +0x17c +0x17d
    float alpha = 1.f;               // +0x180 (GetAlpha @0x1506e8)
    bool greyed = false;             // +0x184 dead player (shader 3)
    float speedScale = 1.f;          // +0x188 animation and fade speed
    float f18c = 0.f, f190 = 0.f;    // +0x18c +0x190
    bool spawnSound = false;         // +0x194
    bool needRemove = false;         // +0x195
    bool dying = false;              // +0x196 the death animation was started
    bool actuallyDying = false;      // +0x197
    int f198 = 0;                    // +0x198 forced ring style
    int ringStyle = 0;               // +0x19c
    void* hoverWindow = nullptr;     // +0x1a0 the hover window that follows it (SetHoverWindowPositionHandling)
    bool removeSpriteOnEnd = false;  // +0x1a8
    bool walkAwayOnEnd = false;      // +0x1a9
    bool f1aa = true;                // +0x1aa
    Projectile* projectile = nullptr;   // +0x1ac its arrow in flight
    int overrideAttack = 0, overrideDefense = 0;   // +0x1bc +0x1c0
    uint32_t uniqueId = 0;           // +0x1c4
};
