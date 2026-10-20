// ChoosePathWindow: "where to next?" (Window_where_to_next.xml), the world map's Navigate button
// and its other openers. A strip of three buttons scrolls over six destinations (home, the
// campaigns, the missions, the winter lands, the desert, the shop), each with its picture, name, a
// progress or time line and a lock; a row of dots marks the one in the middle, whose description
// shows below. Two "featured" holders lead to PvP and the arena. Every choice ends on the world map
// (an arrow at the place, a travel) or in the window that helps with the next quest there.
// Port of ChoosePathWindow (libkingdom.so 5.11), 0x295170..0x298fa4 (UpdateContents is
// 0x296c14..0x298690; Update is 0x298690..0x298df0, Ghidra cut it at 0x298728 into a bogus
// "_INIT_78"). Statics at 0x61c244.
//
// The travel type (OnTravel) is the destination's slot: 0 home, 1 campaigns, 2 missions, 3 winter,
// 4 desert, 5 shop (the six regular buttons), 6 PvP, 7 the arena (the featured holders).
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <string>
#include <vector>

#include "engine/IconManager.h"
#include "engine/Timer.h"
#include "game/BuildingHovers.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/Locations.h"
#include "game/Professions.h"
#include "game/Map.h"
#include "game/Setting.h"
#include "game/StringTable.h"
#include "game/Tasks.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/ContentScroller.h"
#include "hud/HUD.h"
#include "windows/Windows.h"

namespace ChoosePathWindow {
namespace {

using GUI::Textfield;
using GUI::Window;

// One destination button (0x3c bytes), "where_to_next_holder.button_where_to_next_%02d".
struct ButtonWhereToNext {
    Window* root = nullptr;             // +0x00
    // +0x04..+0x1c the pictures, indexed by the background type: winter, shop, home, summer,
    // raids, desert, arena (the first six take a depth slot).
    Window* image[7] = {};
    Textfield* twoLines1 = nullptr;     // +0x20 text_2_lines_01: the name above a second line
    Textfield* twoLines2 = nullptr;     // +0x24 text_2_lines_02: the progress / mission time
    Textfield* oneLine = nullptr;       // +0x28 text_1_line: the name alone
    Textfield* threeLines = nullptr;    // +0x2c text_3_lines_01_small: the running mission's name
    Textfield* desc = nullptr;          // +0x30 text_location_desc
    Window* lock = nullptr;             // +0x34
    Window* icon = nullptr;             // +0x38 icon_35 (takes a depth slot)

    // @0x2960dc
    void LoadFrom(Window* parent, const char* prefix) {
        static const char* const kImages[7] = {"winter", "shop", "home", "summer", "raids", "desert", "arena"};
        root = GUI::GetWindowTypedF<Window>(parent, "%s", prefix);
        for (int k = 0; k < 7; ++k) {
            char name[64];
            std::snprintf(name, sizeof name, "%%s.image_where_to_%s", kImages[k]);
            image[k] = GUI::GetWindowTypedF<Window>(parent, name, prefix);
        }
        for (int k = 0; k < 6; ++k) image[k]->takesZ = true;
        twoLines1 = GUI::GetWindowTypedF<Textfield>(parent, "%s.text_2_lines_01", prefix);
        twoLines2 = GUI::GetWindowTypedF<Textfield>(parent, "%s.text_2_lines_02", prefix);
        oneLine = GUI::GetWindowTypedF<Textfield>(parent, "%s.text_1_line", prefix);
        threeLines = GUI::GetWindowTypedF<Textfield>(parent, "%s.text_3_lines_01_small", prefix);
        desc = GUI::GetWindowTypedF<Textfield>(parent, "%s.text_location_desc", prefix);
        lock = GUI::GetWindowTypedF<Window>(parent, "%s.lock", prefix);
        icon = GUI::GetWindowTypedF<Window>(parent, "%s.icon_35", prefix);
        icon->takesZ = true;
    }

    // @0x295170: only the picture of that type shows; a picture not loaded yet takes src's.
    void SetBackground(unsigned type, const ButtonWhereToNext* src) {
        for (unsigned k = 0; k < 7; ++k) {
            image[k]->SetVisibility(type == k);
            if (src && !image[k]->texture && src->image[k]->texture) image[k]->SetTexture(src->image[k]->texture);
        }
    }
};

// A featured destination (0x24 bytes), "where_to_featured_holder_%02d": 1 PvP, 2 the arena.
struct WhereToFeaturedHolder {
    Window* root = nullptr;             // +0x00
    Window* arena = nullptr;            // +0x04 where_to_next_small_arena (takes a depth slot)
    Window* pvp = nullptr;              // +0x08 where_to_next_small_pvp (takes a depth slot)
    Textfield* oneLine = nullptr;       // +0x0c text_1_line
    Textfield* twoLines2 = nullptr;     // +0x10 text_2_lines_02
    Window* lock = nullptr;             // +0x14
    Window* icon = nullptr;             // +0x18 icon_60 (takes a depth slot)
    Textfield* desc = nullptr;          // +0x1c text_desc
    Textfield* desc2 = nullptr;         // +0x20 text_desc_02

