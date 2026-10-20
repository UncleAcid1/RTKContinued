#include "game/PresentsContainer.h"

#include <pugixml.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "engine/FileManager.h"
#include "game/Building.h"
#include "game/GameData.h"
#include "game/GameState.h"
#include "game/Map.h"
#include "game/Rand48.h"
#include "game/SaveManager.h"
#include "game/StringTable.h"
#include "gui/WindowManager.h"

namespace PresentsContainer {
namespace {
// The statics (FUN_001e5e74 constructs the vectors and registers their destructors, FUN_001e6324
// and the vector destructor @0x1e6364 free them at exit: the vectors here do).
int g_warehouseSize = 2;                       // 0x60f024
float g_announceTimer = 1.f;                   // 0x60f028 Update's wait before the next pack
std::vector<Present*> g_tabs[2];               // 0x613ea4 the warehouse, the presents
std::vector<PresentPack*> g_packs;             // 0x613ebc
struct StartPresent {                          // 0x8 bytes
    int type, id;
};
std::vector<StartPresent*> g_startQueue;       // 0x613ec8

const unsigned kServerTypes[4] = {1, 1, 1, 2};   // 0x586e14

void FreePack(PresentPack* pack) {
    for (PackItem* it : pack->items) delete it;
    delete pack;
}
}  // namespace

// @0x1e5e40
int GetWareHouseSize() { return g_warehouseSize; }

// @0x1e5e50
unsigned GetPresentTypeFromServer(unsigned t) {
    if (t - 1 < 4) return kServerTypes[t - 1];
    return 1;
}

// @0x1e5f10
const char32_t* Present::GetName() const {
    if (type == kPresentBuilding) {
        if (const GameData::BuildingData* data = GameData::GetBuilding(id)) return StringTable::GetString(data->name.c_str());
    }
    return nullptr;
}

// @0x1e5f48
void ToggleWarehouseAnim() {
    if (Map::Building* b = Map::GetBuildingWithID(kWarehouseId)) {
        b->flashTime = 2.3f;
        b->flashing = true;
    }
}

// @0x1e5f7c
PresentPack* GetPackByNum(unsigned n) {
    unsigned seen = 0;
    for (PresentPack* pack : g_packs) {
        if (pack->levelFrom != 0 && pack->levelFrom > (unsigned)GameState::GetLevel()) continue;
        if (pack->levelTo != 0 && pack->levelTo < (unsigned)GameState::GetLevel()) continue;
        if (seen++ == n) return pack;
    }
    return nullptr;
}

// @0x1e6064
int GetPresentPacksCount() {
    int count = 0;
    for (PresentPack* pack : g_packs) {
        if (pack->levelFrom != 0 && pack->levelFrom > (unsigned)GameState::GetLevel()) continue;
        if (pack->levelTo != 0 && pack->levelTo < (unsigned)GameState::GetLevel()) continue;
        ++count;
    }
    return count;
}

// @0x1e6118 The first pack for the player's level not announced yet is announced (one a second,
// with no window open).
void Update(float dt) {
    if (WindowManager::GetShownWindowCount() != 0) {
        g_announceTimer = 1.f;
        return;
    }
    g_announceTimer -= dt;
    if (g_announceTimer < 0.f) {
        const unsigned count = (unsigned)GetPresentPacksCount();
        for (unsigned i = 0; i < count; ++i) {
            PresentPack* pack = GetPackByNum(i);
            if (!pack->shown) {
                pack->shown = true;
                g_announceTimer = 1.f;
                return;
            }
        }
    }
}

// @0x1e61c8
void DeletePresent(Present* p, bool destroy) {
    for (int tab = 0; tab < 2; ++tab) {
        std::vector<Present*>& list = g_tabs[tab];
        for (auto it = list.begin(); it != list.end(); ++it) {
            if (*it != p) continue;
            list.erase(it);
            if (tab == kWarehouse) ToggleWarehouseAnim();
            break;
        }
    }
    if (!destroy || !p) return;
    delete p->building;
    delete p;
}

// @0x1e63a4
int GetPresentCount(int tab) { return (int)g_tabs[tab].size(); }

// @0x1e63cc
int GetWareHouseFreeSize() { return g_warehouseSize - GetPresentCount(kWarehouse); }

// @0x1e63ec
Present* GetPresent(int tab, unsigned i) { return i < g_tabs[tab].size() ? g_tabs[tab][i] : nullptr; }

// @0x1e641c; @0x1e6560 _INIT_24 (to 0x1e65b8) is this function's tail, split off by Ghidra.
void Save() {
    using namespace SaveManager;
    BeginChunk(8);
    SaveUnsigned((uint32_t)g_tabs[kPresents].size());
    for (const Present* p : g_tabs[kPresents]) {
        SaveChar((uint8_t)p->type);
        SaveUnsigned(p->id);
        SaveChar(p->f0c);
        SaveChar(p->f0d);
    }
    SaveUnsigned((uint32_t)g_tabs[kWarehouse].size());
    for (const Present* p : g_tabs[kWarehouse]) {
        if (p->type != kPresentBuilding) continue;
        SaveUnsigned(p->id);
        SaveChar(p->f0c);
        SaveChar(p->f0d);
        // PORT: the original follows with Map::Patch::SaveBuilding(p->building), whose BeginChunk(0xc)
        // rewinds the chunk being written (SaveManager has one current chunk) and which Load never
        // reads back: left out, the chunk stays whole.
    }
    SaveUnsigned((uint32_t)g_warehouseSize);
    uint32_t shown = 0;
    for (const PresentPack* pack : g_packs)
        if (pack->shown) ++shown;
    SaveUnsigned(shown);
    for (const PresentPack* pack : g_packs)
        if (pack->shown) SaveUnsigned(pack->id);
    EndChunk();
}

// @0x1e65d4
Present* AddPresent(int tab, int type, uint32_t id, Map::Building* b) {
    Present* p = new Present();
    p->type = type;
    p->id = id;
    p->building = b;
    g_tabs[tab].push_back(p);
    return p;
}

// @0x1e6760
void AddToWarehouse(Map::Building* b) {
    Present* p = AddPresent(kWarehouse, kPresentBuilding, b->data->id, b);
    p->f0c = true;
}

// @0x1e678c
bool UnqueueStartPresents() {
    if (g_startQueue.empty()) return false;
    for (StartPresent* s : g_startQueue) {
        AddPresent(kPresents, s->type, (uint32_t)s->id, nullptr);
        delete s;
    }
    g_startQueue.clear();
    return true;
}

// @0x1e67fc The crystals are paid and each item's presents picked at random from its ids.
void BuyPack(PresentPack* pack) {
    GameState::ChangeResourceAmount(GameState::kCrystal, -GameState::AdjustCrystalCost((int)pack->cost));
    for (PackItem* item : pack->items) {
        if (item->count == 0 || item->ids.empty()) continue;   // (PORT: the original divides by an empty list)
        for (uint32_t i = 0; i < item->count; ++i) {
            const int id = item->ids[(size_t)((unsigned long)Rand48::lrand48() % item->ids.size())];
            AddPresent(kPresents, (int)GetPresentTypeFromServer(item->type), (uint32_t)id, nullptr);
        }
    }
}

// @0x1e68bc
void AddStartPresentToQueue(int type, int id) { g_startQueue.push_back(new StartPresent{type, id}); }

// @0x1e6a04
void Clear(bool all) {
    for (auto& list : g_tabs) {
        for (Present* p : list) {
            delete p->building;
            delete p;
        }
        list.clear();
    }
    if (!all) return;
    for (StartPresent* s : g_startQueue) delete s;
    g_startQueue.clear();
    for (PresentPack* pack : g_packs) FreePack(pack);
    g_packs.clear();
}

// @0x1e6ccc The buildings of the warehouse are not saved with an id only: they come back without
// their building.
void Load(SaveManager::SaveBlock* block) {
    Clear(false);
    block->SkipToChunk(8);
    const SaveManager::Chunk& chunk = block->GetChunk();
    SaveManager::Reader r(chunk);
    if (chunk.type == 8) {
        uint32_t n = r.u32();
        for (uint32_t i = 0; i < n; ++i) {
            const int type = (int8_t)r.u8();
            const uint32_t id = r.u32();
            const uint8_t c0c = r.u8(), c0d = r.u8();
            Present* p = AddPresent(kPresents, type, id, nullptr);
            p->f0d = c0d == 1;
            p->f0c = c0c == 1;
        }
        n = r.u32();
        for (uint32_t i = 0; i < n; ++i) {
            const uint32_t id = r.u32();
            const uint8_t c0c = r.u8(), c0d = r.u8();
            Present* p = AddPresent(kWarehouse, kPresentBuilding, id, nullptr);
            p->f0d = c0d == 1;
            p->f0c = c0c == 1;
        }
        g_warehouseSize = (int)r.u32();
        n = r.u32();
        for (uint32_t i = 0; i < n; ++i) {
            const uint32_t id = r.u32();
            for (PresentPack* pack : g_packs)
                if (pack->id == id) pack->shown = true;
        }
    }
    block->EndChunkLoading(r);
}

// @0x1e6eb4 The packs for the shop (shop_packs.xml). A pack that is targeted (2 and up) is not
// kept; the others take their items.
bool LoadPresentPacks(const char* file) {
    uint32_t size = 0;
    uint8_t* buf = FileManager::LoadFile(file, size);
    if (!buf) {
        std::printf("PresentsContainer::LoadPresentPacks() Failed to load present packs list (File not found: %s)\n", file);
        return false;
    }
    pugi::xml_document doc;
    pugi::xml_parse_result res = doc.load_buffer_inplace(buf, size, 0x74);
    if (!res) {
        std::printf("PresentsContainer::LoadPresentPacks() Failed to load present packs list (%s)\n", res.description());
        FileManager::FreeFile(buf);
        return false;
    }
    if (!doc.child("presents_packs")) {
        std::printf("PresentsContainer::LoadPresentPacks() Incorrect file format (%s)\n", file);
        FileManager::FreeFile(buf);
        return false;
    }
    for (pugi::xml_node n = doc.child("presents_packs").child("presents_set"); n; n = n.next_sibling("presents_set")) {
        PresentPack* pack = new PresentPack();
        pack->id = n.attribute("id").as_uint();
        pack->cost = n.attribute("cost").as_uint();
        pack->visible = n.attribute("visible").as_uint() == 1;
        pack->targeting = n.attribute("targeting").as_uint();
        pack->image = n.attribute("image").value();
        pack->bigImage = n.attribute("big_image").value();
        pack->packTab = n.attribute("packtab").as_uint();
        pack->packSubTab = n.attribute("packsubtab").as_uint();
        pack->ts = n.attribute("ts").as_uint();
        pack->newTs = n.attribute("new_ts").as_uint();
        pack->levelFrom = n.attribute("level_from").as_uint();
        pack->levelTo = n.attribute("level_to").as_uint();
        pack->shown = false;
        if (pack->targeting >= 2) {
            pack->visible = false;
            delete pack;
            continue;
        }
        pack->visible = true;
        for (pugi::xml_node it = n.child("item"); it; it = it.next_sibling("item")) {
            PackItem* item = new PackItem();
            item->type = it.attribute("type").as_uint();
            item->count = it.attribute("count").as_uint();
            std::string token;
            for (const char* c = it.attribute("value").value(); *c; ++c) {
                if (*c == ';') {
                    item->ids.push_back(std::atoi(token.c_str()));
                    token.clear();
                } else {
                    token += *c;
                }
            }
            if (!token.empty()) item->ids.push_back(std::atoi(token.c_str()));
            pack->items.push_back(item);
        }
        g_packs.push_back(pack);
    }
    FileManager::FreeFile(buf);
    return true;
}

}  // namespace PresentsContainer
