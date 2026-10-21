// ArenaCombat and PvPArenaCombat (see Combat.h). Port of libkingdom.so 5.11 @0xfa030..0xfee54 and
// @0x1f31dc..0x1f4da4.
//
// UNVERIFIED (milestone 6): the sounds.
#include "engine/SoundsManager.h"
#include "game/Combat.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "engine/IconManager.h"
#include "engine/Render.h"
#include "engine/Timer.h"
#include "game/AI.h"
#include "game/AIState.h"
#include "game/Animation.h"
#include "game/BuildingHovers.h"
#include "game/Entity.h"
#include "game/EntityData.h"
#include "game/EntityManager.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/Map.h"
#include "game/MetaData.h"
#include "game/MetaExpression.h"
#include "game/PvP.h"
#include "game/Rand48.h"
#include "game/Setting.h"
#include "game/SpellMovement.h"
#include "game/Spells.h"
#include "game/Squad.h"
#include "game/StringTable.h"
#include "game/Tasks.h"
#include "hud/HUD.h"
#include "windows/Windows.h"

namespace {

// The formations the squads stand in (rows of 5 tiles, two rows; 2 the owner, 1 a soldier), by
// the squad's size: the player's (0x579688) and the PvP opponent's (0x587554).
const uint8_t kPlayerFormation[7][10] = {
    {0, 0, 2, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 2, 0, 0, 0, 0, 1, 0, 0}, {0, 1, 2, 1, 0, 0, 0, 0, 0, 0},
    {0, 1, 2, 1, 0, 0, 0, 1, 0, 0}, {0, 1, 2, 1, 0, 0, 1, 0, 1, 0}, {0, 1, 2, 1, 0, 0, 1, 1, 1, 0},
    {1, 1, 2, 1, 1, 0, 1, 1, 1, 0}};
// The arena waves' (0x5796ce: its rows start at a squad of 2, the first is the zero gap before
// it, so a lone owner finds no place, as in the original).
const uint8_t kWaveFormation[8][10] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 1, 0, 0, 0, 0, 2, 0, 0}, {0, 0, 0, 0, 0, 0, 1, 2, 1, 0},
    {0, 0, 1, 0, 0, 0, 1, 2, 1, 0}, {0, 1, 0, 1, 0, 0, 1, 2, 1, 0}, {0, 1, 1, 1, 0, 0, 1, 2, 1, 0},
    {1, 0, 1, 1, 1, 0, 1, 2, 1, 0}, {1, 1, 1, 1, 1, 0, 1, 2, 1, 0}};
const uint8_t kOpponentFormation[7][10] = {
    {0, 0, 0, 0, 0, 0, 0, 2, 0, 0}, {0, 0, 1, 0, 0, 0, 0, 2, 0, 0}, {0, 0, 0, 0, 0, 0, 1, 2, 1, 0},
    {0, 0, 1, 0, 0, 0, 1, 2, 1, 0}, {0, 1, 0, 1, 0, 0, 1, 2, 1, 0}, {0, 1, 1, 1, 0, 0, 1, 2, 1, 0},
    {0, 1, 1, 1, 1, 0, 1, 2, 1, 0}};

// The formation's table entry for squad member `i` at cell k (a squad of `count`).
uint8_t Cell(const uint8_t (*table)[10], int count, int k) { return table[count - 1][k]; }

// The squad's members take the free tiles of their formation, rows of 5 diagonal steps from
// (x, y); `place` puts member i on (x, y). Each placed tile gets weight 1000.
template <class Place>
void PlaceFormation(BaseSquad* squad, const uint8_t (*table)[10], int startX, int startY, Place place) {
    for (unsigned i = 0; i < (unsigned)squad->GetSoldierCount(); ++i) {
        int rowX = startX, rowY = startY, col = 0;
        for (;;) {
            int k = col, x = rowX, y = rowY;
            bool placed = false;
            for (;;) {
                AI::Waypoint* wp = AI::GetWaypoint(x, y, false);
                // PORT: the original reads the weight of a tile outside the map (no waypoint) too.
                if (wp && wp->weight <= 1.f) {
                    Entity* e = squad->GetSoldier(i);
                    const bool owner = e == squad->GetOwner();
                    const uint8_t v = Cell(table, squad->GetSoldierCount(), k);
                    if ((owner && v == 2) || (!owner && v == 1)) {
                        place(e, x, y);
                        wp->weight = 1000.f;
                        placed = true;
                        break;
                    }
                }
                ++k;
                if (y % 2 == 1) ++x;
                if (y == rowY + 4) break;
                ++y;
            }
            if (std::abs(rowY) & 1) ++rowX;
            if (col == 5) break;
            col += 5;
            if (placed) break;
            --rowX;
            ++rowY;
        }
    }
}

// Where the player's formation starts (mBossMapPositions [2], [3]).
void PlayerFormationStart(int& x, int& y) {
    const int px = GameState::mBossMapPositions[2], py = GameState::mBossMapPositions[3];
    x = px;
    if (std::abs(py) & 1) ++x;
    --x;
    y = py - 2;
    if ((std::abs(py - 1) & 1) == 0) --x;
}

// The "idle" posture after a stance change: anyone in a victory pose turns to the front.
void LeaveVictoryPose(BaseSquad* squad, unsigned i) {
    Entity* e = squad->GetSoldier(i);
    AnimationController* ac = e->GetAnimController();
    if (ac && ac->anim && std::strstr(ac->anim->name.c_str(), "vict")) {
        e->SetDirection(3);
        e->SetAnimationP("idle_1", true, false, false);
    }
}

bool Busy(Entity* e) { return e->GetAI()->GetState() != 0; }

}  // namespace

// ------------------------------------------------------------------------------------ ArenaCombat

ArenaCombat::ArenaCombat() {
    wave = 0;
    eventStart = 0x1571;
    allAtOnce = true;
    type = 5;
    playerTurn = true;
    const uint32_t difficulty = GameState::GetEventDifficulty(2);
    if (difficulty == 0) waveCount = 3;
    else if (GameState::GetEventDifficulty(2) == 1) waveCount = 4;
    else waveCount = 5;
    turnTimer = 0.f;
    turnSteps = 0;
    spellEpicenter = nullptr;
    stance = 0;
    autoCombat = false;
    if (GameState::arenaBotActive) waveCount = 35000;
    turn = 1;
    counterBlows = true;
    eventStart = GameState::GetEventStartTime(2);
    DisableOuterWPs();
    spentGold = 0;
}

// The waves' squad lets its soldiers go; the boss's copied meta is freed.
ArenaCombat::~ArenaCombat() {
    if (enemySquad) {
        enemySquad->RemoveAllSoldiers();
        enemySquad = nullptr;
    }
    delete BossCombatData::mLastBossMeta;
    BossCombatData::mLastBossMeta = nullptr;
    for (Pair1vs1* p : duels) delete p;
}

void ArenaCombat::RunningOut() {
    paused = true;
    SwitchState(2);
}

void ArenaCombat::StartPositioning() {
    if (!cancel) {
        UpdateAttackPosition();
        return;
    }
    cancel = false;
    EndCombat();
}

void ArenaCombat::SpellCast() { PlayerSpellCast(duels[0]); }

void ArenaCombat::SkipCombat() {
    timeMult = 10.f;
    autoCombat = true;
}

void ArenaCombat::SpellAreaSelectionStarted() {
    paused = true;
    SwitchState(0xf);
}

void ArenaCombat::PostCombat() {
    if (!enemySquad) return;
    enemySquad->OnEndCombat();
    enemySquad->RemoveAllSoldiers();
    enemySquad = nullptr;
}

void ArenaCombat::PlayerDied() {
    SwitchState(0xe);
    if (BottomBattleWindow::IsVisible()) BottomBattleWindow::Hide();
}

// The aim of an area spell is over: the bar comes back while the turn time lasts.
void ArenaCombat::SpellAreaSelectionEnded() {
    paused = true;
    if (turnTimer < (float)Setting("arena_turn_time").GetInt()) BottomBattleWindow::Show();
    SwitchState(2);
}