    // @0x295fb4
    void LoadFrom(Window* parent, const char* prefix) {
        root = GUI::GetWindowTypedF<Window>(parent, "%s", prefix);
        arena = GUI::GetWindowTypedF<Window>(parent, "%s.where_to_next_small_arena", prefix);
        arena->takesZ = true;
        pvp = GUI::GetWindowTypedF<Window>(parent, "%s.where_to_next_small_pvp", prefix);
        pvp->takesZ = true;
        oneLine = GUI::GetWindowTypedF<Textfield>(parent, "%s.text_1_line", prefix);
        twoLines2 = GUI::GetWindowTypedF<Textfield>(parent, "%s.text_2_lines_02", prefix);
        lock = GUI::GetWindowTypedF<Window>(parent, "%s.lock", prefix);
        icon = GUI::GetWindowTypedF<Window>(parent, "%s.icon_60", prefix);
        icon->takesZ = true;
        desc = GUI::GetWindowTypedF<Textfield>(parent, "%s.text_desc", prefix);
        desc2 = GUI::GetWindowTypedF<Textfield>(parent, "%s.text_desc_02", prefix);
    }
};

// The campaign lists (0x593688): the campaigns button's, the winter lands', the desert's.
const uint32_t kCampaigns[4] = {2, 3, 4, 0xb};
const uint32_t kWinter[1] = {9};
const uint32_t kDesert[2] = {5, 0x3ea};

// The campaigns button's quest helps (0x5936a0, 0xc bytes each): the first started, unfinished
// quest whose prerequisite (if any) is done gets the help arrow (help) or its info window.
struct QuestHelp { uint32_t task; bool help; uint32_t requires; };
const QuestHelp kQuestHelps[15] = {
    {0x429, false, 0}, {0x26a, false, 0}, {0x35a, false, 0}, {0x35c, false, 0}, {0x43f, true, 0},
    {0x45d, true, 0},  {0x2f1, false, 0}, {0x35e, false, 0}, {0x360, false, 0}, {0x2da, false, 0},
    {0x47f, true, 0},  {0x2a7, false, 0}, {0x482, true, 0},  {0x484, false, 0}, {0x485, true, 0x36d},
};

WindowManager::FunctionalWindow* g_wnd = nullptr;   // ChoosePathWindow::wnd 0x61c500
float g_scale = 1.f;                   // 0x60f1c8
Window* g_root = nullptr;              // 0x61c244
GUI::TweenEffect* g_tween = nullptr;   // 0x61c248
bool g_showOnLoad = false;             // 0x61c24c RunShowOnLoad opens it after the next map load
Shared::ContentScroller g_scroller;    // 0x61c250
Window* g_title = nullptr;             // 0x61c35c
Window* g_titleCompact = nullptr;      // 0x61c360 black_param_bg_01
ButtonWhereToNext g_buttons[3];        // 0x61c364
Window* g_nodeOn[11] = {};             // 0x61c418 the dots under the strip
Window* g_nodeOff[11] = {};            // 0x61c444
WhereToFeaturedHolder g_featured[2];   // 0x61c470 PvP, the arena
std::vector<unsigned> g_options;       // 0x61c4b8 the background type of each slot
unsigned g_missionIndex = 0;           // 0x61c4c8 the running mission shown (they take turns)
float g_arenaTime = 0.f;               // 0x61c4cc the arena holder's two events take turns
bool g_compact = false;                // 0x61c4d0 a screen up to 800x480
Window* g_close = nullptr;             // 0x61c4d4 x_button
Window* g_closeCompact = nullptr;      // 0x61c4d8 x_button_compact
Textfield* g_header = nullptr;         // 0x61c4dc text_header
Textfield* g_headerCompact = nullptr;  // 0x61c4e0 text_header_compact
Window* g_nodeLine = nullptr;          // 0x61c4e4 updater_node_line
Textfield* g_desc = nullptr;           // 0x61c4e8 the middle destination's description
Window* g_left = nullptr;              // 0x61c4ec arrow_left
Window* g_right = nullptr;             // 0x61c4f0 arrow_right
unsigned g_current = 0;                // 0x61c4f4 the slot in the middle
float g_missionTime = 0.f;             // 0x61c4f8 the mission line's fade
int g_lastSecond = 0;                  // 0x61c4fc

// The slot in the middle of the strip.
unsigned CurrentSlot() {
    const float f = ((float)g_scroller.firstIndex - (float)g_scroller.offset / (float)g_scroller.itemSize) + 0.5f;
    return f > 0.f ? (unsigned)f : 0;
}

// A location of the mission kind that can be played: open, unlocked, with a map.
bool IsOpenMission(Locations::LocationInfo* loc) {
    return loc->mission && !loc->disabled && !Locations::IsLocationLocked(loc) && loc->startMap != 0;
}

// An open mission that runs now (time left, and started).
bool IsRunningMission(Locations::LocationInfo* loc) {
    return IsOpenMission(loc) && Locations::GetMissionRemainingTime(loc->startMap) != 0 &&
           GameState::GetMapStartTime(loc->startMap) != 0;
}

// "%d%%" of a progress (the ARM conversion of NaN, a campaign list without quests, gives 0).
int Percent(float p) { return std::isnan(p) ? 0 : (int)(p * 100.f + 0.5f); }

// @0x295364
void OnTweenIn() {}

// @0x295530
void OnTweenOut() {
    GameState::DropGamePauseState();
    WindowManager::WindowHide(false);
    g_wnd->shown = false;
    g_wnd->MoveWindowDown(true);
}

// @0x295254
void SetZ(float z) {
    if (!g_wnd->shown) return;
    g_root->SetZ(z);
    g_root->SetPosition(g_tween->GetCenteredX(), g_tween->GetCenteredY());
}

// @0x29530c / @0x295338: the strip moves one button (and a pixel).
void OnLeft() { g_scroller.bounce += (float)(g_scroller.itemSize + 1); }
void OnRight() { g_scroller.bounce -= (float)(g_scroller.itemSize + 1); }

// @0x295564: the campaigns' completed share of their quests; anyUnlocked: one of them is open.
float GetCampaignProgress(const uint32_t* campaigns, unsigned n, bool& anyUnlocked) {
    unsigned done = 0, total = 0;
    anyUnlocked = false;
    for (unsigned i = 0; i < n; ++i) {
        Locations::LocationInfo* loc = Locations::GetCampaignLocation(campaigns[i]);
        if (!loc) continue;
        if (!Locations::IsLocationLocked(loc)) anyUnlocked = true;
        total += (unsigned)Locations::GetLocationTaskCount(loc, false);
        done += (unsigned)Locations::GetLocationTaskCompleted(loc, false);
    }
    return (float)done / (float)total;
}

// @0x2955fc: the help arrow for a quest's subtask, after travelling to its map when it is not here.
void HelpWithTask(uint32_t task, unsigned sub) {
    Tasks::TaskInfo* t = Tasks::GetTask(task);
    if (!t) return;
    if (Map::GetCurrentFarm() && !t->isFarm) HUDWindow::ExitFarm();
    if (t->subtasks[sub].type != 0x1e) {
        const bool cityHere = t->isCity && GameState::GetCurrentMapID() == 0;
        if (!cityHere) {
            for (uint32_t map : {t->mapId, t->debugMap}) {
                if (map != 0 && (uint32_t)GameState::GetCurrentMapID() != Tasks::NormalizeMapID(map)) {
                    Tasks::HelpWithMapSwitchTo(Tasks::NormalizeMapID(map), true, false, 0);
                    return;
                }
            }
        }
        GlobalMapWindow::Hide();
    }
    BuildingHovers::SetArrowVisibleWindowLimit(1);
    t->subtasks[sub].OnHelp(true);
}

// Opens the world map (if it is not up) with the arrow limit of a dialog over it.
void ShowGlobalMap() {
    if (!GlobalMapWindow::IsVisible()) GlobalMapWindow::Show();
    GlobalMapWindow::SetArrowVisibleWindowLimit(2);
}

// @0x295764: the destination of a slot (see the file's head).
void OnTravel(unsigned type) {
    const uint32_t* list = kDesert;
    unsigned n = 2;
    if (type == 2) {
        // The missions: the running one the button shows, else every open one.
        ShowGlobalMap();
        GlobalMapWindow::CenterOnMap(0);
        GlobalMapWindow::HideArrow();
        unsigned k = g_missionIndex;
        for (unsigned i = 0; Locations::LocationInfo* loc = Locations::EnumLocationInfo(i); ++i) {
            if (!IsRunningMission(loc)) continue;
            if (k == 0) {
                GlobalMapWindow::ArrowAtMap(loc->startMap, false, true);
                return;
            }
            --k;
        }
        bool any = false;
        for (unsigned i = 0; Locations::LocationInfo* loc = Locations::EnumLocationInfo(i); ++i) {
            if (!IsOpenMission(loc)) continue;
            GlobalMapWindow::ArrowAtMap(loc->startMap, true, false);
            any = true;
        }
        if (!any) return;
        GlobalMapWindow::CenterOnArrows();
        HUDWindow::UpdateTasks();
        Hide();
        return;
    }
    if (type == 1) {
        for (const QuestHelp& h : kQuestHelps) {
            if (GameState::GetTaskBeginTime(h.task) == 0 || GameState::TaskCompleted(h.task)) continue;
            if (h.requires != 0 && !GameState::TaskCompleted(h.requires)) continue;
            if (h.help) {
                HelpWithTask(h.task, 0);
            } else {
                TaskInfoWindow::SetTask(h.task, false);
                TaskInfoWindow::Show();
            }
            Hide();
            return;
        }
        list = kCampaigns;
        n = 4;
    } else if (type == 3) {
        if (!GameState::TaskCompleted(0x301)) {
            HelpWithTask(0x301, 0);
            Hide();
            return;
        }
        list = kWinter;
        n = 1;
    } else if (type == 4) {
        // The desert: locked until the sultan's quest 0x5c9 starts, then his arrival, its quests.
        if (GameState::GetTaskBeginTime(0x5c9) == 0 && !GameState::TaskCompleted(0x5c9)) {
            PopupWindow::Show(StringTable::GetString("LOCATION_LOCKED"), PopupWindow::Hide, nullptr, nullptr);
            return;
        }
        if (!GameState::TaskCompleted(0x5c9)) {
            if (!GlobalMapWindow::IsVisible()) GlobalMapWindow::Show();
            SultanArrivingWindow::Show();
            HUDWindow::UpdateTasks();
            Hide();
            return;
        }
        if (!GameState::TaskCompleted(0x591)) {
            HelpWithTask(0x591, 0);
            Hide();
            return;
        }
        if (GameState::TaskCompleted(0x562) && !GameState::TaskCompleted(0x608)) {
            if (GameState::GetCurrentMapID() == 0xb) {
                if (GlobalMapWindow::IsVisible()) GlobalMapWindow::Hide();
                if (Tasks::TaskInfo* t = Tasks::GetTask(0x608)) {
                    BuildingHovers::SetArrowVisibleWindowLimit(1);
                    t->subtasks[0].OnHelp(true);
                }
            } else {
                ShowGlobalMap();
                GlobalMapWindow::ArrowAtMap(0xb, false, true);
            }
            Hide();
            return;
        }
    } else {
        if (type == 0) {
            // Home: the arrow at the city (nothing in the city itself).
            if (GameState::GetCurrentMapID() != 0 || GlobalMapWindow::IsVisible()) {
                ShowGlobalMap();
                GlobalMapWindow::ArrowAtMap(0, false, true);
            }
        } else if (type == 5) {
            if (GameState::TaskCompleted((uint32_t)Setting("shop_unlock_task").GetInt())) ItemShopWindow::Show();
        } else if (type == 6) {
            LevelInfoWindow::HelpWithPvP(true);
        } else if (type == 7) {
            // The arena: its event of the moment (1 or 2, five seconds each).
            if (Locations::LocationInfo* loc = Locations::GetLocationInfo(0x49)) {
                const uint32_t map = GlobalMapWindow::GetNearestMapToMap(loc->startMap);
                const unsigned event = std::fmod(g_arenaTime, 10.f) < 5.f ? 1 : 2;
                if (map == (uint32_t)GameState::GetCurrentMapID()) {
                    if (GlobalMapWindow::IsVisible()) GlobalMapWindow::Hide();
                    LevelInfoWindow::HelpWithArena(event);
                } else if (!Locations::IsLocationLocked(loc)) {
                    ShowGlobalMap();
                    if (!Locations::IsLocationLocked(loc) && GameState::TaskCompleted(0x773)) {
                        if (ArenaEventWindow::IsEventUnlocked(event)) GlobalMapWindow::EnqueueArenaEventWindow(event);
                        GlobalMapWindow::SwitchMapAnimated(map, false, 500.f, false);
                    } else {
                        GlobalMapWindow::ArrowAtMap(map, false, true);
                        HUDWindow::UpdateTasks();
                    }
                } else {
                    GameState::TryToHelpWithTask(0x509);
                }
            }
        }
        Hide();
        return;
    }
    // The first unfinished campaign of the list (by IsCampaignCompleted, then by the completion
    // status), else its last one: the arrow at its nearest map when it is unlocked.
    Locations::LocationInfo* loc = nullptr;
    for (unsigned i = 0; i < n; ++i) {
        if (loc) continue;
        if (Locations::LocationInfo* c = Locations::GetCampaignLocation(list[i]))
            loc = Locations::IsCampaignCompleted(c) ? nullptr : c;
    }
    for (unsigned i = 0; i < n; ++i) {
        if (loc) continue;
        if (Locations::LocationInfo* c = Locations::GetCampaignLocation(list[i]))
            loc = GameState::GetCampaignCompletionStatus(c->id) ? nullptr : c;
    }
    if (!loc) loc = Locations::GetCampaignLocation(list[n - 1]);
    if (loc && !Locations::IsLocationLocked(loc)) {
        const uint32_t map = GlobalMapWindow::GetNearestMapToMap(loc->startMap);
        ShowGlobalMap();
        GlobalMapWindow::ArrowAtMap(map == 0x2f ? 0 : map, false, true);
        HUDWindow::UpdateTasks();
    }
    Hide();
}

// @0x295df4
void OnTravelFeatured(unsigned i) { OnTravel(i + 6); }

// @0x295dfc: a button cut by the strip's edge (less than 100 pixels in view) scrolls in instead.
void OnTravelRegular(unsigned i) {
    const Window* mask = g_scroller.track;
    const Window* b = g_buttons[i].root;
    const int minVisible = (int)(g_scale * 100.f);
    if (mask->w + mask->x - b->x < minVisible) {
        g_scroller.bounce -= (float)g_scroller.itemSize;
    } else if (b->w + b->x - mask->x < minVisible) {
        g_scroller.bounce += (float)g_scroller.itemSize;
    } else {
        OnTravel(i + g_scroller.firstIndex);
    }
}

// @0x29573c
bool OnBack() {
    if (!g_wnd->shown) return false;
    Hide();
    return true;
}

// @0x295eb8
void Move(int x, int y) { g_scroller.Move(x, y); }

// @0x295ed4
bool Click(int x, int y, bool pressed) {
    if (g_wnd->shown && g_scroller.Click(x, y, pressed, g_root)) return true;
    const bool visible = g_root->visibleSelf;
    g_root->Click(x, y, pressed, false);
    return visible;
}

// @0x295f74
void Deinit() {
    GUI::RemoveTweenEffect(g_tween);
    g_tween = nullptr;
    delete g_root;
}

// @0x298690 (with the bogus "_INIT_78" 0x298728..0x298df0, its rest)
void Update(float dt) {
    if (!g_wnd->shown) return;
    g_scroller.Update(dt);
    g_left->SetEnabled(g_scroller.CanMoveLeft());
    g_right->SetEnabled(g_scroller.CanMoveRight());
    const unsigned cur = CurrentSlot();
    if (g_current != cur) UpdateContents();
    g_current = cur;
    // The middle slot's dot fades in, the others out (4 per second).
    for (unsigned k = 0; k < 11; ++k) {
        Window* w = g_nodeOn[k];
        const float target = k == cur ? 1.f : 0.f;
        float a = w->alpha;
        if (target != a) {
            if (target <= a) {
                if (target < a) {
                    a -= dt * 4.f;
                    if (a < target) a = target;
                }
            } else {
                a += dt * 4.f;
                if (target < a) a = target;
            }
        }
        w->SetAlpha(a, false);
    }
    // The arena holder: its two events take turns every five seconds, faded in and out.
    g_arenaTime += dt;
    const unsigned event = std::fmod(g_arenaTime, 10.f) < 5.f ? 1 : 2;
    const float phase = std::fmod(g_arenaTime, 5.f);
    const float alpha = phase < 0.5f ? phase + phase : (phase < 4.5f ? 1.f : (5.f - phase) + (5.f - phase));
    WhereToFeaturedHolder& arena = g_featured[1];
    arena.icon->SetAlpha(alpha, false);
    arena.desc->SetAlpha(alpha, false);
    arena.desc2->SetAlpha(alpha, false);
    if (Items::ItemInfo* item = Items::GetItemInfo(event == 1 ? 0x1bb : 0x1ba))
        arena.icon->SetTexture(item->GetIcon(), true);
    // The event's reputation (2 for event 1, 1 for event 2): its name, and "Resume (time left)" or
    // "Start".
    if (const Professions::Profession* p = Professions::GetProfession(event == 1 ? 2 : 1)) {
        const std::u32string left = StringTable::GetTimeString((int)ArenaEventWindow::GetEventRemainingTime(event), false);
        arena.desc->SetText(p->title);
        arena.desc2->SetText(ArenaEventWindow::IsEventActive(event)
                                 ? ToWideString(U"%s (%s)", {StringTable::GetString("CAMPAIGN_INFO_RESUME"), left.c_str()})
                                 : StringTable::GetString("START_CAMP"));
    }
    // The running missions take turns every five seconds; their lines fade.
    const unsigned oldIndex = g_missionIndex;
    g_missionTime += dt;
    if (5.f < g_missionTime) {
        ++g_missionIndex;
        g_missionTime = 0.f;
    }
    for (unsigned i = 0; i < 3; ++i) {
        const unsigned slot = i + g_scroller.firstIndex;
        if (slot >= g_options.size() || g_options[slot] != 4) continue;
        const float t = g_missionTime;
        const float a = t < 0.5f ? t + t : (t <= 4.5f ? 1.f : (5.f - t) + (5.f - t));
        g_buttons[i].threeLines->SetAlpha(a, false);
        g_buttons[i].twoLines2->SetAlpha(a, false);
    }
    const int now = (int)Timer::GetGlobalTime();
    if (now != g_lastSecond || oldIndex != g_missionIndex) UpdateContents();
    g_lastSecond = now;
}

}  // namespace

// Static FunctionalWindow("ChoosePathWindow", Init, Deinit, Click, SetZ, Hide, Move) (_INIT_
// 0x295378, which also constructs the scroller and the vectors).
WindowManager::FunctionalWindow* Queue() {
    if (!g_wnd) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.deinit = Deinit;
        f.click = Click;
        f.setZ = SetZ;
        f.hide = Hide;
        f.move = Move;
        g_wnd = new WindowManager::FunctionalWindow("ChoosePathWindow", std::move(f));
    }
    return g_wnd;
}

