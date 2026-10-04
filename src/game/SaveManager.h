// SaveManager: chunked binary format shared by map files and saves.
// Port of SaveManager::SaveBlock::ParseDataIntoChunks @0x216e40 and SaveManager::ConvertOldFormatMap
// @0x2169f4. Chunk stream: [u32 type BE][u32 size BE][payload], terminated by type 0x13 (END).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace SaveManager {

enum ChunkType : uint32_t {
    kFog = 0x17, kEnd = 0x13, kSpawnCount = 2, kPortalCount = 3, kPatch = 4, kDecorCount = 5,
    kBuildingCount = 6, kPatchMask = 7, kSpawn = 9, kPortal = 10, kDecor = 0xb, kBuilding = 0xc, kHeader = 0xe,
};

struct Chunk {
    uint32_t type = kEnd;
    std::vector<uint8_t> data;
};

// Big-endian reader over one chunk (GetData<T>, SaveChunk::LoadChar/LoadShort/LoadUnsigned).
struct Reader {
    const uint8_t* p;
    size_t n, i = 0;
    explicit Reader(const Chunk& c) : p(c.data.data()), n(c.data.size()) {}
    bool ok(size_t k) const { return i + k <= n; }
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

// Mirrors the loader: data starting with (u32 1, u32 mapId) is an old-format map and is converted.
// Returns false if the data does not parse to exactly its length.
bool ParseDataIntoChunks(const uint8_t* data, size_t size, uint32_t mapId, std::vector<Chunk>& out);

}  // namespace SaveManager
