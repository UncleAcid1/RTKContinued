// BeltBarWindow: the inventory belt at the bottom right (inventory button, item slots, Crafting
// button) and the crafting progress bar. Port of BeltBarWindow (libkingdom.so 5.11),
// 0x24c63c..0x251694.
#include <cstdio>

#include "engine/IconManager.h"
#include "engine/Render.h"
#include "engine/Text.h"
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
#include "game/Professions.h"
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
// The crafting bar that slides out when a profession point is gained (ShowProfessionPoint): the
// point's picture flies into the bar's icon, the points count up, the bar slides away.
GUI::ClipRect g_craftPanelClip = {0, 0, 0, 0};   // 0x617d88, set by OnCraftPanelMove; nothing reads it
bool g_craftActive = false;               // 0x617d80 the bar is out
float g_craftTimer = 0.f;                 // 0x617d84 seconds since the last point
Professions::Profession* g_craftProfession = nullptr;   // 0x617dc8
Professions::ProfessionLevel* g_craftNext = nullptr;    // 0x617dd8 the next rank (else the current one)
uint32_t g_craftNeed = 0;                 // 0x617ddc the points that rank needs
uint32_t g_craftHave = 0;                 // 0x617de0 the points before this one
Render::Sprite* g_craftSprite = nullptr;  // 0x617dec the flying picture
float g_craftFromX = 0.f, g_craftFromY = 0.f;   // 0x617df0 0x617df4 where it starts (screen)
ItemHoverWindow* g_hover = nullptr;       // 0x617c14 the card of the tapped slot's item
int g_arrowCallback = 0;                  // 0x617dc4 the id of the arrow callback that points at a slot
int g_barNameX = 0;                       // 0x617e08 text_bar_name's laid-out x
Shared::BuffHolderManager* g_buffs = nullptr;   // 0x617e20 the running potions above the belt
int g_lastSecond = 0;                     // 0x617e24
// The flying picture (an item icon, or the fury potion's in the second tutorial) that goes from a
// slot to a point: set by RunRageAnimation, moved by Update.
Render::Sprite* g_flySprite = nullptr;    // 0x617d98
bool g_flyActive = false;                 // 0x617d9c
float g_flyTime = 0.f;                    // 0x617da0
float g_flyFromX = 0.f, g_flyFromY = 0.f; // 0x617da4 0x617da8
float g_flyToX = 0.f, g_flyToY = 0.f;     // 0x617dac 0x617db0
float g_flyDuration = 1.f;                // 0x60f0c4 seconds
int g_flySlot = -1;                       // 0x60f0d0 the slot whose icon is hidden while it flies
// The inventory asked for while the belt was sliding (ShowInventory): its arrow comes once the
// belt is in, with the button locked as the only thing to tap if lockInventory.
bool g_lockInventory = false;             // 0x617db4
bool g_inventoryPending = false;          // 0x617dbc
// RaiseWindow: the belt drawn above a big window for a while, then back at its own depth.
float g_raisedZ = 0.f;                    // 0x617dd0
float g_ownZ = 0.f;                       // 0x617dcc
float g_raiseTime = 0.f;                  // 0x617dd4 seconds left
// A belt item dragged out (with the character window open): pressed, picked up, where.
bool g_dragPressed = false;               // 0x617e28
bool g_dragging = false;                  // 0x617e0c
int g_dragX = 0, g_dragY = 0;             // 0x617e30 0x617e2c
// Per slot (BottomSlotInfo +0x58..+0x60): the charge bar glides to progressValue.
struct Glide { bool started = false; float shown = 0.f, time = 0.f; } g_glide[3];

// The world point under a slot's top middle (the text popups rise from there).
void SlotWorldPoint(unsigned i, int& x, int& y) {
    Window* h = g_slots[i].holder;
    x = g_root->x + h->x + h->w / 2;
    y = g_root->y + h->y;
    Map::MouseCoordinatesToWorld(x, y);
}

