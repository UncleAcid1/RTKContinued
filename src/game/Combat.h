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
#include <cstdint>
#include <vector>

class BaseSquad;
class Entity;
namespace AI { struct Waypoint; }
namespace Spells { struct SpellInfo; }

class BaseCombat {
public:
    // Pair1vs1 (0x28 bytes): one duel of an arena turn (ArenaCombat).
    struct Pair1vs1 {
        Entity* attacker = nullptr;      // +0x00
        AI::Waypoint* attackerPos = nullptr;   // +0x04 where the attacker stands to strike
        AI::Waypoint* targetPos = nullptr;     // +0x08
        int f0c = 0, f10 = 0;            // +0x0c +0x10
        Entity* target = nullptr;        // +0x14
        bool started = false;            // +0x18 the blow (or the counter-blow) has begun
        bool ready = false;              // +0x19 both stand in place
        int initiative = 0;              // +0x1c
        int state = 0;                   // +0x20 0 the blow, 1 the counter-blow, 2 over
        bool skip = false;               // +0x24
    };
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
    // +0x24 @0x10c3dc: the spell (its id) the hero casts next; x, y: an area spell's tile (the arenas).
    virtual void QueueSpellCast(unsigned s, int, int) { spell = s; spellQueued = true; }
    virtual void Continue(int mode);                  // +0x28 @0x10cf78 (the arenas' modes: ArenaCombat)
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
    unsigned spell = 0;                  // +0x28 the queued spell
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

class MetaExpression;
namespace BossCombatData {
extern MetaExpression* mLastBossMeta;   // 0x6117ac the boss's meta, copied by SwitchToBossFight
}

// BossCombat (@0x11620c..0x11a250, vtable 0x607ea8, 0x8c bytes): a boss fight on the boss's own map
// (Entity::GetBossMapID), set up by BaseCombat::SwitchToBossFight and Map::SpawnEntities. The
// squads run to their places (RunningOut, state 0x10), then turns as in BaseCombat; between turns
// the fight waits for the player (paused) up to "arena_turn_time" seconds, shown in
// ArenaTurnWindow. Its enemies and their HP are kept in GameState::mBossFights (RestoreBossCombat,
// EndTurn), so a fight left half way goes on where it stopped until the boss's restore_time.
class BossCombat : public BaseCombat {
public:
    BossCombat();                                     // @0x117870
    ~BossCombat() override;                           // @0x11777c
    void Update(float dt) override;                   // +0x08 @0x116c44
    void BeginCombat(BaseSquad* player, BaseSquad* enemy) override;   // +0x0c @0x11757c
    using BaseCombat::BeginCombat;
    void Continue(int mode) override;                 // +0x28 @0x117558
    void EndCombat() override;                        // +0x3c @0x11688c
    void OnEnemyDied() override;                      // +0x68 @0x116800
    void WaitingForContinue() override { BaseCombat::WaitingForContinue(); }   // +0xa8 @0x116b0c
    void WaitingForBossBattleClick() override;        // +0xac @0x116aa8
    void UpdatePositions() override;                  // +0xd8 @0x11620c
    void UpdateAttackPosition() override;             // +0xdc @0x117270
    void StartNewTurn() override;                     // +0xe4 @0x117950
    void EndTurn() override;                          // +0xe8 @0x118e54
    virtual void RestoreBossCombat();                 // +0xf4 @0x119100
    virtual void RunningOut();                        // +0xf8 @0x116b10

    bool enemyFirst = false;             // +0x80 Continue(1): the boss's side strikes first
    float turnTimer = 0.f;               // +0x84 the wait for the player
    int turn = 0;                        // +0x88
};

// BossCombatWaves (0x9c bytes, type 4): a boss fight whose enemies come in waves. The first wave stands
// with the boss; when it is dead (WaitingForContinue) the next one is led in. The waves are the groups of
// mBossWaves between its 0 entries; the boss comes last.
class BossCombatWaves : public BossCombat {
public:
    BossCombatWaves();                                // @0x11a474 (also @0x11a4c4)
    ~BossCombatWaves() override;                      // @0x11a514 (deleting @0x11ac04, @0x11acd0)
    void BeginCombat(BaseSquad* player, BaseSquad* enemy) override;   // +0x0c @0x11a254
    using BaseCombat::BeginCombat;
    void OnEnemyDied() override;                      // +0x68 @0x11a888
    void WaitingForContinue() override;               // +0xa8 @0x11a5e0
    void RestoreBossCombat() override;                // +0xf4 @0x11acec

