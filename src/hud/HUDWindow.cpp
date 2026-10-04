// HUDWindow: the driver of the city HUD. Port of HUDWindow (libkingdom.so 5.11), 0x2c6498..0x2c9e00.
#include "hud/HUD.h"

#include <cstdlib>

#include "game/GameState.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"

namespace HUDWindow {
namespace {
WindowManager::FunctionalWindow* g_queue = nullptr;
float g_z = 0.f;   // @0x2c6554
}

WindowManager::FunctionalWindow* Queue() {   // static FunctionalWindow "HUDWindow" (_INIT_ 0x2c6720)
    if (!g_queue) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.setZ = SetZ;
        f.hide = [] {};                                   // HUDWindow::Hide @0x2c655c (empty)
        f.click = [](int, int, bool) { return false; };   // HUDWindow::Click @0x2c6498: milestone 2d
        g_queue = new WindowManager::FunctionalWindow("HUDWindow", std::move(f));
    }
    return g_queue;
}

// @0x2c9a7c. UNVERIFIED: the HUD's own popups (player/task locators, farm timer, map name) are
// hidden on a fresh city and depend on entities and quests; they are ported with those systems.
void Init() {}

float GetZ() { return g_z; }

// @0x2c909c
void Show() {
    Queue()->MoveWindowOnTop(true);
    int loc = GameState::GetCurrentLocation();
    if (loc != 1) BattleBarWindow::Show();
    if (GameState::GetCurrentLocation() != 1) BeltBarWindow::Show();
    PlayerTopWindow::Show();
    if (GameState::GetCurrentLocation() != 3) TopCityWindow::Show();
    CastleTopWindow::Show();
    // BottomBattleWindow (location 2) belongs to campaign maps (milestone 4).
    if (GameState::GetCurrentLocation() == 0) BottomCityWindow::Show();
    else BottomCityWindow::Hide();
    if (GameState::IsPlayerCity()) TaskHolderWindow::Show();
    else TaskHolderWindow::Hide();
}

// @0x2c9158: the panels take the HUD depth (and offsets of it) in this order.
void SetZ(float z) {
    g_z = z;
    // (the HUD's own popups and the frame border are positioned here too: see Init)
    PlayerTopWindow::SetZ(z);
    CastleTopWindow::SetZ(z);
    BeltBarWindow::SetZ(z);
    BattleBarWindow::SetZ(z);
    // BottomBattleWindow::SetZ (campaign maps)
    TaskHolderWindow::SetZ(z);
    BottomCityWindow::SetZ(z);
    TopCityWindow::SetZ(z);
}

// @0x2c81c8 UNVERIFIED: locators, boss/mission timers, task arrows and screen darkening need
// entities, combat and quests (milestones 3-4).
void Update(float) {}

// @0x2c6560: step size grows with the distance to the target.
int UpdateNumberToTarget(int cur, int target) {
    if (target < cur) {
        int d = std::abs(target - cur);
        if (10000 < d) return cur - (int)((float)d * 0.2f);
        if (2000 < d) return cur - 0x3cb;
        if (500 < d) return cur - 0xb3;
        if (d < 0x51) return d < 0xb ? cur - 1 : cur - 7;
        return cur - 0x2b;
    }
    if (target <= cur) return cur;
    int d = std::abs(target - cur);
    if (10000 < d) return cur + (int)((float)d * 0.2f);
    if (2000 < d) return cur + 0x3cb;
    if (d < 0x1f5) {
        if (d < 0x51) return d < 0xb ? cur + 1 : cur + 7;
        return cur + 0x2b;
    }
    return cur + 0xb3;
}

}  // namespace HUDWindow
