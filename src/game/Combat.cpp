#include "game/Combat.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "engine/Timer.h"
#include "game/AI.h"
#include "game/AIState.h"
#include "game/Animation.h"
#include "game/BRand.h"
#include "game/BuildingHovers.h"
#include "game/Entity.h"
#include "game/EntityData.h"
#include "game/EntityManager.h"
#include "game/GameState.h"
#include "game/Locations.h"
#include "game/Map.h"
#include "game/MetaData.h"
#include "game/MetaExpression.h"
#include "game/PvP.h"
#include "game/SpawnPoint.h"
#include "game/Setting.h"
#include "game/Squad.h"
#include "gui/GUI.h"

namespace {

bool Busy(Entity* e) { return e->GetAI()->GetState() != 0; }
bool Gone(Entity* e) { return e->IsDead() || e->IsDying(); }

// The start of every BeginCombat: the squads, each other's owner as the target, and the fight's
// own random sequence (seeded from the enemy leader and the player's seed, so a fight replays the
// same way).
void LinkSquads(BaseCombat* c, BaseSquad* player, BaseSquad* enemy) {
    c->playerSquad = player;
    c->enemySquad = enemy;
    c->wp10 = nullptr;
    c->cancel = false;
    c->paused = false;
    c->pair = nullptr;
    player->OnBeginCombat(c);
    enemy->OnBeginCombat(c);
    player->SetCurrentTarget(enemy->GetOwner());
    enemy->SetCurrentTarget(player->GetOwner());
}

void SeedFight(BaseCombat* c) {
    Entity* boss = c->enemySquad->GetOwner();
    BRand::seed((uint32_t)(boss->tileY + boss->tileX + (int)boss->GetEntityData()->id + boss->GetHP() +
                           (int)GameState::playerSeed));
}

void FightMusic(BaseCombat* c) {
    // SoundsManager::PlayMusic("music_boss") for a boss, else "music_fight" once tutorial >= 0x17,
    // unless it already plays: milestone 6.
    (void)c;
}

void DeletePairs(std::vector<BaseCombat::Pair*>& pairs) {
    for (BaseCombat::Pair* p : pairs) delete p;
    pairs.clear();
}

// The end of UpdatePositions once both squads stand in place: a boss turns the fight into its boss
// fight, anyone else gets the first turn.
void PositionsReady(BaseCombat* c) {
    if (c->enemySquad->GetOwner()->IsBoss()) {
        c->SwitchToBossFight();
        return;
    }
    if (c->pairs.empty()) c->StartNewTurn();
    c->SwitchState(9);
}

}  // namespace

// ------------------------------------------------------------------------------------ BaseCombat

BaseCombat::BaseCombat() {
    float aggro = GameState::GetSetting("enemy_player_agro");
    playerAggro = aggro > 0.f ? (int)aggro : 0;
}

// @0x10de94: the pairs are freed; the squads and entities belong to their owners.
BaseCombat::~BaseCombat() { DeletePairs(pairs); }

void BaseCombat::Update(float) {
    if (!paused) {
        switch (state) {
        case 1: WaitingForBossBattleClick(); break;
        case 2: WaitingForContinue(); break;
        case 4: StartPositioning(); break;
        case 5: UpdatePositions(); break;
        case 6: CombatReady(); break;
        case 7: PlayerTurn(); break;
        case 8: EnemyTurn(); break;
        case 9: CheckSpellCast(); break;
        case 10: SpellCast(); break;
        case 0xc: UpdateEnemyPosition(); break;
        default: break;
        }
        return;
    }
    // Paused between turns: the helper arrow points at the enemy whose turn is next; tapping it (or
    // the enemy) continues the fight.
    if (!enemySquad || BuildingHovers::ArrowVisible() || GameState::secondTutorial <= 0x9e ||
        GameState::IsVirtualTutorial())
        return;
    if (playerSquad->GetOwner()->IsDead()) return;
    if (GameState::IsBossCombatActive() && !GameState::IsArenaCombatActive()) return;
    Entity* e = enemySquad->GetActiveSoldier();
    if (!e) return;
    if (!BuildingHovers::GetClickCallback() || BuildingHovers::GetArrowClickCallbackID() != arrowCallback) {
        AIBaseState* ai = EntityManager::GetPlayer()->GetAI();
        arrowCallback = BuildingHovers::SetArrowClickCallback(new GUI::Callback([ai, e] { ai->ClickedEntity(e); }));
    }
    BuildingHovers::ArrowAt(e->worldX, e->worldY - e->GetIdleHeight(), false, false, false, false, false, false,
                            false);
    BuildingHovers::PreventArrowAutoHide();
}

