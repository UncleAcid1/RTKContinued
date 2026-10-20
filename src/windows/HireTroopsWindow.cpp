// HireTroopsWindow: the tavern and barracks window (Window_tavern.xml). Six tabs, one per training
// building (the tavern, the barracks, the range, the mage tower, ... and the storage's goblins); a
// strip of four warrior cards that scrolls sideways; under it the squad (3 slots) and the reserve
// (5 slots), whose soldiers are dragged from one to the other. Port of HireTroopsWindow
// (libkingdom.so 5.11), 0x2cdd30..0x2dae58. Statics at 0x621170..0x622a8c and 0x60f284..0x60f2b0.
//
// Milestone 5a: the window, its parts, opening, tabs, scrolling, the soldier drag; 5b: UpdateContents
// (the cards, the slots); 5c-1: Train and Hire, the hire glide, Update, OnTweenIn's tutorial arrows;
// 5c-2: the cost cells' hints, refresh, the squad / reserve slots' hints (move, dismiss, unlock);
// 5c-3: the build / upgrade / task button.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "engine/IconManager.h"
#include "engine/StlSort.h"
#include "engine/Render.h"
#include "engine/Timer.h"
#include "game/AIState.h"
#include "game/BuildingHovers.h"
#include "game/Contracts.h"
#include "game/Entity.h"
#include "game/EntityData.h"
#include "game/EntityManager.h"
#include "game/GameData.h"
#include "game/Items.h"
#include "game/Locations.h"
#include "game/GameState.h"
#include "game/Map.h"
#include "game/Setting.h"
#include "game/SoldierSlots.h"
#include "game/Squad.h"
#include "game/StringTable.h"
#include "game/Tasks.h"
#include "game/Tavern.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/ContentScroller.h"
#include "hud/HUD.h"
#include "hud/HealHoverWindow.h"
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
    // @0x2ce488: one price.
    void ShowPrice(unsigned price) {
        crystal->SetVisibility(true);
        textFor->SetVisibility(true);
        textFor->SetText(ToWideString((int)price));
        crystalDisc->SetVisibility(false);
        textForActual->SetVisibility(false);
        textForDisc->SetVisibility(false);
        crossout->SetVisibility(false);
    }
    // @0x2ce394: the sale price, and the full one crossed out.
    void ShowPriceDiscounted(unsigned price, unsigned full) {
        crystal->SetVisibility(false);
        textFor->SetVisibility(false);
        crystalDisc->SetVisibility(true);
        textForActual->SetVisibility(true);
        textForActual->SetText(ToWideString((int)price));
        textForDisc->SetVisibility(true);
        textForDisc->SetText(ToWideString((int)full));
        crossout->SetVisibility(true);
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
// 0x622904 (a small vector: data, inline buffer 0x622908, capacity 0x622910, count 0x622914):
// the reserve's soldiers and their unit ids, made by UpdateContents.
struct ReserveEntry { Entity* soldier; uint32_t entity; };
std::vector<ReserveEntry> g_reserveList;
std::vector<Tavern::TavernInfo*> g_offers;   // 0x622918 the tab's warriors, by slot (UpdateContents)
ItemHoverWindow* g_hover = nullptr;      // 0x622924
HealHoverWindow* g_heal = nullptr;       // 0x622220 the healing card over a wounded soldier's slot
int g_buildTask[4] = {};                 // 0x622928 per card: the task its build button goes to (0: none)
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
// 0x60f28c: seconds before a tab's next warrior can be taken: Settings "tavern_respawn",
// "barracks_war_respawn", "barracks_arch_respawn", "barracks_mage_resapwn" (set by Init); the
// tamer's 0 and the goblins' 1 stay.
int g_respawn[6] = {0, 0xa8c, 0xa8c, 0xe10, 0, 1};
float g_scale = 1.f;                     // 0x60f2a4
unsigned g_squadSlot = 0xffffffff;       // 0x60f2a8 the squad slot a dragged soldier left
int g_tappedUnit = -1;                   // 0x60f2ac the tapped slot's unit id (-1: none; only written)
bool g_tablet = true;                    // 0x60f2b0

int g_lastSecond = 0;                    // 0x622a60 the global second Update last refreshed the contents in

void OnTrain(unsigned n);
void OnHire(unsigned n, bool confirmed);
void Update(float dt);
void OnTweenIn();

void OnItem(unsigned n, unsigned k);
void OnRefresh(unsigned n, bool confirmed);
void OnSquad(unsigned n);
void OnReserve(unsigned n);
void OnHintGold(bool confirmed);
void OnHintDismissContinue();

void OnBlueButton(unsigned n);

bool InRect(const Window* w, int x, int y) {
    const int l = g_root->x + w->x, t = g_root->y + w->y;
    return l <= x && x <= l + w->w && t <= y && y <= t + w->h;
}

// @0x2cdd30
bool SortTavernInfoBySlot(const Tavern::TavernInfo* a, const Tavern::TavernInfo* b) { return a->slot < b->slot; }

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
        if (g_heal->Click(x, y, pressed)) return true;
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
    g_buildTask[3] = 0;
    g_buildTask[0] = g_buildTask[1] = g_buildTask[2] = 0;
    g_tablet = GUI::IsTabletVersion();
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Window_tavern.xml", "Window_tavern.png", -1.f, 0, 0,
                             0x24, 0x23, false, 1.f);
    g_wnd->RegisterTopWindow(g_root);
    g_scale = g_root->scale;
    g_root->SetVisibility(false);
    g_wnd->fn.show = Show;
    g_wnd->fn.update = Update;
    g_tween = GUI::CreateTweenEffect(g_root, nullptr, OnTweenIn, OnTweenOut);
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
    g_respawn[0] = Setting("tavern_respawn").GetInt();
    g_respawn[1] = Setting("barracks_war_respawn").GetInt();
    g_respawn[2] = Setting("barracks_arch_respawn").GetInt();
    g_respawn[3] = Setting("barracks_mage_resapwn").GetInt();
    for (unsigned i = 0; i < 3; ++i) g_slots[i] = &g_squad[i];
    for (unsigned i = 0; i < 5; ++i) g_slots[3 + i] = &g_reserve[i];
    g_wnd->hideOnOuterClick = true;
    g_wnd->fn.zRange = ZRange;
    g_wnd->fn.back = OnBack;
    g_wnd->fn.show = Show;
    g_hover = new ItemHoverWindow();
    g_heal = new HealHoverWindow();
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
        g_tappedUnit = -1;
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

namespace {

// @0x2ce260: a unit's resource price; with the server's setting 0xb3 (never set offline) the
// building that makes item `item` adds that setting times its order's resource count.
int GetAdjustedResourceCost(uint32_t item, int cost) {
    const int mult = GameState::GetServerSetting(0xb3);
    if (mult == 0) return cost;
    unsigned slot = 0;
    const uint32_t id = NeedItemWindow::GetBuildingThatProducesItem(item, slot);
    const GameData::BuildingData* b = id ? GameData::GetBuilding(id) : nullptr;
    if (b && b->delivery) cost += (int)b->delivery->missions[slot].priceResourceCount * mult;
    return cost;
}

// @0x2ce2e0: the crystals a new offer costs on tab `tab` (the refresh button).
int GetResetCost(unsigned tab) {
    if (tab == 1) return Setting("tavern_reset_war").GetInt();
    if (tab == 2) return Setting("tavern_reset_arch").GetInt();
    if (tab == 3) return Setting("tavern_reset_mage").GetInt();
    return Setting("tavern_reset").GetInt();
}

// The task that unlocks a building's level `level` (0: none).
// PORT: a level past the building's list counts as none (the original reads past it).
uint32_t UpgradeQuest(const GameData::BuildingData* b, unsigned level) {
    if (!b || level == 0 || level - 1 >= b->upgrades.size()) return 0;
    return (uint32_t)b->upgrades[level - 1].quest;
}

// The requirement text is built in one 0x100-character buffer, each line appended.
void Append(std::u32string& text, const char32_t* format, std::initializer_list<StringArgument> args = {}) {
    text += SWPrintf(0x100 - (unsigned)text.size(), format, args);
}

// One cost line of a card: the icon, "n" in plain and red, and the owned / missing marks.
void ShowCost(CostInfo& c, int need, int have) {
    c.quantity->SetText(ToWideString(need));
    c.quantityRed->SetText(ToWideString(need));
    c.icon.owned->SetVisibility(need <= have);
    c.icon.redX->SetVisibility(have < need);
    c.quantity->SetVisibility(c.icon.owned->visibleSelf);
    c.quantityRed->SetVisibility(c.icon.redX->visibleSelf);
    c.icon.itemClass[0]->SetVisibility(c.icon.owned->visibleSelf);
    c.icon.locked->SetVisibility(c.icon.redX->visibleSelf);
}

}  // namespace

