#include "game/Entity.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "engine/Render.h"
#include "engine/Resources.h"
#include "game/AI.h"
#include "game/AIState.h"
#include "game/BuildingHovers.h"
#include "game/Animation.h"
#include "game/EntityData.h"
#include "game/EntityManager.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/MetaData.h"
#include "game/MetaExpression.h"
#include "game/Map.h"
#include "game/Rand48.h"
#include "game/Setting.h"
#include "game/Squad.h"

namespace {
uint32_t g_entityTime = 0;     // 0x6119b0 Entity::SetCurrentTime
bool g_hpKeepExcess = false;   // 0x6119b4 SetupPlayerHPOverlimit
bool g_hpConsume = false;      // 0x6119b5
bool g_removingPlayer = false; // 0x6119cc the player is being deleted (its squad goes with it)
bool g_squadUsesOrbs = false;  // 0x6119cd

const char* const kRingImages[8] = {   // 0x6012a4
    "", "images/Rings/under_ring_npc", "images/Rings/under_rings_boss", "images/Rings/under_rings_enemy",
    "images/Rings/under_rings_player", "images/Rings/under_rings_squad", "images/Rings/under_rings_enemy_idle",
    "images/Rings/under_rings_boss_idle"};
}  // namespace

void Entity::SetCurrentTime(uint32_t t) { g_entityTime = t; }

Entity::Entity(EntityData* d, int x, int y) : data(d) {
    hp = d->GetHpForLevel(GameState::GetLevel());
    hpRate = d->hpRate;
    ResetStats(true);
    int c = d->clas;
    if (c == 6 || c == 3 || c == 4 || c == 0xf || c == 7) onFarm = true;
    spawnX = tileX = linearX = x;
    spawnY = tileY = linearY = y;
    Map::TileCoordinatesToLinear(linearX, linearY);
    observers.clear();
    // UNVERIFIED: classes 9 and 10 get an EventManager<Entity> (+0x14); not ported.
    SetPos(x, y);
    CreateAnims();
    firstName = surname = U"";   // (+0x160/+0x164 point at an empty string)
    g_squadUsesOrbs = Setting("squad_uses_orbs").GetInt() == 1;
    CreateAI();
}

Entity::~Entity() {
    delete anim;
    anim = nullptr;
    sprite = Render::RemoveSprite(sprite);
    ring = Render::RemoveSprite(ring);
    delete ai;
    ai = nullptr;
    underlay = Render::RemoveSprite(underlay);
    while (debugText) debugText = Render::RemoveSprite(debugText);
    if (player) g_removingPlayer = true;
    if (squad) {
        delete squad;
        squad = nullptr;
        // (EntityManager::AssertNoReferencesToSquad is empty)
    }
    if (player) g_removingPlayer = false;
    // UNVERIFIED (milestone 4g): the projectile (+0x1ac) is deleted here.
    glow = Render::RemoveSprite(glow);
    if (data && ownsData) delete data;
    data = nullptr;
}

void Entity::SetPos(int x, int y) {
    prevX = tileX;
    tileX = x;
    prevY = tileY;
    tileY = y;
    if (!onFarm) {
        worldY = (float)y * 42.f * 0.5f;
        worldX = ((y % 2 == 1) ? 42.f : 0.f) + (float)x * 84.f + 42.f;
    } else {
        worldX = (float)x * 196.f + ((y % 2 == 1) ? 98.f : 0.f) + 98.f + Map::GetFarmWorldX();
        worldY = (float)tileY * 98.f * 0.5f + Map::GetFarmWorldY();
    }
    AI::Waypoint* old = waypoint;
    waypoint = AI::GetWaypoint(tileX, tileY, false);
    if (old == waypoint) return;
    linearX = tileX;
    linearY = tileY;
    Map::TileCoordinatesToLinear(linearX, linearY);
    OnMoved();
    if (old && EntityManager::GetEntityCountAtXY(old->x, old->y) == 0) old->weight = 1.f;
}

void Entity::SetDirection(int dir) {
    direction = dir;
    if (sprite) Render::SetFrameMirror(sprite, dir == 5 || dir == 7 || dir == 6);
}

