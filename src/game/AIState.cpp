#include "game/AIState.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "engine/Render.h"
#include "game/Animation.h"
#include "game/Building.h"
#include "game/BuildingHovers.h"
#include "game/Entity.h"
#include "game/EntityData.h"
#include "game/EntityManager.h"
#include "game/GameData.h"
#include "game/GameState.h"
#include "game/Map.h"
#include "game/Rand48.h"
#include "game/Setting.h"

void TurnAIToward(Entity* e, const AI::Waypoint* wp);   // game/Entity.cpp

namespace {
std::vector<BaseAI*> g_ais;   // 0x610b28 (count 0x610b34)
constexpr float kUnvisited = 1048576.f;
}  // namespace

// ------------------------------------------------------------------------------------- BaseAI

const std::vector<BaseAI*>& BaseAI::All() { return g_ais; }

BaseAI::BaseAI() {
    regIndex = (unsigned)g_ais.size();
    g_ais.push_back(this);
}

BaseAI::~BaseAI() {
    BaseAI* last = g_ais.back();
    last->regIndex = regIndex;
    g_ais[regIndex] = last;
    g_ais.pop_back();
}

void BaseAI::OnWaypointRemove(AI::Waypoint* wp) { entity->OnWaypointRemove(wp); }

bool BaseAI::HasEntity(Entity* e) { return entity == e; }

void BaseAI::SetSpeed(float s) {
    speed = s;
    if (entity->data->clas != 0x10) return;
    if (GameState::TutorialStep() == 0x30) return;
    entity->anim->mult = speed / baseSpeed;
}

// --------------------------------------------------------------------------------- TargetedAI

TargetedAI::TargetedAI(Entity* e) {
    entity = e;
    AI::Waypoint* wp = AI::GetWaypoint(e->tileX, e->tileY, farm);
    x = e->worldX;
    y = e->worldY;
    speed = baseSpeed = 12.f;
    current = next = wp;
}

void TargetedAI::OnMapRemove() {
    next = nullptr;
    pathCopy.clear();
    path.clear();
    current = nullptr;
}

void TargetedAI::OnWaypointRemove(AI::Waypoint* wp) {
    entity->OnWaypointRemove(wp);
    if (current == wp) {
        current = next = nullptr;
        if (path.empty()) return;
        path.pop_back();
        current = next = path.empty() ? nullptr : path.back();
        // (the original reads the popped slot here; path.back() before the pop is the same entry)
    }
    if (path.empty()) return;
    // Keep only the steps before the removed waypoint (those nearer the target).
    size_t i = 0;
    while (i < path.size() && path[i] != wp) ++i;
    if (i == path.size()) return;
    std::vector<AI::Waypoint*> keep(path.begin() + (long)i + 1, path.end());
    path = keep;
    current = next = path.empty() ? nullptr : path.back();
}

// The Dijkstra search shared by SetTarget and GetClosestReachableWP: costs from `current` over the
// part it is in; the four straight steps (left/down/right/up) cost weightCost times the weight
// (50 times for class 9 and type 0x11). Stops when target is taken off the open list.
static void SearchFrom(TargetedAI* ai, AI::Waypoint* target, bool reachMode) {
    for (AI::Waypoint* wp : ai->farm ? AI::GetFarmWaypoints() : AI::GetWaypoints()) {
        wp->cost = kUnvisited;
        wp->parent = nullptr;
    }
    ai->current->cost = 0.f;
    std::vector<AI::Waypoint*>& open = AI::SearchList();
    open.clear();
    open.push_back(ai->current);
    float straight = (ai->entity->data->clas != 9 && ai->entity->data->type != 0x11) ? AI::weightCost : 50.f;
    while (!open.empty()) {
        int i = AI::GetNearest(open);
        if (i == -1) break;
        AI::Waypoint* wp = open[(size_t)i];
        if (wp->cost == kUnvisited) break;
        open[(size_t)i] = open.back();
        open.pop_back();
        if (wp == target) break;
        for (int k = 0; k < 8; ++k) {
            AI::Waypoint* n = wp->n[k];
            if (!n) continue;
            if (n->cost == kUnvisited) open.push_back(n);
            float w = n->weight;
            if (k >= 4) w *= straight;
            if (n->cost > w + wp->cost) {
                float c = w + wp->cost;
                if (reachMode && n == target) c = 0.f;
                n->cost = c;
                n->parent = wp;
            }
        }
    }
}