void BaseCombat::BeginCombat(BaseSquad* player, BaseSquad* enemy) {
    LinkSquads(this, player, enemy);
    SeedFight(this);
    SwitchState(2);
    FightMusic(this);
}

void BaseCombat::Pause(Entity* e) {
    paused = true;
    if (e) EndTurn();
}

void BaseCombat::Continue(int) {
    paused = false;
    if (GameState::secondTutorial != 0x91) return;
    if (BuildingHovers::GetArrowClickCallbackID() != arrowCallback) return;
    BuildingHovers::HideArrow();
}

void BaseCombat::AddGoodGuy(Entity* e) {
    if (std::find(goodGuys.begin(), goodGuys.end(), e) != goodGuys.end()) return;
    goodGuys.push_back(e);
    everyone.push_back(e);
}

void BaseCombat::AddBadGuy(Entity* e) {
    if (std::find(badGuys.begin(), badGuys.end(), e) != badGuys.end()) return;
    badGuys.push_back(e);
    everyone.push_back(e);
}

void BaseCombat::EndCombat() {
    DeletePairs(pairs);
    if (playerSquad) {
        playerSquad->OnEndCombat();
        playerSquad = nullptr;
    }
    if (enemySquad) {
        enemySquad->OnEndCombat();
        enemySquad = nullptr;
    }
    goodGuys.clear();
    badGuys.clear();
    for (size_t i = 0; i < everyone.size(); ++i) everyone[i]->GetAI()->RemovedFromCombat();
    everyone.clear();
    pair = nullptr;
    completed = true;
    if (turnCount == 0) turnCount = -1;
    SwitchState(0);
    if (!BuildingHovers::ArrowVisible()) return;
    if (GameState::secondTutorial < 0x9f) return;
    if (GameState::IsVirtualTutorial()) return;
    if (BuildingHovers::GetArrowClickCallbackID() == arrowCallback) BuildingHovers::HideArrow();
}

bool BaseCombat::KilledBoss(int spawnId) { return GameState::mBossSpawnID == spawnId ? killedBoss : false; }

bool BaseCombat::HasEntity(Entity* e) {
    if (playerSquad && playerSquad->HasEntity(e)) return true;
    if (enemySquad && enemySquad->HasEntity(e)) return true;
    if (pair && (pair->attacker == e || (pair->attacker && pair->attacker->HasEntity(e)))) return true;
    for (Pair* p : pairs)
        if (p->attacker == e || (p->attacker && p->attacker->HasEntity(e))) return true;
    for (const std::vector<Entity*>* list : {&goodGuys, &badGuys, &everyone})
        for (Entity* o : *list)
            if (o == e || (o && o->HasEntity(e))) return true;
    return false;
}

bool BaseCombat::HasSquad(BaseSquad* s) {
    if (playerSquad && playerSquad == s) return true;
    if (enemySquad) return s == enemySquad;
    return false;
}

bool BaseCombat::CanSpellCastNow() { return !spellQueued && state != 5 && state != 0x10 && state != 10; }

void BaseCombat::ForceBossFight(int kind, int arg) {
    Entity* player = EntityManager::GetPlayer();
    if (!player) return;
    Entity* boss = enemySquad->GetOwner();
    wp10 = AI::GetWaypoint(boss->tileX, boss->tileY, false);
    wp14 = AI::GetWaypoint(player->tileX, player->tileY, false);
    if (kind == 5) SwitchToArenaFight();
    else if (kind == 6) SwitchToPvPArenaFight(arg);
    else SwitchToBossFight();
    completed = false;
}

Entity* BaseCombat::GetBoss() { return enemySquad ? enemySquad->GetOwner() : nullptr; }

