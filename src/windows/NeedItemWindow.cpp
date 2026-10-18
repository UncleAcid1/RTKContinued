// NeedItemWindow: "Item needed" (Popup_yes_no.xml): an item is missing; Craft points at the
// building that makes it (or its shop cell), Buy opens the item shop. Its other modes ask about a
// quest's items to hand over (SetItemsToBeRemoved), an item to find (SetFindParameters) or what a
// map object needs (SetSearchParameters), with OK / Cancel callbacks.
// Port of NeedItemWindow (libkingdom.so 5.11), 0x2f4660..0x2f6b80. Statics at 0x6257e0.
//
// The item shop is milestone 5 (ItemShopWindow::ItemCanBeBought is false until then), so Show for
// an item goes straight to OnProduce, as the original does for an item the shop does not sell.
#include <functional>

#include "engine/IconManager.h"
#include "engine/Render.h"
#include "game/Building.h"
#include "game/BuildingHovers.h"
#include "game/Contracts.h"
#include "game/GameData.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/Map.h"
#include "game/StringTable.h"
#include "game/Tasks.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/HUD.h"
#include "hud/Shared.h"
#include "windows/Windows.h"

namespace NeedItemWindow {
namespace {

using GUI::Textfield;
using GUI::Window;

WindowManager::FunctionalWindow* g_queue = nullptr;   // NeedItemWindow::wnd
Window* g_root = nullptr;                 // 0x6257e0 Popup_yes_no.xml
unsigned g_lastHelpItem = 0;              // 0x6257e4
bool g_lastHelpEnabled = false;           // 0x6257e8
GUI::Callback* g_onOk = nullptr;          // 0x6257ec (owned)
GUI::Callback* g_onCancel = nullptr;      // 0x6257f0 (owned)
uint32_t g_item = 0;                      // 0x6257f4 SetParameters
uint32_t g_findItem = 0;                  // 0x6257f8 SetFindParameters
uint32_t g_task = 0;                      // 0x6257fc SetItemsToBeRemoved
Textfield* g_title = nullptr;             // 0x625800 text_header_title
Window* g_icon = nullptr;                 // 0x625804 icon_item
Textfield* g_amount = nullptr;            // 0x625808 shop_required
Textfield* g_text = nullptr;              // 0x62580c text_header
Shared::ButtonTripleInfo g_left;          // 0x625810 button_1 (Craft / OK / Find)
Shared::ButtonTripleInfo g_right;         // 0x625838 button_2 (Buy / Cancel)
Map::Decor* g_decor = nullptr;            // 0x625860 the decoration that asked (SetParameters)
GUI::TweenEffect* g_tween = nullptr;      // 0x625864
Window* g_close = nullptr;                // 0x625868 x_button
Window* g_titleBar = nullptr;             // 0x62586c header_title_header_lol
Window* g_mentor = nullptr;               // 0x625870 char_mentor_hint
Window* g_male = nullptr;                 // 0x625874 char_male_hint
Window* g_female = nullptr;               // 0x625878 char_female_hint
Window* g_goblin = nullptr;               // 0x62587c char_goblin_hint
Window* g_empty = nullptr;                // 0x625880 char_empty_hint
Shared::ButtonTripleInfo g_button3;       // 0x625884 button_3 (hidden)
int g_rootScale = 0;                      // 0x60f2ec (the root's scale field, unused)

bool ItemCanBeBought(uint32_t) { return false; }   // ItemShopWindow::ItemCanBeBought: UNVERIFIED (5)
bool PackCanBeBought(uint32_t) { return false; }   // ItemShopWindow::PackCanBeBought: UNVERIFIED (5)

void DropCallbacks() {
    delete g_onOk;
    g_onOk = nullptr;
    delete g_onCancel;
    g_onCancel = nullptr;
}

// @0x2f4660
bool Click(int x, int y, bool pressed) {
    bool visible = g_root->visibleSelf;
    if (g_root->Click(x, y, pressed, false)) return true;
    return visible;
}

// @0x2f480c
void OnTweenOut() {
    GameState::DropGamePauseState();
    WindowManager::WindowHide(false);
    g_queue->shown = false;
    g_queue->MoveWindowDown(true);
}

// @0x2f66e4
void SetZ(float z) {
    g_root->SetZ(z);
    g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::GetVerticalCenter(g_root->h));
}

// @0x2f5ed8
bool OnBack() {
    if (!g_queue->shown) return false;
    Hide();
    return true;
}

// @0x2f6560
void OnLeft() { OnProduce(0); }

// @0x2f6110 (a jump to OnBuy)
void OnRight() { OnBuy(); }

}  // namespace

