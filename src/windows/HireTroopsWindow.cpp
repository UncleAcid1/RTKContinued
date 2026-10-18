// HireTroopsWindow: the tavern and barracks window (Window_tavern.xml). Six tabs, one per training
// building (the tavern, the barracks, the range, the mage tower, ... and the storage's goblins); a
// strip of four warrior cards that scrolls sideways; under it the squad (3 slots) and the reserve
// (5 slots), whose soldiers are dragged from one to the other. Port of HireTroopsWindow
// (libkingdom.so 5.11), 0x2cdd30..0x2dae58. Statics at 0x621170..0x622a8c and 0x60f284..0x60f2b0.
//
// Milestone 5a: the window, its parts, opening, tabs, scrolling, the soldier drag. UNVERIFIED
// (milestone 5b): UpdateContents (the cards, the slots); (milestone 5c): the buttons' actions,
// Update, OnTweenIn's tutorial arrows.
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "engine/IconManager.h"
#include "engine/Render.h"
#include "engine/Timer.h"
#include "game/BuildingHovers.h"
#include "game/Entity.h"
#include "game/EntityData.h"
#include "game/EntityManager.h"
#include "game/GameState.h"
#include "game/Map.h"
#include "game/Setting.h"
#include "game/SoldierSlots.h"
#include "game/Squad.h"
#include "game/StringTable.h"
#include "game/Tavern.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/ContentScroller.h"
#include "hud/HUD.h"
#include "hud/ItemHoverWindow.h"
#include "hud/Shared.h"
#include "windows/Windows.h"

namespace HireTroopsWindow {
namespace {

using GUI::Textfield;
using GUI::Window;

// One item a warrior costs (CostInfo, 0x7c bytes): "%s.warrior_cost_field_tavern_%02d".
struct CostInfo {
    Window* root = nullptr;              // +0x00
    Shared::ItemIconHolder icon;         // +0x04 inventory_item_icon.inventory_item_icon_holder
    Textfield* quantity = nullptr;       // +0x74 inventory_item_icon.text_quantity
    Textfield* quantityRed = nullptr;    // +0x78 inventory_item_icon.text_quantity_red
    // @0x2cf964
    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        char name[300];
        std::snprintf(name, sizeof name, "%s.inventory_item_icon.inventory_item_icon_holder", p);
        icon.LoadFrom(w, name, false);
        quantity = GUI::GetWindowTypedF<Textfield>(w, "%s.inventory_item_icon.text_quantity", p);
        quantityRed = GUI::GetWindowTypedF<Textfield>(w, "%s.inventory_item_icon.text_quantity_red", p);
    }
};

// "Upgrade in progress" on a card (UpgradeInProgress, 0xc bytes).
struct UpgradeInProgress {
    Window* root = nullptr;              // +0x00
    Window* icon = nullptr;              // +0x04 icon_35 (takes a depth slot)
    Textfield* text = nullptr;           // +0x08 text_upgrade
    // @0x2cf8f4
    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        icon = GUI::GetWindowTypedF<Window>(w, "%s.icon_35", p);
        icon->takesZ = true;
        text = GUI::GetWindowTypedF<Textfield>(w, "%s.text_upgrade", p);
    }
};

// The card's crystal "buy now" button, with a crossed-out price for discounts
// (ButtonWarriorBuyNow, 0x20 bytes).
struct ButtonWarriorBuyNow {
    Window* root = nullptr;              // +0x00
    Textfield* text = nullptr;           // +0x04
    Window* crystal = nullptr;           // +0x08 icon_holder_button_mill_crystal ("16_crystal")
    Textfield* textFor = nullptr;        // +0x0c
    Window* crystalDisc = nullptr;       // +0x10 icon_holder_button_mill_crystal_disc ("16_crystal")
    Textfield* textForActual = nullptr;  // +0x14
    Textfield* textForDisc = nullptr;    // +0x18
    Window* crossout = nullptr;          // +0x1c element_crossout
    // @0x2cf77c
    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        text = GUI::GetWindowTypedF<Textfield>(w, "%s.text", p);
        crystal = GUI::GetWindowTypedF<Window>(w, "%s.icon_holder_button_mill_crystal", p);
        crystal->SetTexture(IconManager::GetIcon("16_crystal", false), true);
        textFor = GUI::GetWindowTypedF<Textfield>(w, "%s.text_for", p);
        crystalDisc = GUI::GetWindowTypedF<Window>(w, "%s.icon_holder_button_mill_crystal_disc", p);
        crystalDisc->SetTexture(IconManager::GetIcon("16_crystal", false), true);
        textForActual = GUI::GetWindowTypedF<Textfield>(w, "%s.text_for_actual", p);
        textForDisc = GUI::GetWindowTypedF<Textfield>(w, "%s.text_for_disc", p);
        crossout = GUI::GetWindowTypedF<Window>(w, "%s.element_crossout", p);
    }
};

