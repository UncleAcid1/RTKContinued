// Particles: short-lived sprites thrown by hits, crits and spells (libkingdom.so 5.11 @0x1dcf00..
// 0x1e1d50). Each particle moves in the map's 3D frame (vec3: x / y on the ground, z up), is drawn
// at its isometric 2D point (to2D) on layer 8 with the rotation shader (8), and is deleted once it
// says it is dead. Particles::Update runs them every frame (main's Tick).
#pragma once

#include <cstdint>

namespace Render { struct Sprite; struct Texture; }

// vec3 @0x1dcf00..: the particles' 3D vector.
struct vec3 {
    float x = 0.f, y = 0.f, z = 0.f;
    vec3() = default;
    vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    vec3 to2D() const;                 // @0x1dcf50 the isometric screen point (z = 0)
    static vec3 to3D(float sx, float sy, float z);   // @0x1dcfb8 a world point lifted to height z
    float length() const;              // @0x1e0450
    void normalize();                  // @0x1e1398
    vec3 normalized() const;           // @0x1e13dc
    void rotateZ(float degrees);       // @0x1dd77c about the vertical axis
};
vec3 operator+(const vec3& a, const vec3& b);   // @0x1dd010
vec3 operator-(const vec3& a, const vec3& b);   // @0x1dd054
vec3 operator*(const vec3& a, float f);         // @0x1dd098
float lerp(float t, float a, float b);          // @0x1dcee4 a + (b - a) * t
float randf();                                  // @0x1e004c lrand48() % 1000 / 1000

namespace Particles {

class Particle {   // 0x4c bytes
public:
    Particle() = default;                     // @0x1dd13c
    virtual ~Particle();                      // @0x1de464 (the sprite goes)
    virtual void Update(float dt);            // +0x0c @0x1de12c
    // @0x1deb7c: the sprite (layer 8, the rotation shader) and the start state.
    void Init(Render::Texture* tex, const vec3& pos, float rot, const vec3& vel, float rotSpeed, float scale);

