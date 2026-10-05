// Static game data used by the map: decorations, buildings.
// Ports of Map::LoadDecorationList @0x13a8a8, Map::LoadBuildingList @0x121748 (only the fields the
// map/render code needs so far).
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace Render { struct Texture; }

namespace GameData {

struct DecorData {               // DecorData
    uint32_t id = 0;             // +0x00 "id"
    std::string name;            // +0x04 "name"
    int w = 0, h = 0;            // +0x08 "lockzoneX", +0x0c "lockzoneY"
    int ox = 0, oy = 0;          // +0x10 "x", +0x14 "y"
    int layer = 0;               // +0x20 "layer"
    std::string img;             // +0x50 "img"
    Render::Texture* image = nullptr;  // +0x4c (DecorData::LoadImage)
    bool imageLoaded = false;
};

struct BuildingPart {            // BuildingPart, from <floor>
    int type = 0;                // +0x00 (0 for <floor>)
    std::string partImg;         // +0x08 "part_img"
    int offX = 0, offY = 0;      // +0x0c "off_x", +0x10 "off_y"
    int height = 0;              // +0x14 "height"
    int based = 0, valign = 0;   // +0x18, +0x1c
    int stage = -1;              // +0x20 (-1: selected by order; trees/149/1000 get explicit stages)
    float frameTime = 1.f;       // +0x24 = 1 / "fs"
    Render::Texture* image = nullptr;  // +0x04 (BuildingPart::LoadImage)
    bool imageLoaded = false;
};

struct BuildingData {            // BuildingData
    uint32_t id = 0;
    std::string name;            // +0x04
    int w = 0, h = 0;            // +0x08 "buildZoneX", +0x0c "buildZoneY"
    int offsetX = 0, offsetY = 0;  // +0x10, +0x14
    int layer = 0;               // +0x20
    uint32_t buildingClass = 0;  // +0x1ac "building_class"
    std::vector<BuildingPart> parts;  // +0x1b0 linked list, in file order
};

bool Load();
const DecorData* GetDecoration(uint32_t id);     // Map::GetDecoration(unsigned) @0x130ebc
const BuildingData* GetBuilding(uint32_t id);    // Map::GetBuilding @0x11c3cc

Render::Texture* DecorImage(const DecorData* d);         // DecorData::LoadImage @0x134bb8
Render::Texture* PartImage(const BuildingPart* p);       // BuildingPart::LoadImage @0x11ee40

}  // namespace GameData
