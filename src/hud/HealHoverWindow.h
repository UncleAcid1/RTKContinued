// HealHoverWindow: the healing card over a wounded soldier's slot (the army bar's, or the hire
// window's squad / reserve): "Save <unit> hp/max" and a scrolled grid of the healing potions
// ("heal_warrior_items"), with the count owned and the health each gives. A potion not owned opens
// NeedItemWindow; drinking one heals (a dead soldier stands up again next to the hero).
// Port of HealHoverWindow (libkingdom.so 5.11), 0x3806a4..0x381d84, Hint_heal_warriors.xml.
// Offsets are the original's.
#pragma once
#include <vector>

#include "hud/ContentScroller.h"
#include "hud/HoverWindows.h"
#include "hud/Shared.h"

class HealHoverWindow : public BuildingHoverWindow {
public:
    HealHoverWindow();                           // @0x381b64 (also @0x381bd4)
    ~HealHoverWindow() override;                 // @0x381a24 (deleting @0x381c44, @0x381d84)
    void Init() override;                        // +0x08 @0x381594
    void SetZ(float z) override;                 // +0x10 @0x3806a4
    bool Click(int x, int y, bool pressed) override;   // +0x14 @0x38148c a tap elsewhere closes it
    void Move(int x, int y) override;            // +0x18 @0x381474
    void Show() override;                        // +0x1c @0x381558
    void Hide() override;                        // +0x20 @0x381520
    void Update(float dt) override;              // +0x24 @0x381410
    void SetPosition(int x, int y) override;     // +0x3c @0x381440
    void SetEntity(Entity* e) override { entity = e; }   // +0x44 @0x38143c
    void UpdateContents();                       // @0x380788
    void OnItem(unsigned i);                     // @0x381098 cell i: drink it, or where to find it

    GUI::Window* root = nullptr;            // +0x34
    GUI::Textfield* title = nullptr;        // +0x38 text_header_title
    Shared::ItemHolder holders[10];         // +0x3c two lines of five, numbered by column
    Shared::ContentScroller scroller;       // +0x58c
    // +0x604 (inside the scroller's object, at its +0x78): scroll_bar_warrior_healer, never read.
    GUI::Window* scrollBarBg = nullptr;
    std::vector<uint32_t> items;            // +0x698 the potions shown
};
