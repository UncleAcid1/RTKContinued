// DailyBonusWindow: the daily bonus. Five day flags (the day in the streak lit), the day's rewards
// (resources, items, soldiers or a daily bonus chest) and "Collect", which drops them by the hero.
// Port of DailyBonusWindow (libkingdom.so 5.11, @0x2a8c10..0x2ab784, Window_daily_bonus.xml;
// statics 0x61e5e0).
//
// The social part (share on Twitter / Facebook, connect accounts) is online: PlayerProfileManager::
// FBEnabled is false offline, so Init hides it as the original does without Facebook.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "engine/IconManager.h"
#include "engine/Render.h"
#include "engine/Timer.h"
#include "game/Animation.h"
#include "game/BuildingHovers.h"
#include "game/Chests.h"
#include "game/Entity.h"
#include "game/EntityData.h"
#include "game/EntityManager.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/Setting.h"
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
    uint32_t shownItem = 0;           // +0x30 the item on the flag (OnItem's card)
    uint32_t shownEntity = 0;         // +0x34 or the soldier

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
int g_bottomShift = 0;                    // 0x61e788 the bottom row's shift (centred when under 3)
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

// The card points at w, a window of the dialog.
void PointHoverAt(Window* w, ItemHoverWindow::Direction dir) {
    const int x = g_root->x + w->x, y = g_root->y + w->y;
    g_hover->SetTargetRect(x, y, x + w->w, y + w->h, dir);
    g_hover->RecomputePosition();
    g_hover->SetZ(0.001f);
    Render::SortRenderLayer(0xe, 1);
}

// @0x2a8f08: the card of a day flag's reward (its item, the soldier's name, or the chest).
void OnItem(unsigned i) {
    const DailyBonusHolder& h = g_days[i];
    Items::ItemInfo* info = Items::GetItemInfo(h.shownItem);
    EntityData* data = h.shownEntity ? EntityFactory::GetEntityByID((int)h.shownEntity) : nullptr;
    // SoundsManager::PlaySound("ui_click"): milestone 6.
    g_hover->Show();
    if (info) g_hover->SetItem(U"", nullptr, info, false, false, nullptr, true, false, false, false);
    else if (data) g_hover->SetText(data->title, nullptr);
    else g_hover->SetText(StringTable::GetString("daily_bonus_chest"), nullptr);
    PointHoverAt(h.item, ItemHoverWindow::kSE);
}

// @0x2a90f4: the card of one of today's rewards (an item, a soldier, the chest or a resource).
void OnBottomItem(unsigned i) {
    const BonusInfo& b = g_bonuses[i];
    Items::ItemInfo* info = Items::GetItemInfo(b.item);
    EntityData* data = b.entity ? EntityFactory::GetEntityByID((int)b.entity) : nullptr;
    // SoundsManager::PlaySound("ui_click"): milestone 6.
    g_hover->Show();
    if (info) g_hover->SetItem(U"", nullptr, info, false, false, nullptr, false, false, false, false);
    else if (data) g_hover->SetText(data->title, nullptr);
    else if (b.type == 0xb) g_hover->SetText(StringTable::GetString("daily_bonus_chest"), nullptr);
    else g_hover->SetText(GameState::GetResourceGameName((int)b.type), nullptr);
    PointHoverAt(g_items[i].icon, ItemHoverWindow::kSW);
}

void OnShare() {}   // @0x2a9320 (empty)

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

uint32_t g_lastSecond = 0;   // 0x61e78c
uint32_t g_lastCheck = 0;    // 0x61e790 the last availability check