// @0x155128 (and AIBaseState::TurnTo(Waypoint*) @0xe8498, the same steps): the angle from this
// entity to the point on screen picks one of the eight directions.
static void TurnToward(Entity* e, int fromX, int fromY, int toX, int toY) {
    Map::WorldCoordinatesToScreen(fromX, fromY);
    Map::WorldCoordinatesToScreen(toX, toY);
    double dx = (double)(fromX - toX), dy = (double)(fromY - toY);
    bool left = false;
    if (dx == 0.0) dx = 0.01;
    else left = !(dx > 0.0);
    float a = (float)(dy / dx);
    if (a < 0.f) a = -a;
    float deg = atanf(a) * 180.f / 3.14f;
    if (!left) {
        if (dy <= 0.0) deg = 360.f - deg;
    } else if (dy < 0.0) {
        deg += 180.f;
    } else {
        deg = 180.f - deg;
    }
    unsigned d = (unsigned)(int)deg;
    if (d - 5 < 0x50) e->SetDirection(5);
    if (d - 0x5f < 0x50) e->SetDirection(3);
    if (d - 0xb9 < 0x50) e->SetDirection(1);
    if (d - 0x113 < 0x50) e->SetDirection(7);
    else if (d < 5) e->SetDirection(6);
    if (d - 0x55 < 10) e->SetDirection(4);
    if (d - 0xaf < 10) e->SetDirection(2);
    if (d - 0x109 < 10) e->SetDirection(0);
    if (d - 0x163 < 5) e->SetDirection(6);
    else if (d == 0x168) e->SetDirection(7);
}

void Entity::TurnTo(int wx, int wy) { TurnToward(this, (int)worldX, (int)worldY, wx, wy); }

void Entity::TurnTo(const AI::Waypoint* wp) { TurnTo((int)wp->wx, (int)wp->wy); }

void TurnAIToward(Entity* e, const AI::Waypoint* wp) {   // used by AIBaseState::TurnTo(Waypoint*)
    TurnToward(e, (int)e->worldX, (int)e->worldY, (int)wp->wx, (int)wp->wy);
}

bool Entity::SetAnimation(const char* name, int playCount, bool randomStart, bool paused, bool holdLast,
                          bool notify, bool bounce) {
    if (data->anims.empty()) return true;
    Animation* a = data->GetAnimation(name);
    if (!a) return false;
    if (!a->loaded) a->Load();
    if (!sprite) {
        Render::Texture* t = a->GetTexByDir(direction);
        if (!t) {
            // Animation::GetAvailableDirTex @0xf9388 (returns nothing usable on the original)
            return false;
        }
        sprite = Render::CreateSprite(t, Render::kLayerObjects, false, false);
    }
    if (a->frameCount == 0) return false;
    int frame = 0;
    if (randomStart || std::strstr(a->name.c_str(), "idle")) frame = (int)(Rand48::lrand48() % a->frameCount);
    if (underlay) {
        // (the custom underlay's size follows its texture's frame width)
        underlay->w = (float)Render::GetFrameWidth(underlay->tex);
        underlay->h = (float)Render::GetFrameWidth(underlay->tex);
    }
    anim->SetAnim(a, frame, playCount, paused, holdLast, notify, bounce);
    return true;
}

bool Entity::SetAnimationP(const char* name, bool randomStart, bool paused, bool bounce) {
    animQueue.clear();
    return SetAnimation(name, -1, randomStart, paused, false, true, bounce);
}

bool Entity::SetAnimationOnce(const char* name, bool randomStart, bool paused, bool holdLast, bool notify) {
    animQueue.clear();
    return SetAnimation(name, 1, randomStart, paused, holdLast, notify, false);
}

bool Entity::SetAnimationMult(const char* name, int count, bool randomStart, bool paused, bool notify) {
    animQueue.clear();
    return SetAnimation(name, count, randomStart, paused, false, notify, false);
}

void Entity::EnqueueAnimation(const char* name, float time) {
    if (animQueue.empty() && !IsDead()) SetAnimationOnce(name, false, false, false, true);
    animQueue.push_back({name, time});
}

void Entity::SetAnimationFrame(int f) { anim->SetCurrentFrame(f); }

