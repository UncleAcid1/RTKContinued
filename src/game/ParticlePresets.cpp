#include "game/ParticlePresets.h"

#include <cstdio>
#include <cstring>

#include "engine/Resources.h"
#include "game/Particles.h"
#include "game/Rand48.h"

using Particles::AddParticle;
using Particles::SimpleSpellParticle;

namespace {

// A float from its bits (the asm's movw / movt constants, kept exact).
float Bits(uint32_t u) {
    float f;
    std::memcpy(&f, &u, sizeof f);
    return f;
}

// The three points around a target the triple meteor and the storm's bolts fall on (120 degrees
// apart, 50 out).
const vec3 kAround1(Bits(0xc1c80001), Bits(0x422d3480), 0.f);   // (-25, 43.3)
const vec3 kAround2(Bits(0xc1c7fffd), Bits(0xc22d3481), 0.f);   // (-25, -43.3)
const vec3 kAround3(Bits(0x42193578), Bits(0xc2008ebb), 0.f);   // (38.3, -32.1)

SimpleSpellParticle* Spell(const vec3& at, const char* name, float fps, float delay, bool additive, float shake,
                           float scale) {
    return new SimpleSpellParticle(at, name, fps, delay, additive, shake, scale);
}

// A meteor `delay` seconds late: from 600 back along `dir` it flies at 1200 per second and burns
// out below 70 (with rot, its tilt).
void Meteor(const vec3& at, const vec3& dir, float delay, bool tilted) {
    SimpleSpellParticle* m = Spell(at, "Spells/meteor_n/meteor/meteor", 2.f, delay, true, 0.f, 1.f);
    m->zStop = 70.f;
    if (tilted) m->rot = 0.5235988f;   // (0x3f060a92: 30 degrees)
    m->vel = dir * 1200.f;
    AddParticle(m, false);
}

// The debris' random spread: (lrand48 % 100) / 100 - 0.5.
float Spread() { return (float)(Rand48::lrand48() % 100) / 100.f - 0.5f; }

}  // namespace

void PreloadImageByPath(const char* path) {
    // (the texture is marked resident and loaded at once; the port's textures always are)
    char buf[256];
    std::snprintf(buf, sizeof buf, "images/%s", path);
    Resources::GetImage(buf);
}

