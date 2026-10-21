// LevelInfoWindow: the levels and professions window ("Window_lvl"). Five tabs: tab 0 the player's
// levels, the others a profession / reputation each, a strip of rank holders (progress bar, rank,
// the items it unlocks and the buildings of the level) under a strip of the items already unlocked.
// Port of LevelInfoWindow (libkingdom.so 5.11, @0x2ea3d8..0x2f13d8 and the LoadFroms; statics
// 0x624014..0x6253f8).
//
// 5n-3: the window opens and closes; 5n-4: UpdateContents; 5n-5: the taps and the help flow.
#include <cstdio>
#include <string>
#include <vector>

#include "engine/SoundsManager.h"
#include "engine/IconManager.h"
#include "engine/Resources.h"
#include "game/BuildingHovers.h"
#include "game/Collections.h"
#include "game/Entity.h"
#include "game/EntityManager.h"
#include "game/Contracts.h"
#include "game/GameData.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/Professions.h"
#include "game/PvP.h"
#include "game/Setting.h"
#include "game/StringTable.h"
#include "game/Tasks.h"
#include "engine/Render.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/ContentScroller.h"
#include "hud/HUD.h"
#include "hud/ItemHoverWindow.h"
#include "hud/Shared.h"
#include "windows/Windows.h"

namespace LevelInfoWindow {

namespace {

using GUI::Textfield;
using GUI::Window;

void UpdateContents();
void OnUnlockedItem(unsigned slot);
void OnItem(unsigned holder, unsigned slot);
void OnBuilding(unsigned holder, unsigned slot);
void OnLevelHolderDown();
void OnLevelHolderUp();
void OnShop(unsigned holder);
void OnHelp();

// @0x2eb83c (0x44 bytes): one item slot (Inventory_item_icon).
struct ItemInfo {
    Window* root = nullptr;           // +0x00 "%s"
    Window* selected = nullptr;       // +0x04 selected_item
    Window* locked = nullptr;         // +0x08 item_class_locked
    Window* itemClass[5] = {};        // +0x0c item_class_01..05
    Window* icon = nullptr;           // +0x20 item_icon_holder_item (takes a depth slot)
    Window* lock = nullptr;           // +0x24 lock
    Textfield* lockLevel = nullptr;   // +0x28 lock.text_level_req
    Window* redX = nullptr;           // +0x2c icon_red_x (icon "red_x")
    Window* goldOwned = nullptr;      // +0x30 icon_gold_owned (icon "gold_confirm")
    Window* recommended = nullptr;    // +0x34 recomended_item
    Textfield* quantity = nullptr;    // +0x38 text_quantity
    Textfield* quantityRed = nullptr; // +0x3c text_quantity_red
    int item = 0;                     // +0x40

    void LoadFrom(Window* w, const char* p) {
        static const char* const kHolder = "%s.inventory_item_icon.inventory_item_icon_holder";
        char name[0x100];
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        selected = GUI::GetWindowTypedF<Window>(w, "%s.inventory_item_icon.selected_item", p);
        std::snprintf(name, sizeof name, "%s.item_class_locked", kHolder);
        locked = GUI::GetWindowTypedF<Window>(w, name, p);
        for (int i = 0; i != 5; ++i) {
            std::snprintf(name, sizeof name, "%s.item_class_%%02d", kHolder);
            itemClass[i] = GUI::GetWindowTypedF<Window>(w, name, p, i + 1);
        }
        std::snprintf(name, sizeof name, "%s.item_icon_holder_item", kHolder);
        icon = GUI::GetWindowTypedF<Window>(w, name, p);
        icon->takesZ = true;
        std::snprintf(name, sizeof name, "%s.lock", kHolder);
        lock = GUI::GetWindowTypedF<Window>(w, name, p);
        std::snprintf(name, sizeof name, "%s.lock.text_level_req", kHolder);
        lockLevel = GUI::GetWindowTypedF<Textfield>(w, name, p);
        std::snprintf(name, sizeof name, "%s.icon_red_x", kHolder);
        redX = GUI::GetWindowTypedF<Window>(w, name, p);
        redX->SetTexture(IconManager::GetIcon("red_x", false));
        std::snprintf(name, sizeof name, "%s.icon_gold_owned", kHolder);
        goldOwned = GUI::GetWindowTypedF<Window>(w, name, p);
        goldOwned->SetTexture(IconManager::GetIcon("gold_confirm", false));
        std::snprintf(name, sizeof name, "%s.recomended_item", kHolder);
        recommended = GUI::GetWindowTypedF<Window>(w, name, p);
        quantity = GUI::GetWindowTypedF<Textfield>(w, "%s.inventory_item_icon.text_quantity", p);
        quantityRed = GUI::GetWindowTypedF<Textfield>(w, "%s.inventory_item_icon.text_quantity_red", p);
        item = 0;
    }
};

// @0x2eb740 (0x30 bytes): the PvP statistics (three lines) of the valor reputation.
struct PvPStatisticsHolder {
    Window* root = nullptr;           // +0x00 "%s"
    Window* bg = nullptr;             // +0x04 sliced_crem_bg_02 (hidden)
    Textfield* unlocked = nullptr;    // +0x08 reputation_unlocked
    Textfield* stat[3] = {};          // +0x0c pvp_stat_%02d.text_stat
    Textfield* num[3] = {};           // +0x18 pvp_stat_%02d.text_stat_num
    Window* icon[3] = {};             // +0x24 pvp_stat_%02d.icon_stat (takes a depth slot)

    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        bg = GUI::GetWindowTypedF<Window>(w, "%s.sliced_crem_bg_02", p);
        bg->SetVisibility(false);
        unlocked = GUI::GetWindowTypedF<Textfield>(w, "%s.reputation_unlocked", p);
        for (int i = 0; i != 3; ++i) {
            stat[i] = GUI::GetWindowTypedF<Textfield>(w, "%s.pvp_stat_%02d.text_stat", p, i + 1);
            num[i] = GUI::GetWindowTypedF<Textfield>(w, "%s.pvp_stat_%02d.text_stat_num", p, i + 1);
            icon[i] = GUI::GetWindowTypedF<Window>(w, "%s.pvp_stat_%02d.icon_stat", p, i + 1);
            icon[i]->takesZ = true;
        }
    }
};

// @0x2eba50 (0x388 bytes): the strip of the items already unlocked (nine slots, a sideways scroll).
struct UnlockedItemsHolder {
    Window* root = nullptr;           // +0x00 "%s"
    Textfield* text = nullptr;        // +0x04 reputation_unlocked
    ItemInfo items[9];                // +0x08 reputation_already_unlocked_item_holder.inventory_slot_holder_%02d
    std::vector<Items::ItemInfo*> list;   // +0x26c the items shown (UpdateContents)
    Shared::ContentScroller scroller; // +0x27c (scrollbar: scroll_bar_level_available.scroll_bar_tiny,
                                      // track and viewport: the holder's mask_universal)

