// Projectile: an archer's arrow. It flies from its owner to the target on a shallow arc; when it
// lands the target's AI is told what the shot did (AIBaseState::ProjectileLanded). Port of
// Projectile (@0x1ea270..0x1ea9d4, 0x6c bytes) and ProjectileData (projectiles.xml,
// @0x1eaeec..0x1eafd8).
#pragma once
#include <cstdint>

namespace Render { struct Sprite; struct Texture; }
struct Animation;
class Entity;

namespace ProjectileData {
struct ProjectileInfo {          // 0x14 bytes
    uint32_t type = 0;           // +0x00 "type"
    float speed = 0.f;           // +0x04 "speed" world units per second
    float fps = 0.f;             // +0x08 "fps"
    float trajectory = 0.f;      // +0x0c "trajectory" the height of the arc (a share of the distance)
    Animation* anim = nullptr;   // +0x10 "anims" path|name
};
bool Init(const char* file);                    // @0x1eafd8
ProjectileInfo* GetProjectileInfo(uint32_t type);   // @0x1eaeec
}

class Projectile {
public:
    // @0x1ea9d4: kind is what the target's AI gets when it lands (0 hit, 1 death, 2 block).
    Projectile(uint32_t type, int kind);
    ~Projectile();                               // (0x1ea8f4)
    bool HasLanded() const { return landed; }    // @0x1ea270
    void SetOwner(Entity* e);                    // @0x1ea4d4 it starts at the middle of e's sprite
    void MoveTo(Entity* target);                 // @0x1ea714 to the middle of the target's sprite
    void MoveTo(int x, int y, int unused);       // @0x1ea554
    void Update(float dt);                       // @0x1ea794
    void UpdateMovement(float dt);               // @0x1ea400
    void UpdateGraphics();                       // @0x1ea27c

    Render::Sprite* sprite = nullptr;   // +0x04
    float startX = 0.f, startY = 0.f;   // +0x08 +0x0c
    float f10 = 0.f, f14 = 0.f;         // +0x10 +0x14 (the start again)
    float distance = 0.f;               // +0x18
    float slope = 0.f;                  // +0x1c
    float trajectory = 0.f;             // +0x20
    float x = 0.f, y = 0.f;             // +0x24 +0x28
    float ownerTileX = 0.f, ownerTileY = 0.f;   // +0x2c +0x30
    float targetX = 0.f, targetY = 0.f; // +0x34 +0x38
    float speed = 1.f;                  // +0x3c
    float angle = 0.f;                  // +0x40
    float fps = 0.f;                    // +0x44
    uint32_t type = 0;                  // +0x48
    Entity* owner = nullptr;            // +0x4c
    void* ownerBuilding = nullptr;      // +0x50 (the towers' shots: dead in 5.11)
    Entity* target = nullptr;           // +0x54
    Animation* anim = nullptr;          // +0x5c
    int kind = 0;                       // +0x60
    // +0x64 BaseAnimController<Projectile> (0x1c bytes): the sprite's frame animation.
    int frame = 0, shownFrame = -1;     // its +0x04 +0x08
    Render::Texture* tex = nullptr;     // its +0x0c
    float frameAcc = 0.f;               // its +0x14
    bool landed = false;                // +0x68
};
