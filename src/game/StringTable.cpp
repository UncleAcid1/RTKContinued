#include "game/StringTable.h"

#include <pugixml.hpp>

#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <vector>
#include <unordered_map>

#include "engine/FileManager.h"
#include "game/GameData.h"
#include "game/GameState.h"
#include "game/Items.h"

namespace StringTable {
namespace {

// The original keeps a 2048-bucket chained hash keyed by StringInsensitiveHash; new entries go to
// the head of their chain, so for duplicate keys the last one loaded wins. A map keyed by the
// lower-cased key with overwrite-on-insert has the same lookup behaviour.
std::unordered_map<std::string, std::u32string> g_table;
bool g_emptyNameReplacement = false;   // 0x615700
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
        // Empty texts stay empty, or become their key in the SetEmptyNameReplacement debug mode (one
        // character per byte of the key).
        std::u32string value = *text ? DecodeUtf8(text) : std::u32string();
        if (!*text && g_emptyNameReplacement)
            for (const char* k = t.attribute("i").value(); *k; ++k) value += (char32_t)(unsigned char)*k;
        if (!dryRun) g_table[Lower(key.c_str())] = std::move(value);
    }
    FileManager::FreeFile(data);
    return true;
}

void Deinit() { g_table.clear(); }

// @0x2277cc StringTable::SetEmptyNameReplacement
void SetEmptyNameReplacement() { g_emptyNameReplacement = true; }

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

// @0x228d7c: the "<lang>_" record if there is one, else the "EN_" record: whether its text is not
// empty (false without either).
bool StringNotEmpty(const char* key) {
    if (const std::u32string* s = Find(std::string(g_lang) + "_" + key)) return !s->empty();
    if (const std::u32string* s = Find(std::string("EN_") + key)) return !s->empty();
    return false;
}

