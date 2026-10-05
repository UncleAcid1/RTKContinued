// CityRenameWindow: naming the player's kingdom (rename_city.xml): a text field of at most 12
// characters, its placeholder, and OK. Opened from the castle's hover window (milestone 3).
// Port of CityRenameWindow (libkingdom.so 5.11), 0x298fac..0x299880. Statics at 0x61c570 and 0x60f1cc.
#include <cmath>

#include "engine/Timer.h"
#include "game/GameState.h"
#include "game/StringTable.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/Shared.h"
#include "windows/Windows.h"

namespace CityRenameWindow {
namespace {

using GUI::Button;
using GUI::Textfield;
using GUI::Window;

WindowManager::FunctionalWindow* g_queue = nullptr;   // 0x61c5a0
Window* g_root = nullptr;                // 0x61c570
Textfield* g_input = nullptr;            // 0x61c574 text_01 (editable, 12 characters)
Window* g_inputBg = nullptr;             // 0x61c578 text_input (pulses while typing)
Textfield* g_placeholder = nullptr;      // 0x61c57c text_type_something
Window* g_confirmIcon = nullptr;         // 0x61c580 icon_gold_confirm
Window* g_cancelIcon = nullptr;          // 0x61c584 icon_gold_cancel
GUI::TweenEffect* g_tween = nullptr;     // 0x61c588
Button* g_close = nullptr;               // 0x61c58c x_button.clickArea
Shared::SmallLogoWindow g_logo;
bool g_tablet = false;                   // 0x60f1cc
float g_scale = 1.f;                     // 0x60f1d0

void CenterOnScreen() {
    g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::GetVerticalCenter(g_root->h));
}

bool HasText() {
    const char32_t* t = g_input->GetText();
    return t && *t;
}

// @0x298fac
bool Click(int x, int y, bool pressed) {
    bool visible = g_root->visibleSelf;
    g_root->Click(x, y, pressed, false);
    return visible;
}

void OnTweenIn() {}   // @0x298ff8

// @0x299218
void OnTweenOut() {
    WindowManager::WindowHide(false);
    g_queue->shown = false;
    g_queue->MoveWindowDown(true);
}

// @0x2990a8: the input box pulses while the field is being edited.
void Update(float) {
    if (!g_queue->shown) return;
    if (!g_input->IsInputFocused()) {
        g_inputBg->SetAlpha(1.f, true);
        return;
    }
    float c = std::cos((float)Timer::GetTime() * 8.f);
    g_inputBg->SetAlpha(1.1f + c * 0.1f, true);
}

// @0x299164: the placeholder shows while the field is empty; the icons follow.
void OnEdit() {
    bool empty = !HasText();
    g_placeholder->SetVisibility(empty);
    g_confirmIcon->SetVisibility(!empty);
    g_cancelIcon->SetVisibility(empty);
}

// @0x299338. UNVERIFIED (PopupWindow not ported): an empty name shows the "ERROR_CITY_NAME" popup.
void OnOk() {
    if (HasText()) {
        GameState::SetCastleName(g_input->GetText());
        Hide();
    }
}

// @0x2993ec
void SetZ(float z) {
    g_root->SetZ(z);
    CenterOnScreen();
}

// @0x299310
bool OnBack() {
    if (!g_queue->shown) return false;
    Hide();
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
        g_queue = new WindowManager::FunctionalWindow("CityRenameWindow", std::move(f));
    }
    return g_queue;
}

// @0x29949c
void Init() {
    g_tablet = GUI::IsTabletVersion();
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/rename_city.xml", "rename_city.png", -1.f, 0, 0, 0, 0,
                             false, 1.f);
    g_queue->RegisterTopWindow(g_root);
    g_scale = g_root->scale;
    g_root->SetVisibility(false);
    CenterOnScreen();
    g_tween = GUI::CreateTweenEffect(g_root, nullptr, OnTweenIn, OnTweenOut);
    g_close = GUI::GetWindowTyped<Button>(g_root, "x_button.clickArea");
    g_close->SetOnClick(Hide);
    g_close->SetActionSound("ui_close", 1);
    g_logo.LoadFrom(g_root, "logo_holder_small");
    g_logo.SetLanguage((unsigned)StringTable::GetLangID());
    g_confirmIcon = GUI::GetWindowTyped<Window>(g_root, "icon_gold_confirm");
    g_confirmIcon->SetVisibility(false);
    g_cancelIcon = GUI::GetWindowTyped<Window>(g_root, "icon_gold_cancel");
    GUI::GetWindowTyped<Textfield>(g_root, "hint_text")->SetText(StringTable::GetString("NAME_KINGDOM_TXT"));
    g_inputBg = GUI::GetWindowTyped<Window>(g_root, "text_input");
    g_placeholder = GUI::GetWindowTyped<Textfield>(g_root, "text_type_something");
    g_placeholder->SetText(StringTable::GetString("NAME_KINGDOM_PLACEHOLDER"));
    g_input = GUI::GetWindowTyped<Textfield>(g_root, "text_01");
    g_input->SetEditable(12);
    g_input->SetOnEdit(OnEdit);
    GUI::GetWindowTyped<Textfield>(g_root, "lvl_up_share_holder.share.text_upgrade")
        ->SetText(StringTable::GetString("HUD_OK"));
    GUI::GetWindowTyped<Button>(g_root, "lvl_up_share_holder.share.clickArea")->SetOnClick(OnOk);
    g_queue->hideOnOuterClick = true;
    g_queue->fn.zRange = WindowManager::ReturnSmallZRange;
    g_queue->fn.show = Show;
    g_queue->fn.back = OnBack;
    g_queue->fn.update = Update;
}

// @0x29945c
void Deinit() {
    GUI::RemoveTweenEffect(g_tween);
    g_tween = nullptr;
    delete g_root;
    g_root = nullptr;
}

// @0x299248
void Show() {
    g_queue->shown = true;
    g_queue->MoveWindowOnTop(true);
    if (!g_root->visibleSelf) {
        WindowManager::WindowShow(false);
        g_tween->AnimateIn();
    }
    g_root->SetVisibility(true);
    g_input->SetText(GameState::GetCastleName());
    OnEdit();
    g_root->PreloadTextures();   // GUI::PreloadWindow
}

// @0x2992ec
void Hide() {
    if (!g_root->visibleSelf) return;
    g_tween->AnimateOut();
}

}  // namespace CityRenameWindow