// The hero casts `s` (at tile x, y for an area spell) in the next step.
void ArenaCombat::QueueSpellCast(unsigned s, int x, int y) {
    turnTimer = 0.f;
    spell = s;
    spellQueued = true;
    for (unsigned i = 0; i < (unsigned)playerSquad->GetSoldierCount(); ++i) {
        Entity* e = playerSquad->GetSoldier(i);
        AnimationController* ac = e->GetAnimController();
        if (ac && ac->anim && std::strstr(ac->anim->name.c_str(), "vict")) {
            e->SetDirection(3);
            e->SetAnimationP("idle_1", true, false, false);
        }
    }
    paused = false;
    SwitchState(2);
    spellEpicenter = AI::GetWaypoint(x, y, false);
}

// Block: more defence (160 when the arena event was never won and this is not PvP, or for the
// test bot; else 30), half the fury gain, no extra critical chance.
void ArenaCombat::BlockStance() {
    turnTimer = 0.f;
    turnSteps = 0;
    playerTurn = false;
    stance = 1;
    if (!autoCombat) BottomBattleWindow::SetSelectedButton(1);
    const bool easy = GameState::GetEventCompletionCount(2) == 0 && !GameState::IsPvPCombatActive();
    const int defence = (GameState::arenaBotActive || easy) ? 0xa0 : 0x1e;
    for (unsigned i = 0; i < (unsigned)playerSquad->GetSoldierCount(); ++i) {
        LeaveVictoryPose(playerSquad, i);
        Entity* e = playerSquad->GetSoldier(i);
        e->SetAdditiveFuryGain(0.5f);
        e->SetAdditiveCrit(0);
        e->SetAdditiveDefence(defence);
    }
}

// Spell turn: the hero aims (area spells) and casts; with every enemy idle the duel is over and
// the stance becomes Block.
void ArenaCombat::PlayerSpellCast(Pair1vs1* p) {
    if (!p->started) {
        if (p->attacker->GetAI()->GetState() != 0) return;
        p->attacker->GetAI()->StartedMagicAttackArea(spellEpicenter);
        p->attacker->GetAI()->MagicAttackArea(spell);
        p->started = true;
        return;
    }
    unsigned idle = 0;
    for (unsigned i = 0; i < (unsigned)enemySquad->GetSoldierCount(); ++i)
        if (enemySquad->GetSoldier(i)->GetAI()->GetState() == 0) ++idle;
    if (p->attacker->GetAI()->GetState() != 0) return;
    if ((unsigned)enemySquad->GetSoldierCount() != idle) return;
    p->attacker->GetAI()->MagicAttackAreaFinished(spell);
    p->state = 2;
    BlockStance();
    turnSteps = 1;
    SwitchState(2);
}

// Attack: more fury gain and critical chance, less defence (-30; 160 when easy, as for Block).
void ArenaCombat::AttackStance() {
    turnTimer = 0.f;
    playerTurn = true;
    turnSteps = 0;
    stance = 0;
    if (!autoCombat) BottomBattleWindow::SetSelectedButton(0);
    bool easy = GameState::GetEventCompletionCount(2) == 0 && !GameState::IsPvPCombatActive();
    if (GameState::arenaBotActive) easy = true;
    for (unsigned i = 0; playerSquad && i < (unsigned)playerSquad->GetSoldierCount(); ++i) {
        LeaveVictoryPose(playerSquad, i);
        Entity* e = playerSquad->GetSoldier(i);
        e->SetAdditiveFuryGain(1.5f);
        e->SetAdditiveCrit(10);
        e->SetAdditiveDefence(easy ? 0xa0 : -0x1e);
    }
}

// The fight runs on by itself (the bar's Auto): a random stance.
void ArenaCombat::AutoCombat() {
    autoCombat = !autoCombat;
    if (autoCombat) BottomBattleWindow::SetSelectedButton(3);
    if (Rand48::lrand48() & 1) BlockStance();
    else AttackStance();
}

// The arena's refunds: gold as given, crystals at "exchange_gold_dropback" gold each.
void ArenaCombat::AddSpentMoney(int gold, int crystals) {
    if (gold == 0) {
        if (crystals != 0 && Setting("exchange_on_dropback").GetInt() == 1)
            spentGold += (int)ceilf((float)crystals / Setting("exchange_gold_dropback").GetFloat());
    } else {
        spentGold += gold;
    }
}

// The player's fighters ran back to their places: they face the front and the next wave comes.
void ArenaCombat::UpdateRunningToPos() {
    int ready = 0;
    for (unsigned i = 0; i < (unsigned)playerSquad->GetSoldierCount(); ++i) {
        if (playerSquad->GetSoldier(i)->IsDead()) continue;
        if (playerSquad->GetSoldier(i)->GetAI()->path.empty()) ++ready;
    }
    if (ready != playerSquad->GetAliveSoldierCount()) return;
    for (unsigned i = 0; i < (unsigned)playerSquad->GetSoldierCount(); ++i) playerSquad->GetSoldier(i)->SetDirection(3);
    enemySquad->RemoveAllSoldiers();
    SetupWave();
}

// @0xfaa1c the arena event's end: its statistics, and on a win the outro.
void ArenaCombat::ArenaCompleted() {
    if (BottomBattleWindow::IsVisible()) return;
    const uint32_t difficulty = GameState::GetEventDifficulty(2);
    const uint32_t id = boss->GetEntityData()->id;
    if (playerSquad->GetOwner()->IsDead()) {
        GameState::AddEventFail(2);
        GameState::AddArena2DeathByBoss(id);
        GameState::AddArena2DeathByDifficulty(difficulty);
        GameState::AddArena2Difficulty(difficulty);
        GameState::AddArena2LossByLevel((unsigned)GameState::GetLevel());
    } else {
        GameState::AddArena2WinByBoss(id);
        GameState::AddArena2WinByDifficulty(difficulty);
        GameState::AddArena2Difficulty(difficulty);
        GameState::AddArena2WinByLevel((unsigned)GameState::GetLevel());
        BossOutroWindow::Show();
    }
    EndCombat();
}

// A wave is beaten: the next one (Continue(7)), or the end.
void ArenaCombat::WaveCleared() {
    timeMult = 1.f;
    SwitchState(2);
    if (wave != waveCount) {
        SoundsManager::PlaySound("ui_campaign_completed", 1, false);
        GameState::GetActiveCombat()->Continue(7);
        return;
    }
    SwitchState(0xe);
    if (BottomBattleWindow::IsVisible()) BottomBattleWindow::Hide();
}

// A player's duel: the blow (the target turns to the attacker, who strikes), then the counter-blow
// from the other side (a ranged target that was struck in melee first takes its distance).
void ArenaCombat::PlayerTurn(Pair1vs1* p) {
    if (!p->started) {
        if (p->skip) {
            p->started = true;
            p->state = 2;
            return;
        }
        if (Busy(p->attacker) || Busy(p->target)) return;
        p->attacker->GetAI()->AddedToCombat();
        p->target->GetAI()->AddedToCombat();
        p->attacker->GetAnimController()->mult = 0.5555556f;
        p->target->GetAnimController()->mult = 0.5555556f;
        if (!p->target->IsDead() && !p->target->IsDying() && !p->attacker->IsDead() && !p->attacker->IsDying()) {
            p->target->GetAI()->TurnTo(AI::GetWaypoint(p->attacker->tileX, p->attacker->tileY, false));
            p->attacker->GetAI()->StartedAttack(p->target);
            p->attacker->GetAI()->Attack(false);
        }
    } else {
        if (Busy(p->attacker) || Busy(p->target)) return;
        if (p->target->IsDead() || p->attacker->IsDead() || 0 < p->state || p->skip || !counterBlows) {
            p->state = 2;
            return;
        }
        Entity* first = p->attacker;
        p->started = false;
        p->attacker = p->target;
        p->target = first;
        p->state = 1;
        AI::Waypoint* pos;
        if (first->GetAttackRanged() < 1 || p->attacker->GetAttackRanged() != 0) {
            pos = p->attackerPos;
        } else {
            pos = AI::GetRandomWaypointInRange(p->target->tileX, p->target->tileY, 3, 2, true, nullptr, false);
            p->attackerPos = pos;
        }
        if (!pos) {
            p->state = 2;
            p->started = true;
        }
        if (AI::IsInMeleeRange(AI::GetWaypoint(p->target->tileX, p->target->tileY, false),
                               AI::GetWaypoint(p->attacker->tileX, p->attacker->tileY, false)))
            return;
        if (p->attacker->GetAttackRanged() != 0) return;
        p->state = 2;
    }
    p->started = true;
}

