// SultanArrivingWindow: the sultan on his way to the desert (Window_ariving.xml), opened from his
// marker on the world map, the desert's node and the "where to next" desert slot while his quest
// 0x5c9 runs: the time left of "sultan_move_time" (from event 0x5dd's start) on a progress bar,
// "Arrive now" for "sultan_unlock" crystals (the quest is completed and the hero travels with him to
// map 0xb) or "Gather" (the quest's brief).
// Port of SultanArrivingWindow (libkingdom.so 5.11), 0x34f708..0x350664. Statics at 0x62db94.
//
// PORT (online removed): Show's Expansions check (a missing required content pack saved the game and
// started the download); OnSpeedUp's purchase log (Billing::LogCBPurchase(0x21, 0x400, price)).
#include "engine/SoundsManager.h"
#include "engine/IconManager.h"
#include "engine/Text.h"
#include "engine/Timer.h"
#include "game/GameState.h"
#include "game/Setting.h"
#include "game/StringTable.h"
#include "game/Tasks.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/Shared.h"
#include "windows/Windows.h"

namespace SultanArrivingWindow {
namespace {

using GUI::Button;
using GUI::Textfield;
using GUI::Window;

// SultanArrivingWindow::ProgressBar (0x24 bytes): the arrival bar; the orange bar and its text are
// clipped to the part done.
struct ProgressBar {
    Window* root = nullptr;          // +0x00 "%s"
    Window* bg = nullptr;            // +0x04 raid_progress_bar_bg
    Window* bar = nullptr;           // +0x08 progress_bar_ariving_orange.progress_bar_large_orange
    Textfield* under = nullptr;      // +0x0c text_progress_under_02
    Textfield* over = nullptr;       // +0x10 progress_bar_ariving_orange.text_progress_over_02
    GUI::ClipRect clip = {};         // +0x14 the bar's x .. x + w, height 0x800

