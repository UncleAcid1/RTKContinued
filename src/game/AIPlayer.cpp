// AIWarrior (soldiers, and the base of the hero's AI) and AIPlayer (the hero). See AIState.h.
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>

#include "engine/IconManager.h"
#include "engine/Render.h"
#include "game/AI.h"
#include "game/AIState.h"
#include "game/Building.h"
#include "game/Combat.h"
#include "game/BuildingHovers.h"
#include "game/BuildingMovement.h"
#include "game/BuildingPlacement.h"
#include "game/Entity.h"
#include "game/EntityData.h"
#include "game/EntityManager.h"
#include "game/GameData.h"
#include "game/GameState.h"
#include "game/HiddenObjects.h"
#include "game/Items.h"
#include "game/MetaExpression.h"
#include "game/StringTable.h"
#include "hud/HUD.h"
#include "game/Map.h"
#include "game/Rand48.h"
#include "game/SpawnPoint.h"
#include "game/Squad.h"
#include "game/Tasks.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "engine/Timer.h"
#include "windows/Windows.h"
#include "game/Tasks.h"

namespace {

Render::Sprite* g_marker = nullptr;   // 0x610b94 the tap marker
float g_markerTime = 0.f;             // 0x610b98 seconds until it goes
int g_tapX = 0, g_tapY = 0;           // 0x610ba0 0x610ba4 the last map tap off the city (screen)
float g_tapTime = 0.f;                // 0x610b9c its time
float g_fogTime = 0.f;                // 0x610ba8 AIPlayer::Update's 0.25 s pass

// FUN_000ef04c / FUN_000eebb4: a marker under the pointer (layer 0xc, centred, bottom on it), shown
// 1.5 s: "goto" for a reachable tap, "blocked" otherwise.
void MarkerAtPointer(Render::Texture* tex) {
    int x = 0, y = 0;
    WindowManager::GetMousePosition(x, y);
    Map::MouseCoordinatesToWorld(x, y);
    Render::RemoveSprite(g_marker);
    Render::Sprite* s = Render::CreateSprite(tex, 0xc, false, false);
    g_marker = s;
    Render::SetPosition(s, (float)x + s->w * -0.5f, (float)y + s->h * 0.5f, 0.2f);
    g_markerTime = 1.5f;
}

void GotoMarker() { MarkerAtPointer(IconManager::GetPlayerMarkerGoto()); }      // FUN_000ef04c
void BlockedMarker() { MarkerAtPointer(IconManager::GetPlayerMarkerBlocked()); } // FUN_000eebb4

// FUN_000ef334: the "goto" marker on a tile.
void GotoMarkerAt(int tx, int ty) {
    Map::TileCoordinatesToWorld(tx, ty);
    Render::RemoveSprite(g_marker);
    Render::Sprite* s = Render::CreateSprite(IconManager::GetPlayerMarkerGoto(), 0xc, false, false);
    g_marker = s;
    Render::SetPosition(s, (float)tx + s->w * -0.5f + 42.f, ((float)ty + s->h * 0.5f) - 21.f, 0.2f);
    g_markerTime = 1.5f;
}

// The attack marker on a tile (AIPlayer::ClickedEntity): three quarters of its size, layer 7.
void AttackMarkerAt(int tx, int ty) {
    Map::TileCoordinatesToWorld(tx, ty);
    Render::RemoveSprite(g_marker);
    Render::Sprite* s = Render::CreateSprite(IconManager::GetPlayerMarkerAttack(), 7, false, false);
    g_marker = s;
    s->w *= 0.75f;
    s->h *= 0.75f;
    Render::SetPosition(s, (float)tx + s->w * -0.5f + 42.f, ((float)ty + s->h * 0.5f) - 21.f, 0.2f);
    g_markerTime = 1.5f;
}

}  // namespace

// --------------------------------------------------------------------------------------- AIWarrior

AIWarrior::AIWarrior(Entity* e) : AIBaseState(e) {
    SetCustomWalkAnimation("run");
    f78 = true;
    deathDamage = 1;
    float s = entity->GetEntityData()->clas == 10 ? 239.4f : 180.f;
    f7c = 0;
    speed = s;
    baseSpeed = s;
    f79 = false;
    lastHpTimer = hitTimer = deathTimer = blockTimer = f90 = f8c = f94 = 0.f;
    f74 = 1.f;
}

void AIWarrior::Update(float dt) {
    // The combat timers: what the attacker's swing does to this fighter when it lands. An archer's
    // target gets it from the arrow instead (Entity::OnProjectileFired).
    auto ran = [dt](float& t) {
        if (!(t > 0.f)) return false;
        t += dt * -1.8f;
        return t < 0.f;
    };
    auto land = [this](int kind) {
        if (linked->GetAttackRanged() > 0) {
            linked->OnProjectileFired(entity, kind);
            return false;
        }
        return true;
    };
    if (ran(hitTimer) && land(0)) Hit(0.f);
    if (deathTimer > 0.f) {
        // (A monster the taming net is aimed at waits for the throw.)
        if (entity->GetSpawnPointID() == 0 || entity->GetSpawnPointID() != BottomBattleWindow::GetNetTargetSpawnID())
            deathTimer += dt * -1.8f;
        if (deathTimer < 0.f && land(1)) Death(0.f);
    }
    if (ran(lastHpTimer)) {
        // (Entity::PlayHitSound unless IsTameTutorial: milestone 6.)
        entity->SetHP(1);
        lastHpTimer = 0.f;
    }
    if (ran(blockTimer) && land(2)) Block(0.f);
    if (ran(f90)) MagicHit(0.f, f78, f74, f79, (unsigned)f7c);
    if (ran(f8c)) MagicDeath(0.f, f78, f74, f79, (unsigned)f7c);
    if (ran(f94)) MagicBlock(0.f, f78, f74, f79, (unsigned)f7c);
    AIBaseState::Update(dt);
}

