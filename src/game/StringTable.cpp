#include "game/StringTable.h"

#include <pugixml.hpp>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <unordered_map>

#include "engine/FileManager.h"

namespace StringTable {
namespace {

// The original keeps a 2048-bucket chained hash keyed by StringInsensitiveHash; new entries go to
// the head of their chain, so for duplicate keys the last one loaded wins. A map keyed by the
// lower-cased key with overwrite-on-insert has the same lookup behaviour.
std::unordered_map<std::string, std::u32string> g_table;
char g_lang[16] = "EN";   // @0x228f40 (strcpy'd by SetLanguage)
int g_langId = 0;

std::string Lower(const char* s) {
    std::string k(s);
    for (char& c : k) c = (char)std::tolower((unsigned char)c);
    return k;
}

const std::u32string* Find(const std::string& key) {
    auto it = g_table.find(Lower(key.c_str()));
    return it == g_table.end() ? nullptr : &it->second;
}

}  // namespace

std::u32string DecodeUtf8(const char* text) {
    // @0x22cd58: lead byte masks, continuation checks and the skip-one-byte-on-error rule as inlined
    // in Init (the counting pass before it uses the same rules).
    const unsigned char* p = (const unsigned char*)text;
    size_t n = std::strlen(text);
    std::u32string out;
    while (n) {
        unsigned c = p[0];
        if (!(c & 0x80)) { out += (char32_t)c; ++p; --n; continue; }
        if (n >= 2 && c - 0xc0 <= 0x1f && (p[1] & 0xc0) == 0x80) {
            out += (char32_t)(((c & 0x3f) << 6) | (p[1] & 0x3f));
            p += 2; n -= 2; continue;
        }
        if (n >= 3 && c - 0xe0 <= 0xf && (p[1] & 0xc0) == 0x80 && (p[2] & 0xc0) == 0x80) {
            out += (char32_t)(((c & 0x1f) << 12) | ((p[1] & 0x3f) << 6) | (p[2] & 0x3f));
            p += 3; n -= 3; continue;
        }
        if (n >= 4 && c - 0xf0 <= 7 && (p[1] & 0xc0) == 0x80 && (p[2] & 0xc0) == 0x80 &&
            (p[3] & 0xc0) == 0x80) {
            out += (char32_t)(((c & 0x0f) << 18) | ((p[1] & 0x3f) << 12) | ((p[2] & 0x3f) << 6) |
                              (p[3] & 0x3f));
            p += 4; n -= 4; continue;
        }
        ++p; --n;   // invalid byte: skipped
    }
    return out;
}

// @0x22cae0
bool Init(const char* file, bool dryRun, bool renameToCurrentLanguage) {
    uint32_t size = 0;
    uint8_t* data = FileManager::LoadFile(file, size);
    if (!data || !size) {
        std::printf("StringTable: cannot load %s\n", file);
        FileManager::FreeFile(data);
        return false;
    }
    pugi::xml_document doc;
    // load_buffer_inplace(..., 0x74 = parse_default, encoding_auto)
    pugi::xml_parse_result r = doc.load_buffer_inplace(data, size, pugi::parse_default, pugi::encoding_auto);
    if (!r) {
        std::printf("StringTable: %s\n", r.description());
        FileManager::FreeFile(data);
        return false;
    }
    for (pugi::xml_node t = doc.child("ts").child("t"); t; t = t.next_sibling("t")) {
        std::string key = t.attribute("i").value();
        if (renameToCurrentLanguage && key.size() > 3) {
            key[0] = g_lang[0];
            key[1] = g_lang[1];
        }
        const char* text = t.child_value();
        // Empty texts stay empty (the key itself is used only in the SetEmptyNameReplacement debug mode).
        std::u32string value = *text ? DecodeUtf8(text) : std::u32string();
        if (!dryRun) g_table[Lower(key.c_str())] = std::move(value);
    }
    FileManager::FreeFile(data);
    return true;
}

void Deinit() { g_table.clear(); }

void SetLanguage(const char* code, int langId) {
    std::snprintf(g_lang, sizeof g_lang, "%s", code);
    g_langId = langId;
}
int GetLangID() { return g_langId; }
const char* GetLanguage() { return g_lang; }

// @0x228ee8
const char32_t* GetString(const char* key) {
    const std::u32string* s = Find(std::string(g_lang) + "_" + key);
    if (s && !s->empty()) return s->c_str();
    s = Find(std::string("EN_") + key);
    return s ? s->c_str() : nullptr;
}

std::u32string GetTimeString(int t, bool compact) {
    std::u32string units;
    if (const char32_t* s = GetString("TIME_STR")) units = s;
    units = units.substr(0, 31);   // SWPrintf(buf, 0x20, TIME_STR)
    size_t c1 = units.find(U','), c2 = c1 == std::u32string::npos ? c1 : units.find(U',', c1 + 1);
    if (c1 == std::u32string::npos || c2 == std::u32string::npos) {
        std::fprintf(stderr, "Incorrect TIME_STR\n");   // ErrorReporter::Printf
        return U"Incorrect TIME_STR";
    }
    std::u32string h = units.substr(0, c1), m = units.substr(c1 + 1, c2 - c1 - 1), s = units.substr(c2 + 1);
    char buf[32];
    std::u32string unit;
    if (t >= 3600) {
        if (compact) std::snprintf(buf, sizeof buf, "%d ", t / 3600);
        else std::snprintf(buf, sizeof buf, "%02d:%02d ", t / 3600, t % 3600 / 60);
        unit = h;
    } else if (t > 59) {
        if (compact) std::snprintf(buf, sizeof buf, "%d ", t % 3600 / 60);
        else std::snprintf(buf, sizeof buf, "%02d:%02d ", t % 3600 / 60, t % 60);
        unit = m;
    } else {
        std::snprintf(buf, sizeof buf, "%d ", t);
        unit = s;
    }
    std::u32string out(buf, buf + std::strlen(buf));
    out += unit;
    return out.substr(0, 31);
}

std::u32string GetCountableString(const char32_t* forms, int n) {
    int form;
    if (n % 10 == 1) form = (unsigned)(n - 10) > 10u ? 0 : 2;
    else form = ((unsigned)(n % 10 - 2) > 2u || (unsigned)(n - 10) < 0xbu) ? 2 : 1;
    std::u32string f = forms ? forms : U"";
    if (f.empty() || f[0] != U'{') {
        std::fprintf(stderr, "Countable string format is incorrect\n");
        return f;
    }
    size_t p = 1;
    for (int i = 0; i < form; ++i) {
        size_t bar = f.find(U'|', p);
        if (bar == std::u32string::npos) {
            std::fprintf(stderr, "Countable string format is incorrect\n");
            return f;
        }
        p = bar + 1;
    }
    size_t end = f.find_first_of(U"|}", p);
    return f.substr(p, end == std::u32string::npos ? std::u32string::npos : end - p);
}

std::u32string GetNumericTimeString(int t, bool) {
    char buf[64];
    std::u32string out;
    if (t > 0x1517f) {
        int d = t / 86400, r = t - d * 86400;
        std::snprintf(buf, sizeof buf, "%d ", d);
        out.assign(buf, buf + std::strlen(buf));
        out += GetCountableString(GetString("COUNT_DAYS"), d);
        std::snprintf(buf, sizeof buf, " %02d:%02d:%02d", r / 3600, r % 3600 / 60, r % 60);
    } else if (t >= 3600) {
        std::snprintf(buf, sizeof buf, "%02d:%02d:%02d", t / 3600, t % 3600 / 60, t % 60);
    } else if (t > 59) {
        std::snprintf(buf, sizeof buf, "%02d:%02d", t % 3600 / 60, t % 60);
    } else {
        std::snprintf(buf, sizeof buf, "00:%02d", t);
    }
    out.append(buf, buf + std::strlen(buf));
    return out.substr(0, 31);
}

// @0x22c980 ("%s_%s" with the current language, then "EN_%s")
bool StringExists(const char* key) {
    return Find(std::string(g_lang) + "_" + key) || Find(std::string("EN_") + key);
}

}  // namespace StringTable
