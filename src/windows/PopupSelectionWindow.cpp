// PopupSelectionWindow: the two-choice question box (Popup_yes_no.xml): a title, a text, up to two
// buttons with their own texts, icons and callbacks (one button alone is centred) and the close
// button, beside the player's portrait (or the mentor's, a goblin's, an item).
// Port of PopupSelectionWindow (libkingdom.so 5.11), 0x32c318..0x32cfa4; statics 0x62aed0..0x62af7c.
#include <string>

#include "game/GameState.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/Shared.h"
#include "windows/Windows.h"

namespace PopupSelectionWindow {
namespace {

using GUI::Textfield;
using GUI::Window;

WindowManager::FunctionalWindow* g_queue = nullptr;   // PopupSelectionWindow::wnd
Window* g_root = nullptr;                 // 0x62aed0
GUI::TweenEffect* g_tween = nullptr;      // 0x62aed4
Shared::ButtonTripleInfo g_center;        // 0x62aed8 button_3 (a single choice)
Shared::ButtonTripleInfo g_first;         // 0x62af00 button_1
Shared::ButtonTripleInfo g_second;        // 0x62af28 button_2
Window* g_male = nullptr;                 // 0x62af50 char_male_hint
Window* g_female = nullptr;               // 0x62af54 char_female_hint
Window* g_mentor = nullptr;               // 0x62af58 char_mentor_hint
Window* g_goblin = nullptr;               // 0x62af5c char_goblin_hint
Window* g_empty = nullptr;                // 0x62af60 char_empty_hint
Window* g_item = nullptr;                 // 0x62af64 icon_item
Window* g_close = nullptr;                // 0x62af68 x_button
void (*g_onExit)() = nullptr;             // 0x62af6c
Textfield* g_title = nullptr;             // 0x62af70 text_header_title
Textfield* g_text = nullptr;              // 0x62af74 text_header
Window* g_header = nullptr;               // 0x62af78 header_title_header_lol
Textfield* g_required = nullptr;          // 0x62af7c shop_required
float g_scale = 1.f;                      // 0x60f358

// @0x32c318
bool Click(int x, int y, bool pressed) {
    bool r = g_root->visibleSelf;
    if (g_root->Click(x, y, pressed, false)) r = true;
    return r;
}

// @0x32c734
void OnTweenOut() {
    WindowManager::WindowHide(false);
    g_queue->shown = false;
    g_queue->MoveWindowDown(true);
    // UNVERIFIED (milestone 4): GameState::IsCombatActive() -> DropGamePauseState (no combats yet).
}

// @0x32cb74
void CallbackExit() {
    if (g_onExit) g_onExit();
    Hide();
}

// @0x32cb9c
bool OnBack() {
    if (!g_queue->shown) return false;
    CallbackExit();
    return true;
}

// @0x32cbc4
void SetZ(float z) {
    g_root->SetZ(z);
    if (g_tween->active) return;
    g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::GetVerticalCenter(g_root->h));
}

void ShowPortrait(Window* w) {
    for (Window* p : {g_male, g_female, g_mentor, g_goblin, g_empty}) p->SetVisibility(p == w);
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
        g_queue = new WindowManager::FunctionalWindow("PopupSelectionWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_root->visibleSelf; }   // @0x32c36c

// @0x32cc84
void Init() {
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Popup_yes_no.xml", "Popup_yes_no.png",
                             GUI::GetHoverScaleFactor(1.f, 1.f), 0, 0, 0, 0, false, 1.f);
    g_queue->RegisterTopWindow(g_root);
    g_scale = g_root->scale;
    g_root->SetVisibility(false);
    g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::GetVerticalCenter(g_root->h));
    g_tween = GUI::CreateTweenEffect(g_root, nullptr, nullptr, OnTweenOut);
    g_close = GUI::GetWindowTyped<Window>(g_root, "x_button");
    g_close->SetOnClick(CallbackExit);
    g_close->SetActionSound("ui_close", 1);
    g_header = GUI::GetWindowTyped<Window>(g_root, "header_title_header_lol");
    g_title = GUI::GetWindowTyped<Textfield>(g_root, "text_header_title");
    g_text = GUI::GetWindowTyped<Textfield>(g_root, "text_header");
    g_mentor = GUI::GetWindowTyped<Window>(g_root, "char_mentor_hint");
    g_male = GUI::GetWindowTyped<Window>(g_root, "char_male_hint");
    g_female = GUI::GetWindowTyped<Window>(g_root, "char_female_hint");
    g_goblin = GUI::GetWindowTyped<Window>(g_root, "char_goblin_hint");
    g_empty = GUI::GetWindowTyped<Window>(g_root, "char_empty_hint");
    g_item = GUI::GetWindowTyped<Window>(g_root, "icon_item");
    g_item->takesZ = true;
    g_required = GUI::GetWindowTyped<Textfield>(g_root, "shop_required");
    g_first.LoadFrom(g_root, "button_1");
    g_second.LoadFrom(g_root, "button_2");
    g_center.LoadFrom(g_root, "button_3");
    g_center.root->SetVisibility(false);
    g_queue->fn.zRange = WindowManager::ReturnSmallZRange;
    g_queue->fn.back = OnBack;
}

// @0x32cc44
void Deinit() {
    GUI::RemoveTweenEffect(g_tween);
    g_tween = nullptr;
    delete g_root;
    g_root = nullptr;
}

// @0x32c7ec
void Show(const char32_t* title, const char32_t* text, const char32_t* okText, Render::Texture* okIcon,
          GUI::Callback onOk, const char32_t* cancelText, Render::Texture* cancelIcon, GUI::Callback onCancel,
          void (*onExit)()) {
    g_queue->shown = true;
    g_queue->MoveWindowOnTop(true);
    if (!g_root->visibleSelf) {
        WindowManager::WindowShow(false);
        g_tween->AnimateIn();
    } else if (g_tween->active) {
        g_tween->AnimateIn();
    }
    g_root->SetVisibility(true);
    g_close->SetVisibility(true);
    g_onExit = onExit ? onExit : Hide;
    g_title->SetText(title);
    g_text->SetText(text);
    bool ok = (bool)onOk;
    if (!onCancel && ok) {
        g_first.root->SetVisibility(false);
        g_second.root->SetVisibility(false);
        g_center.root->SetVisibility(true);
        g_center.ShowBlue(okText, okIcon);
        g_center.root->SetOnClick(std::move(onOk));
    } else {
        g_first.root->SetVisibility(ok);
        g_first.ShowBlue(okText, okIcon);
        if (ok) g_first.root->SetOnClick(std::move(onOk));
        g_second.root->SetVisibility((bool)onCancel);
        g_second.ShowBlue(cancelText, cancelIcon);
        if (ok) g_second.root->SetOnClick(std::move(onCancel));
        g_center.root->SetVisibility(false);
    }
    g_male->SetVisibility(GameState::IsMalePlayer());
    g_female->SetVisibility(!GameState::IsMalePlayer());
    g_mentor->SetVisibility(false);
    g_goblin->SetVisibility(false);
    g_empty->SetVisibility(false);
    // UNVERIFIED (milestone 4): GameState::IsCombatActive() -> RaiseGamePauseState (no combats yet).
}

// @0x32cb50
void Hide() {
    if (!g_root->visibleSelf) return;
    g_tween->AnimateOut();
}

void SetMentorIcon() { ShowPortrait(g_mentor); }   // @0x32c47c
void SetGoblinIcon() { ShowPortrait(g_goblin); }   // @0x32c508

// @0x32c594
void SetCustomIcon(Render::Texture* tex) {
    ShowPortrait(g_empty);
    g_item->SetTexture(tex, true);
}

void HideClose() { g_close->SetVisibility(false); }   // @0x32c65c

// @0x32c7b0 (the text is copied first: ShowGreen sets it on the same field)
void SetFirstGreen() {
    std::u32string t = g_first.text->GetText();
    g_first.ShowGreen(t.c_str(), g_first.icon->texture);
}

// @0x32c774
void SetSecondGreen() {
    std::u32string t = g_second.text->GetText();
    g_second.ShowGreen(t.c_str(), g_second.icon->texture);
}

}  // namespace PopupSelectionWindow
