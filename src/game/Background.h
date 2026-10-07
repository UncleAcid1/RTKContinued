// Background: tiled ground and the horizon strip above the map.
// Port of Background::Init @0x10c308 (ground textures) and Background::CreateLand @0x10b9ec.
#pragma once

namespace Background {
void CreateLand(unsigned tileset);
void CreateFarm(int farm);      // @0x10b4fc the farm view's ground and frame (3f)
void RemoveFarm();              // @0x10b47c
void GetFarmBounds(float& left, float& top, float& right, float& bottom);   // @0x10b1f0
void SetBrokenFence(bool broken);   // @0x10b3a0 the animal farm's fence drawn under (broken) or over the animals
}
