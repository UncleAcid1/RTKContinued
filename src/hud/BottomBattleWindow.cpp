// BottomBattleWindow: the bar at the bottom of the campaign maps (Rage_bar.xml): three large round
// buttons (fury spells left and right, the centre one for running or, when a monster can be
// tamed, the net), the mana bars and the rage buttons.
// Port of BottomBattleWindow (libkingdom.so 5.11), 0x256e0c..0x25d8d8. Statics at 0x618224.
// PORTED (4h, for the opening tutorial's taming lesson and ambush): the window, the large buttons,
// the net (EnableNet, the centre button's net look, the throw and the catch), the stances of the
// arena and boss fights (OnLeft / OnRight: OnAttack, OnBlock, their look in Update, SetEnabled,
// the tutorial's arrows ShowAttackButton / ShowBlockButton / ShowMagicButton).
// PORTED (5j): the rage bar: the mana bars and their numbers, the three spell slots (tiers, locks,
// the new-spell marker, connectors), the spell that flies from a slot to its target, OnAutoCast /
// OnSpell / HelpWithSpell, the run / walk switch of the centre button, the stances of every
// fight, the tutorial's arrow at the first slot, EnableFuryGlow.
// UNVERIFIED (milestone 5): SpellMovement (5k: placing an area spell in the arenas, the large
// buttons' accept / decline, the second tutorial's step 0x9e), the spell drag in Click, the mana
// potion hints' actions (5k, 5l), the round mask on the centre button's icon, showing the bar
// on every campaign map (Map::Load).
#include "engine/SoundsManager.h"
#include "hud/HUD.h"

#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>

#include "engine/IconManager.h"
#include "engine/Render.h"
#include "engine/Timer.h"
#include "game/AIState.h"
#include "game/BuildingHovers.h"
#include "game/Combat.h"
#include "game/Entity.h"
#include "game/EntityData.h"
#include "game/EntityManager.h"
#include "game/Fog.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/Map.h"
#include "game/MetaData.h"
#include "game/MetaExpression.h"
#include "game/Professions.h"
#include "game/Rand48.h"
#include "game/Setting.h"
#include "game/SoldierSlots.h"
#include "game/SpellMovement.h"
#include "game/ParticlePresets.h"
#include "game/Spells.h"
#include "game/Squad.h"
#include "game/StringTable.h"
#include "game/Tasks.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "windows/Windows.h"

