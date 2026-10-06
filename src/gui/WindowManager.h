// WindowManager: the queue of top-level game windows (HUD panels, dialogs) and their depth ranges.
// Port of WindowManager (libkingdom.so 5.11): WindowQueue (vtable +0x08 Init ... +0x4c OnOuterClick),
// FunctionalWindow (a WindowQueue whose methods are plain function pointers, one static instance
// per game window), MoveWindowOnTop @0x36f270, InitWindows @0x36f408, ProcessClick @0x36ed98.
#pragma once
#include <functional>
#include <string>
#include <vector>

namespace GUI { class Window; }

namespace WindowManager {

class WindowQueue {
public:
    WindowQueue();            // @0x36dec0: appended to the global list (construction order)
    virtual ~WindowQueue();   // @0x36e000
    virtual void Init() {}                                   // +0x08
    virtual void Deinit() {}                                 // +0x0c
    virtual void SetZ(float) {}                              // +0x10
    virtual bool Click(int, int, bool) { return false; }     // +0x14
    virtual void Move(int, int) {}                           // +0x18
    virtual void Show() {}                                   // +0x1c
    virtual void Hide() {}                                   // +0x20
    virtual void Update(float) {}                            // +0x24
    virtual void Wheel(bool) {}                              // +0x28
    virtual bool Back() { return false; }                    // +0x2c
    virtual bool IsVisible() { return true; }                // +0x30 @0x184c6c
    virtual float ZRange() { return 0.03f; }                 // +0x34 @0x184c74

    void MoveWindowOnTop(bool reassignZ);   // @0x36f270
    void MoveWindowDown(bool reassignZ);    // @0x36f0c4

    std::string name;                 // +0x04
    bool usesZRange = true;           // +0x08
    WindowQueue* next = nullptr;      // +0x0c towards the bottom (ProcessClick walks head -> next)
    WindowQueue* prev = nullptr;      // +0x10 towards the top (ProcessUpdate/Move walk tail -> prev)
    bool pendingDestroy = false;      // +0x14
    bool shown = false;               // +0x15 set by the windows' Show/Hide
};

// A window implemented as a set of static functions (FunctionalWindow @0x36e468).
class FunctionalWindow : public WindowQueue {
public:
    struct Functions {
        std::function<void()> init, deinit;            // +0x18 +0x1c
        std::function<void(float)> setZ;               // +0x20
        std::function<bool(int, int, bool)> click;     // +0x24
        std::function<void(int, int)> move;            // +0x28
        std::function<void()> show, hide;              // +0x2c +0x30
        std::function<void(float)> update;             // +0x34
        std::function<void(bool)> wheel;               // +0x38
        std::function<bool()> back;                    // +0x3c
        std::function<bool()> isVisible;               // +0x40
        std::function<float()> zRange;                 // +0x44
    };
    FunctionalWindow(const char* n, Functions f);
    void Init() override { if (fn.init) fn.init(); }
    void Deinit() override { if (fn.deinit) fn.deinit(); }
    void SetZ(float z) override { if (fn.setZ) fn.setZ(z); }
    bool Click(int x, int y, bool pressed) override;     // @0x36e694
    void Move(int x, int y) override { if (fn.move) fn.move(x, y); }
    void Show() override { if (fn.show) fn.show(); }
    void Hide() override { if (fn.hide) fn.hide(); }
    void Update(float dt) override { if (fn.update) fn.update(dt); }
    void Wheel(bool up) override { if (fn.wheel) fn.wheel(up); }
    bool Back() override { return fn.back ? fn.back() : false; }
    bool IsVisible() override { return fn.isVisible ? fn.isVisible() : true; }     // @0x36e77c
    float ZRange() override { return fn.zRange ? fn.zRange() : 0.03f; }          // @0x36e798
    void RegisterTopWindow(GUI::Window* w);              // @0x36eec0
    bool OnOuterClick(int x, int y);                     // @0x36ecc0

    Functions fn;
    std::vector<GUI::Window*> topWindows;   // +0x48
    GUI::Window* topWindow = nullptr;       // +0x68 (its "clickArea" child if it has one)
    bool hideOnOuterClick = false;          // +0x6c
};

// The bottom of the queue (main_Loop_Init creates it last as "desktop_window"): it takes every
// click nothing above took, and ProcessClick returning it means the click belongs to the map.
class DesktopWindow : public WindowQueue {
public:
    DesktopWindow() { name = "desktop_window"; usesZRange = false; }
    bool Click(int, int, bool) override { return true; }   // @0x184c8c
};
extern DesktopWindow* g_desktopWindow;   // 0x612380

WindowQueue* Head();                          // the top of the queue (0x630b1c)
void InitWindows();                           // @0x36f408: Init() of every queued window, in order
WindowQueue* ProcessClick(int x, int y, bool pressed);   // @0x36ed98 (top first)
void ProcessUpdate(float dt);                 // @0x36e2a8
void ProcessMove(int x, int y);               // @0x36e218
void DestroyPendingWindows();                 // @0x36e85c deletes the windows marked pendingDestroy
void SetMousePosition(int x, int y);          // @0x36e8a4
int GetShownWindowCount();                    // @0x36e7b8
WindowQueue* ProcessBack();                   // @0x36f008 (the first window, top down, whose Back() takes it)
void WindowShow(bool quiet);                  // @0x36ec60
void WindowHide(bool quiet);                  // @0x36ebec
float GetTopWindowRange();                    // @0x36e8f4
// @0x36eb30: show `fn` once nothing is in the way (no window shown, no placement, ...), after
// `delay` seconds; a function already waiting is not added twice.
void EnqueueWindow(void (*fn)(), float delay);
// @0x36ea6c: the first waiting window, when nothing is in the way: its delay counts down, then it
// is removed and shown.
void Update(float dt);
void ClearQueue();                            // @0x36ea14
inline float ReturnSmallZRange() { return 0.01f; }    // @0x36e950
inline float ReturnMediumZRange() { return 0.02f; }   // @0x36e95c
inline float ReturnBigZRange() { return 0.04f; }      // @0x36e968
inline float ReturnLargeZRange() { return 0.06f; }    // @0x36e974

}  // namespace WindowManager
