// TaskHolderWindow: the quest list down the left side of the city: up to 32 quest buttons
// (Shared::TaskWindow) in a scroller, the "new quest" / "progress" banners beside them and the
// quick task description line. Port of TaskHolderWindow (libkingdom.so 5.11), 0x354364..0x359d68.
// HUDWindow::UpdateTasks @0x2c7ba8 is a jump to UpdateTasks here.
//
// Not ported: the expansion download task 0x5eb (online, Expansions) and ExecuteFirstHelp (the QA
// bot's, dead).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "engine/SoundsManager.h"
#include "engine/Render.h"
#include "engine/Resources.h"
#include "engine/Timer.h"
#include "game/BuildingHovers.h"
#include "game/BuildingMovement.h"
#include "game/BuildingPlacement.h"
#include "game/Combat.h"
#include "game/Entity.h"
#include "game/EntityManager.h"
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

namespace TaskHolderWindow {
namespace {

using GUI::Textfield;
using GUI::Window;

WindowManager::FunctionalWindow* g_queue = nullptr;

constexpr int kCount = 32;

// The quick task description: a line of text that folds out at the bottom of the screen.
struct QuickTaskWindow {         // 0x2c bytes, 0x62e08c
    Window* root = nullptr;      // +0x00 "Quick_task_line_holder"
    Window* holder = nullptr;    // +0x04 quick_task_line_hodler_9s
    Window* crown = nullptr;     // +0x08 quick_task_line_crown
    Textfield* text = nullptr;   // +0x0c text_quick_task
    GUI::ClipRect clip = {0, 0, 0, 0};   // +0x10 the text's
    bool in = false, out = false;        // +0x20 +0x21
    float time = 0.f;            // +0x24
    float stay = 2.f;            // +0x28 seconds before it folds away again
    void Init();                 // @0x355150
    void AnimateIn();            // @0x354fe8
    void AnimateOut();           // @0x354394
    void Update(float dt);       // @0x3543b4
};

struct Banner { Window* window = nullptr; Textfield* text = nullptr; };   // 8 bytes

// What the list last showed of a task: a change raises its banner.
struct KnownTaskInfo {           // 0x18 bytes
    uint32_t id = 0;
    int progress = 0;            // finished steps (Tasks::GetTaskProgress)
    int subProgress = 0;         // counted actions (Tasks::GetTaskSubProgress)
    int steps[3] = {};           // each step's counter
};

QuickTaskWindow g_quick;                         // 0x62e08c
Shared::TaskWindow g_windows[kCount];            // 0x62e0b8
Banner g_huge[kCount];                           // 0x62f5b8 "New_task_huge_container"
Banner g_small[kCount];                          // 0x62f6b8 "New_task_container"
int g_slotY[kCount] = {};                        // 0x62f7b8 each slot's y
Shared::ContentScroller g_scroller;              // 0x62f838
std::vector<KnownTaskInfo> g_known;              // 0x62f944
std::vector<Tasks::TaskInfo*> g_campaignTasks;   // 0x62f950 the campaign tasks, shown one at a time
Window* g_scrollBar = nullptr;                   // 0x62f95c "Scroll_bar_tiny_container"
unsigned g_campaignIndex = 0;                    // 0x62f960
float g_campaignTimer = 0.f;                     // 0x62f964
bool g_checkOverlimit = false;                   // 0x62f968
uint32_t g_lastSecond = 0;                       // 0x62f970 UpdateContents' second

bool g_arrowsShown = false;      // 0x60f3d8
int g_topY = 0;                  // 0x60f3dc the first button's y
int g_x = -5;                    // 0x60f3e0 the buttons' x
int g_bottomMargin = 0;          // 0x60f3e4 the list ends this far above the screen's bottom
int g_hugeDX = -60, g_hugeDY = -5;      // 0x60f3e8 0x60f3ec the large banner's place by its button
int g_smallDX = -30, g_smallDY = -5;    // 0x60f3f0 0x60f3f4
int g_gap = -25;                 // 0x60f3f8 between two buttons
int g_unusedFC = -25;            // 0x60f3fc
bool g_taskVisibility = true;    // 0x60f400
float g_scale = 1.f;             // 0x60f404

// @0x3548ec TaskHolderWindow::TaskOrdering: the higher priority first, then the later begun.
bool TaskOrdering(Tasks::TaskInfo* a, Tasks::TaskInfo* b) {
    if (a->priority == b->priority) return GameState::GetTaskBeginTime(b->id) < GameState::GetTaskBeginTime(a->id);
    return b->priority < a->priority;
}

// @0x354940 TaskHolderWindow::SetTaskCharacter: the quest's picture in every background's icon.
void SetTaskCharacter(Shared::TaskWindow& w, Render::Texture* image) {
    if (!image) return;
    unsigned size = (unsigned)w.icon->origW;   // (+0x24: the icon's layout size, unless the image is smaller)
    if (image->w <= (int)size) size = 0;
    w.icon->SetTexture(image, true, size, 0, true, 0);
    w.iconBattle->SetTexture(image, true, size, 0, true, 0);
    w.iconBattleSide->SetTexture(image, true, size, 0, true, 0);
    w.iconQuick->SetTexture(image, true, size, 0, true, 0);
    w.icon->ApplyMask(Resources::GetUIImage("quest_icon_left_mask.png", false, false), w.bg);
    w.iconBattle->ApplyMask(Resources::GetUIImage("quest_icon_left_mask.png", false, false), w.bgBattle);
    w.iconBattleSide->ApplyMask(Resources::GetUIImage("quest_icon_left_mask.png", false, false), w.bgBattleSide);
    w.iconQuick->ApplyMask(Resources::GetUIImage("quick_task_quest_holder_mask.png", false, false), w.quickBlue);
}

// @0x3591e8 TaskHolderWindow::OnTaskInfo: a quest button (or its banner) was tapped.
void OnTaskInfo(uint32_t id) {
    for (Shared::TaskWindow& w : g_windows)
        if (w.taskId == id) w.bannerTimer = 0.f;
    // (0x5eb: the expansion update, online.)
    if (id == 0x5fb) {
        ChoosePathWindow::Show();
        return;
    }
    // The topmost button (the first used one in place) with a campaign look opens its map's
    // campaign window; for the arena map 0x74 the world map's arrow at it, there the heal window
    // for a dead hero, else the arena events.
    Shared::TaskWindow* top = nullptr;
    for (Shared::TaskWindow& w : g_windows)
        if (w.taskId != 0 && w.target == 0) {
            top = &w;
            break;
        }
    if (top && top->taskId == id &&
        (top->campaign->visibleSelf || top->desert->visibleSelf || top->arena->visibleSelf ||
         top->campaignDone->visibleSelf)) {   // (FIXED 2026-10-26: the port read four other buttons)
        if (Tasks::TaskInfo* t = Tasks::GetTask(id)) {
            if (t->debugMap == 0x74) {
                if (GameState::GetCurrentMapID() != 0x74) {
                    GlobalMapWindow::Show();
                    GlobalMapWindow::ArrowAtMap(0x74, false, true);
                    UpdateTasks();
                    return;
                }
                if (EntityManager::GetPlayer()->IsDead()) {
                    HealSquadWindow::Show();
                    HealSquadWindow::ResetRecentDeaths();
                    return;
                }
                ArenaEventWindow::Show();
                return;
            }
            if (GlobalMapWindow::ShowCampaignWindowForMap(t->debugMap)) return;
        }
    }
    if (id != 0x5c9) {
        std::printf("TaskHolderWindow::OnTaskInfo() Showing task %d\n", id);
        TaskInfoWindow::SetTask(id, false);
        TaskInfoWindow::Show();
        return;
    }
    // The sultan's visit: the world map on him, with his arrival window.
    if (Map::GetCurrentFarm()) HUDWindow::ExitFarm();
    GlobalMapWindow::Show();
    GlobalMapWindow::CenterOnSultan();
    UpdateTasks();
    SultanArrivingWindow::Show();
}

}  // namespace

// @0x355150 TaskHolderWindow::QuickTaskWindow::Init
void QuickTaskWindow::Init() {
    float scale = GUI::GetHoverScaleFactor(1.f, 1.f) * (GUI::IsHighDPIVersion() ? 1.5f : 1.25f);
    root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Quick_task_line_holder.xml", "Quick_task_line_holder.png",
                           scale, 0, 0, 0, 0, false, 1.f);
    root->SetVisibility(false);
    holder = GUI::GetWindowTyped<Window>(root, "quick_task_line_hodler_9s");
    holder->border[1] = 2;
    holder->border[3] = 2;
    crown = GUI::GetWindowTyped<Window>(root, "quick_task_line_crown");
    text = GUI::GetWindowTyped<Textfield>(root, "text_quick_task");
    text->SetClipRect(&clip);
    clip = {holder->border[0], 0, holder->border[2], 0x800};
    in = out = false;
    time = 0.f;
    stay = 2.f;
}

