// RaidInfoWindow: a mission's brief (Window_raid_brief.xml), opened from its world-map node or when
// the mission's time runs out on its map. The mission's name, three tabs (one per star; one "epic"
// tab at three stars), the star progress bar with "Raid Complete!" per finished star and the
// overall percentage, three flags (one per star) with the gold, XP and items each can drop, the
// timer (time to complete, time left, time up, timeout), the start / resume / restart buttons
// (the restart for crystals), and the requirements of the next run. Finishing a star plays a
// completion animation (RunCompletionAnimation, from RaidCompleteWindow): the bar fills, the new
// star's flag fades in.
// Port of RaidInfoWindow (libkingdom.so 5.11), 0x32f150..0x3360a4 (UpdateContents is
// 0x332728..0x335c04: Ghidra cut it at 0x3352b8 into a bogus "_INIT_112"). Statics at 0x62b1c4.
#include <SDL3/SDL_stdinc.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "engine/SoundsManager.h"
#include "engine/IconManager.h"
#include "engine/Render.h"
#include "engine/Timer.h"
#include "game/BuildingHovers.h"
#include "game/Combat.h"
#include "game/EntityData.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/Locations.h"
#include "game/Professions.h"
#include "game/MetaData.h"
#include "game/SaveManager.h"
#include "game/Setting.h"
#include "game/StringTable.h"
#include "game/Tasks.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/ContentScroller.h"
#include "hud/HUD.h"
#include "hud/ItemHoverWindow.h"
#include "hud/Shared.h"
#include "windows/Windows.h"

namespace RaidInfoWindow {
namespace {

using GUI::Textfield;
using GUI::Window;

// A tab (8 bytes): "raid_name_holder_%02d", "raid_name_epic".
struct TabInfo {
    Window* root = nullptr;             // +0x00
    Textfield* text = nullptr;          // +0x04 text_raid_name
    // @0x330bfc
    void LoadFrom(Window* parent, const char* prefix) {
        root = GUI::GetWindowTypedF<Window>(parent, "%s", prefix);
        text = GUI::GetWindowTypedF<Textfield>(parent, "%s.text_raid_name", prefix);
    }
};

// The star progress bar (0x3c bytes), "raid_progress_bar_holder".
struct ProgressInfo {
    Window* root = nullptr;             // +0x00
    Window* bar = nullptr;              // +0x04 progress_bar_raid_orange
    GUI::ClipRect clip = {};            // +0x08
    Textfield* under[3] = {};           // +0x18 text_progress_under_01..03
    Textfield* over[3] = {};            // +0x24 progress_bar_raid_orange.text_progress_over_01..03
    Window* lock[3] = {};               // +0x30 lock_01..03
    // @0x330a80
    void LoadFrom(Window* parent, const char* prefix) {
        root = GUI::GetWindowTypedF<Window>(parent, "%s", prefix);
        bar = GUI::GetWindowTypedF<Window>(parent, "%s.progress_bar_raid_orange", prefix);
        clip = {bar->x, bar->y, bar->x + bar->w, bar->y + bar->h};
        bar->SetClipRect(&clip);
        for (int i = 0; i < 3; ++i) under[i] = GUI::GetWindowTypedF<Textfield>(parent, "%s.text_progress_under_%02d", prefix, i + 1);
        for (int i = 0; i < 3; ++i)
            over[i] = GUI::GetWindowTypedF<Textfield>(parent, "%s.progress_bar_raid_orange.text_progress_over_%02d", prefix, i + 1);
        for (int i = 0; i < 3; ++i) lock[i] = GUI::GetWindowTypedF<Window>(parent, "%s.lock_%02d", prefix, i + 1);
    }
};

// One drop cell (0x50 bytes), "inventory_item_icon_%02d".
struct ItemInfo {
    Window* root = nullptr;             // +0x00
    Window* level[5] = {};              // +0x04 inventory_item_icon_holder.item_class_01..05
    Window* icon = nullptr;             // +0x18 item_icon_holder_item (takes a depth slot)
    Window* owned = nullptr;            // +0x1c icon_gold_owned
    Textfield* quantity = nullptr;      // +0x20 text_quantity
    Window* discountHolder = nullptr;   // +0x24 discount_label_holder
    Shared::DiscountInfo label[4];      // +0x28 red, green (high chance), blue (medium), purple (low)
    uint32_t id = 0;                    // +0x48 the item shown (0: gold or XP)
    int probability = 0;                // +0x4c
    // @0x330c44
    void LoadFrom(Window* parent, const char* prefix) {
        root = GUI::GetWindowTypedF<Window>(parent, "%s", prefix);
        for (int k = 0; k < 5; ++k)
            level[k] = GUI::GetWindowTypedF<Window>(parent, "%s.inventory_item_icon_holder.item_class_%02d", prefix, k + 1);
        icon = GUI::GetWindowTypedF<Window>(parent, "%s.inventory_item_icon_holder.item_icon_holder_item", prefix);
        icon->takesZ = true;
        owned = GUI::GetWindowTypedF<Window>(parent, "%s.inventory_item_icon_holder.icon_gold_owned", prefix);
        quantity = GUI::GetWindowTypedF<Textfield>(parent, "%s.text_quantity", prefix);
        discountHolder = GUI::GetWindowTypedF<Window>(parent, "%s.inventory_item_icon_holder.discount_label_holder", prefix);
        static const char* const kLabels[4] = {"red", "green", "blue", "purple"};
        for (int k = 0; k < 4; ++k) {
            char name[192];
            std::snprintf(name, sizeof name, "%s.inventory_item_icon_holder.discount_label_holder.discount_label_%s", prefix,
                          kLabels[k]);
            label[k].LoadFrom(parent, name);
        }
        id = 0;
    }
};

// A star's flag (0x3c0 bytes), "raid_flag_holder_%02d": its picture, "chance to find" and the
// scrolled 2-per-line grid of its drops, or the "complete" tick (a tap shows the grid).
struct FlagInfo {
    Window* root = nullptr;             // +0x00
    Window* image[3] = {};              // +0x04 raid_flag_simple, _medium, _hard
    Textfield* title = nullptr;         // +0x10 text_find_items
    Window* checkbox = nullptr;         // +0x14 raid_complete_checkbox
    ItemInfo cells[8];                  // +0x18
    int offX = 0, offY = 0;             // +0x298 +0x29c how far the cells were moved to centre few
    bool showItems = false;             // +0x2a0
    std::vector<uint32_t> shown;        // +0x2a4 the ids in the grid's order
    Shared::ContentScroller scroller;   // +0x2b4

