#include "engine/Timer.h"

#include "game/GameState.h"
#include "game/Map.h"

namespace Timer {

int timeAdvance = 0;

void AdvanceGlobalTime(uint32_t seconds) {
    timeAdvance += (int)seconds;
    if (!Map::IsLoaded()) return;
    GameState::totalTimeSpent += (double)seconds;
}

}  // namespace Timer