// The first turn's start, and every later one's: with a spell waiting the hero casts first (a
// pair of the hero and the enemy's active soldier is made if the turn has none yet).
void BaseCombat::WaitingForContinue() {
    if (!spellQueued) {
        SwitchState(4);
        return;
    }
    SwitchState(9);
    if (!pairs.empty()) return;
    Pair* p = new Pair();
    p->attacker = playerSquad->GetOwner();
    p->done = false;
    p->initiative = p->attacker->GetInitiative(0);
    p->targets.push_back(enemySquad->GetActiveSoldier());
    pairs.push_back(p);
}

void BaseCombat::StartPositioning() {
    if (GameState::tutorial == 0x80 && GameState::secondTutorial == 0x100 && BuildingHovers::ArrowVisible() &&
        BuildingHovers::GetArrowClickCallbackID() == arrowCallback)
        BuildingHovers::HideArrow();
    if (!cancel) {
        UpdateAttackPosition();
        return;
    }
    cancel = false;
    EndCombat();
}

void BaseCombat::CombatReady() {
    Pair* p = GetPair();
    if (!p) {
        EndTurn();
        return;
    }
    pair = p;
    int clas = p->attacker->GetEntityData()->clas;
    SwitchState(clas == 10 || clas == 5 ? 7 : 8);
}

void BaseCombat::PlayerTurn() {
    Pair* p = pair;
    Entity* target = p->targets[0];
    if (p->done) {
        if (Busy(p->attacker) || Busy(target)) return;
        SwitchState(4);
        return;
    }
    if (Busy(p->attacker) || Busy(target)) return;
    if (!Gone(target) && !Gone(p->attacker)) {
        p->attacker->GetAI()->StartedAttack(target);
        p->attacker->GetAI()->Attack(false);
    }
    p->done = true;
}

void BaseCombat::EnemyTurn() {
    Pair* p = pair;
    if (p->done) {
        if (Busy(p->attacker) || Busy(p->targets[0])) return;
        if (!savedWp10 || !savedWp14) {
            SwitchState(4);
            return;
        }
        // It had walked to a ranged target: back to its place.
        enemySquad->SetAttackWP(savedWp14);
        enemySquad->MoveTo(savedWp10);
        wp10 = savedWp10;
        wp14 = savedWp14;
        SwitchState(0xc);
        return;
    }
    if (Busy(p->attacker) || Busy(p->targets[0])) return;
    if (!Gone(p->targets[0]) && !Gone(p->attacker)) {
        Entity* target = p->attacker->GetCurrentTarget();
        if (!target) {
            // The hero ("enemy_player_agro" % of the time, or always when alone), else anyone else.
            size_t hero = 0;
            for (size_t i = 0; i < p->targets.size(); ++i)
                if (p->targets[i]->player) hero = i;
            if (playerAggro < BRand::Percent() && p->targets.size() > 1) {
                size_t i;
                do i = (unsigned)BRand::Percent() % p->targets.size();
                while (i == hero);
                target = p->targets[i];
            } else {
                target = p->targets[hero];
            }
            p->attacker->SetCurrentTarget(target);
        }
        if (target->GetAttackRanged() > 0 &&
            !AI::IsInMeleeRange(AI::GetWaypoint(p->attacker->tileX, p->attacker->tileY, false),
                                AI::GetWaypoint(target->tileX, target->tileY, false))) {
            // A ranged fighter stands back: the enemy squad walks to it first.
            savedWp14 = wp14;
            savedWp10 = wp10;
            int dir = -1;
            wp10 = AI::GetNearestWP(target->tileX, target->tileY, p->attacker->worldX, p->attacker->worldY, dir);
            wp14 = AI::GetWaypoint(target->tileX, target->tileY, false);
            enemySquad->SetAttackWP(wp14);
            enemySquad->MoveTo(wp10);
            SwitchState(5);
            std::printf("attacking ranged!");
            return;
        }
        p->attacker->GetAI()->StartedAttack(p->attacker->GetCurrentTarget());
        p->attacker->SetCurrentTarget(nullptr);
        p->attacker->GetAI()->Attack(false);
    }
    p->done = true;
}

