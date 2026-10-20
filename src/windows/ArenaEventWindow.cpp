// ArenaEventWindow: an arena event's card (event 1 the arena's boss, event 2 its waves). Port of
// ArenaEventWindow (libkingdom.so 5.11, @0x245ec8..0x249000).
//
// Window_arena_select_event.xml: the two events side by side, each with its picture, texts, the
// reputation bar and the start / unlock button. SetParameters picks the event (0 / 1) the arrow
// points at when the window comes in.
//
// UNVERIFIED (milestone 5s-3): UpdateContents, Update, OnStartEvent, OnUnlockEvent and
// OnReputationInfo are still empty.
#include <cstdio>
#include <string>

#include "engine/Timer.h"
#include "game/BuildingHovers.h"
#include "game/Entity.h"
#include "game/EntityManager.h"
#include "game/GameState.h"
#include "game/Professions.h"
#include "game/Setting.h"
#include "game/StringTable.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "windows/Windows.h"

namespace ArenaEventWindow {

namespace {

using GUI::Textfield;
using GUI::Window;

// ArenaEventWindow::TimerInfo (0x10 bytes): the event's time.
struct TimerInfo {
    Window* root = nullptr;           // +0x00
    Textfield* title = nullptr;       // +0x04 text_title
    Textfield* timer = nullptr;       // +0x08 text_timer
    Textfield* timerOut = nullptr;    // +0x0c text_timer_out

    // @0x2482b4
    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        title = GUI::GetWindowTypedF<Textfield>(w, "%s.text_title", p);
        timer = GUI::GetWindowTypedF<Textfield>(w, "%s.text_timer", p);
        timerOut = GUI::GetWindowTypedF<Textfield>(w, "%s.text_timer_out", p);
    }
};

// ArenaEventWindow::ButtonInfo (0x2c bytes): start (in progress / begin) or unlock for crystals.
struct ButtonInfo {
    Window* root = nullptr;           // +0x00
    Window* backdrop = nullptr;       // +0x04 backdrop_01
    Textfield* inProgress = nullptr;  // +0x08 text_in_progress
    Window* active = nullptr;         // +0x0c button_active (tapped: OnStartEvent)
    Textfield* begin = nullptr;       // +0x10 text_begin
    Window* icon = nullptr;           // +0x14 icon_35
    Window* gold = nullptr;           // +0x18 button_gold (tapped: OnUnlockEvent)
    Window* crystal = nullptr;        // +0x1c icon_35_crystal
    Textfield* goldText = nullptr;    // +0x20 text_gold
    Textfield* price = nullptr;       // +0x24 text_price
    Window* locked = nullptr;         // +0x28 icon_locked

    // @0x248334
    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        backdrop = GUI::GetWindowTypedF<Window>(w, "%s.backdrop_01", p);
        inProgress = GUI::GetWindowTypedF<Textfield>(w, "%s.text_in_progress", p);
        active = GUI::GetWindowTypedF<Window>(w, "%s.button_active", p);
        begin = GUI::GetWindowTypedF<Textfield>(w, "%s.text_begin", p);
        icon = GUI::GetWindowTypedF<Window>(w, "%s.icon_35", p);
        icon->takesZ = true;
        gold = GUI::GetWindowTypedF<Window>(w, "%s.button_gold", p);
        crystal = GUI::GetWindowTypedF<Window>(w, "%s.icon_35_crystal", p);
        goldText = GUI::GetWindowTypedF<Textfield>(w, "%s.text_gold", p);
        price = GUI::GetWindowTypedF<Textfield>(w, "%s.text_price", p);
        locked = GUI::GetWindowTypedF<Window>(w, "%s.icon_locked", p);
    }
};

// ArenaEventWindow::EventInfo (0x98 bytes): one event's holder (arena_event_holder_01 / 02).
struct EventInfo {
    Window* root = nullptr;           // +0x00
    Window* chestBg = nullptr;        // +0x04 event_image_holder.arena_event_chest_bg
    Window* wavesBg = nullptr;        // +0x08 event_image_holder.arena_event_waves_bg
    Textfield* day1 = nullptr;        // +0x0c text_day_01
    Textfield* day2 = nullptr;        // +0x10 text_day_02
    Textfield* description = nullptr; // +0x14 text_description
    Window* repHolder = nullptr;      // +0x18 arena_event_pb.rep_icon_holder (tapped: OnReputationInfo)
    Window* repAttack = nullptr;      // +0x1c its icon_rep_attack
    Window* repDefence = nullptr;     // +0x20 its icon_rep_defence
    Textfield* repRank = nullptr;     // +0x24 its text_rank
    Textfield* repText = nullptr;     // +0x28 its text
    Window* bar = nullptr;            // +0x2c progress_bar_arena_event_orange (tapped: OnReputationInfo)
    Textfield* under = nullptr;       // +0x30 text_progress_under_01
    Textfield* over = nullptr;        // +0x34 the bar's text_progress_over_01
    Window* barFill = nullptr;        // +0x38 the bar's progress_bar_large_orange
    GUI::ClipRect clip{};             // +0x3c the fill's rect, clipping it and the text over it
    Window* itemsInfo = nullptr;      // +0x4c button_rep_items_info (tapped: OnReputationInfo)
    Textfield* unlockItems = nullptr; // +0x50 its text_unlock_items
    Window* infoIcon = nullptr;       // +0x54 its icon_info
    TimerInfo time;                   // +0x58 event_time_holder
    ButtonInfo button;                // +0x68 button_arena_event_start
    float f94 = 1.f;                  // +0x94

