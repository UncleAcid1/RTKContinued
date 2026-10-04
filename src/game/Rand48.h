// srand48 / lrand48 as implemented by Android bionic (the original's libc): 48-bit LCG
// x' = (0x5DEECE66D * x + 0xB) mod 2^48, srand48(s): x = (s << 16) | 0x330E, lrand48 = x >> 17.
// Implemented here so results are identical on every host OS.
#pragma once
#include <cstdint>

namespace Rand48 {
void srand48(long seed);
long lrand48();
}
