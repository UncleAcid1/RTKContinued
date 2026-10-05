#include "game/Setting.h"

#include <pugixml.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>

#include "engine/FileManager.h"
#include "engine/Timer.h"
#include "game/StringTable.h"

uint32_t StringHash(const char* s) {
    uint32_t h = 0x1505;
    for (; *s; ++s) h = h * 0x21 + (uint8_t)*s;
    return h;
}

uint32_t StringHash(const char* begin, const char* end) {
    uint32_t h = 0x1505;
    for (; begin != end; ++begin) h = h * 0x21 + (uint8_t)*begin;
    return h;
}

namespace GameState {
namespace {

std::map<uint32_t, SettingData*> g_settings;   // 0x6133a8
std::vector<AreaInfo> g_areas;                 // 0x612d40
uint32_t g_levelXP[0x100];                     // 0x6129d8

bool IsDelimiter(char c) { return c == ',' || c == '\0' || c == '|' || c == ';' || c == '='; }

// The parsers allocate from ObjectBlockPool<SettingData,256> on the original; nodes live until exit.
SettingData* NewList(SettingData* first) {
    SettingData* d = new SettingData;
    d->child = first;
    return d;
}

// @0x1a2e5c
SettingData* ParseSimpleSetting(const char*& p) {
    const char* start = p;
    if ((unsigned)(*start - '0') < 10) {
        SettingData* d = new SettingData;
        d->isNumber = true;
        d->value = (float)std::strtod(p, nullptr);
        while (!IsDelimiter(*p)) ++p;
        return d;
    }
    while (!IsDelimiter(*p)) ++p;
    if (p == start) return nullptr;
    SettingData* d = new SettingData;
    d->isString = true;
    d->string = new char[p - start + 1];
    std::memcpy(d->string, start, p - start);
    d->string[p - start] = 0;
    return d;
}

// @0x1a3210
SettingData* ParseAssignmentSetting(const char*& p) {
    SettingData* first = ParseSimpleSetting(p);
    if (!first || *p != '=') return first;
    ++p;
    first->next = ParseSimpleSetting(p);
    return NewList(first);
}

// @0x1ac334
SettingData* ParseWallDividedSetting(const char*& p) {
    SettingData* first = ParseSimpleSetting(p);
    if (!first || *p != '|') return first;
    SettingData* last = first;
    while (*p == '|') {
        ++p;
        last->next = ParseSimpleSetting(p);
        last = last->next;
    }
    return NewList(first);
}

// @0x1ac534
SettingData* ParseSemicolonDividedSetting(const char*& p) {
    SettingData* first = ParseWallDividedSetting(p);
    if (!first || *p != ';') return first;
    SettingData* last = first;
    while (*p == ';') {
        ++p;
        last->next = ParseWallDividedSetting(p);
        last = last->next;
    }
    return NewList(first);
}

}  // namespace

void LoadSettings(const char* file) {
    double t0 = Timer::GetTime();
    uint32_t size = 0;
    uint8_t* buf = FileManager::LoadFile(file, size);
    if (!buf) {
        std::printf("GameState::LoadSettings() Failed to load settings config (File not found: %s)\n", file);
        return;
    }
    pugi::xml_document doc;
    pugi::xml_parse_result r = doc.load_buffer_inplace(buf, size, 0x74);
    if (!r) {
        std::printf("GameState::LoadSettings() Failed to load settings config (%s)\n", r.description());
        FileManager::FreeFile(buf);
        return;
    }
    const char* p = doc.child("FBC").child("px").attribute("val").value();
    if (!*p) {
        std::puts("GameState::LoadSettings() Failed to load settings config (element is not found)");
        FileManager::FreeFile(buf);
        return;
    }
    while (*p) {
        const char* eq = std::strchr(p, '=');
        char* name = new char[eq - p + 1];
        std::memcpy(name, p, eq - p);
        name[eq - p] = 0;
        uint32_t h = StringHash(p, eq);
        const char* v = eq + 1;
        g_settings[h] = ParseSemicolonDividedSetting(v);
        g_settings[h]->name = name;   // (crashes on an empty value on the original too)
        const char* comma = std::strchr(p, ',');
        if (!comma) break;
        p = comma + 1;
        if (!*p) break;
    }

    for (pugi::xml_node n = doc.child("FBC").child("ar"); n; n = n.next_sibling("ar")) {
        g_areas.emplace_back();
        AreaInfo& a = g_areas.back();
        a.id = (uint32_t)n.attribute("id").as_int();
        a.name = StringTable::GetString(n.attribute("name").value());
        a.defObjCount = 0;
        const char* d = n.attribute("def_objs").value();
        while (*d) {
            Setting s(ParseAssignmentSetting(d));
            if (*d == ',') ++d;
            a.defObjs[a.defObjCount].id = s.GetChild(0).GetInt();
            a.defObjs[a.defObjCount].count = s.GetChild(1).GetInt();
            ++a.defObjCount;
        }
        a.x = n.attribute("x").as_int();
        a.y = n.attribute("y").as_int();
        a.w = n.attribute("w").as_int();
        a.h = n.attribute("h").as_int();
        a.c = n.attribute("c").as_int();
        a.c2 = n.attribute("c2").as_int();
        a.level = (uint32_t)n.attribute("l").as_int();
        a.a = n.attribute("a").as_bool();
    }
    FileManager::FreeFile(buf);   // (the settings keep their own copies of every string)

    double t1 = Timer::GetTime();
    std::printf("GameState::LoadSettings() Loaded settings in %.3fms\n", (t1 - t0) * 1000.0);
    g_levelXP[0] = 0;
    char key[128];
    for (unsigned i = 1;; ++i) {
        std::snprintf(key, sizeof key, "level_exp_%d", i);
        auto it = g_settings.find(StringHash(key));
        if (it == g_settings.end()) break;
        g_levelXP[i] = (uint32_t)(int)it->second->value + g_levelXP[i - 1];
    }
}

float GetSetting(const char* name) {
    SettingData*& d = g_settings[StringHash(name)];
    if (!d) {
        std::fprintf(stderr, "ERROR: GameState::GetSetting() Setting '%s' has an empty value in the dynamic configuration\n", name);
        return 0.f;
    }
    if (!d->isNumber)
        std::fprintf(stderr, "ERROR: GameState::GetSetting() Setting '%s' is not a number\n", name);
    return d->value;
}

void SetSetting(const char* name, float value) {
    uint32_t h = StringHash(name);
    if (!g_settings.count(h)) {
        SettingData* d = new SettingData;
        d->isNumber = true;
        d->value = value;
        g_settings[h] = d;
        d->name = new char[std::strlen(name) + 1];
        std::strcpy(d->name, name);
    } else if (SettingData* d = g_settings[h]) {
        d->value = value;
    }
}

bool SettingExists(const char* name) { return g_settings.count(StringHash(name)) != 0; }

uint32_t GetLevelXP(unsigned level) { return g_levelXP[level]; }

bool GetAreaInfo(uint32_t id, AreaInfo& out) {
    for (const AreaInfo& a : g_areas)
        if (a.id == id) {
            out = a;
            return true;
        }
    return false;
}

unsigned GetAreaCount() { return (unsigned)g_areas.size(); }
const AreaInfo* EnumAreaInfo(unsigned i) { return &g_areas[i]; }

bool HasAreaForLevel(unsigned level) {
    for (const AreaInfo& a : g_areas)
        if (a.level == level) return true;
    return false;
}

}  // namespace GameState

