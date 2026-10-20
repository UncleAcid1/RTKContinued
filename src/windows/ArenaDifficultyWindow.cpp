// ArenaDifficultyWindow: the arena waves event's difficulty choice (event 2, "King of the Hill").
// Three flags (the difficulties), the chosen one's challenge, chance and reward strip, and the start
// button. Port of ArenaDifficultyWindow (libkingdom.so 5.11, @0x242b4c..0x245ec8, Win_arn_dif.xml;
// statics 0x616bc4).
//
// UNVERIFIED (milestone 5s-5): UpdateContents and Update are still empty. UNVERIFIED (milestone
// 5s-6): OnSelectDifficulty, OnItem, OnHelp, OnTavern, OnItemShop and OnStartEvent.
#include <cstdio>
#include <string>

#include "engine/Resources.h"
#include "engine/Timer.h"
#include "game/BuildingHovers.h"
#include "game/GameState.h"
#include "game/Rand48.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/ContentScroller.h"
#include "hud/ItemHoverWindow.h"
#include "hud/Shared.h"
#include "windows/Windows.h"

namespace ArenaDifficultyWindow {
namespace {

using GUI::Textfield;
using GUI::Window;

// ArenaDifficultyWindow::FlagHolder (0x64 bytes): one difficulty's flag ("arena_flag_holder_%02d"),
// its gold pile pictures and the sparkles over them.
struct FlagHolder {
    Window* root = nullptr;           // +0x00 (tapped: OnSelectDifficulty)
    Window* masked = nullptr;         // +0x04 arena_flag_masked
    Window* goldSmall = nullptr;      // +0x08 image_gold_small
    Window* goldMedium = nullptr;     // +0x0c image_gold_medium
    Window* goldLarge = nullptr;      // +0x10 image_gold_large
    Window* sparksSmall = nullptr;    // +0x14 sparks_pile_small_01 (takes a depth slot)
    Window* sparksMedium[2] = {};     // +0x18 sparks_pile_med_01 / 02 (take depth slots)
    Window* sparksLarge[4] = {};      // +0x20 sparks_pile_large_01 .. 04 (take depth slots)
    int sparkFrame[4] = {};           // +0x30 a random frame of "images/Buildings/mk/bless/bless"
    float sparkTime[4] = {};          // +0x40 a random 0 .. 9.9 s
    Window* selector = nullptr;       // +0x50 arena_flag_selector
    Textfield* day1 = nullptr;        // +0x54 text_day_01
    Textfield* day2 = nullptr;        // +0x58 text_day_02
    Textfield* req = nullptr;         // +0x5c text_req
    float f60 = 1.f;                  // +0x60

