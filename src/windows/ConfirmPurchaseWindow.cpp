// ConfirmPurchaseWindow: "spend n crystals?" before a crystal purchase (speed-ups). OK runs the
// purchase once the dialog has slid out; Cancel drops it. During the tutorials, or when the caller
// asks to skip it, the purchase runs at once.
// Port of ConfirmPurchaseWindow (libkingdom.so 5.11), 0x299888..0x29a034. Statics at 0x61c610.
#include "engine/IconManager.h"
#include "game/GameState.h"
#include "game/StringTable.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/Shared.h"
#include "windows/Windows.h"

namespace ConfirmPurchaseWindow {
namespace {

using GUI::Textfield;
using GUI::Window;

WindowManager::FunctionalWindow* g_queue = nullptr;   // ConfirmPurchaseWindow::wnd 0x61c6ac
Window* g_root = nullptr;                // 0x61c610 Window_popup_spend_crystals.xml
GUI::TweenEffect* g_tween = nullptr;     // 0x61c614
GUI::Callback g_callback;                // 0x61c618 the purchase (owned)
unsigned g_price = 0;                    // 0x61c61c
bool g_skip = false;                     // 0x61c620 run the purchase without asking
Window* g_icon = nullptr;                // 0x61c624 pvp_winner_holder.icon_60
Textfield* g_priceText = nullptr;        // 0x61c628 text_price
Shared::ButtonTripleInfo g_button3;      // 0x61c62c button_3 (hidden)
Textfield* g_header = nullptr;           // 0x61c654 text_header
Shared::ButtonTripleInfo g_ok;           // 0x61c658 button_1
Shared::ButtonTripleInfo g_cancel;       // 0x61c680 button_2
Window* g_darkBg = nullptr;              // 0x61c6a8 sliced_dark_bg
bool g_tablet = false;                   // 0x60f1d4
float g_scale = 1.f;                     // 0x60f1d8

// @0x299888
bool Click(int x, int y, bool pressed) {
    bool visible = g_root->visibleSelf;
    g_root->Click(x, y, pressed, false);
    return visible;
}

void Move(int, int) {}   // @0x2998d4

// @0x2998d8
void SetZ(float z) {
    if (!g_queue->shown) return;
    g_root->SetZ(z);
    g_root->SetPosition(g_tween->GetCenteredX(), g_tween->GetCenteredY());
}

float ZRange() { return g_queue->shown ? 0.021f : 0.001f; }   // @0x299978

void Update(float) {}   // @0x2999a8

void OnTweenIn() {}     // @0x299e48

// @0x299ab0: the purchase runs once the dialog is gone (OK leaves the callback set, Cancel clears it).
void OnTweenOut() {
    WindowManager::WindowHide(false);
    g_queue->shown = false;
    g_queue->MoveWindowDown(true);
    GameState::DropGamePauseState();
    if (g_callback) g_callback();
}

// @0x299b0c
void UpdateContents() {
    g_icon->SetTexture(IconManager::GetIcon("crystal_60"), true);
    g_priceText->SetText(ToWideString((int)g_price));
    g_button3.root->SetVisibility(false);
    // ToWideString(format, ...) @0x22df6c: SWPrintf into one of 4 rotating 0x80-character buffers.
    g_header->SetText(
        SWPrintf(0x80, StringTable::GetString("v5_crystal_confirm"), {ToWideString((int)g_price)}).c_str());
    g_ok.ShowGreen(StringTable::GetString("HUD_OK"), IconManager::GetIcon("gold_confirm"));
    g_cancel.ShowBlue(StringTable::GetString("CANCEL"), IconManager::GetIcon("gold_cancel"));
}

// @0x299fb4
void OnOk() {
    if (!g_root->visibleSelf) return;
    g_tween->AnimateOut();
}

// @0x299f6c
void OnCancel() {
    g_callback = nullptr;
    Hide();
}

// @0x299f44
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
        f.move = Move;
        g_queue = new WindowManager::FunctionalWindow("ConfirmPurchaseWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_queue->shown; }   // @0x299964

// @0x29a034
void Init() {
    g_tablet = GUI::IsTabletVersion();
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Window_popup_spend_crystals.xml",
                             "Window_popup_spend_crystals.png", -1.f, 0, 0, 0, 0, false, 1.f);
    g_queue->RegisterTopWindow(g_root);
    g_scale = g_root->scale;
    g_root->SetVisibility(false);
    g_tween = GUI::CreateTweenEffect(g_root, nullptr, OnTweenIn, OnTweenOut);
    g_icon = GUI::GetWindowTyped<Window>(g_root, "pvp_winner_holder.icon_60");
    g_icon->takesZ = true;
    g_priceText = GUI::GetWindowTyped<Textfield>(g_root, "text_price");
    g_darkBg = GUI::GetWindowTyped<Window>(g_root, "sliced_dark_bg");
    g_darkBg->unk5f = true;   // +0x5f (Update9Slices skips the centre slice)
    g_header = GUI::GetWindowTyped<Textfield>(g_root, "text_header");
    g_ok.LoadFrom(g_root, "button_1");
    g_ok.root->SetOnClick(OnOk);
    g_button3.LoadFrom(g_root, "button_3");
    g_cancel.LoadFrom(g_root, "button_2");
    g_cancel.root->SetOnClick(OnCancel);
    g_price = 0;
    g_callback = nullptr;
    g_queue->fn.zRange = ZRange;
    g_queue->fn.update = Update;
    g_queue->fn.show = Show;
    g_queue->fn.back = OnBack;
}

// @0x299fb8
void Deinit() {
    GUI::RemoveTweenEffect(g_tween);
    g_tween = nullptr;
    delete g_root;
    g_root = nullptr;
    g_callback = nullptr;
}

// @0x2999ac
void SetParameters(GUI::Callback cb, unsigned price, bool skip) {
    g_skip = skip;
    g_callback = std::move(cb);
    g_price = price;
}

// @0x299e4c: asks only after the second tutorial; otherwise (or when skipped) buys at once.
void Show() {
    if (!g_skip && GameState::SecondTutorialStep() == 0x100) {
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
        return;
    }
    if (g_callback) g_callback();
}

// @0x299f20
void Hide() {
    if (!g_root->visibleSelf) return;
    g_tween->AnimateOut();
}

}  // namespace ConfirmPurchaseWindow