void AIWarrior::WalkTo(int x, int y) {
    // UNVERIFIED (milestone 5, arenas): in an arena combat (types 5, 6) the waypoint under the entity gets
    // weight 1 first (no waypoint: an error, SetPos(x, y) and UpdateLastTarget).
    AIBaseState::WalkTo(x, y);
}

void AIWarrior::WalkCompleted() {
    if (GameState::GetCurrentMapID() == 0 && entity->GetWorkplace()) {
        ChangeState(0x1f);
        StartTraining();
    } else if (BaseSquad* sq = entity->GetSquad()) {
        if (entity->ReportReadiness()) sq->SetReady(entity);
    }
    if (GameState::tutorial == 5) {
        GameState::tutorial = 6;
    } else if (GameState::tutorial == 100) {
        Tasks::CompleteSubtask(Tasks::kTrainWorkerInBuilding, 0x8e, 1);
        Tasks::CompleteSubtask(Tasks::kTrainAnyWorker, 1, 1);
        GameState::tutorial = 0x65;
    }
    // UNVERIFIED (milestone 5, arenas): in an arena combat (types 5, 6) the waypoint under it gets weight 1000.
}

void AIWarrior::StartTraining() {
    float wx = 0.f, wy = 0.f;
    entity->GetWorkplace()->GetParkingSpot(0, wx, wy);
    entity->SetWorldPos(wx, wy);
    entity->SetDirection(entity->GetWorkplace()->IsMirrored() ? 3 : 5);
    entity->SetAnimationOnce("attack1", false, false, false, true);
    ChangeState(0x1e);
}

void AIWarrior::AssignToJob(Map::Building* b) {
    if (entity->GetOfflineMode()) {
        StartTraining();
        return;
    }
    int tx = 0, ty = 0;
    b->GetWorkTile(tx, ty);
    AI::Waypoint* wp = AI::GetWaypoint(tx, ty, false);
    path.clear();
    WalkTo(wp->x, wp->y);
    ChangeState(0x1d);
}

void AIWarrior::RemoveFromJob() {
    int tx = entity->tileX, ty = entity->tileY;
    entity->SetWorldPos((ty % 2 == 1 ? 42.f : 0.f) + (float)tx * 84.f + 42.f, (float)ty * 42.f * 0.5f);
    entity->SetDirection(1);
    entity->SetAnimationP("idle_1", true, false, false);
    ChangeState(0);
}

void AIWarrior::StopMovement() {
    if (path.empty()) return;
    entity->GetAI()->Reset(true);
    if (entity->IsWalkingOnPatrol()) entity->OnWalkComplete();
}

void AIWarrior::AnimEnded() {
    int st = state;
    if (st == 2 && entity->attackRanged == 0) {
        state = 0;
        return;
    }
    if (st == 3) {
        state = 0;
        return;
    }
    if (st == 4) {
        state = 0;
        entity->OnDeath(linked);
        st = state;
    }
    if (st != 0x1e) return;
    // Training: a random swing, block or rest. Sounds: milestone 6 (the random draws are kept).
    switch (Rand48::lrand48() % 3) {
    case 0: {
        entity->SetAnimationOnce("attack1", false, false, false, true);
        const std::string& n = entity->GetEntityData()->name;
        if (n.find("fat") != std::string::npos) {
            Rand48::lrand48();   // "battle_attack_soldier_morningstar1"/"2"
        } else if (n.find("skinny") != std::string::npos && n.find("melee") != std::string::npos) {
            Rand48::lrand48();   // "battle_attack_soldier_sword1"/"2"
        } else if (n.find("skinny") != std::string::npos && n.find("archer") != std::string::npos) {
            Rand48::lrand48();   // "battle_attack_soldier_bow1"/"2"
        }
        // (otherwise "battle_attack_soldier_morningstar1"; every attack sound plays 0.4 s late)
        break;
    }
    case 1:
        entity->SetAnimationOnce("block", false, false, false, true);   // ("battle_block_soldier")
        break;
    case 2:
        entity->SetAnimationOnce("idle_1", false, false, false, true);
        break;
    }
}

// ---------------------------------------------------------------------------------------- AIPlayer

AIPlayer::AIPlayer(Entity* e) : AIWarrior(e) {
    actionTarget = nullptr;
    baseSpeed = 239.4f;
    action = 0;
    BaseAI::SetSpeed(239.4f);
    SetCustomWalkAnimation("run");
    arrivalDir = -1;
    hidden = new HiddenObjects();
    movingSquad = false;
}

AIPlayer::~AIPlayer() {
    if (g_marker) g_marker = Render::RemoveSprite(g_marker);
    delete hidden;
}