// @0x354fe8 TaskHolderWindow::QuickTaskWindow::AnimateIn
void QuickTaskWindow::AnimateIn() {
    time = 0.f;
    out = false;
    in = true;
    root->SetVisibility(false);
    root->SetPosition((GUI::ScreenWidth() - root->w) / 2,
                      GUI::ScreenHeight() - root->h - (int)(GUI::GetHudScaleFactor() * 100.f));
}

// @0x354394 TaskHolderWindow::QuickTaskWindow::AnimateOut
void QuickTaskWindow::AnimateOut() {
    in = false;
    time = 0.f;
    out = true;
}

// @0x3543b4 TaskHolderWindow::QuickTaskWindow::Update
void QuickTaskWindow::Update(float dt) {
    // The text's width (100 without a built text).
    int textWidth = 100;
    if (text->sprite && text->sprite->tex) textWidth = text->sprite->tex->w;
    float scale = root->scale;
    int borders = holder->border[2] + holder->border[0];
    if (in) {
        if (!root->visibleSelf) root->SetVisibility(true);
        float t = time / 0.2f;
        int closed = (int)((float)(borders + 2) * scale);
        if (t > 1.f) t = 1.f;
        root->SetAlpha(t, true);
        int grow = (int)(scale * 42.f) - closed + textWidth;
        if (grow < 0) grow = 0;
        int width = (int)((float)grow * t) + closed;
        holder->SetSize((unsigned)width, (unsigned)holder->h);
        holder->SetAlpha(t, true);
        holder->SetPosition((GUI::ScreenWidth() - width) / 2, holder->y);
        float pad = root->scale * 20.f;
        clip.top = 0;
        clip.bottom = 0x800;
        clip.right = holder->w + holder->x - (int)pad;
        clip.left = (int)pad + holder->x;
        if (time > 0.2f) {
            in = false;
            time = 0.f;
        }
        root->UpdatePosition();
    } else if (out) {
        if (time >= 0.5f) {
            root->SetVisibility(false);
            out = false;
            time = 0.f;
        } else {
            root->SetAlpha(1.f - (time + time), true);
        }
    } else if (stay < time) {
        time = 0.f;
        AnimateOut();
        return;
    }
    if (dt > 0.05f) dt = 0.05f;
    time += dt;
}