    // @0x243638
    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        masked = GUI::GetWindowTypedF<Window>(w, "%s.arena_flag_masked", p);
        goldSmall = GUI::GetWindowTypedF<Window>(w, "%s.image_gold_small", p);
        goldMedium = GUI::GetWindowTypedF<Window>(w, "%s.image_gold_medium", p);
        goldLarge = GUI::GetWindowTypedF<Window>(w, "%s.image_gold_large", p);
        sparksSmall = GUI::GetWindowTypedF<Window>(w, "%s.sparks_pile_small_%02d", p, 1);
        sparksSmall->takesZ = true;
        for (int k = 0; k != 2; ++k) {
            sparksMedium[k] = GUI::GetWindowTypedF<Window>(w, "%s.sparks_pile_med_%02d", p, k + 1);
            sparksMedium[k]->takesZ = true;
        }
        for (int k = 0; k != 4; ++k) {
            sparksLarge[k] = GUI::GetWindowTypedF<Window>(w, "%s.sparks_pile_large_%02d", p, k + 1);
            sparksLarge[k]->takesZ = true;
        }
        for (int& f : sparkFrame)
            f = (int)Rand48::lrand48() % Resources::GetFrameCount("images/Buildings/mk/bless/bless");
        for (float& t : sparkTime) t = (float)((int)Rand48::lrand48() % 100) / 10.f;
        selector = GUI::GetWindowTypedF<Window>(w, "%s.arena_flag_selector", p);
        day1 = GUI::GetWindowTypedF<Textfield>(w, "%s.text_day_01", p);
        day2 = GUI::GetWindowTypedF<Textfield>(w, "%s.text_day_02", p);
        req = GUI::GetWindowTypedF<Textfield>(w, "%s.text_req", p);
        f60 = 1.f;
    }
};

WindowManager::FunctionalWindow* g_queue = nullptr;   // 0x6171dc
Window* g_root = nullptr;                 // 0x616bc4
GUI::TweenEffect* g_tween = nullptr;      // 0x616bc8
FlagHolder g_flags[3];                    // 0x616bcc
Shared::ItemHolder g_rewards[6];          // 0x616cf8 the reward strip's slots (tapped: OnItem)
Window* g_rewardBar = nullptr;            // 0x6170a0 scroll_bar_wave_rew
Shared::ContentScroller g_scroller;       // 0x617028 the reward strip
ItemHoverWindow* g_hover = nullptr;       // 0x617164
Textfield* g_title = nullptr;             // 0x617168
Window* g_help = nullptr;                 // 0x61716c button_help
Textfield* g_titleDesc = nullptr;         // 0x617170
Textfield* g_challenge = nullptr;         // 0x617174 dif_exp.text_challenge
Window* g_backdrop = nullptr;             // 0x617178 dif_exp.backdrop_02
Textfield* g_challengeDesc = nullptr;     // 0x61717c dif_exp.text_challenge_desc
Textfield* g_chance = nullptr;            // 0x617180 dif_exp.text_chance
Textfield* g_challengeExpl = nullptr;     // 0x617184 text_challenge_expl
Shared::ButtonRaidStart g_start;          // 0x617188 button_collect_bonus (its button: OnStartEvent)
uint32_t g_openTime = 0;                  // 0x6171cc when the choice was first opened (record 0x400)
Window* g_close = nullptr;                // 0x6171d8 x_button.clickArea
int g_difficulty = -1;                    // 0x60f094 the chosen difficulty (-1: none)
float g_scale = 1.f;                      // 0x60f098
bool g_tablet = false;                    // 0x60f09c

// (@0x242ee0 makes the GUI callback objects of these taps: std::function here.)
// UNVERIFIED (milestone 5s-6): OnSelectDifficulty @0x244ed8, OnItem @0x242f58, OnHelp @0x2430bc,
// OnStartEvent @0x24500c.
void OnSelectDifficulty(unsigned) {}
void OnItem(unsigned) {}
void OnHelp() {}
// UNVERIFIED (milestone 5s-5): @0x244ad0.
void Update(float) {}

// @0x2431a4: the arrow, the reward strip, the item card, then the window.
bool Click(int x, int y, bool pressed) {
    if (g_root->visibleSelf) {
        if (BuildingHovers::ClickOnArrow(x, y, pressed)) return true;
        if (!pressed) BuildingHovers::HideArrow();
        if (g_scroller.Click(x, y, pressed, g_root)) return true;
    }
    if (g_hover->Click(x, y, pressed)) return true;
    const bool visible = g_root->visibleSelf;
    g_root->Click(x, y, pressed, false);
    return visible;
}

// @0x242b4c
void SetZ(float z) {
    if (!g_queue->shown) return;
    g_root->SetZ(z);
    g_root->SetPosition(g_tween->GetCenteredX(), g_tween->GetCenteredY());
}

// @0x242bec
float ZRange() { return g_queue->shown ? 0.021f : 0.001f; }

// @0x243184: the reward strip follows a drag.
void Move(int x, int y) { g_scroller.Move(x, y); }

void OnTweenIn() {}   // @0x244acc (empty)

// @0x242e98
void OnTweenOut() {
    WindowManager::WindowHide(false);
    g_queue->shown = false;
    g_queue->MoveWindowDown(true);
    GameState::DropGamePauseState();
    g_difficulty = -1;
}

// @0x243128: the item card closes first.
bool OnBack() {
    if (!g_queue->shown) return false;
    if (g_hover->shownHover) g_hover->Hide();
    else Hide();
    return true;
}

}  // namespace

