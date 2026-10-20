// ArenaEventWindow: an arena event's card (event 1 the arena's boss, event 2 its waves). Port of
// ArenaEventWindow (libkingdom.so 5.11, @0x245ec8..0x249000).
//
// Window_arena_select_event.xml: the two events side by side, each with its picture, texts, the
// reputation bar and the start / unlock button. SetParameters picks the event (0 / 1) the arrow
// points at when the window comes in.
//
#include <cmath>
#include <cstdio>
#include <string>

#include "engine/IconManager.h"
#include "engine/Render.h"
#include "engine/Timer.h"
#include "game/BuildingHovers.h"
#include "game/Entity.h"
#include "game/EntityData.h"
#include "game/EntityManager.h"
#include "game/MetaData.h"
#include "game/MetaExpression.h"
#include "game/GameState.h"
#include "game/Professions.h"
#include "game/Setting.h"
#include "game/SpawnPoint.h"
#include "game/StringTable.h"
#include "game/Tasks.h"
#include "game/AIState.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/HUD.h"
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
float g_pulseTime = 0.f;                  // 0x617588 the chosen event's picture pulse
int g_lastSecond = 0;                     // 0x61758c Update's last wall-clock second
unsigned g_event = 0xffffffff;            // 0x60f0a0 SetParameters: the event (0 / 1) to point at
bool g_tablet = false;                    // 0x60f0a4
float g_scale = 1.f;                      // 0x60f0a8

// The arena NPC at spawn point `spawn` offers its talk `talk` (none while it has a quest to give).
void OfferTalk(uint32_t spawn, uint32_t talk, bool needPvP) {
    Entity* e = EntityManager::GetEntityBySpawnPoint(spawn);
    if (!e || BuildingHovers::IsWorldDialogVisible()) return;
    if (needPvP && !GameState::TaskCompleted(0x74d)) return;
    e->AssignSpecialTalk(e->GetTalkTask() != 0 ? 0 : talk);
}

// @0x247594: while the window is up, the chosen event's pictures swell to 1.2 and back each second
// (the others shrink back by dt). Once a wall-clock second the contents are refreshed and the
// events are kept: a running event whose time is up ends (its quests reset: event 1's 0x5b1 ..
// 0x5b7, event 2's 0x5b9) and starts its cooldown; one whose last quest (0x5b7 / 0x5b9) is done
// counts as completed; then the arena NPCs offer their talks.
void Update(float dt) {
    if (g_queue->shown) {
        for (unsigned i = 0; i != 2; ++i) {
            EventInfo& e = g_events[i];
            if (i == g_event) {
                const float t = g_pulseTime;
                if (t > 1.f) {
                    e.f94 = 1.f;
                    g_pulseTime = 0.f;
                } else if (t < 0.5f) {
                    const float x = t + t;
                    const float s = std::sqrt(1.f - x * x) - 1.f;
                    const float d = 0.f + s * -0.2f;
                    e.f94 = d + 1.f;
                } else {
                    float x = t - 0.5f;
                    x = x + x;
                    x = x - 1.f;
                    const float s = std::sqrt(1.f - x * x);
                    const float d = 0.2f + s * -0.2f;
                    e.f94 = d + 1.f;
                }
            } else if (e.f94 > 1.f) {
                e.f94 -= dt;
                if (e.f94 < 1.f) e.f94 = 1.f;
            } else {
                continue;
            }
            e.chestBg->SetAlpha(e.f94, true);
            e.wavesBg->SetAlpha(e.f94, true);
        }
        g_pulseTime += dt;
    }
    if (Timer::GetGlobalTime() == g_lastSecond) return;
    g_lastSecond = Timer::GetGlobalTime();
    if (g_queue->shown) UpdateContents();
    for (unsigned event = 1; event <= 2; ++event) {
        if (GameState::GetEventStartTime(event) != 0 && GetEventRemainingTime(event) == 0) {
            if (event == 1) {
                GameState::SetEventStartTime(1, 0);
                GameState::SetEventEndTime(1, (uint32_t)Timer::GetGlobalTime());
                if (Tasks::TaskInfo* t = Tasks::GetTask(0x5b1)) t->dnShow = t->dnShowDefault;
                GameState::ResetTask(0x5b1);
                GameState::SetTaskBeginTime(0x5b1, 1);
                GameState::ResetTask(0x5b2);
                GameState::ResetTask(0x5b3);
                GameState::ResetTask(0x5b4);
                GameState::ResetTask(0x5b7);
                Tasks::Update(true);
                HUDWindow::UpdateTasks();
            } else {
                GameState::SetEventStartTime(2, 0);
                GameState::SetEventStartTime(0x400, 0);
                GameState::SetEventEndTime(2, (uint32_t)Timer::GetGlobalTime());
                if (Tasks::TaskInfo* t = Tasks::GetTask(0x5b9)) t->dnShow = t->dnShowDefault;
                GameState::ResetTask(0x5b9);
            }
        }
        if (GameState::GetEventStartTime(event) == 0 || GetEventRemainingTime(event) == 0) continue;
        if (!GameState::TaskCompleted(event == 1 ? 0x5b7 : 0x5b9)) continue;
        GameState::SetEventStartTime(event, 0);
        if (event == 2) GameState::SetEventStartTime(0x400, 0);
        GameState::AddEventCompletion(event);
        if (GameState::GetEventEndTime(event) == 0) GameState::SetEventEndTime(event, (uint32_t)Timer::GetGlobalTime());
    }
    OfferTalk(0x4b6, 0x4b6, true);
    OfferTalk(0x4b4, 0x4b4, true);
    if (Entity* e = EntityManager::GetEntityBySpawnPoint(0x578)) {
        if (!BuildingHovers::IsWorldDialogVisible())
            e->AssignSpecialTalk(e->GetTalkTask() == 0 && GameState::GetTaskBeginTime(0x5b7) != 0 &&
                                         !GameState::TaskCompleted(0x5b7)
                                     ? 0x578
                                     : 0);
    }
    // PORT: the NPC at spawn point 0x695 would offer talk 0x695 (PlayerCompetitionWindow, online and
    // not ported; milestones 5t / 5u decide); it is left without one so its bubble leads nowhere.
}

