// Timer: game clocks. Port of the parts of Timer (libkingdom.so 5.11) used so far.
#pragma once
#include <SDL3/SDL.h>

#include <cstdint>
#include <ctime>

namespace Timer {

extern int timeAdvance;   // 0x616b10 seconds the global time runs ahead (saved, chunk 0xf)

// @0x23ec08: wall-clock seconds plus the time advance.
inline int GetGlobalTime() { return (int)time(nullptr) + timeAdvance; }
inline void ResetTimeAdvance() { timeAdvance = 0; }                  // @0x23eb68
inline uint32_t GetTimeAdvance() { return (uint32_t)timeAdvance; }   // @0x23ebd4
// @0x23ea38: the frame time less the compensation gathered since the last frame (0x616a00; added
// by TimeCompensationBlock around slow work, not ported: always 0), at most 0.2 s; below 0 it is
// 0.001 s.
double ApplyDeltaTimeCompensation(double dt);
// @0x23eb80: once a map is loaded the advance also counts to GameState::totalTimeSpent.
void AdvanceGlobalTime(uint32_t seconds);

// @0x23ec2c: seconds since the timer start (0x616b18: the performance counter at Timer init,
// 0x616b20: its frequency). The port's start is the first call.
inline double GetTime() {
    static const uint64_t start = SDL_GetPerformanceCounter();
    static const uint64_t freq = SDL_GetPerformanceFrequency();
    return (double)(SDL_GetPerformanceCounter() - start) / (double)freq;
}

}  // namespace Timer
