// FoundItemsWindow: the "items found" strip (Endscreen_item_holder.xml) that slides up from the
// bottom of the screen over a dark band when a map, a task, a campaign, a raid, a boss fight or a
// chest hands out loot: up to ten item / resource cells in a scrolling strip (tap one for its
// ItemHoverWindow), and an Ok button (or "tap to continue"), with Go to shop / Go to tavern for the
// arena. The owners collect the loot here (AddCollectedItem / AddCollectedResource, or one of the
// SetParameters forms), show the window, and give the loot with GiveCollectedToPlayer.
// Port of FoundItemsWindow (libkingdom.so 5.11), 0x2b8de4..0x2bd35c. Statics at 0x61f478.
//
// Over the PvP arena's outro (PlayerBattleInOutWindow) it has no band and its buttons are the
// arena's: after a win "Continue" (back from the fight), after a loss "Attack again" (the fight
// restarts), "Continue" and "Restock".
// UNVERIFIED (later milestones):
//  - PlayerWeakWindow (5: the warning before a rematch with a weak army or belt);
//  - SetParameters(Locations::LocationInfo*, ...) (0x2bd21c) comes with the global map's windows (5);
//  - RunPostAction's windows (item shop, hire troops: 5; level info: 5) and OnClose's owners
//    (raid / campaign / boss / chest windows) are named in comments only.
// PORT (online removed): the share ticks (Shared::ShareButtonsHolder, the Facebook / Twitter
// toggles of SettingsWindow) stay hidden and no wall posts are made (OnShare); Ok only closes.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "engine/IconManager.h"
#include "engine/Render.h"
#include "engine/Resources.h"
#include "engine/Text.h"
#include "game/Tasks.h"
#include "game/BuildingHovers.h"
#include "game/Combat.h"
#include "game/PvP.h"
#include "game/Squad.h"
#include "game/Entity.h"
#include "game/EntityManager.h"
#include "game/GameState.h"
#include "hud/HUD.h"
#include "game/Items.h"
#include "game/Locations.h"
#include "game/Professions.h"
#include "game/Setting.h"
#include "game/StringTable.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/ContentScroller.h"
#include "hud/ItemHoverWindow.h"
#include "windows/Windows.h"

