// BeltBarWindow: the inventory belt at the bottom right (inventory button, item slots, Crafting
// button) and the crafting progress bar. Port of BeltBarWindow (libkingdom.so 5.11),
// 0x24c63c..0x251694.
#include <cstdio>

#include "engine/IconManager.h"
#include "engine/Render.h"
#include "engine/Timer.h"
#include "game/AIState.h"
#include "game/BuildingHovers.h"
#include "game/BuildingMovement.h"
#include "game/BuildingPlacement.h"
#include "game/Entity.h"
#include "game/EntityManager.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/Map.h"
#include "game/MetaExpression.h"
#include "game/Setting.h"
#include "game/StringTable.h"
#include "hud/ItemHoverWindow.h"
#include "windows/Windows.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/HUD.h"
#include "hud/Shared.h"

namespace BeltBarWindow {
namespace {

using GUI::Textfield;
using GUI::Window;

WindowManager::FunctionalWindow* g_queue = nullptr;
int g_offsetX = 0x89, g_offsetY = 0x14;   // 0x60f0c8 / 0x60f0cc
bool g_tablet = false;                    // 0x60f0d4
Window* g_root = nullptr;                 // "Belt_bar"
GUI::MovementEffect* g_effect = nullptr;
Window* g_bg2Slots = nullptr;             // battle_bar_main_bg_2_slots_right
Window* g_bgRight = nullptr;              // bottom_bar_bg_right
Window* g_inventoryIcon = nullptr;        // button_inventory.icon_60
Textfield* g_barName = nullptr;           // text_bar_name
Textfield* g_barSize = nullptr;           // text_bar_size
Window* g_inventoryClick = nullptr;       // button_inventory.clickArea
Shared::BottomSlotInfo g_slots[3];
Window* g_craft = nullptr;                // hud_button_craft
Window* g_craftBg = nullptr;
Textfield* g_craftText = nullptr;
Window* g_craftIcon = nullptr;
Window* g_craftLock = nullptr;
Window* g_craftBar = nullptr;             // "Crafting_p_bar"
Textfield* g_craftProf = nullptr;
Textfield* g_craftBarText2 = nullptr;
Textfield* g_craftBarText = nullptr;
Window* g_craftProgress = nullptr;
GUI::ClipRect g_craftClip;
Window* g_craftProfIcon = nullptr;
GUI::MovementEffect* g_craftEffect = nullptr;
ItemHoverWindow* g_hover = nullptr;       // 0x617c14 the card of the tapped slot's item
int g_arrowCallback = 0;                  // 0x617dc4 the id of the arrow callback that points at a slot
int g_barNameX = 0;                       // 0x617e08 text_bar_name's laid-out x
int g_lastSecond = 0;                     // 0x617e24
// Per slot (BottomSlotInfo +0x58..+0x60): the charge bar glides to progressValue.
struct Glide { bool started = false; float shown = 0.f, time = 0.f; } g_glide[3];

// The world point under a slot's top middle (the text popups rise from there).
void SlotWorldPoint(unsigned i, int& x, int& y) {
    Window* h = g_slots[i].holder;
    x = g_root->x + h->x + h->w / 2;
    y = g_root->y + h->y;
    Map::MouseCoordinatesToWorld(x, y);
}

// (0x24c94c) UNVERIFIED (milestone 5): CharacterInfoWindow::Show on the belt tab (3).
void OnEmptySlot(unsigned) {}

// (0x24d9e4): none of the slot's item left. Before the shop's quest: "later"; after it the item
// shop opens at the item (UNVERIFIED, milestone 5: ItemShopWindow::Show / ShowShopItem).
void OnAddMoreSlot(unsigned i) {
    if (GameState::TaskCompleted((unsigned)Setting("shop_unlock_task").GetInt())) return;
    int x, y;
    SlotWorldPoint(i, x, y);
    BuildingHovers::ShowTextHoverWithStyle((float)x, (float)y, StringTable::GetString("BOTTOM_BAR_LATER"), 0xe, 2.f,
                                           50.f, true, true);
}

// @0x24f0e4: a tap on a slot: the item's card (Drink / Turn on) over it. UNVERIFIED (milestone 5):
// with the character window or the item shop open the tap goes to them instead
// (ShowInventoryItem / ShowShopItem).
void OnSlot(unsigned i) {
    if (i >= 3) return;
    Items::ItemInfo* info = Items::GetItemInfo(GameState::GetBeltItemAt(i));
    if (!info) {
        OnEmptySlot(i);
        return;
    }
    if (GameState::GetItemAmount(info->id, true) == 0) {
        OnAddMoreSlot(i);
        return;
    }
    // SoundsManager::PlaySound("ui_click"): milestone 6.
    int& st = GameState::secondTutorial;
    if (st == 0x96) {
        OnUseSlot(i);
        BuildingHovers::HideArrow();
        BuildingHovers::HideWorldDialog(nullptr);
        st = 0x97;
        return;
    }
    g_hover->Show();
    uint32_t id = GameState::GetBeltItemAt(i);
    g_hover->SetItem(StringTable::GetString("BELT_ITEM"), GameState::GetFirstItemByID(id, false), Items::GetItemInfo(id),
                     true, true, nullptr, false, false, false, false);
    Window* h = g_slots[i].holder;
    int x = g_root->x + h->x, y = g_root->y + h->y;
    g_hover->SetTargetRect(x, y, x + h->w, y + h->h, ItemHoverWindow::kSE);
    g_hover->RecomputePosition();
    g_hover->SetZ(0.001f);
    Render::SortRenderLayer(0xe, 1);
    if (st == 0x87) {
        g_hover->HelpEnable(true);
        BuildingHovers::PreventArrowAutoHide();
        st = 0x88;
    } else if (st == 0x8c) {
        g_hover->HelpEnable(true);
        BuildingHovers::PreventArrowAutoHide();
    }
}

// @0x2509ac. UNVERIFIED (milestone 5): with the character window open a slot's item can be dragged
// out of the belt; the edit modes' handling of taps over the belt.
bool Click(int x, int y, bool pressed) {
    if (!g_root) return false;
    if (g_hover->Click(x, y, pressed)) return true;
    return (g_craftBar && g_craftBar->Click(x, y, pressed, false)) || g_root->Click(x, y, pressed, false);
}

}  // namespace

WindowManager::FunctionalWindow* Queue() {
    if (!g_queue) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.setZ = SetZ;
        f.hide = Hide;
        f.click = Click;
        g_queue = new WindowManager::FunctionalWindow("BeltBarWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_root && g_root->visibleSelf; }

void GetBeltItemLocation(int& x, int& y, int slot) {
    x = g_root->x + g_slots[slot].holder->x;
    y = g_root->y + g_slots[slot].holder->y;
}

void GetAdjustedSideButtonLocation(int& x, int& y, int w, int h) {
    x = g_root->x + g_inventoryIcon->x + g_inventoryIcon->w / 2 - w / 2;
    if (!g_root->visibleSelf) x -= g_root->w / 2;
    y = g_root->y + g_inventoryIcon->y + g_inventoryIcon->h / 2 - h / 2;
}

void AutoBind(uint32_t id) {
    if (GameState::GetItemAmount(id, true) == 0) return;
    Items::ItemInfo* info = Items::GetItemInfo(id);
    if (!info || info->type != 3 || info->IsLocked()) return;
    for (unsigned i = 0; i < GameState::GetBeltSlotCount(); ++i)
        if (GameState::GetBeltItemAt(i) == id) return;
    for (unsigned i = 0; i < GameState::GetBeltSlotCount(); ++i) {
        if (i == 0 && GameState::SecondTutorialStep() == 0x94) continue;
        uint32_t cur = GameState::GetBeltItemAt(i);
        if (cur != 0 && GameState::GetItemAmount(cur, true) != 0) continue;
        GameState::BindBeltItemTo(i, id);
        Items::ItemInfo* bound = Items::GetItemInfo(id);
        if (!bound || !bound->CanBeActivated() || GameState::SecondTutorialStep() == 0x85) return;
        GameState::SetBeltItemActivationTimeAt(i, (uint32_t)Timer::GetGlobalTime());
        return;
    }
}

// @0x250ebc
void Init() {
    g_offsetX = 0x89;
    Queue()->usesZRange = false;
    g_offsetY = 0x14;
    g_tablet = GUI::IsTabletVersion();
    const int W = GUI::ScreenWidth(), H = GUI::ScreenHeight();
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Belt_bar.xml", "Belt_bar.png",
                             GUI::GetHudScaleFactor(), 0, 0, 0, 0, false, 1.f);
    if (!g_root) return;
    Queue()->RegisterTopWindow(g_root);
    g_root->SetVisibility(false);
    g_root->SetPosition(W, H - g_root->h);
    // OnTweenMove @0x24c674 is empty. OnTweenIn @0x24cb9c: an inventory asked for while the belt
    // was away (ShowInventory's flag 0x617dbc, milestone 5) opens once the belt is in; nothing
    // sets the flag before then.
    g_effect = GUI::CreateMovementEffect(g_root, [] {}, [] {});
    const int beltSlots = (int)GameState::GetBeltSlotCount();
    g_bg2Slots = GUI::GetWindowTyped<Window>(g_root, "battle_bar_main_bg_2_slots_right");
    g_bg2Slots->SetVisibility(beltSlots != 3);
    g_bgRight = GUI::GetWindowTyped<Window>(g_root, "bottom_bar_bg_right");
    g_bgRight->SetVisibility(beltSlots == 3);
    g_inventoryIcon = GUI::GetWindowTyped<Window>(g_root, "button_inventory.icon_60");
    g_inventoryIcon->takesZ = true;
    g_barName = GUI::GetWindowTyped<Textfield>(g_root, "text_bar_name");
    g_barSize = GUI::GetWindowTyped<Textfield>(g_root, "text_bar_size");
    g_inventoryClick = GUI::GetWindowTyped<GUI::Button>(g_root, "button_inventory.clickArea");
    g_inventoryClick->SetOnClick([] {});   // UNVERIFIED (milestone 5): OnInventory (CharacterInfoWindow)
    g_inventoryClick->SetActionSound("ui_click", 1);
    // Slots: numbering 1..3 for a 3-slot belt, else 2, 3, 1.
    int numbers[3] = {beltSlots == 3 ? 1 : 2, beltSlots == 3 ? 2 : 3, beltSlots == 3 ? 3 : 1};
    for (int i = 0; i < 3; ++i) {
        char name[64];
        std::snprintf(name, sizeof name, "bottom_slot_item_%02d", numbers[i]);
        g_slots[i].LoadFrom(g_root, name);
        g_slots[i].iconHolder->SetTexture(IconManager::GetIcon("gold_potion", false));
        g_slots[i].holder->SetOnClick([i] { OnSlot((unsigned)i); });
    }
    g_craft = GUI::GetWindowTyped<Window>(g_root, "hud_button_craft");
    g_craft->SetOnClick([] {});             // OnCrafting
    g_craftBg = GUI::GetWindowTyped<Window>(g_root, "hud_button_craft.hud_button_small_round_blue");
    g_craftText = GUI::GetWindowTyped<Textfield>(g_root, "text_craft");
    g_craftIcon = GUI::GetWindowTyped<Window>(g_root, "hud_button_craft.icon_35");
    g_craftIcon->takesZ = true;
    g_craftLock = GUI::GetWindowTyped<Window>(g_root, "hud_button_craft.icon_16_lock");
    g_craftLock->SetTexture(IconManager::GetIcon("gold_16_lock", false), true);
    g_barNameX = g_barName->x;
    g_hover = new ItemHoverWindow();
    // (The TapHoverWindow created here, the belt's tap hint, is never shown: dead in 5.11.)

    g_craftBar = GUI::RegisterUI("../resource/kingdom_ui/1Original/Crafting_p_bar.xml", "Crafting_p_bar.png",
                                 GUI::GetHudScaleFactor(), 0, 0, 0, 0, false, 1.f);
    if (g_craftBar) {
        Queue()->RegisterTopWindow(g_craftBar);
        g_craftBar->SetVisibility(false);
        g_craftBar->SetOnClick([] {});
        g_craftProf = GUI::GetWindowTyped<Textfield>(g_craftBar, "text_prof");
        g_craftBarText2 = GUI::GetWindowTyped<Textfield>(g_craftBar, "pbar_craft.text_hp_bar_2");
        g_craftBarText = GUI::GetWindowTyped<Textfield>(g_craftBar, "pbar_craft.text_hp_bar");
        g_craftProgress = GUI::GetWindowTyped<Window>(g_craftBar, "pbar_craft.progress_bar_green");
        g_craftClip = {g_craftProgress->x, g_craftProgress->y, g_craftProgress->w + g_craftProgress->x,
                       g_craftProgress->h + g_craftProgress->y};
        g_craftBarText->SetClipRect(&g_craftClip);
        g_craftProgress->SetClipRect(&g_craftClip);
        g_craftProfIcon = GUI::GetWindowTyped<Window>(g_craftBar, "icon_prof");
        g_craftProfIcon->takesZ = true;
        g_craftEffect = GUI::CreateMovementEffect(g_craftBar, [] {});   // OnCraftPanelMove
        g_craftEffect->blocksInput = false;
    }
    Queue()->fn.update = Update;
    Queue()->fn.show = Show;
    // Shared::BuffHolderManager(0x18, 2, -1, 0) (active buffs above the belt) follows with items.
}

// @0x250444: slide in from the right
void Show() {
    if (!g_root || !GameState::IsPlayerCity()) return;
    if (g_effect->active && g_effect->hideAtEnd) g_root->SetVisibility(false);
    if (GameState::TutorialStep() <= 0x13) return;   // (also not while placing a building)
    Queue()->MoveWindowOnTop(true);
    const int W = GUI::ScreenWidth(), H = GUI::ScreenHeight();
    if (!g_root->visibleSelf) {
        int y = H - g_root->h;
        g_effect->Animate(W, y, (W + g_offsetX) - g_root->w, y + g_offsetY, true);
        g_root->SetVisibility(true);
        UpdateContents();
    }
    // UNVERIFIED: the Crafting button visibility (TaskCompleted(Setting "craft_unlock_task")) and the
    // boss-combat lock follow with quests; the button stays as laid out.
}

// @0x24d1d4
void Hide() {
    if (!g_root || !g_root->visibleSelf) return;
    const int W = GUI::ScreenWidth(), H = GUI::ScreenHeight();
    int y = H - g_root->h;
    g_effect->Animate((W + g_offsetX) - g_root->w, y + g_offsetY, W, y, false);
}

// @0x250664
void SetZ(float) {
    if (!g_root) return;
    float z = HUDWindow::GetZ();
    if (g_craftBar) g_craftBar->SetZ(z + 0.001f);
    g_root->SetZ(z);
    if (!g_effect->active) {
        const int W = GUI::ScreenWidth(), H = GUI::ScreenHeight();
        if (!g_root->visibleSelf) g_root->SetPosition(W, (g_offsetY + H) - g_root->h);
        else g_root->SetPosition((g_offsetX + W) - g_root->w, (g_offsetY + H) - g_root->h);
    }
}

// @0x24c63c
void HideHoverWindow() {
    if (g_hover && g_hover->shownHover) g_hover->Hide();
}

// @0x24f05c: the slot's item is used on the hero (a potion is drunk) if it would do anything.
void OnUseSlot(unsigned i) {
    Entity* player = EntityManager::GetPlayer();
    if (Items::ItemInfo* info = Items::GetItemInfo(GameState::GetBeltItemAt(i))) {
        if (info->meta) {
            if (!info->meta->CanApplyItemEffect(player)) return;
            info->meta->OnApplyItemEffect(player);
        }
        GameState::RemoveItem(info->id, 1);
    }
    // SoundsManager::PlaySound("drink_potion"): milestone 6.
    UpdateContents();
}

// @0x24e944: the slot's orb is switched on or off (a text popup says which).
void OnActivateSlot(unsigned i) {
    int& st = GameState::secondTutorial;
    if (BuildingHovers::GetArrowClickCallbackID() == g_arrowCallback) {
        BuildingHovers::HideArrow();
        BuildingHovers::HideWorldDialog(nullptr);
    }
    if (st == 0x88) {
        // The tutorial's first orb: on to the nearest monster.
        GUI::SetInteractionLock(false);
        Entity* player = EntityManager::GetPlayer();
        if (Entity* enemy = EntityManager::GetNearestEnemy(player, false, false)) {
            AIBaseState* ai = player->GetAI();
            BuildingHovers::SetArrowClickCallback(new GUI::Callback([ai, enemy] { ai->ClickedEntity(enemy); }));
            BuildingHovers::ArrowAt(enemy->worldX + 20.f, enemy->worldY + enemy->GetIdleHeight() * -0.5f, false, true, false,
                                    false, false, false, false);
            Render::CenterOn(enemy->worldX, enemy->worldY);   // (animated over 0.4 s on the original)
        }
        st = 0x206;
    } else if (st == 0x8c) {
        BuildingHovers::HideWorldDialog(nullptr);
        st = 0x8d;
    }
    if (GameState::PlayerItem* item = GameState::GetFirstItemByID(GameState::GetBeltItemAt(i), false)) {
        // "<item>\n<Activated / Deactivated>". UNVERIFIED: the order of the two texts (the decompile
        // lost SWPrintf's arguments).
        const char32_t* title = item->info->title ? item->info->title : U"";
        std::u32string text;
        if (GameState::GetBeltItemActivationTimeAt(i) == 0) {
            // SoundsManager::PlaySound("ui_activate_slot"): milestone 6.
            text = SWPrintf(0x100, U"%s\n%s", {title, StringTable::GetString("BOTTOM_BAR_ACTIVATED")});
            GameState::SetBeltItemActivationTimeAt(i, (uint32_t)Timer::GetGlobalTime());
        } else {
            // SoundsManager::PlaySound("ui_deactivate_slot"): milestone 6.
            text = SWPrintf(0x100, U"%s\n%s", {title, StringTable::GetString("BOTTOM_BAR_DEACTIVATED")});
            GameState::SetBeltItemActivationTimeAt(i, 0);
        }
        int x, y;
        SlotWorldPoint(i, x, y);
        BuildingHovers::ShowTextHoverWithStyle((float)x, (float)y, text.c_str(), (int)item->info->itemLevel + 0xe, 2.f,
                                               50.f, true, true);
    }
    UpdateContents();
}

// @0x24d790: which of the player's items ride on the belt. The belt holds GetBeltSize pieces; when
// the bound items are more, each slot gets its share (at least an even third).
void UpdateBeltItems() {
    GameState::ClearBeltItems();
    unsigned amount[3] = {}, id[3] = {};
    for (unsigned i = 0; i < GameState::GetBeltSlotCount(); ++i) {
        id[i] = GameState::GetBeltItemAt(i);
        amount[i] = (unsigned)GameState::GetItemAmount(id[i], false);
    }
    unsigned total = amount[0] + amount[1] + amount[2];
    if (total == 0) return;
    float share[3] = {(float)amount[0] / (float)total, (float)amount[1] / (float)total, (float)amount[2] / (float)total};
    unsigned even = GameState::GetBeltSize() / GameState::GetBeltSlotCount();
    if (GameState::GetBeltSize() < total) {
        for (int i = 0; i != 3; ++i) {
            int v = (int)(0.5f + (float)GameState::GetBeltSize() * share[i]);
            if (v >= (int)even) amount[i] = (unsigned)v;
        }
        for (unsigned k = 0; amount[0] + amount[1] + amount[2] > GameState::GetBeltSize(); ++k)
            if (even < amount[k % 3]) --amount[k % 3];
    }
    for (unsigned k = 0; GameState::PlayerItem* it = GameState::EnumItems(k); ++k) {
        for (int slot = 0; slot != 3; ++slot) {
            if (id[slot] != it->id || amount[slot] == 0) continue;
            GameState::AddBeltItem(it->uniqueId);
            --amount[slot];
            break;
        }
    }
}

// @0x24dc20
void UpdateContents() {
    if (!g_root || !g_root->visibleSelf) return;
    if (!EntityManager::GetPlayer()) return;
    if (!GameState::GetBeltItemMode()) {
        g_barName->SetText(StringTable::GetString("SQ_PLAYER"));
        g_barSize->SetVisibility(false);
        g_inventoryIcon->SetTexture(IconManager::GetIcon("bottom_bar_inventory", false), true);
    } else {
        // With a belt item worn: how full the belt is, and the belt's own picture on the button.
        int carried = 0;
        for (unsigned i = 0; i < GameState::GetBeltSlotCount(); ++i)
            carried += GameState::GetItemAmount(GameState::GetBeltItemAt(i), true);
        g_barName->SetText(StringTable::GetString("ITEM_TYPE_BELT"));
        g_barSize->SetVisibility(true);
        g_barSize->SetText(ToWideString(U"%d/%d", {carried, (int)GameState::GetBeltSize()}));
        if (GameState::PlayerItem* belt = GameState::GetItemAt(8))
            g_inventoryIcon->SetTexture(belt->info->GetIcon(), true);
    }
    const unsigned slots = GameState::GetBeltSlotCount();
    g_bg2Slots->noInput = true;
    g_bgRight->noInput = true;
    g_bg2Slots->SetVisibility(slots != 3);
    g_bgRight->SetVisibility(slots == 3);
    g_barName->SetPosition(g_barNameX - (slots == 3 ? g_slots[0].holder->w / 2 : 0), g_barName->y);
    for (unsigned i = 0; i != 3; ++i) {
        Shared::BottomSlotInfo& s = g_slots[i];
        s.holder->SetVisibility(true);
        s.bgGreen->SetVisibility(false);
        s.bgRed->SetVisibility(false);
        s.bgSimple->SetVisibility(false);
        s.bgDisabled->SetVisibility(false);
        s.quantity->SetVisibility(false);
        s.quantityActive->SetVisibility(false);
        s.quantityNone->SetVisibility(false);
        Items::ItemInfo* info = i < slots ? Items::GetItemInfo(GameState::GetBeltItemAt(i)) : nullptr;
        if (info) {
            s.bgSimple->SetVisibility(true);
            s.wingsEmpty->SetVisibility(false);
            s.soldierFine->SetVisibility(true);
            s.iconHolder->SetVisibility(false);
            s.soldierFine->SetTexture(info->GetIcon(), true);
            int n = GameState::GetItemAmount(info->id, true);
            if (n == 0) {
                // None left: red (the belt item mode: a plain 0).
                if (!GameState::GetBeltItemMode()) {
                    s.bgRed->SetVisibility(true);
                    s.quantityNone->SetVisibility(true);
                    s.quantityNone->SetText(ToWideString(0));
                } else {
                    s.quantityActive->SetVisibility(true);
                    s.quantityActive->SetText(ToWideString(0));
                }
                s.progress->SetVisibility(false);
            } else if (!info->CanBeActivated() || GameState::GetBeltItemActivationTimeAt(i) == 0) {
                s.quantityActive->SetVisibility(true);
                s.quantityActive->SetText(ToWideString(n));
                s.progress->SetVisibility(false);
            } else {
                // An orb that is switched on: green, with its charge bar.
                s.bgSimple->SetVisibility(false);
                s.bgGreen->SetVisibility(true);
                s.quantity->SetVisibility(true);
                s.quantity->SetText(ToWideString(n));
                s.progress->SetVisibility(true);
            }
            s.lockIcon->SetVisibility(false);
        } else if (i < slots) {
            if (GameState::GetBeltItemAt(i) != 0) GameState::BindBeltItemTo(i, 0);
            s.bgSimple->SetVisibility(true);
            s.wingsEmpty->SetVisibility(true);
            s.soldierFine->SetVisibility(false);
            s.iconHolder->SetVisibility(true);
            s.progress->SetVisibility(false);
            s.lockIcon->SetVisibility(false);
        } else if (i < 2) {
            s.bgSimple->SetVisibility(true);
            s.wingsEmpty->SetVisibility(true);
            s.soldierFine->SetVisibility(false);
            s.iconHolder->SetVisibility(false);
            s.progress->SetVisibility(false);
        } else {
            s.holder->SetVisibility(false);
        }
        // (The slot an item is flying into, MoveIntoBelt, keeps its picture hidden: with the
        // pick-up animation.)
        if (s.progress->visibleSelf) {
            if (GameState::PlayerItem* it = GameState::GetFirstItemByID(GameState::GetBeltItemAt(i), true))
                s.progressValue = (float)(int)it->f0c / (float)it->info->GetUseCount();
        }
    }
    // UNVERIFIED (milestone 4h): the second tutorial's belt steps (0x95: item 0x88 is bound to
    // slot 1 and the arrow points at it, 0x96; 0x8b: the arrow at slot 0, 0x8c).
    g_craftText->SetText(StringTable::GetString("CRAFT_ITEM_TITLE"));
    g_craftIcon->SetTexture(IconManager::GetIcon("tab_inv_acc_craft", false), true);
    g_craftLock->SetVisibility(!GameState::TaskCompleted((unsigned)Setting("craft_unlock_task").GetInt()));
    const bool craftOn = !GameState::IsBossCombatActive() && !g_craftLock->visibleSelf;
    g_craftIcon->SetEnabled(craftOn);
    g_craftBg->SetEnabled(craftOn);
}

// @0x24f744. UNVERIFIED (milestone 5): the profession point's flight to the crafting bar
// (ShowProfessionPoint), the fury flash (RunRageAnimation, with the spells), an item dragged out
// of the belt, the raised window's timer (RaiseWindow) and the row of running buffs
// (Shared::BuffHolderManager).
void Update(float dt) {
    if (!g_root) return;
    if (g_hover->shownHover) {
        float z = PopupWindow::IsVisible() ? g_root->z - 0.01f : 0.001f;
        if (z != g_hover->GetZ()) {
            g_hover->SetZ(z);
            Render::SortRenderLayer(0xe, 1);
        }
        if (WindowManager::GetShownWindowCount() != 0 && GameState::secondTutorial == 0x100) g_hover->Hide();
    }
    for (int i = 0; i != 3; ++i) {
        Shared::BottomSlotInfo& s = g_slots[i];
        if (!s.progress->visibleSelf) continue;
        Glide& g = g_glide[i];
        if (!g.started) g.shown = s.progressValue;
        g.started = true;
        g.time += dt;
        if (g.time > 0.0333333f) {
            g.time = 0.f;
            g.shown = g.shown - (s.progressValue - g.shown) * -0.133975f;
        }
        s.SetFillPercent(g.shown);
    }
    // UNVERIFIED (milestone 4h): the second tutorial's arrows at the inventory button (0xa3) and
    // at slot 0 (0x86).
    if (Timer::GetGlobalTime() != g_lastSecond && GameState::secondTutorial != 0x95) {
        g_lastSecond = Timer::GetGlobalTime();
        UpdateContents();
    }
}

}  // namespace BeltBarWindow
