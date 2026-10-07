// The farm view's AI: the soil patches (AIPatch, @0xecfb0..0xee398), the farmer (AIFarmerBig,
// @0xea8b0..0xebd20) and the patch's growth-powder animation (PatchAnimationController,
// @0x1e3e04..0x1e3fdc).
//
// A farm building keeps each patch's order in patchContract (1-based), its growth start in
// patchStart and its rot timer start in patchArg; the patch AI turns those into its state every
// frame and writes the current stage's progress to the farm's patchProgress.
#include <cstdio>

#include "engine/Render.h"
#include "engine/Resources.h"
#include "engine/Timer.h"
#include "game/AIState.h"
#include "game/Animation.h"
#include "game/Background.h"
#include "game/Building.h"
#include "game/Entity.h"
#include "game/EntityManager.h"
#include "game/GameData.h"
#include "game/GameState.h"
#include "game/Map.h"
#include "game/Rand48.h"

// ------------------------------------------------------------------- PatchAnimationController

void PatchAnimationController::Update(float dt) {
    if (!tex) return;
    if (patch && patch->GetDust()) {
        Render::Sprite* s = patch->GetDust();
        if (s->tex != tex) Render::SetTexture(s, tex);
        Render::SetFrame(s, (float)Render::GetFrameHeight(tex), (float)Render::GetFrameWidth(tex), frame);
    }
    if (paused) return;
    acc += dt;
    while (0.f < acc) {
        ++frame;
        acc += 1.5f / (-1.f / tex->frameTime);
        if (frame < tex->frames || !patch) continue;
        patch->StopDust();
        if (acc <= 0.f) return;
    }
}

// ------------------------------------------------------------------------------------ AIPatch

AIPatch::AIPatch(Entity* e) : AIBaseState(e) {
    state = 0x12;
    farmBuilding = Map::GetCurrentFarm();
    farm = true;
    lockedTex = Resources::GetImage("images/Farm/soil_patch_locked2");
    buyTex = Resources::GetImage("images/Farm/soil_patch_buy2");
    uint32_t id = farmBuilding->data->id;
    if (id == 0x13) type = 1;
    else if (id == 0x72) type = 3;
    else if (id == 0x3ee) type = 4;
}

AIPatch::~AIPatch() { delete dustAnim; }

bool AIPatch::HasEntity(Entity* e) {
    // (the original also asks the crop entity's own HasEntity)
    return AIBaseState::HasEntity(e) || (crop && crop == e);
}

void AIPatch::UpdateLastTarget() { entity->AdjustWorldPos(46.f, 20.f); }

void AIPatch::StopDust() { dust = Render::RemoveSprite(dust); }

// Places the vegetable farm's sign texture at the patch, centred on its x.
static Render::Sprite* PlaceSign(Entity* e, Render::Texture* tex) {
    Render::Sprite* s = Render::CreateSprite(tex, Render::kLayerObjects, false, false);
    float wx, wy;
    e->GetWorldPos(wx, wy);
    Render::SetPosition(s, wx + s->w * -0.5f, wy, Map::GetSpriteZ(wy, (float)tex->w, 0));
    return s;
}

