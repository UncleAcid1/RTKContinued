// Map: grid, patches (land areas), decorations and buildings, and their sprites.
// Port of the Map namespace (libkingdom.so 5.11): load path Map::Load @0x1c3518 -> LoadPlayer @0x1c0cd8
// -> Patch::Load @0x1e3870 (LoadDecors @0x1e271c, LoadBuidings @0x1e2bdc), CreateRandomDecors, UpdateAreaBorders.
// Gameplay state (timers, workers, AI, quests) is not ported yet; only what is needed to show the map.
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Render { struct Sprite; }
namespace GameData { struct DecorData; struct BuildingData; }

namespace Map {

struct Patch;

struct Decor {
    Patch* patch = nullptr;            // +0x04
    uint8_t x = 0, y = 0;              // +0x0c, +0x0d
    uint32_t id = 0;                   // +0x18
    bool mirrored = false;             // +0x1c (map flag bit 0)
    bool visible = true;               // +0x1d
    bool fake = false;                 // random decoration (Patch::AddRandomDecors), Decor::IsFake
    const GameData::DecorData* data = nullptr;  // +0x3c
    Render::Sprite* sprite = nullptr;  // +0x88
    bool removed = false;
};

struct Building {
    Patch* patch = nullptr;
    uint8_t x = 0, y = 0;              // +0x0c, +0x0d
    uint32_t id = 0;                   // +0x18
    bool mirrored = false;             // +0x1c
    int level = 0;                     // +0x50
    const GameData::BuildingData* data = nullptr;
    float baseX = 0, baseY = 0;        // +0x24, +0x30 (FindBaseCoordinates)
    float minX = 0, maxX = 0, minY = 0, maxY = 0;  // +0x28, +0x2c, +0x38, +0x34 (sprite bounds)
    std::vector<Render::Sprite*> sprites;  // +0xd4 chain
    Render::Sprite* ring = nullptr;    // +0x174
};

struct Patch {
    uint32_t areaId = 0;               // +0xe4
    bool owned = false;                // +0xe8
    bool bordered = false;             // +0xe9 (recomputed in UpdateAreaBorders)
    int x = 0, y = 0, w = 0, h = 0;    // +0x8c..+0x98 (from AreaInfo)
    bool buyable = false;              // +0xa8
    std::vector<std::unique_ptr<Decor>> decors;      // +0xac
    std::vector<std::unique_ptr<Building>> buildings;
    std::string mask;                  // chunk 7 ('0'/'1' per tile, x-major)
};

bool Load(uint32_t mapId, long playerSeed);  // reads "maps/map_<id>.bin"
void Free();

int GetGridWidth();
int GetGridHeight();
uint32_t GetMapID();
int GetTileset();

void TileCoordinatesToWorld(int& x, int& y);           // @0x1b632c
float GetSpriteZ(float a, float b, int c);             // @0x1b6814
// Extent (tiles) of the unowned patches that border owned land; the whole grid if there are none.
void GetAreaBorders(int& minX, int& minY, int& maxX, int& maxY);   // @0x1b6a20
// Camera paths (tutorial and quest camera moves, Map::AddCameraPoint) are not ported yet, so the
// list is always empty: InterruptCamera does nothing and IsCameraMoving is false.
void InterruptCamera();                                // @0x1bf6d4
bool IsCameraMoving();                                 // @0x1b82c4

}  // namespace Map