// @0x2952e0
bool IsVisible() { return g_wnd->shown; }

// @0x2952f4: it opens after the next map load (CampaignCompleteWindow's end).
void EnqueueShowOnLoad() { g_showOnLoad = true; }

// @0x298f74 (Map::Load)
void RunShowOnLoad() {
    if (g_showOnLoad) Show();
    g_showOnLoad = false;
}

// @0x2962b8
void Init() {
    // Up to 800x480 the compact layout (its own title and close button).
    int offX = -0x15, offY = -0x20;
    g_compact = GUI::ScreenWidth() <= 0x320 || GUI::ScreenHeight() <= 0x1e0;
    if (g_compact) offX = -0x22, offY = -0x28;
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Window_where_to_next.xml", "Window_where_to_next.png",
                             -1.f, 0, 0, offX, offY, false, 1.f);
    g_wnd->RegisterTopWindow(g_root);
    g_scale = g_root->scale;
    g_root->SetVisibility(false);
    g_tween = GUI::CreateTweenEffect(g_root, nullptr, OnTweenIn, OnTweenOut);
    if (g_compact)
        g_tween->CenterWith(0, (int)(g_scale * 33.f), (int)(-11.f * g_scale), (int)(g_scale * -3.f));
    else
        g_tween->CenterWith(0, (int)(g_scale * 13.f), (int)(0.f * g_scale), (int)(g_scale * -3.f));
    g_close = GUI::GetWindowTyped<Window>(g_root, "x_button");
    g_close->SetOnClick(Hide);
    g_close->SetActionSound("ui_close", 1);
    g_closeCompact = GUI::GetWindowTyped<Window>(g_root, "x_button_compact");
    g_closeCompact->SetOnClick(Hide);
    g_closeCompact->SetActionSound("ui_close", 1);
    g_title = GUI::GetWindowTyped<Window>(g_root, "title");
    g_titleCompact = GUI::GetWindowTyped<Window>(g_root, "black_param_bg_01");
    g_header = GUI::GetWindowTyped<Textfield>(g_root, "text_header");
    g_headerCompact = GUI::GetWindowTyped<Textfield>(g_root, "text_header_compact");
    char name[128];
    for (unsigned i = 0; i < 3; ++i) {
        std::snprintf(name, sizeof name, "where_to_next_holder.button_where_to_next_%02d", i + 1);
        g_buttons[i].LoadFrom(g_root, name);
        g_buttons[i].root->SetOnClick([i] { OnTravelRegular(i); });
        g_buttons[i].root->SetActionSound("ui_click", 1);
    }
    // The three buttons side by side, 5 pixels apart.
    g_buttons[1].root->MoveWindow(g_buttons[0].root->w + 5, 0);
    g_buttons[2].root->MoveWindow((g_buttons[0].root->w + 5) * 2, 0);
    g_nodeLine = GUI::GetWindowTyped<Window>(g_root, "updater_node_line");
    for (int k = 0; k < 11; ++k)
        g_nodeOn[k] = GUI::GetWindowTypedF<Window>(
            g_root, "updater_node_line.updater_pic_node_holder_%02d.updater_pic_node_on", k + 1);
    for (int k = 0; k < 11; ++k)
        g_nodeOff[k] = GUI::GetWindowTypedF<Window>(
            g_root, "updater_node_line.updater_pic_node_holder_%02d.updater_pic_node_off", k + 1);
    for (unsigned i = 0; i < 2; ++i) {
        std::snprintf(name, sizeof name, "where_to_featured_holder_%02d", i + 1);
        g_featured[i].LoadFrom(g_root, name);
        g_featured[i].root->SetOnClick([i] { OnTravelFeatured(i); });
        g_featured[i].root->SetActionSound("ui_click", 1);
    }
    g_desc = GUI::GetWindowTyped<Textfield>(g_root, "text_desc");
    g_left = GUI::GetWindowTyped<Window>(g_root, "arrow_left");
    g_left->SetOnClick(OnLeft);
    g_left->SetActionSound("ui_click", 1);
    g_right = GUI::GetWindowTyped<Window>(g_root, "arrow_right");
    g_right->SetOnClick(OnRight);
    g_right->SetActionSound("ui_click", 1);
    g_scroller.Init();
    Window* mask = GUI::GetWindowTyped<Window>(g_root, "where_to_next_holder.mask_unviersal");
    g_scroller.viewport = {mask->x, mask->y, mask->x + mask->w, mask->y + mask->h};
    g_scroller.track = mask;
    for (ButtonWhereToNext& b : g_buttons) {
        b.root->SetClipRect(&g_scroller.viewport);
        g_scroller.windows.push_back(b.root);
    }
    // One button per line; the strip's ends pad so a button can sit in the middle.
    const int pad = (mask->w - g_buttons[0].root->w) / 2;
    g_scroller.horizontal = true;
    g_scroller.perLine = 1;
    g_wnd->hideOnOuterClick = true;
    g_scroller.unkA0 = 3;
    g_scroller.onScroll = UpdateContents;
    g_scroller.padStart = pad;
    g_scroller.itemSize = g_buttons[1].root->x - g_buttons[0].root->x;
    g_scroller.padEnd = pad;
    g_wnd->fn.zRange = WindowManager::ReturnSmallZRange;
    g_wnd->fn.back = OnBack;
    g_wnd->fn.update = Update;
    g_wnd->fn.show = Show;
    g_scroller.HandleMove(pad);
}

