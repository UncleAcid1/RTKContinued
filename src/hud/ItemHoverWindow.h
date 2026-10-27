// ItemHoverWindow: the info popup of an item, a resource or a unit, with its action buttons
// (Drink, Turn on/off, Equip/Unequip, Repair, Remove, Improve/Craft, Find, Shop, Valor, Open, Ask
// friends, Train, Save, Move, Dismiss, Feed, Place). It points at a rectangle of its owner's UI
// (SetTargetRect) with one of eight frame arrows, picking another side when it would leave the
// screen (RecomputePosition). Port of ItemHoverWindow (0x38c bytes, vtable 0x609f00,
// @0x383478..0x38b...): about 25 owners create one (the belt and battle bars, the character,
// shop, crafting, chest, task, army windows ...).
//
// Each action opens its window (crafting, repair, the tavern / army, the world map, the raids,
// the reputations); the friends hub's is online (dormant).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "hud/HoverWindows.h"

namespace Items { struct ItemInfo; }
namespace GameState { struct PlayerItem; }
struct EntityData;

class ItemHoverWindow : public BuildingHoverWindow {
public:
    enum Direction { kS, kSE, kE, kNE, kN, kNW, kW, kSW };   // the arrow that points at the target

    // One action button (Button_dismiss.xml), 0x18 bytes. @0x38a52c ItemButton::Create.
    struct ItemButton {
        GUI::Window* root = nullptr;      // +0x00
        GUI::Window* blue = nullptr;      // +0x04 button_belt_blue
        GUI::Window* green = nullptr;     // +0x08 button_belt_green
        GUI::Window* icon = nullptr;      // +0x0c icon_placeholder_35
        GUI::Textfield* text = nullptr;   // +0x10
        GUI::Button* click = nullptr;     // +0x14 clickArea
        void Create();
    };
    struct EffectLine {                   // Hint_info_effect_1_line.xml, 0xc bytes
        GUI::Window* root = nullptr;
        GUI::Window* icon = nullptr;
        GUI::Textfield* text = nullptr;   // text_desc
    };

    ItemHoverWindow();                           // @0x38a7e8 (also @0x38a8d0)
    ~ItemHoverWindow() override;                 // @0x38a664 (deleting @0x38a718, @0x38a734)
    void Init() override;                        // +0x08 @0x38a9b8
    void SetZ(float z) override;                 // +0x10 @0x383478
    bool Click(int x, int y, bool pressed) override;   // +0x14 @0x38a308
    void Show() override;                        // +0x1c @0x38a4c0
    void Hide() override;                        // +0x20 @0x38a464
    void SetPosition(int x, int y) override;     // +0x3c @0x389bdc the frame's top left
    virtual float GetZ() { return z; }           // +0x54 @0x383504
    virtual void SetProbability(int p) { probability = p; }   // +0x58 @0x38350c a drop chance line
    // +0x5c @0x386d14: an item. item: the player's record (null: an item not owned); inventory: the
    // owned item's actions; allowFind: the Find button for an item a building makes; sourceText:
    // where it is found (the luck line); viewOnly: no crafting/find/place actions; noDescription:
    // no description and no Shop button; healMode: Drink uses it on the squad (HealSquadWindow);
    // noReputation: no Valor button.
    virtual void SetItem(const char32_t* unused, GameState::PlayerItem* item, Items::ItemInfo* info,
                         bool inventory, bool allowFind, const char32_t* sourceText, bool viewOnly,
                         bool noDescription, bool healMode, bool noReputation);
    virtual void SetResource(int type, bool allowFind);              // +0x60 @0x386b24
    virtual void SetText(const char32_t* title, const char32_t* text);   // +0x64 @0x385340
    // +0x68 @0x385c80: a unit (e, or its data when e is null): health, attacks, defenses; train:
    // Train more (army window); manage: Save / Move / Dismiss.
    virtual void SetEntity(Entity* e, EntityData* data, bool train, bool manage);
    using BuildingHoverWindow::SetEntity;
    virtual void OnDrink();                      // +0x6c @0x385a7c
    virtual void OnEnable();                     // +0x70 @0x385768 (also the Turn off button)
    virtual void OnEquip();                      // +0x74 @0x385728
    virtual void OnUnequip();                    // +0x78 @0x3856d0
    virtual void OnRepair();                     // +0x7c @0x385654
    virtual void OnRemove();                     // +0x80 @0x3854b4
    virtual void OnRemoveActual();               // +0x84 @0x385314
    virtual void OnImprove();                    // +0x88 @0x385270
    virtual void OnFind();                       // +0x8c @0x385030
    virtual void OnShop();                       // +0x90 @0x384f6c
    virtual void OnReputation();                 // +0x94 @0x384f10
    virtual void OnOpen();                       // +0x98 @0x384d14
    virtual void OnAskFriends();                 // +0x9c @0x384cdc
    virtual void OnTrain();                      // +0xa0 @0x384c6c
    virtual void OnHeal();                       // +0xa4 @0x384bd4
    virtual void OnMove();                       // +0xa8 @0x384bb4
    virtual void OnDismiss();                    // +0xac @0x384b94
    virtual void OnFeed();                       // +0xb0 @0x3849d8
    virtual void OnPlace();                      // +0xb4 @0x384620
    // The tutorials' arrows at a button (lock: only it takes taps).
    virtual void HelpEnable(bool lock);          // +0xb8 @0x3844b8
    virtual void HelpEquip(bool lock);           // +0xbc @0x3843cc
    virtual void HelpImprove(bool lock);         // +0xc0 @0x3842dc
    virtual void HelpRepair(bool lock);          // +0xc4 @0x3841f0
    virtual void HelpFeed(bool lock);            // +0xc8 @0x3840f8
    virtual void HelpDrink(bool lock);           // +0xcc @0x384004
    // +0xd0 @0x383514: the rectangle (screen pixels) to point at and the preferred side.
    virtual void SetTargetRect(int left, int top, int right, int bottom, Direction dir);
    virtual void RecomputePosition();            // +0xd4 @0x383570
    virtual void FinalizeSetup();                // +0xd8 @0x383c24 frame size and arrows