    // @0x248480
    void LoadFrom(Window* w, const char* p) {
        char q[128];
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        chestBg = GUI::GetWindowTypedF<Window>(w, "%s.event_image_holder.arena_event_chest_bg", p);
        wavesBg = GUI::GetWindowTypedF<Window>(w, "%s.event_image_holder.arena_event_waves_bg", p);
        day1 = GUI::GetWindowTypedF<Textfield>(w, "%s.text_day_01", p);
        day2 = GUI::GetWindowTypedF<Textfield>(w, "%s.text_day_02", p);
        description = GUI::GetWindowTypedF<Textfield>(w, "%s.text_description", p);
        repHolder = GUI::GetWindowTypedF<Window>(w, "%s.arena_event_pb.rep_icon_holder", p);
        repAttack = GUI::GetWindowTypedF<Window>(w, "%s.arena_event_pb.rep_icon_holder.icon_rep_attack", p);
        repDefence = GUI::GetWindowTypedF<Window>(w, "%s.arena_event_pb.rep_icon_holder.icon_rep_defence", p);
        repRank = GUI::GetWindowTypedF<Textfield>(w, "%s.arena_event_pb.rep_icon_holder.text_rank", p);
        repText = GUI::GetWindowTypedF<Textfield>(w, "%s.arena_event_pb.rep_icon_holder.text", p);
        bar = GUI::GetWindowTypedF<Window>(w, "%s.arena_event_pb.progress_bar_arena_event_orange", p);
        under = GUI::GetWindowTypedF<Textfield>(w, "%s.arena_event_pb.text_progress_under_01", p);
        over = GUI::GetWindowTypedF<Textfield>(
            w, "%s.arena_event_pb.progress_bar_arena_event_orange.text_progress_over_01", p);
        barFill = GUI::GetWindowTypedF<Window>(
            w, "%s.arena_event_pb.progress_bar_arena_event_orange.progress_bar_large_orange", p);
        clip = {barFill->x, barFill->y, barFill->x + barFill->w, barFill->y + barFill->h};
        over->SetClipRect(&clip);
        barFill->SetClipRect(&clip);
        itemsInfo = GUI::GetWindowTypedF<Window>(w, "%s.arena_event_pb.button_rep_items_info", p);
        unlockItems = GUI::GetWindowTypedF<Textfield>(w, "%s.arena_event_pb.button_rep_items_info.text_unlock_items", p);
        infoIcon = GUI::GetWindowTypedF<Window>(w, "%s.arena_event_pb.button_rep_items_info.icon_info", p);
        infoIcon->takesZ = true;
        std::snprintf(q, sizeof q, "%s.event_time_holder", p);
        time.LoadFrom(w, q);
        std::snprintf(q, sizeof q, "%s.button_arena_event_start", p);
        button.LoadFrom(w, q);
        f94 = 1.f;
    }
};

WindowManager::FunctionalWindow* g_queue = nullptr;   // 0x617594
Window* g_root = nullptr;                 // 0x61724c
GUI::TweenEffect* g_tween = nullptr;      // 0x617250
EventInfo g_events[2];                    // 0x617254
std::u32string g_description;             // 0x617384 GetEventDescription's text (0x80 characters)
Textfield* g_title = nullptr;             // 0x617584 text_title
Window* g_close = nullptr;                // 0x617590 x_button.clickArea
unsigned g_event = 0xffffffff;            // 0x60f0a0 SetParameters: the event (0 / 1) to point at
bool g_tablet = false;                    // 0x60f0a4
float g_scale = 1.f;                      // 0x60f0a8

// UNVERIFIED (milestone 5s-3): the event's start (OnStartEvent @0x247d64), unlock
// (OnUnlockEvent @0x247acc) and reputation card (OnReputationInfo @0x2481a0).
void OnStartEvent(unsigned) {}
void OnUnlockEvent(unsigned, bool) {}
void OnReputationInfo(unsigned) {}
// UNVERIFIED (milestone 5s-3): @0x247594.
void Update(float) {}

