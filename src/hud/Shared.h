// Shared HUD helpers (the original's Shared namespace).
#pragma once
#include <cstdint>

namespace GUI { class Window; class Textfield;  class Button; }
namespace Render { struct Texture; }
namespace Items { struct ItemInfo; }

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
    // @0x340858: the bar keeps its lower (1 - f) part: f is the share that is empty.
    void SetFillPercent(float f);
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

// Shared::DiscountInfo (8 bytes): a discount label. LoadFrom @0x343984.
struct DiscountInfo {
    GUI::Window* root = nullptr;         // +0x00 "%s"
    GUI::Textfield* text = nullptr;      // +0x04 text_discount
    void LoadFrom(GUI::Window* root, const char* prefix);
};

// Shared::ItemIconHolder (0x70 bytes): an item's icon with its quality frame, lock, discount
// labels and state marks. LoadFrom @0x3448b8 (repOverlay: the shop's valor overlay, else the
// dummy window), SetItem @0x3459ec.
struct ItemIconHolder {
    GUI::Window* root = nullptr;         // +0x00 "%s"
    GUI::Window* locked = nullptr;       // +0x04 item_class_locked
    GUI::Window* broken = nullptr;       // +0x08 item_class_broken
    GUI::Window* repOverlay = nullptr;   // +0x0c shop_rep_item_overlay
    GUI::Window* itemClass[5] = {};      // +0x10 item_class_01..05
    GUI::Window* icon = nullptr;         // +0x24 item_icon_holder_item (takes a depth slot)
    DiscountInfo discount[4];            // +0x28 blue, green, purple, red
    GUI::Window* lock = nullptr;         // +0x48
    GUI::Textfield* lockLevel = nullptr; // +0x4c lock.text_level_req
    GUI::Window* redX = nullptr;         // +0x50 icon_red_x ("red_x" when it has no image)
    GUI::Window* owned = nullptr;        // +0x54 icon_gold_owned ("gold_confirm" likewise)
    GUI::Window* recommended = nullptr;  // +0x58 recomended_item
    GUI::Window* state = nullptr;        // +0x5c icon_16_state
    GUI::Window* lockSmall = nullptr;    // +0x60 (takes a depth slot)
    GUI::Window* repProf = nullptr;      // +0x64 icon_16_rep_prof (takes a depth slot)
    GUI::Textfield* levelReq = nullptr;  // +0x68 text_level_req
    unsigned itemId = 0;                 // +0x6c
    void LoadFrom(GUI::Window* root, const char* prefix, bool repOverlay);
    // The item's quality frame and image, the marks hidden (null: only forgets the item).
    void SetItem(Items::ItemInfo* info);
};

// Shared::TaskWindow (0xa8 bytes): a quest button of the HUD's quest list, with one background
// per kind of quest. LoadFrom @0x342ef0 ("Slider_mission_scroll_bg").
struct TaskWindow {
    GUI::Window* root = nullptr;             // +0x00
    GUI::Window* bg = nullptr;               // +0x04 button_scroll_bg (a city quest)
    GUI::Window* bgBattle = nullptr;         // +0x08 button_scroll_bg_battle
    GUI::Window* bgBattleSide = nullptr;     // +0x0c button_scroll_bg_battle_side (a side quest)
    GUI::Window* campaign = nullptr;         // +0x10 button_campaign_quest
    GUI::Window* desert = nullptr;           // +0x14 button_desert_quest
    GUI::Window* arena = nullptr;            // +0x18 button_arena_quest
    GUI::Textfield* campaignPercent = nullptr;   // +0x1c button_campaign_quest.text_percent
    GUI::Textfield* desertPercent = nullptr;     // +0x20 button_desert_quest.text_percent
    GUI::Window* campaignDone = nullptr;     // +0x28 button_campaign_100_quest
    GUI::Window* raid = nullptr;             // +0x2c button_raid_quest
    GUI::Window* quick = nullptr;            // +0x30 quick_task_bg_generic
    GUI::Window* quickBlue = nullptr;        // +0x34
    GUI::Window* quickGreen = nullptr;       // +0x38
    GUI::Window* quickOrange = nullptr;      // +0x3c
    GUI::Window* quickRed = nullptr;         // +0x40
    GUI::Window* allQuestsBg = nullptr;      // +0x44 button_bg_all_quests
    GUI::Textfield* allQuestsText = nullptr; // +0x48 text_all_quests
    GUI::Window* allQuestsIcon = nullptr;    // +0x4c icon_all_quests
    GUI::Window* icon = nullptr;             // +0x50 button_scroll_bg.icon (these six take a depth slot)
    GUI::Window* iconBattle = nullptr;       // +0x54 button_scroll_bg_battle.icon
    GUI::Window* iconBattleSide = nullptr;   // +0x58 button_scroll_bg_battle_side.icon
    GUI::Window* iconQuick = nullptr;        // +0x5c icon_all_quests_quick_task
    GUI::Window* iconNew = nullptr;          // +0x60 icon_gold_quest_new
    GUI::Window* iconAll = nullptr;          // +0x64 icon_all_quests
    GUI::Textfield* timeText = nullptr;      // +0x68 text_goblin_time_2
    GUI::Textfield* status = nullptr;        // +0x6c text_status
    GUI::Button* clickArea = nullptr;        // +0x70 clickArea
    uint32_t taskId = 0;                     // +0x74 (0: free)
    unsigned slot = 0, target = 0;           // +0x78 +0x7c its place in the list and where it is moving to
    bool isNew = false;                      // +0x80 slides in for the first time (the "new quest" sound)
    bool statusShown = false;                // +0x81
    bool animating = false;                  // +0x82
    float animTime = 0.f, animDuration = 0.f;    // +0x84 +0x88
    int animMode = 0;                        // +0x8c 0 eases out, 1 eases in
    int fromX = 0, fromY = 0, toX = 0, toY = 0;  // +0x90..+0x9c
    float bannerTimer = 0.f;                 // +0xa0 how long its "new" / "progress" banner stays
    bool bannerSmall = false;                // +0xa4 the small banner (a step's progress)
    // @0x342ef0: with a prefix the names are "<prefix>.<name>".
    void LoadFrom(GUI::Window* root, const char* prefix);
    void Animate(int fromX, int fromY, int toX, int toY);   // @0x3408dc half a second
};

// Shared::ItemHolder (0x88 bytes): an inventory cell. LoadFrom @0x344c5c.
struct ItemHolder {
    GUI::Window* root = nullptr;         // +0x00 "%s"
    GUI::Window* selected = nullptr;     // +0x04 selected_item
    GUI::Window* emptyBg = nullptr;      // +0x08 item_icon_inventory_empty_bg
    ItemIconHolder icon;                 // +0x0c inventory_item_icon_holder
    GUI::Textfield* quantity = nullptr;  // +0x7c text_quantity
    GUI::Textfield* quantityRed = nullptr;   // +0x80 text_quantity_red
    GUI::Window* lockTiny = nullptr;     // +0x84 icon_lock_tiny
    void LoadFrom(GUI::Window* root, const char* prefix, bool repOverlay);
};

}  // namespace Shared
