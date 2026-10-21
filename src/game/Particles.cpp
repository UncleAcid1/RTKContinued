// Port of Particles and vec3 (libkingdom.so 5.11), see Particles.h.
#include "game/Particles.h"

#include <cmath>
#include <cstdio>
#include <vector>

#include "engine/Render.h"
#include "engine/Resources.h"
#include "game/Map.h"
#include "game/Rand48.h"

// ---------------------------------------------------------------------------------------- vec3

vec3 vec3::to2D() const {
    const float c = 0.70710677f;   // 0x3f3504f3
    vec3 r;
    r.x = y * -c + x * c;
    r.y = (y * c + x * c) * 0.5f - z;
    r.z = 0.f;
    return r;
}

vec3 vec3::to3D(float sx, float sy, float z) {
    const float c = 0.70710677f;
    const float yy = (sy + sy) * c;
    return vec3(yy + sx * c, yy + sx * -c, z);
}

float vec3::length() const { return std::sqrt(y * y + x * x + z * z); }

void vec3::normalize() {
    const float f = 1.f / length();
    z *= f;
    x *= f;
    y *= f;
}

vec3 vec3::normalized() const {
    vec3 r = *this;
    r.normalize();
    return r;
}

void vec3::rotateZ(float degrees) {
    const float a = degrees / 180.f * 3.1416f;   // (0x40490fe6)
    const float c = (float)std::cos((double)a), s = (float)std::sin((double)a);
    const float ox = x, oy = y;
    y = c * oy + ox * s;
    x = ox * c - oy * s;
}

vec3 operator+(const vec3& a, const vec3& b) { return vec3(a.x + b.x, a.y + b.y, a.z + b.z); }
vec3 operator-(const vec3& a, const vec3& b) { return vec3(a.x - b.x, a.y - b.y, a.z - b.z); }
vec3 operator*(const vec3& a, float f) { return vec3(a.x * f, a.y * f, a.z * f); }

