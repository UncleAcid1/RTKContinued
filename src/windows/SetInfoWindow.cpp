// SetInfoWindow: the item set window ("Window_set_info"). Seven tabs, each a set (and the set it
// replaced); three flags, one per bonus, each with three item slots. A slot opens the character
// window on the owned piece, or shows where to get it. Port of SetInfoWindow (libkingdom.so 5.11,
// @0x33b0b8..0x33ddcc; statics 0x62cc34..0x62d018 and 0x60f384..0x60f38c).
//
// UNVERIFIED (milestone 5, 5m-2): UpdateContents (@0x33b6a0), which fills the tabs, the flags and
// the slot lists below, is not ported yet.
#include <cstdio>

#include "game/BuildingHovers.h"
#include "game/Collections.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/Locations.h"
#include "game/MetaData.h"
#include "game/Sets.h"
#include "game/Setting.h"
#include "game/StringTable.h"
#include "game/Tasks.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "windows/Windows.h"

namespace SetInfoWindow {

namespace {

using GUI::Textfield;
using GUI::Window;

// @0x33d72c (0x18 bytes)
struct SetTabInfo {
    Window* root = nullptr;        // +0x00
    Window* active = nullptr;      // +0x04 tab_active
    Window* inactive = nullptr;    // +0x08 tab_inactive
    Window* icon = nullptr;        // +0x0c icon_place_holder
    uint32_t set = 0;              // +0x10 the tab's set (the next active one in sort order)
    uint32_t replaced = 0;         // +0x14 the inactive set before it (the one it replaced)

    void LoadFrom(Window* w, const char* name) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", name);
        active = GUI::GetWindowTypedF<Window>(w, "%s.tab_active", name);
        inactive = GUI::GetWindowTypedF<Window>(w, "%s.tab_inactive", name);
        icon = GUI::GetWindowTypedF<Window>(w, "%s.icon_place_holder", name);
        icon->takesZ = true;
    }
};

// @0x33d40c (0x18 bytes): the bonus' slot names.
struct SetGroupNameHolder {
    Window* root = nullptr;        // +0x00
    Textfield* text[5] = {};       // +0x04 text_item_class_01..05

    void LoadFrom(Window* w, const char* name) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", name);
        for (int i = 0; i != 5; ++i) text[i] = GUI::GetWindowTypedF<Textfield>(w, "%s.text_item_class_%02d", name, i + 1);
    }
};

// @0x33d470 (0x30 bytes): one item slot of a flag.
struct SetItemHolder {
    Window* root = nullptr;        // +0x00
    Window* locked = nullptr;      // +0x04 item_class_locked
    Window* itemClass[5] = {};     // +0x08 item_class_01..05
    Window* icon = nullptr;        // +0x1c item_icon_holder_item
    Window* owned = nullptr;       // +0x20 icon_owned
    Window* goldOwned = nullptr;   // +0x24 icon_gold_owned
    Window* find = nullptr;        // +0x28 icon_find
    Window* allItems = nullptr;    // +0x2c icon_tab_inv_all_items

    void LoadFrom(Window* w, const char* name) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", name);
        locked = GUI::GetWindowTypedF<Window>(
            w, "%s.inventory_slot_holder_01.inventory_item_icon.inventory_item_icon_holder.item_class_locked", name);
        for (int i = 0; i != 5; ++i)
            itemClass[i] = GUI::GetWindowTypedF<Window>(
                w, "%s.inventory_slot_holder_01.inventory_item_icon.inventory_item_icon_holder.item_class_%02d", name,
                i + 1);
        icon = GUI::GetWindowTypedF<Window>(
            w, "%s.inventory_slot_holder_01.inventory_item_icon.inventory_item_icon_holder.item_icon_holder_item", name);
        icon->takesZ = true;
        owned = GUI::GetWindowTypedF<Window>(w, "%s.icon_owned", name);
        goldOwned = GUI::GetWindowTypedF<Window>(
            w, "%s.inventory_slot_holder_01.inventory_item_icon.inventory_item_icon_holder.icon_gold_owned", name);
        find = GUI::GetWindowTypedF<Window>(w, "%s.icon_find", name);
        allItems = GUI::GetWindowTypedF<Window>(w, "%s.icon_tab_inv_all_items", name);
    }
};

