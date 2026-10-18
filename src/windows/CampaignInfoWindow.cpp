// CampaignInfoWindow: a campaign's brief (Window_campaign_brief.xml), opened from its world-map
// node: the campaign's name, the main / side quest progress bar with the newest quest of each kind,
// the overall percentage, the items its maps may drop (with the counts already found there), the
// mentor (or the sultan) whose tap shows the campaign's description in a HintHoverWindow, and
// Begin / Resume, which travels to the campaign's map.
// Port of CampaignInfoWindow (libkingdom.so 5.11), 0x272b4c..0x275a80 (UpdateContents is
// 0x2731a8..0x2742f4: Ghidra cut it at 0x273378 into a bogus "_INIT_71"). Statics at 0x618ac8.
#include <cstdio>
#include <vector>

#include "engine/IconManager.h"
#include "engine/Render.h"
#include "engine/Resources.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/Locations.h"
#include "game/MetaData.h"
#include "game/StringTable.h"
#include "game/Tasks.h"
#include "game/BuildingHovers.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/ContentScroller.h"
#include "hud/HoverWindows.h"
#include "hud/ItemHoverWindow.h"
#include "hud/Shared.h"
#include "windows/Windows.h"

namespace CampaignInfoWindow {
namespace {

using GUI::Textfield;
using GUI::Window;

// The two-part progress bar (0x4c bytes): main quests fill the orange half from the left, side
// quests the green half from the right; each clips its bar.
struct ProgressInfo {
    Window* root = nullptr;             // +0x00
    Window* mainBar = nullptr;          // +0x04 campaign_progress_bar_orange
    Textfield* mainUnder = nullptr;     // +0x08 text_progress_under_01
    Textfield* mainOver = nullptr;      // +0x0c campaign_progress_bar_orange.text_progress_over_01
    GUI::ClipRect mainClip = {};        // +0x10 the orange bar's rectangle
    Window* mainBg = nullptr;           // +0x20 button_scroll_bg_battle
    Window* mainIcon = nullptr;         // +0x24 its icon: the newest main quest's
    Window* sideBar = nullptr;          // +0x28 campaign_progress_bar_green
    Textfield* sideUnder = nullptr;     // +0x2c text_progress_under_02
    Textfield* sideOver = nullptr;      // +0x30 campaign_progress_bar_green.text_progress_over_02
    GUI::ClipRect sideClip = {};        // +0x34
    Window* sideBg = nullptr;           // +0x44 button_scroll_bg_battle_side_right
    Window* sideIcon = nullptr;         // +0x48 its icon: the newest side quest's

    // @0x274b94
    void LoadFrom(Window* parent, const char* prefix) {
        root = GUI::GetWindowTypedF<Window>(parent, "%s", prefix);
        mainBar = GUI::GetWindowTypedF<Window>(parent, "%s.campaign_progress_bar_orange", prefix);
        mainUnder = GUI::GetWindowTypedF<Textfield>(parent, "%s.text_progress_under_01", prefix);
        mainOver =
            GUI::GetWindowTypedF<Textfield>(parent, "%s.campaign_progress_bar_orange.text_progress_over_01", prefix);
        mainClip = {mainBar->x, mainBar->y, mainBar->x + mainBar->w, mainBar->y + mainBar->h};
        mainBar->SetClipRect(&mainClip);
        mainBg = GUI::GetWindowTypedF<Window>(parent, "%s.button_scroll_bg_battle", prefix);
        mainIcon = GUI::GetWindowTypedF<Window>(parent, "%s.button_scroll_bg_battle.icon", prefix);
        mainIcon->takesZ = true;
        sideBar = GUI::GetWindowTypedF<Window>(parent, "%s.campaign_progress_bar_green", prefix);
        sideUnder = GUI::GetWindowTypedF<Textfield>(parent, "%s.text_progress_under_02", prefix);
        sideOver =
            GUI::GetWindowTypedF<Textfield>(parent, "%s.campaign_progress_bar_green.text_progress_over_02", prefix);
        sideClip = {sideBar->x, sideBar->y, sideBar->x + sideBar->w, sideBar->y + sideBar->h};
        sideBg = GUI::GetWindowTypedF<Window>(parent, "%s.button_scroll_bg_battle_side_right", prefix);
        sideIcon = GUI::GetWindowTypedF<Window>(parent, "%s.button_scroll_bg_battle_side_right.icon", prefix);
        sideIcon->takesZ = true;
        sideBar->SetClipRect(&sideClip);
    }
    // @0x272b4c: the orange bar shows p of its left half.
    void SetMainProgress(float p) { mainClip.right = (int)((float)mainClip.left + (float)mainBar->w * p * 0.5f); }
    // @0x272b84: the green bar shows p of its right half.
    void SetSideProgress(float p) { sideClip.left = (int)((float)sideBar->x + (float)sideBar->w * (1.f - p * 0.5f)); }
};

// One item cell (0x28 bytes), "inventory_item_icon_%02d".
struct ItemInfo {
    Window* root = nullptr;             // +0x00
    Window* level[5] = {};              // +0x04 inventory_item_icon_holder.item_class_01..05
    Window* icon = nullptr;             // +0x18 item_icon_holder_item (takes a depth slot)
    Window* owned = nullptr;            // +0x1c icon_gold_owned
    Textfield* quantity = nullptr;      // +0x20 text_quantity
    uint32_t id = 0;                    // +0x24 the item shown

