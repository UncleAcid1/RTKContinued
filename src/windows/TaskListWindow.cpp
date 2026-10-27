// TaskListWindow: the full quest list (Task_holder.xml), opened by the bottom bar's "all quests"
// button: tabs All / Battle / City / Completed over eight slots in two columns that scroll; a slot
// opens the quest's window (a completed one only says so).
// Port of TaskListWindow (libkingdom.so 5.11), 0x35e4e4..0x361fcc. Statics at 0x62fe24.
//
// The goblins away on a job (GameState::GetOfflineGoblin) are listed before the quests in the All
// and Battle tabs (goblin picture, the map's name, a countdown that Update refreshes); a tap leads
// to the goblin; BuyGoblin leads home to the goblins' hire window.
// The original's sort helpers (0x35ef14, 0x35f000, 0x35f210: std::sort; 0x35fef8, 0x35ffe4,
// 0x360138, 0x360224, 0x360854: std::stable_sort; 0x35f194: the slot list's growth) are the
// standard library's here.
#include <algorithm>
#include <cstdio>
#include <vector>

#include "engine/IconManager.h"
#include "engine/Render.h"
#include "engine/Resources.h"
#include "engine/Timer.h"
#include "game/BuildingHovers.h"
#include "game/Entity.h"
#include "game/GameState.h"
#include "game/Locations.h"
#include "game/Map.h"
#include "game/StringTable.h"
#include "game/Tasks.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/ContentScroller.h"
#include "hud/HUD.h"
#include "hud/Shared.h"
#include "windows/Windows.h"

