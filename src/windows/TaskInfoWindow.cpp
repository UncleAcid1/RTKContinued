// TaskInfoWindow: a quest's window (Task_info_win.xml): who gives it and its text, up to three
// step boxes (icon, text, count, the action button or "skip for crystals", or "done"), and the
// reward strip. The window shrinks to the number of steps. Port of TaskInfoWindow
// (libkingdom.so 5.11), 0x359dc8..0x35e9xx. Statics at 0x62f9e4.
//
// Not ported, marked where they happen: the boss form of the window (a boss's portrait, fight /
// shop buttons, OnFight: milestone 4g), the task locator icon (4d part 4), ExecuteFirstHelp
// (0x35d3dc, the autoplay bot: 4d part 4), the windows Show opens for a few special tasks (chests,
// PvP, item shop: milestone 5), Locations::GetMapName (4f), sounds (6).
#include <cstdio>
#include <vector>

#include "engine/IconManager.h"
#include "engine/Render.h"
#include "engine/Resources.h"
#include "engine/Text.h"
#include "engine/Timer.h"
#include "game/BuildingHovers.h"
#include "game/Contracts.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/Map.h"
#include "game/MetaData.h"
#include "game/StringTable.h"
#include "game/Tasks.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/ContentScroller.h"
#include "hud/HUD.h"
#include "hud/ItemHoverWindow.h"
#include "windows/Windows.h"

