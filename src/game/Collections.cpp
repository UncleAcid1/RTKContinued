#include "game/Collections.h"

#include <pugixml.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

#include "engine/FileManager.h"
#include "engine/Timer.h"
#include "game/GameState.h"
#include "game/Items.h"

namespace Collections {
namespace {
std::vector<std::unique_ptr<Collection>> g_list;   // 0x611850
}  // namespace

// @0x12dbd0
bool Init(const char* file) {
    double t0 = Timer::GetTime();
    uint32_t size = 0;
    uint8_t* buf = FileManager::LoadFile(file, size);
    if (!buf) {
        std::printf("Collections::Init() Failed to load collection list (File not found: %s)\n", file);
        return false;
    }
    pugi::xml_document doc;
    pugi::xml_parse_result r = doc.load_buffer_inplace(buf, size, 0x74);
    if (!r) {
        std::printf("Collections::Init() Failed to load collection list (%s)\n", r.description());
        FileManager::FreeFile(buf);
        return false;
    }
    if (!doc.child("Collections")) {
        std::printf("Collections::Init() Incorrect file format (%s)\n", file);
        FileManager::FreeFile(buf);
        return false;
    }
    for (pugi::xml_node n = doc.child("Collections").child("Collection"); n; n = n.next_sibling("Collection")) {
        if (!n.attribute("active").as_bool()) continue;
        uint32_t id = n.attribute("id").as_uint();
        Collection* c = GetCollection(id);
        if (!c) {
            g_list.push_back(std::make_unique<Collection>());
            c = g_list.back().get();
        } else {
            for (Collection::Part& p : c->parts) p = {};
        }
        c->id = id;
        c->type = n.attribute("type").as_uint();
        c->from = n.attribute("item_id_from").as_uint();
        c->to = n.attribute("item_id").as_uint();
        // <Item slot="n">: slot 1..4 are the parts (slot 0 would write "from" and "to": the original
        // indexes the record by slot).
        for (pugi::xml_node it = n.child("Items").child("Item"); it; it = it.next_sibling("Item")) {
            unsigned slot = it.attribute("slot").as_uint();
            if (slot < 1 || slot > 4) continue;   // PORT: bounds
            Collection::Part& p = c->parts[slot - 1];
            p.item = it.attribute("item_id").as_uint();
            p.amount = it.attribute("amount").as_uint();
            if (p.item != 0 && p.amount == 0) p.amount = 1;
        }
        c->reputationId = n.attribute("reputation_id").as_uint();
        c->reputationLevel = n.attribute("reputation_level").as_uint();
        c->sort = n.attribute("sort").as_uint();
        c->tab = n.attribute("tab").as_uint();
        // (The original reports a part item listed twice through ErrorReporter: a data check.)
        c->next = nullptr;
        c->prev = nullptr;
    }
    FileManager::FreeFile(buf);
    std::printf("Collections::Init() Loaded collection list in %.3fms\n", (Timer::GetTime() - t0) * 1000.0);
    for (auto& c : g_list)
        if (Collection* n = GetCollectionWithSource(c->to)) c->next = n;
    for (auto& c : g_list)
        if (Collection* p = GetCollectionWithResult(c->from)) c->prev = p;
    return true;
}

Collection* GetCollection(uint32_t id) {
    for (auto& c : g_list)
        if (c->id == id) return c.get();
    return nullptr;
}

Collection* EnumCollection(unsigned i) { return i < g_list.size() ? g_list[i].get() : nullptr; }

Collection* GetCollectionWithSource(uint32_t item) {
    for (auto& c : g_list)
        if (c->type == 0 && c->from == item) return c.get();
    return nullptr;
}

Collection* GetCollectionWithResult(uint32_t item) {
    for (auto& c : g_list)
        if (c->to == item) return c.get();
    return nullptr;
}

Collection* GetCollectionStart(uint32_t item) {
    Collection* c = GetCollectionWithResult(item);
    if (!c) return nullptr;
    while (c->prev) c = c->prev;
    return c;
}

Collection* GetCollectionEnd(uint32_t item) {
    Collection* c = GetCollectionWithSource(item);
    if (!c) return nullptr;
    while (c->next) c = c->next;
    return c;
}

Collection* GetCraftingCollectionForItem(uint32_t item) {
    for (auto& c : g_list) {
        if (c->type != 1) continue;
        if (c->from == item) return c.get();
        for (const Collection::Part& p : c->parts)
            if (p.item == item) return c.get();
    }
    return nullptr;
}

void ChangeCollectionAttribute(uint32_t id, unsigned part, const char* attr, const char* value) {
    Collection* c = GetCollection(id);
    if (!c) return;
    uint32_t v = (uint32_t)std::atoi(value);
    if (!std::strcmp(attr, "from")) c->from = v;
    else if (!std::strcmp(attr, "to")) c->to = v;
    else if (!std::strcmp(attr, "reputation_id")) c->reputationId = v;
    else if (!std::strcmp(attr, "reputation_level")) c->reputationLevel = v;
    else if (!std::strcmp(attr, "tab")) c->tab = v;
    else if (!std::strcmp(attr, "sort")) c->sort = v;
    else if (!std::strcmp(attr, "item_id")) {
        if (part < 4) c->parts[part].item = v;
    } else if (!std::strcmp(attr, "item_count")) {
        if (part < 4) c->parts[part].amount = v;
    }
}

uint32_t GetMaxItemLevel(uint32_t item) {
    Items::ItemInfo* info = Items::GetItemInfo(item);
    if (!info) return 0;
    uint32_t level = info->itemLevel;
    if (Collection* end = GetCollectionEnd(info->id))
        if (Items::ItemInfo* best = Items::GetItemInfo(end->to))
            if (level < best->itemLevel) level = best->itemLevel;
    return level;
}

bool CanImproveItem(uint32_t item) {
    Collection* c = GetCollectionWithSource(item);
    if (!c) return false;
    for (const Collection::Part& p : c->parts)
        if (p.item != 0 && GameState::GetItemAmount(p.item, false) == 0) return false;
    return true;
}

}  // namespace Collections
