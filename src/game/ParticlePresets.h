// ParticlePresets: the effects of the game's events. Port of ParticlePresets (libkingdom.so 5.11).
//
// PORT: only OnSpell's timing is here. The particles themselves (Particles::SimpleSpellParticle)
// are milestone 6; until then a spell shows its damage numbers and nothing else.
#pragma once

namespace ParticlePresets {

// @0x1db8f8: starts the effect of spell `spell` (its id) at world (x, y) and returns the seconds
// until it lands (the fight waits that long before the target is hit). `big` picks spell 7's
// bigger ice cube (the delay is the same). An unknown spell prints an error and returns 0.
float OnSpell(unsigned spell, float x, float y, bool big);

}  // namespace ParticlePresets
