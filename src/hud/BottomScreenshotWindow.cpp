// BottomScreenshotWindow: the screenshot mode. The game pauses, the bottom bars make way for two
// buttons (OK with the camera icon, Cancel) and the tutorial arrow points at OK; OK saves the scene
// without the HUD, framed (Render::SaveImage), and counts as the quest step "make a screenshot".
// Port of BottomScreenshotWindow (libkingdom.so 5.11), 0x262540..0x262f14 (its static
// FunctionalWindow @0x262598). Statics at 0x618844 and 0x60f12c.
#include "engine/IconManager.h"
#include "engine/Render.h"
#include "game/BuildingHovers.h"
#include "game/GameState.h"
#include "game/StringTable.h"
#include "game/Tasks.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/HUD.h"

namespace BottomScreenshotWindow {
namespace {

using GUI::Textfield;
using GUI::Window;

WindowManager::FunctionalWindow* g_queue = nullptr;   // BottomScreenshotWindow::wnd
Window* g_root = nullptr;                             // 0x618844 Main_ui.xml
Window* g_ok = nullptr;                               // 0x618848 button_building_controls_01
Textfield* g_okText = nullptr;                        // 0x61884c
Textfield* g_cancelText = nullptr;                    // 0x618850
GUI::MovementEffect* g_effect = nullptr;              // 0x618854
int g_offsetX = 0;                                    // 0x618858
Window* g_okIcon = nullptr;                           // 0x61885c
Window* g_cancel = nullptr;                           // 0x618860 button_building_controls_02
Window* g_cancelIcon = nullptr;                       // 0x618864
int g_offsetY = 2;                                    // 0x60f12c
float g_scale = 1.f;                                  // 0x60f130

int CenterX() { return (GUI::ScreenWidth() - g_root->w) / 2; }

// @0x262540
bool Click(int x, int y, bool pressed) { return g_root->Click(x, y, pressed, false); }

// @0x262b00: at the HUD's depth; below the screen while hidden, at the bottom while shown (left alone
// while it slides).
void SetZ(float) {
    g_root->SetZ(HUDWindow::GetZ());
    if (g_effect->active) return;
    if (!g_root->visibleSelf)
        g_root->SetPosition(CenterX(), GUI::ScreenHeight());
    else
        g_root->SetPosition(g_offsetX + CenterX(), (g_offsetY + GUI::ScreenHeight()) - g_root->h);
}

// @0x262644: slid out: the game runs again and the bars that belong on this map come back.
void OnTweenOut() {
    if (g_queue->shown) {
        GameState::DropGamePauseState();
        if (GameState::GetCurrentMapID() != 0 && GameState::GetCurrentMapID() != 0xb) BottomBattleWindow::Show();
        if (GameState::GetCurrentMapID() == 0 && GameState::GetCurrentLocation() == 0) BottomCityWindow::Show();
        BattleBarWindow::Show();
        BeltBarWindow::Show();
    }
    g_queue->shown = false;
    g_root->SetVisibility(false);
}

// @0x2628fc: the scene drawn three times without the GUI layer, framed and saved; the quest step
// "make a screenshot" (0x31) is done. FileManager::PostImage(1) (@0x16f0f4, the Facebook photo
// post) is online: offline the picture is only saved.
void OnConfirm() {
    Render::renderUI = false;
    Render::Update();
    Render::Update();
    Render::Update();
    Render::SaveImage();
    Tasks::CompleteSubtask(Tasks::kMakeScreenshot, 0, 1);
    Render::renderUI = true;
    Hide();
}

// @0x2626ec: slid in: the tutorial arrow at OK (a tap on it is OK).
void OnTweenIn() {
    BuildingHovers::SetArrowClickCallback(new GUI::Callback(OnConfirm));
    BuildingHovers::ArrowAt((float)(g_root->x + g_ok->x + g_ok->w / 2), (float)(g_ok->y + g_root->y - 0xf), false,
                            false, false, false, true, true, false);
}

// @0x2628cc OnCancel is a jump to Hide.
void OnCancel() { Hide(); }

// @0x2628d0
bool OnBack() {
    if (!g_queue->shown) return false;
    BuildingHovers::HideArrow();
    OnCancel();
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
        g_queue = new WindowManager::FunctionalWindow("BottomScreenshotWindow", std::move(f));
    }
    return g_queue;
}

// @0x262584
bool IsVisible() { return g_queue && g_queue->shown; }

// @0x262c34
void Init() {
    g_offsetX = 0;
    g_offsetY = 2;
    g_queue->usesZRange = false;
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Main_ui.xml", "Main_ui.png",
                             GUI::GetHoverScaleFactor(1.f, 1.f), 0, 0, 0, 0, false, 1.f);
    g_queue->RegisterTopWindow(g_root);
    g_scale = g_root->scale;
    g_root->SetVisibility(false);
    g_effect = GUI::CreateMovementEffect(g_root, nullptr, OnTweenIn, OnTweenOut);
    g_ok = GUI::GetWindowTyped<Window>(g_root, "button_building_controls_01");
    g_okIcon = GUI::GetWindowTyped<Window>(g_root, "button_building_controls_01.icon_35");
    g_okIcon->SetTexture(IconManager::GetIcon("gold_screenshot"), true);
    g_okText = GUI::GetWindowTyped<Textfield>(g_root, "button_building_controls_01.text");
    g_ok->SetOnClick(OnConfirm);
    g_cancel = GUI::GetWindowTyped<Window>(g_root, "button_building_controls_02");
    g_cancelIcon = GUI::GetWindowTyped<Window>(g_root, "button_building_controls_02.icon_35");
    g_cancelIcon->SetTexture(IconManager::GetIcon("gold_cancel"), true);
    g_cancelText = GUI::GetWindowTyped<Textfield>(g_root, "button_building_controls_02.text");
    g_cancel->SetOnClick(OnCancel);
    g_queue->fn.zRange = WindowManager::ReturnSmallZRange;
    g_queue->fn.back = OnBack;
    g_queue->fn.show = Show;
    g_offsetY = (int)(g_scale * (float)g_offsetY);
    g_offsetX = (int)((float)g_offsetX * g_scale);
}