// @0x247d64 (holder i: event i + 1): a locked event shows the help to unlock it. Event 1 not yet
// running starts: its time, difficulty 1 the first time (0 afterwards), its quests 0x5b1 .. 0x5b7
// afresh (all but the last done at once by a held item 0x201, with the arrow at the NPC 0x578), and
// the guardian at spawn point 0x4e4 becomes a random arena-1 monster (0x21a on difficulty 1) at the
// player's level, fighting on map 112; a running event 1 shows its open quest. Event 2 opens the
// difficulty choice, or goes on when it runs.
void OnStartEvent(unsigned i) {
    const unsigned event = i + 1;
    if (!IsEventUnlocked(event)) {
        LevelInfoWindow::HelpWithArena(event);
        Hide();
        return;
    }
    if (event == 1) {
        if (GameState::GetEventStartTime(1) == 0) {
            GameState::SetEventStartTime(1, (uint32_t)Timer::GetGlobalTime());
            GameState::SetEventEndTime(1, 0);
            const uint32_t done = GameState::GetEventCompletionCount(1);
            GameState::SetEventDifficulty(1, done <= 1 ? 1 - done : 0);
            GameState::ResetTask(0x5b1);
            GameState::StartTask(0x5b1);
            GameState::ResetTask(0x5b2);
            GameState::ResetTask(0x5b3);
            GameState::ResetTask(0x5b4);
            GameState::ResetTask(0x5b7);
            if (GameState::GetItemAmount(0x201, false) != 0) {
                GameState::CompleteTask(0x5b2, false);
                GameState::CompleteTask(0x5b3, false);
                GameState::CompleteTask(0x5b4, false);
                GameState::CompleteTask(0x5b1, false);
                if (Entity* npc = EntityManager::GetEntityBySpawnPoint(0x578)) {
                    AIBaseState* ai = EntityManager::GetPlayer()->GetAI();
                    BuildingHovers::SetArrowClickCallback(new GUI::Callback([ai, npc] { ai->ClickedEntity(npc); }));
                    const float x = npc->worldX;
                    const float y = npc->worldY;
                    BuildingHovers::ArrowAt(x, y - npc->GetIdleHeight() - 70.f, false, false, false, false, false,
                                            false, false);
                    Render::CenterOn(npc->worldX, npc->worldY, true, 0.4f, 0, false);
                }
            }
            if (Entity* guard = EntityManager::GetEntityBySpawnPoint(0x4e4)) {
                EntityData* d = EntityFactory::GetEntityForArena(1, 0);
                if (GameState::GetEventDifficulty(1) == 1) d = EntityFactory::GetEntityByID(0x21a);
                if (d) {
                    guard->SwitchEntityData(d);
                    if (SpawnPoint* sp = guard->GetSpawnPoint()) {
                        sp->deathTime = 0;
                        sp->entityId = d->id;
                        sp->hp = d->GetHpForLevel(GameState::GetLevel());
                        sp->hpMax = d->GetHpForLevel(GameState::GetLevel());
                        char* text = nullptr;
                        if (d->bossMinionsLevels) {
                            text = new char[0x80];
                            std::sprintf(text, "MAP_ID=112,BOSS=%d:%d", d->bossMinionsLevels->GetChild(0)->GetInt(),
                                         d->bossMinionsLevels->GetChild(1)->GetInt());
                        } else if (GameState::GetEventDifficulty(1) == 1) {
                            text = new char[0x80];
                            std::memcpy(text, "MAP_ID=112,BOSS=539:3", 0x16);
                        }
                        if (text) {
                            delete sp->meta;
                            sp->meta = new MetaExpression(text, true);
                        }
                    }
                }
            }
            Tasks::Update(true);
            HUDWindow::UpdateTasks();
        } else {
            unsigned task = 0;
            if (!GameState::TaskCompleted(0x5b1)) task = 0x5b1;
            else if (!GameState::TaskCompleted(0x5b3)) task = 0x5b3;
            else if (!GameState::TaskCompleted(0x5b7)) task = 0x5b7;
            if (task) {
                TaskInfoWindow::SetTask(task, false);
                TaskInfoWindow::Show();
            }
        }
    } else if (event == 2) {
        if (GameState::GetEventStartTime(2) == 0) ArenaDifficultyWindow::Show();
        else ArenaDifficultyWindow::OnStartEvent();
    }
    UpdateContents();
    Hide();
}

