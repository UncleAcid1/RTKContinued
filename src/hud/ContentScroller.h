// Shared::ContentScroller: drags, flings and snaps a strip of windows inside a viewport (the
// resource bar, the quest list, shop pages ...). Port of Shared::ContentScroller (libkingdom.so 5.11),
// 0x340b10..0x3420a0 and Update @0x346728.
//
// The content is itemCount items in lines of perLine, itemSize pixels per line, with padStart /
// padEnd pixels before and after. "offset" is how far the windows have been moved; with recycling
// (noRecycle false) the windows of a line that leaves the view are reused for the next one, so
// firstIndex counts the lines scrolled past and offset stays within one line.
#pragma once

#include <functional>
#include <vector>

#include "gui/GUI.h"

namespace Shared {

class ContentScroller {
public:
    // A rectangle of content (0x20 bytes) and the typed window shown in it while it is in view.
    struct VirtualRect {
        unsigned type;                      // +0x00
        int x, y, w, h;                     // +0x04..+0x10 in viewport coordinates
        unsigned data;                      // +0x14
        GUI::Window* window;                // +0x18
        GUI::Window* prevWindow;            // +0x1c
    };
    struct TypedWindow { unsigned type; GUI::Window* window; bool used; };   // 0xc bytes

    std::vector<GUI::Window*> windows;      // +0x04 moved along with the content
    std::vector<GUI::ClipRect*> rects;      // +0x14 rectangles moved along with the content
    std::vector<VirtualRect> virtualRects;  // +0x24
    std::vector<TypedWindow> typedWindows;  // +0x50
    std::vector<int> snapPoints;            // +0x68 offsets the content settles on
    GUI::Window* scrollbar = nullptr;       // +0x7c
    GUI::Window* track = nullptr;           // +0x80 the scrollbar's track (else the viewport)
    GUI::ClipRect viewport = {0, 0, 0, 0};  // +0x84
    bool horizontal = false;                // +0x94
    unsigned itemCount = 0;                 // +0x98
    int unkA0 = 1;                          // +0xa0
    unsigned firstIndex = 0;                // +0xa4
    int itemSize = 1;                       // +0xac line size in pixels
    unsigned perLine = 1;                   // +0xb0
    int offset = 0;                         // +0xb4
    bool pressed = false;                   // +0xbc
    bool dragging = false;                  // +0xbd
    int lastPos = 0;                        // +0xc0
    int padStart = 0, padEnd = 0;           // +0xc4 +0xc8
    int barMarginStart = 0, barMarginEnd = 0;   // +0xcc +0xd0
    bool noRecycle = false;                 // +0xd4
    bool freeScroll = true;                 // +0xd5 false: never scroll past the ends
    float velocity = 0.f;                   // +0xd8 fling speed
    float accum = 0.f;                      // +0xdc movement not applied yet
    float bounce = 0.f;                     // +0xe0 programmed scroll distance
    bool fadeBar = false;                   // +0xe4 the scrollbar fades out when still
    float fadeTimer = 0.f;                  // +0xe8
    int minBarSize = 0x18;                  // +0xec
    float barRatio = 0.f;                   // +0xf0
    // Callbacks: each a function pointer and a callback object in the original (+0xf4/+0x100,
    // +0xf8/+0x104, +0xfc/+0x108), always called in that order.
    std::function<void()> onScroll, onNext, onPrev;

    ContentScroller() { Init(); }
    void Init();                                                  // @0x340b10
    bool Click(int x, int y, bool pressed, GUI::Window* win);     // @0x342164
    void Move(int x, int y);                                      // @0x340d04
    bool Update(float dt);                                        // @0x346728
    bool UpdateScroll();                                          // @0x340ec0
    int HandleMove(int d);                                        // @0x341410
    bool CanMove();                                               // @0x341a1c
    bool CanMoveLeft();                                           // @0x341a7c
    bool CanMoveRight();                                          // @0x341ae8
    void ScrollIntoView(unsigned index);                          // @0x341b64
    void ScrollToTop();                                           // @0x341d00
    void ClearVirtualRects() { virtualRects.clear(); }            // @0x341d70
    void ClearTypedWindows() { typedWindows.clear(); }            // @0x341d7c
    int GetWindowIndex(GUI::Window* w);                           // @0x341d88
    int GetWindowData(GUI::Window* w);                            // @0x341e28
    void AddTypedWindow(unsigned type, GUI::Window* w);           // @0x341ffc
    void AddVirtualrect(unsigned type, int x, int y, int w, int h, unsigned data);   // @0x3420a0

private:
    unsigned Lines(unsigned n) const { return (n - 1 + perLine) / perLine; }
    int ViewLen() const { return horizontal ? viewport.right - viewport.left : viewport.bottom - viewport.top; }
    int ContentLen() const { return itemSize * (int)Lines(itemCount) + padEnd; }
    int Scrolled() const { return itemSize * (int)Lines(firstIndex) - offset; }
    void Shift(int d);   // moves the windows and rectangles along the axis
};

}  // namespace Shared