// @0x355098 TaskHolderWindow::ShowQuickTaskDescription
void ShowQuickTaskDescription(const char32_t* text, float seconds) {
    g_quick.root->SetSize((unsigned)GUI::ScreenWidth(), (unsigned)g_quick.root->h);
    g_quick.text->SetSize((unsigned)GUI::ScreenWidth(), (unsigned)g_quick.text->h);
    g_quick.text->SetText(text);
    g_quick.crown->SetPosition((GUI::ScreenWidth() - g_quick.crown->w) / 2, g_quick.crown->y);
    g_quick.stay = seconds;
    g_quick.AnimateIn();
}

// @0x354674 TaskHolderWindow::HideQuickTaskDescription
void HideQuickTaskDescription() { g_quick.AnimateOut(); }

// Static FunctionalWindow("TaskHolderWindow", Init, Deinit, Click, SetZ, Hide, Move) @0x354690
WindowManager::FunctionalWindow* Queue() {
    if (!g_queue) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.setZ = SetZ;
        f.hide = Hide;
        f.click = Click;
        f.move = [](int x, int y) { g_scroller.Move(x, y); };   // Move @0x354d6c
        g_queue = new WindowManager::FunctionalWindow("TaskHolderWindow", std::move(f));
    }
    return g_queue;
}

// @0x355384 TaskHolderWindow::Init
void Init() {
    g_x = -5;
    g_topY = (int)(GUI::GetHudScaleFactor() * 65.f);
    g_bottomMargin = (int)(GUI::GetHudScaleFactor() * 170.f);
    if (GUI::ScreenHeight() > 600) g_bottomMargin += (int)(GUI::GetHudScaleFactor() * 35.f);
    g_unusedFC = -25;
    g_gap = -25;
    g_taskVisibility = true;
    g_smallDY = -5;
    g_hugeDX = -60;
    g_hugeDY = -5;
    g_smallDX = -30;
    g_scale = GUI::IsHighDPIVersion() ? GUI::GetHudScaleFactor() * 0.96f : 1.f;
    for (int i = 0; i < kCount; ++i) {
        Shared::TaskWindow& w = g_windows[i];
        Window* root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Slider_mission_scroll_bg.xml",
                                       "Slider_mission_scroll_bg.png", g_scale, 0, 0, 0, 0, false, 1.f);
        w.LoadFrom(root, nullptr);
        w.root->SetVisibility(false);
        w.bg->SetVisibility(false);
        w.bgBattle->SetVisibility(false);
        w.bgBattleSide->SetVisibility(false);
        w.campaign->SetVisibility(false);
        w.desert->SetVisibility(false);
        w.arena->SetVisibility(false);
        w.campaignDone->SetVisibility(false);
        w.raid->SetVisibility(false);
        g_huge[i].window = GUI::RegisterUI("../resource/kingdom_ui/1Original/New_task_huge_container.xml",
                                           "New_task_huge_container.png", g_scale, 0, 0, 0, 0, false, 1.f);
        g_huge[i].window->SetVisibility(false);
        g_huge[i].text = GUI::GetWindowTyped<Textfield>(g_huge[i].window, "new_task_huge.new_task_huge_anim.text01");
        g_small[i].window = GUI::RegisterUI("../resource/kingdom_ui/1Original/New_task_container.xml",
                                            "New_task_container.png", g_scale, 0, 0, 0, 0, false, 1.f);
        g_small[i].window->SetVisibility(false);
        g_small[i].text = GUI::GetWindowTyped<Textfield>(g_small[i].window, "new_task.n_t.text01");
    }
    g_quick.Init();
    g_known.clear();
    g_gap = (int)((float)g_gap * g_scale);
    g_unusedFC = (int)(g_scale * (float)g_unusedFC);
    g_hugeDX = (int)(g_scale * (float)g_hugeDX);
    g_smallDX = (int)(g_scale * (float)g_smallDX);
    g_hugeDY = (int)(g_scale * (float)g_hugeDY);
    g_smallDY = (int)(g_scale * (float)g_smallDY);
    g_scrollBar = GUI::RegisterUI("../resource/kingdom_ui/1Original/Scroll_bar_tiny_container.xml",
                                  "Scroll_bar_tiny_container.png", g_scale, 0, 0, 0, 0, false, 1.f);
    g_scroller.Init();
    g_scroller.track = nullptr;
    g_scroller.viewport.left = g_x;
    g_scroller.viewport.top = g_topY;
    g_scroller.viewport.bottom = GUI::ScreenHeight() - g_bottomMargin;
    g_scroller.viewport.right = g_windows[0].root->w + g_x;
    for (int i = 0; i < kCount; ++i) {
        g_windows[i].root->SetClipRect(&g_scroller.viewport);
        g_windows[i].root->ignoreClip = true;
        g_scroller.windows.push_back(g_windows[i].root);
    }
    g_scroller.itemCount = 1;
    g_scroller.noRecycle = true;
    g_scroller.onScroll = UpdateContents;
    g_scroller.unkA0 = 1;
    g_scroller.perLine = 1;
    g_scroller.horizontal = false;
    g_scroller.itemSize = (g_windows[0].root->h + g_gap) * kCount;
    g_scroller.padStart = (int)(GUI::GetHudScaleFactor() * 10.f);
    if (GUI::ScreenHeight() > 600) g_scroller.padEnd = (int)(GUI::GetHudScaleFactor() * 35.f);
    // (The original also stores the bar's container, g_scrollBar, in the scroller's +0x78; no
    // ContentScroller function reads it.)
    g_scroller.scrollbar = g_scrollBar->GetChild("scroll_bar_tiny");
    g_scroller.scrollbar->SetTexture(Resources::GetDirectImage("images/scroll.png"), false, 0, 0, false, 0);
    g_scroller.scrollbar->MoveWindow(g_windows[0].root->w - (int)(g_scale * 15.f), 0);
    g_scroller.scrollbar->SetVisibility(false);
    Queue()->fn.update = Update;
    Queue()->fn.show = Show;
    Queue()->fn.zRange = WindowManager::ReturnSmallZRange;
    g_scroller.fadeBar = true;
    g_scroller.barRatio = 0.75f;
}