// One warrior card (WarriorInfo, 0x3e4 bytes): "tavern_warriors_cole.hire_warrior_tavern_%02d".
struct WarriorInfo {
    Window* root = nullptr;              // +0x00
    Window* transparent = nullptr;       // +0x04 scaler_transparent
    Window* selecter = nullptr;          // +0x08 scaler_golden_border_selecter
    Window* icon1 = nullptr;             // +0x0c icon_01 ("gold_16_health")
    Window* icon2 = nullptr;             // +0x10 icon_02 (takes a depth slot)
    Textfield* hp = nullptr;             // +0x14 text_hp
    Textfield* attack = nullptr;         // +0x18 text_attack
    Textfield* name = nullptr;           // +0x1c text_warrior_name
    Textfield* cost = nullptr;           // +0x20 text_cost
    Textfield* requires = nullptr;       // +0x24 text_requires
    CostInfo costs[3];                   // +0x28 warrior_cost_field_tavern_01..03
    Window* train = nullptr;             // +0x19c button_warrior_train
    Window* trainIcon = nullptr;         // +0x1a0 .icon_sword (takes a depth slot)
    Textfield* trainText = nullptr;      // +0x1a4 .text
    UpgradeInProgress upgrading;         // +0x1a8 upgrade_in_progress
    ButtonWarriorBuyNow buyNow;          // +0x1b4 button_warrior_buy_now
    Window* hireRes = nullptr;           // +0x1d4 button_tavern_hire_res_small
    Window* hireResIcon = nullptr;       // +0x1d8 .icon_sword (takes a depth slot)
    Textfield* hireResText = nullptr;    // +0x1dc .text
    Window* hireCrystal = nullptr;       // +0x1e0 button_tavern_hire_crystal_small
    Textfield* hireCrystalText = nullptr;    // +0x1e4 .text_button_2
    Window* hireCrystalIcon = nullptr;   // +0x1e8 .icon_35 (takes a depth slot)
    Textfield* hireCrystalPrice = nullptr;   // +0x1ec .text_price
    Window* hireCrystalCrystal = nullptr;    // +0x1f0 .icon_35_crystal ("35_crystal")
    Window* build = nullptr;             // +0x1f4 button_build_upgrade
    Textfield* buildText = nullptr;      // +0x1f8 .text
    Textfield* buildTextInactive = nullptr;  // +0x1fc .text_inactive
    Window* buildLock = nullptr;         // +0x200 .lock
    Window* buildIcon = nullptr;         // +0x204 .icon_build ("gold_job_builder")
    Window* hang = nullptr;              // +0x208 button_tavern_hang (a new offer for crystals)
    Textfield* hangText = nullptr;       // +0x20c .text_button_2
    Textfield* hangPrice = nullptr;      // +0x210 .text_price
    Window* hangIcon = nullptr;          // +0x214 .icon_35 (takes a depth slot)
    Window* hangCrystal = nullptr;       // +0x218 .icon_35_crystal ("16_crystal")
    Window* timeout = nullptr;           // +0x21c tavern_warrior_timeout
    Window* timeoutIcon = nullptr;       // +0x220 .icon_timer (the sand clock)
    Textfield* nextIn = nullptr;         // +0x224 .text_next_warrior_in
    Textfield* nextTime = nullptr;       // +0x228 .text_next_warrior_time
    Shared::WarriorSlotInfo slot1;       // +0x22c slot_warrior_01
    Shared::WarriorSlotInfo slot2;       // +0x308 slot_warrior_02

