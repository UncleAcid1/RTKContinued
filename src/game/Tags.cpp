#include "game/Tags.h"

#include <cstring>
#include <ctime>
#include <vector>

namespace Tags {

namespace {
std::vector<Tag*> g_tags;   // 0x616944
}

// @0x22e42c: a tag without a time window is on when switched on (offline: by the calendar),
// else while the time is inside its window.
bool Tag::IsActive() const {
    if (to == 0) return active || CalendarActive(name, time(nullptr));
    // Timer::GetRealGlobalTime (@0x23ebe8: time(nullptr)) + GameState::GetServerTimeOffset (0x612f1c, 0 offline).
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

bool CalendarActive(const char* name, time_t now) {
    if (!name) return false;
    std::tm t{};
#if defined(_WIN32)
    localtime_s(&t, &now);
#else
    localtime_r(&now, &t);
#endif
    const int date = t.tm_mon * 100 + t.tm_mday;   // month (0-11) * 100 + day
    if (std::strcmp(name, "xmas_2012") == 0 || std::strcmp(name, "xmas_2012_raid") == 0)
        return date >= 1101 || date <= 6;
    if (std::strcmp(name, "thanksgiving_2012") == 0) {
        std::tm nov{};
        nov.tm_year = t.tm_year;
        nov.tm_mon = 10;
        nov.tm_mday = 1;
        nov.tm_hour = 12;
        nov.tm_isdst = -1;
        std::mktime(&nov);   // its weekday
        const int thursday = 1 + (4 - nov.tm_wday + 7) % 7 + 21;
        const int from = 1000 + thursday - 3;
        const int sunday = thursday + 3;
        const int to = sunday > 30 ? 1100 + sunday - 30 : 1000 + sunday;
        return from <= date && date <= to;
    }
    return false;
}

}  // namespace Tags
