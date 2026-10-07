#include "game/Contracts.h"

#include <pugixml.hpp>

#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>

#include "engine/FileManager.h"
#include "engine/Splitter.h"
#include "engine/Timer.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/Setting.h"
#include "game/StringTable.h"

namespace Contracts {
namespace {
std::map<uint32_t, Contract*> g_byId;               // HashMap<unsigned, Contract*, 1024> (0x611870)
std::vector<std::unique_ptr<Contract>> g_contracts;  // 0x611874
}

bool Init(const char* file) {
    double t0 = Timer::GetTime();
    uint32_t size = 0;
    uint8_t* buf = FileManager::LoadFile(file, size);
    if (!buf) {
        std::printf("Contracts::Init() Failed to load contract list (File not found: %s)\n", file);
        return false;
    }
    pugi::xml_document doc;
    pugi::xml_parse_result r = doc.load_buffer_inplace(buf, size, 0x74);
    if (!r) {
        std::printf("Contracts::Init() Failed to parse contract list (%s)\n", r.description());
        FileManager::FreeFile(buf);
        return false;
    }
    if (!doc.child("d_deliveries")) {
        std::printf("Contracts::Init() Incorrect file format (%s)\n", file);
        FileManager::FreeFile(buf);
        return false;
    }
    for (pugi::xml_node d = doc.child("d_deliveries").child("d_delivery"); d; d = d.next_sibling("d_delivery")) {
        uint32_t id = d.attribute("mid").as_uint();
        Contract* c = const_cast<Contract*>(GetContract(id));
        if (!c) {
            g_contracts.push_back(std::make_unique<Contract>());
            c = g_contracts.back().get();
            g_byId[id] = c;
        }
        c->id = id;
        c->type = d.attribute("type").as_uint();
        c->title = StringTable::GetString(d.attribute("title").value());
        c->count = 0;
        c->missions.clear();
        c->missions.reserve(5);
        for (pugi::xml_node m = d.child("d_mission"); m; m = m.next_sibling("d_mission")) {
            ContractMission cm;
            cm.title = StringTable::GetString(m.attribute("title").value());
            cm.item = Items::GetItemInfo(m.attribute("title").value());
            cm.icon = m.attribute("icon").value();
            cm.price = m.attribute("price").as_uint();
            cm.priceResource = GameState::ExternalResourceTypeToInternal(m.attribute("price_resource").as_uint());
            cm.priceResourceCount = m.attribute("price_resource_count").as_uint();
            cm.playerLevel = m.attribute("player_lvl").as_uint();
            cm.buildingLevel = m.attribute("bulding_lvl").as_uint();
            unsigned plant = m.attribute("time_plant").as_uint();
            float contractCoeff = GameState::GetSetting("contract_coeff");
            for (const std::string& part : SplitterParse(m.attribute("time_grow").value(), ":")) {
                int t = (int)((float)std::atoi(part.c_str()) * GameState::GetSetting("farm_coeff"));
                cm.growTimes.push_back(t);
                cm.growTotal += t;
            }
            cm.timeHarvest = m.attribute("time_harvest").as_uint();
            cm.timeClean = m.attribute("time_clean").as_uint();
            cm.rewardGold = m.attribute("reward_gold").as_uint();
            cm.rewardXp = m.attribute("reward_xp").as_uint();
            cm.speedupCost2 = m.attribute("speedup_cost2").as_uint();
            cm.rewardResource = GameState::ExternalResourceTypeToInternal(m.attribute("reward_resource").as_uint());
            cm.rewardResourceCount = m.attribute("reward_resource_count").as_uint();
            cm.timePlant = (int)((float)plant * contractCoeff);
            c->missions.push_back(std::move(cm));
            ++c->count;
        }
    }
    FileManager::FreeFile(buf);
    double t1 = Timer::GetTime();
    std::printf("Contracts::Init() Loaded contract list in %.3fms\n", (t1 - t0) * 1000.0);
    return true;
}

const Contract* GetContract(uint32_t id) {
    auto it = g_byId.find(id);
    return it == g_byId.end() ? nullptr : it->second;
}

const Contract* EnumContract(unsigned i) { return i < g_contracts.size() ? g_contracts[i].get() : nullptr; }

}  // namespace Contracts