// @0x33d344 (0x20 bytes): the bonus' stats.
struct SetGroupStatsHolder {
    Window* root = nullptr;        // +0x00
    Window* icon[3] = {};          // +0x04 icon_stats_01..03
    Textfield* text[3] = {};       // +0x10 text_param_01..03_white
    unsigned shown = 0;            // +0x1c

    void LoadFrom(Window* w, const char* name) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", name);
        for (int i = 0; i != 3; ++i) {
            icon[i] = GUI::GetWindowTypedF<Window>(w, "%s.icon_stats_%02d", name, i + 1);
            icon[i]->takesZ = true;
        }
        for (int i = 0; i != 3; ++i) text[i] = GUI::GetWindowTypedF<Textfield>(w, "%s.text_param_%02d_white", name, i + 1);
        shown = 0;
    }
};

// @0x33d584 (0xe4 bytes): one flag, a bonus of the set.
struct SetFlagHolder {
    Window* root = nullptr;        // +0x00
    Window* bgUnequipped = nullptr;   // +0x04 sets_flag_bg_unequiped
    Window* bg = nullptr;          // +0x08 sets_flag_bg
    SetGroupNameHolder names;      // +0x0c set_info_set_part_01
    SetItemHolder items[3];        // +0x24 item_sets_holder_x3.set_info_item_01..03
    Textfield* bonus = nullptr;    // +0xb4 text_bonus_01
    Window* goldBorder = nullptr;  // +0xb8 char_bg_gold_border
    Window* parameters = nullptr;  // +0xbc info_parameters
    SetGroupStatsHolder stats;     // +0xc0 set_bonus_stat_01
    unsigned state = 0;            // +0xe0

    void LoadFrom(Window* w, const char* name) {
        char sub[0x40];
        root = GUI::GetWindowTypedF<Window>(w, "%s", name);
        bgUnequipped = GUI::GetWindowTypedF<Window>(w, "%s.sets_flag_bg_unequiped", name);
        bg = GUI::GetWindowTypedF<Window>(w, "%s.sets_flag_bg", name);
        std::snprintf(sub, sizeof sub, "%s.set_info_set_part_01", name);
        names.LoadFrom(w, sub);
        for (int i = 0; i != 3; ++i) {
            std::snprintf(sub, sizeof sub, "%s.item_sets_holder_x3.set_info_item_%02d", name, i + 1);
            items[i].LoadFrom(w, sub);
        }
        bonus = GUI::GetWindowTypedF<Textfield>(w, "%s.text_bonus_01", name);
        goldBorder = GUI::GetWindowTypedF<Window>(w, "%s.char_bg_gold_border", name);
        parameters = GUI::GetWindowTypedF<Window>(w, "%s.info_parameters", name);
        std::snprintf(sub, sizeof sub, "%s.set_bonus_stat_01", name);
        stats.LoadFrom(w, sub);
        state = 0;
    }
};

