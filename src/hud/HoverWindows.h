// The world hover windows BuildingHovers shows over buildings, decorations and entities: a GUI
// layout drawn in world space (SetScreenSpace(false)) that follows its object. Each is a
// WindowQueue of its own, created by HoverInfo::SetHoverType and destroyed through RemoveWindow
// (pendingDestroy, WindowManager::DestroyPendingWindows).
// Ports of BuildingHoverWindow (vtable 0x609788), BubbleHoverWindow (0x609690, Init @0x370c0c),
// TaxesHoverWindow (Init @0x39a7b0) and the progress boxes; below them the info windows of a tapped
// building or decoration (BaseHoverWindow and its subclasses, FactoryHoverWindow; implemented in
// InfoHoverWindows.cpp). Offsets are the original's.
#pragma once
#include <vector>

#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/Shared.h"

namespace Map { struct Building; struct Decor; }
class Entity;

class BuildingHoverWindow : public WindowManager::WindowQueue {
public:
    BuildingHoverWindow();                       // @0x374854
    ~BuildingHoverWindow() override;             // @0x3747b0
    void Show() override;                        // +0x1c @0x374740
    void Hide() override { shownHover = false; } // +0x20 @0x3746b8
    bool IsVisible() override { return shownHover; }   // +0x30 @0x3746cc
    virtual void RemoveWindow();                 // +0x38 @0x374720
    virtual void SetPosition(int x, int y) { posX = x; posY = y; }   // +0x3c @0x3746d4
    virtual void SetBuilding(Map::Building* b) { building = b; }     // +0x40 @0x3746e0
    virtual void SetEntity(Entity* e) { entity = e; }                // +0x44 @0x3746e8
    virtual void SetDecoration(Map::Decor* d) { decor = d; }         // +0x48 @0x3746f0
    virtual void Activate(bool) {}               // +0x4c @0x3746f8
    virtual void FixWindowPosition(int& x, int& y);   // +0x50 @0x3746fc

    bool shownHover = false;           // +0x16
    Map::Building* building = nullptr; // +0x18
    Entity* entity = nullptr;          // +0x1c
    Map::Decor* decor = nullptr;       // +0x20
    int posX = 0, posY = 0;            // +0x24 +0x28
    int f2c = 1, f30 = 1;              // +0x2c +0x30
};

// The bobbing icon bubble: build (gold_build), cut tree/rock (gold_trash), finished order, farm,
// training.
class BubbleHoverWindow : public BuildingHoverWindow {
public:
    BubbleHoverWindow();                         // @0x370d88
    ~BubbleHoverWindow() override;               // @0x370cc4
    void Init() override;                        // +0x08 @0x370c0c
    void SetZ(float z) override;                 // +0x10 @0x370b98
    bool Click(int x, int y, bool pressed) override;   // +0x14 @0x370848
    void Show() override;                        // +0x1c @0x3700dc
    void Hide() override;                        // +0x20 @0x370bd4
    void Update(float dt) override;              // +0x24 @0x370110
    void SetPosition(int x, int y) override;     // +0x3c @0x370744
    void SetBuilding(Map::Building* b) override; // +0x40 @0x37048c
    void SetEntity(Entity* e) override;          // +0x44 @0x3703ac

    GUI::Window* root = nullptr;       // +0x34
    GUI::Window* icon = nullptr;       // +0x38 "icon_holder"
    float t = 0.f;                     // +0x3c bob time
    int baseY = 0;                     // +0x40
};

// The "Zzz" over an idle farm (its farmer asleep) or a workshop without an order: worker_sleep
// drifts up and right from beside the top of the building, turning and fading, every 1.5 s.
class SleepingHoverWindow : public BuildingHoverWindow {
public:
    SleepingHoverWindow() { name = "SleepingHoverWindow"; }   // @0x395b34
    ~SleepingHoverWindow() override;             // @0x395a90
    void Init() override { t = 0.f; }            // +0x08 @0x395744
    void SetZ(float z) override;                 // +0x10 @0x3959f0
    bool Click(int x, int y, bool pressed) override;   // +0x14 @0x3959bc (never takes the tap)
    void Show() override;                        // +0x1c @0x395a34
    void Hide() override;                        // +0x20 @0x395a5c
    void Update(float dt) override;              // +0x24 @0x395754
    void SetPosition(int x, int y) override;     // +0x3c @0x395994
    void SetBuilding(Map::Building* b) override; // +0x40 @0x395944

    Render::Sprite* sprite = nullptr;  // +0x34 worker_sleep (layer 0xe)
    float t = 0.f;                     // +0x38
    int startX = 0, startY = 0;        // +0x3c +0x40
};

