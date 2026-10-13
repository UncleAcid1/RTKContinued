#include "game/Projectile.h"

#include <cmath>
#include <cstdio>
#include <vector>

#include <pugixml.hpp>

#include "engine/FileManager.h"
#include "engine/Render.h"
#include "engine/Splitter.h"
#include "engine/Timer.h"
#include "game/AI.h"
#include "game/AIState.h"
#include "game/Animation.h"
#include "game/Entity.h"

namespace ProjectileData {

namespace {
std::vector<ProjectileInfo*> g_infos;   // 0x613f34
}

ProjectileInfo* GetProjectileInfo(uint32_t type) {
    for (ProjectileInfo* p : g_infos)
        if (p->type == type) return p;
    return nullptr;
}

bool Init(const char* file) {
    double t0 = Timer::GetTime();
    uint32_t size = 0;
    uint8_t* buf = FileManager::LoadFile(file, size);
    if (!buf) {
        std::printf("Projectiles::Init() Failed to load projectile list (File not found: %s)\n", file);
        return false;
    }
    pugi::xml_document doc;
    pugi::xml_parse_result r = doc.load_buffer_inplace(buf, size, pugi::parse_default, pugi::encoding_auto);
    if (!r) {
        std::printf("Projectiles::Init() Failed to load projectile list (%s)\n", r.description());
        FileManager::FreeFile(buf);
        return false;
    }
    pugi::xml_node root = doc.child("Projectiles");
    if (!root) {
        std::printf("Projectiles::Init() Incorrect file format (%s)\n", file);
        FileManager::FreeFile(buf);
        return false;
    }
    g_infos.reserve(0x10);
    for (pugi::xml_node x = root.child("Projectile"); x; x = x.next_sibling("Projectile")) {
        ProjectileInfo* p = new ProjectileInfo();
        g_infos.push_back(p);
        p->type = x.attribute("type").as_uint();
        // "anims": the image path and the animation's name.
        std::vector<std::string> parts = SplitterParse(x.attribute("anims").value(), "|");
        Animation* a = new Animation();
        a->path = parts[0];
        a->name = parts.size() > 1 ? parts[1] : "";
        a->fps = x.attribute("fps").as_float();
        a->loop = false;
        p->fps = x.attribute("fps").as_float();
        p->trajectory = x.attribute("trajectory").as_float();
        p->anim = a;
        p->speed = x.attribute("speed").as_float();
    }
    std::printf("Projectiles::Init() Loaded projectile list in %.3fms\n", (Timer::GetTime() - t0) * 1000.0);
    FileManager::FreeFile(buf);
    return true;
}

}  // namespace ProjectileData

Projectile::Projectile(uint32_t type_, int kind_) : type(type_), kind(kind_) {
    ProjectileData::ProjectileInfo* info = ProjectileData::GetProjectileInfo(type);
    if (!info) {
        std::printf("ERROR: couldn't find projectile info for %d projectile type\n", (int)type);
        return;
    }
    speed = info->speed;
    fps = info->fps;
    anim = info->anim;
    trajectory = info->trajectory;
}

Projectile::~Projectile() { Render::RemoveSprite(sprite); }

void Projectile::SetOwner(Entity* e) {
    x = e->worldX;
    owner = e;
    float sy = e->worldY + e->GetSprite()->h * -0.5f;
    slope = 0.f;
    ownerTileX = (float)e->tileX;
    ownerTileY = (float)e->tileY;
    f10 = x;
    startX = x;
    distance = 0.f;
    f14 = sy;
    y = sy;
    startY = sy;
}

void Projectile::MoveTo(Entity* t) {
    if (!anim || landed) return;
    target = t;
    MoveTo((int)t->worldX, (int)(t->worldY + t->GetSprite()->h * -0.5f), t->tileX);
}

void Projectile::MoveTo(int tx, int ty, int) {
    if (!anim || landed) return;
    targetX = (float)tx;
    targetY = (float)ty;
    if (!sprite) {
        if (!anim->loaded) anim->Load();
        Render::Texture* t = anim->GetTexByDir(0);
        // (Animation::GetAvailableDirTex: any direction's texture when direction 0 has none.)
        for (int d = 0; !t && d < 8; ++d) t = anim->tex[d];
        if (!t) return;
        t->frameTime = 1.f / fps;
        sprite = Render::CreateSprite(t, 0xb, false, false);
        // Shader 8 turns the sprite about its centre (colour slots: the centre and the angle).
        Render::SetShaderType(sprite, 8);
        sprite->color[0] = sprite->x + sprite->w * 0.5f;
        sprite->color[1] = sprite->h * 0.5f - sprite->y;
        angle = AI::GetRadianBetweenPoints(x, y, targetX, targetY) + 1.57f;
    }
    float dy = targetY - startY, dx = targetX - startX;
    distance = std::sqrt(dy * dy + dx * dx);
    slope = (dx * -2.f * trajectory) / distance;
    if (slope > 0.f) slope = -slope;
    Render::SetPosition(sprite, x, y, 1.f);
    frameAcc = 0.f;
    frame = 0;
    shownFrame = -1;
    tex = sprite->tex;
}

void Projectile::Update(float dt) {
    if (landed) return;
    if (tex) {
        float step = dt <= 8.f ? dt : 8.f;
        if (sprite) {
            if (tex != sprite->tex) {
                Render::SetTexture(sprite, tex);
                shownFrame = -1;
            }
            if (shownFrame != frame)
                Render::SetFrame(sprite, (float)Render::GetFrameHeight(tex), (float)Render::GetFrameWidth(tex), frame);
            shownFrame = frame;
        }
        frameAcc += step;
        while (frameAcc > 0.f) {
            if (++frame >= tex->frames) frame = 0;
            frameAcc += 1.5f / (-1.f / tex->frameTime);
        }
    }
    UpdateMovement(dt);
    UpdateGraphics();
}

void Projectile::UpdateMovement(float dt) {
    if (!sprite || landed) return;
    float dy = targetY - y, dx = targetX - x;
    float left = std::sqrt(dy * dy + dx * dx);
    float step = dt * speed;
    if (step <= left) {
        y += (dy / left) * step;
        x += (dx / left) * step;
        return;
    }
    x = targetX;
    y = targetY;
    landed = true;
    if (target) target->GetAI()->ProjectileLanded(kind);
    // (A tower's shot: Map::Building::ProjectileLanded, dead in 5.11.)
    if (owner) owner->ProjectileLanded();
}

void Projectile::UpdateGraphics() {
    // How far along it is (0..1), by x unless it flies straight up or down.
    float t;
    float span = targetX - startX;
    if (span > 0.01f || span < -0.01f) {
        t = (x - startX) / span;
    } else {
        span = targetY - startY;
        t = (span > 0.01f || span < -0.01f) ? (y - startY) / span : 0.f;
    }
    t -= 0.5f;
    float tilt = std::atan(t * slope);
    if (targetX < startX) tilt = 6.28f - tilt;
    Render::SetPosition(sprite, x + sprite->w * -0.5f,
                        ((y - 21.f) - (0.25f - t * t) * distance * trajectory) + sprite->h * 0.5f, 1.f);
    if (angle != 0.f) {
        Render::SetShaderType(sprite, 8);
        sprite->color[0] = sprite->x + sprite->w * 0.5f;
        sprite->color[1] = sprite->h * 0.5f - sprite->y;
        sprite->color[2] = tilt - angle;
    }
}
