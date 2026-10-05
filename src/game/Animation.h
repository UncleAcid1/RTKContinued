// Animation: one named character animation of an EntityData (persons.xml XGraphics "anims"), with a
// texture per direction, and AnimationController, which plays one on an entity.
// Port of Animation (@0xf935c..0xf9afc) and AnimationController (@0xf9b00..0xf9e60).
//
// Directions 0..7 are S, SE, E, NE, N, then NE, E, SE again (drawn mirrored); a "single" animation
// has one texture for all of them.
#pragma once
#include <string>

namespace Render { struct Texture; }
class Entity;

struct Animation {               // 0x54 bytes
    std::string path;            // +0x00 image path under images/ (field 0)
    std::string name;            // +0x04 (field 5)
    float fps = 12.f;            // +0x08 (field 1)
    bool loop = false;           // +0x0c (field 2 == 1)
    int off2X = 0, off2Y = 0;    // +0x10 +0x14 offsets for directions 3..6 (field 3, values 3 and 4)
    int offX = 0, offY = 0;      // +0x18 +0x1c offsets for directions 0, 1, 2, 7 (field 3, values 1 and 2)
    int delay[2] = {};           // +0x20 +0x24 (field 4 "a,b", each halved), see GetDelay
    int frameCount = 0;          // +0x28
    bool single = false;         // +0x2c one texture for every direction
    bool loaded = false;         // +0x2d
    // +0x30 GlowFilter set (ids 0x285/0x286 only)  UNVERIFIED: glow filters are not ported.
    Render::Texture* tex[8] = {};  // +0x34 per direction

    void Load();                                 // @0xf9550
    Render::Texture* GetTexByDir(int dir) const; // @0xf94f8
    void GetOffsets(int dir, int& x, int& y) const;  // @0xf93ac
    float GetDelay(bool first) const;            // @0xf935c (1.5 / fps) * delay
};

struct AnimationController {     // 0x30 bytes
    explicit AnimationController(Entity* owner);  // @0xf9b00

    // @0xf9be8 SetAnim(anim, frame, playCount, ?, paused, holdLast, notify, bounce)
    void SetAnim(Animation* a, int frame, int playCount, bool paused, bool holdLast, bool notify, bool bounce);
    void SetCurrentFrame(int f);                 // @0xf9bc8
    void SetFrame(int f);                        // @0xf9c68 (-1: the last frame)
    void SetAnimFrameRange(int first, int last); // @0xf9ca8
    void SetPlayCount(int n, bool notify) { notify_ = notify; playCount = n; }  // @0xf9c94
    void Pause() { paused = true; }              // @0xf9c5c
    void Update(float dt);                       // @0xf9cc0

    Animation* anim = nullptr;   // +0x00
    Animation* prev = nullptr;   // +0x04 resumed when a counted animation ends
    int frame = 0;               // +0x08
    int playCount = 1;           // +0x0c (-1: forever)
    Entity* owner = nullptr;     // +0x10
    float acc = 0.f;             // +0x14
    bool paused = false;         // +0x18
    bool holdLast = false;       // +0x19 stop on the last frame
    bool notify_ = true;         // +0x1a tell the AI and the entity when a play ends
    float mult = 1.f;            // +0x1c speed multiplier
    int first = -1, last = -1;   // +0x20 +0x24 frame range (-1: the whole animation)
    bool bounce = true;          // +0x28 play back and forth (loop flag of the caller)
    int step = 1;                // +0x2c +1 or -1
};