bool TargetedAI::SetTarget(AI::Waypoint* wp, bool noCopy) {
    if (!path.empty()) {
        repathing = true;
        current = next = path.back();
    }
    if (!wp || current == wp) return false;
    if (!current) {
        current = next = wp;
        return false;
    }
    if (current->part != wp->part) return false;
    SearchFrom(this, wp, false);
    // (the debug path overlay loads images/influence_mid here)
    if (!wp->parent) {
        path.clear();
        return false;
    }
    if (!noCopy) {
        path.clear();
        for (AI::Waypoint* p = wp; p; p = p->parent) path.push_back(p);
        pathCopy = path;
    }
    return true;
}

AI::Waypoint* TargetedAI::SetTarget(Map::Building* b) {
    AI::Waypoint* wp = AI::GetWaypointNearBuilding(b, false);
    if (!wp) return nullptr;
    SetTarget(wp, false);
    return path.empty() ? nullptr : wp;
}

AI::Waypoint* TargetedAI::SetTarget(Map::Decor* d) {
    AI::Waypoint* wp = AI::GetWaypointNearDecoration(d, false);
    if (!wp) return nullptr;
    SetTarget(wp, false);
    return path.empty() ? nullptr : wp;
}

AI::Waypoint* TargetedAI::GetClosestReachableWP(AI::Waypoint* wp, int steps) {
    if (!wp || current == wp) return nullptr;
    if (current->part != wp->part) return nullptr;
    reach.clear();
    SearchFrom(this, wp, true);
    AI::Waypoint* p = wp->parent;
    if (!p) return nullptr;
    for (;;) {
        if (--steps < 1) {
            if (!p) break;
            reach.push_back(p);
        }
        if (!p) break;
        p = p->parent;
    }
    return reach.empty() ? wp : reach.front();
}

// -------------------------------------------------------------------------------- AIBaseState

AIBaseState::AIBaseState(Entity* e) : TargetedAI(e) {
    speed = 80.f;
    ChangeState(0);
}

bool AIBaseState::HasEntity(Entity* e) { return BaseAI::HasEntity(e) || e == linked; }

void AIBaseState::Update(float dt) {
    AnimationController* ac = entity->GetAnimController();
    if (ac && ac->anim && std::strstr(ac->anim->name.c_str(), "run")) {
        bool rest = 3.f <= dt + runTime;
        runTime += dt;
        if (rest) entity->SetAnimationP("idle_1", true, false, false);
    } else {
        runTime = 0.f;
    }
    if (!path.empty()) UpdateWalking(dt, 4);
}

void AIBaseState::UpdateLastTarget() {
    AI::Waypoint* wp = AI::GetWaypoint(entity->tileX, entity->tileY, false);
    if (!wp) return;
    next = wp;
    x = entity->worldX;
    current = wp;
    y = entity->worldY;
}

static void StartWalkAnimation(AIBaseState* ai, bool randomStart) {
    if (ai->walkAnim.empty()) {
        ai->entity->SetAnimationP("walk", randomStart, false, false);
    } else if (!ai->entity->SetAnimationP(ai->walkAnim.c_str(), false, false, false)) {
        ai->entity->SetAnimationP("walk", false, false, false);
    }
}

void AIBaseState::WalkTo(int tx, int ty) {
    if (!current) UpdateLastTarget();
    AI::Waypoint* wp = AI::GetWaypoint(tx, ty, farm);
    if (entity->tileX == tx && entity->tileY == ty) {
        path.clear();
        entity->OnWalkComplete();
        WalkCompleted();
        return;
    }
    if (SetTarget(wp, false)) {
        if (!repathing) StartWalkAnimation(this, false);
        return;
    }
    if (!current || wp != current) {
        std::vector<AI::Waypoint*> saved = path;
        path.clear();
        entity->OnUnableToWalk(tx, ty);
        UnableToWalk(tx, ty);
        if (path.empty()) {
            path = saved;
        } else if (!repathing) {
            StartWalkAnimation(this, false);
        }
    } else if (path.size() > 1) {
        path.assign(1, wp);
    }
}

void AIBaseState::WalkTo(AI::Waypoint* wp) {
    if (wp) {
        WalkTo(wp->x, wp->y);
        return;
    }
    std::fprintf(stderr, "ERROR: AIBaseState::WalkTo() Waypoint is invalid\n");
}