// @0x2d05e4: the tabs, the visible cards and the squad / reserve slots.
void UpdateContents() {
    g_reserveList.clear();
    for (unsigned i = 0; i < SoldierPool::GetOccupiedSlotCount(); ++i)
        g_reserveList.push_back({SoldierPool::GetSoldierBySlot(i), SoldierPool::GetSoldierBySlot(i)->GetEntityData()->id});
    // (A search for an empty squad slot follows here; its result is not used.)
    for (unsigned i = 0; i < 6; ++i) g_tabs[i].SetMode(g_tab == i, false);
    g_tabs[0].icon->SetTexture(IconManager::GetIcon("army_tavern_40", false), true);
    g_tabs[1].icon->SetTexture(IconManager::GetIcon("army_warrior_40", false), true);
    g_tabs[2].icon->SetTexture(IconManager::GetIcon("army_archer_40", false), true);
    g_tabs[3].icon->SetTexture(IconManager::GetIcon("army_mage_40", false), true);
    g_tabs[4].icon->SetTexture(IconManager::GetIcon("prof_tamer_35", false), true);
    g_tabs[5].icon->SetTexture(IconManager::GetIcon("goblin_60", false), false);
    // The goblins only in the city; the tamer after task 0x771.
    g_tabs[5].root->SetVisibility(GameState::GetCurrentMapID() == 0);
    g_tabs[4].root->SetEnabled(GameState::TaskCompleted(0x771));
    // The tab's offers, by slot.
    g_offers.clear();
    for (unsigned i = 0; Tavern::TavernInfo* t = Tavern::EnumTavernInfo(i); ++i)
        if (t->type == g_tab) g_offers.push_back(t);
    if (!g_offers.empty())
        StlSort::Sort(g_offers.data(), g_offers.data() + g_offers.size(),
                      [](const Tavern::TavernInfo* a, const Tavern::TavernInfo* b) { return SortTavernInfoBySlot(a, b); });
    // The tab's building: its name in the title, its level and whether it is being upgraded (a
    // map away from the city: the copy kept for it). The tavern (0x8f) and 0x609 always count as built.
    const GameData::BuildingData* building = GameData::GetBuilding(kTabBuildings[g_tab]);
    bool hasBuilding = true;
    unsigned buildingLevel = 0;
    bool upgrading = false;
    if (building) {
        g_title->SetText(StringTable::GetString(building->name.c_str()));
        if (building->requiresQuest != 0) GameState::TaskCompleted((unsigned)building->requiresQuest);
        Map::Building* b = GameState::GetCurrentMapID() == 0 ? Map::GetBuildingWithID(building->id) : nullptr;
        if (b) {
            buildingLevel = (unsigned)b->level;
            upgrading = b->upgrading != 0;
        } else if (GameState::OfflineBuilding* o = GameState::GetOfflineBuildingWithID(building->id)) {
            buildingLevel = o->level;
            upgrading = (o->flags & 4) != 0;
        } else {
            hasBuilding = false;
        }
        if (building->id == 0x8f || building->id == 0x609) hasBuilding = true;
    }
    g_left->SetEnabled(g_scroller.CanMoveLeft());
    g_right->SetEnabled(g_scroller.CanMoveRight());
    for (unsigned n = 0; n < 4; ++n) {
        WarriorInfo& card = g_warriors[n];
        const unsigned index = g_scroller.firstIndex + n;
        if (index >= g_offers.size()) {
            card.root->SetVisibility(false);
            continue;
        }
        Tavern::TavernInfo* offer = g_offers[index];
        card.root->SetVisibility(offer != nullptr);
        if (!offer) continue;
        const EntityData* d = EntityFactory::GetEntityByID((int)offer->entity);
        if (!d) return;
        // An offer seen for the first time is ready at once, with its HP drawn (the barracks'
        // first two warriors: 8 and 12).
        const int respawn = g_respawn[g_tab];
        if (GameState::GetTavernWarriorTS(offer->entity) == 0) {
            GameState::SetTavernWarriorTS(offer->entity, Timer::GetGlobalTime() - (uint32_t)respawn);
            GameState::SetTavernWarriorHP(offer->entity, (uint32_t)offer->GenerateHP());
            if (g_tab == 1) GameState::SetTavernWarriorHP(offer->entity, index == 0 ? 8 : 0xc);
        }
        // levelLock: the player's level is below the offer's (not for goblins); trainable: its
        // task is done, the building is high enough and so is the player (goblins: always);
        // canTrain: also built, not card 2 during tutorial step 0x61, and a goblin not yet hired.
        const bool levelLock = offer->level > (unsigned)GameState::GetLevel() && g_tab != 5;
        bool trainable;
        if (d->unlockQuest != 0 && !GameState::TaskCompleted(d->unlockQuest))
            trainable = false;
        else if (buildingLevel < (unsigned)d->trainingObjCount)
            trainable = false;
        else if (offer->level > (unsigned)GameState::GetLevel())
            trainable = g_tab == 5;
        else
            trainable = true;
        bool canTrain;
        if (!hasBuilding || !trainable)
            canTrain = false;
        else if (n == 1)
            canTrain = GameState::tutorial != 0x61;
        else
            canTrain = true;
        if (g_tab == 5 && index > (unsigned)GameState::GetGoblinCount()) {
            canTrain = false;
            trainable = false;
        }
        Tasks::TaskInfo* unlockTask = Tasks::GetTask(d->unlockQuest);
        const int elapsed = (int)(Timer::GetGlobalTime() - GameState::GetTavernWarriorTS(offer->entity));
        const int remaining = respawn - elapsed > 0 ? respawn - elapsed : 0;
        const bool ready = remaining == 0;
        int attack = d->attackMelee;
        if (attack == 0) attack = d->attackRanged;
        if (attack == 0) attack = d->attackMagic;
        if (offer->attack != 0) attack = (int)offer->attack;
        const unsigned gold = d->GetCostGold(false, offer);
        const unsigned crystals = d->GetCostCrystals(false, offer);
        card.icon1->SetVisibility(g_tab != 5);
        card.hp->SetVisibility(g_tab != 5);
        card.hp->SetText(SWPrintf(0x100, U"%s %s",
                                  {offer->hpMax ? ToWideString(offer->hpMax) : U"??",
                                   StringTable::GetString("stats_hp_points")}).c_str());
        card.icon2->SetVisibility(g_tab != 5);
        card.attack->SetVisibility(g_tab != 5);
        card.attack->SetText(SWPrintf(0x100, U"%s %s",
                                      {attack ? ToWideString(attack) : U"??",
                                       StringTable::GetString("stats_dmg_points")}).c_str());
        if (g_tab == 5)
            card.name->SetText(StringTable::GetString("STORAGE_GOBLINS_STATIC"));
        else
            card.name->SetText(offer->hpMax ? d->title : StringTable::GetString("TAVERN_UNKNOWN"));
        // In the city the barracks train; the tavern (and away from the city everything) hires.
        const bool training = GameState::GetCurrentMapID() == 0 && g_tab != 0;
        const char* performKey = training ? "PERFORM_TRAIN" : "PERFORM_HIRE";
        const char* hireKey = training && g_tab == 5 ? "SPAWN_GOB" : "HIRE_TROOPS_HIRE";
        card.cost->SetText(StringTable::GetString(training ? "HIRE_TROOPS_TRAINING_COST" : "HIRE_TROOPS_HIRE_COST"));
        // buyCrystal: the offer can be had for crystals.
        bool buyCrystal = false;
        if (canTrain) {
            canTrain = ready && gold != 0 ? !levelLock : false;
            buyCrystal = crystals != 0;
        }
        bool goblinOwned = false;
        if (g_tab == 5 && index < (unsigned)GameState::GetGoblinCount()) {
            goblinOwned = true;
            canTrain = false;
        }
        // The tamer's tab: a "tame" or "find" task (0x3f / 0x43) that is ready and not locked.
        bool tamerTask = false;
        if (hasBuilding && !buyCrystal && unlockTask && g_tab == 4 && ready &&
            (unlockTask->subtasks[0].type == 0x3f || unlockTask->subtasks[0].type == 0x43) && !unlockTask->IsLockedByTask())
            tamerTask = true;
        // Crystals buy only at the tavern, and goblins above the player's level.
        if (canTrain && buyCrystal && g_tab != 0) {
            if (g_tab != 5 || offer->level <= (unsigned)GameState::GetLevel()) buyCrystal = false;
        }
        if (d->attackMagic != 0)
            card.icon2->SetTexture(IconManager::GetIcon(d->GetSmallSpellIcon(), false), true);
        else if (d->attackRanged != 0)
            card.icon2->SetTexture(IconManager::GetIcon("gold_16_ranged_attack", false), true);
        else if (d->attackMelee != 0)
            card.icon2->SetTexture(IconManager::GetIcon("gold_16_attack", false), true);
        // A crystal-priced unit has the golden frame.
        card.transparent->SetVisibility(crystals == 0);
        card.selecter->SetVisibility(crystals != 0);
        card.transparent->SetEnabled(canTrain || buyCrystal);
        card.selecter->SetEnabled(canTrain || buyCrystal);
        // The goblins show the second slot window, the others the first.
        Shared::WarriorSlotInfo& slot = g_tab == 5 ? card.slot2 : card.slot1;
        slot.root->SetVisibility(true);
        (g_tab == 5 ? card.slot1 : card.slot2).root->SetVisibility(false);
        if (!canTrain) {
            if (buyCrystal) {
                slot.root->SetEnabled(true);
                slot.lockIcon->SetVisibility(false);
                slot.wingsEmpty->SetVisibility(false);
                card.cost->SetVisibility(false);
            } else {
                slot.root->SetEnabled(goblinOwned);
                slot.lockIcon->SetVisibility(!trainable);
                slot.wingsEmpty->SetVisibility(false);
                card.cost->SetVisibility(false);
            }
            for (CostInfo& c : card.costs) c.root->SetVisibility(false);
        } else {
            slot.root->SetEnabled(true);
            slot.lockIcon->SetVisibility(false);
            slot.wingsEmpty->SetVisibility(false);
            card.cost->SetVisibility(true);
            // The costs: an item, gold, a resource (each line only once the offer is ready).
            card.costs[0].root->SetVisibility(ready && d->itemCostId != 0);
            if (Items::ItemInfo* item = Items::GetItemInfo((uint32_t)d->itemCostId))
                card.costs[0].icon.icon->SetTexture(item->GetIcon(), true);
            ShowCost(card.costs[0], d->itemCost, GameState::GetItemAmount((uint32_t)d->itemCostId, false));
            card.costs[1].root->SetVisibility(ready);
            card.costs[1].icon.icon->SetTexture(IconManager::GetIcon(GameState::GetResourceIconName(GameState::kGold), false),
                                                true);
            ShowCost(card.costs[1], (int)d->GetCostGold(false, offer), (int)GameState::GetResourceAmount(GameState::kGold));
            const int resource = GetAdjustedResourceCost((uint32_t)d->itemCostId, d->resourceCost);
            card.costs[2].root->SetVisibility(ready && d->resourceCost != 0);
            card.costs[2].icon.icon->SetTexture(
                IconManager::GetIcon(GameState::GetResourceMapIconName(d->resourceCostType), false), true);
            ShowCost(card.costs[2], resource, (int)GameState::GetResourceAmount(d->resourceCostType));
            if (GameState::GetServerSetting(0xb3) != 0) card.costs[0].root->SetVisibility(false);
        }
        card.upgrading.root->SetVisibility(upgrading);
        card.upgrading.text->SetText(SWPrintf(0x100, StringTable::GetString("HIRE_TROOPS_UPGRADING"),
                                              {building ? StringTable::GetString(building->name.c_str()) : U""})
                                         .c_str());
        // The buttons: Train (resources), Buy now (crystals), and the two small ones when both apply.
        card.train->SetVisibility(canTrain && !buyCrystal && !upgrading && g_tab != 4);
        card.trainIcon->SetTexture(IconManager::GetIcon(g_tab == 5 ? "school" : "gold_attack", false), true);
        card.trainText->SetText(StringTable::GetString(performKey));
        card.buyNow.root->SetVisibility(!canTrain && buyCrystal && !upgrading && !goblinOwned && g_tab != 4);
        card.buyNow.text->SetText(StringTable::GetString(hireKey));
        if (d->GetCostCrystals(false, offer) == d->GetCostCrystals(true, offer))
            card.buyNow.ShowPrice((unsigned)GameState::AdjustCrystalCost((int)d->GetCostCrystals(false, offer)));
        else
            card.buyNow.ShowPriceDiscounted((unsigned)GameState::AdjustCrystalCost((int)d->GetCostCrystals(false, offer)),
                                            (unsigned)GameState::AdjustCrystalCost((int)d->GetCostCrystals(true, offer)));
        const bool both = (canTrain || g_tab == 4) && buyCrystal && !upgrading;
        card.hireRes->SetVisibility(both);
        card.hireResIcon->SetTexture(
            IconManager::GetIcon(g_tab == 5 ? "school" : g_tab == 4 ? "gold_find" : "gold_attack", false), true);
        card.hireResText->SetText(StringTable::GetString(g_tab == 4 ? "PERFORM_FIND" : performKey));
        card.hireCrystal->SetVisibility(both);
        card.hireCrystalIcon->SetTexture(IconManager::GetIcon(g_tab == 5 ? "school" : "gold_attack", false), true);
        card.hireCrystalText->SetText(StringTable::GetString(hireKey));
        card.hireCrystalPrice->SetText(
            ToWideString(GameState::AdjustCrystalCost((int)d->GetCostCrystals(false, offer))));
        slot.soldierFine->SetTexture(d->side0, false);
        slot.soldierFine->SetVisibility(true);
        slot.lowerPart->SetVisibility(false);
        slot.hpIcon->SetVisibility(false);
        // The requirement line; the refresh button and the timer while the offer is not ready (the
        // tamer's task shows its counter there instead).
        if (hasBuilding && !trainable)
            card.requires->SetVisibility(!tamerTask);
        else if (canTrain)
            card.requires->SetVisibility(false);
        else
            card.requires->SetVisibility(ready && !tamerTask);
        g_buildTask[n] = 0;
        card.hang->SetVisibility(!ready && !upgrading && trainable && hasBuilding);
        card.timeout->SetVisibility(ready ? tamerTask : (trainable && hasBuilding) || tamerTask);
        card.timeoutIcon->SetTexture(tamerTask ? IconManager::GetIcon("ui_prof_tamer_60", false)
                                               : IconManager::GetSandClockIcon(),
                                     false);
        card.hangIcon->SetTexture(IconManager::GetIcon("gold_speedup", false), true);
        card.hangText->SetText(StringTable::GetString("TAVERN_REFRESH"));
        card.hangText->SetEnabled(GameState::tutorial > 0x7f);
        card.hangPrice->SetText(ToWideString(GameState::AdjustCrystalCost(GetResetCost(g_tab))));
        if (!ready) {
            card.nextIn->SetText(StringTable::GetString("TAVERN_AVAILABLE_IN"));
            card.nextTime->SetText(StringTable::GetNumericTimeString(remaining, false).c_str());
        } else if (tamerTask) {
            card.nextIn->SetText(StringTable::GetString("FACTORY_UNLOCK_TO_ORDER"));
            card.nextTime->SetText(SWPrintf(0x100, U"%d/%d",
                                            {GameState::GetSubTaskDone(unlockTask->subtasks[0].id),
                                             unlockTask->subtasks[0].amount})
                                       .c_str());
        }
        card.build->SetVisibility(false);
        // What is missing: the building (or its level, or the task that unlocks that level), the
        // player's level, the unit's task (or the first locked one in its chain).
        std::u32string text;
        const unsigned required = (unsigned)d->trainingObjCount;
        if (building && (!hasBuilding || buildingLevel < required)) {
            Append(text, StringTable::GetString("HIRE_TROOPS_BUILDING_LOCK"),
                   {StringTable::GetString(building->name.c_str())});
            if (buildingLevel < required) Append(text, U" [%d]", {(int)required + 1});
            Append(text, U"\n");
        }
        if (buildingLevel < required) {
            const uint32_t quest = UpgradeQuest(building, required);
            if (quest == 0 || GameState::TaskCompleted(quest)) {
                Append(text, StringTable::GetString("HOUSE_INFO_LEVEL"), {ToWideString((int)required + 1)});
                Append(text, U"\n");
            } else if (Tasks::TaskInfo* t = Tasks::GetTask(quest)) {
                Append(text, StringTable::GetString("HIRE_TROOPS_TASK_LOCK"), {t->name});
            } else {
                Append(text, StringTable::GetString("UPGRADE_LOCKED"));
            }
        }
        if (offer->level > (unsigned)GameState::GetLevel()) {
            if (g_tab == 5) {
                Append(text, StringTable::GetString("SPAWN_GOB_HINT"));
                Append(text, U"\n");
            }
            Append(text, StringTable::GetString("SPAWN_GOB_NEED_LEVEL"), {ToWideString((int)offer->level)});
            Append(text, U"\n");
        }
        uint32_t quest = d->unlockQuest;
        if (Tasks::TaskInfo* t = Tasks::GetTask(quest))
            if (t->IsLockedByTask()) quest = t->GetFirstLockedTaskInChain(true);
        if (quest != 0 && !GameState::TaskCompleted(quest)) {
            if (Tasks::TaskInfo* t = Tasks::GetTask(quest)) {
                // A quick task is named by its first step.
                if (t->quickTask) {
                    const std::u32string step =
                        StringTable::GetUndecoratedString(StringTable::GetStringF("STASK%d", t->subtasks[0].id), 0x100);
                    Append(text, StringTable::GetString("HIRE_TROOPS_TASK_LOCK"), {step.c_str()});
                } else {
                    Append(text, StringTable::GetString("HIRE_TROOPS_TASK_LOCK"), {t->name});
                }
            } else {
                Append(text, StringTable::GetString("UPGRADE_LOCKED"));
            }
        }
        // A ready offer that cannot be trained yet describes itself (the tamer's tab: an advert).
        bool showRequires = false;
        if (!hasBuilding || !trainable) {
            card.requires->SetText(text.c_str());
            showRequires = true;
        } else if (!canTrain && ready && !tamerTask) {
            if (g_tab == 4) {
                card.requires->SetText(StringTable::GetString("PROMO_TAMER"));
                showRequires = true;
            } else {
                card.requires->SetText(StringTable::GetStringF("%s_DESC", d->name.c_str()));
                showRequires = g_tab != 5 || goblinOwned;
            }
        } else {
            showRequires = true;
        }
        if (showRequires) {
            // A hired goblin says so in the upgrade line.
            if (goblinOwned) {
                card.upgrading.root->SetVisibility(true);
                card.upgrading.icon->SetTexture(IconManager::GetIcon("gold_confirm", false), true);
                card.upgrading.text->SetText(SWPrintf(0x100, StringTable::GetString("SOLDIERS_HIRED"),
                                                      {StringTable::GetString("STORAGE_GOBLINS_STATIC")})
                                                 .c_str());
            }
            if (!hasBuilding) {
                card.requires->SetVisibility(true);
            } else if (!trainable) {
                // The build button: the unit's task (find it on the tamer's tab), the building's
                // next level, or (two or more levels short) the level before that first.
                if (quest != 0 && !GameState::TaskCompleted(quest)) {
                    card.build->SetVisibility(!upgrading);
                    card.buildText->SetVisibility(true);
                    if (tamerTask && d->unlockQuest == quest) {
                        card.buildIcon->SetTexture(IconManager::GetIcon("gold_find", false), true);
                        card.buildText->SetText(StringTable::GetString("PERFORM_FIND"));
                    } else {
                        card.buildIcon->SetTexture(IconManager::GetIcon("gold_flag", false), true);
                        card.buildText->SetText(StringTable::GetString("TASK_BUTTON_EXEC"));
                    }
                    card.buildTextInactive->SetVisibility(false);
                    g_buildTask[n] = 0;
                    Tasks::TaskInfo* current[0x80] = {};
                    const unsigned count = Tasks::GetCurrentTasks(current, 0x80, false, true);
                    bool found = false;
                    for (unsigned i = 0; i < count && !found; ++i) found = current[i]->id == quest;
                    const bool done440 = GameState::TaskCompleted(0x440);
                    card.build->SetEnabled((found || d->id == 0x195) && !GameState::IsArenaCombatActive() && done440 &&
                                           !GameState::IsBossCombatActive());
                    if (Tasks::GetTask(quest)) g_buildTask[n] = quest;
                } else if (buildingLevel < required) {
                    if (required - buildingLevel == 1) {
                        card.build->SetVisibility(!upgrading);
                        card.buildIcon->SetTexture(IconManager::GetIcon("gold_job_builder", false), true);
                        card.buildText->SetVisibility(true);
                        card.buildText->SetText(StringTable::GetString("PERFORM_UPGRADE"));
                        card.buildTextInactive->SetVisibility(false);
                        const uint32_t level = UpgradeQuest(building, required);
                        g_buildTask[n] = 0;
                        Tasks::TaskInfo* current[0x80] = {};
                        const unsigned count = Tasks::GetCurrentTasks(current, 0x80, true, true);
                        bool found = false;
                        for (unsigned i = 0; i < count && !found; ++i) found = current[i]->id == level;
                        if (!GameState::TaskCompleted(level)) {
                            card.build->SetEnabled(found && !GameState::IsArenaCombatActive());
                            if (Tasks::TaskInfo* t = Tasks::GetTask(level)) g_buildTask[n] = t->id;
                        } else {
                            card.build->SetEnabled(!GameState::IsArenaCombatActive());
                            g_buildTask[n] = 0;
                        }
                    } else {
                        card.upgrading.root->SetVisibility(true);
                        card.upgrading.icon->SetTexture(IconManager::GetIcon("gold_build", false), true);
                        card.upgrading.text->SetText(StringTable::GetString("PERFORM_PREVIOUS_UPGRADE"));
                    }
                }
            }
        }
        // The sale label (none offline: GetDiscountSize is always 100).
        for (Shared::DiscountInfo& di : slot.discount) di.root->SetVisibility(false);
        const int discount = d->GetDiscountSize();
        const int colour = discount < 50 ? 2 : discount < 75 ? 0 : discount <= 99 ? 1 : -1;
        if (colour < 0) {
            slot.golden->SetVisibility(false);
        } else {
            slot.golden->SetVisibility(true);
            slot.discount[colour].root->SetVisibility(true);
            slot.discount[colour].text->SetText(SWPrintf(0x80, U"-%d%%", {100 - d->GetDiscountSize()}).c_str());
        }
    }
    // Fewer than three offers: the cards are centred in the strip.
    if (g_offers.size() < 3) {
        for (WarriorInfo& w : g_warriors) {
            w.root->RestorePosition();
            w.root->MoveWindow((int)(g_scale * -15.f), 0);
            w.root->MoveWindow((int)((unsigned)(g_scroller.track->w - g_scroller.itemSize * (int)g_offers.size()) >> 1), 0);
        }
        g_warriors[3].root->MoveWindow(g_warriors[1].root->x - g_warriors[0].root->x, 0);
    }
    g_scroller.itemCount = (unsigned)g_offers.size();
    g_scroller.UpdateScroll();
    // The squad: a soldier with its health bar; an empty slot shows a faded helmet (locked past the
    // player's slot count).
    g_squadHeader->SetText(StringTable::GetString("HIRE_TROOPS_SQUAD"));
    for (unsigned i = 0; i < 3; ++i) {
        Shared::WarriorSlotInfo& s = g_squad[i];
        s.bgRed->SetVisibility(false);
        s.lowerPart->SetVisibility(false);
        s.hpBar->SetVisibility(false);
        s.trainingBar->SetVisibility(false);
        s.manageSlot->SetVisibility(false);
        s.locked->SetVisibility(false);
        s.quantityActive->SetVisibility(false);
        Entity* soldier = SoldierSlots::GetSoldierBySlot(i);
        s.soldier = soldier ? soldier->GetEntityData()->id : 0;
        s.noDrop = GameState::GetSoldierSlotCount() <= i;
        if (s.soldier == 0) {
            s.wingsEmpty->SetVisibility(true);
            s.wingsEmpty->SetEnabled(i < GameState::GetSoldierSlotCount());
            s.wingsEmpty->SetAlpha(0.5f, false);
            s.soldierFine->SetVisibility(true);
            s.soldierFine->SetTexture(IconManager::GetIcon("icon_gold_helmet", false), true);
            s.lockIcon->SetVisibility(GameState::GetSoldierSlotCount() <= i);
            continue;
        }
        s.lowerPart->SetVisibility(true);
        s.wingsEmpty->SetVisibility(false);
        // Hidden while it is dragged or gliding back.
        s.soldierFine->SetVisibility(!s.returnSprite && !s.dragSprite);
        s.soldierFine->SetTexture(soldier->GetEntityData()->side0, false);
        s.soldierFine->SetEnabled(true);
        s.lockIcon->SetVisibility(false);
        s.hpBar->SetVisibility(true);
        const int hp = soldier->GetHP() > 0 ? soldier->GetHP() : 0;
        const float ratio = (float)hp / (float)soldier->GetHpMax();
        const float fill = ratio >= 1.f ? 1.f : ratio < 0.f ? 0.f : ratio;
        const std::u32string text = SWPrintf(0x100, U"%d/%d", {hp, soldier->GetHpMax()});
        s.hpText2->SetText(text.c_str());
        s.hpText->SetText(text.c_str());
        s.hpIcon->SetEnabled(soldier->GetHP() != 0);
        s.hpClip.right = (int)((float)s.hpClip.left + fill * (float)s.hpBar->w);
    }
    // The reserve: the soldiers waiting (one being trained shows its 5-second bar; one still in the
    // queue only its picture), then empty slots, the slot to buy (not during tutorial steps 0x61 /
    // 0x62) and the locked ones.
    g_reserveHeader->SetText(StringTable::GetString("HIRE_TROOPS_RESERVE"));
    const unsigned reserveSlots = GameState::GetSoldierReserveSlots();
    g_hired->SetVisibility(g_reserveList.size() < GameState::GetSoldierReserveSlots());
    g_hiredFull->SetVisibility(GameState::GetSoldierReserveSlots() == g_reserveList.size());
    g_hired->SetText(StringTable::GetString("HIRE_TROOPS_SLOTS_TAKEN"));
    g_hiredFull->SetText(StringTable::GetString("HIRE_TROOPS_SLOTS_TAKEN"));
    const std::u32string taken = SWPrintf(0x100, U"%d/%d", {(unsigned)g_reserveList.size(), reserveSlots});
    g_hiredNum->SetVisibility(g_reserveList.size() < GameState::GetSoldierReserveSlots());
    g_hiredFullNum->SetVisibility(GameState::GetSoldierReserveSlots() == g_reserveList.size());
    g_hiredNum->SetText(taken.c_str());
    g_hiredFullNum->SetText(taken.c_str());
    static const char* const kTrainingIcons[3] = {"gold_16_attack", "gold_16_attack", "gold_16_ranged_attack"};  // 0x601728
    for (unsigned i = 0; i < 5; ++i) {
        Shared::WarriorSlotInfo& s = g_reserve[i];
        Entity* soldier = nullptr;
        if (i < g_reserveList.size()) {
            soldier = g_reserveList[i].soldier;
            s.soldier = g_reserveList[i].entity;
        } else {
            s.soldier = 0;
        }
        s.noDrop = GameState::GetSoldierReserveSlots() <= i;
        if (soldier) {
            const EntityData* d = soldier->GetEntityData();
            // (A scan of the squad's slots follows here; its result is not used.)
            const int type = d->trainingObjType == 0x8e ? 1 : d->trainingObjType == 0x69 ? 2 : 0;
            const bool training = soldier->workType == 1;
            s.golden->SetVisibility(false);
            s.soldierFine->SetTexture(d->side0, false);
            s.soldierFine->SetAlpha(1.f, false);
            s.soldierFine->SetVisibility(!s.returnSprite && !s.dragSprite);
            s.lockIcon->SetVisibility(false);
            s.hpBar->SetVisibility(!training);
            s.trainingBar->SetVisibility(training);
            s.timeLeft->SetVisibility(training);
            if (!training) {
                s.hpIcon->SetEnabled(soldier->GetHP() != 0);
                const std::u32string text = SWPrintf(0x100, U"%d/%d", {soldier->GetHP(), soldier->GetHpMax()});
                s.hpText2->SetText(text.c_str());
                s.hpText->SetText(text.c_str());
                s.hpClip.right = (int)((float)s.hpClip.left + (float)s.hpFill->w *
                                                                  ((float)soldier->GetHP() / (float)soldier->GetHpMax()));
            } else {
                // Training takes five seconds from the building's timer.
                s.trainingType->SetTexture(IconManager::GetIcon(kTrainingIcons[type], false), true);
                s.wingsEmpty->SetVisibility(false);
                const GameData::BuildingData* b = GameData::GetBuilding((uint32_t)d->trainingObjType);
                uint32_t started = 0;
                Map::Building* mb = GameState::GetCurrentMapID() == 0 ? Map::GetBuildingWithID(b->id) : nullptr;
                if (mb) {
                    started = mb->IsOpened() ? mb->stateTime : 0;
                } else if (GameState::OfflineBuilding* o = GameState::GetOfflineBuildingWithID(b->id)) {
                    started = o->stateTime;
                }
                int elapsed = (int)(Timer::GetGlobalTime() - started);
                if (elapsed > 4) elapsed = 5;
                s.timeLeft->SetText(StringTable::GetTimeString(5 - elapsed, false).c_str());
                s.trainingText2->SetText(StringTable::GetString("WARRIOR_TRAINING_SMALL"));
                s.trainingText->SetText(StringTable::GetString("WARRIOR_TRAINING_SMALL"));
                s.trainingClip.right =
                    (int)((float)s.trainingClip.left + (float)s.trainingFill->w * ((float)elapsed / 5.f));
            }
            s.lowerPart->SetVisibility(true);
            s.locked->SetVisibility(false);
            s.gold->SetVisibility(false);
            s.manageSlot->SetVisibility(false);
        } else if (s.soldier != 0) {
            // Hired but not made yet.
            const EntityData* d = EntityFactory::GetEntityByID((int)s.soldier);
            s.golden->SetVisibility(false);
            s.wingsEmpty->SetVisibility(false);
            s.soldierFine->SetTexture(d->side0, false);
            s.soldierFine->SetAlpha(0.5f, false);
            s.soldierFine->SetVisibility(!s.returnSprite);
            s.lockIcon->SetVisibility(false);
            s.hpBar->SetVisibility(false);
            s.trainingBar->SetVisibility(false);
            s.timeLeft->SetVisibility(false);
            s.lowerPart->SetVisibility(true);
            s.locked->SetVisibility(false);
            s.gold->SetVisibility(false);
            s.manageSlot->SetVisibility(true);
            s.manageSlot->SetText(StringTable::GetString("HIRE_TROOPS_IN_QUEUE"));
        } else {
            s.hpBar->SetVisibility(false);
            s.trainingBar->SetVisibility(false);
            s.timeLeft->SetVisibility(false);
            s.locked->SetVisibility(false);
            s.gold->SetVisibility(false);
            s.lowerPart->SetVisibility(true);
            const char* icon;
            if (i < GameState::GetSoldierReserveSlots()) {
                s.bgSimple->SetEnabled(true);
                s.golden->SetVisibility(false);
                s.manageSlot->SetVisibility(false);
                s.locked->SetVisibility(true);
                s.locked->SetText(StringTable::GetString("HIRE_TROOPS_EMPTY"));
                s.lockIcon->SetVisibility(false);
                s.wingsEmpty->SetVisibility(true);
                s.wingsEmpty->SetEnabled(true);
                s.soldierFine->SetVisibility(true);
                icon = "icon_gold_helmet";
            } else if (GameState::GetSoldierReserveSlots() == i && GameState::tutorial != 0x61 &&
                       GameState::tutorial != 0x62) {
                s.bgSimple->SetEnabled(true);
                s.golden->SetVisibility(true);
                s.manageSlot->SetVisibility(false);
                s.gold->SetVisibility(true);
                s.gold->SetText(StringTable::GetString("HIRE_TROOPS_UNLOCK"));
                s.lockIcon->SetVisibility(false);
                s.wingsEmpty->SetVisibility(false);
                s.soldierFine->SetVisibility(true);
                s.soldierFine->SetTexture(IconManager::GetIcon("gold_add", false), true);
                s.soldierFine->SetAlpha(1.f, false);
                continue;
            } else {
                s.bgSimple->SetEnabled(false);
                s.golden->SetVisibility(false);
                s.manageSlot->SetVisibility(false);
                s.locked->SetVisibility(true);
                s.locked->SetText(StringTable::GetString("BOTTOM_BAR_LOCKED"));
                s.lockIcon->SetVisibility(true);
                s.wingsEmpty->SetVisibility(true);
                s.wingsEmpty->SetEnabled(false);
                s.soldierFine->SetVisibility(true);
                icon = "icon_gold_helmet";
            }
            s.soldierFine->SetTexture(IconManager::GetIcon(icon, false), true);
            s.soldierFine->SetAlpha(1.f, false);
        }
    }
    g_root->UpdatePosition();
}

