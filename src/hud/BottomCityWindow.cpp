// BottomCityWindow: the building controls at the bottom centre of the city. Two main buttons (Build,
// Edit) and, while editing, five instruments (Move, Rotate, Sell, Confirm, Cancel). Port of
// BottomCityWindow (libkingdom.so 5.11), 0x25d9a4..0x25fa74. Statics at 0x6184d0.
#include <cmath>
#include <vector>

#include "engine/IconManager.h"
#include "game/GameState.h"
#include "game/StringTable.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/HUD.h"
#include "hud/Shared.h"

namespace BottomCityWindow {
namespace {

using GUI::Window;
using Shared::ButtonBuildingControls;

WindowManager::FunctionalWindow* g_queue = nullptr;   // 0x618640
std::vector<Window*> g_windows;                       // +0x00 the seven button roots, in Init order
GUI::MovementEffect* g_instrumentEffects[5] = {};     // +0x10 Move, Rotate, Sell, Confirm, Cancel
int g_mode = 0;                                       // +0x24 0 move, 1 rotate, 2 demolish (pulses)
float g_pulseTime = 0.f;                              // +0x28
ButtonBuildingControls g_build;                       // +0x30
ButtonBuildingControls g_edit;                        // +0x5c
ButtonBuildingControls g_move;                        // +0x88
ButtonBuildingControls g_rotate;                      // +0xb4
ButtonBuildingControls g_sell;                        // +0xe0
ButtonBuildingControls g_confirm;                     // +0x10c
ButtonBuildingControls g_cancel;                      // +0x138
int g_offsetY = 0;                                    // +0x164
GUI::MovementEffect* g_buildEffect = nullptr;         // +0x168
GUI::MovementEffect* g_editEffect = nullptr;          // +0x16c
// 0x60f120..0x60f123, set by BuildingMovement through Enable*Button
bool g_rotateEnabled = true, g_okEnabled = true, g_cancelEnabled = true, g_undoEnabled = true;

ButtonBuildingControls* const kInstruments[5] = {&g_move, &g_rotate, &g_sell, &g_confirm, &g_cancel};
ButtonBuildingControls* const kPulsing[3] = {&g_move, &g_rotate, &g_sell};   // table 0x6016d4

// The buttons' x positions: the main pair meets at the centre; the instruments are spaced by the
// button width less 12 HUD pixels (Init tail and SetZ @0x25ec60).
void LayOut() {
    const int half = GUI::ScreenWidth() / 2;
    g_edit.x = half;
    g_build.x = half - g_build.root->w;
    const int step = g_move.root->w - (int)(GUI::GetHudScaleFactor() * 12.f);
    const int right = half - (g_move.root->w / 2) / 2;
    g_confirm.x = right + step;
    g_sell.x = half - step;
    g_cancel.x = right + step * 2;
    g_move.x = half - step * 3;
    g_rotate.x = half - step * 2;
}

int ShownY(const ButtonBuildingControls& b) { return GUI::ScreenHeight() + g_offsetY - b.root->h; }

ButtonBuildingControls& LoadButton(ButtonBuildingControls& b, GUI::Callback onClick, bool clickSound) {
    Window* root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Button_building_controls.xml",
                                   "Button_building_controls.png", GUI::GetHudScaleFactor(), 0, 0, 0, 0,
                                   false, 1.f);
    b.LoadFrom(root, nullptr);
    b.root->SetVisibility(false);
    b.clickArea->SetOnClick(std::move(onClick));
    if (clickSound) b.clickArea->SetActionSound("ui_click", 1);
    g_windows.push_back(b.root);
    return b;
}

// Hide two of the three backgrounds, then set the icon and the caption.
void SetButton(ButtonBuildingControls& b, Window* hide1, Window* hide2, const char* icon, const char* text) {
    hide1->SetVisibility(false);
    hide2->SetVisibility(false);
    b.icon->SetTexture(IconManager::GetIcon(icon, false), true, 0, 0, false, 0);
    b.text->SetText(StringTable::GetString(text));
}

// @0x25dbec. UNVERIFIED (later milestones): ShopWindow::Show(), and at tutorial step 0x5b
// ShopWindow::OnSelectItemID(0x8e) + ShowArrowOnItem(0x8e).
void OnBuild() {}

// @0x25e924 (BuildingMovement::SetLegacyMode(false), Activate(), ToggleMovement(): milestone 3)
void OnEdit() {
    HideMainButton();
    ShowInstruments();
    g_mode = 0;
}

// @0x25dbcc / @0x25dbac / @0x25db8c: BuildingMovement::ToggleMovement / ToggleRotation /
// ToggleDemolishion (milestone 3) and the mode whose button pulses.
void OnMove() { g_mode = 0; }
void OnRotate() { g_mode = 1; }
void OnDestroy() { g_mode = 2; }

// @0x25eb30. UNVERIFIED: BuildingMovement::Accept() (milestone 3) decides whether the edit ends; until
// it exists the edit always ends. Then SetLegacyMode(true) and the sound "ui_swing_out".
void OnAccept() {
    ShowMainButton();
    HideInstruments();
}

// @0x25ead0 (BuildingMovement::Decline(), SetLegacyMode(true), sound "ui_swing_out": milestone 3)
void OnDecline() {
    ShowMainButton();
    HideInstruments();
}

// @0x25eb04: Back cancels an edit in progress.
bool OnBack() {
    if (!g_move.root->visibleSelf) return false;
    OnDecline();
    return true;
}

// @0x25da44: the Edit button's slide-out clears the window's shown flag.
void OnTweenOut() { Queue()->shown = false; }

// @0x25d9a4
bool Click(int x, int y, bool pressed) {
    for (Window* w : g_windows)
        if (w->Click(x, y, pressed, true)) return true;
    return false;
}

}  // namespace