    // @0x330e0c
    void LoadFrom(Window* parent, const char* prefix) {
        root = GUI::GetWindowTypedF<Window>(parent, "%s", prefix);
        image[0] = GUI::GetWindowTypedF<Window>(parent, "%s.raid_flag_simple", prefix);
        image[1] = GUI::GetWindowTypedF<Window>(parent, "%s.raid_flag_medium", prefix);
        image[2] = GUI::GetWindowTypedF<Window>(parent, "%s.raid_flag_hard", prefix);
        title = GUI::GetWindowTypedF<Textfield>(parent, "%s.text_find_items", prefix);
        checkbox = GUI::GetWindowTypedF<Window>(parent, "%s.raid_complete_checkbox", prefix);
        char name[160];
        for (int i = 0; i < 8; ++i) {
            std::snprintf(name, sizeof name, "%s.raid_items_found_holder.inventory_item_icon_%02d", prefix, i + 1);
            cells[i].LoadFrom(parent, name);
        }
        showItems = false;
        offX = offY = 0;
        scroller.Init();
        Window* mask =
            GUI::GetWindowTypedF<Window>(parent, "%s.raid_items_found_holder.raid_flag_items_holder_mask", prefix);
        scroller.viewport = {mask->x, mask->y, mask->x + mask->w, mask->y + mask->h};
        scroller.track = mask;
        for (ItemInfo& c : cells) {
            c.root->SetClipRect(&scroller.viewport);
            scroller.windows.push_back(c.root);
        }
        Window* bar = GUI::GetWindowTypedF<Window>(parent, "%s.raid_items_found_holder.scroll_bar_tiny", prefix);
        scroller.itemSize = cells[2].root->y - cells[0].root->y;
        scroller.horizontal = false;
        scroller.onScroll = UpdateContents;
        scroller.unkA0 = 8;
        scroller.perLine = 2;
        scroller.scrollbar = bar;
    }
};

// The timer (0x14 bytes), "raid_time_holder".
struct RaidTimeInfo {
    Window* root = nullptr;             // +0x00
    Textfield* time = nullptr;          // +0x04 text_time
    Textfield* caption = nullptr;       // +0x08 text_time_to_complete
    Textfield* timeout = nullptr;       // +0x0c text_timeout
    Window* icon = nullptr;             // +0x10 icon_35_timer_out (takes a depth slot)
    // @0x330878
    void LoadFrom(Window* parent, const char* prefix) {
        root = GUI::GetWindowTypedF<Window>(parent, "%s", prefix);
        time = GUI::GetWindowTypedF<Textfield>(parent, "%s.text_time", prefix);
        caption = GUI::GetWindowTypedF<Textfield>(parent, "%s.text_time_to_complete", prefix);
        timeout = GUI::GetWindowTypedF<Textfield>(parent, "%s.text_timeout", prefix);
        icon = GUI::GetWindowTypedF<Window>(parent, "%s.icon_35_timer_out", prefix);
        icon->takesZ = true;
    }
};

// One requirement (0x10 bytes), "raid_start_res_%02d".
struct CostResourceInfo {
    Window* root = nullptr;             // +0x00
    Window* icon = nullptr;             // +0x04 icon_res_enough (takes a depth slot)
    Textfield* enough = nullptr;        // +0x08 text_res_enough
    Textfield* notEnough = nullptr;     // +0x0c text_res_not_enough
    // @0x33091c
    void LoadFrom(Window* parent, const char* prefix) {
        root = GUI::GetWindowTypedF<Window>(parent, "%s", prefix);
        icon = GUI::GetWindowTypedF<Window>(parent, "%s.icon_res_enough", prefix);
        icon->takesZ = true;
        enough = GUI::GetWindowTypedF<Textfield>(parent, "%s.text_res_enough", prefix);
        notEnough = GUI::GetWindowTypedF<Textfield>(parent, "%s.text_res_not_enough", prefix);
    }
};

// The requirements (0x58 bytes), "raid_cost_holder".
struct RaidCostInfo {
    Window* root = nullptr;             // +0x00
    Textfield* text = nullptr;          // +0x04 text_requirements
    CostResourceInfo res[5];            // +0x08
    // @0x3309a4
    void LoadFrom(Window* parent, const char* prefix) {
        root = GUI::GetWindowTypedF<Window>(parent, "%s", prefix);
        text = GUI::GetWindowTypedF<Textfield>(parent, "%s.text_requirements", prefix);
        char name[96];
        for (int i = 0; i < 5; ++i) {
            std::snprintf(name, sizeof name, "%s.raid_start_res_%02d", prefix, i + 1);
            res[i].LoadFrom(parent, name);
        }
    }
};

// A drop listed under a flag (0x10 bytes): gold (type 8), XP (10) or an item (0xb), sorted by key
// (gold 10, XP 9, a high-chance item 7, medium 5, low 0), highest first.
struct CollectedInfo {
    int type = 0;
    uint32_t id = 0;
    int amount = 0;
    int key = 0;
};
// @0x32f150 (the stable sort's order)
bool operator<(const CollectedInfo& a, const CollectedInfo& b) { return b.key < a.key; }

// What the hero already found on the mission's map (0x10 bytes): a resource (type 0..10) or an
// item (type 0xb).
struct MapFind {
    int type = 0;
    uint32_t id = 0;
    int amount = 0;
    int unused = 0;
};

// ShowItem's flag order by the stars already won (0x59fbf0): the flags it searches first.
const unsigned kFlagOrder[4][3] = {{0, 1, 2}, {1, 0, 2}, {2, 1, 0}, {2, 1, 0}};

WindowManager::FunctionalWindow* g_wnd = nullptr;   // RaidInfoWindow::wnd 0x62bf1c
int g_arrowFlag = -1, g_arrowCell = -1;   // 0x60f368 0x60f36c the cell OnTweenIn points at (ShowItem)
bool g_tablet = false;                 // 0x60f370
float g_scale = 1.f;                   // 0x60f374
Window* g_root = nullptr;              // 0x62b1c4
GUI::TweenEffect* g_tween = nullptr;   // 0x62b1c8
bool g_animateOnce = false;            // 0x62b1cc Start travels with the world map's animation
bool g_animPending = false;            // 0x62b1cd RunCompletionAnimation: starts with the opening
bool g_animRunning = false;            // 0x62b1ce
float g_animTime = 0.f;                // 0x62b1d0
TabInfo g_tabs[3];                     // 0x62b1d4
ProgressInfo g_progress;               // 0x62b1ec
FlagInfo g_flags[3];                   // 0x62b228
Window* g_chain[6] = {};               // 0x62bd68 chain_01..06: links beside the start buttons
RaidCostInfo g_cost;                   // 0x62bd80
std::vector<CollectedInfo> g_collected;   // 0x62bdd8
std::vector<MapFind> g_finds;          // 0x62bdf4
Locations::LocationInfo* g_info = nullptr;   // 0x62be10
ItemHoverWindow* g_hover = nullptr;    // 0x62be14
Window* g_close = nullptr;             // 0x62be18 x_button
bool g_completed = false;              // 0x62be1c SetParameters' bool: shown after the mission's end
uint32_t g_map = 0;                    // 0x62be20
Window* g_help = nullptr;              // 0x62be24 button_brief_help
Textfield* g_title = nullptr;          // 0x62be28 text_title
TabInfo g_epicTab;                     // 0x62be2c raid_name_epic
RaidTimeInfo g_time;                   // 0x62be34
Shared::ButtonRaidStart g_restart;     // 0x62be48 button_raid_start_02 (restart)
Shared::ButtonRaidStart g_centre;      // 0x62be8c button_raid_start_centre (start / resume / restart)
Shared::ButtonRaidStart g_resume;      // 0x62bed0 button_raid_start (resume beside restart)
int g_costOffset = 0;                  // 0x62bf14 the requirements' centring
int g_lastSecond = 0;                  // 0x62bf18

// @0x32f168
void SetZ(float z) {
    if (!g_wnd->shown) return;
    g_root->SetZ(z);
    g_root->SetPosition(g_tween->GetCenteredX(), g_tween->GetCenteredY());
}

// @0x32f24c
float ZRange() { return g_wnd->shown ? 0.06f : 0.01f; }

// @0x32f804: how likely item is to drop at that star: 0 high, 1 medium, 2 low. An item of a
// profession: its gathering chance (> 50: high, > 19: medium). Else the location's own drop table
// first; else the best chance among its enemies' drops (> 10: high, > 4: medium).
int GetItemDropProbability(Locations::LocationInfo* loc, unsigned star, uint32_t item) {
    if (Items::ItemInfo* info = Items::GetItemInfo(item)) {
        if (Professions::GetProfession(info->reputationId)) {
            const unsigned p = (unsigned)Professions::GetItemProbability(info->reputationId, item);
            return p > 50 ? 0 : (p > 19 ? 1 : 2);
        }
    }
    if (loc->HadItemDropChance(item, star)) return loc->GetItemDropChance(item, star);
    if (!loc->enemies) return 2;
    const unsigned s = std::min(star, 2u);
    unsigned best = 0;
    auto scan = [&](const MetaData* list) {
        if (!list) return;
        for (unsigned i = 0; i < list->GetChildrenCount(); ++i) {
            const MetaData* d = list->GetChild(i);
            if ((uint32_t)d->GetChild(0)->GetInt() != item) continue;
            best = std::max(best, (unsigned)d->GetChild(1)->GetChild(0)->GetInt());
        }
    };
    for (unsigned i = 0; i < loc->enemies->GetChildrenCount(); ++i) {
        EntityData* data = EntityFactory::GetEntityByID(loc->enemies->GetChild(i)->GetInt());
        if (!data) {
            // (The original passes the failed lookup, so it prints 0.)
            std::printf("ERROR: RaidInfoWindow::GetItemDropProbability No entityData found with ID: %d\n", 0);
        } else {
            scan(data->itemsDrop);
            scan(data->starItems[s]);
        }
    }
    if (best > 10) return 0;
    if (best > 4) return 1;
    return 2;
}

// @0x32fa84
void OnTweenOut() {
    WindowManager::WindowHide(false);
    g_wnd->shown = false;
    g_wnd->MoveWindowDown(true);
    GameState::DropGamePauseState();
    g_animTime = 0.f;
    g_animRunning = false;
    g_info = nullptr;
    g_animPending = false;
}

// @0x32fc2c: a drop cell's item popup with its chance.
void OnItem(unsigned flag, unsigned cell) {
    ItemInfo& c = g_flags[flag].cells[cell];
    if (c.id == 0 || !IsVisible()) return;
    SoundsManager::PlaySound("ui_click", 1, false);
    const char32_t* chance = nullptr;
    if (c.probability == 0) chance = StringTable::GetString("PROBABILITY_HIGH");
    else if (c.probability == 1) chance = StringTable::GetString("PROBABILITY_MEDIUM");
    else if (c.probability == 2) chance = StringTable::GetString("PROBABILITY_LOW");
    g_hover->Show();
    g_hover->SetItem(StringTable::GetString("CAMPAIGN_INFO_CHANCE_TO_FIND"), nullptr, Items::GetItemInfo(c.id), false,
                     false, chance, false, false, false, false);
    const int x = g_root->x + c.icon->x, y = g_root->y + c.icon->y;
    g_hover->SetTargetRect(x, y, x + c.icon->w, y + c.icon->h, ItemHoverWindow::kW);
    g_hover->RecomputePosition();
    g_hover->SetZ(0.001f);
    Render::SortRenderLayer(Render::kLayerGUI, 1);
}

// @0x32fadc: the completion animation starts; the arrow ShowItem asked for.
void OnTweenIn() {
    if (g_animPending) g_animRunning = true;
    if (g_arrowFlag == -1 || g_arrowCell == -1) return;
    const Window* w = g_flags[g_arrowFlag].cells[g_arrowCell].root;
    BuildingHovers::SetArrowVisibleWindowLimit(2);
    BuildingHovers::ArrowAt((float)(g_root->x + w->x + w->w / 2), (float)(g_root->y + w->y), false, false, false, false,
                            true, false, false);
    const unsigned flag = (unsigned)g_arrowFlag, cell = (unsigned)g_arrowCell;
    BuildingHovers::SetArrowClickCallback(new GUI::Callback([flag, cell] { OnItem(flag, cell); }));
    g_arrowCell = -1;
    g_arrowFlag = -1;
}

// @0x32fe3c
void OnInfo() { PopupWindow::Show(StringTable::GetString("window_raid_explain"), PopupWindow::Hide, nullptr, nullptr); }

// @0x32fed0
bool OnBack() {
    if (!g_wnd->shown) return false;
    if (g_hover->shownHover) {
        g_hover->Hide();
        return true;
    }
    if (g_close->visibleSelf) {
        Hide();
        return true;
    }
    return false;
}

// @0x3305e4: the close button; on the mission's own map after its time ran out, back to the city.
void OnClose() {
    if (!g_info) return;
    if (g_info->mission && GameState::GetMapStartTime(g_map) != 0 &&
        GameState::GetMapStartTime(g_map) + g_info->timeout < (uint32_t)Timer::GetGlobalTime() &&
        (uint32_t)GameState::GetCurrentMapID() == g_map) {
        if (GameState::IsCombatActive()) GameState::GetActiveCombat()->EndCombat();
        GlobalMapWindow::Show();
        GlobalMapWindow::SwitchMap(0, false, false);
        GlobalMapWindow::Show();
    }
    Hide();
}

// @0x33068c
void Move(int x, int y) {
    for (FlagInfo& f : g_flags) f.scroller.Move(x, y);
}

// @0x3306e4
bool Click(int x, int y, bool pressed) {
    if (g_hover->Click(x, y, pressed)) return true;
    if (g_root->visibleSelf) {
        if (BuildingHovers::ClickOnArrow(x, y, pressed)) return true;
        if (!pressed) BuildingHovers::HideArrow();
        for (FlagInfo& f : g_flags)
            if (f.scroller.Click(x, y, pressed, g_root)) return true;
    }
    if (TopCityWindow::Click(x, y, pressed)) return true;
    const bool visible = g_root->visibleSelf;
    g_root->Click(x, y, pressed, false);
    return visible;
}

// @0x335c04: the "complete" tick shows the flag's drops anyway.
void OnFlag(unsigned flag) {
    if (!g_flags[flag].checkbox->visibleSelf) return;
    g_flags[flag].showItems = true;
    UpdateContents();
}

// @0x335e34: once a second the contents refresh; on a mission map whose time ran out the brief
// opens by itself.
void Update(float dt) {
    const int now = (int)Timer::GetGlobalTime();
    if (now != g_lastSecond) {
        const uint32_t map = (uint32_t)GameState::GetCurrentMapID();
        if (Locations::IsMissionMap(map) && Locations::GetMissionRemainingTime(map) == 0 && !g_root->visibleSelf &&
            WindowManager::GetShownWindowCount() == 0) {
            SetParameters(Locations::GetLocationWithMap(map), map, false);
            Show();
        }
        if (g_root->visibleSelf) UpdateContents();
    }
    g_lastSecond = (int)Timer::GetGlobalTime();
    if (!g_wnd->shown) return;
    for (FlagInfo& f : g_flags) f.scroller.Update(dt);
    if (!g_wnd->shown || !g_animRunning) return;
    UpdateContents();
    g_animTime += dt;
}

// @0x335f68: the crystal restart (asked first): ends the timeout, or restarts the time.
void OnGold(bool confirmed) {
    const int cost = GameState::AdjustCrystalCost((int)g_info->restartCost);
    NotEnoughWindow::ResetRequirements();
    NotEnoughWindow::AddRequirement(9, (unsigned)cost);
    if (!NotEnoughWindow::CheckRequirements()) {
        NotEnoughWindow::Show();
        return;
    }
    if (!confirmed) {
        ConfirmPurchaseWindow::SetParameters([] { OnGold(true); }, (unsigned)cost, false);
        ConfirmPurchaseWindow::Show();
        return;
    }
    SoundsManager::PlaySound("ui_buy_with_crystals", 1, false);
    GameState::ChangeResourceAmount(9, -cost);
    const uint32_t end = GameState::GetMapEndTime(g_map);
    // (Billing::LogCBPurchase(0x1d or 0x1e, location, cost): purchase statistics, online.)
    if (end != 0 && (uint32_t)Timer::GetGlobalTime() < end + g_info->timeout) {
        GameState::SetMapEndTime(g_map, 0);
        UpdateContents();
        OnBlue();
        return;
    }
    GameState::SetMapStartTime(g_map, (uint32_t)Timer::GetGlobalTime());
    const uint32_t cur = (uint32_t)GameState::GetCurrentMapID();
    UpdateContents();
    if (cur == g_map) {
        Hide();
        return;
    }
    OnBlue();
}

}  // namespace

// Static FunctionalWindow("RaidInfoWindow", Init, Deinit, Click, SetZ, Hide, Move).
WindowManager::FunctionalWindow* Queue() {
    if (!g_wnd) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.click = Click;
        f.setZ = SetZ;
        f.hide = Hide;
        f.move = Move;
        g_wnd = new WindowManager::FunctionalWindow("RaidInfoWindow", std::move(f));
    }
    return g_wnd;
}

