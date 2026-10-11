#include "engine/SystemFuncs.h"

#include <SDL3/SDL.h>
#include <zlib.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>

#include "engine/FileManager.h"

namespace SystemFuncs {
namespace {
// "<file>/<key>"; a null file is the default preferences file
std::map<std::string, int> g_ints;
std::map<std::string, bool> g_bools;
std::map<std::string, std::string> g_strings;
std::string Key(const char* key, const char* file) { return std::string(file ? file : "") + "/" + key; }

// PORT: the settings file. One setting per line: type (i, b or s), a space, "<file>/<key>", '=',
// the value (in strings a backslash and a line break are written \\ and \n).
std::string SettingsPath() { return FileManager::GetStoragePath() + "settings.txt"; }

void SaveSettings() {
    std::FILE* f = std::fopen(SettingsPath().c_str(), "wb");
    if (!f) return;
    for (auto& [k, v] : g_ints) std::fprintf(f, "i %s=%d\n", k.c_str(), v);
    for (auto& [k, v] : g_bools) std::fprintf(f, "b %s=%d\n", k.c_str(), v ? 1 : 0);
    for (auto& [k, v] : g_strings) {
        std::string e;
        for (char c : v) {
            if (c == '\\') e += "\\\\";
            else if (c == '\n') e += "\\n";
            else e += c;
        }
        std::fprintf(f, "s %s=%s\n", k.c_str(), e.c_str());
    }
    std::fclose(f);
}

void LoadSettings() {
    g_ints.clear();
    g_bools.clear();
    g_strings.clear();
    std::FILE* f = std::fopen(SettingsPath().c_str(), "rb");
    if (!f) return;
    std::string line;
    for (int c; (c = std::fgetc(f)) != EOF || !line.empty();) {
        if (c != '\n' && c != EOF) {
            line += (char)c;
            continue;
        }
        size_t eq = line.find('=');
        if (line.size() > 2 && line[1] == ' ' && eq != std::string::npos) {
            std::string key = line.substr(2, eq - 2), value = line.substr(eq + 1);
            if (line[0] == 'i') g_ints[key] = std::atoi(value.c_str());
            else if (line[0] == 'b') g_bools[key] = value == "1";
            else if (line[0] == 's') {
                std::string d;
                for (size_t i = 0; i < value.size(); ++i) {
                    if (value[i] == '\\' && i + 1 < value.size()) d += value[++i] == 'n' ? '\n' : value[i];
                    else d += value[i];
                }
                g_strings[key] = d;
            }
        }
        line.clear();
        if (c == EOF) break;
    }
    std::fclose(f);
}

uint8_t* Copy(const std::vector<uint8_t>& v, uint32_t& size) {
    uint8_t* p = new uint8_t[v.size() + 1];
    if (!v.empty()) std::memcpy(p, v.data(), v.size());
    p[v.size()] = 0;
    size = (uint32_t)v.size();
    return p;
}
}  // namespace

void Init() { LoadSettings(); }

int getCurrentLanguage() { return GetSetting_Int("selected_language", 0, "MyCountry_flags"); }

bool HasSelectedLanguage() { return g_ints.count(Key("selected_language", "MyCountry_flags")) != 0; }

int GetSystemLanguage() {
    // the codes SystemFuncs::getCurrentLanguage (0x3da2f0) knows, by language id
    static const char* const kCodes[9] = {"en", "ru", "de", "lv", "es", "it", "fr", "ko", "ja"};
    int id = 0, count = 0;
    SDL_Locale** locales = SDL_GetPreferredLocales(&count);
    if (locales && count > 0 && locales[0]->language)
        for (int i = 1; i < 9; ++i)
            if (!std::strcmp(locales[0]->language, kCodes[i])) id = i;
    SDL_free(locales);
    return id;
}

int GetSetting_Int(const char* key, int def, const char* file) {
    auto it = g_ints.find(Key(key, file));
    return it == g_ints.end() ? def : it->second;
}
void SetSetting_Int(const char* key, int value, const char* file) {
    g_ints[Key(key, file)] = value;
    SaveSettings();
}

bool GetSetting_Bool(const char* key, bool def, const char* file) {
    auto it = g_bools.find(Key(key, file));
    return it == g_bools.end() ? def : it->second;
}
void SetSetting_Bool(const char* key, bool value, const char* file) {
    g_bools[Key(key, file)] = value;
    SaveSettings();
}

std::string GetSetting_String(const char* key, const char* def, const char* file) {
    auto it = g_strings.find(Key(key, file));
    return it == g_strings.end() ? std::string(def) : it->second;
}
void SetSetting_String(const char* key, const char* value, const char* file) {
    g_strings[Key(key, file)] = value;
    SaveSettings();
}

std::string getGUID() { return std::string(); }
std::string getSID() { return std::string(); }

uint8_t* GZIP_Compress(const void* data, uint32_t& size) {
    static const uint8_t header[10] = {0x1f, 0x8b, 8, 0, 0, 0, 0, 0, 0, 0};
    std::vector<uint8_t> out(header, header + 10);
    z_stream zs{};
    deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
    zs.next_in = (Bytef*)data;
    zs.avail_in = size;
    uint8_t buf[0x1000];
    int rc;
    do {
        zs.next_out = buf;
        zs.avail_out = sizeof buf;
        rc = deflate(&zs, Z_FINISH);
        out.insert(out.end(), buf, buf + (sizeof buf - zs.avail_out));
    } while (rc == Z_OK);
    deflateEnd(&zs);
    uint32_t crc = (uint32_t)crc32(0, (const Bytef*)data, size);
    for (int k = 0; k < 4; ++k) out.push_back((uint8_t)(crc >> (k * 8)));
    for (int k = 0; k < 4; ++k) out.push_back((uint8_t)(size >> (k * 8)));
    return Copy(out, size);
}

uint8_t* GZIP_Decompress(const void* data, uint32_t& size) {
    const uint8_t* p = (const uint8_t*)data;
    if (size < 2 || p[0] != 0x1f || p[1] != 0x8b) return Copy(std::vector<uint8_t>(p, p + size), size);
    std::vector<uint8_t> out;
    z_stream zs{};
    inflateInit2(&zs, 16 + MAX_WBITS);
    zs.next_in = (Bytef*)p;
    zs.avail_in = size;
    uint8_t buf[0x10000];
    int rc;
    do {
        zs.next_out = buf;
        zs.avail_out = sizeof buf;
        rc = inflate(&zs, Z_NO_FLUSH);
        out.insert(out.end(), buf, buf + (sizeof buf - zs.avail_out));
    } while (rc == Z_OK);
    inflateEnd(&zs);
    return Copy(out, size);
}

}  // namespace SystemFuncs
