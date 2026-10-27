// BossCombat: a boss fight on the boss's own map (see Combat.h).
// Port of BossCombat (libkingdom.so 5.11), 0x11620c..0x11a250, and BossCombatWaves to 0x11c3b0. StartNewTurn (0x117950..0x118e54)
// was range-decompiled (out/big/bc_snt.c): Ghidra split it into FUN_001182b8 / FUN_00118418 (the
// STL helpers placed inside it).
//
// The music (EndCombat: "music_*" by the map's tileset, unless playing) and the sounds ("battle_boss_outro" when the boss's own side loses its leader,
// "battle_boss_intro" when the boss reaches its place).
#include "engine/SoundsManager.h"
#include "game/Combat.h"

#include <algorithm>
#include <cstdlib>

#include "engine/IconManager.h"
#include "engine/Text.h"
#include "game/AI.h"
#include "game/AIState.h"
#include "game/Animation.h"
#include "game/BRand.h"
#include "game/BuildingHovers.h"
#include "game/Entity.h"
#include "game/EntityData.h"
#include "game/EntityManager.h"
#include "game/GameState.h"
#include "game/Map.h"
#include "game/MetaExpression.h"
#include "game/Setting.h"
#include "game/Squad.h"
#include "game/StringTable.h"
#include "engine/Timer.h"
#include "hud/HUD.h"
#include "windows/Windows.h"

namespace BossCombatData {
MetaExpression* mLastBossMeta = nullptr;   // 0x6117ac
}

namespace {

// GameState::mBossFights' key of this fight: the boss's spawn point and the map it was met on.
int FightKey() { return GameState::mBossSpawnID | (int)(GameState::mReturnMapID << 16); }

}  // namespace

// @0x117870
BossCombat::BossCombat() {
    type = 3;
    turnTimer = 0.f;
    turn = 0;
}

// @0x11777c: the copied boss meta goes with the fight.
BossCombat::~BossCombat() {
    if (BossCombatData::mLastBossMeta) {
        delete BossCombatData::mLastBossMeta;
        BossCombatData::mLastBossMeta = nullptr;
    }
}

// @0x11757c: the hero and the boss stand at the map's positions 0 and 2, the boss facing 7; the fight
// is restored (or set up), then both sides run to positions 1 and 3 (RunningOut, state 0x10).
void BossCombat::BeginCombat(BaseSquad* player, BaseSquad* enemy) {
    enemySquad = enemy;
    enemyFirst = false;
    cancel = false;
    paused = false;
    pair = nullptr;
    wp10 = nullptr;
    playerSquad = player;
    player->OnBeginCombat(this);
    enemySquad->OnBeginCombat(this);
    playerSquad->DisableSpeedRun();
    const int* pos = GameState::mBossMapPositions;
    playerSquad->GetOwner()->SetPos(pos[0], pos[1]);
    enemySquad->GetOwner()->SetPos(pos[4], pos[5]);
    enemySquad->GetOwner()->SetDirection(7);
    RestoreBossCombat();
    SwitchState(0x10);
    playerSquad->MoveTo(AI::GetWaypoint(pos[2], pos[3], false));
    enemySquad->MoveTo(AI::GetWaypoint(pos[6], pos[7], false));
    playerSquad->SetCurrentTarget(enemySquad->GetOwner());
    enemySquad->SetCurrentTarget(playerSquad->GetOwner());
    Entity* boss = enemySquad->GetOwner();
    BRand::seed((uint32_t)(boss->tileY + boss->tileX + (int)boss->GetEntityData()->id + boss->GetHP() +
                           (int)GameState::playerSeed));
}

