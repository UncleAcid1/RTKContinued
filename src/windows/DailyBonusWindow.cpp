// DailyBonusWindow: the daily bonus. Five day flags (the day in the streak lit), the day's rewards
// (resources, items, soldiers or a daily bonus chest) and "Collect", which drops them by the hero.
// Port of DailyBonusWindow (libkingdom.so 5.11, @0x2a8c10..0x2ab784, Window_daily_bonus.xml;
// statics 0x61e5e0).
//
// The social part (share on Twitter / Facebook, connect accounts) is online: PlayerProfileManager::
// FBEnabled is false offline, so Init hides it as the original does without Facebook.
#include <cstdio>
#include <vector>

#include "engine/IconManager.h"
#include "game/BuildingHovers.h"
#include "game/Chests.h"
#include "game/Entity.h"
#include "game/EntityManager.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/SoldierSlots.h"
#include "game/StringTable.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/ItemHoverWindow.h"
#include "windows/Windows.h"

namespace DailyBonusWindow {
namespace {

using GUI::Button;
using GUI::Textfield;
using GUI::Window;

// DailyBonusWindow::DailyBonusHolder (0x38 bytes): one day's flag.
struct DailyBonusHolder {
    Window* root = nullptr;           // +0x00
    Window* flag = nullptr;           // +0x04 daily_flag_bg
    Window* flagDisabled = nullptr;   // +0x08 daily_flag_bg_disabled
    Window* glow = nullptr;           // +0x0c pickup_item_glow
    Textfield* day1 = nullptr;        // +0x10
    Textfield* day2 = nullptr;        // +0x14
    Window* item = nullptr;           // +0x18 item_holder.icon_60_on (tapped: OnItem)
    Textfield* quantity = nullptr;    // +0x1c
    Window* chestOpened = nullptr;    // +0x20
    Window* chestClosed = nullptr;    // +0x24
    Window* chestIcon = nullptr;      // +0x28 the chest's "?"
    Window* done = nullptr;           // +0x2c the tick

    // @0x2a97cc
    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        flag = GUI::GetWindowTypedF<Window>(w, "%s.daily_flag_bg", p);
        flagDisabled = GUI::GetWindowTypedF<Window>(w, "%s.daily_flag_bg_disabled", p);
        glow = GUI::GetWindowTypedF<Window>(w, "%s.pickup_item_glow", p);
        day1 = GUI::GetWindowTypedF<Textfield>(w, "%s.text_day_01", p);
        day2 = GUI::GetWindowTypedF<Textfield>(w, "%s.text_day_02", p);
        item = GUI::GetWindowTypedF<Window>(w, "%s.item_holder.icon_60_on", p);
        item->takesZ = true;
        quantity = GUI::GetWindowTypedF<Textfield>(w, "%s.item_holder.text_quantity", p);
        chestOpened = GUI::GetWindowTypedF<Window>(w, "%s.daily_chest_holder.icon_chest_opened", p);
        chestClosed = GUI::GetWindowTypedF<Window>(w, "%s.daily_chest_holder.icon_chest_closed", p);
        chestIcon = GUI::GetWindowTypedF<Window>(w, "%s.daily_chest_holder.icon_35", p);
        chestIcon->SetTexture(IconManager::GetIcon("gold_question", false), true);
        done = GUI::GetWindowTypedF<Window>(w, "%s.icon_35", p);
        done->SetTexture(IconManager::GetIcon("gold_confirm", false), true);
    }
};

// DailyBonusWindow::ItemIconHolder (0xc bytes): one of the three rewards at the bottom.
struct ItemIconHolder {
    Window* root = nullptr;           // +0x00
    Window* icon = nullptr;           // +0x04 (tapped: OnBottomItem)
    Textfield* quantity = nullptr;    // +0x08

    // @0x2a99c4
    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        icon = GUI::GetWindowTypedF<Window>(w, "%s.icon_60", p);
        icon->takesZ = true;
        quantity = GUI::GetWindowTypedF<Textfield>(w, "%s.text_quantity", p);
    }
};

// DailyBonusWindow::SocialLinkHolder (0x24 bytes): share and connect (online).
struct SocialLinkHolder {
    Window* root = nullptr;           // +0x00 (tapped: OnShare)
    Window* twitter = nullptr;        // +0x04
    Window* facebook = nullptr;       // +0x08
    Window* tick1 = nullptr;          // +0x0c
    Window* tick2 = nullptr;          // +0x10
    Window* connect = nullptr;        // +0x14
    Window* connectIcon = nullptr;    // +0x18
    Textfield* connectText = nullptr; // +0x1c
    Textfield* connected = nullptr;   // +0x20

