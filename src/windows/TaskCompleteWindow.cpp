// TaskCompleteWindow: "Quest complete!" (Completed_win.xml): the quest's name and a strip of its
// reward (resources, gold, crystals, XP, items) with arrows to scroll it; an item cell opens the
// item's popup. The quest is completed (Tasks::CompleteTask, with the reward dropped) when the
// window closes; the quest checks are blocked while it is up.
// Port of TaskCompleteWindow (libkingdom.so 5.11), 0x352730..0x354368. Statics at 0x62dde4.
//
// PORT (online removed): the Facebook/Twitter share (WallPost::PostForEvent(4, task, ...), the
// ShareButtonsHolder ticks shown from level 3 and their toggles OnTwitterToggle (0x353bb4) /
// OnFacebookToggle (0x353bd4)) is not offered; the button is the plain "Ok".
#include <cstdio>
#include <vector>

#include "engine/SoundsManager.h"
#include "engine/IconManager.h"
#include "engine/Render.h"
#include "game/Entity.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/Setting.h"
#include "game/Spells.h"
#include "game/StringTable.h"
#include "game/Tasks.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/ContentScroller.h"
#include "hud/ItemHoverWindow.h"
#include "hud/Shared.h"
#include "windows/Windows.h"

namespace TaskCompleteWindow {
namespace {

using GUI::Button;
using GUI::Textfield;
using GUI::Window;

// One cell of the reward strip (TaskCompleteWindow::RewardInfo, 0x20 bytes), "item_holder_%02d".
struct RewardInfo {
    Window* root = nullptr;          // +0x00
    Window* icon = nullptr;          // +0x04 icon_60_on (takes a depth slot)
    Textfield* name[5] = {};         // +0x08 text_item_class_01..05: the name in the item level's colour
    Textfield* count = nullptr;      // +0x1c text_quantity
};

// A reward (0x14 bytes).
struct Reward {
    bool resource = false;           // +0x00
    int type = 0;                    // +0x04 the resource (0xb for an item)
    uint32_t item = 0;               // +0x08
    uint32_t itemLevel = 0;          // +0x0c
    uint32_t count = 0;              // +0x10
};

WindowManager::FunctionalWindow* g_queue = nullptr;   // TaskCompleteWindow::wnd
Shared::ContentScroller g_scroller;    // 0x62dde4
RewardInfo g_holders[5];               // 0x62def0
// 0x62df90: the rewards. The original keeps a filled count (0x62dfe4) beside the list's size
// (0x62dfac): an entry is added for every reward item, but a boost item (type 0x10) without the
// "item_boosts" setting is not filled in, and the next reward overwrites it.
std::vector<Reward> g_rewards;
int g_filled = 0;                      // 0x62dfe4
Spells::SpellInfo* g_lockedSpell = nullptr;   // 0x62dfb0 the spell the task unlocks (GetSpellLockedByTask)
uint32_t g_completed = 0;              // 0x62dfb4 the task the closing window has completed
ItemHoverWindow* g_hover = nullptr;    // 0x62dfb8
Window* g_root = nullptr;              // 0x62dfbc Completed_win.xml
Window* g_left = nullptr;              // 0x62dfc0 button_navigate_left
Window* g_right = nullptr;             // 0x62dfc4 button_navigate_right
GUI::TweenEffect* g_tween = nullptr;   // 0x62dfc8
Textfield* g_shareText = nullptr;      // 0x62dfcc lvl_up_share_holder.share.text_upgrade
Window* g_shareIcon = nullptr;         // 0x62dfd0 lvl_up_share_holder.share.icon_holder
Textfield* g_header = nullptr;         // 0x62dfd4 text_header
Textfield* g_bonus = nullptr;          // 0x62dfd8 text_level_bonus
uint32_t g_task = 0;                   // 0x62dfdc the task shown
Textfield* g_desc = nullptr;           // 0x62dfe0 text_level_desc: the task's name
Window* g_social = nullptr;            // 0x62dfe8 ShareButtonsHolder root, lvl_up_share_social_02
int g_centerShift = 0;                 // 0x62dffc the strip's shift when it holds fewer than 5
Shared::LargeLogoWindow g_logo;        // 0x62e000
Window* g_share = nullptr;             // 0x62e010 lvl_up_share_holder
Button* g_leftClick = nullptr;         // 0x62e014
Button* g_rightClick = nullptr;        // 0x62e018
bool g_tablet = false;                 // 0x60f3d0
float g_scale = 1.f;                   // 0x60f3d4

// The order of the rewards in the strip (0x5a343c): crystals, XP, gold, then the resources.
const int kOrder[11] = {9, 10, 8, 0, 1, 2, 3, 4, 5, 6, 7};

// @0x352744 / @0x352768: one cell along.
void OnLeft() { g_scroller.bounce += (float)g_scroller.itemSize; }
void OnRight() { g_scroller.bounce -= (float)g_scroller.itemSize; }

// @0x352a38: the item's popup, pointing at its cell.
void OnItem(unsigned i) {
    unsigned index = i + g_scroller.firstIndex;
    if (index >= g_rewards.size()) return;
    Items::ItemInfo* info = Items::GetItemInfo(g_rewards[index].item);
    if (!info) return;
    SoundsManager::PlaySound("ui_click", 1, false);
    g_hover->Show();
    g_hover->SetItem(StringTable::GetString("BELT_ITEM"), nullptr, info, false, false, nullptr, true, false, false,
                     false);
    Window* r = g_holders[i].root;
    int x = g_root->x + r->x, y = g_root->y + r->y;
    g_hover->SetTargetRect(x, y, x + r->w, y + r->h, ItemHoverWindow::kS);
    g_hover->RecomputePosition();
    g_hover->SetZ(0.001f);
    Render::SortRenderLayer(Render::kLayerGUI, 1);
}

// @0x352fb8. PORT: no wall posts (see the top of the file).
void OnShare() { Hide(); }

// @0x353048: Back closes the popup, else the window (as Ok).
bool OnBack() {
    if (!g_queue->shown) return false;
    if (!g_hover->shownHover) {
        OnShare();
        return true;
    }
    g_hover->Hide();
    return true;
}

// @0x3530a4: centred once the tween is over.
void SetZ(float z) {
    if (!g_queue->shown) return;
    g_root->SetZ(z);
    if (g_tween->active) return;
    g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::GetVerticalCenter(g_root->h));
}

// @0x35313c
void Move(int x, int y) { g_scroller.Move(x, y); }

// @0x353154
bool Click(int x, int y, bool pressed) {
    if ((!g_root->visibleSelf || !g_scroller.Click(x, y, pressed, g_root)) && !g_hover->Click(x, y, pressed)) {
        bool r = g_root->visibleSelf;
        g_root->Click(x, y, pressed, false);
        return r;
    }
    return true;
}

// @0x352a14
void OnTweenIn() {
    if (GameState::tutorial == 0x5f || GameState::tutorial == 0x65) GUI::SetInteractionLock(false);
}

// @0x3528e8: the tutorial steps move on and the quest checks run again.
void OnTweenOut() {
    if (GameState::tutorial == 0x5f) GameState::tutorial = 0x60;
    else if (GameState::tutorial == 0x65) GameState::tutorial = 0x66;
    if (GameState::secondTutorial == 0x208) GameState::secondTutorial = 0x20b;
    else if (GameState::secondTutorial == 0x210 || GameState::secondTutorial == 0x212)
        GameState::secondTutorial = 0x217;
    // A spell the task unlocked is shown in CraftSuccessWindow.
    if (g_lockedSpell) {
        CraftSuccessWindow::SetParameters(g_lockedSpell);
        CraftSuccessWindow::Show();
        g_lockedSpell = nullptr;
    }
    Tasks::SetBlocked(false);
    WindowManager::WindowHide(false);
    g_queue->shown = false;
    g_queue->MoveWindowDown(true);
    // A set completed (every bonus) opens the set window on it.
    if (const Tasks::TaskInfo* t = Tasks::GetTask(g_completed))
        for (const Tasks::TaskSubtask& s : t->subtasks)
            if (s.type == Tasks::kCollectSet && s.value % 10 == 3) {
                SetInfoWindow::SelectSet(s.value / 10);
                SetInfoWindow::Show();
            }
    GameState::DropGamePauseState();
}

// @0x352ba4: the arrows follow the strip; the popup stays above the window (under a PopupWindow);
// the window comes back on top when it is the only one shown.
void Update(float dt) {
    if (!g_queue->shown) return;
    g_scroller.Update(dt);
    g_left->SetEnabled(g_scroller.CanMoveLeft());
    g_right->SetEnabled(g_scroller.CanMoveRight());
    g_left->SetVisibility(g_scroller.CanMove());
    g_right->SetVisibility(g_scroller.CanMove());
    if (g_hover->shownHover) {
        float z = PopupWindow::IsVisible() ? g_root->z - 0.01f : 0.001f;
        if (g_hover->GetZ() != z) {
            g_hover->SetZ(z);
            Render::SortRenderLayer(Render::kLayerGUI, 1);
        }
    }
    if (WindowManager::GetShownWindowCount() == 1 && g_queue->prev)   // (not WindowQueue::head)
        g_queue->MoveWindowOnTop(true);
}

}  // namespace