    void LoadFrom(Window* w, const char* p) {
        char name[0x100];
        scroller.Init();
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        text = GUI::GetWindowTypedF<Textfield>(w, "%s.reputation_unlocked", p);
        for (int i = 0; i != 9; ++i) {
            std::snprintf(name, sizeof name, "%s.reputation_already_unlocked_item_holder.inventory_slot_holder_%02d", p,
                          i + 1);
            items[i].LoadFrom(w, name);
        }
        scroller.scrollbar = GUI::GetWindowTypedF<Window>(w, "%s.scroll_bar_level_available.scroll_bar_tiny", p);
        Window* mask = GUI::GetWindowTypedF<Window>(w, "%s.reputation_already_unlocked_item_holder.mask_universal", p);
        scroller.track = mask;
        scroller.viewport = {mask->x, mask->y, mask->x + mask->w, mask->y + mask->h};
        for (ItemInfo& it : items) {
            scroller.windows.push_back(it.root);
            it.root->SetClipRect(&scroller.viewport);
        }
        items[8].root->MoveWindow(items[1].root->x - items[0].root->x, 0);
        mask->MoveWindow((int)(w->scale * -12.f), 0);   // DAT_002ebcc0
        scroller.perLine = 1;
        scroller.horizontal = true;
        scroller.itemSize = items[1].root->x - items[0].root->x;
        scroller.onScroll = UpdateContents;
        scroller.unkA0 = 9;
    }
};

// @0x2ebce4 (0x4b8 bytes): one rank holder (a level / a rank): its bar, name, the items it
// unlocks (seven slots, a sideways scroll) and the buildings it opens (five slots, another).
struct LevelHolder {
    struct UnlockButton {
        Window* root = nullptr;       // button_unlock_item_NN
        Window* icon = nullptr;       // .icon_attack (takes a depth slot)
        Textfield* text = nullptr;    // .text_give_job
    };
    Window* root = nullptr;           // +0x000 "%s"
    Shared::ProgressBarLarge bar;     // +0x004 rep_lvl_bar
    Window* flag = nullptr;           // +0x028 flag_reputation_level
    Textfield* levelText = nullptr;   // +0x02c text_level_text
    Textfield* rank = nullptr;        // +0x030 text_rank
    Textfield* digit1 = nullptr;      // +0x034 text_level_number.text_level_digit_1
    Textfield* digit2 = nullptr;      // +0x038 text_level_number.text_level_digit_2
    Window* icon = nullptr;           // +0x03c icon_reputation (takes a depth slot)
    Textfield* rankNum = nullptr;     // +0x040 text_rank_num
    Window* checkbox = nullptr;       // +0x044 rank_achieved_checkbox
    Textfield* unlocked = nullptr;    // +0x048 rep_items_rank.reputation_unlocked
    ItemInfo items[7];                // +0x04c rep_items_rank.reputation_rank_items_holder.inventory_slot_holder_%02d
    Window* idle[5] = {};             // +0x228 ...image_holder_idle_%02d (take a depth slot)
    Textfield* itemClass[5] = {};     // +0x23c rep_items_rank.text_item_class_%02d
    UnlockButton unlock[3];           // +0x250 button_unlock_item_01, _03, _02 (the original's order)
    // +0x274 the slots of the items strip (UpdateContents): an item, or an order that makes one.
    struct Slot {
        Items::ItemInfo* item = nullptr;
        const Contracts::ContractMission* order = nullptr;
    };
    std::vector<Slot> slots;
    // +0x288 what the level opens: a building (kind 1: its first look, n + 1: its upgrade n) or a decoration.
    struct Opens {
        GameData::BuildingData* building = nullptr;
        GameData::DecorData* decor = nullptr;
        unsigned kind = 0;
    };
    std::vector<Opens> opens;
    Shared::ContentScroller scrollItems;   // +0x2a0 the items (scrollbar: scroll_bar_rank_unlocks.scroll_bar_tiny,
                                           // track and viewport: the holder's mask_universal)
    Shared::ContentScroller scrollIdle;    // +0x3ac the buildings (the same track and viewport)

    void LoadFrom(Window* w, const char* p) {
        char name[0x100];
        scrollItems.Init();
        scrollIdle.Init();
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        std::snprintf(name, sizeof name, "%s.rep_lvl_bar", p);
        bar.LoadFrom(w, name);
        flag = GUI::GetWindowTypedF<Window>(w, "%s.flag_reputation_level", p);
        levelText = GUI::GetWindowTypedF<Textfield>(w, "%s.text_level_text", p);
        rank = GUI::GetWindowTypedF<Textfield>(w, "%s.text_rank", p);
        digit1 = GUI::GetWindowTypedF<Textfield>(w, "%s.text_level_number.text_level_digit_1", p);
        digit2 = GUI::GetWindowTypedF<Textfield>(w, "%s.text_level_number.text_level_digit_2", p);
        icon = GUI::GetWindowTypedF<Window>(w, "%s.icon_reputation", p);
        icon->takesZ = true;
        rankNum = GUI::GetWindowTypedF<Textfield>(w, "%s.text_rank_num", p);
        checkbox = GUI::GetWindowTypedF<Window>(w, "%s.rank_achieved_checkbox", p);
        unlocked = GUI::GetWindowTypedF<Textfield>(w, "%s.rep_items_rank.reputation_unlocked", p);
        for (int i = 0; i != 7; ++i) {
            std::snprintf(name, sizeof name, "%s.rep_items_rank.reputation_rank_items_holder.inventory_slot_holder_%02d",
                          p, i + 1);
            items[i].LoadFrom(w, name);
            items[i].root->MoveWindow((int)(root->root->scale * -20.f), 0);   // DAT_002ebfe0
        }
        const int step = items[1].root->x - items[0].root->x;
        items[5].root->MoveWindow(step, 0);
        items[6].root->MoveWindow(step * 2, 0);
        for (int i = 0; i != 5; ++i) {
            idle[i] = GUI::GetWindowTypedF<Window>(
                w, "%s.rep_items_rank.reputation_rank_items_holder.image_holder_idle_%02d", p, i + 1);
            idle[i]->takesZ = true;
        }
        const int d = (idle[0]->x - idle[1]->x) + step * 2;
        for (int i = 1; i != 5; ++i) idle[i]->MoveWindow(d * i, 0);
        for (Window* g : idle) g->MoveWindow(d / 2, 0);
        idle[4]->MoveWindow(idle[1]->x - idle[0]->x, 0);
        for (int i = 0; i != 5; ++i)
            itemClass[i] = GUI::GetWindowTypedF<Textfield>(w, "%s.rep_items_rank.text_item_class_%02d", p, i + 1);
        static const char* const kButtons[3] = {"01", "03", "02"};
        for (int i = 0; i != 3; ++i) {
            unlock[i].root = GUI::GetWindowTypedF<Window>(w, "%s.rep_items_rank.button_unlock_item_%s", p, kButtons[i]);
            unlock[i].icon =
                GUI::GetWindowTypedF<Window>(w, "%s.rep_items_rank.button_unlock_item_%s.icon_attack", p, kButtons[i]);
            unlock[i].icon->takesZ = true;
            unlock[i].text = GUI::GetWindowTypedF<Textfield>(
                w, "%s.rep_items_rank.button_unlock_item_%s.text_give_job", p, kButtons[i]);
        }
        scrollItems.scrollbar =
            GUI::GetWindowTypedF<Window>(w, "%s.rep_items_rank.scroll_bar_rank_unlocks.scroll_bar_tiny", p);
        Window* mask =
            GUI::GetWindowTypedF<Window>(w, "%s.rep_items_rank.reputation_rank_items_holder.mask_universal", p);
        scrollItems.track = mask;
        scrollIdle.track = mask;
        scrollItems.viewport = scrollIdle.viewport = {mask->x, mask->y, mask->x + mask->w, mask->y + mask->h};
        for (ItemInfo& it : items) {
            scrollItems.windows.push_back(it.root);
            it.root->SetClipRect(&scrollItems.viewport);
        }
        for (Window* g : idle) {
            scrollIdle.windows.push_back(g);
            g->SetClipRect(&scrollIdle.viewport);
        }
        scrollItems.unkA0 = 7;
        scrollItems.horizontal = true;
        scrollItems.itemSize = items[1].root->x - items[0].root->x;
        scrollItems.perLine = 1;
        scrollItems.onScroll = UpdateContents;
        scrollIdle.unkA0 = 5;
        scrollIdle.horizontal = true;
        scrollIdle.itemSize = idle[1]->x - idle[0]->x;
        scrollIdle.perLine = 1;
        scrollIdle.onScroll = UpdateContents;
    }
};

// @0x2ec424 (0x1310 bytes): the scrolling column of the window: the unlocked items, the PvP
// statistics and three rank holders, one under the other.
struct GeneralHolder {
    Window* root = nullptr;           // +0x000 "%s"
    UnlockedItemsHolder unlocked;     // +0x004 rep_items_un_holder
    PvPStatisticsHolder pvp;          // +0x38c rep_stats_holder
    LevelHolder level[3];             // +0x3bc rep_lvl_hold_%02d
    std::vector<unsigned> levels;     // +0x11e4 the player levels (tab 0)
    std::vector<Professions::ProfessionLevel*> ranks;   // +0x11f4 a reputation's ranks (the other tabs)
    Shared::ContentScroller scroller; // +0x1204 (scrollbar: scroll_bar_reputation_scroller.scroll_bar_tiny,
                                      // track and viewport: mask_universal)

