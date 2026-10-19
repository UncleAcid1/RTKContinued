#include "game/Spells.h"

#include <pugixml.hpp>

#include <cstdio>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

#include "engine/FileManager.h"
#include "engine/IconManager.h"
#include "engine/Timer.h"
#include "game/GameState.h"

namespace Spells {
namespace {

// 0x6156ac every SpellInfo, in load order (FUN_002252e4 / FUN_0022534c: the vector's allocate / free)
std::vector<SpellInfo*> g_spells;
std::deque<std::string> g_names;             // FUN_002251b4's string pool (0x6156b8)

const char* InternName(const char* s) {   // @0x2251b4
    g_names.emplace_back(s);
    return g_names.back().c_str();
}

}  // namespace

// @0x225768 Spells::Init. A spell read again (a content pack's copy of the file) replaces the first.
bool Init(const char* file) {
    double t0 = Timer::GetTime();
    uint32_t size = 0;
    uint8_t* buf = FileManager::LoadFile(file, size);
    if (!buf) {
        std::printf("Spells::Init() Failed to load spell list (File not found: %s)\n", file);
        return false;
    }
    pugi::xml_document doc;
    pugi::xml_parse_result r = doc.load_buffer_inplace(buf, size, pugi::parse_default, pugi::encoding_auto);
    if (!r) {
        std::printf("Spells::Init() Failed to load spell list (%s)\n", r.description());
        FileManager::FreeFile(buf);
        return false;
    }
    pugi::xml_node root = doc.child("Spells");
    if (!root) {
        std::printf("Spells::Init() Incorrect file format (%s)\n", file);
        FileManager::FreeFile(buf);
        return false;
    }
    g_spells.reserve(0x10);
    for (pugi::xml_node x = root.child("Spell"); x; x = x.next_sibling("Spell")) {
        uint32_t id = x.attribute("spell_id").as_uint();
        SpellInfo* s = GetSpellInfo(id);
        if (!s) {
            s = new SpellInfo;
            g_spells.push_back(s);
        }
        s->id = id;
        s->name = InternName(x.attribute("name").value());
        s->icon = IconManager::GetIcon(x.attribute("icon").value(), true);
        std::string on = x.attribute("marker").value();
        size_t off = on.find("_off");
        if (off != std::string::npos) on.replace(off, std::string::npos, "_on");
        s->markerOn = IconManager::GetIcon(on.c_str(), true);
        s->marker = IconManager::GetIcon(x.attribute("marker").value(), true);
        s->animation = InternName(x.attribute("animation").value());
        s->damage = x.attribute("damage").as_uint();
        s->range = x.attribute("range").as_uint();
        s->slot = x.attribute("slot").as_uint();
        s->level = x.attribute("level").as_uint();
        s->unlockQuest = x.attribute("unlock_quest").as_uint();
        s->cost = x.attribute("cost").as_uint();
    }
    FileManager::FreeFile(buf);
    std::printf("Spells::Init() Loaded spell list in %.3fms\n", (Timer::GetTime() - t0) * 1000.0);
    return true;
}

SpellInfo* GetSpellInfo(uint32_t id) {
    for (SpellInfo* s : g_spells)
        if (s->id == id) return s;
    return nullptr;
}

SpellInfo* EnumSpellInfo(unsigned i) { return i < g_spells.size() ? g_spells[i] : nullptr; }

SpellInfo* GetSpellLockedByTask(uint32_t task) {
    for (SpellInfo* s : g_spells)
        if (s->unlockQuest == task) return s;
    return nullptr;
}

SpellInfo* GetBestSpellForSlot(uint32_t slot, uint32_t mana, bool unlocked) {
    SpellInfo* best = nullptr;
    for (SpellInfo* s : g_spells) {
        if (s->slot != slot || s->cost > mana) continue;
        if (best && s->cost <= best->cost) continue;
        if (unlocked && ((s->unlockQuest && !GameState::TaskCompleted(s->unlockQuest)) || s->level > (uint32_t)GameState::GetLevel()))
            continue;
        best = s;
    }
    return best;
}

}  // namespace Spells