    // @0x2cfa2c
    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        transparent = GUI::GetWindowTypedF<Window>(w, "%s.scaler_transparent", p);
        selecter = GUI::GetWindowTypedF<Window>(w, "%s.scaler_golden_border_selecter", p);
        icon1 = GUI::GetWindowTypedF<Window>(w, "%s.icon_01", p);
        icon2 = GUI::GetWindowTypedF<Window>(w, "%s.icon_02", p);
        icon2->takesZ = true;
        icon1->SetTexture(IconManager::GetIcon("gold_16_health", false), true);
        hp = GUI::GetWindowTypedF<Textfield>(w, "%s.text_hp", p);
        attack = GUI::GetWindowTypedF<Textfield>(w, "%s.text_attack", p);
        name = GUI::GetWindowTypedF<Textfield>(w, "%s.text_warrior_name", p);
        cost = GUI::GetWindowTypedF<Textfield>(w, "%s.text_cost", p);
        requires = GUI::GetWindowTypedF<Textfield>(w, "%s.text_requires", p);
        char buf[300];
        for (int i = 0; i < 3; ++i) {
            std::snprintf(buf, sizeof buf, "%s.warrior_cost_field_tavern_%02d", p, i + 1);
            costs[i].LoadFrom(w, buf);
        }
        train = GUI::GetWindowTypedF<Window>(w, "%s.button_warrior_train", p);
        trainIcon = GUI::GetWindowTypedF<Window>(w, "%s.button_warrior_train.icon_sword", p);
        trainIcon->takesZ = true;
        trainText = GUI::GetWindowTypedF<Textfield>(w, "%s.button_warrior_train.text", p);
        std::snprintf(buf, sizeof buf, "%s.button_warrior_buy_now", p);
        buyNow.LoadFrom(w, buf);
        std::snprintf(buf, sizeof buf, "%s.upgrade_in_progress", p);
        upgrading.LoadFrom(w, buf);
        hireRes = GUI::GetWindowTypedF<Window>(w, "%s.button_tavern_hire_res_small", p);
        hireResIcon = GUI::GetWindowTypedF<Window>(w, "%s.button_tavern_hire_res_small.icon_sword", p);
        hireResIcon->takesZ = true;
        hireResText = GUI::GetWindowTypedF<Textfield>(w, "%s.button_tavern_hire_res_small.text", p);
        hireCrystal = GUI::GetWindowTypedF<Window>(w, "%s.button_tavern_hire_crystal_small", p);
        hireCrystalIcon = GUI::GetWindowTypedF<Window>(w, "%s.button_tavern_hire_crystal_small.icon_35", p);
        hireCrystalIcon->takesZ = true;
        hireCrystalText = GUI::GetWindowTypedF<Textfield>(w, "%s.button_tavern_hire_crystal_small.text_button_2", p);
        hireCrystalPrice = GUI::GetWindowTypedF<Textfield>(w, "%s.button_tavern_hire_crystal_small.text_price", p);
        hireCrystalCrystal = GUI::GetWindowTypedF<Window>(w, "%s.button_tavern_hire_crystal_small.icon_35_crystal", p);
        hireCrystalCrystal->SetTexture(IconManager::GetIcon("35_crystal", false), true);
        build = GUI::GetWindowTypedF<Window>(w, "%s.button_build_upgrade", p);
        buildText = GUI::GetWindowTypedF<Textfield>(w, "%s.button_build_upgrade.text", p);
        buildTextInactive = GUI::GetWindowTypedF<Textfield>(w, "%s.button_build_upgrade.text_inactive", p);
        buildLock = GUI::GetWindowTypedF<Window>(w, "%s.button_build_upgrade.lock", p);
        buildIcon = GUI::GetWindowTypedF<Window>(w, "%s.button_build_upgrade.icon_build", p);
        buildIcon->SetTexture(IconManager::GetIcon("gold_job_builder", false), true);
        hang = GUI::GetWindowTypedF<Window>(w, "%s.button_tavern_hang", p);
        hangText = GUI::GetWindowTypedF<Textfield>(w, "%s.button_tavern_hang.text_button_2", p);
        hangPrice = GUI::GetWindowTypedF<Textfield>(w, "%s.button_tavern_hang.text_price", p);
        hangIcon = GUI::GetWindowTypedF<Window>(w, "%s.button_tavern_hang.icon_35", p);
        hangIcon->takesZ = true;
        hangCrystal = GUI::GetWindowTypedF<Window>(w, "%s.button_tavern_hang.icon_35_crystal", p);
        hangCrystal->SetTexture(IconManager::GetIcon("16_crystal", false), true);
        timeout = GUI::GetWindowTypedF<Window>(w, "%s.tavern_warrior_timeout", p);
        timeoutIcon = GUI::GetWindowTypedF<Window>(w, "%s.tavern_warrior_timeout.icon_timer", p);
        timeoutIcon->SetTexture(IconManager::GetSandClockIcon(), false);
        nextIn = GUI::GetWindowTypedF<Textfield>(w, "%s.tavern_warrior_timeout.text_next_warrior_in", p);
        nextTime = GUI::GetWindowTypedF<Textfield>(w, "%s.tavern_warrior_timeout.text_next_warrior_time", p);
        std::snprintf(buf, sizeof buf, "%s.slot_warrior_01", p);
        slot1.LoadFrom(w, buf);
        std::snprintf(buf, sizeof buf, "%s.slot_warrior_02", p);
        slot2.LoadFrom(w, buf);
    }
};

// The building each tab trains at (0x598884).
const uint32_t kTabBuildings[6] = {0x8f, 0x8e, 0x69, 0x93, 0x609, 0x12};