namespace BottomBattleWindow {
namespace {

using GUI::Textfield;
using GUI::Window;

// ButtonFuryLarge (0x1c bytes): a large round button. LoadFrom @0x259748, SetColor @0x256e0c.
struct ButtonFuryLarge {
    Window* root = nullptr;      // +0x00 "%s"
    Window* active = nullptr;    // +0x04 fury_button_active_state (the glow)
    Window* bg[4] = {};          // +0x08 button_bg_orange, _blue, _green_circle, _red
    Window* icon = nullptr;      // +0x18 icon_60
    void LoadFrom(Window* r, const char* name) {
        root = GUI::GetWindowTypedF<Window>(r, "%s", name);
        active = GUI::GetWindowTypedF<Window>(r, "%s.fury_button_active_state", name);
        bg[0] = GUI::GetWindowTypedF<Window>(r, "%s.button_bg_orange", name);
        bg[1] = GUI::GetWindowTypedF<Window>(r, "%s.button_bg_blue", name);
        bg[2] = GUI::GetWindowTypedF<Window>(r, "%s.button_bg_green_circle", name);
        bg[3] = GUI::GetWindowTypedF<Window>(r, "%s.button_bg_red", name);
        icon = GUI::GetWindowTypedF<Window>(r, "%s.icon_60", name);
        icon->takesZ = true;
    }
    // Only background `c` shows.
    void SetColor(int c) {
        for (int i = 0; i < 4; ++i) bg[i]->SetVisibility(c == i);
    }
};

// RageButton (0x58 bytes): a spell slot of the rage bar, "rage_bar_button_%02d". LoadFrom @0x2598e8.
// The looks of the slot: the active glow and the connector to the next slot in the spell's colour
// (orange fire, green lightning, red arcane), the bronze / red button, the spell's icon (or the lock),
// the "new spell" marker and the cost in the colour of the mana it needs. The slot's tap is its click
// area's.
struct RageButton {
    Window* root = nullptr;           // +0x00
    Window* activeMid = nullptr;      // +0x04 rage_bar_spell_active_mid
    Window* activeOrange = nullptr;   // +0x08 ..._mid_orange
    Window* activeGreen = nullptr;    // +0x0c ..._mid_green
    Window* activeRed = nullptr;      // +0x10 ..._mid_red
    Window* connectorBg = nullptr;    // +0x14 rage_bar_connector_mid_bg
    Window* connectorOrange = nullptr;   // +0x18
    Window* connectorGreen = nullptr;    // +0x1c
    Window* connectorRed = nullptr;      // +0x20
    Window* bronze = nullptr;         // +0x24 rage_bar_button_bronze
    Window* red = nullptr;            // +0x28 rage_bar_button_red
    Window* iconActive = nullptr;     // +0x2c icon_active_spell
    Window* iconLocked = nullptr;     // +0x30 icon_spell_locked ("gold_16_lock")
    Window* addedMarker = nullptr;    // +0x34 spell_added_marker ("Pickup_item_glow", hidden)
    bool f38 = true;                  // +0x38
    float f3c = 1.f;                  // +0x3c
    Window* swirlHolder = nullptr;    // +0x40 icon_swirl_holder
    Textfield* costNotEnough = nullptr;   // +0x44 spell_cost_not_enough
    Textfield* costOrange = nullptr;      // +0x48 text_orange
    Textfield* costGreen = nullptr;       // +0x4c text_green
    Textfield* costRed = nullptr;         // +0x50 text_red
    GUI::Button* clickArea = nullptr;     // +0x54
    void LoadFrom(Window* r, const char* name) {
        root = GUI::GetWindowTypedF<Window>(r, "%s", name);
        activeMid = GUI::GetWindowTypedF<Window>(r, "%s.rage_bar_spell_active_mid", name);
        activeOrange = GUI::GetWindowTypedF<Window>(r, "%s.rage_bar_spell_active_mid_orange", name);
        activeGreen = GUI::GetWindowTypedF<Window>(r, "%s.rage_bar_spell_active_mid_green", name);
        activeRed = GUI::GetWindowTypedF<Window>(r, "%s.rage_bar_spell_active_mid_red", name);
        connectorBg = GUI::GetWindowTypedF<Window>(r, "%s.rage_bar_connector_mid_bg", name);
        connectorOrange = GUI::GetWindowTypedF<Window>(r, "%s.rage_bar_connector_mid_orange", name);
        connectorGreen = GUI::GetWindowTypedF<Window>(r, "%s.rage_bar_connector_mid_green", name);
        connectorRed = GUI::GetWindowTypedF<Window>(r, "%s.rage_bar_connector_mid_red", name);
        bronze = GUI::GetWindowTypedF<Window>(r, "%s.rage_bar_button_bronze", name);
        red = GUI::GetWindowTypedF<Window>(r, "%s.rage_bar_button_red", name);
        iconActive = GUI::GetWindowTypedF<Window>(r, "%s.icon_active_spell", name);
        iconActive->takesZ = true;
        iconLocked = GUI::GetWindowTypedF<Window>(r, "%s.icon_spell_locked", name);
        iconLocked->SetTexture(IconManager::GetIcon("gold_16_lock"), true);
        addedMarker = GUI::GetWindowTypedF<Window>(r, "%s.spell_added_marker", name);
        addedMarker->SetTexture(IconManager::GetIcon("Pickup_item_glow"), true);
        addedMarker->MoveWindow(-12, -10);
        addedMarker->SetVisibility(false);
        f3c = 1.f;
        f38 = true;
        swirlHolder = GUI::GetWindowTypedF<Window>(r, "%s.icon_swirl_holder", name);
        swirlHolder->MoveWindow(4, 2);
        costNotEnough = GUI::GetWindowTypedF<Textfield>(r, "%s.spell_cost_not_enough", name);
        costOrange = GUI::GetWindowTypedF<Textfield>(r, "%s.text_orange", name);
        costGreen = GUI::GetWindowTypedF<Textfield>(r, "%s.text_green", name);
        costRed = GUI::GetWindowTypedF<Textfield>(r, "%s.text_red", name);
        clickArea = GUI::GetWindowTypedF<GUI::Button>(r, "%s.clickArea", name);
    }
};

// ManaProgress (0x1c bytes): one mana bar of the holder (orange, green, red). Its fill is a bar
// image and its number clipped at the same right edge (clip.right: the bar's width times the share
// of mana, set while the bar is updated). LoadFrom @0x259824.
struct ManaProgress {
    Window* root = nullptr;      // +0x00 the bar's holder
    Window* bar = nullptr;       // +0x04 its background image, the fill
    Textfield* text = nullptr;   // +0x08 the number on it
    GUI::ClipRect clip = {};     // +0x0c left, top, right, bottom (in the root's pixels)
    void LoadFrom(Window* r, const char* holder, const char* barName, const char* textName) {
        root = GUI::GetWindowTypedF<Window>(r, "%s", holder);
        bar = GUI::GetWindowTypedF<Window>(r, "%s.%s", holder, barName);
        text = GUI::GetWindowTypedF<Textfield>(r, "%s.%s", holder, textName);
        clip.bottom = 0x400;
        clip.top = 0;
        clip.left = bar->x;
        clip.right = bar->x + bar->w;
        bar->SetClipRect(&clip);
        text->SetClipRect(&clip);
    }
};

WindowManager::FunctionalWindow* g_queue = nullptr;   // BottomBattleWindow::wnd
Window* g_root = nullptr;              // 0x618224 Rage_bar.xml
ButtonFuryLarge g_left;                // 0x618228 button_fury_large_01 (attack)
ButtonFuryLarge g_right;               // 0x618244 button_fury_large_02 (block)
RageButton g_rage[3];                  // 0x618260 (FUN_002571b4: the statics' constructors, @0x258f9c above)
ManaProgress g_mana[3];                // 0x61839c orange (fire), green (lightning), red (arcane)
Window* g_background = nullptr;        // 0x61xxxx rage_bar_bg (its tap: OnManaBar)
Textfield* g_holderText = nullptr;     // 0x618450 rage_bar_bar_holder.text_02
bool g_furyGlow = false;               // 0x618370 EnableFuryGlow: the rage buttons glow
float g_furyGlowStart = 0.f;           // 0x618374 g_pulse when it was enabled
ButtonFuryLarge g_centre;              // 0x618380 button_fury_large_03
float g_pulse = 0.f;                   // 0x618378 the glows' clock
uint32_t g_netTarget = 0;              // 0x61836c the spawn point of the monster the net may catch
bool g_tameNext = false;               // 0x61837c (debug: TameNext throws at once)
uint32_t g_netTask = 0;                // 0x6183f0 a task the throw completes
int g_netArrowCallback = 0;            // 0x6183f4
Render::Sprite* g_net = nullptr;       // 0x618414 the thrown net
float g_netT = 0.f;                    // 0x618418 its flight, 0..1
float g_netFromX = 0.f, g_netFromY = 0.f;   // 0x61841c 0x618420
float g_netToX = 0.f, g_netToY = 0.f;       // 0x618424 0x618428
float g_netTimer = 0.f;                // 0x61842c how long the dying monster can still be caught
Textfield* g_stance = nullptr;         // 0x618458 text_stance
GUI::MovementEffect* g_effect = nullptr;   // 0x618430
uint32_t g_slotStamp = 0;              // 0x618368 the game time the rage slots were last refreshed (0: now)
bool g_manaInit = false;               // 0x618444 g_manaShown has a first value
float g_manaShown = 0.f;               // 0x618448 the mana the bars show: eases to the hero's
float g_manaTimer = 0.f;               // 0x61844c
float g_spin = 0.f;                    // 0x61845c the swirls' angle, 0..pi
int g_lastMana = 0;                    // 0x60f114 the mana the slots were last refreshed for
bool g_placing = false;                // 0x618434 never set in 5.11 (SpellMovement keeps its own flag)
int g_dragX = 0;                       // 0x618438 where a press on a rage slot began
int g_dragY = 0;                       // 0x61843c
bool g_dragging = false;               // 0x618440 a press on a rage slot is held
int g_offsetY = 0x17;                  // 0x60f0fc
unsigned g_selected = (unsigned)-1;    // 0x60f100 the stance: 0 attack, 1 block, 3 auto, -1 none
bool g_enabled = false;                // 0x60f104 SetEnabled's last state
int g_offsetX = 0;                     // 0x60f10c
int g_spell = -1;                      // 0x60f110 (the spells': milestone 5)
int g_castSlot = 0;                    // 0x60f108 the rage button cast last: 1..3
// The spell flying from its rage button to the target (OnAutoCast starts it, Update 5j-3c moves it).
int g_flyX = 0, g_flyY = 0;            // 0x6183f8 0x6183fc where the marker starts: over the button
Render::Sprite* g_flySprite = nullptr; // 0x618400 the spell's marker image
bool g_flying = false;                 // 0x618404
float g_flyT = 0.f;                    // 0x618408 its flight, 0..1
int g_flyToX = 0, g_flyToY = 0;        // 0x61840c 0x618410 the target's place
bool g_tablet = false;                 // 0x60f118
float g_scale = 1.f;                   // 0x60f11c

void OnTweenIn() {}                                    // @0x256e50
void OnTweenOut() { g_root->SetVisibility(false); }    // @0x256e54
void Move(int, int) {}                                 // @0x256e4c

// The stance glow's alpha.
float Pulse(float speed) { return 0.5f + std::cos(g_pulse * speed) * 0.6f; }

// The stance line: "STANCE_PRE" with the stance's name.
void SetStanceText(const char* stance) {
    g_stance->SetText(SWPrintf(0x100, StringTable::GetString("STANCE_PRE"), {StringTable::GetString(stance)}).c_str());
}

float FromBits(uint32_t bits) {
    float f;
    std::memcpy(&f, &bits, sizeof f);
    return f;
}

// A spell is being aimed (SpellMovement, in the arenas): the large buttons confirm and cancel.
bool SpellAiming() { return SpellMovement::Activated(); }

// The large buttons' look before a fight object exists (a boss's, GameState::IsBossCombatActive):
// attack on the left, block on the right, both glowing until one is chosen.
void UpdateFirstLook() {
    g_left.root->SetVisibility(true);
    g_right.root->SetVisibility(true);
    g_centre.root->SetVisibility(false);
    g_left.root->SetEnabled(true);
    for (Window* b : g_left.bg) b->SetEnabled(true);
    g_left.icon->SetEnabled(true);
    g_right.root->SetEnabled(true);
    for (Window* b : g_right.bg) b->SetEnabled(true);
    g_right.icon->SetEnabled(true);
    g_left.SetColor(1);
    g_left.icon->SetTexture(IconManager::GetIcon("gold_attack"), true);
    g_left.active->SetVisibility(g_selected == 0);
    g_left.active->SetAlpha(Pulse(8.f), false);
    g_right.SetColor(0);
    g_right.icon->SetTexture(IconManager::GetIcon("gold_shield"), true);
    g_right.active->SetVisibility(g_selected == 1);
    g_right.active->SetAlpha(Pulse(8.f), false);
    if (g_selected == (unsigned)-1) {
        g_left.active->SetVisibility(true);
        g_left.active->SetAlpha(Pulse(8.f), false);
        g_right.active->SetVisibility(true);
        g_right.active->SetAlpha(Pulse(8.f), false);
        g_stance->SetText(StringTable::GetString("STANCE_NONE"));
    } else if (g_selected < 2) {
        SetStanceText(g_selected == 1 ? "STANCE_BLOCK" : "STANCE_ATTACK");
    }
    for (RageButton& r : g_rage) r.root->SetEnabled(true);
}

// The large buttons' look in an arena fight: attack on the left, block on the right, the chosen
// one glowing. While a spell is being placed they are its confirm and cancel.
void UpdateFightLook() {
    g_left.root->SetVisibility(true);
    g_right.root->SetVisibility(true);
    g_centre.root->SetVisibility(false);
    g_right.root->SetEnabled(true);
    if (!SpellAiming()) {
        g_left.SetColor(1);
        g_left.icon->SetTexture(IconManager::GetIcon("gold_attack"), true);
        g_left.active->SetVisibility(g_selected == 0);
        g_left.active->SetAlpha(Pulse(4.f), false);
        g_right.SetColor(0);
        g_right.icon->SetTexture(IconManager::GetIcon("gold_shield"), true);
        g_right.active->SetVisibility(g_selected == 1);
        g_right.active->SetAlpha(Pulse(4.f), false);
    } else {
        g_left.SetColor(2);
        g_left.icon->SetTexture(IconManager::GetIcon("gold_confirm"), true);
        g_left.active->SetVisibility(false);
        g_right.SetColor(3);
        g_right.icon->SetTexture(IconManager::GetIcon("gold_cancel"), true);
        g_right.active->SetVisibility(false);
    }
    if (SpellAiming()) g_stance->SetText(StringTable::GetString("cast_spell"));
    else if (g_selected == (unsigned)-1) g_stance->SetText(StringTable::GetString("STANCE_NONE"));
    else if (g_selected == 0) SetStanceText("STANCE_ATTACK");
    else if (g_selected == 1) SetStanceText("STANCE_BLOCK");
    else if (g_selected == 3) SetStanceText("ARENA_AUTO");
}

// The tutorial task a stance button completes (ShowAttackButton / ShowBlockButton): the hero's
// hint and the arrow go.
void CompleteButtonTask() {
    if (g_netTask == 0) return;
    Tasks::CompleteTask(g_netTask, true, true, false);
    if (PlayerTopWindow::IsPlayerDialogVisible()) PlayerTopWindow::HideDialog();
    GUI::SetInteractionLock(false);
    if (BuildingHovers::ArrowVisible() && BuildingHovers::GetArrowClickCallbackID() == g_netArrowCallback)
        BuildingHovers::HideArrow();
}

// The arenas' stance: the opponent's hero is the target and the turn goes on in mode `mode`
// (0 attack, 1 block, 4 auto).
void ChooseStance(int mode) {
    if (!GameState::IsCombatActive()) return;
    BaseSquad* enemy = GameState::GetActiveCombat()->GetEnemySquad();
    if (!enemy) return;
    enemy->SetActiveSoldier(nullptr);
    GameState::GetActiveCombat()->Continue(mode);
}

// @0x2585c8
void OnAuto() {
    CompleteButtonTask();
    ChooseStance(4);
}

// @0x25869c
void OnBlock() {
    CompleteButtonTask();
    ChooseStance(1);
}

// @0x258770: in the tutorial's ambush (until task 0x773) the target is the rival's hero (class 0x2b).
void OnAttack() {
    CompleteButtonTask();
    if (GameState::TaskCompleted(0x773)) {
        if (GameState::IsCombatActive() && GameState::GetActiveCombat()->GetEnemySquad()) {
            ChooseStance(0);
            return;
        }
        BuildingHovers::HideArrow();
        return;
    }
    GameState::GetActiveCombat()->GetEnemySquad()->SetActiveSoldier(EntityManager::GetEntityByClass(0x2b, true, false));
    GameState::GetActiveCombat()->Continue(0);
}

// @0x259164: block (the arenas) or the boss fight's block stance.
void OnRight() {
    if (GameState::tutorial == 0x17) TutorialSender::AddTutorialStep(0x720);
    if (!GameState::IsArenaCombatActive()) {
        if (GameState::IsBossCombatActive()) {
            SetSelectedButton(1);
            GameState::GetActiveCombat()->Continue(1);
        }
    } else {
        if (!SpellMovement::Activated()) OnBlock();
        else SpellMovement::Decline();
    }
    SoundsManager::PlaySound("ui_click", 1, false);
}

// @0x259208: attack (the arenas) or the boss fight's attack stance. At second-tutorial step 0x9e
// the ambush's task is done and the tutorial goes on (step 0x81, tutorial 0x17).
void OnLeft() {
    if (GameState::tutorial == 0x17) TutorialSender::AddTutorialStep(0x6bc);
    if (GameState::secondTutorial == 0x9e) {
        GameState::CompleteTask(0x773, false);
        GUI::SetInteractionLock(false);
        BuildingHovers::HideArrow();
        GameState::secondTutorial = 0x81;
        GameState::tutorial = 0x17;
    }
    if (!GameState::IsArenaCombatActive()) {
        if (GameState::IsBossCombatActive()) {
            SetSelectedButton(0);
            GameState::GetActiveCombat()->Continue(0);
        }
        // (with a CombatManager, GameState::g_Combat, an aimed spell is accepted: the siege system, dead
        // in 5.11)
    } else if (!SpellMovement::Activated()) {
        OnAttack();
    } else {
        BuildingHovers::HideArrow();
        SpellMovement::Accept();
    }
    SoundsManager::PlaySound("ui_click", 1, false);
}

// The tutorial's arrow at a button: its callback, the interaction lock on `lock`.
void PointAt(int task, void (*cb)(), Window* lock, bool locked, bool arrow, float x, float y) {
    g_netTask = (uint32_t)task;
    if (arrow && BuildingHovers::GetArrowClickCallbackID() != g_netArrowCallback)
        g_netArrowCallback = BuildingHovers::SetArrowClickCallback(new GUI::Callback(cb));
    if (locked) GUI::SetInteractionObjectLock(lock, nullptr);
    if (!arrow) return;
    BuildingHovers::ArrowAt(x, y, false, false, false, false, true, false, false);
    BuildingHovers::PreventArrowAutoHide();
}

// The net lands: in the taming lesson always a catch, else by the net's chance (meta 0x25)
// times the tamed form's success rate, out of 10000.
void NetLanded() {
    g_net = Render::RemoveSprite(g_net);
    const Items::ItemInfo* net = Items::GetItemInfo(Items::GetPlayersBestNet());
    const MetaData* chance = net && net->meta ? net->meta->FindChildData(0x25) : nullptr;
    int odds = chance ? chance->GetInt() : 0;
    Entity* e = EntityManager::GetEntityBySpawnPoint(g_netTarget);
    EntityData* tamed = nullptr;
    if (!e) {
        odds = 0;
    } else {
        tamed = EntityFactory::GetTamedEntityForID(e->GetEntityData()->id);
        odds = tamed ? tamed->successRate * odds : 0;
    }
    long roll = Rand48::lrand48();
    if (GameState::tutorial == 0x17 || GameState::IsTameTutorial() || roll % 10000 < odds) {
        if (!e) return;
        g_netTarget = 0;
        e->Disappear();
        e->SetAlpha(0.f);
        if (uint32_t n = Items::GetPlayersBestNet()) GameState::RemoveItem(n, 1);
        Tasks::CompleteSubtask(Tasks::kTameEnemyClass, (uint32_t)e->GetEntityData()->enemyClass, 1);
        // PORT (user's bug report 2026-10-26): a tamed beast leaves the map as a killed one does, so
        // taming the last enemy also clears the map (Entity::OnDeath's "kill all" step; the
        // original asks only on a death, and a raid ended by a catch was never completed).
        bool enemyLeft = false;
        for (unsigned k = 0; Entity* o = EntityManager::EnumEntities(k); ++k)
            if (o != e && o->IsActive() && !o->IsDead() && o->IsEnemy()) enemyLeft = true;
        if (!enemyLeft) Tasks::CompleteSubtask(Tasks::kKillAllOnMap, GameState::GetCurrentMapID(), 1);
        if (tamed) {
            if (SoldierPool::GetOccupiedSlotCount() < GameState::GetSoldierReserveSlots()) {
                Entity* s = EntityManager::CreateEntity((int)tamed->id, false, true);
                GameState::HireSoldier(s, true, 2);
                s->SetPos(e->tileX, e->tileY);
                s->GetAI()->UpdateLastTarget();
                s->SetHP(s->GetHpMax() / 2);
            } else {
                // The reserve is full: the item that holds this animal (meta 0x27) drops instead.
                const Items::ItemInfo* holder = nullptr;
                for (unsigned i = 0; const Items::ItemInfo* it = Items::EnumItemInfo(i); ++i) {
                    const MetaData* d = it->meta ? it->meta->FindChildData(0x27) : nullptr;
                    if (d && (uint32_t)d->GetInt() == tamed->id) {
                        holder = it;
                        break;
                    }
                }
                if (holder) BuildingHovers::DropItem(e->worldX, e->worldY, const_cast<Items::ItemInfo*>(holder), 0, 1);
                else std::fprintf(stderr, "ERROR: BottomBattleWindow::Update() no item holds tamed entity %u\n", tamed->id);
            }
            BattleBarWindow::UpdateContents();
        }
        if (GameState::IsTameTutorial()) {
            // The lesson is over: the arrow at the way out, the portal on (10, 0x12).
            BuildingHovers::HideWorldDialog(nullptr);
            BuildingHovers::HideArrow();
            BattleBarWindow::UpdateContents();
            if (Map::Decor* d = Map::GetDecoration(10, 0x12)) {
                AIBaseState* ai = EntityManager::GetPlayer()->GetAI();
                Portal* portal = d->portal;
                BuildingHovers::SetArrowClickCallback(new GUI::Callback([ai, portal] { ai->ClickedPortal(portal); }));
                Render::Sprite* sp = d->sprite;
                BuildingHovers::ArrowAt(sp->x + sp->w * 0.5f, sp->y - sp->h, false, false, false, false, false, false, false);
                if (GameState::secondTutorial == 0x100) GUI::SetInteractionLock(false);
                else GUI::SetInteractionObjectLock(d->portal, d);
                Fog::Open(sp->x + sp->w * 0.5f, sp->y - sp->h, 200.f, 0.3f);
            }
            Hide();
            Tasks::CompleteTask(0x771, true, true, false);
        }
    } else {
        if (e)
            BuildingHovers::ShowTextHoverWithStyle(e->worldX, e->worldY, StringTable::GetString("effect_text_resist"), 1, 2.f,
                                                   50.f, false, false);
        if (uint32_t n = Items::GetPlayersBestNet()) GameState::RemoveItem(n, 1);
        g_netTarget = 0;
    }
}

void FlyNet(float dt);

// The spell marker's flight from its rage button to the target (OnAutoCast started it): a quarter
// circle in half a second, then the spell is cast (OnSpell). It waits while a popup alone is open
// and is dropped when any other window opens.
void UpdateSpellFlight(float dt) {
    int shown = WindowManager::GetShownWindowCount();
    bool popupOnly = shown == 1 && PopupWindow::IsVisible();
    if (shown != 0 && !popupOnly) {
        g_dragging = false;
        g_placing = false;
        g_flySprite = Render::RemoveSprite(g_flySprite);
        g_flying = false;
        return;
    }
    if (!g_flying || !g_flySprite) return;
    if (!popupOnly) g_flyT += dt;
    float t = g_flyT + g_flyT;
    int x = g_flyToX, y = g_flyToY;
    if (t < 1.f) {
        Map::WorldCoordinatesToScreen(x, y);
        float u = t - 1.f;
        float ease = std::sqrt(1.f - u * u);
        Render::SetPosition(g_flySprite, (float)g_flyX + ((float)x - (float)g_flyX) * ease + g_flySprite->w * -0.5f,
                            (float)g_flyY + ((float)y - (float)g_flyY) * ease + g_flySprite->h * 0.5f,
                            g_flySprite->z);
        return;
    }
    g_flySprite = Render::RemoveSprite(g_flySprite);
    g_flying = false;
    Map::WorldCoordinatesToScreen(x, y);
    OnSpell((unsigned)g_castSlot, x, y, false);
}

// The mana the bars show: the hero's, eased (a step of 13% every 1/30 s; the tutorial's rise is
// slower). Fills the three bars (a bar holds 300) and the numbers. Returns the mana shown.
int UpdateManaBars(float dt, Entity* player) {
    float step = GameState::secondTutorial == 0x9a ? dt * 0.75f : dt;
    int ap = player->GetAP();
    if (!g_manaInit) g_manaShown = (float)ap;
    g_manaTimer = step + g_manaTimer;
    g_manaInit = true;
    if (g_manaTimer > FromBits(0x3d088889)) {
        g_manaTimer = 0.f;
        g_manaShown = g_manaShown - ((float)ap - g_manaShown) * FromBits(0xbe0930a4);
    }
    int mana;
    float share;
    bool full;
    if (GameState::secondTutorial == 0x98 || GameState::secondTutorial == 0x99) {
        g_manaShown = 62.f;   // (the tutorial shows a fixed 62)
        mana = 62;
        share = FromBits(0x3e53a06d);
        full = false;
    } else {
        mana = (int)(g_manaShown + 0.5f);
        share = (float)mana / 300.f;
        full = mana > 99;
    }
    if (ap != 0 || g_pulse - g_furyGlowStart > 2.f) g_furyGlow = false;
    if (GameState::secondTutorial == 0x9a && full) GameState::secondTutorial = 0x9d;
    std::u32string stats = SWPrintf(0x100, StringTable::GetString("MANA_BAR_STATS"),
                                    {ToWideString(mana), ToWideString(player->GetApMax())});
    g_holderText->SetText(stats.c_str());
    for (ManaProgress& m : g_mana) {
        m.text->SetText(stats.c_str());
        float filled = share <= 1.f ? share : 1.f;
        m.clip.right = (int)((float)m.clip.left + (float)m.bar->w * (filled * 0.782f + 0.088f));
        share -= 1.f;
    }
    return mana;
}

// The fury a search job the hero works costs to finish now (search_to_fury_mult per second left),
// with its profession; 0 when there is none, it is free or the hero has too little fury. (Inlined
// in Update and OnCenter.)
int SearchFuryCost(Entity* player, Professions::Profession*& prof) {
    prof = nullptr;
    Map::Decor* d = player->GetWorkplaceDecoration();
    if (!d) return 0;
    Professions::Profession* p = Professions::GetProfessionForObject(d->id);
    const uint32_t end = d->f50 + d->collectStart + 1;
    const int cost = (int)((float)(end - (uint32_t)Timer::GetGlobalTime()) * Setting("search_to_fury_mult").GetFloat());
    if (cost <= 0 || cost > player->GetAP() || !p) return 0;
    prof = p;
    return cost;
}

// The big centre button outside fights: the net while there is a target and a net, else hurrying
// the hero's search job with fury, else the run / walk switch.
void UpdateCentre(Entity* player) {
    g_left.root->SetVisibility(false);
    g_right.root->SetVisibility(false);
    g_centre.root->SetVisibility(true);
    g_centre.active->SetVisibility(false);
    uint32_t net = 0;
    bool offer = false;
    Professions::Profession* fury = nullptr;
    int furyCost = 0;
    if (g_netTarget == 0) furyCost = SearchFuryCost(player, fury);
    if (g_netTarget != 0) {
        net = Items::GetPlayersBestNet();
        if (!net) {
            g_netTimer = 0.f;
        } else {
            offer = true;
            if (!g_net && !(g_netTimer < 0.f)) {
                Entity* e = EntityManager::GetEntityBySpawnPoint(g_netTarget);
                if (e && !EntityFactory::GetTamedEntityForID(e->GetEntityData()->id)) g_netTimer = 0.f;
            }
        }
    }
    if (offer && !g_net) {
        g_centre.SetColor(1);
        g_centre.active->SetVisibility(true);
        g_centre.active->SetAlpha(Pulse(8.f), false);
        if (Items::ItemInfo* info = Items::GetItemInfo(net)) {
            g_centre.icon->SetTexture(info->GetIcon(), true);
            // UNVERIFIED (milestone 5, the mask shader): the icon is masked round
            // (button_large_round_mask.png, GUI::Window::ApplyMask).
        }
        g_stance->SetText(StringTable::GetString("TAME_HINT"));
    } else if (fury) {
        // The profession's best tool as the icon, "boost for <cost>".
        g_centre.SetColor(1);
        g_centre.active->SetVisibility(true);
        g_centre.active->SetAlpha(Pulse(8.f), false);
        if (Items::ItemInfo* info = Items::GetItemInfo(Professions::GetProfessionTool(fury->id, false, nullptr, true))) {
            g_centre.icon->SetTexture(info->GetIcon(), true);
            // UNVERIFIED (milestone 5, the mask shader): masked round (button_large_round_mask.png).
        }
        g_stance->SetText(SWPrintf(0x100, U"%s %s %s",
                                   {StringTable::GetString("BUILDING_BOOST"),
                                    StringTable::GetString("HEAL_SCREEN_FOR"), ToWideString(furyCost)})
                              .c_str());
    } else {
        // Running: the icon offers walking and the line says "run".
        bool running = player->IsSpeedRunning();
        g_centre.SetColor(0);
        g_centre.active->SetVisibility(running);
        g_centre.active->SetAlpha(Pulse(8.f), false);
        g_centre.icon->SetTexture(IconManager::GetIcon(running ? "gold_walk" : "gold_run"), true);
        g_stance->SetText(SWPrintf(0x100, StringTable::GetString("STANCE_MODE"),
                                   {StringTable::GetString(running ? "player_spell_run" : "player_spell_walk")})
                              .c_str());
    }
    for (RageButton& r : g_rage) r.root->SetEnabled(true);
}

// The rage slots: each shows its spell (the dearest the hero can pay) with the colour of its cost
// tier (orange 1..300, green 301..600, red 601..900), the "new spell" marker, the lock with what
// unlocks it and the connector to the next slot. Refreshed when the mana or the second changes.
void UpdateSlots(float dt, Entity* player, int mana) {
    g_spin += dt;
    if ((double)g_spin > 3.141592653589793) g_spin -= FromBits(0x40490fdb);
    uint32_t now = (uint32_t)Timer::GetGlobalTime();
    auto tier = [](const Spells::SpellInfo* s, uint32_t first) { return s->cost - first < 300u; };
    for (unsigned i = 0; i < 3; ++i) {
        RageButton& r = g_rage[i];
        unsigned slot = i + 1;
        r.swirlHolder->SetRotation(g_spin * -6.f);
        if (r.addedMarker->visibleSelf) {
            r.addedMarker->SetRotation(g_spin * -2.f);
            float t = r.f3c;
            if (t < 0.3f) r.addedMarker->SetAlpha(t / 0.3f, false);
            else if (t < 0.7f) r.addedMarker->SetAlpha(1.f, false);
            else if (t < 1.f) r.addedMarker->SetAlpha((t - 0.7f) / -0.3f + 1.f, false);
            else r.addedMarker->SetVisibility(false);
            r.f3c += dt;
        }
        if (g_lastMana == mana && g_slotStamp == now) continue;
        // A new spell comes within reach: the marker flashes.
        if (!Spells::GetBestSpellForSlot(slot, (uint32_t)mana, true)) {
            r.swirlHolder->SetVisibility(false);
            r.f38 = false;
        } else {
            r.swirlHolder->SetVisibility(true);
            if (!r.f38) {
                r.f38 = true;
                r.f3c = 0.f;
                r.addedMarker->SetVisibility(true);
                r.addedMarker->SetAlpha(0.f, false);
            }
        }
        Spells::SpellInfo* castable = Spells::GetBestSpellForSlot(slot, (uint32_t)player->GetAP(), true);
        if (!castable) {
            // Nothing to cast: the inactive look, the locked spell's icon and cost.
            r.activeMid->SetVisibility(true);
            r.activeOrange->SetVisibility(false);
            r.activeGreen->SetVisibility(false);
            r.activeRed->SetVisibility(false);
            r.red->SetEnabled(false);
            if (Spells::SpellInfo* shown = Spells::GetBestSpellForSlot(slot, 300, false)) {
                r.iconActive->SetTexture(shown->icon, false);
                r.costNotEnough->SetText(ToWideString((int)shown->cost));
                r.iconLocked->SetVisibility(shown->level > (uint32_t)GameState::GetLevel() ||
                                            (shown->unlockQuest != 0 && !GameState::TaskCompleted(shown->unlockQuest)));
            } else {
                r.iconLocked->SetVisibility(true);
            }
            r.costNotEnough->SetVisibility(true);
            r.costOrange->SetVisibility(false);
            r.costGreen->SetVisibility(false);
            r.costRed->SetVisibility(false);
        } else {
            r.activeMid->SetVisibility(false);
            r.activeOrange->SetVisibility(tier(castable, 1));
            r.activeGreen->SetVisibility(tier(castable, 301));
            r.activeRed->SetVisibility(tier(castable, 601));
            r.red->SetEnabled(true);
            r.iconActive->SetTexture(castable->icon, false);
            r.iconLocked->SetVisibility(false);
            r.costNotEnough->SetVisibility(false);
            Textfield* costs[3] = {r.costOrange, r.costGreen, r.costRed};
            const uint32_t firsts[3] = {1, 301, 601};
            for (int k = 0; k < 3; ++k) {
                costs[k]->SetVisibility(tier(castable, firsts[k]));
                if (costs[k]->visibleSelf) costs[k]->SetText(ToWideString((int)castable->cost));
            }
        }
        // The connector to the next slot: in the colour of the dearest spell the hero could pay
        // with more mana, plain when there is none.
        Spells::SpellInfo* connect = Spells::GetBestSpellForSlot(slot, (uint32_t)player->GetAP(), false);
        r.connectorBg->SetVisibility(connect == nullptr);
        r.connectorOrange->SetVisibility(connect && tier(connect, 1));
        r.connectorGreen->SetVisibility(connect && tier(connect, 301));
        r.connectorRed->SetVisibility(connect && tier(connect, 601));
        r.bronze->SetVisibility(false);
    }
}

// @0x259c70 (its middle and tail, the stance look and the slots, are _INIT_63 @0x25c784), the whole of it: the spell in flight, the mana bars and the glow, the stance / run /
// net buttons, the flying net, the rage slots and the tutorial's arrow at the first one.
void Update(float dt) {
    if (!g_root->visibleSelf) return;
    UpdateSpellFlight(dt);
    Entity* player = EntityManager::GetPlayer();
    int mana = UpdateManaBars(dt, player);
    g_pulse += dt;
    // The bar's glow: while the fury is ready it pulses, else it settles back to normal.
    if (g_furyGlow) {
        g_background->SetAlpha(1.2f + std::cos(g_pulse * 8.f) * 0.2f, false);
    } else {
        float a = g_background->alpha;
        if (a != 1.f) {
            if (a < 1.f) {
                a += dt;
                if (a > 1.f) a = 1.f;
            } else {
                a -= dt;
                if (!(a >= 1.f)) a = 1.f;
            }
        }
        g_background->SetAlpha(a, false);
    }
    // The catch window closes; in the taming lesson it stays open.
    if (GameState::tutorial == 0x17 || GameState::IsTameTutorial()) {
        if (g_netTarget == 0x1f) g_netTarget = 0;
    } else {
        g_netTimer -= dt;
    }
    if (g_netTimer < 0.f && !g_net) g_netTarget = 0;
    // The stance buttons in the arenas and the boss fights (and in a siege, GameState::g_Combat: dead
    // in 5.11); in campaign fights the centre button stays, the net's in the taming lesson.
    bool arena = GameState::IsArenaCombatActive();
    if (arena || GameState::IsBossCombatActive()) {
        if (!arena) UpdateFirstLook();
        else UpdateFightLook();
    } else {
        UpdateCentre(player);
    }
    FlyNet(dt);
    UpdateSlots(dt, player, mana);
    // The second tutorial's arrow at the first slot.
    if (GameState::secondTutorial == 0x9d && !g_flying) {
        if (!g_placing && !g_dragging) {
            Window* a = g_rage[0].clickArea;
            GUI::SetInteractionObjectLock(g_root, a);
            if (BuildingHovers::GetArrowClickCallbackID() != g_netArrowCallback)
                g_netArrowCallback = BuildingHovers::SetArrowClickCallback(new GUI::Callback([] { OnAutoCast(0); }));
            BuildingHovers::ArrowAt((float)(g_root->x + a->x + a->w / 2), (float)(g_root->y + a->y), false, false, false,
                                    false, true, false, false);
        } else {
            BuildingHovers::HideArrow();
            g_netArrowCallback = 0;
        }
    }
    // The arrow at the confirm button once the footprint's middle is on an enemy; until then the
    // button is off and the screen locked.
    if (GameState::secondTutorial == 0x9e) {
        int ex, ey;
        SpellMovement::GetEpicenterPos(ex, ey);
        if (!EntityManager::GetEnemyAtXY(ex, ey)) {
            GUI::SetInteractionLock(true);
            g_netArrowCallback = 0;
            BuildingHovers::HideArrow();
            g_left.root->SetEnabled(false);
        } else {
            GUI::SetInteractionObjectLock(g_root, g_left.root);
            if (BuildingHovers::GetArrowClickCallbackID() != g_netArrowCallback)
                g_netArrowCallback = BuildingHovers::SetArrowClickCallback(new GUI::Callback(OnLeft));
            BuildingHovers::ArrowAt((float)(g_root->x + g_left.root->x + g_left.root->w / 2),
                                    (float)(g_root->y + g_left.root->y), false, false, false, false, true, false, false);
            g_left.root->SetEnabled(true);
        }
    }
    if (g_lastMana != mana) g_root->UpdatePosition();
    g_slotStamp = (uint32_t)Timer::GetGlobalTime();
    g_lastMana = mana;
}

// The flying net: from the hero to the monster in 2/3 s, growing from 0.2 to its full size.
void FlyNet(float dt) {
    if (g_net) {
        if (g_netT < 1.f) {
            Render::SetAlpha(g_net, 1.f);
            const float t = g_netT, k = 0.2f + t * 0.8f;
            g_net->h = (float)g_net->tex->h * k;
            g_net->w = (float)g_net->tex->w * k;
            Render::SetPosition(g_net, g_netFromX + t * (g_netToX - g_netFromX) + g_net->w * -0.5f,
                                g_netFromY + t * (g_netToY - g_netFromY) + g_net->h * -0.5f, 0.2f);
            // Shader 8 turns it about its middle (color.xy) by color.z.
            g_net->color[0] = g_net->x + g_net->w * 0.5f;
            g_net->color[1] = g_net->h * 0.5f - g_net->y;
            g_net->color[2] = t * -3.5f;
            g_netT = t + dt * 1.5f;
        } else {
            NetLanded();
        }
    }
}

// @0x25769c (the belt's potion, else the inventory's, else the shop's): points the player at a
// mana potion (the shop's: 0x88).
bool OnShowManaPotion(unsigned) {
    for (unsigned slot = 0; slot < GameState::GetBeltSlotCount(); ++slot) {
        GameState::PlayerItem* item = GameState::GetFirstItemByID(GameState::GetBeltItemAt(slot), true);
        if (item && item->info->meta && item->info->meta->FindByType(kExpMana)) {
            BeltBarWindow::ArrowAtSlot(slot);
            return true;
        }
    }
    if (GameState::IsArenaCombatActive()) return false;
    for (unsigned i = 0; GameState::PlayerItem* item = GameState::EnumItems(i); ++i)
        if (item->info->meta && item->info->meta->FindByType(kExpMana)) {
            CharacterInfoWindow::Show();
            CharacterInfoWindow::ShowInventoryItem(item->id, 4, item);
            return true;
        }
    if (ItemShopWindow::ItemCanBeBought(0x88, 0)) {
        ItemShopWindow::Show();
        ItemShopWindow::ShowShopItem(0x88, 0, true, (unsigned)-1);
        return true;
    }
    return false;
}

// @0x2577a4: a tap on the bar while the hero has no mana points at a mana potion.
void OnManaBar() {
    Entity* player = EntityManager::GetPlayer();
    if (!player || player->GetAP() != 0) return;
    OnShowManaPotion((unsigned)-1);
}

// The hint over rage button i: its text rises from the button (style 0xe).
void ShowSlotHint(unsigned i, const char32_t* text) {
    const Window* b = g_rage[i].root;
    int x = g_root->x + b->x + b->w / 2, y = g_root->y + b->y;
    Map::MouseCoordinatesToWorld(x, y);
    BuildingHovers::ShowTextHoverWithStyle((float)x, (float)y, text, 0xe, 2.f, 50.f, true, false);
}

// The slot's spell can be cast (its background is not the "inactive" one): in the arenas it is
// aimed with SpellMovement at a random opponent, else it flies to the opponent the hero faces.
bool CastSlot(unsigned i) {
    BaseCombat* combat = GameState::GetActiveCombat();
    if (!combat) {
        ShowSlotHint(i, StringTable::GetString("SPELL_NO_TARGET"));
        return false;
    }
    if (combat->GetState() == 0x11) return true;
    BaseSquad* enemy = combat->GetEnemySquad();
    if (!enemy) return false;
    Entity* player = EntityManager::GetPlayer();
    int type = combat->GetType();
    if (type == 5 || type == 6) {
        if (!combat->CanSpellCastNow()) return true;
        Spells::SpellInfo* spell = Spells::GetBestSpellForSlot(i + 1, (uint32_t)player->GetAP(), true);
        if (!spell) return false;
        Entity* target = enemy->GetRandomAliveSolider();
        enemy->SetActiveSoldier(target);
        SpellMovement::Activate(spell, target->tileX, target->tileY);
        SoundsManager::PlaySound("ui_swing_in", 1, false);
        ParticlePresets::SpellPreload(spell->id, false);
        return true;
    }
    Entity* target = enemy->GetActiveSoldier();
    if (!target) return false;
    if (!target->IsEnemy() || target->IsDead() || target->IsDying()) return true;
    if (!combat->CanSpellCastNow()) return true;
    Spells::SpellInfo* spell = Spells::GetBestSpellForSlot(i + 1, (uint32_t)player->GetAP(), true);
    if (!spell) return false;
    // The marker starts on the button, centred on it horizontally.
    const Render::Texture* marker = spell->marker;
    g_flyX = g_root->x + g_rage[i].root->x - marker->w / 2;
    g_castSlot = (int)i + 1;
    g_flyY = g_root->y + g_rage[i].root->y + marker->h / 2;
    if (!g_flySprite) {
        g_flySprite = Render::CreateSprite(spell->marker, 0xd, false, false);
        g_flySprite->screenSpace = true;
        Render::SetShaderType(g_flySprite, 1);
        Render::SetPosition(g_flySprite, (float)g_flyX, (float)g_flyY, 0.001f);
        g_flySprite->w *= GUI::GetHoverScaleFactor(1.f, 1.f);
        g_flySprite->h *= GUI::GetHoverScaleFactor(1.f, 1.f);
    }
    g_flyT = 0.f;
    g_flying = true;
    g_flyToX = (int)target->worldX;
    g_flyToY = (int)(target->worldY - 20.f);
    player->OnSpellCast(target);
    SoundsManager::PlaySound("spell_fly", 1, false);
    // (Online, dormant: OG::MakeRequest(0x14, 0x10, spell id, -1, 0).)
    ParticlePresets::SpellPreload(spell->id, false);
    return true;
}

// The slot is locked: its hint says what unlocks the spell: the level, or the quest (the first of
// a chain that is not yet open: its window opens, else its name shows). The original asks for
// slot i, not i + 1, here.
void ShowLockedHint(unsigned i) {
    Spells::SpellInfo* spell = Spells::GetBestSpellForSlot(i, 300, false);
    if (!spell) return;
    std::u32string text;
    if (spell->level > (uint32_t)GameState::GetLevel()) {
        text = SWPrintf(0x100, StringTable::GetString("BUILDING_BAKERY_UNLOCKS"), {ToWideString((int)spell->level)});
    } else {
        if (spell->unlockQuest == 0 || GameState::TaskCompleted(spell->unlockQuest)) return;
        uint32_t quest = spell->unlockQuest;
        for (int n = 0; n != 0x10;) {
            Tasks::TaskInfo* task = Tasks::GetTask(quest);
            if (!task) break;
            bool locked = task->IsLockedByTask();
            ++n;
            if (!locked) break;
            uint32_t before = (uint32_t)task->lockTasks->GetChild(0)->GetInt();
            Tasks::TaskInfo* prior = Tasks::GetTask(before);
            if (prior && !prior->tutorialType && !prior->isHint &&
                (prior->subtasks[0].type != 0x25 || prior->subtasks[1].type != 0))
                quest = before;
        }
        if (GameState::GetTaskBeginTime(quest) != 0 && !GameState::IsBossCombatActive() &&
            !GameState::IsArenaCombatActive()) {
            TaskInfoWindow::SetTask(quest, false);
            TaskInfoWindow::Show();
            return;
        }
        // (The messages are the original's; "HIRE_TROOPS_TASK_LOCK" and "BUILDING_BAKERY_UNLOCKS"
        // belong to other lines of the game.)
        if (Tasks::TaskInfo* task = Tasks::GetTask(quest))
            text = SWPrintf(0x100, StringTable::GetString("HIRE_TROOPS_TASK_LOCK"), {task->name});
        else
            text = SWPrintf(0x100, U"%s", {StringTable::GetString("UPGRADE_LOCKED")});
    }
    ShowSlotHint(i, text.c_str());
}

// @0x2577d4: a rage button was tapped. False when nothing came of it.
bool OnAutoCastImpl(unsigned i) {
    if (g_netTarget != 0 || !g_rage[i].root->enabled) return false;
    if (GameState::secondTutorial == 0x9d) GameState::secondTutorial = 0x9e;
    if (!g_rage[i].activeMid->visibleSelf) return CastSlot(i);
    if (!g_rage[i].iconLocked->visibleSelf) {
        // No mana: a mana potion is pointed out, else the hint says so.
        if (OnShowManaPotion(i)) return false;
        ShowSlotHint(i, StringTable::GetString("SPELL_NO_MANA"));
        return false;
    }
    ShowLockedHint(i);
    return false;
}

// @0x2594dc: the click reaches the bar. After the second tutorial's step 0x9e, in a fight, a press on
// a rage slot starts a drag: released where it was pressed (within 20 pixels) it is a tap on the slot
// (OnAutoCast); a spell being placed (SpellMovement, 5k) is cast where it is released. No click
// while a spell flies.
bool Click(int x, int y, bool pressed) {
    if (g_flying) return false;
    if (GUI::CanInteractWith(g_root) && g_root->visibleSelf && GameState::secondTutorial >= 0x9e &&
        !GameState::IsVirtualTutorial()) {
        if (pressed) {
            for (unsigned i = 0; i < 3; ++i) {
                const Window* a = g_rage[i].clickArea;
                int left = g_root->x + a->x, top = g_root->y + a->y;
                if (left <= x && x <= left + a->w && top <= y && y <= top + a->h && GameState::IsCombatActive()) {
                    g_flySprite = Render::RemoveSprite(g_flySprite);
                    g_dragX = x;
                    g_flyX = x;
                    g_flyY = y;
                    g_castSlot = (int)i + 1;
                    g_dragY = y;
                    g_dragging = true;
                    break;
                }
            }
        } else if (g_placing) {
            g_placing = false;
            g_dragging = false;
            g_flySprite = Render::RemoveSprite(g_flySprite);
            OnSpell((unsigned)g_castSlot, x, y, true);
            return true;
        } else if (g_dragging) {
            g_dragging = false;
            if (std::abs(g_dragY - y) + std::abs(g_dragX - x) < 0x14 && OnAutoCast((unsigned)(g_castSlot - 1))) return true;
        }
    }
    return g_root->Click(x, y, pressed, false);
}

}  // namespace

WindowManager::FunctionalWindow* Queue() {
    if (!g_queue) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.click = Click;
        f.setZ = SetZ;
        f.hide = Hide;
        f.move = Move;
        f.deinit = Deinit;
        g_queue = new WindowManager::FunctionalWindow("BottomBattleWindow", std::move(f));
    }
    return g_queue;
}