namespace TaskListWindow {
namespace {

// PORT: a map without a name (the city, an unknown map) reads as an empty text.
const char32_t* MapName(uint32_t map) {
    const char32_t* name = Locations::GetMapName(map);
    return name ? name : U"";
}

using GUI::Button;
using GUI::Textfield;
using GUI::Window;

constexpr unsigned kSlots = 8;
enum Tab { kAll = 0, kBattle = 1, kCity = 2, kCompleted = 3 };

// One slot (TaskListWindow::TaskInfo, 0x4c bytes), "quests_holder_slot_%02d" and inside it
// "slider_mission_scroll_win.<name>".
struct Slot {
    Window* root = nullptr;             // +0x00
    Window* locked = nullptr;           // +0x04 slot_locked_bg
    Window* bg = nullptr;               // +0x08 button_scroll_bg
    Window* bgBattle = nullptr;         // +0x0c button_scroll_bg_battle
    Window* bgSide = nullptr;           // +0x10 button_scroll_bg_battle_side
    Window* goblin = nullptr;           // +0x14 quest_icon_goblin
    Window* quickGeneric = nullptr;     // +0x18 quick_task_bg_generic
    Window* quickBlue = nullptr;        // +0x1c quick_task_bg_blue
    Window* quickGreen = nullptr;       // +0x20 quick_task_bg_green
    Window* quickOrange = nullptr;      // +0x24 quick_task_bg_orange
    Window* quickRed = nullptr;         // +0x28 quick_task_bg_red
    Window* icon = nullptr;             // +0x2c button_scroll_bg.icon (takes a depth slot, as the next three)
    Window* iconBattle = nullptr;       // +0x30 button_scroll_bg_battle.icon
    Window* iconSide = nullptr;         // +0x34 button_scroll_bg_battle_side.icon
    Window* iconQuick = nullptr;        // +0x38 icon_all_quests_quick_task
    Window* iconNew = nullptr;          // +0x3c icon_gold_quest_new
    Textfield* time = nullptr;          // +0x40 text_goblin_time_2
    Textfield* title = nullptr;         // +0x44 text_title
    Textfield* place = nullptr;         // +0x48 text_item_type
};

// An entry of the list (TaskListWindow::TaskListSlotInfo, 0x10 bytes): a quest or a goblin.
struct SlotInfo {
    Tasks::TaskInfo* task = nullptr;    // +0x00
    Entity* goblin = nullptr;           // +0x04
    uint32_t map = 0;                   // +0x08 the goblin's map
    uint32_t endTime = 0;               // +0x0c when the goblin returns
};

WindowManager::FunctionalWindow* g_queue = nullptr;   // TaskListWindow::wnd
Window* g_root = nullptr;                // 0x62fe24
GUI::TweenEffect* g_tween = nullptr;     // 0x62fe28
Shared::TabHolder g_tabs[5];             // 0x62fe2c
Slot g_slots[kSlots];                    // 0x62fee0
std::vector<SlotInfo> g_list;            // 0x630140
Shared::ContentScroller g_scroller;      // 0x63015c
Window* g_scrollHolder = nullptr;        // 0x6301d4 (the scroller's +0x78) "scrollbar.scrollbar_all_tasks"
bool g_buyGoblin = false;                // 0x630268 set by BuyGoblin
unsigned g_tab = kAll;                   // 0x63026c
Entity* g_goblin = nullptr;              // 0x630270 the goblin FindGoblin travels to
Button* g_close = nullptr;               // 0x630274
Textfield* g_title = nullptr;            // 0x630278
unsigned g_lastHelpItem = 0xffffffff;    // 0x60f418 the slot tapped before travelling
bool g_tablet = false;                   // 0x60f41c
float g_scale = 1.f;                     // 0x60f420

// @0x35e98c TaskListWindow::TaskListOrdering: the higher priority first, then the later begun.
bool TaskListOrdering(Tasks::TaskInfo* a, Tasks::TaskInfo* b) {
    if (a->priority == b->priority) return GameState::GetTaskBeginTime(b->id) < GameState::GetTaskBeginTime(a->id);
    return b->priority < a->priority;
}

// @0x35eee0 TaskListWindow::SortByEndTime: the later completed first.
bool SortByEndTime(const SlotInfo& a, const SlotInfo& b) {
    return GameState::GetTaskEndTime(b.task->id) < GameState::GetTaskEndTime(a.task->id);
}

// @0x35e958
void OnTweenOut() {
    WindowManager::WindowHide(false);
    g_queue->shown = false;
    g_queue->MoveWindowDown(true);
    GameState::DropGamePauseState();
}

// @0x35eb44 TaskListWindow::OnTask: a slot was tapped.
void OnTask(unsigned slot) {
    const unsigned index = slot + g_scroller.firstIndex;
    if (index >= g_list.size()) return;
    const SlotInfo info = g_list[index];
    if (g_tab == kCompleted && info.task && info.task->quickTask) {
        PopupWindow::Show(StringTable::GetString("TASK_COMPLETE_TEXT"), PopupWindow::Hide, nullptr, nullptr);
        return;
    }
    Hide();
    if (info.task) {
        TaskInfoWindow::SetTask(info.task->id, false);
        TaskInfoWindow::Show();
        return;
    }
    if (!info.goblin || GameState::GetCurrentMapID() != (int)info.map) {
        if (Map::GetCurrentFarm()) HUDWindow::ExitFarm();
        g_lastHelpItem = slot;
        Tasks::HelpWithMapSwitchTo(info.map, true, false, 0);
        return;
    }
    Entity* e = info.goblin;
    if (e->workX != 0 && e->workY != 0)
        Tasks::HelpWithLocation((int)e->worldX, (int)(e->worldY - 50.f), e->tileX, e->tileY);
}

// @0x35e9e0 TaskListWindow::Update: the goblins' countdowns, then the scroller.
void Update(float dt) {
    if (!g_root->visibleSelf) return;
    for (unsigned i = 0; i < kSlots; ++i) {
        const unsigned index = i + g_scroller.firstIndex;
        if (index >= g_list.size()) continue;
        const SlotInfo& info = g_list[index];
        if (info.task) continue;
        const int left = (int)info.endTime - (int)Timer::GetGlobalTime();
        g_slots[i].time->SetVisibility(true);
        g_slots[i].time->SetText(StringTable::GetNumericTimeString(left < 0 ? 0 : left, false).c_str());
    }
    g_scroller.Update(dt);
    g_scroller.UpdateScroll();
    g_root->UpdatePosition();
}

// @0x35edc4
bool OnBack() {
    if (!g_queue->shown) return false;
    Hide();
    return true;
}

// @0x35edec
void Move(int x, int y) { g_scroller.Move(x, y); }

// @0x35ee08
bool Click(int x, int y, bool pressed) {
    bool r = false;
    if (g_root->visibleSelf) {
        if (g_scroller.Click(x, y, pressed, g_root) && !pressed) return true;
        r = g_root->visibleSelf;
    }
    g_root->Click(x, y, pressed, false);
    return r;
}

// @0x35e4e4
void SetZ(float z) {
    if (!g_queue->shown) return;
    g_root->SetZ(z);
    g_root->SetPosition(g_tween->GetCenteredX(), g_tween->GetCenteredY());
}

// @0x35eea0
void Deinit() {
    GUI::RemoveTweenEffect(g_tween);
    g_tween = nullptr;
    delete g_root;
}

// @0x361dd0
void OnTab(unsigned tab) {
    g_tab = tab;
    g_scroller.ScrollToTop();
    UpdateContents();
}

// The slot's looks for a quest: one background by kind, plus the quick task's coloured ones.
void SetSlotTask(Slot& s, Tasks::TaskInfo* t) {
    const bool battle = t->showInBattle, side = t->sidequest, quick = t->quickTask;
    s.bg->SetVisibility(!battle && !side && !quick);
    s.bgBattle->SetVisibility(battle && !side && !quick);
    s.bgSide->SetVisibility(side && !quick);
    s.goblin->SetVisibility(false);
    s.quickGeneric->SetVisibility(quick);
    s.quickBlue->SetVisibility(!battle && !side && quick);
    s.quickGreen->SetVisibility(side && quick);
    s.quickOrange->SetVisibility(battle && !side && quick);
    s.quickRed->SetVisibility(false);
    s.iconQuick->SetVisibility(quick);
    if (Render::Texture* image = t->GetIcon()) {
        unsigned size = (unsigned)s.icon->w;
        if (image->w <= (int)size) size = 0;
        s.icon->SetTexture(image, true, size, 0, false, 0);
        s.iconBattle->SetTexture(image, true, size, 0, false, 0);
        s.iconSide->SetTexture(image, true, size, 0, false, 0);
        s.iconQuick->SetTexture(image, true, size, 0, false, 0);
        s.icon->ApplyMask(Resources::GetUIImage("quest_icon_left_mask.png", false, false), s.bg);
        s.iconBattle->ApplyMask(Resources::GetUIImage("quest_icon_left_mask.png", false, false), s.bgBattle);
        s.iconSide->ApplyMask(Resources::GetUIImage("quest_icon_left_mask.png", false, false), s.bgSide);
        s.iconQuick->ApplyMask(Resources::GetUIImage("quick_task_quest_holder_mask.png", false, false), s.quickBlue);
    }
    s.time->SetVisibility(false);
    // A quick task is named by its step's text "STASK<step>" when there is one.
    bool named = false;
    if (quick) {
        char key[128];
        std::snprintf(key, sizeof key, "STASK%d", t->subtasks[0].id);
        if (StringTable::StringExists(key) && StringTable::StringNotEmpty(key)) {
            s.title->SetText(StringTable::GetUndecoratedString(StringTable::GetString(key), 0x100).c_str());
            named = true;
        }
    }
    if (!named) s.title->SetText(t->name);
    // Where: the first step's map, else the quest's. Hidden for the map the player is on.
    uint32_t map = t->subtasks[0].assistMap;
    if (map == 0) map = t->mapId ? t->mapId : t->debugMap;
    const char32_t* where;
    if ((int)map == GameState::GetCurrentMapID()) {
        s.place->SetVisibility(false);
        where = MapName(0);
    } else {
        s.place->SetVisibility(map != 0);
        where = map == 0x2f ? GameState::GetCastleName() : MapName(map);
    }
    s.place->SetText(SWPrintf(0x100, StringTable::GetString("TASK_FOUND_AT"), {where}).c_str());
}

// The slot's looks for a goblin away on another map.
void SetSlotGoblin(Slot& s, const SlotInfo& info) {
    for (Window* w : {s.bg, s.bgBattle, s.bgSide, s.quickGeneric, s.quickBlue, s.quickGreen, s.quickOrange, s.quickRed,
                      s.iconQuick})
        w->SetVisibility(false);
    s.goblin->SetVisibility(true);
    const int left = (int)info.endTime - (int)Timer::GetGlobalTime();
    s.time->SetVisibility(true);
    s.time->SetText(StringTable::GetNumericTimeString(left < 0 ? 0 : left, false).c_str());
    s.title->SetText(MapName(info.map));
    s.place->SetVisibility(false);
}

}  // namespace

WindowManager::FunctionalWindow* Queue() {
    if (!g_queue) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.deinit = Deinit;
        f.click = Click;
        f.setZ = SetZ;
        f.hide = Hide;
        f.move = Move;
        g_queue = new WindowManager::FunctionalWindow("TaskListWindow", std::move(f));
    }
    return g_queue;
}

