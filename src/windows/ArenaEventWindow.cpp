// ArenaEventWindow: an arena event's card (event 1 the arena's boss, event 2 its waves). Port of
// ArenaEventWindow (libkingdom.so 5.11, @0x245ec8..0x249000).
//
// So far the event queries the other windows use (5s-1); the window itself is 5s-2 / 5s-3.
#include <string>

#include "engine/Timer.h"
#include "game/GameState.h"
#include "game/Professions.h"
#include "game/Setting.h"
#include "game/StringTable.h"
#include "windows/Windows.h"

namespace ArenaEventWindow {

namespace {
std::u32string g_description;   // 0x617384 (0x80 characters)
}

// @0x246124
const char32_t* GetEventName(unsigned event) {
    return StringTable::GetString(event == 1 ? "ARENA_EVENT_01" : "ARENA_EVENT_02");
}

// @0x246154 the event is running (it has a start time).
bool IsEventActive(unsigned event) { return GameState::GetEventStartTime(event) != 0; }

// @0x246168 event 1 opens with quest 0x5b0, event 2 with 0x512.
bool IsEventUnlocked(unsigned event) { return GameState::TaskCompleted(event == 1 ? 0x5b0 : 0x512); }

// @0x24617c the seconds until the event can start again after its end (0: no end recorded, or over).
unsigned GetEventCooldownTime(unsigned event) {
    int cooldown = Setting("arena_boss_cooldown_time").GetInt();
    const int waves = Setting("arena_waves_cooldown_time").GetInt();
    if (event != 1) cooldown = waves;
    const uint32_t end = GameState::GetEventEndTime(event);
    if (end == 0) return 0;
    const int left = (int)(cooldown + end) - Timer::GetGlobalTime();
    return left < 0 ? 0 : (unsigned)left;
}

// @0x246200 the seconds left of a running event; the boss event never finished before: one day.
unsigned GetEventRemainingTime(unsigned event) {
    int time = Setting("arena_boss_completion_time").GetInt();
    const int waves = Setting("arena_waves_completion_time").GetInt();
    if (event != 1) time = waves;
    else if (GameState::GetEventCompletionCount(event) == 0) return 0x15180;
    const int left = (int)(time - Timer::GetGlobalTime() + GameState::GetEventStartTime(event));
    return left < 0 ? 0 : (unsigned)left;
}

// @0x24629c the event's text with its reputation's name (event 1: Professions 2, after "5" fights
// the first time and "1" afterwards; event 2: Professions 1).
const char32_t* GetEventDescription(unsigned event) {
    if (event == 1) {
        const Professions::Profession* p = Professions::GetProfession(2);
        const char32_t* format = StringTable::GetString("ARENA_EVENT_01_DESC");
        const char32_t* count = GameState::GetEventCompletionCount(event) == 0 ? U"5" : U"1";
        g_description = SWPrintf(0x80, format, {count, p->title});
    } else {
        const Professions::Profession* p = Professions::GetProfession(1);
        g_description = SWPrintf(0x80, StringTable::GetString("ARENA_EVENT_02_DESC"), {p->title});
    }
    return g_description.c_str();
}

}  // namespace ArenaEventWindow