// @0x25d4a8
void Init() {
    g_offsetY = 0x17;
    g_offsetX = 0;
    g_queue->usesZRange = false;
    g_tablet = GUI::IsTabletVersion();
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Rage_bar.xml", "Rage_bar.png", GUI::GetHudScaleFactor(), 0,
                             0, 0, 0, false, 1.f);
    g_queue->RegisterTopWindow(g_root);
    g_scale = g_root->scale;
    g_root->SetVisibility(false);
    g_root->SetPosition(-g_root->w, 0);
    g_effect = GUI::CreateMovementEffect(g_root, nullptr, OnTweenIn, OnTweenOut);
    g_background = GUI::GetWindow(g_root, "rage_bar_bg");
    g_background->SetOnClick(OnManaBar);
    g_left.LoadFrom(g_root, "button_fury_large_01");
    g_left.root->SetOnClick(OnLeft);
    g_right.LoadFrom(g_root, "button_fury_large_02");
    g_right.root->SetOnClick(OnRight);
    g_centre.LoadFrom(g_root, "button_fury_large_03");
    g_centre.root->SetOnClick(OnCenter);
    g_stance = GUI::GetWindowTyped<Textfield>(g_root, "text_stance");
    g_holderText = GUI::GetWindowTyped<Textfield>(g_root, "rage_bar_bar_holder.text_02");
    const char* const bars[3] = {"rage_bar_bar_holder.rage_bar_orange", "rage_bar_bar_holder.rage_bar_green",
                                 "rage_bar_bar_holder.rage_bar_red"};
    const char* const barBgs[3] = {"rage_bar_orange_bg", "rage_bar_green_bg", "rage_bar_red_bg"};
    const char* const barTexts[3] = {"text_orange", "text_green", "text_red"};
    for (int i = 0; i < 3; ++i) g_mana[i].LoadFrom(g_root, bars[i], barBgs[i], barTexts[i]);
    for (int i = 0; i < 3; ++i) {
        char name[32];
        std::snprintf(name, sizeof name, "rage_bar_button_%02d", i + 1);
        g_rage[i].LoadFrom(g_root, name);
        // (FUN_00259c18, its callback object @0x259c28: calls OnAutoCast(i))
        g_rage[i].clickArea->SetOnClick([i] { OnAutoCast((unsigned)i); });
    }
    g_queue->fn.zRange = WindowManager::ReturnSmallZRange;
    g_queue->fn.update = Update;
    g_queue->fn.show = Show;
    g_offsetY = (int)(g_scale * (float)g_offsetY);
    g_offsetX = (int)((float)g_offsetX * g_scale);
}

