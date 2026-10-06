// The city HUD: HUDWindow (the driver) and its panels. Each panel is a set of static functions behind
// a WindowManager::FunctionalWindow, as on the original.
#pragma once

namespace WindowManager { class FunctionalWindow; }

namespace HUDWindow {
void Init();                  // @0x2c9a7c
void Show();                  // @0x2c909c
void SetZ(float z);           // @0x2c9158
float GetZ();                 // @0x2c6548
void UpdateFrameBorder();     // @0x2c76e0
void Update(float dt);        // @0x2c81c8
int UpdateNumberToTarget(int current, int target);   // @0x2c6560 (animated counters)
inline int GetDeltaTimeMultiplier() { return 1; }    // @0x2c6664
WindowManager::FunctionalWindow* Queue();
// @0x2c6948: a line of text under the top bar (the second map-name popup), or none.
void SetInfoText(const char32_t* text);
// @0x2c8d64: which bottom bar shows. 0: the city's (when no shop or world map is open), 3: the
// farm's, anything else: none.
void SetBottomType(int type);
}

// Panels: Init/Show/Hide/SetZ/Update/IsVisible as on the original.
#define RTK_HUD_PANEL(Name)                                       \
    namespace Name {                                              \
    void Init(); void Show(); void Hide(); void SetZ(float z);    \
    void Update(float dt); bool IsVisible();                      \
    WindowManager::FunctionalWindow* Queue();                     \
    }
RTK_HUD_PANEL(TopCityWindow)
RTK_HUD_PANEL(PlayerTopWindow)
RTK_HUD_PANEL(CastleTopWindow)
RTK_HUD_PANEL(BeltBarWindow)
RTK_HUD_PANEL(BattleBarWindow)
RTK_HUD_PANEL(BottomCityWindow)
RTK_HUD_PANEL(TaskHolderWindow)
#undef RTK_HUD_PANEL

namespace TopCityWindow {
bool Click(int x, int y, bool pressed);   // @0x365908 (NotEnoughWindow passes its clicks here first)
}

namespace TaskHolderWindow {
void SetTaskVisibility(bool on);   // @0x359648 (hidden while buildings are edited)
}

namespace CastleTopWindow {
inline void HideTools() {}         // @0x2769cc (empty in 5.11)
}

namespace BottomCityWindow {
void ShowMainButton();        // @0x25e95c
void HideMainButton();        // @0x25e860
void ShowInstruments();       // @0x25e68c
void HideInstruments();       // @0x25e564
void UpdateContents();        // @0x25df84
// Called by BuildingMovement while a building is being edited.
void EnableUndoButton(bool on);     // @0x25e514
void EnableRotateButton(bool on);   // @0x25e528
void EnableCancelButton(bool on);   // @0x25e53c
void EnableOkButton(bool on);       // @0x25e550
}