    struct Wave {
        unsigned index;                  // the position in mBossWaves of its first enemy
        std::vector<Entity*> enemies;
    };
    int wave = 0;                        // +0x8c the wave in play
    std::vector<Wave> waves;             // +0x90 the waves after the boss's own side
};

namespace PvP { struct PlayerInfo; }

// ArenaCombat (@0xfa030..0xfee54, vtable 0x6077f0, 0xc0 bytes): the arenas' turn-based fight. The
// two sides take turns (playerTurn); in each the side's fighters are paired 1 to 1 with the other
// side's (WaitingForContinue), walk to striking distance (UpdateAttackPosition, UpdatePositions),
// strike and get struck back (CombatReady -> PlayerTurn / EnemyTurn). The player picks the stance
// (attack / block) on the battle bar before his turn, within the turn time.
// States beyond BaseCombat's: 3 the wave is cleared, 0xd the next wave runs in, 0xe completed,
// 0xf a spell's area is being chosen, 0x10 running out.
class ArenaCombat : public BaseCombat {
public:
    ArenaCombat();                                    // @0xfbf20
    ~ArenaCombat() override;                          // @0xfe318
    void Update(float dt) override;                   // +0x08 @0xfb2a8
    void BeginCombat(BaseSquad* player, BaseSquad* enemy) override;   // +0x0c @0xfb184
    using BaseCombat::BeginCombat;
    void Pause(Entity*) override {}                   // +0x14 @0xfa030
    void PlayerDied() override;                       // +0x18 @0xfa180
    void QueueSpellCast(unsigned s, int x, int y) override;   // +0x24 @0xfa21c
    void Continue(int mode) override;                 // +0x28 @0xfc698
    void AddGoodGuy(Entity* e) override;              // +0x2c @0xfe4cc
    void PostCombat() override;                       // +0x38 @0xfa128
    void EndCombat() override;                        // +0x3c @0xfc8ac
    bool CanSpellCastNow() override;                  // +0x70 @0xfc430
    AI::Waypoint* GetSpellEpicenter() override { return spellEpicenter; }   // +0x74 @0xfa0e0
    Entity* GetBoss() override { return boss; }       // +0x84 @0xfa100
    void SpellAreaSelectionStarted() override;        // +0x8c @0xfa108
    void SpellAreaSelectionEnded() override;          // +0x90 @0xfa1a8
    int GetCurrentWave() override { return wave; }    // +0x94 @0xfa0ac
    int GetWaveCount() override { return waveCount; }   // +0x98 @0xfa0b4
    void AddSpentMoney(int gold, int crystals) override;   // +0x9c @0xfa858
    int GetSpentGold() override { return spentGold; } // +0xa0 @0xfa0bc
    void WaitingForContinue() override;               // +0xa8 @0xfee54
    void StartPositioning() override;                 // +0xb0 @0xfa054
    void CombatReady() override;                      // +0xb4 @0xfc44c
    void CheckSpellCast() override { SwitchState(6); }   // +0xcc @0xfa08c
    void SpellCast() override;                        // +0xd0 @0xfa0c4
    void UpdatePositions() override;                  // +0xd8 @0xfbb1c
    void UpdateAttackPosition() override;             // +0xdc @0xfc110
    virtual void ArenaCompleted();                    // +0xf4 @0xfaa1c
    virtual void EnemyTurn(Pair1vs1* p);              // +0xf8 @0xfae54
    virtual void PlayerTurn(Pair1vs1* p);             // +0xfc @0xfab6c
    virtual void PlayerSpellCast(Pair1vs1* p);        // +0x100 @0xfa550
    virtual void RunningOut();                        // +0x104 @0xfa034
    virtual void WaveCleared();                       // +0x108 @0xfaae0
    virtual void UpdateRunningToPos();                // +0x10c @0xfa90c
    virtual void SpawnEnemies();                      // +0x110 @0xfdec0
    virtual void SpawnPlayer();                       // +0x114 @0xfdb70
    virtual void RunToSpawnPos();                     // +0x118 @0xfd884
    virtual void SetupWave();                         // +0x11c @0xfcae0
    virtual void SortPairs();                         // +0x120 @0xfe6e4
    static void ClearArenaWPs();                      // +0x124 @0xfbc18
    static void DisableOuterWPs();                    // +0x128 @0xfbd94
    void SetWaveCount(int n) { waveCount = n; }       // @0xfa0a4
    void SkipCombat();                                // @0xfa0e8
    void BlockStance();                               // @0xfa328
    void AttackStance();                              // @0xfa69c
    void AutoCombat();                                // @0xfa80c

    bool playerTurn = true;              // +0x80
    int turn = 1;                        // +0x84 half-turns ((turn + 1) / 2 is shown)
    int turnSteps = 0;                   // +0x88
    bool allAtOnce = true;               // +0x8c every ready duel strikes in the same step
    bool counterBlows = true;            // +0x8d
    int wave = 0;                        // +0x90
    int waveCount = 3;                   // +0x94
    bool autoCombat = false;             // +0x98
    int spentGold = 0;                   // +0x9c
    Entity* boss = nullptr;              // +0xa0 the enemy squad's owner
    std::vector<Pair1vs1*> duels;        // +0xa4
    float turnTimer = 0.f;               // +0xb0 the player's time to choose
    int stance = 0;                      // +0xb4 0 attack, 1 block
    uint32_t eventStart = 0;             // +0xb8
    AI::Waypoint* spellEpicenter = nullptr;   // +0xbc
};

// PvPArenaCombat (@0x1f31dc..0x1f4da4, vtable 0x608b28, 200 bytes): the PvP arena: the opponent's
// hero and warriors (mBossWaves), the tutorial's ambush dialogue and the valor won.
class PvPArenaCombat : public ArenaCombat {
public:
    PvPArenaCombat(PvP::PlayerInfo* opponent, bool secondWave);   // @0x1f46a4
    ~PvPArenaCombat() override;                       // @0x1f4540
    void Update(float dt) override;                   // +0x08 @0x1f37e8
    void BeginCombat(BaseSquad* player, BaseSquad* enemy) override;   // +0x0c @0x1f3690
    using BaseCombat::BeginCombat;
    void PlayerDied() override;                       // +0x18 @0x1f34cc
    void PostCombat() override;                       // +0x38 @0x1f3388
    void EndCombat() override;                        // +0x3c @0x1f4c6c
    void OnValorReceived(unsigned valor) override;    // +0xa4 @0x1f31dc
    void ArenaCompleted() override;                   // +0xf4 @0x1f3524
    void WaveCleared() override;                      // +0x108 @0x1f3470
    void SpawnEnemies() override;                     // +0x110 @0x1f4814
    void SetupWave() override;                        // +0x11c @0x1f4da4

    PvP::PlayerInfo* opponent = nullptr; // +0xc0
    bool waveSet = false;                // +0xc4 the first wave has been set up
    bool waitingForServer = false;       // +0xc5 (the valor's online report)
};
