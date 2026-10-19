#include "game/ParticlePresets.h"

#include <cstdio>

namespace ParticlePresets {

float OnSpell(unsigned spell, float, float, bool) {
    switch (spell) {
    case 1: return 0.f;
    case 2: return 0.9f;
    case 3: return 1.f;
    case 4:
    case 5: return 0.25f;
    case 6: return 0.75f;
    case 7: return 1.75f;
    case 8: return 0.25f;
    case 9: return 0.5f;
    default:
        std::printf("ERROR: ParticlePresets::OnSpell() Unknown spell effect %d\n", (int)spell);
        return 0.f;
    }
}

}  // namespace ParticlePresets