void AIPatch::ChangeState(int s) {
    if (!force && state == s) return;
    force = false;
    switch (s) {
    case 0x12:
    case 0x1c:
        if (type == 1) {
            Render::RemoveSprite(sign);
            sign = nullptr;
            entity->SetAnimationP("soil_patch_empty", true, false, false);
        } else if (type == 3) {
            entity->SetAnimationP("oil_farm_empty_patch", true, false, false);
        } else if (type == 4) {
            entity->SetAnimationP("animal_farm_patch", true, false, false);
        }
        break;
    case 0x13:
        if (type == 1) {
            Render::RemoveSprite(sign);
            sign = nullptr;
            entity->SetAnimationP("soil_patch_dirty", true, false, false);
        } else if (type == 3) {
            entity->SetAnimationP("oil_farm_trashed", true, false, false);
        } else if (type == 4) {
            entity->SetAnimationP("animal_farm_patch_clean", true, false, false);
        }
        break;
    case 0x14:
        if (type == 1) {
            Render::RemoveSprite(sign);
            sign = PlaceSign(entity, lockedTex);
            entity->SetAnimationP("soil_patch_empty", true, false, false);
        } else if (type == 3) {
            entity->SetAnimationP("oil_farm_patch_locked", true, false, false);
        }
        break;
    case 0x15: case 0x16: case 0x17: case 0x18:
        crop->SetAnimationFrame(s - 0x15);
        state = s;
        return;
    case 0x19:
        crop->SetAnimationFrame(4);
        state = s;
        speedingUp = false;
        return;
    case 0x1a:
        if (type == 4) {
            entity->SetAnimationP("animal_farm_patch_escaped", true, false, false);
            entity->AdjustWorldPos(0.f, 63.f);
            Render::SetVisibility(crop->GetSprite(), false);
            Background::SetBrokenFence(true);
        } else {
            crop->SetAnimationFrame(5);
        }
        break;
    case 0x1b:
        if (type == 1) {
            Render::RemoveSprite(sign);
            sign = PlaceSign(entity, buyTex);
            entity->SetAnimationP("soil_patch_empty", true, false, false);
        } else if (type == 3) {
            entity->SetAnimationP("oil_farm_patch_buy", true, false, false);
        }
        break;
    default:
        state = s;
        return;
    }
    speedingUp = false;
    state = s;
}

void AIPatch::Clean() {
    sign = Render::RemoveSprite(sign);
    dust = Render::RemoveSprite(dust);
    if (!crop) return;
    EntityManager::RemoveEntity(crop, true);
    crop = nullptr;
}

// The growth stage from the time since patchStart: 0x15 in the first growTimes step, one more per
// step, 0x19 past them all. Past the rot time (patchArg + timeRot) the crop rots once task 0x429
// is done; until then the rot timer gets 1200 s more. Tutorial step 0x45 holds a ready crop.
void AIPatch::Update(float dt) {
    AIBaseState::Update(dt);
    if (crop) {
        if (speedingUp) SpeedUp(-2);
        if (Render::Sprite* cs = crop->GetSprite()) crop->SetCustomZ(Map::GetSpriteZ(cs->y, cs->w * 0.5f, 0));
        uint32_t now = Timer::GetGlobalTime();
        Map::Building* cur = Map::GetCurrentFarm();
        if (now <= cur->patchArg[patchNum] + (uint32_t)mission.timeRot || GameState::tutorial == 0x45) {
            uint32_t elapsed = Timer::GetGlobalTime() - farmBuilding->patchStart[patchNum];
            const std::vector<int>& g = mission.growTimes;
            int stage = 0x15;
            if (!g.empty()) {
                bool progress = true;
                uint32_t before = 0, step = (uint32_t)g[0];
                if (step < elapsed) {
                    uint32_t acc = step;
                    size_t i = 0;
                    for (;;) {
                        before = acc;
                        size_t k = i++;
                        if (i == g.size()) {
                            stage = (int)k + 0x16;
                            progress = false;
                            break;
                        }
                        step = (uint32_t)g[i];
                        acc = step + before;
                        if (!(acc < elapsed)) {
                            stage = (int)k + 0x16;
                            break;
                        }
                    }
                }
                if (progress) farmBuilding->patchProgress[patchNum] = (float)(elapsed - before) / (float)step;
                if (stage == 0x19 && GameState::tutorial == 0x45) return;
            }
            ChangeState(stage);
            if (!dust) return;
            if (dustAnim) dustAnim->Update(dt);
            return;
        }
        if (!GameState::TaskCompleted(0x429)) {
            Map::GetCurrentFarm()->patchArg[patchNum] += 0x4b0;
            return;
        }
        ChangeState(0x1a);
    }
    if (!dust) return;
    if (dustAnim) dustAnim->Update(dt);
}

