#include "hud/ContentScroller.h"

#include <cmath>
#include <cstdlib>
#include <limits>

namespace Shared {

using GUI::Window;

namespace {
// FUN_00340234: steps cur towards target by step without passing it.
float MoveTowards(float cur, float target, float step) {
    if (cur == target) return cur;
    if (target > cur) {
        cur += step;
        return cur > target ? target : cur;
    }
    cur -= step;
    return cur < target ? target : cur;
}

// VCVT.S32.F32: truncates, saturates, NaN gives 0 (the scrollbar divisions can divide by zero).
int ToInt(float f) {
    if (std::isnan(f)) return 0;
    if (f >= 2147483648.f) return std::numeric_limits<int>::max();
    if (f <= -2147483648.f) return std::numeric_limits<int>::min();
    return (int)f;
}
}  // namespace

void ContentScroller::Init() {
    velocity = accum = bounce = fadeTimer = 0.f;
    freeScroll = true;
    unkA0 = 1;
    itemSize = 1;
    perLine = 1;
    windows.clear();
    rects.clear();
    scrollbar = track = nullptr;
    viewport = {0, 0, 0, 0};
    horizontal = false;
    itemCount = 0;
    firstIndex = 0;
    offset = 0;
    pressed = dragging = false;
    lastPos = 0;
    padStart = padEnd = 0;
    barMarginStart = barMarginEnd = 0;
    noRecycle = false;
    fadeBar = false;
    minBarSize = 0x18;
    barRatio = 0.f;
    onScroll = onNext = onPrev = nullptr;
}

// A press inside the viewport (coordinates relative to win) starts tracking; the release after a
// drag flings the content at the pointer's speed (at most 2000 px/s, nothing under 20) and takes
// the click.
bool ContentScroller::Click(int x, int y, bool isPressed, Window* win) {
    if (win) {
        x -= win->x;
        y -= win->y;
    }
    if (isPressed) {
        if (viewport.left <= x && x <= viewport.right && viewport.top <= y && y <= viewport.bottom) {
            if (horizontal) lastPos = (win ? win->x : 0) + x;
            else lastPos = (win ? win->y : 0) + y;
            bounce = velocity = accum = 0.f;
            pressed = true;
            return false;
        }
    } else {
        pressed = false;
        if (dragging) {
            dragging = false;
            float speed = GUI::GetMouseSpeed(horizontal);
            if (speed < -2000.f) {
                velocity = -2000.f;
            } else if (speed > 2000.f) {
                velocity = 2000.f;
            } else {
                velocity = speed;
                if (std::fabs(speed) < 20.f) velocity = 0.f;
            }
            return true;
        }
    }
    return false;
}

// Dragging starts once the pointer is more than 10 pixels from the press. Past either end the
// content follows ever more slowly (not at all once out by 30% of the view) unless pulled back.
void ContentScroller::Move(int x, int y) {
    int pos = horizontal ? x : y;
    if (pressed && std::abs(pos - lastPos) > 10) {
        accum = velocity = 0.f;
        dragging = true;
    } else if (!dragging) {
        return;
    }
    int delta = pos - lastPos;
    int contentLen = ContentLen();
    int scrolled = Scrolled();
    int over = scrolled >= -padStart ? 0 : scrolled + padStart;
    int end = ViewLen() + scrolled;
    if (contentLen < end) over = end - contentLen;
    float f = std::fabs((float)over) / ((float)ViewLen() * 0.3f);
    if (f > 1.f) f = 1.f;
    if ((over & delta) < 0) f = 0.f;
    if (over > 0 && delta > 0) f = 0.f;
    accum += (float)delta * (1.f - f);
    HandleMove(0);
    lastPos = pos;
}

// Shows the typed windows in the virtual rectangles that are in view (true if any changed) and
// sizes and places the scrollbar.
bool ContentScroller::UpdateScroll() {
    for (VirtualRect& vr : virtualRects) {
        vr.prevWindow = vr.window;
        vr.window = nullptr;
    }
    for (TypedWindow& tw : typedWindows) tw.used = false;
    for (VirtualRect& vr : virtualRects) {
        bool inView = horizontal ? vr.x + vr.w >= 0 && vr.x <= viewport.right - viewport.left
                                 : vr.y + vr.h >= 0 && vr.y <= viewport.bottom - viewport.top;
        if (!inView) continue;
        for (TypedWindow& tw : typedWindows) {
            if (tw.used || tw.type != vr.type) continue;
            vr.window = tw.window;
            tw.used = true;
            tw.window->SetPosition(vr.x + viewport.left, vr.y + viewport.top);
            break;
        }
    }
    bool changed = false;
    for (VirtualRect& vr : virtualRects) {
        changed = vr.prevWindow != vr.window;
        if (changed) break;
    }

    Window* bar = scrollbar;
    if (!bar) return changed;
    if (itemCount == 0) {
        bar->SetVisibility(false);
        return changed;
    }
    int contentLen = ContentLen();
    int scrolled = Scrolled();
    int viewLen = ViewLen();
    int end = viewLen + scrolled;
    int over = scrolled >= -padStart ? 0 : -padStart - scrolled;
    if (contentLen < end) over = end - contentLen;
    int over2 = over * over;
    int total = contentLen + padStart;
    int size = minBarSize;
    if (barRatio != 0.f) size = ToInt((float)viewLen * ((float)viewLen * barRatio) / (float)total);
    int barLen = ToInt((float)viewLen * (float)viewLen / (float)(over2 + total));
    if (size < barLen) size = barLen;
    bar->SetVisibility(viewLen < total);
    float num = (float)(scrolled + padStart + (contentLen < end ? over2 : over));
    float den = (float)(total - viewLen + over2);
    if (!horizontal) {
        int oldY = bar->y, oldH = bar->h;
        bar->SetBorders(3, 6, 3, 6);
        bar->SetSize(bar->w, size);
        int base = track ? track->y : viewport.top;
        float range = (float)(barMarginEnd + viewport.bottom - viewport.top - bar->h);
        bar->SetPosition(bar->x, ToInt(range * (num / den)) + barMarginStart + base);
        if (oldY != bar->y || oldH != bar->h) fadeTimer = 0.f;
    } else {
        int oldX = bar->x, oldW = bar->w;
        bar->SetBorders(6, 3, 6, 3);
        bar->SetSize(size, bar->h);
        int base = track ? track->x : viewport.left;
        float range = (float)(barMarginEnd + viewport.right - viewport.left - bar->w);
        bar->SetPosition(ToInt(range * (num / den)) + barMarginStart + base, bar->y);
        if (oldX != bar->x || oldW != bar->w) fadeTimer = 0.f;
    }
    return changed;
}

void ContentScroller::Shift(int d) {
    for (Window* w : windows) {
        if (horizontal) w->MoveWindow(d, 0);
        else w->MoveWindow(0, d);
    }
    for (GUI::ClipRect* r : rects) {
        if (horizontal) {
            r->left += d;
            r->right += d;
        } else {
            r->top += d;
            r->bottom += d;
        }
    }
}

// Applies d plus the whole pixels of the accumulated movement, at most one line per call (and,
// without freeScroll, not past the ends). Returns the pixels moved.
int ContentScroller::HandleMove(int d) {
    float f = (float)d + accum;
    int step = (int)f;
    accum = f;
    if (step != 0) accum = f - (float)step;
    if (!CanMove() && !CanMoveLeft() && !CanMoveRight()) step = 0;
    if (step >= itemSize) step = itemSize;
    if (step < -itemSize) step = -itemSize;
    if (!freeScroll) {
        int scrolled = Scrolled();
        if (scrolled - step < -padStart) step = -padStart < scrolled ? scrolled + padStart : 0;
        int end = ViewLen() + scrolled;
        int contentLen = ContentLen();
        if (contentLen < end - step) {
            if (contentLen <= end) return 0;
            step = end - contentLen;
        }
    }
    if (step == 0) return 0;

    offset += step;
    Shift(step);
    for (VirtualRect& vr : virtualRects) {
        if (horizontal) vr.x += step;
        else vr.y += step;
    }
    bool changed = UpdateScroll();
    if (!windows.empty() && windows.back()->root) windows.back()->root->UpdatePosition();

    if (offset < -itemSize && !noRecycle) {
        offset += itemSize;
        firstIndex += perLine;
        Shift(itemSize);
        if (onNext) onNext();
        if (onScroll) onScroll();
    }
    if (offset > 0 && firstIndex != 0 && !noRecycle) {
        firstIndex -= perLine;
        offset -= itemSize;
        Shift(-itemSize);
        if (onPrev) onPrev();
        if (onScroll) onScroll();
    }
    if (changed && onScroll) onScroll();
    return step;
}

bool ContentScroller::CanMove() { return ViewLen() < ContentLen() + padStart; }

bool ContentScroller::CanMoveLeft() {
    int scrolled = Scrolled();
    if (-padStart < scrolled) return true;
    if (!CanMove()) return scrolled < -padStart;
    return false;
}

bool ContentScroller::CanMoveRight() { return ViewLen() + Scrolled() < ContentLen(); }

// Back to the start, then forward until the item's line ends inside the view (the content's end
// at most at the view's end).
void ContentScroller::ScrollIntoView(unsigned index) {
    firstIndex = 0;
    HandleMove(-offset);
    accum = bounce = 0.f;
    if (!CanMove()) return;
    unsigned line = index / perLine;
    int start = itemSize * (int)Lines(firstIndex) - offset;
    if ((int)(perLine * line / perLine) * itemSize + itemSize <= ViewLen() + start) return;
    firstIndex = perLine * ((itemSize * (int)perLine * (int)line / itemSize) / (int)perLine);
    int contentLen = ContentLen();
    int end = ViewLen() + itemSize * (int)Lines(firstIndex) - offset;
    if (contentLen < end) {
        int rest = end - contentLen;
        firstIndex -= perLine * (rest / itemSize);
        HandleMove(rest % itemSize);
        return;
    }
    if (onScroll) onScroll();
}

void ContentScroller::ScrollToTop() {
    firstIndex = 0;
    HandleMove(-offset);
    for (int rest = padStart; rest > 0; rest -= itemSize) HandleMove(rest <= itemSize ? rest : itemSize);
    accum = bounce = 0.f;
}

// Index of the window among the virtual rectangles of its type, -1 if it shows none.
int ContentScroller::GetWindowIndex(Window* w) {
    unsigned type = 0;
    for (TypedWindow& tw : typedWindows)
        if (tw.window == w) type = tw.type;
    int n = 0;
    for (VirtualRect& vr : virtualRects) {
        if (vr.type != type) continue;
        if (vr.window == w) return n;
        ++n;
    }
    return -1;
}

int ContentScroller::GetWindowData(Window* w) {
    unsigned type = 0;
    for (TypedWindow& tw : typedWindows)
        if (tw.window == w) type = tw.type;
    for (VirtualRect& vr : virtualRects)
        if (vr.type == type && vr.window == w) return (int)vr.data;
    return -1;
}

void ContentScroller::AddTypedWindow(unsigned type, Window* w) { typedWindows.push_back({type, w, false}); }

void ContentScroller::AddVirtualrect(unsigned type, int x, int y, int w, int h, unsigned data) {
    if (horizontal) x += offset;
    else y += offset;
    virtualRects.push_back({type, x, y, w, h, data, nullptr, nullptr});
}

// Settles the content: back inside the ends (15x the overshoot per second, at least 10 px/s),
// onto the nearest snap point (10x), the fling (slowing by 2000 px/s each second) and the
// programmed bounce; fades the scrollbar out 0.1-0.3 s after it last moved. True if it moved.
bool ContentScroller::Update(float dt) {
    if (!dragging && !pressed && (CanMove() || CanMoveLeft())) {
        int contentLen = ContentLen();
        int scrolled = Scrolled();
        int end = ViewLen() + scrolled;
        int over;
        if (-padStart > scrolled) over = scrolled + padStart;
        else if (contentLen < end) over = end - contentLen;
        else over = 0;
        if (over < 0) {
            float o = (float)-over;
            bounce = 0.f;
            float s = (o >= 10.f ? o : 10.f) * 15.f * dt;
            if (o < s) s = o;
            accum -= s;
            if (velocity > 0.f) velocity = MoveTowards(velocity, 0.f, dt * 5000.f);
        } else if (over > 0) {
            float o = (float)over;
            bounce = 0.f;
            float s = (o >= 10.f ? o : 10.f) * 15.f * dt;
            if (o < s) s = o;
            accum += s;
            if (velocity < 0.f) {
                velocity += dt * 5000.f;
                if (velocity > 0.f) velocity = 0.f;
            }
        }
        int nearest = -1;
        for (int i = 0; i < (int)snapPoints.size(); ++i)
            if (nearest == -1 || std::fabs((float)(offset + snapPoints[i])) < std::fabs((float)(offset + snapPoints[nearest])))
                nearest = i;
        int d = nearest == -1 ? 0 : snapPoints[nearest] + offset;
        if (over == 0 && bounce == 0.f && d != 0) {
            float fd = (float)d;
            if (-fd > 0.f) {
                float m = -fd >= 10.f ? -fd : 10.f;
                float s = m * 10.f * dt;
                if (s > -fd) s = -fd;
                if (-padStart < scrolled && std::fabs(velocity) < m) accum += s;
            } else {
                float m = fd >= 10.f ? fd : 10.f;
                float s = m * 10.f * dt;
                if (fd < s) s = fd;
                if (end < contentLen && std::fabs(velocity) < m) accum -= s;
            }
        }
    }
    if (velocity != 0.f) {
        if (!CanMove()) {
            velocity = 0.f;
        } else {
            accum += dt * velocity;
            velocity = MoveTowards(velocity, 0.f, dt * 2000.f);
        }
    }
    float b = std::fabs(bounce) * 10.f;
    if (b < 50.f) b = 50.f;
    if (bounce != 0.f) {
        accum += (bounce > 0.f ? b : -b) * dt;
        if (bounce < 0.f) {
            bounce += b * dt;
            if (bounce > 0.f) bounce = 0.f;
        } else {
            bounce -= b * dt;
            if (bounce < 0.f) bounce = 0.f;
        }
    }
    if (fadeBar) {
        fadeTimer += dt;
        if (fadeTimer > 0.3f) scrollbar->SetAlpha(0.f, true);   // and then the ramp below, as the original
        if (fadeTimer <= 0.1f) scrollbar->SetAlpha(1.f, true);
        else scrollbar->SetAlpha((fadeTimer - 0.1f) / -0.2f + 1.f, true);
    }
    if ((int)accum != 0) {
        if (HandleMove(0) == 0) velocity = 0.f;
        accum -= (float)(int)accum;
        return true;
    }
    return false;
}

}  // namespace Shared