    void LoadFrom(Window* w, const char* p) {
        char name[0x100];
        scroller.Init();
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        std::snprintf(name, sizeof name, "%s.rep_items_un_holder", p);
        unlocked.LoadFrom(w, name);
        std::snprintf(name, sizeof name, "%s.rep_stats_holder", p);
        pvp.LoadFrom(w, name);
        for (int i = 0; i != 3; ++i) {
            std::snprintf(name, sizeof name, "%s.rep_lvl_hold_%02d", p, i + 1);
            level[i].LoadFrom(w, name);
        }
        // The second and third holder go down by one and two holders' heights (their clip too).
        const int h = level[0].root->h;
        level[1].root->MoveWindow(0, h);
        level[1].scrollItems.viewport.top += h;
        level[1].scrollItems.viewport.bottom += h;
        level[1].scrollIdle.viewport.top += h;
        level[1].scrollIdle.viewport.bottom += h;
        level[2].root->MoveWindow(0, h * 2);
        level[2].scrollItems.viewport.top += h * 2;
        level[2].scrollItems.viewport.bottom += h * 2;
        level[2].scrollIdle.viewport.top += h * 2;
        level[2].scrollIdle.viewport.bottom += h * 2;
        scroller.scrollbar = GUI::GetWindowTypedF<Window>(w, "%s.scroll_bar_reputation_scroller.scroll_bar_tiny", p);
        Window* mask = GUI::GetWindowTypedF<Window>(w, "%s.mask_universal", p);
        scroller.track = mask;
        scroller.viewport = {mask->x, mask->y, mask->x + mask->w, mask->y + mask->h};
        scroller.windows.assign({unlocked.root, pvp.root, level[0].root, level[1].root, level[2].root});
        scroller.rects.push_back(&unlocked.scroller.viewport);
        for (LevelHolder& l : level) {
            scroller.rects.push_back(&l.scrollItems.viewport);
            scroller.rects.push_back(&l.scrollIdle.viewport);
        }
        unlocked.root->SetClipRect(&scroller.viewport);
        pvp.root->SetClipRect(&scroller.viewport);
        for (LevelHolder& l : level) l.root->SetClipRect(&scroller.viewport);
        // Everything goes up by the unlocked strip's height (the first holder's start).
        const int up = unlocked.root->h;
        for (Window* g : scroller.windows) g->MoveWindow(0, -up);
        for (GUI::ClipRect* r : scroller.rects) {
            r->top -= up;
            r->bottom -= up;
        }
        scroller.horizontal = false;
        scroller.unkA0 = 3;
        scroller.itemSize = level[1].root->y - level[0].root->y;
        scroller.perLine = 1;
        scroller.padStart = unlocked.root->h;
        scroller.onScroll = UpdateContents;
        scroller.onNext = OnLevelHolderUp;
        scroller.onPrev = OnLevelHolderDown;
    }
};

bool g_tablet = false;                          // 0x60f2dc
float g_scale = 1.f;                            // 0x60f2e0 the layout's scale
Window* g_root = nullptr;                       // 0x624014
GUI::TweenEffect* g_tween = nullptr;            // 0x624018
Shared::TabHolder g_tabs[5];                    // 0x62401c tab_01..05
GeneralHolder g_general;                        // 0x6240d0 rep_level_scr
unsigned g_tab = 0;                             // 0x6253e4 the shown tab (OnTab; HelpWith... sets it)
Window* g_close = nullptr;                      // 0x6253ec x_button.clickArea
Textfield* g_title = nullptr;                   // 0x6253f0 text_title
Textfield* g_explained = nullptr;               // 0x6253f4 profession_explained
ItemHoverWindow* g_hover = nullptr;             // 0x6253e0
bool g_pvpOpen = false;                         // 0x6253e8 HelpWithPvP's argument
bool g_helpPending = false;                     // 0x6253ed OnHelp: the window closes, then OnActualHelp
WindowManager::FunctionalWindow* g_wnd = nullptr;   // 0x6253f8 "LevelInfoWindow"

// @0x2ea484
float ZRange() { return g_wnd->shown ? 0.07f : 0.01f; }

// @0x2eb094
bool OnBack() {
    if (!g_wnd->shown) return false;
    if (!g_hover->shownHover) {
        Hide();
        return true;
    }
    g_hover->Hide();
    return true;
}

// @0x2ea3d8
void SetZ(float z) {
    if (!g_wnd->shown) return;
    g_root->SetZ(z);
    if (g_tween->active) return;
    g_root->SetPosition(g_tween->GetCenteredX(), g_tween->GetCenteredY());
}

// @0x2eb454: the scrollers that are not being dragged follow the pointer; a drag on the column
// or on one of the holders' strips goes to that one alone.
void Move(int x, int y) {
    bool dragging = false;
    for (LevelHolder& l : g_general.level) dragging |= l.scrollItems.dragging | l.scrollIdle.dragging;
    const bool columnDragging = g_general.scroller.dragging;
    if (!dragging) {
        g_general.scroller.Move(x, y);
        if (columnDragging) return;
    } else if (columnDragging) {
        return;
    }
    for (LevelHolder& l : g_general.level) {
        l.scrollItems.Move(x, y);
        l.scrollIdle.Move(x, y);
    }
}

// @0x2eb558
bool Click(int x, int y, bool pressed) {
    if (!g_root->visibleSelf) {
        g_root->Click(x, y, pressed, false);
        return false;
    }
    if (g_hover->Click(x, y, pressed) || BuildingHovers::ClickOnArrow(x, y, pressed)) return true;
    if (!pressed) BuildingHovers::HideArrow();
    bool scrolled = g_general.scroller.Click(x, y, pressed, g_root);
    for (LevelHolder& l : g_general.level) {
        const bool a = l.scrollItems.Click(x, y, pressed, g_root);
        const bool b = l.scrollIdle.Click(x, y, pressed, g_root);
        scrolled = a | scrolled | b;
    }
    if (scrolled) return true;
    const bool visible = g_root->visibleSelf;
    g_root->Click(x, y, pressed, false);
    return visible;
}

// @0x2eb6d8
void Deinit() {
    GUI::RemoveTweenEffect(g_tween);
    g_tween = nullptr;
    delete g_root;
    delete g_hover;
}

// @0x2eaff4
void Update(float dt) {
    if (!g_root->visibleSelf) return;
    g_general.scroller.Update(dt);
    for (LevelHolder& l : g_general.level) {
        l.scrollItems.Update(dt);
        l.scrollIdle.Update(dt);
    }
}

// @0x2eb14c: the help the window asked for (OnHelp, HelpWithArena, HelpWithPvP): the windows in
// the way close, then the player is taken to the PvP arena's, the arena event's or the arena's
// first quests.
void OnActualHelp() {
    CharacterInfoWindow::Hide();
    ItemShopWindow::Hide();
    HireTroopsWindow::Hide();
    CraftItemWindow::Hide();
    Hide();
    ArenaDifficultyWindow::Hide();
    CampaignInfoWindow::Hide();
    PlayerCompetitionWindow::Hide();
    PlayerInfoWindow::Hide();
    if (GameState::GetCurrentMapID() == 0) {
        HealSquadWindow::Hide();
    } else if (Entity* player = EntityManager::GetPlayer()) {
        if (!player->IsDead()) HealSquadWindow::Hide();
    }
    if (g_tab != 1) {
        if (GameState::GetCurrentMapID() != 0x74) {   // the arena's map
            GlobalMapWindow::Show();
            GlobalMapWindow::SetArrowVisibleWindowLimit(5);
            GlobalMapWindow::ArrowAtMap(0x74, false, true);
            return;
        }
        if (g_tab == 2 && !GameState::TaskCompleted(0x50d)) {
            TaskInfoWindow::SetTask(0x50d, false);
            TaskInfoWindow::Show();
            return;
        }
        if (g_tab == 3 && !GameState::TaskCompleted(0x5af)) {
            TaskInfoWindow::SetTask(0x5af, false);
            TaskInfoWindow::Show();
            return;
        }
        ArenaEventWindow::Show();
        ArenaEventWindow::SetParameters(g_tab == 2);
        return;
    }
    if (GameState::TaskCompleted(0x74d)) {
        if (g_pvpOpen) CastleTopWindow::OnPvP(0xffffffffu);
        else CastleTopWindow::HelpWithPvP();
        return;
    }
    if (Tasks::IsTaskActive(0x74d, false)) {
        TaskInfoWindow::SetTask(0x74d, false);
        TaskInfoWindow::Show();
    } else if (Tasks::IsTaskActive(0x509, false)) {
        TaskInfoWindow::SetTask(0x509, false);
        TaskInfoWindow::Show();
    } else {
        GameState::TryToHelpWithTask(0x74f);
    }
}

// @0x2eb2f4
void OnTweenOut() {
    WindowManager::WindowHide(false);
    g_wnd->shown = false;
    g_wnd->MoveWindowDown(true);
    GameState::DropGamePauseState();
    if (!g_helpPending) return;
    g_helpPending = false;
    OnActualHelp();
}

// The item card over a slot (the slot's item, viewed only).
void ShowItemCard(const ItemInfo& slot, Items::ItemInfo* info) {
    SoundsManager::PlaySound("ui_click", 1, false);
    g_hover->Show();
    g_hover->SetItem(nullptr, nullptr, info, false, true, nullptr, false, false, false, false);
    const Window* w = slot.root;
    const int x = g_root->x + w->x, y = g_root->y + w->y;
    g_hover->SetTargetRect(x, y, x + w->w, y + w->h, ItemHoverWindow::kS);
    g_hover->RecomputePosition();
    g_hover->SetZ(0.001f);
    Render::SortRenderLayer(0xe, 1);
}

// @0x2ea6ac: an item of the unlocked strip.
void OnUnlockedItem(unsigned slot) {
    if (slot >= 9) return;
    Items::ItemInfo* info = Items::GetItemInfo((uint32_t)g_general.unlocked.items[slot].item);
    if (info) ShowItemCard(g_general.unlocked.items[slot], info);
}

// @0x2ea7f4: an item a rank unlocks.
void OnItem(unsigned holder, unsigned slot) {
    if (holder >= 3 || slot >= 7) return;
    Items::ItemInfo* info = Items::GetItemInfo((uint32_t)g_general.level[holder].items[slot].item);
    if (info) ShowItemCard(g_general.level[holder].items[slot], info);
}

// @0x2ea950: a building or decoration a level opens: its name and description.
void OnBuilding(unsigned holder, unsigned slot) {
    if (holder >= 3) return;
    LevelHolder& l = g_general.level[holder];
    const unsigned at = slot + l.scrollIdle.firstIndex;
    if (at >= l.opens.size()) return;
    SoundsManager::PlaySound("ui_click", 1, false);
    g_hover->Show();
    const LevelHolder::Opens& o = l.opens[at];
    if (o.building || o.decor) {
        const char* name = o.building ? o.building->name.c_str() : o.decor->name.c_str();
        std::u32string title;
        if (o.building)
            title = SWPrintf(0x100, U"%s [%d]", {StringTable::GetString(name), (int)o.kind});
        else
            title = StringTable::GetString(name);
        g_hover->SetText(title.c_str(), StringTable::GetStringF("%s_DESC", name));
    }
    const Window* w = l.idle[slot];
    const int x = g_root->x + w->x, y = g_root->y + w->y;
    g_hover->SetTargetRect(x, y, x + w->w, y + w->h, ItemHoverWindow::kS);
    g_hover->RecomputePosition();
    g_hover->SetZ(0.001f);
    Render::SortRenderLayer(0xe, 1);
}

// The state the holders' scrollers had before the column moved by one holder (the original keeps
// it in three spare fields of each scroller: +0x9c, +0xa8, +0xb8).
struct ScrollerState {
    unsigned itemCount = 0, firstIndex = 0;
    int offset = 0;
};
ScrollerState g_saved[3][2];

void SaveHolderScrollers() {
    for (unsigned k = 0; k != 3; ++k) {
        LevelHolder& l = g_general.level[k];
        g_saved[k][0] = {l.scrollItems.itemCount, l.scrollItems.firstIndex, l.scrollItems.offset};
        g_saved[k][1] = {l.scrollIdle.itemCount, l.scrollIdle.firstIndex, l.scrollIdle.offset};
    }
}

// Holder `to` takes the saved scroll state of holder `from` (strip 0: the items, 1: the buildings).
void TakeHolderScroll(unsigned to, unsigned from, unsigned strip) {
    LevelHolder& l = g_general.level[to];
    Shared::ContentScroller& s = strip == 0 ? l.scrollItems : l.scrollIdle;
    s.itemCount = g_saved[from][strip].itemCount;
    s.firstIndex = g_saved[from][strip].firstIndex;
    s.HandleMove(g_saved[from][strip].offset - g_saved[to][strip].offset);
}

// @0x2ead60: the column went one holder down: each holder's strips follow their level to the next
// holder, the first one starts at the left.
void OnLevelHolderDown() {
    SaveHolderScrollers();
    for (unsigned k = 1; k != 3; ++k) {
        TakeHolderScroll(k, k - 1, 0);
        TakeHolderScroll(k, k - 1, 1);
    }
    LevelHolder& l = g_general.level[0];
    l.scrollItems.firstIndex = 0;
    l.scrollItems.HandleMove(-g_saved[0][0].offset);
    l.scrollIdle.firstIndex = 0;
    l.scrollIdle.HandleMove(-g_saved[0][1].offset);
}

// @0x2eaebc: the column went one holder up: each holder takes the strips of the one below it, the
// last one starts at the left.
void OnLevelHolderUp() {
    SaveHolderScrollers();
    for (unsigned k = 0; k != 2; ++k) {
        TakeHolderScroll(k, k + 1, 0);
        TakeHolderScroll(k, k + 1, 1);
    }
    LevelHolder& l = g_general.level[2];
    l.scrollItems.firstIndex = 0;
    l.scrollItems.HandleMove(-l.scrollItems.offset);
    l.scrollIdle.firstIndex = 0;
    l.scrollIdle.HandleMove(-l.scrollIdle.offset);
}

// @0x2eb0f0: a holder's shop button: the shop on the holder's first item (the valor shop tab 7 on
// the valor tab).
void OnShop(unsigned holder) {
    if (holder >= 3) return;
    Hide();
    ItemShopWindow::Show();
    ItemShopWindow::ShowShopItem((uint32_t)g_general.level[holder].items[0].item, 0, true, g_tab == 1 ? 7 : (unsigned)-1);
}

// @0x2eb384: the help button: closes this window (OnTweenOut then runs OnActualHelp), or not
// while a fight is on.
void OnHelp() {
    if (GameState::IsCombatActive()) {
        PopupWindow::Show(StringTable::GetString("CAN_NOT_TRAVEL"), PopupWindow::Hide, nullptr, nullptr);
        return;
    }
    Hide();
    g_helpPending = true;
}

// UpdateContents' pieces (one 15 KB function in the original).

// A slot showing `item`: the class mark that fits, its icon and its id.
void ShowItem(ItemInfo& s, Items::ItemInfo* item) {
    for (unsigned j = 0; j != 5; ++j) s.itemClass[j]->SetVisibility(item->itemLevel == j);
    s.icon->SetTexture(item->GetIcon(), true);
    s.item = (int)item->id;
}

// The marks of a slot that the strips never show.
void HideItemMarks(ItemInfo& s) {
    s.goldOwned->SetVisibility(false);
    s.redX->SetVisibility(false);
    s.lock->SetVisibility(false);
    s.recommended->SetVisibility(false);
    s.locked->SetVisibility(false);
}

// The text of a bar (both of its texts show the same).
void SetBarText(LevelHolder& l, const std::u32string& text) {
    l.bar.textUnder->SetText(text.c_str());
    l.bar.textOver->SetText(text.c_str());
}

// Tab 0: the player's levels, three holders at a time: the level number, the xp bar and what the
// level unlocks (items, orders, buildings and decorations).
void UpdateLevels() {
    GeneralHolder& g = g_general;
    g.unlocked.root->SetVisibility(false);
    g_title->SetText(StringTable::GetString("LEVEL_INFO_HEADER"));
    g_explained->SetText(StringTable::GetString("LEVEL_INFO_DESCRIPTION"));
    for (unsigned k = 0; k != 3; ++k) {
        LevelHolder& l = g.level[k];
        l.root->SetVisibility(g.scroller.firstIndex + k < g.scroller.itemCount);
        if (l.root->visibleSelf) {
            const unsigned level = g.levels[g.scroller.firstIndex + k];
            const unsigned current = (unsigned)GameState::GetLevel();
            l.slots.clear();
            l.opens.clear();
            // Buildings (a building takes the level of its first look or of one of its upgrades)
            // and decorations open here; each takes two empty slots of the items strip.
            for (unsigned i = 0; GameData::BuildingData* b = GameData::EnumBuildings(i); ++i) {
                if (b->tab >= 10 || !b->buy) continue;
                unsigned kind = level == b->level;
                for (unsigned u = 0; u < b->upgrades.size(); ++u)
                    if (level == (unsigned)b->upgrades[u].level) kind = u + 2;
                if (kind == 0) continue;
                l.opens.push_back({b, nullptr, kind});
                l.slots.push_back({});
                l.slots.push_back({});
            }
            for (unsigned i = 0; GameData::DecorData* d = GameData::EnumDecors(i); ++i) {
                if (d->tab >= 10 || !d->buy || level != (unsigned)d->needLevel) continue;
                l.opens.push_back({nullptr, d, 1});
                l.slots.push_back({});
                l.slots.push_back({});
            }
            for (unsigned i = 0; Items::ItemInfo* it = Items::EnumItemInfo(i); ++i) {
                Professions::Profession* p = Professions::GetProfession(it->reputationId);
                if (it->level != level || !it->IsOnSale() || !it->GetIcon() || (p && p->type == Professions::kReputation))
                    continue;
                l.slots.push_back({it, nullptr});
            }
            for (unsigned i = 0; const Contracts::Contract* c = Contracts::EnumContract(i); ++i) {
                if (!GameData::GetBuildingDataWithContract(c)) continue;
                for (unsigned m = 0; m < c->count; ++m) {
                    const Contracts::ContractMission& order = c->missions[m];
                    if (order.playerLevel == level && order.item) l.slots.push_back({nullptr, &order});
                }
            }
            l.scrollItems.itemCount = (unsigned)l.slots.size();
            l.scrollIdle.itemCount = ((unsigned)l.slots.size() + 1) >> 1;   // (as in the original: not the buildings)
            const bool reached = level <= current;
            const bool passed = level < current;
            l.levelText->SetVisibility(true);
            l.digit1->SetVisibility(true);
            l.digit2->SetVisibility(true);
            l.rank->SetVisibility(false);
            l.icon->SetVisibility(false);
            l.rankNum->SetVisibility(false);
            l.flag->SetEnabled(reached);
            l.levelText->SetEnabled(reached);
            l.digit1->SetEnabled(reached);
            l.digit2->SetEnabled(reached);
            l.levelText->SetText(StringTable::GetString("NEW_LEVEL_DESC_3"));
            l.digit1->SetText(ToWideString((int)level));
            l.digit2->SetText(ToWideString((int)level));
            l.checkbox->SetVisibility(passed);
            l.bar.lock->SetVisibility(level > current);
            l.bar.root->SetEnabled(reached);
            if (passed) {
                l.unlocked->SetText(StringTable::GetString("LEVEL_INFO_DONE_ITEMS"));
                SetBarText(l, SWPrintf(0x100, StringTable::GetString("LEVEL_INFO_DONE"),
                                       {ToWideString((int)GameState::GetLevelXP(level))}));
                l.bar.SetProgress(1.f);
            } else if (reached) {   // the current level: the xp towards it
                l.unlocked->SetText(StringTable::GetString("LEVEL_INFO_DONE_ITEMS"));
                const unsigned before = GameState::GetLevelXP(level - 1);
                const float progress = (float)(int)(GameState::GetResourceAmount(10) - before) /
                                       (float)(unsigned)(GameState::GetLevelXP(level) - before);
                const char32_t* have = ToWideString((int)GameState::GetResourceAmount(10));
                const char32_t* need = ToWideString((int)GameState::GetLevelXP(level));
                SetBarText(l, SWPrintf(0x100, StringTable::GetString("LEVEL_INFO_NEED"), {have, need}));
                l.bar.SetProgress(progress);
            } else {
                l.unlocked->SetText(SWPrintf(0x100, StringTable::GetString("LEVEL_INFO_NEXT_ITEMS"),
                                             {ToWideString((int)level)})
                                        .c_str());
                SetBarText(l, SWPrintf(0x100, StringTable::GetString("LEVEL_INFO_NEXT"),
                                       {ToWideString((int)level - 1)}));
                l.bar.SetProgress(0.f);
            }
            for (unsigned i = 0; i != 7; ++i) {
                ItemInfo& s = l.items[i];
                const unsigned at = i + l.scrollItems.firstIndex;
                s.root->SetVisibility(at < l.slots.size());
                s.root->SetEnabled(true);
                if (!s.root->visibleSelf) continue;
                const LevelHolder::Slot slot = l.slots[at];
                HideItemMarks(s);
                if (slot.item) {
                    ShowItem(s, slot.item);
                } else if (!slot.order) {
                    s.root->SetVisibility(false);
                    s.item = 0;
                } else {
                    for (unsigned j = 0; j != 5; ++j) s.itemClass[j]->SetVisibility(j == 0);
                    if (slot.order->item) {
                        s.icon->SetTexture(slot.order->item->GetIcon(), true);
                        s.item = (int)slot.order->item->id;
                    } else {
                        s.item = 0;
                    }
                }
            }
            for (unsigned i = 0; i != 5; ++i) {
                const unsigned at = i + l.scrollIdle.firstIndex;
                l.idle[i]->SetVisibility(at < l.opens.size());
                if (!l.idle[i]->visibleSelf) continue;
                const LevelHolder::Opens& o = l.opens[at];
                Render::Texture* image;
                if (o.building)
                    image = GameData::BuildingImage(o.building, o.kind - 1);
                else if (o.decor)
                    image = GameData::DecorImage(o.decor);
                else
                    continue;
                GUI::FitImageIntoWindow(l.idle[i], image, true, true);
            }
            for (Textfield* t : l.itemClass) t->SetVisibility(false);
            for (LevelHolder::UnlockButton& b : l.unlock) b.root->SetVisibility(false);
        }
        l.scrollItems.UpdateScroll();
    }
}

// The other tabs: a reputation's ranks, three holders at a time: the rank, its points bar, the
// items it unlocks and the shop buttons; the strip of the items already unlocked (or the PvP
// statistics on the valor tab) over them.
void UpdateReputation() {
    GeneralHolder& g = g_general;
    Shared::ContentScroller& s = g.scroller;
    if (s.firstIndex == s.itemCount - 1 && s.offset == 0) s.HandleMove((int)(g_scale * 110.f));
    Professions::Profession* profession =
        g.ranks.empty() ? nullptr : Professions::GetProfession(g.ranks[0]->profession);
    g_title->SetText(profession ? profession->title : StringTable::GetString("LEVEL_INFO_HEADER_REP"));
    g_explained->SetText(StringTable::GetString(g_tab == 1   ? "LEVEL_INFO_REPUTATION_VALOR"
                                                : g_tab == 2 ? "LEVEL_INFO_REPUTATION_DESCRIPTION"
                                                             : "LEVEL_INFO_REPUTATION_DESCRIPTION2"));
    for (unsigned k = 0; k != 3; ++k) {
        LevelHolder& l = g.level[k];
        l.root->SetVisibility(s.firstIndex + k < s.itemCount);
        if (l.root->visibleSelf) {
            const unsigned at = s.firstIndex + k;
            const Professions::ProfessionLevel* rank = g.ranks[at];
            const Professions::ProfessionLevel* before = at != 0 ? g.ranks[at - 1] : nullptr;
            Professions::Profession* p = Professions::GetProfession(rank->profession);
            const unsigned points = GameState::GetProfessionPoints(p->id);
            const bool reached = rank->level <= Professions::GetPlayerProfessionLevel(p->id);
            const bool near = (int)(rank->level - Professions::GetPlayerProfessionLevel(p->id)) <= 1;   // not locked yet
            l.slots.clear();
            l.opens.clear();
            for (unsigned i = 0; Items::ItemInfo* it = Items::EnumItemInfo(i); ++i) {
                if (it->reputationId != p->id || it->reputationLevel != rank->level || it->isCraft || !it->GetIcon() ||
                    (!it->IsOnSale() && !Collections::GetCollectionWithResult(it->id)))
                    continue;
                l.slots.push_back({it, nullptr});
            }
            l.scrollItems.itemCount = (unsigned)l.slots.size();
            l.flag->SetEnabled(near);
            l.levelText->SetVisibility(false);
            l.digit1->SetVisibility(false);
            l.digit2->SetVisibility(false);
            l.rank->SetVisibility(true);
            l.icon->SetVisibility(true);
            l.rankNum->SetVisibility(true);
            l.rank->SetText(rank->name);
            l.icon->SetTexture(p->iconLarge, true);
            l.icon->SetEnabled(near);
            l.rankNum->SetText(ToWideString((int)(s.firstIndex + 1 + k)));
            l.checkbox->SetVisibility(reached);
            const char32_t* rankWord = StringTable::GetString("ARENA_EVENT_RANK");
            std::u32string text;
            if (reached) {
                l.unlocked->SetText(
                    SWPrintf(0x100, StringTable::GetString("LEVEL_INFO_REPUTATION_ACHIEVED"), {rank->name}).c_str());
                text = SWPrintf(0x100, U"%s %d - %s - %d/%d - %s",
                                {rankWord, (int)rank->level, rank->name, (int)rank->points, (int)rank->points,
                                 StringTable::GetString("CRAFT_ITEM_ACHIEVED")});
            } else {
                l.unlocked->SetText(
                    SWPrintf(0x100, StringTable::GetString("LEVEL_INFO_REPUTATION_UNLOCKS"), {rank->name}).c_str());
                if (near)
                    text = SWPrintf(0x100, U"%s %d - %s - %d/%d",
                                    {rankWord, (int)rank->level, rank->name, (int)points, (int)rank->points});
                else
                    text = SWPrintf(0x100, U"%s %d - %s", {rankWord, (int)rank->level, rank->name});
            }
            SetBarText(l, text);
            l.bar.lock->SetVisibility(!near);
            float progress = 0.f;
            const unsigned base = before ? before->points : 0;
            if (!before || base <= points) {
                progress = (float)(unsigned)(points - base) / (float)(unsigned)(rank->points - base);
                if (progress < 0.f)
                    progress = 0.f;
                else if (progress > 1.f)
                    progress = 1.f;
            }
            l.bar.SetProgress(progress);
            l.bar.root->SetEnabled(near);
            Items::ItemInfo* last = nullptr;
            for (unsigned i = 0; i != 7; ++i) {
                ItemInfo& slot = l.items[i];
                slot.root->SetVisibility(i + l.scrollItems.firstIndex < l.slots.size());
                slot.root->SetEnabled(true);
                if (!slot.root->visibleSelf) continue;
                last = l.slots[i + l.scrollItems.firstIndex].item;
                HideItemMarks(slot);
                ShowItem(slot, last);
            }
            for (Window* w : l.idle) w->SetVisibility(false);
            if (!last) {
                for (Textfield* t : l.itemClass) t->SetVisibility(false);
            } else {
                for (unsigned j = 0; j != 5; ++j) l.itemClass[j]->SetVisibility(last->itemLevel == j);
                l.itemClass[last->itemLevel]->SetText(last->title);
            }
            // The unlock help button (while no rank is reached) and the two shop buttons.
            bool shop = GameState::secondTutorial == 0x100;
            if (shop && g_tab == 1) shop = GameState::TaskCompleted((unsigned)Setting("shop_unlock_task").GetInt());
            const bool help = !reached && shop;
            l.unlock[0].root->SetVisibility(help);
            l.unlock[0].text->SetText(StringTable::GetString("ITEM_UNLOCK"));
            l.unlock[1].root->SetVisibility(shop ? !help : false);
            l.unlock[1].icon->SetTexture(IconManager::GetIcon("Icon_bottom_bar_shop", false), true);
            l.unlock[1].text->SetText(StringTable::GetString("GO_TO_SHOP"));
            l.unlock[2].root->SetVisibility(shop ? help : false);
            l.unlock[2].icon->SetTexture(IconManager::GetIcon("Icon_bottom_bar_shop", false), true);
            l.unlock[2].text->SetText(StringTable::GetString("GO_TO_SHOP"));
        }
        l.scrollItems.UpdateScroll();
    }
    if (g.ranks.empty()) return;
    Professions::Profession* first = Professions::GetProfession(g.ranks[0]->profession);
    g.unlocked.root->SetVisibility(g_tab != 1);
    g.pvp.root->SetVisibility(g_tab == 1);
    const unsigned level = Professions::GetPlayerProfessionLevel(first->id);
    g.unlocked.list.clear();
    for (unsigned i = 0; Items::ItemInfo* it = Items::EnumItemInfo(i); ++i) {
        if (it->reputationId != first->id || it->reputationLevel > level || it->isCraft || !it->GetIcon() ||
            (!it->IsOnSale() && !Collections::GetCollectionWithResult(it->id)))
            continue;
        g.unlocked.list.push_back(it);
    }
    g.unlocked.scroller.itemCount = (unsigned)g.unlocked.list.size();
    if (g.unlocked.list.empty() && s.padStart != 0 && g_tab != 1) {
        s.padStart = 0;
        s.HandleMove(-s.offset);
    }
    g.unlocked.text->SetText(StringTable::GetString("LEVEL_INFO_DONE_ITEMS"));
    for (unsigned i = 0; i != 9; ++i) {
        ItemInfo& slot = g.unlocked.items[i];
        slot.root->SetVisibility(i + g.unlocked.scroller.firstIndex < g.unlocked.list.size());
        if (!slot.root->visibleSelf) continue;
        Items::ItemInfo* it = g.unlocked.list[i + g.unlocked.scroller.firstIndex];
        for (unsigned j = 0; j != 5; ++j) slot.itemClass[j]->SetVisibility(it->itemLevel == j);
        slot.goldOwned->SetVisibility(false);
        slot.redX->SetVisibility(false);
        slot.lock->SetVisibility(false);
        slot.recommended->SetVisibility(false);
        slot.icon->SetTexture(it->GetIcon(), true);
        slot.item = (int)it->id;
    }
    g.unlocked.scroller.UpdateScroll();
    PvPStatisticsHolder& pvp = g.pvp;
    pvp.unlocked->SetText(StringTable::GetString("PVP_YOUR_PATH"));
    pvp.icon[0]->SetTexture(IconManager::GetIcon("rep_pvp_valor_60", false), true);
    pvp.stat[0]->SetText(StringTable::GetString("PVP_RANK"));
    pvp.num[0]->SetText(ToWideString((int)Professions::GetPlayerProfessionLevel(7)));
    pvp.icon[1]->SetTexture(IconManager::GetIcon("pvp_won_60", false), true);
    pvp.stat[1]->SetText(StringTable::GetString("PVP_VICTORIES"));
    pvp.num[1]->SetText(ToWideString((int)PvP::GetVictoryCount()));
    pvp.icon[2]->SetTexture(IconManager::GetIcon("pvp_lost_60", false), true);
    pvp.stat[2]->SetText(StringTable::GetString("PVP_DEFEATS"));
    pvp.num[2]->SetText(ToWideString((int)PvP::GetDefeatCount()));
}

// @0x2ed6d0: the tabs (the active one raised, a reputation's with its icon), the list the column
// scrolls over (the levels, or the reputation's ranks), then the tab's holders.
void UpdateContents() {
    for (unsigned i = 0; i != 5; ++i) {
        g_tabs[i].SetMode(g_tab == i, false);
        Professions::Profession* p = i != 0 ? Professions::GetProfessionForTab(i, Professions::kReputation) : nullptr;
        if (p) g_tabs[i].icon->SetTexture(p->icon, true, (unsigned)(g_tabs[i].icon->origW + 15), 0, false, 0);
        g_tabs[i].root->SetVisibility(i == 0 || p);
    }
    GeneralHolder& g = g_general;
    if (g_tab == 0) {
        g.levels.clear();
        for (unsigned n = 1; n <= (unsigned)GameState::maxLevel; ++n) g.levels.push_back(n);
        g.scroller.itemCount = (unsigned)g.levels.size();
    } else {
        g.ranks.clear();
        for (unsigned i = 0; Professions::Profession* p = Professions::EnumProfession(i); ++i)
            if (p->type == Professions::kReputation && p->tab == g_tab)
                for (Professions::ProfessionLevel* pl : p->levels) g.ranks.push_back(pl);
        g.scroller.itemCount = (unsigned)g.ranks.size();
    }
    if (g_tab == 0)
        UpdateLevels();
    else
        UpdateReputation();
    g.scroller.UpdateScroll();
    g_root->UpdatePosition();
}

}  // namespace