// @0x355eb8 TaskHolderWindow::ResetTasks
void ResetTasks() {
    g_known.clear();
    for (Shared::TaskWindow& w : g_windows) {
        w.root->SetVisibility(false);
        w.taskId = 0;
        w.bannerTimer = 0.f;
    }
}

// @0x354ad8 TaskHolderWindow::RemoveTaskArrows (HUDWindow::RemoveTaskArrows @0x2c7bac calls it)
void RemoveTaskArrows() {
    if (!g_arrowsShown) return;
    g_windows[0].bannerTimer = 8.f;
    g_huge[0].text->SetText(StringTable::GetString("NEW_TASK_ARROW"));
    g_small[0].text->SetText(StringTable::GetString("NEW_TASK_ARROW"));
    for (int i = 1; i < kCount; ++i) {
        g_windows[i].bannerTimer = 0.f;
        g_huge[i].window->SetVisibility(false);
        g_small[i].window->SetVisibility(false);
    }
    g_arrowsShown = false;
}

// @0x354c04
void Show() {
    if (!GameState::IsPlayerCity()) return;
    Queue()->MoveWindowOnTop(true);
}

// @0x359668: outside the player's city the tasks are refreshed (UpdateTasks hides them).
void Hide() {
    if (GameState::IsPlayerCity()) return;
    UpdateTasks();
}

// @0x354c2c TaskHolderWindow::SetZ
void SetZ(float) {
    float z = HUDWindow::GetZ();
    g_scroller.viewport.top = g_topY;
    g_scroller.viewport.bottom = GUI::ScreenHeight() - g_bottomMargin;
    g_scroller.viewport.left = g_x;
    g_scroller.viewport.right = g_windows[0].root->w + g_x;
    for (int i = 0; i < kCount; ++i) {
        g_windows[i].root->SetZ(z + (float)g_windows[i].target * 0.0008f);
        g_windows[i].root->UpdatePosition();
        g_huge[i].window->SetZ(z - 0.01f);
        g_small[i].window->SetZ(z - 0.01f);
    }
    g_quick.root->SetZ(0.001f);
}

// @0x354d8c TaskHolderWindow::Click
bool Click(int x, int y, bool pressed) {
    if (g_scroller.Click(x, y, pressed, nullptr) && !pressed) return true;
    if (!BuildingMovement::Activated() && !BuildingPlacement::Activated() && !ShopWindow::IsVisible() &&
        !BottomScreenshotWindow::IsVisible()) {
        for (int i = 0; i < kCount; ++i) {
            if (y < GUI::ScreenHeight() - g_bottomMargin && g_windows[i].root->Click(x, y, pressed, false)) return true;
            if (g_huge[i].window->Click(x, y, pressed, false)) return true;
            if (g_small[i].window->Click(x, y, pressed, false)) return true;
        }
    } else {
        // While building or shopping the buttons only swallow the taps on them.
        for (int i = 0; i < kCount; ++i) {
            if (g_windows[i].root->GetWindowAtPosition(x, y, false)) return true;
            if (g_huge[i].window->GetWindowAtPosition(x, y, false)) return true;
            if (g_small[i].window->GetWindowAtPosition(x, y, false)) return true;
        }
    }
    return false;
}