    // @0x2a9674
    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        twitter = GUI::GetWindowTypedF<Window>(w, "%s.icon_social_twitter", p);
        facebook = GUI::GetWindowTypedF<Window>(w, "%s.icon_social_facebook", p);
        tick1 = GUI::GetWindowTypedF<Window>(w, "%s.social_share_tickbox_01", p);
        tick2 = GUI::GetWindowTypedF<Window>(w, "%s.social_share_tickbox_02", p);
        connect = GUI::GetWindowTypedF<Window>(w, "%s.button_connect_accounts", p);
        connectIcon = GUI::GetWindowTypedF<Window>(w, "%s.button_connect_accounts.icon_connect", p);
        connectIcon->SetTexture(IconManager::GetIcon("gold_energy", false), true);
        connectText = GUI::GetWindowTypedF<Textfield>(w, "%s.button_connect_accounts.text_start_raid", p);
        connected = GUI::GetWindowTypedF<Textfield>(w, "%s.button_connect_accounts.text_accs_connected", p);
    }
};

// One reward (DailyBonusWindow::BonusInfo, 0x10 bytes).
struct BonusInfo {
    uint32_t type = 0;      // +0x00 GameState::ResourceType; 0xb an item, a soldier or the chest
    uint32_t item = 0;      // +0x04 an item id; 0x2000 and above: a daily bonus chest
    uint32_t entity = 0;    // +0x08 the soldier (an id that is no item)
    uint32_t amount = 0;    // +0x0c
};

WindowManager::FunctionalWindow* g_queue = nullptr;   // 0x61e794
ItemHoverWindow* g_hover = nullptr;       // 0x61e5e0
Window* g_root = nullptr;                 // 0x61e5e4
GUI::TweenEffect* g_tween = nullptr;      // 0x61e5e8
bool g_forced = false;                    // 0x61e5ec ForceBonus: Show moves the streak on regardless
DailyBonusHolder g_days[5];               // 0x61e5f0
ItemIconHolder g_items[3];                // 0x61e708
std::vector<BonusInfo> g_bonuses;         // 0x61e72c the rewards UpdateContents chose
unsigned g_bonusCount = 0;                // 0x61e744 how many of them Collect gives
Window* g_close = nullptr;                // 0x61e748 x_button (always hidden)
Textfield* g_title = nullptr;             // 0x61e74c
Textfield* g_titleDesc = nullptr;         // 0x61e750
Textfield* g_connectText = nullptr;       // 0x61e754
SocialLinkHolder g_social;                // 0x61e758
Textfield* g_received = nullptr;          // 0x61e77c text_receved
Textfield* g_collectText = nullptr;       // 0x61e780
Button* g_collect = nullptr;              // 0x61e784 button_collect_bonus.clickArea
bool g_tablet = false;                    // 0x60f204
float g_scale = 1.f;                      // 0x60f208

// @0x2a8c10
bool Click(int x, int y, bool pressed) {
    if (g_hover->Click(x, y, pressed)) return true;
    bool visible = g_root->visibleSelf;
    g_root->Click(x, y, pressed, false);
    return visible;
}

// @0x2a8c90
void SetZ(float z) {
    if (!g_queue->shown) return;
    g_root->SetZ(z);
    g_root->SetPosition(g_tween->GetCenteredX(), g_tween->GetCenteredY());
}

// @0x2a8d30
float ZRange() { return g_queue->shown ? 0.015f : 0.001f; }

// @0x2a8ed8
void OnTweenOut() {
    WindowManager::WindowHide(false);
    g_queue->shown = false;
    g_queue->MoveWindowDown(true);
}

// Milestone 5 (5r-5): the day flag's item card (OnItem @0x2a8f08).
void OnItem(unsigned i) { (void)i; }

// Milestone 5 (5r-5): the reward's item card (OnBottomItem @0x2a90f4).
void OnBottomItem(unsigned i) { (void)i; }

// Milestone 5 (5r-5): sharing (OnShare @0x2a9320, online).
void OnShare() {}

// @0x2a9348 Collect: each reward is dropped by the hero (resources and items) or joins him (a
// soldier, while the army has room; the troops window opens after); a daily bonus chest is rolled
// and opens in ChestOpenedWindow.
void OnClose() {
    Entity* player = EntityManager::GetPlayer();
    bool hired = false;
    for (unsigned i = 0; i < g_bonusCount; ++i) {
        const BonusInfo& b = g_bonuses[i];
        if (b.type != 0xb) {
            BuildingHovers::DropResource(player->worldX, player->worldY, (int)b.type, b.amount, false, false);
        } else if (b.item < 0x2000) {
            if (Items::ItemInfo* info = Items::GetItemInfo(b.item)) {
                for (unsigned n = 0; n < g_bonuses[i].amount; ++n)
                    BuildingHovers::DropItem(player->worldX, player->worldY, info, 0, 1);
            } else if (b.entity != 0 && SoldierPool::GetOccupiedSlotCount() < SoldierPool::GetTotalSlotsFree()) {
                hired = true;
                GameState::HireSoldier(EntityManager::CreateEntity((int)b.entity, false, true), true, 2);
            }
        } else if (Chests::ChestInfo* chest = Chests::GetRandomChestByType(4)) {
            Chests::DropItem drops[0x10];
            const unsigned n = chest->GenerateContents(drops, 0x10, false);
            FoundItemsWindow::ClearCollected();
            FoundItemsWindow::SetHeader(StringTable::GetString("TASK_COMPLETE_DESC"));
            FoundItemsWindow::SetSharing(true);
            for (unsigned k = 0; k < n; ++k) {
                if (drops[k].type == 0xb) FoundItemsWindow::AddCollectedItem(drops[k].item, drops[k].amount);
                else FoundItemsWindow::AddCollectedResource((int)drops[k].type, drops[k].amount);
            }
            ChestOpenedWindow::SetName(chest->name);
            ChestOpenedWindow::Show();
        }
    }
    GameState::DailyBonusReceived();
    g_forced = false;
    Hide();
    if (hired) HireTroopsWindow::Show();
}