WindowManager::FunctionalWindow* g_wnd = nullptr;   // HireTroopsWindow::wnd (0x622a8c)
Window* g_root = nullptr;                // 0x621170 Window_tavern
GUI::TweenEffect* g_tween = nullptr;     // 0x621174
Window* g_speedup = nullptr;             // 0x621178 Compact_warrior_speedup
Shared::ContentScroller g_scroller;      // 0x62117c the cards
Window* g_scrollFrame = nullptr;         // 0x6211f4 (the scroller's +0x78) tavern_hori_scrollbar.scaleform
unsigned g_tab = 0;                      // 0x621288
WarriorInfo g_warriors[4];               // 0x62128c
bool g_arrowOnBuild = false;             // 0x62221c ShowForBuilding's flag: the arrow at the build button
Shared::WarriorSlotInfo g_squad[3];      // 0x622224 troops_squad_holder.counter_slot_01..03
Shared::WarriorSlotInfo g_reserve[5];    // 0x6224b8 troops_reserve_holder.counter_slot_01..05
std::vector<Tavern::TavernInfo*> g_offers;   // 0x622918 the tab's warriors (UpdateContents)
ItemHoverWindow* g_hover = nullptr;      // 0x622924
int g_unk622928[4] = {};                 // 0x622928..0x622934 (zeroed by Init)
Shared::WarriorSlotInfo* g_slots[8];     // 0x622938 the squad's three, then the reserve's five
Shared::TabHolder g_tabs[6];             // 0x622958 tab_01..06
Textfield* g_title = nullptr;            // 0x622a30 text_title
Window* g_left = nullptr;                // 0x622a34 tavern_warriors_cole.button_navigate_left
Window* g_right = nullptr;               // 0x622a38 tavern_warriors_cole.button_navigate_right
Textfield* g_squadHeader = nullptr;      // 0x622a3c troops_squad_holder.header_text
Textfield* g_reserveHeader = nullptr;    // 0x622a40 troops_reserve_holder.header_text
Textfield* g_hired = nullptr;            // 0x622a44 text_hired_count
Textfield* g_hiredFull = nullptr;        // 0x622a48 text_hired_count_full
Textfield* g_hiredNum = nullptr;         // 0x622a4c text_hired_count_num
Textfield* g_hiredFullNum = nullptr;     // 0x622a50 text_hired_count_full_num
Textfield* g_speedupJob = nullptr;       // 0x622a54 text_job
Textfield* g_hurryText = nullptr;        // 0x622a58 button_hurry_tiny.text
Textfield* g_hurryPrice = nullptr;       // 0x622a5c button_hurry_tiny.text_price
Window* g_luxBg = nullptr;               // 0x622a64 sliced_lux_bg
Window* g_separator1 = nullptr;          // 0x622a68 sepparator_01
Window* g_separator2 = nullptr;          // 0x622a6c sepparator_02
Window* g_darkenNum = nullptr;           // 0x622a70 troops_reserve_holder.darken_num
Window* g_squadHolder = nullptr;         // 0x622a74 troops_squad_holder
Window* g_reserveHolder = nullptr;       // 0x622a78 troops_reserve_holder
Window* g_backLight = nullptr;           // 0x622a7c back_light_blue
int g_backLightH = 0;                    // 0x622a80 its height in the layout
Window* g_close = nullptr;               // 0x622a84 x_button.clickArea
Window* g_hurry = nullptr;               // 0x622a88 button_hurry_tiny
int g_arrowSlot = -1;                    // 0x60f284 the card the tutorial arrow points at (ShowArrow)
unsigned g_poolSlot = 0xffffffff;        // 0x60f288 the reserve slot SoldierMove takes from
int g_tavernRespawn = 0;                 // 0x60f28c Setting "tavern_respawn"
int g_warRespawn = 0xa8c;                // 0x60f290 "barracks_war_respawn"
int g_archRespawn = 0xa8c;               // 0x60f294 "barracks_arch_respawn"
int g_mageRespawn = 0xe10;               // 0x60f298 "barracks_mage_resapwn"
float g_scale = 1.f;                     // 0x60f2a4
unsigned g_squadSlot = 0xffffffff;       // 0x60f2a8 the squad slot a dragged soldier left
int g_unk60f2ac = -1;                    // 0x60f2ac (Show: -1)
bool g_tablet = true;                    // 0x60f2b0

// UNVERIFIED (milestone 5c): the buttons' actions are not ported yet.
void OnItem(unsigned, unsigned) {}       // (0x2ceca4)
void OnTrain(unsigned) {}                // (0x2d9a1c)
void OnHire(unsigned, bool) {}           // (0x2d8b90)
void OnBlueButton(unsigned) {}           // (0x2d03f4)
void OnRefresh(unsigned, bool) {}        // (0x2d8830)
void OnSquad(unsigned) {}                // (0x2d80d0)
void OnReserve(unsigned) {}              // (0x2d7d08)
void OnHintGold(bool) {}                 // (0x2d7ae8)

bool InRect(const Window* w, int x, int y) {
    const int l = g_root->x + w->x, t = g_root->y + w->y;
    return l <= x && x <= l + w->w && t <= y && y <= t + w->h;
}

// @0x2cdd48
float ZRange() { return g_wnd->shown ? 0.12f : 0.001f; }

// @0x2cdd78
void SetZ(float z) {
    if (!g_wnd->shown) return;
    g_root->SetZ(z);
    g_root->SetPosition(g_tween->GetCenteredX(), g_tween->GetCenteredY());
    g_speedup->SetZ(0.001f);
}

// @0x2cde4c / @0x2cde70: the strip scrolls by one card.
void OnLeft() { g_scroller.bounce += (float)g_scroller.itemSize; }
void OnRight() { g_scroller.bounce -= (float)g_scroller.itemSize; }

// @0x2cded0
void OnTweenOut() {
    WindowManager::WindowHide(false);
    g_wnd->shown = false;
    g_wnd->MoveWindowDown(true);
    GameState::DropGamePauseState();
}

// @0x2cef54
bool OnBack() {
    if (!g_wnd->shown) return false;
    if (!g_hover->shownHover) {
        Hide();
        return true;
    }
    g_hover->Hide();
    return true;
}

// @0x2d871c: a squad soldier goes to the reserve; else the one in reserve slot g_poolSlot joins
// the squad.
void SoldierMove(uint32_t id) {
    Entity* e = SoldierSlots::GetFirstSoldierByID(id);
    if (!e) {
        e = SoldierPool::GetSoldierBySlot(g_poolSlot);
        SoldierPool::RemoveSoldier(e);
        SoldierSlots::AddSoldier(e);
        GameState::AddSoldiersToSquad(EntityManager::GetPlayer()->GetSquad(), e);
    } else {
        SoldierSlots::RemoveSoldier(e, false);
        SoldierPool::AddSoldier(e);
    }
    UpdateContents();
    BattleBarWindow::UpdateContents();
}