    ItemButton& Button(int k) { return buttons[k]; }
    enum ButtonIndex {
        kDrink, kEnable, kDisable, kEquip, kUnequip, kRepair, kRemove, kImprove, kFind, kShop,
        kReputation, kOpen, kAskFriends, kTrain, kHeal, kMove, kDismiss, kFeed, kPlace, kButtonCount
    };

    std::vector<GUI::Window*> windows;      // +0x34 every layout, owned (the frame first)
    std::vector<GUI::Window*> buttonRoots;  // +0x44 the buttons' roots (Click tries them)
    GUI::Window* frame = nullptr;           // +0x54 Mini_hint.xml
    GUI::Window* border = nullptr;          // +0x58 golden_border_box
    GUI::Window* arrows[8] = {};            // +0x5c dialog_arrow_s, se, e, ne, n, nw, w, sw
    int borderW = 0, borderH = 0;           // +0x7c +0x80 the border's layout size
    GUI::Window* nameWin = nullptr;         // +0x84 Hint_info_item_name.xml
    GUI::Textfield* classText[5] = {};      // +0x88 text_item_class_01..05 (by item_level)
    GUI::Window* separator = nullptr;       // +0x9c Window_hints_sepparator_small.xml
    EffectLine lines[8];                    // +0xa0
    GUI::Window* descWin = nullptr;         // +0x100 Unit_info_hint_6_lines_small.xml
    GUI::Textfield* descText = nullptr;     // +0x104 description_text
    GUI::Window* durSeparator = nullptr;    // +0x108 Window_hints_sepparator_small.xml
    GUI::Window* durWin = nullptr;          // +0x10c Hint_info_item_durability_small.xml
    GUI::Window* barDarkRed = nullptr;      // +0x110
    GUI::Textfield* textDarkRed = nullptr;  // +0x114
    GUI::Window* barRed = nullptr;          // +0x118
    GUI::Textfield* textRed = nullptr;      // +0x11c
    GUI::ClipRect clipRed = {0, 0, 0, 0x400};     // +0x120
    GUI::Window* barGreen = nullptr;        // +0x130
    GUI::Textfield* textGreen = nullptr;    // +0x134
    GUI::ClipRect clipGreen = {0, 0, 0, 0x400};   // +0x138
    GUI::Window* infoLine = nullptr;        // +0x148 Unit_info_info_line.xml (never shown in 5.11)
    GUI::Window* infoIcon = nullptr;        // +0x14c
    GUI::Textfield* infoTextRes = nullptr;  // +0x150
    GUI::Textfield* infoText = nullptr;     // +0x154
    GUI::Window* luckLine = nullptr;        // +0x158 Hint_info_icon_text.xml
    GUI::Window* luckIcon = nullptr;        // +0x15c icon_35
    GUI::Textfield* luckText = nullptr;     // +0x160 text_item_from_02
    ItemButton buttons[kButtonCount];       // +0x164
    bool inventory = false;                 // +0x32c
    GameState::PlayerItem* item = nullptr;  // +0x330
    Items::ItemInfo* info = nullptr;        // +0x334
    int resource = 0xb;                     // +0x338 (0xb: none)
    Entity* unit = nullptr;                 // +0x33c
    EntityData* unitData = nullptr;         // +0x340
    bool healMode = false;                  // +0x344
    int probability = 0;                    // +0x348
    int padLeft = 0x33, padTop = 0x2d;      // +0x34c +0x350
    int padRight = -0x22, padBottom = 10;   // +0x354 +0x358
    int target[4] = {};                     // +0x35c left, top, right, bottom
    int screen[4] = {};                     // +0x36c 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT
    int direction = kSW;                    // +0x37c (Direction)
    float z = 0.f;                          // +0x380
    int arrowShift = 0;                     // +0x384 the side arrow moved to stay on screen
    bool helpingImprove = false;            // +0x388
};

// @0x3845b0: CHANCE_LOW up to 100, MEDIUM up to 3000, HIGH up to 7000, else VERY_HIGH.
const char32_t* GetProbabilityString(unsigned p);
// @0x3849b4: the popup's "travel home" answer: home to the city (Tasks::HelpWithMapSwitchTo).
void ItemPlaceContinuation();