WindowManager::FunctionalWindow* Queue() {
    if (!g_queue) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.deinit = Deinit;
        f.click = Click;
        f.setZ = SetZ;
        f.hide = Hide;
        f.move = Move;
        g_queue = new WindowManager::FunctionalWindow("TaskCompleteWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_queue->shown; }   // @0x352730

// @0x353c20
void Init() {
    g_task = 0;
    g_centerShift = 0;
    g_tablet = GUI::IsTabletVersion();
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Completed_win.xml", "Completed_win.png", -1.f, 0, 0, 0,
                             0, false, 1.f);
    g_queue->RegisterTopWindow(g_root);
    g_scale = g_root->scale;
    g_root->SetVisibility(false);
    g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::GetVerticalCenter(g_root->h));
    g_tween = GUI::CreateTweenEffect(g_root, nullptr, OnTweenIn, OnTweenOut);
    g_logo.LoadFrom(g_root, "logo_holder_large");
    g_logo.SetLanguage((unsigned)StringTable::GetLangID());
    g_share = GUI::GetWindowTyped<Window>(g_root, "lvl_up_share_holder");
    g_share->SetOnClick(OnShare);
    g_shareText = GUI::GetWindowTyped<Textfield>(g_root, "lvl_up_share_holder.share.text_upgrade");
    g_shareIcon = GUI::GetWindowTyped<Window>(g_root, "lvl_up_share_holder.share.icon_holder");
    g_shareIcon->takesZ = true;
    // ShareButtonsHolder::LoadFrom @0x3423d4 (its Twitter tick toggles OnTwitterToggle).
    // PORT: only its root, kept hidden.
    g_social = GUI::GetWindowTyped<Window>(g_root, "lvl_up_share_social_02");
    g_header = GUI::GetWindowTyped<Textfield>(g_root, "text_header");
    g_desc = GUI::GetWindowTyped<Textfield>(g_root, "text_level_desc");
    g_bonus = GUI::GetWindowTyped<Textfield>(g_root, "text_level_bonus");
    g_left = GUI::GetWindowTyped<Window>(g_root, "button_navigate_left");
    g_right = GUI::GetWindowTyped<Window>(g_root, "button_navigate_right");
    g_leftClick = GUI::GetWindowTyped<Button>(g_root, "button_navigate_left.clickArea");
    g_leftClick->SetOnClick(OnLeft);
    g_leftClick->SetActionSound("ui_click", 1);
    g_rightClick = GUI::GetWindowTyped<Button>(g_root, "button_navigate_right.clickArea");
    g_rightClick->SetOnClick(OnRight);
    g_rightClick->SetActionSound("ui_click", 1);
    for (unsigned i = 0; i < 5; ++i) {
        RewardInfo& h = g_holders[i];
        h.root = GUI::GetWindowTypedF<Window>(g_root, "item_holder_%02d", i + 1);
        h.root->SetOnClick([i] { OnItem(i); });
        h.icon = GUI::GetWindowTypedF<Window>(g_root, "item_holder_%02d.icon_60_on", i + 1);
        h.icon->takesZ = true;
        for (unsigned c = 0; c < 5; ++c)
            h.name[c] = GUI::GetWindowTypedF<Textfield>(g_root, "item_holder_%02d.text_item_class_%02d", i + 1, c + 1);
        h.count = GUI::GetWindowTypedF<Textfield>(g_root, "item_holder_%02d.text_quantity", i + 1);
    }
    g_scroller.Init();
    Window* view = GUI::GetWindowTyped<Window>(g_root, "scaler_transparent");
    g_scroller.viewport = {view->x, view->y, view->x + view->w, view->y + view->h};
    g_scroller.track = view;
    for (RewardInfo& h : g_holders) {
        h.root->SetClipRect(&g_scroller.viewport);
        g_scroller.windows.push_back(h.root);
    }
    g_scroller.viewport.bottom = 0x800;
    g_scroller.viewport.right -= 3;
    g_queue->fn.zRange = WindowManager::ReturnSmallZRange;
    g_scroller.unkA0 = 5;
    g_scroller.itemSize = g_holders[1].root->x - g_holders[0].root->x;
    g_scroller.onScroll = UpdateContents;
    g_scroller.viewport.left += 3;
    g_scroller.viewport.top = 0;
    g_scroller.horizontal = true;
    g_scroller.perLine = 1;
    g_queue->hideOnOuterClick = true;
    g_queue->fn.update = Update;
    g_queue->fn.back = OnBack;
    g_queue->fn.show = Show;
    g_hover = new ItemHoverWindow();
}