// @0x32f1f4
bool IsVisible() { return g_wnd->shown; }

// @0x32f208
void SetAnimationOnce() { g_animateOnce = true; }

// @0x32f220: the next opening animates the newly won star.
void RunCompletionAnimation() {
    g_animTime = 0.f;
    g_animPending = true;
    g_animRunning = false;
}

// @0x3319c8
void Init() {
    g_tablet = GUI::IsTabletVersion();
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Window_raid_brief.xml", "Window_raid_brief.png", -1.f, 0,
                             0, 0, 0, false, 1.f);
    g_wnd->RegisterTopWindow(g_root);
    g_scale = g_root->scale;
    g_root->SetVisibility(false);
    g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::GetVerticalCenter(g_root->h));
    g_tween = GUI::CreateTweenEffect(g_root, nullptr, OnTweenIn, OnTweenOut);
    g_tween->CenterWith(0, 0, 0, (int)(g_scale * -64.f));
    g_close = GUI::GetWindowTyped<Window>(g_root, "x_button");
    g_close->SetOnClick(OnClose);
    g_close->SetActionSound("ui_close", 1);
    g_help = GUI::GetWindowTyped<Window>(g_root, "button_brief_help");
    g_help->SetOnClick(OnInfo);
    g_title = GUI::GetWindowTyped<Textfield>(g_root, "text_title");
    char name[64];
    for (int i = 0; i < 3; ++i) {
        std::snprintf(name, sizeof name, "raid_name_holder_%02d", i + 1);
        g_tabs[i].LoadFrom(g_root, name);
    }
    g_epicTab.LoadFrom(g_root, "raid_name_epic");
    g_progress.LoadFrom(g_root, "raid_progress_bar_holder");
    for (unsigned k = 0; k < 3; ++k) {
        std::snprintf(name, sizeof name, "raid_flag_holder_%02d", k + 1);
        g_flags[k].LoadFrom(g_root, name);
        for (unsigned i = 0; i < 8; ++i) g_flags[k].cells[i].root->SetOnClick([k, i] { OnItem(k, i); });
        for (Window* w : g_flags[k].image) w->SetOnClick([k] { OnFlag(k); });
    }
    g_time.LoadFrom(g_root, "raid_time_holder");
    for (int i = 0; i < 6; ++i) g_chain[i] = GUI::GetWindowTypedF<Window>(g_root, "chain_%02d", i + 1);
    g_restart.LoadFrom(g_root, "button_raid_start_02");
    g_restart.root->SetOnClick(OnBlue);
    g_centre.LoadFrom(g_root, "button_raid_start_centre");
    g_centre.root->SetOnClick(OnBlue);
    g_centre.gold->SetOnClick([] { OnGold(false); });
    g_resume.LoadFrom(g_root, "button_raid_start");
    g_resume.root->SetOnClick(OnBlue);
    g_resume.gold->SetOnClick([] { OnGold(false); });
    g_cost.LoadFrom(g_root, "raid_cost_holder");
    g_wnd->hideOnOuterClick = true;
    g_wnd->fn.update = Update;
    g_wnd->fn.zRange = ZRange;
    g_wnd->fn.back = OnBack;
    g_wnd->fn.show = Show;
    g_hover = new ItemHoverWindow();
}

