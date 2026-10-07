// Contracts: the order lists of production buildings and farms (deliveries.xml): each <d_delivery>
// is one building's list, each <d_mission> one order (crop, product) with its times and rewards.
// Port of Contracts::Init @0x12f6e0, GetContract @0x12e760, EnumContract @0x12eb40.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace Items { struct ItemInfo; }

namespace Contracts {

struct ContractMission {         // 0x5c bytes
    int growTotal = 0;           // +0x00 sum of growTimes
    unsigned rewardGold = 0;     // +0x04 "reward_gold"
    unsigned rewardXp = 0;       // +0x08 "reward_xp"
    const char32_t* title = nullptr;  // +0x0c StringTable "title"
    Items::ItemInfo* item = nullptr;  // +0x10 Items::GetItemInfo(title): the item the order makes
    std::string icon;            // +0x14 "icon"
    unsigned price = 0;          // +0x18 "price" (gold)
    unsigned priceResourceCount = 0;  // +0x1c "price_resource_count"
    unsigned playerLevel = 0;    // +0x20 "player_lvl"
    int priceResource = 0;       // +0x24 "price_resource" (ExternalResourceTypeToInternal)
    int rewardResource = 0;      // +0x28 "reward_resource" (ExternalResourceTypeToInternal)
    unsigned buildingLevel = 0;  // +0x2c "bulding_lvl"
    int timePlant = 0;           // +0x30 (int)("time_plant" * contract_coeff)
    unsigned timeHarvest = 0;    // +0x34 "time_harvest"
    int timeRot = 999999999;     // +0x38 (the file's "time_rot" is not read)
    unsigned timeClean = 0;      // +0x3c "time_clean"
    unsigned speedupCost2 = 0;   // +0x40 "speedup_cost2"
    unsigned rewardResourceCount = 0;  // +0x44 "reward_resource_count"
    std::vector<int> growTimes;  // +0x50 "time_grow" a:b:c..., each (int)(n * farm_coeff)
};

struct Contract {                // 0x1c bytes
    uint32_t id = 0;             // +0x00 "mid"
    unsigned type = 0;           // +0x04 "type"
    const char32_t* title = nullptr;  // +0x08 StringTable "title"
    std::vector<ContractMission> missions;  // +0x0c
    unsigned count = 0;          // +0x18
};

bool Init(const char* file);                 // @0x12f6e0
const Contract* GetContract(uint32_t id);    // @0x12e760
const Contract* EnumContract(unsigned i);    // @0x12eb40

}  // namespace Contracts
