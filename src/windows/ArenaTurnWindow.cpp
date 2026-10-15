// ArenaTurnWindow: the arena's turn banner at the top (Pvp_player_turn.xml): the turn's number,
// whose turn it is, the time left (red under five seconds) or an icon, on a coloured circle.
// Port of ArenaTurnWindow (libkingdom.so 5.11), 0x248d04..0x249b50. Statics at 0x617604.
#include <string>

#include "engine/Render.h"
#include "game/GameState.h"
#include "game/StringTable.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/HUD.h"
#include "windows/Windows.h"

namespace ArenaTurnWindow {
namespace {

using GUI::Textfield;
using GUI::Window;

WindowManager::FunctionalWindow* g_queue = nullptr;   // ArenaTurnWindow::wnd
Window* g_root = nullptr;            // 0x617604 Pvp_player_turn.xml
int g_background = 0;                // 0x617608 which circle shows
Render::Texture* g_icon = nullptr;   // 0x61760c (instead of the time)
Textfield* g_turn = nullptr;         // 0x617610 text_turn
std::u32string g_header;             // 0x617614 (0x20 characters)
Window* g_circles[4] = {};           // 0x617694 luck_circle_purple, _orange, _blue, _red
Textfield* g_timeRed = nullptr;      // 0x6176a4 text_time_red
bool g_hurry = false;                // 0x6176a8 the time shows red
std::u32string g_number;             // 0x6176ac (0x10 characters)
Textfield* g_timeBlue = nullptr;     // 0x6176ec text_time_blue
Window* g_clock = nullptr;           // 0x6176f0 icon_clock
Textfield* g_description = nullptr;  // 0x6176f4 text_quick_task
std::u32string g_descriptionText;    // 0x6176f8 (0x80 characters)
int g_offsetX = 0;                   // 0x6178f8
GUI::MovementEffect* g_effect = nullptr;   // 0x6178fc
int g_offsetY = 0;                   // 0x617900
Window* g_line = nullptr;            // 0x617904 quick_task_line_hodler_9s
Window* g_crown = nullptr;           // 0x617908 crown_upside_down

// @0x248f5c
void OnTweenOut() {
    g_queue->shown = false;
    g_root->SetVisibility(false);
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
        g_queue = new WindowManager::FunctionalWindow("ArenaTurnWindow", std::move(f));
    }
    return g_queue;
}

// @0x248d04
bool Click(int x, int y, bool pressed) { return g_root->Click(x, y, pressed, false); }

bool IsVisible() { return g_queue->shown; }   // @0x248d48

// @0x248d5c
void SetIcon(int background, Render::Texture* icon) {
    g_background = background;
    g_icon = icon;
}

// @0x248d70: the time shows on the red text (hurry) or the blue one, unless an icon replaces it.
void UpdateContents() {
    g_turn->SetText(g_header.c_str());
    for (int i = 0; i < 4; ++i) g_circles[i]->SetVisibility(i == g_background);
    g_timeRed->SetVisibility(g_hurry && !g_icon);
    if (g_hurry) g_timeRed->SetText(g_number.c_str());
    g_timeBlue->SetVisibility(!g_hurry && !g_icon);
    if (!g_hurry) g_timeBlue->SetText(g_number.c_str());
    g_clock->SetVisibility(g_icon != nullptr);
    if (g_icon) g_clock->SetTexture(g_icon, true);
    g_description->SetText(g_descriptionText.c_str());
}

// @0x249070: centred; on small screens in the PvP tutorial it keeps to the right.
int GetHorizontalPosition() {
    const int W = GUI::ScreenWidth();
    if (GUI::ScreenHeight() < 600 && GameState::IsPvPTutorial()) return (W - g_root->w) - (int)((float)W * 0.1625f);
    return g_offsetX + (W - g_root->w) / 2;
}

void SetDescription(const char32_t* text) { g_descriptionText = SWPrintf(0x80, U"%s", {text}); }   // @0x24910c

// @0x2492f8: the time (no icon) on circle `background`; hurry: red.
void SetNumber(int background, const char32_t* text, bool hurry) {
    g_background = background;
    g_number = SWPrintf(0x10, U"%s", {text});
    g_icon = nullptr;
    g_hurry = hurry;
}

void SetHeader(const char32_t* text) { g_header = SWPrintf(0x20, U"%s", {text}); }   // @0x249508

// @0x2496f4: it slides up out of the screen.
void Hide() {
    if (!g_root->visibleSelf) return;
    g_effect->Animate(GetHorizontalPosition(), g_offsetY, GetHorizontalPosition(), -g_root->h, false);
}

// @0x249760: it slides down from above the screen.
void Show() {
    if (g_effect->active && g_effect->hideAtEnd) g_root->SetVisibility(false);
    g_queue->shown = true;
    g_queue->MoveWindowOnTop(true);
    if (!g_root->visibleSelf) g_effect->Animate(GetHorizontalPosition(), -g_root->h, GetHorizontalPosition(), g_offsetY, true);
    g_root->SetPosition(GetHorizontalPosition(), -g_root->h);
    g_root->SetVisibility(true);
    UpdateContents();
    g_root->PreloadTextures();   // GUI::PreloadWindow
}

// @0x249884
void SetZ(float) {
    g_root->SetZ(HUDWindow::GetZ());
    if (g_effect->active) return;
    if (!g_root->visibleSelf) g_root->SetPosition(GetHorizontalPosition(), -g_root->h);
    else g_root->SetPosition(GetHorizontalPosition(), g_offsetY);
}

// @0x249918
void Deinit() {
    GUI::RemoveMovementEffect(g_effect);
    g_effect = nullptr;
    delete g_root;
    g_root = nullptr;
}

// @0x249958
void Init() {
    g_queue->usesZRange = false;
    g_offsetX = 0;
    g_offsetY = 0;
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Pvp_player_turn.xml", "Pvp_player_turn.png",
                             GUI::GetHudScaleFactor(), 0, 0, 0, 0, false, 1.f);
    g_queue->RegisterTopWindow(g_root);
    g_root->SetVisibility(false);
    g_effect = GUI::CreateMovementEffect(g_root, nullptr, nullptr, OnTweenOut);
    g_turn = GUI::GetWindowTypedF<Textfield>(g_root, "text_turn");
    g_circles[0] = GUI::GetWindowTypedF<Window>(g_root, "luck_circle_holder.luck_circle_purple");
    g_circles[1] = GUI::GetWindowTypedF<Window>(g_root, "luck_circle_holder.luck_circle_orange");
    g_circles[2] = GUI::GetWindowTypedF<Window>(g_root, "luck_circle_holder.luck_circle_blue");
    g_circles[3] = GUI::GetWindowTypedF<Window>(g_root, "luck_circle_holder.luck_circle_red");
    g_timeRed = GUI::GetWindowTypedF<Textfield>(g_root, "text_time_red");
    g_timeBlue = GUI::GetWindowTypedF<Textfield>(g_root, "text_time_blue");
    g_clock = GUI::GetWindowTypedF<Window>(g_root, "icon_clock");
    g_clock->takesZ = true;
    g_line = GUI::GetWindowTypedF<Window>(g_root, "quick_task_line_hodler_9s");
    g_line->SetBorders((unsigned)g_line->border[0], 1, (unsigned)g_line->border[2], (unsigned)g_line->border[3]);
    g_description = GUI::GetWindowTypedF<Textfield>(g_root, "text_quick_task");
    g_crown = GUI::GetWindowTypedF<Window>(g_root, "crown_upside_down");
    g_crown->MoveWindow(0, -2);
    g_queue->fn.zRange = WindowManager::ReturnSmallZRange;
    g_offsetX = (int)(float)g_offsetX;
    g_offsetY = (int)(float)g_offsetY;
}

}  // namespace ArenaTurnWindow
