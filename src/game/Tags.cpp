#include "game/Tags.h"

#include <cstring>
#include <ctime>
#include <vector>

namespace Tags {

namespace {
std::vector<Tag*> g_tags;   // 0x616944
}

bool Tag::IsActive() const {
    if (to == 0) return active;
    // Timer::GetRealGlobalTime + GameState::GetServerTimeOffset (0x612f1c, 0 offline).
    uint32_t now = (uint32_t)time(nullptr);
    return from <= now && now <= to;
}

Tag* GetTag(const char* name) {
    if (!name || !*name) return nullptr;
    for (Tag* t : g_tags)
        if (std::strcmp(t->name, name) == 0) return t;
    Tag* t = new Tag;
    t->name = new char[std::strlen(name) + 1];
    std::strcpy(t->name, name);
    g_tags.push_back(t);
    return t;
}

Tag* EnumTag(unsigned i) { return i < g_tags.size() ? g_tags[i] : nullptr; }

}  // namespace Tags