void AIPlayer::Update(float dt) {
    g_fogTime += dt;
    if (0.25f < g_fogTime) {
        if (Render::Sprite* sp = entity->GetSprite()) Map::RemoveFog(sp->x, sp->y - sp->h, sp->x + sp->w, sp->y);
        hidden->ShowAll();
        // In combat the decorations in front of the squad (lower on screen, covering a member's
        // feet) fade to 0.4.
        if (entity->f36) {
            for (int x = entity->tileX - 3; x <= entity->tileX + 2; ++x) {
                for (int y = entity->tileY; y <= entity->tileY + 4; ++y) {
                    Map::Decor* d = Map::GetDecoration(x, y);
                    if (!d || !d->sprite || (d->data && d->data->layer != 0)) continue;
                    BaseSquad* sq = entity->GetSquad();
                    for (unsigned i = 0; i < (unsigned)sq->GetSoldierCount(); ++i) {
                        Entity* s = sq->GetSoldier(i);
                        if (s->GetSprite() && d->sprite->z < s->GetSprite()->z &&
                            Render::HasPixelAt(d->sprite, s->worldX, s->worldY - 21.f)) {
                            hidden->HideObject(d, 0.4f);
                            break;
                        }
                    }
                }
            }
        }
        g_fogTime = 0.f;
    }
    float t = g_markerTime;
    if (GameState::IsCombatActive() && (entity->GetSquad()->IsAnyoneInCombat() || entity->f38)) t = 0.f;
    g_markerTime = t - dt;
    if (g_marker && t - dt < 0.f) g_marker = Render::RemoveSprite(g_marker);
    AIWarrior::Update(dt);
    hidden->Update(dt);
}

void AIPlayer::ClickedEntity(Entity* e) {
    // (While a CombatManager exists, GameState::g_Combat, it takes the tap: never in 5.11.)
    if (!GameState::IsPlayerCity()) return;
    if (entity->IsRetreating()) return;
    if (!GUI::CanInteractWith(e)) return;
    // (BottomScreenshotWindow, not ported, is never visible)
    std::puts("AIPlayer::ClickedEntity()");
    if (entity == e) return;
    if (!TryToInterruptJob()) return;
    if ((unsigned)(action - 6) < 2) return;
    if (entity->IsDead() && GameState::GetCurrentMapID() != 0 && GameState::GetCurrentMapID() != 0xb) {
        HealSquadWindow::Show();
        return;
    }
    Entity::UpdateSafePos(entity);
    // UNVERIFIED (milestone 4h): the tame tutorial (GameState::IsTameTutorial) hides the arrow and
    // reports spawn point 0x1e to TutorialSender.
    int& st = GameState::secondTutorial;
    if (st == 0x91 || st == 0x8e) {
        BuildingHovers::HideWorldDialog(nullptr);
        BuildingHovers::HideArrow();
    }
    if (st == 0x203 && e->IsEnemy()) st = 0x204;
    else if (st == 0x206 && e->IsEnemy()) st = 0x207;
    else if (st == 0x20e && e->IsEnemy()) st = 0x20f;
    if (st == 0x21a && e->IsBoss()) st = 0x9f;

    MetaExpression* meta = e->GetMetaExpression();
    if (meta && meta->FindByType(kExpPvpSpawn)) {
        // A PvP opponent's stand-in: the hero walks next to it (action 8 on arrival).
        int dir = 0;
        AI::Waypoint* wp = AI::GetNearestFreeWP(e->tileX, e->tileY, entity->worldX, entity->worldY, dir, entity);
        if (!wp) return;
        ChangeState(0x20);
        actionTarget = e;
        actionArg = 0;
        action = 8;
        // "interact_(fe)male_player_move(_run)": milestone 6.
        entity->GetSquad()->MoveTo(wp);
        BuildingHovers::HideWorldDialog(nullptr);
        return;
    }
    if (GameState::IsCombatActive()) {
        // A tap on a monster in a fight: the fight goes on (the arenas: against that monster).
        if (e->GetEntityData()->clas != 9) return;
        BuildingHovers::HideWorldDialog(entity);
        BaseCombat* c = GameState::GetActiveCombat();
        if (c->GetType() == 5 || c->GetType() == 6) {
            if (!entity->GetAI()->SetTarget(AI::GetWaypoint(e->tileX, e->tileY, false), true)) return;
            c->GetEnemySquad()->SetActiveSoldier(e);
            c->Continue();
        } else {
            // (A boss fight: BottomBattleWindow::SetSelectedButton(0), milestone 5.)
            c->Continue();
        }
    }
    if (!entity->f36) {
        // Somebody with something to say: the hero walks up to it (action 2 on arrival).
        if ((e->GetTalkTask() != 0 || e->GetSpecialTalk() != 0) && !GameState::IsCombatActive()) {
            if (GameState::GetCurrentMapID() == 0) {
                Map::Patch* p = Map::GetPatchForCoordinates(e->tileX, e->tileY, false);
                if (p && !p->owned) return;
            }
            AI::Waypoint* near = AI::GetNearestWP(e->tileX, e->tileY, entity->worldX, entity->worldY, arrivalDir);
            if (!near) return;
            actionWP = near;
            action = 2;
            actionTarget = e;
            // (OG2::MakeRequest for its spawn point: online statistics.)
            if (GameState::tutorial == 0x51) near = AI::GetWaypoint(0x1b, 0x15, false);
            GotoMarkerAt(e->tileX, e->tileY);
            // "interact_(fe)male_player_move(_run)": milestone 6.
            BuildingHovers::HideWorldDialog(nullptr);
            entity->GetSquad()->MoveTo(near);
            return;
        }
        // A corpse that still holds something: the hero walks onto it (action 6 on arrival).
        if (e->HasCorpseItem() && !GameState::IsCombatActive()) {
            AI::Waypoint* wp = AI::GetWaypoint(e->tileX, e->tileY, false);
            if (!wp) return;
            ChangeState(0x20);
            actionTarget = e;
            action = 6;
            actionArg = 0;
            GotoMarker();
            entity->GetSquad()->MoveTo(wp);
            BuildingHovers::HideWorldDialog(nullptr);
            return;
        }
    }
    AI::Waypoint* wp = AI::GetWaypoint(e->tileX, e->tileY, false);
    if (!e->IsDead() && !GameState::IsCombatActive() && wp && entity->GetAI()->SetTarget(wp, true)) {
        e->OnClick();
        if (e->GetEntityData()->clas == 9 && (!e->GetSpawnPoint() || !e->GetSpawnPoint()->npc)) {
            // PORT (until milestone 5's BossCombat): a boss cannot be attacked, see AIEnemy::CheckAggro.
            if (e->IsBoss()) return;
            // A monster: the attack marker on its tile, and the fight begins.
            // "interact_(fe)male_player_attack": milestone 6.
            AttackMarkerAt(e->tileX, e->tileY);
            GameState::SetCombat(new BaseCombat());
            if (e->IsOnPatrol()) {
                e->RemovePatrol();
                e->GetAI()->Reset(true);
            }
            e->CreateSquad();
            if (!current) UpdateLastTarget();
            BuildingHovers::HideWorldDialog(nullptr);
            GameState::GetActiveCombat()->BeginCombat(entity->GetSquad(), e->GetSquad());
            return;
        }
        BlockedMarker();
        return;
    }
    if (!GameState::IsCombatActive()) {
        BlockedMarker();
        movingSquad = true;
        entity->GetSquad()->MoveTo(wp);
        movingSquad = false;
    }
}