// The static FunctionalWindow "NeedItemWindow" (its initialiser FUN_002f474c).
WindowManager::FunctionalWindow* Queue() {
    if (!g_queue) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.deinit = Deinit;
        f.click = Click;
        f.setZ = SetZ;
        f.hide = Hide;
        g_queue = new WindowManager::FunctionalWindow("NeedItemWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_queue->shown; }                 // @0x2f46b4
void SetLastHelpItem(unsigned id) { g_lastHelpItem = id; }   // @0x2f46c8
void EnableLastHelpItem() { g_lastHelpEnabled = true; }     // @0x2f46dc

// @0x2f6800
void Init() {
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Popup_yes_no.xml", "Popup_yes_no.png",
                             GUI::GetHoverScaleFactor(1.f, 1.f), 0, 0, 0, 0, false, 1.f);
    g_queue->RegisterTopWindow(g_root);
    g_rootScale = (int)g_root->scale;
    g_root->SetVisibility(false);
    g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::GetVerticalCenter(g_root->h));
    g_tween = GUI::CreateTweenEffect(g_root, nullptr, nullptr, OnTweenOut);
    g_close = GUI::GetWindowTyped<Window>(g_root, "x_button");
    g_close->SetOnClick(Hide);
    g_close->SetActionSound("ui_close", 1);
    g_titleBar = GUI::GetWindowTyped<Window>(g_root, "header_title_header_lol");
    g_title = GUI::GetWindowTyped<Textfield>(g_root, "text_header_title");
    g_text = GUI::GetWindowTyped<Textfield>(g_root, "text_header");
    g_mentor = GUI::GetWindowTyped<Window>(g_root, "char_mentor_hint");
    g_male = GUI::GetWindowTyped<Window>(g_root, "char_male_hint");
    g_female = GUI::GetWindowTyped<Window>(g_root, "char_female_hint");
    g_goblin = GUI::GetWindowTyped<Window>(g_root, "char_goblin_hint");
    g_empty = GUI::GetWindowTyped<Window>(g_root, "char_empty_hint");
    g_icon = GUI::GetWindowTyped<Window>(g_root, "icon_item");
    g_icon->takesZ = true;
    g_amount = GUI::GetWindowTyped<Textfield>(g_root, "shop_required");
    g_left.LoadFrom(g_root, "button_1");
    g_left.root->SetOnClick(OnLeft);
    g_right.LoadFrom(g_root, "button_2");
    g_right.root->SetOnClick(OnRight);
    g_button3.LoadFrom(g_root, "button_3");
    g_button3.root->SetVisibility(false);
    g_mentor->SetVisibility(false);
    g_male->SetVisibility(false);
    g_female->SetVisibility(false);
    g_goblin->SetVisibility(false);
    g_queue->fn.back = OnBack;
    g_queue->fn.show = Show;
}

// @0x2f6754
void Deinit() {
    GUI::RemoveTweenEffect(g_tween);
    g_tween = nullptr;
    delete g_root;
    g_root = nullptr;
    DropCallbacks();
}

