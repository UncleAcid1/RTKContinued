// Settings: the game's tunable values from dynamic_config.xml (<FBC><px val="key=value,...">), the
// land areas (<ar>) and the level XP table built from them.
// Port of GameState::LoadSettings @0x1ac850, the Parse*Setting grammar (@0x1a2e5c..0x1ac534), the
// Setting wrapper (@0x190efc..0x19a694) and GameState::Get/SetSetting (libkingdom.so 5.11).
//
// A value is a tree of SettingData nodes. Grammar (delimiters , | ; =):
//   simple     := number (starts with a digit: strtod, as float) | string (non-empty, up to a delimiter)
//   wall       := simple ('|' simple)*        several -> a list node whose children are the items
//   semicolon  := wall (';' wall)*            several -> a list node
// "a=b" pairs (ParseAssignmentSetting, used by <ar def_objs>) are a list node of two children.
// Settings are keyed by StringHash (djb2) of the name in a std::map (0x6133a8), so hash collisions
// behave as in the original.
#pragma once
#include <cstdint>

namespace GameState {

struct SettingData {             // 0x18 bytes
    char* name = nullptr;        // +0x00 (top-level settings only)
    bool isNumber = false;       // +0x04
    bool isString = false;       // +0x05
    float value = 0.f;           // +0x08
    char* string = nullptr;      // +0x0c
    SettingData* child = nullptr;  // +0x10 first child (list nodes)
    SettingData* next = nullptr;   // +0x14 next sibling
};

struct AreaInfo {                // GameState::AreaInfo, 0xac bytes, from <ar>
    uint32_t id = 0;             // +0x00 "id"
    const char32_t* name = nullptr;  // +0x04 StringTable "name"
    uint32_t defObjCount = 0;    // +0x08
    struct { int id, count; } defObjs[16];  // +0x0c "def_objs" (id=count,...)
    int x = 0, y = 0, w = 0, h = 0;  // +0x8c "x" "y" "w" "h"
    int c = 0, c2 = 0;           // +0x9c "c", +0xa0 "c2"
    uint32_t level = 0;          // +0xa4 "l" (HasAreaForLevel)
    bool a = false;              // +0xa8 "a"
};

void LoadSettings(const char* file);                 // @0x1ac850
float GetSetting(const char* name);                  // @0x199db0 (0 if missing/empty)
void SetSetting(const char* name, float value);      // @0x19f344 (the string overload @0x1ac674 is console-only)
bool SettingExists(const char* name);                // @0x195538
// @0x190690: table 0x6129d8, [0] = 0, [n] = level_exp_1 + ... + level_exp_n.
uint32_t GetLevelXP(unsigned level);

bool GetAreaInfo(uint32_t id, AreaInfo& out);        // @0x194b40
unsigned GetAreaCount();                             // @0x1952c4
const AreaInfo* EnumAreaInfo(unsigned i);            // @0x190ab8
bool HasAreaForLevel(unsigned level);                // @0x194abc

}  // namespace GameState

uint32_t StringHash(const char* s);                  // @0x22d498 djb2
uint32_t StringHash(const char* begin, const char* end);  // @0x22d4fc

// Handle to a SettingData node (Setting::*). Errors print like the original's ErrorReporter.
class Setting {
public:
    Setting() = default;                             // @0x190efc
    explicit Setting(GameState::SettingData* d);     // @0x1924d0
    explicit Setting(const char* name);              // @0x199e50
    bool IsString() const;                           // @0x192350
    bool IsNumber() const;                           // @0x192380
    unsigned GetChildrenCount() const;               // @0x1923b0
    const char* GetString() const;                   // @0x192400
    int GetInt() const;                              // @0x192444 (int)(value + 0.5f)
    float GetFloat() const;                          // @0x192494
    Setting GetNext() const;                         // @0x19a614
    Setting GetChild(unsigned i) const;              // @0x19a694
    GameState::SettingData* data = nullptr;
};
