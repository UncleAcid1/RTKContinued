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

// ---- SWPrintf (global functions on the original) ----

std::u32string SWPrintf(unsigned size, const char32_t* f, std::initializer_list<StringArgument> argList) {
    static const char* kTypeNames[6] = {"none", "integer", "long integer", "double", "string", "wide string"};
    std::u32string out;
    if (!f) return out;
    const StringArgument* args = argList.begin();
    const size_t argCount = argList.size();
    unsigned argi = 0;
    // Appends one character; false once the text is full (the original then cuts the last one).
    auto put = [&](char32_t c) {
        if (out.size() == size) return false;
        out.push_back(c);
        return true;
    };
    auto full = [&] { return out.size() == size; };
    auto arg = [&](unsigned k) -> const StringArgument* { return k < argCount ? &args[k] : nullptr; };
    char32_t c = *f;
    while (c != 0) {
        if (c != U'%') {
            if (!put(c)) break;
            c = *++f;
            continue;
        }
        const char32_t* p = f + 1;
        char32_t n = *p;
        if (n == 0) {   // a lone '%' at the end is written
            if (!put(c)) break;
            c = *++f;
            continue;
        }
        if (n == U'%') {
            if (!put(U'%')) break;
            f += 2;
            c = *f;
            continue;
        }
        const StringArgument* a = arg(argi);
        if (!a || a->type == StringArgument::kNone) {
            std::fprintf(stderr, "ERROR: SWPrintf expects more arguments (%d specified)\n", argi);
            f = p;
            c = *f;
            continue;
        }
        bool plus = n == U'+';
        if (plus) n = *++p;
        bool zero = n == U'0';
        int prec = 0;
        if (zero) {
            prec = (int)(p[1] - U'0');
            p += 2;
            n = *p;
        }
        if (n == U'.') {
            prec = (int)(p[1] - U'0');
            p += 2;
            n = *p;
        }
        bool group = false;
        if (n == U'$') {
            n = *++p;
            group = true;   // the separator 0x60f070 is ","
        }
        f = p;
        bool stop = false;
        if (n == U'u' || n == U'd') {
            const StringArgument* x = arg(argi);
            if (x->type != StringArgument::kInt) {
                std::fprintf(stderr, "ERROR: SWPrintf expects integer at %%d (argument %d), but %s was passed\n", argi + 1,
                             kTypeNames[x->type]);
                break;
            }
            char num[32];
            bool u = n == U'u';
            if (zero) {   // (".n" alone only affects %f)
                // ("%+.*u" / "%+u": the sign flag does nothing for unsigned conversions.)
                if (u) std::snprintf(num, sizeof num, "%.*u", prec, (unsigned)x->i);
                else std::snprintf(num, sizeof num, plus ? "%+.*d" : "%.*d", prec, x->i);
            } else {
                if (u) std::snprintf(num, sizeof num, "%u", (unsigned)x->i);
                else std::snprintf(num, sizeof num, plus ? "%+d" : "%d", x->i);
            }
            size_t len = std::strlen(num);
            size_t first = 0;   // digits before the first ","
            if (group) {
                if (len < 4) group = false;
                else first = len % 3 == 0 ? 3 : (len - 3) % 3;
            }
            for (size_t k = 0; k < len && !stop; ++k) {
                if (group && k != 0 && (k == first || (k > first && (k - first) % 3 == 0)))
                    stop = !put(U',');
                if (!stop) stop = !put((char32_t)(unsigned char)num[k]);
            }
            if (stop || full()) break;
            n = *++f;
            ++argi;
        }
        if (n == U'f') {
            const StringArgument* x = arg(argi);
            if (!x || x->type != StringArgument::kDouble) {
                std::fprintf(stderr, "ERROR: SWPrintf expects floating point number at %%f (argument %d), but %s was passed\n",
                             argi + 1, kTypeNames[x ? x->type : 0]);
                break;
            }
            char num[64];
            std::snprintf(num, sizeof num, "%.*f", prec, x->d);
            for (const char* q = num; *q && !stop; ++q) stop = !put((char32_t)(unsigned char)*q);
            if (stop || full()) break;
            n = *++f;
            ++argi;
        }
        if (n == U's') {
            const StringArgument* x = arg(argi);
            if (!x || x->type != StringArgument::kWide) {
                std::fprintf(stderr, "ERROR: SWPrintf expects Unicode string at %%s (argument %d), but %s was passed\n",
                             argi + 1, kTypeNames[x ? x->type : 0]);
                break;
            }
            for (const char32_t* q = x->w ? x->w : U""; *q && !stop; ++q) stop = !put(*q);
            if (stop || full()) break;
            n = *++f;
            ++argi;
        }
        if (n == U'S') {
            const StringArgument* x = arg(argi);
            if (!x || x->type != StringArgument::kUtf8) {
                std::fprintf(stderr, "ERROR: SWPrintf expects utf8 string at %%S (argument %d), but %s was passed\n",
                             argi + 1, kTypeNames[x ? x->type : 0]);
                break;
            }
            // (The original copies the bytes as characters, without UTF-8 decoding.)
            for (const char* q = x->s ? x->s : ""; *q && !stop; ++q) stop = !put((char32_t)(unsigned char)*q);
            if (stop || full()) break;
            n = *++f;
            ++argi;
        }
        if (n == U'c') {
            const StringArgument* x = arg(argi);
            if (!x || x->type != StringArgument::kInt) {
                std::fprintf(stderr, "ERROR: SWPrintf expects character at %%c (argument %d), but %s was passed\n",
                             argi + 1, kTypeNames[x ? x->type : 0]);
                break;
            }
            if (!put((char32_t)x->i)) break;
            ++argi;
            n = *++f;
        }
        c = n;   // an unknown specifier character is written as text
    }
    if (out.size() == size && size != 0) out.pop_back();
    return out;
}

const char32_t* ToWideString(int n) {
    static std::u32string ring[16];   // 0x616140 counter, 16 x 0x80-byte buffers
    static unsigned next = 0;
    std::u32string& s = ring[next++ & 0xf];
    s = SWPrintf(0x20, U"%d", {n});
    return s.c_str();
}