// @0x331228: the mission (info), its map and what the hero found there (resources 0..10, then
// items, not packs); completed: shown after the mission's end. The flags scroll back to the top.
void SetParameters(Locations::LocationInfo* info, uint32_t map, bool completed) {
    g_info = info;
    g_finds.clear();
    g_map = map;
    for (int type = 0; type <= 10; ++type)
        if (int amount = GameState::GetMapResourceCollectionInfo(g_map, type)) g_finds.push_back({type, 0, amount, 0});
    for (unsigned i = 0; Items::ItemInfo* item = Items::EnumItemInfo(i); ++i) {
        const int amount = GameState::GetMapItemCollectionInfo(g_map, item->id);
        if (amount == 0 || item->id == 0x2000) continue;
        g_finds.push_back({0xb, item->id, amount, 0});
    }
    for (FlagInfo& f : g_flags) {
        f.scroller.firstIndex = 0;
        f.scroller.HandleMove(-f.scroller.offset);
    }
    g_completed = completed;
}

// @0x32ff44: start, resume or restart the mission. A start (or a restart after the time ran out)
// pays the star's requirements first (NotEnoughWindow when short) and starts its tasks again.
void OnBlue() {
    if (g_completed) {
        Hide();
        RaidCompleteWindow::ResetRaid(false);
        return;
    }
    if (g_info->mission) {
        const uint32_t start = GameState::GetMapStartTime(g_map);
        const bool expired = start != 0 && start + g_info->timeout < (uint32_t)Timer::GetGlobalTime();
        if (start == 0 || expired) {
            NotEnoughWindow::ResetRequirements();
            NotEnoughWindow::SetDescriptionText(StringTable::GetString("REQUIREMENT_RAID"), nullptr);
            const unsigned stars = (unsigned)Locations::GetMissionStarCount(g_info->startMap, 0);
            MetaData* req = g_info->requiresResources;
            if (!req || req->GetChildrenCount() <= stars) {
                std::printf("ERROR: RaidInfoWindow::OnBlue() No requirements for star count %d\n", (int)stars);
            } else {
                const MetaData* list = req->GetChild(stars);
                for (unsigned i = 0; i < list->GetChildrenCount(); ++i) {
                    const MetaData* c = list->GetChild(i);
                    if (c->GetChild(0)->type == 2) {
                        const char* name = c->GetChild(0)->GetString();
                        if (SDL_strcasecmp(name, "REPUTATION") != 0)
                            NotEnoughWindow::AddRequirement(GameState::StringToResourceType(name),
                                                            (unsigned)c->GetChild(1)->GetInt());
                        else
                            NotEnoughWindow::AddReputationRequirement((unsigned)c->GetChild(1)->GetChild(0)->GetInt(),
                                                                      (unsigned)c->GetChild(1)->GetChild(1)->GetInt());
                    } else if (c->GetChild(0)->type == 1) {
                        NotEnoughWindow::AddItemRequirement((unsigned)c->GetChild(0)->GetInt(),
                                                            (unsigned)c->GetChild(1)->GetInt());
                    }
                }
                if (!NotEnoughWindow::CheckRequirements()) {
                    NotEnoughWindow::SetActionCallback(OnBlue, StringTable::GetString("CAMPAIGN_INFO_BEGIN_FARM"), false);
                    NotEnoughWindow::Show();
                    return;
                }
                // The resources are paid (items and reputation only checked).
                for (unsigned i = 0; i < list->GetChildrenCount(); ++i) {
                    const MetaData* c = list->GetChild(i);
                    if (c->GetChild(0)->type != 2) continue;
                    const char* name = c->GetChild(0)->GetString();
                    if (SDL_strcasecmp(name, "REPUTATION") == 0) continue;
                    GameState::ChangeResourceAmount(GameState::StringToResourceType(name), -c->GetChild(1)->GetInt());
                }
            }
            GameState::SetMapEndTime(g_map, 0);
            for (int id : g_info->clearTasks) {
                Tasks::TaskInfo* t = Tasks::GetTask((uint32_t)id);
                if (t && !t->dnShowDefault) GameState::StartTask((unsigned)t->id);
            }
        }
    }
    Hide();
    // After the time ran out the map starts over: its quests, its items to remove, its save.
    if (g_info->mission && GameState::GetMapStartTime(g_map) != 0 &&
        GameState::GetMapStartTime(g_map) + g_info->timeout < (uint32_t)Timer::GetGlobalTime()) {
        for (int id : g_info->clearTasks) GameState::ResetTask((unsigned)id);
        for (int id : g_info->clearTasks) {
            Tasks::TaskInfo* t = Tasks::GetTask((uint32_t)id);
            if (t && !t->dnShowDefault) GameState::StartTask((unsigned)t->id);
        }
        for (int id : g_info->removeItems)
            if (id != 0) GameState::RemoveItem((uint32_t)id, GameState::GetItemAmount((uint32_t)id, false));
        SaveManager::GetMainSave()->ResetMap(g_map);
        GameState::ResetBossFightsOnMap(g_map);
        GameState::ResetMapCollectionInfo(g_map);
        GameState::SetMapStartTime(g_map, 0);
        GameState::SetMapEndTime(g_map, 0);
        if ((uint32_t)GameState::GetCurrentMapID() == g_map) {
            if (GameState::IsCombatActive()) GameState::GetActiveCombat()->EndCombat();
            GlobalMapWindow::SwitchMap(g_map, true, false);
            return;
        }
    }
    // On the mission's own map the time starts now.
    if ((uint32_t)GameState::GetCurrentMapID() == g_map)
        GameState::SetMapStartTime((uint32_t)GameState::GetCurrentMapID(), (uint32_t)Timer::GetGlobalTime());
    if (g_animateOnce) GlobalMapWindow::SwitchMapAnimated(g_map, false, 500.f, false);
    else GlobalMapWindow::SwitchMap(g_map, false, false);
}