void AIPatch::SpeedUp(int n) {
    if (!crop) return;
    if (n == -2) {
        uint32_t now = Timer::GetGlobalTime();
        uint32_t start = farmBuilding->patchStart[patchNum];
        const std::vector<int>& g = mission.growTimes;
        if (g.empty()) return;
        uint32_t elapsed = now - start;
        uint32_t acc = (uint32_t)g[0];
        if (acc < elapsed) {
            size_t i = 0;
            do {
                if (++i == g.size()) return;
                acc += (uint32_t)g[i];
            } while (acc < elapsed);
        }
        farmBuilding->patchStart[patchNum] = (elapsed + start) - acc;
    } else if (n == -1) {
        speedingUp = true;
        Render::Texture* tex = Resources::GetImage("images/Buildings/mk/farm_powder/powder");
        if (!tex) return;
        dust = Render::CreateSprite(tex, 0xd, false, false);
        tex->frameTime = 0.125f;
        float wx, wy;
        entity->GetWorldPos(wx, wy);
        Render::SetPosition(dust, wx + dust->w * -0.5f, wy + 15.f, Map::GetSpriteZ(wy, dust->w, 0));
        if (!dustAnim) dustAnim = new PatchAnimationController(this);
        dustAnim->SetAnim(tex);
    } else {
        farmBuilding->patchStart[patchNum] -= (uint32_t)n;
        farmBuilding->patchArg[patchNum] -= (uint32_t)n;
    }
}

// The crop entity per delivery list and order; each also sets the farm's small-farm crop entity
// (Building +0x160, shown on the city map).
static const struct { int crop, cityCrop; } kCrops[3][5] = {
    {{0x12, 0xca}, {0x19, 0xc9}, {0x16, 0xcd}, {0x13, 0xcb}, {0x11, 0xcc}},   // list 1 (vegetables)
    {{0x1a, 0xce}, {0x1b, 0xcf}, {0x0f, 0xd3}, {0x1e, 0xd2}, {0x1c, 0xd0}},   // list 3 (oil)
    {{0xe2, 0x26}, {0xe1, 0x27}, {0xe3, 0x28}, {0xde, 0x2a}, {0xe4, 0x29}},   // list 4 (animals)
};

void AIPatch::SetItem(int deliveryId, int contract, int p) {
    cropId = contract;
    int list = deliveryId == 1 ? 0 : deliveryId == 3 ? 1 : deliveryId == 4 ? 2 : -1;
    if (list >= 0 && contract >= 0 && contract < 5) {
        cropId = kCrops[list][contract].crop;
        farmBuilding->farmEntityId = kCrops[list][contract].cityCrop;
    }
    const GameData::BuildingData* d = farmBuilding->data;
    int idx = farmBuilding->patchContract[p] - 1;
    int last = d->delivery ? (int)d->delivery->missions.size() - 1 : -1;
    if (cropId == 0) std::fprintf(stderr, "ERROR: AIPatch::SetItem() mCurrentItem = 0\n");
    if (!d->delivery) std::fprintf(stderr, "ERROR: AIPatch::SetItem() !mFarm->bData->contract\n");
    if (p > 6) std::fprintf(stderr, "ERROR: AIPatch::SetItem() patch > 6\n");
    if (last < idx) std::fprintf(stderr, "ERROR: AIPatch::SetItem() idx > size\n");
    // (the original goes on with a negative idx; PORT: no copy then, as for the other errors)
    if (cropId == 0 || !d->delivery || p > 6 || last < idx || idx < 0) return;
    patchNum = p;
    mission = d->delivery->missions[(size_t)idx];
    if (state != 0x12) return;
    crop = EntityManager::SpawnEntityAt(cropId, (unsigned)entity->tileX, (unsigned)entity->tileY, false, false);
    crop->AdjustWorldPos(46.f, 20.f);
    if (deliveryId == 4) crop->SetAnimationP("idle_1", true, false, false);
    else crop->SetAnimationP("crop", false, true, false);
}

// The animal farm's escaped animals come back (the fence is mended).
void AIPatch::Revive() {
    if (type != 4) return;
    entity->SetAnimationP("animal_farm_patch", true, false, false);
    entity->AdjustWorldPos(-3.f, -35.f);
    Render::SetVisibility(crop->GetSprite(), true);
    Background::SetBrokenFence(false);
}