namespace ParticlePresets {

float OnSpell(unsigned spell, float x, float y, bool big) {
    const vec3 at = vec3::to3D(x, y, 50.f);
    switch (spell) {
    case 1:   // the fire ball
        AddParticle(Spell(at, "Spells/ball_expl/ball", 16.f, 0.f, false, 0.f, 1.f), true);
        return 0.f;
    case 2: {   // the meteor
        vec3 dir(0.f, 0.f, -1.f);
        dir.normalize();
        Meteor(at - dir * 600.f, dir, 0.f, false);
        AddParticle(Spell(at + vec3(0.f, 0.f, 20.f), "Spells/incinerate/incinerate", 15.f, 0.45f, false, 15.f, 1.f), false);
        return 0.9f;
    }
    case 3: {   // three meteors, each with its fire and burning ring
        vec3 dir(1.f, 0.f, -1.f);
        dir.normalize();
        const vec3 from = at - dir * 600.f;
        const vec3* around[3] = {&kAround1, &kAround2, &kAround3};
        const float meteor[3] = {0.f, 0.1f, 0.2f}, fire[3] = {0.45f, 0.55f, 0.65f}, ring[3] = {0.5f, 0.6f, 0.7f};
        for (int i = 0; i < 3; ++i) {
            Meteor(from + *around[i], dir, meteor[i], true);
            AddParticle(Spell(at + *around[i], "Spells/incinerate/incinerate", 15.f, fire[i], false, 10.f, 1.f), false);
            AddParticle(Spell(at + *around[i], "Spells/fire_ring_1/f_ring", 15.f, ring[i], false, 0.f, 1.f), false);
        }
        return 1.f;
    }
    case 4:   // the spark
        AddParticle(Spell(at + vec3(-20.f, 0.f, 0.f), "Spells/spark/spark", 15.f, 0.f, false, 0.f, 1.f), false);
        return 0.25f;
    case 5:   // the lightning
        AddParticle(Spell(at + vec3(0.f, 0.f, 140.f), "Spells/lighting_n/lighting/lighting", 15.f, 0.f, false, 0.f, 2.f),
                    false);
        return 0.25f;
    case 6: {   // the storm: a cloud and three bolts around
        AddParticle(Spell(at + vec3(0.f, 0.f, 130.f), "Spells/lighting_n/cloud/cloud", 10.f, 0.f, true, 0.f, 1.f), false);
        const vec3 bolts[3] = {vec3(kAround1.x, kAround1.y, 50.f), vec3(kAround2.x, kAround2.y, 50.f),
                               vec3(50.f, Bits(0xb77d5434), 50.f)};
        const float delays[3] = {0.25f, 0.5f, 0.75f};
        for (int i = 0; i < 3; ++i)
            AddParticle(Spell(at + bolts[i], "Spells/lighting_n/lighting/lighting", 15.f, delays[i], false, 5.f, 1.f), false);
        return 0.75f;
    }
    case 7:   // the ice cube
        AddParticle(Spell(at, big ? "Spells/ice_qube_big/qube_big" : "Spells/ice_qube_small/qube_small", 15.f, 0.f, false,
                          0.f, 1.f),
                    false);
        return 1.75f;
    case 8:   // the ice spikes
        AddParticle(Spell(at, "Spells/ice_spikes/spikes", 15.f, 0.f, false, 0.f, 1.f), false);
        return 0.25f;
    case 9:   // the blizzard
        AddParticle(Spell(at + vec3(-20.f, 0.f, 0.f), "Spells/blizzard/blizzard", 15.f, 0.f, false, 0.f, 1.f), false);
        return 0.5f;
    default:
        std::printf("ERROR: ParticlePresets::OnSpell() Unknown spell effect %d\n", (int)spell);
        return 0.f;
    }
}

void SpellPreload(unsigned spell, bool big) {
    switch (spell) {
    case 1: PreloadImageByPath("Spells/ball_expl/ball"); return;
    case 2:
        PreloadImageByPath("Spells/meteor_n/meteor/meteor");
        PreloadImageByPath("Spells/incinerate/incinerate");
        return;
    case 3:
        PreloadImageByPath("Spells/meteor_n/meteor/meteor");
        PreloadImageByPath("Spells/incinerate/incinerate");
        PreloadImageByPath("Spells/fire_ring_1/f_ring");
        return;
    case 4: PreloadImageByPath("Spells/spark/spark"); return;
    case 5: PreloadImageByPath("Spells/lighting_n/lighting/lighting"); return;
    case 6:
        PreloadImageByPath("Spells/lighting_n/cloud/cloud");
        PreloadImageByPath("Spells/lighting_n/lighting/lighting");
        return;
    case 7: PreloadImageByPath(big ? "Spells/ice_qube_big/qube_big" : "Spells/ice_qube_small/qube_small"); return;
    case 8: PreloadImageByPath("Spells/ice_spikes/spikes"); return;
    case 9: PreloadImageByPath("Spells/blizzard/blizzard"); return;
    default: std::printf("ERROR: ParticlePresets::OnSpell() Unknown spell effect %d\n", (int)spell); return;
    }
}

// Each piece: the direction from the hitter to the hit one, 60 per second times a random factor,
// turned by a random angle; a random rotation (lrand48 / 1000) and spin.
void OnCriticalHit(int kind, float fx, float fy, float tx, float ty) {
    switch (kind) {
    case kBlood: {
        const vec3 at = vec3::to3D(tx, ty, 30.f), from = vec3::to3D(fx, fy, 50.f);
        for (int i = 0; i < 6; ++i) {
            vec3 v = (at - from).normalized() * 60.f;
            v = v * (Spread() + 4.f);
            v.rotateZ(Spread() * 90.f);
            const float rot = (float)Rand48::lrand48() / 1000.f;
            AddParticle(new Particles::BloodParticle(true, at, rot, v, 4.f, 0.75f), false);
        }
        return;
    }
    case kBlock: {
        const vec3 at = vec3::to3D(tx, ty, 50.f), from = vec3::to3D(fx, fy, 50.f);
        const vec3 mid = (at + from) * 0.5f;
        const float rot = (float)Rand48::lrand48() / 1000.f;
        AddParticle(new Particles::HitParticle(mid, rot, vec3(0.f, 0.f, 0.f), 0.f, 0.1f), false);
        return;
    }
    case kHuman: {
        const vec3 at = vec3::to3D(tx, ty, 30.f), from = vec3::to3D(fx, fy, 30.f);
        for (int i = 0; i < 3; ++i) {
            vec3 v = (at - from).normalized() * 60.f;
            v = v * (Spread() * 2.f + 5.f);
            v.rotateZ(Spread() * 90.f);
            const float rot = (float)Rand48::lrand48() / 1000.f;
            const float spin = Spread() * 8.f;
            AddParticle(new Particles::HumanParticle(at, rot, v, spin, 0.75f), false);
        }
        return;
    }
    case kBones: {
        const vec3 at = vec3::to3D(tx, ty, 20.f), from = vec3::to3D(fx, fy, 5.f);
        for (int i = 0; i < 6; ++i) {
            vec3 v = (at - from).normalized() * 60.f;
            v = v * (Spread() * 2.f + 4.f);
            v.rotateZ(Spread() * 40.f);
            const float rot = (float)Rand48::lrand48() / 1000.f;
            const float spin = 7.5f + Spread() * 4.f;
            AddParticle(new Particles::BoneParticle(at, rot, v, spin, 0.75f), false);
        }
        return;
    }
    case kFur: {
        const vec3 at = vec3::to3D(tx, ty, 30.f), from = vec3::to3D(fx, fy, 0.f);
        for (int i = 0; i < 3; ++i) {
            vec3 v = (at - from).normalized() * 60.f;
            v = v * (Spread() + 2.f);
            v.x = v.x * 0.25f;
            v.y = v.y * 0.5f;
            v.rotateZ(Spread() * 45.f);
            const float rot = (float)Rand48::lrand48() / 1000.f;
            const float spin = Spread() * 4.f;
            AddParticle(new Particles::FurParticle(at, rot, v, spin, 1.f), false);
        }
        return;
    }
    default:
        return;
    }
}

}  // namespace ParticlePresets