    // @0x274d6c
    void LoadFrom(Window* parent, const char* prefix) {
        root = GUI::GetWindowTypedF<Window>(parent, "%s", prefix);
        for (int k = 0; k < 5; ++k)
            level[k] = GUI::GetWindowTypedF<Window>(parent, "%s.inventory_item_icon_holder.item_class_%02d", prefix, k + 1);
        icon = GUI::GetWindowTypedF<Window>(parent, "%s.inventory_item_icon_holder.item_icon_holder_item", prefix);
        icon->takesZ = true;
        owned = GUI::GetWindowTypedF<Window>(parent, "%s.inventory_item_icon_holder.icon_gold_owned", prefix);
        quantity = GUI::GetWindowTypedF<Textfield>(parent, "%s.text_quantity", prefix);
        id = 0;
    }
};

// One of the two flags (0x270 bytes): "items" (the drops, a scrolled grid of 8 cells two per line)
// and "story" (the mentor / sultan; its cells are always hidden).
struct FlagInfo {
    Window* root = nullptr;             // +0x00
    Window* bg = nullptr;               // +0x04 campaign_flag_bg
    Textfield* title = nullptr;         // +0x08 campaign_text.text_find_items
    Textfield* subtitle = nullptr;      // +0x0c text_find_02
    Window* checkbox = nullptr;         // +0x10 raid_complete_checkbox: every quest done
    ItemInfo items[8];                  // +0x14
    int offX = 0, offY = 0;             // +0x154 +0x158 how far the cells were moved to centre few
    Window* mentor = nullptr;           // +0x15c campaign_text.image_campaign_mentor
    Window* sultan = nullptr;           // +0x160 campaign_text.image_campaign_sultan
    Shared::ContentScroller scroller;   // +0x164
    // +0x1dc (inside the scroller's object, at its +0x78): raid_items_found_holder, never read.
    Window* holder = nullptr;

