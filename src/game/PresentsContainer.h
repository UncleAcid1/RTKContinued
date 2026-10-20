// PresentsContainer: the presents (gifts and the packs of the shop_packs.xml list) waiting to be
// built, and the warehouse, where the city's edit mode can store buildings (mode 4, which nothing
// in 5.11 enters). Gifts only arrive from the online services (AddStartPresentToQueue, BuyPack):
// offline the presents tab stays empty, and the pack list loads and is saved as the original does.
// Port of PresentsContainer (libkingdom.so 5.11, @0x1e5e40..0x1e7f10; statics 0x60f024..0x60f030
// and 0x613ea4..0x613ed0).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace Map {
struct Building;
}
namespace SaveManager {
struct SaveBlock;
}

namespace PresentsContainer {

enum TPresentTab { kWarehouse = 0, kPresents = 1 };
// What a present is (GetPresentTypeFromServer maps the server's 1..4); 1 is a building.
enum TPresentType { kPresentBuilding = 1 };

constexpr uint32_t kWarehouseId = 378;   // 0x60f02c WAREHOUSE_ID: the building that shows the warehouse

struct Present {                    // 0x10 bytes
    int type = 0;                   // +0x00 TPresentType
    uint32_t id = 0;                // +0x04 the building's id (BuildingData)
    Map::Building* building = nullptr;   // +0x08 the stored building (warehouse; null after a load)
    bool f0c = false;               // +0x0c saved
    bool f0d = false;               // +0x0d saved
    const char32_t* GetName() const;   // @0x1e5f10 the building's name (null: not a building)
};

struct PackItem {                   // 0x14 bytes, <item>
    uint32_t type = 0;              // +0x00 "type" (the server's present type)
    uint32_t count = 0;             // +0x04 "count" presents of this kind
    std::vector<int> ids;           // +0x08 "value" the ids it picks from, split at ';'
};

struct PresentPack {                // 0x68 bytes, <presents_set>
    uint32_t id = 0;                // +0x00 "id"
    uint32_t cost = 0;              // +0x04 "cost" in crystals
    bool visible = false;           // +0x08 "visible" (set when "targeting" < 2)
    uint32_t targeting = 0;         // +0x0c "targeting"
    std::string image;              // +0x10 "image"
    std::string bigImage;           // +0x28 "big_image"
    uint32_t packTab = 0;           // +0x40 "packtab"
    uint32_t packSubTab = 0;        // +0x44 "packsubtab"
    uint32_t newTs = 0;             // +0x48 "new_ts"
    uint32_t ts = 0;                // +0x4c "ts"
    uint32_t levelFrom = 0;         // +0x50 "level_from" (0: any)
    uint32_t levelTo = 0;           // +0x54 "level_to" (0: any)
    bool shown = false;             // +0x58 announced to the player (saved)
    std::vector<PackItem*> items;   // +0x5c
};

int GetWareHouseSize();                          // @0x1e5e40
unsigned GetPresentTypeFromServer(unsigned t);   // @0x1e5e50
void ToggleWarehouseAnim();                      // @0x1e5f48 the warehouse building flashes
PresentPack* GetPackByNum(unsigned n);           // @0x1e5f7c the n-th pack for the player's level
int GetPresentPacksCount();                      // @0x1e6064
void Update(float dt);                           // @0x1e6118
void DeletePresent(Present* p, bool destroy);    // @0x1e61c8
int GetPresentCount(int tab);                    // @0x1e63a4
int GetWareHouseFreeSize();                      // @0x1e63cc
Present* GetPresent(int tab, unsigned i);        // @0x1e63ec (null past the end)
void Save();                                     // @0x1e641c chunk 8
Present* AddPresent(int tab, int type, uint32_t id, Map::Building* b);   // @0x1e65d4
void AddToWarehouse(Map::Building* b);           // @0x1e6760
bool UnqueueStartPresents();                     // @0x1e678c the queued start presents -> presents
void BuyPack(PresentPack* pack);                 // @0x1e67fc
void AddStartPresentToQueue(int type, int id);   // @0x1e68bc
void Clear(bool all);                            // @0x1e6a04 all: the queue and the pack list too
void Load(SaveManager::SaveBlock* block);        // @0x1e6ccc chunk 8
bool LoadPresentPacks(const char* file);         // @0x1e6eb4 shop_packs.xml

}  // namespace PresentsContainer