// @0x22c908: GetString of the key printed from fmt (vsnprintf into 0x100 characters).
const char32_t* GetStringF(const char* fmt, ...) {
    char key[0x100];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(key, sizeof key, fmt, ap);
    va_end(ap);
    return GetString(key);
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

std::u32string GetGenderString(const char32_t* forms, bool male) {
    std::u32string f = forms ? forms : U"";
    size_t start = 0;
    if (!male) {
        size_t comma = f.find(U',');
        if (comma == std::u32string::npos) {
            std::fprintf(stderr, "Gender string format is incorrect\n");
            return f;
        }
        start = comma + 1;
    }
    size_t end = f.find(U',', start);
    return f.substr(start, end == std::u32string::npos ? std::u32string::npos : end - start);
}

// No text gives "ERROR", a missing form "Declination string format is incorrect: <forms>"
// (and the error printed).
std::u32string GetDeclinationString(const char32_t* forms, unsigned size, int declination) {
    if (!forms) {
        std::fprintf(stderr, "Declination string format is incorrect\n");   // ErrorReporter::Printf
        return U"ERROR";
    }
    std::u32string f = forms;
    size_t start = 0;
    for (int i = 0; i < declination; ++i) {
        size_t comma = f.find(U',', start);
        if (comma == std::u32string::npos) {
            std::fprintf(stderr, "Declination string format is incorrect\n");
            return SWPrintf(size, U"Declination string format is incorrect: %s", {forms});
        }
        start = comma + 1;
    }
    size_t end = f.find(U',', start);
    return f.substr(start, end == std::u32string::npos ? std::u32string::npos : end - start);
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

std::u32string GetNumericTimeString(int t, bool full) {
    char buf[64];
    std::u32string out;
    const int h = t / 3600 % 24, m = t % 3600 / 60, sec = t % 60;
    if (t > 0x1517f) {
        int d = t / 86400;
        std::snprintf(buf, sizeof buf, "%d ", d);
        out.assign(buf, buf + std::strlen(buf));
        out += GetCountableString(GetString("COUNT_DAYS"), d);
        if (full) std::snprintf(buf, sizeof buf, " %02d:%02d:%02d", h, m, sec);
        else std::snprintf(buf, sizeof buf, " %02d:%02d", h, m);
    } else if (full) {
        std::snprintf(buf, sizeof buf, "%02d:%02d:%02d", t / 3600, m, sec);
    } else if (t >= 3600) {
        std::snprintf(buf, sizeof buf, "%02d:%02d", t / 3600, m);
    } else if (t > 59) {
        std::snprintf(buf, sizeof buf, "%02d:%02d", m, sec);
    } else {
        std::snprintf(buf, sizeof buf, "00:%02d", t);
    }
    out.append(buf, buf + std::strlen(buf));
    return out.substr(0, 31);
}

std::u32string GetExtendedTimeString(unsigned size, int t, bool) {
    auto unit = [](const char* key, int n) { return GetCountableString(GetString(key), n); };
    if (t >= 0x15180) {
        const int d = t / 0x15180, h = t % 0x15180 / 0xe10, m = t % 0xe10 / 0x3c;
        const std::u32string ds = unit("COUNT_DAYS", d), hs = unit("COUNT_HOURS", h), ms = unit("COUNT_MINUTES", m);
        if (h != 0 && m != 0)
            return SWPrintf(size, U"%d %s %d %s %d %s", {d, ds.c_str(), h, hs.c_str(), m, ms.c_str()});
        if (h != 0) return SWPrintf(size, U"%d %s %d %s", {d, ds.c_str(), h, hs.c_str()});
        if (m != 0) return SWPrintf(size, U"%d %s %d %s", {d, ds.c_str(), m, ms.c_str()});
        return SWPrintf(size, U"%d %s", {d, ds.c_str()});
    }
    if (t >= 0xe10) {
        const int h = t / 0xe10, m = t % 0xe10 / 0x3c;
        const std::u32string hs = unit("COUNT_HOURS", h), ms = unit("COUNT_MINUTES", m);
        if (m == 0) return SWPrintf(size, U"%d %s", {h, hs.c_str()});
        return SWPrintf(size, U"%d %s %d %s", {h, hs.c_str(), m, ms.c_str()});
    }
    if (t >= 0x3c) {
        const int m = t % 0xe10 / 0x3c, s = t % 0x3c;
        const std::u32string ms = unit("COUNT_MINUTES", m), ss = unit("COUNT_SECONDS", s);
        if (s == 0) return SWPrintf(size, U"%d %s", {m, ms.c_str()});
        return SWPrintf(size, U"%d %s %d %s", {m, ms.c_str(), s, ss.c_str()});
    }
    const std::u32string ss = unit("COUNT_SECONDS", t);
    return SWPrintf(size, U"%d %s", {t, ss.c_str()});
}

// @0x22c980 ("%s_%s" with the current language, then "EN_%s")
bool StringExists(const char* key) {
    return Find(std::string(g_lang) + "_" + key) || Find(std::string("EN_") + key);
}

namespace {
// FUN_0022c08c: a bracketed word's text (the word itself when it names nothing).
const char32_t* ResolveDecoration(const char32_t* w) {
    if (std::u32string_view(w) == U"player") return GameState::GetPlayerName();
    if (std::u32string_view(w) == U"castle") return GameState::GetCastleName();
    size_t len = std::char_traits<char32_t>::length(w);
    if (len - 5 < 0x1a) {   // 5..30 characters, narrowed to bytes
        char buf[32];
        for (size_t i = 0; i < len; ++i) buf[i] = (char)w[i];
        buf[len] = 0;
        if (std::memcmp(buf, "item:", 5) == 0) {
            if (const Items::ItemInfo* it = Items::GetItemInfo((uint32_t)std::atoi(buf + 5))) return it->title;
        }
        if (len > 8 && std::memcmp(buf, "building:", 9) == 0) {
            if (const GameData::BuildingData* b = GameData::GetBuilding((uint32_t)std::atoi(buf + 9)))
                return GetString(b->name.c_str());
        }
    }
    return w;
}
}  // namespace

std::u32string GetUndecoratedString(const char32_t* text, unsigned size, std::u32string* last, unsigned lastSize,
                                    const int* counts) {
    if (!text) {
        if (last) last->clear();
        return U"";
    }
    // WSplitter(text, L"["): every part, empty ones included; each part after the first ends at ']'.
    std::vector<std::u32string> parts;
    std::u32string all = text;
    for (size_t start = 0;;) {
        size_t open = all.find(U'[', start);
        parts.push_back(all.substr(start, open - start));
        if (open == std::u32string::npos) break;
        start = open + 1;
    }
    for (size_t i = 1; i < parts.size(); ++i) {
        size_t close = parts[i].find(U']');
        if (close != std::u32string::npos) parts[i].resize(close);
    }
    const char32_t* format = ResolveDecoration(parts[0].c_str());
    const char32_t* a[8];
    for (size_t i = 0; i < 8; ++i) {
        if (i + 1 < parts.size()) a[i] = ResolveDecoration(parts[i + 1].c_str());
        else a[i] = counts && i < 3 ? ToWideString(counts[i]) : U"";
    }
    std::u32string out = SWPrintf(size, format, {a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]});
    if (last) {
        if (parts.size() < 2) last->clear();
        else *last = SWPrintf(lastSize, U"%s", {parts.back().c_str()});
    }
    return out;
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

const char32_t* ToWideString(const char32_t* format, std::initializer_list<StringArgument> args) {
    static std::u32string ring[4];    // 0x61593c counter, 4 x 0x80-character buffers
    static unsigned next = 0;
    std::u32string& s = ring[next++ & 3];
    s = SWPrintf(0x80, format, args);
    return s.c_str();
}

const char32_t* ToWideString(int n) {
    static std::u32string ring[16];   // 0x616140 counter, 16 x 0x80-byte buffers
    static unsigned next = 0;
    std::u32string& s = ring[next++ & 0xf];
    s = SWPrintf(0x20, U"%d", {n});
    return s.c_str();
}