// @0x353224
void Deinit() {
    GUI::RemoveTweenEffect(g_tween);
    g_tween = nullptr;
    delete g_root;
    delete g_hover;
}

// @0x35327c: the window is filled for the task (and the quest checks blocked until it is gone):
// the rewards in the strip's order, then the visible cells (centred while there are fewer than five).
void Complete(uint32_t task) {
    Tasks::SetBlocked(true);
    const Tasks::TaskInfo* t = Tasks::GetTask(task);
    if (!t) {
        std::printf("TaskCompleteWindow::Complete() Failed to find task %d information\n", task);
        Hide();
        return;
    }
    if (g_task != task) g_scroller.firstIndex = 0;
    g_task = task;
    g_desc->SetText(t->name);

    g_filled = 0;
    g_rewards.clear();
    auto fill = [] { return &g_rewards[(size_t)g_filled]; };
    for (int type : kOrder) {
        if (t->award[type] == 0) continue;
        g_rewards.emplace_back();
        Reward* r = fill();
        r->resource = true;
        r->count = t->award[type];
        r->itemLevel = 0;
        r->item = 0;
        r->type = type;
        ++g_filled;
    }
    for (unsigned i = 0; i < t->itemCount; ++i) {
        g_rewards.emplace_back();
        Reward* r = fill();
        r->resource = false;
        r->item = t->items[i].id;
        r->type = 0xb;
        const Items::ItemInfo* info = Items::GetItemInfo(t->items[i].id);
        if (!info) {
            r->itemLevel = 0;
        } else {
            if (info->type == 0x10 && Setting("item_boosts").GetInt() == 0) continue;   // (see g_rewards)
            r->itemLevel = info->itemLevel;
        }
        r->count = t->items[i].count;
        ++g_filled;
    }

    // PORT: the share ticks (shown from level 3 with the Facebook/Twitter states) stay hidden.
    g_social->SetVisibility(false);
    g_scroller.itemCount = (unsigned)g_rewards.size();
    g_left->SetEnabled(g_scroller.CanMoveLeft());
    g_right->SetEnabled(g_scroller.CanMoveRight());
    g_left->SetVisibility(g_scroller.CanMove());
    g_right->SetVisibility(g_scroller.CanMove());
    for (RewardInfo& h : g_holders) h.root->MoveWindow(-g_centerShift, 0);
    g_centerShift = 0;

    const unsigned first = g_scroller.firstIndex;
    unsigned shown = 0;
    for (; shown < 5 && (int)(shown + first) < g_filled; ++shown) {
        RewardInfo& h = g_holders[shown];
        const Reward& r = g_rewards[shown + first];
        h.icon->SetVisibility(true);
        for (unsigned c = 0; c < 5; ++c) h.name[c]->SetVisibility(r.itemLevel == c);
        h.count->SetVisibility(true);
        if (r.resource) {
            h.icon->SetTexture(IconManager::GetIcon(GameState::GetResourceMapIconName(r.type)), true);
            h.name[r.itemLevel]->SetText(GameState::GetResourceGameName(r.type));
            h.count->SetText(ToWideString((int)r.count));
        } else if (Items::ItemInfo* info = Items::GetItemInfo(r.item)) {
            info->PutImageInHolder(h.icon);
            h.name[r.itemLevel]->SetText(info->title);
            h.count->SetText(ToWideString((int)r.count));
        } else {
            h.icon->SetVisibility(false);
            for (Textfield* n : h.name) n->SetVisibility(false);
            h.count->SetVisibility(false);
        }
    }
    if (g_filled != 0 && g_filled <= 4) {
        int cell = g_holders[1].root->x - g_holders[0].root->x;
        g_centerShift = (int)((float)(unsigned)(cell * (4 - (int)shown)) * 0.5f);
    }
    for (RewardInfo& h : g_holders) h.root->MoveWindow(g_centerShift, 0);
    for (unsigned i = shown; i < 5; ++i) {
        RewardInfo& h = g_holders[i];
        h.icon->SetVisibility(false);
        for (Textfield* n : h.name) n->SetVisibility(false);
        h.count->SetVisibility(false);
    }
    g_lockedSpell = Spells::GetSpellLockedByTask(g_task);
    g_root->UpdatePosition();
}

