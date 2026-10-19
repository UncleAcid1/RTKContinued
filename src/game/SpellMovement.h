// SpellMovement: aiming an area spell in the arenas. The spell's footprint (5 x 5 tiles, three
// shades) is drawn on the ground around the selected tile; a tap or a drag moves the selection
// inside the arena; the large buttons confirm (Accept) or cancel (Decline).
// Port of SpellMovement (libkingdom.so 5.11, 0x3a56ec..0x3a622c).
#pragma once

namespace Spells { struct SpellInfo; }

namespace SpellMovement {

void Init();                 // @0x3a5770 (main_Loop_Init) loads the three tile images
inline void Deinit() {}      // @0x3a56ec
inline void Update() {}      // @0x3a56f0
bool Activated();            // @0x3a56f4
// @0x3a61b0: start aiming `spell`, the selection on tile (x, y) (the target's); the combat is told.
void Activate(Spells::SpellInfo* spell, int x, int y);
void Decline();              // @0x3a5838 stop aiming
void Accept();               // @0x3a58a0 stop aiming and cast at the selected tile
// @0x3a5edc: a tap at screen (x, y), pressed or released. True: taken.
bool Click(int x, int y, bool pressed);
bool Move(int x, int y, int dx, int dy);   // @0x3a6054 dragging the selection
void GetEpicenterPos(int& x, int& y);      // @0x3a5750 the footprint's middle tile

}  // namespace SpellMovement