// @0x10efb0: the boss fight on the boss's own map (loaded next frame; Map::SpawnEntities starts the
// BossCombat there): where the player came from and stood, the boss's spawn point and tile, the
// map's positions ("bat" list, else the defaults), a copy of the boss's meta, and the enemies: each
// wave's (entity id, count) entries, a 0 between waves (then a BossCombatWaves, type 4), the boss
// last. PORT (online removed): the fight's statistics request (OG2::MakeRequest(1, 1, spawn, male)).
void BaseCombat::SwitchToBossFight() {
    Entity* boss = enemySquad->GetOwner();
    const uint32_t map = boss->GetBossMapID();
    Map::EnqueueLoadMap((int)map, false, true, 0.f);
    GameState::mReturnMapID = (uint32_t)GameState::GetCurrentMapID();
    const SpawnPoint* sp = enemySquad->GetOwner()->GetSpawnPoint();
    GameState::mBossWaves.clear();
    GameState::mBossWavesHP.clear();
    GameState::mBossSpawnID = sp ? (int)sp->id : 0;   // PORT: the original reads a spawn point it assumes
    GameState::mLastBossXY[0] = wp10->x;
    GameState::mLastBossXY[1] = wp10->y;
    GameState::mLastBossPlayerXY[0] = wp14->x;
    GameState::mLastBossPlayerXY[1] = wp14->y;
    const int dir = playerSquad->GetOwner()->GetDirection();
    static const int kDefaults[12] = {3, 0x16, 5, 0x11, 0xb, 6, 8, 0xb, 7, 0xe, 5, 3};
    for (int i = 0; i < 12; ++i) GameState::mBossMapPositions[i] = kDefaults[i];
    GameState::mCurrentBossCombatType = 3;
    GameState::mLastPlayerDir = dir;
    if (const MetaData* bat = Locations::GetMapBat(map))
        for (unsigned i = 0; i < bat->GetChildrenCount(); ++i) {
            const MetaData* pair = bat->GetChild(i);
            GameState::mBossMapPositions[i * 2] = pair->GetChild(0)->GetInt();
            GameState::mBossMapPositions[i * 2 + 1] = pair->GetChild(1)->GetInt();
        }
    const char* text = enemySquad->GetOwner()->GetMetaExpression()->GetExpression();
    char* copy = new char[std::strlen(text) + 1];
    std::strcpy(copy, text);
    BossCombatData::mLastBossMeta = new MetaExpression(copy, true);
    if (const MetaData* waves = enemySquad->GetOwner()->GetBossWaveInfo()) {
        for (unsigned w = 0; w < waves->GetChildrenCount(); ++w) {
            if (w != 0) {
                GameState::mBossWaves.push_back(0);
                GameState::mBossWavesHP.push_back(0);
                GameState::mCurrentBossCombatType = 4;
            }
            const MetaData* wave = waves->GetChild(w);
            for (unsigned j = 0; j < wave->GetChildrenCount(); ++j) {
                const MetaData* entry = wave->GetChild(j);
                for (int k = 0; k < entry->GetChild(1)->GetInt(); ++k) {
                    GameState::mBossWaves.push_back((uint32_t)entry->GetChild(0)->GetInt());
                    GameState::mBossWavesHP.push_back(0);
                }
            }
        }
    }
    GameState::mBossWaves.push_back(enemySquad->GetOwner()->GetEntityData()->id);
    GameState::mBossWavesHP.push_back(0);
    EndCombat();
    SwitchState(0x11);
}

