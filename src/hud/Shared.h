// Shared HUD helpers (the original's Shared namespace).
#pragma once

namespace GUI { class Window; class Textfield; }

namespace Shared {

// Shared::BottomSlotInfo (0x68 bytes): one slot of the army bar or the belt.
// LoadFrom @0x3445f0 fetches the slot's windows by "%s.<name>" and sets the fixed icons.
struct BottomSlotInfo {
    GUI::Window* holder = nullptr;       // +0x00 "%s"
    GUI::Window* bgSimple = nullptr;     // +0x04
    GUI::Window* bgGreen = nullptr;      // +0x08
    GUI::Window* bgRed = nullptr;        // +0x0c
    GUI::Window* bgDisabled = nullptr;   // +0x10
    GUI::Window* wingsEmpty = nullptr;   // +0x14 icon "inv_wings"
    GUI::Window* soldierFine = nullptr;  // +0x18 (the item / warrior icon)
    GUI::Window* iconHolder = nullptr;   // +0x1c icon_holder_belt_warrior
    GUI::Window* progress = nullptr;     // +0x20 item_proress_bar (frame 1)
    int progressY = 0, progressH = 0;    // +0x24 +0x28
    GUI::Textfield* timeLeft = nullptr;  // +0x2c
    GUI::Window* damage = nullptr;       // +0x30 damage_effect_square_red
    float damageTime = 0, damageAlpha = 0;   // +0x34 +0x38
    GUI::Window* golden = nullptr;       // +0x3c slot_golden_borderv
    GUI::Window* lockIcon = nullptr;     // +0x40 icon_holder_16x16, icon "gold_16_lock"
    GUI::Textfield* quantityNone = nullptr;    // +0x44
    GUI::Textfield* quantity = nullptr;        // +0x48
    GUI::Textfield* quantityActive = nullptr;  // +0x4c
    int unk50 = 0;                       // +0x50
    float progressValue = 0;             // +0x54
    float unk64 = 0;                     // +0x64
    void LoadFrom(GUI::Window* root, const char* prefix);
};

// Shared::ButtonBuildingControls (0x2c bytes): one round button of the building controls
// (Button_building_controls.xml). LoadFrom @0x343634 takes the windows by "%s.<name>", or by plain
// name with a null prefix (then the root itself is the button).
struct ButtonBuildingControls {
    GUI::Window* root = nullptr;         // +0x00 "%s" (or the root)
    GUI::Window* red = nullptr;          // +0x04 button_large_round_sp_red
    GUI::Window* green = nullptr;        // +0x08 button_large_round_sp_green
    GUI::Window* blue = nullptr;         // +0x0c button_bg_blue_back
    GUI::Window* icon = nullptr;         // +0x10 icon_60 (takes a depth slot)
    GUI::Textfield* text = nullptr;      // +0x14 text
    GUI::Textfield* counter = nullptr;   // +0x18 text_counter_red
    GUI::Window* clickArea = nullptr;    // +0x1c clickArea (a Button)
    int x = 0;                           // +0x20 BottomCityWindow: the button's x position
    int unk24 = 0;                       // +0x24
    int unk28 = 0;                       // +0x28 set to 0 by LoadFrom
    void LoadFrom(GUI::Window* root, const char* prefix);
};

// Shared::SmallLogoWindow (0x10 bytes): the small game logo in a dialog, one image per language.
struct SmallLogoWindow {
    GUI::Window* holder = nullptr;   // +0x00 "%s"
    GUI::Window* eng = nullptr;      // +0x04 logo_small_eng
    GUI::Window* lv = nullptr;       // +0x08 logo_small_lv
    GUI::Window* rus = nullptr;      // +0x0c logo_small_rus
    void LoadFrom(GUI::Window* root, const char* prefix);   // @0x3426f4
    void SetLanguage(unsigned langId);                      // @0x340288: 1 Russian, 3 Latvian, else English
};

}  // namespace Shared