    // @0x274e34
    void LoadFrom(Window* parent, const char* prefix) {
        root = GUI::GetWindowTypedF<Window>(parent, "%s", prefix);
        bg = GUI::GetWindowTypedF<Window>(parent, "%s.campaign_flag_bg", prefix);
        title = GUI::GetWindowTypedF<Textfield>(parent, "%s.campaign_text.text_find_items", prefix);
        subtitle = GUI::GetWindowTypedF<Textfield>(parent, "%s.text_find_02", prefix);
        checkbox = GUI::GetWindowTypedF<Window>(parent, "%s.raid_complete_checkbox", prefix);
        char name[128];
        for (int i = 0; i < 8; ++i) {
            std::snprintf(name, sizeof name, "%s.raid_items_found_holder.inventory_item_icon_%02d", prefix, i + 1);
            items[i].LoadFrom(parent, name);
        }
        offX = offY = 0;
        mentor = GUI::GetWindowTypedF<Window>(parent, "%s.campaign_text.image_campaign_mentor", prefix);
        sultan = GUI::GetWindowTypedF<Window>(parent, "%s.campaign_text.image_campaign_sultan", prefix);
        scroller.Init();
        Window* mask =
            GUI::GetWindowTypedF<Window>(parent, "%s.raid_items_found_holder.raid_flag_items_holder_mask", prefix);
        scroller.track = mask;
        scroller.viewport = {mask->x, mask->y, mask->x + mask->w, mask->y + mask->h};
        for (ItemInfo& c : items) {
            c.root->SetClipRect(&scroller.viewport);
            scroller.windows.push_back(c.root);
        }
        holder = GUI::GetWindowTypedF<Window>(parent, "%s.raid_items_found_holder", prefix);
        Window* bar = GUI::GetWindowTypedF<Window>(parent, "%s.raid_items_found_holder.scroll_bar_tiny", prefix);
        scroller.itemSize = items[2].root->y - items[0].root->y;
        scroller.horizontal = false;
        scroller.onScroll = UpdateContents;
        scroller.unkA0 = 8;
        scroller.perLine = 2;
        scroller.scrollbar = bar;
    }
};

// The in-window dialog (0x1c bytes), "dialog_campaign_brief": hidden by SetParameters and never
// shown in 5.11 (the HintHoverWindow shows the description instead).
struct DialogInfo {
    Window* root = nullptr;             // +0x00
    Window* frame = nullptr;            // +0x04 dialog_hint_window_9s
    int unk08 = 0;                      // +0x08
    Textfield* text = nullptr;          // +0x0c dialog_text
    Window* arrow = nullptr;            // +0x10 dialog_arrow_se
    Window* next = nullptr;             // +0x14 button_dialog_hint_next
    Textfield* nextText = nullptr;      // +0x18 button_dialog_hint_next.text_give_job

    // @0x274a1c
    void LoadFrom(Window* parent, const char* prefix) {
        root = GUI::GetWindowTypedF<Window>(parent, "%s", prefix);
        frame = GUI::GetWindowTypedF<Window>(parent, "%s.dialog_hint_window_9s", prefix);
        unk08 = 0;
        text = GUI::GetWindowTypedF<Textfield>(parent, "%s.dialog_text", prefix);
        arrow = GUI::GetWindowTypedF<Window>(parent, "%s.dialog_arrow_se", prefix);
        arrow->MoveWindow(0, -2);
        next = GUI::GetWindowTypedF<Window>(parent, "%s.button_dialog_hint_next", prefix);
        nextText = GUI::GetWindowTypedF<Textfield>(parent, "%s.button_dialog_hint_next.text_give_job", prefix);
    }
};

// The start button (0x14 bytes).
struct ButtonInfo {
    Window* root = nullptr;             // +0x00
    Window* pusher = nullptr;           // +0x04 campaign_start_button_pusher
    Window* lock = nullptr;             // +0x08 icon_locked_campaign
    Textfield* text = nullptr;          // +0x0c text_begin
    Window* icon = nullptr;             // +0x10 icon_35 (takes a depth slot)

