#include "game/Map.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "engine/FileManager.h"
#include "engine/Render.h"
#include "engine/Resources.h"
#include "engine/Timer.h"
#include "game/Background.h"
#include "game/GameData.h"
#include "game/Setting.h"
#include "game/GameState.h"
#include "game/Rand48.h"
#include "game/SaveManager.h"

namespace Map {
namespace {

struct Cell {               // grid cell, 0x10 bytes on the original
    Decor* decor = nullptr;        // +0x04 / +0x08 (SetDecoration)
    Building* building = nullptr;  // +0x00 (SetBuilding)
    bool block = false;            // SetBlock (walk mask)
};

uint32_t g_mapId = 0;
int g_gridW = 0, g_gridH = 0, g_tileset = 0;
std::vector<Cell> g_grid;
std::vector<std::unique_ptr<Patch>> g_patches;
std::vector<Render::Sprite*> g_borderSprites;  // dark grass, posts, signs
uint32_t g_currentTime = 0;                     // 0x6118c4

Cell* At(int x, int y) {
    if ((unsigned)x >= (unsigned)g_gridW || (unsigned)y >= (unsigned)g_gridH) return nullptr;
    return &g_grid[(size_t)y * g_gridW + x];
}

// Tile walk shared by Decor::UpdateMapLink @0x1346b8, Map::RemoveDecoration @0x1bb148 and
// Building::LinkBaseToBuilding @0x11e4a4 (start tile = MapObject::GetStartTile @0x1c7658).
template <class Fn>
void ForEachFootprintTile(int bx, int by, int w, int h, Fn&& fn) {
    unsigned x = (unsigned)bx, y = (unsigned)by;
    for (int i = 0; i < w / 2; ++i) {
        if (((unsigned)(by + i) & 1) == 0) x -= 1;
    }
    y += (unsigned)(w / 2);
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

int DecorLayer(int layer) {  // Decor::UpdateImage: table at 0x57b79c for layer+1 in [0,4], else 8
    static const int table[5] = {11, 8, 3, 3, 2};
    unsigned i = (unsigned)(layer + 1);
    return i < 5 ? table[i] : 8;
}

// ------------------------------------------------------------------------------- decorations
// @0x1346b8 Map::Decor::UpdateMapLink (home-map fake/real precedence included)
void DecorUpdateMapLink(Decor* d) {
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
void DecorUpdateImage(Decor* d) {
    if (d->sprite) { Render::RemoveSprite(d->sprite); d->sprite = nullptr; }
    if (!d->data) return;  // UNVERIFIED: decor ids that resolve to buildings (else-branch) not ported
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
void LoadDecors(Patch* p, SaveManager::Chunk& c) {  // @0x1e271c, one chunk 0xb
    SaveManager::Reader r(c);
    auto d = std::make_unique<Decor>();
    d->patch = p;
    d->x = r.u8();
    d->y = r.u8();
    d->id = r.u32();
    d->data = GameData::GetDecoration(d->id);
    uint8_t flags = r.u8();
    d->mirrored = (flags & 1) != 0;
    // remaining fields (timers, resources, meta expression) are gameplay state: not needed yet
    DecorUpdateImage(d.get());
    DecorUpdateMapLink(d.get());
    p->decors.push_back(std::move(d));
}

// @0x1e2bdc Map::LoadBuidings, one building: chunk 0xc, then the optional 0x2d (farm patch start
// times), 0x2f (farm patch contracts), 0x31 (farm patch data) and 0x50 (unique id) chunks.
void LoadBuilding(Patch* p, std::vector<SaveManager::Chunk>& chunks, size_t& ci) {
    SaveManager::Reader r(chunks[ci++]);
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
        DecorUpdateImage(dec.get());
        DecorUpdateMapLink(dec.get());
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
    // UNVERIFIED (3b): BuildingHovers::RegisterBuilding(b).
    b->workers.assign(d->parkingCount, nullptr);
    b->builder = nullptr;
    auto nextIs = [&](uint32_t type) { return ci < chunks.size() && chunks[ci].type == type; };
    if (nextIs(0x2d)) {
        SaveManager::Reader cr(chunks[ci++]);
        for (auto& v : b->patchStart) v = cr.u32();
    }
    if (nextIs(0x2f)) {
        SaveManager::Reader cr(chunks[ci++]);
        for (auto& v : b->patchContract) v = (int)cr.u32();
    }
    if (nextIs(0x31)) {
        SaveManager::Reader cr(chunks[ci++]);
        for (auto& v : b->patchArg) v = cr.u32();
    }
    if (nextIs(0x50)) {
        SaveManager::Reader cr(chunks[ci++]);
        b->SetUniqueID(cr.u32());
    }
}

// ------------------------------------------------------------------------- random decorations
// @0x1e3a9c Map::Patch::AddRandomDecors (re-seeds with playerSeed for every patch)
void AddRandomDecors(Patch* p, long seed) {
    Rand48::srand48(seed);
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
        DecorUpdateImage(d.get());
        DecorUpdateMapLink(d.get());
        p->decors.push_back(std::move(d));
    }
}

// ------------------------------------------------------------------------------ area borders
// @0x1bcda8 Map::UpdateAreaBorders (dark grass patches, flag posts, for-sale signs)
void UpdateAreaBorders(long seed) {
    for (auto* s : g_borderSprites) Render::RemoveSprite(s);
    g_borderSprites.clear();
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
    Rand48::srand48(seed);
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
    // UNVERIFIED: UpdateOwnedAreaBorders @0x1b68f4 (the owned extent, read by code not ported yet)
    // runs here.
    int minX, minY, maxX, maxY;
    GetAreaBorders(minX, minY, maxX, maxY);
    if (GameState::GetCurrentMapID() == 0xd) minX -= 1;
    Render::SetViewportMapBounds(minX, minY - 0xd, maxX + 1, maxY);
}

}  // namespace

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

// @0x11e4a4 (UNVERIFIED (3c): also sets each tile's AI waypoint +0x44)
void Building::LinkBaseToBuilding() {
    if (!data) return;
    ForEachFootprintTile(x, y, data->w, data->h, [&](int tx, int ty) {
        Cell* c = At(tx, ty);
        if (!c) return;
        if (GameState::GetCurrentMapID() == 0) RemoveDecorationAt(tx, ty);
        else c->decor = nullptr;
        c->building = this;
    });
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
    for (auto& p : g_patches) {
        for (auto& d : p->decors) if (d->sprite) Render::RemoveSprite(d->sprite);
    }
    g_patches.clear();
    for (auto* s : g_borderSprites) Render::RemoveSprite(s);
    g_borderSprites.clear();
    g_grid.clear();
}

bool Load(uint32_t mapId, long playerSeed) {
    Free();
    char name[64];
    std::snprintf(name, sizeof name, "maps/map_%u.bin", mapId);
    uint32_t n = 0;
    uint8_t* data = FileManager::LoadFile(name, n);
    if (!data) { std::fprintf(stderr, "Map::Load: %s not found\n", name); return false; }
    std::vector<SaveManager::Chunk> chunks;
    bool ok = SaveManager::ParseDataIntoChunks(data, n, mapId, chunks);
    FileManager::FreeFile(data);
    if (!ok) { std::fprintf(stderr, "Map::Load: %s does not parse\n", name); return false; }

    size_t ci = 0;
    auto next = [&](uint32_t type) -> SaveManager::Chunk* {  // SkipToChunk + GetChunk
        while (ci < chunks.size() && chunks[ci].type != type && chunks[ci].type != SaveManager::kEnd) ++ci;
        return ci < chunks.size() && chunks[ci].type == type ? &chunks[ci++] : nullptr;
    };
    // LoadPlayer: header chunk 0xe
    SaveManager::Chunk* hc = next(SaveManager::kHeader);
    if (!hc) return false;
    SaveManager::Reader hr(*hc);
    hr.u32();                // version
    g_mapId = hr.u32();
    g_gridW = hr.u8();
    g_gridH = hr.u8();
    hr.s8(); hr.s8();        // start position (validated against the grid)
    g_tileset = hr.u8();
    int patchCount = hr.u8();
    g_grid.assign((size_t)g_gridW * g_gridH, Cell{});

    Background::CreateLand((unsigned)g_tileset);  // Map::Load calls it after LoadPlayer; layer order is
                                                  // per-layer, so creating it first changes nothing.
    for (int i = 0; i < patchCount; ++i) {
        SaveManager::Chunk* pc = next(SaveManager::kPatch);
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
        if (SaveManager::Chunk* dc = next(SaveManager::kDecorCount)) {
            int cnt = SaveManager::Reader(*dc).s16();
            for (int k = 0; k < cnt; ++k) if (auto* c = next(SaveManager::kDecor)) LoadDecors(raw, *c);
        }
        if (SaveManager::Chunk* bc = next(SaveManager::kBuildingCount)) {
            int cnt = SaveManager::Reader(*bc).s16();
            for (int k = 0; k < cnt; ++k) {
                if (!next(SaveManager::kBuilding)) continue;
                --ci;   // LoadBuilding reads the chunk itself, then its optional followers
                LoadBuilding(raw, chunks, ci);
            }
        }
        if (SaveManager::Chunk* mc = next(SaveManager::kPatchMask)) {
            SaveManager::Reader mr(*mc);
            raw->mask = mr.str16();
            if (g_mapId != 0 && (int)raw->mask.size() == g_gridW * g_gridH) {
                for (int x = 0; x < g_gridW; ++x)
                    for (int y = 0; y < g_gridH; ++y) At(x, y)->block = raw->mask[(size_t)x * g_gridH + y] == '1';
            }
        }
    }
    // Map::Load: CreateRandomDecors, UpdateAreaBorders (spawns/portals/fog not rendered yet)
    for (auto& p : g_patches) AddRandomDecors(p.get(), playerSeed);
    UpdateAreaBorders(playerSeed);
    std::printf("Map %u: %dx%d tileset %d, %zu patches, %zu sprites\n", g_mapId, g_gridW, g_gridH, g_tileset,
                g_patches.size(), Render::SpriteCount());
    return true;
}

}  // namespace Map