// The round tax button over houses and decorations with collectable gold.
class TaxesHoverWindow : public BuildingHoverWindow {
public:
    TaxesHoverWindow();                          // @0x39a9c8
    ~TaxesHoverWindow() override;                // @0x39a8ec
    void Init() override;                        // +0x08 @0x39a7b0
    void SetZ(float z) override;                 // +0x10 @0x399d64
    bool Click(int x, int y, bool pressed) override;   // +0x14 @0x39a704
    void Show() override;                        // +0x1c @0x399d30
    void Hide() override;                        // +0x20 @0x39a778
    void Update(float dt) override;              // +0x24 @0x399f54
    void SetPosition(int x, int y) override;     // +0x3c @0x39a59c
    void SetBuilding(Map::Building* b) override; // +0x40 @0x39a4d8
    void SetDecoration(Map::Decor* d) override;  // +0x48 @0x39a400
    void Activate(bool on) override;             // +0x4c @0x399f3c
    void OnCollect();                            // @0x399dd8

    GUI::Window* root = nullptr;       // +0x34
    GUI::Window* blue = nullptr;       // +0x38 "rage_bar_button_blue"
    GUI::Window* green = nullptr;      // +0x3c "rage_bar_button_green"
    GUI::Window* bronze = nullptr;     // +0x40 "rage_bar_button_bronze"
    GUI::Window* icon = nullptr;       // +0x44 "button_round_icon_holder"
    GUI::Button* clickArea = nullptr;  // +0x48
    int gold = 0;                      // +0x4c the ready gold shown
    unsigned bonusTime = 0;            // +0x50 Setting "tax_bonus_time"
    bool bonusPulse = false;           // +0x54
    float pulse = 0.f;                 // +0x58 pulse timer (1 s)
    bool bonusReady = false;           // +0x5c full and still in the bonus time
};

// The compact progress box over a regrowing tree or rock: time left, a progress bar and the hurry
// button. Port of ResourceRestoreHoverWindow (Init @0x39511c, Update @0x394a48).
class ResourceRestoreHoverWindow : public BuildingHoverWindow {
public:
    ResourceRestoreHoverWindow();                // @0x3955d4
    ~ResourceRestoreHoverWindow() override;      // @0x395510
    void Init() override;                        // +0x08 @0x39511c
    void SetZ(float z) override;                 // +0x10 @0x3944fc
    bool Click(int x, int y, bool pressed) override;   // +0x14 @0x394fc8
    void Show() override;                        // +0x1c @0x3950a4
    void Hide() override;                        // +0x20 @0x395038
    void Update(float dt) override;              // +0x24 @0x394a48
    float ZRange() override { return 0.002f; }   // +0x34 @0x39452c
    void RemoveWindow() override;                // +0x38 @0x394838
    void SetPosition(int x, int y) override;     // +0x3c @0x3949e8
    void SetBuilding(Map::Building* b) override; // +0x40 @0x3948dc
    void FixWindowPosition(int& x, int& y) override;   // +0x50 @0x39489c
    void OnSpeedUp(bool confirmed);              // @0x39462c
    void OnSpeedUpFinished();                    // @0x39459c

    GUI::Window* root = nullptr;       // +0x34
    GUI::Window* border = nullptr;     // +0x38 "golden_border_box"
    int borderH = 0;                   // +0x3c
    GUI::Window* footer = nullptr;     // +0x40 "hint_window_footer"
    GUI::Textfield* jobText = nullptr; // +0x44 "text_job"
    GUI::Window* bar = nullptr;        // +0x48 "unit_info_progress_bar"
    GUI::Window* barColor = nullptr;   // +0x4c
    GUI::Textfield* textUnder = nullptr;   // +0x50
    GUI::Textfield* textOver = nullptr;    // +0x54
    GUI::ClipRect clip = {0, 0, 0, 0}; // +0x58 the bar's fill
    GUI::Textfield* hereWorks = nullptr;   // +0x68 "TAP_TO_SPEED_UP"
    GUI::Window* hurry = nullptr;      // +0x6c "button_hurry_tiny"
    GUI::Window* crystalIcon = nullptr;    // +0x70
    GUI::Window* itemIcon = nullptr;   // +0x74
    GUI::Textfield* priceText = nullptr;   // +0x78
    bool speedingUp = false;           // +0x7c
    float speedT = 0.f;                // +0x80
    bool barStarted = false;           // +0x84
    float barValue = 0.f;              // +0x88 the shown fill, eased towards the progress
    float barTick = 0.f;               // +0x8c
    bool itemBoosts = false;           // +0x90 Setting "item_boosts" == 1
    unsigned boostItem = 0;            // +0x94
    int boostAmount = 0;               // +0x98
};

