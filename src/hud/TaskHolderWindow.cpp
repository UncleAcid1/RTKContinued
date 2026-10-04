// TaskHolderWindow (placeholder while porting)
#include "hud/HUD.h"
#include "gui/WindowManager.h"
namespace TaskHolderWindow {
namespace { WindowManager::FunctionalWindow* g_queue = nullptr; }
WindowManager::FunctionalWindow* Queue() {
    if (!g_queue) g_queue = new WindowManager::FunctionalWindow("TaskHolderWindow", {});
    return g_queue;
}
void Init() {}
void Show() {}
void Hide() {}
void SetZ(float) {}
void Update(float) {}
bool IsVisible() { return false; }
}
