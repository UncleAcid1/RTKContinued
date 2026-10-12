#include "game/AIState.h"

#include "engine/Render.h"
#include "engine/Resources.h"
#include "game/AI.h"
#include "game/Entity.h"
#include "game/EntityManager.h"
#include "game/SpawnPoint.h"

namespace {

Render::Texture* g_zoneImage = nullptr;   // 0x610b90 images/red_mid_02

void RemoveDebugSprites(Render::Sprite*& s) {
    while (s) s = Render::RemoveSprite(s);
}

}  // namespace

// @0xea714
AIEnemy::~AIEnemy() { RemoveDebugSprites(debugSprites); }

// @0xea198: a waypoint that goes also leaves the zone (the last one takes its place).
void AIEnemy::OnWaypointRemove(AI::Waypoint* wp) {
    TargetedAI::OnWaypointRemove(wp);
    for (size_t i = 0; i < zone.size(); ++i) {
        if (zone[i] != wp) continue;
        zone[i] = zone.back();
        zone.pop_back();
        return;
    }
}

// @0xea204
AI::Waypoint* AIEnemy::IsPlayerInAggroRange() {
    if (!entity->GetSpawnPoint() || entity->GetSpawnPoint()->aggroRange <= 0) return nullptr;
    Entity* player = EntityManager::GetPlayer();
    for (AI::Waypoint* wp : zone)
        if (player->GetAI()->arrived && wp->x == player->tileX && wp->y == player->tileY) return wp;
    return nullptr;
}

// @0xea2ac
void AIEnemy::DebugZone(AI::Waypoint* wp) {
    if (!g_zoneImage) g_zoneImage = Resources::GetImage("images/red_mid_02");
    Render::Sprite* s = Render::CreateSprite(g_zoneImage, 9, false, false);
    s->h = 24.f;
    s->w = 48.f;
    Render::SetPosition(s, wp->wx - 24.f, wp->wy + 12.f, 0.2f);
    s->next = debugSprites;
    debugSprites = s;
}

// @0xea370: the waypoints within the spawn point's range of where the enemy stands.
void AIEnemy::UpdateAggroZone() {
    SpawnPoint* sp = entity->GetSpawnPoint();
    if (!sp || sp->aggroRange <= 0) return;
    zone.clear();
    AI::GetWaypointsInRange(zone, (unsigned)entity->tileX, (unsigned)entity->tileY, (unsigned)sp->aggroRange, true);
    RemoveDebugSprites(debugSprites);
    if (AI::showDebug)
        for (AI::Waypoint* wp : zone) DebugZone(wp);
}

// @0xea694
void AIEnemy::Update(float dt) {
    if (entity->IsDead()) {
        RemoveDebugSprites(debugSprites);
        return;
    }
    if (!AI::showDebug) RemoveDebugSprites(debugSprites);
    AIWarrior::Update(dt);
}