AI::Waypoint* AIBaseState::WalkToBuilding(Map::Building* b) {
    AI::Waypoint* wp = SetTarget(b);
    if (wp) StartWalkAnimation(this, true);
    return wp;
}

AI::Waypoint* AIBaseState::WalkToDecoration(Map::Decor* d) {
    AI::Waypoint* wp = SetTarget(d);
    if (wp) StartWalkAnimation(this, true);
    return wp;
}

void AIBaseState::TurnTo(AI::Waypoint* wp) { TurnAIToward(entity, wp); }

// @0xe83d0: face the neighbouring tile (tx, ty) diagonally.
void AIBaseState::TurnTo(int fx, int fy, int tx, int ty) {
    int left, right;
    if ((fy & 1) == 0) {
        right = fx;
        left = fx - 1;
    } else {
        right = fx + 1;
        left = fx;
    }
    bool isLeft = left == tx, up = ty == fy - 1, down = fy + 1 == ty;
    if (up && isLeft) entity->SetDirection(5);
    if (down && right == tx) entity->SetDirection(1);
    if (right == tx && up) {
        entity->SetDirection(3);
        if (!down || !isLeft) return;
    } else if (!down || !isLeft) {
        return;
    }
    entity->SetDirection(7);
}

// @0xe97f8: face the first tile of a w x h zone (corner bx, by) that touches (fx, fy) diagonally.
void AIBaseState::TurnTo(int fx, int fy, int bx, int by, int w, int h) {
    int left = (fy & 1) == 0 ? fx - 1 : fx;
    int right = (fy & 1) == 0 ? fx : fx + 1;
    int up = fy - 1, down = fy + 1;
    if (h <= 0) return;
    std::vector<std::pair<int, int>> tiles;
    int rowX = bx;
    for (int ry = by; ry != by - h;) {
        if (w > 0) {
            int cx = rowX;
            for (int cy = ry; cy != ry - w;) {
                tiles.emplace_back(cx, cy);
                bool odd = (cy & 1) != 0;
                --cy;
                if (odd) ++cx;
            }
        }
        bool odd = (ry & 1) != 0;
        --ry;
        if (!odd) --rowX;
    }
    for (auto& t : tiles) {
        if (left == t.first && t.second == up) { entity->SetDirection(5); break; }
        if (right == t.first && down == t.second) { entity->SetDirection(1); break; }
        if (right == t.first && up == t.second) { entity->SetDirection(3); break; }
        if (left == t.first && t.second == down) { entity->SetDirection(7); break; }
    }
}

void AIBaseState::Reset(bool idle) {
    path.clear();
    linked = nullptr;
    ChangeState(0);
    f98 = f84 = f80 = f88 = 0.f;
    f9c = 1;
    if (idle) entity->SetAnimationP("idle_1", true, false, false);
    entity->SetPos(entity->tileX, entity->tileY);
    UpdateLastTarget();
}

int AIBaseState::GetFutureDirection() {
    if (path.size() < 2) return -1;
    AI::Waypoint* a = path[0];
    AI::Waypoint* b = path[1];
    int dir = (a->n[1] == b || a->n[6] == b) ? 5 : -1;
    if (a->n[0] == b || a->n[7] == b || a->n[4] == b) dir = 1;
    if (a->n[2] != b && a->n[5] != b) {
        if (a->n[3] == b) dir = 7;
        return dir;
    }
    dir = 3;
    if (a->n[3] == b) dir = 7;
    return dir;
}

void AIBaseState::SetCustomIdleAnimation(const char* name, int first, int last) {
    idleAnim = name;
    if (entity->GetAnimController()) entity->GetAnimController()->SetAnimFrameRange(first, last);
}

