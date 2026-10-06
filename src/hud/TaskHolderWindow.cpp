// TaskHolderWindow: the quest list down the left side of the city (up to 32 task buttons in a
// scroller, "new task" banners, the quick task description). Port of TaskHolderWindow
// (libkingdom.so 5.11), 0x354364..0x359d68.
//
// Every window here starts hidden and is filled from the quests (UpdateTasks @0x356048), so the
// layout (Init @0x355384: New_task_huge_container / New_task_container / Slider_mission_scroll_bg /
// Scroll_bar_tiny_container, Shared::TaskWindow, Shared::ContentScroller) is ported together with
// the quest system (milestone 4). Until then the window takes its place in the queue and nothing
// of it is visible, as on a city without quests.
#include "game/GameState.h"
#include "gui/WindowManager.h"
#include "hud/HUD.h"

namespace TaskHolderWindow {
namespace {
WindowManager::FunctionalWindow* g_queue = nullptr;
}

// Static FunctionalWindow("TaskHolderWindow", Init, Deinit, Click, SetZ, Hide, Move) @0x354690
WindowManager::FunctionalWindow* Queue() {
    if (!g_queue) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.setZ = SetZ;
        f.hide = Hide;
        f.click = [](int, int, bool) { return false; };   // Click @0x354d8c: the task buttons
        f.move = [](int, int) {};                         // Move @0x354d6c: ContentScroller::Move
        g_queue = new WindowManager::FunctionalWindow("TaskHolderWindow", std::move(f));
    }
    return g_queue;
}

// @0x355384 (milestone 4, see above)
void Init() {}

// @0x354c04
void Show() {
    if (!GameState::IsPlayerCity()) return;
    Queue()->MoveWindowOnTop(true);
}

// @0x359668: outside the player's city the tasks are refreshed (UpdateTasks hides them).
void Hide() {
    if (GameState::IsPlayerCity()) return;
    // UpdateTasks(): milestone 4
}

// @0x354c2c: places the task windows at HUD depth (milestone 4).
void SetZ(float) {}

// @0x359680: task animations, arrows and the quick description (milestone 4).
void Update(float) {}

bool IsVisible() { return true; }   // FunctionalWindow default (no IsVisible function)

namespace {
bool g_taskVisibility = true;   // 0x60f400
}

// @0x359648
void SetTaskVisibility(bool on) {
    bool was = g_taskVisibility;
    g_taskVisibility = on;
    if (was == on) return;
    // UpdateTasks(): milestone 4
}

}  // namespace TaskHolderWindow
