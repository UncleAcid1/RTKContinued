#include "engine/Timer.h"

#include "game/GameState.h"
#include "game/Map.h"

namespace Timer {

int timeAdvance = 0;
static double g_compensation = 0.0;   // 0x616a00

double ApplyDeltaTimeCompensation(double dt) {
    double d = dt - g_compensation;
    g_compensation = 0.0;
    if (d < 0.0) return 0.001;
    if (0.2 < d) d = 0.2;
    return d;
}

void AdvanceGlobalTime(uint32_t seconds) {
    timeAdvance += (int)seconds;
    if (!Map::IsLoaded()) return;
    GameState::totalTimeSpent += (double)seconds;
}

}  // namespace Timer