// An enemy's duel, as PlayerTurn but the enemy strikes the target it chose (its current target,
// else the duel's).
void ArenaCombat::EnemyTurn(Pair1vs1* p) {
    if (!p->started) {
        if (p->skip) {
            p->started = true;
            p->state = 2;
            return;
        }
        if (Busy(p->attacker) || Busy(p->target)) return;
        p->attacker->GetAI()->AddedToCombat();
        p->target->GetAI()->AddedToCombat();
        p->attacker->GetAnimController()->mult = 0.5555556f;
        p->target->GetAnimController()->mult = 0.5555556f;
        if (!p->target->IsDead() && !p->target->IsDying() && !p->attacker->IsDead() && !p->attacker->IsDying()) {
            if (!p->attacker->GetCurrentTarget()) p->attacker->SetCurrentTarget(p->target);
            Entity* t = p->attacker->GetCurrentTarget();
            t->GetAI()->TurnTo(AI::GetWaypoint(p->attacker->tileX, p->attacker->tileY, false));
            p->attacker->GetAI()->StartedAttack(p->attacker->GetCurrentTarget());
            p->attacker->SetCurrentTarget(nullptr);
            p->attacker->GetAI()->Attack(false);
        }
    } else {
        if (Busy(p->attacker) || Busy(p->target)) return;
        if (p->target->IsDead() || p->attacker->IsDead() || 0 < p->state || p->skip || !counterBlows) {
            p->state = 2;
            return;
        }
        Entity* first = p->attacker;
        p->started = false;
        p->attacker = p->target;
        p->target = first;
        p->state = 1;
        AI::Waypoint* pos;
        if (first->GetAttackRanged() < 1 || p->attacker->GetAttackRanged() != 0) {
            pos = p->attackerPos;
        } else {
            pos = AI::GetRandomWaypointInRange(p->target->tileX, p->target->tileY, 3, 2, true, nullptr, false);
            p->attackerPos = pos;
        }
        if (!pos) {
            p->state = 2;
            p->started = true;
        }
        if (AI::IsInMeleeRange(AI::GetWaypoint(p->target->tileX, p->target->tileY, false),
                               AI::GetWaypoint(p->attacker->tileX, p->attacker->tileY, false)))
            return;
        if (p->attacker->GetAttackRanged() != 0) return;
        p->state = 2;
    }
    p->started = true;
}

// The arena's lamp decoration (0x56d at (0xf, 0x17)) is hidden.
void ArenaCombat::BeginCombat(BaseSquad* player, BaseSquad* enemy) {
    playerSquad = player;
    enemySquad = enemy;
    cancel = false;
    paused = false;
    pair = nullptr;
    boss = enemy->GetOwner();
    boss->SetActive(false, true);
    playerSquad->DisableSpeedRun();
    if (!BottomBattleWindow::IsVisible()) BottomBattleWindow::Show();
    if (GameState::GetEventCompletionCount(2) == 0 && !GameState::IsPvPCombatActive()) {
        Entity* hero = playerSquad->GetOwner();
        hero->SetHP(playerSquad->GetOwner()->GetHpMax());
        GameState::UpdatePlayerRegenerationState();
    }
    SoundsManager::PlaySound("arena_win", 1, false);
    SetupWave();
    Map::Decor* d = Map::GetDecoration(0xf, 0x17);
    if (!d) return;
    if (d->id == 0x56d) d->visible = false;
}

// The turn's banner (ArenaTurnWindow) and the bar follow the fight; the player has the arena's
// turn time to choose a stance, else the last one is kept. The arena's lessons (tasks 0x5d0,
// 0x5cd, 0x5ce, 0x5d7) show the attack, block and magic buttons in turn.
void ArenaCombat::Update(float dt) {
    if (!ArenaTurnWindow::IsVisible() && !BossIntroWindow::IsVisible() && !BossOutroWindow::IsVisible() && !killedBoss)
        ArenaTurnWindow::Show();
    if (TopCityWindow::IsVisible() && !killedBoss) TopCityWindow::Hide();
    ArenaTurnWindow::SetHeader(SWPrintf(0x80, StringTable::GetString("ARENA_TURN_NUM"), {ToWideString((turn + 1) / 2)}).c_str());
    bool opponent = false;
    if ((state == 0xf || state == 2) && paused) {
        const float turnTime = (float)Setting("arena_turn_time").GetInt();
        if (turnTime <= turnTimer) {
            if (state == 0xf) SpellMovement::Decline();
            if (!BottomBattleWindow::IsVisible()) BottomBattleWindow::Show();
            turnTimer = 0.f;
            if (stance == 0) AttackStance();
            else BlockStance();
            paused = false;
            BottomBattleWindow::SetEnabled(paused, false, autoCombat);
            goto step;
        }
        ArenaTurnWindow::SetDescription(StringTable::GetString("ARENA_TURN_PLAYER"));
        ArenaTurnWindow::SetIcon(2, IconManager::GetSandClockIcon());
        if (GameState::TaskCompleted(0x5d7) && !PlayerTopWindow::IsPlayerDialogVisible()) {
            turnTimer = dt + turnTimer;
            const std::u32string n = SWPrintf(0x80, U"%s", {ToWideString((int)(turnTime - turnTimer))});
            const bool hurry = turnTime - turnTimer < 5.f;
            ArenaTurnWindow::SetNumber(hurry ? 3 : 2, n.c_str(), hurry);
        } else if (GameState::TaskCompleted(0x5d0) && !GameState::TaskCompleted(0x5cd)) {
            if (!PlayerTopWindow::IsPlayerDialogVisible()) {
                PlayerTopWindow::SetParameters(StringTable::GetString("DESC1485"), 0, false, true);
                PlayerTopWindow::Show();
                BottomBattleWindow::ShowAttackButton(0x5cd, true, true);
            }
        } else if (GameState::TaskCompleted(0x5cd) && !GameState::TaskCompleted(0x5ce)) {
            if (!PlayerTopWindow::IsPlayerDialogVisible()) {
                PlayerTopWindow::SetParameters(StringTable::GetString("DESC1486"), 0, false, true);
                PlayerTopWindow::Show();
                BottomBattleWindow::ShowBlockButton(0x5ce, true, true);
            }
        } else if (GameState::TaskCompleted(0x5ce) && !GameState::TaskCompleted(0x5d7) &&
                   !PlayerTopWindow::IsPlayerDialogVisible()) {
            PlayerTopWindow::SetParameters(StringTable::GetString("DESC1495"), 0x5d7, false, false);
            PlayerTopWindow::Show();
            BottomBattleWindow::ShowMagicButton(0x5d7, false, true);
        }
    } else if (!playerTurn && !EntityManager::GetPlayer()->IsDead() && !killedBoss) {
        ArenaTurnWindow::SetIcon(1, IconManager::GetSandClockIcon());
        ArenaTurnWindow::SetDescription(StringTable::GetString("ARENA_TURN_OPPONENT"));
        ArenaTurnWindow::UpdateContents();
        opponent = true;
        BottomBattleWindow::SetEnabled(paused, opponent, autoCombat);
        goto step;
    } else if (!playerTurn) {
        BottomBattleWindow::SetEnabled(paused, false, autoCombat);
        goto step;
    } else {
        ArenaTurnWindow::SetIcon(2, IconManager::GetSandClockIcon());
        ArenaTurnWindow::SetDescription(StringTable::GetString("ARENA_TURN_PLAYER"));
    }
    ArenaTurnWindow::UpdateContents();
    BottomBattleWindow::SetEnabled(paused, opponent, autoCombat);
step:
    if (paused) return;
    switch (state) {
    case 2: WaitingForContinue(); break;
    case 3: WaveCleared(); break;
    case 4: StartPositioning(); break;
    case 5: UpdatePositions(); break;
    case 6: CombatReady(); break;
    case 9: CheckSpellCast(); break;
    case 10: SpellCast(); break;
    case 0xd: UpdateRunningToPos(); break;
    case 0xe: ArenaCompleted(); break;
    case 0x10: RunningOut(); break;
    default: break;
    }
}