// @0x2cee94: the strip scrolls so that card slot n shows.
void ShowSlot(unsigned n) { g_scroller.ScrollIntoView(n); }

// @0x2d913c: the strip scrolls to the tab's offer of unit `entity` (if it has one).
void ShowEntity(uint32_t entity) {
    for (unsigned i = 0; i < g_offers.size(); ++i) {
        if (g_offers[i]->entity != entity) continue;
        g_scroller.ScrollIntoView(i);
        UpdateContents();
        return;
    }
}

namespace {

// @0x2d899c: the hired soldier's icon glides from card n's slot to the squad slot of its kind; when
// it went to the reserve (or the squad has none of it), to the last reserve slot showing its icon.
// Update moves it.
void RunHireAnimation(const EntityData* d, unsigned n) {
    unsigned squad = 0xffffffff;
    for (unsigned i = 0; i < GameState::GetSoldierSlotCount() && squad == 0xffffffff; ++i) {
        if (!SoldierSlots::GetSoldierBySlot(i)) continue;
        squad = SoldierSlots::GetSoldierBySlot(i)->GetEntityData()->id == d->id ? i : 0xffffffff;
    }
    int reserve = -1;
    for (int i = 0; i < 5; ++i)
        if (g_reserve[i].soldierFine->texture == d->side0) reserve = i;
    Shared::WarriorSlotInfo* s;
    if ((squad == 0xffffffff || SoldierPool::GetFirstSoldierByID(d->id)) && reserve != -1)
        s = &g_reserve[reserve];
    else if (squad != 0xffffffff)
        s = &g_squad[squad];
    else
        return;   // PORT: the original writes to g_squad[-1] (inside the fourth card) here.
    Render::RemoveSprite(s->returnSprite);
    Render::Sprite* sp = Render::CreateSprite(d->side0, 0xf, false, false);
    s->returnSprite = sp;
    sp->screenSpace = true;
    Render::SetShaderType(sp, 1);
    s->returnTime = 0.f;
    const Render::Sprite* from = g_warriors[n].slot1.soldierFine->sprite;
    s->returnX = from->x;
    s->returnY = from->y;
    UpdateContents();
}

// Fills in a hired soldier from its offer, and the offer's next one (its time and HP).
void SetUpHired(Entity* e, Tavern::TavernInfo* offer) {
    e->SetHP((int)offer->hpMax);
    e->SetHpMax((int)offer->hpMax);
    e->SetOverrideAttack((int)offer->attack);
    e->SetOverrideDefense((int)offer->defense);
}
void NextOffer(Tavern::TavernInfo* offer) {
    GameState::SetTavernWarriorTS(offer->entity, (uint32_t)Timer::GetGlobalTime());
    GameState::SetTavernWarriorHP(offer->entity, offer->GenerateHP());
}

// @0x2d9a1c: card n's Train button (and the small Hire for resources). On the goblins' tab the
// player level comes first; with the squad and the reserve full it refuses (not for goblins). On
// the tamer's tab (4) the arrow shows where a beast of that kind is instead. Else the price is
// charged (NotEnough when short, its button trains again) and: a goblin joins the storage; the
// tavern's warrior is hired at once; on the city map the building trains the warrior
// (LaunchContract), elsewhere it joins at once.
void OnTrain(unsigned n) {
    Tavern::TavernInfo* offer = g_offers[g_scroller.firstIndex + n];
    EntityData* d = EntityFactory::GetEntityByID((int)offer->entity);
    if (g_tab == 5 && offer->level > (unsigned)GameState::GetLevel()) {
        NotEnoughWindow::ResetRequirements();
        NotEnoughWindow::AddLevelRequirement(offer->level);
        NotEnoughWindow::AddRequirement(8, d->GetCostGold(false, offer));
        if (!NotEnoughWindow::CheckRequirements()) {
            NotEnoughWindow::Show();
            return;
        }
    }
    const bool room = (GameState::GetCurrentMapID() != 0 && SoldierSlots::GotFreeSlot(offer->entity)) ||
                      SoldierPool::GotFreeSlot(offer->entity);
    if (!room && g_tab != 5) {
        PopupWindow::Show(StringTable::GetString("HIRE_TROOPS_NO_SLOTS"), PopupWindow::Hide, nullptr, nullptr);
        return;
    }
    if (g_tab == 4) {
        Entity* player = EntityManager::GetPlayer();
        if (Entity* beast = EntityManager::GetNearestEnemyWithClassTo(player, (uint32_t)d->enemyClass)) {
            float x = 0.f, y = 0.f;
            beast->GetWorldPos(x, y);
            AIBaseState* ai = player->GetAI();
            BuildingHovers::SetArrowClickCallback(new GUI::Callback([ai, beast] { ai->ClickedEntity(beast); }));
            BuildingHovers::FollowEntity(beast);
            const float h = beast->GetSprite() ? beast->GetSprite()->h : 0.f;
            Tasks::HelpWithLocation((int)x, (int)(y - h - 10.f), 0, 0);
            Hide();
            return;
        }
        const uint32_t map = Locations::GetMapWithEntityOfClass((uint32_t)d->enemyClass, false);
        if (map != 0) {
            if (map == 0x74) return;
            Hide();
            Tasks::HelpWithMapSwitchTo(map, true, false, 0);
            return;
        }
        if (Locations::GetMapWithEntityOfClass((uint32_t)d->enemyClass, true) != 0)
            PopupWindow::Show(StringTable::GetString("SERVER_ERROR"), PopupWindow::Hide, nullptr, nullptr);
        return;
    }
    const int resources = GetAdjustedResourceCost((uint32_t)d->itemCostId, d->resourceCost);
    NotEnoughWindow::ResetRequirements();
    NotEnoughWindow::AddRequirement(8, d->GetCostGold(false, offer));
    NotEnoughWindow::AddRequirement(d->resourceCostType, (unsigned)resources);
    if (GameState::GetServerSetting(0xb3) == 0)
        NotEnoughWindow::AddItemRequirement((unsigned)d->itemCostId, (unsigned)d->itemCost);
    if (!NotEnoughWindow::CheckRequirements()) {
        NotEnoughWindow::SetDescriptionText(
            SWPrintf(0x80, StringTable::GetString("REQUIREMENT_TROOPS"), {d->title}).c_str(), d->title);
        NotEnoughWindow::SetActionCallback([n] { OnTrain(n); }, StringTable::GetString("PERFORM_HIRE"), false);
        NotEnoughWindow::Show();
        return;
    }
    NotEnoughWindow::Hide();
    GameState::ChangeResourceAmount(8, -(int)d->GetCostGold(false, offer));
    GameState::ChangeResourceAmount(d->resourceCostType, -resources);
    if (d->itemCostId != 0 && GameState::GetServerSetting(0xb3) == 0)
        GameState::RemoveItem((uint32_t)d->itemCostId, d->itemCost);
    if (g_tab == 5) {
        if (Map::Building* b = Map::GetBuildingWithID(0x12)) b->HireGolbin();
        Tasks::CompleteSubtask(Tasks::kTrainWorkerInBuilding, 0x12, 1);
        UpdateContents();
        return;
    }
    if (kTabBuildings[g_tab] == 0x8f) {
        Tasks::CompleteSubtask(Tasks::kTrainWorkerInBuilding, 0x8f, 1);
        Tasks::CompleteSubtask(Tasks::kTrainAnyWorker, 1, 1);
        Entity* e = new Entity(d, 0, 0);
        SetUpHired(e, offer);
        GameState::HireSoldier(e, true, 1);
        NextOffer(offer);
        UpdateContents();
        if (GameState::GetLevel() > 4) return;
        Hide();
        return;
    }
    if (GameState::GetCurrentMapID() == 0) {
        if (Map::Building* b = Map::GetBuildingWithID(kTabBuildings[g_tab])) {
            if (b->HasActiveContract() && !b->contractDone) b->OnContractCompleted(false);
            int tx = 0, ty = 0;
            Map::GetHQ()->GetSpawnTile(tx, ty);
            Entity* e = EntityManager::SpawnEntityAt((int)d->id, (unsigned)tx, (unsigned)ty, true, true);
            e->SetActive(true, true);
            SetUpHired(e, offer);
            NextOffer(offer);
            b->LaunchContract(e);
            UpdateContents();
        }
    } else {
        Entity* e = EntityManager::CreateEntity((int)offer->entity, false, true);
        GameState::HireSoldier(e, true, 2);
        SetUpHired(e, offer);
        NextOffer(offer);
        Tasks::CompleteSubtask(Tasks::kTrainWorkerInBuilding, kTabBuildings[g_tab], 1);
        Tasks::CompleteSubtask(Tasks::kTrainAnyWorker, 1, 1);
        UpdateContents();
    }
    if (GameState::GetLevel() < 5)
        Hide();
    else
        RunHireAnimation(d, n);
    if (GameState::tutorial != 0x61) return;
    BuildingHovers::HideArrow();
    GameState::tutorial = 0x62;
    GUI::SetInteractionLock(true);
    Hide();
}

// @0x2d8b90: card n's crystal price: asks first (ConfirmPurchaseWindow), then hires at once: a
// goblin for the storage, else a soldier (to the squad or the reserve) with the hire glide.
void OnHire(unsigned n, bool confirmed) {
    Tavern::TavernInfo* offer = g_offers[g_scroller.firstIndex + n];
    EntityData* d = EntityFactory::GetEntityByID((int)offer->entity);
    const bool room = (GameState::GetCurrentMapID() != 0 && SoldierSlots::GotFreeSlot(offer->entity)) ||
                      SoldierPool::GotFreeSlot(offer->entity);
    if (!room && g_tab != 5) {
        PopupWindow::Show(StringTable::GetString("HIRE_TROOPS_NO_SLOTS"), PopupWindow::Hide, nullptr, nullptr);
        return;
    }
    const int price = GameState::AdjustCrystalCost((int)d->GetCostCrystals(false, offer));
    NotEnoughWindow::ResetRequirements();
    NotEnoughWindow::AddRequirement(9, (unsigned)price);
    if (!NotEnoughWindow::CheckRequirements()) {
        NotEnoughWindow::Show();
        return;
    }
    if (!confirmed) {
        ConfirmPurchaseWindow::SetParameters([n] { OnHire(n, true); }, (unsigned)price, true);
        ConfirmPurchaseWindow::Show();
        return;
    }
    // (FileManager::OnItemBought("warrior_%d", ...) and Billing::LogCBPurchase(0x19, ...): purchase
    // statistics, removed.) UNVERIFIED (milestone 6): SoundsManager::PlaySound("ui_buy_with_crystals").
    GameState::ChangeResourceAmount(9, -price);
    if (g_tab == 5) {
        if (Map::Building* b = Map::GetBuildingWithID(0x12)) b->HireGolbin();
        Tasks::CompleteSubtask(Tasks::kTrainWorkerInBuilding, 0x12, 1);
        UpdateContents();
        return;
    }
    Entity* e = EntityManager::CreateEntity((int)offer->entity, false, true);
    GameState::HireSoldier(e, true, 2);
    SetUpHired(e, offer);
    NextOffer(offer);
    if (kTabBuildings[g_tab] != 0) Tasks::CompleteSubtask(Tasks::kTrainWorkerInBuilding, kTabBuildings[g_tab], 1);
    Tasks::CompleteSubtask(Tasks::kTrainWorkerInBuilding, (uint32_t)d->trainingObjType, 1);
    Tasks::CompleteSubtask(Tasks::kTrainAnyWorker, 1, 1);
    UpdateContents();
    BattleBarWindow::UpdateContents();
    RunHireAnimation(d, n);
}

// One glide step (a drag let go, a hire): a quarter circle (ease-out) in 0.5 s to the slot's icon;
// false when it has landed (the sprite is gone). The reserve's fade in from half transparent.
bool StepGlide(Shared::WarriorSlotInfo& s, float dt, bool fade) {
    bool flying = true;
    Render::Sprite* sp = s.returnSprite;
    float t = s.returnTime + s.returnTime;
    if (t < 1.f) {
        t = t - 1.f;
        const float k = std::sqrt(1.f - t * t);
        const float dx = (float)(g_root->x + s.soldierFine->x) - s.returnX;
        const float dy = ((float)(g_root->y + s.soldierFine->y) + sp->h) - s.returnY;
        Render::SetPosition(sp, s.returnX + dx * k, s.returnY + dy * k, 0.01f);
        if (fade) Render::SetAlpha(s.returnSprite, 0.5f + k * 0.5f);
    } else {
        s.returnSprite = Render::RemoveSprite(sp);
        flying = false;
    }
    s.returnTime += dt;
    return flying;
}

// @0x2d91b8
void Update(float dt) {
    if (g_hover->shownHover) {
        if (g_hover->GetZ() != 0.001f) {
            g_hover->SetZ(0.001f);
            Render::SortRenderLayer(0xe, 1);
        }
        if (WindowManager::Head() != g_wnd) g_hover->Hide();
    }
    if (GameState::tutorial == 0x63) {
        GUI::SetInteractionLock(true);
        GameState::tutorial = 0x64;
    }
    if (!g_root->visibleSelf) {
        for (Shared::WarriorSlotInfo& s : g_reserve) s.returnSprite = Render::RemoveSprite(s.returnSprite);
        return;
    }
    g_scroller.Update(dt);
    g_left->SetEnabled(g_scroller.CanMoveLeft());
    g_right->SetEnabled(g_scroller.CanMoveRight());
    bool landed = false;
    for (Shared::WarriorSlotInfo& s : g_squad)
        if (s.returnSprite && !StepGlide(s, dt, false)) landed = true;
    for (Shared::WarriorSlotInfo& s : g_reserve)
        if (s.returnSprite && !StepGlide(s, dt, true)) landed = true;
    // The contents refresh once a second (the timers), and when a glide lands.
    if (Timer::GetGlobalTime() != g_lastSecond || landed) UpdateContents();
    g_lastSecond = Timer::GetGlobalTime();
}

// @0x2cdf50: after the slide in, the tutorial arrow: at card g_arrowSlot's Train (or small Hire)
// button, in step 0x61 the first card's; with ShowForBuilding's flag at the first card's build
// button.
void OnTweenIn() {
    // UNVERIFIED (milestone 6): SoundsManager::PlaySound(g_tab == 0 ? "ui_tavern_opened" : "ui_barracks_opened").
    if (GameState::tutorial == 0x61) g_arrowSlot = 0;
    if (g_arrowSlot != -1) {
        TaskInfoWindow::BlockArrowLimitChangeOnce();
        BuildingHovers::SetArrowVisibleWindowLimit(2);
        const WarriorInfo& w = g_warriors[g_arrowSlot];
        if (w.train->visibleSelf) {
            BuildingHovers::ArrowAt((float)(w.train->w + w.train->x + g_root->x),
                                    (float)(g_root->y + w.train->y + w.train->h / 2), false, true, false, false, true,
                                    false, false);
        } else if (w.hireRes->visibleSelf) {
            BuildingHovers::ArrowAt((float)(g_root->x + w.hireRes->x),
                                    (float)(g_root->y + w.hireRes->y + w.hireRes->h / 2), false, true, true, false,
                                    true, false, false);
        }
        const unsigned slot = (unsigned)g_arrowSlot;
        BuildingHovers::SetArrowClickCallback(new GUI::Callback([slot] { OnTrain(slot); }));
        g_arrowSlot = -1;
    }
    if (g_arrowOnBuild && g_warriors[0].build->visibleSelf) {
        BuildingHovers::SetArrowVisibleWindowLimit(2);
        const Window* b = g_warriors[0].build;
        BuildingHovers::ArrowAt((float)(g_root->x + b->x + b->w / 2), (float)(g_root->y + b->y), false, false, false,
                                false, true, false, false);
        BuildingHovers::SetArrowClickCallback(new GUI::Callback([] { OnBlueButton(0); }));
        g_arrowOnBuild = false;
    }
}

}  // namespace