bool IsVisible() { return g_root && g_root->visibleSelf; }   // @0x256e98
void SetOffset(float y) { g_offsetY = (int)y; }               // @0x256e7c
void SetSelectedButton(unsigned b) { g_selected = b; }        // @0x256eac
bool IsInAttackStance() { return g_selected == 0; }        // @0x2570b0
bool IsInBlockStance() { return g_selected == 1; }            // @0x2570cc

// @0x256ec0: the stance buttons' (and rage buttons') enabled look; the unchosen glows follow it.
// Not at second-tutorial step 0x9e.
void SetEnabled(bool on, bool, bool) {
    if (GameState::secondTutorial == 0x9e) return;
    g_left.root->SetEnabled(true);
    for (Window* b : g_left.bg) b->SetEnabled(on);
    g_left.icon->SetEnabled(on);
    g_right.root->SetEnabled(true);
    for (Window* b : g_right.bg) b->SetEnabled(on);
    g_right.icon->SetEnabled(on);
    if (g_selected == (unsigned)-1 || g_selected == 0) g_left.active->SetVisibility(on);
    if (g_selected == (unsigned)-1 || g_selected == 1) g_right.active->SetVisibility(on);
    for (RageButton& r : g_rage) r.root->SetEnabled(on);
    if (g_enabled != on) {
        g_enabled = on;
        g_slotStamp = 0;
    }
}