int g_helpItem = -1;                   // 0x60f384 ShowNextItemHelp's slot: OnTweenIn points at it
bool g_tablet = false;                 // 0x60f388
float g_scale = 1.f;                   // 0x60f38c the layout's scale
SetTabInfo g_tabs[7];                  // 0x62cc34 tab_01..07
SetFlagHolder g_flags[3];              // 0x62ccdc set_info_flag_holder_01..03
MetaData* g_slots[9] = {};             // 0x62cf88 per item slot (flag * 3 + i): its item list (UpdateContents)
Items::ItemInfo* g_slotItems[9] = {};  // 0x62cfd0 per item slot: the item shown (UpdateContents)
Window* g_root = nullptr;              // 0x62cff4
Sets::Set* g_set = nullptr;            // 0x62cff8 the shown set
Sets::Set* g_replaced = nullptr;       // 0x62cffc the set it replaced (its tab's other set)
unsigned g_tab = 0;                    // 0x62d000
Textfield* g_title = nullptr;          // 0x62d004 text_title
Textfield* g_levelDesc = nullptr;      // 0x62d008 text_level_desc
GUI::TweenEffect* g_tween = nullptr;   // 0x62d00c
Window* g_help = nullptr;              // 0x62d010 button_brief_help
GUI::Button* g_close = nullptr;        // 0x62d014 x_button.clickArea
WindowManager::FunctionalWindow* g_wnd = nullptr;   // 0x62d018 "SetInfoWindow"

// @0x33b0b8
float ZRange() { return g_wnd->shown ? 0.027f : 0.001f; }

// @0x33b228
void OnTweenOut() {
    WindowManager::WindowHide(false);
    g_wnd->shown = false;
    g_wnd->MoveWindowDown(true);
}

// @0x33b258: the arrow at ShowNextItemHelp's slot; tapping it taps the slot.
void OnTweenIn() {
    const unsigned i = (unsigned)g_helpItem;
    if (g_helpItem == -1) return;
    if (i < 9) {
        if (Window* w = g_flags[i / 3].items[i % 3].root) {
            BuildingHovers::SetArrowVisibleWindowLimit(3);
            BuildingHovers::ArrowAt((float)(g_root->x + w->x + w->w / 2), (float)(g_root->y + w->y), false, false,
                                    false, false, true, false, false);
            BuildingHovers::SetArrowClickCallback(new GUI::Callback([i] { OnItem(i); }));
        }
    }
    g_helpItem = -1;
}

// @0x33cef0
void OnInfo() { PopupWindow::Show(StringTable::GetString("window_sets_explain"), PopupWindow::Hide, nullptr, nullptr); }

// @0x33cfdc
bool OnBack() {
    if (!g_wnd->shown) return false;
    Hide();
    return true;
}

// @0x33d1f4
void SetZ(float z) {
    if (!g_wnd->shown) return;
    g_root->SetZ(z);
    g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::GetVerticalCenter(g_root->h));
}

// @0x33d27c
bool Click(int x, int y, bool pressed) {
    bool visible = false;
    if (g_root->visibleSelf) {
        if (BuildingHovers::ClickOnArrow(x, y, pressed)) return true;
        if (!pressed) BuildingHovers::HideArrow();
        visible = g_root->visibleSelf;
    }
    g_root->Click(x, y, pressed, false);
    return visible;
}

// @0x33d304
void Deinit() {
    GUI::RemoveTweenEffect(g_tween);
    g_tween = nullptr;
    delete g_root;
}

