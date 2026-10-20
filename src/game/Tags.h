// Tags: named switches for seasonal content (items.xml "tags", e.g. "xmas_2012"). Port of Tags
// (@0x22e354..0x22e6d0). A tag is made the first time a name is asked for; the server turned tags
// on or set their time window (Tags::ChangeTagAttribute, online only, not ported). PORT (DECIDED,
// user 2026-10-19): offline the data's two events come back every year by the local calendar
// (CalendarActive); any other tag stays off. (Tags::Init @0x22e354, called by main_Loop_Init, is
// empty.)
#pragma once
#include <cstdint>
#include <ctime>

namespace Tags {

struct Tag {                  // 0x10 bytes
    char* name = nullptr;     // +0x00
    bool active = false;      // +0x04
    bool f05 = false;         // +0x05 IsActive when UpdateTagState last looked
    uint32_t from = 0;        // +0x08 time window (real global time + server offset)
    uint32_t to = 0;          // +0x0c 0: no window, active decides
    bool IsActive() const;    // @0x22e42c
};

Tag* GetTag(const char* name);   // @0x22e4f4 nullptr for an empty name
Tag* EnumTag(unsigned i);        // @0x22e6d0
// PORT: "xmas_2012" and "xmas_2012_raid" (the Christmas raid's quest) from December 1 to
// January 6; "thanksgiving_2012" the week (Monday to Sunday) of US Thanksgiving, the fourth
// Thursday of November; by the local date of `now`.
bool CalendarActive(const char* name, time_t now);

}  // namespace Tags
