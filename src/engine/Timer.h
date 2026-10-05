// Timer: game clocks. Port of the parts of Timer (libkingdom.so 5.11) used so far.
#pragma once
#include <ctime>

namespace Timer {

// @0x23ec08: wall-clock seconds plus the server time offset. Offline port: the offset stays 0.
inline int GetGlobalTime() { return (int)time(nullptr) + 0; }

}  // namespace Timer
