#include "engine/ErrorReporter.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "engine/Render.h"

namespace ErrorReporter {
namespace {

struct Line {
    Render::Sprite* sprite = nullptr;
    float time = 0.f;   // seconds left; the last second fades the line out
};
Line g_lines[10];
int g_count = 0;

}  // namespace

void Print(const char* text, float, float, float) {
    std::printf("%s", text);
    if (!std::strchr(text, '\n')) std::putchar('\n');
}

void Printf(const char* fmt, ...) {
    char buf[0x400];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    buf[0x3ff] = 0;
    Print(buf, 1.f, 0.f, 0.f);
}

void Printf(float r, float g, float b, const char* fmt, ...) {
    char buf[0x400];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    buf[0x3ff] = 0;
    Print(buf, r, g, b);
}

void Update(float dt) {
    for (Line& l : g_lines) l.time -= dt;
    Render::Sprite* s = g_lines[0].sprite;
    if (!s) return;
    const float t = g_lines[0].time;
    if (t > 0.f && t < 1.f) {
        Render::SetAlpha(s, t);
        return;
    }
    if (t >= 0.f) return;
    g_lines[0].sprite = Render::RemoveSprite(s);
    for (int i = 1; i < 10; ++i) {
        g_lines[i - 1] = g_lines[i];
        if (g_lines[i - 1].time < 1.f) g_lines[i - 1].time = 1.f;
    }
    g_lines[9] = Line();   // PORT: the original leaves the last line doubled (never reached: no lines)
    for (int i = 0; i < 10; ++i)
        if (g_lines[i].sprite) Render::SetPosition(g_lines[i].sprite, 0.f, (float)(unsigned)i * 16.f, 0.001f);
    --g_count;
}

}  // namespace ErrorReporter