// The progress box of a building under construction, being upgraded or working an order (and of a
// decoration job): time left, the progress bar and the hurry button. The info windows use it in
// screen space; as a hover it is in world space. Port of BuildProgressHoverWindow (Init @0x373f40,
// Update @0x3720f8).
class BuildProgressHoverWindow : public BuildingHoverWindow {
public:
    explicit BuildProgressHoverWindow(bool screenSpace);   // @0x374470
    ~BuildProgressHoverWindow() override;        // @0x3743ac
    void Init() override;                        // +0x08 @0x373f40
    void SetZ(float z) override;                 // +0x10 @0x370e28
    bool Click(int x, int y, bool pressed) override;   // +0x14 @0x373ab0
    void Show() override;                        // +0x1c @0x373dac
    void Hide() override;                        // +0x20 @0x373d6c
    void Update(float dt) override;              // +0x24 @0x3720f8
    float ZRange() override { return 0.002f; }   // +0x34 @0x370e58
    void SetPosition(int x, int y) override;     // +0x3c @0x371fc0
    void SetBuilding(Map::Building* b) override; // +0x40 @0x371b90
    void SetEntity(Entity* e) override;          // +0x44 @0x3717a8
    void SetDecoration(Map::Decor* d) override;  // +0x48 @0x37183c
    void Activate(bool on) override;             // +0x4c @0x370e64
    void FixWindowPosition(int& x, int& y) override;   // +0x50 @0x370fec
    int GetSpeedUpCost();                        // @0x3710ac
    int GetRemainingTime();                      // @0x37163c
    void OnSpeedUp(bool confirmed);              // @0x371430
    void OnSpeedUpFinished();                    // @0x37116c
    void FakeSpeedup();                          // @0x370f6c
    // The repeated layout step: the hurry button and "tap to speed up" lines shown or hidden, the
    // border box resized to them and the footer moved under it.
    void Relayout(bool hurryVisible);

    bool screen = false;               // +0x34
    GUI::Window* root = nullptr;       // +0x38
    GUI::Window* border = nullptr;     // +0x3c "golden_border_box"
    int borderH = 0;                   // +0x40
    GUI::Window* footer = nullptr;     // +0x44 "hint_window_footer"
    GUI::Textfield* jobText = nullptr; // +0x48 "text_job"
    GUI::Window* bar = nullptr;        // +0x4c
    GUI::Window* barColor = nullptr;   // +0x50
    GUI::Textfield* textUnder = nullptr;   // +0x54
    GUI::Textfield* textOver = nullptr;    // +0x58
    GUI::ClipRect clip = {0, 0, 0, 0}; // +0x5c
    GUI::Textfield* hereWorks = nullptr;   // +0x6c
    GUI::Window* hurry = nullptr;      // +0x70
    GUI::Window* crystalIcon = nullptr;    // +0x74
    GUI::Window* itemIcon = nullptr;   // +0x78
    GUI::Textfield* priceText = nullptr;   // +0x7c
    float left = 0.f;                  // +0x80 the share of the work still to do
    float leftNext = 0.f;              // +0x84 the same one second later (orders interpolate)
    uint32_t second = 0;               // +0x88
    float secondFrac = 0.f;            // +0x8c
    bool speedingUp = false;           // +0x90
    bool fake = false;                 // +0x91
    float speedT = 0.f;                // +0x94
    bool barStarted = false;           // +0x98
    float barValue = 0.f;              // +0x9c
    float barTick = 0.f;               // +0xa0
    bool canSpeedUp = false;           // +0xa4
    bool expanded = true;              // +0xa5
    bool showHereWorks = true;         // +0xa6
    int hideSpeedup = 0;               // +0xa8 Setting "hide_speedup" (after the tutorial)
    bool itemBoosts = false;           // +0xac
    unsigned boostItem = 0;            // +0xb0
    int boostAmount = 0;               // +0xb4
};

// The confirm / rotate / cancel buttons sliding up at the bottom of the screen while a building
// or decoration is placed (BuildingPlacement) or moved (BuildingMovement). Confirm is enabled only
// where the object fits. Port of PlaceBuildingHoverWindow (0xc4 bytes, Init @0x3901e4).
class PlaceBuildingHoverWindow : public BuildingHoverWindow {
public:
    PlaceBuildingHoverWindow();                  // @0x39059c
    ~PlaceBuildingHoverWindow() override;        // @0x3904b8
    void Init() override;                        // +0x08 @0x3901e4
    void SetZ(float z) override;                 // +0x10 @0x38ff28
    bool Click(int x, int y, bool pressed) override;   // +0x14 @0x38fbb8
    void Show() override;                        // +0x1c @0x390010
    void Hide() override;                        // +0x20 @0x38ffd8
    void SetPosition(int x, int y) override;     // +0x3c @0x38fda0
    void EnableAccept();                         // @0x38fbf4
    void ResetArrow() { arrowCallback = 0; }     // @0x38fc14
    static void OnAccept();                      // @0x38fd00
    static void OnRotate();                      // @0x38fcb0
    static void OnDecline();                     // @0x38fc78

    GUI::Window* root = nullptr;                 // +0x34
    Shared::ButtonBuildingControls accept;       // +0x38 "button_building_controls_01" (green)
    Shared::ButtonBuildingControls rotate;       // +0x64 "button_building_controls_02" (blue)
    Shared::ButtonBuildingControls cancel;       // +0x90 "button_building_controls_03" (red)
    GUI::MovementEffect* slide = nullptr;        // +0xbc
    int arrowCallback = 0;                       // +0xc0 the tutorial arrow's callback id
};