// @0x33d7b4
void Init() {
    g_tablet = GUI::IsTabletVersion();
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Window_set_info.xml", "Window_set_info.png", -1.f, 0, 0,
                             0, 0, false, 1.f);
    g_wnd->RegisterTopWindow(g_root);
    g_scale = g_root->scale;
    g_root->SetVisibility(false);
    g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::GetVerticalCenter(g_root->h));
    char name[0x40];
    // Each tab takes the sets in sort order up to the next active one: that one is the tab's,
    // the last inactive one before it the set it replaced.
    unsigned next = 0;
    for (unsigned i = 0; i != 7; ++i) {
        SetTabInfo& t = g_tabs[i];
        std::snprintf(name, sizeof name, "tab_%02d", i + 1);
        t.LoadFrom(g_root, name);
        t.root->SetOnClick([i] { OnTab(i); });
        t.root->SetActionSound("ui_tab", 1);
        t.set = 0;
        while (Sets::Set* s = Sets::EnumSet(next++)) {
            if (s->active) {
                t.set = s->id;
                break;
            }
            t.replaced = s->id;
        }
    }
    g_help = GUI::GetWindowTyped<Window>(g_root, "button_brief_help");
    g_help->SetOnClick(OnInfo);
    g_title = GUI::GetWindowTyped<Textfield>(g_root, "text_title");
    g_levelDesc = GUI::GetWindowTyped<Textfield>(g_root, "text_level_desc");
    for (unsigned k = 0; k != 3; ++k) {
        std::snprintf(name, sizeof name, "set_info_flag_holder_%02d", k + 1);
        g_flags[k].LoadFrom(g_root, name);
        for (unsigned j = 0; j != 3; ++j) {
            const unsigned slot = k * 3 + j;
            g_flags[k].items[j].root->SetOnClick([slot] { OnItem(slot); });
        }
    }
    g_tween = GUI::CreateTweenEffect(g_root, nullptr, OnTweenIn, OnTweenOut);
    g_close = GUI::GetWindowTyped<GUI::Button>(g_root, "x_button.clickArea");
    g_close->SetOnClick(Hide);
    g_close->SetActionSound("ui_close", 1);
    g_wnd->hideOnOuterClick = true;
    g_wnd->fn.zRange = ZRange;
    g_wnd->fn.back = OnBack;
    g_wnd->fn.show = Show;
}

}  // namespace

// Static FunctionalWindow("SetInfoWindow", Init, Deinit, Click, SetZ, Hide).
WindowManager::FunctionalWindow* Queue() {
    if (!g_wnd) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.deinit = Deinit;
        f.click = Click;
        f.setZ = SetZ;
        f.hide = Hide;
        g_wnd = new WindowManager::FunctionalWindow("SetInfoWindow", std::move(f));
    }
    return g_wnd;
}

// @0x33b3d4: the player's item `id`, or one it improves into (Collections' chain): worn (bindings
// 0..9) first, else the first in the bag.
GameState::PlayerItem* GetPlayerItemOrBetter(uint32_t id) {
    for (unsigned i = 0; i != 10; ++i) {
        GameState::PlayerItem* it = GameState::GetItemAt(i);
        if (!it) continue;
        if (it->id == id) return it;
        for (uint32_t k = id; Collections::Collection* c = Collections::GetCollectionWithSource(k); k = c->to)
            if (it->id == c->to) return it;
    }
    for (uint32_t k = id;;) {
        if (GameState::GetItemAmount(k, false) != 0) return GameState::GetFirstItemByID(k, false);
        Collections::Collection* c = Collections::GetCollectionWithSource(k);
        if (!c) return nullptr;
        k = c->to;
    }
}

// @0x33b484: the set's tab. A set no tab shows (one a newer set replaced) is shown with the tab
// of the set of its sort id.
void SelectSet(uint32_t id) {
    g_set = Sets::GetSet(id);
    g_replaced = nullptr;
    unsigned tab = g_tab;
    bool found = false;
    for (unsigned i = 0; i != 7; ++i)
        if (g_tabs[i].set == id) {
            found = true;
            tab = i;
        }
    g_tab = tab;
    if (found) return;
    for (unsigned i = 0; i != 7; ++i) {
        Sets::Set* s = Sets::GetSet(g_tabs[i].set);
        if (!s || s->sortId != g_set->sortId) continue;
        g_replaced = g_set;
        g_tab = i;
        g_set = Sets::GetSet(g_tabs[i].set);
    }
}

// @0x33b56c: the next slot of the shown set without an owned piece (OnTweenIn points at it).
void ShowNextItemHelp() {
    if (!g_set) return;
    for (unsigned i = 0; i != 9; ++i) {
        MetaData* list = g_slots[i];
        if (!list) continue;
        GameState::PlayerItem* owned = nullptr;
        for (unsigned k = 0; k < list->GetChildrenCount() && !owned; ++k)
            owned = GetPlayerItemOrBetter((uint32_t)list->GetChild(k)->GetInt());
        if (!owned) {
            g_helpItem = (int)i;
            return;
        }
    }
}