// @0x257490: the tutorial's arrow at the attack button (task: completed by it).
void ShowAttackButton(unsigned task, bool lock, bool arrow) {
    Window* b = g_left.root;
    PointAt((int)task, OnLeft, b, lock, arrow, (float)(b->x + g_root->x + b->w / 2), (float)(b->y + g_root->y));
}

// @0x257388: the same at the block button.
void ShowBlockButton(unsigned task, bool lock, bool arrow) {
    Window* b = g_right.root;
    PointAt((int)task, OnRight, b, lock, arrow, (float)(b->x + g_root->x + b->w / 2), (float)(b->y + g_root->y));
}

// @0x25d3bc: the same at the first rage button (the arrow's tap is its spell's).
void ShowMagicButton(unsigned task, bool lock, bool arrow) {
    Window* b = g_rage[0].root;
    PointAt((int)task, [] { OnAutoCast(0); }, g_rage[0].clickArea, lock, arrow, (float)(b->x + b->w / 2), (float)b->y);
}

// @0x259708
void Deinit() {
    GUI::RemoveMovementEffect(g_effect);
    g_effect = nullptr;
    delete g_root;
    g_root = nullptr;
}

// @0x25cfd8: from the taming lesson on; it slides up from the bottom, centred.
void Show() {
    if (GameState::tutorial <= 0x16 || !g_root) return;
    if (GameState::secondTutorial == 0x97) BeltBarWindow::RunRageAnimation();
    if (g_effect->active && g_effect->hideAtEnd) g_root->SetVisibility(false);
    g_queue->MoveWindowOnTop(true);
    if (!g_root->visibleSelf) {
        int x = (GUI::ScreenWidth() - g_root->w) / 2;
        g_effect->Animate(x, GUI::ScreenHeight(), x + g_offsetX, GUI::ScreenHeight() + g_offsetY - g_root->h, true);
    }
    g_root->SetVisibility(true);
    g_root->PreloadTextures();   // GUI::PreloadWindow
    // (0x61844c, 0x618448, 0x60f114, 0x618444: the fury bar's animation, milestone 5)
    g_selected = (unsigned)-1;
    Update(0.f);
}