    // @0x274af0
    void LoadFrom(Window* parent, const char* prefix) {
        root = GUI::GetWindowTypedF<Window>(parent, "%s", prefix);
        pusher = GUI::GetWindowTypedF<Window>(parent, "%s.campaign_start_button_pusher", prefix);
        lock = GUI::GetWindowTypedF<Window>(parent, "%s.icon_locked_campaign", prefix);
        text = GUI::GetWindowTypedF<Textfield>(parent, "%s.text_begin", prefix);
        icon = GUI::GetWindowTypedF<Window>(parent, "%s.icon_35", prefix);
        icon->takesZ = true;
    }
};

// What the hero already found on the campaign's map (0xc bytes): a resource (type 0..10, id 0) or
// an item (type 0xb).
struct Collected {
    int type = 0;
    uint32_t id = 0;
    int amount = 0;
};

WindowManager::FunctionalWindow* g_wnd = nullptr;   // CampaignInfoWindow::wnd 0x619090
int g_arrowItem = -1;                  // 0x60f14c the cell OnTweenIn points the arrow at (ShowItem)
float g_scale = 1.f;                   // 0x60f150
bool g_tablet = false;                 // 0x60f154
Window* g_root = nullptr;              // 0x618ac8
GUI::TweenEffect* g_tween = nullptr;   // 0x618acc
bool g_animateOnce = false;            // 0x618ad0 Start travels with the world map's animation
ProgressInfo g_progress;               // 0x618ad4
FlagInfo g_items;                      // 0x618b20
FlagInfo g_story;                      // 0x618d90
std::vector<Collected> g_collected;    // 0x619000
std::vector<Items::ItemInfo*> g_findable;   // 0x619018 the items the campaign's maps drop
Locations::LocationInfo* g_info = nullptr;  // 0x619028
bool g_showItems = false;              // 0x61902c the items flag shows its grid even when complete
uint32_t g_map = 0;                    // 0x619030
Window* g_close = nullptr;             // 0x619034 x_button
Textfield* g_name = nullptr;           // 0x619038
Textfield* g_progressText = nullptr;   // 0x61903c
Textfield* g_counter = nullptr;        // 0x619040
HintHoverWindow* g_hint = nullptr;     // 0x619044
ButtonInfo g_start;                    // 0x619048
ItemHoverWindow* g_hover = nullptr;    // 0x61905c
DialogInfo g_dialog;                   // 0x619060
bool g_setParam3 = false;              // 0x61907c SetParameters' third argument (never read)
Shared::SmallLogoWindow g_logo;        // 0x619080

// @0x272bc8
void SetZ(float z) {
    if (!g_wnd->shown) return;
    g_root->SetZ(z);
    g_root->SetPosition(g_tween->GetCenteredX(), g_tween->GetCenteredY());
}

// @0x272c80
float ZRange() { return g_wnd->shown ? 0.05f : 0.001f; }

// @0x272f58
void OnTweenOut() {
    WindowManager::WindowHide(false);
    g_wnd->shown = false;
    g_wnd->MoveWindowDown(true);
    GameState::DropGamePauseState();
    g_showItems = false;
    g_info = nullptr;
}

// @0x274400: an item cell's popup (the story flag's cells, type 1, do nothing).
void OnItem(unsigned type, unsigned i) {
    if (type != 0) return;
    // UNVERIFIED (milestone 6): SoundsManager::PlaySound("ui_click", 1, false).
    g_hover->Show();
    g_hover->SetItem(StringTable::GetString("CAMPAIGN_INFO_CHANCE_TO_FIND"), nullptr,
                     Items::GetItemInfo(g_items.items[i].id), false, true, nullptr, false, false, false, false);
    const Window* w = g_items.items[i].root;
    const int x = g_root->x + w->x, y = g_root->y + w->y;
    g_hover->SetTargetRect(x, y, x + w->w, y + w->h, ItemHoverWindow::kS);
    g_hover->RecomputePosition();
    g_hover->SetZ(0.001f);
    Render::SortRenderLayer(Render::kLayerGUI, 1);
}

// @0x274360: the mentor (or the sultan) tells the campaign's story.
void OnDialogShow() {
    g_hint->Show();
    g_hint->SetPosition((GUI::ScreenWidth() - g_hint->f2c) / 2, (GUI::ScreenHeight() - g_hint->f30) / 2);
    g_hint->MoveWindowOnTop(true);
    // UNVERIFIED (milestone 6): SoundsManager::PlaySound("ui_swing_in", 1, false).
}

// @0x27430c
void OnDialogHide() {
    g_hint->Hide();
    g_wnd->MoveWindowOnTop(true);
    // UNVERIFIED (milestone 6): SoundsManager::PlaySound("ui_swing_out", 1, false).
}

// @0x272fa0: the arrow ShowItem asked for, or, before the desert campaign (map 0xf) was ever
// entered, an arrow at the story flag's mentor.
void OnTweenIn() {
    if (g_arrowItem != -1) {
        const Window* w = g_items.items[g_arrowItem].root;
        BuildingHovers::SetArrowVisibleWindowLimit(2);
        BuildingHovers::ArrowAt((float)(g_root->x + w->x + w->w / 2), (float)(g_root->y + w->y), false, false, false,
                                false, true, false, false);
        const unsigned i = (unsigned)g_arrowItem;
        BuildingHovers::SetArrowClickCallback(new GUI::Callback([i] { OnItem(0, i); }));
        g_arrowItem = -1;
    }
    if (g_map == 0xf && GameState::GetMapStartTime(0xf) == 0) {
        BuildingHovers::SetArrowClickCallback(new GUI::Callback(OnDialogShow));
        BuildingHovers::ArrowAt((float)(g_root->x + g_story.root->x),
                                (float)g_story.root->y + g_scale * 60.f + (float)g_root->y, false, true, true, false,
                                true, false, false);
    }
}

// @0x2742f4: the "complete" tick shows the items anyway.
void OnLeftFlag() {
    g_showItems = true;
    UpdateContents();
}

// @0x27454c
void Update(float dt) {
    if (!g_wnd->shown) return;
    g_items.scroller.Update(dt);
    g_story.scroller.Update(dt);
}

// @0x2746f4: not while the tutorial's world map waits for the tap (0x67).
bool OnBack() {
    if (GameState::tutorial == 0x67 || !g_wnd->shown) return false;
    if (g_hint->shownHover) {
        OnDialogHide();
        return true;
    }
    if (g_hover->shownHover) {
        g_hover->Hide();
        return true;
    }
    Hide();
    return true;
}

// @0x27478c: travel to the campaign's map (animated when opened from its world-map node).
void OnStart() {
    if (g_dialog.root->visibleSelf) return;
    Hide();
    if (g_animateOnce) {
        if (!GlobalMapWindow::IsVisible()) {
            GlobalMapWindow::CenterOnMap((uint32_t)GameState::GetCurrentMapID());
            GlobalMapWindow::Show();
        }
        GlobalMapWindow::SwitchMapAnimated(g_map, false, 500.f, false);
        return;
    }
    GlobalMapWindow::SwitchMap(g_map, false, false);
}

// @0x27480c
void Move(int x, int y) {
    g_items.scroller.Move(x, y);
    g_story.scroller.Move(x, y);
}

// @0x27484c
bool Click(int x, int y, bool pressed) {
    if (g_hover->Click(x, y, pressed) || g_hint->Click(x, y, pressed)) return true;
    if (!g_root->visibleSelf) {
        g_root->Click(x, y, pressed, false);
        return false;
    }
    if (BuildingHovers::ClickOnArrow(x, y, pressed)) return true;
    if (!pressed) BuildingHovers::HideArrow();
    if (g_items.scroller.Click(x, y, pressed, g_root) || g_story.scroller.Click(x, y, pressed, g_root)) return true;
    const bool visible = g_root->visibleSelf;
    g_root->Click(x, y, pressed, false);
    return visible;
}

// @0x27498c
void Deinit() {
    GUI::RemoveTweenEffect(g_tween);
    g_tween = nullptr;
    delete g_root;
    delete g_hover;
    delete g_hint;
}

}  // namespace

// Static FunctionalWindow("CampaignInfoWindow", Init, Deinit, Click, SetZ, Hide, Move) (_INIT_
// 0x272cd4, which also constructs the scrollers and the vectors).
WindowManager::FunctionalWindow* Queue() {
    if (!g_wnd) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.deinit = Deinit;
        f.click = Click;
        f.setZ = SetZ;
        f.hide = Hide;
        f.move = Move;
        g_wnd = new WindowManager::FunctionalWindow("CampaignInfoWindow", std::move(f));
    }
    return g_wnd;
}

