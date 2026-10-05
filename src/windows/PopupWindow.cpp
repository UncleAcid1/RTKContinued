// PopupWindow: the game's message box (Popup_yes_no.xml): a text, up to two buttons (OK / Cancel,
// or one centred OK) and a close button, each with its callback. The box grows with the text.
// Port of PopupWindow (libkingdom.so 5.11), 0x32cfa8..0x32df58; statics PopupWindow::* 0x62b060..0x62b130.
#include <cmath>

#include "engine/IconManager.h"
#include "engine/Render.h"
#include "game/GameState.h"
#include "game/StringTable.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/Shared.h"
#include "windows/Windows.h"

namespace PopupWindow {
namespace {

using GUI::Textfield;
using GUI::Window;

WindowManager::FunctionalWindow* g_queue = nullptr;   // wnd 0x62aff0
Callback g_onOk = nullptr;                // 0x62b060
Callback g_onCancel = nullptr;            // 0x62b064
Callback g_onExit = nullptr;              // 0x62b068
Window* g_window = nullptr;               // 0x62b06c
Window* g_buttonX = nullptr;              // 0x62b070
Window* g_scaler = nullptr;               // 0x62b074 dialog_hint_window_9s
Window* g_header = nullptr;               // 0x62b078
Window* g_headerLeft = nullptr;           // 0x62b07c
Window* g_headerRight = nullptr;          // 0x62b080
Textfield* g_textHeader = nullptr;        // 0x62b084
Textfield* g_textDescription = nullptr;   // 0x62b088
Window* g_wndMentor = nullptr;            // 0x62b08c
Window* g_wndMale = nullptr;              // 0x62b090
Window* g_wndFemale = nullptr;            // 0x62b094
Window* g_wndGoblin = nullptr;            // 0x62b098
Window* g_wndEmpty = nullptr;             // 0x62b09c
Window* g_wndIcon = nullptr;              // 0x62b0a0
Textfield* g_textCount = nullptr;         // 0x62b0a4 shop_required
Shared::ButtonTripleInfo g_left;          // 0x62b0a8 button_1 (OK)
Shared::ButtonTripleInfo g_right;         // 0x62b0d0 button_2 (Cancel)
Shared::ButtonTripleInfo g_center;        // 0x62b0f8 button_3 (the only OK)
GUI::TweenEffect* g_tween = nullptr;      // 0x62b120
int g_heightShift = 0;                    // 0x62b124 how much the box grew for the text
const char* g_customOkText = nullptr;     // 0x62b128 StringTable keys
const char* g_customCancelText = nullptr; // 0x62b12c
bool g_forceOnTop = false;                // 0x62b130
float g_windowScale = 1.f;                // 0x60f35c

void Grow(int d) {
    g_window->SetSize(g_window->w, g_window->h + d);
    g_scaler->SetSize(g_scaler->w, g_scaler->h + d);
    g_left.root->MoveWindow(0, d);
    g_right.root->MoveWindow(0, d);
    g_center.root->MoveWindow(0, d);
}

// @0x32d114: undo the last growth, set the text (centred, top-aligned) and grow the box by however
// much the text is taller than its field (else centre it vertically); then the buttons.
void UpdateContents(const char32_t* text) {
    Grow(-g_heightShift);
    g_heightShift = 0;
    g_textDescription->SetAlignment(1, 0);
    g_textDescription->SetText(text);
    int shift = 0;
    if (g_textDescription->sprite)
        shift = (int)(std::fabs(g_textDescription->sprite->h) - (float)g_textDescription->h);
    if (shift <= 0) {
        g_textDescription->SetAlignment(1, 1);
        shift = 0;
    }
    g_heightShift = shift;
    Grow(shift);
    g_buttonX->SetVisibility(g_onExit != nullptr);
    if (g_onOk && !g_onCancel) {
        g_left.root->SetVisibility(false);
        g_right.root->SetVisibility(false);
        g_center.root->SetVisibility(true);
        g_center.ShowBlue(StringTable::GetString(g_customOkText ? g_customOkText : "HUD_OK"),
                          IconManager::GetIcon("gold_confirm", false));
        return;
    }
    g_left.root->SetVisibility(g_onOk != nullptr);
    g_left.ShowBlue(StringTable::GetString(g_customOkText ? g_customOkText : "HUD_OK"),
                    IconManager::GetIcon("gold_confirm", false));
    g_right.root->SetVisibility(g_onCancel != nullptr);
    g_right.ShowBlue(StringTable::GetString(g_customCancelText ? g_customCancelText : "CANCEL"),
                     IconManager::GetIcon("red_x", false));
    g_center.root->SetVisibility(false);
}

// @0x32cfa8
bool Click(int x, int y, bool pressed) {
    bool r = g_window->visibleSelf;
    if (g_window->Click(x, y, pressed, false)) r = true;
    return r;
}

// @0x32d0d8. UNVERIFIED (milestone 3): BuildingHovers::SetArrowAlpha(1) is not ported.
void OnTweenOut() {
    WindowManager::WindowHide(false);
    g_queue->shown = false;
    g_queue->MoveWindowDown(true);
    GameState::DropGamePauseState();
}

// @0x32d5e0: a forced popup stays at the top of the window queue.
void Update(float) {
    if (!g_forceOnTop || !g_queue->shown) return;
    if (WindowManager::Head() == g_queue) return;
    g_queue->MoveWindowOnTop(true);
}

// @0x32d8d8
void SetZ(float z) {
    g_window->SetZ(z);
    if (g_tween->active) return;
    g_window->SetPosition((GUI::ScreenWidth() - g_window->w) / 2, GUI::GetVerticalCenter(g_window->h));
}

void CallbackExit() {   // @0x32d7f0
    if (g_onExit) g_onExit();
    Hide();
}

void CallbackCancel() {   // @0x32d818
    if (!g_onCancel) return;
    g_onCancel();
    Hide();
}

void CallbackOk() {   // @0x32d840
    if (!g_onOk) return;
    g_onOk();
    Hide();
}

// @0x32d868
bool OnBack() {
    if (!g_queue->shown) return false;
    if (g_onExit) CallbackExit();
    else if (g_onCancel) CallbackCancel();
    else CallbackOk();
    return true;
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
        g_queue = new WindowManager::FunctionalWindow("PopupWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_queue && g_queue->shown; }   // @0x32d000
void ForceOnTop() { g_forceOnTop = true; }               // @0x32d014

// @0x32d9b0
void Init() {
    g_window = GUI::RegisterUI("../resource/kingdom_ui/1Original/Popup_yes_no.xml", "Popup_yes_no.png",
                               GUI::GetHoverScaleFactor(1.f, 1.f), 0, 0, 0, 0, false, 1.f);
    g_queue->RegisterTopWindow(g_window);
    g_windowScale = g_window->scale;
    g_window->SetVisibility(false);
    g_window->SetPosition((GUI::ScreenWidth() - g_window->w) / 2, GUI::GetVerticalCenter(g_window->h));
    g_tween = GUI::CreateTweenEffect(g_window, nullptr, nullptr, OnTweenOut);
    g_buttonX = GUI::GetWindowTyped<Window>(g_window, "x_button");
    g_buttonX->SetOnClick(CallbackExit);
    g_buttonX->SetActionSound("ui_close", 1);
    g_scaler = GUI::GetWindowTyped<Window>(g_window, "dialog_hint_window_9s");
    g_header = GUI::GetWindowTyped<Window>(g_window, "header_title_header_lol");
    g_headerLeft = GUI::GetWindowTyped<Window>(g_window, "header_left_cap");
    g_headerRight = GUI::GetWindowTyped<Window>(g_window, "header_right_cap");
    g_textHeader = GUI::GetWindowTyped<Textfield>(g_window, "text_header_title");
    g_textDescription = GUI::GetWindowTyped<Textfield>(g_window, "text_header");
    g_wndMentor = GUI::GetWindowTyped<Window>(g_window, "char_mentor_hint");
    g_wndMale = GUI::GetWindowTyped<Window>(g_window, "char_male_hint");
    g_wndFemale = GUI::GetWindowTyped<Window>(g_window, "char_female_hint");
    g_wndGoblin = GUI::GetWindowTyped<Window>(g_window, "char_goblin_hint");
    g_wndEmpty = GUI::GetWindowTyped<Window>(g_window, "char_empty_hint");
    g_wndIcon = GUI::GetWindowTyped<Window>(g_window, "icon_item");
    g_wndIcon->takesZ = true;
    g_textCount = GUI::GetWindowTyped<Textfield>(g_window, "shop_required");
    g_left.LoadFrom(g_window, "button_1");
    g_left.root->SetOnClick(CallbackOk);
    g_right.LoadFrom(g_window, "button_2");
    g_right.root->SetOnClick(CallbackCancel);
    g_center.LoadFrom(g_window, "button_3");
    g_center.root->SetOnClick(CallbackOk);
    g_header->SetVisibility(false);
    g_headerLeft->SetVisibility(false);
    g_headerRight->SetVisibility(false);
    g_wndMale->SetVisibility(false);
    g_wndFemale->SetVisibility(false);
    g_wndGoblin->SetVisibility(false);
    g_wndEmpty->SetVisibility(false);
    g_queue->fn.zRange = WindowManager::ReturnSmallZRange;
    g_queue->fn.back = OnBack;
    g_queue->fn.update = Update;
}

// @0x32d964
void Deinit() {
    GUI::RemoveTweenEffect(g_tween);
    g_tween = nullptr;
    delete g_window;
}

// @0x32d630. UNVERIFIED (milestone 3): BuildingHovers::SetArrowAlpha(0) is not ported.
void Show(const char32_t* text, Callback onOk, Callback onCancel, Callback onExit, const char* okText,
          const char* cancelText) {
    if (!g_queue->shown) GameState::RaiseGamePauseState();
    g_queue->shown = true;
    g_queue->MoveWindowOnTop(true);
    if (!g_window->visibleSelf) {
        WindowManager::WindowShow(false);
        g_tween->AnimateIn();
    }
    g_window->SetVisibility(true);
    g_onOk = onOk;
    g_onCancel = onCancel;
    g_onExit = onExit;
    g_customOkText = okText;
    g_customCancelText = cancelText;
    UpdateContents(text);
    g_window->SetZ(g_window->z);
    Render::SortRenderLayer(Render::kLayerGUI, 1);
    GUI::SetOutOfBandInteractions(g_left.root, g_right.root);
}

// @0x32d79c: also interrupts a running tween so the popup always falls out.
void Hide() {
    if (g_queue->shown) {
        g_tween->active = false;
        g_tween->AnimateOut();
    }
    g_forceOnTop = false;
}

}  // namespace PopupWindow