void AIPlayer::ClickedTile(int x, int y, int, int) {
    // (A CombatManager would take the tap, OnTileClicked: the siege system, dead in 5.11.)
    if (entity->IsRetreating()) return;
    // (BottomScreenshotWindow, not ported, is never visible)
    int tut = GameState::tutorial;
    if (tut < 8) return;
    if (tut == 0x17) {
        if (!GameState::TaskCompleted(0x771)) return;
        tut = GameState::tutorial;
    }
    if (tut < 0x18 && action == 1) return;
    if (GameState::secondTutorial == 0x87 || GameState::secondTutorial == 0xa0) return;
    if (GameState::IsCombatActive() && GameState::GetActiveCombat()->GetType() > 2) return;
    if (!TryToInterruptJob()) return;
    if (entity->IsDead() && GameState::GetCurrentMapID() != 0 && GameState::GetCurrentMapID() != 0xb &&
        GameState::GetCurrentMapID() != 0x74) {
        HealSquadWindow::Show();
        return;
    }
    Entity* e = EntityManager::GetEntityAtXY(x, y);
    if (e && !e->IsDead()) {
        ClickedEntity(e);
        return;
    }
    if (GameState::GetCurrentMapID() == 0xd) {
        float dx, dy;
        if (BuildingHovers::HasDroppedItem(0x254, dx, dy)) return;   // (the item must be taken first)
        if ((unsigned)(GameState::tutorial - 0x14) < 2) return;
    }
    if (GameState::GetCurrentMapID() == 0) {
        // In the city only after its tutorial, not over the shop or an edit mode, on owned land and
        // not onto an expensive tile (buildings, layer-0 decorations).
        if (GameState::secondTutorial != 0x100) return;
        if (ShopWindow::IsVisible()) return;
        if (BuildingPlacement::Activated()) return;
        if (BuildingMovement::Activated()) return;
        Map::Patch* p = Map::GetPatchForCoordinates(x, y, true);
        if (!p || !p->owned) return;
        AI::Waypoint* wp = AI::GetWaypoint(x, y, false);
        if (wp && 1.f < wp->weight) return;
    }
    Entity::UpdateSafePos(entity);
    action = 0;
    if (entity->GetSquad()->IsAnyoneInCombat()) return;
    if (entity->f38) return;
    g_marker = Render::RemoveSprite(g_marker);
    // UNVERIFIED (milestones 4g/4h): GameState::EndActiveCombat(true), BuildingHovers::HideWorldDialog.
    // Tutorial steps 0xb and 0xd: the arrow points at the entity at (4, 6) / (5, 0xd), whose tap
    // goes to ClickedEntity.
    for (int step : {0xb, 0xd}) {
        if (GameState::tutorial != step) continue;
        Entity* t = step == 0xb ? EntityManager::GetEntityAtXY(4, 6) : EntityManager::GetEntityAtXY(5, 0xd);
        if (!t) continue;
        GUI::SetInteractionObjectLock(t, nullptr);
        BuildingHovers::SetArrowClickCallback(new GUI::Callback([this, t] { ClickedEntity(t); }));
        BuildingHovers::ArrowAt(t->worldX, t->worldY - t->GetIdleHeight(), false, false, false, false, false,
                                false, false);
    }
    AI::Waypoint* wp = AI::GetWaypoint(x, y, false);
    bool walk = true;
    if (!wp || (wp->x == entity->tileX && wp->y == entity->tileY)) {
        BlockedMarker();
        wp = AI::GetNearestWPFromNonWP(x, y, nullptr);
        walk = wp != nullptr;
    } else {
        GotoMarker();
    }
    if (walk) {
        movingSquad = true;
        entity->GetSquad()->MoveTo(wp);
        movingSquad = false;
    }
    if (GameState::GetCurrentMapID() != 0 && GameState::GetCurrentMapID() != 0xb) {
        g_tapTime = (float)Timer::GetTime();
        WindowManager::GetMousePosition(g_tapX, g_tapY);
    }
    // UNVERIFIED (milestone 6): when the hero walks, "interact_male/female_player_move" (or "..._run"
    // while speed running).
}

