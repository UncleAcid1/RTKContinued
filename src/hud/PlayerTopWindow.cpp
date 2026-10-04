// PlayerTopWindow (placeholder while porting)
#include "hud/HUD.h"
#include "gui/WindowManager.h"
namespace PlayerTopWindow {
namespace { WindowManager::FunctionalWindow* g_queue = nullptr; }
WindowManager::FunctionalWindow* Queue() {
    if (!g_queue) g_queue = new WindowManager::FunctionalWindow("PlayerTopWindow", {});
    return g_queue;
}
void Init() {}
void Show() {}
void Hide() {}
void SetZ(float) {}
void Update(float) {}
bool IsVisible() { return false; }
}
