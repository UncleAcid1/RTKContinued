// AIWarrior (soldiers, and the base of the hero's AI) and AIPlayer (the hero). See AIState.h.
#include <cstdio>
#include <cstdlib>
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
#include "game/Map.h"
#include "game/Rand48.h"
#include "game/Squad.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "engine/Timer.h"
#include "windows/Windows.h"

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

}  // namespace

// --------------------------------------------------------------------------------------- AIWarrior

AIWarrior::AIWarrior(Entity* e) : AIBaseState(e) {
    SetCustomWalkAnimation("run");
    f78 = true;
    f9c = 1;
    float s = entity->GetEntityData()->clas == 10 ? 239.4f : 180.f;
    f7c = 0;
    speed = s;
    baseSpeed = s;
    f79 = false;
    f98 = f84 = f80 = f88 = f90 = f8c = f94 = 0.f;
    f74 = 1.f;
}

void AIWarrior::Update(float dt) {
    // UNVERIFIED (milestone 4g): the combat timers +0x80..+0x98 (set by StartedAttack and the magic
    // attacks) count down at 1.8 per second and fire Hit/Death/Block, their magic forms or a
    // projectile when they pass 0; outside combat they stay 0.
    AIBaseState::Update(dt);
}

void AIWarrior::WalkTo(int x, int y) {
    // UNVERIFIED (milestone 4g): in an arena combat (types 5, 6) the waypoint under the entity gets
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
        // UNVERIFIED (milestone 4c): Tasks::CompleteSubtask(0x24, 0x8e, 1) and (0x30, 1, 1).
        GameState::tutorial = 0x65;
    }
    // UNVERIFIED (milestone 4g): in an arena combat (types 5, 6) the waypoint under it gets weight 1000.
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
    // UNVERIFIED (milestone 4f): an entity walking its patrol (Entity::IsWalkingOnPatrol) also gets
    // Entity::OnWalkComplete.
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
        // UNVERIFIED (milestone 4g): Entity::OnDeath(entity, the attacker +0x68).
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
        // UNVERIFIED (milestone 4f): Map::RemoveFog over the hero's sprite box.
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
    // UNVERIFIED (milestone 4g): while a CombatManager exists (GameState::g_Combat) it takes the tap.
    if (!GameState::IsPlayerCity()) return;
    if (entity->IsRetreating()) return;
    if (!GUI::CanInteractWith(e)) return;
    // (BottomScreenshotWindow, not ported, is never visible)
    std::puts("AIPlayer::ClickedEntity()");
    if (entity == e) return;
    if (!TryToInterruptJob()) return;
    if ((unsigned)(action - 6) < 2) return;
    if (entity->IsDead() && GameState::GetCurrentMapID() != 0 && GameState::GetCurrentMapID() != 0xb) {
        // UNVERIFIED (milestone 4g): HealSquadWindow::Show.
        return;
    }
    Entity::UpdateSafePos(entity);
    // (the tame tutorial, GameState::IsTameTutorial, is never on)
    if (GameState::secondTutorial == 0x91 || GameState::secondTutorial == 0x8e) {
        // UNVERIFIED (milestone 4h): BuildingHovers::HideWorldDialog(nullptr).
        BuildingHovers::HideArrow();
    }
    // UNVERIFIED (milestones 4f/4g): the PvP-tutorial steps on an enemy tap (secondTutorial 0x203 ->
    // 0x204, 0x206 -> 0x207, 0x20e -> 0x20f; 0x21a on a boss -> 0x9f) need Entity::IsEnemy/IsBoss;
    // (4c) an entity with a meta expression of type 0x98 (a PvP shadow) starts action 8; (4g) in
    // combat a monster tap selects the target; (4c/4f) a talk task or special talk walks to it for
    // action 2; (4b/4g) a corpse with an item is searched (action 6).
    AI::Waypoint* wp = AI::GetWaypoint(e->tileX, e->tileY, false);
    if (!e->IsDead() && !GameState::IsCombatActive() && wp && SetTarget(wp, true)) {
        e->OnClick();
        if (e->GetEntityData()->clas == 9) {
            // UNVERIFIED (milestone 4g): a monster (not an NPC spawn) is attacked: the attack marker,
            // a new BaseCombat with the two squads.
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
    // UNVERIFIED (milestone 4g): while a CombatManager exists it takes the tap (OnTileClicked).
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
        // UNVERIFIED (milestone 4g): HealSquadWindow::Show.
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
        // UNVERIFIED (milestone 4f): while an enemy still has the player in its aggro range
        // (EntityManager::IsPlayerInAggroRange) the squad keeps retreating away from it.
        entity->f38 = false;
        entity->SetRetreating(false);
    }
    // UNVERIFIED (milestone 4g): in an arena combat (types 5, 6) the waypoint under it gets weight 1000.
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
        // UNVERIFIED (milestone 4e): TargetedAI::SetBlockedPathTarget(GetWaypoint(x, y)) finds the
        // quest-blocked decoration next to the end of the path; then action 4 shows its message.
        (void)x, (void)y;
    }
    BlockedMarker();
}

bool AIPlayer::TryToInterruptJob() {
    Map::Decor* d = entity->GetWorkplaceDecoration();
    if (action == 3 && d && !GameState::IsTutorial()) {
        // UNVERIFIED (milestone 4e): Map::Decor::RemoveWorker(d).
    }
    if (d && IsWorking() && !GameState::IsTutorial()) {
        // UNVERIFIED (milestone 4e): a job without a required item is cancelled: Decor::ReturnCost,
        // Decor::RemoveWorker, BuildingHovers::Update(0, true).
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
        // UNVERIFIED (milestone 4e): outside the tutorial the decoration job's worker is removed
        // (Map::Decor::RemoveWorker).
        return;
    }
    if (IsWorking()) return;
    BaseSquad* sq = entity->GetSquad();
    for (unsigned i = 0; i < (unsigned)sq->GetSoldierCount(); ++i) sq->GetSoldier(i)->GetAI()->Reset(true);
}

void AIPlayer::OnMapUnload() { hidden->Clear(false); }

void AIPlayer::ProcessAction() {
    if (action == 0) return;
    switch (action) {
    case 1:
        // UNVERIFIED (milestone 4f): the portal: its teleport sound, the travel subtasks 0x22/0x23,
        // the global map from map 0xe's exit (10, 0x12), else Map::EnqueueLoadMap.
        break;
    case 2:
        // UNVERIFIED (milestones 4c/4f): talking: NeedItemWindow for a task wanting items, else
        // Entity::OnTalkTo.
        break;
    case 3:
        // UNVERIFIED (milestone 4e): a decoration job: NeedItemWindow for its cost or required item,
        // else SearchDecoration.
        break;
    case 4:
        // UNVERIFIED (milestone 4e): the blocking decoration's message over the hero, the view centred
        // on it.
        break;
    case 5:
        // UNVERIFIED (milestones 4c/4e): a decoration action: teleport, a goblin to work, its start and
        // end quests, the item it needs (NeedItemWindow, crafting), its message.
        break;
    case 6:
        entity->SetDirection(5);
        entity->SetAnimationMult("search", 2, false, false, true);
        action = 7;
        return;
    case 7:
        // UNVERIFIED (milestones 4b/4f): the corpse's item drops by chance (EntityData corpse_drop) and
        // the corpse goes.
        break;
    case 8:
        // PORT: a PvP shadow (PlayerCompetitionWindow, online); milestone 5 decides its offline form.
        break;
    }
    action = 0;
}