// @0x10fa40: the arena event's fight on the boss's map (loaded next frame): where the player came
// from and stood, the boss's spot, the arena's positions from the map's "bat" list, the boss's meta
// and the boss itself (ArenaCombat makes its waves).
void BaseCombat::SwitchToArenaFight() {
    Entity* boss = enemySquad->GetOwner();
    const uint32_t map = boss->GetBossMapID();
    Map::EnqueueLoadMap((int)map, false, true, 0.f);
    GameState::mReturnMapID = (uint32_t)GameState::GetCurrentMapID();
    const SpawnPoint* sp = enemySquad->GetOwner()->GetSpawnPoint();
    GameState::mBossWaves.clear();
    GameState::mBossSpawnID = sp ? (int)sp->id : 0;   // PORT: the original reads a spawn point it assumes
    GameState::mBossWavesHP.clear();
    GameState::mLastBossXY[0] = wp10->x;
    GameState::mLastBossXY[1] = wp10->y;
    GameState::mLastBossPlayerXY[0] = wp14->x;
    GameState::mLastBossPlayerXY[1] = wp14->y;
    const int dir = playerSquad->GetOwner()->GetDirection();
    GameState::mCurrentBossCombatType = 5;
    static const int kDefaults[12] = {3, 0x16, 5, 0x11, 0xb, 6, 8, 0xb, 0, 0, 6, 3};
    for (int i = 0; i < 12; ++i) GameState::mBossMapPositions[i] = kDefaults[i];
    GameState::mLastPlayerDir = dir;
    if (const MetaData* bat = Locations::GetMapBat(map))
        for (unsigned i = 0; i < bat->GetChildrenCount(); ++i) {
            const MetaData* pair = bat->GetChild(i);
            GameState::mBossMapPositions[i * 2] = pair->GetChild(0)->GetInt();
            GameState::mBossMapPositions[i * 2 + 1] = pair->GetChild(1)->GetInt();
        }
    const char* text = enemySquad->GetOwner()->GetMetaExpression()->GetExpression();
    char* copy = new char[std::strlen(text) + 1];
    std::strcpy(copy, text);
    GameState::mCurrentBossCombatType = 5;
    BossCombatData::mLastBossMeta = new MetaExpression(copy, true);
    GameState::mBossWaves.push_back(enemySquad->GetOwner()->GetEntityData()->id);
    GameState::mBossWavesHP.push_back(0);
    EndCombat();
    SwitchState(0x11);
}
// @0x10ff38: the PvP arena fight on map `map` (loaded next frame): where the player came from and
// stood, the arena's positions from the map's "bat" list, the opponent's warriors and hero.
void BaseCombat::SwitchToPvPArenaFight(int map) {
    Map::EnqueueLoadMap(map, false, true, 0.f);
    GameState::mReturnMapID = (uint32_t)GameState::GetCurrentMapID();
    if (Locations::LocationInfo* l = Locations::GetLocationWithMap((uint32_t)GameState::GetCurrentMapID()))
        if (l->mission && GameState::GetMapStartTime(l->startMap) == 0 && GameState::GetMapEndTime(l->startMap) != 0)
            GameState::mReturnMapID = 0;
    int spawn = 0;
    if (enemySquad && enemySquad->GetOwner() && enemySquad->GetOwner()->GetSpawnPoint())
        spawn = (int)enemySquad->GetOwner()->GetSpawnPoint()->id;
    GameState::mBossWaves.clear();
    GameState::mBossSpawnID = spawn;
    GameState::mBossWavesHP.clear();
    GameState::mLastBossXY[0] = GameState::mLastBossXY[1] = 0;
    GameState::mLastBossPlayerXY[0] = wp14->x;
    GameState::mLastBossPlayerXY[1] = wp14->y;
    const int dir = playerSquad->GetOwner()->GetDirection();
    GameState::mCurrentBossCombatType = 5;
    static const int kDefaults[12] = {3, 0x16, 5, 0x11, 0xb, 6, 8, 0xb, 0, 0, 6, 3};
    for (int i = 0; i < 12; ++i) GameState::mBossMapPositions[i] = kDefaults[i];
    GameState::mLastPlayerDir = dir;
    if (const MetaData* bat = Locations::GetMapBat((uint32_t)map))
        for (unsigned i = 0; i < bat->GetChildrenCount(); ++i) {
            const MetaData* pair = bat->GetChild(i);
            GameState::mBossMapPositions[i * 2] = pair->GetChild(0)->GetInt();
            GameState::mBossMapPositions[i * 2 + 1] = pair->GetChild(1)->GetInt();
        }
    GameState::mCurrentBossCombatType = 6;
    if (PvP::PlayerInfo* info = PvP::GetPlayerInfoByGID(PlayerCompetitionWindow::GetSelectedPlayerGID(), 0xff))
        for (const PvP::PlayerInfo::WarriorInfo& w : info->warriors) {
            if (w.id == 0) continue;
            GameState::mBossWaves.push_back(w.id);
            GameState::mBossWavesHP.push_back(w.hp);
        }
    GameState::mBossWaves.push_back(enemySquad->GetOwner()->GetEntityData()->id);
    GameState::mBossWavesHP.push_back(0);
    EndCombat();
    SwitchState(0x11);
}