// @0x2f1224: the tab's column starts at its top; the player's tab (0) at the player's level, a
// profession's at its level (first: the strip of unlocked items stays in view when the level is 0).
void OnTab(unsigned tab, bool first) {
    g_tab = tab;
    Shared::ContentScroller& s = g_general.scroller;
    s.firstIndex = 0;
    s.HandleMove(-s.offset);
    if (tab == 0) {
        s.padStart = 0;
        const unsigned level = (unsigned)GameState::GetLevel() - 1;
        s.firstIndex = level;
        const unsigned last = (unsigned)GameState::maxLevel - 2;
        if (level > last) {
            s.firstIndex = last;
            s.HandleMove(-(int)(g_scale * 100.f));   // DAT_002f13bc
        }
        UpdateContents();
        return;
    }
    s.padStart = g_general.unlocked.root->h;
    bool toStart;   // the column's top rests under the strip
    if (Professions::Profession* p = Professions::GetProfessionForTab(tab, Professions::kReputation)) {
        const unsigned level = Professions::GetPlayerProfessionLevel(p->id);
        s.firstIndex = level;
        if (level == 0) {
            toStart = true;
        } else if (!first) {
            s.firstIndex = level - 1;
            toStart = s.firstIndex == 0;
        } else {
            toStart = false;
        }
    } else {
        toStart = s.firstIndex == 0;
    }
    s.HandleMove(toStart ? (int)g_general.unlocked.root->h : 0);
    for (LevelHolder& l : g_general.level) {
        l.scrollItems.firstIndex = 0;
        l.scrollIdle.firstIndex = 0;
        l.scrollItems.HandleMove(-l.scrollItems.offset);
        l.scrollIdle.HandleMove(-l.scrollIdle.offset);
    }
    UpdateContents();
}