// @0x24cf9c: the bar keeps to the screen left of the 30 px edge (a rectangle in the bar's own
// coordinates).
void OnCraftPanelMove() {
    const int w = GUI::ScreenWidth(), h = GUI::ScreenHeight();
    g_craftPanelClip.left = -g_craftBar->x;
    g_craftPanelClip.top = -g_craftBar->y;
    g_craftPanelClip.bottom = h - g_craftBar->y;
    g_craftPanelClip.right = (w - (int)(GUI::GetHudScaleFactor() * 30.f)) - g_craftBar->x;
    g_craftBar->UpdatePosition();
}

// @0x24d038: a tap on the bar opens the profession's recipes.
void OnMovingCraftPanel() {
    if (!g_craftProfession) return;
    CraftItemWindow::Show();
    CraftItemWindow::OnTab(0);
    CraftItemWindow::OnListItem(g_craftProfession->tab - 1);
}

// @0x24d074: the Crafting button; locked until "craft_unlock_task" is done.
void OnCrafting() {
    if (GameState::IsBossCombatActive()) return;
    if (!GameState::TaskCompleted((unsigned)Setting("craft_unlock_task").GetInt())) {
        PopupWindow::Show(StringTable::GetString("CRAFTING_LOCKED_HINT"), PopupWindow::Hide, nullptr, nullptr);
        return;
    }
    CraftItemWindow::Show();
}

// The bar's rank text and the points' fill, for `points` (a fraction while they count up).
void SetCraftBarText(float points) {
    const Professions::Profession* name = g_craftProfession;
    const char32_t* title = g_craftNext ? g_craftNext->name : name->title;
    const std::u32string text =
        SWPrintf(0x80, U"%s - %d/%d", {title, (int)(points + 0.5f), (int)g_craftNeed});
    g_craftBarText2->SetText(text.c_str());
    g_craftBarText->SetText(text.c_str());
}

// @0x24c94c: the character window on its belt tab.
void OnEmptySlot(unsigned) {
    CharacterInfoWindow::Show();
    CharacterInfoWindow::ShowInventoryTab(3);
}

// @0x24caa4: the arrow at the inventory button, whose tap opens the character window.
void OnShowInventory() {
    BuildingHovers::SetArrowClickCallback(new GUI::Callback(OnInventory));
    if (g_lockInventory) GUI::SetInteractionObjectLock(g_inventoryClick, nullptr);
    const Window* w = g_inventoryIcon;
    BuildingHovers::ArrowAt((float)(g_root->x + w->x), (float)(w->y + g_root->y - 0xf + w->h / 2), false, true, true,
                            false, true, false, false);
}

// @0x24cb9c
void OnTweenIn() {
    if (!g_inventoryPending) return;
    OnShowInventory();
    g_lockInventory = false;
    g_inventoryPending = false;
}

// @0x24d9e4: none of the slot's item left. Before the shop's quest: "later"; after it the item
// shop opens at the item.
void OnAddMoreSlot(unsigned i) {
    if (GameState::TaskCompleted((unsigned)Setting("shop_unlock_task").GetInt())) {
        ItemShopWindow::Show();
        ItemShopWindow::ShowShopItem(GameState::GetBeltItemAt(i), 0, true, (unsigned)-1);
        return;
    }
    int x, y;
    SlotWorldPoint(i, x, y);
    BuildingHovers::ShowTextHoverWithStyle((float)x, (float)y, StringTable::GetString("BOTTOM_BAR_LATER"), 0xe, 2.f,
                                           50.f, true, true);
}

