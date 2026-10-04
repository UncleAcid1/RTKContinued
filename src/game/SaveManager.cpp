#include "game/SaveManager.h"

namespace SaveManager {
namespace {

struct Src {
    const uint8_t* p;
    size_t n, i = 0;
    bool fail = false;
    const uint8_t* take(size_t k) {
        if (i + k > n) { fail = true; return nullptr; }
        const uint8_t* r = p + i;
        i += k;
        return r;
    }
    int16_t s16() { const uint8_t* b = take(2); return b ? (int16_t)((b[0] << 8) | b[1]) : 0; }
    uint8_t u8() { const uint8_t* b = take(1); return b ? b[0] : 0; }
    uint32_t u32() { const uint8_t* b = take(4); return b ? ((uint32_t)b[0] << 24 | (uint32_t)b[1] << 16 | (uint32_t)b[2] << 8 | b[3]) : 0; }
};

struct Builder {
    std::vector<Chunk>& out;
    void begin(uint32_t t) { out.push_back({t, {}}); }
    void bytes(Src& s, size_t k) {
        if (k > s.n) { s.fail = true; return; }
        const uint8_t* b = s.take(k);
        if (b) out.back().data.insert(out.back().data.end(), b, b + k);
    }
    void s16(int16_t v) { out.back().data.push_back((uint8_t)(v >> 8)); out.back().data.push_back((uint8_t)v); }
    void u8(uint8_t v) { out.back().data.push_back(v); }
    void u32(uint32_t v) { for (int k = 3; k >= 0; --k) out.back().data.push_back((uint8_t)(v >> (k * 8))); }
    void s16blob(Src& s) { int16_t k = s.s16(); s16(k); if (k < 0) { s.fail = true; return; } bytes(s, (size_t)k); }
};

// @0x2169f4 SaveManager::ConvertOldFormatMap. The 8-byte (1, mapId) header is part of the first
// 13-byte header chunk (the function receives the start of the data).
bool ConvertOldFormatMap(const uint8_t* data, size_t size, std::vector<Chunk>& out) {
    Src s{data, size};
    Builder b{out};
    b.begin(kHeader);
    b.bytes(s, 0xd);
    uint8_t nPatches = s.u8();
    b.u8(nPatches);
    for (unsigned p = 0; p < nPatches && !s.fail; ++p) {
        b.begin(kPatch);
        b.bytes(s, 3);
        int16_t n = s.s16();
        b.begin(kDecorCount);
        b.s16(n);
        for (int k = 0; k < n && !s.fail; ++k) {
            b.begin(kDecor);
            b.bytes(s, 0x14);
            b.s16blob(s);
            b.s16blob(s);
        }
        n = s.s16();
        b.begin(kBuildingCount);
        b.s16(n);
        for (int k = 0; k < n && !s.fail; ++k) {
            b.begin(kBuilding);
            b.bytes(s, 0x2e);
            uint8_t cnt = s.u8();
            b.u8(cnt);
            b.bytes(s, (size_t)cnt * 2);
            b.bytes(s, 0xe);
            b.s16blob(s);
            b.s16blob(s);
            b.bytes(s, 4);
        }
        b.begin(kPatchMask);
        b.s16blob(s);
    }
    int16_t n = s.s16();
    b.begin(kSpawnCount);
    b.s16(n);
    for (int k = 0; k < n && !s.fail; ++k) {
        b.begin(kSpawn);
        b.bytes(s, 0xf);
        b.s16blob(s);
    }
    n = s.s16();
    b.begin(kPortalCount);
    b.s16(n);
    for (int k = 0; k < n && !s.fail; ++k) {
        b.begin(kPortal);
        b.bytes(s, 0xb);
    }
    n = s.s16();
    b.begin(kFog);
    b.s16(n);
    if (n > 0) b.bytes(s, (size_t)n * 3);
    b.begin(kEnd);
    b.u32(0);
    return !s.fail && s.i == size;
}

}  // namespace

bool ParseDataIntoChunks(const uint8_t* data, size_t size, uint32_t mapId, std::vector<Chunk>& out) {
    out.clear();
    if (size < 8) return false;
    Src head{data, size};
    uint32_t a = head.u32(), b = head.u32();
    if (a == 1 && b == mapId) return ConvertOldFormatMap(data, size, out);
    Src s{data, size};
    for (;;) {
        uint32_t t = s.u32(), n = s.u32();
        if (s.fail) return false;
        const uint8_t* p = s.take(n);
        if (!p) return false;
        out.push_back({t, std::vector<uint8_t>(p, p + n)});
        if (t == kEnd) return s.i == size;
    }
}

}  // namespace SaveManager
