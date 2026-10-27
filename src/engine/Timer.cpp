#include "engine/Timer.h"

#include "game/GameState.h"
#include "game/Map.h"

namespace Timer {

int timeAdvance = 0;
static double g_compensation = 0.0;   // 0x616a00

static unsigned g_sampleCount = 0;   // 0x23eabc
static double g_samples[32];         // 0x23eac4

void AddSample(double frameTime) { g_samples[g_sampleCount++ & 0x1f] = frameTime; }

double ApplyDeltaTimeCompensation(double dt) {
    double d = dt - g_compensation;
    g_compensation = 0.0;
    if (d < 0.0) return 0.001;
    if (0.2 < d) d = 0.2;
    return d;
}

void AddDeltaTimeCompensation(double seconds) { g_compensation += seconds; }

void AdvanceGlobalTime(uint32_t seconds) {
    timeAdvance += (int)seconds;
    if (!Map::IsLoaded()) return;
    GameState::totalTimeSpent += (double)seconds;
}

}  // namespace Timer