// @0x272c54
bool IsVisible() { return g_wnd->shown; }

// @0x272c68: the next Start travels with the world map's animation.
void SetAnimationOnce() { g_animateOnce = true; }

// @0x275450
void Init() {
    g_tablet = GUI::IsTabletVersion();
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Window_campaign_brief.xml", "Window_campaign_brief.png",
                             -1.f, 0, 0, 0, -0x32, false, 1.f);
    g_wnd->RegisterTopWindow(g_root);
    g_scale = g_root->scale;
    g_root->SetVisibility(false);
    g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::GetVerticalCenter(g_root->h));
    g_tween = GUI::CreateTweenEffect(g_root, nullptr, OnTweenIn, OnTweenOut);
    g_tween->CenterWith(0, 0, 0, (int)(g_scale * -50.f));
    g_close = GUI::GetWindowTyped<Window>(g_root, "x_button");
    g_close->SetOnClick(Hide);
    g_close->SetActionSound("ui_close", 1);
    g_logo.LoadFrom(g_root, "logo_holder");
    g_logo.SetLanguage((unsigned)StringTable::GetLangID());
    g_name = GUI::GetWindowTyped<Textfield>(g_root, "campaign_name_holder.text_raid_name");
    g_progress.LoadFrom(g_root, "campaign_progress_bar_holder");
    g_items.LoadFrom(g_root, "campaign_flag_holder_items");
    for (unsigned i = 0; i < 8; ++i) g_items.items[i].root->SetOnClick([i] { OnItem(0, i); });
    g_items.checkbox->SetOnClick(OnLeftFlag);
    g_story.LoadFrom(g_root, "campaign_flag_holder_story");
    for (unsigned i = 0; i < 8; ++i) g_story.items[i].root->SetOnClick([i] { OnItem(1, i); });
    g_story.mentor->SetOnClick(OnDialogShow);
    g_story.sultan->SetOnClick(OnDialogShow);
    g_progressText = GUI::GetWindowTyped<Textfield>(g_root, "campaign_progress_holder.text_progress");
    g_counter = GUI::GetWindowTyped<Textfield>(g_root, "campaign_progress_holder.text_counter");
    g_dialog.LoadFrom(g_root, "dialog_campaign_brief");
    g_dialog.next->SetOnClick(OnDialogHide);
    g_dialog.frame->SetOnClick(OnDialogHide);
    g_start.LoadFrom(g_root, "button_campaign_start");
    g_start.root->SetOnClick(OnStart);
    g_wnd->fn.update = Update;
    g_wnd->fn.zRange = ZRange;
    g_wnd->hideOnOuterClick = true;
    g_wnd->fn.back = OnBack;
    g_wnd->fn.show = Show;
    g_hover = new ItemHoverWindow();
    g_hint = new HintHoverWindow();
    g_hint->hoverScale = 1.3f;
}