void BaseCombat::CheckSpellCast() {
    Pair* p = GetPlayerPair();
    if (spellQueued && p) {
        pair = p;
        spellQueued = false;
        p->done = false;
        SwitchState(10);
        return;
    }
    SwitchState(6);
}

void BaseCombat::SpellCast() {
    Pair* p = pair;
    Entity* target = p->targets[0];
    if (p->done) {
        if (target->IsDead()) {
            f6c = true;
            EndTurn();
            paused = true;
            return;
        }
        if (Busy(p->attacker) || Busy(target)) return;
        SwitchState(4);
        f6c = true;
        return;
    }
    if (Busy(p->attacker) || Busy(target)) return;
    if (!target->IsDead() && !p->attacker->IsDead()) {
        p->attacker->GetAI()->StartedMagicAttack(target);
        p->attacker->GetAI()->MagicAttack(spell);
    }
    p->done = true;
}

void BaseCombat::UpdateEnemyPosition() {
    if (!enemySquad->IsInPosition(wp14, false)) return;
    savedWp14 = nullptr;
    savedWp10 = nullptr;
    SwitchState(4);
}

void BaseCombat::UpdatePositions() {
    if (!playerSquad->IsInPosition(wp10, false)) return;
    if (!enemySquad->IsInPosition(wp14, false)) return;
    PositionsReady(this);
}

// The player's squad walks up to the enemy: next to the enemy leader's tile, on the hero's side.
void BaseCombat::UpdateAttackPosition() {
    if (!wp10) {
        int dir = -1;
        Entity* player = playerSquad->GetOwner();
        Entity* boss = enemySquad->GetOwner();
        wp10 = AI::GetNearestWP(boss->tileX, boss->tileY, player->worldX, player->worldY, dir);
        wp14 = AI::GetWaypoint(boss->tileX, boss->tileY, false);
    }
    playerSquad->SetAttackWP(wp14);
    playerSquad->MoveTo(wp10);
    enemySquad->SetAttackWP(wp10);
    enemySquad->MoveTo(wp14);
    SwitchState(5);
}

void BaseCombat::StartNewTurn() {
    for (size_t i = 0; i < goodGuys.size(); ++i) {
        Entity* g = goodGuys[i];
        if (g->IsDead()) continue;
        Pair* p = new Pair();
        p->attacker = g;
        p->initiative = g->GetInitiative(0);
        if (g->player) p->initiative = g->GetInitiative(badGuys[0]->GetEntityData()->level);
        for (Entity* b : badGuys) p->targets.push_back(b);
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
        pairs.push_back(p);
        b->GetAI()->AddedToCombat();
        b->GetAnimController()->mult = 0.5555556f;
    }
    // std::sort with Pair_less_func (the higher initiative first). STLport's sort of 16 or fewer
    // elements is an insertion sort, which keeps equal ones in order; a fight has no more.
    std::stable_sort(pairs.begin(), pairs.end(),
                     [](const Pair* a, const Pair* b) { return a->initiative > b->initiative; });
    playerSquad->OnTurnStarted();
    enemySquad->OnTurnStarted();
    f6c = false;
}

void BaseCombat::EndTurn() {
    DeletePairs(pairs);
    for (size_t i = 0; i < goodGuys.size();) {
        if (!Gone(goodGuys[i])) {
            ++i;
            continue;
        }
        goodGuys[i]->f36 = false;
        goodGuys.erase(goodGuys.begin() + (long)i);
    }
    for (size_t i = 0; i < badGuys.size();) {
        if (!Gone(badGuys[i])) {
            ++i;
            continue;
        }
        OnEnemyDied();
        badGuys[i]->f36 = false;
        badGuys.erase(badGuys.begin() + (long)i);
    }
    playerSquad->OnTurnEnded();
    enemySquad->OnTurnEnded();
    if (turnCount > 0) --turnCount;
    if (enemySquad->GetAliveSoldierCount() != 0 && turnCount != 0) {
        SwitchState(2);
        return;
    }
    EndCombat();
}

