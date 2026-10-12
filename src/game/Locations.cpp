#include "game/Locations.h"

#include <pugixml.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "engine/FileManager.h"
#include "engine/Timer.h"
#include "game/EntityData.h"
#include "game/EntityManager.h"
#include "game/GameState.h"
#include "game/MetaData.h"
#include "game/MetaExpression.h"
#include "game/StringTable.h"
#include "game/Tasks.h"

// @0x1d2ad8: terminals split by '='.
MetaData* ParseLocationEqualityDividedList(const char*& p, bool wrap) {
    MetaData* node = ParseTerminal(p);
    if (*p == '=' || wrap) {
        MetaData* list = new MetaData(MetaData::kList);
        list->AppendChild(node);
        node = list;
    }
    while (*p == '=') {
        ++p;
        if (MetaData* m = ParseTerminal(p)) node->AppendChild(m);
    }
    return node;
}

// @0x1d2b7c: equality lists split by ','.
MetaData* ParseLocationCommaDividedList(const char*& p, bool wrap) {
    MetaData* node = ParseLocationEqualityDividedList(p, wrap);
    if (*p == ',' || wrap) {
        MetaData* list = new MetaData(MetaData::kList);
        list->AppendChild(node);
        node = list;
    }
    while (*p == ',') {
        ++p;
        if (MetaData* m = ParseLocationEqualityDividedList(p, wrap)) node->AppendChild(m);
    }
    return node;
}

// @0x1d2c28: comma lists split by '|'.
MetaData* ParseLocationWallDividedList(const char*& p, bool wrap) {
    MetaData* node = ParseLocationCommaDividedList(p, wrap);
    if (*p == '|' || wrap) {
        MetaData* list = new MetaData(MetaData::kList);
        list->AppendChild(node);
        node = list;
    }
    while (*p == '|') {
        ++p;
        if (MetaData* m = ParseLocationCommaDividedList(p, wrap)) node->AppendChild(m);
    }
    return node;
}