// Each duel whose fighters stand in place (or one of whom died) is ready.
void ArenaCombat::UpdatePositions() {
    for (Pair1vs1* p : duels) {
        if (p->state == 2) continue;
        if (p->target->IsDead() || p->attacker->IsDead()) p->ready = true;
        if (p->attacker->tileX == p->attackerPos->x && p->attacker->tileY == p->attackerPos->y &&
            p->target->tileX == p->targetPos->x && p->target->tileY == p->targetPos->y)
            p->ready = true;
    }
    SwitchState(9);
}

// The arena's rectangle (centre [8], [9], size [10] x [11] of mBossMapPositions) is walkable again.
void ArenaCombat::ClearArenaWPs() {
    int x = GameState::mBossMapPositions[8], y = GameState::mBossMapPositions[9];
    int w = GameState::mBossMapPositions[10];
    int n = w / 2;
    if ((w & 1) == 0) --n;
    if (0 < n) {
        int yy = y;
        do {
            const bool odd = std::abs(yy) & 1;
            --yy;
            if (odd) ++x;
            --x;
        } while (yy != y - n);
        y -= n;
    }
    const int h = GameState::mBossMapPositions[11];
    int n2 = h / 2;
    if ((h & 1) == 0) --n2;
    if (0 < n2) {
        int yy = y;
        int next;
        do {
            next = yy - 1;
            if (yy % 2 == 1) ++x;
            yy = next;
        } while (next != y - n2);
        y -= n2;
    }
    if (0 < h) {
        for (int row = 0;;) {
            if (0 < w) {
                int cx = x, cy = y;
                for (int k = 0; k < GameState::mBossMapPositions[10];) {
                    ++k;
                    if (AI::Waypoint* wp = AI::GetWaypoint(cx, cy, false)) wp->weight = 1.f;
                    if (cy % 2 == 1) ++cx;
                    ++cy;
                }
            }
            ++row;
            if (std::abs(y) & 1) ++x;
            if (GameState::mBossMapPositions[11] <= row) break;
            --x;
            ++y;
            w = GameState::mBossMapPositions[10];
        }
    }
}

// Everything around the arena (its rectangle grown by 3 or 2 tiles on each side) is blocked
// (weight 100000).
void ArenaCombat::DisableOuterWPs() {
    int x = GameState::mBossMapPositions[8], y = GameState::mBossMapPositions[9];
    int w = GameState::mBossMapPositions[10];
    int n = (w & 1) == 0 ? w / 2 + 2 : w / 2 + 3;
    if (0 < n) {
        int yy = y;
        do {
            const bool odd = std::abs(yy) & 1;
            --yy;
            if (odd) ++x;
            --x;
        } while (yy != y - n);
        y -= n;
    }
    const int h = GameState::mBossMapPositions[11];
    int n2 = (h & 1) == 0 ? h / 2 + 2 : h / 2 + 3;
    if (0 < n2) {
        int yy = y;
        int next;
        do {
            next = yy - 1;
            if (yy % 2 == 1) ++x;
            yy = next;
        } while (next != y - n2);
        y -= n2;
    }
    if (-6 < h) {
        for (int row = 0;;) {
            if (-6 < w) {
                int cx = x, cy = y;
                int k = 0;
                do {
                    ++k;
                    if (AI::Waypoint* wp = AI::GetWaypoint(cx, cy, false)) wp->weight = 100000.f;
                    if (cy % 2 == 1) ++cx;
                    ++cy;
                } while (k <= GameState::mBossMapPositions[10] + 5);
            }
            ++row;
            if (std::abs(y) & 1) ++x;
            if (GameState::mBossMapPositions[11] + 5 < row) break;
            --x;
            ++y;
            w = GameState::mBossMapPositions[10];
        }
    }
}

// Each duel's attacker walks to striking distance of its target (any nearby tile for a ranged
// one) and the target to its own tile; without a way to its target, on the player's turn the
// attacker tries the other enemies in turn.
void ArenaCombat::UpdateAttackPosition() {
    for (unsigned i = 0; i < duels.size(); ++i) {
        Pair1vs1* p = duels[i];
        if (p->attackerPos) {
            p->attacker->GetAI()->SetTarget(p->attackerPos, true);
            continue;
        }
        Entity* t = p->target;
        Entity* a = p->attacker;
        const int tx = t->tileX, ty = t->tileY;
        if (0 < a->GetAttackRanged()) p->attackerPos = AI::GetRandomWaypointInRange(tx, ty, 3, 2, true, nullptr, false);
        else p->attackerPos = AI::GetNearestReachableWPZeroWeight(a, t);
        p->targetPos = AI::GetWaypoint(tx, ty, false);
        if (p->attackerPos) {
            p->attackerPos->weight = 1000.f;
            p->targetPos->weight = 1000.f;
            p->attacker->GetAI()->WalkTo(p->attackerPos);
            p->target->GetAI()->WalkTo(p->targetPos);
            continue;
        }
        if (!playerTurn) {
            p->state = 2;
            continue;
        }
        for (unsigned k = 0; k < badGuys.size(); ++k) {
            Entity* b = badGuys[k];
            if (0 < p->attacker->GetAttackRanged())
                p->attackerPos = AI::GetRandomWaypointInRange(b->tileX, b->tileY, 3, 2, true, nullptr, false);
            else
                p->attackerPos = AI::GetNearestReachableWPZeroWeight(p->attacker, b);
            p->targetPos = AI::GetWaypoint(b->tileX, b->tileY, false);
            if (p->attackerPos) {
                p->attackerPos->weight = 1000.f;
                p->targetPos->weight = 1000.f;
                p->target = badGuys[k];
                p->attacker->GetAI()->WalkTo(p->attackerPos);
                p->target->GetAI()->WalkTo(p->targetPos);
                p->state = 0;
                break;
            }
            p->state = 2;
        }
    }
    SwitchState(5);
}

bool ArenaCombat::CanSpellCastNow() { return duels.empty() ? paused : false; }

// The ready duels strike: all at once (allAtOnce), else the first. With the hero dead in one of
// them, every duel is skipped.
void ArenaCombat::CombatReady() {
    SortPairs();
    if (!goodGuys.empty() && !duels.empty()) {
        bool heroDead = false;
        for (Pair1vs1* p : duels) {
            if (p->attacker->player && p->attacker->IsDead()) heroDead = true;
            else if (p->target->player && p->target->IsDead()) heroDead = true;
        }
        for (unsigned i = 0; i < duels.size(); ++i) {
            Pair1vs1* p = duels[i];
            if (heroDead) p->skip = true;
            if (!p->ready || !(allAtOnce || p->state != 2)) continue;
            const int clas = p->attacker->GetEntityData()->clas;
            if (clas == 10 || clas == 5) PlayerTurn(p);
            else EnemyTurn(p);
            if (!allAtOnce) break;
        }
    }
    SwitchState(2);
}

// The bar's choices: -1 a tap on an enemy, 0 Attack, 1 Block, 2 the victory pose, 4 Auto, 5 / 6
// all duels at once or one by one, 7 the next wave (or the end), 8 skip, 9 counter-blows on / off,
// 10 the enemy's turn.
void ArenaCombat::Continue(int m) {
    if (!(duels.empty() || m == 4) || state == 0xf) return;
    mode = m;
    paused = false;
    turnSteps = 0;
    switch (m) {
    case 0: AttackStance(); return;
    case 1: BlockStance(); return;
    case 2:
        playerTurn = false;
        for (unsigned i = 0; i < (unsigned)playerSquad->GetSoldierCount(); ++i) {
            playerSquad->GetSoldier(i)->SetAnimationP("idle_1", true, false, false);
            playerSquad->GetSoldier(i)->GetAI()->SetCustomIdleAnimation("idle_1", -1, -1);
        }
        return;
    case 4:
        enemySquad->SetActiveSoldier(nullptr);
        AutoCombat();
        return;
    case 5:
        allAtOnce = true;
        paused = true;
        return;
    case 6:
        allAtOnce = false;
        paused = true;
        return;
    case 7:
        if (wave == waveCount) {
            SwitchState(0xe);
        } else {
            SwitchState(0xd);
            RunToSpawnPos();
        }
        return;
    case 8: SkipCombat(); return;
    case 9:
        paused = true;
        counterBlows = !counterBlows;
        return;
    case 10: playerTurn = false; return;
    default: return;
    }
}

