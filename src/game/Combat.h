// BaseCombat: a fight between the player's squad and an enemy squad, run as a state machine from
// GameState's update. Port of BaseCombat (@0x10c3cc..0x110bd0, vtable 0x607cc8, 0x80 bytes),
// AggroCombat (@0xf8cf8..0xf9318, the enemy noticed the hero and comes to him) and RangeAggroCombat
// (@0x1f57ac..0x1f5c08). Virtual functions are in the original's vtable order.
//
// A fight is a series of turns. A turn pairs every living fighter with its possible targets, sorted
// by initiative (StartNewTurn); each pair attacks once (CombatReady -> PlayerTurn / EnemyTurn) and
// the turn ends when no pair is left whose targets are all alive (EndTurn).
//
// States (+0x20): 0 over, 1 WaitingForBossBattleClick, 2 WaitingForContinue, 4 StartPositioning,
// 5 UpdatePositions, 6 CombatReady, 7 PlayerTurn, 8 EnemyTurn, 9 CheckSpellCast, 10 SpellCast,
// 0xc UpdateEnemyPosition.
#pragma once
#include <vector>

class BaseSquad;
class Entity;
namespace AI { struct Waypoint; }
namespace Spells { struct SpellInfo; }

class BaseCombat {
public:
    struct Pair {                        // 0x18 bytes
        Entity* attacker = nullptr;      // +0x00
        std::vector<Entity*> targets;    // +0x04
        bool done = false;               // +0x10 it has attacked this turn
        int initiative = 0;              // +0x14
    };

    BaseCombat();                                     // @0x10d720
    virtual ~BaseCombat();                            // +0x00 @0x10de94
    virtual void Update(float dt);                    // +0x08 @0x10d158
    virtual void BeginCombat(BaseSquad* player, BaseSquad* enemy);   // +0x0c @0x10cfb8
    virtual void BeginCombat(std::vector<Entity*>&, std::vector<Entity*>&) {}   // +0x10 @0x10c424
    // +0x14 @0x10c3f8: the fight stops between turns (a soldier or the hero is dying); without an
    // entity the turn ends at once.
    virtual void Pause(Entity* e);
    virtual void PlayerDied() {}                      // +0x18 @0xf8cbc
    virtual bool IsPaused() { return paused; }        // +0x1c @0x10c41c
    virtual void CancelCombat() { cancel = true; }    // +0x20 @0x10c3ec
    virtual void QueueSpellCast(Spells::SpellInfo* s) { spell = s; spellQueued = true; }   // +0x24 @0x10c3dc
    virtual void Continue();                          // +0x28 @0x10cf78
    virtual void AddGoodGuy(Entity* e);               // +0x2c @0x110998
    virtual void AddBadGuy(Entity* e);                // +0x30 @0x110bd0
    virtual bool HasActivePair() { return pair != nullptr; }   // +0x34 @0x10c494
    virtual void PostCombat() {}                      // +0x38 @0xf8cc0
    virtual void EndCombat();                         // +0x3c @0x10d8d8
    virtual bool IsCompleted() { return completed; }  // +0x40 @0x10c3cc
    virtual void SetCompleted(bool on) { completed = on; }   // +0x44 @0x10c518
    virtual bool KilledBoss(int spawnId);             // +0x48 @0x10c4b4
    virtual int GetState() { return state; }          // +0x4c @0x10c4ac
    virtual int GetType() { return type; }            // +0x50 @0x10c4a4 (2 aggro; 5, 6 the arenas)
    virtual int GetBadiesCount() { return (int)badGuys.size(); }   // +0x54 @0x10d70c
    virtual bool HasEntity(Entity* e);                // +0x58 @0x10da94
    virtual bool HasSquad(BaseSquad* s);              // +0x5c @0x10c4d4
    virtual void SetEnemySquad(BaseSquad* s) { enemySquad = s; }   // +0x60 @0x10c508
    virtual BaseSquad* GetEnemySquad() { return enemySquad; }      // +0x64 @0x10c510
    virtual void OnEnemyDied() {}                     // +0x68 @0xf8cc4
    virtual bool IsSpellCastInQueue() { return spellQueued; }      // +0x6c @0x10c520
    virtual bool CanSpellCastNow();                   // +0x70 @0x10c528
    virtual AI::Waypoint* GetSpellEpicenter() { return nullptr; }  // +0x74 @0x10c5bc
    // +0x78 @0x10c8fc: straight to a boss (other), arena (5) or PvP arena (6) fight.
    virtual void ForceBossFight(int kind, int arg);
    virtual void SetTurnCount(int n) { turnCount = n; }            // +0x7c @0x10c55c
    virtual int GetCurrentMode() { return mode; }     // +0x80 @0x10c5b4
    virtual Entity* GetBoss();                        // +0x84 @0x10c5c4 the enemy squad's owner
    virtual float GetCombatTimeMult() { return timeMult; }         // +0x88 @0x10c5ec
    virtual void SpellAreaSelectionStarted() {}       // +0x8c @0xf8cc8
    virtual void SpellAreaSelectionEnded() {}         // +0x90 @0xf8ccc
    virtual int GetCurrentWave() { return 0; }        // +0x94 @0xf8cd0
    virtual int GetWaveCount() { return 0; }          // +0x98 @0xf8cd8
    virtual void AddSpentMoney(int, int) {}           // +0x9c @0xf8ce0
    virtual int GetSpentGold() { return 0; }          // +0xa0 @0xf8ce4
    virtual void OnValorReceived(unsigned) {}         // +0xa4 @0xf8cec
    virtual void WaitingForContinue();                // +0xa8 @0x1106e8
    virtual void WaitingForBossBattleClick() {}       // +0xac @0xf8cf0
    virtual void StartPositioning();                  // +0xb0 @0x10cee0
    virtual void CombatReady();                       // +0xb4 @0x10c9c4
    virtual void EnemyTurn();                         // +0xb8 @0x10eab0
    virtual void PlayerTurn();                        // +0xbc @0x10cc78
    virtual void SwitchToBossFight();                 // +0xc0 (0x10efb0)
    virtual void SwitchToArenaFight();                // +0xc4 (0x10fa40)
    virtual void SwitchToPvPArenaFight(int arg);      // +0xc8 (0x10ff38)
    virtual void CheckSpellCast();                    // +0xcc @0x10c428
    virtual void SpellCast();                         // +0xd0 @0x10cb08
    virtual void UpdateEnemyPosition();               // +0xd4 @0x10c564
    virtual void UpdatePositions();                   // +0xd8 @0x10d648
    virtual void UpdateAttackPosition();              // +0xdc @0x10cdd4
    virtual void SwitchState(int s) { state = s; }    // +0xe0 @0x10c3d4
    virtual void StartNewTurn();                      // +0xe4 @0x10e058
    virtual void EndTurn();                           // +0xe8 @0x10d420
    // +0xec @0x10e868: the first pair that has not attacked and whose targets are all alive.
    virtual Pair* GetPair();
    virtual Pair* GetPlayerPair();                    // +0xf0 @0x10d3cc the hero's pair

