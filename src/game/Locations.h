// Locations: locations.xml, the catalogue of campaigns and missions and the maps each one holds
// (libkingdom.so 5.11, Locations @0x1b1a04..0x1b5f58). A location is a campaign (story maps) or a
// mission ("raid": a repeatable map with stars); the city is the special location 0x400.
#pragma once
#include <cstdint>
#include <vector>

class MetaData;
class MetaExpression;

namespace Locations {

struct MapInfo {                       // 0x38 bytes, @0x1b1a04
    uint32_t id = 0;                   // +0x00 "id"
    const char32_t* name = nullptr;    // +0x04 "name" (string table)
    bool customFog = false;            // +0x08 "custom_fog"
    bool openFog = false;              // +0x09 "open_fog"
    uint32_t nodeQuality = 0;          // +0x0c "node_quality"
    MetaData* nodeContents = nullptr;  // +0x10 "node_contents" (item ids)
    MetaExpression* meta = nullptr;    // +0x14 "meta"
    float goldMult[4] = {1, 1, 1, 1};  // +0x18 by mission stars (Override "gold_mult")
    float xpMult[4] = {1, 1, 1, 1};    // +0x28 (Override "xp_mult")
};

struct ItemDrop {                      // 0xc bytes
    uint32_t id = 0;
    uint32_t star = 0;                 // 0: at any star count
    int rate = 0;                      // 0 HIGH, 1 MEDIUM, 2 anything else
};

struct LocationInfo {                  // 0xec bytes
    uint32_t id = 0;                   // +0x00 "location_id"
    const char32_t* name = nullptr;    // +0x04
    const char32_t* description = nullptr;   // +0x08
    MetaData* endTasks = nullptr;      // +0x0c "end_tasks_id": blocks (',') of tasks (':')
    std::vector<int> clearTasks;       // +0x10 "clear_tasks"
    std::vector<int> removeItems;      // +0x20 "remove_items_after_restart"
    uint32_t campaign = 0;             // +0x30 "campaign"
    MetaData* tasks = nullptr;         // +0x34 "tasks_id": any one completed unlocks it
    uint32_t unlockMapId = 0;          // +0x38
    uint32_t level = 0;                // +0x3c
    MetaData* returnTasks = nullptr;   // +0x40 (only location 0x2f, InitSpecial)
    uint32_t timeout = 0;              // +0x44 seconds
    uint32_t restartCost = 0;          // +0x48
    bool disabled = false;             // +0x4c
    bool mission = false;              // +0x4d
    uint32_t startMap = 0;             // +0x50 the first map, or the one with start="1"
    // +0x54: only the city (0x400) has it after loading. UNVERIFIED (milestone 5): the global map
    // sets it for the locations it shows.
    bool onGlobalMap = false;
    uint32_t starsCount[3] = {};       // +0x58 completions needed for 1, 2, 3 stars
    std::vector<MapInfo> maps;         // +0x64
    std::vector<std::pair<int, int>> spawnPoints;   // +0xa8 the hero's tile, per map
    MetaData* requiresResources = nullptr;   // +0xbc
    MetaData* items = nullptr;         // +0xc0
    MetaData* bat = nullptr;           // +0xc4
    MetaData* enemies = nullptr;       // +0xc8 the first map's "enemies" (entity ids)
    int gold = 0;                      // +0xcc Drops/gold
    int xp = 0;                        // +0xd0 Drops/xp
    std::vector<ItemDrop> itemDrops;   // +0xd4

    bool HadItemDropChance(uint32_t item, uint32_t star) const;   // @0x1b1a9c
    int GetItemDropChance(uint32_t item, uint32_t star) const;    // @0x1b1afc (2 when not listed)
    bool HasEntityWithClass(uint32_t enemyClass) const;           // @0x1b240c
};

bool Init(const char* file);                       // @0x1b4a70
void InitSpecial();                                // @0x1b2530 the city's location, 0x2f's tasks
LocationInfo* GetLocationInfo(uint32_t id);        // @0x1b24c8
LocationInfo* EnumLocationInfo(unsigned i);        // @0x1b4900
bool GetLocationType(const LocationInfo* loc);     // @0x1b1b64 locations 0x45, 0x47, 0x4c, 0x53
bool IsExpansionMap(uint32_t map);                 // @0x1b1b9c
bool IsCampaignCompleted(const LocationInfo* loc); // @0x1b21a4
bool IsLocationLocked(const LocationInfo* loc);    // @0x1b2258
int GetLocationTaskCompleted(const LocationInfo* loc, bool mainOnly);   // @0x1b22e8
int GetLocationTaskCount(const LocationInfo* loc, bool mainOnly);       // @0x1b2344
// @0x1b2a28: the shortest time left on a started mission, times 0.75 (999999 without one).
uint32_t GetNearestUnfinishedFarmMapTime();
void ChangeMapAttribute(uint32_t map, const char* name, const char* value);        // @0x1b2ba8
void ChangeLocationAttribute(uint32_t loc, const char* name, const char* value);   // @0x1b3830
uint32_t GetLastVisitedBattleMap();                // @0x1b31b4
// @0x1b3318: the start map of an open location that lists a monster of this class.
uint32_t GetMapWithEntityOfClass(uint32_t enemyClass, bool otherMap);
int GetMissionStarCount(uint32_t map, int extra);  // @0x1b3d0c
int GetMissionRemainingTime(uint32_t map);         // @0x1b3df8
bool CanCraftNodeDropItem(uint32_t map, uint32_t item);   // @0x1b3f10
bool IsMissionMap(uint32_t map);                   // @0x1b400c
uint32_t GetCampaignForMap(uint32_t map);          // @0x1b40b0
uint32_t GetMissionForMap(uint32_t map);           // @0x1b417c
LocationInfo* GetLocationWithMap(uint32_t map);    // @0x1b4248
MetaData* GetMapBat(uint32_t map);                 // @0x1b42d4
const char32_t* GetMapName(uint32_t map);          // @0x1b4364
LocationInfo* GetCampaignLocation(uint32_t campaign);   // @0x1b43fc
LocationInfo* GetMissionLocation(uint32_t campaign);    // @0x1b4480
void GetPlayerDefaultPosition(uint32_t map, int* x, int* y);   // @0x1b4510 (-1, -1 when unknown)
uint32_t GetMapNodeQuality(uint32_t map);          // @0x1b45c4
bool MapHasCustomFog(uint32_t map);                // @0x1b4664 (the city: yes)
bool MapOpensFogAfterCompletion(uint32_t map);     // @0x1b4708
float GetMapXPMult(uint32_t map, uint32_t stars);  // @0x1b47a8
float GetMapGoldMult(uint32_t map, uint32_t stars);   // @0x1b4854
MetaData* GetMapNodeContents(uint32_t map);        // @0x1b4928
MetaExpression* GetMapMeta(uint32_t map);          // @0x1b49cc

}  // namespace Locations

// The "items" attribute: '|' lists of ',' lists of '=' lists of terminals.
MetaData* ParseLocationEqualityDividedList(const char*& p, bool wrap);   // @0x1d2ad8
MetaData* ParseLocationCommaDividedList(const char*& p, bool wrap);      // @0x1d2b7c
MetaData* ParseLocationWallDividedList(const char*& p, bool wrap);       // @0x1d2c28