void AIBaseState::UpdateWalking(float dt, int depth) {
    AI::Waypoint* cur = current;
    AI::Waypoint* nxt = path.back();
    arrived = false;
    next = nxt;
    // The step direction: which neighbour of the current waypoint is next.
    static const int kCodeForNeighbour[8] = {3, 4, 1, 2, 5, 6, 7, 8};
    static const int kDirForCode[9] = {-1, 7, 3, 5, 1, 6, 0, 2, 4};
    int k = 0;
    while (k < 8 && cur->n[k] != nxt) ++k;
    if (k < 8) stepDir = kCodeForNeighbour[k];
    if (stepDir >= 1 && stepDir <= 8) entity->SetDirection(kDirForCode[stepDir]);

    float tx, ty;
    if (!farm) {
        tx = ((nxt->y & 1) ? 42.f : 0.f) + (float)nxt->x * 84.f + 42.f;
        ty = (float)nxt->y * 42.f * 0.5f;
    } else {
        tx = (float)nxt->x * 196.f + ((nxt->y & 1) ? 98.f : 0.f) + 98.f + Map::GetFarmWorldX();
        ty = (float)nxt->y * 98.f * 0.5f + Map::GetFarmWorldY();
    }
    float dy = ty - y, dx = tx - x;
    float sp = speed;
    uint32_t mapId = GameState::GetCurrentMapID();
    float dist = std::sqrt(dy * dy + dx * dx);
    if ((entity->player || entity->data->clas == 10) && GameState::GetLevel() > 4 &&
        (mapId == 0 || (mapId == 0xb && !entity->IsSpeedRunning()))) {
        float b = baseSpeed * entity->GetBaseSpeedMultiplier();
        sp = b + b;
    }
    float step = dt * sp;
    if (step <= dist) {
        y = y + (dy / dist) * step;
        x = x + (dx / dist) * step;
        entity->SetWorldPos(x, y);
        return;
    }
    arrived = true;
    x = tx;
    y = ty;
    runTime = 0.f;
    // UNVERIFIED (milestone 4): the player's sprint costs AP off the city and map 0xb.
    current = nxt;
    if (repathing) repathing = false;
    entity->SetPos(nxt->x, nxt->y);
    PositionChanged();
    path.pop_back();
    if (!path.empty()) {
        if (depth > 0 && dist > 0.f) {
            UpdateWalking((step - dist) / speed, depth - 1);
            arrived = true;
            return;
        }
        entity->SetWorldPos(x, y);
        return;
    }
    ChangeState(0);
    if (idleAnim.empty()) {
        entity->SetAnimationP("idle_1", false, false, false);
    } else if (idleAnim == "death") {
        entity->SetAnimationOnce("death", false, false, true, true);
        if (entity->GetSprite()) Render::ChangeLayer(entity->GetSprite(), 6);
    } else {
        entity->SetAnimationP(idleAnim.c_str(), false, false, false);
    }
    entity->SetPos(current->x, current->y);
    entity->OnWalkComplete();
    WalkCompleted();
}

// ----------------------------------------------------------------------------------- AIWorker

AIWorker::AIWorker(Entity* e) : AIBaseState(e) {
    path.clear();
    job = 0;
    float s = GameState::GetCurrentMapID() != 0xb ? 140.f : 70.f;
    baseSpeed = s;
    SetSpeed(s);
    wanderTime = (float)(Rand48::lrand48() % 0x1e);
    strollTarget = nullptr;
    jobTime = (float)(Rand48::lrand48() % 6 + 1);
}

void AIWorker::StartWorking() {
    ChangeState(6);
    entity->OnStartedToWork();
    if (Map::Building* b = entity->GetWorkplace()) {
        b->WorkStarted();
    } else if (entity->GetWorkplaceDecoration()) {
        // UNVERIFIED (milestone 3, decorations): Decor::WorkStarted unless offline.
    } else {
        std::fprintf(stderr, "ERROR: AIWorker::StartWorking() Cannot start work, no target specified\n");
    }
    BuildingHovers::Update(0.0, true);
    float px = 0.f, py = 0.f;
    if (Map::Building* b = entity->GetWorkplace()) {
        if (job == 2 || job == 4) {
            b->GetParkingSpot(1, px, py);
            entity->SetWorldPos(px, py);
            entity->SetDirection(5);
            if (b->GetGatheredResCount() < (int)GameState::GetSetting("stack_size"))
                entity->SetAnimationP(job == 2 ? "cut" : "miner", true, false, false);
            // UNVERIFIED (milestone 5): "work_lumber_started" / "work_stone_started" unless offline.
        } else if (job == 1) {
            b->GetBuildingSpot(px, py);
            entity->SetWorldPos(px, py);
            entity->SetDirection(3);
            entity->SetAnimationP("build", true, false, false);
        }
    } else if (entity->GetWorkplaceDecoration()) {
        entity->SetDirection(3);
        entity->SetAnimationP("build", true, false, false);
    }
    // (tutorial step 0x5d centres the camera on (2150, 258) and moves to 0x5e)
}

