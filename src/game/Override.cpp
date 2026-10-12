#include "game/Override.h"

#include <cstring>
#include <vector>

#include "game/GameData.h"
#include "game/Locations.h"
#include "game/Map.h"
#include "game/Tasks.h"

namespace Override {
namespace {

struct Parameter {               // 0x8c bytes
    uint32_t kind = 0;           // +0x00
    uint32_t id = 0;             // +0x04
    uint32_t sub = 0;            // +0x08
    char name[64] = {};          // +0x0c
    char value[64] = {};         // +0x4c
};
std::vector<Parameter> g_queue;  // 0x613e5c

}  // namespace

// @0x1da7ac Override::AddParameter
void AddParameter(uint32_t kind, uint32_t id, uint32_t sub, const char* name, const char* value) {
    if (!name || !value || std::strlen(name) > 0x3f || std::strlen(value) > 0x3f) return;
    Parameter p;
    p.kind = kind;
    p.id = id;
    p.sub = sub;
    std::strcpy(p.name, name);
    std::strcpy(p.value, value);
    if (p.value[0] != '\0')
        for (char* c = p.value + 1; *c; ++c)
            if (*c == '^') *c = '|';
    g_queue.push_back(p);
}

// @0x1daad4 Override::Apply
void Apply(int kind) {
    for (size_t i = 0; i < g_queue.size();) {
        const Parameter p = g_queue[i];
        bool mine = (int)p.kind == kind || kind == kAll;
        if (p.kind == kLocation) mine = kind == kLocation || kind == kAll;
        if (p.kind >= kItem && p.kind <= kLocation && !mine) {
            ++i;
            // (As the original: a location parameter that stays takes the one after it out of
            // the queue unapplied; its case runs on into the default one.)
            if (p.kind == kLocation && i < g_queue.size()) g_queue.erase(g_queue.begin() + (long)i);
            continue;
        }
        switch (p.kind) {
        case kTask: Tasks::ChangeTaskAttribute(p.id, p.name, p.value); break;
        case kSubTask: Tasks::ChangeSubTaskAttribute(p.id, p.sub, p.name, p.value); break;
        case kObject: Map::ChangeObjectAttribute(p.id, p.sub, p.name, p.value); break;
        case kBuilding: GameData::ChangeBuildingAttribute(p.id, p.sub, p.name, p.value); break;
        case kMap: Locations::ChangeMapAttribute(p.id, p.name, p.value); break;
        case kLocation: Locations::ChangeLocationAttribute(p.id, p.name, p.value); break;
        default:
            // UNVERIFIED (online, dormant): the other kinds are only sent by the server:
            // Items::ChangeItemAttribute (1), GameState::SetSetting(name, text) 0x1ac674 (4),
            // Contracts::ChangeContractAttribute (6),
            // EntityFactory::ChangeEntityAttribute (7), Collections::ChangeCollectionAttribute (8),
            // Chests::ChangeChestAttribute (9), Tavern::ChangeWarriorAttribute (0xc),
            // OG::SetAttribute (0xd), Items::ChangeItemsOfTypeAttributes (0xe) /
            // ChangeItemsOfSlotAttributes (0xf), Tags::ChangeTagAttribute of the tag `name` (0x10).
            // An unknown kind is just dropped.
            break;
        }
        g_queue.erase(g_queue.begin() + (long)i);
    }
}

}  // namespace Override