// @0x2f4840: the building (BuildingData id) with an order that makes item; slot: that order's index
// (-1 when none). 0 when no building makes it.
uint32_t GetBuildingThatProducesItem(uint32_t item, unsigned& slot) {
    slot = 0xffffffff;
    for (unsigned i = 0; GameData::BuildingData* b = GameData::EnumBuildings(i); ++i) {
        if (b->buildingClass == 4 || !b->delivery || b->delivery->count == 0) continue;
        for (unsigned k = 0; k < b->delivery->count; ++k) {
            const Items::ItemInfo* made = b->delivery->missions[k].item;
            if (made && made->id == item) {
                slot = k;
                return b->id;
            }
        }
    }
    return 0;
}

// @0x2f48ec: a quest takes items (Tasks: the task's item +0x1ac and count +0x1b0); OK runs cb.
void SetItemsToBeRemoved(unsigned task, GUI::Callback* cb) {
    delete g_onOk;
    g_onOk = cb;
    delete g_onCancel;
    g_onCancel = nullptr;
    g_task = task;
    g_decor = nullptr;
    g_item = 0;
    g_findItem = 0;
    g_title->SetText(StringTable::GetString("NEED_SOMETHING_HEADER"));
    // The item the task takes: its icon, the amount (what the player has, at most the count; the
    // count when he has none) and its name.
    if (Tasks::TaskInfo* t = Tasks::GetTask(task)) {
        const uint32_t item = t->giveItems[0].id;
        if (Items::ItemInfo* info = Items::GetItemInfo(item)) {
            g_icon->SetTexture(info->GetIcon(), true);
            const unsigned count = t->giveItems[0].count;
            unsigned have = (unsigned)GameState::GetItemAmount(item, false);
            if (have == 0 || count <= have) have = count;
            g_amount->SetText(ToWideString((int)have));
            g_text->SetText(SWPrintf(0x100, StringTable::GetString("NEED_ITEM_DESC_2"), {info->title}).c_str());
        }
    }
    g_left.ShowGreen(StringTable::GetString("HUD_OK"), IconManager::GetIcon("gold_confirm"));
    g_right.ShowBlue(StringTable::GetString("CANCEL"), IconManager::GetIcon("gold_cancel"));
}

// @0x2f4cf0: an item to find; Find runs cb.
void SetFindParameters(uint32_t item, GUI::Callback* cb) {
    delete g_onOk;
    g_onOk = cb;
    delete g_onCancel;
    g_onCancel = nullptr;
    g_decor = nullptr;
    g_findItem = item;
    g_item = 0;
    g_task = 0;
    g_title->SetText(StringTable::GetString("NEED_SOMETHING_HEADER"));
    if (Items::ItemInfo* info = Items::GetItemInfo(item)) {
        g_icon->SetTexture(info->GetIcon(), true);
        std::u32string s = SWPrintf(0x100, StringTable::GetString("NEED_ITEM_DESC_3"), {info->title});
        g_text->SetText(s.c_str());
    }
    g_amount->SetText(ToWideString(1));
    g_left.ShowGreen(StringTable::GetString("PERFORM_FIND"), IconManager::GetIcon("gold_find"));
    g_right.ShowBlue(StringTable::GetString("NEED_ITEM_BUY"), IconManager::GetIcon("profession_hauler"));
}

// @0x2f50c8: a map object needs an item (or, without one, a resource: 0xb none) and amount; OK
// runs onOk, Cancel onCancel.
void SetSearchParameters(uint32_t item, int resource, unsigned amount, GUI::Callback* onOk,
                         GUI::Callback* onCancel) {
    delete g_onOk;
    g_onOk = onOk;
    delete g_onCancel;
    g_onCancel = onCancel;
    g_decor = nullptr;
    g_item = 0;
    g_findItem = 0;
    g_title->SetText(StringTable::GetString("NEED_SOMETHING_HEADER"));
    if (Items::ItemInfo* info = Items::GetItemInfo(item)) {
        g_icon->SetTexture(info->GetIcon(), true);
        std::u32string s = SWPrintf(0x100, StringTable::GetString("NEED_ITEM_DESC_2"), {info->title});
        g_text->SetText(s.c_str());
    } else if (resource != 0xb) {
        g_icon->SetTexture(IconManager::GetIcon(GameState::GetResourceMapIconName(resource)), true);
        std::u32string s = SWPrintf(0x100, StringTable::GetString("NEED_ITEM_DESC_2"),
                                    {GameState::GetResourceGameName(resource)});
        g_text->SetText(s.c_str());
    }
    g_amount->SetText(ToWideString((int)amount));
    g_left.ShowGreen(StringTable::GetString("HUD_OK"), IconManager::GetIcon("gold_confirm"));
    g_right.ShowBlue(StringTable::GetString("CANCEL"), IconManager::GetIcon("gold_cancel"));
}