bool Entity::AnimExists(const char* name) const { return data->AnimationExists(name); }

void Entity::AnimationEnded() {
    // UNVERIFIED (milestone 4): class 0x14 (spells) forwards to the linked entity and flags removal;
    // a patrol step in state 3 moves to state 4.
    if (removeSpriteOnEnd) {
        SetActive(false, false);
        sprite = Render::RemoveSprite(sprite);
        removeSpriteOnEnd = false;
    }
    // UNVERIFIED (milestone 3, decorations): a decoration search animation ends here
    // (Decor::GetPlayerSearchAnimation).
    if (walkAwayOnEnd) {
        std::vector<AI::Waypoint*> near;
        AI::GetWaypointsInRange(near, (unsigned)tileX, (unsigned)tileY, 1, false);
        if (!near.empty()) {
            if (AI::Waypoint* wp = near[(unsigned)Rand48::lrand48() % near.size()]) ai->WalkTo(wp->x, wp->y);
        }
        walkAwayOnEnd = false;
    }
    if (entityState == 2) ChangeState(0);
}

void Entity::Update(float dt) {
    if (appearing && sprite) {
        if (alpha == 0.f && spawnSound) {
            spawnSound = false;
            // UNVERIFIED (milestones 4-5): class 10 soldiers play their spawn sound here.
        }
        if (alpha < 1.f) {
            alpha = alpha + dt * speedScale;
        } else {
            alpha = 1.f;
            appearing = false;
            OnAppeared();
        }
        Render::SetShaderType(sprite, 1);
        Render::SetAlpha(sprite, alpha);
    }
    // UNVERIFIED (milestone 5): the glow sprite (+0xa0) pulse.
    if (disappearing && sprite) {
        if (alpha <= 0.f) {
            alpha = 0.f;
            disappearing = false;
            OnDisappeared();
        } else {
            alpha = alpha - dt * speedScale;
        }
        Render::SetShaderType(sprite, 1);
        Render::SetAlpha(sprite, alpha);
    }
    if (greyed && sprite) Render::SetShaderType(sprite, 3);
    // UNVERIFIED (milestone 4): the player's squad update, ConsumeHpOverlimit and projectiles.
    if (ai) ai->Update(dt);
    if (anim) {
        if (anim->anim) anim->Update(dt * speedScale);
        if (anim && anim->anim) UpdateGraphics();
    }
    UpdateRing();
    // UNVERIFIED (milestone 5): UpdateParticles.
    if (!animQueue.empty() && !IsDead()) {
        animQueue.front().time -= dt;
        if (animQueue.front().time < 0.f) {
            animQueue.erase(animQueue.begin());
            if (!animQueue.empty()) SetAnimation(animQueue.front().name.c_str(), 1, false, false, false, true, false);
        }
    }
    // UNVERIFIED (milestone 4): patrols (+0xe0), spawn-point wandering and respawns.
    if (hoverWindow &&
        !BuildingHovers::SetHoverWindowPosition(hoverWindow, (int)worldX, (int)(worldY - GetIdleHeight())))
        hoverWindow = nullptr;
    // UNVERIFIED (milestone 4): meta-expression spawns, the boss camera/time modifier and class 9
    // events.
}

float Entity::GetIdleHeight() {
    if (!data || f190 == 0.f) UpdateIdleInfo();
    return f190;
}

void Entity::UpdateIdleInfo() {
    if (!sprite || !sprite->tex) return;
    Render::Texture* t = sprite->tex;
    f18c = (float)t->w;
    f190 = (float)t->h / (t->frames != 0 ? (float)t->frames : 1.f);
}

