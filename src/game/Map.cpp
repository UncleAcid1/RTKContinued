#include "game/Map.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "engine/FileManager.h"
#include "engine/Render.h"
#include "engine/Resources.h"
#include "engine/SystemFuncs.h"
#include "engine/Timer.h"
#include "game/AI.h"
#include "game/AIState.h"
#include "game/EntityData.h"
#include "game/Background.h"
#include "game/BuildingHovers.h"
#include "game/BuildingPlacement.h"
#include "game/BuildingMovement.h"
#include "game/Entity.h"
#include "game/EntityManager.h"
#include "game/GameData.h"
#include "game/Setting.h"
#include "game/GameState.h"
#include "game/Rand48.h"
#include "game/SaveManager.h"
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
std::vector<Render::Sprite*> g_borderSprites;  // dark grass, posts, signs
uint32_t g_currentTime = 0;                     // 0x6118c4
bool g_loaded = false;                          // 0x613798 (set at the end of Map::Load)
int g_lastWorldX = 0, g_lastWorldY = 0;         // Map::lastWorldX/Y 0x6138dc 0x6138e0
int g_owned[4] = {};                            // 0x613660 owned extent: min x, min y, max x, max y
Building* g_currentFarm = nullptr;              // 0x6136b8 the farm shown (Map::ShowFarm, 3f)
std::vector<std::pair<uint8_t, uint8_t>> g_blockedTiles;   // 0x61379c tiles blocked after load
int g_startX = -1, g_startY = -1;               // 0x60eff0 0x60eff4 the player's saved position
std::vector<SaveManager::Chunk> g_otherChunks;  // the map chunks not loaded yet (spawns, portals, fog)

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
    // UNVERIFIED (milestone 4): a decoration type without hit points (+0x7c) gets 50, on the type and
    // the decoration (+0xb8, +0xbc).
    uint8_t flags = r.u8();
    d->hidden = (flags & 2) != 0;
    d->mirrored = (flags & 1) != 0;
    if ((city && GameState::tutorial == 0x17) || (city && !p->owned)) d->hidden = true;
    d->collectStart = r.u32();
    d->f4c = r.u8();
    d->f50 = r.u32();
    d->f54 = r.u32();
    d->resourceText = r.str16();   // UNVERIFIED: not parsed into the per-resource amounts (+0x5c) yet
    d->metaText = r.str16();       // UNVERIFIED (milestone 4): MetaExpression not ported; kept as text
    if (!d->metaText.empty() && city) d->hidden = false;
    d->UpdateImage();
    d->UpdateMapLink();
    // UNVERIFIED (milestone 4): decorations with a meta expression (+0x20) or hit points (+0xb8,
    // DecorData +0x7c) register too.
    if (d->f4c != 0 || (d->data && d->data->collectTime != 0)) BuildingHovers::RegisterDecoration(d.get());
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
        // UNVERIFIED: the decoration also takes the building's timer, resources and meta expression.
        auto dec = std::make_unique<Decor>();
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
    if (!tex) return;      // the original shows a debug placeholder texture here
    const GameData::DecorData& D = *d->data;
    Render::Sprite* s = Render::CreateSprite(tex, DecorLayer(D.layer), d->mirrored, false);
    d->sprite = s;
    if (tex->frames > 0) Render::SetFrame(s, tex->h / tex->frames, 0);  // BaseAnimController, frame 0
    float wx = (float)(d->x * 84) + ((d->y & 1) ? 42.f : 0.f);
    float wy = (float)d->y * 42.f * 0.5f;
    int w = D.w, h = D.h;
    float X, Y;
    if (!d->mirrored) {
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
    float half = s->w * 0.5f;
    // UNVERIFIED: GetSpriteZ's third argument (r2) is not set by this caller; it only adds a
    // (c % 5) / 10000 tie-break. Passed as 0.
    float z = GetSpriteZ(wy, half, 0);
    Render::SetPosition(s, X - (float)(int)half, Y, z);
    s->visible = d->visible;
}


