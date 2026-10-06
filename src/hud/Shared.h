// Shared HUD helpers (the original's Shared namespace).
#pragma once

namespace GUI { class Window; class Textfield; }
namespace Render { struct Texture; }

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

// Shared::ButtonTripleInfo (0x28 bytes): a dialog button with blue, green and gold backgrounds
// (button_active_large / button_active_green / button_gold) and an inactive state. LoadFrom @0x343d7c.
struct ButtonTripleInfo {
    GUI::Window* root = nullptr;         // +0x00 "%s"
    GUI::Window* blue = nullptr;         // +0x04 button_active_large
    GUI::Window* green = nullptr;        // +0x08 button_active_green
    GUI::Window* gold = nullptr;         // +0x0c button_gold
    GUI::Window* inactive = nullptr;     // +0x10 button_inactive (hidden by LoadFrom)
    GUI::Textfield* text = nullptr;      // +0x14
    GUI::Window* icon = nullptr;         // +0x18 icon_build (takes a depth slot)
    GUI::Textfield* textGold = nullptr;  // +0x1c
    GUI::Textfield* textPrice = nullptr; // +0x20
    GUI::Window* crystal = nullptr;      // +0x24 icon_35_crystal
    void LoadFrom(GUI::Window* root, const char* prefix);
    void ShowBlue(const char32_t* text, Render::Texture* icon);    // @0x3403f8
    void ShowGreen(const char32_t* text, Render::Texture* icon);   // @0x340524
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

// Shared::LargeLogoWindow (0x10 bytes): the large game logo, one image per language.
struct LargeLogoWindow {
    GUI::Window* holder = nullptr;   // +0x00 "%s"
    GUI::Window* eng = nullptr;      // +0x04 logo_large_eng
    GUI::Window* lv = nullptr;       // +0x08 logo_large_lv
    GUI::Window* rus = nullptr;      // +0x0c logo_large_rus
    void LoadFrom(GUI::Window* root, const char* prefix);   // @0x342674
    void SetLanguage(unsigned langId);                      // @0x340340: 1 Russian, 3 Latvian, else English
};

// Shared::TabHolder (0x24 bytes): a tab with active / inactive looks (blue and red), a lock and an
// icon. LoadFrom @0x342294, SetMode @0x340974.
struct TabHolder {
    GUI::Window* root = nullptr;         // +0x00 "%s"
    GUI::Window* active = nullptr;       // +0x04 tab_active
    GUI::Window* inactive = nullptr;     // +0x08 tab_inactive
    GUI::Window* activeRed = nullptr;    // +0x0c tab_active_red
    GUI::Window* inactiveRed = nullptr;  // +0x10 tab_inactive_red
    GUI::Window* locked = nullptr;       // +0x14 tab_locked (hidden; GUI::dummyWindow without lock)
    GUI::Window* icon = nullptr;         // +0x18 icon_place_holder (takes a depth slot)
    GUI::Window* lockIcon = nullptr;     // +0x1c icon_gold_16_lock (takes a depth slot)
    int unk20 = 0;                       // +0x20
    void LoadFrom(GUI::Window* root, const char* prefix, bool withLock);
    // The active tab shows its active look and lifts its icon 4 px (scaled); red picks the red looks.
    void SetMode(bool isActive, bool red);
};

// The purpose lines of the shop's info panel (Building_info_panel.xml), one per building kind.
struct HouseLivingInfo {                 // 0x10 bytes, LoadFrom @0x3441a0
    GUI::Window* root = nullptr;         // +0x00
    GUI::Textfield* time = nullptr;      // +0x04 text_time
    GUI::Window* icon = nullptr;         // +0x08 icon_60_converted_to (takes a depth slot)
    GUI::Textfield* tax = nullptr;       // +0x0c text_tax
    void LoadFrom(GUI::Window* root, const char* prefix);
};
struct HouseTrainingInfo {               // 0xc bytes, LoadFrom @0x344130
    GUI::Window* root = nullptr;         // +0x00
    GUI::Window* icon = nullptr;         // +0x04 icon_60 (takes a depth slot)
    GUI::Textfield* text = nullptr;      // +0x08 text_hired
    void LoadFrom(GUI::Window* root, const char* prefix);
};
struct HouseConvertingInfo {             // 0x10 bytes, LoadFrom @0x3440a0
    GUI::Window* root = nullptr;         // +0x00
    GUI::Textfield* text = nullptr;      // +0x04 text_converts
    GUI::Window* from = nullptr;         // +0x08 icon_60_converted_from (takes a depth slot)
    GUI::Window* to = nullptr;           // +0x0c icon_60_converted_to (takes a depth slot)
    void LoadFrom(GUI::Window* root, const char* prefix);
};
struct HouseProducingInfo {              // 0x44 bytes, LoadFrom @0x343fa8
    GUI::Window* root = nullptr;         // +0x00
    GUI::Textfield* text = nullptr;      // +0x04 text_converts
    GUI::Window* icons[5] = {};          // +0x08 upgrade_unlocked_item_holder_%02d.icon_active_01
    GUI::Window* confirms[5] = {};       // +0x1c ...icon_gold_confirm
    GUI::Window* locks[5] = {};          // +0x30 ...icon_lock
    void LoadFrom(GUI::Window* root, const char* prefix);
};
struct HouseDecorationInfo {             // 8 bytes, LoadFrom @0x343f60
    GUI::Window* root = nullptr;         // +0x00
    GUI::Textfield* text = nullptr;      // +0x04 text_decor
    void LoadFrom(GUI::Window* root, const char* prefix);
};
struct HouseLivingWorkerInfo {           // 0x14 bytes, LoadFrom @0x343ebc
    GUI::Window* root = nullptr;         // +0x00
    GUI::Textfield* time = nullptr;      // +0x04 text_time
    GUI::Window* icon = nullptr;         // +0x08 icon_60_converted_to (takes a depth slot)
    GUI::Textfield* tax = nullptr;       // +0x0c text_tax
    GUI::Textfield* workers = nullptr;   // +0x10 text_worker
    void LoadFrom(GUI::Window* root, const char* prefix);
};

}  // namespace Shared
