#include "game/Building.h"

#include <cmath>
#include <cstdio>

#include "engine/Render.h"
#include "engine/Resources.h"
#include "engine/Timer.h"
#include "game/AIState.h"
#include "game/BuildingHovers.h"
#include "game/Contracts.h"
#include "game/Entity.h"
#include "game/EntityManager.h"
#include "game/EntityData.h"
#include "game/GameData.h"
#include "game/GameState.h"
#include "game/Map.h"
#include "game/Rand48.h"
#include "game/MetaData.h"
#include "game/Setting.h"
#include "game/StringTable.h"

namespace Map {
namespace {

std::vector<const GameData::BuildingPart*> g_filtered;   // 0x6117c4
uint32_t g_latestUniqueId = 0;                            // GameState::latestUniqueID
bool g_wipLoaded = false;                                 // 0x611800
Render::Texture* g_wip[7] = {};                           // 0x611804 WipBase_1x1 .. 4x4
Render::Texture* g_construction[4] = {};                  // 0x611820 stone big/small, wood big/small

// @0x11ecb8
void InitWIPImages() {
    if (g_wipLoaded) return;
    static const char* names[7] = {"%s/WipBase_1x1", "%s/WipBase_2x2", "%s/WipBase_2x3", "%s/WipBase_3x2",
                                   "%s/WipBase_3x3", "%s/WipBase_3x4", "%s/WipBase_4x4"};
    g_wipLoaded = true;
    char path[256];
    for (int i = 0; i < 7; ++i) {
        std::snprintf(path, sizeof path, names[i], "images/Buildings");
        g_wip[i] = Resources::GetImage(path);
    }
    g_construction[0] = Resources::GetImage("images/Buildings/stone_construction_big");
    g_construction[1] = Resources::GetImage("images/Buildings/stone_construction_small");
    g_construction[2] = Resources::GetImage("images/Buildings/wood_construction_big");
    g_construction[3] = Resources::GetImage("images/Buildings/wood_construction_small");
}

// The scaffold image for a footprint (UpdateImage): 1x1, 2x2, 2x3, 3x2, 3x3, 3x4, 4x4; any other
// size gets index 7, one past the table (reads the next global on the original).
int WipIndex(int w, int h) {
    if (w == 1) return h == 1 ? 0 : 7;
    if (w == 2) return h == 2 ? 1 : h == 3 ? 2 : 7;
    if (w == 3) return h == 2 ? 3 : h == 3 ? 4 : 7;
    if (w == 4) return h == 3 ? 5 : h == 4 ? 6 : 7;
    return 7;
}

int FrameHeight(const Render::Texture* t) {   // Render::Texture::GetFrameHeight @0x20146c
    return t->frames != 0 ? t->h / t->frames : t->h;
}

// @0x11ef2c: one part sprite at (baseX - texW/2 + off_x, baseY + off_y), x truncated, each part
// 0.1 further in z. Returns the part's height.
int CreateBuildingPartSprite(float baseX, float baseY, bool mirror, Building& b, const GameData::BuildingPart* p) {
    Render::Texture* tex = GameData::PartImage(p);
    if (!tex) return p->height;
    if (tex->frames > 0) tex->frameTime = p->frameTime;
    Render::Sprite* s = Render::CreateSprite(tex, Render::kLayerObjects, mirror, false);
    float zSpan = (float)(unsigned)(b.data->h + b.data->w) * 84.f * 0.5f;
    s->extra = b.baseY;
    b.mainSprite = s;
    b.sprites.insert(b.sprites.begin(), s);
    float z = GetSpriteZ(b.baseY + b.partZ, zSpan, 0);
    Render::SetPosition(s, baseX + (float)tex->w * -0.5f + (float)p->offX, (float)p->offY + baseY, z);
    s->x = (float)(int)s->x;
    b.partZ += 0.1f;
    return p->height;
}

// The scaffold sprite (UpdateImage, under construction / upgrading / no image).
void CreateWipSprite(Building& b, int w, int h) {
    Render::Sprite* s = Render::CreateSprite(g_wip[WipIndex(w, h)], Render::kLayerObjects, false, false);
    if (!s) return;   // (index 7 is past the table; the original reads whatever follows it)
    b.mainSprite = s;
    b.sprites.insert(b.sprites.begin(), s);
    float z = GetSpriteZ(b.baseY - 2.f, s->w, 0);
    Render::SetPosition(s, b.baseX + s->w * -0.5f, b.baseY, z);
}

}  // namespace

const std::vector<const GameData::BuildingPart*>& FilterBuildingParts(const GameData::BuildingData& d, int type, int stage) {
    g_filtered.clear();
    int order = 0;
    for (const auto& p : d.parts) {
        if (p.type != type) continue;
        if (p.stage == -1) {
            if (order == stage) g_filtered.push_back(&p);
            ++order;
        } else if (p.stage == stage) {
            g_filtered.push_back(&p);
        }
    }
    return g_filtered;
}

void BuildingAnim::Update(float dt) {
    if (!tex) return;
    if (dt > 8.f) dt = 8.f;
    if (owner && owner->mainSprite) {
        Render::Sprite* s = owner->mainSprite;
        int f = shownFrame;
        if (tex != s->tex) {
            Render::SetTexture(s, tex);
            f = shownFrame = -1;
        }
        if (f != frame) {
            Render::SetFrame(s, (float)FrameHeight(tex), (float)tex->w, frame);
            f = frame;
        }
        shownFrame = f;
    }
    if (paused) return;
    acc += dt;
    while (acc > 0.f) {
        if (++frame >= tex->frames) frame = 0;
        acc += 1.5f / (-1.f / tex->frameTime);
    }
}

Building::Building() {
    workers.clear();
}

Building::~Building() {
    for (auto* s : sprites) Render::RemoveSprite(s);
    Render::RemoveSprite(ring);
    delete meta;
}

void Building::GetBuildZone(int& w, int& h) const {
    if (!data) { w = h = 0; return; }
    w = data->w;
    h = data->h;
}

void Building::GetStartTile(int& tx, int& ty) const {
    if (!data) { tx = ty = 0; return; }
    tx = x;
    unsigned uy = y;
    ty = (int)uy;
    int n = (mirrored ? data->h : data->w) / 2;
    for (int i = 0; i < n; ++i) {
        if ((uy & 1) == 0) --tx;
        uy = (unsigned)++ty;
    }
}

void Building::FindBaseCoordinates() {
    const GameData::BuildingData* d = GameData::GetBuilding(id);
    float wx = (float)x * 84.f + ((y & 1) ? 42.f : 0.f);
    float wy = (float)y * 42.f * 0.5f;
    float hx = (float)((unsigned)d->w >> 1), hy = (float)((unsigned)d->h >> 1);
    float bx;
    if (!mirrored) {
        bx = (42.f + hx * -42.f) - (float)d->offsetX;
    } else {
        // The first part of the last FilterBuildingParts call (whatever it was for).
        float first = g_filtered.empty() ? 0.f : (float)g_filtered[0]->offX;
        bx = (42.f + hx * -42.f + (float)d->offsetX) - first;
    }
    float by = hy * 21.f - (float)d->offsetY;
    baseX = minX = maxX = bx + wx;
    baseY = maxY = minY = by + wy;
    for (auto* s : sprites) {
        if (s->x < minX) minX = s->x;
        if (maxX < s->x + s->w) maxX = s->x + s->w;
        float top = s->y - s->h;
        if (top < minY) minY = top;
        if (maxY < s->y) maxY = s->y;
    }
}

void Building::UpdateImage() {
    const GameData::BuildingData* d = GameData::GetBuilding(id);
    for (auto* s : sprites) Render::RemoveSprite(s);
    sprites.clear();
    ring = Render::RemoveSprite(ring);
    partZ = 0.f;
    InitWIPImages();
    int w = d->w, h = d->h;
    GetBuildZone(w, h);
    bool wip = upgrading != 0 || !IsOpened();
    // UNVERIFIED (milestone 4): in a combat, a building at 0 hp also shows the scaffold.
    if (wip) {
        CreateWipSprite(*this, w, h);
    } else {
        const std::vector<const GameData::BuildingPart*>* parts = &FilterBuildingParts(*d, 0, level);
        for (int lv = level - 1; parts->empty() && lv >= 0; --lv) parts = &FilterBuildingParts(*d, 0, lv);
        if (parts->empty()) {
            std::fprintf(stderr, "ERROR: Building::UpdateImage() Building %d with upgrade %d doesn't have an image\n", id, level);
        } else {
            const GameData::BuildingPart* first = (*parts)[0];
            CreateBuildingPartSprite(baseX, baseY, mirrored, *this, first);
            if (Render::Texture* tex = first->image) {
                if (tex->frames > 0) {
                    anim.acc = 0.f;
                    anim.frame = 0;
                    anim.tex = tex;
                    anim.shownFrame = -1;
                }
                std::vector<const GameData::BuildingPart*> decors = FilterBuildingParts(*d, 3, level);
                for (const auto* p : decors) CreateBuildingPartSprite(baseX, baseY, mirrored, *this, p);
            } else {
                // No image for this level: the level-0 part, else the scaffold. (The original prints the
                // error in both cases.)
                const auto& base = FilterBuildingParts(*d, 0, 0);
                if (!base.empty() && GameData::PartImage(base[0]))
                    CreateBuildingPartSprite(baseX, baseY, mirrored, *this, base[0]);
                else
                    CreateWipSprite(*this, w, h);
                std::fprintf(stderr, "ERROR: Building::UpdateImage() Building %d with upgrade %d doesn't have an image\n", id, level);
            }
        }
    }
    anim.owner = this;
    anim.Update(0.f);
    for (auto* s : sprites) {
        if (s->x < minX) minX = s->x;
        if (maxX < s->x + s->w) maxX = s->x + s->w;
        float top = s->y - s->h;
        if (top < minY) minY = top;
        if (maxY < s->y) maxY = s->y;
    }
    if (d->buildingClass == 4) {
        if (Render::Texture* rt = Resources::GetImage("images/Rings/under_rings_tree_rock")) {
            Render::Sprite* r = Render::CreateSprite(rt, Render::kLayerRings, false, false);
            ring = r;
            float dx = 0.f, dy = 0.f;
            if (id == 0x11) { dx = -1.f; dy = -12.f; }
            else if (id == 0x14) { dx = -12.f; dy = -25.f; }
            Render::SetPosition(r, r->w * -0.5f + (minX + maxX) * 0.5f + dx, maxY + r->h * 0.5f + dy, 0.6f);
        }
    }
    Render::SortRenderLayer(Render::kLayerObjects, 1);
    linearX = x;
    linearY = y;
    TileCoordinatesToLinear(linearX, linearY);
}

bool Building::IsOpened() const { return data->buildingClass == 4 ? true : opened; }

void Building::SetUniqueID(uint32_t uid) {
    uniqueId = uid;
    if (g_latestUniqueId < uid) g_latestUniqueId = uid;
}

bool Building::BuilderIsWorking() const { return builder && builder->GetAI()->IsWorking(); }

bool Building::WorkerIsWorking(int i) const {
    Entity* e = workers[(size_t)i];
    return e && e->GetAI()->IsWorking();
}

bool Building::WorkerAssigned(int i) const {
    if (workers.empty()) return false;
    return workers[(size_t)i] != nullptr;
}

void Building::RemoveWorker(Entity* e) {
    for (auto& w : workers) {
        if (w == e) {
            e->GetAI()->RemoveFromJob();
            w->SetWorkplace(nullptr);
            w = nullptr;
        }
    }
    if (!builder || e != builder) return;
    e->GetAI()->RemoveFromJob();
    builder->SetWorkplace(nullptr);
    builder = nullptr;
}

void Building::AssignWorker(Entity* e, int slot) {
    if (!IsOpened()) {
        builder = e;
    } else {
        if ((int)workers.size() <= slot) return;
        workers[(size_t)slot] = e;
    }
    e->SetWorkplace(this);
    e->GetAI()->AssignToJob(this);
    if (Building* home = e->GetHome()) home->WorkerLeftToWork();
}

void Building::AssignLiver(Entity* e) {
    e->temporary = false;
    livers.push_back(e);
}

void Building::WorkerLeftToWork() {
    if (liverAway) {
        for (Entity* e : livers) {
            if (!e->IsActive()) {
                e->Appear(true, true);
                e->SetActive(true, true);
                return;
            }
        }
    }
    liverAway = false;
}

void Building::WorkStarted() {
    if (data->buildingClass != 4) {
        if (data->buildingClass != 0xd) stateTime = Timer::GetGlobalTime();
        return;
    }
    if (data->id == 0x11 || data->id == 0x95) {
        level = 5;
        anim.paused = false;
    } else {
        level = 4;
    }
    if (!workers[0]->GetOfflineMode()) lastGather = Timer::GetGlobalTime();
    else lastGather = (uint32_t)(int64_t)((double)Timer::GetGlobalTime() - gatherAcc);
    UpdateImage();
}

void Building::WorkEnded() {
    BuildingHovers::Update(0.0, true);
    if (data->buildingClass == 4) {
        if (data->id == 0x11 || data->id == 0x95) anim.paused = true;
    }
    // UNVERIFIED (farm): class 0xd on the current farm moves the patch worker on to its next step
    // (planting, harvest drops, cleaning); the farm is not ported yet.
}

void Building::GetSpawnTile(int& tx, int& ty) const {
    tx = x + ((y & 1) ? 1 : 0);
    ty = y + 1;
}

void Building::GetWorkTile(int& tx, int& ty) const {
    tx = x + ((y & 1) ? 1 : 0);
    ty = y + 1;
}

void Building::GetBuildTile(int& tx, int& ty) const {
    tx = x;
    ty = y;
    int n = mirrored ? data->h : data->w;
    for (int i = 0; i <= n / 2; ++i) {
        if ((ty & 1) == 0) tx -= 1;
        ty += 1;
    }
}

void Building::GetBuildingSpot(float& wx, float& wy) const {
    int tx, ty;
    GetBuildTile(tx, ty);
    wy = (float)ty * 42.f * 0.5f;
    wx = ((ty % 2 == 1) ? 42.f : 0.f) + (float)tx * 84.f + 42.f;
}

void Building::GetParkingSpot(unsigned i, float& wx, float& wy) const {
    if (i > data->parkingCount) return;
    float bx = ((y & 1) ? 42.f : 0.f) + (float)x * 84.f + 42.f;
    float by = (float)y * 42.f * 0.5f;
    // (the original indexes the point list with i itself, so spot 1 is the second point)
    const auto& p = i < 10 ? data->parking[i] : data->particles[i - 10];
    if (!IsMirrored()) wx = p.x + bx;
    else if ((unsigned)data->w < 4) wx = (bx - 84.f) - p.x;
    else wx = (bx - 168.f) - p.x;
    wy = p.y + by;
}

int Building::GetContractTime(int p) const {
    if (contract == 0) return 0;
    if (data->buildingClass == 0xc) return GameState::TutorialStep() != 0x62 ? 5 : 3;
    const auto& missions = data->delivery->missions;
    if (p == -1 || data->buildingClass != 0xd) return missions[(size_t)contract - 1].timePlant;
    int c = patchContract[p];
    if (c == 0) return 1;
    int total = 0;
    for (int t : missions[(size_t)c - 1].growTimes) total += t;
    return total;
}

float Building::GetContractProgress(int offset, int p) const {
    if (contractDone || contract == 0) return 1.f;
    int cls = data->buildingClass;
    if (cls == 0xc && !WorkerIsWorking(0)) return 0.f;
    uint32_t now = Timer::GetGlobalTime();
    if (p == -1 || cls != 0xd) {
        int done = (int)(offset - stateTime + now);
        if (GetContractTime(-1) < done) done = GetContractTime(-1);
        return (float)done / (float)(unsigned)GetContractTime(-1);
    }
    unsigned done = offset - patchStart[p] + now;
    unsigned total = (unsigned)GetContractTime(p);
    if (done > total) return 1.f;
    return (float)done / (float)total;
}

int Building::GetGatheredResCount() const { return resources[data->produceResource]; }

int Building::GetReadyTime() const {
    if (data->buildingClass != 0 && data->buildingClass != 9) return 0;
    uint32_t now = Timer::GetGlobalTime();
    if (now - stateTime < (uint32_t)data->collectTime) return 0;
    return (int)(now - stateTime - (uint32_t)data->collectTime);
}

int Building::GetMissionID() const { return contract - 1; }

bool Building::HasActiveContract() const {
    if (data->buildingClass != 0xd) return contract != 0;
    for (int c : patchContract)
        if (c != 0) return true;
    return false;
}

int Building::GetFullGoldAmount() const { return (int)livers.size() * 5 + data->collectMoney; }

int Building::GetReadyGoldAmount() const {
    if (data->buildingClass != 0 && data->buildingClass != 9) return 0;
    float r = (float)(Timer::GetGlobalTime() - stateTime) / (float)(uint32_t)data->collectTime;
    if (r < 0.f) r = 0.f;
    else if (r > 1.f) r = 1.f;
    return (int)((float)(uint32_t)GetFullGoldAmount() * r);
}

void Building::OnStorageFull(int type) { GameState::CancelWork(type); }

// Class 4: the gathered pile goes to the player's resources, as much as the storage takes. Class 0/9:
// the taxes restart.
void Building::CollectResources(bool& full, int& amount) {
    int cls = (int)data->buildingClass;
    if (cls == 4) {
        int type = data->produceResource;
        int pile = resources[type];
        if (pile == 0) return;
        int max = GameState::resourceAmountMax;
        if ((int)GameState::GetResourceAmount(type) < max) {
            if ((int)GameState::GetResourceAmount(type) + pile < max) {
                GameState::ChangeResourceAmount(type, pile);
                resources[type] -= pile;
                amount = pile;
                // SoundsManager::PlaySound("collect_resource", 1, false): sounds are not ported yet.
                GameState::RemoveAllOrders(this, pile);
                EntityManager::RemoveEntity(workers[1], true);
                workers[1] = nullptr;
                return;
            }
            int take = max - (int)GameState::GetResourceAmount(type) - GameState::GetOrderCount(type);
            GameState::ChangeResourceAmount(type, take);
            resources[type] -= take;
            amount = take;
            // SoundsManager::PlaySound("collect_resource", 1, false)
            GameState::RemoveAllOrders(this, take);
        }
        OnStorageFull(type);
        full = true;
        return;
    }
    if (cls != 0 && cls != 9) return;
    stateTime = Timer::GetGlobalTime();
}

void Building::LaunchContract(unsigned index, int patch) {
    int c = (int)index + 1;
    const auto& missions = data->delivery->missions;
    contract = c;
    lastContract = c;
    contractDone = false;
    uint32_t now = Timer::GetGlobalTime();
    f54 = (uint32_t)GetContractTime(patch) + now;
    if (data->buildingClass == 0xd) {
        patchContract[patch] = c;
        patchArg[patch] = Timer::GetGlobalTime() + (uint32_t)missions[index].growTotal;
        uint32_t t = Timer::GetGlobalTime();
        patchStart[patch] = t;
        if (GameState::TutorialStep() < 0x61)   // the tutorial's orders start half done
            patchStart[patch] = t - (uint32_t)(int)((float)(unsigned)GetContractTime(0) * 0.5f);
    }
    // UNVERIFIED (milestone 4, Tasks): Tasks::CompleteSubtask(0xc, index + delivery id * 10, 1).
    uint32_t t = Timer::GetGlobalTime();
    stateTime = t;
    if (GameState::TutorialStep() < 0x61)
        stateTime = t - (uint32_t)(int)((float)(unsigned)GetContractTime(-1) * 0.5f);
    // UNVERIFIED: AddSmoke @0x11e898 (the smoke entity 0x17c over workshops 0x6b 0x3ef 0x86 0x66 0x65
    // 0x6a 0x3f0) needs the BuildingData smoke positions, not parsed yet.
    AddDeliveryOrder();
}

const GameData::UpgradeInfo& Building::GetNextUpgradeInfo() const { return data->upgrades[(size_t)level]; }

int Building::GetContractSpeedUpCost() const {
    if (contract == 0) return 0;
    if (data->buildingClass == 0xc) return workers[0]->GetEntityData()->speedupCost;
    return (int)data->delivery->missions[(size_t)contract - 1].speedupCost2;
}

const char32_t* Building::GetContractName() const {
    if (data->buildingClass != 0xc) return data->delivery->missions[(size_t)contract - 1].title;
    return StringTable::GetString(data->name.c_str());
}

void Building::AddDeliveryOrder() {
    uint32_t id = data->id;
    if (id != 0x6b && id != 0x3ef && id != 0x86 && id != 0x66 && id != 0x65 && id != 0x3f0) return;
    if (Building* storage = Map::GetNearestStorage(this, false))
        GameState::PlaceOrder(storage, this, 0, data->delivery->missions[(size_t)contract - 1].priceResource, 1);
}

void Building::ResetResource() {
    if (data->buildingClass != 4) return;
    resourceState = 0;
    level = 0;
    growStart = Timer::GetGlobalTime();
    UpdateImage();
}

// Trees and rocks regrow through the respawn stages (BuildingData.respawn: amount, time).
void Building::UpdateGrowing() {
    if (data->buildingClass != 4) return;
    if (resourceState != 0) {
        if (data->produceResource == 0 && level == 5) anim.paused = true;   // a cut tree
        return;
    }
    const auto& rs = data->respawn;
    if (rs.empty()) return;
    uint32_t elapsed = Timer::GetGlobalTime() - growStart;
    unsigned stage = 0;
    unsigned t = (unsigned)rs[0].time;
    while (t < elapsed) {
        if (stage == rs.size() - 1) {
            level = (int)stage;
            resourceState = 1;
            UpdateImage();
        }
        ++stage;
        if (stage >= rs.size()) return;
        t += (unsigned)rs[stage].time;
    }
    resourceLeft = rs[stage].amount;
    if (level != (int)stage) {
        level = (int)stage;
        UpdateImage();
    }
    if (stage == rs.size() - 1) resourceState = 1;
}

int Building::GetFirstGrowingPatchNum() const {
    for (int i = 0; i < 6; ++i)
        if (patchContract[i] > 0) return i;
    return -1;
}

// Patch state: 0 empty, 1 rotten, 2 planted, 3.. growth stage + 3; a crop past its rot time turns
// 1 once task 0x429 is done, before that it gets 1200 s more and reports 6.
int Building::GetFarmState(int p) {
    if (p < 0 || patchContract[p] < 1) return 0;
    const auto& m = data->delivery->missions[(size_t)patchContract[p] - 1];
    uint32_t now = Timer::GetGlobalTime();
    if ((uint32_t)(patchStart[p] + m.timeRot) < now) {
        if (GameState::TaskCompleted(0x429)) return 1;
        patchStart[p] += 0x4b0;
        return 6;
    }
    now = Timer::GetGlobalTime();
    if (!m.growTimes.empty()) {
        unsigned t = (unsigned)m.growTimes[0];
        if (t < now - patchStart[p]) {
            size_t stage = 0, k = 0;
            for (;;) {
                stage = k++;
                if (k == m.growTimes.size()) break;
                t += (unsigned)m.growTimes[k];
                if (!(t < now - patchStart[p])) break;
            }
            return (int)stage + 3;
        }
    }
    return 2;
}

void Building::OnContractCompleted(bool silent) {
    // UNVERIFIED (milestone 5): SoundsManager "ui_contract_complete" for class 2 when not silent and
    // the map is not loading.
    // UNVERIFIED (3c): the trainee entity (+0x170) is removed, class 0xc calls TrainingCompleted.
    trainee = nullptr;
    BuildingHovers::Update(0.0, true);
    (void)silent;
}

namespace {
// A pile entity's stage animation for a fill level in percent (0 is no pile).
void SetPileStage(Entity* e, int level) {
    if ((unsigned)(level - 1) < 9) e->SetAnimationP("stage_1", true, false, false);
    else if ((unsigned)(level - 10) < 10) e->SetAnimationP("stage_2", true, false, false);
    else if ((unsigned)(level - 0x14) < 10) e->SetAnimationP("stage_3", true, false, false);
    else if ((unsigned)(level - 0x1e) < 10) e->SetAnimationP("stage_4", true, false, false);
    else if (level > 0x27) e->SetAnimationP("stage_5", true, false, false);
}
}  // namespace

// Storage (class 7): the player's amounts per resource, and a pile entity for each of lumber,
// rocks, food and planks whose stage is the fill level against "storage_space" at this level.
void Building::UpdateStorage() {
    if (data->buildingClass != 7) return;
    if (!IsOpened()) {
        for (auto& p : piles) {
            if (p) EntityManager::RemoveEntity(p, true);
            p = nullptr;
        }
        return;
    }
    float space = Setting("storage_space").GetChild((unsigned)level).GetFloat();
    static const char* const kPiles[4] = {"resource_lumber", "resource_rocks", "resource_food", "resource_planks"};
    for (int t = 0; t < 8; ++t) {
        int amount = resources[t] = (int)GameState::GetResourceAmount(t);
        if (t >= 4) continue;
        Entity* pile = piles[t];
        int stage = (int)(((float)amount / space) * 100.f);
        if (!pile) {
            if (stage == 0) continue;
            pile = EntityManager::SpawnEntityAt(kPiles[t], x, y, false, false);
            if (mainSprite) pile->SetCustomZ(mainSprite->z - 0.001f);
            SetPileStage(pile, stage);
            float px = 0.f, py = 0.f;
            GetParkingSpot((unsigned)t, px, py);
            pile->SetWorldPos(px, py);
            piles[t] = pile;
        } else {
            if (mainSprite) pile->SetCustomZ(mainSprite->z - 0.001f);
            if (stage == 0) {
                EntityManager::RemoveEntity(pile, true);
                piles[t] = nullptr;
            } else {
                SetPileStage(pile, stage);
            }
        }
    }
}

// A tree or rock: the gathered pile (an entity in worker slot 1, at parking spot 0) shows what
// waits for a goblin.
void Building::UpdateResources() {
    if (data->buildingClass != 4) return;
    int amount = resources[data->produceResource];
    if (Entity* pile = workers[1]) {
        if (amount == 0) {
            EntityManager::RemoveEntity(pile, true);
            workers[1] = nullptr;
        } else {
            SetPileStage(pile, amount);
        }
        return;
    }
    if (amount == 0) return;
    Entity* pile = nullptr;
    if (data->produceResource == 0) pile = EntityManager::SpawnEntityAt("resource_lumber", x, y, false, false);
    else if (data->produceResource == 1) pile = EntityManager::SpawnEntityAt("resource_rocks", x, y, false, false);
    float px = 0.f, py = 0.f;
    GetParkingSpot(0, px, py);
    SetPileStage(pile, amount);
    pile->SetWorldPos(px, py);
    workers[1] = pile;
    pile->SetHome(this);
    if (GameState::TutorialStep() == 0x29) pile->Appear(true, true);   // UNVERIFIED: the third argument is not set
}

namespace {
// OnBuilded/OnUpgraded: a new worker (id 0 or 0x30) at the spawn tile when it is free, else at a
// free waypoint around the building (w x h tries), else at the start tile.
Entity* SpawnLiver(Building* b, int id) {
    int sx = 0, sy = 0;
    b->GetSpawnTile(sx, sy);
    AI::Waypoint* wp = AI::GetWaypoint(sx, sy, false);
    Decor* d = GetDecoration(sx, sy);
    if (d && d->fake) d = nullptr;
    if (wp && !EntityManager::GetEntityAtXY(sx, sy) && !GetBuilding(sx, sy) && !d)
        return EntityManager::SpawnEntityAt(id, (unsigned)sx, (unsigned)sy, true, true);
    int tries = b->data->w * b->data->h;
    AI::Waypoint* free = nullptr;
    int left = 0;
    for (; tries != 0; --tries) {
        AI::Waypoint* c = AI::GetWaypointNearBuilding(b, false);
        if (!c) continue;   // (the original reads through the null waypoint here)
        if (!EntityManager::GetEntityAtXY(c->x, c->y) && !GetBuilding(c->x, c->y) && !GetDecoration(c->x, c->y)) {
            free = c;
            left = tries - 1;
            break;
        }
    }
    if (left == 0) {
        int tx = 0, ty = 0;
        b->GetStartTile(tx, ty);
        return EntityManager::SpawnEntityAt(id, (unsigned)tx, (unsigned)ty, true, true);
    }
    return EntityManager::SpawnEntityAt(id, (unsigned)free->x, (unsigned)free->y, true, true);
}
}  // namespace

void Building::OnBuilded() {
    // UNVERIFIED (milestone 4): Tasks::CompleteSubtask(1, id, 1).
    int cls = data->buildingClass;
    if (cls == 0) {
        for (unsigned i = 0; i < data->givePopulation; ++i) {
            int id = Rand48::lrand48() % 2 == 1 ? 0x30 : 0;
            if ((unsigned)(GameState::TutorialStep() - 0x20) < 2) id = 0x30;
            Entity* e = SpawnLiver(this, id);
            AssignLiver(e);
            e->SetHome(this);
            e->SetHP(0x400);
        }
        stateTime = Timer::GetGlobalTime();
        if (GameState::TutorialStep() == 0x20) stateTime = Timer::GetGlobalTime() - (uint32_t)data->collectTime;
    } else if (cls == 0xd) {
        int id = Rand48::lrand48() % 2 == 1 ? 7 : 6;
        if (GameState::TutorialStep() < 0x3b) id = 7;
        Entity* e = EntityManager::SpawnEntityAt(id, x, y, false, false);
        AssignLiver(e);
        e->SetHome(this);
        AssignWorker(e, 0);
        if (GameState::TutorialStep() < 0x3b) {
            e->Disappear();
            e->SetAlpha(0.f);
            e->Update(0.f);
        }
        // (+0xd4 is the head of the sprite chain, the newest sprite: sprites.front() here)
        if (!sprites.empty()) e->SetCustomZ(sprites.front()->z - 0.0005f);
    } else if (cls == 7) {
        UpdateStorageMax();
    }
    // UNVERIFIED (milestone 5): SoundsManager "building_build_end".
}

void Building::OnUpgraded() {
    int cls = data->buildingClass;
    if (cls == 0) {
        unsigned n = (unsigned)data->upgrades[(size_t)level - 1].givePopulation;
        for (unsigned i = 0; i < n; ++i) {
            int id = Rand48::lrand48() % 2 == 1 ? 0x30 : 0;
            Entity* e = SpawnLiver(this, id);
            if (!liverAway) liverAway = true;
            else e->SetActive(false, false);
            AssignLiver(e);
            e->SetHome(this);
            e->SetHP(0x400);
        }
    } else if (cls == 7) {
        UpdateStorageMax();
    } else if (cls == 0xd) {
        if (!livers.empty() && livers[0]) livers[0]->Appear(false, true);
        if (farmEntity && farmEntity->GetSprite()) Render::SetVisibility(farmEntity->GetSprite(), true);
    }
    // UNVERIFIED (milestone 4): Tasks::CompleteSubtask(3, id, 1).
    // UNVERIFIED (milestone 5): SoundsManager "building_upgrade_end".
}

void Building::HireGolbin() {
    if (data->buildingClass != 7) return;
    int tx = 0, ty = 0;
    GetSpawnTile(tx, ty);
    Entity* g = EntityManager::SpawnEntityAt(0x133, (unsigned)tx, (unsigned)ty, true, true);
    g->SetHP(0x400);
    AssignLiver(g);
    g->SetHome(this);
}

void Building::GetDeliveryTile(int& tx, int& ty) const { GetBuildTile(tx, ty); }   // (the same steps)

// UNVERIFIED (3c): the crop entity of a farm's first growing patch (ids by contract type and crop).
void Building::SetupSmallFarm() {}

// A workshop with a contract still running shows smoke. UNVERIFIED (milestone 5): AddSmoke.
void Building::UpdateOfflineStateNoWorker() {
    if (data->buildingClass != 2 || contract == 0) return;
    if (GameState::TutorialStep() == 0x54 && id == 0x6b) {
        if ((float)(Timer::GetGlobalTime() - stateTime) > 90.f) stateTime = Timer::GetGlobalTime() - 0x5a;
    }
    if ((uint32_t)GetContractTime(-1) + stateTime <= Timer::GetGlobalTime()) return;
    // AddSmoke(this);
}

// Catch-up for the time the game was closed: a tree's or rock's gathering, a finished
// construction or upgrade.
void Building::UpdateOfflineState() {
    double now = (double)(uint32_t)Timer::GetGlobalTime();
    if (data->buildingClass == 4) {
        int n = (int)(int64_t)((now - (double)(int)lastGather) / (double)(uint32_t)data->collectTime);
        if (resourceLeft < n) n = resourceLeft;
        if (n < 0) n = 0;
        int stack = (int)GameState::GetSetting("stack_size");
        if (stack < n) n = (int)GameState::GetSetting("stack_size");
        int& pile = resources[data->produceResource];
        if (pile < (int)GameState::GetSetting("stack_size")) {
            if ((int)GameState::GetSetting("stack_size") < n + pile) n = (int)GameState::GetSetting("stack_size") - pile;
        } else {
            n = 0;
        }
        pile += n;
        resourceLeft -= n;
        if (GetGatheredResCount() < (int)GameState::GetSetting("stack_size"))
            gatherAcc = (double)((uint32_t)(int)(now - (double)(int)lastGather) % (uint32_t)data->collectTime);
        else
            gatherAcc = 0.0;
        if (resourceLeft == 0) {
            gatherAcc = 0.0;
            level = data->produceResource == 0 ? 6 : 0;
            resourceState = 2;
            UpdateImage();
        }
    }
    if (needsBuilder) {
        double t = now - (double)stateTime;
        if (buildLeft <= t) {
            built = 1;
            buildLeft = 0.0;
            needsBuilder = 0;
            SetOpened();
            UpdateImage();
            OnBuilded();
        } else {
            buildLeft -= t;
        }
    }
    if (upgrading) {
        double t = now - (double)stateTime;
        if (buildLeft <= t) {
            buildLeft = 0.0;
            ++level;
            upgrading = 0;
            SetOpened();
            UpdateImage();
            OnUpgraded();
            return;
        }
        buildLeft -= t;
    }
}

// A tree's or rock's gathered pile goes to the storages as delivery orders.
void Building::UpdateOfflineResources() {
    if (data->buildingClass != 4) return;
    for (int i = 0; i < resources[data->produceResource]; ++i)
        GameState::PlaceOrder(this, Map::GetNearestStorage(this, false), 1, data->produceResource, 0);
    UpdateResources();
}

void Building::Update(double dt) {
    anim.Update((float)dt);
    bool opened = IsOpened();
    bool flash = flashing;
    if (!opened || !flash) {
        for (auto* s : sprites) s->animTime = -1.f;
        flash = !opened && flashing;
    }
    if (flash && flashTime > 0.f) {
        flashTime -= (float)dt;
        if (!(flashTime > 0.f)) flashing = false;
    }
    int cls = data->buildingClass;
    if (cls == 0xc && contract != 0 && !contractDone && !WorkerIsWorking(0)) stateTime = Timer::GetGlobalTime();
    if ((cls == 2 || cls == 0xc) && contract != 0 && !contractDone) {
        if (GameState::TutorialStep() == 0x54 && id == 0x6b) {
            if ((float)(Timer::GetGlobalTime() - stateTime) > 180.f) stateTime = Timer::GetGlobalTime() - 0x1e;
        }
        if ((uint32_t)GetContractTime(-1) + stateTime < Timer::GetGlobalTime()) {
            contractDone = true;
            OnContractCompleted(false);
        }
    }
    if (!IsOpened() && BuilderAssigned() && BuilderIsWorking()) {
        if (buildLeft <= 0.0) {
            buildLeft = 0.0;
            stateTime = Timer::GetGlobalTime();
            SetOpened();
            RemoveWorker(builder);
            if (built == 0) {
                needsBuilder = 0;
                built = 1;
                OnBuilded();
            } else {
                upgrading = 0;
                ++level;
                OnUpgraded();
            }
            UpdateStorage();
            UpdateImage();
            Render::SortRenderLayer(Render::kLayerObjects, 1);
        } else {
            buildLeft -= dt;
            if (GameState::TutorialStep() == 0x5f && buildLeft < 20.0) buildLeft = 20.0;
        }
    } else if (cls == 4) {
        // Gathering: every collectTime seconds of work one unit moves from the tree or rock to its
        // pile (at most stack_size), and the nearest storage gets a delivery order.
        bool gathered = false;
        if (WorkerAssigned(0) && WorkerIsWorking(0) &&
            GetGatheredResCount() < (int)GameState::GetSetting("stack_size")) {
            gathered = true;
            unsigned period = (unsigned)data->collectTime;
            if (!(gatherAcc < (double)period)) {
                unsigned whole = gatherAcc > 0.0 ? (unsigned)(int64_t)gatherAcc : 0u;
                unsigned n = whole / period, rem = whole % period;
                if ((unsigned)resourceLeft <= n) n = (unsigned)resourceLeft;
                float st = GameState::GetSetting("stack_size");
                if ((st > 0.f ? (unsigned)(int)st : 0u) < n) n = (unsigned)GameState::GetSetting("stack_size");
                int pr = data->produceResource;
                if (resources[pr] < (int)GameState::GetSetting("stack_size")) {
                    float st2 = GameState::GetSetting("stack_size");
                    if ((st2 > 0.f ? (unsigned)(int)st2 : 0u) < n + (unsigned)resources[pr]) n = (unsigned)((int)st2 - resources[pr]);
                } else {
                    n = 0;
                }
                resourceLeft -= (int)n;
                resources[pr] += (int)n;
                gatherAcc = (double)rem;
                lastGather = Timer::GetGlobalTime();
                UpdateResources();
                if (Building* storage = GetNearestStorage(this, false))
                    GameState::PlaceOrder(this, storage, data->produceAmount, pr, 0);
                if (resourceLeft == 0) {
                    level = pr == 0 ? 6 : 0;
                    resourceState = 2;
                    UpdateImage();
                    RemoveWorker(workers[0]);
                }
            } else {
                gatherAcc = dt + gatherAcc;
                unsigned t = (unsigned)GameState::TutorialStep();
                if (t - 0x28u < 2u && (double)period - 1.0 <= gatherAcc) gatherAcc = (double)period - 1.0;
            }
        }
        if (!gathered) UpdateGrowing();
    } else if (cls == 0xd) {
        // UNVERIFIED (3c): the crop entity's frame follows GetFarmState; tutorial worker fade-out.
    }
    if (firstUpdate) {
        UpdateStorage();
        SetupSmallFarm();
        UpdateOfflineStateNoWorker();
        firstUpdate = false;
    }
    // UNVERIFIED (milestone 4): fog visibility on campaign maps, tower attacks (0x84/0x8c) in combat
    // and the building's combat event list.
}

}  // namespace Map
