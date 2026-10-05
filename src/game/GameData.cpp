#include "game/GameData.h"

#include <pugixml.hpp>

#include <cstdio>
#include <cstring>
#include <memory>

#include "engine/FileManager.h"
#include "engine/Resources.h"
#include "game/GameState.h"
#include "game/MetaData.h"

namespace GameData {
namespace {

std::map<uint32_t, DecorData> g_decors;
std::map<uint32_t, BuildingData> g_buildings;

bool LoadXml(const char* path, pugi::xml_document& doc) {
    uint32_t n = 0;
    uint8_t* p = FileManager::LoadFile(path, n);
    if (!p) {
        std::fprintf(stderr, "GameData: missing %s\n", path);
        return false;
    }
    pugi::xml_parse_result r = doc.load_buffer(p, n);
    FileManager::FreeFile(p);
    if (!r) std::fprintf(stderr, "GameData: %s: %s\n", path, r.description());
    return (bool)r;
}

void LoadDecorations() {
    pugi::xml_document doc;
    if (!LoadXml("../resource/res_files/1Original/decors.xml", doc)) return;
    for (auto n : doc.child("DecorOptions").children("Decor")) {
        DecorData d;
        d.id = n.attribute("id").as_uint();
        d.img = n.attribute("img").value();
        d.name = n.attribute("name").value();
        d.w = (int)n.attribute("lockzoneX").as_uint();
        d.h = (int)n.attribute("lockzoneY").as_uint();
        d.ox = n.attribute("x").as_int();
        d.oy = n.attribute("y").as_int();
        d.layer = n.attribute("layer").as_int();
        g_decors[d.id] = d;
    }
}

// Value of child i of a list, 0 past the end (the loader's "count > i ? GetInt : 0" pattern).
int IntAt(const MetaData* m, unsigned i) { return i < m->GetChildrenCount() ? m->GetChild(i)->GetInt() : 0; }

MetaData* ParseAttr(pugi::xml_node n, const char* name, const char* delims, const char* wrap) {
    const char* v = n.attribute(name).value();
    return ParseCustomStyleData(v, delims, wrap);
}

MetaData* ParseOptional(pugi::xml_node n, const char* name) {
    const char* v = n.attribute(name).value();
    return *v ? ParseCustomStyleData(v, ",", "1") : nullptr;
}

void LoadBuildings() {
    pugi::xml_document doc;
    if (!LoadXml("../resource/res_files/1Original/buildings.xml", doc)) return;
    for (auto n : doc.child("BuildingsOptionsDefault").children("Building")) {
        uint32_t id = n.attribute("id").as_uint();
        BuildingData& b = g_buildings[id];   // (a repeated id is reloaded in place)
        b = BuildingData();
        b.id = id;
        b.name = n.attribute("name").value();
        b.offsetX = n.attribute("offsetX").as_int();
        b.offsetY = n.attribute("offsetY").as_int();
        b.w = n.attribute("buildZoneX").as_int();
        b.h = n.attribute("buildZoneY").as_int();
        b.iconY = n.attribute("icon_y").as_int();
        b.buildingClass = n.attribute("building_class").as_uint();
        if (id == 0x96 || id == 0x3ea || id == 0x3e9 || id == 0x13 || id == 0x72 || id == 0x3ee || id == 0x433)
            b.buildingClass = 0xd;
        b.layer = n.attribute("layer").as_int();
        std::unique_ptr<MetaData> speedupCb(ParseAttr(n, "speedupcb", ",", "1"));
        b.requiresQuest = n.attribute("requiresquest").as_int();
        std::unique_ptr<MetaData> reqBuild(ParseAttr(n, "requirestobuild", ",=", "11"));
        if (reqBuild->GetChildrenCount() != 0) {
            const MetaData* r = reqBuild->GetChild(0);
            if (r->GetChildrenCount() == 2) {
                b.requiredId = r->GetChild(0)->GetInt();
                b.requiredCount = r->GetChild(1)->GetInt();
            } else {
                std::fprintf(stderr, "ERROR: Map::LoadBuildingList() Building %u 'requirestobuild' is malformed\n", id);
            }
        }
        b.constructionTime = n.attribute("constructiontime").as_uint();
        b.level = n.attribute("level").as_uint();
        static const char* costs[] = {"lumber", "rocks", "food", "planks", "stones", "meat", "sausage", "oil"};
        for (int i = 0; i < 8; ++i) b.cost[i] = n.attribute(costs[i]).as_int();
        b.costPopulation = n.attribute("cost_population").as_uint();
        b.givePopulation = n.attribute("give_population").as_uint();
        b.speedupCb = speedupCb->GetChild(0)->GetInt();

        // Upgrades: one 0x50-byte record per "upgradelevels" entry.
        const char* none = "";
        const char* upTime = n.attribute("upgradetime").value();
        const char* upCost = n.attribute("upgradecost").value();
        if (std::strcmp(upTime, "0") == 0) upTime = none;
        if (std::strcmp(upCost, "0") == 0) upCost = none;
        std::unique_ptr<MetaData> quests(ParseAttr(n, "upgradequests", ",", "1"));
        std::unique_ptr<MetaData> reqUp(ParseAttr(n, "requirestoupgrade", ",:=", "111"));
        std::unique_ptr<MetaData> times(ParseCustomStyleData(upTime, ",", "1"));
        std::unique_ptr<MetaData> levels(ParseAttr(n, "upgradelevels", ",", "1"));
        std::unique_ptr<MetaData> upCosts(ParseCustomStyleData(upCost, "|,=", "111"));
        std::unique_ptr<MetaData> upPop(ParseAttr(n, "upgrade_population", ",", "1"));
        std::unique_ptr<MetaData> upGivePop(ParseAttr(n, "give_upgrade_population", ",", "1"));
        unsigned count = levels->GetChildrenCount();
        if (count != times->GetChildrenCount())
            std::fprintf(stderr, "ERROR: Map::LoadBuildingList() Building %u 'upgradetime' has %u values, but must have %u\n",
                         id, times->GetChildrenCount(), count);
        if (count != upCosts->GetChildrenCount())
            std::fprintf(stderr, "ERROR: Map::LoadBuildingList() Building %u 'upgradecost' has %u values, but must have %u\n",
                         id, upCosts->GetChildrenCount(), count);
        if (speedupCb->GetChildrenCount() <= count)
            std::fprintf(stderr, "ERROR: Map::LoadBuildingList() Building %u 'speedupcb' field has %u values, but must have %u\n",
                         id, speedupCb->GetChildrenCount(), count + 1);
        for (unsigned i = 0; i < count; ++i) {
            UpgradeInfo u;
            u.quest = IntAt(quests.get(), i);
            u.time = IntAt(times.get(), i);
            u.level = IntAt(levels.get(), i);
            if (i < upCosts->GetChildrenCount()) {
                const MetaData* c = upCosts->GetChild(i);
                for (unsigned k = 0; k < c->GetChildrenCount(); ++k) {
                    const MetaData* pair = c->GetChild(k);
                    if (pair->GetChildrenCount() == 2)
                        u.cost[GameState::StringToResourceType(pair->GetChild(0)->GetString())] = pair->GetChild(1)->GetInt();
                    else
                        std::fprintf(stderr, "ERROR: Map::LoadBuildingList() Building %u 'upgradecost' is malformed\n", id);
                }
            }
            u.population = IntAt(upPop.get(), i);
            u.givePopulation = IntAt(upGivePop.get(), i);
            u.speedupCb = IntAt(speedupCb.get(), i + 1);
            b.upgrades.push_back(u);
        }
        // "level:id=count" (each side a wrapped list): the building `id` (count of them) needed for that
        // upgrade level.
        for (unsigned i = 0; i < reqUp->GetChildrenCount(); ++i) {
            const MetaData* r = reqUp->GetChild(i);
            if (r->type != MetaData::kList || r->GetChildrenCount() != 2 || r->GetChild(1)->type != MetaData::kList ||
                r->GetChild(1)->GetChildrenCount() != 2) {
                std::fprintf(stderr, "ERROR: Map::LoadBuildingList() Building %u 'requirestoupgrade' item %u is malformed\n", id, i);
                continue;
            }
            unsigned lvl = (unsigned)r->GetChild(0)->GetChild(0)->GetInt();
            if (lvl == 0 || lvl >= count + 1) {
                std::fprintf(stderr, "ERROR: Map::LoadBuildingList() Building %u 'requirestoupgrade' level %u out of range\n", id, lvl);
                continue;
            }
            b.upgrades[lvl - 1].requiredId = r->GetChild(1)->GetChild(0)->GetInt();
            b.upgrades[lvl - 1].requiredCount = r->GetChild(1)->GetChild(1)->GetInt();
        }

        std::unique_ptr<MetaData> speedupRes(ParseAttr(n, "speedupresources", ",=", "11"));
        if (speedupRes->GetChildrenCount() > 1)
            std::fprintf(stderr, "ERROR: Map::LoadBuildingList() Building %u has multiple 'speedupresources', which are unsupported\n", id);
        if (speedupRes->GetChildrenCount() != 0) {
            b.speedupResource = GameState::StringToResourceType(speedupRes->GetChild(0)->GetChild(0)->GetString());
            b.speedupAmount = speedupRes->GetChild(0)->GetChild(1)->GetInt();
        }
        std::unique_ptr<MetaData> produce(ParseAttr(n, "produce_resource", "|=", "11"));
        if (produce->GetChildrenCount() > 1)
            std::fprintf(stderr, "ERROR: Map::LoadBuildingList() Building %u has multiple 'produce_resource', which are unsupported\n", id);
        if (produce->GetChildrenCount() != 0) {
            b.produceResource = GameState::StringToResourceType(produce->GetChild(0)->GetChild(0)->GetString());
            b.produceAmount = produce->GetChild(0)->GetChild(1)->GetInt();
        }
        std::unique_ptr<MetaData> respawn(ParseAttr(n, "resourcesrespawn", ",", "1"));
        std::unique_ptr<MetaData> respawnTime(ParseAttr(n, "resourcesrespawntime", ",", "1"));
        if (respawn->GetChildrenCount() != 0) {
            if (respawn->GetChildrenCount() != 5)
                std::fprintf(stderr, "ERROR: Map::LoadBuildingList() Building %u 'resourcesrespawn' has %u values, but must have 5\n",
                             id, respawn->GetChildrenCount());
            if (respawnTime->GetChildrenCount() != 0 && respawnTime->GetChildrenCount() != 5)
                std::fprintf(stderr, "ERROR: Map::LoadBuildingList() Building %u 'resourcesrespawntime' has %u values, but must have 5\n",
                             id, respawnTime->GetChildrenCount());
            b.respawn.resize(5);
            for (unsigned i = 0; i < 5; ++i) {
                b.respawn[i].amount = IntAt(respawn.get(), i);
                b.respawn[i].time = IntAt(respawnTime.get(), i);
            }
        }
        b.farmPatchDefault = n.attribute("farm_patch_default").as_uint();
        b.farmPatchCost = ParseOptional(n, "farm_patch_cost");
        b.farmPatchCost2 = ParseOptional(n, "farm_patch_cost2");
        b.farmPatchLevels = ParseOptional(n, "farm_patch_levels");
        b.tileset = n.attribute("tileset").as_int();
        b.tab = n.attribute("tab").as_int();
        b.subtab = n.attribute("subtab").as_int();
        b.buy = n.attribute("buy").as_bool();
        b.collectTime = n.attribute("collecttime").as_int();
        b.collectMoney = n.attribute("collectmoney").as_int();
        b.cantSellLast = n.attribute("cant_sell_last").as_bool();
        // "x,y|x,y..." up to 10 points each.
        auto points = [](const char* v, auto* out, unsigned& cnt) {
            for (int i = 0; i < 10; ++i) out[i].x = out[i].y = 0.f;
            cnt = 0;
            if (!v || !*v) return;
            while (cnt < 10) {
                std::sscanf(v, "%f,%f", &out[cnt].x, &out[cnt].y);
                ++cnt;
                const char* bar = std::strchr(v, '|');
                if (!bar || !bar[1]) break;
                v = bar + 1;
            }
        };
        points(n.attribute("parking_points").value(), b.parking, b.parkingCount);
        points(n.attribute("particle_points").value(), b.particles, b.particleCount);
        b.hp = n.attribute("hp").as_int();
        if (b.hp == 0) b.hp = 100;
        b.visid = n.attribute("visid").as_int();
        b.deliveryType = n.attribute("delivery_type").as_uint();
        if (b.deliveryType == 0) {   // three buildings get their delivery list by id
            if (id == 0x8e) b.deliveryType = 0x800;
            else if (id == 0x69) b.deliveryType = 0x801;
            else if (id == 0x93) b.deliveryType = 0x802;
        }
        // UNVERIFIED (milestone 3, Contracts): b.delivery = Contracts::GetContract(b.deliveryType) for
        // deliveryType != 0.
        b.cb = n.attribute("cb").as_uint();
        b.gold = n.attribute("gold").as_uint();
        b.unlockLevel = ParseOptional(n, "unlocklevel");
        int counter = 0;  // LoadBuildingList @0x122ae4 stage assignment
        for (auto f = n.first_child(); f; f = f.next_sibling()) {
            BuildingPart p;
            const char* tag = f.name();
            p.type = !std::strcmp(tag, "floor") ? 0 : !std::strcmp(tag, "centerpart") ? 1
                   : !std::strcmp(tag, "ceil") ? 2 : !std::strcmp(tag, "decors") ? 3 : 0;
            p.partImg = f.attribute("part_img").value();
            p.height = f.attribute("height").as_int();
            p.offX = f.attribute("off_x").as_int();
            p.offY = f.attribute("off_y").as_int();
            p.based = f.attribute("based").as_int();
            p.valign = f.attribute("valign").as_int();
            float fs = f.attribute("fs").as_float();
            p.frameTime = 1.0f / fs;  // as in the original (inf when fs == 0)
            if (p.type == 0) {
                if (id == 1000 || id == 0x11 || id == 0x95) {
                    if (std::strstr(p.partImg.c_str(), "stump")) { p.stage = 6; p.frameTime = 1.0f; counter = 0; }
                    else if (std::strstr(p.partImg.c_str(), "cut")) p.stage = 5;
                    else p.stage = counter++;
                } else {
                    p.stage = -1;
                }
            } else {
                p.stage = f.attribute("stg").as_int();
            }
            p.uc = f.attribute("uc").as_float();
            p.pD = f.attribute("p_d").as_uint();
            p.pCb = f.attribute("p_cb").as_uint();
            p.lvl = f.attribute("lvl").as_uint();
            p.xp = f.attribute("xp").as_uint();
            b.parts.push_back(p);
        }
        // UNVERIFIED (milestone 3, Items): Items::AddBuildingAsItem(id) when buy or byte +0x31 is set.
    }
}

}  // namespace

bool Load() {
    LoadDecorations();
    LoadBuildings();
    std::printf("GameData: %zu decorations, %zu buildings\n", g_decors.size(), g_buildings.size());
    return !g_decors.empty() && !g_buildings.empty();
}

const DecorData* GetDecoration(uint32_t id) {
    auto it = g_decors.find(id);
    return it == g_decors.end() ? nullptr : &it->second;
}

const BuildingData* GetBuilding(uint32_t id) {
    auto it = g_buildings.find(id);
    return it == g_buildings.end() ? nullptr : &it->second;
}


Render::Texture* DecorImage(const DecorData* d) {
    auto* m = const_cast<DecorData*>(d);
    if (!m->imageLoaded) {
        m->image = Resources::GetDecoration(m->img.c_str());
        m->imageLoaded = true;
        if (!m->image && !m->img.empty()) std::printf("DecorData::LoadImage() can't load %s\n", m->img.c_str());
    }
    return m->image;
}

Render::Texture* PartImage(const BuildingPart* p) {
    auto* m = const_cast<BuildingPart*>(p);
    if (!m->imageLoaded) {
        m->image = Resources::GetImage(m->partImg.c_str());
        m->imageLoaded = true;
    }
    return m->image;
}

}  // namespace GameData