namespace TaskInfoWindow {
namespace {

using GUI::Button;
using GUI::Textfield;
using GUI::Window;

struct StepBox {                 // 0x68 bytes, "task_box_%02d"
    Window* root = nullptr;              // +0x00
    Window* iconHolder = nullptr;        // +0x04 icon_holder_round_task (the icon's mask)
    Window* ringIncomplete = nullptr;    // +0x08 quest_holder_battle_ring_incomplete_bg
    Window* ringComplete = nullptr;      // +0x0c quest_holder_battle_ring_complete_bg
    Window* ringCity = nullptr;          // +0x10 quest_holder_city_ring_bg
    Window* ringBattle = nullptr;        // +0x14 quest_holder_battle_ring_bg
    Window* ringBoss = nullptr;          // +0x18 quest_holder_boss_ring_bg
    Window* icon = nullptr;              // +0x1c info_icon_box
    Textfield* status = nullptr;         // +0x20 text_status
    Textfield* statusDone = nullptr;     // +0x24 text_status_done
    Window* mapBox = nullptr;            // +0x28 tasks_map_name
    Textfield* mapName = nullptr;        // +0x2c tasks_map_name.task_map_name
    Textfield* header = nullptr;         // +0x30 task_description_header (with the map's name above)
    Textfield* headerCentered = nullptr; // +0x34 task_description_header_centered
    Window* perform = nullptr;           // +0x38 button_perform
    Textfield* performText = nullptr;    // +0x3c
    Window* performIcon = nullptr;       // +0x40
    Button* performClick = nullptr;      // +0x44
    Window* skipBox = nullptr;           // +0x48 container_task_skip
    Textfield* smallText = nullptr;      // +0x4c button_perform_small.text_price
    Window* smallIcon = nullptr;         // +0x50
    Button* smallClick = nullptr;        // +0x54
    Textfield* skipText = nullptr;       // +0x58 button_skip_quest.text_upgrade
    Button* skipClick = nullptr;         // +0x5c
    Window* doneIcon = nullptr;          // +0x60 task_done_icon
    Textfield* doneText = nullptr;       // +0x64
};

struct RewardInfo {              // 0x20 bytes, "task_reward_holder.item_holder_%02d"
    Window* root = nullptr;      // +0x00
    Window* icon = nullptr;      // +0x04 icon_60_on
    Textfield* name[5] = {};     // +0x08 text_item_class_01..05 (one per item level)
    Textfield* quantity = nullptr;   // +0x1c
};

struct AwardItem {               // 0x14 bytes
    bool resource = false;       // +0x00
    int type = 0;                // +0x04 the resource (0xb for an item)
    uint32_t item = 0;           // +0x08
    int level = 0;               // +0x0c the item's level: which name text shows
    unsigned amount = 0;         // +0x10
};

WindowManager::FunctionalWindow* g_queue = nullptr;   // TaskInfoWindow::wnd
Window* g_root = nullptr;                // 0x62f9e4
GUI::TweenEffect* g_tween = nullptr;     // 0x62f9e8
Entity* g_boss = nullptr;                // 0x62f9ec SetFightBossCallback
void (*g_onContinue)() = nullptr;        // 0x62f9f0
Shared::ContentScroller g_scroller;      // 0x62f9f4 the rewards
Window* g_rewardClip = nullptr;          // 0x62fa74 scaler_reward
RewardInfo g_rewards[6];                 // 0x62fb00
std::vector<AwardItem> g_awards;         // 0x62fbc0
ItemHoverWindow* g_hover = nullptr;      // 0x62fbe0
uint32_t g_task = 0;                     // 0x62fbe4
Textfield* g_text = nullptr;             // 0x62fbe8 text_01
Render::Font* g_smallFont = nullptr;     // 0x62fbec the text's font two (four) points smaller
Textfield* g_bossText = nullptr;         // 0x62fbf0 boss_task_info.text_01
StepBox g_boxes[3];                      // 0x62fbf4
bool g_bossMode = false;                 // 0x62fd2c
Window* g_bossIcon = nullptr;            // 0x62fd30 boss_task_info.icon_96
int g_cut = 0;                           // 0x62fd34 pixels the window is shorter by
int g_rewardShift = 0;                   // 0x62fd38 centres fewer than six rewards
Window* g_giverMale = nullptr;           // 0x62fd3c
Window* g_giverFemale = nullptr;         // 0x62fd40
Textfield* g_title = nullptr;            // 0x62fd44 text_player_name
Window* g_bubble = nullptr;              // 0x62fd48 dialog_hint_window_9s
Window* g_bubbleArrow = nullptr;         // 0x62fd4c dialog_arrow_sw
Window* g_bossInfo = nullptr;            // 0x62fd50 boss_task_info
Textfield* g_bossTimeLabel = nullptr;    // 0x62fd58
Textfield* g_bossTime = nullptr;         // 0x62fd5c
unsigned g_awardCount = 0;               // 0x62fd60
Window* g_rewardHolder = nullptr;        // 0x62fd64 task_reward_holder
Textfield* g_rewardTitle = nullptr;      // 0x62fd68
Window* g_left = nullptr;                // 0x62fd6c button_navigate_left
Window* g_right = nullptr;               // 0x62fd70
Window* g_fight = nullptr;               // 0x62fd74 button_boss_boss_01
Window* g_fightIcon = nullptr;           // 0x62fd78
Textfield* g_fightText = nullptr;        // 0x62fd7c
Window* g_shop = nullptr;                // 0x62fd80 button_boss_boss_02
Window* g_shopIcon = nullptr;            // 0x62fd84
Textfield* g_shopText = nullptr;         // 0x62fd88
int g_rootHeight = 0;                    // 0x62fd8c the layout's height
Window* g_back = nullptr;                // 0x62fd90 back_light_blue
int g_backW = 0, g_backH = 0;            // 0x62fd94 0x62fd98 its layout size
uint32_t g_lastSecond = 0;               // 0x62fda0
Button* g_close = nullptr;               // 0x62fda8
float g_scale = 1.f;                     // 0x60f410
int g_pendingHelp = -1;                  // 0x60f40c the step whose button was pressed
bool g_resetArrowLimit = true;           // 0x60f408

void OnTweenOut();

// @0x35a1c4 TaskInfoWindow::UpdateDescription
void UpdateDescription() {
    if (!g_root->visibleSelf) return;
    Tasks::TaskInfo* t = Tasks::GetTask(g_task);
    if (!t) return;
    // UNVERIFIED: GetUndecoratedString is given the task here: its three steps' amounts fill the
    // text's first "%d" slots (StringTable.h).
    std::u32string text = t->description ? StringTable::GetUndecoratedString(t->description, 0x200)
                                         : SWPrintf(0x200, U"None", {});
    if (g_text->visibleSelf) g_text->SetText(text.c_str(), nullptr, StringTable::GetLangID() == 0 ? nullptr : g_smallFont);
    if (g_bossText->visibleSelf) g_bossText->SetText(text.c_str());
}

// @0x35a648 TaskInfoWindow::OnHelpActual: the step's action button, once the window has closed.
void OnHelpActual(unsigned step) {
    // UNVERIFIED (milestone 5): in tutorial step 0x61 HireTroopsWindow::ShowForBuilding(0x8f) instead.
    Tasks::TaskInfo* t = Tasks::GetTask(g_task);
    if (!t) return;
    Tasks::TaskSubtask& s = t->subtasks[step];
    if (Map::GetCurrentFarm() && !t->isFarm) {
        // Off the farm, unless the step collects an order of this very farm.
        const Contracts::Contract* c = s.type == Tasks::kDeliveryMissionEnd ? Contracts::GetContract(s.value / 10) : nullptr;
        Map::Building* b = c ? Map::GetBuildingWithContract(c, true) : nullptr;
        if (!b || b != Map::GetCurrentFarm()) HUDWindow::ExitFarm();
    }
    Tasks::SetLastSubTaskHelp(s.id);
    // UNVERIFIED (4d part 4): HUDWindow::SetTaskLocaterIcon(s.GetIcon(), showInBattle, sidequest, quickTask).
    if (s.type != Tasks::kEnterMap && t->id != 0x5c9) {
        // (GlobalMapWindow::Hide here: milestone 5; a city task met outside the city also
        // refreshes the quest list)
        if (t->isCity && GameState::GetCurrentMapID() == 0) TaskHolderWindow::UpdateTasks();
        else if (t->mapId != 0 && GameState::GetCurrentMapID() != Tasks::NormalizeMapID(t->mapId)) {
            Tasks::HelpWithMapSwitchTo(Tasks::NormalizeMapID(t->mapId), true, false, 0);
            return;
        }
    }
    // UNVERIFIED (milestone 4g): with a boss set, a "kill" step for it starts the fight (OnFight).
    if (GameState::GetSubTaskDone(s.id) < s.amount && s.OnHelp(!t->dnAssist)) {
        if (g_resetArrowLimit) BuildingHovers::SetArrowVisibleWindowLimit(1);
        return;
    }
    if (s.type == Tasks::kEnterMap || t->debugMap == 0) return;
    if (GameState::GetCurrentMapID() == Tasks::NormalizeMapID(t->debugMap)) return;
    Tasks::HelpWithMapSwitchTo(Tasks::NormalizeMapID(t->debugMap), true, false, 0);
}

// @0x35a490 TaskInfoWindow::Hide
void HideWindow() {
    if (g_root->visibleSelf) g_tween->AnimateOut();
    // (Render::Texture::SetMagnifyFilter(true) on the step icons' textures: the port's textures
    // always filter.)
}

// @0x35adec TaskInfoWindow::OnHelp: the button closes the window first.
void OnHelp(unsigned step) {
    g_pendingHelp = (int)step;
    HideWindow();
}

// @0x35acf4 TaskInfoWindow::OnContinue (the close button)
void OnContinue() {
    void (*cb)() = g_onContinue;
    g_onContinue = nullptr;
    if (cb) cb();
    // UNVERIFIED (milestone 4g): with a boss set (not task 0x5b7) the hero's AI is told (vfunc +0xe8).
    if (g_task == 0x49f) {
        TaskCompleteWindow::Complete(0x49f);
        TaskCompleteWindow::Show();
    }
    HideWindow();
}

// @0x35d174 TaskInfoWindow::OnSubtaskSkip: the step is finished for crystals.
void OnSubtaskSkip(unsigned box) {
    Tasks::TaskInfo* t = Tasks::GetTask(g_task);
    if (!t) return;
    unsigned n = 0;
    for (Tasks::TaskSubtask& s : t->subtasks) {
        if (s.type == 0) continue;
        if (box == n) {
            int price = GameState::AdjustCrystalCost((int)s.skipPrice);
            if ((int)GameState::GetResourceAmount(GameState::kCrystal) < price) {
                NotEnoughWindow::ResetRequirements();
                NotEnoughWindow::AddRequirement(GameState::kCrystal, (unsigned)price);
                NotEnoughWindow::CheckRequirements();
                NotEnoughWindow::Show();
                return;
            }
            int done = GameState::GetSubTaskDone(s.id);
            // (the balance steps are marked done for good)
            if (s.type == Tasks::kBallance || s.type == Tasks::kBallanceItem || s.type == Tasks::kCompleteCompaign)
                Tasks::CompleteSubtaskDirect(s.id, 0x2000 - done);
            else
                Tasks::CompleteSubtaskDirect(s.id, s.amount - done);
            // UNVERIFIED (milestone 6): SoundsManager::PlaySound("ui_buy_with_crystals").
            GameState::ChangeResourceAmount(GameState::kCrystal, -price);
            // (Billing::LogCBPurchase(10, step, price): online logging, not ported.)
            Map::Save(0);
        }
        ++n;
    }
    bool all = true;
    for (const Tasks::TaskSubtask& s : t->subtasks)
        if (s.type != 0 && GameState::GetSubTaskDone(s.id) < s.amount) all = false;
    if (all) {
        HideWindow();
        // UNVERIFIED (milestone 5): task 0x5c9 on the world map moves the sultan on
        // (GlobalMapWindow::ShowSultanMovement / SwitchMapAnimated(0xb)).
    }
    UpdateContents();
}

// @0x35a058 TaskInfoWindow::OnItem: a reward's popup.
void OnItem(unsigned cell) {
    unsigned i = cell + g_scroller.firstIndex;
    if (i >= g_awards.size()) return;
    Items::ItemInfo* info = Items::GetItemInfo(g_awards[i].item);
    if (!info) return;
    // UNVERIFIED (milestone 6): SoundsManager::PlaySound("ui_click").
    g_hover->Show();
    g_hover->SetItem(StringTable::GetString("BELT_ITEM"), nullptr, info, false, false, nullptr, true, false, false, false);
    Window* r = g_rewards[cell].root;
    int x = g_root->x + r->x, y = g_root->y + r->y;
    g_hover->SetTargetRect(x, y, x + r->w, y + r->h, ItemHoverWindow::kN);
    g_hover->RecomputePosition();
    g_hover->SetZ(0.001f);
    Render::SortRenderLayer(Render::kLayerGUI, 1);
}

void OnLeft() { g_scroller.bounce += (float)g_scroller.itemSize; }    // @0x359eb0
void OnRight() { g_scroller.bounce -= (float)g_scroller.itemSize; }   // @0x359ed4

// @0x35a8e0 TaskInfoWindow::OnTweenOut
void OnTweenOut() {
    if (!g_queue->shown) return;
    WindowManager::WindowHide(false);
    g_queue->shown = false;
    g_queue->MoveWindowDown(true);
    if (g_pendingHelp != -1) {
        OnHelpActual((unsigned)g_pendingHelp);
        g_pendingHelp = -1;
    }
    g_boss = nullptr;
    Tasks::SetBlocked(false);
    if (g_resetArrowLimit) BuildingHovers::SetArrowVisibleWindowLimit(0);
    g_resetArrowLimit = true;
    // UNVERIFIED (milestone 5): GlobalMapWindow::SetArrowVisibleWindowLimit(1).
    GameState::DropGamePauseState();
}

// @0x35ad90 TaskInfoWindow::OnBack
bool OnBack() {
    if (!g_queue->shown) return false;
    if (!g_hover->shownHover) OnContinue();
    else g_hover->Hide();
    return true;
}

}  // namespace

WindowManager::FunctionalWindow* Queue() {
    if (!g_queue) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.deinit = Deinit;
        f.setZ = SetZ;
        f.click = Click;
        f.move = [](int x, int y) { g_scroller.Move(x, y); };   // Move @0x35ae1c
        f.hide = Hide;
        g_queue = new WindowManager::FunctionalWindow("TaskInfoWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_queue && g_queue->shown; }          // @0x359e54
void BlockArrowLimitChangeOnce() { g_resetArrowLimit = false; } // @0x359e68
void SetFightBossCallback(Entity* boss) { g_boss = boss; }      // @0x359e80
void SetOnContinue(void (*cb)()) { g_onContinue = cb; }         // @0x359e9c
void Hide() { HideWindow(); }                                   // @0x35a490

// @0x35ae00 TaskInfoWindow::TaskTimeout
void TaskTimeout(uint32_t task) {
    if (g_task == task) HideWindow();
}

// @0x35a9a0 TaskInfoWindow::ForceHide
void ForceHide() {
    g_root->SetVisibility(false);
    OnTweenOut();
}

// @0x35d514 TaskInfoWindow::Init
void Init() {
    g_task = 0;
    g_cut = 0;
    g_rootHeight = 0;
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Task_info_win.xml", "Task_info_win.png", -1.f, 0, 0, 0,
                             0x32, false, 1.f);
    g_queue->RegisterTopWindow(g_root);
    g_scale = g_root->scale;
    g_root->SetVisibility(false);
    g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::GetVerticalCenter(g_root->h));
    g_tween = GUI::CreateTweenEffect(g_root, nullptr, [] { Update(0.f); }, OnTweenOut);   // (OnTweenIn @0x35d16c)
    g_tween->CenterWith(0, (int)(g_scale * -40.f), 0, (int)(g_scale * 20.f));
    g_close = GUI::GetWindowTyped<Button>(g_root, "x_button.clickArea");
    g_close->SetOnClick(OnContinue);
    g_close->SetActionSound("ui_close", 1);
    g_back = GUI::GetWindowTyped<Window>(g_root, "back_light_blue");
    g_backH = g_back->origH;
    g_backW = g_back->origW;
    g_giverMale = GUI::GetWindowTyped<Window>(g_root, "quest_giver_male");
    g_giverFemale = GUI::GetWindowTyped<Window>(g_root, "quest_giver_female");
    g_title = GUI::GetWindowTyped<Textfield>(g_root, "text_player_name");
    g_bubble = GUI::GetWindowTyped<Window>(g_root, "dialog_hint_window_9s");
    g_bubbleArrow = GUI::GetWindowTyped<Window>(g_root, "dialog_arrow_sw");
    g_text = GUI::GetWindowTyped<Textfield>(g_root, "text_01");
    g_bossInfo = GUI::GetWindowTyped<Window>(g_root, "boss_task_info");
    g_bossIcon = GUI::GetWindowTyped<Window>(g_root, "boss_task_info.icon_96");
    g_bossIcon->takesZ = true;
    g_bossText = GUI::GetWindowTyped<Textfield>(g_root, "boss_task_info.text_01");
    g_bossTimeLabel = GUI::GetWindowTyped<Textfield>(g_root, "boss_task_info.text_time_to_heal");
    g_bossTime = GUI::GetWindowTyped<Textfield>(g_root, "boss_task_info.text_time_time_time");
    for (unsigned i = 0; i < 3; ++i) {
        StepBox& b = g_boxes[i];
        int n = (int)i + 1;
        b.root = GUI::GetWindowTypedF<Window>(g_root, "task_box_%02d", n);
        b.root->SetOnClick([i] { OnHelp(i); });
        b.iconHolder = GUI::GetWindowTypedF<Window>(g_root, "task_box_%02d.icon_holder_round_task", n);
        b.ringIncomplete = GUI::GetWindowTypedF<Window>(
            g_root, "task_box_%02d.icon_holder_round_task.quest_holder_battle_ring_incomplete_bg", n);
        b.ringComplete = GUI::GetWindowTypedF<Window>(
            g_root, "task_box_%02d.icon_holder_round_task.quest_holder_battle_ring_complete_bg", n);
        b.ringCity = GUI::GetWindowTypedF<Window>(g_root, "task_box_%02d.icon_holder_round_task.quest_holder_city_ring_bg", n);
        b.ringBattle = GUI::GetWindowTypedF<Window>(g_root, "task_box_%02d.icon_holder_round_task.quest_holder_battle_ring_bg", n);
        b.ringBoss = GUI::GetWindowTypedF<Window>(g_root, "task_box_%02d.icon_holder_round_task.quest_holder_boss_ring_bg", n);
        b.icon = GUI::GetWindowTypedF<Window>(g_root, "task_box_%02d.icon_holder_round_task.info_icon_box", n);
        b.icon->takesZ = true;
        b.mapBox = GUI::GetWindowTypedF<Window>(g_root, "task_box_%02d.tasks_map_name", n);
        b.mapName = GUI::GetWindowTypedF<Textfield>(g_root, "task_box_%02d.tasks_map_name.task_map_name", n);
        b.status = GUI::GetWindowTypedF<Textfield>(g_root, "task_box_%02d.text_status", n);
        b.statusDone = GUI::GetWindowTypedF<Textfield>(g_root, "task_box_%02d.text_status_done", n);
        b.header = GUI::GetWindowTypedF<Textfield>(g_root, "task_box_%02d.task_description_header", n);
        b.headerCentered = GUI::GetWindowTypedF<Textfield>(g_root, "task_box_%02d.task_description_header_centered", n);
        b.perform = GUI::GetWindowTypedF<Window>(g_root, "task_box_%02d.button_perform", n);
        b.performText = GUI::GetWindowTypedF<Textfield>(g_root, "task_box_%02d.button_perform.text_price", n);
        b.performIcon = GUI::GetWindowTypedF<Window>(g_root, "task_box_%02d.button_perform.icon_action", n);
        b.performIcon->takesZ = true;
        b.performClick = GUI::GetWindowTypedF<Button>(g_root, "task_box_%02d.button_perform.clickArea", n);
        b.performClick->SetOnClick([i] { OnHelp(i); });
        b.skipBox = GUI::GetWindowTypedF<Window>(g_root, "task_box_%02d.container_task_skip", n);
        b.smallText = GUI::GetWindowTypedF<Textfield>(g_root, "task_box_%02d.container_task_skip.button_perform_small.text_price", n);
        b.smallIcon = GUI::GetWindowTypedF<Window>(g_root, "task_box_%02d.container_task_skip.button_perform_small.icon_action", n);
        b.smallIcon->takesZ = true;
        b.smallClick = GUI::GetWindowTypedF<Button>(g_root, "task_box_%02d.container_task_skip.button_perform_small.clickArea", n);
        b.smallClick->SetOnClick([i] { OnHelp(i); });
        b.skipText = GUI::GetWindowTypedF<Textfield>(g_root, "task_box_%02d.container_task_skip.button_skip_quest.text_upgrade", n);
        b.skipClick = GUI::GetWindowTypedF<Button>(g_root, "task_box_%02d.container_task_skip.button_skip_quest.clickArea", n);
        b.skipClick->SetOnClick([i] { OnSubtaskSkip(i); });
        b.doneIcon = GUI::GetWindowTypedF<Window>(g_root, "task_box_%02d.task_done_icon", n);
        b.doneText = GUI::GetWindowTypedF<Textfield>(g_root, "task_box_%02d.task_done_icon.text_completed", n);
    }
    g_fight = GUI::GetWindowTypedF<Window>(g_root, "button_boss_boss_%02d", 1);
    g_fightIcon = GUI::GetWindowTypedF<Window>(g_root, "button_boss_boss_%02d.button_boss_task_button.icon_60_boss", 1);
    g_fightIcon->takesZ = true;
    g_fightText = GUI::GetWindowTypedF<Textfield>(g_root, "button_boss_boss_%02d.button_boss_task_button.text_upgrade", 1);
    g_shop = GUI::GetWindowTypedF<Window>(g_root, "button_boss_boss_%02d", 2);
    g_shopIcon = GUI::GetWindowTypedF<Window>(g_root, "button_boss_boss_%02d.button_boss_task_button.icon_60_boss", 2);
    g_shopIcon->takesZ = true;
    g_shopText = GUI::GetWindowTypedF<Textfield>(g_root, "button_boss_boss_%02d.button_boss_task_button.text_upgrade", 2);
    g_fight->SetOnClick([] {});   // UNVERIFIED (milestone 4g): OnFight (0x35a548)
    g_shop->SetOnClick([] {});    // UNVERIFIED (milestone 5): OnShop (0x35a4f8) (the item shop's tab 3)
    g_fight->MoveWindow(0, (int)(g_scale * 20.f));
    g_shop->MoveWindow(0, (int)(g_scale * 20.f));
    g_rewardHolder = GUI::GetWindowTyped<Window>(g_root, "task_reward_holder");
    g_rewardTitle = GUI::GetWindowTyped<Textfield>(g_root, "task_reward_holder.text_rewards");
    g_left = GUI::GetWindowTyped<Window>(g_root, "task_reward_holder.button_navigate_left");
    g_right = GUI::GetWindowTyped<Window>(g_root, "task_reward_holder.button_navigate_right");
    GUI::GetWindowTyped<Button>(g_root, "task_reward_holder.button_navigate_left.clickArea")->SetOnClick(OnLeft);
    GUI::GetWindowTyped<Button>(g_root, "task_reward_holder.button_navigate_right.clickArea")->SetOnClick(OnRight);
    for (unsigned i = 0; i < 6; ++i) {
        RewardInfo& r = g_rewards[i];
        int n = (int)i + 1;
        r.root = GUI::GetWindowTypedF<Window>(g_root, "task_reward_holder.item_holder_%02d", n);
        r.root->SetOnClick([i] { OnItem(i); });
        r.icon = GUI::GetWindowTypedF<Window>(g_root, "task_reward_holder.item_holder_%02d.icon_60_on", n);
        r.icon->takesZ = true;
        for (int k = 0; k < 5; ++k)
            r.name[k] = GUI::GetWindowTypedF<Textfield>(g_root, "task_reward_holder.item_holder_%02d.text_item_class_%02d", n, k + 1);
        r.quantity = GUI::GetWindowTypedF<Textfield>(g_root, "task_reward_holder.item_holder_%02d.text_quantity", n);
    }
    g_scroller.Init();
    g_rewardClip = GUI::GetWindowTyped<Window>(g_root, "task_reward_holder.scaler_reward");
    g_scroller.viewport = {g_rewardClip->x, g_rewardClip->y, g_rewardClip->w + g_rewardClip->x, g_rewardClip->h + g_rewardClip->y};
    g_scroller.track = g_rewardClip;   // (+0x80)
    for (RewardInfo& r : g_rewards) {
        r.root->SetClipRect(&g_scroller.viewport);
        g_scroller.windows.push_back(r.root);
    }
    g_scroller.itemSize = g_rewards[1].root->x - g_rewards[0].root->x;
    g_scroller.onScroll = UpdateContents;
    g_scroller.viewport.left += 3;
    g_scroller.viewport.right -= 3;
    g_scroller.unkA0 = 6;
    g_scroller.viewport.top = 0;
    g_scroller.horizontal = true;
    g_scroller.viewport.bottom = 0x800;
    g_scroller.perLine = 1;
    g_back->SetSize((unsigned)g_back->w, (unsigned)((int)(g_scale * 20.f) + g_back->h));
    g_backH = g_back->origH;
    g_rootHeight = g_root->origH;
    g_queue->fn.update = Update;
    g_queue->fn.zRange = WindowManager::ReturnMediumZRange;
    g_queue->fn.back = OnBack;
    g_queue->fn.show = Show;
    // A smaller font for the long quest texts (not for Korean, language 7).
    if (StringTable::GetLangID() != 7)
        g_smallFont = Render::CreateFont(GUI::GetFontFile(), g_text->font->size - (GUI::IsHighDPIVersion() ? 4 : 2));
    g_hover = new ItemHoverWindow();
}

// @0x35af38 TaskInfoWindow::Deinit
void Deinit() {
    GUI::RemoveTweenEffect(g_tween);
    g_tween = nullptr;
    delete g_hover;
    g_hover = nullptr;
    delete g_root;
    g_root = nullptr;
}

// @0x359dc8 TaskInfoWindow::SetZ
void SetZ(float z) {
    if (!g_queue->shown) return;
    g_root->SetZ(z);
    g_root->SetPosition(g_tween->GetCenteredX(), g_tween->GetCenteredY());
}

// @0x35ae38 TaskInfoWindow::Click
bool Click(int x, int y, bool pressed) {
    if (g_root->visibleSelf) {
        if (BuildingHovers::ClickOnArrow(x, y, pressed)) return true;
        if (!pressed) BuildingHovers::HideArrow();
        if (g_scroller.Click(x, y, pressed, g_root)) return true;
    }
    if (TopCityWindow::Click(x, y, pressed)) return true;
    if (g_hover->Click(x, y, pressed)) return true;
    bool visible = g_root->visibleSelf;
    g_root->Click(x, y, pressed, false);
    return visible;
}

// @0x35afb4 TaskInfoWindow::SetTask: the window is filled for the task (boss: the boss form).
void SetTask(uint32_t id, bool boss) {
    bool first = g_task != id;   // a new task: the window is laid out again
    if (first) {
        g_scroller.firstIndex = 0;
        g_root->RescaleWindow(1.f);
        g_root->RestorePosition();
        g_rewardShift = 0;
        g_cut = 0;
        std::printf("TaskInfoWindow::SetTask() Setup for task %d\n", id);
    }
    g_bossMode = boss;
    g_task = id;
    Tasks::TaskInfo* t = Tasks::GetTask(id);
    if (!t) {
        std::printf("TaskInfoWindow::SetTask() Failed to find task %d information\n", id);
        HideWindow();
        return;
    }
    // UNVERIFIED (milestone 4g): task 0x5b7 takes the boss at spawn point 0x4e4 and the boss form.
    g_giverMale->SetVisibility(!g_bossMode && GameState::IsMalePlayer());
    g_giverFemale->SetVisibility(!g_bossMode && !GameState::IsMalePlayer());
    g_title->SetText(t->name);
    g_bubble->SetVisibility(!g_bossMode);
    g_bubbleArrow->SetVisibility(!g_bossMode);
    g_text->SetVisibility(!g_bossMode);
    g_bossInfo->SetVisibility(g_bossMode);
    g_bossText->SetVisibility(g_bossMode);
    UpdateDescription();
    // UNVERIFIED (milestone 4g): with a boss set its portrait (the "idle_1" animation) and the
    // time left of its fight ("MISSION_TIME_LEFT", GameState::GetBossFightStartTime).

    // The step boxes; the window is shorter by one box for every step the task does not have.
    const int boxHeight = g_boxes[0].root->h;
    int backExtra = 0x14;        // added to the background's height
    int cut = boxHeight * -2;    // (negative) added to the window's height
    for (int i = 0; i < 3; ++i) {
        StepBox& b = g_boxes[i];
        Tasks::TaskSubtask& s = t->subtasks[i];
        if (s.type == 0) {
            b.root->SetVisibility(false);
            continue;
        }
        if (i != 0) {
            backExtra += b.root->h;
            cut += b.root->h;
        }
        b.root->SetVisibility(true);
        b.icon->SetTexture(s.GetIcon(), true, 0, 0, false, 0);
        b.icon->ApplyMask(Resources::GetUIImage("mask_task_icon_holder.png", false, false));
        int done = GameState::GetSubTaskDone(s.id);
        bool hasSkip = s.skipPrice != 0;
        bool complete = s.amount <= done;
        // (a boss's own "kill" steps show neither count nor button)
        bool bossStep = g_bossMode && (s.type == Tasks::kKillMob || s.type == Tasks::kKillSpawn);
        std::u32string status;
        // UNVERIFIED (milestone 5): a reputation step (0x3e) counts
        // Professions::GetPlayerProfessionLevel(value) instead.
        if (!bossStep && s.type == Tasks::kCompleteCompaign) status = SWPrintf(0x100, U"%d%%", {std::min(done, 100)});
        else status = SWPrintf(0x100, U"%d/%d", {std::min(done, s.amount), s.amount});
        b.status->SetText(status.c_str());
        b.status->SetVisibility(!complete && !bossStep);
        b.statusDone->SetText(status.c_str());
        b.statusDone->SetVisibility(complete && !bossStep);
        if (s.type == Tasks::kBuyRealMoney) {
            b.status->SetVisibility(false);
            b.statusDone->SetVisibility(false);
        }
        b.ringIncomplete->SetVisibility(!complete || bossStep);
        b.ringComplete->SetVisibility(complete && !bossStep);
        b.ringBattle->SetVisibility(t->showInBattle && !g_bossMode);
        b.ringCity->SetVisibility(!t->showInBattle && !g_bossMode);
        b.ringBoss->SetVisibility(g_bossMode);
        // Where the step is done, when not here.
        uint32_t map = s.assistMap ? s.assistMap : t->mapId ? t->mapId : t->debugMap;
        bool showMap = false, centered = true;
        if (map == GameState::GetCurrentMapID()) {
            b.mapBox->SetVisibility(false);
        } else {
            showMap = map != 0;
            b.mapBox->SetVisibility(showMap);
            if (showMap) {
                // UNVERIFIED (milestone 4f): another map's name is Locations::GetMapName(map).
                b.mapName->SetText(map == 0x2f ? GameState::GetCastleName() : U"");
            }
            centered = map == 0;
        }
        std::u32string text = StringTable::GetUndecoratedString(StringTable::GetStringF("STASK%d", s.id), 0x100);
        b.header->SetVisibility(showMap);
        if (b.header->visibleSelf) b.header->SetText(text.c_str());
        b.headerCentered->SetVisibility(centered);
        if (b.headerCentered->visibleSelf) b.headerCentered->SetText(text.c_str());
        // The button is greyed until the steps this one requires are done.
        bool enabled = true;
        if (s.required)
            for (unsigned k = 0; k < s.required->GetChildrenCount() && enabled; ++k) {
                const MetaData* r = s.required->GetChild(k);
                if (r->type == MetaData::kNumber && r->GetInt() != 0)
                    enabled = GameState::SubTaskCompleted(t->id, (unsigned)r->GetInt());
            }
        b.perform->SetVisibility(!complete && !hasSkip && !bossStep);
        b.perform->SetEnabled(enabled);
        b.performText->SetText(s.buttonText);
        b.performIcon->SetTexture(s.buttonIcon, false, 0, 0, false, 0);
        b.skipBox->SetVisibility(!complete && hasSkip && !bossStep);
        b.smallText->SetText(s.buttonText);
        b.smallIcon->SetTexture(s.buttonIcon, false, 0, 0, false, 0);
        b.skipText->SetText(SWPrintf(0x100, U"%s %d", {StringTable::GetString("SKIP_BUTTON"),
                                                        GameState::AdjustCrystalCost((int)s.skipPrice)}).c_str());
        b.doneIcon->SetVisibility(complete && !bossStep);
        b.doneText->SetText(StringTable::GetString("TASK_SUBTASK_COMPLETE"));
    }
    g_cut = -cut;

    // The reward: crystals, XP, gold, the resources, then the items.
    g_awards.clear();
    static const int kOrder[11] = {9, 10, 8, 0, 1, 2, 3, 4, 5, 6, 7};   // 0x5a3858
    for (int type : kOrder)
        if (t->award[type] != 0) {
            AwardItem a;
            a.resource = true;
            a.type = type;
            a.amount = t->award[type];
            g_awards.push_back(a);
        }
    for (unsigned i = 0; i < t->itemCount; ++i) {
        AwardItem a;
        a.type = 0xb;
        a.item = t->items[i].id;
        const Items::ItemInfo* info = Items::GetItemInfo(a.item);
        a.level = info ? (int)info->itemLevel : 0;
        a.amount = t->items[i].count;
        g_awards.push_back(a);
    }
    g_awardCount = (unsigned)g_awards.size();
    if (t->doNotShowReward) g_awardCount = 0;
    g_rewardHolder->SetVisibility(g_awardCount != 0);
    if (g_awardCount != 0) backExtra += g_rewardHolder->h;
    else if (!g_bossMode) cut -= g_rewardHolder->h;
    g_rewardTitle->SetText(StringTable::GetString("text_reward"));
    g_scroller.itemCount = (unsigned)g_awards.size();
    g_left->SetEnabled(g_scroller.CanMoveLeft());
    g_right->SetEnabled(g_scroller.CanMoveRight());
    g_left->SetVisibility(g_scroller.CanMove());
    g_right->SetVisibility(g_scroller.CanMove());
    for (RewardInfo& r : g_rewards) r.root->MoveWindow(-g_rewardShift, 0);
    g_rewardShift = 0;
    unsigned shown = 0;
    for (; shown < 6; ++shown) {
        unsigned index = shown + g_scroller.firstIndex;
        if (g_awardCount <= index) break;
        RewardInfo& r = g_rewards[shown];
        const AwardItem& a = g_awards[index];
        r.icon->SetVisibility(true);
        for (int k = 0; k < 5; ++k) r.name[k]->SetVisibility(a.level == k);
        r.quantity->SetVisibility(true);
        if (!a.resource) {
            Items::ItemInfo* info = Items::GetItemInfo(a.item);
            if (!info) {
                r.icon->SetVisibility(false);
                for (int k = 0; k < 5; ++k) r.name[k]->SetVisibility(false);
                r.quantity->SetVisibility(false);
            } else {
                info->PutImageInHolder(r.icon);
                r.name[a.level]->SetText(info->title);
                r.quantity->SetText(ToWideString((int)a.amount));
            }
        } else {
            r.icon->SetTexture(IconManager::GetIcon(GameState::GetResourceMapIconName(a.type), false), true, 0, 0, false, 0);
            r.name[a.level]->SetText(GameState::GetResourceGameName(a.type));
            r.quantity->SetText(ToWideString((int)a.amount));
        }
    }
    // UNVERIFIED (milestone 4g): task 0x5b7 with its first step done hides the fight and shop buttons.
    g_fightIcon->SetTexture(IconManager::GetIcon("gold_attack", false), true, 0, 0, false, 0);
    g_fightText->SetText(StringTable::GetString("BOSS_FIGHT"));
    g_shopIcon->SetTexture(IconManager::GetIcon("gold_buy", false), true, 0, 0, false, 0);
    g_shopText->SetText(StringTable::GetString("SQ_SHOP"));
    if (first) {
        // The shortened window is scaled to the screen again and its parts follow.
        g_scale = GUI::GetScaleFactor(g_root->origW, g_rootHeight + 0x32 + cut, !GUI::IsTabletVersion(), 1.f);
        g_root->RescaleWindow(g_scale);
        g_tween->CenterWith(0, (int)(g_scale * -40.f), 0, 0);
        g_back->SetBorders(0x1e, 0x28, 0x1e, 0x20);
        g_back->SetSize((unsigned)(int)((float)g_backW * g_scale), (unsigned)(int)(g_scale * (float)(backExtra + g_backH)));
        g_rewardHolder->MoveWindow(0, (int)((float)-g_cut * g_scale));
        g_root->SetSize((unsigned)g_root->w, (unsigned)(int)((float)(cut + g_rootHeight) * g_scale));
        g_scroller.viewport.bottom = 0x800;
        g_scroller.viewport.left = g_rewardClip->x + 3;
        g_scroller.itemSize = g_rewards[1].root->x - g_rewards[0].root->x;
        g_scroller.viewport.top = 0;
        g_scroller.viewport.right = g_rewardClip->w + g_rewardClip->x - 3;
    }
    // Fewer than six rewards stand in the middle.
    if (g_awardCount != 0 && g_awardCount <= 5)
        g_rewardShift = (int)((float)(unsigned)((g_rewards[1].root->x - g_rewards[0].root->x) * (int)(5 - shown)) * 0.5f);
    for (RewardInfo& r : g_rewards) r.root->MoveWindow(g_rewardShift, 0);
    for (unsigned k = shown; k < 6; ++k) {
        g_rewards[k].icon->SetVisibility(false);
        for (int n = 0; n < 5; ++n) g_rewards[k].name[n]->SetVisibility(false);
        g_rewards[k].quantity->SetVisibility(false);
    }
    g_fight->SetVisibility(g_bossMode);
    g_shop->SetVisibility(g_bossMode);
    if (first) {
        g_fight->MoveWindow(0, (int)((float)backExtra * g_scale));
        g_shop->MoveWindow(0, (int)((float)backExtra * g_scale));
    }
    g_root->UpdatePosition();
}

// @0x35cecc TaskInfoWindow::UpdateContents
void UpdateContents() { SetTask(g_task, g_bossMode); }

// @0x35a9cc TaskInfoWindow::Show
void Show() {
    Tasks::TaskInfo* t = Tasks::GetTask(g_task);
    if (t && t->IsFeaturedTask()) {
        // UNVERIFIED (4d part 4): GameState::SetFeaturedTaskClickTime(id, now).
        HUDWindow::UpdateTasks();
    }
    // UNVERIFIED (milestone 5): the tasks that open another window instead: 0x6b4 and 0x75b the
    // chests (ChestsWindow), 0x77b the PvP button, 0x77c..0x780 the item shop's tabs and packs.
    if (!t || !t->quickTask) {
        bool wasShown = g_queue->shown;
        g_queue->shown = true;
        g_queue->MoveWindowOnTop(true);
        if (!g_root->visibleSelf) {
            WindowManager::WindowShow(false);
            g_tween->AnimateIn();
        }
        g_root->SetVisibility(true);
        g_bossIcon->SetVisibility(false);
        UpdateDescription();
        if (!wasShown && GameState::GetCurrentLocation() == 2) GameState::RaiseGamePauseState();
        g_root->PreloadTextures();
        return;
    }
    // A quick task has no window: its open step's text shows at the bottom and its action runs.
    unsigned step;
    if (GameState::GetSubTaskDone(t->subtasks[0].id) < t->subtasks[0].amount) step = 0;
    else if (GameState::GetSubTaskDone(t->subtasks[1].id) < t->subtasks[1].amount) step = 1;
    else if (GameState::GetSubTaskDone(t->subtasks[2].id) < t->subtasks[2].amount) step = 2;
    else return;
    std::u32string text =
        StringTable::GetUndecoratedString(StringTable::GetStringF("STASK%d", t->subtasks[step].id), 0x100);
    TaskHolderWindow::ShowQuickTaskDescription(text.c_str(), 2.f);
    OnHelpActual(step);
}

// @0x35cee4 TaskInfoWindow::Update
void Update(float dt) {
    if (!g_root->visibleSelf) return;
    // (the boss portrait's animation frames, 0.1 s each: milestone 4g)
    g_scroller.Update(dt);
    g_left->SetEnabled(g_scroller.CanMoveLeft());
    g_right->SetEnabled(g_scroller.CanMoveRight());
    g_left->SetVisibility(g_scroller.CanMove());
    g_right->SetVisibility(g_scroller.CanMove());
    if (g_hover->shownHover) {
        // The popup stays just above the window (a pop-up message goes in between).
        float z = PopupWindow::IsVisible() ? g_root->z - 0.01f : 0.001f;
        if (g_hover->GetZ() != z) {
            g_hover->SetZ(z);
            Render::SortRenderLayer(Render::kLayerGUI, 1);
        }
    }
    // Once a second the contents are made again (not while the window drops out).
    if (Timer::GetGlobalTime() != g_lastSecond && !(g_tween->active && g_tween->hideAtEnd)) {
        UpdateContents();
        g_lastSecond = Timer::GetGlobalTime();
    }
}

}  // namespace TaskInfoWindow