// @0x2ab784: the card stays in front (closed when another window opens); the share ticks follow
// the connected accounts (new contents when they change). Once a second, after the city tutorial:
// a streak older than two days is broken; between one and two days after the last bonus day, with
// nothing in the way and not more often than hourly, the availability is checked; a waiting bonus
// opens the window (then its contents are refreshed every second).
void Update(float) {
    if (g_hover->shownHover) {
        if (g_hover->GetZ() != 0.001f) {
            g_hover->SetZ(0.001f);
            Render::SortRenderLayer(0xe, 1);
        }
        if (WindowManager::GetShownWindowCount() > 1) g_hover->Hide();
    }
    if (g_root->visibleSelf) {
        const bool fb = g_social.tick2->visibleSelf, tw = g_social.tick1->visibleSelf;
        const bool both = GameState::GetFacebookConnection() && GameState::GetTwitterConnection();
        g_social.tick2->SetVisibility(GameState::GetFacebookConnection());
        g_social.tick1->SetVisibility(GameState::GetTwitterConnection());
        g_social.connectIcon->SetVisibility(!both);
        g_social.connectText->SetVisibility(!both);
        g_social.connected->SetVisibility(both);
        if (GameState::GetFacebookConnection() != fb || GameState::GetTwitterConnection() != tw) UpdateContents();
    }
    const uint32_t now = (uint32_t)Timer::GetGlobalTime();
    if (now == g_lastSecond) return;
    g_lastSecond = (uint32_t)Timer::GetGlobalTime();
    if (GameState::secondTutorial != 0x100) return;
    if (GameState::GetDailyBonusTime() == 0 ||
        (uint32_t)Timer::GetGlobalTime() - GameState::GetDailyBonusTime() > 0x2a300)
        GameState::UpdateDailyBonusTime(false);
    const uint32_t since = (uint32_t)Timer::GetGlobalTime() - GameState::GetDailyBonusTime();
    const bool free = WindowManager::GetShownWindowCount() == 0 && !GameState::IsCombatActive() &&
                      !BuildingHovers::IsHoverVisible() && !BuildingHovers::IsWorldDialogVisible() &&
                      GameState::GetCurrentMapID() != 0x70;
    if (since - 86400 <= 86400 && free &&
        (g_lastCheck == 0 || (uint32_t)Timer::GetGlobalTime() - g_lastCheck > 3600)) {
        g_lastCheck = (uint32_t)Timer::GetGlobalTime();
        // PORT (user 2026-10-19): FileManager::CheckDailyBonusAvailability asked the server
        // (CMyServer::NET_DailyBonus_Validate, its answer "1" setting status 1 through
        // CommonSetFlags 0x1f6); offline the bonus is simply available in this window of time.
        GameState::SetDailyBonusStatus(1);
    }
    if (!g_queue->shown) {
        if (!free) return;
        if (!GameState::IsPaused() && GameState::IsDailyBonusAvailable()) Show();
        if (!g_queue->shown) return;
    }
    UpdateContents();
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
    g_queue->fn.update = Update;
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

// @0x2aa050: the five day flags (each day's first "new_daily_bonus_<n>" reward: an item, or an
// "ENTITY" soldier shown by its idle figure; day 5 the chest), today's rewards at the bottom and the
// Collect text. Days 1-4 give the day's "new_daily_bonus_<day>" list (an item id or a resource name
// and an amount, or "ENTITY" and a soldier); day 5 a daily bonus chest; a connected Facebook /
// Twitter account adds "connect_fb_bonus" / "connect_tw_bonus" of resource 8.
void UpdateContents() {
    const uint32_t days = GameState::GetDailyBonusDays();
    g_title->SetText(StringTable::GetString("DAILY_BONUS_HEADER"));
    g_titleDesc->SetText(StringTable::GetString("DAILY_BONUS_DESCRIPTION"));
    for (uint32_t day = 1; day <= 5; ++day) {
        DailyBonusHolder& h = g_days[day - 1];
        char key[64];
        std::snprintf(key, sizeof key, "new_daily_bonus_%d", (int)day);
        int amount = 1;
        uint32_t item = 0;
        int entity = 0;
        bool several = false;
        if (GameState::SettingExists(key)) {
            Setting s(key);
            // A list of rewards shows its first; a single one is the setting itself.
            Setting e = s.GetChild(0).GetChildrenCount() == 0 ? s : s.GetChild(0);
            if (!e.GetChild(0).IsNumber()) {
                if (std::strcmp(e.GetChild(0).GetString(), "ENTITY") != 0)
                    std::fprintf(stderr, "ERROR: DailyBonusWindow::UpdateContents() Daily bonus '%s' is invalid\n", key);   // ErrorReporter::Printf
                entity = e.GetChild(1).GetInt();
            } else {
                item = (uint32_t)e.GetChild(0).GetInt();
                amount = e.GetChild(1).GetInt();
                several = amount > 1;
            }
        }
        const bool reached = day <= days;
        h.flag->SetVisibility(reached);
        h.flagDisabled->SetVisibility(days < day);
        h.shownEntity = 0;
        h.shownItem = 0;
        EntityData* data = entity != 0 ? EntityFactory::GetEntityByID(entity) : nullptr;
        if (data) {
            Animation* anim = data->GetAnimation("idle_1");
            if (anim) {
                if (!anim->loaded) anim->Load();
                if (Render::Texture* t = anim->GetTexByDir(0)) {
                    h.item->SetTexture(t, true);
                    if (Render::Sprite* sp = h.item->sprite) {
                        Render::SetFrame(sp, (float)Render::GetFrameHeight(sp->tex),
                                         (float)Render::GetFrameWidth(sp->tex), 0);
                        Render::SetMagnifyFilter(sp->tex, true);
                        sp->h *= g_scale;
                        sp->w *= g_scale;
                        h.root->UpdatePosition();
                    }
                }
            }
            h.done->SetVisibility(reached);
            h.shownEntity = (uint32_t)entity;
        } else if (Items::ItemInfo* info = Items::GetItemInfo(item)) {
            h.item->SetTexture(info->GetIcon(), true);
            if (h.item->sprite) h.item->sprite->vBottom = 1.f;
            h.done->SetVisibility(reached);
            h.shownItem = item;
        } else {
            h.done->SetVisibility(false);
        }
        const bool today = days == day;
        h.glow->SetVisibility(today);
        if (!today) {
            std::u32string text = SWPrintf(0x100, StringTable::GetString("DAILY_BONUS_DAY"), {ToWideString((int)day)});
            h.day1->SetText(text.c_str());
            h.day2->SetText(text.c_str());
        } else {
            h.day1->SetText(StringTable::GetString("DAILY_BONUS_TODAY"));
            h.day2->SetText(StringTable::GetString("DAILY_BONUS_TODAY"));
        }
        h.quantity->SetVisibility(several);
        h.quantity->SetText(ToWideString(amount));
        h.chestOpened->SetVisibility(day == 5 && days == 5);
        h.chestClosed->SetVisibility(day == 5 && days != 5);
        h.chestIcon->SetVisibility(day == 5 && days != 5);
    }
    g_connectText->SetText(StringTable::GetString("DAILY_BONUS_CONNECT"));
    g_social.connectText->SetText(StringTable::GetString("SOCIAL_CONNECT"));
    g_social.connected->SetText(StringTable::GetString("SOCIAL_CONNECTED"));
    g_received->SetText(StringTable::GetString("DAILY_BONUS_TOTAL"));

    // Today's rewards.
    g_bonuses.clear();
    auto add = [](const Setting& e) {
        g_bonuses.push_back(BonusInfo{0xb, 0, 0, 1});
        BonusInfo& b = g_bonuses.back();
        if (e.GetChild(0).IsString() && std::strcmp(e.GetChild(0).GetString(), "ENTITY") == 0) {
            b.entity = (uint32_t)e.GetChild(1).GetInt();
            return;
        }
        b.item = e.GetChild(0).IsNumber() ? (uint32_t)e.GetChild(0).GetInt() : 0;
        b.type = e.GetChild(0).IsString() ? (uint32_t)GameState::StringToResourceType(e.GetChild(0).GetString()) : 0xb;
        b.amount = (uint32_t)e.GetChild(1).GetInt();
    };
    if (days - 1 < 4) {
        char key[64];
        std::snprintf(key, sizeof key, "new_daily_bonus_%d", (int)days);
        Setting s(key);
        if (s.GetChild(0).GetChildrenCount() == 0) add(s);
        else
            for (unsigned i = 0; i < s.GetChildrenCount(); ++i) add(s.GetChild(i));
    } else if (days == 5) {
        g_bonuses.push_back(BonusInfo{0xb, 0x2000, 0, 1});
    }
    int social = 0;
    if (GameState::GetFacebookConnection()) social = Setting("connect_fb_bonus").GetInt();
    if (GameState::GetTwitterConnection()) social += Setting("connect_tw_bonus").GetInt();
    if (social != 0) g_bonuses.push_back(BonusInfo{8, 0, 0, (uint32_t)social});
    // PORT (docs/balance_plan.md, user 2026-10-19): every bonus day also gives crystals,
    // port_daily_crystals + level / port_daily_crystals_div (3 + level / 5), shown in the row.
    if (days - 1 < 5) {
        const float div = GameState::GetSetting("port_daily_crystals_div");
        const int crystals = (int)GameState::GetSetting("port_daily_crystals") +
                             (div > 0.f ? (int)((float)GameState::GetLevel() / div) : 0);
        if (crystals > 0) g_bonuses.push_back(BonusInfo{GameState::kCrystal, 0, 0, (uint32_t)crystals});
    }
    g_bonusCount = (unsigned)g_bonuses.size();

    for (unsigned i = 0; i != 3; ++i) {
        ItemIconHolder& h = g_items[i];
        h.root->MoveWindow(-g_bottomShift, 0);
        if (i >= g_bonusCount) {
            h.root->SetVisibility(false);
            continue;
        }
        h.root->SetVisibility(true);
        const BonusInfo& b = g_bonuses[i];
        if (b.type != 0xb) {
            h.icon->SetTexture(IconManager::GetIcon(GameState::GetResourceMapIconName((int)b.type), false), true);
        } else if (b.item >= 0x2000) {
            h.icon->SetTexture(IconManager::GetIcon("chest_small", false), true);
        } else if (Items::ItemInfo* info = Items::GetItemInfo(b.item)) {
            h.icon->SetTexture(info->GetIcon(), true);
        } else if (EntityData* data = EntityFactory::GetEntityByID((int)b.entity)) {
            h.icon->SetTexture(data->side0, true);
        }
        h.quantity->SetVisibility(b.amount > 1);
        h.quantity->SetText(ToWideString((int)b.amount));
    }
    g_bottomShift = g_bonusCount < 3 ? (int)(g_items[0].root->w * (3 - g_bonusCount)) >> 1 : 0;
    for (unsigned i = 0; i != 3; ++i) g_items[i].root->MoveWindow(g_bottomShift, 0);
    g_collectText->SetText(StringTable::GetString(days == 5 ? "DAILY_BONUS_CLAIM_CHEST" : "DAILY_BONUS_CLAIM_BONUS"));
}

}  // namespace DailyBonusWindow
