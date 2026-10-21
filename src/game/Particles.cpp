// Port of Particles and vec3 (libkingdom.so 5.11), see Particles.h.
#include "game/Particles.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "engine/Render.h"
#include "engine/Image.h"
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

float lerp(float t, float a, float b) { return a + (b - a) * t; }

float randf() { return (float)(Rand48::lrand48() % 1000) * 0.001f; }

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

// ------------------------------------------------------------------------- the image systems

ImageParticleSystem::ImageParticleSystem(const vec3& p, const char* path, int frames, int frame, bool mirror,
                                         unsigned m) {
    count = 0;
    dots = nullptr;
    mode = m;
    // The dot: 5x5, brightest in the middle (255 - distance * 85 in every channel).
    uint8_t dot[5 * 5 * 4];
    for (unsigned row = 0; row < 5; ++row) {
        for (unsigned col = 0; col < 5; ++col) {
            const float d = vec3((float)col - 2.f, (float)row - 2.f, 0.f).length();
            const uint8_t b = (uint8_t)~(uint8_t)(int)(d * 85.f);
            uint8_t* px = dot + (row * 5 + col) * 4;
            px[0] = px[1] = px[2] = px[3] = b;
        }
    }
    Render::Texture* dotTex = Render::CreateCustomTexture(dot, 5, 5);
    Render::SetMagnifyFilter(dotTex, false);
    unsigned bpp = 0;
    ImageRGBA img = Resources::GetImageData(path, bpp);
    if (!img.ok() || bpp != 32) return;
    const unsigned w = (unsigned)img.w;
    const unsigned frameH = (unsigned)img.h / (unsigned)frames;
    count = (w >> 1) * (frameH >> 1);
    dots = new Dot[count];
    std::memset(dots, 0, count * sizeof(Dot));
    const unsigned rowStart = frameH * (unsigned)frame, rowEnd = (unsigned)frame * frameH + frameH;
    unsigned i = 0;
    for (unsigned row = rowStart; row < rowEnd; row += 2) {
        for (unsigned col = 0; col < w; col += 2) {
            if (i >= count) continue;
            const uint8_t* px = &img.px[(row * w + col) * 4];
            if ((int8_t)px[3] >= 0) {   // alpha below 128: one dot fewer
                --count;
                continue;
            }
            Dot& d = dots[i++];
            d.sprite = Render::CreateSprite(dotTex, 8, false, false);
            Render::SetAlpha(d.sprite, (float)px[3] / 255.f);
            d.color.x = (float)px[0] / 255.f / d.sprite->alpha[0];
            d.color.y = (float)px[1] / 255.f / d.sprite->alpha[0];
            d.color.z = (float)px[2] / 255.f / d.sprite->alpha[0];
            Render::SetColor(d.sprite, d.color.x, d.color.y, d.color.z);
            Render::SetShaderType(d.sprite, 0xb);
            const float x = mirror ? (float)(w - col) : (float)col;
            d.pos = p + vec3(x, 0.f, 0.f);
            // (The original's height: the frame's height less the row less the frame's first row;
            // right for frame 0, the only frame its callers pass.)
            d.pos = vec3::to3D(d.pos.x, d.pos.y, (float)frameH - (float)row - (float)rowStart);
            d.vel = vec3((float)((std::cos((double)((float)col * 0.05f)) + 1.5) * 20.0),
                         (float)((std::sin((double)((float)row * 0.05f)) + 1.5) * -20.0), 0.f);
            d.start = 0.f;
        }
    }
}

ImageParticleSystem::~ImageParticleSystem() {
    for (unsigned i = 0; i < count; ++i) dots[i].sprite = Render::RemoveSprite(dots[i].sprite);
    delete[] dots;
}