// @0x353bf4
void UpdateContents() {
    if (g_queue->shown) Complete(g_task);
}

// @0x352d4c: deferred while anything else is up; the strip starts at the beginning and a fight
// map pauses.
void Show() {
    if (!g_root->visibleSelf && (WindowManager::GetShownWindowCount() != 0 || Entity::IsDeathAnimationRunning())) {
        WindowManager::EnqueueWindow(Show, 0.f);
        return;
    }
    g_queue->shown = true;
    g_queue->MoveWindowOnTop(true);
    if (!g_root->visibleSelf) {
        WindowManager::WindowShow(false);
        g_tween->AnimateIn();
    }
    g_root->SetVisibility(true);
    g_scroller.firstIndex = 0;
    g_scroller.HandleMove(-g_scroller.offset);
    g_shareText->SetText(StringTable::GetString("SHARE_OK"));
    g_shareIcon->SetTexture(IconManager::GetIcon("gold_confirm"), true);
    g_header->SetText(StringTable::GetString("TASK_COMPLETE_TEXT"));
    g_bonus->SetText(StringTable::GetString("TASK_COMPLETE_DESC"));
    if (GameState::GetCurrentLocation() == 2) GameState::RaiseGamePauseState();
    SoundsManager::PlaySound("ui_task_complete", 1, false);
    g_root->PreloadTextures();   // GUI::PreloadWindow
}

// @0x352f54: closing completes the task.
void Hide() {
    if (g_task != 0) {
        g_completed = g_task;
        Tasks::CompleteTask(g_task, true, true, false);
        g_task = 0;
    }
    if (g_root->visibleSelf) g_tween->AnimateOut();
}

}  // namespace TaskCompleteWindow
