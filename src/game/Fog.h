// Fog: the cover over the unexplored part of a campaign map (libkingdom.so 5.11, Fog @0x17361c..
// 0x175ae0). The map is cut into 42-pixel blocks; each has the alpha it is fading to. A quad tree
// over the blocks draws a whole covered region with one sprite of the wrapping fog image and splits
// it down to single blocks around opened tiles; an opened block's sprite takes its corner alphas
// from its neighbours and goes when all four reach 0.
#pragma once
#include <cstdint>

namespace SaveManager { struct SaveBlock; }

namespace Fog {

extern float scale;   // 0x60ef34 (1)

void Init();                                   // @0x174118 the fog images
// @0x174e94: a fog of w tiles by h half-tiles for the tileset (2: the desert's image), all covered.
void Create(unsigned w, unsigned h, unsigned tileset);
void Deinit(bool forgetImages);                // @0x17404c
float GetBlockAlpha(int col, int row);         // @0x17361c the alpha it shows now (0 outside)
void FillZero();                               // @0x173668 every block's target: open
// @0x1736d8: the blocks whose centre lies outside the circle in the world box get the target 1 again.
void FillOne(int x0, int y0, int x1, int y1);
// @0x173888: some block under the world box is not fully covered (always when the fog is not drawn).
bool IsVisible(int x0, int y0, int x1, int y1);
void StoreFogState();                          // @0x173b24 the targets, kept over a map reload
void RestoreFogState();                        // @0x1759c8
void SaveFog();                                // @0x173bd0 chunk 0x30
void LoadFog(SaveManager::SaveBlock* block);   // @0x174770
void Update(float dt);                         // @0x173c88 the fading
void ToggleFogRender();                        // @0x1742c8
// @0x1748fc: the blocks within radius of the world point open; from `edge` of the radius (squared
// ratio) outwards they keep a rising part of the fog.
void Open(float x, float y, float radius, float edge);
void Open(int col, int row);                   // @0x174aa4 one block, fully
void OpenZero();                               // @0x174ac4 every block to its target (and the map's rim)

}  // namespace Fog