using GameState::SettingData;

Setting::Setting(SettingData* d) : data(d) {
    if (!d) std::fprintf(stderr, "ERROR: Setting::Setting() setting cannot be NULL\n");
}

Setting::Setting(const char* name) {
    if (!GameState::SettingExists(name))
        std::fprintf(stderr, "ERROR: Setting::Setting(%s) Setting doesn't exist in dynamic configuration\n", name);
    data = GameState::g_settings[StringHash(name)];
    if (!data)
        std::fprintf(stderr, "ERROR: Setting::Setting(%s) Setting has an empty value in the dynamic configuration\n", name);
}

bool Setting::IsString() const {
    if (data) return data->isString;
    std::fprintf(stderr, "ERROR: Setting::IsString() setting is not initialized\n");
    return false;
}

bool Setting::IsNumber() const {
    if (data) return data->isNumber;
    std::fprintf(stderr, "ERROR: Setting::IsNumber() setting is not initialized\n");
    return false;
}

unsigned Setting::GetChildrenCount() const {
    if (!data) {
        std::fprintf(stderr, "ERROR: Setting::GetChildrenCount() setting is not initialized\n");
        return 0;
    }
    unsigned n = 0;
    for (SettingData* c = data->child; c; c = c->next) ++n;
    return n;
}

const char* Setting::GetString() const {
    if (data && data->isString) return data->string;
    std::fprintf(stderr, "ERROR: Setting::GetString() setting is not a string\n");
    return "";
}

int Setting::GetInt() const {
    if (data && data->isNumber) return (int)(data->value + 0.5f);
    std::fprintf(stderr, "ERROR: Setting::GetInt() setting is not a number\n");
    return 0;
}

float Setting::GetFloat() const {
    if (!data || !data->isNumber)
        std::fprintf(stderr, "ERROR: Setting::GetFloat() setting is not a number\n");
    return data->value;   // (null on the original too)
}

Setting Setting::GetNext() const {
    if (!data) {
        std::fprintf(stderr, "ERROR: Setting::GetNext() setting is not initialized\n");
        return Setting("");
    }
    if (!data->next) std::fprintf(stderr, "ERROR: Setting::GetNext() there is no next setting in the list\n");
    return Setting(data->next);
}

Setting Setting::GetChild(unsigned i) const {
    if (!data) {
        std::fprintf(stderr, "ERROR: Setting::GetChild() setting is not initialized\n");
        return Setting("");
    }
    if (i >= GetChildrenCount())
        std::fprintf(stderr, "ERROR: Setting::GetChild(%d) setting doesn't have that many children (have only %d)\n",
                     i + 1, GetChildrenCount());
    SettingData* c = data->child;
    while (i != 0 && c) {
        --i;
        c = c->next;
    }
    return Setting(c);
}