// @0x262bf4
void Deinit() {
    GUI::RemoveMovementEffect(g_effect);
    g_effect = nullptr;
    delete g_root;
    g_root = nullptr;
}

// @0x26296c: the game pauses and the bottom bars hide (unless it is already up), then it slides up.
void Show() {
    if (g_effect->active && g_effect->hideAtEnd) g_root->SetVisibility(false);
    if (!g_queue->shown) {
        GameState::RaiseGamePauseState();
        BottomBattleWindow::Hide();
        BattleBarWindow::Hide();
        BeltBarWindow::Hide();
        BottomCityWindow::Hide();
    }
    g_queue->shown = true;
    g_queue->MoveWindowOnTop(true);
    if (!g_root->visibleSelf) {
        const int x = CenterX();
        g_effect->Animate(x, GUI::ScreenHeight(), x + g_offsetX, (GUI::ScreenHeight() + g_offsetY) - g_root->h, true);
    }
    g_root->SetPosition(CenterX(), GUI::ScreenHeight());
    g_root->SetVisibility(true);
    UpdateContents();
    g_root->PreloadTextures();   // GUI::PreloadWindow @0x178828
}

// @0x26283c: slides down (OnTweenOut follows).
void Hide() {
    if (!g_root->visibleSelf) return;
    const int x = CenterX();
    g_effect->Animate(x + g_offsetX, (GUI::ScreenHeight() + g_offsetY) - g_root->h, x, GUI::ScreenHeight(), false);
}

// @0x2627c0
void UpdateContents() {
    g_okText->SetText(StringTable::GetString("HUD_OK"));
    g_cancelText->SetText(StringTable::GetString("CANCEL"));
}

}  // namespace BottomScreenshotWindow