// @0x2cf4d8: a pressed slot whose finger moved over 10 px starts a drag: its icon is copied into a
// half-transparent sprite that follows the finger.
void Move(int x, int y) {
    g_scroller.Move(x, y);
    for (Shared::WarriorSlotInfo* s : g_slots) {
        if (!s->pressed) continue;
        if (!s->dragging) {
            if (std::abs(y - s->pressY) + std::abs(x - s->pressX) <= 10) continue;
            Window* icon = s->soldierFine;
            s->dragging = true;
            icon->SetVisibility(false);
            Render::RemoveSprite(s->dragSprite);
            Render::Sprite* sp = Render::CreateSprite(icon->texture, 0xf, false, false);
            sp->screenSpace = true;
            Render::SetShaderType(sp, 1);
            Render::SetAlpha(sp, 0.5f);
            Render::SetPosition(sp, (float)(g_root->x + icon->x), (float)(icon->h + icon->y + g_root->y), 0.001f);
            sp->h = (float)icon->h;
            sp->w = (float)icon->w;
            Render::SetMagnifyFilter(sp->tex, true);
            s->dragSprite = sp;
        }
        if (Render::Sprite* sp = s->dragSprite)
            Render::SetPosition(sp, (float)(x - s->pressX) + sp->x, (float)(y - s->pressY) + sp->y, sp->z);
        s->pressY = y;
        s->pressX = x;
    }
}

// The soldier let go over slot `to` from slot `from` (squad 0..2, reserve 3..7): it changes sides
// (swapping with the one there when that side is full). True when the slots changed.
bool DropSoldier(unsigned from, unsigned to) {
    Shared::WarriorSlotInfo* s = g_slots[from];
    Shared::WarriorSlotInfo* t = g_slots[to];
    if (from < 3) {
        // squad -> reserve (not in a fight, nor before the second tutorial's end)
        if (s->soldier == t->soldier || GameState::IsCombatActive() || GameState::secondTutorial != 0x100)
            return false;
        if (SoldierPool::GetOccupiedSlotCount() < GameState::GetSoldierReserveSlots()) {
            g_squadSlot = from;
            SoldierMove(s->soldier);
            return true;
        }
        Entity* in = SoldierPool::GetSoldierBySlot(to - 3);
        if (in->workType != 0 || SoldierSlots::GetFirstSoldierByID(in->GetEntityData()->id) || in->IsDead())
            return false;
        SoldierPool::RemoveSoldier(in);
        Entity* out = SoldierSlots::GetSoldierBySlot(from);
        SoldierSlots::RemoveSoldier(out, false);
        SoldierSlots::AddSoldier(in);
        GameState::AddSoldiersToSquad(EntityManager::GetPlayer()->GetSquad(), in);
        SoldierPool::AddSoldier(out);
        return true;
    }
    // reserve -> squad (one of each kind in the squad)
    if (s->soldier == t->soldier || SoldierSlots::GetFirstSoldierByID(s->soldier)) return false;
    if (SoldierSlots::GetOccupiedSlotCount() < GameState::GetSoldierSlotCount()) {
        Entity* in = SoldierPool::GetSoldierBySlot(from - 3);
        SoldierPool::RemoveSoldier(in);
        SoldierSlots::AddSoldier(in);
        GameState::AddSoldiersToSquad(EntityManager::GetPlayer()->GetSquad(), in);
    } else {
        Entity* out = SoldierSlots::GetSoldierBySlot(to);
        SoldierSlots::RemoveSoldier(out, false);
        Entity* in = SoldierPool::GetSoldierBySlot(from - 3);
        SoldierPool::RemoveSoldier(in);
        SoldierSlots::AddSoldier(in);
        GameState::AddSoldiersToSquad(EntityManager::GetPlayer()->GetSquad(), in);
        SoldierPool::AddSoldier(out);
    }
    return true;
}

// @0x2da374
bool Click(int x, int y, bool pressed) {
    if (g_root->visibleSelf) {
        if (BuildingHovers::ClickOnArrow(x, y, pressed)) return true;
        if (!pressed && GameState::tutorial != 0x61) BuildingHovers::HideArrow();
        // UNVERIFIED (milestone 5e): the HealHoverWindow (0x622220) takes the click first.
        if (g_hover->Click(x, y, pressed)) return true;
        if (GameState::tutorial != 0x61 && g_scroller.Click(x, y, pressed, g_root)) return true;
        if (pressed) {
            // A press on a slot holding a soldier (in reserve: one at home and alive) may start a drag.
            if (!GameState::IsCombatActive()) {
                for (unsigned i = 0; i < 8; ++i) {
                    Shared::WarriorSlotInfo* s = g_slots[i];
                    if (s->soldier == 0) continue;
                    if (i > 2) {
                        Entity* e = SoldierPool::GetSoldierBySlot(i - 3);
                        if (e && (e->workType == 1 || e->IsDead())) continue;
                    }
                    if (!InRect(s->root, x, y)) continue;
                    s->pressX = x;
                    s->pressed = true;
                    s->pressY = y;
                    break;
                }
            }
        } else {
            // A drag let go: over a slot of the other side the soldier moves; else its icon glides back.
            bool handled = false;
            for (unsigned i = 0; i < 8; ++i) {
                Shared::WarriorSlotInfo* s = g_slots[i];
                s->pressed = false;
                if (!s->dragging) continue;
                s->dragging = false;
                handled = true;
                bool moved = false;
                for (unsigned j = 0; j < 8; ++j) {
                    Shared::WarriorSlotInfo* t = g_slots[j];
                    if (t->noDrop || !InRect(t->root, x, y)) continue;
                    if (i != j && (i < 3) != (j < 3)) moved = DropSoldier(i, j);
                    break;
                }
                if (moved) {
                    s->dragSprite = Render::RemoveSprite(s->dragSprite);
                    UpdateContents();
                    BattleBarWindow::UpdateContents();
                    continue;
                }
                Render::Sprite* sp = s->dragSprite;
                s->returnSprite = Render::RemoveSprite(s->returnSprite);
                s->returnTime = 0.f;
                s->dragSprite = nullptr;
                s->returnX = sp->x;
                s->returnY = sp->y;
                s->returnSprite = sp;
            }
            if (handled) return true;
        }
    }
    // (Also while hidden: in tutorial step 0x62 every click stops here, else the speed-up bubble
    // and the city's top bar get it.)
    if (GameState::tutorial == 0x62) return true;
    if (g_speedup->Click(x, y, pressed, false)) return true;
    if (!pressed) g_speedup->SetVisibility(false);
    if (TopCityWindow::Click(x, y, pressed)) return true;
    const bool visible = g_root->visibleSelf;
    g_root->Click(x, y, pressed, false);
    return visible;
}

}  // namespace

