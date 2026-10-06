// Static game data used by the map: decorations, buildings.
// Ports of Map::LoadDecorationList @0x13a8a8 and Map::LoadBuildingList @0x121748.
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace Render { struct Texture; }
class MetaData;
namespace Contracts { struct Contract; }

namespace GameData {

// Map::RoadData (0x50 bytes): one road style, the table 0x57b7b0 copied by LoadDecorationList. A
// decoration whose id is among `ids` is a road tile of that style (DecorData+0x78); the tile at
// connection type i (Map::GetRoadConnectionType) is ids[i], mirrored when mirror[i].
struct RoadData {
    uint32_t ids[16];            // +0x00
    uint8_t mirror[16];          // +0x40
};

struct DecorData {               // DecorData
    uint32_t id = 0;             // +0x00 "id"
    std::string name;            // +0x04 "name"
    int w = 0, h = 0;            // +0x08 "lockzoneX", +0x0c "lockzoneY"
    int ox = 0, oy = 0;          // +0x10 "x", +0x14 "y"
    int layer = 0;               // +0x20 "layer"
    bool isRoad = false;         // +0x6e "isroad" (waypoint weight 0.1)
    bool giveable = false;       // +0x31 "giveable"
    uint32_t collectTime = 0;    // +0x34 "collecttime" seconds between a decoration's taxes
    uint32_t collectMoney = 0;   // +0x38 "collectmoney"
    uint32_t collectExp = 0;     // +0x3c "collectexp"
    uint32_t collectChest = 0;   // +0x40 "collect_chest"
    int cost1 = 0;               // +0x44 "cost1" gold
    int cost2 = 0;               // +0x48 "cost2" crystals
    const RoadData* road = nullptr;  // +0x78 the road style it belongs to
    std::string img;             // +0x50 "img"
    Render::Texture* image = nullptr;  // +0x4c (DecorData::LoadImage)
    bool imageLoaded = false;
};

struct BuildingPart {            // BuildingPart, 0x44 bytes, one per <floor>
    int type = 0;                // +0x00 0 floor, 1 centerpart, 2 ceil, 3 decors (5.11 data: floors only)
    std::string partImg;         // +0x08 "part_img"
    int offX = 0, offY = 0;      // +0x0c "off_x", +0x10 "off_y"
    int height = 0;              // +0x14 "height"
    int based = 0, valign = 0;   // +0x18, +0x1c
    int stage = -1;              // +0x20 (-1: selected by order; trees/149/1000 get explicit stages;
                                 //        non-floor parts: "stg")
    float frameTime = 1.f;       // +0x24 = 1 / "fs"
    float uc = 0.f;              // +0x28 "uc"
    unsigned pD = 0, pCb = 0;    // +0x2c "p_d", +0x30 "p_cb"
    unsigned lvl = 0, xp = 0;    // +0x34 "lvl", +0x38 "xp"
    Render::Texture* image = nullptr;  // +0x04 (BuildingPart::LoadImage)
    bool imageLoaded = false;
};

struct UpgradeInfo {             // 0x50 bytes per upgrade level (BuildingData+0xb8)
    int quest = 0;               // +0x00 "upgradequests"
    int requiredId = 0, requiredCount = 0;  // +0x04, +0x08 "requirestoupgrade" level:id=count
    int time = 0;                // +0x0c "upgradetime"
    int level = 0;               // +0x10 "upgradelevels"
    int cost[11] = {};           // +0x14 "upgradecost" RESOURCE=n per resource type
    int population = 0;          // +0x40 "upgrade_population"
    int givePopulation = 0;      // +0x44 "give_upgrade_population"
    int speedupCb = 0;           // +0x48 "speedupcb"[level + 1]
};

struct ResourceRespawn { int amount = 0, time = 0; };   // "resourcesrespawn" / "...time", 5 entries

struct BuildingData {            // BuildingData, 0x1b4 bytes
    uint32_t id = 0;             // +0x00
    std::string name;            // +0x04
    int w = 0, h = 0;            // +0x08 "buildZoneX", +0x0c "buildZoneY"
    int offsetX = 0, offsetY = 0;  // +0x10, +0x14
    int iconY = 0;               // +0x18 "icon_y"
    int tileset = 0;             // +0x1c
    int layer = 0;               // +0x20
    int tab = 0, subtab = 0;     // +0x24, +0x28
    int visid = 0;               // +0x2c
    bool buy = false;            // +0x30
    int collectTime = 0;         // +0x34 "collecttime"
    int collectMoney = 0;        // +0x38 "collectmoney"
    unsigned gold = 0;           // +0x44
    unsigned cb = 0;             // +0x48
    int hp = 100;                // +0x4c ("hp", 0 -> 100)
    int requiresQuest = 0;       // +0x68
    int requiredId = 0, requiredCount = 0;  // +0x6c, +0x70 "requirestobuild" id=count
    unsigned constructionTime = 0;  // +0x74
    unsigned level = 0;          // +0x78
    int cost[11] = {};           // +0x7c lumber..oil (types 0..7)
    unsigned costPopulation = 0; // +0xa8
    unsigned givePopulation = 0; // +0xac
    int speedupCb = 0;           // +0xb0 "speedupcb"[0]
    std::vector<UpgradeInfo> upgrades;  // +0xb8
    const Contracts::Contract* delivery = nullptr;  // +0xc4 Contracts::GetContract("delivery_type")
    struct { float x, y; } parking[10], particles[10];   // +0xd0 / +0x120, counts +0xc8 / +0xcc
    unsigned parkingCount = 0, particleCount = 0;
    bool cantSellLast = false;   // +0x178
    int speedupResource = 8;     // +0x17c "speedupresources" type (default GOLD)
    int speedupAmount = 0;       // +0x180
    int produceResource = 8;     // +0x184 "produce_resource" type (default GOLD)
    int produceAmount = 0;       // +0x188
    std::vector<ResourceRespawn> respawn;  // +0x18c (5 entries when set)
    unsigned farmPatchDefault = 0;         // +0x198
    MetaData* farmPatchCost = nullptr;     // +0x19c (lists, or null when empty)
    MetaData* farmPatchCost2 = nullptr;    // +0x1a0
    MetaData* farmPatchLevels = nullptr;   // +0x1a4
    MetaData* unlockLevel = nullptr;       // +0x1a8
    uint32_t buildingClass = 0;  // +0x1ac "building_class" (13 forced for 0x96 0x3ea 0x3e9 0x13 0x72 0x3ee 0x433)
    std::vector<BuildingPart> parts;  // +0x1b0 linked list, in file order
};

bool Load();
const DecorData* GetDecoration(uint32_t id);     // Map::GetDecoration(unsigned) @0x130ebc
const BuildingData* GetBuilding(uint32_t id);    // Map::GetBuilding @0x11c3cc

Render::Texture* DecorImage(const DecorData* d);         // DecorData::LoadImage @0x134bb8
Render::Texture* PartImage(const BuildingPart* p);       // BuildingPart::LoadImage @0x11ee40

}  // namespace GameData