// @0x275148: the campaign (info), the map Start goes to, and what the hero found there: the
// resources 0..10, then the items (not packs).
void SetParameters(Locations::LocationInfo* info, uint32_t map, bool param3) {
    g_info = info;
    g_collected.clear();
    g_map = map;
    for (int type = 0; type <= 10; ++type)
        if (int amount = GameState::GetMapResourceCollectionInfo(g_map, type)) g_collected.push_back({type, 0, amount});
    for (unsigned i = 0; Items::ItemInfo* item = Items::EnumItemInfo(i); ++i) {
        const int amount = GameState::GetMapItemCollectionInfo(g_map, item->id);
        if (amount == 0 || item->id == 0x2000) continue;
        g_collected.push_back({0xb, item->id, amount});
    }
    g_setParam3 = param3;
    g_dialog.root->SetVisibility(false);
}

// @0x2731a8
void UpdateContents() {
    if (!g_info) return;
    g_close->SetVisibility(GameState::tutorial != 0x67);
    g_name->SetText(SWPrintf(0x100, StringTable::GetString("CAMPAIGN_HEADER"), {g_info->name}).c_str());

    // The quests of the campaign's maps: main, and all (side = all - main).
    unsigned mainDone = 0, mainCount = 0, allDone = 0, allCount = 0;
    float side = 1.f;
    if (g_info->maps.empty()) {
        g_progress.SetMainProgress(1.f);
    } else {
        for (const Locations::MapInfo& m : g_info->maps) {
            mainDone += (unsigned)Tasks::GetMapTaskCompleted(m.id, true);
            mainCount += (unsigned)Tasks::GetMapTaskCount(m.id, true);
            allDone += (unsigned)Tasks::GetMapTaskCompleted(m.id, false);
            allCount += (unsigned)Tasks::GetMapTaskCount(m.id, false);
        }
        g_progress.SetMainProgress(mainCount != 0 ? (float)mainDone / (float)mainCount : 1.f);
        const unsigned sideCount = allCount - mainCount, sideDone = allDone - mainDone;
        if (sideCount != 0) side = (float)sideDone / (float)sideCount;
    }
    g_progress.SetSideProgress(side);
    g_progress.mainUnder->SetText(StringTable::GetString("CAMPAIGN_MAIN"));
    g_progress.mainOver->SetText(StringTable::GetString("CAMPAIGN_MAIN"));
    g_progress.sideUnder->SetText(StringTable::GetString("CAMPAIGN_SIDE"));
    g_progress.sideOver->SetText(StringTable::GetString("CAMPAIGN_SIDE"));
    g_progressText->SetText(StringTable::GetString("CAMPAIGN_PROGRESS"));
    // (A campaign without quests divides 0 by 0: the ARM conversion of NaN gives 0.)
    const int percent = allCount != 0 ? (int)((float)allDone / (float)allCount * 100.f) : 0;
    g_counter->SetText(SWPrintf(0x100, U"%d%%", {percent}).c_str());

    // The icons: the campaign's newest running main and side quests, else a tick when every main
    // quest is done, else the quest scroll.
    Tasks::TaskInfo* tasks[0x80];
    const unsigned n = Tasks::GetCurrentTasks(tasks, 0x80, false, true);
    Tasks::TaskInfo* main = nullptr;
    Tasks::TaskInfo* sideTask = nullptr;
    for (unsigned i = 0; i < n; ++i) {
        Tasks::TaskInfo* t = tasks[i];
        if (t->campaign != g_info->campaign) continue;
        Tasks::TaskInfo*& best = t->sidequest ? sideTask : main;
        if (!best || GameState::GetTaskBeginTime(t->id) > GameState::GetTaskBeginTime(best->id)) best = t;
    }
    const char* noQuest = mainCount == mainDone ? "gold_confirm" : "gold_quest";
    if (main && main->GetIcon()) g_progress.mainIcon->SetTexture(main->GetIcon(), true);
    else g_progress.mainIcon->SetTexture(IconManager::GetIcon(noQuest, false), true);
    if (sideTask) g_progress.sideIcon->SetTexture(sideTask->GetIcon(), true);
    else g_progress.sideIcon->SetTexture(IconManager::GetIcon(noQuest, false), true);
    g_progress.mainIcon->ApplyMask(Resources::GetUIImage("mask_icon_holder_60_round.png", false, false));
    g_progress.sideIcon->ApplyMask(Resources::GetUIImage("mask_icon_holder_60_round.png", false, false));

    // The items flag: "chance to find" and the grid, or the tick once every quest is done (its tap
    // shows the grid again).
    g_items.subtitle->SetText(StringTable::GetString("CAMPAIGN_INFO_CHANCE_TO_FIND"));
    const bool complete = !g_showItems && allDone == allCount;
    g_items.subtitle->SetVisibility(!complete);
    g_items.checkbox->SetVisibility(complete);
    if (g_info->items && g_info->items->GetChildrenCount() != 0) {
        const MetaData* list = g_info->items->GetChild(0);
        g_findable.clear();
        for (unsigned i = 0; i < list->GetChildrenCount(); ++i) {
            Items::ItemInfo* item = Items::GetItemInfo((uint32_t)list->GetChild(i)->GetChild(0)->GetInt());
            if (!item) continue;
            // UNVERIFIED (milestone 5n): an item of a profession (reputationId) is listed only when
            // Professions::GetProfession finds none, or it IsUnlocked and GetPlayerProfessionLevel
            // >= reputationLevel; professions are not ported, so every item is listed.
            g_findable.push_back(item);
        }
        g_items.scroller.itemCount = complete ? 0 : (unsigned)g_findable.size();
        g_items.scroller.UpdateScroll();
        for (ItemInfo& c : g_items.items) c.root->MoveWindow(-g_items.offX, -g_items.offY);
        g_items.offX = g_items.offY = 0;
        // Fill the cells from the scroller's first line; one or two items are centred.
        unsigned shown = 0;
        bool centreY = true;
        const unsigned first = g_items.scroller.firstIndex;
        if (!complete && first < g_findable.size()) {
            for (;;) {
                ItemInfo& c = g_items.items[shown];
                c.root->SetVisibility(true);
                Items::ItemInfo* item = g_findable[shown + first];
                for (unsigned k = 0; k < 5; ++k) c.level[k]->SetVisibility(item->itemLevel == k);
                c.icon->SetTexture(item->GetIcon(), true);
                // The first non-zero count found for it on the map.
                int amount = 0;
                for (const Collected& e : g_collected) {
                    amount = e.id == item->id ? e.amount : 0;
                    if (amount != 0) break;
                }
                c.owned->SetVisibility(amount != 0);
                c.quantity->SetVisibility(amount != 0);
                c.quantity->SetText(ToWideString(amount));
                c.id = item->id;
                ++shown;
                if (shown + first >= g_findable.size()) {
                    if (shown == 1)
                        g_items.offX = (g_items.items[1].root->x - g_items.items[0].root->x) / 2;
                    centreY = shown <= 2;
                    break;
                }
                if (shown == 8) {
                    centreY = false;
                    break;
                }
            }
        }
        if (centreY) g_items.offY = (g_items.items[2].root->y - g_items.items[0].root->y) / 2;
        for (ItemInfo& c : g_items.items) c.root->MoveWindow(g_items.offX, g_items.offY);
        for (unsigned k = shown; k < 8; ++k) g_items.items[k].root->SetVisibility(false);
    }

    // The story flag: the mentor (the sultan for campaign 5) and the description bubble.
    g_story.checkbox->SetVisibility(false);
    g_story.title->SetText(StringTable::GetString("CAMPAIGN_YOUR_TASK"));
    g_story.mentor->SetVisibility(g_info->campaign != 5);
    g_story.sultan->SetVisibility(g_info->campaign == 5);
    g_story.scroller.UpdateScroll();
    for (ItemInfo& c : g_story.items) c.root->SetVisibility(false);
    g_hint->SetParameters(g_info->description);
    g_hint->SetOnClose(OnDialogHide);
    g_start.lock->SetVisibility(false);
    g_start.text->SetText(StringTable::GetString(GameState::GetMapStartTime(g_map) != 0 ? "CAMPAIGN_INFO_RESUME"
                                                                                        : "CAMPAIGN_INFO_BEGIN_CAMPAIGN"));
    g_root->UpdatePosition();
}

// @0x274598: the next opening points the arrow at item id's cell (scrolled into view).
void ShowItem(uint32_t id) {
    for (unsigned i = 0; i < g_findable.size(); ++i) {
        if (g_findable[i]->id != id) continue;
        g_items.scroller.ScrollIntoView(i);
        g_showItems = true;
        g_arrowItem = (int)(i - g_items.scroller.firstIndex);
        return;
    }
}

// @0x274630
void Show() {
    g_wnd->shown = true;
    g_wnd->MoveWindowOnTop(true);
    if (!g_root->visibleSelf) {
        WindowManager::WindowShow(false);
        g_tween->AnimateIn();
    }
    g_root->SetVisibility(true);
    g_items.scroller.firstIndex = 0;
    g_items.scroller.HandleMove(-g_items.scroller.offset);
    UpdateContents();
    g_root->PreloadTextures();   // GUI::PreloadWindow
    GameState::RaiseGamePauseState();
    g_animateOnce = false;
}

// @0x2746d0
void Hide() {
    if (g_root->visibleSelf) g_tween->AnimateOut();
}

}  // namespace CampaignInfoWindow