void AIWorker::WalkCompleted() {
    // (tutorial steps 0x1c, 0x23, 0x26 and 0x2b advance here)
    if (entity) {
        if (entity->GetWorkplace() || entity->GetWorkplaceDecoration()) StartWorking();
        if (entity && entity->disappearOnArrival) {
            entity->Disappear();
            entity->disappearOnArrival = false;
        }
    }
    if (strollTarget) strollTarget = nullptr;
}

void AIWorker::RemoveFromJob() {
    if (Map::Building* b = entity->GetWorkplace()) {
        b->WorkEnded();
    } else if (entity->GetWorkplaceDecoration()) {
        // UNVERIFIED (milestone 3, decorations): Decor::WorkEnded.
    } else {
        std::fprintf(stderr, "ERROR: AIWorker::RemoveFromJob() Cannot start work, no target specified\n");
    }
    if (GetState() == 6) entity->SetAnimationP("idle_1", true, false, false);
    ChangeState(0);
    job = 0;
    if (GameState::TutorialStep() != 0x80 || !entity->GetHome()) return;
    // (tutorial step 0x80: the worker walks back home)
    Map::Building* home = entity->GetHome();
    if (!home->liverAway) home->liverAway = true;
    else entity->disappearOnArrival = true;
    int sx = 0, sy = 0;
    home->GetSpawnTile(sx, sy);
    if (AI::GetWaypoint(sx, sy, false) && !EntityManager::GetEntityAtXY(sx, sy) && !Map::GetBuilding(sx, sy) &&
        !Map::GetDecoration(sx, sy)) {
        WalkTo(sx, sy);
        return;
    }
    // A free waypoint around home; the last try does not count.
    int tries = home->data->w * home->data->h, left = 0;
    AI::Waypoint* wp = nullptr;
    while (tries != 0) {
        wp = AI::GetWaypointNearBuilding(home, false);
        if (!wp) {
            if (--tries == 0) break;
            continue;
        }
        if (!EntityManager::GetEntityAtXY(wp->x, wp->y) && !Map::GetBuilding(wp->x, wp->y) &&
            !Map::GetDecoration(wp->x, wp->y)) {
            left = --tries;
            break;
        }
        --tries;
        wp = nullptr;
    }
    if (left == 0) {
        home->GetSpawnTile(sx, sy);
        WalkTo(sx, sy);
    } else {
        WalkTo(wp->x, wp->y);
    }
}

bool AIWorker::AssignToJob(Map::Decor* d) {
    speed = 140.f;
    job = 1;
    // UNVERIFIED (milestone 3, decorations): the action point is Decor::GetActionPoint(x, y, 0).
    AI::Waypoint* wp = AI::GetWaypoint(d->x, d->y, false);
    Reset(true);
    repathing = false;
    entity->disappearOnArrival = false;
    if (wp && entity->tileX == wp->x && entity->tileY == wp->y) {
        StartWorking();
    } else if (wp) {
        WalkTo(wp->x, wp->y);
        ChangeState(5);
    }
    // (tutorial step 0x1e locks the GUI and hides the arrow and dialog)
    return true;
}

void AIWorker::AssignToJob(Map::Building* b) {
    const GameData::BuildingData* d = b->data;
    int tx = 0, ty = 0;
    if (d->buildingClass == 4) {
        if (d->produceAmount != 0) {
            if (d->produceResource == 0) job = 2;
            else if (d->produceResource == 1) job = 4;
        }
        speed = 140.f;
        b->GetWorkTile(tx, ty);
    } else {
        if (b->IsOpened()) return;
        job = 1;
        speed = 140.f;
        b->GetBuildTile(tx, ty);
    }
    AI::Waypoint* wp = AI::GetWaypoint(tx, ty, false);
    Reset(true);
    repathing = false;
    entity->disappearOnArrival = false;
    if (entity->tileX == wp->x && entity->tileY == wp->y) {
        StartWorking();
    } else {
        // UNVERIFIED (milestone 5): "worker_assigned" sound.
        WalkTo(wp->x, wp->y);
        ChangeState(5);
    }
}