void AIPlayer::Retreat() {
    ClearPath();
    entity->SetRetreating(true);
    if (Map::GetLastPlayerX() == entity->tileX && Map::GetLastPlayerY() == entity->tileY) {
        if (Entity* m = EntityManager::IsPlayerInAggroRange()) {
            int range = m->GetSpawnPoint()->aggroRange;
            entity->GetSquad()->MoveTo(AI::GetRandomWaypointInRange(m->tileX, m->tileY, range + 1, range, false, nullptr, false));
        }
    } else {
        entity->GetSquad()->MoveTo(AI::GetWaypoint(entity->spawnX, entity->spawnY, false));
    }
    action = 0;
    entity->OnRetreat();
}

void AIPlayer::AnimEnded() {
    if (action == 7) ProcessAction();
    AIWarrior::AnimEnded();
}

void AIPlayer::Reset(bool) {
    AIBaseState::Reset(true);
    autoInteraction = false;
    action = 0;
}

void AIPlayer::WalkCompleted() {
    if (GameState::tutorial == 0xf) {
        arrivalDir = 7;
        entity->SetDirection(7);
        arrivalDir = -1;
    } else if (arrivalDir != -1) {
        entity->SetDirection(arrivalDir);
        arrivalDir = -1;
    }
    // Someone else standing on the hero's tile steps aside.
    Entity* o = EntityManager::GetNonPlayerEntityAtXY(entity->tileX, entity->tileY);
    if (o && o != entity && !o->f39) {
        if (AI::Waypoint* wp = AI::GetRandomWaypointInRange(entity->tileX, entity->tileY, 2, 1, false, nullptr, false))
            o->GetAI()->WalkTo(wp);
    }
    ProcessAction();
    entity->GetSquad()->SetReady(entity);
    if (entity->IsRetreating()) {
        // Still within a monster's reach: on, out of its range; else the retreat is over.
        if (Entity* m = EntityManager::IsPlayerInAggroRange()) {
            int range = m->GetSpawnPoint()->aggroRange;
            entity->GetSquad()->MoveTo(AI::GetRandomWaypointInRange(m->tileX, m->tileY, range + 1, range, false, nullptr, false));
        } else {
            entity->f38 = false;
            entity->SetRetreating(false);
        }
    }
    // (In an arena combat, types 5 and 6, the waypoint under him gets weight 1000: milestone 5.)
}

void AIPlayer::UnableToWalk(int x, int y) {
    if (!movingSquad) {
        if (Map::Decor* d = entity->GetWorkplaceDecoration()) {
            d->f50 = 0;
            actionWP = nullptr;
            ChangeState(0);
            autoInteraction = false;
            action = 0;
        } else if (action == 2 || action == 5) {
            ChangeState(0);
            autoInteraction = false;
            action = 0;
        }
    } else {
        // The way is only shut by a quest's blockage: the hero walks up to it (action 4 shows its
        // message on arrival).
        Map::Decor* blocker = nullptr;
        if (SetBlockedPathTarget(AI::GetWaypoint(x, y, false), &blocker) && blocker) {
            actionTarget = blocker;
            action = 4;
        }
    }
    BlockedMarker();
}

bool AIPlayer::TryToInterruptJob() {
    Map::Decor* d = entity->GetWorkplaceDecoration();
    if (action == 3 && d && !GameState::IsTutorial()) entity->GetWorkplaceDecoration()->RemoveWorker();
    // A job without a required item is cancelled and its price returned.
    if (d && IsWorking() && !GameState::IsTutorial() && d->GetRequiredItem() == 0) {
        d->ReturnCost();
        d->RemoveWorker();
        BuildingHovers::Update(0.f, true);
    }
    if (action == 3) return false;
    return !IsWorking();
}

void AIPlayer::RemoveActionMarker() {
    if (!g_marker) return;
    g_marker = Render::RemoveSprite(g_marker);
    g_markerTime = 0.f;
}

void AIPlayer::StopMovement() {
    if (entity->IsRetreating()) return;
    if (path.empty()) return;
    if (entity->GetSquad()->IsAnyoneInCombat()) return;
    if (entity->f38) return;
    if (action == 3) {
        if (entity->GetWorkplaceDecoration() && !GameState::IsTutorial())
            entity->GetWorkplaceDecoration()->RemoveWorker();
        if (action == 3) return;   // (RemoveWorker's RemoveFromJob clears the action)
    }
    if (IsWorking()) return;
    BaseSquad* sq = entity->GetSquad();
    for (unsigned i = 0; i < (unsigned)sq->GetSoldierCount(); ++i) sq->GetSoldier(i)->GetAI()->Reset(true);
}

void AIPlayer::OnMapUnload() { hidden->Clear(false); }

