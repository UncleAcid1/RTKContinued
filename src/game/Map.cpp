#include "game/Particles.h"
#include "game/ParticlePresets.h"
#include "engine/SoundsManager.h"
#include "game/MetaData.h"
#include "game/MetaExpression.h"
#include "game/Professions.h"
#include "game/Map.h"
#include "game/Notifications.h"

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "engine/FileManager.h"
#include "engine/Render.h"
#include "engine/Resources.h"
#include "engine/Splitter.h"
#include "engine/SystemFuncs.h"
#include "engine/Timer.h"
#include "game/AI.h"
#include "game/AIState.h"
#include "game/EntityData.h"
#include "game/Background.h"
#include "game/BuildingHovers.h"
#include "game/BuildingPlacement.h"
#include "game/BuildingMovement.h"
#include "game/Combat.h"
#include "game/PvP.h"
#include "game/Entity.h"
#include "game/EntityManager.h"
#include "game/Fog.h"
#include "game/GameData.h"
#include "game/Setting.h"
#include "game/GameState.h"
#include "game/Locations.h"
#include "game/Chests.h"
#include "game/Rand48.h"
#include "game/SaveManager.h"
#include "game/Game.h"
#include "game/SoldierSlots.h"
#include "game/SpawnPoint.h"
#include "game/Items.h"
#include "game/Tasks.h"
#include "game/Squad.h"
#include "gui/GUI.h"
#include "hud/HUD.h"
#include "windows/Windows.h"

