// PORT (user 2026-10-18, 2026-10-27): the desktop's display options, kept in settings.txt (file
// "port"). The frame-rate cap ("fps_cap": 30, 60 or 0 for the display's rate; 60 by default, the
// phones' rate: the port otherwise runs at the display's 120 Hz) waits after the buffer swap; on a
// display of the cap's rate or slower vsync alone paces the frames. "fullscreen" is the window's
// state. Both are switched from the settings window's two desktop buttons (SettingsWindow), or with
// keys: F10 cycles the cap; F11, Cmd+Ctrl+F, Cmd+Enter or Alt+Enter switch fullscreen (as the
// window's green button does). Headless test runs are never capped and never fullscreen.
#pragma once

struct SDL_Window;

namespace DisplayOptions {

// The window, whether this is a headless run, and --fps (-1: the setting).
void Init(SDL_Window* window, bool headless, int fpsOverride);
void Present();             // the buffer swap, then the cap's wait
int GetCap();               // 30, 60 or 0 (the display's rate)
void CycleCap();            // 30 -> 60 -> the display's rate -> 30
bool IsFullscreen();
void ToggleFullscreen();
void OnFullscreen(bool on);   // the window entered or left fullscreen (a key, a button, the system)

}  // namespace DisplayOptions