// CallbackUpgradeBuilding::Perform @0x374bd0: the upgrade dialog for b's next level. A locked
// upgrade (its quest not done) or the worker limit shows a popup and returns false; otherwise
// NotEnoughWindow gets the upgrade's requirements and, unless prepareOnly, shows them with the
// "Improve" action (CallbackUpgradeBuildingContinuation).
bool UpgradeBuilding(Map::Building* b, bool prepareOnly);
// CallbackUpgradeBuildingContinuation::Perform @0x374b94: leave the farm view if it is b's, start
// the upgrade, close NotEnoughWindow.
void UpgradeBuildingContinuation(Map::Building* b);

// The order panel of a workshop or a farm patch (Hint_order_item_holder.xml): the five orders
// (locked ones with a padlock), the selected order's cost, time and reward, and Order / Change,
// or, while one runs, its progress with Complete Now and Stop. On the farm it plants (one patch,
// or every empty one: StartMultipleFarmContracts). Port of FactoryHoverWindow (0x184 bytes, Init
// @0x37cf98, SetBuilding @0x37a9c4, Update @0x378fa8).
class FactoryHoverWindow : public BuildingHoverWindow {
public:
    FactoryHoverWindow();                        // @0x37d408
    ~FactoryHoverWindow() override;              // @0x37d344
    void Init() override;                        // +0x08 @0x37cf98
    void SetZ(float z) override;                 // +0x10 @0x377e10
    bool Click(int x, int y, bool pressed) override;   // +0x14 @0x37c79c
    void Show() override;                        // +0x1c @0x37c888
    void Hide() override;                        // +0x20 @0x37c83c
    void Update(float dt) override;              // +0x24 @0x378fa8
    float ZRange() override { return 0.01f; }    // +0x34 @0x377e4c
    void SetPosition(int x, int y) override;     // +0x3c @0x37c658
    void SetBuilding(Map::Building* b) override; // +0x40 @0x37a9c4
    void FixWindowPosition(int& x, int& y) override;   // +0x50 @0x377e90
    void IgnoreCompletion() { ignoreCompletion = true; }   // @0x377e58
    void OnButton(unsigned i);                   // @0x377e64 an order cell
    void StartMultipleFarmContracts(unsigned n) { multiCount = n; multiFarm = true; }   // @0x377e80
    int GetSpeedUpCost();                        // @0x37802c
    int GetCurrentContract();                    // @0x3780e4 1-based, 0: none
    void OnSpeedUpFinished();                    // @0x37814c
    void OnSpeedUp(bool confirmed);              // @0x378498
    void OnStopWithConfirm();                    // @0x3786b0
    void OnStop();                               // @0x3789ec
    void OnUpgrade(bool confirmed);              // @0x378d0c
    void OnStopAndUpgrade(bool confirmed);       // @0x378f60
    void OnLaunchContract();                     // @0x379a1c
    void ShowArrowAt(unsigned i);                // @0x37a7d0
    void ShowArrowAtUpgrade();                   // @0x37a938 at the first locked order

    struct ItemHolder {                // 0x10 bytes, item_order_item_holder_%02d
        GUI::Window* root = nullptr;   // +0x00
        GUI::Window* ordered = nullptr;    // +0x04 item_order_ordered_item (the running order)
        GUI::Window* icon = nullptr;   // +0x08 icon_active
        GUI::Window* lock = nullptr;   // +0x0c icon_lock_16
    };
    struct ProgressInfo {              // 0x20 bytes, progress_bar_item_order
        GUI::Window* root = nullptr;   // +0x00
        GUI::Window* color = nullptr;  // +0x04 progres_bar_color
        GUI::Textfield* under = nullptr;   // +0x08 text_under
        GUI::Textfield* over = nullptr;    // +0x0c text_over
        GUI::ClipRect clip = {0, 0, 0, 0}; // +0x10 the fill
        void SetProgress(float p);     // @0x377de4
    };
    // The order rows: icons and texts of time, cost and reward.
    struct Row {
        GUI::Window* root = nullptr;
        GUI::Window* icon[3] = {};
        GUI::Textfield* text[3] = {};
    };

