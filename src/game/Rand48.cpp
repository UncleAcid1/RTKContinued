#include "game/Rand48.h"

namespace Rand48 {
namespace {
uint64_t g_x = 0x1234ABCD330EULL;
}

void srand48(long seed) { g_x = (((uint64_t)(uint32_t)seed) << 16) | 0x330E; }

long lrand48() {
    g_x = (0x5DEECE66DULL * g_x + 0xB) & ((1ULL << 48) - 1);
    return (long)(g_x >> 17);
}
}  // namespace Rand48