// @0x259338
void Hide() {
    if (!g_root) return;
    if (g_root->visibleSelf) {
        int x = (GUI::ScreenWidth() - g_root->w) / 2;
        g_effect->Animate(x + g_offsetX, GUI::ScreenHeight() + g_offsetY - g_root->h, x, GUI::ScreenHeight(), false);
    }
    g_selected = (unsigned)-1;
    g_spell = -1;
}

// @0x2593e4
void SetZ(float) {
    g_root->SetZ(HUDWindow::GetZ());
    if (g_effect->active) return;
    if (!g_root->visibleSelf) {
        g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::ScreenHeight());
        return;
    }
    g_root->SetPosition(g_offsetX + (GUI::ScreenWidth() - g_root->w) / 2, g_offsetY + GUI::ScreenHeight() - g_root->h);
}

// @0x258f30 (its tail, the debug TameNext throw and the reset, is _INIT_61 @0x258f9c): a monster dies: if it has a tamed form (and that form's quest is open) the net may
// catch it for a moment (0.5 to 1 s). The taming lesson's boar (spawn point 0x1e, or 0x6e9)
// gets the hero's hint and the arrow at the centre button.
void EnableNet(uint32_t spawn) {
    g_netTarget = 0;
    Entity* e = EntityManager::GetEntityBySpawnPoint(spawn);
    if (e) {
        EntityData* tamed = EntityFactory::GetTamedEntityForID(e->GetEntityData()->id);
        if (tamed) {
            if (spawn == 0x1e || spawn == 0x6e9) {
                Render::CenterOn(616.f, 260.f, true);
                PlayerTopWindow::SetParameters(StringTable::GetString("TAME_TUT2"), 0, false, true);
                g_netTask = 0;
                g_netArrowCallback = BuildingHovers::SetArrowClickCallback(new GUI::Callback(OnCenter));
                GUI::SetInteractionObjectLock(g_centre.root, nullptr);
                BuildingHovers::ArrowAt((float)(g_centre.root->x + g_root->x + g_centre.root->w / 2),
                                        (float)(g_centre.root->y + g_root->y), false, false, false, false, true, false,
                                        false);
                BuildingHovers::PreventArrowAutoHide();
            }
            g_netTarget = spawn;
            g_netTimer = 0.5f + ((float)(Rand48::lrand48() % 1000) / 1000.f) * 0.5f;
            if (tamed->unlockQuest != 0) {
                Tasks::TaskInfo* t = Tasks::GetTask(tamed->unlockQuest);
                if (!t || t->IsLockedByTask()) g_netTarget = 0;
            }
        }
    }
    if (HUDWindow::GetDebugState() && g_tameNext && g_netTarget != 0) OnCenter();
    g_tameNext = false;
}