namespace Locations {

namespace {

std::vector<LocationInfo*> g_locations;   // 0x61364c (the infos come from a pool, 0x61363c)

// The maps other than these count as expansion maps (0x58388c).
const uint32_t kBaseMaps[9] = {0, 47, 13, 14, 15, 21, 109, 112, 11};

// The comma-separated numbers of Init's and ChangeLocationAttribute's lists.
void ParseIntList(const char* p, std::vector<int>& out) {
    if (!p || !*p) return;
    do {
        out.push_back(std::atoi(p));
        const char* c = std::strchr(p, ',');
        if (!c) break;
        p = c + 1;
    } while (*p);
}

MetaData* ParseOrNull(const char* text, const char* delims, const char* wrap) {
    if (!*text) return nullptr;
    return ParseCustomStyleData(text, delims, wrap);
}

// The map's entry in whichever location lists it (the lookup every per-map getter repeats).
MapInfo* FindMap(uint32_t map, LocationInfo** owner = nullptr) {
    for (LocationInfo* loc : g_locations)
        for (MapInfo& m : loc->maps)
            if (m.id == map) {
                if (owner) *owner = loc;
                return &m;
            }
    return nullptr;
}

// The mission that lists the map.
LocationInfo* FindMission(uint32_t map) {
    for (LocationInfo* loc : g_locations) {
        if (!loc->mission) continue;
        for (MapInfo& m : loc->maps)
            if (m.id == map) return loc;
    }
    return nullptr;
}

// GetCampaignForMap / GetMissionForMap leave out the campaign numbers 1000, 1001 and above 1002.
bool CountedCampaign(uint32_t c) { return c != 0 && (c <= 999 || c == 0x3ea); }

template <class T> void Free(T*& p) {
    delete p;
    p = nullptr;
}

}  // namespace

// @0x1b1a9c
bool LocationInfo::HadItemDropChance(uint32_t item, uint32_t star) const {
    for (const ItemDrop& d : itemDrops)
        if (d.id == item && d.star == star) return true;
    return false;
}

// @0x1b1afc
int LocationInfo::GetItemDropChance(uint32_t item, uint32_t star) const {
    for (const ItemDrop& d : itemDrops)
        if (d.id == item && (d.star == 0 || d.star == star)) return d.rate;
    return 2;
}

// @0x1b240c
bool LocationInfo::HasEntityWithClass(uint32_t enemyClass) const {
    if (!enemies || startMap == 0xd) return false;
    for (const MetaData* m = enemies->GetChild(0); m; m = m->GetNext()) {
        EntityData* d = EntityFactory::GetEntityByID(m->GetInt());
        if (d && (uint32_t)d->enemyClass == enemyClass) return true;
    }
    return false;
}

// @0x1b24c8
LocationInfo* GetLocationInfo(uint32_t id) {
    for (LocationInfo* loc : g_locations)
        if (loc->id == id) return loc;
    return nullptr;
}

// @0x1b4900
LocationInfo* EnumLocationInfo(unsigned i) { return i < g_locations.size() ? g_locations[i] : nullptr; }

// @0x1b4a70 Locations::Init. A location read again (a content pack's copy of the file) replaces the
// first.
bool Init(const char* file) {
    double t0 = Timer::GetTime();
    uint32_t size = 0;
    uint8_t* buf = FileManager::LoadFile(file, size);
    if (!buf) {
        std::printf("Locations::Init() Failed to load location list (File not found: %s)\n", file);
        return false;
    }
    pugi::xml_document doc;
    pugi::xml_parse_result r = doc.load_buffer_inplace(buf, size, pugi::parse_default, pugi::encoding_auto);
    if (!r) {
        std::printf("Locations::Init() Failed to load location list (%s)\n", r.description());
        FileManager::FreeFile(buf);
        return false;
    }
    pugi::xml_node root = doc.child("Locations");
    if (!root) {
        std::printf("Locations::Init() Incorrect file format (%s)\n", file);
        FileManager::FreeFile(buf);
        return false;
    }
    g_locations.reserve(0x10);
    for (pugi::xml_node x = root.child("Location"); x; x = x.next_sibling("Location")) {
        LocationInfo* loc = GetLocationInfo(x.attribute("location_id").as_uint());
        if (!loc) {
            loc = new LocationInfo;
            g_locations.push_back(loc);
        } else {
            for (MapInfo& m : loc->maps) {
                Free(m.nodeContents);
                Free(m.meta);
            }
            Free(loc->endTasks);
            Free(loc->tasks);
            Free(loc->requiresResources);
            Free(loc->items);
            Free(loc->bat);
            Free(loc->enemies);
            Free(loc->returnTasks);
            loc->itemDrops.clear();
            loc->clearTasks.clear();
            loc->removeItems.clear();
            loc->maps.clear();
            loc->spawnPoints.clear();
        }
        loc->id = x.attribute("location_id").as_uint();
        loc->name = StringTable::GetString(x.attribute("name").value());
        loc->description = StringTable::GetString(x.attribute("description").value());
        loc->endTasks = ParseOrNull(x.attribute("end_tasks_id").value(), ",:", "11");
        ParseIntList(x.attribute("clear_tasks").value(), loc->clearTasks);
        ParseIntList(x.attribute("remove_items_after_restart").value(), loc->removeItems);
        loc->campaign = x.attribute("campaign").as_uint();
        loc->tasks = ParseOrNull(x.attribute("tasks_id").value(), ",", "1");
        loc->unlockMapId = x.attribute("unlock_map_id").as_uint();
        loc->level = x.attribute("level").as_uint();
        loc->returnTasks = nullptr;
        loc->timeout = x.attribute("timeout").as_uint();
        loc->restartCost = x.attribute("restart_cost").as_uint();
        loc->disabled = x.attribute("is_disabled").as_bool();
        loc->mission = x.attribute("is_mission").as_bool();
        loc->onGlobalMap = false;
        {
            // PORT: the original writes as many numbers as the text has; the three it reads are kept.
            std::vector<int> stars;
            ParseIntList(x.attribute("stars_count").value(), stars);
            for (size_t i = 0; i < stars.size() && i < 3; ++i) loc->starsCount[i] = (uint32_t)stars[i];
        }
        pugi::xml_node maps = x.child("Maps");
        loc->startMap = maps.child("Map").attribute("id").as_uint();
        for (pugi::xml_node m = maps.child("Map"); m; m = m.next_sibling("Map")) {
            MapInfo info;
            info.id = m.attribute("id").as_uint();
            info.name = StringTable::GetString(m.attribute("name").value());
            info.customFog = m.attribute("custom_fog").as_bool();
            info.openFog = m.attribute("open_fog").as_bool();
            info.nodeQuality = m.attribute("node_quality").as_uint();
            info.nodeContents = ParseOrNull(m.attribute("node_contents").value(), ",", "1");
            const char* meta = m.attribute("meta").value();
            if (std::strlen(meta) > 1) {
                char* text = new char[std::strlen(meta) + 1];
                std::strcpy(text, meta);
                info.meta = new MetaExpression(text, true);
            }
            loc->maps.push_back(info);
            if (m.attribute("start").as_bool()) loc->startMap = m.attribute("id").as_uint();
            loc->spawnPoints.emplace_back(m.attribute("spawnpoint_x").as_int(), m.attribute("spawnpoint_y").as_int());
        }
        loc->requiresResources = ParseOrNull(x.attribute("requires_resources").value(), "|,=:", "1110");
        {
            const char* p = x.attribute("items").value();
            loc->items = *p ? ParseLocationWallDividedList(p, true) : nullptr;
        }
        {
            const char* p = x.attribute("bat").value();
            loc->bat = *p ? ParseWallDividedList(p, true) : nullptr;
        }
        loc->enemies = ParseOrNull(maps.child("Map").attribute("enemies").value(), ",", "1");
        pugi::xml_node drops = x.child("Drops");
        loc->gold = std::atoi(drops.child("gold").child_value());
        loc->xp = std::atoi(drops.child("xp").child_value());
        for (pugi::xml_node d = x.child("ItemsDrops").child("Item"); d; d = d.next_sibling("Item")) {
            ItemDrop drop;
            drop.id = d.attribute("id").as_uint();
            drop.star = d.attribute("star").as_uint();
            const char* rate = d.attribute("droprate").value();
            if (loc->HadItemDropChance(drop.id, drop.star)) continue;
            drop.rate = !std::strcmp(rate, "MEDIUM") ? 1 : !std::strcmp(rate, "HIGH") ? 0 : 2;
            loc->itemDrops.push_back(drop);
        }
    }
    std::printf("Locations::Init() Loaded location list in %.3fms\n", (Timer::GetTime() - t0) * 1000.0);
    FileManager::FreeFile(buf);
    return true;
}

// @0x1b2530 Locations::InitSpecial: the city is location 0x400 (map 0, the hero at (0x1b, 0xd));
// location 0x2f gets the tasks that send the player back to it.
void InitSpecial() {
    LocationInfo* city = GetLocationInfo(0x400);
    if (!city) {
        city = new LocationInfo;
        g_locations.push_back(city);
    } else {
        city->spawnPoints.clear();
        city->maps.clear();
    }
    city->startMap = 0;
    city->id = 0x400;
    city->name = nullptr;
    city->onGlobalMap = true;
    city->description = nullptr;
    city->endTasks = nullptr;
    city->campaign = 0;
    city->tasks = nullptr;
    city->unlockMapId = 0;
    city->level = 0;
    city->returnTasks = nullptr;
    city->timeout = 0;
    city->restartCost = 0;
    city->disabled = false;
    city->mission = false;
    MapInfo map;
    map.customFog = true;
    city->maps.push_back(map);
    city->spawnPoints.emplace_back(0x1b, 0xd);
    city->enemies = nullptr;
    city->requiresResources = nullptr;
    city->items = nullptr;
    city->bat = nullptr;
    if (LocationInfo* loc = GetLocationInfo(0x2f)) {
        Free(loc->returnTasks);
        const char* p = "753,862,864,730";
        loc->returnTasks = ParseCustomStyleData(p, ",", "1");
    }
}

// @0x1b1b64
bool GetLocationType(const LocationInfo* loc) {
    if (!loc) return false;
    return loc->id == 0x45 || loc->id == 0x47 || loc->id == 0x4c || loc->id == 0x53;
}

// @0x1b1b9c
bool IsExpansionMap(uint32_t map) {
    for (uint32_t m : kBaseMaps)
        if (m == map) return false;
    return true;
}

// @0x1b21a4: one of the campaign's end blocks has all its tasks completed.
bool IsCampaignCompleted(const LocationInfo* loc) {
    if (!loc || loc->mission || !loc->endTasks) return false;
    unsigned blocks = loc->endTasks->GetChildrenCount();
    for (unsigned b = 0; b < blocks; ++b) {
        const MetaData* block = loc->endTasks->GetChild(b);
        unsigned n = block->GetChildrenCount(), done = 0;
        for (unsigned i = 0; i < n; ++i)
            if (GameState::TaskCompleted((unsigned)block->GetChild(i)->GetInt())) ++done;
        if (done != 0 && block->GetChildrenCount() == done) return true;
    }
    return false;
}

// @0x1b2258: below its level, or none of its unlocking tasks completed (a task 0 always unlocks).
bool IsLocationLocked(const LocationInfo* loc) {
    if (!loc) return false;
    if ((uint32_t)GameState::GetLevel() < loc->level) return true;
    if (!loc->tasks) return false;
    unsigned n = loc->tasks->GetChildrenCount();
    for (unsigned i = 0; i < n; ++i) {
        unsigned task = (unsigned)loc->tasks->GetChild(i)->GetInt();
        if (task == 0 || GameState::TaskCompleted(task)) return false;
    }
    return true;
}

// @0x1b22e8
int GetLocationTaskCompleted(const LocationInfo* loc, bool mainOnly) {
    if (!loc) return 0;
    int n = 0;
    for (const MapInfo& m : loc->maps) n += Tasks::GetMapTaskCompleted(m.id, mainOnly);
    return n;
}

// @0x1b2344
int GetLocationTaskCount(const LocationInfo* loc, bool mainOnly) {
    if (!loc) return 0;
    int n = 0;
    for (const MapInfo& m : loc->maps) n += Tasks::GetMapTaskCount(m.id, mainOnly);
    return n;
}

// @0x1b2a28
uint32_t GetNearestUnfinishedFarmMapTime() {
    uint32_t best = 999999;
    for (LocationInfo* loc : g_locations) {
        if (!loc->mission) continue;
        for (MapInfo& m : loc->maps) {
            uint32_t start = GameState::GetMapStartTime(m.id);
            if (start == 0 || start + loc->timeout <= (uint32_t)Timer::GetGlobalTime()) continue;
            uint32_t left = start + loc->timeout - (uint32_t)Timer::GetGlobalTime();
            uint32_t t = (uint32_t)((float)left * 0.75f);
            if (t <= best) best = t;
        }
    }
    return best;
}

// @0x1b2ba8 (Override::Apply).
void ChangeMapAttribute(uint32_t map, const char* name, const char* value) {
    int n = std::atoi(value);
    for (LocationInfo* loc : g_locations) {
        for (MapInfo& m : loc->maps) {
            if (m.id != map) continue;
            const char* p = value;
            if (!std::strcmp(name, "quality")) {
                m.nodeQuality = (uint32_t)n;
            } else if (!std::strcmp(name, "contents")) {
                delete m.nodeContents;
                m.nodeContents = ParseCustomStyleData(p, ",", "1");
            } else if (!std::strcmp(name, "gold_mult") || !std::strcmp(name, "xp_mult")) {
                // As the original: each of the (up to four) numbers reads the list's first one and
                // writes the first slot, so only the multiplier without stars changes.
                bool gold = !std::strcmp(name, "gold_mult");
                MetaData* list = ParseCustomStyleData(p, ",", "1");
                for (unsigned i = 0; i < 4 && i < list->GetChildrenCount(); ++i)
                    (gold ? m.goldMult : m.xpMult)[0] = list->GetChild(0)->GetFloat();
                delete list;
            } else if (!std::strcmp(name, "open_fog")) {
                m.openFog = n == 1;
            }
        }
    }
}

// @0x1b3830 (Override::Apply).
void ChangeLocationAttribute(uint32_t id, const char* name, const char* value) {
    uint32_t n = (uint32_t)std::atoi(value);
    for (LocationInfo* loc : g_locations) {
        if (loc->id != id) continue;
        const char* p = value;
        if (!std::strcmp(name, "end_tasks")) {
            delete loc->endTasks;
            loc->endTasks = ParseCustomStyleData(p, ",:", "11");
        } else if (!std::strcmp(name, "reset_tasks")) {
            loc->clearTasks.clear();
            ParseIntList(p, loc->clearTasks);
        } else if (!std::strcmp(name, "remove_items")) {
            loc->removeItems.clear();
            ParseIntList(p, loc->removeItems);
        } else if (!std::strcmp(name, "req_tasks")) {
            delete loc->tasks;
            loc->tasks = ParseCustomStyleData(p, ",", "1");
        } else if (!std::strcmp(name, "req_map")) {
            loc->unlockMapId = n;
        } else if (!std::strcmp(name, "req_level")) {
            loc->level = n;
        } else if (!std::strcmp(name, "req_res")) {
            delete loc->requiresResources;
            loc->requiresResources = ParseCustomStyleData(p, "|,=:", "1110");
        } else if (!std::strcmp(name, "campaign_id")) {
            loc->campaign = n;
        } else if (!std::strcmp(name, "timeout")) {
            loc->timeout = n;
        } else if (!std::strcmp(name, "restart_cb")) {
            loc->restartCost = n;
        } else if (!std::strcmp(name, "disabled")) {
            loc->disabled = n != 0;
        } else if (!std::strcmp(name, "mission")) {
            loc->mission = n != 0;
        } else if (!std::strcmp(name, "star_count")) {
            std::vector<int> stars;
            ParseIntList(p, stars);
            for (size_t i = 0; i < stars.size() && i < 3; ++i) loc->starsCount[i] = (uint32_t)stars[i];
        }
    }
}

// @0x1b31b4: the campaign map visited last; a location whose return task is active wins.
uint32_t GetLastVisitedBattleMap() {
    uint32_t result = 0, best = 0;
    for (LocationInfo* loc : g_locations) {
        if (!loc->onGlobalMap || loc->mission || loc->campaign >= 100 || loc->startMap == 0) continue;
        for (MapInfo& m : loc->maps) {
            if (!GameState::HasMapVisitTime(m.id)) continue;
            uint32_t t = GameState::GetMapVisitTime(m.id);
            if (best < t) {
                result = m.id;
                best = t;
            }
        }
        if (!loc->returnTasks) continue;
        unsigned n = loc->returnTasks->GetChildrenCount();
        for (unsigned i = 0; i < n; ++i)
            if (Tasks::IsTaskActive((uint32_t)loc->returnTasks->GetChild(i)->GetInt(), false)) {
                result = loc->startMap;
                best = 0xffffffff;
            }
    }
    return result;
}

// @0x1b3318: open missions first, then open campaigns (not location 0x49), then any mission.
// Unless anyMap, or a living monster of the class walks the current map, the current map is skipped.
uint32_t GetMapWithEntityOfClass(uint32_t enemyClass, bool anyMap) {
    if (!anyMap) anyMap = EntityManager::GetEntityByEnemyClassID(enemyClass) != nullptr;
    for (int pass = 0; pass < 3; ++pass) {
        for (LocationInfo* loc : g_locations) {
            if (!loc->onGlobalMap) continue;
            if (pass == 1 ? loc->mission : !loc->mission) continue;
            if (pass != 2 && IsLocationLocked(loc)) continue;
            if (pass == 1 && loc->id == 0x49) continue;
            if (!loc->HasEntityWithClass(enemyClass)) continue;
            if (anyMap || GameState::GetCurrentMapID() != loc->startMap) return loc->startMap;
        }
    }
    return 0;
}

// @0x1b3d0c
int GetMissionStarCount(uint32_t map, int extra) {
    LocationInfo* loc = FindMission(map);
    if (!loc) return 0;
    int stars = 0;
    for (int i = 1; i != 4; ++i)
        if (loc->starsCount[i - 1] <= (uint32_t)(GameState::GetMapCompletionCount(map) + extra)) stars = i;
    return stars;
}

// @0x1b3df8
int GetMissionRemainingTime(uint32_t map) {
    LocationInfo* loc = FindMission(map);
    if (!loc) return 0;
    uint32_t start = GameState::GetMapStartTime(map);
    if (start == 0) return (int)loc->timeout;
    if (start + loc->timeout < (uint32_t)Timer::GetGlobalTime()) return 0;
    return (int)(start + loc->timeout) - Timer::GetGlobalTime();
}

// @0x1b3f10 (every location's entry for the map is asked)
bool CanCraftNodeDropItem(uint32_t map, uint32_t item) {
    if (map == 0) return false;
    for (LocationInfo* loc : g_locations) {
        MapInfo* found = nullptr;
        for (MapInfo& m : loc->maps)
            if (m.id == map) {
                found = &m;
                break;
            }
        if (!found || !found->nodeContents) continue;
        unsigned n = found->nodeContents->GetChildrenCount();
        for (unsigned i = 0; i < n; ++i)
            if ((uint32_t)found->nodeContents->GetChild(i)->GetInt() == item) return true;
    }
    return false;
}

// @0x1b400c
bool IsMissionMap(uint32_t map) { return map != 0 && FindMission(map) != nullptr; }

// @0x1b40b0
uint32_t GetCampaignForMap(uint32_t map) {
    if (map == 0 || map == 0x2f) return 0;
    for (LocationInfo* loc : g_locations) {
        if (loc->mission || !CountedCampaign(loc->campaign)) continue;
        for (MapInfo& m : loc->maps)
            if (m.id == map) return loc->campaign;
    }
    return 0;
}

// @0x1b417c
uint32_t GetMissionForMap(uint32_t map) {
    if (map == 0 || map == 0x2f) return 0;
    for (LocationInfo* loc : g_locations) {
        if (!loc->mission || !CountedCampaign(loc->campaign)) continue;
        for (MapInfo& m : loc->maps)
            if (m.id == map) return loc->campaign;
    }
    return 0;
}

// @0x1b4248
LocationInfo* GetLocationWithMap(uint32_t map) {
    LocationInfo* loc = nullptr;
    FindMap(map, &loc);
    return loc;
}

// @0x1b42d4
MetaData* GetMapBat(uint32_t map) {
    LocationInfo* loc = GetLocationWithMap(map);
    return loc ? loc->bat : nullptr;
}

// @0x1b4364
const char32_t* GetMapName(uint32_t map) {
    MapInfo* m = FindMap(map);
    return m ? m->name : nullptr;
}

// @0x1b43fc
LocationInfo* GetCampaignLocation(uint32_t campaign) {
    for (LocationInfo* loc : g_locations)
        if (!loc->maps.empty() && loc->campaign == campaign && !loc->mission) return loc;
    return nullptr;
}

// @0x1b4480
LocationInfo* GetMissionLocation(uint32_t campaign) {
    for (LocationInfo* loc : g_locations)
        if (!loc->maps.empty() && loc->campaign == campaign && loc->mission) return loc;
    return nullptr;
}

// @0x1b4510
void GetPlayerDefaultPosition(uint32_t map, int* x, int* y) {
    *x = -1;
    *y = -1;
    for (LocationInfo* loc : g_locations)
        for (size_t i = 0; i < loc->maps.size(); ++i)
            if (loc->maps[i].id == map) {
                *x = loc->spawnPoints[i].first;
                *y = loc->spawnPoints[i].second;
                return;
            }
}

// @0x1b45c4
uint32_t GetMapNodeQuality(uint32_t map) {
    MapInfo* m = map ? FindMap(map) : nullptr;
    return m ? m->nodeQuality : 0;
}

// @0x1b4664
bool MapHasCustomFog(uint32_t map) {
    if (map == 0) return true;
    MapInfo* m = FindMap(map);
    return m && m->customFog;
}

// @0x1b4708
bool MapOpensFogAfterCompletion(uint32_t map) {
    MapInfo* m = map ? FindMap(map) : nullptr;
    return m && m->openFog;
}

// @0x1b47a8
float GetMapXPMult(uint32_t map, uint32_t stars) {
    MapInfo* m = map ? FindMap(map) : nullptr;
    return m ? m->xpMult[stars > 3 ? 0 : stars] : 0.f;
}

// @0x1b4854
float GetMapGoldMult(uint32_t map, uint32_t stars) {
    MapInfo* m = map ? FindMap(map) : nullptr;
    return m ? m->goldMult[stars > 3 ? 0 : stars] : 0.f;
}

// @0x1b4928
MetaData* GetMapNodeContents(uint32_t map) {
    MapInfo* m = map ? FindMap(map) : nullptr;
    return m ? m->nodeContents : nullptr;
}

// @0x1b49cc
MetaExpression* GetMapMeta(uint32_t map) {
    MapInfo* m = map ? FindMap(map) : nullptr;
    return m ? m->meta : nullptr;
}

}  // namespace Locations
