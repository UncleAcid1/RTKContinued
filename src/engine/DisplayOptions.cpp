#include "engine/DisplayOptions.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <string>

#include "engine/SystemFuncs.h"

namespace DisplayOptions {
namespace {
SDL_Window* g_window = nullptr;
bool g_headless = false;
int g_fpsCap = 60;
uint64_t g_nextFrame = 0;   // SDL_GetTicksNS of the next capped frame
}  // namespace

void Init(SDL_Window* window, bool headless, int fpsOverride) {
    g_window = window;
    g_headless = headless;
    g_fpsCap = fpsOverride >= 0 ? fpsOverride : SystemFuncs::GetSetting_Int("fps_cap", 60, "port");
}

void Present() {
    SDL_GL_SwapWindow(g_window);
    if (g_headless || g_fpsCap <= 0) return;
    const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(g_window));
    if (mode && mode->refresh_rate > 0.f && mode->refresh_rate <= (float)g_fpsCap + 1.f) return;
    const uint64_t period = 1000000000ull / (uint64_t)g_fpsCap;
    const uint64_t now = SDL_GetTicksNS();
    if (now < g_nextFrame) {
        SDL_DelayPrecise(g_nextFrame - now);
        g_nextFrame += period;
    } else if (now - g_nextFrame < period) {
        g_nextFrame += period;   // a little late: keep the rhythm
    } else {
        g_nextFrame = now + period;
    }
}

int GetCap() { return g_fpsCap; }

void CycleCap() {
    g_fpsCap = g_fpsCap == 30 ? 60 : (g_fpsCap == 60 ? 0 : 30);
    SystemFuncs::SetSetting_Int("fps_cap", g_fpsCap, "port");
    std::printf("frame-rate cap: %s\n", g_fpsCap ? std::to_string(g_fpsCap).c_str() : "the display's rate");
}

bool IsFullscreen() { return g_window && (SDL_GetWindowFlags(g_window) & SDL_WINDOW_FULLSCREEN) != 0; }

void ToggleFullscreen() {
    if (!g_window || g_headless) return;
    SDL_SetWindowFullscreen(g_window, !IsFullscreen());
}

void OnFullscreen(bool on) {
    if (!g_headless) SystemFuncs::SetSetting_Bool("fullscreen", on, "port");
}

}  // namespace DisplayOptions
