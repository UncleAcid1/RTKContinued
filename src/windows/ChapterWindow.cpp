// ChapterWindow: the chapter banner (Window_chapters.xml) that opens when a chapter task comes up:
// the game's logo, "Chapter N", the chapter's name and text, "tap to continue" pulsing. A tap
// closes it and completes the task.
// Port of ChapterWindow (libkingdom.so 5.11), 0x27a054..0x27ae4c. Statics at 0x6192c8.
#include <cmath>

#include "game/GameState.h"
#include "game/Locations.h"
#include "game/StringTable.h"
#include "game/Tasks.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/HUD.h"
#include "hud/Shared.h"
#include "engine/Text.h"
#include "windows/Windows.h"

namespace ChapterWindow {
namespace {

using GUI::Textfield;
using GUI::Window;

WindowManager::FunctionalWindow* g_queue = nullptr;   // ChapterWindow::wnd
uint32_t g_task = 0;                   // 0x6192c8
bool g_mapNameWasShown = false;        // 0x6192cc the map's name comes back after the banner
Textfield* g_digit[3] = {};            // 0x6192d0 text_level_digit_1..3
Textfield* g_header = nullptr;         // 0x6192dc text_chapter_header
Textfield* g_text = nullptr;           // 0x6192e0 text_chapter_text
Textfield* g_tap = nullptr;            // 0x6192e4 text_tap_to_continue
Window* g_tapCopy = nullptr;           // 0x6192e8 its duplicate (the steady one under the pulse)
float g_time = 0.f;                    // 0x6192ec
Window* g_root = nullptr;              // 0x6192f0
GUI::TweenEffect* g_tween = nullptr;   // 0x6192f4
Shared::LargeLogoWindow g_logo;        // 0x6192f8
bool g_tablet = false;                 // 0x60f170
float g_scale = 1.f;                   // 0x60f174

// @0x27aa78: a release anywhere closes it.
bool Click(int, int, bool pressed) {
    bool visible = g_root->visibleSelf;
    if (visible && !pressed) Hide();
    return visible;
}

// @0x27aab4
void SetZ(float z) {
    g_root->SetZ(z);
    g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::GetVerticalCenter(g_root->h));
}

// @0x27a208
void OnTweenIn() {
    if (WindowManager::GetShownWindowCount() == 1 && WindowManager::Head() != g_queue) g_queue->MoveWindowOnTop(true);
    // UNVERIFIED (milestone 6): "ui_chapter_opened".
}

// @0x27a128: the chapter's task is completed, the tutorials move on, the HUD returns.
void OnTweenOut() {
    if (g_task != 0) Tasks::CompleteTask(g_task, true, true, false);
    g_task = 0;
    if (GameState::tutorial == 1) {
        GameState::tutorial = 2;
    } else if (GameState::tutorial == 0x1b) {
        GUI::SetInteractionLock(true);
        GameState::tutorial = 0x1c;
    } else if (GameState::tutorial == 0x58) {
        GameState::tutorial = 0x59;
    }
    GameState::DropGamePauseState();
    HUDWindow::Show();
    if (g_mapNameWasShown)
        if (const char32_t* name = Locations::GetMapName(GameState::GetCurrentMapID())) HUDWindow::ShowMapName(name);
    if (GameState::secondTutorial == 0x200) GameState::secondTutorial = 0x201;
}

// @0x27aa50
bool OnBack() {
    if (!g_queue->shown) return false;
    Hide();
    return true;
}

// @0x27a828: "tap to continue" pulses.
void Update(float dt) {
    g_time += dt;
    g_tap->SetAlpha(std::fabs(std::cos(g_time * 3.f) * 0.7f), false);
}

// @0x27a264
void UpdateContents() {
    if (Tasks::TaskInfo* t = Tasks::GetTask(g_task)) {
        // (Locations::GetLocationType answers 0 or 1, so "CHAPTER_WINTER", its 2, is never used.)
        Locations::LocationInfo* loc = Locations::GetLocationWithMap(GameState::GetCurrentMapID());
        const char* key = loc && Locations::GetLocationType(loc) ? "CHAPTER_STRING_DESERT" : "CHAPTER_STRING";
        std::u32string number =
            SWPrintf(0x200, StringTable::GetString(key), {ToWideString((int)t->chapterNumber)});
        for (Textfield* d : g_digit) d->SetText(number.c_str());
        g_header->SetText(t->name);
        if (!t->description) g_text->SetText(U"NO DESCRIPTION");
        else g_text->SetText(StringTable::GetUndecoratedString(t->description, 0x200).c_str());
    }
    g_tap->SetText(StringTable::GetString("TAP_TO_CONTINUE"));
    static_cast<Textfield*>(g_tapCopy)->SetText(StringTable::GetString("TAP_TO_CONTINUE"));
}

}  // namespace