void TameNext() { g_tameNext = true; }   // @0x25710c

bool OnAutoCast(unsigned i) { return OnAutoCastImpl(i); }

// @0x25858c: the PvP bot casts: not while the slot is locked or a spell is flying.
bool OnBotAutoCast(unsigned i) {
    if (g_rage[i].iconLocked->visibleSelf || g_flySprite) return false;
    return OnAutoCast(i);
}

// @0x25d170: points the player at the way to cast spell `spellId`: no mana or another spell
// better for its slot: the popup / the arrow that offers the explanation again; no fight: the
// same for "no target"; else the arrow at its button, which casts it.
void HelpWithSpell(unsigned spellId, bool popup) {
    Spells::SpellInfo* spell = Spells::GetSpellInfo(spellId);
    if (!spell) return;
    unsigned slot = spell->slot - 1;
    Entity* player = EntityManager::GetPlayer();
    Spells::SpellInfo* best = Spells::GetBestSpellForSlot(spell->slot, (uint32_t)player->GetAP(), true);
    int callback;
    if ((uint32_t)player->GetAP() < spell->cost || spell != best) {
        if (popup) {
            PopupWindow::Show(StringTable::GetString("SPELL_NO_MANA"), PopupWindow::Hide, nullptr, nullptr);
            return;
        }
        callback = BuildingHovers::SetArrowClickCallback(new GUI::Callback([spellId] { HelpWithSpell(spellId, true); }));
    } else if (!GameState::IsCombatActive()) {
        if (popup) {
            PopupWindow::Show(StringTable::GetString("SPELL_NO_TARGET"), PopupWindow::Hide, nullptr, nullptr);
            return;
        }
        callback = BuildingHovers::SetArrowClickCallback(new GUI::Callback([spellId] { HelpWithSpell(spellId, true); }));
    } else {
        callback = BuildingHovers::SetArrowClickCallback(new GUI::Callback([slot] { OnAutoCast(slot); }));
    }
    g_netArrowCallback = callback;
    Window* area = g_rage[slot].clickArea;
    BuildingHovers::ArrowAt((float)(g_root->x + area->x + area->w / 2), (float)(g_root->y + area->y), false, false, false,
                            false, true, false, false);
}