namespace {

// @0x2ecbd0
void Init() {
    g_tablet = GUI::IsTabletVersion();
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Window_lvl.xml", "Window_lvl.png", -1.f, 0, 0, 0, 0, false,
                             1.f);
    g_wnd->RegisterTopWindow(g_root);
    g_scale = g_root->scale;
    g_root->SetVisibility(false);
    g_tween = GUI::CreateTweenEffect(g_root, nullptr, nullptr, OnTweenOut);
    g_close = GUI::GetWindowTyped<GUI::Button>(g_root, "x_button.clickArea");
    g_close->SetOnClick(Hide);
    g_close->SetActionSound("ui_close", 1);
    g_title = GUI::GetWindowTyped<Textfield>(g_root, "text_title");
    g_explained = GUI::GetWindowTyped<Textfield>(g_root, "profession_explained");
    char name[0x40];
    for (unsigned i = 0; i != 5; ++i) {
        std::snprintf(name, sizeof name, "tab_%02d", i + 1);
        g_tabs[i].LoadFrom(g_root, name, true);
        g_tabs[i].root->SetOnClick([i] { OnTab(i, true); });
        g_tabs[i].root->SetActionSound("ui_tab", 1);
    }
    g_general.LoadFrom(g_root, "rep_level_scr");
    Render::Texture* scroll = Resources::GetDirectImage("images/scroll.png");
    g_general.unlocked.scroller.scrollbar->SetTexture(scroll);
    for (unsigned i = 0; i != 9; ++i) g_general.unlocked.items[i].root->SetOnClick([i] { OnUnlockedItem(i); });
    for (unsigned k = 0; k != 3; ++k) {
        LevelHolder& l = g_general.level[k];
        l.scrollItems.scrollbar->SetTexture(scroll);
        for (unsigned i = 0; i != 7; ++i) l.items[i].root->SetOnClick([k, i] { OnItem(k, i); });
        for (unsigned i = 0; i != 5; ++i) l.idle[i]->SetOnClick([k, i] { OnBuilding(k, i); });
        l.unlock[0].root->SetOnClick(OnHelp);
        l.unlock[1].root->SetOnClick([k] { OnShop(k); });
        l.unlock[2].root->SetOnClick([k] { OnShop(k); });
    }
    g_tabs[0].icon->SetTexture(IconManager::GetIcon("gold_crown", false), true);
    g_wnd->hideOnOuterClick = true;
    g_wnd->fn.update = Update;
    g_wnd->fn.zRange = ZRange;
    g_wnd->fn.back = OnBack;
    g_wnd->fn.show = Show;
    g_hover = new ItemHoverWindow();
}

}  // namespace