// @0xee7a0 AIPlayer::GetBestDecorationActionPoint
unsigned AIPlayer::GetBestDecorationActionPoint(Map::Decor* d) {
    if (d->GetActionPointCount() < 2) return 0;
    // The action point with the shortest way (one without a way counts as the farthest).
    unsigned best = 0, bestSteps = 0xffffffff;
    for (unsigned i = 0; i < (unsigned)d->GetActionPointCount(); ++i) {
        int x = 0, y = 0;
        d->GetActionPoint(x, y, (int)i);
        unsigned steps = (unsigned)GetDistanceToTarget(AI::GetWaypoint(x, y, false));
        if (steps < bestSteps) {
            best = i;
            bestSteps = steps;
        }
    }
    return best;
}

// @0xee858 AIPlayer::SendGoblinToWork
void AIPlayer::SendGoblinToWork(Map::Decor* d) {
    Entity* goblin = GameState::GetGoblin();
    if (!goblin || goblin->GetWorkplaceDecoration()) {
        // "Find" (FUN_000eeb70) leads to the goblin, "buy" (FUN_000eeb60) home to the hire window.
        PopupSelectionWindow::Show(StringTable::GetString("MESSAGE_GOBLIN_NOT_AVAILABLE"),
                                   StringTable::GetString("GOBLIN_NOT_AVAILABLE_DESC"),
                                   StringTable::GetString("PERFORM_FIND"), IconManager::GetIcon("gold_find", false),
                                   [] {
                                       PopupSelectionWindow::Hide();
                                       TaskListWindow::FindGoblin();
                                   },
                                   StringTable::GetString("PERFORM_BUY"), IconManager::GetIcon("gold_add", false), [] {
                                       PopupSelectionWindow::Hide();
                                       TaskListWindow::BuyGoblin();
                                   });
        PopupSelectionWindow::SetGoblinIcon();
        return;
    }
    goblin->SetCurrentMap((int)Map::GetMapID());
    int ax = 0, ay = 0;
    d->GetActionPoint(ax, ay, 0);
    AI::Waypoint* wp = AI::GetRandomWaypointInRange(ax, ay, 1, 0, false, nullptr, false);
    if (!wp) wp = AI::GetWaypoint(entity->tileX, entity->tileY, false);
    EntityManager::SpawnEntityAt(goblin, (unsigned)wp->x, (unsigned)wp->y, false, false);
    d->AssignWorker(goblin);
    d->collectStart = Timer::GetGlobalTime();
    // Someone standing on the action point steps aside.
    Entity* o = EntityManager::GetNonPlayerEntityAtXY(ax, ay);
    if (o && o != entity && !o->f39)
        if (AI::Waypoint* to = AI::GetRandomWaypointInRange(ax, ay, 1, 0, false, nullptr, false)) o->GetAI()->WalkTo(to);
}

// @0xeee64 AIPlayer::SearchDecoration
void AIPlayer::SearchDecoration(Map::Decor* d) {
    int sx = (int)entity->worldX, sy = (int)entity->worldY;
    Map::WorldCoordinatesToScreen(sx, sy);
    if (sx < 0x32 || sx >= Render::ScreenWidth() - 0x31 || sy < 0x32 || sy >= Render::ScreenHeight() - 0x31)
        Render::CenterOn(entity->worldX, entity->worldY);   // (over 0.4 s on the original)
    ChangeState(0x21);
    entity->OnStartedToWork();
    Map::Decor* job = entity->GetWorkplaceDecoration();
    if (!job) {
        entity->SetWorkplaceDecoration(d);
        d->worker = entity;
        job = d;
    }
    job->WorkStarted();
    unsigned time = 0;
    const char* anim = entity->GetWorkplaceDecoration()->GetPlayerSearchAnimation(&time);
    if (!anim) entity->SetAnimationP("search", false, false, false);
    else entity->SetAnimationMult(anim, (int)time, false, false, true);
    int w = 0, h = 0, tx = 0, ty = 0;
    d->GetBuildZone(w, h);
    d->GetStartTile(tx, ty);
    TurnTo(entity->tileX, entity->tileY, tx, ty, w, h);
    BuildingHovers::Update(0.f, true);
}

// @0xef104 AIPlayer::ClickedDecoration
// @0xf0418 AIPlayer::ClickedPortal
void AIPlayer::ClickedPortal(Portal* p) {
    if (entity->IsRetreating()) return;
    if (!GUI::CanInteractWith(p)) return;
    // (BottomScreenshotWindow, the screenshot mode: not in the port.)
    if (!TryToInterruptJob()) return;
    if (GameState::IsCombatActive()) return;
    if (entity->IsDead() && GameState::GetCurrentMapID() != 0) {
        HealSquadWindow::Show();
        return;
    }
    if (GameState::tutorial < 8) return;
    if (GameState::tutorial == 9) BuildingHovers::HideArrow();
    // (tutorial 0x17: TutorialSender::AddTutorialStep(0x52c), online)
    AI::Waypoint* wp = AI::GetWaypoint(p->walkX, p->walkY, false);
    if (!wp) return;
    actionTarget = p;
    action = 1;
    // UNVERIFIED (milestone 6): "interact_(fe)male_player_move(_run)".
    GotoMarkerAt(p->walkX, p->walkY);
    entity->GetSquad()->MoveTo(wp);
    BuildingHovers::HideWorldDialog(nullptr);
}