// @0x116c44: the turn window (turn number, whose turn, the player's time left), the battle bar's
// buttons, then the states; 0x10 is the run to the places.
void BossCombat::Update(float dt) {
    if (!ArenaTurnWindow::IsVisible() && !BossIntroWindow::IsVisible() && !BossOutroWindow::IsVisible() && !killedBoss)
        ArenaTurnWindow::Show();
    if (TopCityWindow::IsVisible() && !killedBoss) TopCityWindow::Hide();
    ArenaTurnWindow::SetHeader(
        SWPrintf(0x80, StringTable::GetString("ARENA_TURN_NUM"), {ToWideString(turn + 1)}).c_str());
    bool enabled;
    bool attack;
    if ((state == 0xf || state == 2) && paused) {
        const float turnTime = (float)Setting("arena_turn_time").GetInt();
        if (turnTimer < turnTime) {
            ArenaTurnWindow::SetDescription(StringTable::GetString("ARENA_TURN_PLAYER"));
            ArenaTurnWindow::SetIcon(2, IconManager::GetSandClockIcon());
            if (!BattleBarWindow::DeathTutorialActive()) turnTimer = dt + turnTimer;
            const std::u32string n = SWPrintf(0x80, U"%s", {ToWideString((int)(turnTime - turnTimer))});
            const bool hurry = turnTime - turnTimer < 5.f;
            ArenaTurnWindow::SetNumber(hurry ? 3 : 2, n.c_str(), hurry);
            ArenaTurnWindow::UpdateContents();
            enabled = paused;
            attack = false;
        } else {
            // The time is up: the fight goes on by itself.
            paused = false;
            turnTimer = 0.f;
            enabled = false;
            attack = false;
        }
    } else if (enemyFirst && !EntityManager::GetPlayer()->IsDead() && !killedBoss) {
        ArenaTurnWindow::SetIcon(1, IconManager::GetSandClockIcon());
        ArenaTurnWindow::SetDescription(StringTable::GetString("ARENA_TURN_OPPONENT"));
        ArenaTurnWindow::UpdateContents();
        enabled = paused;
        attack = true;
    } else {
        if (!enemyFirst) {
            ArenaTurnWindow::SetIcon(2, IconManager::GetSandClockIcon());
            ArenaTurnWindow::SetDescription(StringTable::GetString("ARENA_TURN_PLAYER"));
            ArenaTurnWindow::UpdateContents();
        }
        enabled = paused;
        attack = false;
    }
    BottomBattleWindow::SetEnabled(enabled, attack, false);
    BaseCombat::Update(dt);
    if (state == 0x10) RunningOut();
}

// @0x116b10: both sides at their places (the hero's squad turned to direction 4): the boss's squad
// gets its attackers and the fight waits for the player's first tap (state 1).
void BossCombat::RunningOut() {
    const int* pos = GameState::mBossMapPositions;
    if (playerSquad->IsInPosition(AI::GetWaypoint(pos[2], pos[3], false), false))
        for (unsigned i = 0; i < (unsigned)playerSquad->GetSoldierCount(); ++i) playerSquad->GetSoldier(i)->SetDirection(4);
    if (!playerSquad->IsInPosition(AI::GetWaypoint(pos[2], pos[3], false), false)) return;
    if (!enemySquad->IsInPosition(AI::GetWaypoint(pos[6], pos[7], false), false)) return;
    enemySquad->UpdateAttackers();
    SwitchState(1);
}

// @0x116aa8: the fight waits for the player (the tutorial's boss on map 0x6c moves the second
// tutorial on to 0x9f).
void BossCombat::WaitingForBossBattleClick() {
    paused = true;
    SwitchState(2);
    if (GameState::secondTutorial != 0x89 && !GameState::IsVirtualTutorial()) return;
    if (GameState::GetCurrentMapID() == 0x6c) GameState::secondTutorial = 0x9f;
}

// @0x117558: mode 0 (the player strikes first) or 1 (the boss's side first) is kept; the wait starts
// again.
void BossCombat::Continue(int mode) {
    turnTimer = 0.f;
    if (mode == 1 || mode == 0) enemyFirst = mode != 0;
    BaseCombat::Continue(mode);
}

// @0x11620c: both squads in place: the turn starts (the tutorial's step 0x9b pauses it once more).
void BossCombat::UpdatePositions() {
    if (!playerSquad->IsInPosition(wp14, false)) return;
    if (!enemySquad->IsInPosition(wp10, false)) return;
    if (pairs.empty()) StartNewTurn();
    SwitchState(9);
    if (GameState::secondTutorial == 0x9b) {
        GameState::secondTutorial = 0x9c;
        paused = true;
    }
}

