// ParticlePresets: the effects of the game's events. Port of ParticlePresets (libkingdom.so 5.11):
// the spells' animations (Particles::SimpleSpellParticle) and the critical hits' debris.
#pragma once

namespace ParticlePresets {

// @0x1db8f8: starts the effect of spell `spell` (its id) at world (x, y) (50 above the ground) and
// returns the seconds until it lands (the fight waits that long before the target is hit). `big`
// picks spell 7's bigger ice cube (the delay is the same). An unknown spell prints an error and
// returns 0.
float OnSpell(unsigned spell, float x, float y, bool big);
// @0x1dc57c: loads the spell's images ahead (big: spell 7's bigger cube).
void SpellPreload(unsigned spell, bool big);

// OnCriticalHit's kinds (an EntityData's "critical_fx"; 100: the hero's block).
enum CriticalHit { kBones = 1, kBlood = 2, kFur = 3, kHuman = 4, kBlock = 100 };
// @0x1dc6dc: debris flying from the hit one at world (tx, ty) away from the hitter at (fx, fy): six
// bones or green blood drops, three tufts of fur or bits of a face; kBlock: the block spark between
// them.
void OnCriticalHit(int kind, float fx, float fy, float tx, float ty);

}  // namespace ParticlePresets

// @0x1dc4e0: images/<path> loaded (and kept) ahead of its first use.
void PreloadImageByPath(const char* path);