// @0x2d8788: the soldier of the tapped squad slot goes to the reserve, else the tapped reserve
// soldier joins the squad.
void OnHintMove() {
    // UNVERIFIED (milestone 6): SoundsManager::PlaySound("ui_click").
    if (g_squadSlot != 0xffffffff) {
        SoldierMove(SoldierSlots::GetSoldierBySlot(g_squadSlot)->GetEntityData()->id);
        return;
    }
    if (g_poolSlot < g_reserveList.size()) SoldierMove(g_reserveList[g_poolSlot].entity);
}

// @0x2ce874: the healing card over soldier e's squad slot, else its reserve slot.
void ShowHealSoldier(Entity* e) {
    unsigned squad = ~0u, pool = ~0u;
    for (unsigned i = 0; i < SoldierSlots::GetTotalSlotCount() && squad == ~0u; ++i)
        if (SoldierSlots::GetSoldierBySlot(i) == e) squad = i;
    for (unsigned i = 0; i < SoldierPool::GetTotalSlotCount() && pool == ~0u; ++i)
        if (SoldierPool::GetSoldierBySlot(i) == e) pool = i;
    if (squad == ~0u && pool == ~0u) return;
    g_heal->SetEntity(e);
    g_heal->Show();
    const Window* s = squad != ~0u ? g_squad[squad].root : g_reserve[pool].root;
    g_heal->SetPosition(g_root->x + s->x + s->w / 2 - g_heal->root->w / 2, g_root->y + s->y - g_heal->root->h);
    g_heal->SetZ(0.021f);
    Render::SortRenderLayer(Render::kLayerGUI, 1);
}