namespace FoundItemsWindow {
namespace {

using GUI::Button;
using GUI::Textfield;
using GUI::Window;

// One cell of the strip (0x2c bytes), "item_holder_endscreen_items.reward_item_%02d".
struct ItemInfo {
    Window* root = nullptr;          // +0x00
    Window* locked = nullptr;        // +0x04 item_class_locked
    Window* cls[5] = {};             // +0x08 item_class_01..05: the frame of the item level
    Window* icon = nullptr;          // +0x1c item_icon_holder_item (takes a depth slot)
    Textfield* quantity = nullptr;   // +0x20 text_quantity
    uint32_t item = 0;               // +0x24 the item shown (0: a resource)
    int resource = 0xb;              // +0x28 the resource shown (0xb: none)
    void LoadFrom(Window* w, const char* name);
};

// One line of loot (0x10 bytes): an item (type 0xb) or a resource.
struct CollectedInfo {
    int type = 0;                    // +0x00 resource type, 0xb for an item
    uint32_t id = 0;                 // +0x04 the item
    int amount = 0;                  // +0x08
    unsigned priority = 0;           // +0x0c the strip is sorted by it, highest first
};

WindowManager::FunctionalWindow* g_queue = nullptr;   // FoundItemsWindow::wnd
Textfield* g_header = nullptr;         // 0x61f478 text_01
bool g_sharing = false;                // 0x61f47c SetSharing (a chest's share)
bool g_hideOk = false;                 // 0x61f47d HideOk: "tap to continue" instead of Ok
int g_postAction = 0;                  // 0x61f480 RunPostAction: 1 item shop, 2 tavern, 3 arena, 4 level info
Shared::ContentScroller g_scroller;    // 0x61f484
ItemInfo g_items[10];                  // 0x61f590
std::vector<CollectedInfo> g_collected;   // 0x61f748
Window* g_root = nullptr;              // 0x61f754 Endscreen_item_holder.xml
Render::Sprite* g_dark = nullptr;      // 0x61f758 the dark band behind the window
Render::Sprite* g_goldLine = nullptr;  // 0x61f75c Endscreen_goldline_scaler.png along its top
GUI::MovementEffect* g_effect = nullptr;   // 0x61f760
Render::Texture* g_darkTex = nullptr;  // 0x61f764
Textfield* g_tapText = nullptr;        // 0x61f768 text_tap_to_continue (with a white glow)
float g_pulse = 0.f;                   // 0x61f76c its blink timer
ItemHoverWindow* g_hover = nullptr;    // 0x61f770
int g_offsetY = 0;                     // 0x61f774 how far the window sinks below the screen edge
const void* g_location = nullptr;      // 0x61f778 Locations::LocationInfo* (5; for the share)
const void* g_task = nullptr;          // 0x61f77c Tasks::TaskInfo* (4c; for the share)
uint32_t g_boss = 0;                   // 0x61f780 the boss entity (for the share)
Window* g_shareHolder = nullptr;       // 0x61f784 lvl_up_share_holder (the Ok button)
Window* g_shareIcon = nullptr;         // 0x61f788
Textfield* g_shareText = nullptr;      // 0x61f78c
Window* g_shopHolder = nullptr;        // 0x61f790 lvl_up_shop_holder
Window* g_shopIcon = nullptr;          // 0x61f794
Textfield* g_shopText = nullptr;       // 0x61f798
Window* g_tavernHolder = nullptr;      // 0x61f79c lvl_up_tavern_holder
Window* g_tavernIcon = nullptr;        // 0x61f7a0
Textfield* g_tavernText = nullptr;     // 0x61f7a4
Window* g_social = nullptr;            // 0x61f7a8 ShareButtonsHolder root, lvl_up_share_social_01
Textfield* g_noneText = nullptr;       // 0x61f7bc text_nothing_found
Window* g_tapTextCopy = nullptr;       // 0x61f7c0 the tap text without the glow, over it
int g_centerShift = 0;                 // 0x61f7c4 the strip's shift when it holds fewer than 10
Window* g_left = nullptr;              // 0x61f7c8 button_navigate_left
Window* g_right = nullptr;             // 0x61f7cc button_navigate_right
bool g_weakShown = false;              // 0x61f7d0 (the arena's PlayerWeakWindow was shown once)
Window* g_shareClick = nullptr;        // 0x61f7d4 lvl_up_share_holder.share.clickArea
Window* g_shopClick = nullptr;         // 0x61f7d8
Window* g_tavernClick = nullptr;       // 0x61f7dc
bool g_tablet = false;                 // 0x60f224
float g_scale = 1.f;                   // 0x60f228

// @0x2b8de4 operator<(CollectedInfo, CollectedInfo): higher priority first.
bool ByPriority(const CollectedInfo& a, const CollectedInfo& b) { return b.priority < a.priority; }

// @0x2b9d20
void ItemInfo::LoadFrom(Window* w, const char* name) {
    root = GUI::GetWindowTypedF<Window>(w, "%s", name);
    locked = GUI::GetWindowTypedF<Window>(w, "%s.inventory_item_icon.inventory_item_icon_holder.item_class_locked", name);
    for (int k = 0; k < 5; ++k)
        cls[k] = GUI::GetWindowTypedF<Window>(w, "%s.inventory_item_icon.inventory_item_icon_holder.item_class_%02d", name,
                                              k + 1);
    icon = GUI::GetWindowTypedF<Window>(w, "%s.inventory_item_icon.inventory_item_icon_holder.item_icon_holder_item",
                                        name);
    icon->takesZ = true;
    quantity = GUI::GetWindowTypedF<Textfield>(w, "%s.inventory_item_icon.text_quantity", name);
    item = 0;
    resource = 0xb;
}

float ScreenH() { return (float)GUI::ScreenHeight(); }
float ScreenW() { return (float)GUI::ScreenWidth(); }

// @0x2b8e8c: the arrows move the strip one cell (left: towards the start).
void OnPositionChange(bool right) {
    if (!right) g_scroller.bounce += (float)g_scroller.itemSize;
    else g_scroller.bounce -= (float)g_scroller.itemSize;
}

float ZRange() { return 0.016f; }   // @0x2b8edc

// @0x2b92c4
void OnTweenOut() {
    g_root->SetVisibility(false);
    Render::SetVisibility(g_dark, false);
    Render::SetVisibility(g_goldLine, false);
}

// @0x2b9308: the band settles at the bottom (or below the screen once hidden).
void OnTweenIn() {
    if (!g_effect->hideAtEnd) Render::SetPosition(g_dark, 0.f, ScreenH(), 0.028f);
    else Render::SetPosition(g_dark, 0.f, ScreenH() + g_dark->h, 0.028f);
    Render::SetPosition(g_goldLine, 0.f, (g_dark->y - g_dark->h) + 5.f, 0.0279f);
}

// @0x2b93d0: the band follows the window.
void OnTweenMove() {
    if (!g_effect->hideAtEnd) Render::SetPosition(g_dark, 0.f, ScreenH() + (1.f - g_effect->t) * g_dark->h, 0.028f);
    else Render::SetPosition(g_dark, 0.f, ScreenH() + g_dark->h * g_effect->t, 0.028f);
    Render::SetPosition(g_goldLine, 0.f, (g_dark->y - g_dark->h) + 5.f, 0.0279f);
    Render::SortRenderLayer(Render::kLayerGUI, 1);
}

// @0x2b94c0: the strip scrolls, the band's texture comes back after a context loss, and the tap
// text blinks.
void Update(float dt) {
    if (!g_queue->shown || !g_dark) return;
    g_scroller.Update(dt);
    if (!g_darkTex || g_darkTex->glId == 0) {
        g_darkTex = Render::CreateSolidFillTexture(0.f, 0.f, 0.f, 1.f);
        Render::SetTexture(g_dark, g_darkTex);
    }
    if (!g_tapText->visibleSelf) return;
    g_pulse += dt;
    g_tapText->SetAlpha(std::fabs(std::cos(g_pulse * 3.f) * 0.7f), false);
}

// @0x2b95bc: the cell's popup, pointing at it from below.
void OnItem(unsigned i) {
    // UNVERIFIED (milestone 5): SoundsManager::PlaySound("ui_click", 1).
    const ItemInfo& c = g_items[i];
    g_hover->Show();
    if (c.item == 0) {
        if (c.resource != 0xb) g_hover->SetResource(c.resource, false);
    } else {
        g_hover->SetItem(StringTable::GetString("YOU_HAVE_FOUND"), nullptr, Items::GetItemInfo(c.item), false, false,
                         nullptr, true, false, false, false);
    }
    Window* r = c.root;
    int x = g_root->x + r->x, y = g_root->y + r->y;
    g_hover->SetTargetRect(x, y, x + r->w, y + r->h, ItemHoverWindow::kS);
    g_hover->RecomputePosition();
    g_hover->SetZ(0.001f);
    Render::SortRenderLayer(Render::kLayerGUI, 1);
}

// @0x2b9984: over the PvP arena's outro the button is "Restock" (after a loss).
void OnTavern() {
    if (!PlayerBattleInOutWindow::IsVisible()) {
        OnClose();
        g_postAction = 2;
        return;
    }
    if (PlayerBattleInOutWindow::EnemyDefeated()) return;
    // UNVERIFIED (milestone 5): PlayerWeakWindow::Show.
}

// @0x2b9b90
void Move(int x, int y) { g_scroller.Move(x, y); }

}  // namespace

// @0x2b9bac: with "tap to continue", a tap anywhere (not on the popup) closes.
bool Click(int x, int y, bool pressed) {
    if ((g_root->visibleSelf && g_scroller.Click(x, y, pressed, g_root)) || g_hover->Click(x, y, pressed)) return true;
    bool r = g_root->Click(x, y, pressed, false);
    if (g_hideOk && !pressed && !g_hover->shownHover) OnClose();
    return r;
}

namespace {

// @0x2b9c94
void Deinit() {
    GUI::RemoveMovementEffect(g_effect);
    g_effect = nullptr;
    delete g_root;
    g_root = nullptr;
    delete g_hover;
    g_hover = nullptr;
    g_dark = Render::RemoveSprite(g_dark);
    g_goldLine = Render::RemoveSprite(g_goldLine);
}

// @0x2bab88 OnTwitterToggle / @0x2bab98 OnFacebookToggle: PORT (online removed), see the top.

// @0x2bc3e4: Back closes the popup, else the window (as Ok).
bool OnBack() {
    if (!g_queue->shown) return false;
    if (!g_hover->shownHover) {
        OnShare();
        return true;
    }
    g_hover->Hide();
    return true;
}

// The tail of each SetParameters: the strip sorted, from its start, under the title.
void FinishParameters(const char32_t* title) {
    std::stable_sort(g_collected.begin(), g_collected.end(), ByPriority);
    g_scroller.firstIndex = 0;
    g_header->SetText(title);
}

}  // namespace

// @0x2b99c4: the band is 30% of the screen; a window lower than that sinks into it halfway.
void SetZ(float z) {
    if (!g_queue->shown) return;
    if (PlayerBattleInOutWindow::IsVisible()) z = PlayerBattleInOutWindow::GetZ() + 0.0002f;
    float h = ScreenH();
    g_dark->h = h * 0.3f;
    g_dark->w = ScreenW();
    Render::SetPosition(g_dark, 0.f, h, 0.028f);
    g_goldLine->w = ScreenW();
    Render::SetPosition(g_goldLine, 0.f, (g_dark->y - g_dark->h) + 5.f, 0.0279f);
    float rh = (float)g_root->h;
    g_offsetY = rh < h * 0.3f ? (int)(rh + h * -0.3f) / 2 : 0;
    g_root->SetZ(z);
    int y = !g_effect->hideAtEnd ? g_offsetY + GUI::ScreenHeight() - g_root->h : GUI::ScreenHeight();
    g_root->SetPosition(g_effect->GetCenteredX(), y);
}

WindowManager::FunctionalWindow* Queue() {
    if (!g_queue) {   // FUN_002b915c (static init)
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.deinit = Deinit;
        f.click = Click;
        f.setZ = SetZ;
        f.hide = Hide;
        f.move = Move;
        g_queue = new WindowManager::FunctionalWindow("FoundItemsWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_queue->shown; }                                // @0x2b8dfc
void SetHeader(const char32_t* text) { g_header->SetText(text); }          // @0x2b8e10
void SetSharing(bool) { g_sharing = true; }                                // @0x2b8e40 (sets it whatever the argument)
void HideOk() { g_hideOk = true; }                                         // @0x2b8e58
bool HasPostAction() { return g_postAction != 0; }                         // @0x2b8e70

// @0x2bbfc0: Ok. Over the PvP arena's outro: after a loss "Attack again" (a weak army or belt is
// warned about once): the loot is given, the fight is set up again on the same map (a new
// PvPArenaCombat against the same opponent, its hero from mBossWaves' last entry) and the outro
// fades into the intro; after a win "Continue": the loot is given, the arena's post action
// (UNVERIFIED (milestone 5): the valor profession's level up shows LevelInfoWindow, action 4) and
// ScheduleReturn queued. PORT (online removed): the wall post of the task / location / boss /
// chest (WallPost::PostForEvent with the share ticks, from level 3) is not made.
void OnShare() {
    if (PlayerBattleInOutWindow::IsVisible() && !PlayerBattleInOutWindow::EnemyDefeated()) {
        const int alive = EntityManager::GetPlayer()->GetSquad()->GetAliveSoldierCount();
        unsigned belt = 0;
        for (unsigned i = 0; i < GameState::GetBeltSlotCount(); ++i)
            belt += (unsigned)GameState::GetItemAmount(GameState::GetBeltItemAt(i), true);
        const bool weak = belt <= 4 && alive == 0;
        if (weak && !g_weakShown) {
            // UNVERIFIED (milestone 5): PlayerWeakWindow::Show.
            g_weakShown = true;
            return;
        }
        GiveCollectedToPlayer(false, nullptr, false);
        GameState::ResetMapCollectionInfo((uint32_t)GameState::GetCurrentMapID());
        OnClose();
        if (GameState::IsCombatActive()) {
            GameState::GetActiveCombat()->PostCombat();
            GameState::EndActiveCombat(false);
            PvP::PlayerInfo* opponent = PvP::GetPlayerInfoByGID(PlayerCompetitionWindow::GetSelectedPlayerGID(), 0xff);
            GameState::SetCombat(new PvPArenaCombat(opponent, true));
            Entity* hero = EntityManager::CreateEntity(GameState::mBossWaves.back(), true, true);
            hero->player = false;
            hero->CreateSquad();
            GameState::GetActiveCombat()->BeginCombat(EntityManager::GetPlayer()->GetSquad(), hero->GetSquad());
            EntityManager::GetPlayer()->SetAnimationP("idle_1", true, false, false);
        }
        PlayerBattleInOutWindow::ScheduleRestart();
        return;
    }
    if (PlayerBattleInOutWindow::IsVisible() && PlayerBattleInOutWindow::EnemyDefeated()) {
        GiveCollectedToPlayer(false, nullptr, false);
        GameState::ResetMapCollectionInfo((uint32_t)GameState::GetCurrentMapID());
        g_postAction = 3;
        WindowManager::EnqueueWindow(PlayerBattleInOutWindow::ScheduleReturn, 0.f);
    }
    OnClose();
}

// @0x2b9748: the window the arena's buttons asked for, once the owner is done.
void RunPostAction() {
    if (g_postAction == 1) {
        // UNVERIFIED (milestone 5): ItemShopWindow::Show.
    } else if (g_postAction == 2) {
        // UNVERIFIED (milestone 5): HireTroopsWindow::Show.
    } else if (g_postAction == 3) {
        if (!GameState::IsPvPTutorial()) {
            if (!PlayerCompetitionWindow::HasReturnWithoutWindow()) {
                // UNVERIFIED (milestone 5): PlayerCompetitionWindow::Show and OnTab(4).
            }
        } else {
            GameState::SetPvPTutorial(false);
        }
        PlayerCompetitionWindow::SetReturnWithoutWindow(false);
    } else if (g_postAction == 4) {
        LevelInfoWindow::Show();
        if (Professions::Profession* valor = Professions::GetProfession(7)) LevelInfoWindow::OnTab(valor->tab, false);
    }
    g_postAction = 0;
}

// @0x2b9804: slides back down; the per-show settings reset.
void Hide() {
    if (g_root->visibleSelf) {
        int cx = g_effect->GetCenteredX();
        g_effect->Animate(cx, g_offsetY + GUI::ScreenHeight() - g_root->h, g_effect->GetCenteredX(), GUI::ScreenHeight(),
                          false);
    }
    g_queue->shown = false;
    g_queue->MoveWindowDown(true);
    g_hideOk = false;
    g_location = nullptr;
    g_task = nullptr;
    g_boss = 0;
    g_sharing = false;
}

// @0x2b98e4: the loot dialogs under it close with it. UNVERIFIED (milestone 5): ChestOpenedWindow (5p),
// between BossOutroWindow and PlayerBattleInOutWindow.
void OnClose() {
    Hide();
    if (RaidCompleteWindow::IsVisible()) RaidCompleteWindow::Hide();
    if (CampaignPerfectedWindow::IsVisible()) CampaignPerfectedWindow::Hide();
    if (CampaignCompleteWindow::IsVisible()) CampaignCompleteWindow::Hide();
    if (BossIntroWindow::IsVisible()) BossIntroWindow::Hide();
    if (BossOutroWindow::IsVisible()) BossOutroWindow::Hide();
    if (PlayerBattleInOutWindow::IsVisible()) PlayerBattleInOutWindow::Hide();
}

// @0x2bbf5c (with its tail _INIT_91 @0x2bbf44): over the PvP arena's outro the button is
// "Continue" (after a loss): the loot is given and the arena's post action follows.
void OnShop() {
    if (!PlayerBattleInOutWindow::IsVisible()) {
        OnClose();
        g_postAction = 1;
        return;
    }
    if (PlayerBattleInOutWindow::EnemyDefeated()) return;
    GiveCollectedToPlayer(false, nullptr, false);
    GameState::ResetMapCollectionInfo((uint32_t)GameState::GetCurrentMapID());
    OnClose();
    g_postAction = 3;
}

// @0x2b9df4: priority 7 for item type 8, 5 for type 7.
void AddCollectedItem(uint32_t id, int amount) {
    for (CollectedInfo& c : g_collected)
        if (c.type == 0xb && c.id == id) {
            c.amount += amount;
            return;
        }
    Items::ItemInfo* info = Items::GetItemInfo(id);
    if (!info) return;
    g_collected.push_back({0xb, id, amount, info->type == 8 ? 7u : info->type == 7 ? 5u : 0u});
}

// @0x2bcb10: priority 15 for crystals, 10 for resource 8, 9 for resource 10, else 2.
void AddCollectedResource(int type, int amount) {
    for (CollectedInfo& c : g_collected)
        if (c.type == type && c.id == 0) {
            c.amount += amount;
            return;
        }
    unsigned p = type == GameState::kCrystal ? 0xf : type == 8 ? 10 : type == 10 ? 9 : 2;
    g_collected.push_back({type, 0, amount, p});
}

// @0x2bc440
void ClearCollected() {
    g_collected.clear();
    g_scroller.firstIndex = 0;
    g_scroller.HandleMove(-g_scroller.offset);
}

// @0x2bbc40: drop: on the ground at the entity (the player by default; items in stacks of 1, or 10
// over 5), else straight into the stores. Crystals are capped at 15 unless noCrystalCap.
void GiveCollectedToPlayer(bool drop, Entity* at, bool noCrystalCap) {
    if (!at) at = EntityManager::GetPlayer();
    for (const CollectedInfo& c : g_collected) {
        if (c.type == 0xb) {
            if (drop) {
                unsigned stack = (unsigned)c.amount < 6 ? 1 : 10;
                for (unsigned given = 0; given < (unsigned)c.amount; given += stack) {
                    unsigned n = std::min(stack, (unsigned)c.amount - given);
                    BuildingHovers::DropItem(at->worldX, at->worldY, Items::GetItemInfo(c.id), 0, n);
                }
            } else {
                // (a profession's point item gives its points)
                if (Professions::Profession* p = Professions::GetProfessionForPointItem(c.id)) {
                    Professions::AddProfessionPoint(p->id, (uint32_t)c.amount);
                } else if (c.id == 0x26a) {   // a healing: the player's HP (a dead player stands up)
                    if (Entity* p = EntityManager::GetPlayer()) {
                        if (p->IsDead()) Render::ChangeLayer(p->GetSprite(), Render::kLayerObjects);
                        p->AddHP(c.amount);
                    }
                } else {
                    GameState::AddItem(c.id, c.amount, false, false, false);
                }
            }
        } else if (c.type == GameState::kCrystal) {
            GameState::AddCrystals(noCrystalCap || (unsigned)c.amount < 0xf ? c.amount : 0xf);
        } else if (drop) {
            BuildingHovers::DropResource(at->worldX, at->worldY, c.type, (unsigned)c.amount, false, false);
        } else {
            GameState::ChangeResourceAmount(c.type, c.amount);
        }
    }
    g_collected.clear();
}

// @0x2ba138: the buttons, then the visible cells of the strip (centred while there are fewer than
// ten).
void UpdateContents() {
    if (!PlayerBattleInOutWindow::IsVisible()) {
        g_shareHolder->SetVisibility(!g_hideOk);
        g_shareIcon->SetTexture(IconManager::GetIcon("gold_confirm"), true);
        g_shareText->SetText(StringTable::GetString("SHARE_OK"));
        g_shopHolder->SetVisibility(GameState::IsArenaCombatActive() && BossIntroWindow::IsVisible());
        g_shopIcon->SetTexture(IconManager::GetIcon("Icon_bottom_bar_shop"), true);
        g_shopText->SetText(StringTable::GetString("GO_TO_SHOP"));
        g_tavernHolder->SetVisibility(GameState::IsArenaCombatActive() && BossIntroWindow::IsVisible());
        g_tavernIcon->SetTexture(IconManager::GetIcon("bottom_bar_army"), true);
        g_tavernText->SetText(StringTable::GetString("GO_TO_TAVERN"));
    } else if (!PlayerBattleInOutWindow::EnemyDefeated()) {   // the PvP arena's loss
        g_shareHolder->SetVisibility(true);
        g_shareIcon->SetTexture(IconManager::GetIcon("gold_attack"), true);
        g_shareText->SetText(StringTable::GetString("PVP_AGAIN"));
        g_shopHolder->SetVisibility(true);
        g_shopIcon->SetTexture(IconManager::GetIcon("gold_profile"), true);
        g_shopText->SetText(StringTable::GetString("CAMPAIGN_INFO_RESUME"));
        g_tavernHolder->SetVisibility(true);
        g_tavernIcon->SetTexture(IconManager::GetIcon("Icon_bottom_bar_shop"), true);
        g_tavernText->SetText(StringTable::GetString("PVP_RESTOCK_HEAD"));
    } else {   // the PvP arena's win
        g_shareHolder->SetVisibility(true);
        g_shareIcon->SetTexture(IconManager::GetIcon("gold_profile"), true);
        g_shareText->SetText(StringTable::GetString("CAMPAIGN_INFO_RESUME"));
        g_shopHolder->SetVisibility(false);
        g_tavernHolder->SetVisibility(false);
    }
    // The share ticks: shown for a location, task or boss, or a chest's share with Ok (over a
    // PvP win: unless the tutorial's ambush). PORT: hidden.
    (void)g_sharing;
    g_social->SetVisibility(false);

    g_noneText->SetText(StringTable::GetString("FOUND_ITEMS_NONE"));
    g_noneText->SetVisibility(g_collected.empty());
    g_tapText->SetVisibility(g_hideOk);
    g_tapTextCopy->SetVisibility(g_hideOk);
    g_tapText->SetText(StringTable::GetString("TAP_TO_CONTINUE"));
    static_cast<Textfield*>(g_tapTextCopy)->SetText(StringTable::GetString("TAP_TO_CONTINUE"));

    for (ItemInfo& c : g_items) c.root->MoveWindow(-g_centerShift, 0);
    g_centerShift = 0;
    const size_t count = g_collected.size();
    const unsigned first = g_scroller.firstIndex;
    unsigned shown = 0;
    for (size_t i = first; i < count && shown != 10; i = first + shown) {
        const CollectedInfo& e = g_collected[i];
        ItemInfo& c = g_items[shown];
        c.root->SetVisibility(true);
        if (Items::ItemInfo* info = Items::GetItemInfo(e.id)) {
            c.locked->SetVisibility(false);
            for (unsigned k = 0; k < 5; ++k) c.cls[k]->SetVisibility(info->itemLevel == k);
            info->PutImageInHolder(c.icon);
            c.quantity->SetText(ToWideString(e.amount));
            c.resource = 0xb;
            c.item = e.id;
        } else if (e.type != 0xb) {
            c.locked->SetVisibility(false);
            for (unsigned k = 0; k < 5; ++k) c.cls[k]->SetVisibility(k == 0);
            c.icon->SetTexture(IconManager::GetIcon(GameState::GetResourceMapIconName(e.type)), true);
            c.quantity->SetText(ToWideString(e.amount));
            c.item = 0;
            c.resource = e.type;
        }
        ++shown;
    }
    if (count != 0 && count < 10) {
        int cell = g_items[1].root->x - g_items[0].root->x;
        g_centerShift = (int)((float)(unsigned)(cell * (9 - (int)shown)) * 0.5f);
    }
    for (unsigned k = shown; k < 10; ++k) g_items[k].root->SetVisibility(false);
    for (ItemInfo& c : g_items) c.root->MoveWindow(g_centerShift, 0);
    g_scroller.itemCount = (unsigned)count;
    g_left->SetEnabled(g_scroller.CanMoveLeft());
    g_right->SetEnabled(g_scroller.CanMoveRight());
    g_left->SetVisibility(g_scroller.CanMove());
    g_right->SetVisibility(g_scroller.CanMove());
    g_root->UpdatePosition();
}

// @0x2baba8: slides up from below the screen with the band (no band over the PvP arena's window).
void Show() {
    g_queue->MoveWindowOnTop(true);
    g_weakShown = false;
    if (g_queue->shown) return;
    g_queue->shown = true;
    int h = GUI::ScreenHeight();
    g_effect->Animate(g_effect->GetCenteredX(), h, g_effect->GetCenteredX(), h + g_offsetY - g_root->h, true);
    g_root->SetPosition(g_effect->GetCenteredX(), h);
    g_root->SetVisibility(true);
    bool band = !PlayerBattleInOutWindow::IsVisible();
    Render::SetVisibility(g_dark, band);
    g_dark->h = (float)h * 0.3f;
    g_dark->w = ScreenW();
    Render::SetPosition(g_dark, 0.f, (float)h + g_dark->h, 0.028f);
    Render::SetVisibility(g_goldLine, band);
    g_goldLine->w = ScreenW();
    Render::SetPosition(g_goldLine, 0.f, (float)h, 0.0279f);
    BattleBarWindow::HideHoverWindow();
    BeltBarWindow::HideHoverWindow();
    UpdateContents();
    g_root->PreloadTextures();   // GUI::PreloadWindow
}

// @0x2bb214 (with its tail _INIT_90 @0x2bbaf4)
void Init() {
    g_goldLine = nullptr;
    g_centerShift = 0;
    g_darkTex = nullptr;
    g_dark = nullptr;
    g_tablet = GUI::IsTabletVersion();
    g_scale = ScreenH() / 650.f;
    if (GUI::GetHoverScaleFactor(1.f, 1.f) < g_scale) g_scale = GUI::GetHoverScaleFactor(1.f, 1.f);
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Endscreen_item_holder.xml", "Endscreen_item_holder.png",
                             g_scale, 0, 0, 0, 0, false, 1.f);
    g_queue->RegisterTopWindow(g_root);
    g_scale = g_root->scale;
    g_root->SetVisibility(false);
    g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::GetVerticalCenter(g_root->h));

    g_shareHolder = GUI::GetWindowTyped<Window>(g_root, "lvl_up_share_holder");
    g_shareText = GUI::GetWindowTyped<Textfield>(g_root, "lvl_up_share_holder.share.text_upgrade");
    g_shareIcon = GUI::GetWindowTyped<Window>(g_root, "lvl_up_share_holder.share.icon_holder");
    g_shareIcon->takesZ = true;
    g_shareClick = GUI::GetWindowTyped<Button>(g_root, "lvl_up_share_holder.share.clickArea");
    g_shareClick->SetOnClick(OnShare);
    // Shared::ShareButtonsHolder::LoadFrom @0x3423d4 (its Twitter tick toggles OnTwitterToggle).
    // PORT: only its root, kept hidden.
    g_social = GUI::GetWindowTyped<Window>(g_root, "lvl_up_share_social_01");
    g_shopHolder = GUI::GetWindowTyped<Window>(g_root, "lvl_up_shop_holder");
    g_shopIcon = GUI::GetWindowTyped<Window>(g_root, "lvl_up_shop_holder.share.icon_holder");
    g_shopIcon->takesZ = true;
    g_shopText = GUI::GetWindowTyped<Textfield>(g_root, "lvl_up_shop_holder.share.text_upgrade");
    g_shopClick = GUI::GetWindowTyped<Window>(g_root, "lvl_up_shop_holder.share.clickArea");
    g_shopClick->SetOnClick(OnShop);
    g_tavernHolder = GUI::GetWindowTyped<Window>(g_root, "lvl_up_tavern_holder");
    g_tavernIcon = GUI::GetWindowTyped<Window>(g_root, "lvl_up_tavern_holder.share.icon_holder");
    g_tavernIcon->takesZ = true;
    g_tavernText = GUI::GetWindowTyped<Textfield>(g_root, "lvl_up_tavern_holder.share.text_upgrade");
    g_tavernClick = GUI::GetWindowTyped<Window>(g_root, "lvl_up_tavern_holder.share.clickArea");
    g_tavernClick->SetOnClick(OnTavern);
    g_header = GUI::GetWindowTyped<Textfield>(g_root, "text_01");
    g_noneText = GUI::GetWindowTyped<Textfield>(g_root, "text_nothing_found");
    g_tapText = GUI::GetWindowTyped<Textfield>(g_root, "text_tap_to_continue");
    g_tapTextCopy = GUI::DuplicateWindow(g_tapText, false, false, nullptr);
    if (Render::GlowFilter* g = g_tapText->glow) {   // a strong white glow behind the copy
        g->b = 1.f;
        g->r = 1.f;
        g->g = 1.f;
        g->blur = 0x19;
        g->strength = 5.f;
        if (g->next) g->next->strength = 0.f;
    }
    g_left = GUI::GetWindowTyped<Window>(g_root, "button_navigate_left");
    g_left->SetOnClick([] { OnPositionChange(false); });
    g_right = GUI::GetWindowTyped<Window>(g_root, "button_navigate_right");
    g_right->SetOnClick([] { OnPositionChange(true); });
    for (unsigned i = 0; i < 10; ++i) {
        char name[64];
        std::snprintf(name, sizeof name, "item_holder_endscreen_items.reward_item_%02d", i + 1);
        g_items[i].LoadFrom(g_root, name);
        g_items[i].root->SetOnClick([i] { OnItem(i); });
    }
    g_effect = GUI::CreateMovementEffect(g_root, OnTweenMove, OnTweenIn, OnTweenOut);
    g_scroller.Init();
    Window* mask = GUI::GetWindowTyped<Window>(g_root, "item_holder_endscreen_items.mask_universal");
    g_scroller.viewport = {mask->x, mask->y, mask->x + mask->w, mask->y + mask->h};
    g_scroller.track = mask;
    for (ItemInfo& c : g_items) {
        c.root->SetClipRect(&g_scroller.viewport);
        g_scroller.windows.push_back(c.root);
    }
    g_queue->fn.zRange = ZRange;
    g_queue->fn.show = Show;
    g_queue->fn.update = Update;
    g_queue->fn.back = OnBack;
    g_scroller.unkA0 = 10;
    g_scroller.itemSize = g_items[1].root->x - g_items[0].root->x;
    g_scroller.onScroll = UpdateContents;
    g_scroller.padEnd += 0x1e;
    g_scroller.padStart += 0x1e;
    g_scroller.horizontal = true;
    g_scroller.perLine = 1;
    g_darkTex = Render::CreateSolidFillTexture(0.f, 0.f, 0.f, 1.f);
    g_dark = Render::CreateSprite(g_darkTex, Render::kLayerGUI, false, false);
    g_dark->screenSpace = true;
    Render::SetShaderType(g_dark, 1);
    Render::SetVisibility(g_dark, false);
    Render::SetAlpha(g_dark, 0.6f);
    g_goldLine = Render::CreateSprite(Resources::GetUIImage("Endscreen_goldline_scaler.png", false, false),
                                      Render::kLayerGUI, false, false);
    g_goldLine->screenSpace = true;
    Render::SetVisibility(g_goldLine, false);
    g_hover = new ItemHoverWindow();
}

// @0x2bcd50: the loot of a setting: [id, n] (an item) or ["resource", n], or a list of them.
void SetParameters(const char* setting, const char32_t* title) {
    ClearCollected();
    Setting s(setting);
    auto add = [](const Setting& e) {
        if (e.GetChild(0).IsNumber()) AddCollectedItem((uint32_t)e.GetChild(0).GetInt(), e.GetChild(1).GetInt());
        else if (e.GetChild(0).IsString())
            AddCollectedResource(GameState::StringToResourceType(e.GetChild(0).GetString()), e.GetChild(1).GetInt());
    };
    if (s.GetChild(0).IsNumber() || s.GetChild(0).IsString()) {
        add(s);
    } else {
        for (unsigned i = 0; i < s.GetChildrenCount(); ++i) add(s.GetChild(i));
    }
    FinishParameters(title);
}

// @0x2bcffc FoundItemsWindow::SetParameters(Tasks::TaskInfo*, ...)
void SetParameters(Tasks::TaskInfo* task, const char32_t* title, unsigned crystals) {
    ClearCollected();
    if (!task) return;
    g_task = task;
    if (crystals != 0) AddCollectedResource(GameState::kCrystal, (int)crystals);
    static const int kOrder[11] = {9, 10, 8, 0, 1, 2, 3, 4, 5, 6, 7};   // 0x597300
    for (int type : kOrder)
        if (task->award[type] != 0) AddCollectedResource(type, (int)task->award[type]);
    for (unsigned i = 0; i < task->itemCount; ++i) AddCollectedItem(task->items[i].id, (int)task->items[i].count);
    FinishParameters(title);
}

// @0x2bd11c: what was picked up on a map (a boss's: for the share).
void SetParameters(uint32_t map, uint32_t boss, const char32_t* title) {
    ClearCollected();
    if (map == 0) return;
    g_boss = boss;
    for (int t = 0; t < 0xb; ++t)
        if (int n = GameState::GetMapResourceCollectionInfo(map, t)) AddCollectedResource(t, n);
    for (unsigned i = 0; Items::ItemInfo* info = Items::EnumItemInfo(i); ++i) {
        int n = GameState::GetMapItemCollectionInfo(map, info->id);
        if (n == 0 || info->id == 0x2000) continue;
        AddCollectedItem(info->id, n);
    }
    FinishParameters(title);
}

// @0x2bd21c: what was picked up on every map of a location (with `crystals` first when not 0).
void SetParameters(Locations::LocationInfo* info, const char32_t* title, unsigned crystals) {
    ClearCollected();
    if (!info) return;
    g_location = info;
    if (crystals != 0) AddCollectedResource(GameState::kCrystal, (int)crystals);
    for (const Locations::MapInfo& m : info->maps) {
        for (int t = 0; t < 0xb; ++t)
            if (int n = GameState::GetMapResourceCollectionInfo(m.id, t)) AddCollectedResource(t, n);
        for (unsigned i = 0; Items::ItemInfo* item = Items::EnumItemInfo(i); ++i) {
            int n = GameState::GetMapItemCollectionInfo(m.id, item->id);
            if (n == 0 || item->id == 0x2000) continue;
            AddCollectedItem(item->id, n);
        }
    }
    FinishParameters(title);
}

}  // namespace FoundItemsWindow
