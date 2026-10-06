#include "engine/FileManager.h"

#include <SDL3/SDL.h>
#include <zlib.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

#include "engine/SystemFuncs.h"
#include "engine/TextInput.h"
#include "game/GameState.h"

namespace FileManager {
namespace {

enum class SourceKind { AndroidData, Blob, Win8 };

struct Source {
    SourceKind kind;
    std::string path;                     // data dir, .kbf file, or Win8 Assets dir
    uint32_t dataBase = 0;                // Blob: 20 + index size (payload base)
    std::map<uint32_t, uint32_t> chunkBase;  // AndroidData: chunk id -> smallest global offset in it
};

std::vector<Source> g_sources;
std::unordered_map<std::string, Entry> g_files;  // key: lower-cased name (StringInsensitiveHash/Compare)

std::string Lower(const char* s) {
    std::string r(s);
    for (auto& c : r) c = (char)std::tolower((unsigned char)c);
    return r;
}

std::vector<uint8_t> ReadWhole(const std::string& path) {
    std::vector<uint8_t> v;
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return v;
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    v.resize((size_t)n);
    if (n > 0 && std::fread(v.data(), 1, (size_t)n, f) != (size_t)n) v.clear();
    std::fclose(f);
    return v;
}

// Index entry: u32 name_len (incl. NUL), name, 8 x u32 LE:
//   hash, width, height, flags (bit0 = gzip), bpp, offset, size, chunk
// Shared by res_desc.bin and the .kbf index (verified on every entry of all sources).
template <class Fn>
bool ParseIndex(const uint8_t* p, size_t n, Fn&& fn) {
    size_t i = 0;
    while (i < n) {
        if (i + 4 > n) return false;
        uint32_t len;
        std::memcpy(&len, p + i, 4);
        i += 4;
        if (i + len + 32 > n) return false;
        Entry e;
        e.name.assign((const char*)p + i, strnlen((const char*)p + i, len));
        i += len;
        uint32_t f[8];
        std::memcpy(f, p + i, 32);
        i += 32;
        e.hash = f[0]; e.width = f[1]; e.height = f[2]; e.flags = f[3];
        e.bpp = f[4]; e.offset = f[5]; e.size = f[6]; e.chunk = f[7];
        fn(std::move(e));
    }
    return i == n;
}

bool Inflate(const uint8_t* src, uint32_t n, std::vector<uint8_t>& out) {
    z_stream zs{};
    if (inflateInit2(&zs, 16 + MAX_WBITS) != Z_OK) return false;  // gzip wrapper
    zs.next_in = const_cast<Bytef*>(src);
    zs.avail_in = n;
    out.clear();
    uint8_t buf[65536];
    int rc;
    do {
        zs.next_out = buf;
        zs.avail_out = sizeof buf;
        rc = inflate(&zs, Z_NO_FLUSH);
        if (rc != Z_OK && rc != Z_STREAM_END) { inflateEnd(&zs); return false; }
        out.insert(out.end(), buf, buf + (sizeof buf - zs.avail_out));
    } while (rc != Z_STREAM_END);
    inflateEnd(&zs);
    return true;
}

}  // namespace

bool RegisterAndroidData(const std::string& dataDir) {
    auto desc = ReadWhole(dataDir + "/res_desc.bin");
    if (desc.empty()) return false;
    Source src{SourceKind::AndroidData, dataDir};
    int id = (int)g_sources.size();
    std::vector<Entry> entries;
    if (!ParseIndex(desc.data(), desc.size(), [&](Entry e) { entries.push_back(std::move(e)); })) return false;
    for (auto& e : entries) {
        auto it = src.chunkBase.find(e.chunk);
        if (it == src.chunkBase.end() || e.offset < it->second) src.chunkBase[e.chunk] = e.offset;
    }
    g_sources.push_back(std::move(src));
    for (auto& e : entries) {
        e.source = id;
        g_files[Lower(e.name.c_str())] = std::move(e);
    }
    return true;
}

// @0x16e7b8 FileManager::RegisterBlobFile. Header: u32 magic 0x75B4A351, u32 index_size, 3 x u32,
// then the index; payloads at 0x14 + index_size + offset (LoadFile seeks blob+0x14+offset).
// UNVERIFIED: the original skips a few names for certain blob types/versions (param_4 0x1b/0x1c).
bool RegisterBlobFile(const std::string& kbfPath) {
    auto blob = ReadWhole(kbfPath);
    if (blob.size() < 20) return false;
    uint32_t magic, indexSize;
    std::memcpy(&magic, blob.data(), 4);
    std::memcpy(&indexSize, blob.data() + 4, 4);
    if (magic != 0x75B4A351u || 20 + (size_t)indexSize > blob.size()) return false;
    int id = (int)g_sources.size();
    g_sources.push_back({SourceKind::Blob, kbfPath, 20 + indexSize});
    return ParseIndex(blob.data() + 20, indexSize, [&](Entry e) {
        e.source = id;
        g_files[Lower(e.name.c_str())] = std::move(e);  // newest registration wins
    });
}

bool RegisterWin8Fallback(const std::string& assetsDir) {
    auto desc = ReadWhole(assetsDir + "/res_desc.bin");
    if (desc.empty()) return false;
    int id = (int)g_sources.size();
    g_sources.push_back({SourceKind::Win8, assetsDir});
    return ParseIndex(desc.data(), desc.size(), [&](Entry e) {
        std::string key = Lower(e.name.c_str());
        if (g_files.count(key)) return;  // fallback only
        e.source = id;
        g_files[key] = std::move(e);
    });
}

bool FileExists(const char* name) { return name && g_files.count(Lower(name)) != 0; }

bool GetFileInfo(const char* name, int& w, int& h, unsigned& bpp, bool quiet) {
    auto it = g_files.find(Lower(name));
    if (it == g_files.end()) {
        if (!quiet) std::printf("FileManager::GetFileInfo() file not found: %s\n", name);
        return false;
    }
    w = (int)it->second.width;
    h = (int)it->second.height;
    bpp = it->second.bpp;
    return true;
}

uint8_t* LoadFile(const char* name, uint32_t& size) {
    size = 0;
    if (!name) return nullptr;
    if (name[0] == '$') {  // '$' prefix: direct file system path (LoadFile/GetFileInfo)
        auto v = ReadWhole(name + 1);
        if (v.empty()) return nullptr;
        auto* p = (uint8_t*)std::malloc(v.size() + 1);
        std::memcpy(p, v.data(), v.size());
        p[v.size()] = 0;
        size = (uint32_t)v.size();
        return p;
    }
    auto it = g_files.find(Lower(name));
    if (it == g_files.end()) return nullptr;
    const Entry& e = it->second;
    const Source& s = g_sources[e.source];
    std::string path;
    long pos = 0;
    switch (s.kind) {
        case SourceKind::AndroidData:
            path = s.path + "/" + std::to_string(e.chunk) + ".jet";  // "data/%u.jet"
            pos = (long)(e.offset - s.chunkBase.at(e.chunk));
            break;
        case SourceKind::Blob:
            path = s.path;
            pos = (long)(s.dataBase + e.offset);
            break;
        case SourceKind::Win8:
            path = s.path + "/res_data.bin";
            pos = (long)e.offset;
            break;
    }
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return nullptr;
    std::vector<uint8_t> raw(e.size);
    std::fseek(f, pos, SEEK_SET);
    size_t got = std::fread(raw.data(), 1, e.size, f);
    std::fclose(f);
    if (got != e.size) return nullptr;
    // Gzip flag (bit0) as in LoadFile; also accept a gzip magic, since some Android entries are gzipped
    // without the flag being set in the data the Java side used.  UNVERIFIED: which of the two the
    // Android Java loader keyed on; the payload bytes are identical either way.
    if ((e.flags & 1) || (raw.size() > 2 && raw[0] == 0x1f && raw[1] == 0x8b)) {
        std::vector<uint8_t> out;
        if (!Inflate(raw.data(), (uint32_t)raw.size(), out)) return nullptr;
        raw.swap(out);
    }
    auto* p = (uint8_t*)std::malloc(raw.size() + 1);
    std::memcpy(p, raw.data(), raw.size());
    p[raw.size()] = 0;
    size = (uint32_t)raw.size();
    return p;
}

void FreeFile(uint8_t* p) { std::free(p); }

size_t EntryCount() { return g_files.size(); }

// ------------------------------------------------------------------------------------- saves
namespace {
std::string g_storagePath;
}

void SetStoragePath(const std::string& path) { g_storagePath = path; }
std::string GetStoragePath() { return g_storagePath; }

bool SaveExists(const char* name) {
    FILE* f = std::fopen((g_storagePath + name).c_str(), "rb");
    if (f) std::fclose(f);
    return f != nullptr;
}

uint8_t* LoadSave(const char* name, uint32_t& size, bool quiet) {
    std::string path = g_storagePath + name;
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        if (!quiet) std::printf("File '%s' cannot be opened\n", name);
        return nullptr;
    }
    std::fseek(f, 0, SEEK_END);
    size = (uint32_t)std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    uint8_t* raw = new uint8_t[size + 1];
    size = (uint32_t)std::fread(raw, 1, size, f);
    std::fclose(f);
    raw[size] = 0;
    uint8_t* data = SystemFuncs::GZIP_Decompress(raw, size);
    delete[] raw;
    return data;
}