// @0x2570ec: the rage buttons glow from now on (Update, 5j-3c, reads g_furyGlow).
void EnableFuryGlow() {
    g_furyGlowStart = g_pulse;
    g_furyGlow = true;
}

// @0x257594: the player chose spell `spell` (a rage button, or SpellMovement's accept). The fight
// is told and goes on if it was waiting; at second-tutorial step 0x9e that step is done (0x9f).
void OnSpell(unsigned spell, int, int, bool) {
    BaseCombat* combat = GameState::GetActiveCombat();
    if (combat) {
        combat->QueueSpellCast(spell, -1, -1);
        bool waiting = combat->GetType() < 3 ? combat->IsPaused() : combat->GetState() == 2 || combat->IsPaused();
        if (waiting) combat->Continue(-1);
    }
    if (GameState::secondTutorial != 0x9e) return;
    BuildingHovers::HideArrow();
    BuildingHovers::HideWorldDialog(nullptr);
    GUI::SetInteractionLock(true);
    if (combat) combat->Continue(-1);
    GameState::secondTutorial = 0x9f;
}

uint32_t GetNetTargetSpawnID() { return g_root && g_root->visibleSelf ? g_netTarget : 0; }   // @0x257090

// @0x2588b4: with a net target the net is thrown at it and the hero cheers; without one the hero
// starts or stops running (running costs mana: with none, the hint says so).
// Outside fights a search job the hero works is first finished with fury when it can be.
void OnCenter() {
    Entity* player = EntityManager::GetPlayer();
    if (g_netTarget == 0) {
        Professions::Profession* prof = nullptr;
        if (int cost = SearchFuryCost(player, prof)) {
            Map::Decor* d = player->GetWorkplaceDecoration();
            d->collectStart = (uint32_t)Timer::GetGlobalTime() + ~d->f50;
            player->AddAP(-cost);
            return;
        }
        const Window* b = g_centre.root;
        int x = g_root->x + b->x + b->w / 2, y = g_root->y + b->y;
        Map::MouseCoordinatesToWorld(x, y);
        if (player->GetAP() == 0) {
            BuildingHovers::ShowTextHoverWithStyle((float)x, (float)y, StringTable::GetString("SPELL_NO_MANA"), 0xe, 2.f,
                                                   50.f, true, false);
            return;
        }
        // The line names the mode the hero goes into.
        std::u32string text = SWPrintf(
            0x100, U"%s\n%s",
            {StringTable::GetString("player_spell_run"),
             StringTable::GetString(player->IsSpeedRunning() ? "BOTTOM_BAR_DEACTIVATED" : "BOTTOM_BAR_ACTIVATED")});
        BuildingHovers::ShowTextHoverWithStyle((float)x, (float)y, text.c_str(), 0xe, 2.f, 50.f, true, false);
        if (!player->IsSpeedRunning()) {
            if (player->GetAP() != 0) {
                player->GetSquad()->EnableSpeedRun();
                SoundsManager::PlaySound("ui_activate_slot", 1, false);
            }
        } else {
            player->GetSquad()->DisableSpeedRun();
            SoundsManager::PlaySound("ui_deactivate_slot", 1, false);
        }
        return;
    }
    if (g_netTask != 0) Tasks::CompleteTask(g_netTask, true, true, false);
    if (GameState::IsTameTutorial()) {
        // (step 0x17: TutorialSender::AddTutorialStep(0x4c8))
        if (GameState::tutorial == 0x17) TutorialSender::AddTutorialStep(0x4c8);
        BuildingHovers::HideArrow();
        GUI::SetInteractionLock(true);
    }
    Entity* e = EntityManager::GetEntityBySpawnPoint(g_netTarget);
    Items::ItemInfo* info = Items::GetItemInfo(Items::GetPlayersBestNet());
    if (info && e) {
        g_net = Render::RemoveSprite(g_net);
        g_net = Render::CreateSprite(info->GetDropIcon(), 0xe, false, false);
        Render::SetShaderType(g_net, 8);
        g_netT = 0.f;
        g_netFromX = player->worldX;
        g_netFromY = player->worldY + player->GetIdleHeight() * -0.5f;
        g_netToX = e->worldX;
        g_netToY = e->worldY;
    }
    player->SetAnimationOnce("bat_vict", false, false, false, true);
}

}  // namespace BottomBattleWindow