// @0x2f578c: item is missing (decor: the map object that asked, for the item shop's continuation).
void SetParameters(uint32_t item, Map::Decor* decor) {
    DropCallbacks();
    g_decor = decor;
    g_item = item;
    g_findItem = 0;
    g_task = 0;
    g_title->SetText(StringTable::GetString("NEED_ITEM_HEADER"));
    if (Items::ItemInfo* info = Items::GetItemInfo(item)) {
        g_icon->SetTexture(info->GetIcon(), false);
        unsigned slot;
        const GameData::BuildingData* b = GameData::GetBuilding(GetBuildingThatProducesItem(info->id, slot));
        std::u32string s;
        if (!b)
            s = SWPrintf(0x100, StringTable::GetString("NEED_ITEM_DESC"), {info->title, U"%unknown%"});
        else
            s = SWPrintf(0x100, StringTable::GetString("NEED_ITEM_DESC"),
                         {info->title, StringTable::GetString(b->name.c_str())});
        g_text->SetText(s.c_str());
    }
    g_amount->SetText(ToWideString(GameState::GetItemAmount(g_item, false)));
    g_left.ShowGreen(StringTable::GetString("NEED_ITEM_PRODUCE"), IconManager::GetIcon("profession_stone_cutter"));
    g_right.ShowBlue(StringTable::GetString("NEED_ITEM_BUY"), IconManager::GetIcon("profession_hauler"));
}

// @0x2f5e08
void Hide() {
    if (g_root->visibleSelf) g_tween->AnimateOut();
    if (g_onCancel) (*g_onCancel)();   // (vfunc +8, Perform)
    DropCallbacks();
    g_decor = nullptr;
    g_findItem = 0;
    g_task = 0;
}

// @0x2f5f00: Cancel of the callback modes; the item shop on the item (or a pack with it), or on the
// item to find.
void OnBuy() {
    if (g_onCancel || g_task) {
        Hide();
        return;
    }
    Items::ItemInfo* pack = Items::GetItemPackWithItem(g_item);
    if (ItemCanBeBought(g_item) || (pack && PackCanBeBought(pack->packId))) {
        // UNVERIFIED (milestone 5): ItemShopWindow::Show, ShowShopItem(item, 0, true, -1) or
        // ShowShopPack(pack id, true); with a decoration that asked, the shop's continuation after the
        // purchase (SetContinuationAfterTargetIsPurchased: the player's AI vfunc +0x144 on it).
        Hide();
        return;
    }
    if (!ItemCanBeBought(g_findItem)) return;
    // UNVERIFIED (milestone 5): ItemShopWindow::Show, ShowShopItem(find item, 0, true, -1).
    Hide();
}

// @0x2f6114: the tutorials' first help: the callback modes run OK, else Buy.
void ExecuteFirstHelp() {
    if (g_findItem != 0x6c) {
        if (g_onOk) {
            delete g_onCancel;
            g_onCancel = nullptr;
            (*g_onOk)();
            Hide();
            return;
        }
        if (g_item == 0) return;
    }
    OnBuy();
}

