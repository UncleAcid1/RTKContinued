#include "game/HiddenObjects.h"

#include "engine/Render.h"
#include "game/Map.h"

void HiddenObjects::HideObject(Map::Decor* d, float alpha) {
    UpdatedObject& o = objects_[d];
    o.alpha = alpha;
    o.restore = false;
}

void HiddenObjects::ShowAll() {
    for (auto& [d, o] : objects_) o.restore = true;
}

void HiddenObjects::Clear(bool restore) {
    if (restore) {
        for (auto& [d, o] : objects_) {
            if (Render::Sprite* s = d->sprite) {
                Render::SetShaderType(s, 0);
                Render::SetAlpha(s, 1.f);
            }
        }
    }
    objects_.clear();
}

void HiddenObjects::Update(float dt) {
    for (auto it = objects_.begin(); it != objects_.end();) {
        Render::Sprite* s = it->first->sprite;
        if (!s) {
            ++it;
            continue;
        }
        const UpdatedObject& o = it->second;
        float a = s->alpha[0];
        if (!o.restore) {
            Render::SetShaderType(s, 1);
            if (a != o.alpha) {
                float step = dt * 4.f;
                if (a < o.alpha) {
                    a = a + step;
                    if (o.alpha < a) a = o.alpha;
                } else {
                    a = a - step;
                    if (o.alpha > a) a = o.alpha;
                }
            }
            Render::SetAlpha(s, a);
            ++it;
            continue;
        }
        if (a != 1.f) {
            float step = dt * 4.f;
            if (a < 1.f) {
                a = a + step;
                if (a > 1.f) a = 1.f;
            } else {
                a = a - step;
                if (a < 1.f) a = 1.f;
            }
        }
        Render::SetAlpha(s, a);
        Render::SetShaderType(s, a != 1.f ? 1 : 0);
        if (a == 1.f) it = objects_.erase(it);
        else ++it;
    }
}