    GUI::Window* root = nullptr;       // +0x34
    GUI::Textfield* header = nullptr;  // +0x38 header_text
    ItemHolder items[5];               // +0x3c
    GUI::Window* order = nullptr;      // +0x8c button_item_order
    GUI::Window* orderIcon = nullptr;  // +0x90
    GUI::Textfield* orderText = nullptr;   // +0x94 text_over_green_2lines
    GUI::Window* speedup = nullptr;    // +0x98 button_item_speed_up
    GUI::Window* speedupIcon = nullptr;    // +0x9c icon_35
    GUI::Window* speedupCrystal = nullptr; // +0xa0 icon_35_crystal
    GUI::Window* speedupItem = nullptr;    // +0xa4 speed_up_item
    GUI::Textfield* speedupText = nullptr; // +0xa8 text
    GUI::Textfield* speedupPrice = nullptr;    // +0xac text_price
    GUI::Window* stop = nullptr;       // +0xb0 button_item_stop_prod
    GUI::Window* stopIcon = nullptr;   // +0xb4 icon_35
    GUI::Textfield* stopText = nullptr;    // +0xb8 text_give_job
    GUI::Textfield* jobText = nullptr; // +0xbc text_job
    ProgressInfo progress;             // +0xc0
    Row resTimeRes;                    // +0xe0 order_holder_res_time_res (time, cost -> reward)
    Row resTimeItem3;                  // +0xfc order_holder_res_time_item_x3 (time, gold, resource)
    Row resTimeItem;                   // +0x118 order_holder_res_time_item (time, gold)
    Row earned;                        // +0x12c order_holder_already_earned (icon_25_02, text_item_from_02)
    Row locked;                        // +0x138 order_holder_order_locked (text_locked)
    GUI::Window* selected[5] = {};     // +0x140 selected_item_%02d
    unsigned selectedOrder = 0;        // +0x154 the order shown, 0-based
    float left = 0.f;                  // +0x158 the share of the running order still to do
    float leftNext = 1.f;              // +0x15c the same one second later
    uint32_t second = 0;               // +0x160
    float secondFrac = 1.f;            // +0x164
    bool speedingUp = false;           // +0x168
    bool finishAfterSpeedup = false;   // +0x169 the speed-up's reward is dropped on the next Update
    float speedT = 0.f;                // +0x16c
    bool progressPercent = false;      // +0x170 Setting "progress_as_time" == 0
    bool multiFarm = false;            // +0x171 planting every empty patch
    unsigned multiCount = 1;           // +0x174 how many patches the price is paid for
    bool arrowShown = false;           // +0x178
    bool ignoreCompletion = false;     // +0x179
    bool itemBoosts = false;           // +0x17a Setting "item_boosts" == 1
    unsigned boostItem = 0;            // +0x17c
    int boostAmount = 0;               // +0x180
};

// The common frame of the stacked info windows (Unit_info_hint.xml, a 9-sliced golden box sized
// to its parts, and the Hint_window_footer.xml tail under it). Subclasses register their parts and
// lay them out downwards from the top. Port of BaseHoverWindow (0x4c bytes, vtable from 0x36f5d0).
class BaseHoverWindow : public BuildingHoverWindow {
public:
    BaseHoverWindow();                           // @0x36fa84
    ~BaseHoverWindow() override;                 // @0x36f988
    void InitBase(bool footerScreenSpace);       // BaseHoverWindow::Init(bool) @0x36f894
    void SetZ(float z) override;                 // +0x10 @0x36f5d0
    bool Click(int x, int y, bool pressed) override;   // +0x14 @0x36f624 inside the box (and 19 px under)
    void Show() override;                        // +0x1c @0x36f840
    void Hide() override;                        // +0x20 @0x36f7ec
    void SetPosition(int x, int y) override;     // +0x3c @0x36f680
    void SetBuilding(Map::Building* b) override { building = b; }   // +0x40 @0x36f7e8
    void FixWindowPosition(int& x, int& y) override;   // +0x50 @0x36f73c
    virtual void SetSize(int w, int h);          // +0x58 @0x36f6e0
    virtual int GetPaddingLeft() { return 0; }   // +0x5c @0x36f72c
    virtual int GetPaddingTop() { return -22; }  // +0x60 @0x36f734

    GUI::Window* root = nullptr;       // +0x34 Unit_info_hint.xml
    GUI::Window* border = nullptr;     // +0x38 golden_border_box
    int borderW = 1;                   // +0x3c its layout width
    GUI::Window* footer = nullptr;     // +0x48 Hint_window_footer.xml
};

// A house's info window: name, the Improve button, taxes earned per collection, the time to the
// next collection, and its residents (tap one to find them). Port of LivingHoverWindow (0xf4
// bytes, Init @0x38ddcc, SetBuilding @0x38c934, Update @0x38c4cc).
class LivingHoverWindow : public BaseHoverWindow {
public:
    LivingHoverWindow();                         // @0x38e564
    ~LivingHoverWindow() override;               // @0x38e2f0
    void Init() override;                        // +0x08 @0x38ddcc
    void SetZ(float z) override;                 // +0x10 @0x38da74
    bool Click(int x, int y, bool pressed) override;   // +0x14 @0x38d974
    void Show() override;                        // +0x1c @0x38dcb8
    void Hide() override;                        // +0x20 @0x38db9c
    void Update(float dt) override;              // +0x24 @0x38c4cc
    void SetPosition(int x, int y) override;     // +0x3c @0x38d658
    void SetBuilding(Map::Building* b) override; // +0x40 @0x38c934
    unsigned GetResidentCount();                 // @0x38c360 (at most 4 rows)
    void OnUpgrade();                            // @0x38c390
    void ShowArrowAtUpgrade();                   // @0x38c3e8

