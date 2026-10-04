// The city HUD: HUDWindow (the driver) and its panels. Each panel is a set of static functions behind
// a WindowManager::FunctionalWindow, as on the original.
#pragma once

namespace WindowManager { class FunctionalWindow; }

namespace HUDWindow {
void Init();                  // @0x2c9a7c
void Show();                  // @0x2c909c
void SetZ(float z);           // @0x2c9158
float GetZ();                 // @0x2c6548
void Update(float dt);        // @0x2c81c8
int UpdateNumberToTarget(int current, int target);   // @0x2c6560 (animated counters)
WindowManager::FunctionalWindow* Queue();
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