void AIWorker::Update(float dt) {
    if (GameState::GetCurrentMapID() == 0) {
        if (job == 1) entity->SetCustomZ(Map::GetSpriteZ(entity->worldY + (float)entity->data->offY, 0.f, 0));
        else entity->SetCustomZ(0.f);
    }
    bool skipToBase = false;
    if (Map::Building* b = entity->GetWorkplace(); b && GetState() != 5) {
        if (b->GetGatheredResCount() < (int)GameState::GetSetting("stack_size")) {
            AnimationController* ac = entity->GetAnimController();
            if (ac->anim && std::strstr(ac->anim->name.c_str(), "idle_1")) {
                if (job == 2) entity->SetAnimationP("cut", true, false, false);
                else if (job == 4) entity->SetAnimationP("miner", true, false, false);
                if (Map::Building* w = entity->GetWorkplace()) w->WorkStarted();
            }
        } else {
            AnimationController* ac = entity->GetAnimController();
            if (ac->anim && !std::strstr(ac->anim->name.c_str(), "idle_1"))
                entity->SetAnimationP("idle_1", true, false, false);
        }
    }
    if (GetState() == 0x24) {
        if (!strollTarget) {
            if (!GameState::IsCityTutorial()) {
                jobTime -= dt;
                if (jobTime < 0.f) {
                    if (Map::Building* idle = Map::GetIdleWorkplace()) idle->AssignWorker(entity, 0);
                    jobTime = (float)(Rand48::lrand48() % 6 + 1);
                }
            }
            if (GetState() != 0x24) skipToBase = true;
        }
        if (!skipToBase && !strollTarget && !GameState::IsCityTutorial()) {
            wanderTime -= dt;
            if (wanderTime < 0.f) {
                wanderTime = (float)(Rand48::lrand48() % 0x1e);
                int x0, y0, x1, y1;
                Map::GetOwnedAreaBorders(x0, y0, x1, y1);
                strollTarget = AI::GetRandomFreeWPRect(x1 - x0, y1 - y0, x0, y0);
                if (strollTarget) WalkTo(strollTarget);
            }
        }
    }
    // Workers (ids 0 and 0x30) on the city without a spawn point start strolling now and then.
    if (GameState::GetCurrentMapID() == 0 && (entity->data->id == 0 || entity->data->id == 0x30) && job == 0 &&
        GetState() != 0x24) {
        wanderTime -= dt;
        if (wanderTime < 0.f) {
            state = 0x24;
            wanderTime = (float)(Rand48::lrand48() % 0x1e);
        }
    }
    AIBaseState::Update(dt);
}

// ----------------------------------------------------------------------------------- AIGoblin

AIGoblin::AIGoblin(Entity* e) : AIBaseState(e) {
    orderTime = fa4;
    step = 0;
    baseSpeed = 90.f;
    order = nullptr;
    SetSpeed(90.f);
}

void AIGoblin::Update(float dt) {
    AIBaseState::Update(dt);
    if (step != 0 || GameState::GetCurrentLocation() != 0) return;
    orderTime -= dt;
    if (orderTime >= 0.f) return;
    GetOrder();
    orderTime = fa4;
}

void AIGoblin::Reset(bool) {
    AIBaseState::Reset(true);
    step = 0;
    SetCustomWalkAnimation("");
}

void AIGoblin::GetOrder() {
    if (GameState::TutorialStep() < 0x2a) return;
    order = GameState::GetTopOrder();
    if (!order) return;
    step = order->kind == 0 ? 4 : 3;
    order->goblin = entity;
    GotoTarget();
}

void AIGoblin::GotoTarget() {
    int tx = 0, ty = 0;
    order->from->GetDeliveryTile(tx, ty);
    AI::Waypoint* wp = AI::GetWaypoint(tx, ty, false);
    ChangeState(7);
    SetSpeed(GameState::TutorialStep() == 0x30 ? 200.f : 100.f);
    WalkTo(wp->x, wp->y);
}

void AIGoblin::GotoDestination() {
    int tx = 0, ty = 0;
    order->to->GetDeliveryTile(tx, ty);
    AI::Waypoint* wp = AI::GetWaypoint(tx, ty, false);
    ChangeState(7);
    WalkTo(wp->x, wp->y);
}