void Entity::UpdateGraphics() {
    if (!sprite || !active) return;
    Animation* a = anim->anim;
    Render::Texture* t = a->GetTexByDir(direction);
    if (sprite->tex != t) Render::SetTexture(sprite, t);
    float fh = (float)Render::GetFrameHeight(sprite->tex);
    float fw = (float)Render::GetFrameWidth(sprite->tex);
    Render::SetFrame(sprite, fh, fw, anim->frame);
    int ox = 0, oy = 0;
    anim->anim->GetOffsets(direction, ox, oy);
    float dataOffX = (float)data->offX, dataOffY = (float)data->offY;
    float baseY = (worldY - 21.f) + dataOffY;
    float x = dataOffX + worldX + sprite->w * -0.5f;
    if (direction < 5) x = (float)ox + x;
    else x = x - (float)ox;
    float y = (float)oy + baseY + sprite->h * 0.5f;
    if (customZ <= 0.f) Render::SetPosition(sprite, x, y, Map::GetSpriteZ(baseY - dataOffY, 0.f, 0));
    else Render::SetPosition(sprite, x, y, customZ);
    if (ring)
        Render::SetPosition(ring, dataOffX + worldX + ring->w * -0.5f, (baseY - dataOffY) + ring->h * 0.5f, 0.5f);
    if (underlay) Render::SetPosition(underlay, x, y, Map::GetSpriteZ(baseY - dataOffY, 0.f, 0));
    // (Map::debugEntity's id/position overlay (+0x80) is off)
}

void Entity::UpdateRing() {
    if (!active || (IsDead() && !player)) {
        CreateUnderlay(0);
        return;
    }
    if (f198 != 0) {
        CreateUnderlay(f198);
        return;
    }
    // (combat not ported: never active, so no boss/enemy/squad combat rings)
    // UNVERIFIED (milestone 4): IsBoss and IsEnemy need meta expressions and spawn points; both
    // are false for everything spawned so far.
    if (player) {
        CreateUnderlay(4);
        return;
    }
    if (selected != 0 || f178 != 0) {
        CreateUnderlay(1);
        return;
    }
    if (ringStyle != 1) return;   // (class 10 squad members get style 5, milestone 4)
    CreateUnderlay(0);
}

void Entity::CreateUnderlay(int style) {
    if (ringStyle == style) return;
    ringStyle = style;
    ring = Render::RemoveSprite(ring);
    if (ringStyle == 0) return;
    Render::Texture* t = Resources::GetImage(kRingImages[ringStyle]);
    if (!t) t = Resources::GetDirectImage(kRingImages[ringStyle]);
    ring = Render::CreateSprite(t, Render::kLayerRings, false, false);
    if (data && ring) {
        float oy = (float)data->offY;
        Render::SetPosition(ring, (float)data->offX + worldX + ring->w * -0.5f,
                            (((worldY - 21.f) + oy) - oy) + ring->h * 0.5f, 0.5f);
    }
}

void Entity::Appear(bool glow, bool sound) {
    disappearing = false;
    appearing = true;
    SetAlpha(0.f);
    // UNVERIFIED (milestone 5): ShowGlowAnimation when glow (and the tutorial is not at 0x1a).
    (void)glow;
    // UNVERIFIED: Map::IsMapLoading (true only during Map::Load) gates this.
    spawnSound = sound;
    // UNVERIFIED (milestone 5): class 0x10 plays "goblin_spawned"; class 0 workers play
    // "interact_entity_" + M/F + N/S + "1talks".
}

void Entity::OnDisappeared() {
    int c = data->clas;
    if (c == 0x10) {
        SetActive(false, false);
        SetCurrentMap(0);
        return;
    }
    if (c == 0) {
        SetActive(false, false);
        return;
    }
    if (c != 10 || data->type == 0x14) return;
    if (EntityManager::GetEntityCountByID(data->id) < 2) return;
    SetActive(false, false);
}

void Entity::SetActive(bool on, bool idle) {
    if (!on) {
        active = false;
        if (ai) ai->ClearPath();
        if (sprite) Render::SetVisibility(sprite, false);
        if (selected != 0) selected = 0;
        if (f178 != 0) f178 = 0;
        UpdateRing();
        RemoveGlow();
        return;
    }
    if (ai) ai->Reset(idle);
    active = true;
    alpha = 1.f;
    if (sprite) {
        Render::SetAlpha(sprite, alpha);
        Render::SetVisibility(sprite, true);
    }
}

void Entity::RemoveGlow() { glow = Render::RemoveSprite(glow); }

void Entity::CreateAnims() {
    if (data->anims.empty()) return;
    anim = new AnimationController(this);
    SetAnimationP("idle_1", true, false, false);
}