void SaveSave(const char* name, const uint8_t* data, uint32_t size, bool online) {
    if (!GameState::IsPlayerCity()) return;
    std::string path = g_storagePath + name;
    uint8_t* gz = SystemFuncs::GZIP_Compress(data, size);
    std::printf("FileManager::EndSave() Compressed save - %d bytes\n", size);
    std::string temp = path, prev = path;
    if (online) {
        temp += "_online";
        prev += "_online";
    }
    temp += "temp";
    prev += "prev";
    std::remove(temp.c_str());
    std::remove(prev.c_str());
    if (FILE* f = std::fopen(temp.c_str(), "wb")) {
        std::fwrite(gz, 1, size, f);
        std::fclose(f);
        if ((f = std::fopen(temp.c_str(), "rb"))) {
            std::fseek(f, 0, SEEK_END);
            uint32_t n = (uint32_t)std::ftell(f);
            std::fclose(f);
            if (n == size) {
                std::rename(path.c_str(), prev.c_str());
                std::rename(temp.c_str(), path.c_str());
                std::printf("FileManager::EndSave() Saving done!\n");
            } else {
                std::printf("Created save file has invalid size of %d instead of %d\n", n, size);
            }
        } else {
            std::puts("Failed to read created save file");
        }
    } else {
        std::puts("Failed to create save file");
    }
    delete[] gz;
}

// ---------------------------------------------------------------------------------------------
// Text input
namespace {
TextInputCallback g_textInputCallback = nullptr;   // 0x611ef8
}  // namespace

void BeginTextInput(TextInputCallback cb, const char32_t* text) {
    TextInput::Enable(text);
    if (SDL_Window* w = SDL_GetKeyboardFocus()) SDL_StartTextInput(w);
    g_textInputCallback = cb;
}

void EndTextInput(const char32_t* text, bool final) {
    if (g_textInputCallback) g_textInputCallback(text, final);
}

void AbortTextInput() {
    TextInput::Disable();
    if (SDL_Window* w = SDL_GetKeyboardFocus()) SDL_StopTextInput(w);
}

}  // namespace FileManager