// MapObject::GetStartTile @0x1c7658 (through Decor::GetData @0x131234): back half the width (the
// height when mirrored) along the row.
const GameData::DecorData* Decor::GetData() const {
    if (!data) data = GameData::GetDecoration(id);
    return data;
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

void Decor::SpeedupDecoration() { collectStart = Timer::GetGlobalTime() + ~f50; }

void Decor::ReplaceWith(uint32_t newId) {
    if (GameState::GetCurrentMapID() == 0 && GameData::GetBuilding(newId)) {
        // UNVERIFIED (milestone 4): Map::EnqueueBuildingPlacement(x, y, id) turns it into a building.
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

int GetGridWidth() { return g_gridW; }
int GetGridHeight() { return g_gridH; }
uint32_t GetMapID() { return g_mapId; }
int GetTileset() { return g_tileset; }

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
    Cell* c = At(x, y);
    return c && c->block;
}

void SetBlock(int x, int y, bool block) {
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

// Decor::Contains @0x1344b0. UNVERIFIED (milestones 3-4): quest decorations (MetaExpression +0x20),
// portals (+0x90) and decorations with a worker (+0x4c) always count; none exist in the port yet.
bool DecorContains(const Decor* d, int x, int y, bool any) {
    if (!d->visible || !d->sprite || !Render::HasPixelAt(d->sprite, (float)x, (float)y)) return false;
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
    }
    // UNVERIFIED (milestone 4): other maps create waypoints from the first patch's walkable tile
    // list (+0xcc, 0xc bytes each, skipped when +8 is set).
    AI::LinkAdjacentWaypoints();
}

void Update(double dt) {
    // UNVERIFIED (milestones 4-5): the 0.1 s visibility pass on campaign maps (fog) and the cloud,
    // weather and camera-path parts that follow the object updates.
    g_currentTime = Timer::GetGlobalTime();   // SetCurrentTime @0x130f2c (0x6118c4)
    for (auto& p : g_patches) {
        for (size_t i = 0; i < p->buildings.size(); ++i) p->buildings[i]->Update(dt);
        // UNVERIFIED (milestone 3): Decor::Update (resource decorations, timers) and the removal of
        // decorations that flag themselves (+0x40) afterwards.
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

void InterruptCamera() {}
bool IsCameraMoving() { return false; }   // UNVERIFIED: GameState::IsPaused path (unlocks the GUI)

float GetSpriteZ(float a, float b, int c) {
    float v = (a + b * -0.25f) / 15.f / 1000.f;
    v = (float)(c % 5) / -10000.f + v;
    return v <= 0.2f ? 0.5f - v : 0.3f;
}

void Free() {
    AI::FreeWaypoints();
    for (auto& p : g_patches) {
        for (auto& b : p->buildings) BuildingHovers::UnregisterBuilding(b.get());
        for (auto& d : p->decors) {
            BuildingHovers::UnregisterDecoration(d.get());   // ~Decor
            if (d->sprite) Render::RemoveSprite(d->sprite);
        }
    }
    g_patches.clear();
    for (auto* s : g_borderSprites) Render::RemoveSprite(s);
    g_borderSprites.clear();
    g_grid.clear();
}

bool Load(SaveManager::SaveBlock* block, uint32_t time) {
    Free();
    Rand48::srand48((long)time);
    g_loaded = false;
    g_blockedTiles.clear();
    if (!block) return false;
    block->next = 0;
    auto next = [&](uint32_t type) -> const SaveManager::Chunk* {  // SkipToChunk + BeginChunkLoading
        block->SkipToChunk(type);
        return block->NextChunkID() == type ? &block->GetChunk() : nullptr;
    };
    // LoadPlayer: header chunk 0xe
    const SaveManager::Chunk* hc = next(SaveManager::kHeader);
    if (!hc) return false;
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
    for (int i = 0; i < patchCount; ++i) {
        const SaveManager::Chunk* pc = next(SaveManager::kPatch);
        if (!pc) break;
        auto p = std::make_unique<Patch>();
        p->areaId = pc->data.size() > 0 ? pc->data[0] : 0;
        p->owned = pc->data.size() > 1 && pc->data[1] != 0;
        p->bordered = pc->data.size() > 2 && pc->data[2] != 0;
        GameState::AreaInfo a;
        if (GameState::GetAreaInfo(p->areaId, a)) {
            p->x = a.x; p->y = a.y; p->w = a.w; p->h = a.h;
            p->buyable = a.a;   // UNVERIFIED: "a" mapped to Patch+0xa8 (used by UpdateAreaBorders)
        } else if (patchCount == 1) {  // single-patch maps (AddOnePatch): whole grid
            p->x = 0; p->y = 0; p->w = g_gridW; p->h = g_gridH;
        }
        Patch* raw = p.get();
        g_patches.push_back(std::move(p));
        // Patch::Load: LoadDecors (5 + n*0xb), LoadBuidings (6 + n*0xc), then mask (7)
        if (const SaveManager::Chunk* dc = next(SaveManager::kDecorCount)) {
            int cnt = SaveManager::Reader(*dc).s16();
            for (int k = 0; k < cnt; ++k) if (auto* c = next(SaveManager::kDecor)) LoadDecors(raw, *c);
        }
        if (const SaveManager::Chunk* bc = next(SaveManager::kBuildingCount)) {
            int cnt = SaveManager::Reader(*bc).s16();
            for (int k = 0; k < cnt; ++k) {
                block->SkipToChunk(SaveManager::kBuilding);
                if (block->NextChunkID() != SaveManager::kBuilding) continue;
                LoadBuilding(raw, block);   // the chunk, then its optional followers
            }
        }
        if (const SaveManager::Chunk* mc = next(SaveManager::kPatchMask)) {
            SaveManager::Reader mr(*mc);
            raw->mask = mr.str16();
            if (g_mapId != 0 && (int)raw->mask.size() == g_gridW * g_gridH) {
                for (int x = 0; x < g_gridW; ++x)
                    for (int y = 0; y < g_gridH; ++y) At(x, y)->block = raw->mask[(size_t)x * g_gridH + y] == '1';
            }
        }
    }
    // PORT (milestone 4): spawn points (2, 9 and followers), portals (3, 10) and the fog (0x17, 0x30)
    // are not loaded yet; their chunks are kept for SaveMap.
    g_otherChunks.clear();
    while (block->NextChunkID() != SaveManager::kEnd) g_otherChunks.push_back(block->GetChunk());
    // Map::Load: CreateRandomDecors, UpdateAreaBorders (spawns/portals/fog not rendered yet)
    for (auto& p : g_patches) AddRandomDecors(p.get());
    UpdateAreaBorders();
    Rand48::srand48((long)time);
    std::printf("Map %u: %dx%d tileset %d, %zu patches, %zu sprites\n", g_mapId, g_gridW, g_gridH, g_tileset,
                g_patches.size(), Render::SpriteCount());
    CreateRoadAI();
    if (GameState::GetCurrentMapID() == 0) UpdateStorageMax();
    // (ProcessOfflineContracts is empty.) UNVERIFIED (milestone 4): GameState::RefreshOfflineGoblins
    // (class 0x10 goblins away on a job), portals, spawns, the player's OnSetup and the static
    // meta expressions.
    AssignEntities();
    g_loaded = true;
    // (Map::ExecuteStaticMeta runs with AI::allowWaypointLink off; then every link is redone)
    AI::LinkAdjacentWaypoints();
    return true;
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

Building* GetBuildingWithID(uint32_t id) {
    for (auto& p : g_patches)
        for (auto& b : p->buildings)
            if (b->data->id == id) return b.get();
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
                    // UNVERIFIED (3f): the farmer stays the farm's worker (+0x124) and lives there.
                    if (!work->IsOpened()) e->Disappear();
                    if (!work->workers.empty()) work->workers[0] = e;
                    if (home) {
                        home->AssignLiver(e);
                        e->SetWorkplace(home);
                        e->GetAI()->AssignToJob(home);
                    }
                } else if (!jobDone) {
                    work->AssignWorker(e, 0);
                }
            } else {
                // UNVERIFIED (milestone 4, decoration jobs): a decoration's job continues if not
                // finished (Decor::GetActionPoint, UpdateOfflineState, WorkFinished, AssignWorker);
                // until then the job ends as for a missing decoration.
                e->workType = 0;
                e->SetWorkplace(nullptr);
                e->SetWorkplaceDecoration(nullptr);
                e->SetCurrentMap(0);
                if (GetMapID() == 0) EntityManager::SpawnEntityAt(e, (unsigned)hx, (unsigned)hy, false, false);
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
    // UNVERIFIED (3f): in the city a farm (class 0xd) without its farmer takes the farmer (ids 6
    // and 7) living at it.
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
    // The player's position (EntityManager::GetPlayer +0x54/+0x58). PORT (milestone 4): with no
    // player in the port the loaded position is kept.
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
    // SaveSpawnPoints, SavePortals, the fog list (0x17) and Fog::SaveFog (0x30). PORT (milestone 4):
    // the chunks as loaded (a map from its file has no 0x30 chunk until the fog is ported).
    for (const SaveManager::Chunk& c : g_otherChunks) {
        BeginChunk(c.type);
        for (uint8_t v : c.data) SaveChar(v);
        EndChunk();
    }
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
    // UNVERIFIED (milestone 4): the player's decoration job ends (Decor::ReturnCost, RemoveWorker).
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

// UNVERIFIED (milestone 3e/4): the world dialog, TaskCompleteWindow and the building placement
// and movement modes are not ported; their closing is skipped.
void SafeSave() {
    // UNVERIFIED (milestone 4): BuildingHovers::HideWorldDialog and TaskCompleteWindow::Hide first.
    BuildingHovers::CollectAll();
    BuildingMovement::Decline();
    BuildingPlacement::Decline();
    Save(0);
}


}  // namespace Map