void Entity::CreateAI() {
    switch (data->clas) {
    case 5:
        initiative = 0x32;
        hp = 10;
        player = true;
        ChangeAIState(2);
        return;
    case 0: ChangeAIState(4); return;
    case 10: ChangeAIState(1); return;
    case 9: ChangeAIState(9); return;
    case 6: ChangeAIState(5); return;
    case 2: ChangeAIState(8); return;
    case 3:
        ChangeAIState(6);
        customZ = 1.f;
        hp = 10;
        return;
    case 0x10: ChangeAIState(7); return;
    case 0x14: ChangeAIState(10); return;
    default: return;   // (0xe and the rest have no AI)
    }
}

void Entity::ChangeAIState(int state) {
    delete ai;
    ai = nullptr;
    ai = CreateAIState(state, this);
}

void Entity::OnMoved() {
    // Observers' AIs are AIBaseState-derived classes whose +0x178 slot takes the moved entity.
    // UNVERIFIED (milestone 4): nothing registers observers yet (squads and followers).
}

void Entity::OnWalkComplete() {
    // UNVERIFIED (milestone 4): a patrol step in state 1 moves to state 2 (its wait time and
    // facing).
}

void Entity::OnUnableToWalk(int x, int y) {
    if (!player) std::printf("entity AIBaseState::OnUnableToWalk(%d, %d)\n", x, y);
    else std::printf("player AIBaseState::OnUnableToWalk(%d, %d)\n", x, y);
}

void Entity::OnWaypointRemove(AI::Waypoint* wp) {
    if (waypoint == wp) waypoint = nullptr;
}

void Entity::OnStartedToWork() {
    // (the player at tutorial step 0x14 centres the camera on itself)
    if (player && GameState::TutorialStep() == 0x14) Render::CenterOn(worldX, worldY);
}

bool Entity::Contains(int x, int y) const {
    if (!sprite || !f9c) return false;
    // UNVERIFIED (milestone 4): IsBoss (meta expressions) divides the width by 3 instead.
    float half = sprite->w * 0.5f;
    float fx = (float)x;
    if (fx < worldX - half || fx > half + worldX) return false;
    float fy = (float)y;
    if (fy < worldY - sprite->h) return false;
    return fy <= worldY;
}

void Entity::OnClick() {
    // (GetSpawnPointID: spawn points are not ported, so always 0)
    std::printf("Clicked on entity: %d (spawn %d)\n", data->id, 0);
}

void Entity::SetWorkplace(Map::Building* b) {
    workplace = b;
    if (!b) {
        workY = 0;
        f78 = 0;
        workType = 0;
        workX = 0;
    } else {
        workType = 1;
        workX = b->x;
        workY = b->y;
    }
}

void Entity::SetWorkplaceDecoration(Map::Decor* d) {
    workplaceDecor = d;
    if (!d) {
        workY = 0;
        workType = 0;
        workX = 0;
    } else {
        workType = 2;
        workX = d->x;
        workY = d->y;
    }
}

void Entity::SetHome(Map::Building* b) {
    home = b;
    hasHome = true;
    homeX = b->x;
    homeY = b->y;
}

bool Entity::IsMale() const {
    if (data->id != 0x16b) return data->female == 0;
    if (anim && anim->anim) {
        const char* n = anim->anim->name.c_str();
        if (std::strstr(n, "idle_1") || std::strstr(n, "witch") || std::strstr(n, "washing") ||
            std::strstr(n, "winter_fat_female"))
            return false;
        return std::strstr(n, "winery") == nullptr;
    }
    return true;
}

bool Entity::IsFat() const {
    if (data->id != 0x16b) return data->soundCharType == 0;
    if (anim && anim->anim) {
        const char* n = anim->anim->name.c_str();
        for (const char* s : {"fisherman", "jeweler", "ranger", "rogue", "wizard", "beekeeper", "washing",
                              "winter_fisherman", "winter_skinny_fire"})
            if (std::strstr(n, s)) return false;
        return std::strstr(n, "harvest_berry") == nullptr;
    }
    return true;
}