// The fight is over: the lists are emptied, the player's squad leaves the fight, the bar goes
// (and the arrow of a second-tutorial arena lesson).
void ArenaCombat::EndCombat() {
    goodGuys.clear();
    badGuys.clear();
    everyone.clear();
    if (playerSquad) {
        playerSquad->OnEndCombat();
        for (unsigned i = 0; i < (unsigned)playerSquad->GetSoldierCount(); ++i)
            playerSquad->GetSoldier(i)->GetAI()->RemovedFromCombat();
        playerSquad = nullptr;
    }
    killedBoss = true;
    if (BottomBattleWindow::IsVisible()) BottomBattleWindow::Hide();
    SwitchState(0);
    if (!BuildingHovers::ArrowVisible()) return;
    if (GameState::secondTutorial < 0x9f) return;
    if (GameState::IsVirtualTutorial()) return;
    if (BuildingHovers::GetArrowClickCallbackID() != arrowCallback) return;
    BuildingHovers::HideArrow();
}

// @0xfcae0: a wave of the boss's minions ("boss_minions", picked by a generator seeded with the
// event's start time: x = x * 0x343fd + 0x269ec3, bits 16..30). 2..8 of them (2..3 and 3 HP, no
// defence, 1 damage while the event was never won outside PvP), one fewer in the last wave, where the
// boss leads them; else the last minion leads. Their HP shares the boss's "char_hp_pool" for the
// player's level; their other stats come from its pools (a ranged one's ranged attack takes the
// melee pool, as the original). The squads take their places and aim at each other; the intro shows
// the wave (from the second), the outro gets "Wave n" or, in the last wave, the boss's name, and the
// gold spent comes back on a win (arena_dropback_victory, at most all of it).
void ArenaCombat::SetupWave() {
    EntityData* d = boss->GetEntityData();
    std::vector<uint32_t> ids;
    for (unsigned i = 0; i < d->bossMinions->GetChildrenCount(); ++i)
        ids.push_back((uint32_t)d->bossMinions->GetChild(i)->GetInt());
    auto next = [this] { return eventStart = eventStart * 0x343fd + 0x269ec3; };
    const bool easy = GameState::GetEventCompletionCount(2) == 0 && !GameState::IsPvPCombatActive();
    unsigned count;
    if (easy) {
        next();
        count = ((next() >> 16) & 1) + 2;
    } else {
        count = ((next() >> 16) & 0x7fff) % 7 + 2;
    }
    const bool last = wave == waveCount - 1;
    if (last) --count;
    const int hp = (int)((unsigned)d->GetWavesHpForLevel(GameState::GetLevel()) / count);
    std::vector<Entity*> minions;
    for (unsigned i = 0; i < count; ++i) {
        const uint32_t pick = ((next() >> 16) & 0x7fff) % (unsigned)ids.size();
        Entity* e = EntityManager::CreateEntity((int)ids[pick], false, true);
        e->SetXP(0);
        e->SetWarriorCost(0);
        e->SetItemDrop(nullptr);
        e->SetHP(easy ? 3 : hp);
        e->SetHpMax(easy ? 3 : hp);
        const int attack = easy ? 1 : d->GetWavesAttackMeleeForLevel(GameState::GetLevel());
        if (e->GetAttackRanged() > 0) e->SetAttackRanged(attack);
        else e->SetAttackMelee(attack);
        if (easy) {
            e->SetDefenseRanged(0);
            e->SetDefenseMelee(0);
            e->SetDefenseMagic(0);
            e->SetCritChance(0);
        } else {
            e->SetDefenseRanged(d->GetWavesDefenseRangedForLevel(GameState::GetLevel()));
            e->SetDefenseMelee(d->GetWavesDefenseMeleeForLevel(GameState::GetLevel()));
            e->SetDefenseMagic(d->GetWavesDefenseMagicForLevel(GameState::GetLevel()));
            e->SetCritChance(d->GetWavesCritChanceForLevel(GameState::GetLevel()));
        }
        e->f75 = true;
        minions.push_back(e);
    }
    if (last) {
        boss->SetActive(true, true);
        enemySquad = boss->GetSquad();
    } else {
        Entity* leader = minions.back();
        minions.pop_back();
        leader->CreateSquad();
        enemySquad = leader->GetSquad();
    }
    for (Entity* e : minions) enemySquad->AddSoldier(e);
    ClearArenaWPs();
    SpawnEnemies();
    if (wave == 0) SpawnPlayer();
    playerSquad->SetCurrentTarget(enemySquad->GetOwner());
    enemySquad->SetCurrentTarget(playerSquad->GetOwner());
    goodGuys.clear();
    badGuys.clear();
    for (Entity* e : playerSquad->GetSoldiers()) goodGuys.push_back(e);
    for (Entity* e : enemySquad->GetSoldiers()) badGuys.push_back(e);
    BossIntroWindow::SetWave(wave + 1, waveCount);
    if (wave != 0 && !GameState::arenaBotActive) {
        BossIntroWindow::SetBossName(boss->GetEntityData()->title);
        BossIntroWindow::Show();
    }
    const std::u32string text = SWPrintf(0x80, U"Wave %d", {wave + 1});
    const char32_t* name = text.c_str();
    if (wave == waveCount - 1) {
        float share = Setting("arena_dropback_victory").GetFloat();
        if (share > 1.f) share = 1.f;
        GameState::AddMapResourceCollectionInfo(0x70, 8, (int)((float)GetSpentGold() * share));
        name = boss->GetEntityData()->title;
    }
    BossOutroWindow::SetBossName(name, boss->GetEntityData()->id);
    autoCombat = false;
    ++wave;
    if (GameState::arenaBotActive) autoCombat = true;
    paused = true;
    playerTurn = true;
    SwitchState(2);
}

// @0xfdec0: the wave in its formation on the far side, facing the player.
void ArenaCombat::SpawnEnemies() {
    const int ex = GameState::mBossMapPositions[6], ey = GameState::mBossMapPositions[7];
    int a = ex;
    if (std::abs(ey) & 1) ++a;
    int startX = a - 1, other;
    if ((std::abs(ey - 1) & 1) == 0) {
        other = startX - 1;
    } else {
        other = startX;
        startX = a;
    }
    const int startY = ey - 3;
    if ((ey - 2) % 2 != 1) startX = other;
    std::printf("idx = %d\n", enemySquad->GetSoldierCount() - 2);
    // (The original also prints each tried tile, "trying to spawn %d at %d, %d, weight = %f", and
    // its formation cell, "array idx = %d", and "bad spawn" for a member of the wrong kind.)
    unsigned spawned = 0;
    PlaceFormation(enemySquad, kWaveFormation, startX, startY, [&spawned](Entity* e, int tx, int ty) {
        ++spawned;
        std::printf("spawning %d at %d, %d\n", (int)e->GetEntityData()->id, tx, ty);
        Entity* s = EntityManager::SpawnEntityAt(e, (unsigned)tx, (unsigned)ty, false, false);
        s->GetAI()->ClearPath();
        s->SetAnimationP("idle_1", true, false, false);
        s->SetDirection(7);
    });
    if (spawned != (unsigned)enemySquad->GetSoldierCount()) std::puts("ERROR!! didn't spawn all enemy soldiers");
}

// The player's squad stands in its formation on its side of the arena, facing the front.
void ArenaCombat::SpawnPlayer() {
    int x, y;
    PlayerFormationStart(x, y);
    PlaceFormation(playerSquad, kPlayerFormation, x, y, [](Entity* e, int tx, int ty) {
        Entity* s = EntityManager::SpawnEntityAt(e, (unsigned)tx, (unsigned)ty, false, false);
        s->GetAI()->ClearPath();
        s->SetAnimationP("idle_1", true, false, false);
        s->SetDirection(3);
    });
}