void AIPatch::CleanFarm() {
    if (type != 4) return;
    entity->SetAnimationP("animal_farm_patch_clean", true, false, false);
    entity->AdjustWorldPos(-3.f, -35.f);
    Background::SetBrokenFence(false);
}

// -------------------------------------------------------------------------------- AIFarmerBig

AIFarmerBig::AIFarmerBig(Entity* e) : AIBaseState(e) {
    farm = true;
    speed = 159.6f;
    current = AI::GetWaypoint(e->tileX, e->tileY, true);
    e->GetAnimController()->mult = 0.8f;
}

// Patches it was still going to work on keep their saved state: a dirty one counts as empty, a
// rotten one as dirty without its order.
AIFarmerBig::~AIFarmerBig() {
    for (const FarmActionQueue& q : queue) {
        Map::Building* b = entity->GetWorkplace();
        if (!b || b->patchEntities.empty()) continue;
        int st = b->patchEntities[(size_t)q.patch]->GetAI()->GetState();
        if (st == 0x13) {
            b->resources[q.patch] = 0x12;
        } else if (st == 0x1a) {
            b->patchContract[q.patch] = 0;
            b->resources[q.patch] = 0x13;
        }
    }
}

void AIFarmerBig::UpdateLastTarget() {
    AI::Waypoint* wp = AI::GetWaypoint(entity->tileX, entity->tileY, true);
    if (!wp) return;
    current = wp;
    x = entity->worldX;
    y = entity->worldY;
}

// The work animation plays twice; each play lasts frames x 1.5 / fps x the speed multiplier.
static float WorkTime(Entity* e) {
    AnimationController* ac = e->GetAnimController();
    return (1.5f / ac->anim->fps) * ac->mult * (float)ac->anim->frameCount * 2.f;
}

void AIFarmerBig::StartHarvesting() {
    ChangeState(0xd);
    entity->OnStartedToWork();
    entity->GetWorkplace()->WorkStarted();
    loops = 1;
    // SoundsManager::PlaySound("farm_harvest_animal" / "farm_harvest_vegetable"): sounds are not
    // ported yet.
    if (entity->GetWorkplace()->id == 0x3ee) entity->SetAnimationOnce("seed", false, false, false, true);
    else entity->SetAnimationOnce("mow", false, false, false, true);
    water = false;
    actionTime = actionLeft = WorkTime(entity);
}

void AIFarmerBig::StartCleaning() {
    // SoundsManager::PlaySound("farm_start_cleaning")
    ChangeState(0xe);
    entity->OnStartedToWork();
    entity->GetWorkplace()->WorkStarted();
    loops = 1;
    entity->SetAnimationOnce("showel", false, false, false, true);
    water = false;
    actionTime = actionLeft = WorkTime(entity);
}

void AIFarmerBig::StartWatering() {
    ChangeState(0xc);
    loops = 1;
    if (entity->GetWorkplace()->id == 0x3ee) entity->SetAnimationOnce("seed", false, false, false, true);
    else entity->SetAnimationOnce("watering", false, false, false, true);
    water = false;
    idleTime = 5.f;
    actionTime = actionLeft = WorkTime(entity);
}

void AIFarmerBig::StartSeeding() {
    // SoundsManager::PlaySound("farm_start_seeding")
    ChangeState(0xb);
    entity->OnStartedToWork();
    entity->GetWorkplace()->WorkStarted();
    loops = 1;
    entity->SetAnimationOnce("seed", false, false, false, true);
    water = false;
    actionTime = actionLeft = WorkTime(entity);
}

void AIFarmerBig::AnimEnded() {
    if (loops < 1) {
        entity->SetAnimationP("idle_1", true, false, false);
        return;
    }
    bool animal = entity->GetWorkplace() && entity->GetWorkplace()->id == 0x3ee;
    switch (state) {
    case 0xb: entity->SetAnimationOnce("seed", false, false, false, true); break;     // + "farm_start_seeding"
    case 0xc: entity->SetAnimationOnce(animal ? "seed" : "watering", false, false, false, true); break;
    case 0xe: entity->SetAnimationOnce("showel", false, false, false, true); break;   // + "farm_start_cleaning"
    case 0xd: entity->SetAnimationOnce(animal ? "seed" : "mow", false, false, false, true); break;   // + the harvest sound
    default: break;
    }
    --loops;
}

