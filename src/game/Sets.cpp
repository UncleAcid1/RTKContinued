#include "game/Sets.h"

#include <pugixml.hpp>

#include <algorithm>
#include <cstdio>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "engine/FileManager.h"
#include "engine/Timer.h"
#include "game/Collections.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/MetaData.h"
#include "game/MetaExpression.h"
#include "game/PlayerEquipmentInfo.h"
#include "game/StringTable.h"

namespace Sets {
namespace {
std::vector<std::unique_ptr<Set>> g_sets;          // 0x615530
std::vector<std::unique_ptr<std::string>> g_texts; // 0x615540 the bonus texts (MetaExpression keeps the pointer)
std::set<uint32_t> g_state;                        // 0x61554c the item ids counted

// Any item of the list (or an improvement of it) in the state.
bool AnyOf(const MetaData* list) {
    bool any = false;
    for (unsigned i = 0; i < list->GetChildrenCount(); ++i)
        if (HasItemOrBetter((uint32_t)list->GetChild(i)->GetInt())) any = true;
    return any;
}
}  // namespace

// @0x21ac70
bool Init(const char* file) {
    double t0 = Timer::GetTime();
    uint32_t size = 0;
    uint8_t* buf = FileManager::LoadFile(file, size);
    if (!buf) {
        std::printf("Sets::Init() Failed to load set list (File not found: %s)\n", file);
        return false;
    }
    pugi::xml_document doc;
    pugi::xml_parse_result r = doc.load_buffer_inplace(buf, size, 0x74);
    if (!r) {
        std::printf("Sets::Init() Failed to load set list (%s)\n", r.description());
        FileManager::FreeFile(buf);
        return false;
    }
    if (!doc.child("Sets")) {
        std::printf("Sets::Init() Incorrect file format (%s)\n", file);
        FileManager::FreeFile(buf);
        return false;
    }
    for (pugi::xml_node n = doc.child("Sets").child("Set"); n; n = n.next_sibling("Set")) {
        uint32_t id = n.attribute("id").as_uint();
        Set* s = GetSet(id);
        if (!s) {
            g_sets.push_back(std::make_unique<Set>());
            s = g_sets.back().get();
        } else {
            for (MetaData*& m : s->bonusSlots) { delete m; m = nullptr; }
            for (MetaExpression*& m : s->bonus) { delete m; m = nullptr; }
            for (MetaData*& m : s->slots) { delete m; m = nullptr; }
        }
        s->id = id;
        s->name = StringTable::GetString(n.attribute("name").value());
        s->active = n.attribute("active").as_bool();
        s->sortId = n.attribute("sort_id").as_uint();
        char name[0x40];
        for (int k = 0; k != 3; ++k) {
            std::snprintf(name, sizeof name, "bonus%d_slots", k + 1);
            const char* v = n.attribute(name).value();
            s->bonusSlots[k] = *v ? ParseCustomStyleData(v, ",", "1") : nullptr;
        }
        // (The first bonus is "bonus", the others "bonus2", "bonus3".)
        for (int k = 0; k != 3; ++k) {
            std::snprintf(name, sizeof name, k == 0 ? "bonus" : "bonus%d", k + 1);
            const char* v = n.attribute(name).value();
            if (!*v) {
                s->bonus[k] = nullptr;
                continue;
            }
            g_texts.push_back(std::make_unique<std::string>(v));
            s->bonus[k] = new MetaExpression(g_texts.back()->c_str(), false);
        }
        for (pugi::xml_node it = n.child("Items").child("Item"); it; it = it.next_sibling("Item")) {
            const char* v = it.attribute("item_id").value();
            unsigned slot = it.attribute("slot").as_uint();
            if (slot < 1 || slot > 9) continue;   // PORT: the original writes outside the slots
            delete s->slots[slot - 1];
            s->slots[slot - 1] = ParseCustomStyleData(v, ",", "1");
        }
    }
    // (FUN_0021aa3c / FUN_0021abd0: STLport's introsort of the list with SortSet.)
    std::sort(g_sets.begin(), g_sets.end(),
              [](const std::unique_ptr<Set>& a, const std::unique_ptr<Set>& b) { return SortSet(a.get(), b.get()); });
    FileManager::FreeFile(buf);
    std::printf("Sets::Init() Loaded set list in %.3fms\n", (Timer::GetTime() - t0) * 1000.0);
    return true;
}

bool SortSet(const Set* a, const Set* b) { return a->sortId < b->sortId; }

Set* GetSet(uint32_t id) {
    for (auto& s : g_sets)
        if (s->id == id) return s.get();
    return nullptr;
}

Set* EnumSet(unsigned i) { return i < g_sets.size() ? g_sets[i].get() : nullptr; }

// (The original compares the sort ids nine times over, the same each time.)
Set* GetSetThatWasReplacedByNewSet(uint32_t id) {
    Set* s = GetSet(id);
    if (!s) return nullptr;
    for (auto& o : g_sets)
        if (o.get() != s && o->sortId == s->sortId) return o.get();
    return nullptr;
}

bool HasItemOrBetter(uint32_t id) {
    if (g_state.count(id)) return true;
    for (Collections::Collection* c = Collections::GetCollectionWithSource(id); c;
         c = Collections::GetCollectionWithSource(c->to))
        if (g_state.count(c->to)) return true;
    return false;
}

// A bonus counts a slot when the state holds one of the slot's items, or else one of the same slot
// of the set this one replaced.
void CommitChanges() {
    for (auto& sp : g_sets) {
        Set* s = sp.get();
        s->covered[0] = s->covered[1] = s->covered[2] = 0;
        if (!s->active) continue;
        for (int k = 0; k != 3; ++k) {
            MetaData* list = s->bonusSlots[k];
            if (!list) continue;
            for (unsigned i = 0; i < list->GetChildrenCount(); ++i) {
                int slot = list->GetChild(i)->GetInt();
                MetaData* items = slot >= 1 && slot <= 9 ? s->slots[slot - 1] : nullptr;   // (PORT: bounds)
                if (items && AnyOf(items)) {
                    ++s->covered[k];
                    continue;
                }
                if (Set* old = GetSetThatWasReplacedByNewSet(s->id)) {
                    MetaData* oldItems = slot >= 1 && slot <= 9 ? old->slots[slot - 1] : nullptr;
                    if (oldItems && AnyOf(oldItems)) ++s->covered[k];
                }
            }
        }
    }
}

Set* GetSetForItem(uint32_t id) {
    for (auto& s : g_sets)
        for (MetaData* m : s->slots) {
            if (!m) continue;
            for (unsigned i = 0; i < m->GetChildrenCount(); ++i)
                if ((uint32_t)m->GetChild(i)->GetInt() == id) return s.get();
        }
    return nullptr;
}

void RemoveItem(uint32_t id) { g_state.erase(id); }
void AddItem(uint32_t id) { g_state.insert(id); }   // (FUN_0021bab0: the tree's insert)

void FindPlayerSetState(const PvP::PlayerEquipmentInfo& info) {
    g_state.clear();
    for (const auto& it : info.items)
        if (it.id) AddItem(it.id);
    CommitChanges();
}

void FindInventorySetState() {
    g_state.clear();
    for (unsigned i = 0; GameState::PlayerItem* it = GameState::EnumItems(i); ++i) AddItem(it->id);
    CommitChanges();
}

void FindActiveSetState() {
    g_state.clear();
    for (unsigned b = 0; b != 10; ++b) {
        GameState::PlayerItem* it = GameState::GetItemAt(b);
        if (it && !it->IsBroken()) AddItem(it->id);
    }
    CommitChanges();
}

}  // namespace Sets