// Between waves the player's squad walks back to its formation.
void ArenaCombat::RunToSpawnPos() {
    int x, y;
    PlayerFormationStart(x, y);
    PlaceFormation(playerSquad, kPlayerFormation, x, y, [](Entity* e, int tx, int ty) { e->GetAI()->WalkTo(tx, ty); });
}

// A good guy joins once.
void ArenaCombat::AddGoodGuy(Entity* e) {
    for (Entity* g : goodGuys)
        if (g == e) return;
    goodGuys.push_back(e);
}

// The duels in striking order: on the player's turn his side first, else the enemies (class 9).
void ArenaCombat::SortPairs() {
    std::vector<Pair1vs1*> sorted;
    const bool enemiesFirst = !playerTurn;
    for (Pair1vs1* p : duels)
        if ((p->attacker->GetEntityData()->clas == 9) == enemiesFirst) sorted.push_back(p);
    for (Pair1vs1* p : duels)
        if ((p->attacker->GetEntityData()->clas == 9) != enemiesFirst) sorted.push_back(p);
    duels = sorted;
}

// Between steps: when every duel is over the turn passes (the dead leave the lists; the auto fight
// picks the next stance or a spell); a new turn pairs the side whose turn it is with the other.
void ArenaCombat::WaitingForContinue() {
    const unsigned n = (unsigned)duels.size();
    if (n != 0) {
        unsigned over = 0;
        for (Pair1vs1* p : duels)
            if (p->state == 2) ++over;
        if (over == n) {
            if (!autoCombat) paused = true;
            if (spellQueued) paused = false;
            for (Pair1vs1* p : duels) {
                p->attacker->GetAI()->RemovedFromCombat();
                if (p->target) p->target->GetAI()->RemovedFromCombat();
                delete p;
            }
            duels.clear();
            for (auto it = goodGuys.begin(); it != goodGuys.end();) {
                Entity* e = *it;
                if (!e->player && e->IsDead()) {
                    e->OnRemovedFromSquad();
                    AI::GetWaypoint(e->tileX, e->tileY, false)->weight = 1.f;
                    it = goodGuys.erase(it);
                } else {
                    ++it;
                }
            }
            for (auto it = badGuys.begin(); it != badGuys.end();) {
                Entity* e = *it;
                if (e->IsDead()) {
                    e->OnRemovedFromSquad();
                    AI::GetWaypoint(e->tileX, e->tileY, false)->weight = 1.f;
                    it = badGuys.erase(it);
                } else {
                    ++it;
                }
            }
            const int steps = turnSteps++;
            if (steps < 1) {
                ++turn;
                playerTurn = !playerTurn;
                if (autoCombat && playerTurn) {
                    // The auto fight: with spells the hero can afford (Spells::GetBestSpellForSlot
                    // for slots 1..3 with his AP) a random slot of that many is queued at the first
                    // enemy's tile; without, or without an enemy, it picks a stance.
                    unsigned spells = 0;
                    for (unsigned slot = 1; slot != 4; ++slot)
                        if (Spells::GetBestSpellForSlot(slot, (uint32_t)playerSquad->GetOwner()->GetAP(), true)) ++spells;
                    if (spells != 0 && !badGuys.empty()) {
                        spellQueued = true;
                        spell = (unsigned)Rand48::lrand48() % spells + 1;
                        spellEpicenter = AI::GetWaypoint(badGuys[0]->tileX, badGuys[0]->tileY, false);
                    } else {
                        Entity* hero = playerSquad->GetOwner();
                        if (!AI::EnemyInMeleeRange(AI::GetWaypoint(hero->tileX, hero->tileY, false)) ||
                            (Rand48::lrand48() & 1) == 0)
                            AttackStance();
                        else
                            BlockStance();
                    }
                }
            }
            if (!playerTurn) {   // the enemy's turn starts at once
                turnSteps = 0;
                turnTimer = 0.f;
                paused = false;
            }
            pair = nullptr;
            wp14 = nullptr;
            if (enemySquad->GetAliveSoldierCount() == 0) {
                SwitchState(3);
                paused = false;
                return;
            }
            if (playerSquad->GetOwner()->IsDead()) {
                SwitchState(0);
                paused = true;
                return;
            }
            SwitchState(2);
            return;
        }
    }
    if (spellQueued && n == 0) {
        Pair1vs1* p = new Pair1vs1();
        p->attacker = playerSquad->GetOwner();
        p->initiative = p->attacker->GetInitiative(0);
        duels.push_back(p);
        SwitchState(10);
        spellQueued = false;
        return;
    }
    auto inDuel = [this](Entity* e) {
        for (Pair1vs1* p : duels)
            if (p->attacker == e || p->target == e) return true;
        return false;
    };
    auto add = [this](Entity* a, Entity* t) {
        Pair1vs1* p = new Pair1vs1();
        p->attacker = a;
        p->target = t;
        p->initiative = a->GetInitiative(0);
        duels.push_back(p);
    };
    if (!playerTurn) {
        // Each enemy strikes a good guy (the PvP tutorial's first blow, against a blocking hero
        // with two soldiers, goes to the third).
        for (unsigned i = 0; i < badGuys.size(); ++i) {
            if (!duels.empty() && inDuel(badGuys[i])) continue;
            unsigned k;
            if (i == 0 && GameState::IsPvPCombatActive() && PlayerCompetitionWindow::GetSelectedPlayerGID() == -1 &&
                stance == 1 && 2 < goodGuys.size())
                k = 2;
            else
                k = i % (unsigned)goodGuys.size();
            add(badGuys[i], goodGuys[k]);
        }
    } else if (!enemySquad->GetActiveSoldier()) {
        if (badGuys.size() < goodGuys.size()) {
            for (unsigned i = 0; i < goodGuys.size(); ++i) {
                if (!duels.empty() && inDuel(goodGuys[i])) continue;
                if (badGuys.empty()) continue;
                add(goodGuys[i], badGuys[i % badGuys.size()]);
            }
        } else {
            for (unsigned i = 0; i < goodGuys.size(); ++i) {
                if (!duels.empty() && inDuel(goodGuys[i])) continue;
                if (badGuys.empty()) continue;
                // the first enemy nobody fights yet
                for (unsigned k = 0; k < badGuys.size(); ++k) {
                    if (!duels.empty() && inDuel(badGuys[k])) continue;
                    add(goodGuys[i], badGuys[k]);
                    break;
                }
            }
        }
    } else {
        // the enemy chosen with a tap
        for (unsigned i = 0; i < goodGuys.size(); ++i) {
            if (!duels.empty() && inDuel(goodGuys[i])) continue;
            add(goodGuys[i], enemySquad->GetActiveSoldier());
        }
    }
    SwitchState(4);
}

// --------------------------------------------------------------------------------- PvPArenaCombat

PvPArenaCombat::PvPArenaCombat(PvP::PlayerInfo* o, bool secondWave) {
    waveSet = secondWave;
    playerTurn = true;
    turnTimer = 0.f;
    allAtOnce = true;
    waveCount = 1;
    if (GameState::arenaBotActive) waveCount = 35000;
    opponent = o;
    type = 6;
    wave = 0;
    autoCombat = false;
    turnSteps = 0;
    spellEpicenter = nullptr;
    counterBlows = true;
    stance = 0;
    DisableOuterWPs();
    waitingForServer = false;
    spentGold = 0;
}

PvPArenaCombat::~PvPArenaCombat() {
    if (enemySquad) {
        enemySquad->OnEndCombat();
        enemySquad->RemoveAllSoldiers();
        enemySquad = nullptr;
    }
}

