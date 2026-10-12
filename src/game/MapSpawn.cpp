// The map's spawn points and portals: loading and saving them (map chunks 2/9 with their optional
// followers, 3/10) and what a map switch keeps about the spawned entities.
#include "game/Map.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

#include "game/Entity.h"
#include "game/EntityManager.h"
#include "game/MetaExpression.h"
#include "game/SaveManager.h"
#include "game/SpawnPoint.h"

// @0x22503c
SpawnPoint::~SpawnPoint() { delete meta; }

namespace Map {

namespace {

std::vector<std::unique_ptr<SpawnPoint>> g_spawnPoints;   // 0x6136a4 (count 0x6136b0)
std::vector<std::unique_ptr<Portal>> g_portals;           // 0x613740 (count 0x61374c)

struct SpawnInfo {       // 0x24 bytes: what StoreSpawnpointInfo keeps of a spawn point over a reload
    uint32_t id = 0;
    uint32_t entityId = 0;
    int hp = 0, hpMax = 0;
    int f40 = 0, f44 = 0, corpseTaken = 0;
    uint32_t deathTime = 0;
    int f50 = 0;
};
std::vector<SpawnInfo> g_spawnInfo;   // 0x6136fc (count 0x613728)

// The optional chunk after a spawn point's, if it is the next one.
const SaveManager::Chunk* Follower(SaveManager::SaveBlock* block, uint32_t type) {
    return block->NextChunkID() == type ? &block->GetChunk() : nullptr;
}

}  // namespace

// @0x1bc0b0 Map::LoadSpawnPoints
void LoadSpawnPoints(SaveManager::SaveBlock* block) {
    block->SkipToChunk(SaveManager::kSpawnCount);
    unsigned count;
    {
        SaveManager::Reader r(block->GetChunk());
        count = (unsigned)(int)r.s16();
        block->EndChunkLoading(r);
    }
    for (unsigned n = 0; n < count; ++n) {
        block->SkipToChunk(SaveManager::kSpawn);
        SaveManager::Reader r(block->GetChunk());
        g_spawnPoints.push_back(std::make_unique<SpawnPoint>());
        SpawnPoint* s = g_spawnPoints.back().get();
        s->id = r.u32();
        s->entityId = (uint32_t)(int)r.s16();
        s->x = r.u8();
        s->y = r.u8();
        s->npc = r.u8() != 0;
        s->clas = r.u8();
        s->f18 = r.u8();
        s->aggroRange = r.u8();
        s->f20 = r.u8() != 0;
        s->f24 = r.u8();
        s->initiative = r.u8();
        std::string meta = r.str16();
        s->meta = nullptr;
        if (!meta.empty()) {
            char* text = new char[meta.size() + 1];
            std::memcpy(text, meta.c_str(), meta.size() + 1);
            s->meta = new MetaExpression(text, true);
        }
        block->EndChunkLoading(r);
        s->hp = 0;
        s->hasHp = false;
        if (const SaveManager::Chunk* c = Follower(block, 0x14)) {
            SaveManager::Reader f(*c);
            s->hasHp = true;
            s->hp = (int)f.u32();
            block->EndChunkLoading(f);
        }
        s->hpMax = 0;
        s->hasHpMax = false;
        if (const SaveManager::Chunk* c = Follower(block, 0x25)) {
            SaveManager::Reader f(*c);
            s->hasHpMax = true;
            s->hpMax = (int)f.u32();
            block->EndChunkLoading(f);
        }
        if (const SaveManager::Chunk* c = Follower(block, 0x28)) {
            SaveManager::Reader f(*c);
            s->f40 = (int)f.u32();
            s->f44 = (int)f.u32();
            block->EndChunkLoading(f);
        }
        if (const SaveManager::Chunk* c = Follower(block, 0x42)) {
            SaveManager::Reader f(*c);
            s->corpseTaken = (int)f.u32();
            block->EndChunkLoading(f);
        }
        if (const SaveManager::Chunk* c = Follower(block, 0x44)) {
            SaveManager::Reader f(*c);
            s->deathTime = f.u32();
            block->EndChunkLoading(f);
        }
        if (const SaveManager::Chunk* c = Follower(block, 0x45)) {
            SaveManager::Reader f(*c);
            s->f50 = (int)f.u32();
            block->EndChunkLoading(f);
        }
    }
}

// @0x1b9630 Map::SaveSpawnPoints. A dead entity, or one placed at random, is saved where it is now
// (unless its place is fixed: an arena or mob spawn).
void SaveSpawnPoints() {
    using namespace SaveManager;
    BeginChunk(kSpawnCount);
    SaveShort((int16_t)g_spawnPoints.size());
    EndChunk();
    for (auto& up : g_spawnPoints) {
        SpawnPoint* s = up.get();
        BeginChunk(kSpawn);
        SaveUnsigned(s->id);
        SaveShort((int16_t)s->entityId);
        int x = s->x, y = s->y;
        Entity* e = EntityManager::GetEntityBySpawnPoint(s->id);
        if (e && (e->IsDead() || e->HasRandomLocation()) && !e->HasFixedLocation()) {
            x = e->tileX;
            y = e->tileY;
        }
        SaveChar((uint8_t)x);
        SaveChar((uint8_t)y);
        SaveChar(s->npc);
        SaveChar((uint8_t)s->clas);
        SaveChar((uint8_t)s->f18);
        SaveChar((uint8_t)s->aggroRange);
        SaveChar(s->f20);
        SaveChar((uint8_t)s->f24);
        SaveChar((uint8_t)s->initiative);
        const char* text = s->meta ? s->meta->GetExpression() : nullptr;
        if (!text) {
            SaveShort(0);
        } else {
            size_t len = std::strlen(text);
            SaveShort((int16_t)len);
            for (size_t i = 0; i < len; ++i) SaveChar((uint8_t)text[i]);
        }
        EndChunk();
        if (e) {
            BeginChunk(0x14);
            SaveUnsigned((uint32_t)e->GetHP());
            EndChunk();
            BeginChunk(0x25);
            SaveUnsigned((uint32_t)e->GetHpMax());
            EndChunk();
        }
        BeginChunk(0x28);
        SaveUnsigned((uint32_t)s->f40);
        SaveUnsigned((uint32_t)s->f44);
        EndChunk();
        BeginChunk(0x42);
        SaveUnsigned((uint32_t)s->corpseTaken);
        EndChunk();
        BeginChunk(0x44);
        SaveUnsigned(s->deathTime);
        EndChunk();
        BeginChunk(0x45);
        SaveUnsigned((uint32_t)s->f50);
        EndChunk();
    }
}

// @0x1bbeb8 Map::LoadPortals
void LoadPortals(SaveManager::SaveBlock* block) {
    block->SkipToChunk(SaveManager::kPortalCount);
    unsigned count;
    {
        SaveManager::Reader r(block->GetChunk());
        count = (unsigned)(int)r.s16();
        block->EndChunkLoading(r);
    }
    for (unsigned n = 0; n < count; ++n) {
        block->SkipToChunk(SaveManager::kPortal);
        SaveManager::Reader r(block->GetChunk());
        g_portals.push_back(std::make_unique<Portal>());
        Portal* p = g_portals.back().get();
        p->map = r.s16();
        p->f04 = r.u8();
        p->f08 = r.s16();
        p->f0c = r.s16();
        p->x = r.u8();
        p->y = r.u8();
        p->walkX = r.u8();
        p->walkY = r.u8();
        block->EndChunkLoading(r);
    }
}

// @0x1b955c Map::SavePortals
void SavePortals() {
    using namespace SaveManager;
    BeginChunk(kPortalCount);
    SaveShort((int16_t)g_portals.size());
    EndChunk();
    for (auto& up : g_portals) {
        Portal* p = up.get();
        BeginChunk(kPortal);
        SaveShort((int16_t)p->map);
        SaveChar((uint8_t)p->f04);
        SaveShort((int16_t)p->f08);
        SaveShort((int16_t)p->f0c);
        SaveChar((uint8_t)p->x);
        SaveChar((uint8_t)p->y);
        SaveChar((uint8_t)p->walkX);
        SaveChar((uint8_t)p->walkY);
        EndChunk();
    }
}

// @0x1b846c Map::LinkPortals: each portal's decoration gets to know it.
void LinkPortals() {
    std::puts("Map::LinkPortals started");
    for (auto& up : g_portals)
        if (Decor* d = GetDecoration(up->x, up->y)) d->portal = up.get();
}

// @0x1b7300
SpawnPoint* GetSpawnByIndex(unsigned i) { return i < g_spawnPoints.size() ? g_spawnPoints[i].get() : nullptr; }

unsigned GetSpawnPointCount() { return (unsigned)g_spawnPoints.size(); }

// @0x1b7324
SpawnPoint* GetSpawnPointAt(int x, int y) {
    for (auto& up : g_spawnPoints)
        if (up->x == x && up->y == y) return up.get();
    return nullptr;
}

void FreeSpawnPointsAndPortals() {
    g_spawnPoints.clear();
    g_portals.clear();
}

// @0x1b88b0 Map::StoreSpawnpointInfo: what the spawn points with a living entity object know, kept
// over a reload of the map from its file.
void StoreSpawnpointInfo() {
    g_spawnInfo.clear();
    for (auto& up : g_spawnPoints) {
        Entity* e = EntityManager::GetEntityBySpawnPoint(up->id);
        if (!e) continue;
        SpawnInfo info;
        info.id = up->id;
        info.entityId = up->entityId;
        info.hp = e->GetHP();
        info.hpMax = up->hpMax;
        info.f40 = up->f40;
        info.f44 = up->f44;
        info.corpseTaken = up->corpseTaken;
        info.deathTime = up->deathTime;
        info.f50 = up->f50;
        g_spawnInfo.push_back(info);
    }
}

// @0x1b7b08 Map::RestoreSpawnpointInfo (an NPC keeps the entity of the map file)
void RestoreSpawnpointInfo() {
    std::printf("Map::RestoreSpawnpointInfo() %d spawnpoints to restore\n", (int)g_spawnInfo.size());
    for (auto& up : g_spawnPoints) {
        SpawnPoint* s = up.get();
        for (const SpawnInfo& info : g_spawnInfo) {
            if (info.id != s->id) continue;
            s->hasHp = true;
            s->hasHpMax = true;
            if (!s->npc) s->entityId = info.entityId;
            s->hp = info.hp;
            s->hpMax = info.hpMax != 0 ? info.hpMax : info.hp;
            s->f40 = info.f40;
            s->f44 = info.f44;
            s->corpseTaken = info.corpseTaken;
            s->deathTime = info.deathTime;
            s->f50 = info.f50;
        }
    }
    g_spawnInfo.clear();
}

}  // namespace Map