void AIFarmerBig::Move(bool, int p) {
    Entity* pe = entity->GetWorkplace()->patchEntities[(size_t)p];
    if (!pe) return;
    patch = p;
    AI::Waypoint* wp = AI::GetWaypoint(pe->tileX, pe->tileY, farm);
    WalkTo(wp->x, wp->y);
    moveTarget = wp;
    ChangeState(10);
}

void AIFarmerBig::ChooseAction() {
    Map::Building* b = entity->GetWorkplace();
    if (!idle) {
        auto st = [&] { return b->patchEntities[(size_t)patch]->GetAI()->GetState(); };
        if (st() == 0x12) StartSeeding();
        if (st() == 0x13 || st() == 0x1a) StartCleaning();
        if (st() == 0x19) StartHarvesting();
    }
    int fs = b->GetFarmState(patch);
    if (1 < fs && fs < 7) {
        idleTime = 2.f;
        water = true;
    }
}

Entity* AIFarmerBig::GetRandomPatch(bool all) {
    Map::Building* b = entity->GetWorkplace();
    int n = 0;
    for (Entity* pe : b->patchEntities) {
        int st = pe->GetAI()->GetState();
        if (st == 0x1b || st == 0x14) continue;
        if (all || st != 0x12) ++n;
    }
    if (n == 0) return nullptr;
    return b->patchEntities[(size_t)(Rand48::lrand48() % n)];
}

void AIFarmerBig::Update(float dt) {
    AIBaseState::Update(dt);
    if (entity) entity->SetCustomZ(Map::GetSpriteZ(y, 0.f, 0));
    if (moveTarget) {
        if (moveTarget->x != entity->tileX || moveTarget->y != entity->tileY) return;
        entity->SetDirection(1);
        ChooseAction();
        moveTarget = nullptr;
        return;
    }
    Map::Building* b = entity->GetWorkplace();
    if (state == 0) {
        idleTime -= dt;
        if (0.f < idleTime) return;
        if (-1 < patch) {
            int fs = b->GetFarmState(patch);
            if (1 < fs && fs < 6) {
                if (water && (entity->tileX != 0 || entity->tileY != 1)) {
                    StartWatering();
                    return;
                }
                // (PORT: the original reads the patch without checking for none)
                if (Entity* pe = GetRandomPatch(false)) Move(false, pe->GetAI()->GetFarmPatchNum());
                return;
            }
        }
        if (Entity* pe = GetRandomPatch(false)) Move(false, pe->GetAI()->GetFarmPatchNum());
        return;
    }
    actionLeft -= dt;
    if (state == 0xb || state == 0xe || state == 0xd)
        b->patchProgress[patch] = (actionTime - actionLeft) / actionTime;
    if (!(actionLeft < 0.f)) return;
    if (!idle) {
        if (!queue.empty()) queue.erase(queue.begin());
        b->WorkEnded(patch);
        if (!queue.empty()) {
            idle = false;
            actionTime = actionLeft = 0.f;
            Move(queue[0].flag == 0, queue[0].patch);
            return;
        }
    }
    actionTime = 0.f;
    idle = true;
    ChangeState(0);
    idleTime = 5.f;
}

void AIFarmerBig::Farm(int contract, int p, bool plant) {
    for (const FarmActionQueue& q : queue)
        if (q.patch == p && q.contract != 999) return;
    Map::Building* b = entity->GetWorkplace();
    b->patchProgress[p] = 0.f;
    idle = false;
    if (plant) b->LaunchContract((unsigned)contract, p);
    queue.push_back({p, contract, 0});
    if (queue.size() < 2) {
        actionTime = actionLeft = 0.f;
        Move(plant, p);
    }
}