// @0x117270: where the two sides meet: a free tile in the map's fighting area (positions 4, 5 its
// corner, 7, 0xe by default; 10, 11 its size), the hero's squad at its neighbour; with the boss's
// side first, at the hero's own place. (Map 0x70, the arena: in 6 x 3 tiles from 0xb, 0x17.)
void BossCombat::UpdateAttackPosition() {
    Entity* active = enemySquad->GetActiveSoldier();
    if (active->IsBoss()) {
        const int* pos = GameState::mBossMapPositions;
        if (enemySquad->GetActiveSoldier()->tileX == pos[6] && enemySquad->GetActiveSoldier()->tileY == pos[7]) {
            SoundsManager::PlaySound("battle_boss_intro", 1, false);
        }
    }
    if (!wp10) {
        AI::Waypoint* wp;
        if (GameState::GetCurrentMapID() == 0x70) {
            wp = AI::GetRandomFreeWP(6, 3, 0xb, 0x17);
            wp10 = wp;
        } else {
            const int* pos = GameState::mBossMapPositions;
            int x = pos[8];
            int y = pos[9];
            if (x == 0 || y == 0) {
                y = 0xe;
                x = 7;
            }
            const int w = pos[10] - 2;
            int half = w / 2;
            if ((pos[10] & 1) == 0) --half;
            if (half > 0) {
                for (int k = y; k != y - half; --k)
                    if ((std::abs(k) & 1) == 0) --x;   // (an odd row: +1 - 1)
                y -= half;
            }
            const int h = pos[11] - 6;
            int half2 = h / 2;
            if ((pos[11] & 1) == 0) --half2;
            if (half2 > 0) {
                for (int k = y; k != y - half2; --k)
                    if (k % 2 == 1) ++x;
                y -= half2;
            }
            wp = AI::GetRandomFreeWP(w, h, x, y);
            wp10 = wp;
            if (enemyFirst) {
                Entity* hero = playerSquad->GetOwner();
                wp10 = AI::GetWaypoint(hero->tileX, hero->tileY, false)->n[3];
                wp14 = AI::GetWaypoint(hero->tileX, hero->tileY, false);
                wp = wp10;
            }
        }
        wp14 = wp->n[2];
        if (!wp14) wp14 = wp->n[1];
    }
    playerSquad->SetAttackWP(wp10);
    playerSquad->MoveTo(wp14);
    enemySquad->SetAttackWP(wp14);
    enemySquad->MoveTo(wp10);
    SwitchState(5);
}

// @0x117950: BaseCombat's turn order, except that with the boss's side first the hero's side gets
// initiative 0 and the boss's 100.
void BossCombat::StartNewTurn() {
    for (size_t i = 0; i < goodGuys.size(); ++i) {
        Entity* g = goodGuys[i];
        if (g->IsDead()) continue;
        Pair* p = new Pair();
        p->attacker = g;
        p->initiative = g->GetInitiative(0);
        if (g->player) p->initiative = g->GetInitiative(badGuys[0]->GetEntityData()->level);
        for (Entity* b : badGuys) p->targets.push_back(b);
        if (enemyFirst) p->initiative = 0;
        pairs.push_back(p);
        g->GetAI()->AddedToCombat();
        g->GetAnimController()->mult = 0.5555556f;
    }
    for (size_t i = 0; i < badGuys.size(); ++i) {
        Entity* b = badGuys[i];
        Pair* p = new Pair();
        p->attacker = b;
        p->initiative = b->GetInitiative(0);
        for (Entity* g : goodGuys) {
            if (g->player) p->initiative = b->GetInitiative(GameState::GetLevel());
            p->targets.push_back(g);
        }
        if (enemyFirst) p->initiative = 100;
        pairs.push_back(p);
        b->GetAI()->AddedToCombat();
        b->GetAnimController()->mult = 0.5555556f;
    }
    // (std::sort with Pair_less_func, the higher initiative first; see BaseCombat::StartNewTurn)
    std::stable_sort(pairs.begin(), pairs.end(),
                     [](const Pair* a, const Pair* b) { return a->initiative > b->initiative; });
    playerSquad->OnTurnStarted();
    enemySquad->OnTurnStarted();
    f6c = false;
}

// @0x118e54: the enemies' HP are saved in the fight's record; the next turn waits for the player
// while the boss's side lives (unless a spell was cast this turn).
void BossCombat::EndTurn() {
    std::vector<std::pair<uint32_t, uint32_t>>& fight = GameState::BossFight(FightKey());
    for (unsigned i = 0; i < fight.size(); ++i) {
        if ((unsigned)enemySquad->GetSoldierCount() <= i) continue;
        fight[i].second = (uint32_t)enemySquad->GetSoldier(i)->GetHP();
    }
    ++turn;
    BaseCombat::EndTurn();
    if (enemySquad && enemySquad->GetAliveSoldierCount() != 0 && !f6c) paused = true;
}