// @0x332728
void UpdateContents() {
    if (!g_info) return;
    // The completion animation: the bar's share a (0..1 over the first half second) and the fade f.
    float a, f;
    if (g_animRunning && g_animTime > 1.5f) {
        g_animRunning = false;
        g_animTime = 0.f;
        g_animPending = false;
        a = 0.f;
        f = -0.5f;
    } else {
        a = std::min(g_animTime + g_animTime, 1.f);
        f = g_animTime - 0.5f;
        if (f > 1.f) f = 1.f;
    }
    const bool late = g_animRunning && !(g_animTime < 0.5f);   // the animation's second half
    unsigned stars = (unsigned)Locations::GetMissionStarCount(g_info->startMap, g_completed ? 1 : 0);
    const unsigned starsBase = (unsigned)Locations::GetMissionStarCount(g_info->startMap, 0);
    if (g_animRunning && g_animTime < 0.5f) stars = starsBase;
    // The progress towards the next star (p) and over all three (total), now and before the run.
    float p, total;
    if (stars == 3) {
        p = total = 1.f;
    } else {
        const int lo = stars == 0 ? 0 : (int)g_info->starsCount[stars - 1];
        const int count = GameState::GetMapCompletionCount(g_map) + (g_completed ? 1 : 0);
        p = (float)(count - lo) / (float)(g_info->starsCount[stars] - (unsigned)lo);
        total = p / 3.f + (float)stars / 3.f;
    }
    {
        const int lo = starsBase == 0 ? 0 : (int)g_info->starsCount[starsBase - 1];
        const int count = GameState::GetMapCompletionCount(g_map);
        const float pBase =
            stars == 3 ? 1.f : (float)(count - lo) / (float)(g_info->starsCount[starsBase] - (unsigned)lo);
        const float totalBase = starsBase == 3 ? 1.f : pBase / 3.f + (float)starsBase / 3.f;
        if (g_animRunning) {
            p = pBase + a * (p - pBase);
            total = totalBase + a * (total - totalBase);
        }
    }
    g_close->SetVisibility(g_info->mission ? !g_completed : false);
    g_title->SetText(SWPrintf(0x100, StringTable::GetString("RAID_HEADER"), {g_info->name}).c_str());
    for (int i = 0; i < 3; ++i) g_tabs[i].text->SetText(StringTable::GetStringF("RAID_TAB_%d", i + 1));
    g_epicTab.text->SetText(StringTable::GetString("RAID_TAB_EPIC"));
    for (TabInfo& t : g_tabs) t.root->SetVisibility(stars != 3);
    g_epicTab.root->SetVisibility(stars == 3);

    // The flags.
    for (unsigned k = 0; k < 3; ++k) {
        FlagInfo& fl = g_flags[k];
        const bool won = k < stars, above = stars < k, wonBefore = k < starsBase;
        bool grid;
        if (!won) grid = true;
        else if (late) grid = !wonBefore;
        else grid = false;
        if (fl.showItems) grid = true;
        for (unsigned j = 0; j < 3; ++j) fl.image[j]->SetVisibility(k == j);
        fl.root->SetVisibility(true);
        if (!grid) fl.title->SetText(StringTable::GetString("TAP_TO_SHOW_ITEMS"));
        else
            fl.title->SetText(StringTable::GetString(above ? "CAMPAIGN_INFO_UNLOCK_TO_FIND" : "CAMPAIGN_INFO_CHANCE_TO_FIND"));
        if (late && !won) {
            fl.title->SetAlpha(1.f, false);
            fl.checkbox->SetAlpha(1.f, false);
            fl.checkbox->SetVisibility(false);
        } else if (late && !wonBefore) {
            fl.title->SetAlpha(1.f - f, false);
            fl.checkbox->SetAlpha(f, false);
            fl.checkbox->SetVisibility(true);
        } else {
            fl.title->SetAlpha(1.f, false);
            fl.checkbox->SetAlpha(1.f, false);
            fl.checkbox->SetVisibility(won && !fl.showItems);
        }
        const MetaData* lists = g_info->items;
        if (!lists || lists->GetChildrenCount() <= k) {
            std::printf("ERROR: RaidInfoWindow::UpdateContents() No items for flag %d\n", (int)k);
            continue;
        }
        const MetaData* list = lists->GetChild(k);
        fl.scroller.itemCount = list->GetChildrenCount() + 2;
        for (ItemInfo& c : fl.cells) c.root->MoveWindow(-fl.offX, -fl.offY);
        fl.offX = fl.offY = 0;
        // The drops: gold and XP of this star, then the flag's items (not those of a profession the
        // hero cannot use yet), the likeliest first.
        g_collected.clear();
        const float goldMult = Locations::GetMapGoldMult(g_info->startMap, k);
        const float xpMult = Locations::GetMapXPMult(g_info->startMap, k);
        g_collected.push_back(
            {8, 0, (int)(goldMult * (float)g_info->gold * Setting("raid_star_gold_mult").GetChild(k).GetFloat()), 10});
        g_collected.push_back({10, 0, (int)(xpMult * (float)g_info->xp), 9});
        for (unsigned i = 0; i < list->GetChildrenCount(); ++i) {
            const MetaData* c = list->GetChild(i);
            if (c->GetChildrenCount() == 0) break;
            Items::ItemInfo* item = Items::GetItemInfo((uint32_t)c->GetChild(0)->GetInt());
            Professions::Profession* prof = item ? Professions::GetProfession(item->reputationId) : nullptr;
            if (!item || (prof && (!prof->IsUnlocked() || Professions::GetPlayerProfessionLevel(item->reputationId) <
                                                              item->reputationLevel))) {
                --fl.scroller.itemCount;
                continue;
            }
            CollectedInfo e{0xb, (uint32_t)c->GetChild(0)->GetInt(), 0, 0};
            const int prob = GetItemDropProbability(g_info, k, item->id);
            e.key = prob == 0 ? 7 : (prob == 1 ? 5 : 0);
            g_collected.push_back(e);
        }
        std::stable_sort(g_collected.begin(), g_collected.end());
        fl.shown.clear();
        for (const CollectedInfo& e : g_collected) fl.shown.push_back(e.id);
        // The cells from the scroller's first line; one or two are centred.
        unsigned shownCells = 0;
        bool centreY = true;
        const unsigned first = fl.scroller.firstIndex;
        if (grid && first < g_collected.size()) {
            for (;;) {
                ItemInfo& c = fl.cells[shownCells];
                c.root->SetVisibility(true);
                const CollectedInfo& e = g_collected[shownCells + first];
                if (Items::ItemInfo* item = Items::GetItemInfo(e.id)) {
                    for (unsigned j = 0; j < 5; ++j) c.level[j]->SetVisibility(item->itemLevel == j);
                    item->PutImageInHolder(c.icon);
                    // The count already found on the map (only on the current star's flag).
                    int found = 0;
                    if (!g_finds.empty() && k == stars) {
                        for (const MapFind& m : g_finds) {
                            found = m.id == item->id ? m.amount : 0;
                            if (found != 0) break;
                        }
                    }
                    c.owned->SetVisibility(false);
                    c.quantity->SetVisibility(found != 0);
                    c.quantity->SetText(ToWideString(found));
                    const int prob = GetItemDropProbability(g_info, k, item->id);
                    for (int j = 0; j < 4; ++j) c.label[j].root->SetVisibility(j - 1 == prob);
                    c.label[1].text->SetText(StringTable::GetString("PROBABILITY_HIGH"));
                    c.label[2].text->SetText(StringTable::GetString("PROBABILITY_MEDIUM"));
                    c.label[3].text->SetText(StringTable::GetString("PROBABILITY_LOW"));
                    c.id = item->id;
                    c.probability = prob;
                } else if (e.type != 0xb) {
                    // Gold or XP.
                    for (unsigned j = 0; j < 5; ++j) c.level[j]->SetVisibility(j == 0);
                    c.icon->SetTexture(IconManager::GetIcon(GameState::GetResourceMapIconName(e.type), false), true);
                    c.owned->SetVisibility(false);
                    c.quantity->SetVisibility(1 < (unsigned)e.amount);
                    c.quantity->SetText(ToWideString(e.amount));
                    for (Shared::DiscountInfo& d : c.label) d.root->SetVisibility(false);
                    c.id = 0;
                }
                ++shownCells;
                if (shownCells + first >= g_collected.size()) {
                    if (shownCells == 1) fl.offX = (fl.cells[1].root->x - fl.cells[0].root->x) / 2;
                    centreY = shownCells <= 2;
                    break;
                }
                if (shownCells == 8) {
                    centreY = false;
                    break;
                }
            }
        }
        if (centreY) fl.offY = (fl.cells[2].root->y - fl.cells[0].root->y) / 2;
        for (ItemInfo& c : fl.cells) c.root->MoveWindow(fl.offX, fl.offY);
        for (unsigned i = shownCells; i < 8; ++i) fl.cells[i].root->SetVisibility(false);
        // A star above the next one is greyed out.
        fl.image[k]->SetEnabled(!above);
        for (ItemInfo& c : fl.cells) {
            for (Window* w : c.level) w->SetEnabled(!above);
            c.icon->SetEnabled(!above);
            for (Shared::DiscountInfo& d : c.label) d.root->SetEnabled(!above);
        }
        fl.scroller.UpdateScroll();
        if (!grid) fl.scroller.scrollbar->SetVisibility(false);
        bool faded = false;
        if (late && won && !wonBefore) {
            for (ItemInfo& c : fl.cells) {
                c.root->SetAlpha(1.f - f, true);
                c.quantity->SetAlpha(1.f - f, true);
            }
            faded = true;
        } else {
            for (ItemInfo& c : fl.cells) c.root->SetAlpha(1.f, true);
        }
        // The newly reached star's flag appears.
        if (late && !above && starsBase < k) {
            fl.root->SetEnabled(true);
            fl.root->SetAlpha(f, true);
            fl.root->SetCustomShader(10, true);
        } else if (!faded) {
            fl.root->SetAlpha(1.f, true);
        }
    }

    // The star bar: "Raid Complete!" over each won star, the overall percentage over the next.
    g_progress.clip.right = (int)((float)g_progress.clip.left + (float)g_progress.bar->w * total);
    for (unsigned i = 0; i < 3; ++i) g_progress.under[i]->SetVisibility(i <= stars);
    for (unsigned i = 0; i < 3; ++i) g_progress.over[i]->SetVisibility(i <= stars);
    for (unsigned i = 0; i <= stars; ++i) {
        std::u32string text;
        if (i < stars) {
            text = SWPrintf(0x100, StringTable::GetString("RAID_COMPLETED"));
        } else {
            const char32_t* percent = ToWideString((int)(p * 100.f));
            text = SWPrintf(0x100, StringTable::GetString("CAMPAIGN_INFO_STAR_PERCENT"), {percent});
        }
        if (i > 2) continue;
        g_progress.under[i]->SetText(text.c_str());
        g_progress.under[i]->SetAlpha(late && starsBase < i ? f : 1.f, false);
        g_progress.over[i]->SetText(text.c_str());
    }
    for (unsigned i = 0; i < 3; ++i) {
        g_progress.lock[i]->SetVisibility(late ? starsBase < i : stars < i);
        g_progress.lock[i]->SetAlpha(late && i == stars ? 1.f - f : 1.f, false);
    }

    // The timer and the buttons.
    g_time.icon->SetTexture(IconManager::GetIcon("tab_inv_acc_timer", false), true);
    g_time.timeout->SetAsyncUpdate(false);
    const uint32_t now = (uint32_t)Timer::GetGlobalTime();
    const uint32_t end = GameState::GetMapEndTime(g_map);
    const uint32_t start = GameState::GetMapStartTime(g_map);
    const char* requirement = "CAMPAIGN_INFO_REQUIRED";
    auto showCentre = [](bool gold, const char* text, const char* icon) {
        g_restart.root->SetVisibility(false);
        g_centre.root->SetVisibility(true);
        g_centre.gold->SetVisibility(gold);
        g_centre.active->SetVisibility(!gold);
        g_centre.crystalIcon->SetVisibility(gold);
        g_centre.lock->SetVisibility(false);
        g_centre.goldText->SetVisibility(gold);
        g_centre.price->SetVisibility(gold);
        g_centre.text->SetVisibility(!gold);
        g_centre.icon->SetVisibility(!gold);
        if (gold) {
            g_centre.goldText->SetText(StringTable::GetString(text));
            g_centre.price->SetText(ToWideString(GameState::AdjustCrystalCost((int)g_info->restartCost)));
        } else {
            g_centre.text->SetText(StringTable::GetString(text));
            g_centre.icon->SetTexture(IconManager::GetIcon(icon, false));
        }
        g_resume.root->SetVisibility(false);
    };
    if (end != 0 && now < end + g_info->timeout) {
        // In the timeout after a run: the restart for crystals.
        g_time.root->SetVisibility(true);
        g_cost.root->SetVisibility(true);
        g_time.caption->SetText(StringTable::GetString("CAMPAIGN_INFO_FARM_IN_TIMEOUT"));
        g_time.time->SetVisibility(false);
        g_time.timeout->SetVisibility(true);
        g_time.timeout->SetText(StringTable::GetExtendedTimeString(0x100, (int)(end + g_info->timeout - now), false).c_str());
        g_restart.root->SetVisibility(false);
        showCentre(true, "CAMPAIGN_INFO_FORCE_RESTART", nullptr);
    } else if (start == 0) {
        // Not started: the time it gives.
        g_time.root->SetVisibility(true);
        g_cost.root->SetVisibility(true);
        g_time.caption->SetText(StringTable::GetString("CAMPAIGN_INFO_TIME_TO_COMPLETE"));
        g_time.time->SetVisibility(true);
        g_time.time->SetText(StringTable::GetExtendedTimeString(0x100, (int)g_info->timeout, false).c_str());
        g_time.timeout->SetVisibility(false);
        showCentre(false, "CAMPAIGN_INFO_BEGIN_FARM", "gold_flag");
    } else if (g_completed) {
        // After the run: the header says so.
        g_time.root->SetVisibility(false);
        g_cost.root->SetVisibility(false);
        g_title->SetText(StringTable::GetString("CAMPAIGN_INFO_FARM_COMPLETE"));
        showCentre(false, "CAMPAIGN_INFO_END_FARM", "gold_confirm");
    } else if (start + g_info->timeout < now) {
        // Time is up: the restart (after a second timeout, beside a crystal resume).
        g_time.root->SetVisibility(true);
        g_cost.root->SetVisibility(true);
        g_time.caption->SetVisibility(true);
        g_time.caption->SetText(StringTable::GetString("CAMPAIGN_INFO_TIME_IS_UP"));
        g_time.time->SetVisibility(false);
        g_time.timeout->SetVisibility(true);
        g_time.timeout->SetText(U"00:00");
        const int remaining = (int)(start + g_info->timeout * 2 - now);
        if (remaining < 1) {
            g_restart.root->SetVisibility(true);
            g_restart.text->SetText(StringTable::GetString("CAMPAIGN_INFO_RESTART_FARM"));
            g_restart.icon->SetTexture(IconManager::GetIcon("gold_rotate", false));
            g_centre.root->SetVisibility(false);
            g_resume.root->SetVisibility(true);
            g_resume.gold->SetVisibility(true);
            g_resume.active->SetVisibility(false);
            g_resume.crystalIcon->SetVisibility(true);
            g_resume.lock->SetVisibility(false);
            g_resume.goldText->SetVisibility(true);
            g_resume.price->SetVisibility(true);
            g_resume.text->SetVisibility(false);
            g_resume.goldText->SetText(StringTable::GetString("CAMPAIGN_INFO_RESUME_BUTTON"));
            g_resume.price->SetText(ToWideString(GameState::AdjustCrystalCost((int)g_info->restartCost)));
            g_resume.icon->SetVisibility(false);
        } else {
            g_time.caption->SetText(StringTable::GetString("CAMPAIGN_INFO_FARM_IN_TIMEOUT"));
            g_time.timeout->SetText(StringTable::GetExtendedTimeString(0x100, remaining, false).c_str());
            showCentre(true, "CAMPAIGN_INFO_RESUME_BUTTON", nullptr);
        }
        requirement = "CAMPAIGN_INFO_RESTART_AND_SPEND";
    } else {
        // Running: the time left.
        g_time.root->SetVisibility(true);
        g_cost.root->SetVisibility(true);
        g_time.caption->SetText(StringTable::GetString("MISSION_TIME_LEFT"));
        g_time.time->SetVisibility(true);
        g_time.time->SetText(StringTable::GetExtendedTimeString(0x100, (int)(start + g_info->timeout - now), false).c_str());
        g_time.timeout->SetVisibility(false);
        showCentre(false, "CAMPAIGN_INFO_RESUME", "gold_flag");
    }
    g_cost.text->SetText(StringTable::GetString(requirement));
    g_chain[0]->SetVisibility(g_restart.root->visibleSelf);
    g_chain[1]->SetVisibility(g_restart.root->visibleSelf);
    g_chain[2]->SetVisibility(g_resume.root->visibleSelf);
    g_chain[3]->SetVisibility(g_resume.root->visibleSelf);
    g_chain[4]->SetVisibility(g_centre.root->visibleSelf);
    g_chain[5]->SetVisibility(g_centre.root->visibleSelf);

    // The next run's requirements (of the star count reached), centred.
    MetaData* req = g_info->requiresResources;
    if (!req || req->GetChildrenCount() <= stars) {
        std::printf("ERROR: RaidInfoWindow::UpdateContents() No requirements for star count %d\n", (int)stars);
        return;
    }
    for (CostResourceInfo& r : g_cost.res) r.root->MoveWindow(-g_costOffset, 0);
    g_costOffset = 0;
    const MetaData* list = req->GetChild(stars);
    for (unsigned i = 0; i < 5; ++i) {
        if (list->GetChildrenCount() <= i) {
            for (unsigned j = i; j < 5; ++j) g_cost.res[j].root->SetVisibility(false);
            g_costOffset = (int)((float)((g_cost.res[1].root->x - g_cost.res[0].root->x) * (int)(5 - i)) * 0.5f);
            for (CostResourceInfo& r : g_cost.res) r.root->MoveWindow(g_costOffset, 0);
            break;
        }
        CostResourceInfo& r = g_cost.res[i];
        r.root->SetVisibility(true);
        r.icon->SetVisibility(true);
        const MetaData* c = list->GetChild(i);
        int need = 0;
        bool enough = true;
        if (c->GetChild(0)->type == 2) {
            const char* name = c->GetChild(0)->GetString();
            if (SDL_strcasecmp(name, "REPUTATION") != 0) {
                const int type = GameState::StringToResourceType(name);
                r.icon->SetTexture(IconManager::GetIcon(GameState::GetResourceIconName(type), false), true);
                need = c->GetChild(1)->GetInt();
                enough = need <= (int)GameState::GetResourceAmount(type);
            } else {
                // REPUTATION: the profession's icon and its level.
                const uint32_t id = (uint32_t)c->GetChild(1)->GetChild(0)->GetInt();
                if (Professions::Profession* p = Professions::GetProfession(id))
                    r.icon->SetTexture(p->icon, true, 0, (unsigned)(int)((float)r.icon->origH * 1.5f), true);
                need = c->GetChild(1)->GetChild(1)->GetInt();
                enough = need <= (int)Professions::GetPlayerProfessionLevel(id);
            }
        } else if (c->GetChild(0)->type == 1) {
            if (Items::ItemInfo* item = Items::GetItemInfo((uint32_t)c->GetChild(0)->GetInt()))
                r.icon->SetTexture(item->GetIcon(), true, 0, (unsigned)(int)((float)r.icon->origH * 1.5f), true);
            need = c->GetChild(1)->GetInt();
            enough = need <= GameState::GetItemAmount((uint32_t)c->GetChild(0)->GetInt(), false);
        }
        r.enough->SetVisibility(enough);
        r.enough->SetText(ToWideString(need));
        r.notEnough->SetVisibility(!enough);
        r.notEnough->SetText(ToWideString(need));
    }
    g_root->UpdatePosition();
    Render::SortRenderLayer(Render::kLayerGUI, 1);
}

