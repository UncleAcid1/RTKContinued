// The world hover windows BuildingHovers shows over buildings, decorations and entities: a GUI
// layout drawn in world space (SetScreenSpace(false)) that follows its object. Each is a
// WindowQueue of its own, created by HoverInfo::SetHoverType and destroyed through RemoveWindow
// (pendingDestroy, WindowManager::DestroyPendingWindows).
// Ports of BuildingHoverWindow (vtable 0x609788), BubbleHoverWindow (0x609690, Init @0x370c0c) and
// TaxesHoverWindow (Init @0x39a7b0). Offsets are the original's.
#pragma once
#include "gui/GUI.h"
#include "gui/WindowManager.h"

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
