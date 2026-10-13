// BRand: the game's seeded generator for fights (BRand::seed @0x10b128, gen_state @0x10b020). It is
// MT19937 with the standard seeding and tempering, which is exactly std::mt19937.
#pragma once
#include <cstdint>
#include <random>

namespace BRand {

inline std::mt19937& State() {
    static std::mt19937 g;
    return g;
}
inline void seed(uint32_t s) { State().seed(s); }
inline uint32_t Next() { return (uint32_t)State()(); }
// The draw every user makes: 0..99 (value * 2^-32 * 100, truncated).
inline int Percent() { return (int)(long long)((double)Next() * 2.3283064370807974e-10 * 100.0); }

}  // namespace BRand