// @0x116800: an enemy died: the player's squad hears of it and the fight goes on.
void BossCombat::OnEnemyDied() {
    if (enemySquad->GetActiveSoldier() == enemySquad->GetOwner()) {
        SoundsManager::PlaySound("battle_boss_outro", 1, false);
    }
    wp10 = nullptr;
    playerSquad->OnEnemyDied();
    Pause(nullptr);
}

// @0x11688c: the boss's side all dead: the boss is beaten (the outro); else its meta goes back to its
// own. On the arena map (0x70) the arena's statistics are kept instead of the outro, and the hero
// taps the arena's gate (the decoration at 0xf, 0x17) to leave.
void BossCombat::EndCombat() {
    // The map's music (tileset 0, 1, 2), unless it plays already.
    const char* music = Map::GetTileset() == 0 ? "music_battle_map"
                      : Map::GetTileset() == 1 ? "music_winter"
                      : Map::GetTileset() == 2 ? "desert/sand_theme" : nullptr;
    if (music && !SoundsManager::IsMusicPlaying(music)) SoundsManager::PlayMusic(music, true);
    bool arenaWon = false;
    if (enemySquad && enemySquad->GetAliveSoldierCount() == 0) {
        killedBoss = true;
        if (GameState::GetCurrentMapID() == 0x70) {
            GameState::AddArena1WinByBoss(enemySquad->GetOwner()->GetEntityData()->id);
            GameState::AddArena1WinByLevel((unsigned)GameState::GetLevel());
            arenaWon = true;
        } else {
            BossOutroWindow::Show();
        }
    }
    if (enemySquad && enemySquad->GetAliveSoldierCount() != 0) {
        enemySquad->GetOwner()->SetForeignMetaExpression(nullptr);
        if (GameState::GetCurrentMapID() == 0x70) {
            GameState::AddEventFail(1);
            GameState::AddArena1DeathByBoss(enemySquad->GetOwner()->GetEntityData()->id);
            GameState::AddArena1LossByLevel((unsigned)GameState::GetLevel());
        }
    }
    BaseCombat::EndCombat();
    if (arenaWon) {
        Entity* player = EntityManager::GetPlayer();
        if (player)
            if (Map::Decor* d = Map::GetDecoration(0xf, 0x17)) {
                player->GetAI()->AutoInteraction();
                BuildingHovers::AllowDecorClickInBattleOnce();
                BuildingHovers::OnDecorClick(d, 0, 0);
            }
    }
    if (killedBoss) completed = false;
    ArenaTurnWindow::Hide();
}

// @0x119100: the fight is set up from its record in GameState::mBossFights, or a new record is made.
// A record older than the boss's restore_time is dropped first. With a record the boss gets its HP
// back and its living followers come back with theirs; else the followers of mBossWaves come (their
// HP from mBossWavesHP when set). Then both squads take their places, the outro learns the boss's
// name and the boss gets the copied meta of SwitchToBossFight.
void BossCombat::RestoreBossCombat() {
    const int key = FightKey();
    const uint32_t started = GameState::GetBossFightTimer(key);
    if (started != 0 && enemySquad->GetOwner()->GetEntityData()->restoreTime < (int)((uint32_t)Timer::GetGlobalTime() - started)) {
        GameState::EraseBossFightTimer(key);
        GameState::EraseBossFight(key);
    }
    if (GameState::HasBossFight(key)) {
        for (unsigned i = 0; i < GameState::BossFight(key).size(); ++i) {
            if (i == 0) {
                enemySquad->GetOwner()->SetHP((int)GameState::BossFight(key)[0].second);
                continue;
            }
            if (GameState::BossFight(key)[i].second == 0) continue;
            Entity* e = EntityManager::CreateEntity(GameState::BossFight(key)[i].first, false, true);
            enemySquad->AddSoldier(e);
            e->SetHP((int)GameState::BossFight(key)[i].second);
        }
        if (!GameState::HasBossFightTimer(key)) GameState::SetBossFightTimer(key, (uint32_t)Timer::GetGlobalTime());
    } else {
        Entity* boss = enemySquad->GetOwner();
        GameState::BossFight(key) = {{boss->GetEntityData()->id, (uint32_t)boss->GetHP()}};
        const size_t n = GameState::mBossWaves.size();
        if (n != 1) {
            for (size_t i = 0; i < n - 1; ++i) {
                Entity* e = EntityManager::CreateEntity(GameState::mBossWaves[i], false, true);
                enemySquad->AddSoldier(e);
                e->f75 = true;
                if (const int hp = GameState::mBossWavesHP[i]) {
                    e->SetHpMax(hp);
                    e->SetHP(GameState::mBossWavesHP[i]);
                }
                GameState::BossFight(key).push_back({e->GetEntityData()->id, (uint32_t)e->GetHP()});
            }
        }
        if (!GameState::HasBossFightTimer(key)) GameState::SetBossFightTimer(key, (uint32_t)Timer::GetGlobalTime());
    }
    Entity* hero = playerSquad->GetOwner();
    playerSquad->SpawnAt(AI::GetWaypoint(hero->tileX, hero->tileY, false));
    Entity* boss = enemySquad->GetOwner();
    enemySquad->SpawnAt(AI::GetWaypoint(boss->tileX, boss->tileY, false));
    BossOutroWindow::SetBossName(boss->GetEntityData()->title, boss->GetEntityData()->id);
    if (BossCombatData::mLastBossMeta) {
        enemySquad->GetOwner()->SetForeignMetaExpression(BossCombatData::mLastBossMeta);
        enemySquad->GetOwner()->OnSetup();
    }
}