// Static FunctionalWindow("BottomCityWindow", Init, Deinit, Click, SetZ, Hide) @0x25da60
WindowManager::FunctionalWindow* Queue() {
    if (!g_queue) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.setZ = SetZ;
        f.click = Click;
        f.hide = Hide;
        g_queue = new WindowManager::FunctionalWindow("BottomCityWindow", std::move(f));
    }
    return g_queue;
}

// @0x25da30
bool IsVisible() { return g_queue && g_queue->shown; }

// @0x25f018
void Init() {
    g_offsetY = 0;
    g_undoEnabled = g_okEnabled = g_cancelEnabled = g_rotateEnabled = true;
    Queue()->usesZRange = false;
    LoadButton(g_build, OnBuild, false);
    g_buildEffect = GUI::CreateMovementEffect(g_build.root);
    LoadButton(g_edit, OnEdit, false);
    g_editEffect = GUI::CreateMovementEffect(g_edit.root, nullptr, nullptr, OnTweenOut);
    LoadButton(g_move, OnMove, true);
    g_instrumentEffects[0] = GUI::CreateMovementEffect(g_move.root);
    LoadButton(g_rotate, OnRotate, true);
    g_instrumentEffects[1] = GUI::CreateMovementEffect(g_rotate.root);
    LoadButton(g_sell, OnDestroy, true);
    g_instrumentEffects[2] = GUI::CreateMovementEffect(g_sell.root);
    LoadButton(g_confirm, OnAccept, false);
    g_instrumentEffects[3] = GUI::CreateMovementEffect(g_confirm.root);
    LoadButton(g_cancel, OnDecline, false);
    g_instrumentEffects[4] = GUI::CreateMovementEffect(g_cancel.root);
    Queue()->fn.zRange = WindowManager::ReturnSmallZRange;
    Queue()->fn.update = Update;
    Queue()->fn.show = Show;
    Queue()->fn.back = OnBack;
    LayOut();
}

// @0x25e95c: Build and Edit slide up from below the screen.
void ShowMainButton() {
    const int H = GUI::ScreenHeight();
    if (g_buildEffect->active && g_buildEffect->hideAtEnd) g_build.root->SetVisibility(false);
    if (!g_build.root->visibleSelf) g_buildEffect->Animate(g_build.x, H, g_build.x, ShownY(g_build), true);
    g_build.root->SetVisibility(true);
    if (g_editEffect->active && g_editEffect->hideAtEnd) g_edit.root->SetVisibility(false);
    if (!g_edit.root->visibleSelf) g_editEffect->Animate(g_edit.x, H, g_edit.x, ShownY(g_edit), true);
    g_edit.root->SetVisibility(true);
}

// @0x25e860 (Hide @0x25e958 is the same function)
void HideMainButton() {
    const int H = GUI::ScreenHeight();
    if (g_build.root->visibleSelf) g_buildEffect->Animate(g_build.x, ShownY(g_build), g_build.x, H, false);
    if (g_edit.root->visibleSelf) g_editEffect->Animate(g_edit.x, ShownY(g_edit), g_edit.x, H, false);
}

void Hide() { HideMainButton(); }

// @0x25e68c
void ShowInstruments() {
    if (g_move.root->visibleSelf) return;
    const int H = GUI::ScreenHeight();
    for (int i = 0; i < 5; ++i) {
        ButtonBuildingControls& b = *kInstruments[i];
        g_instrumentEffects[i]->Animate(b.x, H, b.x, ShownY(b), true);
        b.root->SetVisibility(true);
    }
    // SoundsManager::PlaySound("ui_swing_in", 1, false): sounds are not ported yet.
}