namespace Map {
namespace {

struct Cell {               // grid cell, 0x10 bytes on the original
    Building* building = nullptr;  // +0x00 (SetBuilding)
    Decor* decor = nullptr;        // +0x04 (SetDecoration)
    Decor* virtualDecor = nullptr; // +0x08 (SetVirtualDecoration: a road tile being placed)
    bool block = false;            // SetBlock (walk mask)
};

uint32_t g_mapId = 0;
int g_gridW = 0, g_gridH = 0, g_tileset = 0;
std::vector<Cell> g_grid;
std::vector<std::unique_ptr<Patch>> g_patches;
struct QueuedPlacement { int x, y; uint32_t id; };       // 0xc bytes
std::vector<QueuedPlacement> g_placements;       // 0x61376c (count 0x613780) EnqueueBuildingPlacement
std::vector<Render::Sprite*> g_borderSprites;  // dark grass, posts, signs
uint32_t g_currentTime = 0;                     // 0x6118c4
bool g_loaded = false;                          // 0x613798 (set at the end of Map::Load)
int g_lastWorldX = 0, g_lastWorldY = 0;         // Map::lastWorldX/Y 0x6138dc 0x6138e0
int g_owned[4] = {};                            // 0x613660 owned extent: min x, min y, max x, max y
Building* g_currentFarm = nullptr;              // 0x6136b8 the farm shown (Map::ShowFarm)
// The farm view: a grid of 3 x 4 tiles (196 x 98 px each) at world (150, 2880), with its own walk
// blocks (0x613794, the 0x10-byte cells' +0xc flag) and its patch (0x6136b4).
const int kFarmGridW = 3, kFarmGridH = 4;       // 0x60effc 0x60f000
const float kFarmWorldX = 150.f, kFarmWorldY = 2880.f;   // 0x60f004 0x60f008
std::vector<uint8_t> g_farmBlocks;              // 0x613794 (empty: the farm is not shown)
std::unique_ptr<Patch> g_farmPatch;             // 0x6136b4
std::vector<std::pair<uint8_t, uint8_t>> g_blockedTiles;   // 0x61379c tiles blocked after load
std::vector<std::pair<uint8_t, uint8_t>> g_openedTiles;    // 0x613784 blocked tiles given a waypoint for a search
int g_startX = -1, g_startY = -1;               // 0x60eff0 0x60eff4 the player's saved position
bool g_victoryPose = false;   // 0x6138cc set by UpdateSpawns after a won boss fight (milestone 5)
bool g_loadQueued = false;                      // 0x613658 (Map::EnqueueLoadMap)
bool g_loadQueuedSave = false;                  // 0x613659
bool g_bossReward = false;                      // 0x61365a a boss reward waits for the next map
float g_loadQueuedDelay = 0.f;                  // 0x61365c
int g_loadQueuedMap = -1;                       // 0x60eff8
bool g_loading = false;                         // 0x61368c (Map::Load is running)
// 0x60f014: Map::Load shows LoadingScreenWindow (cleared when the load does not start from the open
// world map; set again at its end).
bool g_loadScreen = true;
// One frame of the loading screen between Map::Load's stages (Render::Update(0, 1.5)).
void LoadScreenFrame() {
    if (g_loadScreen) Render::Update();
}
bool g_newGameLoad = false;                     // 0x6137f8 Map::Load's third argument
bool g_storedPosition = false;                  // 0x6136c0
int g_storedX = -1, g_storedY = -1;             // 0x60f00c 0x60f010
int g_nextSpawnX = 0, g_nextSpawnY = 0;         // 0x6136d8 0x6136dc
struct FogMarker { int x, y, size; bool f0c; };  // 0x10 bytes
std::vector<FogMarker> g_fogMarkers;            // 0x613750 (count 0x613768) the map's covered spots
bool g_versionUpdate = false;                   // 0x6138cd (Map::Load's version check is running)
// 0x613800: UpdateCityMap's owned patches (area id, the patch saved alone) that LoadPlayer puts
// into the map file's new version.
std::vector<std::pair<uint32_t, std::unique_ptr<SaveManager::SaveBlock>>> g_cityUpdateBlocks;
bool g_allowBuildingVisibility = false;         // 0x60ef20
bool g_allowDecorationVisibility = false;       // 0x60ef21
float g_visibilityTimer = 0.f, g_fogTimer = 0.f;   // 0x6138d0 0x6138d4
float g_resortTimer = 0.f;                      // 0x6138d8 layer 8 is re-sorted every 0.757 s
float g_mapTimer = 0.f;                         // 0x6138c8 360 s on mission maps, then a ding
// The ambient sky (PrepareImages, Map::Load, Map::Update).
bool g_imagesPrepared = false;                  // 0x6137ac
Render::Texture* g_birdTex[12] = {};            // 0x6137b0 bird_000..003, bird_c_000/002/003/004, bird_d_000..003
Render::Texture* g_cloudTex[6] = {};            // 0x6137e0 cloud_no_shad_001..003, cloud_small_001..003
Render::Sprite* g_clouds[12] = {};              // 0x61388c
struct Bird {                                   // 0x10 bytes (FUN_001c273c resizes the vector)
    float tx = 0.f, ty = 0.f;                   // +0x00 +0x04 where it flies to
    Render::Sprite* sprite = nullptr;           // +0x08
    int dir = 0;                                // +0x0c the heading sector shown (0..7)
};
std::vector<Bird> g_birds;                      // 0x613828

Cell* At(int x, int y) {
    if ((unsigned)x >= (unsigned)g_gridW || (unsigned)y >= (unsigned)g_gridH) return nullptr;
    return &g_grid[(size_t)y * g_gridW + x];
}

// The footprint walk from a start tile (MapObject::GetStartTile @0x1c7658), shared by
// Building::LinkBaseToBuilding @0x11e4a4, Building::CanBePlaced @0x11e5f0 and Decor::CanBePlaced
// @0x134554: h rows of w tiles, each row stepping up-right, the rows stepping up-left.
template <class Fn>
void ForEachFootprintTileFrom(int sx, int sy, int w, int h, Fn&& fn) {
    unsigned x = (unsigned)sx, y = (unsigned)sy;
    for (int r = 0; r < h; ++r) {
        unsigned cx = x, cy = y;
        for (int c = 0; c < w; ++c) {
            fn((int)cx, (int)cy);
            unsigned odd = cy & 1;
            cy -= 1;
            if (odd) cx += 1;
        }
        if ((y & 1) == 0) x -= 1;
        y -= 1;
    }
}

// The same walk with the start tile computed inline from w (no mirroring), as Decor::UpdateMapLink
// @0x1346b8 and Map::RemoveDecoration @0x1bb148 do.
template <class Fn>
void ForEachFootprintTile(int bx, int by, int w, int h, Fn&& fn) {
    int x = bx, y = by;
    for (int i = 0; i < w / 2; ++i) {
        if (((unsigned)(by + i) & 1) == 0) x -= 1;
    }
    y += w / 2;
    ForEachFootprintTileFrom(x, y, w, h, fn);
}

int DecorLayer(int layer) {  // Decor::UpdateImage: table at 0x57b79c for layer+1 in [0,4], else 8
    static const int table[5] = {11, 8, 3, 3, 2};
    unsigned i = (unsigned)(layer + 1);
    return i < 5 ? table[i] : 8;
}

void RemoveDecorationAt(int x, int y) {  // @0x1bb148 RemoveDecoration(x, y, false)
    Cell* c = At(x, y);
    if (!c || !c->decor) return;
    Decor* d = c->decor;
    if (d->data) {
        ForEachFootprintTile(d->x, d->y, d->data->w, d->data->h, [&](int tx, int ty) {
            if (Cell* t = At(tx, ty)) t->decor = nullptr;
        });
    }
    if (d->sprite) { Render::RemoveSprite(d->sprite); d->sprite = nullptr; }
    d->removed = true;
}

// ------------------------------------------------------------------------------- chunk decode
void LoadDecors(Patch* p, const SaveManager::Chunk& c) {  // @0x1e271c, one chunk 0xb
    SaveManager::Reader r(c);
    bool city = GameState::GetCurrentMapID() == 0;
    auto d = std::make_unique<Decor>();
    d->patch = p;
    d->x = r.u8();
    d->y = r.u8();
    d->id = r.u32();
    d->data = GameData::GetDecoration(d->id);
    // A type without hit points gets 50, and so does this decoration: the first of its type loaded.
    if (d->data && d->data->hp == 0) {
        const_cast<GameData::DecorData*>(d->data)->hp = 0x32;
        d->hpMax = d->hp = 0x32;
    }
    uint8_t flags = r.u8();
    d->hidden = (flags & 2) != 0;
    d->mirrored = (flags & 1) != 0;
    if ((city && GameState::tutorial == 0x17) || (city && !p->owned)) d->hidden = true;
    d->collectStart = r.u32();
    d->f4c = r.u8();
    d->f50 = r.u32();
    d->f54 = r.u32();
    d->resourceText = r.str16();
    if (!d->resourceText.empty())
        for (const std::string& part : SplitterParse(d->resourceText.c_str(), "|")) {
            std::vector<std::string> kv = SplitterParse(part.c_str(), "=");
            // PORT: a part without '=' or of an unknown type is skipped (the original reads past it).
            int type = GameState::StringToResourceType(kv[0].c_str());
            if (kv.size() > 1 && type >= 0 && type < 11) d->cost[type] = atoi(kv[1].c_str());
        }
    d->SetMeta(r.str16());
    if (!d->metaText.empty() && city) d->hidden = false;
    d->UpdateImage();
    d->UpdateMapLink();
    if (d->meta || d->f4c != 0 || (d->data && d->data->collectTime != 0) || d->hp > 0)
        BuildingHovers::RegisterDecoration(d.get());
    p->decors.push_back(std::move(d));
}

// @0x1e2bdc Map::LoadBuidings, one building: chunk 0xc, then the optional 0x2d (farm patch start
// times), 0x2f (farm patch contracts), 0x31 (farm patch data) and 0x50 (unique id) chunks.
void LoadBuilding(Patch* p, SaveManager::SaveBlock* block) {
    SaveManager::Reader r(block->GetChunk());
    auto bp = std::make_unique<Building>();
    Building* b = bp.get();
    b->x = r.u8();
    b->y = r.u8();
    b->id = r.u32();
    b->uniqueId = 0;
    if (b->id == 100) b->id = 99;
    b->stateTime = r.u32();
    b->buildLeft = (double)r.u32();
    b->f54 = r.u32();
    uint8_t six[6];
    for (auto& v : six) v = r.u8();
    b->mirrored = six[0] != 0;
    b->level = six[2];
    r.u32(); r.u32();
    b->built = r.u8() != 0;
    b->upgrading = r.u8();
    b->ff8 = r.u32();
    b->lastGather = r.u32();
    b->resourceLeft = (int)r.u32();
    int n = r.u8();
    for (int i = 0; i < n; ++i) b->resources[i] = r.s16();   // (n <= 11 in every save)
    b->resourceState = r.u8();
    b->growStart = r.u32();
    b->f10c = r.u8();
    b->f110 = r.u32();
    b->f114 = r.u32();
    b->resourceText = r.str16();
    b->metaText = r.str16();   // UNVERIFIED: MetaExpression (quest conditions) not ported; kept as text
    b->contract = (int)r.u32();

    const GameData::BuildingData* d = GameData::GetBuilding(b->id);
    uint32_t now = Timer::GetGlobalTime();
    if (d) {
        if (d->buildingClass == 4 && d->produceResource == 1 && b->level == 6) b->level = 0;
        b->hp = b->maxHp = d->hp;
        if (!b->built && now < b->stateTime) {
            b->stateTime = Timer::GetGlobalTime();
            if (b->buildLeft > 0.0) b->buildLeft = (double)d->constructionTime;
        }
        if (b->upgrading && now < b->stateTime) {
            b->stateTime = Timer::GetGlobalTime();
            if (b->buildLeft > 0.0) b->buildLeft = (double)(unsigned)d->upgrades[(size_t)b->level].time;
        }
    }
    bool city = GameState::GetCurrentMapID() == 0;
    if (!city && !b->metaText.empty()) d = nullptr;   // quest buildings on campaign maps: decorations
    if (!d) {
        const GameData::DecorData* dd = city ? GameData::GetDecoration(b->id) : nullptr;
        if (!city && !b->metaText.empty()) dd = GameData::GetDecoration(b->id);
        if (!dd) {
            std::printf("Map::LoadBuidings() Cannot find building ID: %d\n", b->id);
            return;
        }
        std::printf("Map::LoadBuidings() Replacing building (ID: %d) with a decoration\n", b->id);
        // UNVERIFIED: the decoration also takes the building's timer and resources.
        auto dec = std::make_unique<Decor>();
        dec->SetMeta(b->metaText);   // (the building's expression moves to it)
        dec->patch = p;
        dec->x = b->x;
        dec->y = b->y;
        dec->id = b->id;
        dec->data = dd;
        dec->mirrored = b->mirrored;
        dec->UpdateImage();
        dec->UpdateMapLink();
        p->decors.push_back(std::move(dec));
        return;
    }
    b->data = d;
    if (Cell* c = At(b->x, b->y)) c->building = b;   // SetBuilding
    b->patch = p;
    b->index = (int)p->buildings.size();
    p->buildings.push_back(std::move(bp));
    b->FindBaseCoordinates();
    b->LinkBaseToBuilding();
    if (b->built && !b->upgrading) b->SetOpened();
    if (b->buildLeft > 0.0) {
        if (!b->built) b->needsBuilder = 1;
        b->SetClosed(true);
    }
    b->UpdateImage();
    BuildingHovers::RegisterBuilding(b);
    b->workers.assign(d->parkingCount, nullptr);
    b->builder = nullptr;
    if (block->NextChunkID() == 0x2d) {
        SaveManager::Reader cr(block->GetChunk());
        for (auto& v : b->patchStart) v = cr.u32();
    }
    if (block->NextChunkID() == 0x2f) {
        SaveManager::Reader cr(block->GetChunk());
        for (auto& v : b->patchContract) v = (int)cr.u32();
    }
    if (block->NextChunkID() == 0x31) {
        SaveManager::Reader cr(block->GetChunk());
        for (auto& v : b->patchArg) v = cr.u32();
    }
    if (block->NextChunkID() == 0x50) {
        SaveManager::Reader cr(block->GetChunk());
        b->SetUniqueID(cr.u32());
    }
}

// ------------------------------------------------------------------------- random decorations
// @0x1e3a9c Map::Patch::AddRandomDecors (re-seeds with playerSeed for every patch)
void AddRandomDecors(Patch* p) {
    Rand48::srand48((long)GameState::playerSeed);
    if (!(g_mapId == 0 && g_tileset == 0)) return;
    static const uint32_t ids[] = {0x12, 0x16, 0x20, 0x22, 0x23, 0x24, 0x25, 0x27, 0x28, 0x2a, 0x2e, 0x2f, 0x30, 0x31, 0x32};
    int count = (p->w * p->h) / 0x23;
    for (int i = 0; i < count; ++i) {
        int x = (int)(Rand48::lrand48() % (p->w - 1)) + p->x;
        int y = (int)(Rand48::lrand48() % (p->h - 1)) + p->y;
        Cell* c = At(x, y);
        if (!c || c->decor || c->building || c->block) continue;
        auto d = std::make_unique<Decor>();
        d->patch = p;
        d->x = (uint8_t)x;
        d->y = (uint8_t)y;
        d->fake = true;
        d->id = ids[(unsigned long)Rand48::lrand48() % (sizeof ids / sizeof ids[0])];
        d->data = GameData::GetDecoration(d->id);
        d->UpdateImage();
        d->UpdateMapLink();
        p->decors.push_back(std::move(d));
    }
}

// ------------------------------------------------------------------------------ area borders
// @0x1bcda8 Map::UpdateAreaBorders (dark grass patches, flag posts, for-sale signs)
// @0x1b68f4
}  // namespace
void UpdateOwnedAreaBorders() {
    int minX = 10000, minY = 10000, maxX = -10000, maxY = -10000;
    g_owned[0] = g_owned[1] = 10000;
    g_owned[2] = g_owned[3] = -10000;
    if (!g_patches.empty()) {
        for (auto& p : g_patches) {
            if (!p->owned) continue;
            if (p->x < minX) minX = p->x;
            if (p->y <= minY) minY = p->y;
            if (maxX < p->x + p->w) maxX = p->x + p->w;
            if (maxY < p->y + p->h) maxY = p->y + p->h;
        }
        g_owned[0] = minX;
        g_owned[1] = minY;
        g_owned[2] = maxX;
        g_owned[3] = maxY;
        if (minX != 10000 && minY != 10000 && maxX != -10000 && maxY != -10000) return;
    }
    g_owned[0] = 0;
    g_owned[1] = 0;
    g_owned[2] = GetGridWidth() - 1;
    g_owned[3] = GetGridHeight();
}
namespace {

void UpdateAreaBorders() {
    for (auto* s : g_borderSprites) Render::RemoveSprite(s);
    g_borderSprites.clear();
    for (auto& p : g_patches) p->sign = nullptr;
    // bordered = owned, or edge-adjacent to an owned patch
    for (auto& a : g_patches) {
        a->bordered = a->owned;
        for (auto& b : g_patches) {
            if (a.get() == b.get() || !b->owned) continue;
            if ((a->x == b->x + b->w || b->x == a->x + a->w) && a->y == b->y) a->bordered = true;
            if (a->y == b->y + b->h && a->x == b->x) a->bordered = true;
        }
    }
    Render::Texture* sign = Resources::GetImage("images/Buildings/land_expand");
    for (auto& p : g_patches) {  // signs, layer 8
        if (!sign || p->owned || p->x + p->w > g_gridW || !p->bordered || !p->buyable) continue;
        Render::Sprite* s = Render::CreateSprite(sign, Render::kLayerObjects, false, false);
        float X = (float)(p->x + p->w / 2) * 84.f;
        float Y = (float)(p->y + p->h / 2) * 42.f * 0.5f;
        Render::SetPosition(s, X, Y, GetSpriteZ(Y, s->w * 0.5f, 0));
        g_borderSprites.push_back(s);
        p->sign = s;
    }
    // dark grass patches, layer 1
    Render::Texture* grass[6] = {};
    if (g_tileset == 0) {
        for (int i = 0; i < 6; ++i) {
            char n[64];
            std::snprintf(n, sizeof n, "images/Tileset/summer/grass_dark_big_%03d", i + 1);
            grass[i] = Resources::GetImage(n);
        }
    }
    unsigned count = (unsigned)(g_gridW * g_gridH) / 0x3c;
    Rand48::srand48((long)GameState::playerSeed);
    for (unsigned i = 0; i < count; ++i) {
        unsigned long r1 = (unsigned long)Rand48::lrand48();
        unsigned long r2 = (unsigned long)Rand48::lrand48();
        int y = (int)(r2 % (unsigned long)(g_gridH - 6)) + 3;
        long r3 = Rand48::lrand48();
        Render::Texture* t = grass[r3 % 6];
        if (!t) continue;
        Render::Sprite* s = Render::CreateSprite(t, Render::kLayerGroundDecal, false, false);
        int x = (int)(r1 % (unsigned long)(g_gridW - 3));
        float X = (float)x * 84.f + ((y & 1) ? 42.f : 0.f) + 42.f;
        Render::SetPosition(s, X, (float)y * 42.f * 0.5f, 0.5f);
        g_borderSprites.push_back(s);
    }
    // flag posts, layer 3
    Render::Texture* flag = Resources::GetDirectImage("images/land_expand_flag.png");
    for (auto& p : g_patches) {
        if (!flag || p->owned || !p->bordered) continue;
        for (int y = p->y; y <= p->y + p->h; ++y) {
            for (int x = p->x; x <= p->x + p->w; ++x) {
                bool place = (x == p->x && (y & 1) == 0) || (x == p->x + p->w && (y & 1) == 0) ||
                             (y == p->y && y != 0) || (y == p->y + p->h);
                if (!place) continue;
                Render::Sprite* s = Render::CreateSprite(flag, Render::kLayerFlat3, false, false);
                float X = (float)x * 84.f + ((y & 1) ? 42.f : 0.f) + 42.f - (float)(int)(s->w * 0.5f);
                Render::SetPosition(s, X, (float)y * 42.f * 0.5f - 21.f, 0.2f);
                g_borderSprites.push_back(s);
            }
        }
    }
    UpdateOwnedAreaBorders();
    int minX, minY, maxX, maxY;
    GetAreaBorders(minX, minY, maxX, maxY);
    if (GameState::GetCurrentMapID() == 0xd) minX -= 1;
    Render::SetViewportMapBounds(minX, minY - 0xd, maxX + 1, maxY);
}

}  // namespace

// ------------------------------------------------------------------------------- decorations
// @0x1346b8 Map::Decor::UpdateMapLink (home-map fake/real precedence included)
void Decor::UpdateMapLink() {
    Decor* d = this;
    if (!d->data) return;
    ForEachFootprintTile(d->x, d->y, d->data->w, d->data->h, [&](int x, int y) {
        Cell* c = At(x, y);
        if (!c) return;
        if (g_mapId == 0 && c->decor) {
            if (c->decor->fake && !d->fake) {
                // RemoveDecoration(x, y, false) of the fake one happens in the original; fakes are
                // only created on free tiles at load, so this path does not trigger during load.
            } else if (!c->decor->fake && d->fake) {
                return;  // real decoration keeps the tile
            }
        }
        c->decor = d;
    });
}

// @0x134c14 Map::Decor::UpdateImage with Map::depthStyle == 0 (never written; verified by xrefs).
void Decor::UpdateImage() {
    Decor* d = this;
    if (d->sprite) { Render::RemoveSprite(d->sprite); d->sprite = nullptr; }
    if (!d->GetData()) return;  // UNVERIFIED: decor ids that resolve to buildings (else-branch) not ported
    Render::Texture* tex = GameData::DecorImage(d->data);
    if (!tex) {
        // A missing picture: the placeholder on the tile (layer 8, or 3 for a flat decoration).
        if (Render::Texture* ph = Render::GetDecoPlaceholderDebugTex()) {
            Render::Sprite* s = Render::CreateSprite(ph, d->data->layer == 0 ? Render::kLayerObjects : Render::kLayerFlat3,
                                                     d->mirrored, false);
            d->sprite = s;
            float wy = (float)d->y * 42.f * 0.5f;
            Render::SetPosition(s, (float)d->x * 84.f + ((d->y & 1) ? 42.f : 0.f), wy, GetSpriteZ(wy, s->w, d->x));
        }
        return;
    }
    const GameData::DecorData& D = *d->data;
    Render::Sprite* s = Render::CreateSprite(tex, DecorLayer(D.layer), d->mirrored, false);
    d->sprite = s;
    float wy = (float)d->y * 42.f * 0.5f;
    float X, Y;
    d->GetBasePoint(X, Y);
    float half = s->w * 0.5f;
    // UNVERIFIED: GetSpriteZ's third argument (r2) is not set by this caller; it only adds a
    // (c % 5) / 10000 tie-break. Passed as 0.
    float z = GetSpriteZ(wy, half, 0);
    Render::SetPosition(s, X - (float)(int)half, Y, z);
    if (tex->frames > 0) {   // an animation sheet: its controller starts over
        tex->frameTime = D.frameTime;
        if (!anim) {
            anim = std::make_unique<DecorAnim>();
            anim->decor = this;
        }
        anim->frame = 0;
        anim->texture = tex;
        anim->timer = 0.f;
        anim->shownFrame = -1;
        anim->Update(0.f);
    } else {
        anim.reset();
    }
    s->visible = d->visible;
    linearX = x;
    linearY = y;
    TileCoordinatesToLinear(linearX, linearY);
}

// PORT: the world point its image hangs from, as UpdateImage and FindBaseCoordinates (0x130be8)
// both compute it.
void Decor::GetBasePoint(float& X, float& Y) const {
    const GameData::DecorData& D = *data;
    float wx = (float)(x * 84) + ((y & 1) ? 42.f : 0.f);
    float wy = (float)y * 42.f * 0.5f;
    int w = D.w, h = D.h;
    if (!mirrored) {
        X = 42.f + (float)(w / 2) * -42.f - (float)D.ox + wx;
        Y = (float)(h / 2) * 21.f - (float)D.oy + wy;
    } else {  // 0x134ecc
        int k = 0;
        if (w > h && (w % 2) == 0) k = w - h;
        if (w < h && (w % 2) == 1) k = w - h;
        if (k == 3) k = 2;
        X = 42.f + (float)(w / 2) * -42.f + (float)D.ox + (float)k * 84.f * 0.5f + wx;
        Y = (float)(h / 2) * 21.f - (float)D.oy + (float)k * 42.f * -0.5f + wy;
    }
}


// MapObject::GetStartTile @0x1c7658 (through Decor::GetData @0x131234): back half the width (the
// height when mirrored) along the row.
const GameData::DecorData* Decor::GetData() const {
    if (!data) data = GameData::GetDecoration(id);
    return data;
}

int Decor::GetIconOffsetY() const {
    if (!data) {
        const GameData::BuildingData* b = GameData::GetBuilding(id);
        return b ? b->iconY : 0;
    }
    return data->iconY;
}

void Decor::GetStartTile(int& tx, int& ty) const {
    if (!GetData()) { tx = ty = 0; return; }
    tx = x;
    unsigned uy = y;
    ty = (int)uy;
    int n = (mirrored ? data->h : data->w) / 2;
    for (int i = 0; i < n; ++i) {
        if ((uy & 1) == 0) --tx;
        uy = (unsigned)++ty;
    }
}

void Decor::GetBuildZone(int& w, int& h) const {
    if (!GetData()) { w = h = 0; return; }
    w = data->w;
    h = data->h;
}

bool Decor::CanBePlaced(bool ignoreFake) const {
    const GameData::DecorData* d = GameData::GetDecoration(id);
    if (!d) return false;
    int sx, sy;
    GetStartTile(sx, sy);
    bool ok = true;
    ForEachFootprintTileFrom(sx, sy, d->w, d->h, [&](int tx, int ty) {
        if (!ok) return;
        if (GetBuilding(tx, ty)) { ok = false; return; }
        Decor* o = GetDecoration(tx, ty);
        if (o != GetVirtualDecoration(tx, ty) && o && ((!ignoreFake && !o->fake) || !o->IsFake())) {
            ok = false;
            return;
        }
        if (tx < 0 || tx >= GetGridWidth() || ty < 0 || ty >= GetGridHeight()) { ok = false; return; }
        Patch* p = GetPatchForCoordinates(tx, ty, false);
        if (p && !p->owned) ok = false;
    });
    return ok;
}

Decor::~Decor() { delete meta; }

void Decor::SetMeta(const std::string& text) {
    delete meta;
    meta = nullptr;
    metaText = text;
    if (!metaText.empty()) meta = new MetaExpression(metaText.c_str(), false);
}

// @0x1316b8 Map::Decor::GetStartQuestID
uint32_t Decor::GetStartQuestID() const {
    const MetaData* d = meta ? meta->FindChildData(kExpStartQuest) : nullptr;
    return d ? (uint32_t)d->GetInt() : 0;
}

// @0x131688 Map::Decor::GetEndQuestID
uint32_t Decor::GetEndQuestID() const {
    const MetaData* d = meta ? meta->FindChildData(kExpEndQuest) : nullptr;
    return d ? (uint32_t)d->GetInt() : 0;
}

namespace {

// A BT entry is "x:y" or "name:x:y": the index of its x.
unsigned BlockedTileX(const MetaData* e) { return e->GetChild(0)->type == MetaData::kString ? 1u : 0u; }

int Int(const MetaData* list, unsigned i) { return list->GetChild(i)->GetInt(); }
const char* Str(const MetaData* list, unsigned i) { return list->GetChild(i)->GetString(); }

}  // namespace

// @0x134100 Map::Decor::SetBlockedTiles
void Decor::SetBlockedTiles() {
    const MetaData* bt = meta ? meta->FindChildData(kExpBt) : nullptr;
    if (!bt) return;
    for (unsigned i = 0; i < bt->GetChildrenCount(); ++i) {
        const MetaData* e = bt->GetChild(i);
        unsigned k = BlockedTileX(e);
        SetBlock(Int(e, k), Int(e, k + 1), true);
        AI::RemoveMapWaypoint(Int(e, k), Int(e, k + 1));
    }
    AI::LinkAdjacentWaypoints();
}

// @0x133fec Map::Decor::RemoveBlockedTiles
void Decor::RemoveBlockedTiles() {
    const MetaData* bt = meta ? meta->FindChildData(kExpBt) : nullptr;
    if (!bt) return;
    for (unsigned i = 0; i < bt->GetChildrenCount(); ++i) {
        const MetaData* e = bt->GetChild(i);
        unsigned k = BlockedTileX(e);
        if (AI::GetWaypoint(Int(e, k), Int(e, k + 1), false)) continue;
        SetBlock(Int(e, k), Int(e, k + 1), false);
        AI::CreateMapWaypoint(Int(e, k), Int(e, k + 1));
    }
    AI::LinkAdjacentWaypoints();
}

// ---- the quest reactions of a decoration's expression ------------------------------------------
namespace {

// SPELL_AFTER_*: the spell's effect (ParticlePresets::OnSpell) on the tile (at, at + 1), its spell at
// spell, its argument (spell 7's big cube) after it when given.
void SpellAt(const MetaData* d, unsigned at, unsigned spell) {
    int x = Int(d, at), y = Int(d, at + 1);
    TileCoordinatesToWorld(x, y);
    ParticlePresets::OnSpell((unsigned)Int(d, spell), (float)x, (float)y,
                             d->GetChildrenCount() > spell + 1 && Int(d, spell + 1) != 0);
}

uint32_t g_setupSeed = 0;   // 0x6118fc PrepareDecorationsForSetup: the RANDOM places' generator

// REPLACE_*: the decoration becomes object `id` and its blocked tiles are freed.
void ReplaceObject(Decor* d, uint32_t id) {
    d->ReplaceWith(id);
    d->RemoveBlockedTiles();
}

void Disable(Decor* d) {
    d->disabled = true;
    d->f4c = 0;
    d->RemoveBlockedTiles();
    d->RemoveParticles();
}

// PARTICLE_*: p's first value is the particle's name ("none": only the old ones go); its play count
// is value `ci` and "fixed start" the one after. At setup only an entry without a count (or with
// count 0) is restored, playing forever.
void SetParticles(Decor* d, const MetaData* p, unsigned ci, bool setup) {
    d->RemoveParticles();
    if (!d->data || d->data->particlePoints.empty()) return;
    const char* name = p->type == MetaData::kString ? p->GetString() : p->GetChild(0)->GetString();
    if (std::strcmp(name, "none") == 0) return;
    unsigned count = p->GetChildrenCount() > ci ? (unsigned)p->GetChild(ci)->GetInt() : 0;
    if (setup && count != 0) return;
    bool randomStart = p->GetChildrenCount() < ci + 2 || p->GetChild(ci + 1)->GetInt() == 0;
    d->CreateParticles(name, count, randomStart);
}

// The decorations GetDecorationToInteract collected (0x613694, count 0x6136a0).
std::vector<Decor*> g_filtered;

}  // namespace

// @0x1341d4 Map::Decor::OnInteract
void Decor::OnInteract(uint32_t object) {
    for (MetaExpression* e = meta; e; e = e->GetNext()) {
        const MetaData* d;
        if ((d = e->GetDataWithType(kExpHideInteract)) && d->type == MetaData::kNumber && (uint32_t)d->GetInt() == object) {
            visible = false;
            RemoveBlockedTiles();
        }
        if ((d = e->GetDataWithType(kExpUnhideInteract)) && d->type == MetaData::kNumber && (uint32_t)d->GetInt() == object) {
            visible = true;
            SetBlockedTiles();
        }
        if ((d = e->GetDataWithType(kExpUnhideAfterInteract)) && d->type == MetaData::kNumber &&
            (uint32_t)d->GetInt() == object) {
            visible = true;
            SetBlockedTiles();
        }
        if ((d = e->GetDataWithType(0x5d)) && d->type == MetaData::kNumber && (uint32_t)d->GetInt() == object)
            Disable(this);
        if ((d = meta->FindChildData(0x77)) && (uint32_t)Int(d, 0) == object) SpellAt(d, 1, 3);   // SPELL_AFTER_INTERACT
        if ((d = meta->FindChildData(kExpReplaceAfterInteract)) && (uint32_t)Int(d, 0) == object)
            ReplaceObject(this, (uint32_t)Int(d, 1));
    }
}

// @0x138430 Map::Decor::OnSubTaskCompleted
void Decor::OnSubTaskCompleted(uint32_t subtask) {
    for (MetaExpression* e = meta; e; e = e->GetNext()) {
        const MetaData* d;
        if ((d = e->GetDataWithType(kExpReplaceAfterSubQuest)) && (uint32_t)Int(d, 1) == subtask)
            ReplaceObject(this, (uint32_t)Int(d, 2));
        if ((d = e->GetDataWithType(kExpHideSubQuest)) && (uint32_t)Int(d, 1) == subtask) {
            visible = false;
            RemoveBlockedTiles();
        }
        if ((d = e->GetDataWithType(kExpUnhideSubQuest)) && (uint32_t)Int(d, 1) == subtask) {
            visible = true;
            SetBlockedTiles();
        }
        if ((d = e->GetDataWithType(kExpUnhideAfterSubQuest)) && (uint32_t)Int(d, 1) == subtask) {
            visible = true;
            SetBlockedTiles();
        }
        if ((d = e->GetDataWithType(kExpDisableAfterSubQuest)) && (uint32_t)Int(d, 1) == subtask) Disable(this);
        // PARTICLE_AFTER_SUB_QUEST (name:task:subtask[:count[:fixed start]]).
        if ((d = e->GetDataWithType(kExpParticleAfterSubQuest)) && (uint32_t)Int(d, 2) == subtask)
            SetParticles(this, d, 3, false);
        // CAM_AFTER_SUB_QUEST (task:subtask, then the path) and SHAKE_AFTER_SUB_QUEST
        // (task:subtask:milliseconds:strength).
        if ((d = e->GetDataWithType(kExpCamAfterSubQuest)) && (uint32_t)Int(d->GetChild(0), 1) == subtask)
            AddCameraPath(d, 1);
        if ((d = e->GetDataWithType(kExpShakeAfterSubQuest)) && (uint32_t)Int(d, 1) == subtask)
            AddCameraShake((float)Int(d, 2) / 1000.f, (float)Int(d, 3));
        if ((d = meta->FindChildData(0x76)) && (uint32_t)Int(d, 1) == subtask) SpellAt(d, 2, 4);   // SPELL_AFTER_SUB_QUEST
    }
}

// @0x138ae4 Map::Decor::OnTaskCompleted
void Decor::OnTaskCompleted(uint32_t task) {
    for (MetaExpression* e = meta; e; e = e->GetNext()) {
        const MetaData* d;
        if ((d = e->GetDataWithType(kExpHideQuest)) && d->type == MetaData::kNumber && (uint32_t)d->GetInt() == task) {
            visible = false;
            RemoveBlockedTiles();
        }
        if ((d = e->GetDataWithType(kExpUnhideQuest)) && d->type == MetaData::kNumber && (uint32_t)d->GetInt() == task) {
            visible = true;
            SetBlockedTiles();
        }
        if ((d = e->GetDataWithType(kExpUnhideAfterQuest)) && d->type == MetaData::kNumber &&
            (uint32_t)d->GetInt() == task) {
            visible = true;
            SetBlockedTiles();
        }
        if ((d = e->GetDataWithType(kExpReplaceAfterQuest)) && (uint32_t)Int(d, 1) == task)
            ReplaceObject(this, (uint32_t)Int(d, 0));
        if ((d = e->GetDataWithType(kExpDisableAfterQuest)) && d->type == MetaData::kNumber &&
            (uint32_t)d->GetInt() == task)
            Disable(this);
        // PARTICLE_AFTER_QUEST (name:task[:count[:fixed start]]).
        if ((d = e->GetDataWithType(kExpParticleAfterQuest)) && (uint32_t)Int(d, 1) == task)
            SetParticles(this, d, 2, false);
        // CAM_AFTER_QUEST (task, then the path) and SHAKE_AFTER_QUEST (task:milliseconds:strength).
        if ((d = e->GetDataWithType(kExpCamAfterQuest)) && (uint32_t)Int(d, 0) == task) AddCameraPath(d, 1);
        if ((d = e->GetDataWithType(kExpShakeAfterQuest)) && (uint32_t)Int(d, 0) == task)
            AddCameraShake((float)Int(d, 1) / 1000.f, (float)Int(d, 2));
        if ((d = meta->FindChildData(0x75)) && (uint32_t)Int(d, 0) == task) SpellAt(d, 1, 3);   // SPELL_AFTER_QUEST
    }
}

// @0x139190 Map::Decor::OnSetup: the state its expression asks for, from the quests' state now.
void Decor::OnSetup() {
    if (!meta) return;
    if (f4c != 0 || !meta->FindChildData(kExpProgressMsg)) SetBlockedTiles();
    auto interacted = [](const MetaData* key) {   // that object has been used up
        Decor* o = GetDecorationToInteract((uint32_t)key->GetInt());
        return o && o->f4c == 0;
    };
    for (MetaExpression* e = meta; e; e = e->GetNext()) {
        const MetaData* d;
        if ((d = e->GetDataWithType(kExpReplaceObj)) && f4c == 0) ReplaceObject(this, (uint32_t)d->GetInt());
        if ((d = e->GetDataWithType(kExpHideQuest)) && d->type == MetaData::kNumber &&
            GameState::TaskCompleted((unsigned)d->GetInt())) {
            visible = false;
            RemoveBlockedTiles();
        }
        if ((d = e->GetDataWithType(kExpUnhideQuest))) {
            visible = GameState::TaskCompleted((unsigned)d->GetInt());
            if (visible) SetBlockedTiles();
            else RemoveBlockedTiles();
        }
        if ((d = e->GetDataWithType(kExpUnhideAfterQuest)) && d->type == MetaData::kNumber &&
            GameState::TaskCompleted((unsigned)d->GetInt())) {
            visible = true;
            SetBlockedTiles();
        }
        if ((d = e->GetDataWithType(kExpReplaceAfterQuest)) && GameState::TaskCompleted((unsigned)Int(d, 1)))
            ReplaceObject(this, (uint32_t)Int(d, 0));
        if ((d = e->GetDataWithType(kExpReplaceAfterSubQuest)) &&
            GameState::SubTaskCompleted((unsigned)Int(d, 0), (unsigned)Int(d, 1)))
            ReplaceObject(this, (uint32_t)Int(d, 2));
        if ((d = e->GetDataWithType(kExpReplaceAfterInteract)) && interacted(d->GetChild(0)))
            ReplaceObject(this, (uint32_t)Int(d, 1));
        if ((d = e->GetDataWithType(kExpHideSubQuest)) &&
            GameState::SubTaskCompleted((unsigned)Int(d, 0), (unsigned)Int(d, 1))) {
            visible = false;
            RemoveBlockedTiles();
        }
        if ((d = e->GetDataWithType(kExpUnhideSubQuest))) {
            visible = GameState::SubTaskCompleted((unsigned)Int(d, 0), (unsigned)Int(d, 1));
            if (visible) SetBlockedTiles();
            else RemoveBlockedTiles();
        }
        if ((d = e->GetDataWithType(kExpUnhideAfterSubQuest)) &&
            GameState::SubTaskCompleted((unsigned)Int(d, 0), (unsigned)Int(d, 1))) {
            visible = true;
            SetBlockedTiles();
        }
        if ((d = e->GetDataWithType(kExpHideInteract)) && interacted(d)) {
            visible = false;
            RemoveBlockedTiles();
        }
        if ((d = e->GetDataWithType(kExpUnhideInteract))) {
            if (Decor* o = GetDecorationToInteract((uint32_t)d->GetInt())) {
                visible = o->f4c == 0;
                if (visible) SetBlockedTiles();
                else RemoveBlockedTiles();
            }
        }
        if ((d = e->GetDataWithType(kExpUnhideAfterInteract)) && interacted(d)) {
            visible = true;
            SetBlockedTiles();
        }
        if ((d = e->GetDataWithType(kExpDisableAfterQuest)) && d->type == MetaData::kNumber &&
            GameState::TaskCompleted((unsigned)d->GetInt()))
            Disable(this);
        if ((d = e->GetDataWithType(kExpDisableAfterSubQuest)) &&
            GameState::SubTaskCompleted((unsigned)Int(d, 0), (unsigned)Int(d, 1)))
            Disable(this);
        if ((d = e->GetDataWithType(0x5d)) && interacted(d)) Disable(this);
        // The particles its state calls for: PARTICLE_IDLE, PARTICLE_AFTER_INTERACT once used up,
        // PARTICLE_AFTER_QUEST and PARTICLE_AFTER_SUB_QUEST once their quest / step is completed.
        if ((d = e->GetDataWithType(kExpParticleIdle))) {
            RemoveParticles();
            if (data && !data->particlePoints.empty() && std::strcmp(d->GetString(), "none") != 0)
                CreateParticles(d->GetString(), 0, true);
        }
        if ((d = e->GetDataWithType(kExpParticleAfterInteract)) && f4c == 0) SetParticles(this, d, 1, true);
        if ((d = e->GetDataWithType(kExpParticleAfterQuest)) && GameState::TaskCompleted((unsigned)Int(d, 1)))
            SetParticles(this, d, 2, true);
        if ((d = e->GetDataWithType(kExpParticleAfterSubQuest)) &&
            GameState::SubTaskCompleted((unsigned)Int(d, 1), (unsigned)Int(d, 2)))
            SetParticles(this, d, 3, true);
        // RANDOM (x:y|x:y|...): the first time on this map the decoration moves to one of the places.
        if ((d = meta->GetDataWithType(kExpRandom)) && GameState::GetMapStartTime(GameState::GetCurrentMapID()) == 0) {
            g_setupSeed = g_setupSeed * 0x343fd + 0x269ec3;
            const MetaData* place = d->GetChild(((g_setupSeed & 0x7fffffff) >> 0x10) % d->GetChildrenCount());
            RemoveDecoration(x, y, true);
            x = (uint8_t)Int(place, 0);
            y = (uint8_t)Int(place, 1);
            UpdateImage();
            UpdateMapLink();
        }
        // CAM: a camera path the first time on this map.
        if ((d = meta->FindChildData(kExpCam2)) && GameState::GetMapStartTime(GameState::GetCurrentMapID()) == 0)
            AddCameraPath(d, 0);
        // CRAFT (regrow seconds as the second value): a profession's gathering node. Once its regrow
        // time is over (and the hero does not stand on it) it is reset to object 0x5e3; a node
        // that is no profession object gets a new one for the map (none: it disappears, its regrow
        // time starts over). A profession object blocks its tile and, while usable, glitters
        // ("bless").
        if ((d = meta->FindChildData(kExpCraft))) {
            uint32_t object = 0;
            bool generate = false;
            Entity* player = nullptr;
            if (d->GetChildrenCount() < 2 || (id != 0 && !Professions::IsProfessionObject(id)) || f4c != 0 ||
                (uint32_t)Timer::GetGlobalTime() <= collectStart + (uint32_t)d->GetChild(1)->GetInt() ||
                !(player = EntityManager::GetPlayer()) || (x == player->tileX && y == player->tileY)) {
                generate = id != 0;
            } else {
                id = 0x5e3;
                generate = true;
            }
            if (generate) {
                if (!Professions::IsProfessionObject(id)) {
                    if (uint32_t gen = Professions::GenerateProfessionObject(Professions::kProfession)) {
                        id = gen;
                        data = GameData::GetDecoration(gen);
                        UpdateImage();
                        Render::SortRenderLayer(Render::kLayerObjects, 1);
                        f4c = 1;
                    } else {
                        visible = false;
                        id = 0;
                        data = nullptr;
                        collectStart = (uint32_t)Timer::GetGlobalTime();
                    }
                }
                object = id;
            }
            if (Professions::IsProfessionObject(object)) {
                SetBlock(x, y, true);
                AI::RemoveMapWaypoint(x, y);
                AI::LinkAdjacentWaypoints();
                if (f4c != 0) CreateParticles("bless", 0, true);
            }
        }
        if (meta->FindChildData(kExpTrapDamage)) collectStart = 0;
    }
}

// @0x1b6f60 Map::GetNearestFilteredDecoration
Decor* GetNearestFilteredDecoration(unsigned px, unsigned py) {
    Decor* best = nullptr;
    auto dist = [px, py](const Decor* d) {
        int dx = (int)d->x - (int)px, dy = (int)d->y - (int)py;
        return (unsigned)(dx * dx + dy * dy);
    };
    for (Decor* d : g_filtered)
        if (!best || dist(d) < dist(best)) best = d;
    return best;
}

// @0x1bebc8 Map::GetDecorationToInteract
Decor* GetDecorationToInteract(uint32_t key) {
    g_filtered.clear();
    uint32_t map = GameState::GetCurrentMapID();
    for (auto& p : g_patches)
        for (auto& d : p->decors) {
            uint32_t place = (uint32_t)d->y + (uint32_t)d->x * 0x100;
            if (place + map * 0x10000 == key) g_filtered.push_back(d.get());
            if (map == 0 && place + 0x2f0000 == key) g_filtered.push_back(d.get());
        }
    Entity* player = EntityManager::GetPlayer();
    return player ? GetNearestFilteredDecoration((unsigned)player->tileX, (unsigned)player->tileY) : nullptr;
}

// @0x1ba9c4 Map::GetDecorationThatIsReplacedWith
Decor* GetDecorationThatIsReplacedWith(uint32_t id) {
    for (auto& p : g_patches)
        for (auto& d : p->decors)
            if (d->GetReplaceWithTarget() == id) return d.get();
    return nullptr;
}

// @0x1baa60 Map::GetTriggeredDecoration
Decor* GetTriggeredDecoration(int x, int y) {
    for (auto& p : g_patches)
        for (auto& d : p->decors)
            if (d->HasTriggerTileAt(x, y)) return d.get();
    return nullptr;
}

// @0x1c0708: every quest-blocked tile without a waypoint gets one (weight 10) for the length of a
// search (TargetedAI::SetBlockedPathTarget); RestoreBlockedTiles takes them away again.
void RemoveBlockedTiles() {
    g_openedTiles.clear();
    for (const auto& t : g_blockedTiles) {
        if (AI::GetWaypoint(t.first, t.second, false)) continue;
        AI::CreateMapWaypoint(t.first, t.second);
        if (AI::Waypoint* wp = AI::GetWaypoint(t.first, t.second, false)) {
            AI::LinkAdjacentWaypoints(wp);
            wp->weight = 10.f;
        }
        g_openedTiles.push_back(t);
    }
}

// @0x1b8824
void RestoreBlockedTiles() {
    for (const auto& t : g_openedTiles)
        if (AI::GetWaypoint(t.first, t.second, false)) AI::RemoveMapWaypoint(t.first, t.second);
    g_openedTiles.clear();
}

// @0x1bab04 Map::GetBlockingDecoration
Decor* GetBlockingDecoration(int x, int y) {
    for (auto& p : g_patches)
        for (auto& d : p->decors)
            if (d->HasBlockTileAt(x, y)) return d.get();
    return nullptr;
}

namespace {
Decor* NearestFilteredToPlayer() {
    Entity* player = EntityManager::GetPlayer();
    return player ? GetNearestFilteredDecoration((unsigned)player->tileX, (unsigned)player->tileY) : nullptr;
}
}  // namespace

// @0x1be870 Map::GetDecorationWithDrop
Decor* GetDecorationWithDrop(uint32_t item) {
    g_filtered.clear();
    for (auto& p : g_patches)
        for (auto& d : p->decors)
            if (d->visible && d->CanDropItem(item) && d->f4c != 0) g_filtered.push_back(d.get());
    return NearestFilteredToPlayer();
}

// @0x1bea14 Map::GetDecorationWithAnyDrop
Decor* GetDecorationWithAnyDrop() {
    g_filtered.clear();
    for (auto& p : g_patches)
        for (auto& d : p->decors)
            if (d->visible && d->f4c != 0 && !d->IsLocked() && d->meta && d->meta->FindChildData(kExpDrop))
                g_filtered.push_back(d.get());
    return NearestFilteredToPlayer();
}

// @0x1be5e8 Map::GetDecorationWithAnyInteraction
Decor* GetDecorationWithAnyInteraction(bool unlocked, bool reachable) {
    g_filtered.clear();
    for (auto& p : g_patches)
        for (auto& d : p->decors) {
            if (!d->visible || d->f4c == 0) continue;
            if (unlocked && d->IsLocked()) continue;
            if (reachable) {
                Entity* player = EntityManager::GetPlayer();
                if (AIPlayer* ai = dynamic_cast<AIPlayer*>(player->GetAI())) {
                    int ax = 0, ay = 0;
                    d->GetActionPoint(ax, ay, (int)ai->GetBestDecorationActionPoint(d.get()));
                    AI::Waypoint* wp = AI::GetWaypoint(ax, ay, false);
                    if (!wp) wp = AI::GetWaypointNearDecoration(d.get(), false);
                    if (!wp || !ai->SetTarget(wp, true)) continue;
                }
            }
            g_filtered.push_back(d.get());
        }
    return NearestFilteredToPlayer();
}

// @0x1bf1ec Map::GetDecorationToClick
Decor* GetDecorationToClick(uint32_t key) {
    g_filtered.clear();
    char buf[32];
    for (auto& p : g_patches)
        for (auto& d : p->decors) {
            if (d->f4c == 0 || !d->visible) continue;
            snprintf(buf, sizeof buf, "%d%d", (int)GameState::GetCurrentMapID(), (int)d->id);
            if ((uint32_t)atoi(buf) == key) g_filtered.push_back(d.get());
            if (GameState::GetCurrentMapID() == 0) {
                snprintf(buf, sizeof buf, "%d%d", 0x2f, (int)d->id);
                if ((uint32_t)atoi(buf) == key) g_filtered.push_back(d.get());
            }
        }
    for (Decor* d : g_filtered)
        if (d->WorkerAssigned()) return d;
    return NearestFilteredToPlayer();
}

bool DecorationVisibilityUpdateAllowed() { return g_allowDecorationVisibility; }
bool BuildingVisibilityUpdateAllowed() { return g_allowBuildingVisibility; }

namespace {
struct DecorInfo { int x, y; uint32_t uses, start; };   // 0x10 bytes
std::vector<DecorInfo> g_decorInfo;                      // 0x6136e0
}  // namespace

// @0x1b85c0 Map::StoreDecorationInfo
void StoreDecorationInfo() {
    if (g_patches.empty()) return;
    g_decorInfo.clear();
    for (auto& d : g_patches[0]->decors) g_decorInfo.push_back({d->x, d->y, d->f4c, d->collectStart});
}

// @0x1b7a38 Map::RestoreDecorationInfo
void RestoreDecorationInfo() {
    if (g_patches.empty()) return;
    printf("Map::RestoreDecorationInfo() %d decorations to restore\n", (int)g_decorInfo.size());
    for (auto& d : g_patches[0]->decors)
        for (const DecorInfo& i : g_decorInfo)
            if (i.x == d->x && i.y == d->y) {
                d->f4c = i.uses;
                d->collectStart = i.start;
            }
    g_decorInfo.clear();
}

// @0x1b8300 Map::RemoveHoldOfEntities
void RemoveHoldOfEntities() {
    for (auto& p : g_patches) {
        for (auto& b : p->buildings) b->RemoveHoldOfEntities();
        for (auto& d : p->decors) d->RemoveHoldOfEntities();
    }
}

// @0x131268 Map::PrepareDecorationsForSetup
void PrepareDecorationsForSetup() { g_setupSeed = Timer::GetGlobalTime(); }

// @0x1baf70 Map::ExecuteStaticMeta
void ExecuteStaticMeta() {
    PrepareDecorationsForSetup();
    for (auto& p : g_patches)
        for (size_t i = 0, n = p->decors.size(); i < n; ++i) p->decors[i]->OnSetup();
}

// @0x1b65d4 Map::OnTaskCompleted
void OnTaskCompleted(uint32_t task) {
    for (auto& p : g_patches)
        for (size_t i = 0, n = p->decors.size(); i < n; ++i) p->decors[i]->OnTaskCompleted(task);
}

// @0x1b6650 Map::OnSubTaskCompleted
void OnSubTaskCompleted(uint32_t subtask) {
    for (auto& p : g_patches)
        for (size_t i = 0, n = p->decors.size(); i < n; ++i) p->decors[i]->OnSubTaskCompleted(subtask);
}

// @0x1b66cc Map::OnInteract
void OnInteract(uint32_t object) {
    for (auto& p : g_patches)
        for (size_t i = 0, n = p->decors.size(); i < n; ++i) p->decors[i]->OnInteract(object);
}

void Decor::SpeedupDecoration() { collectStart = Timer::GetGlobalTime() + ~f50; }

// @0x1b8724 Map::EnqueueBuildingPlacement
void EnqueueBuildingPlacement(int x, int y, uint32_t id) { g_placements.push_back({x, y, id}); }

void Decor::ReplaceWith(uint32_t newId) {
    if (GameState::GetCurrentMapID() == 0 && GameData::GetBuilding(newId)) {
        EnqueueBuildingPlacement(x, y, newId);   // (in the city it becomes a real building)
        return;
    }
    id = newId;
    data = GameData::GetDecoration(newId);
    UpdateImage();
    Render::SortRenderLayer(Render::kLayerObjects, 1);
}

// @0x11e5f0: every tile of the footprint is free of other buildings and of decorations (fake ones
// count as free when ignoreFake), on the grid, not on unowned land and without an NPC on it.
bool Building::CanBePlaced(bool ignoreFake) const {
    int sx, sy, w, h;
    GetStartTile(sx, sy);
    GetBuildZone(w, h);
    bool ok = true;
    ForEachFootprintTileFrom(sx, sy, w, h, [&](int tx, int ty) {
        if (!ok) return;
        Building* b = GetBuilding(tx, ty);
        Decor* d = GetDecoration(tx, ty);
        if ((b && b != this) || (d && ((!ignoreFake && !d->fake) || !d->IsFake())) ||
            tx < 0 || tx >= GetGridWidth() || ty < 0 || ty >= GetGridHeight()) {
            ok = false;
            return;
        }
        Patch* p = GetPatchForCoordinates(tx, ty, false);
        if (p && !p->owned) { ok = false; return; }
        Entity* e = EntityManager::GetEntityAtXY(tx, ty);
        if (e && e->IsNPC()) ok = false;
    });
    return ok;
}

int GetGridWidth() { return g_gridW; }     // @0x1b68cc
int GetGridHeight() { return g_gridH; }   // @0x1b68e0
uint32_t GetMapID() { return g_mapId; }   // @0x1b68b8
int GetTileset() { return g_tileset; }    // @0x1b73d8

void TileCoordinatesToLinear(int& x, int& y) {
    int d = y + x * -2;
    if (d < 0) d = (d - 1) - ((d - 1) >> 31);
    x = (x * 2 + y + 1) / 2;
    y = d >> 1;
}

// @0x11e4a4
void Building::LinkBaseToBuilding() {
    int sx, sy, w, h;
    GetStartTile(sx, sy);
    GetBuildZone(w, h);
    ForEachFootprintTileFrom(sx, sy, w, h, [&](int tx, int ty) {
        Cell* c = At(tx, ty);
        if (!c) return;
        if (GameState::GetCurrentMapID() == 0) RemoveDecorationAt(tx, ty);
        else c->decor = nullptr;
        c->building = this;
        if (AI::Waypoint* wp = AI::GetWaypoint(tx, ty, false)) wp->weight = 1000.f;
    });
}

bool IsLoaded() { return g_loaded; }

Building* GetCurrentFarm() { return g_currentFarm; }
int GetFarmGridWidth() { return kFarmGridW; }
int GetFarmGridHeight() { return kFarmGridH; }
float GetFarmWorldX() { return kFarmWorldX; }
float GetFarmWorldY() { return kFarmWorldY; }
Patch* GetFarmPatch() { return g_farmPatch.get(); }

// @0x1b94f0: the farm view's patch (area 0x29a at row 120, 3 x 6 tiles).
void CreateFarmPatch() {
    g_farmPatch = std::make_unique<Patch>();
    g_farmPatch->areaId = 0x29a;
    g_farmPatch->x = 0;
    g_farmPatch->y = 0x78;
    g_farmPatch->w = 3;
    g_farmPatch->h = 6;
}

// @0x1b9228: in: the farm's location, blocks, view bounds, ground, waypoints (every tile but (1, 0)),
// its entities, the farm HUD and the camera; out: back to the city as it was.
// PORT: the low-memory device path (Render::FreeMemory) is left out.
void ShowFarm(bool show, Building* b) {
    if (show) {
        GameState::SetCurrentLocation(1);
        g_currentFarm = b;
        g_farmBlocks.assign((size_t)(kFarmGridW * kFarmGridH), 1);
        Render::SetViewportMapBounds(1, 0x75, 0xc, 0x98);
        Background::CreateFarm(0);
        for (int y = 0; y < kFarmGridH; ++y)
            for (int x = 0; x < kFarmGridW; ++x)
                if (!(x == 1 && y == 0)) AI::CreateFarmWaypoints(x, y);
        AI::LinkAdjacentFarmWaypoints();
        b->SpawnFarm();
        HUDWindow::EnterFarm();
        Render::CenterOn(600.f, 2840.f);
        return;
    }
    if (GameState::GetCurrentLocation() == 1) GameState::SetCurrentLocation(0);
    UpdateOwnedAreaBorders();
    int minX = 0, minY = 0, maxX = 0, maxY = 0;
    GetAreaBorders(minX, minY, maxX, maxY);
    Render::SetViewportMapBounds(minX, minY - 0xd, maxX + 1, maxY);
    Background::RemoveFarm();
    AI::RemoveFarmWaypoints();
    g_farmBlocks.clear();
    Building* farm = g_currentFarm;
    Render::CenterOn(farm->baseX, (farm->maxY + farm->minY) * 0.5f);
    farm->DespawnFarm();
    g_currentFarm = nullptr;
}

Building* GetBuilding(int x, int y) {
    Cell* c = At(x, y);
    return c ? c->building : nullptr;
}

void SetBuilding(int x, int y, Building* b) {
    if (Cell* c = At(x, y)) c->building = b;
}

Decor* GetDecoration(int x, int y) {
    Cell* c = At(x, y);
    if (!c) return nullptr;
    return c->decor ? c->decor : c->virtualDecor;
}

Decor* GetVirtualDecoration(int x, int y) {
    Cell* c = At(x, y);
    return c ? c->virtualDecor : nullptr;
}

void SetVirtualDecoration(int x, int y, Decor* d) {
    if (Cell* c = At(x, y)) c->virtualDecor = d;
}

void SetDecoration(int x, int y, Decor* d) {
    if (Cell* c = At(x, y)) c->decor = d;
}

void RemoveBuilding(int x, int y, bool onlyUnlink, bool deleteIt) {
    Building* b = GetBuilding(x, y);
    if (!b) return;
    if (!onlyUnlink) b->OnDestroy();
    Patch* p = b->patch;
    int sx = 0, sy = 0, w = 0, h = 0;
    b->GetStartTile(sx, sy);
    b->GetBuildZone(w, h);
    ForEachFootprintTileFrom(sx, sy, w, h, [&](int tx, int ty) {
        SetBuilding(tx, ty, nullptr);
        if (AI::Waypoint* wp = AI::GetWaypoint(tx, ty, false)) wp->weight = 1.f;
    });
    if (onlyUnlink || !p) return;
    // Swap-removed from the patch's list, as the original's array.
    const int i = b->index;
    std::unique_ptr<Building> owned = std::move(p->buildings[(size_t)i]);
    if ((size_t)i + 1 != p->buildings.size()) {
        p->buildings[(size_t)i] = std::move(p->buildings.back());
        p->buildings[(size_t)i]->index = i;
    }
    p->buildings.pop_back();
    BuildingHovers::UnregisterBuilding(b);
    if (!deleteIt) owned.release();   // the caller keeps it (the warehouse)
}

void RemoveDecoration(int x, int y, bool onlyUnlink) {
    Cell* c = At(x, y);
    Decor* d = c ? c->decor : nullptr;
    if (!d) return;
    if (const GameData::DecorData* data = d->GetData()) {
        ForEachFootprintTile(d->x, d->y, data->w, data->h, [&](int tx, int ty) { SetDecoration(tx, ty, nullptr); });
    }
    if (onlyUnlink) return;
    if (d->sprite) Render::RemoveSprite(d->sprite);
    d->sprite = nullptr;
    d->removed = true;
}

Decor* GetDecorationIgnoringBuildzones(int x, int y) {
    Patch* p = GetPatchForCoordinates(x, y, true);
    if (!p) return nullptr;
    for (auto& d : p->decors)
        if (d->x == x && d->y == y) return d.get();
    return nullptr;
}

namespace {
// The AreaInfo copy at the start of a patch (0xac bytes); the port keeps the parts it reads.
std::unique_ptr<Patch> PatchFromArea(const GameState::AreaInfo& a) {
    auto p = std::make_unique<Patch>();
    p->infoId = a.id;
    p->x = a.x; p->y = a.y; p->w = a.w; p->h = a.h;
    p->price = a.c;
    p->buyable = a.a;   // UNVERIFIED: "a" mapped to Patch+0xa8 (used by UpdateAreaBorders)
    return p;
}

// @0x1bee2c (LoadPlayer, the city): a patch for every area of the settings, ids 1, 2, ... while
// GetAreaInfo finds one, each with its id at +0xe4.
void AddPatches() {
    if (!g_patches.empty()) return;
    GameState::AreaInfo a;
    for (uint32_t id = 1; GameState::GetAreaInfo(id, a); ++id) {
        g_patches.push_back(PatchFromArea(a));
        g_patches.back()->areaId = id;
    }
}

// @0x1c0b90 (LoadPlayer, other maps): one patch covering the grid, area 1 (its AreaInfo zeroed but
// for id 1 and a = 1).
void AddOnePatch() {
    if (!g_patches.empty()) return;
    GameState::AreaInfo a;
    a.id = 1;
    a.w = GetGridWidth();
    a.h = GetGridHeight();
    a.a = true;
    g_patches.push_back(PatchFromArea(a));
    g_patches.back()->areaId = 1;
}
}  // namespace

// @0x1bb7c8: every patch's random decorations.
void CreateRandomDecors() {
    for (auto& p : g_patches) AddRandomDecors(p.get());
}

// @0x1b7c0c: every building without a unique id gets the next one.
void AssignUniqueIDs() {
    for (auto& p : g_patches)
        for (auto& b : p->buildings) {
            if (b->uniqueId != 0) continue;
            std::printf("Setting new ID: %d\n", (int)GameState::latestUniqueID + 1);
            b->SetUniqueID(GameState::latestUniqueID + 1);
        }
    std::printf("latest unique ID: %d\n", (int)GameState::latestUniqueID);
}

// @0x1b7e30: the pause's reminders from the buildings: the first farm patch to rot (three quarters
// of the time left from a planted patch's rot timer to its order's timeRot), the last tax building
// to be ready (class 0: collectTime after stateTime), and the gathering building that fills last
// (class 4 with its worker at work: collectTime per resource still missing of 50).
void UpdateNotifications() {
    if (g_patches.empty()) return;
    uint32_t rot = 99999999, gather = 0, money = 0;
    for (auto& p : g_patches) {
        for (auto& bp : p->buildings) {
            Building* b = bp.get();
            if (b->data->buildingClass == 2 && (uint32_t)Timer::GetGlobalTime() < b->f54)
                Timer::GetGlobalTime();   // (sic: nothing comes of it)
            if (b->data->buildingClass == 0xd) {
                const int count = b->data->id == 0x3ee ? 1 : 6;
                const auto& missions = b->data->delivery->missions;
                for (int i = 0; i < count; ++i) {
                    if (b->patchArg[i] == 0 || b->patchContract[i] < 1) continue;
                    const uint32_t end = b->patchArg[i] + (uint32_t)missions[(size_t)b->patchContract[i] - 1].timeRot;
                    const int left = (int)(end - (uint32_t)Timer::GetGlobalTime());
                    // (unsigned while the timer has not run out, else signed: past the end, 0)
                    const float f = (float)left * 0.75f;
                    const uint32_t t = f > 0.f ? (uint32_t)f : 0;
                    if (t <= rot) rot = t;
                }
            }
            if (b->data->buildingClass == 0) {
                const uint32_t end = (uint32_t)b->data->collectTime + b->stateTime;
                if ((uint32_t)Timer::GetGlobalTime() < end) {
                    const uint32_t left = end - (uint32_t)Timer::GetGlobalTime();
                    if (money < left) money = left;
                }
            }
            if (b->data->buildingClass == 4 && b->WorkerAssigned(0) && b->WorkerIsWorking(0)) {
                const int missing = 0x32 - b->GetGatheredResCount();
                if (missing > 0) {
                    const uint32_t t = (uint32_t)b->data->collectTime * (uint32_t)missing;
                    if (gather < t) gather = t;
                }
            }
        }
    }
    if (rot != 99999999) GetNotifications().PushNotification(Notifications::kFarmWillRot, rot);
    if (gather != 0) GetNotifications().PushNotification(Notifications::kResourcesReady, gather);
    if (money != 0) GetNotifications().PushNotification(Notifications::kMoneyReady, money);
}

Patch* GetAreaByID(uint32_t id) {
    for (auto& p : g_patches)
        if (p->infoId == id) return p.get();
    GameState::AreaInfo a;
    if (!GameState::GetAreaInfo(id, a)) return nullptr;
    g_patches.push_back(PatchFromArea(a));
    return g_patches.back().get();
}

namespace {
struct ObjectAttribute {         // 0x88 bytes
    uint32_t id = 0, sub = 0;    // +0x00 +0x04
    char name[64] = {};          // +0x08
    char value[64] = {};         // +0x48
};
std::vector<ObjectAttribute> g_objectAttributes;   // 0x61380c
}  // namespace

// @0x1bca94 Map::ChangeObjectAttribute
void ChangeObjectAttribute(uint32_t id, uint32_t sub, const char* name, const char* value) {
    for (ObjectAttribute& a : g_objectAttributes)
        if (a.id == id && a.sub == sub && !std::strcmp(a.name, name)) std::strcpy(a.value, value);
    ObjectAttribute a;
    a.id = id;
    a.sub = sub;
    std::strcpy(a.name, name);
    std::strcpy(a.value, value);
    g_objectAttributes.push_back(a);
}

// @0x1b6cb4 Map::GetBuildingWithContract
Building* GetBuildingWithContract(const Contracts::Contract* contract, bool done) {
    auto delivers = [contract](const Building* b) {
        return (b->data->buildingClass == 2 || b->data->buildingClass == 0xd) && b->data->delivery == contract;
    };
    if (done)
        for (auto& p : g_patches)
            for (auto& b : p->buildings)
                if (delivers(b.get()) && b->contractDone) return b.get();
    for (auto& p : g_patches)
        for (auto& b : p->buildings)
            if (delivers(b.get())) return b.get();
    return nullptr;
}

// @0x1bacd0 Map::GetUpgradeableBuilding
Building* GetUpgradeableBuilding() {
    for (auto& p : g_patches)
        for (auto& b : p->buildings)
            if (b->IsOpened() && (unsigned)b->level != b->data->GetMaxUpgradeCount()) return b.get();
    return nullptr;
}

// @0x1bf540 Map::GetFarmWithPatchToBuy
Building* GetFarmWithPatchToBuy(uint32_t id) {
    // (resourceLeft +0x100 counts a farm's bought patches from 0; +0x150 has one entry per patch)
    auto hasPatch = [](const Building* b) {
        return b->IsOpened() && b->data->buildingClass == 0xd && (size_t)b->resourceLeft + 1 != b->patchEntities.size();
    };
    for (auto& p : g_patches) {
        size_t n = p->buildings.size();
        if (n == 0) continue;
        if (id != 0) {
            // The farms of that type; when the patch's last building is one of them the search
            // moves on to the next patch, else any farm of this patch will do (as the original).
            bool lastWasOne = false;
            for (size_t i = 0; i < n; ++i) {
                Building* b = p->buildings[i].get();
                lastWasOne = b->id == id;
                if (lastWasOne && hasPatch(b)) return b;
            }
            if (lastWasOne) continue;
        }
        for (size_t i = 0; i < n; ++i)
            if (hasPatch(p->buildings[i].get())) return p->buildings[i].get();
    }
    return nullptr;
}

// @0x1b7264 Map::GetNextAreaID
uint32_t GetNextAreaID() {
    const Patch* best = nullptr;
    for (auto& p : g_patches)
        if (!p->owned && p->bordered && p->buyable && (!best || p->price < best->price)) best = p.get();
    return best ? best->areaId : 0;
}

Patch* GetPatchForCoordinates(int x, int y, bool quiet) {
    for (auto& p : g_patches)
        if (p->x <= x && x < p->x + p->w && p->y <= y && y < p->y + p->h) return p.get();
    std::printf("Map::GetPatchForCoordinates() Failed to find patch for coordinates %d %d\n", x, y);
    if (quiet) return nullptr;
    auto p = std::make_unique<Patch>();
    p->owned = true;
    p->bordered = true;
    p->w = g_gridW;
    p->h = g_gridH;
    g_patches.push_back(std::move(p));
    return g_patches.back().get();
}

bool IsValidAreaForBuildZone(int x, int y, int w, int h) {
    unsigned ux = (unsigned)x, uy = (unsigned)y;
    for (int i = 0; i < w / 2; ++i)
        if (((uy + (unsigned)i) & 1) == 0) --ux;
    if (w / 2 > 0) uy += (unsigned)(w / 2);
    bool ok = true;
    ForEachFootprintTileFrom((int)ux, (int)uy, w, h, [&](int tx, int ty) {
        if (!ok) return;
        Patch* p = GetPatchForCoordinates(tx, ty, true);
        if (!p || !p->owned) ok = false;
    });
    return ok;
}

Building* GetHQ() {
    if (Building* b = GetBuildingWithID(99)) return b;
    return GetBuildingWithID(100);
}

// A road tile on (x, y) of the given style; a large decoration covering the tile counts only when it
// was placed on exactly that tile. @0x1312cc
Decor* GetRoadNeighbor(int x, int y, const GameData::RoadData* road) {
    Decor* d = GetDecoration(x, y);
    if (!d || !d->data) return nullptr;
    if (d->data->road == road) return d;
    if (d->data->w > 1 || d->data->h > 1) {
        Decor* e = GetDecorationIgnoringBuildzones(x, y);
        if (e && e->data && e->data->road == road) return e;
    }
    return nullptr;
}

int GetRoadConnectionType(const Decor* d) {
    if (!d->data || !d->data->road) return 1;
    const GameData::RoadData* road = d->data->road;
    int x = d->x, y = d->y;
    bool even = (y & 1) == 0;
    // Neighbours: up-left, up-right, down-left, down-right.
    int mask = (GetRoadNeighbor(even ? x - 1 : x, y - 1, road) ? 1 : 0) |
               (GetRoadNeighbor(even ? x : x + 1, y - 1, road) ? 2 : 0) |
               (GetRoadNeighbor(even ? x - 1 : x, y + 1, road) ? 4 : 0) |
               (GetRoadNeighbor(even ? x : x + 1, y + 1, road) ? 8 : 0);
    // The original's branch tree, tabulated by mask.
    static const int kType[16] = {1, 10, 11, 6, 9, 5, 0, 12, 8, 1, 4, 14, 7, 15, 13, 3};
    return kType[mask];
}

void UpdateRoadConnections(int x0, int y0, int x1, int y1) {
    if (GameState::GetCurrentMapID() != 0) return;
    if (x0 > g_gridW || x1 < x0 || y0 > g_gridH || y1 < y0) return;
    int ys = y0 < 0 ? 0 : y0, xs = x0 < 0 ? 0 : x0;
    if ((unsigned)x1 >= (unsigned)g_gridW) x1 = g_gridW - 1;
    if ((unsigned)y1 >= (unsigned)g_gridH) y1 = g_gridH - 1;
    for (int y = ys; y <= y1; ++y) {
        for (int x = xs; x <= x1; ++x) {
            Decor* d = GetDecoration(x, y);
            if (!d) continue;
            // A large decoration covering the tile is looked up by its own tile unless it is
            // hidden (+0x44) and small.
            if (d->hidden && d->data && (d->data->w > 1 || d->data->h > 1)) {
                d = GetDecorationIgnoringBuildzones(x, y);
                if (!d) continue;
            }
            if (!d->metaText.empty() || !d->data || !d->data->road) continue;
            int type = GetRoadConnectionType(d);
            const GameData::RoadData* road = d->data->road;
            if (road->ids[type] != d->id) {
                d->ReplaceWith(road->ids[type]);
                road = d->data->road;
            }
            if (road->mirror[type] != (uint8_t)d->mirrored) {
                d->ToggleMirror();
                d->UpdateImage();
            }
        }
    }
}

// UNVERIFIED: both read the farm's own grid (0x613794) while the current location is the farm;
// the farm is not ported yet.
bool GetBlock(int x, int y) {
    if (GameState::GetCurrentLocation() == 1) {
        if ((unsigned)x >= (unsigned)kFarmGridW || (unsigned)y >= (unsigned)kFarmGridH || g_farmBlocks.empty())
            return false;
        return g_farmBlocks[(size_t)(kFarmGridW * y + x)] != 0;
    }
    Cell* c = At(x, y);
    return c && c->block;
}

void SetBlock(int x, int y, bool block) {
    if (GameState::GetCurrentLocation() == 1) {
        g_farmBlocks[(size_t)(kFarmGridW * y + x)] = block;
        return;
    }
    if (g_loaded && block) {
        bool found = false;
        for (auto& t : g_blockedTiles) found = found || (t.first == (uint8_t)x && t.second == (uint8_t)y);
        if (!found) g_blockedTiles.emplace_back((uint8_t)x, (uint8_t)y);
    }
    if (Cell* c = At(x, y)) c->block = block;
}

Building* GetIdleWorkplace() {
    for (auto& p : g_patches) {
        for (auto& bp : p->buildings) {
            Building* b = bp.get();
            if (!b->patch->owned) continue;
            if (b->WorkerAssigned(0) || b->BuilderAssigned()) continue;
            if (b->data->buildingClass == 4 && b->resourceLeft != 0 && b->resourceState == 1) return b;
            if ((b->needsBuilder != 0 || b->upgrading != 0) && !b->BuilderAssigned() && !b->BuilderIsWorking())
                return b;
        }
    }
    return nullptr;
}

Building* GetNearestStorage(Building* from, bool notSelf) {
    Building* best = nullptr;
    float bestD = 10000.f;
    for (auto& p : g_patches) {
        for (auto& bp : p->buildings) {
            Building* b = bp.get();
            if ((notSelf && b == from) || b->data->buildingClass != 7) continue;
            float dy = (float)((int)from->y - (int)b->y), dx = (float)((int)from->x - (int)b->x);
            float d = std::sqrt(dy * dy + dx * dx);
            if (d < bestD) {
                best = b;
                bestD = d;
            }
        }
    }
    return best;
}

void UpdateStorageMax() {
    GameState::updated = true;
    GameState::resourceAmountMax = 0;
    if (g_patches.empty()) return;
    for (auto& p : g_patches)
        for (auto& b : p->buildings)
            if (b->data->buildingClass == 7)
                GameState::resourceAmountMax += Setting("storage_space").GetChild((unsigned)b->level).GetInt();
    if (GameState::resourceAmountMax > 0) GameState::lastResourceAmountMax = GameState::resourceAmountMax;
}

void AllowBuildingVisibilityUpdate(bool on) { g_allowBuildingVisibility = on; }
void AllowDecorationVisibilityUpdate(bool on) { g_allowDecorationVisibility = on; }

void UpdateResources() {
    for (auto& p : g_patches)
        for (auto& b : p->buildings) b->UpdateResources();
}

void PrepareImages() {
    if (g_imagesPrepared) return;
    g_imagesPrepared = true;
    static const char* const kBirds[12] = {
        "images/bird/bird_000.png",   "images/bird/bird_001.png",   "images/bird/bird_002.png",
        "images/bird/bird_003.png",   "images/bird/bird_c_000.png", "images/bird/bird_c_002.png",
        "images/bird/bird_c_003.png", "images/bird/bird_c_004.png", "images/bird/bird_d_000.png",
        "images/bird/bird_d_001.png", "images/bird/bird_d_002.png", "images/bird/bird_d_003.png"};
    bool all = true;
    for (int i = 0; i < 12; ++i) {
        g_birdTex[i] = Resources::GetDirectImage(kBirds[i]);
        if (g_birdTex[i]) g_birdTex[i]->frameTime = 0.2f;
        else all = false;
    }
    if (all)
        for (int c = 0; c < 12; c += 4)
            for (int f = 0; f < 4; ++f) g_birdTex[c + f]->next = g_birdTex[c + (f + 1) % 4];
    static const char* const kClouds[6] = {
        "images/Effects/clouds/cloud_no_shad_001", "images/Effects/clouds/cloud_no_shad_002",
        "images/Effects/clouds/cloud_no_shad_003", "images/Effects/clouds/cloud_small_001",
        "images/Effects/clouds/cloud_small_002",   "images/Effects/clouds/cloud_small_003"};
    for (int i = 0; i < 6; ++i) g_cloudTex[i] = Resources::GetImage(kClouds[i]);
    // UNVERIFIED: each cloud texture's byte +0x28 is cleared; the port's Texture has no such field.
}

// (Map::Load) twelve clouds on layer 11 at random spots above the map.
void CreateClouds() {
    for (unsigned i = 0; i < 12; ++i) {
        Render::Texture* t = g_cloudTex[i % 6];
        if (!t) continue;
        Render::Sprite* s = Render::CreateSprite(t, Render::kLayerFog, false, false);
        g_clouds[i] = s;
        long rx = Rand48::lrand48();
        long ry = Rand48::lrand48();
        Render::SetPosition(s, (float)(rx % 4000) - 1000.f, (float)(ry % 300) - 350.f, 0.31f);
    }
}

// (Map::Load) one bird per 256 tiles, at a random spot of the map, each flying to a point up to
// 250 px away.
void CreateBirds() {
    int maxX = (int)(1.f + (float)(unsigned)GetGridWidth() * 84.f);
    int maxY = (int)(1.f + (float)(unsigned)GetGridHeight() * 42.f * 0.5f);
    g_birds.resize((unsigned)(GetGridWidth() * GetGridHeight()) >> 8);
    for (Bird& b : g_birds) {
        if (g_birdTex[0]) {
            b.sprite = Render::CreateSprite(g_birdTex[0], Render::kLayerFog, false, false);
            b.sprite->animTime = 0.f;
            long rx = Rand48::lrand48();
            long ry = Rand48::lrand48();
            Render::SetPosition(b.sprite, (float)(rx % maxX), (float)(ry % maxY), 0.31f);
        }
        if (!b.sprite) continue;   // PORT: the original reads the missing sprite
        b.tx = b.sprite->x + (float)(Rand48::lrand48() % 500) - 250.f;
        b.ty = b.sprite->y + (float)(Rand48::lrand48() % 500) - 250.f;
    }
}

// (Map::Update) the clouds drift right at 30 px/s, turned by -22.5 degrees (shader 8), and come
// back from the left past the map's right edge.
void UpdateClouds(double dt) {
    for (Render::Sprite* s : g_clouds) {
        if (!s) continue;
        Render::SetPosition(s, (float)(dt * 30.0) + s->x, s->y, s->z);
        Render::SetShaderType(s, 8);
        s->color[2] = -0.39269909f;
        s->color[1] = s->h * 0.5f - s->y;
        s->color[0] = s->x + s->w * 0.5f;
        if (512.f + (float)(unsigned)GetGridWidth() * 84.f < s->x) s->x = -1000.f;
        if (s->y < -1000.f) s->y = 3000.f;
    }
}

// (Map::Update) each bird flies to its point at 100 px/s, showing one of eight headings (a frame
// chain and its mirroring); at the point it picks another within 250 px; past the map's edges it
// comes back from the other side.
void UpdateBirds(double dt) {
    // vBottom, vTop, u0, u1 per heading (0x583db0) and the heading's frame chain (0x583e30).
    static const float kUV[8][4] = {{1, 0, 0, 1}, {0, 1, 1, 0}, {0, 1, 1, 0}, {0, 1, 0, 1},
                                    {1, 0, 1, 0}, {1, 0, 0, 1}, {1, 0, 1, 0}, {1, 0, 1, 0}};
    static const int kFrames[8] = {8, 0, 4, 0, 8, 0, 4, 0};
    for (Bird& b : g_birds) {
        Render::Sprite* s = b.sprite;
        if (!s) continue;
        float dy = s->y - b.ty, dx = s->x - b.tx;
        float d2 = dy * dy + dx * dx;
        float d = std::sqrt(d2);
        float uy = -dy / d, ux = -dx / d;
        double sy = (double)uy * dt * 100.0, sx = (double)ux * dt * 100.0;
        s->y = s->y + (float)sy;
        s->x = s->x + (float)sx;
        double reach2 = (double)uy * sy * dt * 100.0 + (double)ux * sx * dt * 100.0;
        float angle = (float)((double)std::atan2(-uy, ux) / 3.141592653589793 * 180.0);
        int dir = 0;
        if (angle >= -22.5f && angle < 22.5f) dir = 0;
        else if (angle >= 22.5f && angle < 67.5f) dir = 1;
        else if (angle >= 67.5f && angle < 112.5f) dir = 2;
        else if (angle >= 112.5f && angle < 157.5f) dir = 3;
        else if (angle >= 157.5f || angle < -157.5f) dir = 4;
        else if (angle < -112.5f) dir = 5;
        else if (angle < -67.5f) dir = 6;
        else if (angle < -22.5f) dir = 7;
        if (dir != b.dir) {
            Render::SetTexture(s, g_birdTex[kFrames[dir] % 12]);
            b.dir = dir;
            s->vBottom = kUV[dir][0];
            s->vTop = kUV[dir][1];
            s->u0 = kUV[dir][2];
            s->u1 = kUV[dir][3];
        }
        if ((double)d2 < reach2) {
            b.tx = s->x + (float)(Rand48::lrand48() % 500) - 250.f;
            b.ty = s->y + (float)(Rand48::lrand48() % 500) - 250.f;
        }
        float maxX = 512.f + (float)(unsigned)GetGridWidth() * 84.f;
        float maxY = (float)(unsigned)GetGridHeight() * 42.f * 0.5f + (GameState::GetCurrentMapID() == 0 ? -128.f : 256.f);
        if (s->x > maxX) {
            s->x = -512.f;
            b.tx = -512.f + (float)(Rand48::lrand48() % 500);
        }
        if (s->x < -512.f) {
            s->x = maxX;
            b.tx = s->x + (float)(Rand48::lrand48() % 500) - 100.f;
        }
        if (s->y > maxY) {
            s->y = -256.f;
            b.ty = s->y + (float)(Rand48::lrand48() % 500);
        }
        if (s->y < -256.f) {
            s->y = maxY;
            b.ty = s->y + (float)(Rand48::lrand48() % 500) - 500.f;
        }
    }
}

void UpdateStorages() {
    for (auto& p : g_patches)
        for (auto& b : p->buildings)
            if (b->data->buildingClass == 7) b->UpdateStorage();
}

int GetTotalStorageLimit() {
    return g_mapId != 0 ? GameState::lastResourceAmountMax : GameState::resourceAmountMax;
}

void GetOwnedAreaBorders(int& minX, int& minY, int& maxX, int& maxY) {
    minX = g_owned[0];
    minY = g_owned[1];
    maxX = g_owned[2];
    maxY = g_owned[3];
}

// @0x1b6004 (also remembers the result as Map::lastWorldX/Y 0x6138dc/0x6138e0)
void MouseCoordinatesToWorld(int& x, int& y) {
    float k = 2.f / Render::zoom;
    x = (int)((((float)x / (float)Render::ScreenWidth()) * k - Render::offsetX) + k * -0.5f);
    float ky = k / Render::aspect;
    y = (int)(Render::offsetY + ((float)y / (float)Render::ScreenHeight()) * ky + ky * -0.5f);
    g_lastWorldX = x;
    g_lastWorldY = y;
}

void WorldCoordinatesToTile(int& x, int& y) {
    int wy = y + 0x2a0;
    y = wy;
    int ry = wy % 0x2a;
    int tx = x / 0x54;
    int rx = x % 0x54;
    int ty = ((int)((float)wy + 42.f) / 0x2a) * 2;
    if (ry < 0x15) {
        float f = (float)ry;
        if (rx < (int)(42.f + (f / 21.f) * -42.f)) {
            --ty;
            --tx;
        }
        if ((int)(42.f + (f / 21.f) * 42.f) < rx) --ty;
    } else if (ry != 0x15) {
        float f = ((float)ry / 21.f) * 42.f - 42.f;
        if (rx < (int)f) {
            ++ty;
            --tx;
        }
        if ((int)(84.f - f) < rx) ++ty;
    }
    x = tx;
    y = ty - 0x20;
}

bool BuildingContains(const Building* b, int x, int y) {
    for (Render::Sprite* s : b->sprites)
        if (Render::HasPixelAt(s, (float)x, (float)y)) return true;
    return false;
}

bool Building::Contains(int x, int y) const { return BuildingContains(this, x, y); }

bool BuildingIsLower(const Building* a, const Building* b) {
    if (!a->sprites.empty() && !b->sprites.empty()) return a->sprites.front()->z < b->sprites.front()->z;
    return a->maxY < b->maxY;
}

Building* GetBuildingAtCoords(int x, int y) {
    Building* best = nullptr;
    for (auto& p : g_patches)
        for (auto& b : p->buildings)
            if (BuildingContains(b.get(), x, y) && (!best || BuildingIsLower(b.get(), best))) best = b.get();
    return best;
}

// Decor::Contains @0x1344b0: a visible pixel of it; quest decorations, portals, decorations still
// usable and collectable ones always count, the others only with `any`.
bool DecorContains(const Decor* d, int x, int y, bool any) {
    if (!d->visible || !d->sprite || !Render::HasPixelAt(d->sprite, (float)x, (float)y)) return false;
    if (d->meta || d->portal || d->f4c != 0) return true;
    if (!d->data || d->data->collectTime == 0) return any;
    return true;
}

bool Decor::Contains(int x, int y, bool any) const { return DecorContains(this, x, y, any); }

bool DecorIsLower(const Decor* a, const Decor* b) {   // @0x130e34
    if (!a->sprite || !b->sprite) return false;
    if (a->data && b->data && a->data->layer != b->data->layer) return (unsigned)a->data->layer < (unsigned)b->data->layer;
    return a->sprite->z < b->sprite->z;
}

Decor* GetDecorAtCoords(int x, int y, bool any) {
    Decor* best = nullptr;
    for (auto& p : g_patches)
        for (auto& d : p->decors)
            if (DecorContains(d.get(), x, y, any) && (!best || DecorIsLower(d.get(), best))) best = d.get();
    if (!best) {
        for (auto& p : g_patches)
            for (auto& d : p->decors)
                if (DecorContains(d.get(), x, y, true) && (!best || DecorIsLower(d.get(), best))) best = d.get();
    }
    return best;
}

void WorldCoordinatesToScreen(int& x, int& y) {
    float sx = 2.f / Render::zoom;
    float sy = sx / Render::aspect;
    x = (int)((((float)x + Render::offsetX + sx * 0.5f) / sx) * (float)Render::ScreenWidth());
    y = (int)(((((float)y - Render::offsetY) + sy * 0.5f) / sy) * (float)Render::ScreenHeight());
}

// @0x1bb868
void CreateRoadAI() {
    if (GameState::GetCurrentMapID() == 0) {
        for (int x = 0; x < GetGridWidth(); ++x)
            for (int y = 0; y < GetGridHeight(); ++y) AI::CreateMapWaypoint(x, y);
    } else if (!g_patches.empty()) {   // elsewhere the first patch's free tiles
        for (const Patch::Tile& t : g_patches[0]->tiles)
            if (!t.blocked) AI::CreateMapWaypoint(t.x, t.y);
    }
    AI::LinkAdjacentWaypoints();
}

void Update(double dt) {
    if (g_loadQueued) {   // EnqueueLoadMap's switch: this map is saved and the next one loaded
        SaveManager::SaveData* save = SaveManager::GetMainSave();
        if (save->HasMapInCache(0x70)) save->ResetMap(0x70);
        if (GameState::GetTutorialType() == 2 && GameState::tutorial == 9) {
            GameState::tutorial = 0x17;
            g_loadQueuedMap = 0;
        }
        if (GameState::tutorial == 0x17 && g_loadQueuedMap == 0) {
            // The way home: the world map opens and the marker walks to the castle first.
            BuildingHovers::HideArrow();
            GlobalMapWindow::Show();
            GlobalMapWindow::CenterOnMap(0);
            FirstCityEnterEffect::Run();
        }
        bool sepia = false;
        BaseCombat* combat = GameState::GetActiveCombat();
        if (!combat || combat->GetState() != 0x11) {
            if (GameState::tutorial != 0x17 && !Render::IsLowPerformanceDevice()) {
                GlobalMapWindow::AnimateSepiaIn();
                sepia = true;
            }
        } else {
            // A boss fight: the intro effect instead of the sepia.
            BuildingHovers::HideArrow();
            if (!Render::IsLowPerformanceDevice()) {
                BossIntroEffect::Prepare();
                BossIntroEffect::Run();
            } else {
                FastBossIntroEffect::Prepare();
                FastBossIntroEffect::Run();
            }
        }
        BuildingHovers::CollectAll();
        Tasks::CompleteSubtask(Tasks::kEnterMap, (uint32_t)g_loadQueuedMap, 1);
        Tasks::Update(true);
        if (0x75 < (unsigned)(GameState::secondTutorial - 0x8a) || GameState::secondTutorial == 0x9f) {
            if (g_loadQueuedSave) SafeSave();
            else Save(0);
        }
        GameState::SetCurrentMapID((uint32_t)g_loadQueuedMap);
        if (GlobalMapWindow::IsVisible()) GlobalMapWindow::Hide();
        const int map = g_loadQueuedMap;
        const bool cached = save->HasMapInCache((uint32_t)map);
        SaveManager::SaveBlock* block = SaveManager::GetMapData((uint32_t)map);
        const bool firstCity = map == 0 && GameState::tutorial != 0x80;
        // (the tutorial's soldier does not come to the city)
        if (GameState::tutorial == 0x17 && map == 0) EntityManager::RemoveEntitiesByID(0x1a2, true);
        Load(block, (uint32_t)Timer::GetGlobalTime(), false, cached);
        BaseCombat* c = GameState::GetActiveCombat();
        if (c && 2 < c->GetType()) {
            if (!Render::IsLowPerformanceDevice()) {
                BossIntroEffect::Continue();
                BossIntroEffect::Run();
            } else {
                FastBossIntroEffect::Continue();
                FastBossIntroEffect::Run();
            }
        } else if (sepia) {
            g_visibilityTimer += 1.f;
            Update(0.0);
            GlobalMapWindow::AnimateSepiaOut();
        }
        if (GameState::tutorial == 0x17) {
            BuildingHovers::HideArrow();
            if (firstCity) GameState::tutorial = 0x18;
        }
        return;
    }
    float fdt = (float)dt;
    g_visibilityTimer += fdt;
    g_fogTimer += fdt;
    if (g_visibilityTimer >= 0.1f) {
        AllowBuildingVisibilityUpdate(true);
        AllowDecorationVisibilityUpdate(true);
        // (Away from the city the sprites of the list 0x613844 would show only where the fog is
        // open; nothing in 5.11 ever adds to that list.)
        g_visibilityTimer = 0.f;
    }
    if (g_fogTimer >= 0.05f) {
        Fog::Update(g_fogTimer);
        g_fogTimer = 0.f;
    }
    g_currentTime = Timer::GetGlobalTime();   // SetCurrentTime @0x130f2c (0x6118c4)
    std::vector<Decor*> destroyed;
    for (auto& p : g_patches) {
        for (size_t i = 0; i < p->buildings.size(); ++i) p->buildings[i]->Update(dt);
        for (size_t i = 0; i < p->decors.size(); ++i) {
            Decor* d = p->decors[i].get();
            if (d->removed) continue;   // PORT: removed decorations stay in the list (RemoveDecoration)
            d->Update(dt);
            if (d->destroyed) destroyed.push_back(d);
        }
    }
    for (Decor* d : destroyed) RemoveDecoration(d->x, d->y, false);
    AllowBuildingVisibilityUpdate(false);
    AllowDecorationVisibilityUpdate(false);
    UpdateClouds(dt);
    // One queued building (EnqueueBuildingPlacement) is placed, finished, each update.
    if (!g_placements.empty()) {
        const QueuedPlacement q = g_placements.front();
        Building* b = new Building();
        b->id = q.id;
        b->data = GameData::GetBuilding(q.id);
        b->x = (uint8_t)q.x;
        b->y = (uint8_t)q.y;
        b->SetOpened();
        Patch* p = GetPatchForCoordinates(b->x, b->y, false);
        p->buildings.emplace_back(b);
        SetBuilding(b->x, b->y, b);
        b->patch = p;
        b->index = (int)p->buildings.size() - 1;
        if (b->data->parkingCount != 0) b->workers.assign(b->data->parkingCount, nullptr);
        b->FindBaseCoordinates();
        b->LinkBaseToBuilding();
        b->UpdateImage();
        b->f54 = 0;
        BuildingHovers::RegisterBuilding(b);
        for (unsigned i = 1; i < b->data->farmPatchDefault; ++i) b->OnSoilPatchBuy();
        b->built = 1;
        b->needsBuilder = 0;
        b->OnBuilded();
        b->stateTime = Timer::GetGlobalTime() - (uint32_t)b->data->collectTime;   // (its taxes are ready)
        g_placements.front() = g_placements.back();
        g_placements.pop_back();
        // The farm the farmer's wife asked for (tutorial step 0x3a): the arrow points at it.
        if (GameState::tutorial == 0x3a) {
            BuildingHovers::SetArrowClickCallback(new GUI::Callback([b] { BuildingHovers::OnBuildingClick(b, 0, 0); }));
            Tasks::HelpWithLocation((int)b->baseX, (int)b->minY, 0, 0);
            GUI::SetInteractionObjectLock(b, nullptr);
        }
    }
    UpdateCamera(fdt);
    g_resortTimer += fdt;
    if (g_resortTimer >= 0.757f) {
        Render::SortRenderLayer(Render::kLayerObjects, 1);
        g_resortTimer = 0.f;
    }
    float t = g_mapTimer, left = t;
    if (0.f < t) left = t - fdt;
    if (0.f < t) g_mapTimer = left;
    if (left < 0.f) {
        SoundsManager::PlaySound("map_timer_ding", 1, false);
        g_mapTimer = 360.f;
    }
    UpdateBirds(dt);
    Background::Update(dt);
}

void OfflineUpdate(double dt) {
    for (auto& p : g_patches) {
        for (size_t i = 0; i < p->buildings.size(); ++i) p->buildings[i]->Update(dt);
        for (size_t i = 0; i < p->decors.size(); ++i)
            if (!p->decors[i]->removed) p->decors[i]->Update(dt);
    }
}

void TileCoordinatesToWorld(int& x, int& y) {
    float fx = (float)x;
    float off = (y & 1) ? 42.f : 0.f;
    x = (int)(off + fx * 84.f);
    y = (int)((float)y * 42.f * 0.5f);
}

void GetAreaBorders(int& minX, int& minY, int& maxX, int& maxY) {
    minY = minX = 10000;
    maxY = maxX = -10000;
    for (auto& p : g_patches) {
        if (p->owned || !p->bordered) continue;
        if (p->x < minX) minX = p->x;
        if (maxX < p->w + p->x) maxX = p->w + p->x;
        if (p->y < minY) minY = p->y;
        if (maxY < p->h + p->y) maxY = p->h + p->y;
    }
    if (minX != 10000 && minY != 10000 && maxX != -10000 && maxY != -10000) return;
    minX = 0;
    minY = 0;
    maxX = GetGridWidth() - 1;
    maxY = GetGridHeight();
}


float GetSpriteZ(float a, float b, int c) {
    float v = (a + b * -0.25f) / 15.f / 1000.f;
    v = (float)(c % 5) / -10000.f + v;
    return v <= 0.2f ? 0.5f - v : 0.3f;
}

// @0x1bf768 (Map::Load, the reload, the exit): everything of the map goes; its entities let go of it
// and the temporary ones are removed.
void Free() {
    Particles::ClearParticles();
    for (auto& p : g_patches) {
        for (auto& d : p->decors) {
            BuildingHovers::UnregisterDecoration(d.get());   // ~Decor
            if (d->sprite) Render::RemoveSprite(d->sprite);
            d.reset();
        }
        for (auto& b : p->buildings) {
            BuildingHovers::UnregisterBuilding(b.get());
            b.reset();
        }
    }
    g_patches.clear();
    // PORT: the area borders' sprites (UpdateAreaBorders) are kept outside the patches.
    for (auto* s : g_borderSprites) Render::RemoveSprite(s);
    g_borderSprites.clear();
    g_grid.clear();
    g_farmPatch.reset();
    g_placements.clear();
    AI::FreeWaypoints();
    for (Render::Sprite*& s : g_clouds) {
        if (s) Render::RemoveSprite(s);
        s = nullptr;
    }
    FreeSpawnPointsAndPortals();
    for (unsigned i = 0;; ++i) {
        Entity* e = EntityManager::EnumEntities(i);
        if (!e) break;
        e->OnMapUnload();
    }
    SoldierPool::DeactivateDeadSoldiers();
    EntityManager::Clean(false);
    HUDWindow::SetBottomType(0);
    TutorialWindow::ResetState();
    if (!GameState::IsTutorial()) {
        GUI::SetInteractionObjectLock(nullptr, nullptr);
        GUI::SetInteractionLock(false);
    } else if (GameState::tutorial == 0x17) {
        GUI::SetInteractionLock(true);
    }
    // (The sprite list 0x613844, never filled in 5.11, is emptied here.)
    for (Bird& b : g_birds) b.sprite = Render::RemoveSprite(b.sprite);
    g_birds.clear();
    ClearFog();
    // (Render::FreeMemory on low-performance devices is not needed.)
}

bool Load(SaveManager::SaveBlock* block, uint32_t time, bool newGame, bool fromCache) {
    g_loading = true;
    // The loading screen only shows when the switch starts from the open world map (drawn twice
    // first, the world map still up).
    if (!GlobalMapWindow::IsVisible()) g_loadScreen = false;
    // PORT (user's request 2026-10-26): with Game::loadScreenSeconds set, every map switch in play
    // shows the card, over a still of the last frame when the world map is not up, and it stays at
    // least that long (below).
    const double loadStart = Timer::GetTime();
    Render::Texture* stillTex = nullptr;
    Render::Sprite* still = nullptr;
    if (gameLoaded && !g_loadScreen && Game::loadScreenSeconds > 0.0) {
        Render::Frame();
        stillTex = Render::CreateScreenTexture();
        still = Render::CreateSprite(stillTex, Render::kLayerGUI, false, false);
        still->screenSpace = true;
        Render::SetPosition(still, 0.f, (float)GUI::ScreenHeight(), 10.f);   // under every window
        Render::SortRenderLayer(Render::kLayerGUI, 1);
        g_loadScreen = true;
    }
    if (gameLoaded && g_loadScreen) {
        Render::Update();
        Render::Update();
    }
    // What is left of the map being left.
    ClearCameraPoints();
    CastleTopWindow::HideTools();
    HUDWindow::HideLocators();
    BuildingHovers::HideWorldDialog(nullptr);
    TaskCompleteWindow::Hide();
    BuildingHovers::CollectAll();
    BuildingMovement::Decline();
    BuildingPlacement::Decline();
    ClearCameraPoints();
    if (g_loadScreen) {
        LoadingScreenWindow::Show();
        Render::minLayer = 0xe;
        Render::clearColor = false;
        Render::Update();
    }
    Free();
    Rand48::srand48((long)time);
    g_loaded = false;
    g_blockedTiles.clear();
    if (!block) {   // PORT: the original goes on with a null block
        g_loading = false;
        return false;
    }
    block->next = 0;
    auto next = [&](uint32_t type) -> const SaveManager::Chunk* {  // SkipToChunk + BeginChunkLoading
        block->SkipToChunk(type);
        return block->NextChunkID() == type ? &block->GetChunk() : nullptr;
    };
    // LoadPlayer: header chunk 0xe
    const SaveManager::Chunk* hc = next(SaveManager::kHeader);
    if (!hc) {
        g_loading = false;
        return false;
    }
    SaveManager::Reader hr(*hc);
    hr.u32();                // version (FileManager::ResaveVersion: online)
    g_mapId = hr.u32();
    GameState::SetCurrentMapID(g_mapId);
    g_gridW = hr.u8();
    g_gridH = hr.u8();
    g_startX = hr.s8();      // the player's position (0x60eff0, validated against the grid)
    g_startY = hr.s8();
    if (g_startX >= 0 && g_startY >= 0 && (g_startX >= g_gridW || g_startY >= g_gridH)) {
        std::fprintf(stderr, "ERROR Map::LoadPlayer() Player position %d;%d is outside the map bounds %d;%d\n",
                     g_startX, g_startY, g_gridW, g_gridH);
        g_startX = g_startY = -1;
    }
    g_tileset = hr.u8();
    int patchCount = hr.u8();
    g_grid.assign((size_t)g_gridW * g_gridH, Cell{});

    Background::CreateLand((unsigned)g_tileset);  // Map::Load calls it after LoadPlayer; layer order is
                                                  // per-layer, so creating it first changes nothing.
    if (g_mapId == 0) AddPatches();
    else AddOnePatch();
    for (int i = 0; i < patchCount; ++i) {
        const SaveManager::Chunk* pc = next(SaveManager::kPatch);
        if (!pc) break;
        uint32_t id = pc->data.size() > 0 ? pc->data[0] : 0;
        Patch* raw = GetAreaByID(id);
        if (!raw) {   // PORT: the original goes on with a null patch (and crashes); skip its contents
            std::fprintf(stderr, "Map::LoadPlayer() no area %u\n", id);
            continue;
        }
        raw->areaId = id;
        raw->owned = pc->data.size() > 1 && pc->data[1] != 0;
        raw->bordered = pc->data.size() > 2 && pc->data[2] != 0;
        // A patch UpdateCityMap kept (the city of an older map version) is owned and loads from its
        // own block; the map file's contents up to its mask are skipped.
        SaveManager::SaveBlock* from = block;
        for (auto& [area, kept] : g_cityUpdateBlocks) {
            if (area != id) continue;
            raw->owned = true;
            block->SkipToChunk(SaveManager::kPatchMask);
            from = kept.get();
            from->next = 0;
            break;
        }
        auto nextIn = [&](uint32_t type) -> const SaveManager::Chunk* {
            from->SkipToChunk(type);
            return from->NextChunkID() == type ? &from->GetChunk() : nullptr;
        };
        // Patch::Load: LoadDecors (5 + n*0xb), LoadBuidings (6 + n*0xc), then mask (7)
        if (const SaveManager::Chunk* dc = nextIn(SaveManager::kDecorCount)) {
            int cnt = SaveManager::Reader(*dc).s16();
            for (int k = 0; k < cnt; ++k) if (auto* c = nextIn(SaveManager::kDecor)) LoadDecors(raw, *c);
        }
        if (const SaveManager::Chunk* bc = nextIn(SaveManager::kBuildingCount)) {
            int cnt = SaveManager::Reader(*bc).s16();
            for (int k = 0; k < cnt; ++k) {
                from->SkipToChunk(SaveManager::kBuilding);
                if (from->NextChunkID() != SaveManager::kBuilding) continue;
                LoadBuilding(raw, from);   // the chunk, then its optional followers
            }
        }
        if (const SaveManager::Chunk* mc = nextIn(SaveManager::kPatchMask)) {
            SaveManager::Reader mr(*mc);
            raw->mask = mr.str16();
            // Away from the city every tile is listed with its mask character ('1': blocked; no mask:
            // all free). PORT: a mask shorter than the grid counts as free past its end.
            if (g_mapId != 0) {
                size_t i = 0;
                for (int x = 0; x < g_gridW; ++x)
                    for (int y = 0; y < g_gridH; ++y, ++i) {
                        bool blocked = i < raw->mask.size() && raw->mask[i] == '1';
                        raw->tiles.push_back({x, y, blocked});
                        SetBlock(x, y, blocked);
                    }
            }
        }
    }
    CreateFarmPatch();   // (LoadPlayer, after the patches)
    std::puts("Map::LoadPlayer() Loading spawn points");
    LoadSpawnPoints(block);
    std::puts("Map::LoadPlayer() Loading portals");
    LoadPortals(block);
    // The fog: the covered spots of a map with its own fog, then what the player has opened.
    if (block->NextChunkID() == SaveManager::kFog) {
        SaveManager::Reader fr(block->GetChunk());
        unsigned n = (uint16_t)fr.s16();
        for (unsigned i = 0; i < n; ++i) {
            int x = fr.u8(), y = fr.u8(), size = fr.u8();
            AddFog(x, y, size);
        }
        block->EndChunkLoading(fr);
    }
    Fog::Init();
    Fog::Create((unsigned)(GetGridWidth() + 2), ((unsigned)GetGridHeight() >> 1) + 4, (unsigned)GetTileset());
    if (block->NextChunkID() == SaveManager::kFogV2) {
        Fog::LoadFog(block);
    } else {
        if (!Locations::MapHasCustomFog(GameState::GetCurrentMapID())) {
            // An ordinary map starts covered; one that opens with its campaign is open once that is
            // completed.
            Locations::LocationInfo* loc = Locations::GetLocationWithMap(GameState::GetCurrentMapID());
            if (loc && Locations::MapOpensFogAfterCompletion(GameState::GetCurrentMapID()) &&
                Locations::IsCampaignCompleted(loc))
                Fog::FillZero();
        } else {
            // A map with its own fog is open but for a circle round each spot (three sizes).
            Fog::FillZero();
            for (const FogMarker& m : g_fogMarkers) {
                int x = m.x, y = m.y;
                TileCoordinatesToWorld(x, y);
                if (m.size == 3) Fog::FillOne(x - 0x5f, y + 0x2e, x + 199, y + 0xc9);
                else if (m.size == 4) Fog::FillOne(x - 0xa0, y + 0x4c, x + 0xf4, y + 0x124);
                else if (m.size == 2) Fog::FillOne(x - 0x31, y + 0x10, x + 0x87, y + 0x73);
            }
        }
        Fog::OpenZero();
    }
    Fog::Update(128.f);
    if (g_mapId == 0) AssignUniqueIDs();   // (the end of LoadPlayer)
    LoadScreenFrame();
    // (back in Map::Load) the queued switch is over.
    g_loadQueuedMap = -1;
    g_loadQueuedDelay = 0.f;
    g_loadQueuedSave = false;
    g_newGameLoad = newGame;
    g_loadQueued = false;
    if (newGame) GameState::tutorial = 0x80;
    // Map::Load: CreateRandomDecors, UpdateAreaBorders (spawns/portals/fog not rendered yet)
    CreateRandomDecors();
    UpdateAreaBorders();
    Rand48::srand48((long)time);
    std::printf("Map %u: %dx%d tileset %d, %zu patches, %zu sprites\n", g_mapId, g_gridW, g_gridH, g_tileset,
                g_patches.size(), Render::SpriteCount());
    LoadScreenFrame();
    LoadScreenFrame();   // (after Background::CreateLand on the original; the port creates the land earlier)
    CreateRoadAI();
    LoadScreenFrame();
    // In the city (outside the PvP tutorial and fights) the rivals met on the maps are loaded anew.
    if (GameState::GetCurrentMapID() == 0) {
        UpdateStorageMax();
        if (!GameState::IsPvPTutorial() && !GameState::IsCombatActive()) {
            PvP::ClearPlayerInfo(4);
            PvP::LoadPvPData(4, nullptr, 0, nullptr);
        }
    }
    // (ProcessOfflineContracts @0x1b7434 is empty.)
    GameState::RefreshOfflineGoblins();
    AssignEntities();
    // The music: the city's; on a map (outside a fight in state 0x11, the boss's) its tileset's, the
    // battle map's only once the tutorial is past 0x16; none restarted while it plays.
    if (GameState::GetCurrentMapID() == 0) {
        if (!SoundsManager::IsMusicPlaying("music_city")) SoundsManager::PlayMusic("music_city", true);
    } else if (GameState::IsCombatActive() && GameState::GetActiveCombat()->GetState() == 0x11) {
        if (!SoundsManager::IsMusicPlaying("music_boss")) SoundsManager::PlayMusic("music_boss", false);
    } else if (GetTileset() == 0) {
        if (!SoundsManager::IsMusicPlaying("music_battle_map") && GameState::tutorial > 0x16)
            SoundsManager::PlayMusic("music_battle_map", true);
    } else if (GetTileset() == 1) {
        if (!SoundsManager::IsMusicPlaying("music_winter")) SoundsManager::PlayMusic("music_winter", true);
    } else if (GetTileset() == 2) {
        if (!SoundsManager::IsMusicPlaying("desert/sand_theme")) SoundsManager::PlayMusic("desert/sand_theme", true);
    }
    LinkPortals();
    UpdateSpawns();
    LoadScreenFrame();
    if (Entity* player = EntityManager::GetPlayer()) {
        if (GameState::GetCurrentMapID() == 0) player->SetSpawnPoint(nullptr);
        player->GetAI()->RemoveActionMarker();
        int x = player->tileX, y = player->tileY;
        TileCoordinatesToWorld(x, y);
        // A boss or arena fight (state 0x11) centres on the arena's camera tile, or on the map's
        // middle without one. (Animated over 0.4 s on the original: CenterOn's tween.)
        BaseCombat* combat = GameState::GetActiveCombat();
        if (!combat || combat->GetState() != 0x11) {
            Render::CenterOn((float)x, (float)y);
        } else if (GameState::mBossMapPositions[8] == 0 || GameState::mBossMapPositions[9] == 0) {
            Render::CenterOn((float)((unsigned)g_gridW >> 1) * 84.f, (float)((unsigned)g_gridH >> 1) * 42.f);
        } else {
            int cx = GameState::mBossMapPositions[8], cy = GameState::mBossMapPositions[9];
            TileCoordinatesToWorld(cx, cy);
            Render::CenterOn((float)cx, (float)cy);
        }
        if (GameState::tutorial == 9) Render::CenterOn(392.f, 201.f);
    }
    LoadScreenFrame();
    // "auto_fix_map_reset": a quest decoration of maps 0x52..0x59 that kept a use although its quest
    // is done loses it.
    if (GameState::SettingExists("auto_fix_map_reset") && Setting("auto_fix_map_reset").GetInt() == 1) {
        auto fix = [](int x, int y, bool done) {
            Decor* d = GetDecoration(x, y);
            if (d && d->f4c == 1 && done) d->f4c = 0;
        };
        switch (GameState::GetCurrentMapID()) {
        case 0x52:
            fix(0x1a, 0x3f, GameState::TaskCompleted(0x2ec));
            fix(0x1d, 0x1e, GameState::TaskCompleted(0x2e5));
            break;
        case 0x54: fix(0x18, 0x3d, GameState::SubTaskCompleted(0x28c, 0x3c8)); break;
        case 0x59: fix(0x20, 0x19, GameState::SubTaskCompleted(0x307, 0x3f3)); break;
        case 0x53: fix(0xd, 0x37, GameState::TaskCompleted(0x38a)); break;
        }
    }
    g_loaded = true;
    if (Entity* player = EntityManager::GetPlayer()) player->OnSetup();
    EntityManager::ExecuteStaticMeta();
    RestoreDecorationInfo();
    AI::allowWaypointLink = false;   // (the decorations block and free tiles; the links are redone once)
    ExecuteStaticMeta();
    AI::allowWaypointLink = true;
    AI::LinkAdjacentWaypoints();
    SpawnEntities();
    EntityManager::UpdateAggroZone();
    LoadScreenFrame();
    PrepareImages();
    if (GameState::GetCurrentMapID() == 0) {
        int x = 0x1e, y = 0xc;
        TileCoordinatesToWorld(x, y);
        Render::CenterOn((float)x, (float)y);
    }
    UpdateResources();
    if (GameState::GetCurrentMapID() == 0) Tasks::CompleteSubtask(Tasks::kEnterMap, 0x2f, 1);
    // (Online: the short tutorials the server can choose, GameState::GetTutorialType 1 and 2, fix
    // their tasks here: type 1 task 0xbe's flag +0x104; type 2 after task 0xd5 the locks of
    // 0x449/0x429/0x438/0x43a/0x43c, 0x120's next task, tasks 0xbe/0xfe/0x434/0x433 completed.)
    Tasks::Update(true);   // (not while Game::skipGameLoad, the online profile choice)
    HUDWindow::RemoveTaskArrows();
    BuildingHovers::Update(0.0, true);
    BeltBarWindow::Show();
    LoadScreenFrame();
    // From tileset 2 on, or off mission maps, the clouds.
    const bool missionMap = Locations::IsMissionMap(GameState::GetCurrentMapID());
    if ((unsigned)g_tileset >= 2 || !missionMap) CreateClouds();
    Render::SortRenderLayer(Render::kLayerObjects, 0);
    Render::SortRenderLayer(Render::kLayerFlat3, 1);
    Render::SortRenderLayer(4, 0);
    Render::SortRenderLayer(5, 1);
    Render::SortRenderLayer(Render::kLayerObjects, 0);
    Render::SortRenderLayer(Render::kLayerObjects, 0);
    Render::SortRenderLayer(Render::kLayerObjects, 1);
    LoadScreenFrame();
    // The map's name; a boss or arena fight with an enemy squad shows its intro instead: the PvP
    // one the players' (PlayerBattleInOutWindow; a real opponent's fight is reported as started),
    // UNVERIFIED (milestone 5) the others the boss's banner (BossIntroWindow with the boss name).
    if (EntityManager::GetPlayer()) {
        BaseCombat* combat = GameState::GetActiveCombat();
        if (!combat || combat->GetType() < 3 || !combat->GetEnemySquad()) {
            if (const char32_t* name = Locations::GetMapName(GameState::GetCurrentMapID())) HUDWindow::ShowMapName(name);
        } else if (combat->GetType() == 6) {
            PlayerBattleInOutWindow::SetModeIntro();
            PlayerBattleInOutWindow::SetParameters(PlayerCompetitionWindow::GetSelectedPlayerGID(), false);
            PlayerBattleInOutWindow::Show();
            // PORT: p null-checked (the original reads it as is).
            const PvP::PlayerInfo* p = PvP::GetPlayerInfoByGID(PlayerCompetitionWindow::GetSelectedPlayerGID(), 0xff);
            if (p && (unsigned)(p->gid + 4) > 3)
                PvP::ReportStateToServer(PlayerCompetitionWindow::GetSelectedPlayerGID(), PvP::kStateStarted);
        }
        CharacterInfoWindow::UpdateContents();
        GameState::SetupPlayerRegenerationState();
    }
    if (!missionMap || GameState::GetMapStartTime(GameState::GetCurrentMapID()) == 0)
        GameState::SetMapStartTime(GameState::GetCurrentMapID(), (uint32_t)Timer::GetGlobalTime());
    EntityManager::UpdateGraphics();
    AllowBuildingVisibilityUpdate(true);
    AllowDecorationVisibilityUpdate(true);
    Update(0.0);
    if (gameLoaded) {
        if (g_loadScreen && Game::loadScreenSeconds > 0.0) {
            // PORT: the card stays up (its cogs turning) until loadScreenSeconds have passed.
            double last = Timer::GetTime();
            while (last - loadStart < Game::loadScreenSeconds) {
                SDL_Delay(16);
                SDL_PumpEvents();
                const double now = Timer::GetTime();
                Render::UpdateAnimatedSprites((float)(now - last));
                last = now;
                Render::Update();
            }
        }
        Render::minLayer = 0;
        if (g_loadScreen) {
            Render::Update();
            LoadingScreenWindow::Hide();
        }
    }
    if (still) {
        Render::RemoveSprite(still);
        Render::RemoveTexture(stillTex);
    }
    g_loadScreen = true;
    if (GameState::tutorial == 9) {
        GameState::tutorial = 10;
    } else if (GameState::tutorial == 0x67 && GameState::GetCurrentMapID() == 0xe) {
        // The first tutorial ends on reaching map 0xe. (TutorialSender::AddTutorialStep(0x80) and
        // FileManager::SendTutorialFinished report it to the server: online.)
        GameState::tutorial = 0x80;
        GameState::lastSentStep = 0x80;
    }
    if (GameState::secondTutorial == 0x20e && GameState::GetCurrentMapID() != 0xf) BuildingHovers::HideArrow();
    // Outside the tutorial the battle bar shows on every map but the city and the emperor's (0xb).
    if (!GameState::IsTutorial()) {
        const uint32_t m = (uint32_t)GameState::GetCurrentMapID();
        if (m == 0 || m == 0xb) {
            if (BottomBattleWindow::IsVisible()) BottomBattleWindow::Hide();
        } else if (!BottomBattleWindow::IsVisible()) {
            BottomBattleWindow::Show();
        }
    }
    // The help that was running when the map was left goes on.
    if (GameState::GetCurrentMapID() == 0) {
        NeedItemWindow::RunLastHelpItem();
        NotEnoughWindow::RunLastHelpItem();
    }
    TaskListWindow::RunLastHelpItem();
    Tasks::RunLastSubTaskHelp();
    FoundItemsWindow::RunPostAction();
    SultanArrivingWindow::RunLastHelp();
    ChoosePathWindow::RunShowOnLoad();
    g_mapTimer = missionMap ? 360.f : 0.f;
    GameState::ResetTimeModifier();
    CreateBirds();
    GameState::SetBeltItemMode(GameState::IsArenaCombatActive());
    GameState::SetMapVisitTime(GameState::GetCurrentMapID(), (uint32_t)Timer::GetGlobalTime());
    g_loading = false;
    // OnMapLoaded @0x1b79c0 only reports some map ids to the online event log (OG2::MakeRequest).
    // The map version: a map from the save that is older than the game's is brought up to date; a
    // map from its file is the game's version.
    if (!g_versionUpdate) {
        g_versionUpdate = true;
        uint32_t map = GameState::GetCurrentMapID();
        if (!fromCache) {
            GameState::SetMapVersion(map, GameState::GetGameMapVersion());
        } else if (GameState::GetMapVersion(map) < GameState::GetGameMapVersion()) {
            std::printf("Map::Load() Updating user map %d from version %d to %d\n", (int)map,
                        (int)GameState::GetMapVersion(map), (int)GameState::GetGameMapVersion());
            if (map == 0) UpdateCityMap();
            else UpdateBattleMap();
            if (GameState::GetMapVersion(GameState::GetCurrentMapID()) == GameState::GetGameMapVersion())
                std::puts("Map::Load() Successfully updated user map");
            else std::fprintf(stderr, "Map::Load() Failed to update user map\n");
        }
        g_versionUpdate = false;
    }
    return true;
}

// @0x1b8514 Map::AddFog: one more covered spot (tile, size 2..4) of a map with its own fog.
void AddFog(int x, int y, int size) { g_fogMarkers.push_back({x, y, size, false}); }

// @0x1b8808
void ClearFog() {
    g_fogMarkers.clear();
    Fog::Deinit(false);
}

// @0x1b84d4 Map::RemoveFog: the fog opens 300 pixels round the middle of the world box.
void RemoveFog(float x0, float y0, float x1, float y1) { Fog::Open((x0 + x1) * 0.5f, (y0 + y1) * 0.5f, 300.f, 0.75f); }

void ToggleFogRender() { Fog::ToggleFogRender(); }   // @0x1b84d0
void StoreFogInfo() { Fog::StoreFogState(); }        // @0x1b88ac
void RestoreFogInfo() { Fog::RestoreFogState(); }    // @0x1b88a8

// @0x1c49a8 Map::UpdateCityMap: the city of an older map version is loaded again from the map file;
// every owned patch is saved alone first (its decorations, buildings and mask, then the end chunk)
// and LoadPlayer puts it back in place of the file's. (The sandbox mode skips the patches: online.)
namespace { void SavePatch(const Patch* p); }   // (below, with the map's save)
void UpdateCityMap() {
    if (GameState::GetCurrentMapID() != 0) {
        std::fprintf(stderr, "ERROR: Map::UpdateBattleMap() Cannot update battle map %d\n",
                     (int)GameState::GetCurrentMapID());
        return;
    }
    g_cityUpdateBlocks.clear();
    for (auto& p : g_patches) {
        if (!p->owned) continue;
        SaveManager::GetMainSave()->BeginData();
        SavePatch(p.get());
        SaveManager::BeginChunk(SaveManager::kEnd);
        SaveManager::SaveUnsigned(0);
        SaveManager::EndChunk();
        g_cityUpdateBlocks.emplace_back(p->areaId, SaveManager::GetMainSave()->GetAggregatedData());
    }
    SaveManager::GetMainSave()->ResetMap(0);
    Load(SaveManager::GetMapData(0), (uint32_t)Timer::GetGlobalTime(), false, false);
    g_cityUpdateBlocks.clear();
    Tasks::Update(true);
    HUDWindow::UpdateTasks();
    GameState::SetMapVersion(0, GameState::GetGameMapVersion());
    SafeSave();
}

// @0x1c4cfc Map::UpdateBattleMap: a campaign map of an older version is loaded again from its file;
// what the player did on it (decorations, spawn points, position, fog) is carried over.
void UpdateBattleMap() {
    uint32_t map = GameState::GetCurrentMapID();
    if (map == 0) {
        std::fprintf(stderr, "ERROR: Map::UpdateBattleMap() Cannot update city map %d\n", 0);
        return;
    }
    if (!GameState::IsCombatActive()) {
        StoreDecorationInfo();
        StoreSpawnpointInfo();
        StorePlayerPosition();
        StoreFogInfo();
        SaveManager::GetMainSave()->ResetMap(map);
        Load(SaveManager::GetMapData(map), (uint32_t)Timer::GetGlobalTime(), false, false);
        UpdateOfflineResources();
        Tasks::Update(true);
        HUDWindow::UpdateTasks();
        RestoreFogInfo();
    } else if (!Locations::GetMapBat(map)) {
        std::fprintf(stderr, "ERROR: Map::UpdateBattleMap() Cannot update map %d while the battle is active\n", map);
        return;
    }
    GameState::SetMapVersion(map, GameState::GetGameMapVersion());
}

// @0x1b6fe8
Decor* GetPortalToMap(uint32_t map) {
    for (auto& p : g_patches)
        for (auto& d : p->decors)
            if (d->portal && (uint32_t)d->portal->map == map) return d.get();
    return nullptr;
}

// @0x1b7874
void StorePlayerPosition() {
    g_storedPosition = true;
    Entity* player = EntityManager::GetPlayer();
    g_storedX = player->tileX;
    g_storedY = player->tileY;
}

// @0x1b73ec
void RestorePlayerPosition() {
    if (!g_storedPosition) return;
    g_startX = g_storedX;
    g_startY = g_storedY;
    g_storedPosition = false;
}

// @0x1b7d00
void PlayerDefaultSpawn() {
    Entity* player = EntityManager::GetPlayer();
    if (!player || !gameLoaded || g_mapId == 0 || !player->GetAI()) return;
    for (unsigned i = 0; SpawnPoint* s = GetSpawnByIndex(i); ++i) {
        if (!((s->clas == 5 || s->clas == 10) && s->entityId == 0xc)) continue;
        int x = -1, y = -1;
        Locations::GetPlayerDefaultPosition(GameState::GetCurrentMapID(), &x, &y);
        if (x == -1 || y == -1) {
            x = s->x;
            y = s->y;
        }
        g_startX = x;
        g_startY = y;
        Entity::UpdateSafePos(player);
        player->SetPos(x, y);
        player->GetAI()->UpdateLastTarget();
        return;
    }
}

// @0x1b747c
void PlayerNextSpawn(int x, int y) {
    g_nextSpawnX = x;
    g_nextSpawnY = y;
}

int GetLastPlayerX() { return g_startX; }   // @0x1b5f58
int GetLastPlayerY() { return g_startY; }   // @0x1b5f68

// @0x1b5f7c
void EnqueueLoadMap(int map, bool bossReward, bool save, float delay) {
    g_loadQueuedDelay = delay;
    g_loadQueuedMap = map;
    g_loadQueued = true;
    g_loadQueuedSave = save;
    g_bossReward = bossReward;
}

bool IsMapLoadQueued() { return g_loadQueued; }   // @0x1b5fb8
bool IsMapLoading() { return g_loading; }         // @0x1b68a4

// @0x1c2914. The spawn points' entities appear; the player stands on its spawn point's saved position, else at the
// map's default tile ((0x1b, 0xd) in the city, (10, 0x14) on map 0x60), facing down-right in the city.
void UpdateSpawns() {
    Entity* player = EntityManager::GetPlayer();
    if (player) {
        player->SetRetreating(false);
        if (player->GetSprite() && player->IsDead() && GameState::GetCurrentMapID() == 0)
            Render::ChangeLayer(player->GetSprite(), 8);
    }
    std::puts("Map::UpdateSpawns started");
    std::printf("Map::UpdateSpawns found %d spawns\n", (int)GetSpawnPointCount());
    RestoreSpawnpointInfo();
    RestorePlayerPosition();
    int defX = -1, defY = -1, defDir = -1;
    // The hero's own spawn point (class 5 or 10, entity 0xc) gives the default tile.
    auto heros = [](const SpawnPoint* s) { return (s->clas == 5 || s->clas == 10) && s->entityId == 0xc; };
    for (unsigned i = 0; SpawnPoint* s = GetSpawnByIndex(i); ++i) {
        if (!heros(s)) continue;
        defX = s->x;
        defY = s->y;
        if (player) {   // PORT: the original has a player on every map
            player->SetSpawnPoint(s);
            player->f38 = false;
            player->SetPos(g_startX, g_startY);
        }
        break;
    }
    // The others spawn their entities. (Not while GameState::g_Combat, 0x613548, is set: the siege system, dead in 5.11.)
    for (unsigned i = 0; SpawnPoint* s = GetSpawnByIndex(i); ++i) {
        if (heros(s)) continue;
        Entity* e = EntityManager::SpawnEntityAt(s, false, false);
        if (e->IsDead()) {   // lying dead
            e->SetAnimationOnce("death", false, false, true, true);
            e->SetAnimationFrame(e->GetAnimationFrameCount() - 1);
            if (e->GetSprite()) Render::ChangeLayer(e->GetSprite(), 6);
        }
        if (s->corpseTaken != 0) e->SetActive(false, true);
        // A PvP rival's stand-in just beaten (PlayerCompetitionWindow's defeated spawn) gets its
        // death time (a standing one also counts for subtask 0x3f / 0x51); the one the PvP fight
        // was started on (GameState::mBossSpawnID) gets the selected rival's gid as its GUID.
        if (!e->IsDead() && s->corpseTaken == 0 && !e->IsBoss() && e->IsPlayerShadow()) {
            if (PlayerCompetitionWindow::HasDefeatedPlayer(s->id)) {
                if (SpawnPoint* sp = e->GetSpawnPoint()) sp->deathTime = (uint32_t)Timer::GetGlobalTime();
                PlayerCompetitionWindow::SetDefeatedPlayer(0);
                if (!e->IsWanderingPlayerShadow()) Tasks::CompleteSubtask(0x3f, 0x51, 1);
                GameState::mBossSpawnID = 0;
            } else if (GameState::mBossSpawnID == (int)e->GetSpawnPointID()) {
                e->SetGUID((uint32_t)PlayerCompetitionWindow::GetSelectedPlayerGID());
                GameState::mBossSpawnID = 0;
            }
            continue;
        }
        if (!e->IsBoss()) continue;
        // A boss whose saved fight (GameState::mBossFights) has no HP left is dead, with its reward
        // and subtasks (0x1f the boss, 0x3d the spawn point, 0x3f its kind).
        if (!GameState::IsCombatActive() && !e->IsDead()) {
            const int key = (int)(s->id | (uint32_t)GameState::GetCurrentMapID() << 16);
            if (GameState::HasBossFight(key)) {
                unsigned hp = 0;
                for (const auto& m : GameState::BossFight(key)) hp += m.second;
                if (hp == 0) {
                    e->SetAnimationOnce("death", false, false, true, true);
                    e->SetHP(0);
                    Render::ChangeLayer(e->GetSprite(), 6);
                    DropBossReward(e, e->GetBossMapID());
                    Tasks::CompleteSubtask(0x1f, e->GetEntityData()->id, 1);
                    Tasks::CompleteSubtask(0x3d, e->GetSpawnPointID(), 1);
                    Tasks::CompleteSubtask(0x3f, e->GetEntityData()->enemyClass, 1);
                }
            }
        }
        // A won boss fight against this spawn point ends here: the boss lies dead where it fell, the
        // hero is back where he stood, and the victory pose follows (SpawnEntities).
        if (GameState::IsCombatActive() && GameState::GetActiveCombat()->GetType() > 2 &&
            GameState::GetActiveCombat()->KilledBoss((int)s->id)) {
            GameState::EndActiveCombat(false);
            g_startX = GameState::mLastBossPlayerXY[0];
            g_startY = GameState::mLastBossPlayerXY[1];
            defDir = GameState::mLastPlayerDir;
            e->SetPos(GameState::mLastBossXY[0], GameState::mLastBossXY[1]);
            e->SetAnimationOnce("death", false, false, true, true);
            e->SetHP(0);
            Render::ChangeLayer(e->GetSprite(), 6);
            g_victoryPose = true;
            // PORT (online removed): OG::MakeRequest(0, 0, boss id, -1, 0) and, for a spawn point,
            // OG2::MakeRequest(2, 1, spawn id, male, 1).
            DropBossReward(e, e->GetBossMapID());
            Tasks::CompleteSubtask(0x1f, e->GetEntityData()->id, 1);
            Tasks::CompleteSubtask(0x3d, e->GetSpawnPointID(), 1);
            Tasks::CompleteSubtask(0x3f, e->GetEntityData()->enemyClass, 1);
        }
        if (GameState::IsCombatActive() && GameState::GetActiveCombat()->GetType() > 4 &&
            GameState::GetActiveCombat()->KilledBoss((int)s->id)) {
            GameState::EndActiveCombat(false);
            Tasks::CompleteSubtask(0x3d, e->GetSpawnPointID(), 1);
        }
    }
    if (GetMapID() == 0) {
        defY = 0xd;
        defX = 0x1b;
        g_startX = g_startY = -1;
    }
    if (GetMapID() == 0x60) {
        defY = 0x14;
        defX = 10;
        g_startX = g_startY = -1;
    }
    // A boss or arena fight on its way (state 0x11) clears the start position too.
    if (BaseCombat* c = GameState::GetActiveCombat(); c && c->GetState() == 0x11) g_startX = g_startY = -1;
    if (g_nextSpawnX != 0 || g_nextSpawnY != 0) {   // PlayerNextSpawn's tile
        g_startX = g_nextSpawnX;
        g_startY = g_nextSpawnY;
        g_nextSpawnX = g_nextSpawnY = 0;
    }
    for (unsigned i = 0; Entity* e = EntityManager::EnumEntities(i); ++i) {
        int clas = e->GetEntityData()->clas;
        if (clas == 0x16) {   // a visited friend (online only)
            e->SetActive(true, true);
            AI::Waypoint* wp = AI::GetRandomFreeWPRect(0x14, 0x20, 0x14, 0);
            int x = wp ? wp->x : 0x1f, y = wp ? wp->y : 0xb;
            e->SetPos(x, y);
            EntityManager::SpawnEntityAt(e, (unsigned)x, (unsigned)y, false, false);
        }
        if (clas != 5) continue;
        if (g_startX < 1 || g_startY < 1) {
            e->SetPos(defX, defY);
        } else if (!AI::GetWaypoint(g_startX, g_startY, false)) {
            std::fprintf(stderr, "ERROR: Player is stuck inside a blocked tile at %d;%d", g_startX, g_startY);
            g_startY = defY;
            g_startX = defX;
            e->SetPos(defX, defY);
            e->GetAI()->UpdateLastTarget();
        } else {
            e->SetPos(g_startX, g_startY);
        }
        if (GetMapID() == 0) e->SetDirection(1);
        else if (defDir > 0) e->SetDirection(defDir);
        if (BaseSquad* sq = e->GetSquad()) sq->ClearDeadSoldiers();
    }
    if (g_bossReward) {   // the arena's: what was picked up on map 0x70 goes to the hero
        DropBossReward(player, 0x70);
        g_bossReward = false;
    }
}

// @0x1b98d8: what was picked up on the boss's map (resources, then items) drops where the boss lies;
// on the arena's map (0x70) it goes straight to the hero.
void DropBossReward(Entity* boss, uint32_t map) {
    if (map == 0x70) {
        FoundItemsWindow::GiveCollectedToPlayer(true, EntityManager::GetPlayer(), false);
    } else {
        for (int type = 0; type < 0xb; ++type)
            if (int n = GameState::GetMapResourceCollectionInfo(map, type))
                BuildingHovers::DropResource(boss->worldX, boss->worldY, type, (unsigned)n, false, false);
        for (unsigned i = 0; Items::ItemInfo* info = Items::EnumItemInfo(i); ++i)
            for (int n = GameState::GetMapItemCollectionInfo(map, info->id); n > 0; --n)
                BuildingHovers::DropItem(boss->worldX, boss->worldY, Items::GetItemInfo(info->id), 0, 1);
    }
    GameState::ResetMapCollectionInfo(map);
    // PORT (DECIDED user 2026-10-18): the boss gives the map's chest key with a 1/2 chance.
    if (map != 0x70)
        if (Items::ItemInfo* key = Items::GetItemInfo(RollHeldChestKey(2, true)))
            BuildingHovers::DropItem(boss->worldX, boss->worldY, key, 0, 1);
}

uint32_t RollHeldChestKey(unsigned oneIn, bool boss) {
    if (!boss)
        for (unsigned i = 0; Entity* e = EntityManager::EnumEntities(i); ++i)
            if (e->IsBoss()) return 0;
    // (A decoration's REQ_UNLOCK_ITEM counts before its unlock quest is done; a chest asks for its key.)
    uint32_t key = 0;
    for (auto& p : g_patches)
        for (auto& d : p->decors) {
            if (key || !d->meta) continue;
            const MetaData* m = d->meta->FindChildData(kExpReqUnlockItem);
            const uint32_t k = m ? (uint32_t)m->GetInt() : d->GetRequiredItem();
            if (Chests::IsChestKey(k)) key = k;
        }
    if (!key || Rand48::lrand48() % oneIn != 0) return 0;
    return key;
}

// @0x1c0460: the player and its squad appear at the player's tile, whose waypoint gets weight 5 (the
// others walk round it), and its spawn tile becomes the start position.
void SpawnEntities() {
    Entity* player = EntityManager::GetPlayer();
    // PORT: the original has a player on every map; saves the port wrote before milestone 4 have none.
    if (!player) return;
    // A boss or arena fight on its way (state 0x11, SwitchToBossFight & co.) is set up instead: the
    // old combat ends, the fight of mCurrentBossCombatType starts against the last entry of
    // mBossWaves (its hero; the PvP one is no player).
    if (BaseCombat* c = GameState::GetActiveCombat(); c && c->GetState() == 0x11) {
        GameState::EndActiveCombat(true);
        if (GameState::mCurrentBossCombatType == 3) GameState::SetCombat(new BossCombat());
        if (GameState::mCurrentBossCombatType == 4) GameState::SetCombat(new BossCombatWaves());
        if (GameState::mCurrentBossCombatType == 5) GameState::SetCombat(new ArenaCombat());
        if (GameState::mCurrentBossCombatType == 6) {
            PvP::PlayerInfo* opponent = PvP::GetPlayerInfoByGID(PlayerCompetitionWindow::GetSelectedPlayerGID(), 0xff);
            GameState::SetCombat(new PvPArenaCombat(opponent, false));
            Entity* hero = EntityManager::CreateEntity(GameState::mBossWaves.back(), true, true);
            hero->player = false;
            hero->CreateSquad();
            GameState::GetActiveCombat()->BeginCombat(player->GetSquad(), hero->GetSquad());
            return;
        }
        if (GameState::IsCombatActive()) {
            Entity* boss = EntityManager::CreateEntity(GameState::mBossWaves.back(), false, true);
            boss->CreateSquad();
            GameState::GetActiveCombat()->BeginCombat(player->GetSquad(), boss->GetSquad());
        }
        return;
    }
    if (AI::Waypoint* wp = AI::GetWaypoint(player->tileX, player->tileY, false)) {
        wp->weight = 5.f;
        player->GetSquad()->SpawnAt(wp);
        player->spawnX = g_startX;
        player->spawnY = g_startY;
    }
    if (g_victoryPose) {
        player->EnqueueAnimation("attack1", 1.f);
        player->EnqueueAnimation("bat_vict", 0.f);
    }
    g_victoryPose = false;
}

int GetBuildingCount(uint32_t id, bool built) {
    int n = 0;
    for (auto& p : g_patches)
        for (auto& b : p->buildings)
            if (b->id == id && (!built || b->needsBuilder == 0)) ++n;
    return n;
}

int GetBuildingMaxUpgrade(uint32_t id) {
    int best = 0;
    for (auto& p : g_patches)
        for (auto& b : p->buildings)
            if (b->id == id && best <= b->level) best = b->level + 1;
    return best;
}

int GetPendingWorkerCount() {
    int n = 0;
    for (auto& p : g_patches) {
        for (auto& b : p->buildings) {
            if (b->data->buildingClass == 4) continue;
            if (b->needsBuilder != 0) n += (int)b->data->givePopulation;
            if (b->upgrading != 0) n += b->GetNextUpgradeInfo().givePopulation;
        }
    }
    return n;
}

int GetUsedWorkerCount() {
    unsigned n = 0;
    for (auto& p : g_patches) {
        for (auto& b : p->buildings) {
            if (b->data->buildingClass == 4) continue;
            n += b->data->costPopulation;
            for (int i = 0; i < b->level - 1; ++i) n += (unsigned)b->data->upgrades[(size_t)i].population;
        }
    }
    return n > 4 ? (int)(n - 4) : 0;
}

// @0x1b8d30: the (last found) open tavern, building 0x8f.
Building* GetTavern() {
    Building* found = nullptr;
    for (auto& p : g_patches)
        for (auto& b : p->buildings)
            if (b->id == 0x8f && b->IsOpened()) found = b.get();
    return found;
}

// @0x1b8dd4: the (last found) open barracks, a building of class 0xc.
Building* GetBarracks() {
    Building* found = nullptr;
    for (auto& p : g_patches)
        for (auto& b : p->buildings)
            if (b->data->buildingClass == 0xc && b->IsOpened()) found = b.get();
    return found;
}

Building* GetBuildingWithID(uint32_t id) {
    for (auto& p : g_patches)
        for (auto& b : p->buildings)
            if (b->data->id == id) return b.get();
    return nullptr;
}

namespace {
std::vector<Building*> g_farmBuildings;   // 0x613818
}

void FillFarmBuildings() {
    g_farmBuildings.clear();
    for (auto& p : g_patches)
        for (auto& b : p->buildings)
            if (b->data->buildingClass == 0xd && b->IsOpened()) g_farmBuildings.push_back(b.get());
}

Building* GetFarmAfter(Building* b) {
    FillFarmBuildings();
    const std::vector<Building*>& f = g_farmBuildings;
    if (f.empty()) return nullptr;
    if (f.size() == 1) return f[0];
    if (f.back() == b) return f[0];
    for (size_t i = 0; i + 1 < f.size(); ++i)
        if (f[i] == b) return f[i + 1];
    return nullptr;
}

Building* GetFarmBefore(Building* b) {
    FillFarmBuildings();
    const std::vector<Building*>& f = g_farmBuildings;
    if (f.empty()) return nullptr;
    if (f.size() == 1) return f[0];
    if (f[0] == b) return f.back();
    for (size_t i = 1; i < f.size(); ++i)
        if (f[i] == b) return f[i - 1];
    return nullptr;
}

Building* GetUnfinishedBuildingWithID(uint32_t id, bool upgrading) {
    for (auto& p : g_patches)
        for (auto& b : p->buildings)
            if (b->data->id == id && (upgrading ? b->upgrading : b->needsBuilder) != 0) return b.get();
    return nullptr;
}

Building* GetUnfinishedBuildingedWithPopulation() {
    for (auto& p : g_patches)
        for (auto& b : p->buildings) {
            if (b->needsBuilder != 0 && b->data && b->data->givePopulation != 0) return b.get();
            if (b->upgrading != 0 && b->data && b->GetNextUpgradeInfo().givePopulation != 0) return b.get();
        }
    return nullptr;
}

Building* GetUpgradeableBuildingWithID(uint32_t id) {
    for (auto& p : g_patches)
        for (auto& b : p->buildings)
            if (b->data->id == id && b->IsOpened() && b->level != (int)b->data->upgrades.size()) return b.get();
    return nullptr;
}

bool ClickToBuyArea(int x, int y) {
    if (GameState::IsTutorial() || GameState::IsCityTutorial() || !GameState::IsPlayerCity()) return false;
    int wx = x, wy = y;
    MouseCoordinatesToWorld(wx, wy);
    for (auto& p : g_patches) {
        const Render::Sprite* s = p->sign;
        if (!s) continue;
        float fx = (float)wx, fy = (float)wy;
        if (fx < s->x || s->x + s->w < fx) continue;
        if (fy < s->y - s->h || s->y < fy) continue;
        LandWindow::SetAreaParameters(p->areaId);
        LandWindow::Show();
        return true;
    }
    return false;
}

void BuyArea(uint32_t areaId) {
    for (auto& p : g_patches)
        if (p->areaId == areaId) p->owned = true;
    UpdateAreaBorders();
    // OG::MakeRequest(0x13, 0xf, area, -1, 0): online, not ported.
}

void UpdateOfflineResources() {
    for (auto& p : g_patches)
        for (auto& b : p->buildings) b->UpdateOfflineResources();
}

// Puts the loaded entities back: workers to their workplace (which catches up on the time away,
// Building::UpdateOfflineState) or near their house, residents into their house's livers.
void AssignEntities() {
    for (unsigned i = 0; Entity* e = EntityManager::EnumEntities(i); ++i) {
        int clas = e->GetEntityData()->clas;
        if (clas == 5 || clas == 10 || clas == 0x16) {
            if (e->hasHome)
                if (Building* home = GetBuilding(e->homeX, e->homeY)) home->AssignLiver(e);
            if (e->workType > 0) {
                if (Building* work = GetBuilding(e->workX, e->workY)) {
                    e->SetOfflineMode(true);
                    EntityManager::SpawnEntityAt(e, (unsigned)e->workX, (unsigned)e->workY, false, false);
                    work->AssignWorker(e, 0);
                    e->SetOfflineMode(false);
                }
            }
            continue;
        }
        if (e->GetCurrentMap() != (int)GetMapID()) {
            e->SetActive(false, false);
            continue;
        }
        e->SetOfflineMode(true);
        int hx = e->homeX, hy = e->homeY;
        Building* home = GetBuilding(hx, hy);
        bool assigned = false;   // LAB_001b9eb0 reached through a job
        bool jobDone = false;    // LAB_001ba0d0
        if (e->hasHome && GetMapID() == 0) {
            e->SetHP(0x400);
            if (home) home->GetSpawnTile(hx, hy);
        }
        if (e->workType > 0) {
            if (e->workType == 1) {
                Building* work = GetBuilding(e->workX, e->workY);
                if (!work) {
                    e->workType = 0;
                    e->SetWorkplace(nullptr);
                    e->SetOfflineMode(false);
                    std::fprintf(stderr, "ERROR: Map::AssignEntities() building at %dx%d does not exist\n", e->workX, e->workY);
                    continue;
                }
                int tx = 0, ty = 0;
                work->GetWorkTile(tx, ty);
                EntityManager::SpawnEntityAt(e, (unsigned)tx, (unsigned)ty, false, false);
                work->UpdateOfflineState();
                uint32_t wc = work->data->buildingClass;
                bool farmer = false;
                if (wc == 4) {
                    jobDone = work->resourceLeft == 0;
                } else if (wc == 0xc && clas == 10) {
                    jobDone = !work->HasActiveContract();
                } else if (wc == 0xd && clas == 2) {
                    farmer = true;
                } else {
                    jobDone = work->IsOpened();
                }
                if (farmer) {
                    // The farmer is the farm's worker and lives there (its home is the farm).
                    if (!work->IsOpened()) e->Disappear();
                    if (!work->workers.empty()) work->workers[0] = e;
                    if (home) {   // PORT: guarded (the original uses the home unchecked)
                        home->AssignLiver(e);
                        e->SetWorkplace(home);
                        e->GetAI()->AssignToJob(home);
                    }
                } else if (!jobDone) {
                    work->AssignWorker(e, 0);
                }
            } else {
                // A decoration's job goes on from its action point unless it ended meanwhile.
                Decor* job = GetDecoration(e->workX, e->workY);
                int ax = e->workX, ay = e->workY;
                bool goesOn = false;
                if (job) {
                    job->GetActionPoint(ax, ay, 0);
                    job->UpdateOfflineState();
                    goesOn = !job->WorkFinished();
                }
                if (goesOn) {
                    EntityManager::SpawnEntityAt(e, (unsigned)ax, (unsigned)ay, false, false);
                    job->AssignWorker(e);
                } else {
                    e->workType = 0;
                    e->SetWorkplace(nullptr);
                    e->SetWorkplaceDecoration(nullptr);
                    e->SetCurrentMap(0);
                    if (GetMapID() == 0) EntityManager::SpawnEntityAt(e, (unsigned)hx, (unsigned)hy, false, false);
                }
            }
            if (jobDone) {
                e->workType = 0;
                e->SetPos(hx, hy);
                e->GetAI()->UpdateLastTarget();
            }
            assigned = true;
        }
        if (!assigned) {
            AI::Waypoint* wp = AI::GetWaypoint(hx, hy, false);
            if (!home) {
                uint32_t id = e->GetEntityData()->id;
                if (id == 6 && e->GetHP() == 0) {
                    std::fprintf(stderr, "ERROR: Map::AssignEntities() Skipping tutorial farmer entity");
                    continue;
                }
                if (id != 0x133) {
                    std::fprintf(stderr, "CRITICAL ERROR: Map::AssignEntities() Entity %d without a home is placed at "
                                 "the city map at unexisting home %d; %d", id, hx, hy);
                    continue;
                }
                std::fprintf(stderr, "ERROR: Map::AssignEntities() Found a goblin without work at map %d", e->GetCurrentMap());
                if (e->GetCurrentMap() != 0) {
                    e->SetCurrentMap(0);
                    continue;
                }
                home = GetBuildingWithID(0x12);
                if (!home) {
                    std::fprintf(stderr, "ERROR: Map::AssignEntities() Goblin without a home at home map cannot be "
                                 "assigned to any stockpile");
                    continue;
                }
                e->homeX = home->x;
                e->homeY = home->y;
            }
            if (!wp || EntityManager::GetEntityAtXY(hx, hy) || GetBuilding(hx, hy)) {
                int tries = home->data->w * home->data->h;
                AI::Waypoint* near = nullptr;
                while (tries != 0) {
                    near = AI::GetWaypointNearBuilding(home, false);
                    if (near && (EntityManager::GetEntityAtXY(near->x, near->y) || GetBuilding(near->x, near->y) ||
                                 GetDecoration(near->x, near->y)))
                        near = nullptr;
                    --tries;
                    if (near) break;
                }
                // (the original tests the remaining tries, not the waypoint: one found on the last
                // try is not used)
                if (tries == 0) {
                    int sx = 0, sy = 0;
                    home->GetStartTile(sx, sy);
                    EntityManager::SpawnEntityAt(e, (unsigned)sx, (unsigned)sy, false, false);
                } else {
                    EntityManager::SpawnEntityAt(e, (unsigned)near->x, (unsigned)near->y, false, false);
                }
            } else {
                EntityManager::SpawnEntityAt(e, (unsigned)hx, (unsigned)hy, false, false);
            }
            if (home->data->buildingClass != 7) {
                if (home->liverAway) e->SetActive(false, false);
                else home->liverAway = true;
            }
        }
        if (e->hasHome && GetMapID() == 0 && clas != 2 && home) {
            home->AssignLiver(e);
            e->SetHome(home);
        }
        e->SetOfflineMode(false);
    }
    // In the city a farm without its farmer takes an entity living at it without a job, else a
    // farmer (ids 6, 7) living at it, taken from any other job.
    if (GameState::GetCurrentMapID() != 0) return;
    for (auto& p : g_patches) {
        for (auto& bp : p->buildings) {
            Building* b = bp.get();
            if (b->data->buildingClass != 0xd || b->workers.empty() || b->workers[0]) continue;
            Entity* e = nullptr;
            for (unsigned i = 0; (e = EntityManager::EnumEntities(i)) != nullptr; ++i)
                if (e->workX == 0 && e->workY == 0 && e->homeX == b->x && e->homeY == b->y) break;
            if (e) {
                int tx = 0, ty = 0;
                b->GetWorkTile(tx, ty);
                EntityManager::SpawnEntityAt(e, (unsigned)tx, (unsigned)ty, false, false);
            } else {
                for (unsigned i = 0; (e = EntityManager::EnumEntities(i)) != nullptr; ++i) {
                    uint32_t id = e->GetEntityData()->id;
                    if ((id == 6 || id == 7) && e->homeX == b->x && e->homeY == b->y) break;
                }
                if (!e) continue;
                if (Building* w = GetBuilding(e->workX, e->workY)) w->RemoveWorker(e);
                int tx = 0, ty = 0;
                b->GetWorkTile(tx, ty);
                e->SetPos(tx, ty);
            }
            b->workers[0] = e;
            b->AssignLiver(e);
            e->SetWorkplace(b);
            e->GetAI()->AssignToJob(b);
            e->SetOfflineMode(false);
        }
    }
}

// ------------------------------------------------------------------------------------- saving
namespace {
using SaveManager::BeginChunk;
using SaveManager::EndChunk;
using SaveManager::SaveChar;
using SaveManager::SaveShort;
using SaveManager::SaveUnsigned;

void SaveString(const std::string& s) {   // a short length and the characters
    SaveShort((int16_t)s.size());
    for (char c : s) SaveChar((uint8_t)c);
}

// @0x1e1f50 Map::Patch::SaveBuilding: chunk 0xc, the farm chunks 0x2d, 0x2f and 0x31 (class 0xd)
// and the unique id (0x50).
void SaveBuilding(const Building* b) {
    BeginChunk(SaveManager::kBuilding);
    SaveChar(b->x);
    SaveChar(b->y);
    SaveUnsigned(b->id);
    SaveUnsigned(b->stateTime);
    SaveUnsigned(b->buildLeft > 0.0 ? (uint32_t)(int64_t)b->buildLeft : 0);
    SaveUnsigned(b->f54);
    SaveChar(b->mirrored);
    SaveChar(0);
    SaveChar((uint8_t)b->level);
    SaveChar(0);
    SaveChar(0);
    SaveChar(0);
    SaveUnsigned(0);
    SaveUnsigned(0);
    SaveChar((uint8_t)b->built);
    SaveChar((uint8_t)b->upgrading);
    SaveUnsigned(b->ff8);
    SaveUnsigned(b->lastGather);
    SaveUnsigned((uint32_t)b->resourceLeft);
    SaveChar(0xb);
    for (int v : b->resources) SaveShort((int16_t)v);
    SaveChar((uint8_t)b->resourceState);
    SaveUnsigned(b->growStart);
    SaveChar((uint8_t)b->f10c);
    SaveUnsigned(b->f110);
    SaveUnsigned(b->f114);
    SaveString(b->resourceText);
    SaveString(b->metaText);
    SaveUnsigned((uint32_t)b->contract);
    EndChunk();
    if (b->data->buildingClass == 0xd) {
        BeginChunk(0x2d);
        for (uint32_t v : b->patchStart) SaveUnsigned(v);
        EndChunk();
        BeginChunk(0x2f);
        for (int v : b->patchContract) SaveUnsigned((uint32_t)v);
        EndChunk();
        BeginChunk(0x31);
        for (uint32_t v : b->patchArg) SaveUnsigned(v);
        EndChunk();
    }
    if (b->uniqueId == 0) return;
    BeginChunk(0x50);
    SaveUnsigned(b->uniqueId);
    EndChunk();
}

// @0x1e21f0 Map::Patch::Save: the decorations (not the random ones), the buildings and, off the
// city, the walk mask. (Removed decorations leave the original's list; the port keeps them
// flagged.)
void SavePatch(const Patch* p) {
    BeginChunk(SaveManager::kDecorCount);
    int16_t n = 0;
    for (auto& d : p->decors) if (!d->fake && !d->removed) ++n;
    SaveShort(n);
    EndChunk();
    for (auto& d : p->decors) {
        if (d->fake || d->removed) continue;
        BeginChunk(SaveManager::kDecor);
        SaveChar(d->x);
        SaveChar(d->y);
        SaveUnsigned(d->id);
        SaveChar((uint8_t)((d->hidden ? 2 : 0) | (d->mirrored ? 1 : 0)));
        SaveUnsigned(d->collectStart);
        SaveChar((uint8_t)d->f4c);
        SaveUnsigned(d->f50);
        SaveUnsigned(d->f54);
        SaveString(d->resourceText);
        SaveString(d->metaText);
        EndChunk();
    }
    BeginChunk(SaveManager::kBuildingCount);
    SaveShort((int16_t)p->buildings.size());
    EndChunk();
    for (auto& b : p->buildings) SaveBuilding(b.get());
    BeginChunk(SaveManager::kPatchMask);
    if (GetMapID() == 0) {
        SaveShort(0);
    } else {
        SaveShort((int16_t)p->mask.size());
        if (!p->mask.empty())
            for (int x = 0; x < GetGridWidth(); ++x)
                for (int y = 0; y < GetGridHeight(); ++y) SaveChar(GetBlock(x, y) ? '1' : '0');
    }
    EndChunk();
}
}  // namespace

// PORT: Expansions::IsRequiredUpdateRunning is false (every pack ships with the port) and the
// flag 0x6137f8 that blocks the save is never set.
bool SaveMap() {
    std::puts("Map::SaveMap() Began save");
    if (GameState::GetCurrentMapID() == 0) {
        size_t buildings = 0;
        for (auto& p : g_patches) buildings += p->buildings.size();
        if (buildings == 0) {
            std::fprintf(stderr, "ERROR: Map::SaveMap() Cannot save town map - no buildings");
            return false;
        }
    }
    SaveManager::GetMainSave()->BeginData();
    BeginChunk(SaveManager::kHeader);
    SaveUnsigned(SaveManager::GetCurrentVersion());
    SaveUnsigned(g_mapId);
    SaveChar((uint8_t)g_gridW);
    SaveChar((uint8_t)g_gridH);
    // The player's position (EntityManager::GetPlayer +0x54/+0x58). PORT: a save the port wrote
    // before milestone 4 has no player; then the loaded position is kept.
    if (Entity* player = EntityManager::GetPlayer()) {
        g_startX = player->spawnX;
        g_startY = player->spawnY;
    }
    SaveChar((uint8_t)g_startX);
    SaveChar((uint8_t)g_startY);
    SaveChar((uint8_t)g_tileset);
    SaveChar((uint8_t)g_patches.size());
    EndChunk();
    for (auto& p : g_patches) {
        BeginChunk(SaveManager::kPatch);
        SaveChar((uint8_t)p->areaId);
        SaveChar(p->owned);
        SaveChar(p->bordered);
        EndChunk();
        SavePatch(p.get());
    }
    SaveSpawnPoints();
    SavePortals();
    BeginChunk(SaveManager::kFog);
    SaveShort((int16_t)g_fogMarkers.size());
    for (const FogMarker& m : g_fogMarkers) {
        SaveChar((uint8_t)m.x);
        SaveChar((uint8_t)m.y);
        SaveChar((uint8_t)m.size);
    }
    EndChunk();
    Fog::SaveFog();
    BeginChunk(SaveManager::kEnd);
    SaveUnsigned(0);
    EndChunk();
    SaveManager::GetMainSave()->EndMapData(g_mapId);
    std::puts("Map::SaveMap() Finished save");
    return true;
}

void SaveState() {
    std::puts("Map::SaveState() Began save");
    SaveManager::GetMainSave()->BeginData();
    GameState::Save(SaveManager::GetCurrentVersion());
    SaveManager::GetMainSave()->EndGameStateData();
    std::puts("Map::SaveState() Finished save");
}

// (LoadSampling's steps are a load-time profiler: not ported.)
void Save(int type) {
    if (GameState::IsPvPTutorial() || GameState::IsTameTutorial()) return;
    int gold = SystemFuncs::GetSetting_Int("pre_resetgame_gold", 0, "settings");
    int crystals = SystemFuncs::GetSetting_Int("pre_resetgame_crystals", 0, "settings");
    bool resetCurrency = !GameState::IsTutorial() && (gold > 0 ? gold : crystals) > 0;
    if (GameState::GetCurrentMapID() == 0) {
        GameState::ClearOfflineBuildings();
        for (auto& p : g_patches) {
            for (auto& bp : p->buildings) {
                Building* b = bp.get();
                if (b->data->buildingClass == 0xc && b->HasActiveContract()) b->OnContractCompleted(true);
                GameState::OfflineBuilding o;
                o.id = b->id;
                o.level = (uint32_t)b->level;
                o.contract = (uint32_t)b->contract;
                o.flags = GameState::GetCurrentMapID();
                if (b->IsOpened()) o.flags |= 1;
                if (b->needsBuilder) o.flags |= 2;
                if (b->upgrading) o.flags |= 4;
                o.stateTime = b->stateTime;
                // (+0x14 and +0x15 are left unset on the original's stack)
                o.x = b->x;
                o.y = b->y;
                GameState::AddOfflineBuilding(o);
            }
        }
    }
    // The hero's job on a decoration ends with the save: its price comes back.
    if (Entity* player = EntityManager::GetPlayer())
        if (Decor* d = player->GetWorkplaceDecoration()) {
            d->ReturnCost();
            d->RemoveWorker();
        }
    SaveState();
    bool ok = SaveMap();
    if (type == 1) {
        // the online save (SendPlayerData2, SendPlayerItems, SaveOnlineSave): not ported
    } else {
        SaveManager::Save(nullptr);
    }
    if (ok && resetCurrency) {
        SystemFuncs::SetSetting_Int("pre_resetgame_gold", 0, "settings");
        SystemFuncs::SetSetting_Int("pre_resetgame_crystals", 0, "settings");
    }
}

void SafeSave() {
    BuildingHovers::HideWorldDialog(nullptr);
    TaskCompleteWindow::Hide();
    BuildingHovers::CollectAll();
    BuildingMovement::Decline();
    BuildingPlacement::Decline();
    Save(0);
}


}  // namespace Map

namespace FirstCityEnterEffect {

double time = 0.0;       // FirstCityEnterEffect::time 0x6138e8
double lastTime = 0.0;   // FirstCityEnterEffect::lastTime 0x6138f0

// @0x1bae50 FirstCityEnterEffect::Run: the world map's tutorial walk to the castle runs as a
// blocking loop of frames for 2 s (1 s when the next map is 0x91), unless the app is suspended.
void Run() {
    time = 0.0;
    lastTime = Timer::GetTime();
    GlobalMapWindow::CenterOnMap(0);
    GlobalMapWindow::ShowTutorialAnimation(2.f, false, Map::g_loadQueuedDelay);
    while ((Map::g_loadQueuedMap == 0x91 ? 1.0 : 2.0) > time && !renderSuspendEvent) {
        double dt = Timer::GetTime() - lastTime;
        dt = (double)HUDWindow::GetDeltaTimeMultiplier() * dt;
        time += dt;
        lastTime = Timer::GetTime();
        GlobalMapWindow::Update((float)dt);
        Render::Update();
    }
}

}  // namespace FirstCityEnterEffect