// ------------------------------------------------------------------------------- BossCombatWaves

// @0x11a474
BossCombatWaves::BossCombatWaves() { type = 4; }

BossCombatWaves::~BossCombatWaves() = default;   // @0x11a514 (the waves go; BossCombat's goes after)

// @0x11a254: as BossCombat's, but the boss's side comes as a formation: its squad takes its place
// around the boss and the boss is ready at once.
void BossCombatWaves::BeginCombat(BaseSquad* player, BaseSquad* enemy) {
    enemySquad = enemy;
    cancel = false;
    paused = false;
    pair = nullptr;
    wp10 = nullptr;
    playerSquad = player;
    player->OnBeginCombat(this);
    enemySquad->OnBeginCombat(this);
    playerSquad->DisableSpeedRun();
    const int* pos = GameState::mBossMapPositions;
    playerSquad->GetOwner()->SetPos(pos[0], pos[1]);
    enemySquad->GetOwner()->SetPos(pos[4], pos[5]);
    enemySquad->GetOwner()->SetDirection(7);
    RestoreBossCombat();
    SwitchState(0x10);
    playerSquad->MoveTo(AI::GetWaypoint(pos[2], pos[3], false));
    enemySquad->MoveSquadTo(AI::GetWaypoint(pos[6], pos[7], false));
    enemySquad->SetReady(enemySquad->GetOwner());
    playerSquad->SetCurrentTarget(enemySquad->GetOwner());
    enemySquad->SetCurrentTarget(playerSquad->GetOwner());
    Entity* boss = enemySquad->GetOwner();
    BRand::seed((uint32_t)(boss->tileY + boss->tileX + (int)boss->GetEntityData()->id + boss->GetHP() +
                           (int)GameState::playerSeed));
}

namespace {
// How many of the boss's side (not counting the boss) are dead.
size_t DeadFollowers(BaseSquad* squad) {
    size_t dead = 0;
    const std::vector<Entity*>& soldiers = squad->GetSoldiers();
    for (size_t i = 1; i < soldiers.size(); ++i)
        if (squad->GetSoldier((unsigned)i)->IsDead()) ++dead;
    return dead;
}
}  // namespace

// @0x11a5e0: the wave in play is dead: it leaves, the next one is led in (the boss, after the last
// wave) and the fight goes on; else as BossCombat's.
void BossCombatWaves::WaitingForContinue() {
    GameState::SetCurrentLocation(4);
    const size_t dead = DeadFollowers(enemySquad);
    if (wave < (int)waves.size() && dead == waves[(size_t)wave].enemies.size()) {
        ++wave;
        for (size_t n = enemySquad->GetSoldiers().size() - 1; n != 0; --n)
            enemySquad->RemoveSoldier(enemySquad->GetSoldier((unsigned)n));
        if (wave != (int)waves.size())
            for (Entity* e : waves[(size_t)wave].enemies) enemySquad->AddSoldier(e);
        enemySquad->SetAttackWP(nullptr);
        enemySquad->SetCurrentTarget(nullptr);
        const int* pos = GameState::mBossMapPositions;
        enemySquad->SpawnSquadAt(AI::GetWaypoint(pos[4], pos[5], false));
        SwitchState(0x10);
        if (wave == (int)waves.size()) {
            enemySquad->MoveTo(AI::GetWaypoint(pos[6], pos[7], false));
        } else {
            enemySquad->MoveSquadTo(AI::GetWaypoint(pos[6], pos[7], false));
            enemySquad->SetReady(enemySquad->GetOwner());
        }
        enemySquad->SetCurrentTarget(playerSquad->GetOwner());
        return;
    }
    BossCombat::WaitingForContinue();
}