// @0x35f4ac
void Init() {
    g_tablet = GUI::IsTabletVersion();
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Task_holder.xml", "Task_holder.png", -1.f, 0, 0, 0, 0,
                             false, 1.f);
    g_queue->RegisterTopWindow(g_root);
    g_scale = g_root->scale;
    g_root->SetVisibility(false);
    g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::GetVerticalCenter(g_root->h));
    g_tween = GUI::CreateTweenEffect(g_root, nullptr, nullptr, OnTweenOut);
    g_tween->CenterWith(0, 0, 0, (int)(g_scale * -80.f));
    g_close = GUI::GetWindowTyped<Button>(g_root, "x_button.clickArea");
    g_close->SetOnClick(Hide);
    g_close->SetActionSound("ui_close", 1);
    g_title = GUI::GetWindowTypedF<Textfield>(g_root, "text_player_name");
    for (unsigned i = 0; i < 5; ++i) {
        char name[32];
        std::snprintf(name, sizeof name, "tab_%02d", i + 1);
        g_tabs[i].LoadFrom(g_root, name, true);
        g_tabs[i].root->SetOnClick([i] { OnTab(i); });
        g_tabs[i].root->SetActionSound("ui_tab", 1);
    }
    g_tabs[kAll].icon->SetTexture(IconManager::GetIcon("bottom_bar_quests", false), true, 0, 0, false, 0);
    g_tabs[kBattle].icon->SetTexture(IconManager::GetIcon("bottom_bar_army", false), true, 0, 0, false, 0);
    g_tabs[kCity].icon->SetTexture(IconManager::GetIcon("bottom_bar_build", false), true, 0, 0, false, 0);
    g_tabs[kCompleted].icon->SetTexture(IconManager::GetIcon("gold_confirm", false), true, 0, 0, false, 0);
    for (unsigned i = 0; i < kSlots; ++i) {
        Slot& s = g_slots[i];
        const int n = (int)i + 1;
        s.root = GUI::GetWindowTypedF<Window>(g_root, "quests_holder_slot_%02d", n);
        s.root->SetOnClick([i] { OnTask(i); });
#define TL_WIN(field, name) \
    s.field = GUI::GetWindowTypedF<Window>(g_root, "quests_holder_slot_%02d.slider_mission_scroll_win." name, n)
        TL_WIN(locked, "slot_locked_bg");
        TL_WIN(bg, "button_scroll_bg");
        TL_WIN(bgBattle, "button_scroll_bg_battle");
        TL_WIN(bgSide, "button_scroll_bg_battle_side");
        TL_WIN(goblin, "quest_icon_goblin");
        TL_WIN(quickGeneric, "quick_task_bg_generic");
        TL_WIN(quickBlue, "quick_task_bg_blue");
        TL_WIN(quickGreen, "quick_task_bg_green");
        TL_WIN(quickOrange, "quick_task_bg_orange");
        TL_WIN(quickRed, "quick_task_bg_red");
        TL_WIN(icon, "button_scroll_bg.icon");
        TL_WIN(iconBattle, "button_scroll_bg_battle.icon");
        TL_WIN(iconSide, "button_scroll_bg_battle_side.icon");
        TL_WIN(iconQuick, "icon_all_quests_quick_task");
        TL_WIN(iconNew, "icon_gold_quest_new");
#undef TL_WIN
        s.icon->takesZ = s.iconBattle->takesZ = s.iconSide->takesZ = s.iconQuick->takesZ = true;
        s.time = GUI::GetWindowTypedF<Textfield>(
            g_root, "quests_holder_slot_%02d.slider_mission_scroll_win.text_goblin_time_2", n);
        s.title = GUI::GetWindowTypedF<Textfield>(g_root,
                                                  "quests_holder_slot_%02d.slider_mission_scroll_win.text_title", n);
        s.place = GUI::GetWindowTypedF<Textfield>(
            g_root, "quests_holder_slot_%02d.slider_mission_scroll_win.text_item_type", n);
    }
    g_scroller.Init();
    for (unsigned i = 0; i < kSlots; ++i) {
        g_slots[i].root->SetClipRect(&g_scroller.viewport);
        g_scroller.windows.push_back(g_slots[i].root);
    }
    g_scrollHolder = GUI::GetWindowTypedF<Window>(g_root, "scrollbar.scrollbar_all_tasks");
    g_scroller.scrollbar = GUI::GetWindowTypedF<Window>(g_root, "scrollbar.scrollbar_all_tasks.scroll_bar_tiny");
    g_scroller.horizontal = false;
    g_scroller.unkA0 = (int)kSlots;
    g_scroller.onScroll = UpdateContents;
    g_scroller.itemSize = g_slots[2].root->y - g_slots[0].root->y;
    g_scroller.perLine = 2;
    Window* first = GUI::GetWindowTyped<Window>(g_root, "slot_locked_bg_01");
    Window* second = GUI::GetWindowTyped<Window>(g_root, "slot_locked_bg_02");
    g_queue->hideOnOuterClick = true;
    g_queue->fn.update = Update;
    g_queue->fn.back = OnBack;
    g_queue->fn.show = Show;
    g_scroller.viewport.bottom = first->y + first->h - 4;
    g_scroller.viewport.left = first->x;
    g_scroller.viewport.top = first->y + 4;
    g_scroller.viewport.right = second->w + second->x;
}