    struct Resident {                  // 0x18 bytes, Button_house_pick_small.xml
        GUI::Window* root = nullptr;   // +0x00
        GUI::Window* bg = nullptr;     // +0x04 green_button_bg_compact
        GUI::Window* icon = nullptr;   // +0x08 icon_gold_find
        GUI::Textfield* job = nullptr; // +0x0c text_job
        GUI::Textfield* name = nullptr;    // +0x10 text_name
        GUI::Button* clickArea = nullptr;  // +0x14
    };

    int padding = 5;                   // +0x4c
    GUI::Window* header = nullptr;     // +0x50 Unit_info_hint_header.xml
    GUI::Textfield* headerText = nullptr;   // +0x54
    GUI::Window* upgrade = nullptr;    // +0x58 Button_house_upgrade.xml
    GUI::Textfield* upgradeText = nullptr;  // +0x5c
    GUI::Button* upgradeClick = nullptr;    // +0x60
    GUI::Window* separator = nullptr;  // +0x64 Window_hints_sepparator.xml
    GUI::Window* taxes = nullptr;      // +0x68 Hint_taxes_earned_holder.xml
    GUI::Window* taxesIcon = nullptr;  // +0x6c icon_25_02
    GUI::Textfield* taxesText = nullptr;    // +0x70 text_item_from_02
    GUI::Window* progress = nullptr;   // +0x74 Progress_bar_mobile.xml
    GUI::Window* bar = nullptr;        // +0x78 unit_info_progress_bar (frame 1)
    GUI::Window* fill = nullptr;       // +0x7c its copy at frame 100, cropped to the progress
    GUI::Textfield* under = nullptr;   // +0x80
    GUI::Textfield* over = nullptr;    // +0x84
    GUI::Window* separator2 = nullptr; // +0x88
    GUI::Window* hereLives = nullptr;  // +0x8c Sepparator_here_works.xml ("Living here:")
    Resident residents[4];             // +0x90
    bool arrowShown = false;           // +0xf0
};

// The storage's info window: its space and goblins, the stored amounts (white, yellow from 80%,
// red when full), the Improve button and the goblin hire buttons (gold, or crystals; shown only by
// the tutorial in 5.11). Port of StorageHoverWindow (0x15c bytes, Init @0x397fd8, SetBuilding
// @0x3961a8).
class StorageHoverWindow : public BaseHoverWindow {
public:
    StorageHoverWindow();                        // @0x398980
    ~StorageHoverWindow() override;              // @0x3986ec
    void Init() override;                        // +0x08 @0x397fd8
    void SetZ(float z) override;                 // +0x10 @0x39777c
    bool Click(int x, int y, bool pressed) override;   // +0x14 @0x397604
    void Show() override;                        // +0x1c @0x397980
    void Hide() override;                        // +0x20 @0x397888
    void Update(float dt) override;              // +0x24 @0x395dd4
    float ZRange() override { return 0.01f; }    // +0x34 @0x395bcc
    void SetPosition(int x, int y) override;     // +0x3c @0x3971e8
    void SetBuilding(Map::Building* b) override; // +0x40 @0x3961a8
    unsigned GetResourceCount();                 // @0x396110 resource types held (at most 8)
    void OnUpgrade();                            // @0x396160
    void ShowArrowAtUpgrade();                   // @0x395cf8
    void OnHireGoblinGold();                     // @0x396010
    void OnHireGoblinCrystals(bool confirmed);   // @0x395f08

    struct Line {                      // 0x18 bytes, Unit_info_info_line.xml
        GUI::Window* root = nullptr;   // +0x00
        GUI::Textfield* name = nullptr;    // +0x04 unit_info_line_button.text_res
        GUI::Textfield* normal = nullptr;  // +0x08 .text
        GUI::Textfield* nearFull = nullptr;    // +0x0c .text_80_percent
        GUI::Textfield* full = nullptr;    // +0x10 .text_full
        GUI::Window* icon = nullptr;   // +0x14 .icon_place_holder
    };
    struct GoblinCost { int gold = 0, crystals = 0, level = 0; };   // 0xc bytes

