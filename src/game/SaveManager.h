// SaveManager: the chunked binary format shared by map files and saves, and the save itself.
// Port of SaveManager (libkingdom.so 5.11): SaveChunk/SaveBlock/SaveData, Load @0x217448,
// Save @0x219b7c, GetMapData @0x2197dc, SaveBlock::ParseDataIntoChunks @0x216e40,
// ConvertOldFormatMap @0x2169f4.
//
// A chunk is [u32 type BE][u32 size BE][payload]; a block is a run of chunks ending with type 0x13
// (END). A save is [u32 block count] then per block [u32 mapId][u32 size][block]: one block per
// player map (map 0 is the city) and the game state block (mapId 0xffffffff). The main save
// (SaveData) caches every block in memory: Map::SaveMap / Map::SaveState replace theirs and
// SaveManager::Save writes them all to the save file.
//
// Not ported (online only): the save diffs (GetDifference, ApplySaveDiff, MinifySave), the online
// and PvP saves and the save ping.
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace SaveManager {

enum ChunkType : uint32_t {   // the GetChunkName table (SaveManager::Init)
    kStateBase = 0, kEntityHeader = 1, kSpawnCount = 2, kPortalCount = 3, kPatch = 4, kDecorCount = 5,
    kBuildingCount = 6, kPatchMask = 7, kSpawn = 9, kPortal = 10, kDecor = 0xb, kBuilding = 0xc,
    kEntity = 0xd, kHeader = 0xe, kEnd = 0x13, kFog = 0x17, kFogV2 = 0x30,
};

struct Chunk {                       // SaveChunk: type +0x00, data +0x08, size +0x0c
    uint32_t type = kEnd;
    std::vector<uint8_t> data;
};

// Big-endian reader over one chunk (GetData<T>, SaveChunk::LoadChar/LoadShort/LoadUnsigned; reads
// past the end give 0).
struct Reader {
    const uint8_t* p;
    size_t n, i = 0;
    explicit Reader(const Chunk& c) : p(c.data.data()), n(c.data.size()) {}
    bool ok(size_t k) const { return i + k <= n; }
    bool HasData(size_t k) const { return ok(k); }   // SaveChunk::HasData
    uint8_t u8() { return ok(1) ? p[i++] : 0; }
    int8_t s8() { return (int8_t)u8(); }
    int16_t s16() { if (!ok(2)) return 0; int16_t v = (int16_t)((p[i] << 8) | p[i + 1]); i += 2; return v; }
    uint32_t u32() {
        if (!ok(4)) return 0;
        uint32_t v = ((uint32_t)p[i] << 24) | ((uint32_t)p[i + 1] << 16) | ((uint32_t)p[i + 2] << 8) | p[i + 3];
        i += 4;
        return v;
    }
    std::string str16() { int16_t k = s16(); std::string s; if (k > 0 && ok((size_t)k)) { s.assign((const char*)p + i, (size_t)k); i += (size_t)k; } return s; }
    bool atEnd() const { return i == n; }
};

// The big-endian writers into a byte vector (::SaveChar, ::SaveShort, ::SaveUnsigned, ::SaveBytes).
void PutChar(std::vector<uint8_t>& v, uint8_t c);
void PutShort(std::vector<uint8_t>& v, int16_t s);
void PutUnsigned(std::vector<uint8_t>& v, uint32_t u);

class SaveData;

struct SaveBlock {                   // 100 bytes
    bool fromDisk = false;           // +0x00 read from the map file (GetMapDataFromDisk)
    uint32_t mapId = 0;              // +0x04 0xffffffff: the game state
    std::vector<uint8_t> data;       // +0x08/+0x0c the merged chunks
    std::vector<Chunk> chunks;       // +0x10/+0x2c
    size_t next = 0;                 // +0x30 the loading cursor
    bool writing = false;            // +0x34
    Chunk cur;                       // +0x38 the chunk being written
    SaveData* owner = nullptr;       // +0x60

    void BeginChunk(uint32_t type);  // @0x212a6c
    void EndChunk();                 // @0x214a48
    void AddChunk(const Chunk& c);   // @0x2147cc a copy of a loaded chunk
    // @0x212ac4 GetChunk / BeginChunkLoading: the chunk at the cursor (END past the last one).
    const Chunk& GetChunk();
    void EndChunkLoading(const Reader& r);   // @0x212fa0 reports a chunk not read to its end
    uint32_t NextChunkID() const;    // @0x212be8
    void SkipChunk(bool quiet);      // @0x2130f8 (never past END)
    void SkipToChunk(uint32_t type, bool quiet = false);   // @0x21317c (stops at END)
    bool HasChunkByType(uint32_t type) const;              // @0x212b00
    const Chunk* GetChunkByType(uint32_t type) const;      // @0x212b5c
    void MergeChunks();              // @0x213eac chunks -> data
    bool ParseDataIntoChunks();      // @0x216e40 data -> chunks (old format maps converted first)
};

class SaveData {                     // 0x14 bytes
public:
    std::vector<std::unique_ptr<SaveBlock>> blocks;   // +0x00/+0x0c
    std::unique_ptr<SaveBlock> current;               // +0x10 the block being written

    SaveBlock* GetMapData(uint32_t mapId);   // @0x212c50 (resets its cursor and disk flag)
    SaveBlock* GetGameStateData() { return GetMapData(0xffffffff); }   // @0x212cc0
    bool HasMapInCache(uint32_t mapId) const;   // @0x213008
    void ResetMap(uint32_t mapId);           // @0x212ef8
    void BeginData();                        // @0x2130a8
    void EndMapData(uint32_t mapId);         // @0x214de4
    void EndGameStateData() { EndMapData(0xffffffff); }   // @0x214f60
    void Reset();                            // @0x2140c0
    void Save(std::vector<uint8_t>& out) const;   // @0x21993c
    void Save(const char* name) const;       // @0x219ad8
};

void Init();                                 // @0x2131c0
void Deinit();                               // @0x2141a4
uint32_t GetCurrentVersion();                // @0x212cc8 0x400
const char* GetChunkName(uint32_t type);     // @0x212cd0
SaveData* GetMainSave();                     // @0x212cfc
void SetMainSave(SaveData* data);            // @0x214160 (takes ownership)
SaveBlock* GetGameStateData();               // @0x212d40
// @0x2197dc: the main save's block for a map, else the map file's (added to the main save); the
// game state has no file.
SaveBlock* GetMapData(uint32_t mapId);
std::unique_ptr<SaveBlock> GetMapDataFromDisk(uint32_t mapId, bool quiet);   // @0x217648
std::unique_ptr<SaveBlock> ConvertOldFormatMap(const uint8_t* data, size_t size);   // @0x2169f4
bool IsSaveValid(const uint8_t* data, size_t size);                          // @0x21431c
std::unique_ptr<SaveData> Load(const uint8_t* data, size_t size, bool* corrupt);   // @0x217230
// @0x217448: the main save from the named save file (null: the current save, then the backup);
// false when there is none.
bool Load(const char* name, bool* corrupt);
void Save(const char* name);                 // @0x219b7c only the player's city, never in the tutorial

void SetCurrentSaveName(const std::string& name);   // @0x214b6c
std::string GetCurrentSaveName();                   // @0x215024
void SetBackupSaveName(const std::string& name);    // @0x2151b8
std::string GetBackupSaveName();                    // @0x214f68

// Writing into the main save's current block (SaveData::BeginData selects it).
void BeginChunk(uint32_t type);              // @0x212d10
void EndChunk();                             // @0x214b54
void SaveChar(uint8_t c);                    // @0x21666c
void SaveShort(int16_t s);                   // @0x21678c
void SaveUnsigned(uint32_t u);               // @0x2169d4

uint32_t GetLastSaveTS();                    // @0x212d74
void SetLastSaveTS(uint32_t t);              // @0x212d88
uint32_t GetSavePeriod();                    // @0x212d9c Setting "save_ping_period" (30)
uint32_t GetSaveDelay();                     // @0x212db0 "save_ping_delay" (120)
bool IsSavePingEnabled();                    // @0x212dc4 "save_ping_enabled" (true)
uint32_t GetSendSavePeriod();                // @0x212dd8 "save_send_period" (300)

}  // namespace SaveManager