// The dots fall (60), the air slows them sideways (0.2 per second), the ground bounces them
// (0.75); gone after 15.5 s.
void ImageParticleSystem::Update(float dt) {
    if (!dots || count == 0) {
        dead = true;
        return;
    }
    for (unsigned i = 0; i < count; ++i) {
        Dot& d = dots[i];
        if (!d.sprite) continue;
        d.vel = d.vel + vec3(0.f, 0.f, -60.f) * dt;
        d.pos = d.pos + d.vel * dt;
        d.vel.y = d.vel.y + d.vel.y * dt * -0.2f;
        d.vel.x = d.vel.x + d.vel.x * dt * -0.2f;
        if (d.pos.z < 0.f) {
            d.pos.z = 0.f;
            d.vel.z = d.vel.z * -0.75f;
        }
        screen = d.pos.to2D();
        Render::Sprite* s = d.sprite;
        Render::SetPosition(s, screen.x + s->w * -0.5f, screen.y + s->h * 0.5f, Map::GetSpriteZ(screen.y, 4.f, 0));
    }
    age = age + dt;
    if (resort) {
        Render::SortRenderLayer(8, 1);
        resort = false;
    }
    if (age > 15.5f) dead = true;
}

DesintegrateImageParticleSystem::DesintegrateImageParticleSystem(const vec3& p, const char* path, float zz, int frames,
                                                                 int frame, bool mirror, unsigned m)
    : ImageParticleSystem(p, path, frames, frame, mirror, 0) {
    for (unsigned i = 0; i < count; ++i) {
        Dot& d = dots[i];
        if (m == 2) {
            const float x = randf() * 75.f;
            const float y = randf() * 75.f;
            const float z2 = randf() * 75.f;
            d.vel = vec3(x, y, z2);
        } else {
            d.vel = vec3(0.f, 0.f, 0.f);
        }
        d.start = randf() * 0.25f;
    }
    z = zz;
    mode = m;
}

DesintegrateImageParticleSystem::~DesintegrateImageParticleSystem() { Render::SortRenderLayer(8, 1); }

// Time runs 1.25 times as fast. Mode 3: once started each dot swirls (200 at an angle turning
// 20 per second) and sinks back (40), tinted towards blue then grey, fading from 2.5 s; gone at
// 3 s. Mode 2: they burst (their start sets the push), tinted to white, fading from 1 s; gone at
// 1.5 s. Others: once started they rise (20), tinted red then dark, fading from 1 s (at once
// below the ground); gone at 2 s.
void DesintegrateImageParticleSystem::Update(float dt) {
    if (!dots || count == 0) {
        dead = true;
        return;
    }
    const float t = dt * 1.25f;
    for (unsigned i = 0; i < count; ++i) {
        Dot& d = dots[i];
        Render::Sprite* s = d.sprite;
        if (!s) continue;
        if (mode == 3) {
            if (age > d.start && d.pos.z > 0.f) {
                const double a = (double)((age + d.start) * 20.f);
                const double c = std::cos(a), sn = std::sin(a);
                d.vel = d.vel + vec3((float)(c * 200.0), (float)(sn * 200.0), -40.f) * t;
            }
            d.pos = d.pos + d.vel * t;
            if (d.pos.z < 0.f) d.pos.z = 0.f;
            if (age < 1.f) {
                s->color[0] = lerp(age, d.color.x, d.color.x * 0.6f);
                s->color[1] = lerp(age, d.color.y, d.color.y * 1.4f);
                s->color[2] = lerp(age, d.color.z, d.color.z + d.color.z);
            } else {
                s->color[0] = lerp((age - 1.f) * 0.5f, d.color.x * 0.6f, 1.f - d.start);
                s->color[1] = lerp((age - 1.f) * 0.5f, d.color.y * 1.4f, 1.f - d.start);
                s->color[2] = lerp((age - 1.f) * 0.5f, d.color.z + d.color.z, 1.f - d.start);
            }
            if (age > 2.5f) Render::SetAlpha(s, 1.f + (age - 2.5f) * -2.f);
        } else if (mode == 2) {
            const float k = (d.start - 0.125f) * 100.f;
            d.vel = d.vel + vec3(k, k, k) * t;
            d.pos = d.pos + d.vel * t;
            if (d.pos.z < 0.f) d.vel.z = -d.vel.z;
            s->color[0] = lerp(age * 0.5f, d.color.x, 1.5f);
            s->color[1] = lerp(age * 0.5f, d.color.y, 1.5f);
            s->color[2] = lerp(age * 0.5f, d.color.z, 2.f);
            if (age > 1.f) Render::SetAlpha(s, 1.f + (age - 1.f) * -2.f);
        } else {
            if (age > d.start) d.vel = d.vel + vec3(0.f, 0.f, 20.f) * t;
            d.pos = d.pos + d.vel * t;
            if (age < 1.f) {
                s->color[0] = lerp(age, d.color.x, d.color.x + d.color.x);
                s->color[1] = lerp(age, d.color.y, d.color.y * 1.4f);
                s->color[2] = lerp(age, d.color.z, d.color.z * 0.6f);
            } else {
                s->color[0] = lerp(age - 1.f, d.color.x + d.color.x, d.color.x * 0.2f);
                s->color[1] = lerp(age - 1.f, d.color.y * 1.4f, d.color.y * 0.2f);
                s->color[2] = lerp(age - 1.f, d.color.z * 0.6f, d.color.z * 0.2f);
            }
            if (d.pos.z < 0.f) Render::SetAlpha(s, 0.f);
            else if (age > 1.f) Render::SetAlpha(s, 1.f - (age - 1.f));
        }
        screen = d.pos.to2D();
        Render::SetPosition(s, screen.x + s->w * -0.5f, screen.y + s->h * 0.5f, z);
    }
    age = age + t;
    if (resort) {
        Render::SortRenderLayer(8, 1);
        resort = false;
    }
    const float end = mode == 3 ? 3.f : mode == 2 ? 1.5f : 2.f;
    if (age > end) dead = true;
}