    BaseSquad* playerSquad = nullptr;    // +0x04
    BaseSquad* enemySquad = nullptr;     // +0x08
    Pair* pair = nullptr;                // +0x0c the pair whose turn it is
    AI::Waypoint* wp10 = nullptr;        // +0x10 where the walking squad stands (the player's; the
    AI::Waypoint* wp14 = nullptr;        // +0x14 enemy's in the aggro combats), +0x14 the other's
    AI::Waypoint* savedWp10 = nullptr;   // +0x18 the places before an enemy walked to a ranged target
    AI::Waypoint* savedWp14 = nullptr;   // +0x1c
    int state = 0;                       // +0x20
    bool spellQueued = false;            // +0x24
    Spells::SpellInfo* spell = nullptr;  // +0x28
    bool cancel = false;                 // +0x2c
    bool paused = false;                 // +0x2d
    bool completed = false;              // +0x2e
    bool killedBoss = false;             // +0x2f
    std::vector<Pair*> pairs;            // +0x30
    std::vector<Entity*> goodGuys;       // +0x3c
    std::vector<Entity*> badGuys;        // +0x48
    std::vector<Entity*> everyone;       // +0x54 told RemovedFromCombat at the end
    int f60 = 0;                         // +0x60
    int turnCount = -1;                  // +0x64 turns left (-1: no limit)
    int mode = -1;                       // +0x68
    bool f6c = false;                    // +0x6c a spell was cast this turn
    int type = 0;                        // +0x70
    int playerAggro = 0;                 // +0x74 "enemy_player_agro": % of enemy attacks at the hero
    int arrowCallback = 0;               // +0x78 the id of its helper arrow's callback
    float timeMult = 1.f;                // +0x7c
};

class AggroCombat : public BaseCombat {
public:
    AggroCombat() { type = 2; }                       // @0xf92d4
    void BeginCombat(BaseSquad* player, BaseSquad* enemy) override;   // +0x0c @0xf9038
    using BaseCombat::BeginCombat;
    // (+0x3c EndCombat @0xf9034 is BaseCombat::EndCombat again.)
    void UpdatePositions() override;                  // +0xd8 @0xf8cf8
    void UpdateAttackPosition() override;             // +0xdc @0xf8f28

    bool bossBlock = false;              // +0x80 the hero has raised his shield at a boss
    float bossBlockTime = 0.f;           // +0x84
};

class RangeAggroCombat : public BaseCombat {
public:
    RangeAggroCombat() { type = 2; }                  // @0x1f5bcc
    void BeginCombat(BaseSquad* player, BaseSquad* enemy) override;   // +0x0c @0x1f5980
    using BaseCombat::BeginCombat;
    // (+0x3c EndCombat @0x1f597c is BaseCombat::EndCombat again.)
    void UpdatePositions() override;                  // +0xd8 @0x1f57ac
    void UpdateAttackPosition() override;             // +0xdc @0x1f5870
};