// @0x25e564
void HideInstruments() {
    if (!g_move.root->visibleSelf) return;
    const int H = GUI::ScreenHeight();
    for (int i = 0; i < 5; ++i) {
        ButtonBuildingControls& b = *kInstruments[i];
        g_instrumentEffects[i]->Animate(b.x, ShownY(b), b.x, H, false);
    }
}

// @0x25eb6c
void Show() {
    if (!GameState::IsPlayerCity() || GameState::TutorialStep() < 0x58) return;
    Queue()->shown = true;
    if (WindowManager::GetShownWindowCount() == 0) Queue()->MoveWindowOnTop(true);
    ShowMainButton();
    HideInstruments();
    UpdateContents();
    for (Window* w : g_windows) w->PreloadTextures();   // GUI::PreloadWindow
}

// @0x25ec0c
void SetZ(float) {
    for (Window* w : g_windows) w->SetZ(HUDWindow::GetZ());
    LayOut();
    if (!g_editEffect->active) {
        for (ButtonBuildingControls* b : {&g_build, &g_edit})
            if (b->root->visibleSelf) b->root->SetPosition(b->x, ShownY(*b));
    }
    if (!g_instrumentEffects[0]->active) {
        for (ButtonBuildingControls* b : kInstruments)
            if (b->root->visibleSelf) b->root->SetPosition(b->x, ShownY(*b));
    }
    UpdateContents();
}

// @0x25df84: each button shows one background colour (Build and Rotate blue; Edit, Move and
// Confirm green; Sell and Cancel red).
void UpdateContents() {
    SetButton(g_build, g_build.green, g_build.red, "Icon_bottom_bar_build", "main_menu_button_build");
    SetButton(g_edit, g_edit.blue, g_edit.red, "gold_select", "PERFORM_EDIT");
    SetButton(g_move, g_move.blue, g_move.red, "gold_move", "HINT_MOVE");
    g_move.root->SetEnabled(true);
    SetButton(g_rotate, g_rotate.green, g_rotate.red, "gold_rotate", "HINT_ROTATE");
    g_rotate.root->SetEnabled(g_rotateEnabled);
    SetButton(g_sell, g_sell.blue, g_sell.green, "gold_sell", "PERFORM_DESRTOY");
    g_sell.root->SetEnabled(true);
    SetButton(g_confirm, g_confirm.blue, g_confirm.red, g_okEnabled ? "gold_confirm" : "gold_quest",
              "building_controls_confirm");
    g_confirm.root->SetEnabled(true);
    SetButton(g_cancel, g_cancel.blue, g_cancel.green, "gold_cancel", "CANCEL");
    g_cancel.root->SetEnabled(g_cancelEnabled);
    // Build only works in the player's own city on the main map.
    g_build.root->SetEnabled(GameState::GetCurrentMapID() == 0 && GameState::GetCurrentLocation() == 0);
}

// @0x25e514 / @0x25e528 / @0x25e53c / @0x25e550
void EnableUndoButton(bool on) { g_undoEnabled = on; UpdateContents(); }
void EnableRotateButton(bool on) { g_rotateEnabled = on; UpdateContents(); }
void EnableCancelButton(bool on) { g_cancelEnabled = on; UpdateContents(); }
void EnableOkButton(bool on) { g_okEnabled = on; UpdateContents(); }

// @0x25dc20: the active instrument (Move, Rotate or Sell) pulses its alpha around 1.1; the others
// settle back to 1. The root's alpha field is the animated value; the backgrounds and icon get it.
void Update(float dt) {
    g_pulseTime += dt * 1.5f;
    for (int i = 0; i < 3; ++i) {
        ButtonBuildingControls& b = *kPulsing[i];
        float& a = b.root->alpha;
        if (i == g_mode) {
            float target = 1.1f + std::cos(g_pulseTime * 6.f) * 0.1f;
            if (a < target) {
                a += dt * 4.f;
                if (target < a) a = target;
            } else if (a > target) {
                a -= dt * 4.f;
                if (a < target) a = target;
            }
        } else if (a > 1.f) {
            a -= dt * 4.f;
            if (a < 1.f) a = 1.f;
        }
        b.red->SetAlpha(a, true);
        b.blue->SetAlpha(a, true);
        b.green->SetAlpha(a, true);
        b.icon->SetAlpha(a, true);
    }
    // UNVERIFIED (milestone 4): at tutorial steps 0x5a/0x5b (0x5b only while the shop is closed) the
    // tutorial points an arrow at the Build button, or assigns a worker to the building at (0x1a, 0x11)
    // and moves on to step 0x5d. Needs Map buildings, BuildingHovers and entities.
}

}  // namespace BottomCityWindow