// @0x11a888: once the wave in play is dead the fight is not paused any more.
void BossCombatWaves::OnEnemyDied() {
    BossCombat::OnEnemyDied();
    if (wave < (int)waves.size() && DeadFollowers(enemySquad) == waves[(size_t)wave].enemies.size()) paused = false;
}

// @0x11acec (to 0x11c3b0; Ghidra split off its tail at @0x11b17c as "_INIT_3", and FUN_0011aa84 /
// FUN_0011a924 are the inlined mBossFightsTimers insert: SetBossFightTimer): as BossCombat's, but the
// followers of mBossWaves are sorted into waves: those before the first 0 stand with the boss at once,
// each later group waits in its wave (the record keeps them all). A fight that was recorded already
// comes back with its living followers and no waves.
void BossCombatWaves::RestoreBossCombat() {
    const int key = FightKey();
    const uint32_t started = GameState::GetBossFightTimer(key);
    if (started != 0 && enemySquad->GetOwner()->GetEntityData()->restoreTime < (int)((uint32_t)Timer::GetGlobalTime() - started)) {
        GameState::EraseBossFightTimer(key);
        GameState::EraseBossFight(key);
    }
    if (GameState::HasBossFight(key)) {
        for (unsigned i = 0; i < GameState::BossFight(key).size(); ++i) {
            if (i == 0) {
                enemySquad->GetOwner()->SetHP((int)GameState::BossFight(key)[0].second);
                continue;
            }
            if (GameState::BossFight(key)[i].second == 0) continue;
            Entity* e = EntityManager::CreateEntity(GameState::BossFight(key)[i].first, false, true);
            enemySquad->AddSoldier(e);
            e->SetHP((int)GameState::BossFight(key)[i].second);
        }
    } else {
        Entity* boss = enemySquad->GetOwner();
        GameState::BossFight(key) = {{boss->GetEntityData()->id, (uint32_t)boss->GetHP()}};
        wave = 0;
        const size_t n = GameState::mBossWaves.size();
        if (n != 1) {
            for (size_t i = 0; i < n - 1; ++i) {
                if (GameState::mBossWaves[i] == 0) {
                    ++wave;
                    continue;
                }
                Entity* e = EntityManager::CreateEntity(GameState::mBossWaves[i], false, true);
                if (wave == 0) enemySquad->AddSoldier(e);
                e->f75 = true;
                if (const int hp = GameState::mBossWavesHP[i]) {
                    e->SetHpMax(hp);
                    e->SetHP(GameState::mBossWavesHP[i]);
                }
                GameState::BossFight(key).push_back({e->GetEntityData()->id, (uint32_t)e->GetHP()});
                if (wave == (int)waves.size()) waves.push_back({(unsigned)i, {e}});
                else waves[(size_t)wave].enemies.push_back(e);
            }
        }
        wave = 0;
    }
    if (!GameState::HasBossFightTimer(key)) GameState::SetBossFightTimer(key, (uint32_t)Timer::GetGlobalTime());
    Entity* hero = playerSquad->GetOwner();
    playerSquad->SpawnAt(AI::GetWaypoint(hero->tileX, hero->tileY, false));
    Entity* boss = enemySquad->GetOwner();
    enemySquad->SpawnAt(AI::GetWaypoint(boss->tileX, boss->tileY, false));
    BossOutroWindow::SetBossName(boss->GetEntityData()->title, boss->GetEntityData()->id);
    if (BossCombatData::mLastBossMeta) {
        enemySquad->GetOwner()->SetForeignMetaExpression(BossCombatData::mLastBossMeta);
        enemySquad->GetOwner()->OnSetup();
    }
}