// The opponent's hero joins as the boss (AI 1); the lamp loses its interaction; until the arena
// event was ever won (and outside PvP) the hero is healed.
void PvPArenaCombat::BeginCombat(BaseSquad* player, BaseSquad* enemy) {
    Map::Decor* d = Map::GetDecoration(0xf, 0x17);
    if (d && d->id == 0x56d) d->visible = false;
    enemySquad = enemy;
    playerSquad = player;
    cancel = false;
    paused = false;
    pair = nullptr;
    GameState::SetStartingArenaHP(player->GetOwner()->GetHP());
    playerSquad->DisableSpeedRun();
    boss = enemySquad->GetOwner();
    boss->SetActive(false, true);
    boss->ChangeAIState(1);
    if (!BottomBattleWindow::IsVisible()) BottomBattleWindow::Show();
    if (GameState::GetEventCompletionCount(2) == 0 && !GameState::IsPvPCombatActive()) {
        Entity* hero = playerSquad->GetOwner();
        hero->SetHP(playerSquad->GetOwner()->GetHpMax());
        GameState::UpdatePlayerRegenerationState();
    }
    SoundsManager::PlaySound("arena_win", 1, false);
    SetupWave();
}

// The opponent's warriors (mBossWaves but the last, with their HP) and hero (her level, the hero
// look 0x285 / 0x286, her unbroken gear's bonuses, initiative 50) take their places; the player's
// side as well; at most four fighters a side; the opponent strikes first.
void PvPArenaCombat::SetupWave() {
    boss->SetActive(true, true);
    enemySquad = boss->GetSquad();
    if (GameState::mBossWaves.size() != 1) {
        for (unsigned i = 0; i < GameState::mBossWaves.size() - 1; ++i) {
            Entity* e = EntityManager::CreateEntity((int)GameState::mBossWaves[i], false, true);
            if (!e) continue;
            EntityData* d = EntityFactory::GetEntityByID((int)GameState::mBossWaves[i])->PartialClone();
            d->clas = 9;
            e->SetCustomEntityData(d);
            e->ChangeAIState(9);
            enemySquad->AddSoldier(e);
            if (GameState::mBossWavesHP[i] != 0) {
                e->SetHpMax(GameState::mBossWavesHP[i]);
                e->SetHP(GameState::mBossWavesHP[i]);
            }
        }
    }
    EntityData* d = EntityFactory::GetEntityByID((int)boss->GetEntityData()->id)->PartialClone();
    d->clas = 9;
    d->level = opponent->level;
    if ((unsigned)(d->id - 0xc) < 2) {
        if (PlayerCompetitionWindow::GetSelectedPlayerGID() == 1) {
            d->hp = 5;
            boss->SetHP(5);
            boss->SetHpMax(5);
        }
        if (EntityData* look = EntityFactory::GetEntityByID(d->id == 0xc ? 0x285 : 0x286))
            if (&d->anims != &look->anims) d->anims = look->anims;
    }
    boss->SetCustomEntityData(d);
    boss->ResetStats(true);
    boss->SetInitiative(0x32);
    if (BaseSquad* sq = boss->GetSquad())
        for (unsigned i = 1; i < (unsigned)sq->GetSoldierCount(); ++i)
            if (Entity* e = sq->GetSoldier(i)) e->ResetStats(false);
    if (PlayerCompetitionWindow::GetSelectedPlayerGID() != 1) {
        for (int b = 0; b < 10; ++b) {
            const Items::ItemInfo* it = Items::GetItemInfo(opponent->equipment.items[b].id);
            if (it && it->meta && !opponent->equipment.items[b].broken) it->meta->OnApplyItemEffect(boss);
        }
    }
    boss->SetHP(boss->GetHpMax());
    ClearArenaWPs();
    // At most four fighters on the opponent's side (checked once more for a big player squad).
    auto trim = [this] {
        const unsigned n = (unsigned)enemySquad->GetSoldierCount();
        if (n - 1 > 3)
            for (unsigned k = n - 1; k != 3; --k) enemySquad->RemoveSoldier(enemySquad->GetSoldier(k));
    };
    if (4 < (unsigned)playerSquad->GetSoldierCount()) trim();
    if (4 < (unsigned)enemySquad->GetSoldierCount()) trim();
    SpawnEnemies();
    if (wave == 0 && !waveSet) SpawnPlayer();
    waveSet = true;
    playerSquad->SetCurrentTarget(enemySquad->GetOwner());
    enemySquad->SetCurrentTarget(playerSquad->GetOwner());
    goodGuys.clear();
    badGuys.clear();
    for (Entity* e : playerSquad->GetSoldiers()) goodGuys.push_back(e);
    for (Entity* e : enemySquad->GetSoldiers()) badGuys.push_back(e);
    autoCombat = false;
    ++wave;
    if (GameState::arenaBotActive) autoCombat = true;
    BottomBattleWindow::SetSelectedButton((unsigned)-1);
    paused = true;
    playerTurn = true;
    SwitchState(2);
}

// The opponent's squad in its formation on the far side, facing the player.
void PvPArenaCombat::SpawnEnemies() {
    const int ex = GameState::mBossMapPositions[6], ey = GameState::mBossMapPositions[7];
    int a = ex;
    if (std::abs(ey) & 1) ++a;
    int startX = a - 1, other;
    if ((std::abs(ey - 1) & 1) == 0) {
        other = startX - 1;
    } else {
        other = startX;
        startX = a;
    }
    const int startY = ey - 3;
    if ((ey - 2) % 2 != 1) startX = other;
    std::printf("idx = %d\n", enemySquad->GetSoldierCount() - 1);
    unsigned spawned = 0;
    PlaceFormation(enemySquad, kOpponentFormation, startX, startY, [&spawned](Entity* e, int tx, int ty) {
        ++spawned;
        std::printf("spawning %d at %d, %d\n", (int)e->GetEntityData()->id, tx, ty);
        Entity* s = EntityManager::SpawnEntityAt(e, (unsigned)tx, (unsigned)ty, false, false);
        s->GetAI()->ClearPath();
        s->SetAnimationP("idle_1", true, false, false);
        s->SetDirection(7);
    });
    if (spawned != (unsigned)enemySquad->GetSoldierCount()) std::puts("ERROR!! didn't spawn all enemy soldiers");
}