// Static FunctionalWindow("HireTroopsWindow", Init, Deinit, Click, SetZ, Hide, Move)
WindowManager::FunctionalWindow* Queue() {
    if (!g_wnd) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.click = Click;
        f.setZ = SetZ;
        f.hide = Hide;
        f.move = Move;
        g_wnd = new WindowManager::FunctionalWindow("HireTroopsWindow", std::move(f));
    }
    return g_wnd;
}

// @0x2cde20
bool IsVisible() { return g_wnd->shown; }

// @0x2cde34: the next opening points the tutorial arrow at the first card.
void ShowArrow() { g_arrowSlot = 1; }

// @0x2dae58
void Init() {
    g_unk622928[3] = 0;
    g_unk622928[0] = g_unk622928[1] = g_unk622928[2] = 0;
    g_tablet = GUI::IsTabletVersion();
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Window_tavern.xml", "Window_tavern.png", -1.f, 0, 0,
                             0x24, 0x23, false, 1.f);
    g_wnd->RegisterTopWindow(g_root);
    g_scale = g_root->scale;
    g_root->SetVisibility(false);
    g_wnd->fn.show = Show;
    // UNVERIFIED (milestone 5c): fn.update = Update (0x2d91b8).
    g_tween = GUI::CreateTweenEffect(g_root, nullptr, nullptr /* UNVERIFIED (milestone 5c): OnTweenIn */, OnTweenOut);
    g_tween->swingIn.clear();   // (SetSwingInSound(nullptr))
    g_tween->CenterWith(0, (int)(g_scale * -35.f), 0, 0);
    g_close = GUI::GetWindowTyped<GUI::Button>(g_root, "x_button.clickArea");
    g_close->SetOnClick(Hide);
    g_close->SetActionSound("ui_close", 1);
    g_backLight = GUI::GetWindowTyped<Window>(g_root, "back_light_blue");
    g_backLightH = g_backLight->h;
    g_title = GUI::GetWindowTyped<Textfield>(g_root, "text_title");
    char name[128];
    for (unsigned i = 0; i < 6; ++i) {
        std::snprintf(name, sizeof name, "tab_%02d", i + 1);
        g_tabs[i].LoadFrom(g_root, name, true);
        g_tabs[i].root->SetOnClick([i] { OnTab(i, false); });
        g_tabs[i].root->SetActionSound("ui_tab", 1);
    }
    g_left = GUI::GetWindowTyped<Window>(g_root, "tavern_warriors_cole.button_navigate_left");
    g_left->SetOnClick(OnLeft);
    g_right = GUI::GetWindowTyped<Window>(g_root, "tavern_warriors_cole.button_navigate_right");
    g_right->SetOnClick(OnRight);
    g_left->MoveWindow(-(int)(g_scale * 30.f), 0);
    g_right->MoveWindow((int)(g_scale * 30.f), 0);
    for (unsigned i = 0; i < 4; ++i) {
        WarriorInfo& w = g_warriors[i];
        std::snprintf(name, sizeof name, "tavern_warriors_cole.hire_warrior_tavern_%02d", i + 1);
        w.LoadFrom(g_root, name);
        w.root->MoveWindow((int)(g_scale * -15.f), 0);
        for (unsigned k = 0; k < 3; ++k) {
            w.costs[k].root->SetOnClick([i, k] { OnItem(i, k); });
            w.costs[k].icon.lock->SetVisibility(false);
            for (Shared::DiscountInfo& d : w.costs[k].icon.discount) d.root->SetVisibility(false);
        }
        w.train->SetOnClick([i] { OnTrain(i); });
        w.buyNow.root->SetOnClick([i] { OnHire(i, false); });
        w.hireRes->SetOnClick([i] { OnTrain(i); });
        w.hireCrystal->SetOnClick([i] { OnHire(i, false); });
        w.build->SetOnClick([i] { OnBlueButton(i); });
        w.buildLock->SetVisibility(false);
        w.hang->SetOnClick([i] { OnRefresh(i, false); });
        w.slot1.golden->SetVisibility(false);
        w.slot1.bgRed->SetVisibility(false);
        w.slot2.golden->SetVisibility(false);
        w.slot2.bgRed->SetVisibility(false);
    }
    // The fourth card waits one card further right (the strip recycles them).
    g_warriors[3].root->MoveWindow(g_warriors[1].root->x - g_warriors[0].root->x, 0);
    g_scroller.Init();
    Window* mask = GUI::GetWindowTyped<Window>(g_root, "tavern_warriors_cole.mask_container");
    g_scroller.viewport = {mask->x, mask->y, mask->x + mask->w, mask->y + mask->h};
    g_scroller.track = mask;
    for (WarriorInfo& w : g_warriors) {
        w.root->SetClipRect(&g_scroller.viewport);
        g_scroller.windows.push_back(w.root);
    }
    g_scrollFrame = GUI::GetWindowTyped<Window>(g_root, "tavern_warriors_cole.tavern_hori_scrollbar.scaleform");
    g_scroller.itemSize = g_warriors[1].root->x - g_warriors[0].root->x;
    g_scroller.perLine = 1;
    g_scroller.horizontal = true;
    g_scroller.unkA0 = 4;
    g_scroller.onScroll = UpdateContents;
    g_scroller.scrollbar =
        GUI::GetWindowTyped<Window>(g_root, "tavern_warriors_cole.tavern_hori_scrollbar.scroll_bar_horisontal_tiny");
    g_luxBg = GUI::GetWindowTyped<Window>(g_root, "sliced_lux_bg");
    g_separator1 = GUI::GetWindowTyped<Window>(g_root, "sepparator_01");
    g_separator2 = GUI::GetWindowTyped<Window>(g_root, "sepparator_02");
    g_darkenNum = GUI::GetWindowTyped<Window>(g_root, "troops_reserve_holder.darken_num");
    g_squadHolder = GUI::GetWindowTyped<Window>(g_root, "troops_squad_holder");
    g_squadHeader = GUI::GetWindowTyped<Textfield>(g_root, "troops_squad_holder.header_text");
    for (unsigned i = 0; i < 3; ++i) {
        std::snprintf(name, sizeof name, "troops_squad_holder.counter_slot_%02d", i + 1);
        g_squad[i].LoadFrom(g_root, name);
        g_squad[i].root->SetOnClick([i] { OnSquad(i); });
        g_squad[i].golden->SetVisibility(false);
        for (Shared::DiscountInfo& d : g_squad[i].discount) d.root->SetVisibility(false);
    }
    g_reserveHolder = GUI::GetWindowTyped<Window>(g_root, "troops_reserve_holder");
    g_reserveHeader = GUI::GetWindowTyped<Textfield>(g_root, "troops_reserve_holder.header_text");
    g_hired = GUI::GetWindowTyped<Textfield>(g_root, "troops_reserve_holder.text_hired_count");
    g_hiredFull = GUI::GetWindowTyped<Textfield>(g_root, "troops_reserve_holder.text_hired_count_full");
    g_hiredNum = GUI::GetWindowTyped<Textfield>(g_root, "troops_reserve_holder.text_hired_count_num");
    g_hiredFullNum = GUI::GetWindowTyped<Textfield>(g_root, "troops_reserve_holder.text_hired_count_full_num");
    for (unsigned i = 0; i < 5; ++i) {
        std::snprintf(name, sizeof name, "troops_reserve_holder.counter_slot_%02d", i + 1);
        g_reserve[i].LoadFrom(g_root, name);
        g_reserve[i].root->SetOnClick([i] { OnReserve(i); });
        g_reserve[i].bgRed->SetVisibility(false);
        for (Shared::DiscountInfo& d : g_reserve[i].discount) d.root->SetVisibility(false);
    }
    // The "hurry" bubble over a slot whose soldier is training.
    g_speedup = GUI::RegisterUI("../resource/kingdom_ui/1Original/Compact_warrior_speedup.xml",
                                "Compact_warrior_speedup.png", g_scale, 0, 0, 0, 0, false, 1.f);
    g_speedup->SetVisibility(false);
    g_speedupJob = GUI::GetWindowTyped<Textfield>(g_speedup, "text_job");
    g_hurry = GUI::GetWindowTyped<Window>(g_speedup, "button_hurry_tiny");
    g_hurry->SetOnClick([] { OnHintGold(false); });
    g_hurryText = GUI::GetWindowTyped<Textfield>(g_speedup, "button_hurry_tiny.text");
    g_hurryPrice = GUI::GetWindowTyped<Textfield>(g_speedup, "button_hurry_tiny.text_price");
    g_tavernRespawn = Setting("tavern_respawn").GetInt();
    g_warRespawn = Setting("barracks_war_respawn").GetInt();
    g_archRespawn = Setting("barracks_arch_respawn").GetInt();
    g_mageRespawn = Setting("barracks_mage_resapwn").GetInt();
    for (unsigned i = 0; i < 3; ++i) g_slots[i] = &g_squad[i];
    for (unsigned i = 0; i < 5; ++i) g_slots[3 + i] = &g_reserve[i];
    g_wnd->hideOnOuterClick = true;
    g_wnd->fn.zRange = ZRange;
    g_wnd->fn.back = OnBack;
    g_wnd->fn.show = Show;
    g_hover = new ItemHoverWindow();
    // UNVERIFIED (milestone 5e): new HealHoverWindow (0x622220).
}

