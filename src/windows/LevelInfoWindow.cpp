// LevelInfoWindow: the levels and professions window ("Window_lvl"). Five tabs: tab 0 the player's
// levels, the others a profession / reputation each, a strip of rank holders (progress bar, rank,
// the items it unlocks and the buildings of the level) under a strip of the items already unlocked.
// Port of LevelInfoWindow (libkingdom.so 5.11, @0x2ea3d8..0x2f13d8 and the LoadFroms; statics
// 0x624014..0x6253f8).
//
// 5n-3: the window opens and closes. UpdateContents (5n-4) and the taps and helps (5n-5) are empty
// stubs for now.
#include <cstdio>

#include "engine/IconManager.h"
#include "engine/Resources.h"
#include "game/BuildingHovers.h"
#include "game/GameState.h"
#include "game/Professions.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/ContentScroller.h"
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
void OnTab(unsigned tab, bool first);

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
unsigned g_tab = 0;                             // 0x62... the shown tab (OnTab)
Window* g_close = nullptr;                      // 0x6253ec x_button.clickArea
Textfield* g_title = nullptr;                   // 0x6253f0 text_title
Textfield* g_explained = nullptr;               // 0x6253f4 profession_explained
ItemHoverWindow* g_hover = nullptr;             // 0x6253e0
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

// 0x2eb14c (5n-5): closes the other windows and takes the player to what OnHelp asked about.
// UNVERIFIED (milestone 5n-5): empty.
void OnActualHelp() {}

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

// 0x2ea6ac (5n-5): an item of the unlocked strip. UNVERIFIED (milestone 5n-5): empty.
void OnUnlockedItem(unsigned) {}

// 0x2ea7f4 (5n-5): an item a rank unlocks. UNVERIFIED (milestone 5n-5): empty.
void OnItem(unsigned, unsigned) {}

// 0x2ea950 (5n-5): a building a rank opens. UNVERIFIED (milestone 5n-5): empty.
void OnBuilding(unsigned, unsigned) {}

// 0x2ead60 (5n-5). UNVERIFIED (milestone 5n-5): empty.
void OnLevelHolderDown() {}

// 0x2eaebc (5n-5). UNVERIFIED (milestone 5n-5): empty.
void OnLevelHolderUp() {}

// 0x2eb0f0 (5n-5): the rank's shop button. UNVERIFIED (milestone 5n-5): empty.
void OnShop(unsigned) {}

// 0x2eb384 (5n-5): the help button. UNVERIFIED (milestone 5n-5): empty.
void OnHelp() {}

// 0x2ed6d0 (5n-4): the tabs, the bars, the rank holders. UNVERIFIED (milestone 5n-4): empty.
void UpdateContents() {}

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