BaseCombat::Pair* BaseCombat::GetPair() {
    for (Pair* p : pairs) {
        if (p->done) continue;
        bool dead = false;
        for (Entity* t : p->targets)
            if (t->IsDead()) {
                dead = true;
                break;
            }
        if (!dead) return p;
    }
    return nullptr;
}

BaseCombat::Pair* BaseCombat::GetPlayerPair() {
    for (Pair* p : pairs)
        if (p->attacker->player) return p;
    return nullptr;
}

// ----------------------------------------------------------------------------------- AggroCombat

void AggroCombat::BeginCombat(BaseSquad* player, BaseSquad* enemy) {
    LinkSquads(this, player, enemy);
    AIBaseState* ai = enemy->GetOwner()->GetAI();
    ai->SetSpeed(ai->GetBaseSpeed() * 1.33f);
    SeedFight(this);
    playerSquad->OnAggro();
    SwitchState(2);
    FightMusic(this);
}

void AggroCombat::UpdatePositions() {
    bool inPlace = playerSquad->IsInPosition(wp14, false) && enemySquad->IsInPosition(wp10, false);
    if (inPlace && enemySquad->GetOwner()->IsBoss() && !bossBlock) {
        playerSquad->GetOwner()->EnqueueAnimation("block", 0.f);
        bossBlock = true;
        bossBlockTime = (float)Timer::GetTime();
    }
    if (!inPlace) return;
    if (enemySquad->GetOwner()->IsBoss()) {
        if (!bossBlock) return;
        if (Timer::GetTime() - (double)bossBlockTime <= 0.4000000059604645) return;
    }
    AIBaseState* ai = enemySquad->GetOwner()->GetAI();
    ai->SetSpeed(ai->GetBaseSpeed());
    PositionsReady(this);
}

// The enemy comes to the hero, who stays where he is.
void AggroCombat::UpdateAttackPosition() {
    if (!wp10) {
        int dir = -1;
        Entity* player = playerSquad->GetOwner();
        Entity* boss = enemySquad->GetOwner();
        wp10 = AI::GetNearestWP(player->tileX, player->tileY, boss->worldX, boss->worldY, dir);
        wp14 = AI::GetWaypoint(player->tileX, player->tileY, false);
    }
    playerSquad->SetAttackWP(wp10);
    playerSquad->MoveTo(wp14);
    enemySquad->SetAttackWP(wp14);
    enemySquad->MoveTo(wp10);
    SwitchState(5);
}

// ------------------------------------------------------------------------------ RangeAggroCombat

void RangeAggroCombat::BeginCombat(BaseSquad* player, BaseSquad* enemy) {
    LinkSquads(this, player, enemy);
    playerSquad->ResetRangedWPs();
    SeedFight(this);
    playerSquad->OnAggro();
    SwitchState(2);
    FightMusic(this);
}

void RangeAggroCombat::UpdatePositions() {
    if (!playerSquad->IsInPosition(wp14, false)) return;
    if (!enemySquad->IsInPosition(wp10, false)) return;
    PositionsReady(this);
}

void RangeAggroCombat::UpdateAttackPosition() {
    if (!wp10) {
        int dir = -1;
        Entity* player = playerSquad->GetOwner();
        Entity* boss = enemySquad->GetOwner();
        wp10 = AI::GetNearestWP(player->tileX, player->tileY, boss->worldX, boss->worldY, dir);
        wp14 = AI::GetWaypoint(player->tileX, player->tileY, false);
    }
    playerSquad->SetAttackWP(wp10);
    playerSquad->MoveTo(wp14);
    enemySquad->SetAttackWP(wp14);
    enemySquad->MoveTo(wp10);
    SwitchState(5);
}