// @0x360908 TaskListWindow::UpdateContents: builds the tab's list and fills the eight slots.
void UpdateContents() {
    Tasks::TaskInfo* cur[0x80] = {};
    unsigned n = Tasks::GetCurrentTasks(cur, 0x80, true, true);
    // Not listed: the timed maps' quests before their map has started, and tasks 0x77c..0x780.
    for (unsigned i = 0; i < n;) {
        const uint32_t id = cur[i]->id;
        if ((id == 0x36c || id == 0x426 || id == 0x5e4 || id == 0x36a) && GameState::GetMapStartTime(cur[i]->mapId) == 0)
            cur[i] = cur[--n];
        else
            ++i;
    }
    for (unsigned i = 0; i < n;) {
        if (cur[i]->id >= 0x77c && cur[i]->id <= 0x780) cur[i] = cur[--n];
        else ++i;
    }
    std::sort(cur, cur + n, TaskListOrdering);
    g_list.clear();
    // In the All and Battle tabs the goblins that are away on a map come first.
    if (g_tab != kCity && g_tab != kCompleted) {
        for (unsigned i = 0; (int)i < (int)GameState::GetOfflineGoblinCount(); ++i) {
            GameState::OfflineGoblin* g = GameState::GetOfflineGoblin(i);
            if (!g->entity || g->entity->GetCurrentMap() == 0) continue;
            g_list.push_back({nullptr, g->entity, (uint32_t)g->entity->GetCurrentMap(),
                              (uint32_t)Timer::GetGlobalTime() + g->timeLeft});
        }
    }
    for (unsigned i = 0; i < n; ++i) {
        if (g_tab == kBattle && !cur[i]->showInBattle) continue;
        if (g_tab == kCity && !cur[i]->isCity) continue;
        if (g_tab == kCompleted) continue;
        g_list.push_back({cur[i], nullptr, 0, 0});
    }
    if (g_tab == kCompleted) {
        // The ten quests completed last (no hints, chapters or hidden ones).
        for (unsigned i = 0;; ++i) {
            Tasks::TaskInfo* t = Tasks::EnumTask(i);
            if (!t) break;
            if (t->isHint || t->chapter || t->dnShow) continue;
            if (GameState::GetTutorialType() == 1 && t->id == 0xd5) continue;
            if (!GameState::TaskCompleted(t->id)) continue;
            if (t->tutorialType && t->subtasks[0].type == 0) continue;
            if (!t->isDublicateTalk) {
                // (a quest with a step of type 0x25 only shows when marked as the duplicate talk)
                const bool noTalk = t->subtasks[1].type != 0x25 && t->subtasks[0].type != 0x25;
                if (t->subtasks[2].type == 0x25 || !noTalk) continue;
            }
            g_list.push_back({t, nullptr, 0, 0});
        }
        std::stable_sort(g_list.begin(), g_list.end(), SortByEndTime);
        if (g_list.size() > 10) g_list.resize(10);
    }
    g_scroller.itemCount = (unsigned)g_list.size();

    const char32_t* name = StringTable::GetString("QUESTS_ALL_NAME");
    if (g_tab == kCompleted) name = StringTable::GetString("QUESTS_COMPLETED");
    else if (g_tab == kCity) name = StringTable::GetString("QUESTS_CITY_NAME");
    else if (g_tab == kBattle) name = StringTable::GetString("QUESTS_BATTLE_NAME");
    g_title->SetText(SWPrintf(0x100, name, {GameState::GetCastleName()}).c_str());
    for (unsigned i = 0; i < 5; ++i) g_tabs[i].SetMode(i == g_tab, false);
    g_tabs[4].root->SetVisibility(false);

    for (unsigned i = 0; i < kSlots; ++i) {
        Slot& s = g_slots[i];
        const unsigned index = i + g_scroller.firstIndex;
        s.root->SetVisibility(index < g_list.size());
        if (index >= g_list.size()) continue;
        if (g_list[index].task) SetSlotTask(s, g_list[index].task);
        else SetSlotGoblin(s, g_list[index]);
    }
    g_root->UpdatePosition();
}