// @0x247acc: a cooling-down event (holder i: event i + 1) starts over at once for "arena_unlock"
// crystals (not enough: the NotEnough dialog); the price is confirmed first (pay = false). Event 2's
// companion record 0x400 is cleared with it.
void OnUnlockEvent(unsigned i, bool pay) {
    NotEnoughWindow::ResetRequirements();
    NotEnoughWindow::AddRequirement(GameState::kCrystal, (unsigned)Setting("arena_unlock").GetInt());
    if (!NotEnoughWindow::CheckRequirements()) {
        NotEnoughWindow::Show();
        return;
    }
    if (!pay) {
        ConfirmPurchaseWindow::SetParameters([i] { OnUnlockEvent(i, true); },
                                             (unsigned)Setting("arena_unlock").GetInt(), true);
        ConfirmPurchaseWindow::Show();
        return;
    }
    const unsigned event = i + 1;
    GameState::ChangeResourceAmount(GameState::kCrystal, -Setting("arena_unlock").GetInt());
    // (Billing::LogCBPurchase(0x21, event, price): online logging, not ported.)
    GameState::SetEventStartTime(event, 0);
    GameState::SetEventEndTime(event, 0);
    // UNVERIFIED (milestone 6): SoundsManager::PlaySound("ui_buy_with_crystals", 1, false).
    if (i == 1) GameState::SetEventStartTime(0x400, 0);
    UpdateContents();
    HUDWindow::UpdateTasks();
}

