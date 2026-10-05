#include "game/Animation.h"

#include <cstdio>
#include <cstring>

#include "engine/Render.h"
#include "engine/Resources.h"
#include "game/AIState.h"
#include "game/Entity.h"

namespace {

const char* const kDirSuffix[8] = {"_s", "_se", "_e", "_ne", "_n", "_ne", "_e", "_se"};   // 0x601268

// Animation::Load: a missing direction borrows a neighbour's texture (both passes are the same).
void FillMissingDirections(Render::Texture** t) {
    Render::Texture* t4 = t[4] ? t[4] : t[3];
    if (!t[4]) t[4] = t[3];
    Render::Texture* t7 = t[7];
    Render::Texture* t6 = t[6] ? t[6] : t[5];
    if (!t[6]) t[6] = t[5];
    Render::Texture* t1 = t[1];
    Render::Texture* t0 = t[0];
    if (!t[0]) t[0] = t0 = t7;
    Render::Texture* t2 = t[2];
    if (!t[2]) t[2] = t2 = t1;
    Render::Texture* t3 = t[3];
    Render::Texture* t5 = t[5];
    if (!t7) t[7] = t5;
    if (!t0) t[0] = t4;
    if (!t1) t[1] = t3;
    if (!t2) t[2] = t6;
    if (!t5) t[5] = t[7];
    if (!t4) t[4] = t[0];
    if (!t3) t[3] = t[1];
    if (!t6) t[6] = t[2];
}

}  // namespace

void Animation::Load() {
    char path[256];
    std::snprintf(path, sizeof path, "images/%s", this->path.c_str());
    Render::Texture* t = Resources::GetImage(path);
    // UNVERIFIED: animations named "l", "b", "m" or "i" whose image is missing fall back to exploded
    // frames ("%s_%d.jpg" under GetExplodedAnimationImagePath, 1/15 s each, chained through
    // Texture+0x14); the chain is not ported.
    if (!t) {
        for (int d = 0; d < 8; ++d) {
            std::snprintf(path, sizeof path, "images/%s%s", this->path.c_str(), kDirSuffix[d]);
            Render::Texture* dt = Resources::GetImage(path);
            if (dt && frameCount == 0) frameCount = Resources::GetFrameCount(path);
            tex[d] = dt;
        }
        FillMissingDirections(tex);
        for (int d = 0; d < 8; ++d) {
            if (tex[d]) continue;
            std::fprintf(stderr, "ERROR: Animation::Load() failed to find animation %s", path);
            for (int i = 0; i < 8; ++i) {
                if (name == "idle") {
                    // Render::GetCharPlaceholderDebugTex @0x1f8784
                    Render::Texture* ph = Resources::GetImage("images/Buildings/dev/monster_orc");
                    if (ph && frameCount == 0) frameCount = 1;
                    tex[i] = ph;
                } else {
                    tex[i] = nullptr;
                }
            }
            FillMissingDirections(tex);
        }
    } else {
        tex[0] = t;
        frameCount = Resources::GetFrameCount(path);
        if (frameCount == 0) frameCount = 1;
        single = true;
    }
    loaded = true;
}

Render::Texture* Animation::GetTexByDir(int dir) const {
    if ((unsigned)dir > 7) {
        std::printf("!!!!!!!!!!!!!ERROR: Animation::GetTexByDir() incorrect direction: %d\n", dir);
        return tex[0];
    }
    return single ? tex[0] : tex[dir];
}

void Animation::GetOffsets(int dir, int& x, int& y) const {
    if (dir == 0 || dir == 1 || dir == 2 || dir == 7) {
        x = offX;
        y = offY;
    } else if (dir >= 3 && dir <= 6) {
        x = off2X;
        y = off2Y;
    } else {
        x = y = 0;
    }
}

float Animation::GetDelay(bool first) const {
    return (1.5f / fps) * (float)(first ? delay[0] : delay[1]);
}

AnimationController::AnimationController(Entity* owner) : owner(owner) {}

void AnimationController::SetAnim(Animation* a, int frame, int playCount, bool paused, bool holdLast,
                                  bool notify, bool bounce) {
    if (anim && this->playCount != 1) prev = anim;
    this->paused = paused;
    acc = 0.f;
    this->bounce = bounce;
    step = 1;
    notify_ = notify;
    this->holdLast = holdLast;
    anim = a;
    this->frame = frame;
    this->playCount = playCount;
}

void AnimationController::SetCurrentFrame(int f) {
    if (!anim || f > anim->frameCount) return;
    if (f < anim->frameCount) frame = f;
}

void AnimationController::SetFrame(int f) {
    frame = f == -1 ? anim->frameCount - 1 : f;
}

void AnimationController::SetAnimFrameRange(int first, int last) {
    this->first = first;
    this->last = last;
    frame = first == -1 ? 0 : first;
}

void AnimationController::Update(float dt) {
    if (paused) return;
    acc += dt;
    while (acc > 0.f) {
        Animation* a = anim;
        int cur = frame, st = step;
        int end = last != -1 ? last : a->frameCount;
        frame = cur + st;
        acc += (-1.5f / a->fps) * mult;
        if (bounce && st < 0) end = first == -1 ? 0 : first;
        if (end != cur + st) continue;

        int n;
        if (!holdLast) {
            if (!bounce) {
                frame = first == -1 ? 0 : first;
                n = playCount;
                if (n > 1) {
                    playCount = n - 1;
                    continue;
                }
            } else {
                if (st > 0) frame = (last == -1 ? a->frameCount : last) - 1;
                n = playCount;
                step = -st;
                if (n >= 2) {
                    playCount = n - 1;
                    continue;
                }
            }
        } else {
            paused = true;
            frame = a->frameCount - 1;
            n = playCount;
            if (n > 1) {
                playCount = n - 1;
                continue;
            }
        }
        if (n == -1) continue;
        if (prev && !holdLast) {
            anim = prev;
            acc = 0.f;
            frame = 0;
            prev = nullptr;
            playCount = -1;
        }
        if (!notify_) continue;
        owner->GetAI()->AnimEnded();   // AI vtable +0xc8
        owner->AnimationEnded();
    }
}