// ---------------------------------------------------------------------------- the buff glow

BuffParticleSystem::BuffParticleSystem(const vec3& p, const vec3& c) {
    Init(Resources::GetDirectImage("images/particles/center_64.png"), p, 0.f, vec3(0.f, 0.f, 0.f), 0.f, 2.f);
    spawnTime = 0.f;
    color = c;
    if (sprite && sprite->tex) Render::SetMagnifyFilter(sprite->tex, false);
}

// Every 0.3 s ten clouds puff out round the glow (half transparent, 0.4-0.6 s, smaller as the
// glow ages); the glow fades in and out over its second (alpha (1 - sqrt(1 - t^2)) / 2, then
// sqrt(1 - t^2) / 2).
void BuffParticleSystem::Update(float dt) {
    spawnTime = spawnTime + dt;
    while (spawnTime > 0.3f) {
        for (int i = 0; i < 10; ++i) {
            const float a = randf() - 0.5f;
            const float b = randf() - 0.5f;
            const float c = randf() - 0.5f;
            const vec3 at = pos + vec3(a, b, c) * 10.f;
            const float e = randf() - 0.5f;
            const float f = randf() - 0.5f;
            const vec3 off = vec3(e, f, 0.f) * 20.f;
            const vec3 dir = (pos - at).normalized();
            const vec3 v = off + dir * (randf() * 75.f);
            Particle* cloud = new Particle();
            Render::Texture* tex = Resources::GetDirectImage("images/particles/cloud_64.png");
            const float spin = randf();
            const float sc = (1.f - age) * 0.8f + randf() * 0.2f;
            cloud->Init(tex, at, 0.f, v, spin, sc);
            cloud->life = 0.4f + randf() * 0.2f;
            Render::SetAlpha(cloud->sprite, 0.5f);
            AddParticle(cloud, false);
        }
        spawnTime = spawnTime - 0.3f;
    }
    Particle::Update(dt);
    if (sprite) {
        if (age < 0.5f) {
            const float k = age + age;
            const double r = std::sqrt((double)(1.f - k * k));
            Render::SetAlpha(sprite, (float)((r - 1.0) * -0.5 + 0.0));
        } else {
            const float k = (age - 0.5f) + (age - 0.5f);
            const double r = std::sqrt((double)(1.f - k * k));
            Render::SetAlpha(sprite, (float)(0.5 + (r - 1.0) * 0.5));
        }
    }
    if (age > 1.f) dead = true;
}

// ------------------------------------------------------------------------------- the mage orb