static void SetCarryAnimation(AIGoblin* ai, int type) {
    if (type == 0) ai->SetCustomWalkAnimation("walk_wood");
    else if ((unsigned)(type - 1) < 2) ai->SetCustomWalkAnimation("walk_sack");
}

void AIGoblin::WalkCompleted() {
    if (step == 4) {
        if ((int)GameState::GetResourceAmount(order->type) < GameState::resourceAmountMax) {
            GameState::Order* o = order;
            if (o->from->resources[o->type] < o->amount) {
                CancelWork();
                o->taken = true;
                return;
            }
            SetSpeed(GameState::TutorialStep() == 0x30 ? 200.f : 60.f);
            SetCarryAnimation(this, order->type);
            Map::Building* from = order->from;
            from->resources[order->type] -= order->amount;
            from->UpdateResources();
            if (order->to) {
                GotoDestination();
                step = 1;
                return;
            }
        }
    } else if (step == 5) {
        // UNVERIFIED (milestone 3, decorations): the goblin's decoration job (search/build
        // animation, Decor::WorkStarted, facing the decoration, state 8).
        return;
    } else if (step == 3) {
        SetCarryAnimation(this, order->type);
        SetSpeed(GameState::TutorialStep() == 0x30 ? 200.f : 60.f);
        order->from->UpdateResources();
        if (order->to) {
            GotoDestination();
            step = 2;
            return;
        }
    } else {
        // (step 1, a pile delivered: tutorial step 0x30 moves to 0x33)
        {
            std::string n = std::to_string(order->amount);
            std::u32string text = U"+" + std::u32string(n.begin(), n.end()) + U" ";   // "+%d %s"
            if (const char32_t* name = GameState::GetResourceGameName(order->type)) text += name;
            if (Map::Building* to = order->to)
                BuildingHovers::ShowTextHover(to->baseX, to->minY, text.c_str(), 1.f, 1.f, 1.f, 0.f, 0.f, 0.f,
                                              0x19, 5, 5.f, true, 2.f, 50.f);
        }
        if (step != 0) {
            GameState::ChangeResourceAmount(order->type, order->amount);
            if (order->to) order->to->UpdateResources();
        }
        SetSpeed(100.f);
        SetCustomWalkAnimation("");
        entity->SetDirection(0);
        step = 0;
        ChangeState(0);
        return;
    }
    CancelWork();
}

bool AIGoblin::AssignToJob(Map::Decor* d) {
    // UNVERIFIED (milestone 3, decorations): the action point is Decor::GetActionPoint(x, y, 0).
    SetSpeed(100.f);
    entity->SetAlpha(0.f);
    entity->Appear(false, true);
    AI::Waypoint* wp = AI::GetWaypoint(d->x, d->y, false);
    if (!wp) wp = AI::GetWaypointNearDecoration(d, false);
    if (!wp) return false;
    path.clear();
    ChangeState(7);
    step = 5;
    WalkTo(wp->x, wp->y);
    return true;
}

void AIGoblin::RemoveFromJob() {
    // UNVERIFIED (milestone 3, decorations): Decor::WorkEnded on the workplace decoration.
    ChangeState(0);
    entity->SetAnimationP("idle_1", true, false, false);
    entity->Disappear();
    entity->SetCurrentMap(0);
    step = 0;
    ChangeState(0);
}

void AIGoblin::CancelWork() {
    if (order && step == 4) {
        order->goblin = nullptr;
        GameState::Order* o = order;
        order = nullptr;
        step = 0;
        o->taken = false;
    } else if (step == 5) {
        step = 0;
    }
    SetSpeed(100.f);
}

void AIGoblin::ResetOrder(Map::Building* b) {
    if (order && order->to == b) order->to = nullptr;
}

// --------------------------------------------------------------------------------- the factory

AIBaseState* CreateAIState(int state, Entity* e) {
    switch (state) {
    case 0: return new AIBaseState(e);
    case 3:
    case 4: return new AIWorker(e);
    case 5: return new AIFarmerBig(e);
    case 6: return new AIPatch(e);
    case 7: return new AIGoblin(e);
    default:
        // UNVERIFIED (milestones 3-4): AIWarrior (1), AIPlayer (2), AIFarmerSmall (8, 3f),
        // AIEnemy (9), AISpell (10), AIPlayerBot (11) are not ported yet;
        // those entities get the base behaviour.
        return new AIBaseState(e);
    }
}
