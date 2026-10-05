#include "gui/WindowManager.h"

#include <cstdio>

#include "engine/Render.h"
#include "gui/GUI.h"

namespace WindowManager {
namespace {

// The queue: head = top-most window (smallest depth, drawn last), linked through next; new
// windows are appended at the tail.
WindowQueue* g_head = nullptr;
WindowQueue* g_tail = nullptr;
int g_shown = 0;              // WindowShow/WindowHide counter
bool g_initialising = false;
int g_mouseX = 0, g_mouseY = 0;   // SetMousePosition

}  // namespace

WindowQueue::WindowQueue() {
    if (!g_head) {
        g_head = g_tail = this;
    } else {
        prev = g_tail;
        g_tail->next = this;
        g_tail = this;
    }
}

WindowQueue::~WindowQueue() {
    if (next) next->prev = prev;
    if (prev) prev->next = next;
    if (this == g_tail) g_tail = prev;
    if (this == g_head) g_head = next;
    next = prev = nullptr;
}

// @0x36f270: unlink and insert at the head; then hand out depths from the top down. Each visible
// window that uses a z range takes ZRange(), others 0.0012.
void WindowQueue::MoveWindowOnTop(bool reassignZ) {
    if (this != g_head) {
        if (this == g_tail) {
            g_tail = prev;
            g_tail->next = nullptr;
        }
        if (prev) prev->next = next;
        if (next) next->prev = prev;
        next = g_head;
        prev = nullptr;
        g_head->prev = this;
        g_head = this;
    }
    if (!reassignZ) return;
    float z = 0.f;
    for (WindowQueue* w = this; w; w = w->next) {
        float range = (w->usesZRange && w->IsVisible()) ? w->ZRange() : 0.0012f;
        w->SetZ(range + z);
        z += (w->usesZRange && w->IsVisible()) ? w->ZRange() : 0.0012f;
    }
    Render::SortRenderLayer(Render::kLayerGUI, 1);
}

// @0x36f0c4: move to the tail, then reassign depths from the head as in MoveWindowOnTop.
void WindowQueue::MoveWindowDown(bool reassignZ) {
    if (this != g_tail) {
        if (this == g_head) {
            g_head = next;
            g_head->prev = nullptr;
        }
        if (prev) prev->next = next;
        if (next) next->prev = prev;
        prev = g_tail;
        next = nullptr;
        g_tail->next = this;
        g_tail = this;
    }
    if (!reassignZ) return;
    float z = 0.f;
    for (WindowQueue* w = g_head; w; w = w->next) {
        float range = (w->usesZRange && w->IsVisible()) ? w->ZRange() : 0.0012f;
        w->SetZ(range + z);
        z += range;
    }
    Render::SortRenderLayer(Render::kLayerGUI, 1);
}

DesktopWindow* g_desktopWindow = nullptr;

FunctionalWindow::FunctionalWindow(const char* n, Functions f) : fn(std::move(f)) { name = n; }

// @0x36e694: a handled release click counts as an outer click for the other windows.
bool FunctionalWindow::Click(int x, int y, bool pressed) {
    bool r = fn.click ? fn.click(x, y, pressed) : false;
    if (r && !pressed) OnOuterClick(x, y);
    return r;
}

// @0x36eec0
void FunctionalWindow::RegisterTopWindow(GUI::Window* w) {
    for (GUI::Window* t : topWindows)
        if (t == w) return;
    topWindows.push_back(w);
    topWindow = w;
    if (GUI::Window* c = w->GetChild("clickArea")) topWindow = c;
}

// @0x36ecc0: a release outside the top window (with a margin) closes it through Back().
bool FunctionalWindow::OnOuterClick(int x, int y) {
    if (GUI::IsAnyAnimationActive() || !GUI::CanInteractWith(nullptr)) return false;
    if (!hideOnOuterClick || !topWindow || this != g_head) return false;
    const GUI::Window* t = topWindow;
    if (t->root) { y -= t->root->y; x -= t->root->x; }
    if (x < t->x - 0x32 || t->x + t->w + 0x28 < x || y < t->y - 0x3c || t->y + t->h + 0x1e < y) {
        Back();
        return true;
    }
    return false;
}

// @0x36f008. UNVERIFIED (milestone 3): Map::GetCurrentFarm is null until farms are ported, so the
// farm condition always passes.
WindowQueue* ProcessBack() {
    std::puts("checking processback");
    if (GUI::IsAnyAnimationActive()) return nullptr;
    for (WindowQueue* w = g_head; w; w = w->next) {
        if (w->Back()) {
            std::printf("back caught by %s\n", w->name.c_str());
            return w;
        }
    }
    return nullptr;
}

WindowQueue* Head() { return g_head; }

void InitWindows() {   // @0x36f408 (the progress bar it updates every 8 windows is not ported)
    g_initialising = true;
    for (WindowQueue* w = g_head; w; w = w->next) w->Init();
    g_initialising = false;
}

// @0x36ed98: top first; nothing takes clicks while a blocking GUI animation runs.
WindowQueue* ProcessClick(int x, int y, bool pressed) {
    if (GUI::IsAnyAnimationActive()) return nullptr;
    WindowQueue* w = g_head;
    while (w && !w->Click(x, y, pressed)) w = w->next;
    return w;
}

void SetMousePosition(int x, int y) {   // @0x36e8a4
    g_mouseX = x;
    g_mouseY = y;
}

// @0x36e2a8 / @0x36e218: bottom first (from the tail through prev).
void ProcessUpdate(float dt) {
    for (WindowQueue* w = g_tail; w; w = w->prev) w->Update(dt);
}

void ProcessMove(int x, int y) {
    for (WindowQueue* w = g_tail; w; w = w->prev) w->Move(x, y);
}

int GetShownWindowCount() { return g_shown; }

void WindowShow(bool quiet) {   // @0x36ec60 (BuildingHovers arrow handling: milestone 3)
    ++g_shown;
    if (!quiet) std::printf("WindowManager: window shown (%d)\n", g_shown);
}

void WindowHide(bool quiet) {   // @0x36ebec
    if (g_shown == 0) std::printf("WindowManager: hide without show\n");
    else --g_shown;
    if (!quiet) std::printf("WindowManager: window hidden (%d)\n", g_shown);
}

float GetTopWindowRange() {   // @0x36e8f4
    if (g_head && g_head->usesZRange && g_head->IsVisible()) return g_head->ZRange();
    return 0.001f;
}

}  // namespace WindowManager
