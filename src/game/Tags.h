// Tags: named switches for seasonal content (items.xml "tags", e.g. "xmas_2012"). Port of Tags
// (@0x22e354..0x22e6d0). A tag is made the first time a name is asked for; the server turned tags
// on or set their time window (Tags::ChangeTagAttribute, online only, not ported), so offline every
// tag stays off.
#pragma once
#include <cstdint>

namespace Tags {

struct Tag {                  // 0x10 bytes
    char* name = nullptr;     // +0x00
    bool active = false;      // +0x04
    bool f05 = false;         // +0x05
    uint32_t from = 0;        // +0x08 time window (real global time + server offset)
    uint32_t to = 0;          // +0x0c 0: no window, active decides
    bool IsActive() const;    // @0x22e42c
};

Tag* GetTag(const char* name);   // @0x22e4f4 nullptr for an empty name
Tag* EnumTag(unsigned i);        // @0x22e6d0

}  // namespace Tags
