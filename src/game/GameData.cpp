#include "game/GameData.h"

#include <pugixml.hpp>

#include <cstdio>
#include <cstring>

#include "engine/FileManager.h"
#include "engine/Resources.h"

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

void LoadBuildings() {
    pugi::xml_document doc;
    if (!LoadXml("../resource/res_files/1Original/buildings.xml", doc)) return;
    for (auto n : doc.child("BuildingsOptionsDefault").children("Building")) {
        BuildingData b;
        b.id = n.attribute("id").as_uint();
        b.name = n.attribute("name").value();
        b.w = n.attribute("buildZoneX").as_int();
        b.h = n.attribute("buildZoneY").as_int();
        b.offsetX = n.attribute("offsetX").as_int();
        b.offsetY = n.attribute("offsetY").as_int();
        b.layer = n.attribute("layer").as_int();
        b.buildingClass = n.attribute("building_class").as_uint();
        int counter = 0;  // LoadBuildingList @0x122ae4 stage assignment
        for (auto f : n.children("floor")) {
            BuildingPart p;
            p.type = 0;  // "floor"
            p.partImg = f.attribute("part_img").value();
            p.height = f.attribute("height").as_int();
            p.offX = f.attribute("off_x").as_int();
            p.offY = f.attribute("off_y").as_int();
            p.based = f.attribute("based").as_int();
            p.valign = f.attribute("valign").as_int();
            float fs = f.attribute("fs").as_float();
            p.frameTime = 1.0f / fs;  // as in the original (inf when fs == 0)
            if (b.id == 1000 || b.id == 0x11 || b.id == 0x95) {
                if (std::strstr(p.partImg.c_str(), "stump")) { p.stage = 6; p.frameTime = 1.0f; counter = 0; }
                else if (std::strstr(p.partImg.c_str(), "cut")) p.stage = 5;
                else p.stage = counter++;
            } else {
                p.stage = -1;
            }
            b.parts.push_back(p);
        }
        g_buildings[b.id] = b;
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