// @0x2ceeac
void Hide() {
    if (g_root->visibleSelf) g_tween->AnimateOut();
    g_speedup->SetVisibility(false);
    for (Shared::WarriorSlotInfo& s : g_squad) s.returnSprite = Render::RemoveSprite(s.returnSprite);
    for (Shared::WarriorSlotInfo& s : g_reserve) s.returnSprite = Render::RemoveSprite(s.returnSprite);
}

// @0x2d9650: not in the opening tutorial, nor in an arena fight (but for its result screen).
void Show() {
    if (GameState::IsTutorial()) return;
    if (GameState::IsArenaCombatActive() && !PlayerBattleInOutWindow::IsVisible()) {
        PopupWindow::Show(StringTable::GetString("CAN_NOT_OPEN_IN_ARENA"), PopupWindow::Hide, nullptr, nullptr, nullptr,
                          nullptr);
        return;
    }
    if (g_root->visibleSelf) return;
    // In step 0x61 only the first card shows: the window is centred higher.
    g_tween->CenterWith((int)(g_scale * -30.f), (int)(g_scale * -50.f), 0,
                        GameState::tutorial == 0x61 ? (int)(g_scale * -120.f) : 0);
    g_wnd->shown = true;
    g_wnd->MoveWindowOnTop(true);
    if (!g_root->visibleSelf) {
        WindowManager::WindowShow(false);
        g_tween->AnimateIn();
    }
    g_root->SetVisibility(true);
    {
        Timer::TimeCompensationBlock block;
        g_unk60f2ac = -1;
        // (EntityFactory::SetTransactionTime(Timer::GetGlobalTime()): the server's clock, online.)
        UpdateContents();
        const bool full = GameState::tutorial != 0x61;
        g_luxBg->SetVisibility(full);
        g_separator1->SetVisibility(full);
        g_separator2->SetVisibility(full);
        g_darkenNum->SetVisibility(full);
        g_squadHolder->SetVisibility(full);
        g_reserveHolder->SetVisibility(full);
        int h = g_backLightH;
        if (!full) {
            BuildingHovers::HideWorldDialog(nullptr);
            GUI::SetInteractionObjectLock(g_warriors[0].train, g_warriors[0].costs[0].root);
            ShowArrow();
            h = g_backLightH - g_luxBg->h;
        }
        g_backLight->SetSize((unsigned)g_backLight->w, (unsigned)h);
    }
    if (GameState::GetCurrentLocation() == 2) GameState::RaiseGamePauseState();
    g_root->PreloadTextures();   // GUI::PreloadWindow
}

