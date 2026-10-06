// BuildingPlacement: placing a bought building or decoration on the city map. The bought object
// becomes a preview that follows the player's drag (snapping to tiles, red/green footprint), with
// the PlaceBuildingHoverWindow buttons: confirm (pay and build), rotate, cancel.
// Port of BuildingPlacement (libkingdom.so 5.11, 0x3a19bc..0x3a4258).
#pragma once

namespace Map { struct Building; struct Decor; }
namespace Render { struct Sprite; }

namespace BuildingPlacement {

void Init();                         // @0x3a1e28 (main_Loop_Init)
void Deinit();                       // @0x3a19c0
bool Activated();                    // @0x3a1a00
bool PlacementControlsVisible();     // @0x3a1a14
bool IsInDragProcess();              // @0x3a1a38
bool IsBuildingDragged();            // @0x3a1a54
void Update(float dt);               // @0x3a1b54
// @0x3a2924: a tap (pressed: the press) at screen (x, y). A press on the preview starts a drag; a
// release ends it or moves the preview to the tapped tile. moving: the map is being dragged.
bool Click(int x, int y, bool pressed, bool moving);
bool Move(int x, int y, int dx, int dy);   // @0x3a25fc dragging the preview
void ToggleRotation();               // @0x3a2594
void Accept();                       // @0x3a37c4 pay, build, place
void Decline();                      // @0x3a1fe4 back to the shop
// @0x3a3138: start placing a copy of `b` (the shop's building), at the screen centre or near it.
// sprite: the shop icon flying to the preview (UNVERIFIED, 3e.3); fromPresents: a present, free.
void BuildingBought(Map::Building* b, Render::Sprite* sprite, bool fromPresents);
// @0x3a2cf0: the same for a decoration (taken over, not copied).
void DecorBought(Map::Decor* d, Render::Sprite* sprite, bool fromPresents);
void UpdateCost(int extraGold, bool show);   // @0x3a35b0 (UNVERIFIED, 3e.3: the cost line)

}  // namespace BuildingPlacement
