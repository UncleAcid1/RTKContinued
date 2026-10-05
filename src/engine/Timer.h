// Timer: game clocks. Port of the parts of Timer (libkingdom.so 5.11) used so far.
#pragma once
#include <SDL3/SDL.h>

#include <ctime>

namespace Timer {

// @0x23ec08: wall-clock seconds plus the server time offset. Offline port: the offset stays 0.
inline int GetGlobalTime() { return (int)time(nullptr) + 0; }

// @0x23ec2c: seconds since the timer start (0x616b18: the performance counter at Timer init,
// 0x616b20: its frequency). The port's start is the first call.
inline double GetTime() {
    static const uint64_t start = SDL_GetPerformanceCounter();
    static const uint64_t freq = SDL_GetPerformanceFrequency();
    return (double)(SDL_GetPerformanceCounter() - start) / (double)freq;
}

}  // namespace Timer