// @0x24f0e4: a tap on a slot: the item's card (Drink / Turn on) over it; with the character window
// open the window shows the item (none left: the shop, once unlocked). With the item shop open
// nothing happens (so the original's own branch for the open shop, 0x24f3ac, never runs).
void OnSlot(unsigned i) {
    if (ItemShopWindow::IsVisible() || i >= 3) return;
    Items::ItemInfo* info = Items::GetItemInfo(GameState::GetBeltItemAt(i));
    if (!info) {
        OnEmptySlot(i);
        return;
    }
    if (CharacterInfoWindow::IsVisible()) {
        if (GameState::GetItemAmount(info->id, true) != 0) {
            CharacterInfoWindow::ShowInventoryItem(info->id, 0, nullptr);
        } else if (!GameState::TaskCompleted((unsigned)Setting("shop_unlock_task").GetInt())) {
            int x, y;
            SlotWorldPoint(i, x, y);
            BuildingHovers::ShowTextHoverWithStyle((float)x, (float)y, StringTable::GetString("BOTTOM_BAR_LATER"), 0xe,
                                                   2.f, 50.f, true, true);
        } else {
            CharacterInfoWindow::Hide();
            ItemShopWindow::Show();
            ItemShopWindow::ShowShopItem(info->id, 0, true, (unsigned)-1);
        }
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

}  // namespace

WindowManager::FunctionalWindow* Queue() {
    if (!g_queue) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.setZ = SetZ;
        f.hide = Hide;
        f.click = Click;
        f.move = Move;
        g_queue = new WindowManager::FunctionalWindow("BeltBarWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_root && g_root->visibleSelf; }

// @0x24c884
bool HasMovedIntoBelt(int x, int y, uint32_t id) {
    const int rx = x - g_root->x, ry = y - g_root->y;
    for (unsigned k = 0; k < GameState::GetBeltSlotCount(); ++k)
        if (GameState::GetBeltItemAt(k) == id) return false;
    for (unsigned k = 0; k < GameState::GetBeltSlotCount(); ++k) {
        const Window* h = g_slots[k].holder;
        if (h->x < rx && rx < h->w + h->x && h->y < ry && ry < h->h + h->y) return true;
    }
    return false;
}

// @0x24f4fc: the sprite becomes the belt's gliding picture, from where it is into the slot.
void MoveIntoBelt(Render::Sprite* sprite, uint32_t id, unsigned slot) {
    Render::RemoveSprite(g_flySprite);
    g_flySprite = sprite;
    for (unsigned k = 0; k < GameState::GetBeltSlotCount() && slot == (unsigned)-1; ++k) {
        const Window* h = g_slots[k].holder;
        const int cx = (int)(sprite->x + sprite->w * 0.5f - (float)g_root->x);
        if (h->x < cx && cx < h->w + h->x) {
            const int cy = (int)(sprite->y + sprite->h * -0.5f - (float)g_root->y);
            if (h->y < cy) slot = h->h + h->y <= cy ? (unsigned)-1 : k;
        }
    }
    if (slot == (unsigned)-1) {
        Render::RemoveSprite(sprite);
        g_flySprite = nullptr;   // PORT: the original keeps the freed sprite
        return;
    }
    // SoundsManager::PlaySound("ui_equip_belt"): milestone 6.
    GameState::BindBeltItemTo(slot, id);
    Items::ItemInfo* info = Items::GetItemInfo(id);
    if (info && info->CanBeActivated()) GameState::SetBeltItemActivationTimeAt(slot, (uint32_t)Timer::GetGlobalTime());
    g_flyTime = 0.f;
    g_flyDuration = 1.f;
    g_flyFromX = g_flySprite->x;
    g_flyFromY = g_flySprite->y;
    Window* w = g_slots[slot].soldierFine;
    g_flyActive = true;
    g_flyToX = (float)(g_root->x + w->x);
    g_flyToY = (float)(g_root->y + w->y + w->h);
    g_flySprite->h = (float)w->h;
    g_flySprite->w = (float)w->w;
    w->SetVisibility(false);
    g_flySlot = (int)slot;
    UpdateContents();
}

// @0x2507d0: past 10 pixels a pressed slot's item is picked up and follows.
void Move(int x, int y) {
    if (g_dragPressed && !g_dragging) {
        if (std::abs(y - g_dragY) + std::abs(x - g_dragX) < 0xb) return;
        g_dragging = true;
        GUI::ReleaseCapture();
        Window* w = g_slots[g_flySlot].soldierFine;
        if (w->sprite) {
            Render::RemoveSprite(g_flySprite);
            g_flySprite = Render::CreateSprite(w->sprite->tex, 0xf, false, false);
            g_flySprite->screenSpace = true;
            Render::SetShaderType(g_flySprite, 1);
            Render::SetPosition(g_flySprite, (float)(g_root->x + w->x), (float)(w->h + w->y + g_root->y), 0.001f);
            g_flyActive = false;
            g_flySprite->w = (float)w->w;
            g_flySprite->h = (float)w->h;
            g_flyToX = g_flySprite->x;
            g_flyToY = g_flySprite->y;
            w->SetVisibility(false);
        }
    }
    if (!g_dragging) return;
    Render::SetPosition(g_flySprite, (float)(x - g_dragX) + g_flySprite->x, (float)(y - g_dragY) + g_flySprite->y,
                        0.001f);
    g_dragY = y;
    g_dragX = x;
}

// @0x2509ac: over an edit mode only presses on the belt are taken. With the character window open a
// slot's item can be dragged into its grid (else it glides back). Then the tutorials' arrow, the
// crafting bar, the card and the belt.
bool Click(int x, int y, bool pressed) {
    if (!g_root) return false;
    if (BuildingMovement::Activated() || BuildingPlacement::Activated())
        return g_root->GetWindowAtPosition(x, y, false) != nullptr;
    if (g_root->visibleSelf) {
        if (pressed) {
            for (unsigned k = 0; k != 3; ++k) {
                GameState::PlayerItem* item = GameState::GetFirstItemByID(GameState::GetBeltItemAt(k), false);
                if (!CharacterInfoWindow::IsVisible() || !item || !g_slots[k].soldierFine->sprite) continue;
                const Window* h = g_slots[k].holder;
                const int hx = g_root->x + h->x, hy = g_root->y + h->y;
                if (x < hx || x > hx + h->w || y < hy || y > hy + h->h) continue;
                if (g_flySprite) {
                    if (g_flySlot != -1) g_slots[g_flySlot].soldierFine->SetVisibility(true);
                    g_flySlot = -1;
                    g_flySprite = Render::RemoveSprite(g_flySprite);
                    g_flyActive = false;
                }
                g_dragPressed = true;
                g_flySlot = (int)k;
                g_dragX = x;
                g_dragY = y;
                break;
            }
        } else {
            g_dragPressed = false;
            if (g_dragging) {
                g_dragging = false;
                const Render::Sprite* s = g_flySprite;
                if (!CharacterInfoWindow::HasMovedIntoInventory((int)(s->x + s->w * 0.5f), (int)(s->y + s->h * -0.5f))) {
                    // Back to its slot.
                    g_flyTime = 0.f;
                    g_flyDuration = 0.5f;
                    g_flyActive = true;
                    g_flyFromX = g_flySprite->x;
                    g_flyFromY = g_flySprite->y;
                    return true;
                }
                const uint32_t id = GameState::GetBeltItemAt((unsigned)g_flySlot);
                // SoundsManager::PlaySound("ui_unequip_belt"): milestone 6.
                GameState::BindBeltItemTo((unsigned)g_flySlot, 0);
                UpdateContents();
                CharacterInfoWindow::MoveIntoInventory(g_flySprite, id);
                g_flySprite = nullptr;
                return true;
            }
        }
    }
    const int st = GameState::secondTutorial;
    if (((unsigned)(GameState::tutorial - 0x14) < 2 || st == 0x96 || st == 0x87 || st == 0x88 || st == 0xa4 ||
         BuildingHovers::GetArrowClickCallbackID() == g_arrowCallback) &&
        BuildingHovers::ClickOnArrow(x, y, pressed))
        return true;
    if ((g_craftBar && g_craftBar->Click(x, y, pressed, false)) || g_hover->Click(x, y, pressed)) return true;
    return g_root->Click(x, y, pressed, false);
}

// @0x24d0fc: the inventory button. In the second tutorial (0xa4) and the opening one (0x14 / 0x15)
// the window opens at the item to equip. Not while the item shop is open.
void OnInventory() {
    if (ItemShopWindow::IsVisible()) return;
    CharacterInfoWindow::Show();
    if (GameState::secondTutorial == 0xa4) CharacterInfoWindow::ShowInventoryItem(0x6b, 1, nullptr);
    if ((unsigned)(GameState::tutorial - 0x14) < 2) CharacterInfoWindow::ShowInventoryItem(0x5b, 1, nullptr);
}

// @0x24cbcc: the arrow at the inventory button now, or once the belt has slid in.
void ShowInventory(bool lock) {
    if (!g_effect->active) {
        OnShowInventory();
        return;
    }
    g_lockInventory = lock;
    g_inventoryPending = true;
}

// @0x24d16c
void RaiseWindow(float z, float seconds) {
    const float own = g_root->z;
    if (own != z) {
        g_raisedZ = z;
        g_ownZ = own;
        g_root->SetZ(z);
        Render::SortRenderLayer(0xe, 1);
    }
    g_raiseTime = seconds;
}

void GetBeltItemLocation(int& x, int& y, int slot) {
    x = g_root->x + g_slots[slot].holder->x;
    y = g_root->y + g_slots[slot].holder->y;
}

// @0x24c678: where a w wide image goes at the right of the belt's first slot, level with the
// inventory button (the row of running buffs).
void GetAdjustedBuffZoneLocation(int& x, int& y, int w, int) {
    x = g_slots[0].holder->x + g_inventoryIcon->x + g_slots[0].holder->w - w;
    y = g_inventoryIcon->y;
}

// @0x24c960: the second tutorial's fury potion (the icon of belt slot 1) flies from its slot to the
// bottom middle of the screen, where the rage bar rises; the tutorial goes on at its landing (0x9a).
void RunRageAnimation() {
    const Window* icon = g_slots[1].soldierFine;
    g_flySprite = Render::CreateSprite(icon->texture, 0xf, false, false);
    g_flySprite->screenSpace = true;
    Render::SetShaderType(g_flySprite, 1);
    Render::SetPosition(g_flySprite, (float)(g_root->x + icon->x), (float)(icon->h + icon->y + g_root->y), 0.001f);
    g_flyActive = true;
    GameState::secondTutorial = 0x99;
    g_flySprite->w = (float)icon->w;
    g_flySprite->h = (float)icon->h;
    g_flyDuration = 1.f;
    g_flyTime = 0.f;
    g_flyFromX = g_flySprite->x;
    g_flyFromY = g_flySprite->y;
    g_flyToX = ((float)GUI::ScreenWidth() - (float)icon->w) * 0.5f;
    g_flyToY = (float)GUI::ScreenHeight();
}

// @0x24cc44: the arrow points at the slot, its tap drinks the slot's potion.
void ArrowAtSlot(unsigned slot) {
    g_arrowCallback = BuildingHovers::SetArrowClickCallback(new GUI::Callback([slot] { OnUseSlot(slot); }));
    const Window* h = g_slots[slot].holder;
    BuildingHovers::ArrowAt((float)(g_root->x + h->x + h->w / 2), (float)(g_root->y + h->y), false, false, false,
                            false, true, false, false);
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
    // OnTweenMove @0x24c674 is empty.
    g_effect = GUI::CreateMovementEffect(g_root, [] {}, OnTweenIn);
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
    g_inventoryClick->SetOnClick(OnInventory);
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
    g_craft->SetOnClick(OnCrafting);
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
        g_craftBar->SetOnClick(OnMovingCraftPanel);
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
        g_craftActive = false;
        g_craftPanelClip = {0, 0, GUI::ScreenWidth() - (int)(GUI::GetHudScaleFactor() * 30.f), GUI::ScreenHeight()};
        g_craftEffect = GUI::CreateMovementEffect(g_craftBar, OnCraftPanelMove);
        g_craftEffect->blocksInput = false;
    }
    Queue()->fn.update = Update;
    Queue()->fn.show = Show;
    g_buffs = new Shared::BuffHolderManager(0x18, 2, -1, 0);
}

// @0x250444: slide in from the right
void Show() {
    if (!g_root || !GameState::IsPlayerCity()) return;
    if (g_effect->active && g_effect->hideAtEnd) g_root->SetVisibility(false);
    if (GameState::TutorialStep() <= 0x13 || BuildingPlacement::Activated()) return;
    g_arrowCallback = 0;
    Queue()->MoveWindowOnTop(true);
    const int W = GUI::ScreenWidth(), H = GUI::ScreenHeight();
    if (!g_root->visibleSelf) {
        int y = H - g_root->h;
        g_effect->Animate(W, y, (W + g_offsetX) - g_root->w, y + g_offsetY, true);
        g_root->SetVisibility(true);
        UpdateContents();
    }
    g_craftLock->SetVisibility(!GameState::TaskCompleted((unsigned)Setting("craft_unlock_task").GetInt()));
    const bool craftOn = !GameState::IsBossCombatActive() && !g_craftLock->visibleSelf;
    g_craftIcon->SetEnabled(craftOn);
    g_craftBg->SetEnabled(craftOn);
    g_buffs->SetVisibility(true);
    g_root->PreloadTextures();   // GUI::PreloadWindow
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
    g_buffs->SetZ(z);
    // Over the open character window the belt goes in front of it (an item can be dragged in).
    if (CharacterInfoWindow::CanRaiseBelt()) z = CharacterInfoWindow::GetZ() - 0.085f;
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
            Render::CenterOn(enemy->worldX, enemy->worldY, true);
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
    // The second tutorial's belt steps: at 0x95 item 0x88 goes to slot 1 (once no item is being
    // moved) and the arrow points at it (0x96); at 0x8b the arrow points at slot 0 (0x8c).
    int& st = GameState::secondTutorial;
    auto pointAtSlot = [](unsigned i) {
        BuildingHovers::SetArrowClickCallback(new GUI::Callback([i] { OnSlot(i); }));
        Window* h = g_slots[i].holder;
        BuildingHovers::ArrowAt((float)(g_root->x + h->x + h->w / 2), (float)(g_root->y + h->y), false, false, false,
                                false, true, true, false);
        GUI::SetInteractionObjectLock(h, nullptr);
    };
    if (st == 0x95 && !BuildingHovers::IsItemMoving()) {
        GameState::BindBeltItemTo(1, 0x88);
        UpdateBeltItems();
        pointAtSlot(1);
        st = 0x96;
    } else if (st == 0x8b) {
        pointAtSlot(0);
        st = 0x8c;
    }
    g_craftText->SetText(StringTable::GetString("CRAFT_ITEM_TITLE"));
    g_craftIcon->SetTexture(IconManager::GetIcon("tab_inv_acc_craft", false), true);
    g_craftLock->SetVisibility(!GameState::TaskCompleted((unsigned)Setting("craft_unlock_task").GetInt()));
    const bool craftOn = !GameState::IsBossCombatActive() && !g_craftLock->visibleSelf;
    g_craftIcon->SetEnabled(craftOn);
    g_craftBg->SetEnabled(craftOn);
}

// The bar's icon: where the point's picture lands (screen coordinates).
void CraftIconTarget(float& x, float& y) {
    x = (float)(g_craft->x + g_root->x + g_craftProfIcon->x + (int)(g_root->scale * 30.f) - g_craftBar->w);
    y = (float)(g_craft->y + g_root->y + g_craftProfIcon->y + g_craftProfIcon->h + (g_craft->h - g_craftBar->h) / 2);
}

// @0x24f744 (a part of Update): for 0.7 s the point's picture flies from where it was gained to
// the bar's icon in an arc; then it swells (0.25 s) and shrinks (0.25 s) there, the icon shows
// again and the points count up; after 2 s the bar slides away.
void UpdateCraftBar(float dt) {
    if (!g_craftActive) return;
    g_craftTimer += dt;
    const float t = g_craftTimer;
    float tx, ty;
    CraftIconTarget(tx, ty);
    if (t <= 0.7f) {
        const float f = t / 0.7f;
        float k = f, x = g_craftFromX + (tx - g_craftFromX) * f;
        if (f > 1.f) {
            x = g_craftFromX + (tx - g_craftFromX);
            k = 1.f;
        }
        const float y = g_craftFromY + k * (ty - g_craftFromY) + sinf(f * 3.14159f) * -100.f;
        if (g_craftSprite) Render::SetPosition(g_craftSprite, x, y, 0.0001f);
        return;
    }
    const float u = t - 0.7f;
    auto grow = [&](float k) {
        Render::SetPosition(g_craftSprite, tx + k * -10.f, ty + k * 10.f, 0.001f);
        Render::SetAlpha(g_craftSprite, k + 1.f);
        g_craftSprite->h = (float)g_craftSprite->tex->h + k * 20.f;
        g_craftSprite->w = (float)g_craftSprite->tex->w + k * 20.f;
    };
    bool hideIcon = true;
    if (u < 0.25f) {
        if (g_craftSprite) grow(u);
    } else if (u < 0.5f) {
        if (g_craftSprite) grow(0.25f - (u - 0.25f));
    } else {
        hideIcon = false;
        if (g_craftSprite) {
            g_craftSprite = Render::RemoveSprite(g_craftSprite);
            g_craftProfIcon->SetVisibility(true);
        }
    }
    if (hideIcon) g_craftProfIcon->SetVisibility(false);
    const float f = u <= 1.f ? u : 1.f;
    const float have = (float)g_craftHave;
    const float points = have + f * ((float)(g_craftHave + 1) - have);
    SetCraftBarText(points);
    // PORT: a rank with no points needed (a profession without ranks) leaves the fill empty.
    g_craftClip.right = g_craftNeed ? (int)((float)g_craftClip.left + ((float)g_craftProgress->w * points) / (float)g_craftNeed)
                                    : g_craftClip.left;
    g_craftBar->UpdatePosition();
    if (u >= 2.f) {
        const int y = g_craft->y + g_root->y + (g_craft->h - g_craftBar->h) / 2;
        const int x = g_craft->x + g_root->x + (int)(g_root->scale * 30.f) - g_craftBar->w;
        g_craftActive = false;
        g_craftEffect->Animate(x, y, GUI::ScreenWidth(), y, false);
    }
}

// @0x24f744. UNVERIFIED (milestone 5): an item dragged out of the belt.
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
    g_buffs->SetPosition(g_root->x + g_inventoryIcon->x + g_inventoryIcon->w, g_root->y);
    g_buffs->Update(dt);
    if (g_raiseTime > 0.f) {
        g_raiseTime -= dt;
        if (g_raiseTime <= 0.f) {
            g_root->SetZ(g_ownZ);
            Render::SortRenderLayer(0xe, 1);
        }
    }
    UpdateCraftBar(dt);
    if (g_flyActive && g_flySprite) {
        const float t = g_flyTime / g_flyDuration;
        if (t < 1.f) {
            if (GameState::secondTutorial == 0x99) {
                Render::SetPosition(g_flySprite, g_flyFromX + t * (g_flyToX - g_flyFromX),
                                    g_flyFromY + t * (g_flyToY - g_flyFromY), 0.001f);
            } else {
                // a quarter circle: fast at first, slowing into the target
                const float k = std::sqrt(1.f - (t - 1.f) * (t - 1.f));
                Render::SetPosition(g_flySprite, g_flyFromX + (g_flyToX - g_flyFromX) * k,
                                    g_flyFromY + (g_flyToY - g_flyFromY) * k, 0.001f);
            }
        } else {
            g_flySprite = Render::RemoveSprite(g_flySprite);
            if (g_flySlot != -1) g_slots[g_flySlot].soldierFine->SetVisibility(true);
            g_flySlot = -1;
            if (GameState::secondTutorial == 0x88) {
                CharacterInfoWindow::Hide();
                GUI::SetInteractionLock(false);
                GameState::secondTutorial = 0x206;
            } else if (GameState::secondTutorial == 0x99) {
                GameState::secondTutorial = 0x9a;
            }
            g_flyActive = false;
        }
        g_flyTime += dt;
    }
    // The second tutorial's arrows at the inventory button (0xa3) and at slot 0 (0x86).
    int& st = GameState::secondTutorial;
    if (st == 0xa3) {
        BuildingHovers::SetArrowClickCallback(new GUI::Callback(OnInventory));
        GUI::SetInteractionObjectLock(g_inventoryClick, nullptr);
        Window* w = g_inventoryIcon;
        BuildingHovers::ArrowAt((float)(g_root->x + w->x), (float)(w->y + g_root->y - 0xf + w->h / 2), false, true,
                                true, false, true, false, false);
        st = 0xa4;
    } else if (st == 0x86) {
        BuildingHovers::SetArrowClickCallback(new GUI::Callback([] { OnSlot(0); }));
        Window* h = g_slots[0].holder;
        GUI::SetInteractionObjectLock(h, nullptr);
        BuildingHovers::ArrowAt((float)(g_root->x + h->x + h->w / 2), (float)(g_root->y + h->y), false, false, false,
                                false, true, false, false);
        BuildingHovers::PreventArrowAutoHide();
        st = 0x87;
    }
    if (Timer::GetGlobalTime() != g_lastSecond && st != 0x95) {
        g_lastSecond = Timer::GetGlobalTime();
        UpdateContents();
    }
}

// @0x24d264: a profession point was gained at the screen point (x, y): the bar comes out (or its
// timer is set back) and the point's picture flies to it; `points` are those before this one.
void ShowProfessionPoint(float x, float y, Professions::Profession* p, uint32_t points) {
    if (!p->icon) return;
    if (p->id == 3 && !GameState::TaskCompleted(0x5a8)) return;
    g_craftProfession = p;
    g_craftHave = points;
    const uint32_t level = Professions::GetPlayerProfessionLevel(p->id);
    g_craftNext = Professions::GetProfessionLevel(p->id, level + 1);
    if (!g_craftNext) g_craftNext = Professions::GetProfessionLevel(p->id, level);
    g_craftNeed = g_craftNext ? g_craftNext->points : points;
    g_craftProf->SetText(p->title);
    g_craftProfIcon->SetTexture(p->icon, true);
    g_craftClip.right = g_craftNeed ? (int)(((uint32_t)g_craftProgress->w * points) / g_craftNeed) + g_craftClip.left
                                    : g_craftClip.left;
    SetCraftBarText((float)points);
    g_craftBar->UpdatePosition();
    if (!g_craftActive) {
        g_craftTimer = 0.f;
        g_craftActive = true;
        g_craftBar->SetVisibility(true);
        g_craftBar->SetPosition(GUI::ScreenWidth(), g_craft->y + g_root->y);
        g_craftEffect->duration = 0.5f;
        const int barY = g_craft->y + g_root->y + (g_craft->h - g_craftBar->h) / 2;
        g_craftEffect->Animate(GUI::ScreenWidth(), barY,
                               (g_craft->x + g_root->x - g_craftBar->w) + (int)(g_root->scale * 30.f), barY, true);
    } else if (g_craftTimer > 0.7f) {
        g_craftTimer = 0.7f;
    }
    g_craftSprite = Render::RemoveSprite(g_craftSprite);
    g_craftSprite = Render::CreateSprite(p->icon, 0xf, false, false);
    g_craftSprite->screenSpace = true;
    Render::SetShaderType(g_craftSprite, 1);
    const float h = (float)p->icon->h;
    Render::SetPosition(g_craftSprite, x, y - h, 0.001f);
    g_craftFromY = y - h;
    g_craftFromX = x;
}

}  // namespace BeltBarWindow
