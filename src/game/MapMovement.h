// MapMovement: dragging the map with the mouse/finger, with inertia after release.
// Port of MapMovement (libkingdom.so 5.11) 0x3a51a8..0x3a56ec; state block at 0x630c28.
//
// A press on the map gives it the focus; once the pointer has travelled more than 20 pixels the drag
// is active and every move shifts Render::offsetX/offsetY by the pointer delta. On release the mouse
// speed (GUI::GetMouseSpeed, clamped to +-2000 px/s) becomes a velocity that Update decelerates at
// 6000 px/s^2, applying whole pixels only. The right mouse button only sets the focus.
#pragma once

namespace MapMovement {

void Init();                                     // @0x3a51a8
void Deinit();                                   // @0x3a51ec
void RemoveFocus();                              // @0x3a51f0
void StopDrag();                                 // @0x3a5208
bool HasFocus();                                 // @0x3a5220
bool IsActive();                                 // @0x3a5234 (dragging)
bool Move(int x, int y, int dx, int dy);         // @0x3a5248
bool Click(int x, int y, bool pressed, bool rightButton);   // @0x3a53a4
void Update(float dt);                           // @0x3a54d4

}  // namespace MapMovement