    GUI::Window* header = nullptr;     // +0x40 Unit_info_hint_header.xml
    GUI::Textfield* headerText = nullptr;   // +0x44
    GUI::Window* upgrade = nullptr;    // +0x4c Button_house_upgrade.xml
    GUI::Textfield* upgradeText = nullptr;  // +0x50
    GUI::Button* upgradeClick = nullptr;    // +0x54
    GUI::Window* separator = nullptr;  // +0x58
    GUI::Window* info = nullptr;       // +0x5c Info_line_storage.xml
    GUI::Textfield* space = nullptr;   // +0x60 text_space
    GUI::Textfield* goblins = nullptr; // +0x64 text_goblins
    Line lines[8];                     // +0x68
    GUI::Window* separator2 = nullptr; // +0x128
    GUI::Window* hire = nullptr;       // +0x12c Buy_goblin_holder.xml (gold)
    GUI::Textfield* hireText = nullptr;     // +0x130 button_hire_goblin.text_hire_regular
    GUI::Textfield* hirePrice = nullptr;    // +0x134 .text_price_regular
    GUI::Window* hireIcon = nullptr;   // +0x138 .icon_bottom_bar_shop
    GUI::Window* hireLocked = nullptr; // +0x13c .locked_goblin
    GUI::Window* hireCrystals = nullptr;    // +0x140 Buy_goblin_holder.xml (crystals)
    GUI::Textfield* hireCrystalsText = nullptr;     // +0x144 button_hire_goblin_crystal.text_hire_premium
    GUI::Textfield* hireCrystalsPrice = nullptr;    // +0x148 .text_price
    unsigned goblinCostCount = 0;      // +0x14c Setting "goblin_cost" entries
    std::vector<GoblinCost> goblinCost;    // +0x150 [0] = 100 gold, then the setting's (by goblin count)
    int padding = 5;                   // +0x154
    bool arrowShown = false;           // +0x158
};

// The castle's info window: name, Rename, Improve, and the kingdom's worker counts (most, busy,
// idle). Port of CastleHoverWindow (0x98 bytes, Init @0x376514, SetBuilding @0x37587c).
class CastleHoverWindow : public BaseHoverWindow {
public:
    CastleHoverWindow();                         // @0x376af8
    ~CastleHoverWindow() override;               // @0x3768fc
    void Init() override;                        // +0x08 @0x376514
    void SetZ(float z) override;                 // +0x10 @0x3762c4
    bool Click(int x, int y, bool pressed) override;   // +0x14 @0x3761f8
    void Show() override;                        // +0x1c @0x376458
    void Hide() override;                        // +0x20 @0x376394
    void Update(float dt) override;              // +0x24 @0x375760
    void SetPosition(int x, int y) override;     // +0x3c @0x375f9c
    void SetBuilding(Map::Building* b) override; // +0x40 @0x37587c
    void OnUpgrade();                            // @0x375614
    void ShowArrowAtUpgrade();                   // @0x37567c

    GUI::Window* header = nullptr;     // +0x40
    GUI::Textfield* headerText = nullptr;   // +0x44
    GUI::Window* rename = nullptr;     // +0x4c Button_house_upgrade.xml
    GUI::Window* renameIcon = nullptr; // +0x50 icon_profession_builder (gold_crown)
    GUI::Textfield* renameText = nullptr;   // +0x54
    GUI::Window* upgrade = nullptr;    // +0x58 Button_house_upgrade.xml
    GUI::Textfield* upgradeText = nullptr;  // +0x5c
    GUI::Window* separator = nullptr;  // +0x60
    GUI::Window* subjects = nullptr;   // +0x64 Sepparator_here_works.xml
    GUI::Textfield* subjectsText = nullptr; // +0x68
    GUI::Window* lines[3] = {};        // +0x6c Info_1_line_castle.xml
    GUI::Textfield* labels[3] = {};    // +0x78 text_res
    GUI::Textfield* values[3] = {};    // +0x84 text
    int padding = 5;                   // +0x90
    bool arrowShown = false;           // +0x94
};

// The info window of building 0x81: only its name. Port of EmptyHoverWindow (0x4c bytes, Init
// @0x377bd8).
class EmptyHoverWindow : public BaseHoverWindow {
public:
    EmptyHoverWindow() { name = "EmptyHoverWindow"; }   // @0x377d5c
    ~EmptyHoverWindow() override { delete header; }     // @0x377c98
    void Init() override;                        // +0x08 @0x377bd8
    void SetZ(float z) override;                 // +0x10 @0x377b2c
    bool Click(int x, int y, bool pressed) override;   // +0x14 @0x377abc
    void Show() override;                        // +0x1c @0x377ba4
    void Hide() override;                        // +0x20 @0x377b6c
    void SetPosition(int x, int y) override;     // +0x3c @0x377a30
    void SetBuilding(Map::Building* b) override; // +0x40 @0x3779e4

    GUI::Window* header = nullptr;     // +0x40 Unit_info_hint_header.xml
    GUI::Textfield* headerText = nullptr;   // +0x44
};