// @0x2cea28: "dismiss <name>?" for the tapped reserve soldier.
void OnHintDismiss() {
    if (g_poolSlot >= g_reserveList.size()) return;
    Entity* e = g_reserveList[g_poolSlot].soldier;
    if (!e) return;
    PopupWindow::Show(
        SWPrintf(0x100, StringTable::GetString("HIRE_TROOPS_DISMISS_CONFIRM"), {e->GetEntityData()->title}).c_str(),
        OnHintDismissContinue, PopupWindow::Hide, nullptr);
}

namespace {

// @0x2d7ca8: the popup's yes: the soldier leaves for good.
void OnHintDismissContinue() {
    if (g_poolSlot >= g_reserveList.size()) return;
    Entity* e = g_reserveList[g_poolSlot].soldier;
    if (!e) return;
    SoldierPool::RemoveSoldier(e);
    EntityManager::RemoveEntity(e, true);
    UpdateContents();
}

// The hint popup over window w (from below), on top.
void ShowHoverAt(const Window* w) {
    const int x = g_root->x + w->x, y = g_root->y + w->y;
    g_hover->SetTargetRect(x, y, x + w->w, y + w->h, ItemHoverWindow::kS);
    g_hover->RecomputePosition();
    g_hover->SetZ(0.001f);
    Render::SortRenderLayer(0xe, 1);
}

// @0x2ceca4: card n's cost cell k: the item (0), gold (1) or the resource (2).
void OnItem(unsigned n, unsigned k) {
    const EntityData* d = EntityFactory::GetEntityByID((int)g_offers[g_scroller.firstIndex + n]->entity);
    if (!d) return;
    // UNVERIFIED (milestone 6): SoundsManager::PlaySound("ui_click").
    g_hover->Show();
    if (k == 0) {
        if (Items::ItemInfo* info = Items::GetItemInfo((uint32_t)d->itemCostId))
            g_hover->SetItem(nullptr, nullptr, info, false, true, nullptr, false, false, false, false);
    } else if (k == 1) {
        g_hover->SetResource(8, true);
    } else if (k == 2) {
        g_hover->SetResource(d->resourceCostType, true);
    }
    ShowHoverAt(g_warriors[n].costs[k].root);
}

// @0x2d8830: card n's "new offer now" for crystals (asks first): the tab's respawn time counts as
// passed and the next HP is drawn.
void OnRefresh(unsigned n, bool confirmed) {
    NotEnoughWindow::ResetRequirements();
    const int price = GameState::AdjustCrystalCost(GetResetCost(g_tab));
    NotEnoughWindow::AddRequirement(9, (unsigned)price);
    if (!NotEnoughWindow::CheckRequirements()) {
        NotEnoughWindow::Show();
        return;
    }
    if (!confirmed) {
        ConfirmPurchaseWindow::SetParameters([n] { OnRefresh(n, true); }, (unsigned)price, true);
        ConfirmPurchaseWindow::Show();
        return;
    }
    // (FileManager::OnItemBought and Billing::LogCBPurchase(0x1a, ...): purchase statistics,
    // removed.) UNVERIFIED (milestone 6): SoundsManager::PlaySound("ui_buy_with_crystals").
    GameState::ChangeResourceAmount(9, -price);
    Tavern::TavernInfo* offer = g_offers[g_scroller.firstIndex + n];
    GameState::SetTavernWarriorTS(offer->entity, (uint32_t)(Timer::GetGlobalTime() - g_respawn[g_tab]));
    GameState::SetTavernWarriorHP(offer->entity, offer->GenerateHP());
    UpdateContents();
}

// @0x2d7ae8: the unlock slot's "Unlock" (the hurry bubble's button) for crystals, asking first:
// one more reserve slot ("reserve_slot_cost" from "reserve_slot_start" on).
void OnHintGold(bool confirmed) {
    if (g_poolSlot < g_reserveList.size()) return;
    NotEnoughWindow::ResetRequirements();
    Setting costs("reserve_slot_cost");
    const int price =
        GameState::AdjustCrystalCost(costs.GetChild((unsigned)((int)g_poolSlot - Setting("reserve_slot_start").GetInt())).GetInt());
    NotEnoughWindow::AddRequirement(9, (unsigned)price);
    g_speedup->SetVisibility(false);
    if (!NotEnoughWindow::CheckRequirements()) {
        NotEnoughWindow::Show();
        return;
    }
    if (!confirmed) {
        ConfirmPurchaseWindow::SetParameters([] { OnHintGold(true); }, (unsigned)price, true);
        ConfirmPurchaseWindow::Show();
        return;
    }
    // (FileManager::OnItemBought("reserve_slot", ...) and Billing::LogCBPurchase(0x1b, ...): purchase
    // statistics, removed.) UNVERIFIED (milestone 6): SoundsManager::PlaySound("ui_buy_with_crystals").
    GameState::ChangeResourceAmount(9, -price);
    GameState::SetSoldierReserveSlots(GameState::GetSoldierReserveSlots() + 1);
    UpdateContents();
}

// @0x2d7d08: reserve slot n: a soldier's hint (Move / Dismiss) unless it is out working (on the
// city map: one at the training building); the first slot past the open ones shows the unlock
// bubble with its price.
void OnReserve(unsigned n) {
    Entity* e = nullptr;
    int unit = 0;
    if (n < g_reserveList.size()) {
        e = g_reserveList[n].soldier;
        unit = (int)g_reserveList[n].entity;
    }
    g_poolSlot = n;
    g_squadSlot = 0xffffffff;
    g_tappedUnit = unit;
    // (The original then calls SoldierSlots::GetFirstSoldierByID(unit) once per squad slot and drops
    // the results.)
    if (!e) {
        const unsigned open = GameState::GetSoldierReserveSlots();
        if (n >= open && open == n && n < 5) {
            // UNVERIFIED (milestone 6): SoundsManager::PlaySound("ui_click").
            g_speedup->SetVisibility(true);
            const Window* s = g_reserve[n].root;
            g_speedup->SetPosition(s->x + g_root->x + s->w / 2 - g_speedup->w / 2, g_root->y + s->y + 0xf - g_speedup->h);
            g_speedupJob->SetText(StringTable::GetString("HIRE_TROOPS_RESERVE_EXPAND"));
            g_hurryText->SetText(StringTable::GetString("HIRE_TROOPS_UNLOCK"));
            Setting costs("reserve_slot_cost");
            g_hurryPrice->SetText(ToWideString(GameState::AdjustCrystalCost(
                costs.GetChild(n - (unsigned)Setting("reserve_slot_start").GetInt()).GetInt())));
        }
        UpdateContents();
        return;
    }
    // UNVERIFIED (milestone 6): SoundsManager::PlaySound("ui_click").
    const bool city = GameState::GetCurrentMapID() == 0;
    if (e->workType == 1 && (!city || e->GetAI()->GetState() != 0x1d)) {
        UpdateContents();
        return;
    }
    if (city && e->GetAI()->GetState() == 0x1d) {
        UpdateContents();
        return;
    }
    // UNVERIFIED (milestone 6): SoundsManager::PlaySound("ui_click").
    g_hover->Show();
    g_hover->SetEntity(e, e->GetEntityData(), false, true);
    ShowHoverAt(g_reserve[n].root);
    UpdateContents();
}

// @0x2d80d0: squad slot n: its soldier's hint (Move / Dismiss); a locked slot (past the open
// ones) tells the quest that unlocks it ("squad_slot_02/03_quest_unlock").
void OnSquad(unsigned n) {
    g_tappedUnit = -1;
    g_squadSlot = n;
    g_poolSlot = 0xffffffff;
    Entity* e = SoldierSlots::GetSoldierBySlot(n);
    if (!e && n < GameState::GetSoldierSlotCount()) {
        UpdateContents();
        return;
    }
    const Tasks::TaskInfo* task =
        Tasks::GetTask((uint32_t)Setting(n == 1 ? "squad_slot_02_quest_unlock" : "squad_slot_03_quest_unlock").GetInt());
    const std::u32string lock = task ? SWPrintf(0x100, StringTable::GetString("HIRE_TROOPS_TASK_LOCK"), {task->name})
                                     : SWPrintf(0x100, StringTable::GetString("SHOP_UNLOCK_TASK"));
    if (!e) {
        // UNVERIFIED (milestone 6): SoundsManager::PlaySound("ui_click").
        g_hover->Show();
        g_hover->SetText(StringTable::GetString("WAR_SLOT_LOCKED"), lock.c_str());
    } else {
        g_tappedUnit = (int)e->GetEntityData()->id;
        // UNVERIFIED (milestone 6): SoundsManager::PlaySound("ui_click").
        g_hover->Show();
        g_hover->SetEntity(e, e->GetEntityData(), false, true);
    }
    ShowHoverAt(g_squad[n].root);
    UpdateContents();
}

}  // namespace