// @0x296c14
void UpdateContents() {
    g_title->SetVisibility(!g_compact);
    g_titleCompact->SetVisibility(g_compact);
    g_close->SetVisibility(!g_compact);
    g_closeCompact->SetVisibility(g_compact);
    g_header->SetVisibility(!g_compact);
    g_headerCompact->SetVisibility(g_compact);
    g_header->SetText(StringTable::GetString("CHOOSE_PATH_SELECT"));
    g_headerCompact->SetText(StringTable::GetString("CHOOSE_PATH_SELECT"));
    unsigned openMissions = 0;
    for (unsigned i = 0; Locations::LocationInfo* loc = Locations::EnumLocationInfo(i); ++i)
        if (IsOpenMission(loc)) ++openMissions;
    // The slots' background types: home, summer, raids, winter, desert, shop.
    g_options = {2, 3, 4, 0, 5, 1};
    g_scroller.itemCount = (unsigned)g_options.size();
    const char32_t* names[7] = {GameState::GetCastleName(),
                                StringTable::GetString("CHOOSE_PATH_LOCATION_1"),
                                StringTable::GetString("CHOOSE_PATH_LOCATION_4"),
                                StringTable::GetString("world_node_winter"),
                                StringTable::GetString("CHOOSE_PATH_LOCATION_2"),
                                StringTable::GetString("SQ_SHOP"),
                                StringTable::GetString("CHOOSE_PATH_LOCATION_3")};
    const char32_t* descs[7] = {
        StringTable::GetString("EXIT_FARM_BUILDING"),
        StringTable::GetString("CHOOSE_PATH_DESC_LONG_1"),
        StringTable::GetString("CHOOSE_PATH_DESC_LONG_4"),
        StringTable::GetString("UPDATE_NORTH"),
        StringTable::GetString(GameState::GetTaskBeginTime(0x5c9) != 0 ? "CHOOSE_PATH_DESC_LONG_2"
                                                                       : "CHOOSE_PATH_DESC_2_LOCKED"),
        StringTable::GetString("INVENTORY_NO_ITEMS_IN_TAB"),
        StringTable::GetString("CHOOSE_PATH_DESC_LONG_3")};
    // The locks: no open mission; the winter lands before quest 0x301; the desert before the
    // sultan's quest 0x5c9 started; the shop before its unlock quest.
    bool locked[7] = {};
    locked[2] = openMissions == 0;
    locked[3] = !GameState::TaskCompleted(0x301) && !Tasks::IsTaskActive(0x301, false);
    locked[4] = GameState::GetTaskBeginTime(0x5c9) == 0;
    locked[5] = !GameState::TaskCompleted((uint32_t)Setting("shop_unlock_task").GetInt());
    bool secondLine[7] = {};
    std::u32string lines[7];
    // The missions slot shows the running mission whose turn it is, with its time left.
    unsigned running = 0;
    for (unsigned i = 0; Locations::LocationInfo* loc = Locations::EnumLocationInfo(i); ++i)
        if (IsRunningMission(loc)) ++running;
    if (running != 0) g_missionIndex %= running;
    unsigned k = g_missionIndex;
    for (unsigned i = 0; Locations::LocationInfo* loc = Locations::EnumLocationInfo(i); ++i) {
        if (!IsRunningMission(loc)) continue;
        if (k != 0) {
            --k;
            continue;
        }
        secondLine[2] = true;
        const std::u32string time = StringTable::GetTimeString(Locations::GetMissionRemainingTime(loc->startMap), false);
        lines[2] = SWPrintf(0x40, U"%s %s", {StringTable::GetString("MISSION_TIME_LEFT"), time.c_str()});
        names[2] = loc->name;
        break;
    }
    lines[1] = SWPrintf(0x40, U"%s: %d%%",
                        {StringTable::GetString("WORLD_MAP_PROGRESS"), Percent(GetCampaignProgress(kCampaigns, 4, secondLine[1]))});
    lines[3] = SWPrintf(0x40, U"%s: %d%%",
                        {StringTable::GetString("WORLD_MAP_PROGRESS"), Percent(GetCampaignProgress(kWinter, 1, secondLine[3]))});
    lines[4] = SWPrintf(0x40, U"%s: %d%%",
                        {StringTable::GetString("WORLD_MAP_PROGRESS"), Percent(GetCampaignProgress(kDesert, 2, secondLine[4]))});
    g_scroller.snapPoints = {-g_scroller.padStart, g_scroller.itemSize - g_scroller.padStart};
    for (unsigned i = 0; i < 3; ++i) {
        ButtonWhereToNext& b = g_buttons[i];
        const unsigned slot = i + g_scroller.firstIndex;
        b.root->SetVisibility(slot < g_options.size());
        if (slot >= g_options.size()) continue;
        b.SetBackground(g_options[slot], &g_buttons[0]);
        // A running mission takes three small lines; other second lines two.
        const bool mission = secondLine[slot] && slot == 2;
        b.twoLines2->SetAsyncUpdate(false);
        b.twoLines1->SetVisibility(secondLine[slot] && !mission);
        b.twoLines2->SetVisibility(secondLine[slot] || mission);
        b.oneLine->SetVisibility(!secondLine[slot]);
        b.threeLines->SetVisibility(mission);
        if (!secondLine[slot]) {
            b.oneLine->SetText(names[slot]);
        } else {
            if (slot == 2) b.threeLines->SetText(names[2]);
            else b.twoLines1->SetText(names[slot]);
            b.twoLines2->SetText(lines[slot].c_str());
        }
        b.lock->SetVisibility(locked[slot]);
    }
    for (unsigned j = 0; j < 11; ++j) g_nodeOn[j]->SetVisibility(j < g_options.size());
    for (unsigned j = 0; j < 11; ++j) g_nodeOff[j]->SetVisibility(j < g_options.size());
    g_desc->SetText(descs[CurrentSlot()]);
    // PvP: the valor reputation's rank.
    WhereToFeaturedHolder& pvp = g_featured[0];
    pvp.oneLine->SetText(StringTable::GetString("PVP"));
    pvp.lock->SetVisibility(false);
    pvp.icon->SetTexture(IconManager::GetIcon("rep_pvp_valor_60", false), true);
    pvp.pvp->SetEnabled(GameState::TaskCompleted(0x74d));
    const uint32_t valorLevel = Professions::GetPlayerProfessionLevel(7);
    Professions::ProfessionLevel* rank = Professions::GetProfessionLevel(7, valorLevel);
    pvp.desc->SetText(ToWideString(U"%s - %s (%d)", {StringTable::GetString("rep_pvp_valor"),
                                                     rank ? rank->name : StringTable::GetString("RANK_NONE"),
                                                     (int)valorLevel}));
    pvp.desc2->SetText(StringTable::GetString("START_CAMP"));
    WhereToFeaturedHolder& arena = g_featured[1];
    arena.oneLine->SetText(StringTable::GetString("MAP_ARENA"));
    arena.lock->SetVisibility(false);
    arena.arena->SetEnabled(GameState::TaskCompleted(0x509));
    g_root->UpdatePosition();
}

