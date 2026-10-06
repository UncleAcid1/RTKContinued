// BuildingMovement: the city's edit mode (the Tools button): move, rotate and remove buildings and
// decorations. Each edit is an action with an undo; moved objects are previews (copies) until
// Accept applies everything at once, Decline undoes it all.
// Port of BuildingMovement (libkingdom.so 5.11, 0x39c4e0..0x3a1698).
//
// Not ported: road painting (mode 3, ToggleRoadType @0x39c874, the road actions and their gold
// cost) and the warehouse (mode 4, ToggleWarehouse @0x39ca78, PresentsContainer); nothing in 5.11
// calls their toggles. Legacy mode (a single object with the placement controls) is only entered
// from the quests' help (milestone 4).
#pragma once

namespace BuildingMovement {

void Init();                 // @0x39cb7c (main_Loop_Init)
void Deinit();               // @0x39c4e0
bool Activated();            // @0x39c520
void SetLegacyMode(bool on); // @0x39c534
void Activate();             // @0x39caf0 enter the edit mode (moving)
void ToggleMovement();       // @0x39ca00
void ToggleRotation();       // @0x39e0c8
void ToggleDemolishion();    // @0x39c988
void UndoAction(bool quiet); // @0x39dacc the last action
bool Accept();               // @0x39e2d4 apply every action (false: some are blocked, shown with arrows)
void Decline();              // @0x39dfa8 undo everything and leave
void Update(float dt);       // @0x39d194
// @0x39f0b4: a tap at screen (x, y), pressed or released; moving: the map was being dragged.
bool Click(int x, int y, bool pressed, bool moving);
bool Move(int x, int y, int dx, int dy);   // @0x39eaa4 dragging the grabbed preview
void RemoveOk();             // @0x3a1698 the removal confirmed

}  // namespace BuildingMovement
