#include "engine/IconManager.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>

#include "engine/Render.h"
#include "engine/Resources.h"

namespace IconManager {
namespace {
std::unordered_map<std::string, Render::Texture*> g_icons;   // keyed by StringHash on the original

// The pattern table at 0x601740
const char* kPatterns[14] = {
    "../resource/A2Static/2Optimized/images/Items/icons_system/%s.jpg",
    "../resource/A2Static/1Original/images/Items/icons_system/%s.png",
    "../resource/A2Static/2Optimized/images/spells/icons/%s.jpg",
    "../resource/A2Static/2Optimized/images/%s.jpg",
    "../resource/kingdom_icons/2Optimized/Icon_%s.png",
    "../resource/kingdom_icons/2Optimized/Icon_%s.jpg",
    "../resource/kingdom_icons/2Optimized/%s.png",
    "../resource/kingdom_icons/2Optimized/%s.jpg",
    "../resource/kingdom_icons/1Original/Icon_%s.png",
    "../resource/kingdom_icons/1Original/%s.png",
    "../resource/res_files/1Original/Icon_%s.png",
    "../resource/res_files/1Original/%s.png",
    "../resource/A2Static/2Optimized/images/Items/icons_reputation/professions/%s.jpg",
    "../resource/A2Static/2Optimized/images/Items/icons_reputation/reputations/%s.jpg",
};
}  // namespace

Render::Texture* GetIcon(const char* name, bool async) {
    (void)async;
    if (!std::strcmp(name, "dollars")) name = "bugs";
    else if (!std::strcmp(name, "cb")) name = "country_bugs";
    auto it = g_icons.find(name);
    if (it != g_icons.end()) return it->second;
    Render::Texture* t = Resources::GetImage(name);
    char buf[512];
    for (int i = 0; !t && i < 14; ++i) {
        std::snprintf(buf, sizeof buf, kPatterns[i], name);
        t = Resources::GetDirectImage(buf);
    }
    if (!t) {
        std::snprintf(buf, sizeof buf, "images/professions/tutorial/%s", name);
        t = Resources::GetImage(buf);
        if (!t) {
            std::snprintf(buf, sizeof buf, "images/professions/tutorial/icon_%s", name);
            t = Resources::GetImage(buf);
        }
    }
    g_icons[name] = t;
    return t;
}

// FUN_002dc20c: frames 1..n of `pattern` loaded once into `cache`, each shown frameTime seconds and
// linked to the next (the last back to the first). The first frame is returned.
static Render::Texture* GetAnimatedIcon(Render::Texture** cache, unsigned n, const char* pattern, float frameTime) {
    if (!cache[0] && n != 0) {
        char path[64];
        for (unsigned i = 0; i < n; ++i) {
            std::snprintf(path, sizeof path, pattern, i + 1);
            cache[i] = Resources::GetDirectImage(path);
            // (the original also clears the texture's +0x28)
        }
        for (unsigned i = 0; i < n; ++i) {
            if (!cache[i]) continue;
            cache[i]->frameTime = frameTime;
            cache[i]->next = cache[(i + 1) % n];
        }
    }
    return cache[0];
}

Render::Texture* GetSandClockIcon() {
    static Render::Texture* frames[1];
    return GetAnimatedIcon(frames, 1, "images/icon_60_timer (%d).png", 1.f / 30.f);
}

Render::Texture* GetExclamaitionIcon() {
    static Render::Texture* frames[15];
    // UNVERIFIED: the original also checks a field of the cached first frame before returning it.
    return GetAnimatedIcon(frames, 15, "images/exc_mark_%04d.png", 1.f / 15.f);
}

Render::Texture* GetPlayerMarkerAttack() {
    static Render::Texture* frames[15];  // 0x622bb8
    return GetAnimatedIcon(frames, 15, "images/attack_marker%04d.png", 1.f / 30.f);
}

Render::Texture* GetPlayerMarkerBlocked() {
    static Render::Texture* frames[13];  // 0x622bf4
    return GetAnimatedIcon(frames, 13, "images/icon_60_player_marker_blocked_%03d.png", 1.f / 30.f);
}

Render::Texture* GetPlayerMarkerGoto() {
    static Render::Texture* frames[15];  // 0x622c28
    return GetAnimatedIcon(frames, 15, "images/icon_60_player_marker_goto_%03d.png", 1.f / 30.f);
}

}  // namespace IconManager
