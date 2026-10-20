// Tavern: the warrior list HireTroopsWindow offers (see Tavern.h).
#include "game/Tavern.h"

#include <pugixml.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

#include "engine/FileManager.h"
#include "game/Rand48.h"
#include "game/Setting.h"

namespace Tavern {
namespace {
std::deque<TavernInfo> g_pool;      // 0x6169e0 (the original's 0x80-entry blocks)
std::vector<TavernInfo*> g_list;    // 0x6169f0

// A new entry at the end of the list (the original's pool allocation and push_back).
TavernInfo* Add() {
    g_pool.emplace_back();
    g_list.push_back(&g_pool.back());
    return g_list.back();
}
}  // namespace

// @0x23d86c: a random HP in [hpMin, hpMax); type 1 always hpMin.
int TavernInfo::GenerateHP() const {
    if (hpMax == hpMin || type == 1) return hpMin;
    return (int)(Rand48::lrand48() % (long)(hpMax - hpMin)) + hpMin;
}

// @0x23ddc8
TavernInfo* EnumTavernInfo(unsigned i) { return i < g_list.size() ? g_list[i] : nullptr; }

// @0x23d910
TavernInfo* GetTavernInfo(uint32_t id) {
    for (TavernInfo* t : g_list)
        if (t->id == id) return t;
    return nullptr;
}

// @0x23d8a8
// @0x23d978
void ChangeWarriorAttribute(uint32_t id, uint32_t copyFrom, const char* name, const char* value) {
    TavernInfo* t = GetTavernInfo(id);
    if (!t) {
        TavernInfo* from = GetTavernInfo(copyFrom);
        // PORT: the original's new pool entry is not cleared (a copy, or what the pool held);
        // here it starts zeroed, so without a copy its id stays 0 unless this sets "id".
        t = Add();
        if (from) *t = *from;
    }
    const int v = std::atoi(value);
    if (!std::strcmp(name, "id")) t->id = (uint32_t)v;
    else if (!std::strcmp(name, "soldier_id")) t->entity = (uint32_t)v;
    else if (!std::strcmp(name, "type")) t->type = (uint32_t)v;
    else if (!std::strcmp(name, "slot")) t->slot = (uint32_t)v;
    else if (!std::strcmp(name, "hp_min")) t->hpMin = t->hpMax = v;   // (as the original: both)
    else if (!std::strcmp(name, "hp_max")) t->hpMax = v;
    else if (!std::strcmp(name, "damage")) t->attack = (uint32_t)v;
    else if (!std::strcmp(name, "defense")) t->defense = (uint32_t)v;
    else if (!std::strcmp(name, "cost_gold")) t->gold = (uint32_t)v;
    else if (!std::strcmp(name, "cost_cb")) t->crystals = (uint32_t)v;
}

TavernInfo* GetForEntity(uint32_t entity) {
    for (TavernInfo* t : g_list)
        if (t->entity == entity) return t;
    return nullptr;
}

// @0x23e138: an id already listed is overwritten.
bool Init(const char* file) {
    uint32_t size = 0;
    uint8_t* buf = FileManager::LoadFile(file, size);
    if (!buf) {
        std::printf("Tavern::Init() Failed to load tavern list (File not found: %s)\n", file);
        return false;
    }
    pugi::xml_document doc;
    pugi::xml_parse_result r = doc.load_buffer_inplace(buf, size, pugi::parse_default, pugi::encoding_auto);
    bool ok = false;
    if (!r) {
        std::printf("Tavern::Init() Failed to load tavern list (%s)\n", r.description());
    } else if (!doc.child("Tavern")) {
        std::printf("Tavern::Init() Incorrect file format (%s)\n", file);
    } else {
        for (pugi::xml_node x = doc.child("Tavern").child("Warrior"); x; x = x.next_sibling("Warrior")) {
            const uint32_t id = x.attribute("id").as_uint();
            TavernInfo* t = GetTavernInfo(id);
            if (!t) t = Add();
            t->id = id;
            t->entity = x.attribute("car_id").as_uint();
            t->probability = x.attribute("probability").as_uint();
            // "a-b"; without a dash both ends are the whole number.
            const std::string range = x.attribute("hp_range").value();
            const size_t dash = range.find('-');
            t->hpMin = std::atoi(range.substr(0, dash).c_str());
            t->hpMax = std::atoi(range.substr(dash == std::string::npos ? 0 : dash + 1).c_str());
            t->type = x.attribute("type").as_uint();
            t->slot = x.attribute("slot").as_uint();
            t->level = 0;
            t->attack = 0;
            t->defense = 0;
            t->gold = 0;
            t->crystals = 0;
        }
        ok = true;
    }
    FileManager::FreeFile(buf);
    return ok;
}

// @0x23ddf0: the storage's goblins (unit 0x133, tab 5): one more than "goblin_cost" has entries, the
// first for 100 gold; HP 25, 30, 35 ...
void InitSpecial() {
    Setting costs("goblin_cost");
    int hp = 0x19;
    for (unsigned n = 0; n < costs.GetChildrenCount() + 1; ++n, hp += 5) {
        TavernInfo* t = GetTavernInfo(n + 0x400);
        if (!t) t = Add();
        t->id = n + 0x400;
        t->entity = 0x133;
        t->hpMin = hp;
        t->hpMax = hp;
        t->type = 5;
        t->probability = 100;
        t->slot = n;
        t->attack = 1;
        t->defense = 0;
        if (n == 0) {
            t->gold = 100;
            t->crystals = 0;
            t->level = 1;
        } else {
            t->gold = (uint32_t)costs.GetChild(n - 1).GetChild(0).GetInt();
            t->crystals = (uint32_t)costs.GetChild(n - 1).GetChild(1).GetInt();
            t->level = (uint32_t)costs.GetChild(n - 1).GetChild(2).GetInt();
        }
    }
}

}  // namespace Tavern