// @0x358c70 TaskHolderWindow::UpdateContents: the icons and the time left, once a second and
// whenever the list scrolls.
void UpdateContents() {
    for (Shared::TaskWindow& w : g_windows) {
        w.root->UpdatePosition();
        SetTaskCharacter(w, w.icon->texture);
        // (0x5eb: the expansion download's percent, online)
        if (!w.root->visibleSelf) continue;
        Tasks::TaskInfo* t = Tasks::GetTask(w.taskId);
        if (t) {
            // A battle task of a mission map that is not a quick task shows the mission's time left
            // when it has no timeout of its own.
            if (t->showInBattle && t->mapId != 0 && Locations::IsMissionMap(t->mapId) && !t->quickTask) {
                if (t->HasTimeout() && t->GetRemainingTime() == 0) {
                    UpdateTasks();
                    return;
                }
                int left = t->HasTimeout() ? t->GetRemainingTime() : Locations::GetMissionRemainingTime(t->mapId);
                w.timeText->SetVisibility(true);
                w.timeText->SetText(StringTable::GetNumericTimeString(left, false).c_str());
            } else if (!t->HasTimeout()) {
                w.timeText->SetVisibility(false);
                w.root->UpdatePosition();
            } else {
                int left = t->GetRemainingTime();
                if (left == 0) {   // timed out: the list is made again
                    UpdateTasks();
                    return;
                }
                if (!t->quickTask) {
                    w.timeText->SetVisibility(true);
                    w.timeText->SetText(StringTable::GetNumericTimeString(t->GetRemainingTime(), t->id == 0x75b).c_str());
                } else {
                    w.timeText->SetVisibility(false);
                    w.root->UpdatePosition();
                }
            }
        }
        w.root->UpdatePosition();
    }
    if (g_checkOverlimit && GameState::GetCurrentMapID() != 0) {
        g_checkOverlimit = false;
        for (Shared::TaskWindow& w : g_windows)
            if (Tasks::TaskInfo* t = Tasks::GetTask(w.taskId)) Tasks::CheckOverlimit(&t, 1, true);
    }
}

