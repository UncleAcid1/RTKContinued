// Chests: the chests (chests.xml). A chest holds resources that it always gives and a list of items
// and objects of which it gives "items_count" by their probabilities; it may need a key item
// ("item_id", the required item) or a completed quest, and it may be sold for crystals. The chests
// on the mission maps (CHEST_DROP / CHEST_DROP_TYPE decorations) are picked by the player's level;
// a chest in the inventory is an item ("chest_item"). Port of Chests (libkingdom.so 5.11,
// @0x12a420..0x12d6ec; statics 0x611834..0x61184c).
#pragma once
#include <cstdint>
#include <vector>

class MetaData;

namespace Chests {

// The chests' "type" in chests.xml: 0 the arena's, 1 and 2 the maps' (chest_system), 3 the
// special ones (statues, starter, imperial), 4 the daily bonus, 5 the "cg_" chests, 6 by level.
using ChestType = uint32_t;

struct ChestItem {                     // 0x18 bytes, <Items><Item> / <Objects><Object>
    uint32_t id = 0;                   // +0x00 "id"
    uint32_t item = 0;                 // +0x04 "item_id", or 0x8000 + an object's "obj_type"
    uint32_t amountMin = 1;            // +0x08 "amount" ("a" or "a-b")
    uint32_t amountMax = 1;            // +0x0c
    int probability = 0;               // +0x10 "probability" in hundredths of a percent
    bool picked = false;               // +0x14 GenerateContents' round
};
bool operator<(const ChestItem& a, const ChestItem& b);   // @0x12a420 the more probable first

// What GenerateContents gives: a resource, an item (type 0xb, item) or an object (type 0xb,
// object).
struct DropItem {                      // 0x1c bytes
    uint32_t type = 0;                 // +0x00 GameState::ResourceType, 0xb an item or object
    uint32_t item = 0;                 // +0x04
    uint32_t object = 0;               // +0x08
    int amount = 0;                    // +0x0c
    uint32_t amountMin = 0;            // +0x10
    uint32_t amountMax = 0;            // +0x14
    int probability = 0;               // +0x18 hundredths of a percent (10000 = always)
};

struct ChestInfo {                     // 0x68 bytes, <Chest>
    uint32_t id = 0;                   // +0x00 "id"
    bool active = false;               // +0x04 "active"
    ChestType type = 0;                // +0x08 "type"
    const char32_t* name = nullptr;    // +0x0c StringTable "name"
    const char32_t* desc = nullptr;    // +0x10 "<name>_desc"
    uint32_t requiredItem = 0;         // +0x14 "item_id" (the key)
    uint32_t requiredQuest = 0;        // +0x18 "task_id"
    uint32_t countMin = 0;             // +0x1c "items_count" ("a" or "a-b")
    uint32_t countMax = 0;             // +0x20
    uint32_t level = 0;                // +0x24 "level"
    MetaData* resources = nullptr;     // +0x28 "resources" ("GOLD=200,443=10")
    std::vector<ChestItem> items;      // +0x2c (an sgl::vector with one inline item), sorted
    bool rewardWindow = false;         // +0x50 "reward_window"
    uint32_t rerollPrice = 0;          // +0x54 "reroll_price" (crystals)
    char* sku = nullptr;               // +0x58 the store's product id
    uint32_t cents = 0;                // +0x5c the real-money price (not set in the port)
    uint32_t crystals = 0;             // +0x60 "cb_price"
    uint32_t chestItem = 0;            // +0x64 the item that is this chest in the inventory

    bool SoldForCrystals() const;      // @0x12a478
    // @0x12a648: the resources, then the items: all of them by turns (all), else "items_count"
    // picked at random, each kept by its probability. Returns the count written to out (at most
    // max).
    unsigned GenerateContents(DropItem* out, unsigned max, bool all);
};

bool Init(const char* file);           // @0x12be18
void Deinit();                         // @0x12b89c
ChestInfo* GetChestInfo(uint32_t id);  // @0x12b404
// @0x12b270 the chest whose chest_item is item (of the starter chests the one "starter_chest_b"
// picks), or 0.
uint32_t GetChestForItem(uint32_t item);
bool IsChestItem(uint32_t item);       // @0x12b3a4
uint32_t GetItemForChest(uint32_t id); // @0x12b830 its chest_item, or 0
// @0x12b46c an attribute of chest id by name (Override, the store); sub is an item's index.
void ChangeChestAttribute(uint32_t id, uint32_t sub, const char* name, const char* value);
// @0x12b974 one of the chests listed in ids (a list) whose level is the nearest to level: active,
// its quest done and, unless ignoreKey, its key owned. nullptr if none.
ChestInfo* GetChestForLevel(const MetaData* ids, uint32_t level, bool ignoreKey);
ChestInfo* GetRandomChestByType(ChestType type);   // @0x12cfb0 its quest done, its key owned
// @0x12d29c as GetChestForLevel, of the chests of a type.
ChestInfo* GetChestForLevelByType(ChestType type, uint32_t level, bool ignoreKey);

}  // namespace Chests