    // @0x3501b8
    void LoadFrom(Window* r, const char* p) {
        root = GUI::GetWindowTypedF<Window>(r, "%s", p);
        bg = GUI::GetWindowTypedF<Window>(r, "%s.raid_progress_bar_bg", p);
        bg->SetBorders((unsigned)bg->border[0], 1, (unsigned)bg->border[2], (unsigned)bg->border[3]);
        under = GUI::GetWindowTypedF<Textfield>(r, "%s.text_progress_under_02", p);
        over = GUI::GetWindowTypedF<Textfield>(r, "%s.progress_bar_ariving_orange.text_progress_over_02", p);
        bar = GUI::GetWindowTypedF<Window>(r, "%s.progress_bar_ariving_orange.progress_bar_large_orange", p);
        clip.bottom = 0x800;
        clip.left = bar->x;
        clip.right = bar->w + bar->x;
        clip.top = 0;
        over->SetClipRect(&clip);
        bar->SetClipRect(&clip);
    }
};

WindowManager::FunctionalWindow* g_queue = nullptr;   // SultanArrivingWindow::wnd 0x62dc64
Window* g_root = nullptr;              // 0x62db94
GUI::TweenEffect* g_tween = nullptr;   // 0x62db98
ProgressBar g_bar;                     // 0x62db9c pb_ariving
Textfield* g_title = nullptr;          // 0x62dbc0 text_title
Textfield* g_name = nullptr;           // 0x62dbc4 text_day_01
Textfield* g_name2 = nullptr;          // 0x62dbc8 text_day_02
Textfield* g_desc = nullptr;           // 0x62dbcc text_desc
Shared::ButtonRaidStart g_now;         // 0x62dbd0 button_arive_now (crystals)
Shared::ButtonRaidStart g_later;       // 0x62dc14 button_arive_later (gather)
Button* g_close = nullptr;             // 0x62dc60 x_button.clickArea
uint32_t g_lastTime = 0;               // 0x62dc58 the second last shown
bool g_runHelp = false;                // RunLastHelp: after "Arrive now", quest 0x591's help
bool g_tablet = false;
float g_scale = 1.f;

// @0x34f708
bool Click(int x, int y, bool pressed) {
    const bool visible = g_root->visibleSelf;
    g_root->Click(x, y, pressed, false);
    return visible;
}

void Move(int, int) {}   // @0x34f754

// @0x34f76c
void SetZ(float z) {
    if (IsVisible()) g_root->SetZ(z);
    g_root->SetPosition(g_tween->GetCenteredX(), g_tween->GetCenteredY());
}

float ZRange() { return g_queue->shown ? 0.01f : 0.001f; }   // @0x34f7f8

// @0x34f8fc
void OnTweenOut() {
    WindowManager::WindowHide(false);
    g_queue->shown = false;
    g_queue->MoveWindowDown(true);
    GameState::DropGamePauseState();
}

// @0x34f930: the time left (all of "sultan_move_time" before event 0x5dd starts), the bar filled by
// the time gone, the names, the buttons.
void UpdateContents() {
    g_title->SetText(StringTable::GetString("SULTAN_HEADER"));
    const int total = Setting("sultan_move_time").GetInt();
    const int start = (int)GameState::GetEventStartTime(0x5dd);
    const int now = (int)Timer::GetGlobalTime();
    int left;
    if (GameState::GetEventStartTime(0x5dd) == 0) left = Setting("sultan_move_time").GetInt();
    else left = total - now + start;
    const float done = 1.f - (float)left / (float)Setting("sultan_move_time").GetInt();
    const std::u32string time = StringTable::GetNumericTimeString(left, true);
    const std::u32string text = SWPrintf(0x100, StringTable::GetString("SULTAN_ARRIVES_IN"), {time.c_str()});
    g_bar.under->SetText(text.c_str());
    g_bar.over->SetText(text.c_str());
    g_bar.clip.right = (int)((float)g_bar.clip.left + (float)g_bar.bar->w * done);
    g_name->SetText(StringTable::GetString("SULTAN_NAME"));
    g_name2->SetText(StringTable::GetString("SULTAN_NAME"));
    g_desc->SetText(StringTable::GetString("SULTAN_DESCRIPTION"));
    g_now.icon->SetTexture(IconManager::GetIcon("gold_run", false), true, 0, 0, false, 0);
    g_now.goldText->SetText(StringTable::GetString("SULTAN_NOW"));
    g_now.price->SetText(ToWideString(Setting("sultan_unlock").GetInt()));
    g_later.icon->SetTexture(IconManager::GetIcon("gold_pick_up", false), true, 0, 0, false, 0);
    g_later.text->SetText(StringTable::GetString("SULTAN_GATHER"));
}

void OnTweenIn() { UpdateContents(); }   // @0x34feec

// @0x34fef0: the timer refreshes once a second.
void Update(float) {
    if (!g_queue->shown) return;
    if ((uint32_t)Timer::GetGlobalTime() == g_lastTime) return;
    g_lastTime = (uint32_t)Timer::GetGlobalTime();
    UpdateContents();
}

// @0x35004c
bool OnBack() {
    if (!g_queue->shown) return false;
    Hide();
    return true;
}

// @0x350074: the quest's brief.
void OnGather() {
    Hide();
    TaskInfoWindow::SetTask(0x5c9, false);
    TaskInfoWindow::Show();
}

// @0x350090: "sultan_unlock" crystals (NotEnoughWindow when short) complete quest 0x5c9; the hero
// travels with the sultan to map 0xb, where quest 0x591's help runs (RunLastHelp).
void OnSpeedUp() {
    const int price = Setting("sultan_unlock").GetInt();
    NotEnoughWindow::ResetRequirements();
    NotEnoughWindow::AddRequirement(GameState::kCrystal, (unsigned)price);
    if (!NotEnoughWindow::CheckRequirements()) {
        NotEnoughWindow::Show();
        return;
    }
    GameState::ChangeResourceAmount(GameState::kCrystal, -price);
    Tasks::CompleteTask(0x5c9, true, false, false);
    SoundsManager::PlaySound("ui_buy_with_crystals", 1, false);
    GlobalMapWindow::CenterOnMap(0x53);
    if (GameState::GetCurrentMapID() != 0xb) GlobalMapWindow::ShowSultanMovement();
    GlobalMapWindow::SwitchMapAnimated(0xb, false, 500.f, false);
    g_runHelp = true;
    Hide();
}

// @0x350178
void Deinit() {
    GUI::RemoveTweenEffect(g_tween);
    g_tween = nullptr;
    delete g_root;
    g_root = nullptr;
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
        g_queue = new WindowManager::FunctionalWindow("SultanArrivingWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_queue->shown; }   // @0x34f758

// @0x34ff38 (Map::Load's end): after "Arrive now", quest 0x591's help.
void RunLastHelp() {
    if (!g_runHelp) return;
    g_runHelp = false;
    if (Tasks::TaskInfo* t = Tasks::GetTask(0x591)) t->subtasks[0].OnHelp(true);
}

// @0x34ff7c
void Show() {
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

// @0x350028
void Hide() {
    if (!g_root->visibleSelf) return;
    g_tween->AnimateOut();
}

// @0x3502d8
void Init() {
    g_tablet = GUI::IsTabletVersion();
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Window_ariving.xml", "Window_ariving.png", -1.f, 0, 0, 0,
                             0, false, 1.f);
    g_queue->RegisterTopWindow(g_root);
    g_scale = g_root->scale;
    g_root->SetVisibility(false);
    g_tween = GUI::CreateTweenEffect(g_root, nullptr, OnTweenIn, OnTweenOut);
    g_close = GUI::GetWindowTyped<Button>(g_root, "x_button.clickArea");
    g_close->SetOnClick(Hide);
    g_close->SetActionSound("ui_close", 1);
    g_title = GUI::GetWindowTyped<Textfield>(g_root, "text_title");
    g_bar.LoadFrom(g_root, "pb_ariving");
    g_bar.bg->SetBorders((unsigned)g_bar.bg->border[0], 5, (unsigned)g_bar.bg->border[2], 5);
    g_name = GUI::GetWindowTyped<Textfield>(g_root, "text_day_01");
    g_name2 = GUI::GetWindowTyped<Textfield>(g_root, "text_day_02");
    g_desc = GUI::GetWindowTyped<Textfield>(g_root, "text_desc");
    g_now.LoadFrom(g_root, "button_arive_now");
    g_now.gold->SetOnClick(OnSpeedUp);
    g_later.LoadFrom(g_root, "button_arive_later");
    g_later.active->SetOnClick(OnGather);
    g_queue->fn.zRange = ZRange;
    g_queue->fn.show = Show;
    g_queue->fn.update = Update;
    g_queue->fn.back = OnBack;
}

}  // namespace SultanArrivingWindow