// @0x361fcc (BattleBarWindow::OnAllTasks @0x24a048 is the same code)
void Show() {
    if (!GameState::IsPlayerCity()) return;
    g_queue->shown = true;
    g_queue->MoveWindowOnTop(true);
    if (!g_root->visibleSelf) {
        g_tween->AnimateIn();
        WindowManager::WindowShow(false);
    }
    g_root->SetVisibility(true);
    g_tab = kAll;
    g_scroller.ScrollToTop();
    UpdateContents();
    if (GameState::GetCurrentLocation() == 2) GameState::RaiseGamePauseState();
}

// @0x35eb20
void Hide() {
    if (g_root->visibleSelf) g_tween->AnimateOut();
}

// @0x35ecb0 TaskListWindow::RunLastHelpItem (Map::Load): after travelling for a slot, its tap runs
// again; after travelling to a goblin the arrow points at it; BuyGoblin's trip home ends in the
// hire window.
void RunLastHelpItem() {
    if (g_lastHelpItem != 0xffffffff) {
        OnTask(g_lastHelpItem);
        g_lastHelpItem = 0xffffffff;
    }
    if (g_goblin) {
        for (unsigned i = 0; (int)i < (int)GameState::GetOfflineGoblinCount(); ++i) {
            Entity* e = GameState::GetOfflineGoblin(i)->entity;
            if (e != g_goblin || e->GetCurrentMap() != (int)GameState::GetCurrentMapID()) continue;
            BuildingHovers::SetArrowVisibleWindowLimit(2);
            Tasks::HelpWithLocation((int)e->worldX, (int)(e->worldY - 50.f), e->tileX, e->tileY);
            break;
        }
        g_goblin = nullptr;
    }
    if (!g_buyGoblin) return;
    HireTroopsWindow::ShowForBuilding(0x12, false);
    HireTroopsWindow::ShowSlot((unsigned)GameState::GetGoblinCount());
    g_buyGoblin = false;
}