// Each equipped item's SPEED adds n / 100 (for every entity: the player's equipment).
float Entity::GetBaseSpeedMultiplier() const {
    float m = 1.f;
    for (unsigned i = 0; i < 10; ++i) {
        GameState::PlayerItem* it = GameState::GetItemAt(i);
        if (!it || !it->info || !it->info->meta) continue;
        if (const MetaData* d = it->info->meta->GetDataWithType(kExpSpeed)) m += (float)d->GetInt() / 100.f;
    }
    return m;
}

namespace {
void AddClamped(int& field, int v, int min) {   // PORT: the Add* pattern
    field += v;
    if (field < min) field = min;
}
}

void Entity::AddHpMax(int v) { AddClamped(hpMax, v, 1); }
void Entity::AddAttackMelee(int v) { AddClamped(attackMelee, v, 0); }
void Entity::AddAttackRanged(int v) { AddClamped(attackRanged, v, 0); }
void Entity::AddAttackMagic(int v) { AddClamped(attackMagic, v, 0); }
void Entity::AddDefenseMelee(int v) { AddClamped(defenseMelee, v, 0); }
void Entity::AddDefenseRanged(int v) { AddClamped(defenseRanged, v, 0); }
void Entity::AddDefenseMagic(int v) { AddClamped(defenseMagic, v, 0); }
void Entity::AddAbsorbMelee(int v) { AddClamped(absorbMelee, v, 0); }
void Entity::AddAbsorbRanged(int v) { AddClamped(absorbRanged, v, 0); }
void Entity::AddAbsorbMagic(int v) { AddClamped(absorbMagic, v, 0); }
void Entity::AddCritChance(int v) { AddClamped(critChance, v, 0); }
void Entity::AddFuryBonus(int v) { AddClamped(furyBonus, v, 0); }
void Entity::AddInitiative(int v) { AddClamped(initiative, v, 0); }

bool Entity::HasRangedDamage() const { return data->GetAttackRangedForLevel(GameState::GetLevel()) > 0; }
bool Entity::HasMagicDamage() const { return data->GetAttackMagicForLevel(GameState::GetLevel()) > 0; }

int Entity::GetAttackRanged() const {
    if (HasRangedDamage() && attackRanged != 0) return attackRanged + f88;
    return 0;
}

int Entity::GetAttackMagic() const {
    if (HasMagicDamage() && attackMagic != 0) return attackMagic + f88;
    return 0;
}

void Entity::SetHP(int v) {
    hp = v < 0 ? 0 : v;
    if (!player) return;
    if (hp != 0) {
        if (!greyed) return;
        if (sprite) Render::SetShaderType(sprite, 1);
        greyed = false;
        if (hp != 0) return;
    }
    greyed = true;
}

void Entity::ResetStats(bool hpToo) {
    int lvl = GameState::GetLevel();
    if (hpToo) hpMax = data->GetHpForLevel(lvl);
    attackMelee = data->GetAttackMeleeForLevel(lvl);
    attackRanged = data->GetAttackRangedForLevel(lvl);
    attackMagic = data->GetAttackMagicForLevel(lvl);
    defenseMelee = data->GetDefenseMeleeForLevel(lvl);
    defenseRanged = data->GetDefenseRangedForLevel(lvl);
    defenseMagic = data->GetDefenseMagicForLevel(lvl);
    absorbMagic = absorbMelee = absorbRanged = 0;
    hpRate = data->hpRate;
    if (overrideAttack != 0) {
        if (attackMelee != 0) attackMelee = overrideAttack;
        if (attackRanged != 0) attackRanged = overrideAttack;
        if (attackMagic != 0) attackMagic = overrideAttack;
    }
    if (overrideDefense != 0) {
        if (defenseMelee != 0) defenseMelee = overrideDefense;
        if (defenseRanged != 0) defenseRanged = overrideDefense;
        if (defenseMagic != 0) defenseMagic = overrideDefense;
    }
    f84 = f8c = f88 = 0;
    f90 = 1.f;
    critChance = data->critChance != 0 ? data->critChance : 10;
    luck = furyBonus = 0;
    // (a spawn point's +0x28 goes to +0x130; spawn points are not ported)
    if (!player) return;
    initiative = 0x32;
    GameState::SetBeltSlotCount(2);
    GameState::SetBeltSize(0);
}