// @0x2d99d8: the tab of building `id` (if it has one), then Show.
void ShowForBuilding(uint32_t id, bool arrowOnBuild) {
    for (unsigned i = 0; i < 6; ++i) {
        if (kTabBuildings[i] == id) {
            OnTab(i, arrowOnBuild);
            break;
        }
    }
    Show();
}

// @0x2d8f04: the strip goes back to its start; it then opens on the strongest warrior the
// building's level trains (with arrowOnBuild: the first one it does not train yet), and on the
// goblins' tab at the last goblin hired.
void OnTab(unsigned tab, bool arrowOnBuild) {
    g_tab = tab;
    g_scroller.firstIndex = 0;
    g_scroller.HandleMove(-g_scroller.offset);
    for (WarriorInfo& w : g_warriors) {
        w.root->RestorePosition();
        w.root->MoveWindow((int)(g_scale * -15.f), 0);
    }
    g_warriors[3].root->MoveWindow(g_warriors[1].root->x - g_warriors[0].root->x, 0);
    UpdateContents();
    int level = Map::GetBuildingMaxUpgrade(kTabBuildings[g_tab]);
    if (GameState::GetCurrentMapID() != 0) {
        if (GameState::OfflineBuilding* b = GameState::GetOfflineBuildingWithID(kTabBuildings[g_tab]))
            level = (int)b->level + 1;
    }
    if (level != 0 && !g_offers.empty()) {
        unsigned best = 0, bestIndex = 0;
        for (unsigned i = 0; i < g_offers.size(); ++i) {
            if (!g_offers[i]) continue;
            const EntityData* d = EntityFactory::GetEntityByID((int)g_offers[i]->entity);
            if ((d->unlockQuest == 0 || GameState::TaskCompleted(d->unlockQuest)) &&
                (unsigned)d->trainingObjCount <= (unsigned)(level - (arrowOnBuild ? 0 : 1)) &&
                best < (unsigned)d->trainingObjCount) {
                best = (unsigned)d->trainingObjCount;
                bestIndex = i;
            }
        }
        if (best != 0) {
            g_scroller.firstIndex = bestIndex;
            UpdateContents();
        }
    }
    if (arrowOnBuild && g_tab == 5) {
        g_scroller.ScrollIntoView(GameState::GetGoblinCount() - 1);
        UpdateContents();
    }
    g_arrowOnBuild = arrowOnBuild;
}

// (0x2d05e4) UNVERIFIED (milestone 5b): the tabs, the cards, the squad and reserve slots (30 KB
// of asm, not ported yet); the window shows its layout as loaded.
void UpdateContents() {}

}  // namespace HireTroopsWindow