WindowManager::FunctionalWindow* Queue() {
    if (!g_queue) {   // FUN_00242c44 (static init)
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.click = Click;
        f.setZ = SetZ;
        f.hide = Hide;
        f.move = Move;
        g_queue = new WindowManager::FunctionalWindow("ArenaDifficultyWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_queue && g_queue->shown; }   // @0x242bd8

// @0x245670
void Init() {
    g_tablet = GUI::IsTabletVersion();
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Win_arn_dif.xml", "Win_arn_dif.png", -1.f, 0, 0, 0, 0,
                             false, 1.f);
    if (!g_root) return;
    Queue()->RegisterTopWindow(g_root);
    g_scale = g_root->scale;
    g_root->SetVisibility(false);
    g_tween = GUI::CreateTweenEffect(g_root, nullptr, OnTweenIn, OnTweenOut);
    g_close = GUI::GetWindowTyped<Window>(g_root, "x_button.clickArea");
    g_close->SetOnClick(Hide);
    g_close->SetActionSound("ui_close", 1);
    g_help = GUI::GetWindowTyped<Window>(g_root, "button_help");
    g_help->SetOnClick(OnHelp);
    g_title = GUI::GetWindowTyped<Textfield>(g_root, "text_title");
    g_titleDesc = GUI::GetWindowTyped<Textfield>(g_root, "text_title_desc");
    for (unsigned i = 0; i != 3; ++i) {
        char p[64];
        std::snprintf(p, sizeof p, "arena_flag_holder_%02d", i + 1);
        g_flags[i].LoadFrom(g_root, p);
        g_flags[i].root->SetOnClick([i] { OnSelectDifficulty(i); });
        g_flags[i].root->SetActionSound("ui_click", 1);
    }
    g_challenge = GUI::GetWindowTyped<Textfield>(g_root, "dif_exp.text_challenge");
    g_backdrop = GUI::GetWindowTyped<Window>(g_root, "dif_exp.backdrop_02");
    g_challengeDesc = GUI::GetWindowTyped<Textfield>(g_root, "dif_exp.text_challenge_desc");
    g_chance = GUI::GetWindowTyped<Textfield>(g_root, "dif_exp.text_chance");
    for (unsigned i = 0; i != 6; ++i) {
        // (The first slot is named "slot__01" in the data, the others "slot_02" ...)
        char p[128];
        std::snprintf(p, sizeof p, "dif_exp.wave_reward.rew_hold.slot_%s%02d.inventory_item_icon", i == 0 ? "_" : "",
                      i + 1);
        g_rewards[i].LoadFrom(g_root, p, false);
        g_rewards[i].root->SetOnClick([i] { OnItem(i); });
    }
    const int step = g_rewards[1].root->x - g_rewards[0].root->x;
    g_rewards[5].root->MoveWindow(step, 0);
    g_scroller.Init();
    Window* mask = GUI::GetWindowTyped<Window>(g_root, "dif_exp.wave_reward.rew_hold.mask_universal");
    g_scroller.viewport = {mask->x, mask->y, mask->x + mask->w, mask->y + mask->h};
    g_scroller.track = mask;
    for (Shared::ItemHolder& r : g_rewards) {
        r.root->SetClipRect(&g_scroller.viewport);
        g_scroller.windows.push_back(r.root);
    }
    g_rewardBar = GUI::GetWindowTyped<Window>(g_root, "dif_exp.wave_reward.scroll_bar_wave_rew");
    g_scroller.scrollbar = GUI::GetWindowTyped<Window>(g_root, "dif_exp.wave_reward.scroll_bar_wave_rew.scroll_bar_tiny");
    g_rewardBar->MoveWindow(0, -(int)(g_scale * 5.f));
    g_scroller.unkA0 = 6;
    g_scroller.itemSize = g_rewards[1].root->x - g_rewards[0].root->x;
    g_scroller.onScroll = UpdateContents;
    g_scroller.horizontal = true;
    g_scroller.perLine = 1;
    g_challengeExpl = GUI::GetWindowTyped<Textfield>(g_root, "text_challenge_expl");
    g_start.LoadFrom(g_root, "button_collect_bonus");
    g_start.active->SetOnClick(OnStartEvent);
    g_queue->hideOnOuterClick = true;
    g_queue->fn.zRange = ZRange;
    g_queue->fn.update = Update;
    g_queue->fn.back = OnBack;
    g_queue->fn.show = Show;
    g_hover = new ItemHoverWindow();
}

// @0x244f1c: the game pauses while it is up; the first opening is recorded (record 0x400), and
// before event 2 was ever won no difficulty is chosen.
void Show() {
    g_queue->shown = true;
    g_queue->MoveWindowOnTop(true);
    if (!g_root->visibleSelf) {
        WindowManager::WindowShow(false);
        g_tween->AnimateIn();
        GameState::RaiseGamePauseState();
    }
    g_root->SetVisibility(true);
    if (GameState::GetEventStartTime(0x400) == 0) {
        g_openTime = (uint32_t)Timer::GetGlobalTime();
        GameState::SetEventStartTime(0x400, g_openTime);
    } else {
        g_openTime = GameState::GetEventStartTime(0x400);
    }
    if (GameState::GetEventCompletionCount(2) == 0) g_difficulty = 0;
    UpdateContents();
    g_root->PreloadTextures();   // GUI::PreloadWindow
}

// @0x243104
void Hide() {
    if (!g_root || !g_root->visibleSelf) return;
    g_tween->AnimateOut();
}

// UNVERIFIED (milestone 5s-5): @0x243894.
void UpdateContents() {}

// UNVERIFIED (milestone 5s-6): @0x24500c.
void OnStartEvent() {}

}  // namespace ArenaDifficultyWindow