// @0x361df4 TaskListWindow::FindGoblin: the way to every goblin that has finished its job, then to
// the one with the least time left (on another map: the trip there first).
void FindGoblin() {
    OnTab(0);
    auto leadTo = [](Entity* e) {
        if ((int)GameState::GetCurrentMapID() == e->GetCurrentMap()) {
            Tasks::HelpWithLocation((int)e->worldX, (int)(e->worldY - 50.f), e->tileX, e->tileY);
        } else {
            g_goblin = e;
            Tasks::HelpWithMapSwitchTo((uint32_t)e->GetCurrentMap(), true, false, 0);
        }
    };
    for (unsigned i = 0; (int)i < (int)GameState::GetOfflineGoblinCount(); ++i) {
        GameState::OfflineGoblin* g = GameState::GetOfflineGoblin(i);
        if (g->entity && g->entity->GetCurrentMap() != 0 && g->timeLeft == 0) leadTo(g->entity);
    }
    unsigned best = 0xffffffff;
    int bestTime = 0;
    for (unsigned i = 0; (int)i < (int)GameState::GetOfflineGoblinCount(); ++i) {
        GameState::OfflineGoblin* g = GameState::GetOfflineGoblin(i);
        if (g->entity && g->entity->GetCurrentMap() != 0 && (best == 0xffffffff || (int)g->timeLeft < bestTime)) {
            best = i;
            bestTime = (int)g->timeLeft;
        }
    }
    if (best != 0xffffffff) leadTo(GameState::GetOfflineGoblin(best)->entity);
}

// @0x35eaf0 TaskListWindow::BuyGoblin: home, where RunLastHelpItem opens the hire window.
void BuyGoblin() {
    g_buyGoblin = true;
    Tasks::HelpWithMapSwitchTo(0, true, true, 0);
}

}  // namespace TaskListWindow