// @0x335c40: the next opening points at item id's cell (its flag scrolled to it).
void ShowItem(uint32_t id) {
    if (!g_info) return;
    const unsigned stars = std::min((unsigned)Locations::GetMissionStarCount(g_info->startMap, 0), 3u);
    for (unsigned flag : kFlagOrder[stars]) {
        FlagInfo& fl = g_flags[flag];
        for (unsigned i = 0; i < fl.shown.size(); ++i) {
            if (fl.shown[i] != id) continue;
            fl.scroller.ScrollIntoView(i);
            fl.showItems = true;
            g_arrowFlag = (int)flag;
            g_arrowCell = (int)(i - fl.scroller.firstIndex);
            UpdateContents();
            return;
        }
    }
}

// @0x335d64
void Show() {
    g_wnd->shown = true;
    g_wnd->MoveWindowOnTop(true);
    if (!g_root->visibleSelf) {
        WindowManager::WindowShow(false);
        g_tween->AnimateIn();
    }
    g_root->SetVisibility(true);
    for (FlagInfo& f : g_flags) f.showItems = false;
    UpdateContents();
    g_root->PreloadTextures();   // GUI::PreloadWindow
    GameState::RaiseGamePauseState();
    if (g_completed) SoundsManager::PlaySound("ui_task_complete", 1, false);
    g_animateOnce = false;
}

// @0x32fe84
void Hide() {
    if (g_root->visibleSelf) g_tween->AnimateOut();
    g_hover->Hide();
}

}  // namespace RaidInfoWindow
