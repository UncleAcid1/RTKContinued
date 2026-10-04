#include "engine/IconManager.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>

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

Render::Texture* GetSandClockIcon() { return Resources::GetDirectImage("images/icon_60_timer (1).png"); }

Render::Texture* GetExclamaitionIcon() { return Resources::GetDirectImage("images/exc_mark_0001.png"); }

}  // namespace IconManager