// A tree's or rock's window while a worker gathers there: what is left, Collect (the gathered
// pile), Speed Up (crystals for a share of it), the time to the next unit, the worker (tap to find
// them) and Free Up. Port of ResourceHoverWindow (0xcc bytes, Init @0x393ad4, Update @0x392274).
class ResourceHoverWindow : public BaseHoverWindow {
public:
    ResourceHoverWindow();                       // @0x394454
    ~ResourceHoverWindow() override;             // @0x394240
    void Init() override;                        // +0x08 @0x393ad4
    void SetZ(float z) override;                 // +0x10 @0x393708
    bool Click(int x, int y, bool pressed) override;   // +0x14 @0x3935e8
    void Show() override;                        // +0x1c @0x3938a4
    void Hide() override;                        // +0x20 @0x3937dc
    void Update(float dt) override;              // +0x24 @0x392274
    float ZRange() override { return 0.002f; }   // +0x34 @0x391d9c
    void SetPosition(int x, int y) override;     // +0x3c @0x3933a8
    void SetBuilding(Map::Building* b) override; // +0x40 @0x392ed0
    void OnCollect();                            // @0x391f28
    void OnBoost();                              // @0x391e94
    void OnBoostFinished();                      // @0x391e00

    GUI::Window* header = nullptr;     // +0x4c Hint_shop_header.xml
    GUI::Textfield* headerText = nullptr;   // +0x50
    GUI::Textfield* headerDesc = nullptr;   // +0x54
    GUI::Window* separator = nullptr;  // +0x58
    GUI::Window* holder = nullptr;     // +0x5c Collect_hurry_holder.xml
    GUI::Window* collect = nullptr;    // +0x60 button_collect_large
    GUI::Textfield* collectText = nullptr;  // +0x64
    GUI::Textfield* collectCount = nullptr; // +0x68 text_header
    GUI::Button* collectClick = nullptr;    // +0x6c
    GUI::Window* collectIcon = nullptr;     // +0x70
    GUI::Window* boost = nullptr;      // +0x74 button_boost_small_small
    GUI::Textfield* boostText = nullptr;    // +0x78
    GUI::Textfield* boostPrice = nullptr;   // +0x7c text_for
    GUI::Textfield* boostAmount = nullptr;  // +0x80 text_quantity
    GUI::Button* boostClick = nullptr;      // +0x84
    GUI::Window* boostIcon = nullptr;  // +0x88 icon_holder_button_mill_res
    GUI::Window* progress = nullptr;   // +0x8c Progress_bar_mobile.xml
    GUI::Window* bar = nullptr;        // +0x90
    GUI::Window* fill = nullptr;       // +0x94
    GUI::Textfield* under = nullptr;   // +0x98
    GUI::Textfield* over = nullptr;    // +0x9c
    GUI::Window* worksHere = nullptr;  // +0xa0 Info_line_works_here_hint.xml
    GUI::Window* separator2 = nullptr; // +0xa4
    GUI::Window* workerHolder = nullptr;    // +0xa8 Button_working_worker.xml
    GUI::Window* worker = nullptr;     // +0xac button_worker
    GUI::Textfield* workerName = nullptr;   // +0xb0 text_give_job
    GUI::Window* workerIcon = nullptr; // +0xb4 icon_placeholder_35
    GUI::Button* workerClick = nullptr;     // +0xb8
    GUI::Window* fire = nullptr;       // +0xbc button_fire
    GUI::Button* fireClick = nullptr;  // +0xc0
    bool boosting = false;             // +0xc4
    float boostT = 0.f;                // +0xc8
};

// A tax-paying decoration's info window: name, its "<NAME>_DESC" text (with its collect time) and
// the time to its next taxes. Port of DecorationHoverWindow (0x74 bytes, Init @0x37761c,
// SetDecoration @0x376f58, Update @0x376ba0).
class DecorationHoverWindow : public BaseHoverWindow {
public:
    DecorationHoverWindow() { name = "DecorationHoverWindow"; }   // @0x37794c
    ~DecorationHoverWindow() override;           // @0x377854
    void Init() override;                        // +0x08 @0x37761c
    void SetZ(float z) override;                 // +0x10 @0x3774e0
    bool Click(int x, int y, bool pressed) override;   // +0x14 @0x377450
    void Show() override;                        // +0x1c @0x3775b8
    void Hide() override;                        // +0x20 @0x377550
    void Update(float dt) override;              // +0x24 @0x376ba0
    void SetPosition(int x, int y) override;     // +0x3c @0x3772e4
    void SetDecoration(Map::Decor* d) override;  // +0x48 @0x376f58

    int padding = 5;                   // +0x4c
    GUI::Window* header = nullptr;     // +0x50 Unit_info_hint_header.xml
    GUI::Textfield* headerText = nullptr;   // +0x54
    GUI::Window* desc = nullptr;       // +0x58 Unit_info_hint_6_lines_small.xml
    GUI::Textfield* descText = nullptr;     // +0x5c description_text
    GUI::Window* progress = nullptr;   // +0x60 Progress_bar_mobile.xml
    GUI::Window* bar = nullptr;        // +0x64
    GUI::Window* fill = nullptr;       // +0x68
    GUI::Textfield* under = nullptr;   // +0x6c
    GUI::Textfield* over = nullptr;    // +0x70
};