// @0x356048 TaskHolderWindow::UpdateTasks: the list is made from the open tasks.
void UpdateTasks() {
    if (!g_windows[0].root) return;   // PORT: asked before the HUD exists (the original's order differs)
    // (While the expansion download window is up nothing changes: online.)
    Tasks::TaskInfo* list[0x80] = {};
    unsigned count = Tasks::GetCurrentTasks(list, 0x80, true, false);
    // On the arena map 0x74 the arena events' task 0x5da joins as the arena's campaign task
    // (priority 10, campaign 0xf) once an event was won or both are open, while one is ready.
    if (GameState::GetCurrentMapID() == 0x74 &&
        (GameState::GetEventCompletionCount(2) != 0 || GameState::GetEventCompletionCount(1) != 0 ||
         (ArenaEventWindow::IsEventUnlocked(1) && ArenaEventWindow::IsEventUnlocked(2))) &&
        (ArenaEventWindow::GetEventCooldownTime(1) == 0 || ArenaEventWindow::GetEventCooldownTime(2) == 0))
        if (Tasks::TaskInfo* t = Tasks::GetTask(0x5da)) {
            t->priority = 10;
            t->f_1e4 = true;
            t->campaign = 0xf;
            list[count++] = t;
        }
    if (GameState::GetCurrentLocation() == 3 && GameState::GetTaskBeginTime(0x43f) != 0)
        if (Tasks::TaskInfo* t = Tasks::GetTask(0x5fb)) {
            t->priority = 10;
            list[count++] = t;
        }
    // (Task 0x5eb, the expansion update: online.)
    if (BaseCombat* combat = GameState::GetActiveCombat())
        if (combat->GetType() > 2) count = 0;
    if (!GameState::IsPlayerCity()) count = 0;
    if (!g_taskVisibility) count = 0;
    if (GameState::GetCurrentLocation() == 3) {
        if (count != 0) {   // only the path choice (0x5fb) and the sultan (0x5c9)
            Tasks::TaskInfo* path = nullptr;
            Tasks::TaskInfo* sultan = nullptr;
            for (unsigned i = 0; i < count; ++i) {
                if (list[i]->id == 0x5c9) sultan = list[i];
                else if (list[i]->id == 0x5fb) path = list[i];
            }
            if (path) list[0] = path;
            count = path ? 1 : 0;
            if (sultan) list[count++] = sultan;
        }
    }
    for (unsigned i = 0; i < count; ++i)
        if (GameState::GetTaskBeginTime(list[i]->id) == 0)
            GameState::SetTaskBeginTime(list[i]->id, Timer::GetGlobalTime());

    // The campaign tasks share one place: one of them is shown, the others wait their turn.
    g_campaignTasks.clear();
    for (unsigned i = 0; i < count; ++i)
        if (list[i]->f_1e4) g_campaignTasks.push_back(list[i]);
    std::sort(g_campaignTasks.begin(), g_campaignTasks.end(), TaskOrdering);
    if (g_campaignTasks.size() <= g_campaignIndex) {
        g_campaignIndex = 0;
        g_campaignTimer = 0.f;
    }
    if (!g_campaignTasks.empty()) {
        Tasks::TaskInfo* shown = g_campaignTasks[g_campaignIndex];
        for (unsigned i = 0; i < count; ++i) {
            Tasks::TaskInfo* t = list[i];
            if (!t->f_1e4) {
                if (t->priority == 0x32) t->priority = 0x2d;
            } else {
                t->priority = t == shown ? 0x32 : -1;
            }
        }
    }
    std::sort(list, list + count, TaskOrdering);
    // The tail is cut: in the city the battle tasks without a priority, anywhere the campaign
    // tasks that wait (priority -1). One task always stays.
    if (count >= 2) {
        Tasks::TaskInfo* last = list[count - 1];
        bool counted = true;
        if (GameState::GetCurrentLocation() == 0) {
            while (last->showInBattle && !last->isCity && last->priority == 0) {
                --count;
                if (count == 1) {
                    counted = false;
                    break;
                }
                last = list[count - 1];
            }
        }
        if (counted && last->priority == -1) {
            --count;
            while (count > 1 && list[count - 1]->priority == -1) --count;
        }
    }
    if (count > kCount) count = kCount;

    // Every slot's place.
    g_scroller.snapPoints.clear();
    int y = g_topY;
    for (int i = 0; i < kCount; ++i) {
        g_slotY[i] = y;
        g_scroller.snapPoints.push_back(y - g_topY);
        y += g_gap + g_windows[i].root->h;
    }
    auto indexOf = [&](uint32_t id) {
        for (unsigned i = 0; i < count; ++i)
            if (list[i]->id == id) return (int)i;
        return -1;
    };
    // Buttons whose task has left the list are freed.
    for (Shared::TaskWindow& w : g_windows)
        if (w.taskId != 0 && indexOf(w.taskId) < 0) {
            w.root->SetVisibility(false);
            w.taskId = 0;
        }
    // The others move to their new slot.
    for (Shared::TaskWindow& w : g_windows) {
        int k = indexOf(w.taskId);
        if (k < 0) continue;
        if (w.slot != (unsigned)k || (unsigned)k != w.target) {
            w.target = (unsigned)k;
            w.Animate(g_x, g_slotY[w.slot], g_x, g_slotY[k]);
        }
    }
    // A new task takes the first free button and slides in from the left.
    for (unsigned k = 0; k < count; ++k) {
        uint32_t id = list[k]->id;
        bool has = false;
        for (const Shared::TaskWindow& w : g_windows)
            if (w.taskId == id) has = true;
        if (has) continue;
        Shared::TaskWindow* free = nullptr;
        for (Shared::TaskWindow& w : g_windows)
            if (w.taskId == 0) {
                free = &w;
                break;
            }
        if (!free) {
            std::fprintf(stderr, "ERROR: TaskHolderWindow::UpdateTasks() Cannot find an empty task holder to show task %d\n", id);
            continue;
        }
        free->taskId = id;
        free->root->SetVisibility(true);
        free->root->SetPosition(-free->root->w, g_scroller.offset + g_slotY[k]);
        free->slot = k;
        free->target = k;
        free->Animate(-free->root->w, g_slotY[k], g_x, g_slotY[k]);
    }

    for (int i = 0; i < kCount; ++i) {
        Shared::TaskWindow& w = g_windows[i];
        Tasks::TaskInfo* t = Tasks::GetTask(w.taskId);
        if (!t) continue;
        uint32_t id = t->id;
        w.clickArea->SetOnClick([id] { OnTaskInfo(id); });
        w.clickArea->SetActionSound("", 1);
        g_huge[i].window->SetOnClick([id] { OnTaskInfo(id); });
        g_small[i].window->SetOnClick([id] { OnTaskInfo(id); });

        // Which background: the top slot of the city shows a battle task as its campaign.
        // (On the arena map its events' task 0x5da always is one.)
        bool battle = GameState::GetCurrentLocation() == 0 && w.target == 0 &&
                      (t->showInBattle || (GameState::GetCurrentMapID() == 0x74 && t->id == 0x5da));
        bool showCampaign = false, campaignDone = false, raid = false;
        if (battle) {
            if (t->campaign != 0) {
                showCampaign = !t->sidequest;
                campaignDone = Locations::IsCampaignCompleted(Locations::GetCampaignLocation(t->campaign));
            }
            if (!t->sidequest && t->mission != 0) raid = true;
        }
        int kind = t->campaign == 5 ? 1 : t->campaign == 0xf ? 2 : 0;   // the desert, the arena
        // In the top slot a completed task of a campaign whose completion is recorded also counts
        // as done.
        if (w.target == 0 && GameState::TaskCompleted(t->id) && t->campaign != 0) {
            Locations::LocationInfo* loc = Locations::GetCampaignLocation(t->campaign);
            if (loc && GameState::GetCampaignCompletionStatus(loc->id)) campaignDone = true;
        }
        bool special = showCampaign || campaignDone || raid;
        if (t->f_1e4 && !special && t->campaign != 0) special = showCampaign = true;
        if (special) {
            w.bg->SetVisibility(false);
            w.bgBattle->SetVisibility(false);
            w.bgBattleSide->SetVisibility(false);
        } else {
            w.bg->SetVisibility(!t->showInBattle && !t->sidequest && !t->quickTask);
            w.bgBattle->SetVisibility(t->showInBattle && !t->sidequest && !t->quickTask);
            w.bgBattleSide->SetVisibility(t->sidequest && !t->quickTask);
        }
        bool campaignButton = showCampaign && !campaignDone;
        w.campaign->SetVisibility(campaignButton && kind == 0);
        w.desert->SetVisibility(campaignButton && kind == 1);
        w.arena->SetVisibility(campaignButton && kind == 2);
        w.campaignDone->SetVisibility(campaignDone);
        w.raid->SetVisibility(raid);
        w.quick->SetVisibility(!special && t->quickTask);
        w.quickBlue->SetVisibility(!special && !t->showInBattle && !t->sidequest && t->quickTask);
        w.quickGreen->SetVisibility(!special && t->sidequest && t->quickTask);
        w.quickOrange->SetVisibility(!special && t->showInBattle && !t->sidequest && t->quickTask);
        w.quickRed->SetVisibility(false);
        w.allQuestsBg->SetVisibility(false);
        w.campaignPercent->SetVisibility(false);
        w.desertPercent->SetVisibility(false);
        // A campaign button (not the arena's) shows how much of the campaign is done.
        if (Locations::LocationInfo* loc = campaignButton ? Locations::GetCampaignLocation(t->campaign) : nullptr) {
            if (kind != 2) {
                GUI::Textfield* percent = kind == 0 ? w.campaignPercent : w.desertPercent;
                percent->SetVisibility(true);
                float done = (float)(unsigned)Locations::GetLocationTaskCompleted(loc, false);
                float all = (float)(unsigned)Locations::GetLocationTaskCount(loc, false);
                // (no tasks: 0 / 0, which the original's float-to-int conversion turns into 0)
                percent->SetText(SWPrintf(0x40, U"%d%%", {all == 0.f ? 0 : (int)(done / all * 100.f)}).c_str());
            }
        }
        w.icon->SetVisibility(w.bg->visibleSelf);
        w.iconBattle->SetVisibility(w.bgBattle->visibleSelf);
        w.iconBattleSide->SetVisibility(w.bgBattleSide->visibleSelf);
        w.iconQuick->SetVisibility(w.quick->visibleSelf);
        SetTaskCharacter(w, t->GetIcon());

        uint32_t showTime = GameState::GetTaskShowTime(t->id);
        KnownTaskInfo* known = nullptr;
        for (KnownTaskInfo& k : g_known)
            if (k.id == t->id) known = &k;
        // A quick task shows its counted actions.
        int done = 0, total = 0;
        for (const Tasks::TaskSubtask& s : t->subtasks) {
            if (s.type == 0) continue;
            unsigned d = (unsigned)GameState::GetSubTaskDone(s.id);
            total += s.amount;
            done += (int)std::min(d, (unsigned)s.amount);
        }
        w.status->SetVisibility(w.quick->visibleSelf);
        w.statusShown = false;
        if (w.quick->visibleSelf) {
            w.status->SetText(SWPrintf(0x40, U"%d/%d", {done, total}).c_str());
            w.statusShown = w.quick->visibleSelf;
        }

        unsigned stepTotal = 0, subTotal = 0;
        auto record = [&](KnownTaskInfo& k) {
            k.id = t->id;
            k.progress = Tasks::GetTaskProgress(t->id, &stepTotal);
            k.subProgress = Tasks::GetTaskSubProgress(t->id, &subTotal);
            for (int s = 0; s < 3; ++s) k.steps[s] = GameState::GetSubTaskDone(t->subtasks[s].id);
        };
        if (showTime == 0) {   // never shown: the "new" banner
            w.isNew = true;
            w.bannerTimer = (GameState::IsFirstVirtualTutorial() || GameState::secondTutorial == 0x208 ||
                             GameState::secondTutorial == 0x20c) ? 32.f : 8.f;
            w.bannerSmall = false;
            g_huge[i].text->SetText(StringTable::GetString("NEW_TASK_ARROW"));
            g_small[i].text->SetText(StringTable::GetString("NEW_TASK_ARROW"));
            g_known.emplace_back();
            record(g_known.back());
        } else if (!known) {
            g_known.emplace_back();
            record(g_known.back());
        } else {
            int progress = Tasks::GetTaskProgress(t->id, &stepTotal);
            if (progress == known->progress || t->anySubquest) {
                // A step counted on: the small banner with that step's count.
                int sub = Tasks::GetTaskSubProgress(t->id, &subTotal);
                if (known->subProgress != sub && (unsigned)sub != subTotal) {
                    int changed = -1;
                    for (int s = 0; s < 3; ++s)
                        if (known->steps[s] != GameState::GetSubTaskDone(t->subtasks[s].id)) changed = s;
                    w.bannerTimer = 8.f;
                    const char32_t* format = StringTable::GetString("PROGRESS_ARROW");
                    if (changed == -1) {
                        w.bannerSmall = false;
                        int p = Tasks::GetTaskProgress(t->id, &stepTotal);
                        std::u32string a = ToWideString(p), b = ToWideString((int)stepTotal);
                        g_small[i].text->SetText(SWPrintf(0x40, format, {a.c_str(), b.c_str()}).c_str());
                    } else {
                        const Tasks::TaskSubtask& s = t->subtasks[changed];
                        unsigned d = std::min((unsigned)GameState::GetSubTaskDone(s.id), (unsigned)s.amount);
                        w.bannerSmall = true;
                        std::u32string a = ToWideString((int)d), b = ToWideString(s.amount);
                        g_small[i].text->SetText(SWPrintf(0x40, format, {a.c_str(), b.c_str()}).c_str());
                    }
                    known->subProgress = sub;
                    for (int s = 0; s < 3; ++s) known->steps[s] = GameState::GetSubTaskDone(t->subtasks[s].id);
                }
            } else if ((unsigned)progress != stepTotal) {
                // A step finished: the large banner with the finished steps.
                w.bannerTimer = 8.f;
                w.bannerSmall = false;
                const char32_t* format = StringTable::GetString("PROGRESS_ARROW");
                int p = Tasks::GetTaskProgress(t->id, &stepTotal);
                std::u32string a = ToWideString(p), b = ToWideString((int)stepTotal);
                std::u32string text = SWPrintf(0x40, format, {a.c_str(), b.c_str()});
                g_huge[i].text->SetText(text.c_str());
                g_small[i].text->SetText(text.c_str());
                known->progress = progress;
                known->subProgress = Tasks::GetTaskSubProgress(t->id, &subTotal);
            }
        }
        if (campaignDone && !battle) w.bannerTimer = 0.f;
    }
    g_scroller.itemSize = (g_windows[0].root->h + g_gap) * (int)count;
    for (unsigned i = 0; i < count; ++i)
        if (GameState::GetTaskShowTime(list[i]->id) == 0)
            GameState::SetTaskShowTime(list[i]->id, Timer::GetGlobalTime());
    if (GameState::tutorial < 0x66 || GameState::IsBossCombatActive()) {
        BattleBarWindow::HideAllTasks();
    } else {
        BattleBarWindow::ShowAllTasks();
        g_checkOverlimit = true;
        UpdateContents();
    }
}