// Static FunctionalWindow("LevelInfoWindow", Init, Deinit, Click, SetZ, Hide, Move).
WindowManager::FunctionalWindow* Queue() {
    if (!g_wnd) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.deinit = Deinit;
        f.click = Click;
        f.setZ = SetZ;
        f.move = Move;
        f.hide = Hide;
        g_wnd = new WindowManager::FunctionalWindow("LevelInfoWindow", std::move(f));
    }
    return g_wnd;
}

// @0x2ea470
bool IsVisible() { return g_wnd->shown; }

// @0x2f1204: the level (1-based) the column starts at.
void OnLevel(unsigned level) {
    g_general.scroller.firstIndex = level != 0 ? level - 1 : 0;
    UpdateContents();
}

// @0x2eb348: the help for an arena event (1: the second one).
void HelpWithArena(unsigned event) {
    g_tab = event == 1 ? 3 : 2;
    OnActualHelp();
}

// @0x2eb368: the help for PvP; open: the PvP window rather than its help (CastleTopWindow).
void HelpWithPvP(bool open) {
    g_pvpOpen = open;
    g_tab = 1;
    OnActualHelp();
}

// @0x2eb070
void Hide() {
    if (g_root->visibleSelf) g_tween->AnimateOut();
}

// @0x2f13d8
void Show() {
    if (!GameState::IsPlayerCity()) return;
    if (GameState::IsTutorial()) return;
    g_wnd->shown = true;
    g_wnd->MoveWindowOnTop(true);
    if (!g_root->visibleSelf) {
        g_tween->AnimateIn();
        WindowManager::WindowShow(false);
    }
    g_root->SetVisibility(true);
    OnTab(0, true);
    if (GameState::GetCurrentLocation() == 2) GameState::RaiseGamePauseState();
}

}  // namespace LevelInfoWindow