// @0x2f619c: Craft: the building that makes the item (its factory window on that order, or an
// arrow at it), else its shop cell. Away from the city the player is sent home first and the help
// runs again there (RunLastHelpItem).
void OnProduce(uint32_t item) {
    if (g_onOk) {
        delete g_onCancel;
        g_onCancel = nullptr;
        GUI::Callback* ok = g_onOk;
        (*ok)();
        Hide();
        return;
    }
    Hide();
    uint32_t id = item ? item : g_item;
    unsigned slot;
    uint32_t producer = GetBuildingThatProducesItem(id, slot);
    if ((!GameState::TaskCompleted(0x3f4) || !GameState::TaskCompleted(0x462) || !GameState::TaskCompleted(0x3f6)) &&
        producer == 0x6a) {
        // While the Smithy's first quests are open, the open one of them is shown instead.
        Tasks::TaskInfo* cur[0x80] = {};
        const unsigned n = Tasks::GetCurrentTasks(cur, 0x80, true, true);
        for (unsigned i = 0; i < n; ++i) {
            const uint32_t t = cur[i]->id;
            if (t == 0x3f4 || t == 0x462 || t == 0x3f6) {
                TaskInfoWindow::SetTask(t, false);
                TaskInfoWindow::Show();
                return;
            }
        }
    }
    if (GameState::GetCurrentMapID() != 0) {
        SetLastHelpItem(item ? item : g_item);
        EnableLastHelpItem();
        Tasks::HelpWithMapSwitchTo(0, true, false, 0);   // home to the city
        return;
    }
    if (!producer) return;
    Map::Building* b = Map::GetBuildingWithID(producer);
    if (!b) {
        if (Map::GetCurrentFarm()) HUDWindow::ExitFarm();
        GlobalMapWindow::Hide();
        ShopWindow::Show();
        ShopWindow::OnSelectItemID(producer);
        ShopWindow::ShowArrowOnItem(producer);
        return;
    }
    GlobalMapWindow::Hide();
    if (slot == 0xffffffff || !b->IsOpened() || b->data->buildingClass == 0xd ||
        (b->HasActiveContract() && b->contractDone && (unsigned)b->GetMissionID() == slot)) {
        Render::CenterOn(b->baseX, (b->maxY + b->minY) * 0.5f, true);
        int sx = (int)b->minX, sy = (int)b->minY;
        Map::WorldCoordinatesToScreen(sx, sy);
        BuildingHovers::SetArrowClickCallback(new GUI::Callback([b, sx, sy] { BuildingHovers::OnBuildingClick(b, sx, sy); }));
        BuildingHovers::ArrowAt(b->baseX, b->minY, false, false, false, false, false, false, false);
    } else {
        BuildingHovers::AllowWindowOverHoverOnce();
        Render::CenterOn(b->baseX, b->minY - 100.f);
        BuildingHovers::OnBuildingClick(b, GUI::ScreenWidth() / 2, GUI::ScreenHeight() / 2);
        BuildingHovers::ShowArrowAtFactoryContract(slot, true);
    }
}

// @0x2f6568: the help asked for away from the city, once back.
void RunLastHelpItem() {
    if (g_lastHelpEnabled && g_lastHelpItem != 0) OnProduce(g_lastHelpItem);
    g_lastHelpItem = 0;
    g_lastHelpEnabled = false;
}

// @0x2f65b4
void Show() {
    if (g_item != 0) {
        Items::ItemInfo* pack = Items::GetItemPackWithItem(g_item);
        if (!ItemCanBeBought(g_item) && !(pack && PackCanBeBought(pack->packId))) {
            OnProduce(g_item);
            return;
        }
        unsigned slot = 0;
        if (g_item != 0 && GetBuildingThatProducesItem(g_item, slot) == 0) {
            // UNVERIFIED (milestone 5): ItemShopWindow::Show, ShowShopItem(item, 0, true, -1).
            return;
        }
    }
    g_queue->shown = true;
    g_queue->MoveWindowOnTop(true);
    if (!g_root->visibleSelf) {
        WindowManager::WindowShow(false);
        g_tween->AnimateIn();
    }
    g_root->SetVisibility(true);
    GameState::RaiseGamePauseState();
}

}  // namespace NeedItemWindow
