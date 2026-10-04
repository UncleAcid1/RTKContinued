// BeltBarWindow: the inventory belt at the bottom right (inventory button, item slots, Crafting
// button) and the crafting progress bar. Port of BeltBarWindow (libkingdom.so 5.11),
// 0x24c63c..0x251694.
#include <cstdio>

#include "engine/IconManager.h"
#include "engine/Render.h"
#include "game/GameState.h"
#include "game/StringTable.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/HUD.h"
#include "hud/Shared.h"

namespace BeltBarWindow {
namespace {

using GUI::Textfield;
using GUI::Window;

WindowManager::FunctionalWindow* g_queue = nullptr;
int g_offsetX = 0x89, g_offsetY = 0x14;   // 0x60f0c8 / 0x60f0cc
bool g_tablet = false;                    // 0x60f0d4
Window* g_root = nullptr;                 // "Belt_bar"
GUI::MovementEffect* g_effect = nullptr;
Window* g_bg2Slots = nullptr;             // battle_bar_main_bg_2_slots_right
Window* g_bgRight = nullptr;              // bottom_bar_bg_right
Window* g_inventoryIcon = nullptr;        // button_inventory.icon_60
Textfield* g_barName = nullptr;           // text_bar_name
Textfield* g_barSize = nullptr;           // text_bar_size
Window* g_inventoryClick = nullptr;       // button_inventory.clickArea
Shared::BottomSlotInfo g_slots[3];
Window* g_craft = nullptr;                // hud_button_craft
Window* g_craftBg = nullptr;
Textfield* g_craftText = nullptr;
Window* g_craftIcon = nullptr;
Window* g_craftLock = nullptr;
Window* g_craftBar = nullptr;             // "Crafting_p_bar"
Textfield* g_craftProf = nullptr;
Textfield* g_craftBarText2 = nullptr;
Textfield* g_craftBarText = nullptr;
Window* g_craftProgress = nullptr;
GUI::ClipRect g_craftClip;
Window* g_craftProfIcon = nullptr;
GUI::MovementEffect* g_craftEffect = nullptr;

}  // namespace

WindowManager::FunctionalWindow* Queue() {
    if (!g_queue) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.setZ = SetZ;
        f.hide = Hide;
        f.click = [](int, int, bool) { return false; };   // Click @0x2509ac: milestone 2d
        g_queue = new WindowManager::FunctionalWindow("BeltBarWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_root && g_root->visibleSelf; }

// @0x250ebc
void Init() {
    g_offsetX = 0x89;
    Queue()->usesZRange = false;
    g_offsetY = 0x14;
    g_tablet = GUI::IsTabletVersion();
    const int W = GUI::ScreenWidth(), H = GUI::ScreenHeight();
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Belt_bar.xml", "Belt_bar.png",
                             GUI::GetHudScaleFactor(), 0, 0, 0, 0, false, 1.f);
    if (!g_root) return;
    Queue()->RegisterTopWindow(g_root);
    g_root->SetVisibility(false);
    g_root->SetPosition(W, H - g_root->h);
    // OnTweenMove (+8) is empty; OnTweenIn (+0xc) re-reads the belt (milestone 3).
    g_effect = GUI::CreateMovementEffect(g_root, [] {}, [] {});
    // UNVERIFIED: GameState::GetBeltSlotCount() (3 on a new game) decides which background shows.
    const int beltSlots = 3;
    g_bg2Slots = GUI::GetWindowTyped<Window>(g_root, "battle_bar_main_bg_2_slots_right");
    g_bg2Slots->SetVisibility(beltSlots != 3);
    g_bgRight = GUI::GetWindowTyped<Window>(g_root, "bottom_bar_bg_right");
    g_bgRight->SetVisibility(beltSlots == 3);
    g_inventoryIcon = GUI::GetWindowTyped<Window>(g_root, "button_inventory.icon_60");
    g_inventoryIcon->takesZ = true;
    g_barName = GUI::GetWindowTyped<Textfield>(g_root, "text_bar_name");
    g_barSize = GUI::GetWindowTyped<Textfield>(g_root, "text_bar_size");
    g_inventoryClick = GUI::GetWindowTyped<GUI::Button>(g_root, "button_inventory.clickArea");
    g_inventoryClick->SetOnClick([] {});   // OnInventory: milestone 3
    g_inventoryClick->SetActionSound("ui_click", 1);
    // Slots: numbering 1..3 for a 3-slot belt, else 2, 3, 1.
    int numbers[3] = {beltSlots == 3 ? 1 : 2, beltSlots == 3 ? 2 : 3, beltSlots == 3 ? 3 : 1};
    for (int i = 0; i < 3; ++i) {
        char name[64];
        std::snprintf(name, sizeof name, "bottom_slot_item_%02d", numbers[i]);
        g_slots[i].LoadFrom(g_root, name);
        g_slots[i].iconHolder->SetTexture(IconManager::GetIcon("gold_potion", false));
        g_slots[i].holder->SetOnClick([] {});   // OnSlot(i)
    }
    g_craft = GUI::GetWindowTyped<Window>(g_root, "hud_button_craft");
    g_craft->SetOnClick([] {});             // OnCrafting
    g_craftBg = GUI::GetWindowTyped<Window>(g_root, "hud_button_craft.hud_button_small_round_blue");
    g_craftText = GUI::GetWindowTyped<Textfield>(g_root, "text_craft");
    g_craftIcon = GUI::GetWindowTyped<Window>(g_root, "hud_button_craft.icon_35");
    g_craftIcon->takesZ = true;
    g_craftLock = GUI::GetWindowTyped<Window>(g_root, "hud_button_craft.icon_16_lock");
    g_craftLock->SetTexture(IconManager::GetIcon("gold_16_lock", false), true);
    // ItemHoverWindow / TapHoverWindow (hover popups) follow with the items.

    g_craftBar = GUI::RegisterUI("../resource/kingdom_ui/1Original/Crafting_p_bar.xml", "Crafting_p_bar.png",
                                 GUI::GetHudScaleFactor(), 0, 0, 0, 0, false, 1.f);
    if (g_craftBar) {
        Queue()->RegisterTopWindow(g_craftBar);
        g_craftBar->SetVisibility(false);
        g_craftBar->SetOnClick([] {});
        g_craftProf = GUI::GetWindowTyped<Textfield>(g_craftBar, "text_prof");
        g_craftBarText2 = GUI::GetWindowTyped<Textfield>(g_craftBar, "pbar_craft.text_hp_bar_2");
        g_craftBarText = GUI::GetWindowTyped<Textfield>(g_craftBar, "pbar_craft.text_hp_bar");
        g_craftProgress = GUI::GetWindowTyped<Window>(g_craftBar, "pbar_craft.progress_bar_green");
        g_craftClip = {g_craftProgress->x, g_craftProgress->y, g_craftProgress->w + g_craftProgress->x,
                       g_craftProgress->h + g_craftProgress->y};
        g_craftBarText->SetClipRect(&g_craftClip);
        g_craftProgress->SetClipRect(&g_craftClip);
        g_craftProfIcon = GUI::GetWindowTyped<Window>(g_craftBar, "icon_prof");
        g_craftProfIcon->takesZ = true;
        g_craftEffect = GUI::CreateMovementEffect(g_craftBar, [] {});   // OnCraftPanelMove
        g_craftEffect->blocksInput = false;
    }
    Queue()->fn.update = Update;
    Queue()->fn.show = Show;
    // Shared::BuffHolderManager(0x18, 2, -1, 0) (active buffs above the belt) follows with items.
}

// @0x250444: slide in from the right
void Show() {
    if (!g_root || !GameState::IsPlayerCity()) return;
    if (g_effect->active && g_effect->hideAtEnd) g_root->SetVisibility(false);
    if (GameState::TutorialStep() <= 0x13) return;   // (also not while placing a building)
    Queue()->MoveWindowOnTop(true);
    const int W = GUI::ScreenWidth(), H = GUI::ScreenHeight();
    if (!g_root->visibleSelf) {
        int y = H - g_root->h;
        g_effect->Animate(W, y, (W + g_offsetX) - g_root->w, y + g_offsetY, true);
        g_root->SetVisibility(true);
        // UpdateContents() returns at once without a player entity (milestone 4).
    }
    // UNVERIFIED: the Crafting button visibility (TaskCompleted(Setting "craft_unlock_task")) and the
    // boss-combat lock follow with quests; the button stays as laid out.
}

// @0x24d1d4
void Hide() {
    if (!g_root || !g_root->visibleSelf) return;
    const int W = GUI::ScreenWidth(), H = GUI::ScreenHeight();
    int y = H - g_root->h;
    g_effect->Animate((W + g_offsetX) - g_root->w, y + g_offsetY, W, y, false);
}

// @0x250664
void SetZ(float) {
    if (!g_root) return;
    float z = HUDWindow::GetZ();
    if (g_craftBar) g_craftBar->SetZ(z + 0.001f);
    g_root->SetZ(z);
    if (!g_effect->active) {
        const int W = GUI::ScreenWidth(), H = GUI::ScreenHeight();
        if (!g_root->visibleSelf) g_root->SetPosition(W, (g_offsetY + H) - g_root->h);
        else g_root->SetPosition((g_offsetX + W) - g_root->w, (g_offsetY + H) - g_root->h);
    }
}

// @0x24f744: the belt items, crafting progress and hover popups all read the player and items
// (milestones 3-4).
void Update(float) {}

}  // namespace BeltBarWindow