// @0x359680 TaskHolderWindow::Update
void Update(float dt) {
    g_scroller.Update(dt);
    g_quick.Update(dt);
    bool newTaskSound = false;
    bool dialog = PlayerTopWindow::IsPlayerDialogVisible() || BuildingHovers::IsWorldDialogVisible();
    for (int i = 0; i < kCount; ++i) {
        Shared::TaskWindow& w = g_windows[i];
        Window* root = w.root;
        // The banners sway beside their button.
        int sway = (int)(10.0 + std::cos(Timer::GetTime() * 5.0) * 10.0);
        g_huge[i].window->SetPosition(g_hugeDX + g_x + root->w + sway,
                                      root->y + g_hugeDY + (root->h - g_huge[i].window->h) / 2);
        g_small[i].window->SetPosition(g_smallDX + g_x + root->w + sway,
                                       root->y + g_smallDY + (root->h - g_small[i].window->h) / 2);
        if (!dialog) w.bannerTimer -= dt;
        int centre = root->y + root->h / 2;
        if (centre < g_scroller.viewport.top || g_scroller.viewport.bottom < centre) {
            g_huge[i].window->SetVisibility(false);
            g_small[i].window->SetVisibility(false);
        } else {
            bool on = w.bannerTimer > 0.f && root->visibleSelf && !dialog && !w.animating;
            g_huge[i].window->SetVisibility(on && !w.statusShown && !w.bannerSmall);
            g_small[i].window->SetVisibility(on && (w.statusShown || w.bannerSmall));
        }
        if (w.bannerTimer <= 0.f) w.bannerSmall = false;
        if (!dialog && w.animating) {
            float t = w.animTime / w.animDuration;
            w.animTime += dt;
            if (t < 1.f) {
                float fx = (float)w.fromX, fy = (float)w.fromY;
                float px, py;
                if (w.animMode == 0) {
                    float e = std::sqrt(1.f - t * t);
                    px = fx + -((float)w.toX - fx) * (e - 1.f);
                    py = (float)g_scroller.offset + fy + -((float)w.toY - fy) * (e - 1.f);
                } else {
                    float e = std::sqrt(1.f - (t - 1.f) * (t - 1.f));
                    px = fx + ((float)w.toX - fx) * e;
                    py = (float)g_scroller.offset + fy + ((float)w.toY - fy) * e;
                }
                root->SetPosition((int)px, (int)py);
            } else {
                newTaskSound = newTaskSound || w.isNew;
                w.isNew = false;
                root->SetPosition(w.toX, g_scroller.offset + w.toY);
                w.animating = false;
                w.slot = w.target;
            }
        }
    }
    // Every 8 seconds the campaign button at the top slides out and the next campaign task
    // comes in.
    int top = -1;
    for (int i = 0; i < kCount; ++i)
        if (g_windows[i].root->visibleSelf && !g_windows[i].animating && g_windows[i].slot == 0) top = i;
    if (top != -1 && g_campaignTasks.size() > 1 && !g_huge[top].window->visibleSelf) {
        g_campaignTimer += dt;
        if (g_campaignTimer > 8.f) {
            Shared::TaskWindow& w = g_windows[top];
            if (!w.animating) {
                int out = -w.root->w;
                if (out < w.root->x) {
                    w.Animate(w.toX, w.toY, out, w.toY);
                    w.animMode = 1;
                    w.timeText->SetVisibility(false);
                } else {
                    ++g_campaignIndex;
                    UpdateTasks();
                    g_campaignTimer = 0.f;
                }
            }
        }
    }
    if (newTaskSound) {
        SoundsManager::PlaySound("ui_new_quest", 1, false);
    }
    if (Timer::GetGlobalTime() == g_lastSecond) return;
    g_lastSecond = Timer::GetGlobalTime();
    UpdateContents();
}

bool IsVisible() { return true; }   // FunctionalWindow default (no IsVisible function)

// @0x359648
void SetTaskVisibility(bool on) {
    bool was = g_taskVisibility;
    g_taskVisibility = on;
    if (was == on) return;
    UpdateTasks();
}

}  // namespace TaskHolderWindow