int Entity::GetHpOverlimit() {
    if (!player) return 0;
    if (Setting("hp_gift_overlimit").GetInt() < ffc) ffc = Setting("hp_gift_overlimit").GetInt();
    return ffc;
}

void Entity::SetAP(int v) {
    f100 = v;
    if (v > GetApMax()) f100 = GetApMax();
}

void Entity::AddAP(int v) {
    int old = f100;
    f100 = v + old;
    if (GetApMax() < v + old) f100 = GetApMax();
    else if (f100 < 0) f100 = 0;
}

void Entity::SetupPlayerHPOverlimit(bool keepExcess, bool consume) {
    g_hpConsume = consume;
    g_hpKeepExcess = keepExcess;
}

void Entity::AddHP(int v) {
    int old = hp;
    hp = v + old;
    if (GetHpMax() < v + old) {
        if (player && g_hpKeepExcess) {
            ffc = hp + ffc - GetHpMax();
            if (Setting("hp_gift_overlimit").GetInt() < ffc) ffc = Setting("hp_gift_overlimit").GetInt();
        }
        hp = GetHpMax();
    } else if (hp < 0) {
        hp = 0;
    }
    if (!player) return;
    if (hp == GetHpMax() && GameState::IsTaskStarted(0x2f9)) {
        // UNVERIFIED (milestone 4c): Tasks::CompleteSubtask(0x27, 1, 1).
    }
    if (v != 0 || hp == GetHpMax()) GameState::UpdatePlayerRegenerationState();
    if (greyed) {
        if (sprite) Render::SetShaderType(sprite, 1);
        greyed = false;
    }
}

void Entity::ConsumeHpOverlimit(bool addHp) {
    if (!player || !g_hpConsume || ffc < 1 || GetHpMax() <= GetHP()) return;
    int n = ffc;
    if (GetHpMax() - GetHP() < n) n = GetHpMax() - GetHP();
    ffc -= n;
    if (!addHp) hp += n;
    else AddHP(n);
}

void Entity::CreateSquad() {
    delete squad;   // (an old player squad reports the removal as an error: see OnRemovedFromSquad)
    squad = nullptr;
    if (!player) squad = new EnemySquad(this);
    else squad = new PlayerSquad(this);
}

void Entity::OnAddedToSquad(BaseSquad* s) {
    squad = s;
    if (player) f3b = true;
}

void Entity::OnRemovedFromSquad() {
    if (!player) {
        squad = nullptr;
        return;
    }
    if (!g_removingPlayer)
        std::fprintf(stderr, "Entity::OnRemovedFromSquad() Player squad data could have been removed while the"
                             " player wasn't in the destructor");
}

bool Entity::HasEntity(Entity* e) { return ai ? ai->HasEntity(e) : false; }

bool Entity::IsEliteSoldier() const {
    if (!data) return false;
    switch (Setting("healable_soldiers").GetInt()) {
    case 1:
        if (data->warriorCost == 0 && data->warriorCost2 != 0) return data->type != 0x11;
        return false;
    case 2: return GameState::secondTutorial == 0x100 && data->type != 0x11;
    case 3: return data->warriorCost == 0 && data->warriorCost2 != 0;
    case 4: return GameState::secondTutorial == 0x100;
    default: return false;
    }
}

void Entity::EnableSpeedRun() {
    speedRunning = true;
    float s = ai->GetBaseSpeed() * GetBaseSpeedMultiplier();
    ai->SetSpeed(s + s);
}

void Entity::DisableSpeedRun() {
    speedRunning = false;
    ai->SetSpeed(ai->GetBaseSpeed() * GetBaseSpeedMultiplier());
}

void Entity::UpdateSafePos(Entity* e) {
    if (e->f36 || e->f38) return;
    e->spawnX = e->prevX;
    e->spawnY = e->prevY;
}

void Entity::SetSpawnPoint(SpawnPoint* sp) {
    spawnPoint = sp;
    // UNVERIFIED (milestone 4f): for a non-player the spawn point sets HP (its NPC flag +0x10: 0x400,
    // else the level's HP; +0x30/+0x34 an HP override), +0x130 (+0x28) and HpMax (+0x38/+0x3c).
}
