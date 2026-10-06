#include "game/SaveManager.h"

#include <cstdio>

#include "engine/FileManager.h"
#include "engine/SystemFuncs.h"
#include "engine/Timer.h"
#include "game/GameState.h"

namespace SaveManager {
namespace {

std::unique_ptr<SaveData> g_mainSave;   // 0x6154ec
SaveData* g_writing = nullptr;          // 0x6154f0 the SaveData of the last BeginData
std::string g_currentSaveName;          // SaveManager::mCurrentSaveName
std::string g_backupSaveName;           // SaveManager::mBackupSaveName
uint32_t g_lastSaveTS = 0;              // SaveManager::mLastSaveTS
uint32_t g_savePeriod = 0;              // SaveManager::mSavePeriod
uint32_t g_saveDelay = 0;               // SaveManager::mSaveDelay
bool g_savePingEnabled = false;         // SaveManager::mSavePingEnabled
uint32_t g_sendSavePeriod = 0;          // SaveManager::mSendSavePeriod

// The chunk name table (0x6150ec, filled by Init).
const char* const kChunkNames[] = {
    "CHUNK_STATE_BASE", "CHUNK_ENTITY_HEADER", "CHUNK_SPAWNPOINTS_HEADER", "CHUNK_PORTALS_HEADER",
    "CHUNK_PATCH_HEADER", "CHUNK_DECORATIONS_HEADER", "CHUNK_BUILDINGS_HEADER", "CHUNK_BLOCKS",
    "obsolete", "CHUNK_SPAWN", "CHUNK_PORTAL", "CHUNK_DECORATION", "CHUNK_BUILDING", "CHUNK_ENTITY",
    "CHUNK_MAP_HEADER", "CHUNK_STATE_TIME_ADVANCE", "CHUNK_STATE_PLAYER_INFO", "CHUNK_STATE_PLAYER_ITEMS",
    "obsolete", "CHUNK_END_MARKER", "CHUNK_SPAWN_EXTRA", "CHUNK_STATE_PLAYER_BELT_ITEMS",
    "CHUNK_STATE_LAST_MAP", "CHUNK_MAP_FOG", "CHUNK_STATE_STARTED_TASKS", "CHUNK_STATE_BOSSFIGHTS",
    "CHUNK_STATE_MAP_COMPLETION", "CHUNK_STATE_PLAYER_BELT_ACTIVATION", "obsolete",
    "CHUNK_STATE_MAP_START_TIME", "CHUNK_STATE_TUTORIAL", "CHUNK_STATE_HPREGEN",
    "CHUNK_STATE_OFFLINE_BUILDINGS", "CHUNK_STATE_SHOWN_TASKS", "CHUNK_STATE_MAP_COLLECTION_INFO",
    "CHUNK_ENTITY_AP", "CHUNK_STATE_SECOND_TUTORIAL", "CHUNK_SPAWN_HP_MAX", "CHUNK_STATE_PLAYER_ITEMS_V2",
    "CHUNK_STATE_MAP_END_TIME", "CHUNK_SPAWN_DROP_INFO", "CHUNK_STATE_SOLDIER_RESERVE", "obsolete",
    "CHUNK_STATE_TIME_START", "CHUNK_STATE_ITEM_ACQUIRE_TIME", "CHUNK_BUILDING_FARM_EXTRA",
    "CHUNK_STATE_ITEM_RECENT_TIME", "CHUNK_BUILDING_FARM_EXTRA2", "CHUNK_MAP_FOG_VERSION_2",
    "CHUNK_BUILDING_FARM_EXTRA3", "CHUNK_ENTITY_MAX_HP", "obsolete", "CHUNK_STATE_BOSSFIGHTS_TIMERS",
    "CHUNK_STATE_TASK_BEGIN_TIME", "CHUNK_STATE_DAILY_BONUS", "obsolete", "CHUNK_STATE_CAMPAIGN_COMPLETION",
    "CHUNK_STATE_TASK_END_TIME", "CHUNK_STATE_PLAYER_ITEMS_V3", "CHUNK_STATE_BUGFIX_WOLF_QUEST",
    "CHUNK_STATE_SOCNET_CONNECTIONS", "CHUNK_STATE_TUTORIAL_TYPE", "CHUNK_STATE_EXPANSION_PACK_VERSIONS",
    "CHUNK_STATE_MAIN_EXPANSION_BACKGROUND_LOAD", "CHUNK_STATE_DAILY_BONUS_COUNT",
    "CHUNK_STATE_PROFESSION_POINTS", "CHUNK_SPAWN_CORPSE_INFO", "CHUNK_STATE_RES_VALIDATION_FAIL_COUNT",
    "CHUNK_SPAWN_KILL_TIME", "CHUNK_SPAWN_KILL_COUNT", "CHUNK_STATE_EVENT_START_TIME",
    "CHUNK_STATE_EVENT_END_TIME", "CHUNK_STATE_MAP_VERSION", "CHUNK_STATE_EVENT_DIFFICULTY",
    "CHUNK_STATE_EVENT_COMPLETION_COUNT", "CHUNK_STATE_MONSTER_KILL_STATS", "CHUNK_STATE_ARENA_STATS",
    "CHUNK_STATE_WARRIOR_RESPAWN_V2", "CHUNK_STATE_ONLINE_STATS", "CHUNK_STATE_PVP",
    "CHUNK_BUILDING_UNIQUE_ID", "CHUNK_STATE_CUSTOM_SUBTASK_REQUIREMENT", "CHUNK_STATE_SOLDIER_ACQUIRE_INFO",
    "CHUNK_ENTITY_OVERRIDE_PARAMETERS", "CHUNK_STATE_CUSTOMIZATION_BINDINGS", "CHUNK_STATE_MAP_VISIT_TIME",
    "CHUNK_STATE_TUTORIAL_PVP", "CHUNK_STATE_ITEM_ACTIVAION_INFO", "CHUNK_STATE_COMPU_GACHA_RECEIVED",
    "CHUNK_STATE_FEATURED_TASK_CLICK_TIME", "CHUNK_UPDATE_DECORATION", "CHUNK_UPDATE_SPAWNPOINT",
    "CHUNK_MAP_FOG_VERSION_3", "CHUNK_STATE_PLAYER_ITEMS_V4", "CHUNK_UPDATE_DECORATIONS_HEADER",
    "CHUNK_UPDATE_SPAWNPOINTS_HEADER", "CHUNK_STATE_PURCHASED_PRODUCTS", "CHUNK_STATE_ID_LIST_DIFF",
    "CHUNK_STATE_ID_VALUE_LIST_DIFF", "CHUNK_STATE_COMPLETED_TASKS", "CHUNK_STATE_COMPLETED_SUBTASKS",
    "CHUNK_STATE_BASE_SHORT", "CHUNK_STATE_ONLINE_STATE_EXTRA", "CHUNK_ENTITY_UNIQUE_ID",
    "CHUNK_PLAYER_TYPE",
};

uint32_t GetU32(const uint8_t*& p) {   // GetData<unsigned int>
    uint32_t v = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
    p += 4;
    return v;
}

// The reading side of ConvertOldFormatMap (GetData / SaveBytes on the raw map file).
struct Src {
    const uint8_t* p;
    size_t n, i = 0;
    const uint8_t* take(size_t k) {
        static const uint8_t zero[0x40] = {};
        if (i + k > n) { i = n; return k <= sizeof zero ? zero : nullptr; }
        const uint8_t* r = p + i;
        i += k;
        return r;
    }
    int16_t s16() { const uint8_t* b = take(2); return (int16_t)((b[0] << 8) | b[1]); }
    uint8_t u8() { return *take(1); }
};

void Bytes(SaveBlock& b, Src& s, int k) {   // SaveBytes
    if (k <= 0) return;
    const uint8_t* p = s.take((size_t)k);
    if (p) b.cur.data.insert(b.cur.data.end(), p, p + k);
}

void Blob16(SaveBlock& b, Src& s) {         // a short length and that many bytes
    int16_t k = s.s16();
    PutShort(b.cur.data, k);
    Bytes(b, s, k);
}

SaveBlock* Current() { return g_writing ? g_writing->current.get() : nullptr; }

}  // namespace

void PutChar(std::vector<uint8_t>& v, uint8_t c) { v.push_back(c); }
void PutShort(std::vector<uint8_t>& v, int16_t s) { v.push_back((uint8_t)((uint16_t)s >> 8)); v.push_back((uint8_t)s); }
void PutUnsigned(std::vector<uint8_t>& v, uint32_t u) {
    for (int k = 3; k >= 0; --k) v.push_back((uint8_t)(u >> (k * 8)));
}

// ----------------------------------------------------------------------------------- SaveBlock
void SaveBlock::BeginChunk(uint32_t type) {
    cur.type = type;
    cur.data.clear();
    writing = true;
}

void SaveBlock::EndChunk() {
    writing = false;
    chunks.push_back(std::move(cur));
    cur = Chunk();
}

void SaveBlock::AddChunk(const Chunk& c) { chunks.push_back(c); }

const Chunk& SaveBlock::GetChunk() {
    static const Chunk end;
    if (next >= chunks.size()) return end;
    return chunks[next++];
}

void SaveBlock::EndChunkLoading(const Reader& r) {
    if (r.i < r.n)
        std::fprintf(stderr, "ERROR: SaveManager::EndChunkLoading() Chunk data was not loaded completely\n");
}

uint32_t SaveBlock::NextChunkID() const { return next < chunks.size() ? chunks[next].type : (uint32_t)kEnd; }

void SaveBlock::SkipChunk(bool quiet) {
    if (NextChunkID() == kEnd) {
        std::puts("SaveBlock::SkipChunk() Cannot skip end marker");
        return;
    }
    if (!quiet) std::printf("SaveBlock::SkipChunk() Skipping chunk %d (%s)\n", NextChunkID(), GetChunkName(NextChunkID()));
    ++next;
}

void SaveBlock::SkipToChunk(uint32_t type, bool quiet) {
    while (NextChunkID() != type && NextChunkID() != kEnd) SkipChunk(quiet);
}

bool SaveBlock::HasChunkByType(uint32_t type) const {
    for (const Chunk& c : chunks) if (c.type == type) return true;
    return false;
}

const Chunk* SaveBlock::GetChunkByType(uint32_t type) const {
    for (const Chunk& c : chunks) if (c.type == type) return &c;
    return nullptr;
}

void SaveBlock::MergeChunks() {
    data.clear();
    for (const Chunk& c : chunks) {
        PutUnsigned(data, c.type);
        PutUnsigned(data, (uint32_t)c.data.size());
        data.insert(data.end(), c.data.begin(), c.data.end());
    }
}

bool SaveBlock::ParseDataIntoChunks() {
    if (mapId != 0xffffffff && data.size() >= 8) {
        const uint8_t* p = data.data();
        uint32_t a = GetU32(p), b = GetU32(p);
        if (a == 1 && b == mapId) {
            std::printf("SaveManager::SaveBlock::ParseDataIntoChunks() Found old format map %d in save\n", b);
            data = ConvertOldFormatMap(data.data(), data.size())->data;
        }
    }
    chunks.clear();
    size_t i = 0;
    for (;;) {
        if (i + 8 > data.size()) {
            std::fprintf(stderr, "SaveManager::SaveBlock::ParseDataIntoChunks() Map %d data ends at %zu of %zu\n",
                         mapId, i, data.size());
            return false;
        }
        const uint8_t* p = data.data() + i;
        uint32_t type = GetU32(p), size = GetU32(p);
        i += 8;
        if (size > data.size() - i) {
            std::fprintf(stderr, "SaveManager::SaveBlock::ParseDataIntoChunks() Map %d chunk ends at %zu of %zu\n",
                         mapId, i + size, data.size());
            return false;
        }
        chunks.push_back({type, std::vector<uint8_t>(p, p + size)});
        i += size;
        if (type == kEnd) return true;
    }
}

// ------------------------------------------------------------------------------------ SaveData
SaveBlock* SaveData::GetMapData(uint32_t mapId) {
    for (auto& b : blocks) {
        if (b->mapId != mapId) continue;
        b->fromDisk = false;
        b->next = 0;
        return b.get();
    }
    return nullptr;
}

bool SaveData::HasMapInCache(uint32_t mapId) const {
    if (mapId == 0xffffffff) std::puts("SaveManager::HasMapInCache() Searching for user game state");
    else std::printf("SaveManager::HasMapInCache() Searching for user map %d\n", mapId);
    for (auto& b : blocks) {
        if (b->mapId != mapId) continue;
        std::printf("SaveManager::HasMapInCache() Found user map (size %d bytes)\n", (int)b->data.size());
        return true;
    }
    return false;
}

void SaveData::ResetMap(uint32_t mapId) {
    for (size_t i = 0; i < blocks.size(); ++i) {
        if (blocks[i]->mapId != mapId) continue;
        std::printf("SaveManager::Save::ResetMap() Removing user map %d (size %d bytes)\n", mapId, (int)blocks[i]->data.size());
        blocks[i] = std::move(blocks.back());   // the last block takes its place
        blocks.pop_back();
        return;
    }
}

void SaveData::BeginData() {
    std::puts("SaveManager::Save::BeginData() Preparing for block save");
    g_writing = this;
    current = std::make_unique<SaveBlock>();
    current->owner = this;
}

void SaveData::EndMapData(uint32_t mapId) {
    current->mapId = mapId;
    current->MergeChunks();
    std::printf("SaveManager::SaveData::EndMapData() Saving map %d\n", mapId);
    for (auto& b : blocks) {
        if (b->mapId != mapId) continue;
        std::printf("SaveManager::EndMapData() Updating existing block (old size %d, new size %d) Block count: %d\n",
                    (int)b->data.size(), (int)current->data.size(), (int)blocks.size());
        b = std::move(current);
        return;
    }
    std::printf("SaveManager::EndMapData() Creating new block (size %d) Block count: %d\n", (int)current->data.size(),
                (int)blocks.size());
    blocks.push_back(std::move(current));
}

void SaveData::Reset() { blocks.clear(); }

void SaveData::Save(std::vector<uint8_t>& out) const {
    PutUnsigned(out, (uint32_t)blocks.size());
    for (auto& b : blocks) {
        PutUnsigned(out, b->mapId);
        PutUnsigned(out, (uint32_t)b->data.size());
        std::printf("SaveManager::SaveData::Save() Saving map %d (size %d bytes)\n", b->mapId, (int)b->data.size());
        out.insert(out.end(), b->data.begin(), b->data.end());
    }
}

void SaveData::Save(const char* name) const {
    std::printf("SaveManager::SaveData::Save() Saving %d blocks\n", (int)blocks.size());
    std::vector<uint8_t> out;
    Save(out);
    std::printf("SaveManager::SaveData::Save() Saving size is %d bytes\n", (int)out.size());
    g_lastSaveTS = Timer::GetGlobalTime();
    FileManager::SaveSave(name, out.data(), (uint32_t)out.size(), false);
}

// --------------------------------------------------------------------------------- SaveManager
// (The online save diff table, Save::Diff::next, is not ported.)
void Init() {
    g_saveDelay = (uint32_t)SystemFuncs::GetSetting_Int("save_ping_delay", 0x78);
    g_savePeriod = (uint32_t)SystemFuncs::GetSetting_Int("save_ping_period", 0x1e);
    g_savePingEnabled = SystemFuncs::GetSetting_Bool("save_ping_enabled", true);
    g_sendSavePeriod = (uint32_t)SystemFuncs::GetSetting_Int("save_send_period", 300);
    g_mainSave = std::make_unique<SaveData>();
}

void Deinit() { g_mainSave.reset(); }

uint32_t GetCurrentVersion() { return 0x400; }

const char* GetChunkName(uint32_t type) {
    if (type < sizeof kChunkNames / sizeof kChunkNames[0]) return kChunkNames[type];
    return "unknown";
}

SaveData* GetMainSave() { return g_mainSave.get(); }
void SetMainSave(SaveData* data) { g_mainSave.reset(data); }
SaveBlock* GetGameStateData() { return g_mainSave ? g_mainSave->GetGameStateData() : nullptr; }

SaveBlock* GetMapData(uint32_t mapId) {
    if (mapId == 0xffffffff) std::puts("SaveManager::GetMapData() Loading game state");
    else std::printf("SaveManager::GetMapData() Loading map %d\n", mapId);
    if (g_mainSave) {
        if (SaveBlock* b = g_mainSave->GetMapData(mapId)) return b;
    }
    if (mapId == 0xffffffff) {
        std::puts("SaveManager::GetMapData() Game state doesn't exist yet");
        return nullptr;
    }
    if (!g_mainSave) g_mainSave = std::make_unique<SaveData>();
    g_mainSave->blocks.push_back(GetMapDataFromDisk(mapId, false));
    return g_mainSave->blocks.back().get();
}

// PORT: sandbox mode ("maps_sandbox/map_%d_sandbox.bin", a debug switch) is off.
std::unique_ptr<SaveBlock> GetMapDataFromDisk(uint32_t mapId, bool quiet) {
    char name[64];
    std::snprintf(name, sizeof name, "maps/map_%d.bin", mapId);
    if (!quiet) std::printf("SaveManager::GetMapDataFromDisk() Loading map (%s)\n", name);
    uint32_t n = 0;
    uint8_t* data = FileManager::LoadFile(name, n);
    if (!data) return nullptr;
    std::unique_ptr<SaveBlock> b = ConvertOldFormatMap(data, n);
    b->mapId = mapId;
    b->fromDisk = true;
    FileManager::FreeFile(data);
    return b;
}

// The 8-byte (1, mapId) header is part of the first 13-byte header chunk.
std::unique_ptr<SaveBlock> ConvertOldFormatMap(const uint8_t* data, size_t size) {
    auto b = std::make_unique<SaveBlock>();
    Src s{data, size};
    b->BeginChunk(kHeader);
    Bytes(*b, s, 0xd);
    uint8_t nPatches = s.u8();
    PutChar(b->cur.data, nPatches);
    b->EndChunk();
    for (unsigned p = 0; p < nPatches; ++p) {
        b->BeginChunk(kPatch);
        Bytes(*b, s, 3);
        b->EndChunk();
        b->BeginChunk(kDecorCount);
        int16_t n = s.s16();
        PutShort(b->cur.data, n);
        b->EndChunk();
        for (int k = 0; k < n; ++k) {
            b->BeginChunk(kDecor);
            Bytes(*b, s, 0x14);
            Blob16(*b, s);
            Blob16(*b, s);
            b->EndChunk();
        }
        b->BeginChunk(kBuildingCount);
        n = s.s16();
        PutShort(b->cur.data, n);
        b->EndChunk();
        for (int k = 0; k < n; ++k) {
            b->BeginChunk(kBuilding);
            Bytes(*b, s, 0x2e);
            uint8_t cnt = s.u8();
            PutChar(b->cur.data, cnt);
            Bytes(*b, s, cnt * 2);
            Bytes(*b, s, 0xe);
            Blob16(*b, s);
            Blob16(*b, s);
            Bytes(*b, s, 4);
            b->EndChunk();
        }
        b->BeginChunk(kPatchMask);
        Blob16(*b, s);
        b->EndChunk();
    }
    b->BeginChunk(kSpawnCount);
    int16_t n = s.s16();
    PutShort(b->cur.data, n);
    b->EndChunk();
    for (int k = 0; k < n; ++k) {
        b->BeginChunk(kSpawn);
        Bytes(*b, s, 0xf);
        Blob16(*b, s);
        b->EndChunk();
    }
    b->BeginChunk(kPortalCount);
    n = s.s16();
    PutShort(b->cur.data, n);
    b->EndChunk();
    for (int k = 0; k < n; ++k) {
        b->BeginChunk(kPortal);
        Bytes(*b, s, 0xb);
        b->EndChunk();
    }
    b->BeginChunk(kFog);
    n = s.s16();
    PutShort(b->cur.data, n);
    Bytes(*b, s, n * 3);
    b->EndChunk();
    b->BeginChunk(kEnd);
    PutUnsigned(b->cur.data, 0);
    b->EndChunk();
    b->MergeChunks();
    return b;
}

bool IsSaveValid(const uint8_t* data, size_t size) {
    std::printf("SaveManager::Load() Found save (size %d bytes), validating\n", (int)size);
    if ((int)size < 4) {
        std::fprintf(stderr, "SaveManager::Load() Save doesn't even have a header\n");
        return false;
    }
    const uint8_t* p = data;
    int left = (int)size - 4;
    uint32_t count = GetU32(p);
    std::printf("SaveManager::Load() Save contains %d blocks\n", count);
    for (uint32_t i = 0; i < count; ++i) {
        if (left < 8) {   // PORT: the original reads the block header past the end
            std::fprintf(stderr, "SaveManager::Load() Save doesn't contain enough data (at block %d)\n", i);
            return false;
        }
        uint32_t mapId = GetU32(p), n = GetU32(p);
        left = left - (int)n - 8;
        if (left < 0) {
            std::fprintf(stderr, "SaveManager::Load() Save doesn't contain enough data (at block %d, mapID %d, block size %d)\n",
                         i, mapId, n);
            return false;
        }
        p += n;
    }
    if (left == 0) return true;
    std::fprintf(stderr, "SaveManager::Load() Save contains excessive amount of data\n");
    return false;
}

std::unique_ptr<SaveData> Load(const uint8_t* data, size_t size, bool* corrupt) {
    auto save = std::make_unique<SaveData>();
    if (!data) return save;
    std::printf("SaveManager::Load() Found save (size %d bytes), loading blocks\n", (int)size);
    const uint8_t* p = data;
    long left = (long)size - 4;
    uint32_t count = GetU32(p);
    std::printf("SaveManager::Load() Save contains %d blocks\n", count);
    for (uint32_t i = 1; i <= count; ++i) {
        auto b = std::make_unique<SaveBlock>();
        if (left < 8) left = -1;   // PORT: the original reads the block header past the end
        else {
            b->mapId = GetU32(p);
            uint32_t n = GetU32(p);
            std::printf("SaveManager::Load() Loading map %d (size %d bytes)\n", b->mapId, n);
            left = left - 8 - (long)n;
            if (left >= 0) b->data.assign(p, p + n), p += n;
        }
        if (left < 0) {
            std::fprintf(stderr, "SaveManager::Load() Save data has ended at the middle of block %d/%d\n", i, count);
            if (corrupt) *corrupt = true;
            return save;
        }
        if (!b->ParseDataIntoChunks()) {
            std::fprintf(stderr, "SaveManager::Load() Incorrect block %d/%d\n", i, count);
            if (corrupt) *corrupt = true;
            continue;
        }
        b->owner = save.get();
        save->blocks.push_back(std::move(b));
    }
    return save;
}

bool Load(const char* name, bool* corrupt) {
    std::puts("SaveManager::Load() Loading save");
    uint32_t n = 0;
    uint8_t* data = FileManager::LoadSave(name ? name : g_currentSaveName.c_str(), n, false);
    bool validate = true;
    if (!data || n == 0) {
        if (!name) data = FileManager::LoadSave(g_backupSaveName.c_str(), n, false);
        if (!data) return false;
        validate = name != nullptr;
    }
    if (validate && !IsSaveValid(data, n)) {
        std::fprintf(stderr, "SaveManager::Load() Save is not valid, trying previous save\n");
        uint32_t m = 0;
        uint8_t* prev = FileManager::LoadSave(g_backupSaveName.c_str(), m, false);
        if (!prev) std::fprintf(stderr, "SaveManager::Load() There is no previous save\n");
        else {
            delete[] data;
            data = prev;
            n = m;
        }
    }
    g_mainSave = Load(data, n, corrupt);
    delete[] data;
    return true;
}

// (The online send, FileManager::SendSaveToServer for a queued send, is not ported.)
void Save(const char* name) {
    if (!GameState::IsPlayerCity()) return;
    if (GameState::IsTutorial()) {
        std::puts("SaveManager::Save() Cannot save in tutorial");
        return;
    }
    g_mainSave->Save(name ? name : g_currentSaveName.c_str());
}

void SetCurrentSaveName(const std::string& name) { g_currentSaveName = name; }
std::string GetCurrentSaveName() { return g_currentSaveName; }
void SetBackupSaveName(const std::string& name) { g_backupSaveName = name; }
std::string GetBackupSaveName() { return g_backupSaveName; }

void BeginChunk(uint32_t type) { Current()->BeginChunk(type); }
void EndChunk() { Current()->EndChunk(); }
void SaveChar(uint8_t c) { PutChar(Current()->cur.data, c); }
void SaveShort(int16_t s) { PutShort(Current()->cur.data, s); }
void SaveUnsigned(uint32_t u) { PutUnsigned(Current()->cur.data, u); }

uint32_t GetLastSaveTS() { return g_lastSaveTS; }
void SetLastSaveTS(uint32_t t) { g_lastSaveTS = t; }
uint32_t GetSavePeriod() { return g_savePeriod; }
uint32_t GetSaveDelay() { return g_saveDelay; }
bool IsSavePingEnabled() { return g_savePingEnabled; }
uint32_t GetSendSavePeriod() { return g_sendSavePeriod; }

}  // namespace SaveManager