    Render::Sprite* sprite = nullptr;   // +0x04
    vec3 pos;                           // +0x08
    float rot = 0.f;                    // +0x14 radians (the shader's angle)
    vec3 vel;                           // +0x18 per second
    float rotSpeed = 0.f;               // +0x24
    float scale = 1.f;                  // +0x28
    vec3 screen;                        // +0x2c pos.to2D()
    float age = 0.f;                    // +0x38 seconds
    float life = 0.f;                   // +0x3c dead past it (0: no limit)
    bool resort = true;                 // +0x40 the layer is sorted on the first update
    bool dead = false;                  // +0x41
    uint32_t handle = 0;                // +0x44 AddParticle's id (0: untracked)
};

class HitParticle : public Particle {   // fx_block: grows, fades after 0.2 s
public:
    HitParticle(const vec3& pos, float rot, const vec3& vel, float rotSpeed, float scale);   // @0x1df13c
    void Update(float dt) override;   // @0x1de2a8
};

class HumanParticle : public Particle {   // an ear, nose, teeth or eye: bounces four times, fades
public:
    HumanParticle(const vec3& pos, float rot, const vec3& vel, float rotSpeed, float scale);   // @0x1dfe0c
    void Update(float dt) override;   // @0x1de338
    int bounces = 0;          // +0x4c
    float restTime = 0.f;     // +0x50
};

class SparkleParticle : public Particle {   // a coloured spark (shader 11), falls, shrinks
public:
    SparkleParticle(const char* image, const vec3& pos, float rot, const vec3& vel, float rotSpeed, float scale,
                    const vec3& color);   // @0x1defcc
    void Update(float dt) override;   // @0x1df64c
    vec3 color;               // +0x4c
};

class FurParticle : public Particle {   // fx_fur: falls, lies 120 s, fades
public:
    FurParticle(const vec3& pos, float rot, const vec3& vel, float rotSpeed, float scale);   // @0x1df284
    void Update(float dt) override;   // @0x1e008c
};

class BloodParticle : public Particle {   // a flying drop that becomes a ground splat (120 s, fades)
public:
    BloodParticle(bool flying, const vec3& pos, float rot, const vec3& vel, float rotSpeed, float scale);   // @0x1df3a4
    void Update(float dt) override;   // @0x1e017c
    bool flying = false;      // +0x4c
};

class BoneParticle : public Particle {   // fx_bone_crit: bounces, rests, fades
public:
    BoneParticle(const vec3& pos, float rot, const vec3& vel, float rotSpeed, float scale);   // @0x1df504
    void Update(float dt) override;   // @0x1e02b4
    int state = 1;            // +0x4c
    float restTime = 0.f;     // +0x50
};

// A spell's animation (images/<name>, its one-file frames at fps), shown after `delay` seconds.
class SimpleSpellParticle : public Particle {   // 0x74 bytes
public:
    SimpleSpellParticle(const vec3& pos, const char* name, float fps, float delay, bool additive, float shake,
                        float scale);   // @0x1df844
    void Update(float dt) override;   // @0x1e1118
    float delay = 0.f;        // +0x4c
    float zStop = 0.f;        // +0x54 a moving one dies below this height
    int frames = 1;           // +0x58
    int frame = 0;            // +0x5c
    bool exploded = false;    // +0x60 the frames are separate files (none in 5.11's data)
    float frameTime = 0.f;    // +0x64
    float duration = 0.f;     // +0x68
    float shake = 0.f;        // +0x6c camera shake strength when it appears (above 1)
    float spriteScale = 1.f;  // +0x70
};

// A picture broken into soft dots (every second pixel of every second row with alpha >= 128),
// each drawn tinted with its pixel's colour (shader 11) and flying on its own. Only
// DesintegrateImageParticleSystem makes one (its own Update runs).
class ImageParticleSystem : public Particle {   // 0x58 bytes
public:
    struct Dot {                    // 0x2c bytes
        Render::Sprite* sprite;     // +0x00
        vec3 pos;                   // +0x04
        vec3 vel;                   // +0x10
        vec3 color;                 // +0x1c the pixel's colour (alpha taken out)
        float start;                // +0x28 when it starts to move (Desintegrate)
    };
    // @0x1e0478: pos is the picture's bottom-left screen point; frame of a one-file sheet of
    // `frames` frames, mirrored if asked.
    ImageParticleSystem(const vec3& pos, const char* path, int frames, int frame, bool mirror, unsigned mode);
    ~ImageParticleSystem() override;   // @0x1de9bc
    void Update(float dt) override;    // @0x1ddf10
    unsigned count = 0;     // +0x4c
    Dot* dots = nullptr;    // +0x50
    unsigned mode = 0;      // +0x54
};

// A dead monster's picture dissolving (Entity::OnDying, killed by a "spell_fx" attacker; mode is
// that spell_fx: 3 swirls up and away, 2 bursts, others rise).
class DesintegrateImageParticleSystem : public ImageParticleSystem {   // 0x5c bytes
public:
    DesintegrateImageParticleSystem(const vec3& pos, const char* path, float z, int frames, int frame, bool mirror,
                                    unsigned mode);   // @0x1e0e58
    ~DesintegrateImageParticleSystem() override;      // @0x1dead8
    void Update(float dt) override;                   // @0x1dd7f0
    float z = 0.f;          // +0x58 the sprites' depth
};

// A buff potion's glow on the hero's chest (AIPlayer::PlayBuffAnimation): it pulses for a second
// while clouds puff out of it.
class BuffParticleSystem : public Particle {   // 0x5c bytes
public:
    BuffParticleSystem(const vec3& pos, const vec3& color);   // @0x1dec0c
    void Update(float dt) override;                           // @0x1e1400
    float spawnTime = 0.f;  // +0x4c
    vec3 color;             // +0x50 (unused)
};

// A mage's spell orb (Entity::UpdateParticles): it follows the hand through "attack1" leaving
// sparkles, then flies to the target and bursts.
class MageParticle : public Particle {   // 0x8c bytes
public:
    MageParticle(const vec3& pos, const vec3& color, const vec3& trailColor);   // @0x1dedbc
    void Update(float dt) override;                                 // @0x1e1a00
    virtual void SetPosition(const vec3& pos, int frame, int frames);   // +0x10 @0x1e1778
    virtual void FlyTo(const vec3& target, float duration);         // +0x14 @0x1dd24c
    bool flying = false;    // +0x4c
    float duration = 0.f;   // +0x50
    float flyTime = 0.f;    // +0x54
    float trailTime = 0.f;  // +0x58
    vec3 from;              // +0x5c
    vec3 to;                // +0x68
    vec3 color;             // +0x74
    vec3 trailColor;        // +0x80
};

void Init();                                   // @0x1dd2c8 (empty)
void Deinit();                                 // @0x1dd2cc
void Update(float dt);                         // @0x1dd504
// @0x1dd6a4: the particle joins (and updates once at 0 s); tracked: it gets an id IsCompleted knows.
uint32_t AddParticle(Particle* p, bool tracked);
bool IsCompleted(uint32_t handle, bool remove);   // @0x1dd334
void ClearParticles();                         // @0x1dd3cc

}  // namespace Particles
