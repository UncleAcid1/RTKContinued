// StartRaidingWindow: the "easy money" offer (Window_start_raiding.xml) the castle's return button
// makes when the gold runs low: a header, the hero or heroine, a description and "Start raiding",
// which opens the world map with an arrow on every open mission.
// Port of StartRaidingWindow (libkingdom.so 5.11), 0x34ef1c..0x34f708. Statics at 0x62db00.
//
// PORT (online removed): Show's Expansions check (no window while the required content packs are
// missing; Expansions are not enabled).
#include "game/GameState.h"
#include "game/Locations.h"
#include "game/StringTable.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/HUD.h"
#include "windows/Windows.h"

namespace StartRaidingWindow {
namespace {

using GUI::Button;
using GUI::Textfield;
using GUI::Window;

WindowManager::FunctionalWindow* g_queue = nullptr;   // StartRaidingWindow::wnd 0x62db24
Window* g_root = nullptr;              // 0x62db00
GUI::TweenEffect* g_tween = nullptr;   // 0x62db04
Textfield* g_title = nullptr;          // 0x62db08 text_title
Window* g_dude = nullptr;              // 0x62db0c dude_on (a hero)
Window* g_dudette = nullptr;           // 0x62db10 dudette_on (a heroine)
Textfield* g_desc = nullptr;           // 0x62db14 text_desc
Textfield* g_startText = nullptr;      // 0x62db18 button_raid_start.text_start_raid_centered
Button* g_close = nullptr;             // 0x62db1c x_button.clickArea
Window* g_start = nullptr;             // 0x62db20 button_raid_start
bool g_tablet = false;                 // 0x60f3c0
float g_scale = 1.f;                   // 0x60f3c4

// @0x34ef1c
bool Click(int x, int y, bool pressed) {
    const bool visible = g_root->visibleSelf;
    g_root->Click(x, y, pressed, false);
    return visible;
}

void Move(int, int) {}   // @0x34ef68

// @0x34ef80
void SetZ(float z) {
    if (IsVisible()) g_root->SetZ(z);
    g_root->SetPosition(g_tween->GetCenteredX(), g_tween->GetCenteredY());
}

float ZRange() { return g_queue->shown ? 0.03f : 0.001f; }   // @0x34f00c

void Update(float) {}   // @0x34f03c

// @0x34f0f0
void OnTweenOut() {
    WindowManager::WindowHide(false);
    g_queue->shown = false;
    g_queue->MoveWindowDown(true);
    GameState::DropGamePauseState();
}

// @0x34f124
void UpdateContents() {
    g_title->SetText(StringTable::GetString("START_RAIDING_HEADER"));
    g_dude->SetVisibility(GameState::IsMalePlayer());
    g_dudette->SetVisibility(!GameState::IsMalePlayer());
    g_desc->SetText(StringTable::GetString("START_RAIDING_DESCRIPTION"));
    g_startText->SetText(StringTable::GetString("START_RAIDING"));
}

void OnTweenIn() { UpdateContents(); }   // @0x34f210

// @0x34f2d8
bool OnBack() {
    if (!g_queue->shown) return false;
    Hide();
    return true;
}

// @0x34f300: the world map with an arrow on every open mission (at most two windows over it).
void OnStart() {
    if (!GlobalMapWindow::IsVisible()) GlobalMapWindow::Show();
    GlobalMapWindow::SetArrowVisibleWindowLimit(2);
    GlobalMapWindow::CenterOnMap(0);
    GlobalMapWindow::HideArrow();
    for (unsigned i = 0; Locations::LocationInfo* info = Locations::EnumLocationInfo(i); ++i)
        if (info->mission && !info->disabled && !Locations::IsLocationLocked(info) && info->startMap != 0)
            GlobalMapWindow::ArrowAtMap(info->startMap, true, false);
    HUDWindow::UpdateTasks();
    Hide();
}

// @0x34f39c
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
        g_queue = new WindowManager::FunctionalWindow("StartRaidingWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_queue->shown; }   // @0x34ef6c

// @0x34f214
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

// @0x34f2b4
void Hide() {
    if (!g_root->visibleSelf) return;
    g_tween->AnimateOut();
}

// @0x34f3dc
void Init() {
    g_tablet = GUI::IsTabletVersion();
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Window_start_raiding.xml", "Window_start_raiding.png", -1.f,
                             0, 0, 0, 0, false, 1.f);
    g_queue->RegisterTopWindow(g_root);
    g_scale = g_root->scale;
    g_root->SetVisibility(false);
    g_tween = GUI::CreateTweenEffect(g_root, nullptr, OnTweenIn, OnTweenOut);
    g_close = GUI::GetWindowTyped<Button>(g_root, "x_button.clickArea");
    g_close->SetOnClick(Hide);
    g_close->SetActionSound("ui_close", 1);
    g_title = GUI::GetWindowTyped<Textfield>(g_root, "text_title");
    g_dude = GUI::GetWindowTyped<Window>(g_root, "dude_on");
    g_dudette = GUI::GetWindowTyped<Window>(g_root, "dudette_on");
    g_desc = GUI::GetWindowTyped<Textfield>(g_root, "text_desc");
    g_start = GUI::GetWindowTyped<Window>(g_root, "button_raid_start");
    g_start->SetOnClick(OnStart);
    g_startText = GUI::GetWindowTyped<Textfield>(g_root, "button_raid_start.text_start_raid_centered");
    g_queue->fn.zRange = ZRange;
    g_queue->fn.update = Update;
    g_queue->hideOnOuterClick = true;
    g_queue->fn.back = OnBack;
    g_queue->fn.show = Show;
}

}  // namespace StartRaidingWindow