// @0x2481a0: the event's reputation in LevelInfoWindow (holder 0: reputation 2, holder 1: 1).
void OnReputationInfo(unsigned i) {
    Hide();
    LevelInfoWindow::Show();
    if (const Professions::Profession* p = Professions::GetProfession(i == 0 ? 2 : 1))
        LevelInfoWindow::OnTab(p->tab, true);
}

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
    if (!g_queue) {   // FUN_00245fd4 (static init)
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

// @0x2466f8: per event (holder i: event i + 1) its pictures (grey while cooling down or locked),
// names, description, the reputation's rank and bar towards the next rank, the time left or the
// cooldown, and the button: start (begin / select a difficulty / unlock quests), in progress, or
// start over early for crystals.
void UpdateContents() {
    g_title->SetText(StringTable::GetString("ARENA_EVENT_TITLE"));
    for (unsigned i = 0; i != 2; ++i) {
        EventInfo& e = g_events[i];
        const unsigned event = i + 1;
        const bool boss = event == 1;
        const unsigned rep = boss ? 2 : 1;
        const bool open = GetEventCooldownTime(event) == 0 && IsEventUnlocked(event);
        e.chestBg->SetEnabled(open);
        e.wavesBg->SetEnabled(open);
        e.day1->SetText(GetEventName(event));
        e.day2->SetText(GetEventName(event));
        e.description->SetText(GetEventDescription(event));
        e.repDefence->SetVisibility(boss);
        e.repAttack->SetVisibility(!boss);   // (the original passes the loop's previous index)
        const uint32_t level = Professions::GetPlayerProfessionLevel(rep);
        const Professions::ProfessionLevel* next = Professions::GetProfessionLevel(rep, level + 1);
        if (!next) next = Professions::GetProfessionLevel(rep, level);
        e.repRank->SetText(StringTable::GetString(level != 0 ? "ARENA_EVENT_RANK" : "ARENA_EVENT_UNRANKED"));
        e.repText->SetVisibility(level != 0);
        e.repText->SetText(ToWideString((int)level));
        e.unlockItems->SetText(StringTable::GetString("ARENA_EVENT_TAP_FOR_UNLOCKED"));
        e.infoIcon->SetTexture(IconManager::GetIcon("gold_info", false), true);
        const uint32_t points = GameState::GetProfessionPoints(rep);
        const std::u32string progress =
            SWPrintf(0x100, U"%s - %d - %s: %d/%d",
                     {StringTable::GetString("ARENA_EVENT_NEXT_RANK"), (int)(level + 1), next->name, (int)points,
                      (int)next->points});
        e.under->SetText(progress.c_str());
        e.over->SetText(progress.c_str());
        float part = (float)points / (float)next->points;
        if (part > 1.f) part = 1.f;
        const float fill = (float)e.barFill->w * part;
        e.clip.right = (int)((float)e.clip.left + fill);
        e.time.root->SetVisibility(IsEventActive(event) || GetEventCooldownTime(event) != 0);
        e.time.timer->SetVisibility(IsEventActive(event));
        e.time.timer->SetAsyncUpdate(false);
        e.time.timerOut->SetVisibility(GetEventCooldownTime(event) != 0);
        e.time.timerOut->SetAsyncUpdate(false);
        if (GetEventCooldownTime(event) == 0) {
            e.time.title->SetText(StringTable::GetString("ARENA_EVENT_TIMER_ACTIVE"));
            e.time.timer->SetText(StringTable::GetTimeString((int)GetEventRemainingTime(event), false).c_str());
        } else {
            e.time.title->SetText(StringTable::GetString("ARENA_EVENT_IN_TIMEOUT"));
            e.time.timerOut->SetText(StringTable::GetTimeString((int)GetEventCooldownTime(event), false).c_str());
        }
        ButtonInfo& b = e.button;
        b.locked->SetVisibility(false);
        const bool startable = !IsEventActive(event) && GetEventCooldownTime(event) == 0;
        const bool cooling = !startable && !IsEventActive(event);
        b.backdrop->SetVisibility(false);
        b.inProgress->SetVisibility(false);
        b.active->SetVisibility(!cooling);
        b.begin->SetVisibility(!cooling);
        b.icon->SetVisibility(!cooling);
        b.gold->SetVisibility(cooling);
        b.crystal->SetVisibility(cooling);
        b.goldText->SetVisibility(cooling);
        b.price->SetVisibility(cooling);
        if (cooling) {
            b.goldText->SetText(StringTable::GetString("ARENA_EVENT_LAUNCH_EARLY"));
            b.price->SetText(ToWideString(Setting("arena_unlock").GetInt()));
        } else {
            b.icon->SetTexture(IconManager::GetIcon("gold_attack", false), true);
            if (!startable)
                b.begin->SetText(StringTable::GetString("ARENA_EVENT_INPROGRESS"));
            else if (!IsEventUnlocked(event))
                b.begin->SetText(StringTable::GetString("PERFORM_UNLOCK"));
            else
                b.begin->SetText(StringTable::GetString(boss ? "ARENA_EVENT_BEGIN_CHALLENGE"
                                                             : "ARENA_EVENT_SELECT_DIFFICULTY"));
        }
        b.active->SetEnabled(true);
        b.begin->SetEnabled(true);
        b.icon->SetEnabled(true);
    }
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