// @0x298df0: the strip starts at the newest open part of the world (the desert, else the winter
// lands, else the campaigns, else home) when opened in the city or the desert's map 0xb.
void Show() {
    const bool was = g_wnd->shown;
    g_wnd->shown = true;
    g_wnd->MoveWindowOnTop(true);
    if (!g_root->visibleSelf) {
        WindowManager::WindowShow(false);
        g_tween->AnimateIn();
    }
    g_root->SetVisibility(true);
    g_arenaTime = 0.f;
    unsigned slot = 0;
    const int map = GameState::GetCurrentMapID();
    if (map == 0 || map == 0xb) {
        bool any = false;
        GetCampaignProgress(kCampaigns, 4, any);
        const bool campaigns = any;
        GetCampaignProgress(kWinter, 1, any);
        slot = any ? 3 : (campaigns ? 1 : 0);
        GetCampaignProgress(kDesert, 2, any);
        if (any) slot = 4;
    }
    g_scroller.ScrollIntoView(slot);
    g_scroller.HandleMove(g_scroller.padStart);
    g_current = 0;
    UpdateContents();
    g_root->PreloadTextures();   // GUI::PreloadWindow
    if (!was) GameState::RaiseGamePauseState();
}

// @0x295718
void Hide() {
    if (g_root->visibleSelf) g_tween->AnimateOut();
}

}  // namespace ChoosePathWindow