MageParticle::MageParticle(const vec3& p, const vec3& c, const vec3& tc) {
    Init(Resources::GetDirectImage("images/mage/big_glow.png"), p, 0.f, vec3(0.f, 0.f, 0.f), 0.f, 0.4f);
    color = c;
    trailColor = tc;
    if (sprite && sprite->tex) Render::SetMagnifyFilter(sprite->tex, false);
    flying = false;
}

namespace {
// The trail's sparkle (images/mage/trail.png, an eighth to a quarter, halved).
void AddTrail(const vec3& at, const vec3& v, const vec3& c) {
    const float s = randf();
    AddParticle(new SparkleParticle("images/mage/trail.png", at, 0.f, v, 0.f, (0.125f + s * 0.125f) * 0.5f, c), false);
}
}  // namespace

// Following the hand: 15 sparkles scattered between the old and the new spot, drifting the way
// it moved.
void MageParticle::SetPosition(const vec3& p, int frame, int) {
    if (flying) return;
    if (pos.x == p.x && pos.y == p.y && pos.z == p.z) return;
    if (frame != 0) {
        for (int i = 0; i < 15; ++i) {
            const float t = randf();
            vec3 at = pos * t + p * (1.f - t);
            const float a = randf();
            const float b = randf();
            const float c = randf();
            at = at + vec3(a * 10.f - 5.f, b * 10.f - 5.f, c * 10.f - 5.f);
            const float e = randf();
            const float f = randf();
            vec3 v(e * 50.f - 25.f, f * 50.f - 25.f, 0.f);
            v = v + (p - pos).normalized() * (randf() * 50.f);
            AddTrail(at, v, trailColor);
        }
    }
    pos = p;
}

// target: a screen point and its height.
void MageParticle::FlyTo(const vec3& target, float d) {
    if (flying) return;
    duration = d;
    trailTime = 0.f;
    flyTime = 0.f;
    from = pos;
    flying = true;
    to = vec3::to3D(target.x, target.y, target.z);
}

// The glow (shader 11, its colour) just in front of its spot. Flying it moves from `from` to `to`
// laying a sparkle every 0.005 s along the way (scattered less as it nears the end); on arrival
// 25 sparkles burst (every third pushed on along the flight) and it ends. Gone after 3 s anyway.
void MageParticle::Update(float dt) {
    age = age + dt;
    screen = pos.to2D();
    if (Render::Sprite* s = sprite) {
        const float h = scale * (float)s->tex->h;
        const float w = (float)s->tex->w * scale;
        s->h = h;
        s->w = w;
        const float z = Map::GetSpriteZ(screen.y + pos.z, 0.f, 0);
        Render::SetPosition(s, screen.x + w * -0.5f, screen.y + h * 0.5f, z - 0.002f);
        Render::SetShaderType(sprite, 0xb);
        Render::SetColor(sprite, color.x, color.y, color.z);
    }
    if (flying) {
        flyTime = flyTime + dt;
        const float k = flyTime / duration;
        trailTime = trailTime + dt;
        pos = to * k + from * (1.f - k);
        if (trailTime > duration) {
            trailTime = 0.f;
        } else {
            while (trailTime > 0.f) {
                const float m = k - trailTime / duration;
                vec3 at = to * m + from * (1.f - m);
                const float a = randf() - 0.5f;
                const float b = randf() - 0.5f;
                const float c = randf() - 0.5f;
                at = at + vec3(a, b, c) * lerp(m, 40.f, 1.f);
                const float e = randf();
                const float f = randf();
                const float g = randf();
                AddTrail(at, vec3(e * 20.f - 10.f, f * 20.f - 10.f, g * 20.f - 10.f), trailColor);
                trailTime = trailTime - 0.005f;
            }
        }
        if (flyTime > duration) {
            for (unsigned i = 0;;) {
                const float a = randf() - 0.5f;
                const float b = randf() - 0.5f;
                const float c = randf() - 0.5f;
                vec3 v = vec3(a, b, c) * 200.f;
                if (i % 3 == 0) v = v + (to - from).normalized() * 100.f;
                ++i;
                AddTrail(to, v, trailColor);
                if (i == 25) break;
            }
            dead = true;
        }
    }
    if (resort) {
        Render::SortRenderLayer(8, 1);
        resort = false;
    }
    if (age > 3.f) dead = true;
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
