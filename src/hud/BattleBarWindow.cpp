// BattleBarWindow: the army bar at the bottom left (Army button, warrior slots) and the quest slider
// above it. Port of BattleBarWindow (libkingdom.so 5.11), 0x24a750..0x24c2b4. Statics 0x60f0ac.
#include <cstdio>

#include "engine/IconManager.h"
#include "engine/Render.h"
#include "game/GameState.h"
#include "game/StringTable.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/HUD.h"
#include "hud/Shared.h"

namespace BattleBarWindow {
namespace {

using GUI::Textfield;
using GUI::Window;

WindowManager::FunctionalWindow* g_queue = nullptr;
// 0x60f0ac..0x60f0b8: bar (y offset, x) and quest slider (y offset, x), scaled by the bar's scale
int g_barDY = 0x15, g_barX = -5, g_sliderDY = -0x70, g_sliderX = -5;
bool g_tallScreen = false;   // screen height > 600
bool g_tablet = false;
float g_scale = 1.f;         // 0x60f0c0
Window* g_root = nullptr;    // "Battle_bar"
GUI::MovementEffect* g_effect = nullptr;
Window* g_bg2Slots = nullptr;
Window* g_bg = nullptr;
Window* g_armyIcon = nullptr;
Textfield* g_barName = nullptr;
Window* g_armyClick = nullptr;
Shared::BottomSlotInfo g_slots[3];
Window* g_allQuests = nullptr;
Textfield* g_allQuestsText = nullptr;
Window* g_slider = nullptr;  // "Slider_mission_scroll_bg" (Shared::TaskWindow)
GUI::MovementEffect* g_sliderEffect = nullptr;

// @0x24bbb8. UNVERIFIED (later milestones): while BuildingMovement or BuildingPlacement is active or
// the shop / screenshot window is open, the bar only absorbs clicks that land on it (root and
// slider GetWindowAtPosition); otherwise the item and heal hover windows (+0xb4, +0xac) get the
// click before the bar and the slider. None of those exist yet.
bool Click(int x, int y, bool pressed) {
    if (!g_root) return false;
    return g_root->Click(x, y, pressed, false) || (g_slider && g_slider->Click(x, y, pressed, false));
}

}  // namespace

WindowManager::FunctionalWindow* Queue() {
    if (!g_queue) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.setZ = SetZ;
        f.hide = Hide;
        f.click = Click;
        g_queue = new WindowManager::FunctionalWindow("BattleBarWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_root && g_root->visibleSelf; }

// @0x24bdd8
void Init() {
    const int W = GUI::ScreenWidth(), H = GUI::ScreenHeight();
    (void)W;
    g_sliderX = -5;
    g_barX = -5;
    g_tallScreen = 600 < H;
    Queue()->usesZRange = false;
    g_barDY = 0x15;
    g_sliderDY = -0x70;
    g_tablet = GUI::IsTabletVersion();
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Battle_bar.xml", "Battle_bar.png",
                             GUI::GetHudScaleFactor(), 0, 0, 0, 0, false, 1.f);
    if (!g_root) return;
    Queue()->RegisterTopWindow(g_root);
    g_scale = g_root->scale;
    g_root->SetVisibility(false);
    g_root->SetPosition(-g_root->w, H - g_root->h);
    g_effect = GUI::CreateMovementEffect(g_root);
    g_bg2Slots = GUI::GetWindowTyped<Window>(g_root, "battle_bar_main_bg_2_slots");
    g_bg = GUI::GetWindowTyped<Window>(g_root, "battle_bar_main_bg");
    g_armyIcon = GUI::GetWindowTyped<Window>(g_root, "button_army.icon_60");
    g_armyIcon->SetTexture(IconManager::GetIcon("bottom_bar_army", false), true);
    g_barName = GUI::GetWindowTyped<Textfield>(g_root, "text_bar_name");
    g_barName->SetText(StringTable::GetString("OPEN_WARRIOR_SCREEN"));
    g_armyClick = GUI::GetWindowTyped<GUI::Button>(g_root, "button_army.clickArea");
    g_armyClick->SetOnClick([] {});        // opens the army window (milestone 4)
    g_armyClick->SetActionSound("", 1);
    for (int i = 0; i < 3; ++i) {
        char p[64];
        std::snprintf(p, sizeof p, "bottom_slot_warrior_%02d", i + 1);
        g_slots[i].LoadFrom(g_root, p);
        g_slots[i].iconHolder->SetTexture(IconManager::GetIcon("icon_gold_helmet", false));
        g_slots[i].damage->SetFrame(0x14, true);
        g_slots[i].holder->SetOnClick([] {});   // OnSlot(i): milestone 4
    }
    g_allQuests = GUI::GetWindowTyped<Window>(g_root, "button_all_quests");
    g_allQuests->SetVisibility(false);
    g_allQuests->SetOnClick([] {});
    g_allQuestsText = GUI::GetWindowTyped<Textfield>(g_root, "button_all_quests.text_all_quests");

    g_slider = GUI::RegisterUI("../resource/kingdom_ui/1Original/Slider_mission_scroll_bg.xml",
                               "Slider_mission_scroll_bg.png", GUI::GetHudScaleFactor(), 0, 0, 0, 0, false, 1.f);
    if (g_slider) {
        // Shared::TaskWindow::LoadFrom (quest lines): milestone 4. Starts hidden.
        g_slider->SetVisibility(false);
        g_slider->SetOnClick([] {});
        g_sliderEffect = GUI::CreateMovementEffect(g_slider);
    }
    Queue()->fn.update = Update;
    Queue()->fn.show = Show;
    g_barDY = (int)((float)g_barDY * g_scale);
    g_barX = (int)((float)g_barX * g_scale);
    g_sliderX = (int)((float)g_sliderX * g_scale);
    g_sliderDY = (int)((float)g_sliderDY * g_scale);
    // ItemHoverWindow and HealHoverWindow follow with items and warriors.
}

// @0x24b8ac: shown in the city during tutorial steps 10..24 and after 54
void Show() {
    if (!g_root || !GameState::IsPlayerCity()) return;
    if (g_effect->active && g_effect->hideAtEnd) g_root->SetVisibility(false);
    int step = GameState::TutorialStep();
    if (!(29u < (unsigned)(step - 0x19) && (unsigned)(step - 10) < 0x7ffffff6u)) return;
    Queue()->MoveWindowOnTop(true);
    if (!g_root->visibleSelf) {
        const int H = GUI::ScreenHeight();
        int y = H - g_root->h;
        g_effect->Animate(-g_root->w, y, g_barX, y + g_barDY, true);
        if (g_slider) {
            int sy = (g_sliderDY + H) - g_slider->h;
            g_sliderEffect->Animate(-g_slider->w, sy, g_sliderX, sy, true);
        }
        g_root->SetVisibility(true);
        // UpdateContents() (warrior slots): milestone 4
    }
    // ShowAllTasks(): quests, milestone 4
}

// @0x24b77c
void Hide() {
    if (!g_root) return;
    const int H = GUI::ScreenHeight();
    if (g_root->visibleSelf) {
        int y = (g_barDY + H) - g_root->h;
        g_effect->Animate(g_barX, y, -g_root->w, y, false);
    }
    if (g_slider && g_slider->visibleSelf) {
        int y = (g_sliderDY + H) - g_slider->h;
        g_sliderEffect->Animate(g_sliderX, y, -g_slider->w, y, false);
    }
}

// @0x24ba4c (the heal hover window takes z - 0.0001: milestone 4)
void SetZ(float) {
    if (!g_root) return;
    float z = HUDWindow::GetZ();
    if (g_slider) g_slider->SetZ(z - 0.009f);
    g_root->SetZ(z - 0.009f);
    const int H = GUI::ScreenHeight();
    if (!g_effect->active && g_root->visibleSelf) g_root->SetPosition(g_barX, (g_barDY + H) - g_root->h);
    if (g_slider && !g_sliderEffect->active && g_slider->visibleSelf)
        g_slider->SetPosition(g_sliderX, (g_sliderDY + H) - g_slider->h);
}

// @0x24a... Update / UpdateContents read the army (milestone 4).
void Update(float) {}

}  // namespace BattleBarWindow