// @0x2da9b4: the bot's first help (Settings "bot_use_cb"): train on the first of the two front
// cards whose unit the army has fewer than two of; else a crystal refresh of card 0 or 1.
bool ExecuteFirstHelp() {
    for (unsigned n = 0; n < 2; ++n) {
        const WarriorInfo& w = g_warriors[n];
        if (!w.train->visible && !w.hireRes->visible) continue;
        const unsigned i = n + g_scroller.firstIndex;
        const Tavern::TavernInfo* offer = i < g_offers.size() ? g_offers[i] : nullptr;
        if (offer && SoldierSlots::GetSoldiersCountByID(offer->entity) + SoldierPool::GetSoldiersCountByID(offer->entity) >= 2)
            continue;
        OnTrain(n);
        Hide();
        return true;
    }
    for (unsigned n = 0; n < 2; ++n) {
        if (g_warriors[n].hang->visible && Setting("bot_use_cb").GetInt() != 0) {
            OnRefresh(n, true);
            return true;
        }
    }
    return false;
}

namespace {

// @0x2ce550: the NotEnough requirements of the tab's building's next level (as UpgradeBuilding).
bool CheckUpgradeRequirements(unsigned tab) {
    Map::Building* b = Map::GetBuildingWithID(kTabBuildings[tab]);
    if (!b) return false;
    const GameData::UpgradeInfo u = b->GetNextUpgradeInfo();
    NotEnoughWindow::ResetRequirements();
    NotEnoughWindow::SetLevelFailMessage(StringTable::GetString("UPGRADE_LEVEL_REQ"));
    const std::u32string desc = SWPrintf(0x100, StringTable::GetString("REQUIREMENT_UPGRADE"),
                                         {StringTable::GetString(b->data->name.c_str()), ToWideString(b->level + 2)});
    NotEnoughWindow::SetDescriptionText(desc.c_str(), nullptr);
    NotEnoughWindow::AddLevelRequirement((unsigned)u.level);
    NotEnoughWindow::AddPopulaionRequirement((unsigned)u.population);
    for (int i = 0; i < 11; ++i) NotEnoughWindow::AddRequirement(i, (unsigned)u.cost[i]);
    if (u.requiredId != 0)
        NotEnoughWindow::AddBuildingLevelRequirement((unsigned)u.requiredId, (unsigned)u.requiredCount);
    NotEnoughWindow::SetExchangeLimit((unsigned)u.exchangeLimit);
    return NotEnoughWindow::CheckRequirements();
}

// @0x2cefe0: NotEnough's Upgrade: the tab's building starts its next level.
void OnUpgradeContinuation() {
    Map::Building* b = Map::GetBuildingWithID(kTabBuildings[g_tab]);
    if (!b) return;
    b->Upgrade((unsigned)b->level + 1);
    Hide();
    NotEnoughWindow::Hide();
}

// The world map with the arrow at the city (map 0).
void ArrowAtCity() {
    GlobalMapWindow::Show();
    GlobalMapWindow::SetArrowVisibleWindowLimit(2);
    GlobalMapWindow::ArrowAtMap(0, false, true);
}

// @0x2cf028: the tab's building's upgrade (NotEnough with the next level's requirements), unless
// the people limit is reached; away from the city, the arrow at it on the world map.
void OnUpgrade() {
    Hide();
    if (GameState::GetCurrentMapID() != 0) {
        ArrowAtCity();
        return;
    }
    Map::Building* b = Map::GetBuildingWithID(kTabBuildings[g_tab]);
    if (!b) return;
    const GameData::UpgradeInfo& u = b->GetNextUpgradeInfo();
    if (GameState::GetPlayerWorkersCount() + Map::GetPendingWorkerCount() + u.givePopulation > GameState::GetMaxWorkerCount()) {
        PopupWindow::Show(
            SWPrintf(0x100, StringTable::GetString("PEOPLE_LIMIT"), {ToWideString(GameState::GetMaxWorkerCount())}).c_str(),
            PopupWindow::Hide, nullptr, nullptr);
        return;
    }
    CheckUpgradeRequirements(g_tab);
    NotEnoughWindow::SetActionCallback(OnUpgradeContinuation, StringTable::GetString("UPGRADE_ACTION"), false);
    NotEnoughWindow::Show();
}

// @0x2cf368: the tab's building is not built: the arrow at its construction site (the camera goes
// there), else the shop on it; away from the city, the arrow at it on the world map.
void OnBuild() {
    Hide();
    if (GameState::GetCurrentMapID() != 0) {
        ArrowAtCity();
        return;
    }
    Map::Building* b = Map::GetUnfinishedBuildingWithID(kTabBuildings[g_tab], false);
    if (!b) {
        ShopWindow::Show();
        ShopWindow::OnSelectItemID(kTabBuildings[g_tab]);
        return;
    }
    Render::CenterOn(b->baseX, (b->maxY + b->minY) * 0.5f, true, 0.4f, 0, false);
    int sx = (int)b->minX, sy = (int)b->minY;
    Map::WorldCoordinatesToScreen(sx, sy);
    BuildingHovers::SetArrowClickCallback(new GUI::Callback([b, sx, sy] { BuildingHovers::OnBuildingClick(b, sx, sy); }));
    BuildingHovers::ArrowAt(b->baseX, b->minY, false, false, false, false, false, false, false);
}

// @0x2cefb0: card n's task (g_buildTask) in TaskInfoWindow.
void OnTaskDo(unsigned n) {
    Hide();
    TaskInfoWindow::SetTask((uint32_t)g_buildTask[n], false);
    TaskInfoWindow::Show();
}

// @0x2d03f4: card n's blue button: build the unit's training building when there is none, else its
// task (the Fighter, unit 0x195: the way to map 0x52 and its camera tour), else upgrade the building
// when its level is too low.
void OnBlueButton(unsigned n) {
    const unsigned i = n + g_scroller.firstIndex;
    if (i >= g_offers.size() || !g_offers[i]) return;
    const EntityData* d = EntityFactory::GetEntityByID((int)g_offers[i]->entity);
    unsigned level = 0;
    if (const GameData::BuildingData* bd = GameData::GetBuilding((uint32_t)d->trainingObjType)) {
        bool built;
        Map::Building* b = GameState::GetCurrentMapID() == 0 ? Map::GetBuildingWithID(bd->id) : nullptr;
        if (b) {
            level = (unsigned)b->level;
            built = true;
        } else {
            const GameState::OfflineBuilding* o = GameState::GetOfflineBuildingWithID(bd->id);
            level = o ? o->level : 0;
            built = o != nullptr;
        }
        if (bd->id != 0x8f && !built) {
            OnBuild();
            return;
        }
    }
    if (g_buildTask[n] == 0) {
        if (level < (unsigned)d->trainingObjCount) OnUpgrade();
        return;
    }
    if (d->id != 0x195) {
        OnTaskDo(n);
        return;
    }
    // The windows over it close first.
    if (ArenaDifficultyWindow::IsVisible()) ArenaDifficultyWindow::Hide();
    if (PlayerCompetitionWindow::IsVisible()) PlayerCompetitionWindow::Hide();
    Hide();
    if (GameState::GetCurrentMapID() != 0x52) {
        if (!GlobalMapWindow::IsVisible()) GlobalMapWindow::Show();
        GlobalMapWindow::SetArrowVisibleWindowLimit(3);
        GlobalMapWindow::ArrowAtMap(0x52, false, true);
        return;
    }
    Map::AddCameraPoint(0x10, 0xe, 1, 1, 0, 0x56a, 0xd2, 0, 1.f, true, 0.f, 0);
}

}  // namespace

}  // namespace HireTroopsWindow