void AIPlayer::ClickedDecoration(Map::Decor* d) {
    // (A CombatManager would take the tap, OnDecorClicked 0x12e494: the siege system, dead in 5.11.)
    if (entity->IsRetreating()) return;
    // (BottomScreenshotWindow, the screenshot mode: not in the port.)
    if (!TryToInterruptJob()) return;
    if (GameState::IsCombatActive()) return;
    if (entity->IsDead() && GameState::GetCurrentMapID() != 0) {
        HealSquadWindow::Show();
        return;
    }
    unsigned point = GetBestDecorationActionPoint(d);
    int ax = 0, ay = 0;
    d->GetActionPoint(ax, ay, (int)point);
    AI::Waypoint* wp = AI::GetWaypoint(ax, ay, false);
    if (!wp && !(wp = AI::GetWaypointNearDecoration(d, false))) return;
    int dir = 0;
    if (AI::Waypoint* near = AI::GetNearestWP(wp->x, wp->y, entity->worldX, entity->worldY, dir)) wp = near;
    actionWP = wp;
    ChangeState(0x20);
    actionArg = (int)point;
    actionTarget = d;
    action = 5;
    // A decoration that will do something gets the "goto" marker: an unlocked usable one, or a
    // used-up one with a portal.
    bool useful = d->f4c != 0 ? !d->IsLocked() : d->meta && d->meta->FindChildData(kExpPortal);
    if (useful) GotoMarker();
    else BlockedMarker();
    // UNVERIFIED (milestone 6): "interact_(fe)male_player_move(_run)".
    entity->GetSquad()->MoveTo(wp);
    // UNVERIFIED (milestone 4h): BuildingHovers::HideWorldDialog(nullptr).
}

// @0xef3ec AIPlayer::AssignToJob
bool AIPlayer::AssignToJob(Map::Decor* d) {
    if (entity->IsDead() && GameState::GetCurrentMapID() != 0) {
        HealSquadWindow::Show();
        return false;
    }
    // UNVERIFIED (milestone 4h): tutorial step 0x14 hides the world dialog (HideWorldDialog).
    int ax = 0, ay = 0;
    d->GetActionPoint(ax, ay, (int)GetBestDecorationActionPoint(d));
    AI::Waypoint* wp = AI::GetWaypoint(ax, ay, false);
    if (!wp && !(wp = AI::GetWaypointNearDecoration(d, false))) return false;
    actionWP = wp;
    ChangeState(0x20);
    action = 3;
    GotoMarkerAt(d->x, d->y);
    // UNVERIFIED (milestone 6): "interact_(fe)male_player_move(_run)".
    entity->GetSquad()->MoveTo(wp);
    return d->f50 != 0;
}

// @0xef654 AIPlayer::RemoveFromJob
void AIPlayer::RemoveFromJob() {
    if (!entity->GetWorkplaceDecoration())
        std::fprintf(stderr, "ERROR: AIPlayer::RemoveFromJob() is called on a player with no job\n");
    else entity->GetWorkplaceDecoration()->WorkEnded();
    ChangeState(0);
    if (action != 3) entity->SetAnimationP("idle_1", true, false, false);
    autoInteraction = false;
    action = 0;
}