// @0x2a95b8
bool OnBack() {
    if (!g_queue->shown) return false;
    if (g_hover->shownHover) g_hover->Hide();
    else OnClose();
    return true;
}

}  // namespace

WindowManager::FunctionalWindow* Queue() {
    if (!g_queue) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.click = Click;
        f.setZ = SetZ;
        f.hide = Hide;
        g_queue = new WindowManager::FunctionalWindow("DailyBonusWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_queue && g_queue->shown; }   // @0x2a8d1c
void ForceBonus() { g_forced = true; }                   // @0x2a8d60

// @0x2a9a34
void Init() {
    g_tablet = GUI::IsTabletVersion();
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Window_daily_bonus.xml", "Window_daily_bonus.png", -1.f,
                             0, 0, 0, 0, false, 1.f);
    if (!g_root) return;
    Queue()->RegisterTopWindow(g_root);
    g_scale = g_root->scale;
    g_root->SetVisibility(false);
    g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::GetVerticalCenter(g_root->h));
    g_tween = GUI::CreateTweenEffect(g_root, nullptr, nullptr, OnTweenOut);
    g_close = GUI::GetWindowTyped<Window>(g_root, "x_button");
    g_close->SetVisibility(false);
    g_title = GUI::GetWindowTyped<Textfield>(g_root, "text_title");
    g_titleDesc = GUI::GetWindowTyped<Textfield>(g_root, "text_title_desc");
    for (unsigned i = 0; i != 5; ++i) {
        char p[64];
        std::snprintf(p, sizeof p, "daily_bonus_flag_%02d", i + 1);
        g_days[i].LoadFrom(g_root, p);
        g_days[i].item->SetOnClick([i] { OnItem(i); });
    }
    g_connectText = GUI::GetWindowTyped<Textfield>(g_root, "text_connect");
    g_social.LoadFrom(g_root, "daily_bonus_accounts");
    g_social.root->SetOnClick(OnShare);
    g_received = GUI::GetWindowTyped<Textfield>(g_root, "text_receved");
    for (unsigned i = 0; i != 3; ++i) {
        char p[64];
        std::snprintf(p, sizeof p, "daily_bonus_3_items.item_holder_no_bg_%02d", i + 1);
        g_items[i].LoadFrom(g_root, p);
        g_items[i].icon->SetOnClick([i] { OnBottomItem(i); });
    }
    g_collectText = GUI::GetWindowTyped<Textfield>(g_root, "button_collect_bonus.text_start_raid_centered");
    g_collect = GUI::GetWindowTyped<Button>(g_root, "button_collect_bonus.clickArea");
    g_collect->SetOnClick(OnClose);
    g_queue->hideOnOuterClick = true;
    // Milestone 5 (5r-5): the queue's update (DailyBonusWindow::Update).
    g_queue->fn.zRange = ZRange;
    g_queue->fn.back = OnBack;
    g_queue->fn.show = Show;
    g_hover = new ItemHoverWindow();
    // PlayerProfileManager::FBEnabled (online) is false: no Facebook hint, connect text or accounts.
    GUI::GetWindowTyped<Window>(g_root, "dialog_hint_window_right")->SetVisibility(false);
    GUI::GetWindowTyped<Window>(g_root, "text_connect")->SetVisibility(false);
    GUI::GetWindowTyped<Window>(g_root, "daily_bonus_accounts")->SetVisibility(false);
}

// @0x2ab6cc (only in the city): the streak moves on (UpdateDailyBonusTime, forced after
// ForceBonus) and the contents are set.
void Show() {
    if (!GameState::IsPlayerCity()) return;
    g_queue->shown = true;
    g_queue->MoveWindowOnTop(true);
    if (!g_root->visibleSelf) {
        WindowManager::WindowShow(false);
        g_tween->AnimateIn();
    }
    g_root->SetVisibility(true);
    // SoundsManager::PlaySound("ui_level_up"): milestone 6.
    GameState::SetDailyBonusStatus(0);
    GameState::UpdateDailyBonusTime(g_forced);
    UpdateContents();
    g_root->PreloadTextures();
}

// @0x2a9324
void Hide() {
    if (!g_root || !g_root->visibleSelf) return;
    g_tween->AnimateOut();
}

// Milestone 5 (5r-4): the flags, the rewards (g_bonuses) and the texts (UpdateContents @0x2aa050).
void UpdateContents() {}

}  // namespace DailyBonusWindow