namespace Particles {

namespace {
// @0x1dd434 (the static initializer): the two lists, freed at exit.
std::vector<Particle*> g_particles;   // 0x613e68
std::vector<uint32_t> g_completed;    // 0x613e78 the tracked ones that ended
uint32_t g_lastHandle = 0;            // 0x613e88

// The sprite at the particle's screen point (its centre), sized by scale; shader 8's centre and angle.
void PlaceSprite(Particle* p, float zBias) {
    Render::Sprite* s = p->sprite;
    const float h = p->scale * (float)s->tex->h;
    const float w = (float)s->tex->w * p->scale;
    s->h = h;
    s->w = w;
    Render::SetPosition(s, p->screen.x + w * -0.5f, p->screen.y + h * 0.5f,
                        Map::GetSpriteZ(p->screen.y + p->pos.z, 0.f, 0) - zBias);
}
}  // namespace

// ------------------------------------------------------------------------------------- Particle

Particle::~Particle() { sprite = Render::RemoveSprite(sprite); }

void Particle::Init(Render::Texture* tex, const vec3& p, float r, const vec3& v, float rs, float sc) {
    if (tex) {
        sprite = Render::CreateSprite(tex, 8, false, false);
        Render::SetShaderType(sprite, 8);
    }
    rot = r;
    pos = p;
    rotSpeed = rs;
    scale = sc;
    vel = v;
    screen = pos.to2D();
}

void Particle::Update(float dt) {
    rot = rot + dt * rotSpeed;
    pos.z = pos.z + dt * vel.z;
    pos.x = pos.x + dt * vel.x;
    pos.y = pos.y + dt * vel.y;
    age = age + dt;
    screen = pos.to2D();
    if (sprite) {
        PlaceSprite(this, 0.f);
        sprite->color[0] = sprite->x + sprite->w * 0.5f;
        sprite->color[1] = sprite->h * 0.5f - sprite->y;
        sprite->color[2] = rot;
    }
    if (resort) {
        Render::SortRenderLayer(8, 1);
        resort = false;
    }
    if (0.f < life && life < age) dead = true;
}

// ---------------------------------------------------------------------------------- the kinds

HitParticle::HitParticle(const vec3& p, float r, const vec3& v, float rs, float sc) {
    Render::Texture* t = Resources::GetDirectImage("images/particles/fx_block.png");
    Init(t, p, r, v, rs, sc);
    if (sprite && sprite->tex) Render::SetMagnifyFilter(sprite->tex, false);
}

// It grows; after 0.2 s it fades in 0.05 s.
void HitParticle::Update(float dt) {
    Particle::Update(dt);
    scale = scale + dt * 10.f;
    if (age > 0.2f && sprite) {
        Render::SetAlpha(sprite, 1.f - (age - 0.2f) / 0.05f);
        if (sprite->alpha[0] < 0.f) dead = true;
    }
}

HumanParticle::HumanParticle(const vec3& p, float r, const vec3& v, float rs, float sc) {
    const char* image;
    switch (Rand48::lrand48() % 5) {
    case 0: image = "images/particles/fx_human_crit_ear.png"; break;
    case 1: image = "images/particles/fx_human_crit_nose.png"; break;
    case 2:
    case 3: image = "images/particles/fx_human_crit_teeth.png"; break;
    default: image = "images/particles/fx_human_crit_eye.png"; break;
    }
    Init(Resources::GetDirectImage(image), p, r, v, rs, sc * 0.8f);
    bounces = 0;
    restTime = 0.f;
}

// Time runs 1.2 times as fast; gravity 9.8 * 40; each landing halves the speed and bounces (four
// times); then it lies still and fades in a second.
void HumanParticle::Update(float dt) {
    const float t = dt * 1.2f;
    Particle::Update(t);
    if (bounces < 4) {
        if (pos.z < 0.f) {
            pos.z = 0.f;
            vel = vel * 0.5f;
            restTime = age;
            ++bounces;
            vel.z = -vel.z;
        } else {
            vel.z = vel.z + t * 9.8f * -40.f;
        }
        return;
    }
    rotSpeed = 0.f;
    vel = vec3(0.f, 0.f, 0.f);
    Render::SetAlpha(sprite, 1.f - (age - restTime));
    if (sprite->alpha[0] < 0.f) dead = true;
}

SparkleParticle::SparkleParticle(const char* image, const vec3& p, float r, const vec3& v, float rs, float sc,
                                 const vec3& c) {
    Init(Resources::GetDirectImage(image), p, r, v, rs, sc);
    color = c;
    if (sprite && sprite->tex) Render::SetMagnifyFilter(sprite->tex, false);
}

// Drawn tinted (shader 11) just in front of its spot; falls (9.8 * 10), bounces off the ground,
// fades in 0.5 s, shrinks after 0.5 s, gone after 0.75 s.
void SparkleParticle::Update(float dt) {
    rot = rot + dt * rotSpeed;
    age = age + dt;
    pos = vel * dt + pos;
    screen = pos.to2D();
    if (sprite) {
        PlaceSprite(this, 0.002f);
        Render::SetShaderType(sprite, 0xb);
        Render::SetColor(sprite, color.x, color.y, color.z);
    }
    if (resort) resort = false;
    vel.z = vel.z + dt * 9.8f * -10.f;
    if (sprite) Render::SetAlpha(sprite, 1.f + age * -2.f);
    if (age > 0.5f) scale = scale + dt * -0.25f;
    if (pos.z < 0.f && vel.z < 0.f) vel.z = -vel.z;
    if (age > 0.75f) dead = true;
}

FurParticle::FurParticle(const vec3& p, float r, const vec3& v, float rs, float sc) {
    Init(Resources::GetDirectImage("images/particles/fx_fur.png"), p, r, v, rs, sc * 0.8f);
}

// Falls (9.8 * 30); on the ground it stops, lies on layer 3 and after 120 s fades in a second.
void FurParticle::Update(float dt) {
    const float t = dt * 1.2f;
    Particle::Update(t);
    if (pos.z >= 0.f) {
        vel.z = vel.z + t * 9.8f * -30.f;
        return;
    }
    rotSpeed = 0.f;
    vel = vec3(0.f, 0.f, 0.f);
    if (age > 120.f) {
        Render::SetAlpha(sprite, 1.f - (age - 120.f));
        if (sprite->alpha[0] < 0.f) dead = true;
    } else {
        Render::ChangeLayer(sprite, 3);
    }
}

BloodParticle::BloodParticle(bool fly, const vec3& p, float r, const vec3& v, float rs, float sc) {
    Render::Texture* t = Resources::GetDirectImage(fly ? "images/particles/green_blood_flying.png"
                                                       : "images/particles/green_blood_ground.png");
    Init(t, p, r, v, rs, sc * 0.8f);
    flying = fly;
}

// A flying drop falls (9.8 * 30) and on the ground becomes a splat there; a splat lies on layer 3
// and after 120 s fades in 2 s.
void BloodParticle::Update(float dt) {
    const float t = dt * 1.2f;
    Particle::Update(t);
    if (flying) {
        vel.z = vel.z + t * 9.8f * -30.f;
        if (pos.z < 0.f) {
            dead = true;
            AddParticle(new BloodParticle(false, pos, rot, vec3(0.f, 0.f, 0.f), 0.f, 1.f), false);
        }
        return;
    }
    if (age > 120.f) {
        Render::SetAlpha(sprite, 1.f + (age - 120.f) * -0.5f);
        if (sprite->alpha[0] < 0.f) dead = true;
    } else {
        Render::ChangeLayer(sprite, 3);
    }
}

BoneParticle::BoneParticle(const vec3& p, float r, const vec3& v, float rs, float sc) {
    Init(Resources::GetDirectImage("images/particles/fx_bone_crit.png"), p, r, v, rs, sc * 0.8f);
    state = 1;
    restTime = 0.f;
}

// In the air (state 1) it falls (9.8 * 30); the air slows it (1.1 per second); the first landing
// bounces, the second stops it; resting it lies on layer 3 and after 120 s fades in a second.
void BoneParticle::Update(float dt) {
    const float t = dt * 1.2f;
    Particle::Update(t);
    if (state <= 1) vel.z = vel.z + t * 9.8f * -30.f;
    vel = vel - (vel * 1.1f) * t;
    if (restTime > 0.f) {
        if (age > restTime + 120.f) {
            Render::SetAlpha(sprite, 1.f - ((age - 120.f) - restTime));
            if (sprite->alpha[0] < 0.f) dead = true;
        } else {
            Render::ChangeLayer(sprite, 3);
        }
    }
    if (pos.z >= 0.f) return;
    if (state == 0) {
        vel.z = -vel.z;
    } else if (state == 1) {
        pos.z = 0.f;
        vel = vec3(0.f, 0.f, 0.f);
        rotSpeed = 0.f;
        restTime = age;
    }
    ++state;
}

SimpleSpellParticle::SimpleSpellParticle(const vec3& p, const char* name, float fps, float d, bool additive,
                                         float sh, float sc) {
    char path[256];
    std::snprintf(path, sizeof path, "images/%s", name);
    frames = Resources::GetFrameCount(path);
    Render::Texture* tex = Resources::GetImage(path);
    exploded = false;
    if (!tex) {
        // The frames as separate files ("<path>_%d.jpg" under the exploded path): 5.11's data has none
        // (Resources.cpp), so the particle ends at once.
        exploded = true;
        frame = 0;
        dead = true;
        frameTime = 1.f / fps;
        duration = frameTime * (float)frames;
    } else {
        frame = 0;
        frameTime = 1.f / fps;
        duration = frameTime * (float)frames;
        sprite = Render::CreateSprite(tex, additive ? 0xb : 8, false, false);
        Render::SetFrame(sprite, (float)(tex->h / frames), (float)tex->w, 0);
        Render::SetShaderType(sprite, 8);
        sprite->w = sprite->w * sc;
        sprite->h = sprite->h * sc;
        if (1.f < sc) Render::SetMagnifyFilter(tex, false);
    }
    rot = 0.f;
    pos = p;
    zStop = 0.f;
    delay = d;
    spriteScale = sc;
    shake = sh;
    rotSpeed = 0.f;
    vel = vec3(0.f, 0.f, 0.f);
    scale = 1.f;
    screen = pos.to2D();
}

// Hidden until its delay runs out (then the camera shakes for a strong one); it moves, plays its
// frames once and ends (a moving one when it sinks below zStop).
void SimpleSpellParticle::Update(float dt) {
    if (Render::Sprite* s = sprite) {
        if (!exploded) Render::SetFrame(s, (float)(s->tex->h / frames), (float)s->tex->w, frame);
        s->w = spriteScale * s->w;
        s->h = spriteScale * s->h;
        const float y = screen.y + s->h * 0.5f;
        Render::SetPosition(s, screen.x + s->w * -0.5f, y, Map::GetSpriteZ(y, 0.f, 0));
        s->color[0] = s->x + s->w * 0.5f;
        s->color[1] = s->h * 0.5f - s->y;
        s->color[2] = rot;
        Render::SetVisibility(s, delay <= 0.f);
    }
    if (resort) {
        Render::SortRenderLayer(8, 1);
        resort = false;
    }
    const float before = delay;
    delay = before - dt;
    if (delay > 0.f) return;
    if (0.f < before && 1.f < shake) Map::AddCameraShake(0.3f, shake);
    pos.x = pos.x + dt * vel.x;
    pos.y = pos.y + dt * vel.y;
    pos.z = pos.z + dt * vel.z;
    age = age + dt;
    screen = pos.to2D();
    if (0.f < vel.length() && pos.z < zStop) dead = true;
    if (!exploded) {
        if (frameTime < age) {
            ++frame;
            age = age - frameTime;
        }
        if (frames <= frame) dead = true;
    } else if (duration < age - delay) {
        dead = true;
    }
}

// ---------------------------------------------------------------------------------- the list

void Init() {}

void Deinit() { ClearParticles(); }   // @0x1dd2cc: the same as ClearParticles

void Update(float dt) {
    const size_t n = g_particles.size();
    for (size_t i = 0; i < n; ++i) g_particles[i]->Update(dt);
    for (size_t i = 0; i < g_particles.size();) {
        Particle* p = g_particles[i];
        if (!p->dead) {
            ++i;
            continue;
        }
        if (p->handle != 0) g_completed.push_back(p->handle);
        delete p;
        g_particles[i] = g_particles.back();
        g_particles.pop_back();
    }
}

uint32_t AddParticle(Particle* p, bool tracked) {
    p->handle = tracked ? ++g_lastHandle : 0;
    g_particles.push_back(p);
    p->Update(0.f);
    return p->handle;
}

bool IsCompleted(uint32_t handle, bool remove) {
    for (size_t i = 0; i < g_completed.size(); ++i) {
        if (g_completed[i] != handle) continue;
        if (remove) {
            g_completed[i] = g_completed.back();
            g_completed.pop_back();
        }
        return true;
    }
    return false;
}

void ClearParticles() {
    for (Particle* p : g_particles) delete p;
    g_particles.clear();
}

}  // namespace Particles