void AIPlayer::ProcessAction() {
    if (action == 0) return;
    switch (action) {
    case 1: {   // through the portal
        Portal* p = static_cast<Portal*>(actionTarget);
        if (Map::Decor* d = Map::GetPortalToMap((uint32_t)p->map)) {
            // UNVERIFIED (milestone 6): Decor::PlayTeleportSound.
            // The travel steps name the decoration ("<map><id>") or its tile; the city also counts
            // as map 0x2f.
            int map = (int)GameState::GetCurrentMapID();
            char key[32];
            std::snprintf(key, sizeof key, "%d%d", map, (int)d->id);
            Tasks::CompleteSubtask(Tasks::kInteractBuildingAny, (uint32_t)std::atoi(key), 1);
            if (map == 0) {
                std::snprintf(key, sizeof key, "%d%d", 0x2f, (int)d->id);
                Tasks::CompleteSubtask(Tasks::kInteractBuildingAny, (uint32_t)std::atoi(key), 1);
            }
            Tasks::CompleteSubtask(Tasks::kInteractBuilding, (uint32_t)(d->y + d->x * 0x100 + map * 0x10000), 1);
            if (map == 0) Tasks::CompleteSubtask(Tasks::kInteractBuilding, (uint32_t)(d->y + 0x2f0000 + d->x * 0x100), 1);
        }
        if (GameState::secondTutorial == 0x20d) {
            GameState::secondTutorial = 0x20e;
            BuildingHovers::HideArrow();
        }
        if (p->map == 0xe && p->x == 10 && p->y == 0x12) {
            // The way out of the tutorial's last map leads to the world map.
            entity->SetHP(entity->GetHpMax());
            GameState::UpdatePlayerRegenerationState();
            // UNVERIFIED (milestone 5): GlobalMapWindow::Show, then its tutorial animation (until
            // task 0x773 is done) or SwitchMapAnimated(0); the player gets item 0x5b, bound to
            // slot 8, unless it has one or better (SetInfoWindow::GetPlayerItemOrBetter).
        } else {
            Map::EnqueueLoadMap(p->map, false, true, 0.f);
        }
        break;
    }
    case 2: {   // talking: a task that takes items asks first
        Entity* e = static_cast<Entity*>(actionTarget);
        Tasks::TaskInfo* t = Tasks::GetTask(e->GetTalkTask());
        if (!t || t->giveItemCount == 0) {
            e->OnTalkTo();
        } else {
            NeedItemWindow::SetItemsToBeRemoved(t->id, new std::function<void()>([e] { e->OnTalkTo(); }));
            NeedItemWindow::Show();
        }
        break;
    }
    case 3: {   // the job on the hero's decoration: its price or item is confirmed first
        Map::Decor* d = entity->GetWorkplaceDecoration();
        auto ask = [this, d](uint32_t item, int resource, unsigned amount) {
            NeedItemWindow::SetSearchParameters(item, resource, amount,
                                                new std::function<void()>([this, d] { SearchDecoration(d); }),
                                                new std::function<void()>([d] { d->RemoveWorker(); }));
            NeedItemWindow::Show();
        };
        bool asked = false;
        for (int i = 0; i < 11; ++i) {
            if (d->cost[i] == 0) continue;
            if (d->GetRequiredItem() != 0) {
                std::fprintf(stderr, "ERROR: AIPlayer::ProcessAction() Decoration requires an item and a resource\n");
            } else if (asked) {
                std::fprintf(stderr, "ERROR: AIPlayer::ProcessAction() Decoration multiple resources\n");
            } else {
                ask(0, i, (unsigned)d->cost[i]);
                asked = true;
            }
        }
        if (asked) break;
        if (uint32_t item = d->GetRequiredItem()) {
            if (!autoInteraction) {
                ask(item, 0xb, 1);
                break;
            }
            autoInteraction = false;
        }
        SearchDecoration(d);
        break;
    }
    case 4: {   // a blocked way: the blocking decoration's message over the hero
        const char32_t* text = static_cast<Map::Decor*>(actionTarget)->GetBlockMessage();
        if (text)
            BuildingHovers::ShowTextHover(entity->worldX, entity->worldY, text, 1.f, 1.f, 1.f, 0.f, 0.f, 0.f, 0x19,
                                          5, 5.f, true, 2.f, 50.f);
        Render::CenterOn(entity->worldX, entity->worldY - entity->GetIdleHeight());   // (over 0.4 s on the original)
        break;
    }
    case 5: {   // a tapped decoration
        Map::Decor* d = static_cast<Map::Decor*>(actionTarget);
        if (!d) break;
        if (d->f4c == 0) {   // used up: only its portal is left
            action = 0;
            d->TeleportAction((unsigned)actionArg);
            break;
        }
        if (!d->IsLocked() && d->f50 != 0) {   // a timed job is the goblin's
            SendGoblinToWork(d);
            break;
        }
        if (Tasks::TaskInfo* t = Tasks::GetTask(d->GetStartQuestID())) {
            GameState::StartTask(t->id);
            Tasks::Update(true);
            BuildingHovers::Update(0.f, true);
            HUDWindow::UpdateTasks();
        }
        Tasks::TaskInfo* end = Tasks::GetTask(d->GetEndQuestID());
        if (end && !GameState::TaskCompleted(end->id)) {
            for (const Tasks::TaskSubtask& st : end->subtasks)
                if (st.id != 0) Tasks::CompleteSubtaskDirect(st.id, 0x400);
            Tasks::CompleteTask(end->id, true, true, false);
            Tasks::Update(true);
            BuildingHovers::Update(0.f, true);
            HUDWindow::UpdateTasks();
        }
        if (uint32_t item = d->GetRequiredItem()) {
            const Items::ItemInfo* info = Items::GetItemInfo(item);
            if (info && (info->type == 9 || info->type == 6)) {   // made in a building
                NeedItemWindow::SetParameters(item, d);
                NeedItemWindow::Show();
                break;
            }
            // UNVERIFIED (milestone 5): an item that a collection gives (Collections::
            // GetCollectionWithResult) opens CraftingWindow on it (type 8) or, once
            // "craft_unlock_task" is done, CraftItemWindow.
        }
        if (d->sprite) {
            std::u32string text = d->GetMessage(0x80);
            BuildingHovers::ShowTextHoverWithStyle(d->sprite->x + d->sprite->w * 0.5f, d->sprite->y - d->sprite->h,
                                                   text.c_str(), 0xe, 2.f, 50.f, false, false);
            // The Smithy's anvil on map 0x5a (tile 0x10, 0x1c): its first open quest is shown.
            if (d->x == 0x10 && d->y == 0x1c && d->id == 0x55e && GameState::GetCurrentMapID() == 0x5a) {
                for (unsigned task : {0x2fbu, 0x3fau, 0x3feu})
                    if (!GameState::TaskCompleted(task)) {
                        TaskInfoWindow::SetTask(task, false);
                        TaskInfoWindow::Show();
                        break;
                    }
            }
        }
        break;
    }
    case 6:
        entity->SetDirection(5);
        entity->SetAnimationMult("search", 2, false, false, true);
        action = 7;
        return;
    case 7:
        // The search is over: the corpse's item drops (corpse_drop's second number is the chance
        // in percent), the spawn point remembers it as taken and the corpse goes.
        if (Entity* corpse = (Entity*)actionTarget) {
            const EntityData* data = corpse->GetEntityData();
            SpawnPoint* sp = corpse->GetSpawnPoint();
            if (sp && data) {
                if (Rand48::lrand48() % 100 < data->corpseDropCount)
                    BuildingHovers::DropItem(corpse->worldX, corpse->worldY, Items::GetItemInfo((uint32_t)data->corpseDropId),
                                             0, 1);
                sp->corpseTaken = data->corpseDropId;
                corpse->SetActive(false, true);
            }
        }
        break;
    case 8:
        // PORT: a PvP shadow (PlayerCompetitionWindow, online); milestone 5 decides its offline form.
        break;
    }
    action = 0;
}