WindowManager::FunctionalWindow* Queue() {
    if (!g_queue) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.click = Click;
        f.setZ = SetZ;
        f.hide = Hide;
        g_queue = new WindowManager::FunctionalWindow("ChapterWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_queue->shown; }   // @0x27a054

void SetParameters(uint32_t task) { g_task = task; }   // @0x27a068

// @0x27ab64
void Init() {
    g_tablet = GUI::IsTabletVersion();
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Window_chapters.xml", "Window_chapters.png", -1.f, 0, 0,
                             0, 0, false, 1.f);
    g_queue->RegisterTopWindow(g_root);
    g_scale = g_root->scale;
    g_root->SetVisibility(false);
    g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::GetVerticalCenter(g_root->h));
    g_tween = GUI::CreateTweenEffect(g_root, nullptr, OnTweenIn, OnTweenOut);
    g_tween->swingIn.clear();    // (SetSwingInSound / SetSwingOutSound: none)
    g_tween->swingOut.clear();
    g_root->SetOnClick(Hide);
    g_logo.LoadFrom(g_root, "logo_holder_large");
    g_logo.SetLanguage((unsigned)StringTable::GetLangID());
    g_digit[0] = GUI::GetWindowTyped<Textfield>(g_root, "text_level_digit_1");
    g_digit[1] = GUI::GetWindowTyped<Textfield>(g_root, "text_level_digit_2");
    g_digit[2] = GUI::GetWindowTyped<Textfield>(g_root, "text_level_digit_3");
    g_header = GUI::GetWindowTyped<Textfield>(g_root, "text_chapter_header");
    g_text = GUI::GetWindowTyped<Textfield>(g_root, "text_chapter_text");
    g_tap = GUI::GetWindowTyped<Textfield>(g_root, "text_tap_to_continue");
    g_tapCopy = GUI::DuplicateWindow(g_tap, false, false, nullptr);
    if (Render::GlowFilter* glow = g_tap->glow) {   // the pulsing copy glows white
        glow->b = 1.f;
        glow->r = 1.f;
        glow->g = 1.f;
        glow->blur = 0x19;
        glow->strength = 5.f;
    }
    g_queue->fn.update = Update;
    g_queue->fn.show = Show;
    g_queue->fn.zRange = WindowManager::ReturnSmallZRange;
    g_queue->fn.back = OnBack;
}

// @0x27a8f4
void Show() {
    const bool was = g_queue->shown;
    g_queue->shown = true;
    g_queue->MoveWindowOnTop(true);
    if (!g_root->visibleSelf) {
        WindowManager::WindowShow(false);
        g_tween->AnimateIn();
    }
    g_root->SetVisibility(true);
    UpdateContents();
    g_root->PreloadTextures();   // GUI::PreloadWindow
    if (!was) GameState::RaiseGamePauseState();
    g_mapNameWasShown = HUDWindow::HideMapName();
    if (GameState::tutorial == 0x1b || GameState::tutorial == 0x58) GUI::SetInteractionLock(false);
    if (GameState::secondTutorial == 0x81 && GameState::GetCurrentMapID() == 0xe) GameState::secondTutorial = 0x200;
}

// @0x27a9f0
void Hide() {
    if (g_root->visibleSelf) {
        g_tween->active = false;
        g_tween->AnimateOut();
    }
    WindowManager::WindowHide(false);
    g_queue->shown = false;
    g_queue->MoveWindowDown(true);
}

// @0x27a890 ChapterWindow::ForceHide (a reload of the saved game)
void ForceHide() {
    if (!g_queue->shown) return;
    WindowManager::WindowHide(false);
    g_queue->shown = false;
    g_queue->MoveWindowDown(true);
    g_root->SetVisibility(false);
    GameState::DropGamePauseState();
}

}  // namespace ChapterWindow