// @0x33b608: the arrow goes to the slot whose list holds item.
void SelectItem(uint32_t item) {
    if (!g_set) return;
    for (unsigned i = 0; i != 9; ++i) {
        MetaData* list = g_slots[i];
        if (!list) continue;
        for (unsigned k = 0; k < list->GetChildrenCount(); ++k)
            if ((uint32_t)list->GetChild(k)->GetInt() == item) {
                g_helpItem = (int)i;
                break;
            }
    }
}

// @0x33b6a0. UNVERIFIED (milestone 5, 5m-2): not ported yet; the window shows its layout as loaded.
void UpdateContents() {}

// @0x33ceb4
void OnTab(unsigned tab) {
    g_tab = tab;
    g_set = Sets::GetSet(g_tabs[tab].set);
    g_replaced = Sets::GetSet(g_tabs[tab].replaced);
    UpdateContents();
}

// @0x33cf38
void Show() {
    g_wnd->shown = true;
    g_wnd->MoveWindowOnTop(true);
    if (!g_root->visibleSelf) {
        WindowManager::WindowShow(false);
        g_tween->AnimateIn();
    }
    g_root->SetVisibility(true);
    UpdateContents();
    g_root->PreloadTextures();   // GUI::PreloadWindow
}

// @0x33cfb8
void Hide() {
    if (g_root->visibleSelf) g_tween->AnimateOut();
}

// @0x33d004: an owned piece opens the character window on it (the improvements tab when it is
// part of a chain); a missing one shows where to get it, once the shop is open: the mission that
// drops it, the item shop, the chests (while the quest 0x6b4 asks for set 8), else a popup.
void OnItem(unsigned slot) {
    if (!g_set) return;
    Items::ItemInfo* info = g_slotItems[slot];
    if (!info) return;
    if (GameState::PlayerItem* it = GetPlayerItemOrBetter(info->id)) {
        Hide();
        CharacterInfoWindow::Show();
        CharacterInfoWindow::ShowInventoryItem(it->id, Collections::GetCollectionStart(it->id) ? 2 : 0, nullptr);
        return;
    }
    if (GameState::secondTutorial != 0x100) return;
    if (!GameState::TaskCompleted((unsigned)Setting("shop_unlock_task").GetInt())) return;
    if (const uint32_t map = GlobalMapWindow::GetMissionMapForItem(info->id, 0)) {
        Hide();
        CharacterInfoWindow::Hide();
        GlobalMapWindow::Show();
        GlobalMapWindow::CenterOnMap(map);
        if (Locations::LocationInfo* loc = Locations::GetLocationWithMap(map)) {
            RaidInfoWindow::SetParameters(loc, map, false);
            RaidInfoWindow::Show();
            RaidInfoWindow::SetAnimationOnce();
            RaidInfoWindow::ShowItem(info->id);
        }
        return;
    }
    // UNVERIFIED (milestone 5, 5q): ItemShopWindow::ItemCanBeBought(id, 0) -> Hide,
    // ItemShopWindow::Show and ShowShopItem(id, 0, true, -1). No item is for sale until then.
    if (g_set->id == 8 && Tasks::IsTaskActive(0x6b4, true)) {
        // UNVERIFIED (milestone 5, 5p): Hide, CharacterInfoWindow::Hide, ChestsWindow::Show and
        // SetChest(1).
        return;
    }
    PopupWindow::Show(StringTable::GetString("HELP_FIND_SET"), PopupWindow::Hide, nullptr, nullptr);
    std::fprintf(stderr, "ERROR: SetInfoWindow::OnItem() Cannot find a place with item %d\n", info->id);   // ErrorReporter::Printf
}

}  // namespace SetInfoWindow