// @0x2481ec
bool Click(int x, int y, bool pressed) {
    bool visible = g_root->visibleSelf;
    if (visible) {
        if (BuildingHovers::ClickOnArrow(x, y, pressed)) return true;
        if (!pressed) BuildingHovers::HideArrow();
        visible = g_root->visibleSelf;
    }
    g_root->Click(x, y, pressed, false);
    return visible;
}

// @0x245edc
void SetZ(float z) {
    if (IsVisible()) g_root->SetZ(z);
    g_root->SetPosition(g_tween->GetCenteredX(), g_tween->GetCenteredY());
}

// @0x245f7c
float ZRange() { return g_queue->shown ? 0.01f : 0.001f; }

// @0x2460dc
void OnTweenOut() {
    WindowManager::WindowHide(false);
    g_queue->shown = false;
    g_queue->MoveWindowDown(true);
    GameState::DropGamePauseState();
    g_event = 0xffffffff;
}

// @0x247414: the arrow at the chosen event's start button, or at its unlock button while the
// start button is hidden; the arrow's tap does what the button does.
void OnTweenIn() {
    if (g_event < 2) {
        const unsigned i = g_event;
        const Window* w = g_events[i].button.active;
        if (!w->visibleSelf) w = g_events[i].button.gold;
        BuildingHovers::SetArrowVisibleWindowLimit(2);
        BuildingHovers::ArrowAt((float)(g_root->x + w->x), (float)(g_root->y + w->y + w->h / 2), false, true, true,
                                false, true, false, false);
        if (g_events[i].button.active->visibleSelf)
            BuildingHovers::SetArrowClickCallback(new GUI::Callback([i] { OnStartEvent(i); }));
        else
            BuildingHovers::SetArrowClickCallback(new GUI::Callback([i] { OnUnlockEvent(i, false); }));
    }
    UpdateContents();
}

// @0x247d38
bool OnBack() {
    if (!g_queue->shown) return false;
    BuildingHovers::HideArrow();
    Hide();
    return true;
}

}  // namespace

WindowManager::FunctionalWindow* Queue() {
    if (!g_queue) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.click = Click;
        f.setZ = SetZ;
        f.hide = Hide;
        g_queue = new WindowManager::FunctionalWindow("ArenaEventWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_queue && g_queue->shown; }   // @0x245ec8
void SetParameters(unsigned event) { g_event = event; }  // @0x245f68

// @0x248780
void Init() {
    g_tablet = GUI::IsTabletVersion();
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Window_arena_select_event.xml",
                             "Window_arena_select_event.png", -1.f, 0, 0, 0, 0, false, 1.f);
    if (!g_root) return;
    Queue()->RegisterTopWindow(g_root);
    g_scale = g_root->scale;
    g_root->SetVisibility(false);
    g_tween = GUI::CreateTweenEffect(g_root, nullptr, OnTweenIn, OnTweenOut);
    g_close = GUI::GetWindowTyped<Window>(g_root, "x_button.clickArea");
    g_close->SetOnClick(Hide);
    g_close->SetActionSound("ui_close", 1);
    g_title = GUI::GetWindowTyped<Textfield>(g_root, "text_title");
    for (unsigned i = 0; i != 2; ++i) {
        char p[64];
        std::snprintf(p, sizeof p, "arena_event_holder_%02d", i + 1);
        EventInfo& e = g_events[i];
        e.LoadFrom(g_root, p);
        e.button.active->SetOnClick([i] { OnStartEvent(i); });
        e.button.gold->SetOnClick([i] { OnUnlockEvent(i, false); });
        e.repHolder->SetOnClick([i] { OnReputationInfo(i); });
        e.bar->SetOnClick([i] { OnReputationInfo(i); });
        e.itemsInfo->SetOnClick([i] { OnReputationInfo(i); });
    }
    g_queue->fn.zRange = ZRange;
    g_queue->fn.update = Update;
    g_queue->fn.back = OnBack;
    g_queue->fn.show = Show;
    g_queue->hideOnOuterClick = true;
}

// @0x247c5c (not while the hero is dead): the game pauses while it is up.
void Show() {
    if (EntityManager::GetPlayer()->IsDead()) return;
    g_queue->shown = true;
    g_queue->MoveWindowOnTop(true);
    if (!g_root->visibleSelf) {
        WindowManager::WindowShow(false);
        g_tween->AnimateIn();
        GameState::RaiseGamePauseState();
    }
    g_root->SetVisibility(true);
    UpdateContents();
    g_root->PreloadTextures();   // GUI::PreloadWindow
}

// @0x247cf8
void Hide() {
    if (!g_root || !g_root->visibleSelf) return;
    g_tween->AnimateOut();
}

// UNVERIFIED (milestone 5s-3): @0x2466f8.
void UpdateContents() {}

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