// The turn's banner and the tutorial's ambush: the rival speaks ("ASSASIN_DAIL", task 0x772),
// then the hero's lessons for Attack (task 0x5cd) and Block (0x5ce); after the ambush (0x773)
// the player has "pvp_turn_time" to choose, shown as the count.
void PvPArenaCombat::Update(float dt) {
    if (waitingForServer) return;
    if (!PlayerBattleInOutWindow::IsVisible()) {
        if (!ArenaTurnWindow::IsVisible() && !PlayerBattleInOutWindow::IsVisible() &&
            !PlayerTopWindow::IsPlayerDialogVisible() && !BuildingHovers::IsWorldDialogVisible() &&
            GameState::TaskCompleted(0x772))
            ArenaTurnWindow::Show();
        if (TopCityWindow::IsVisible()) TopCityWindow::Hide();
    }
    ArenaTurnWindow::SetHeader(SWPrintf(0x100, StringTable::GetString("ARENA_TURN_NUM"), {ToWideString((turn + 1) / 2)}).c_str());
    bool opponentTurn = false;
    if ((state == 0xf || state == 2) && paused) {
        const float turnTime = (float)Setting("pvp_turn_time").GetInt();
        if (turnTime <= turnTimer) {
            if (state == 0xf) SpellMovement::Decline();
            if (!BottomBattleWindow::IsVisible()) BottomBattleWindow::Show();
            turnTimer = 0.f;
            if (stance == 0) AttackStance();
            else BlockStance();
            paused = false;
            BottomBattleWindow::SetEnabled(paused, false, autoCombat);
            goto step;
        }
        if (!PlayerBattleInOutWindow::IsVisible() || !GameState::IsPvPTutorial()) {
            if (!GameState::TaskCompleted(0x773)) {
                if (!GameState::TaskCompleted(0x772)) {
                    if (!PlayerTopWindow::IsPlayerDialogVisible() && !BuildingHovers::IsWorldDialogVisible()) {
                        const std::u32string text =
                            SWPrintf(0x100, StringTable::GetString("ASSASIN_DAIL"), {GameState::GetPlayerName()});
                        BuildingHovers::ShowWorldDialog(enemySquad->GetOwner(), text.c_str(), 0x772, true, true, true, 0, false,
                                                        true);
                        Render::CenterOn(797.f, 282.f, true);
                    }
                } else if (!GameState::TaskCompleted(0x5cd)) {
                    if (!PlayerTopWindow::IsPlayerDialogVisible() && !BuildingHovers::IsWorldDialogVisible()) {
                        PlayerTopWindow::SetParameters(StringTable::GetString("DESC1485"), 0, false, true);
                        PlayerTopWindow::Show();
                        BottomBattleWindow::ShowAttackButton(0x5cd, true, true);
                    }
                } else if (GameState::TaskCompleted(0x5cd) && !GameState::TaskCompleted(0x5ce) &&
                           !PlayerTopWindow::IsPlayerDialogVisible() && !BuildingHovers::IsWorldDialogVisible()) {
                    PlayerTopWindow::SetParameters(StringTable::GetString("DESC1486"), 0, false, true);
                    PlayerTopWindow::Show();
                    BottomBattleWindow::ShowBlockButton(0x5ce, true, true);
                }
            } else {
                if (!PlayerBattleInOutWindow::IsVisible() && !PlayerTopWindow::IsPlayerDialogVisible() &&
                    !BuildingHovers::IsWorldDialogVisible())
                    turnTimer = turnTimer + dt;
                const std::u32string n = SWPrintf(0x100, U"%s", {ToWideString((int)(turnTime - turnTimer))});
                const bool hurry = turnTime - turnTimer < 5.f;
                ArenaTurnWindow::SetNumber(hurry ? 3 : 2, n.c_str(), hurry);
                ArenaTurnWindow::SetDescription(StringTable::GetString("ARENA_TURN_PLAYER"));
                ArenaTurnWindow::UpdateContents();
            }
            if (!GameState::TaskCompleted(0x773)) {
                ArenaTurnWindow::SetIcon(2, IconManager::GetSandClockIcon());
                ArenaTurnWindow::SetDescription(StringTable::GetString("ARENA_TURN_PLAYER"));
                ArenaTurnWindow::UpdateContents();
            }
        }
    } else if (!playerTurn && !EntityManager::GetPlayer()->IsDead() && !killedBoss) {
        ArenaTurnWindow::SetIcon(1, IconManager::GetSandClockIcon());
        ArenaTurnWindow::SetDescription(
            SWPrintf(0x100, StringTable::GetString("PVP_TURN_OPPONENT"), {(const char32_t*)opponent->name}).c_str());
        ArenaTurnWindow::UpdateContents();
        opponentTurn = true;
    } else if (playerTurn) {
        ArenaTurnWindow::SetIcon(2, IconManager::GetSandClockIcon());
        ArenaTurnWindow::SetDescription(StringTable::GetString("ARENA_TURN_PLAYER"));
        ArenaTurnWindow::UpdateContents();
    }
    BottomBattleWindow::SetEnabled(paused, opponentTurn, autoCombat);
step:
    if (paused) return;
    switch (state) {
    case 2: WaitingForContinue(); break;
    case 3: WaveCleared(); break;
    case 4: StartPositioning(); break;
    case 5: UpdatePositions(); break;
    case 6: CombatReady(); break;
    case 9: CheckSpellCast(); break;
    case 10: SpellCast(); break;
    case 0xd: UpdateRunningToPos(); break;
    case 0xe: ArenaCompleted(); break;
    case 0x10: RunningOut(); break;
    default: break;
    }
}

// The valor (map item 0x249) and the PvP victory: the outro, the win's subtasks (0x42, 0x3f with
// the beaten hero's enemy class). UNVERIFIED (milestone 5): the pumpkin event's extra drops (task
// 0x77b; "pumkin_drop_count" / "pumkin_drop_chance", item 0x300); LoadingInternetWindow (online).
void PvPArenaCombat::OnValorReceived(unsigned valor) {
    waitingForServer = false;
    GameState::AddMapItemCollectionInfo((uint32_t)GameState::GetCurrentMapID(), 0x249, (int)valor);
    SoundsManager::PlaySound("ui_campaign_completed", 1, false);
    PvP::SetVictoryCount(PvP::GetVictoryCount() + 1);
    PlayerBattleInOutWindow::SetModeOutro();
    PlayerBattleInOutWindow::SetParameters(PlayerCompetitionWindow::GetSelectedPlayerGID(), true);
    PlayerBattleInOutWindow::Show();
    PlayerCompetitionWindow::SetDefeatedPlayer((uint32_t)GameState::mBossSpawnID);
    Tasks::CompleteSubtask(Tasks::kWinInRow, 1, 1);
    if (enemySquad && enemySquad->GetOwner())
        if (EntityData* d = enemySquad->GetOwner()->GetEntityData())
            Tasks::CompleteSubtask(Tasks::kKillEnemyClass, (uint32_t)d->enemyClass, 1);
}

// After the fight: the opponent's squad goes, the player's runs back, the camera to the arena.
void PvPArenaCombat::PostCombat() {
    if (enemySquad) {
        enemySquad->OnEndCombat();
        enemySquad->RemoveAllSoldiers();
        enemySquad = nullptr;
    }
    if (playerSquad) RunToSpawnPos();
    int x = GameState::mBossMapPositions[8];
    if (x != 0) {
        int y = GameState::mBossMapPositions[9];
        if (y != 0) {
            Map::TileCoordinatesToWorld(x, y);
            Render::CenterOn((float)x, (float)y, true);
        }
    }
    completed = true;
}

void PvPArenaCombat::WaveCleared() {
    timeMult = 1.f;
    SwitchState(2);
    if (wave != waveCount) return;
    SwitchState(0xe);
    if (BottomBattleWindow::IsVisible()) BottomBattleWindow::Hide();
}

void PvPArenaCombat::PlayerDied() {
    SwitchState(0xe);
    if (BottomBattleWindow::IsVisible()) BottomBattleWindow::Hide();
    if (ArenaTurnWindow::IsVisible()) ArenaTurnWindow::Hide();
    if (!TopCityWindow::IsVisible()) TopCityWindow::Show();
    PlayerCompetitionWindow::SetDefeatedPlayer(0);
}

// A win marks the opponent beaten (and when, before the PvP tutorial is done) and pays its valor;
// a real opponent's win is reported first and the valor comes with the answer (PvP::
// ReportLoadFinished -> OnValorReceived; offline PvPServer answers on the next frame). A loss
// resets the winning streak.
void PvPArenaCombat::ArenaCompleted() {
    if (ArenaTurnWindow::IsVisible()) ArenaTurnWindow::Hide();
    if (!TopCityWindow::IsVisible()) TopCityWindow::Show();
    if (BottomBattleWindow::IsVisible()) return;
    if (!playerSquad->GetOwner()->IsDead()) {
        opponent->defeated = 1;
        ++opponent->wins;
        if (!PvP::IsCompletedPvPTutorial()) opponent->lastFight = (uint32_t)Timer::GetGlobalTime();
        const unsigned t = (unsigned)opponent->type;
        if (3 < (unsigned)(opponent->gid + 4) && (t == 1 || t == 2 || t == 4 || t == 5)) {
            PvP::ReportStateToServer(PlayerCompetitionWindow::GetSelectedPlayerGID(), PvP::kStateWon);
            waitingForServer = true;
            // (LoadingInternetWindow "WEBVIEW_LOADING" with rep_pvp_valor_120 until the answer: online)
        } else {
            OnValorReceived(opponent->valor);
        }
    } else {
        Tasks::ResetSubtask(Tasks::kWinInRow, 1);
    }
    EndCombat();
}

void PvPArenaCombat::EndCombat() {
    goodGuys.clear();
    badGuys.clear();
    everyone.clear();
    if (playerSquad) {
        playerSquad->OnEndCombat();
        playerSquad->RearrangeSoldiers();
        for (unsigned i = 0; i < (unsigned)playerSquad->GetSoldierCount(); ++i)
            playerSquad->GetSoldier(i)->GetAI()->RemovedFromCombat();
        playerSquad = nullptr;
    }
    killedBoss = true;
    if (BottomBattleWindow::IsVisible()) BottomBattleWindow::Hide();
    SwitchState(0);
    if (!BuildingHovers::ArrowVisible()) return;
    if (GameState::secondTutorial < 0x9f) return;
    if (GameState::IsVirtualTutorial()) return;
    if (BuildingHovers::GetArrowClickCallbackID() != arrowCallback) return;
    BuildingHovers::HideArrow();
}
