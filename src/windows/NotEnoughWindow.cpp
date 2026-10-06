// NotEnoughWindow: what a purchase or upgrade lacks. The requirements (resources, level, people,
// buildings, items) are collected first; Show lists the missing ones, each line with a Find button
// that leads to where it is earned, and "Buy all" pays the shortfall in crystals. With a single
// missing level, gold, crystals or people a popup or the exchange opens instead.
// Port of NotEnoughWindow (libkingdom.so 5.11), 0x2f6c5c..0x2fd9a4. Statics at 0x62591c.
#include "windows/Windows.h"

#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

#include "engine/IconManager.h"
#include "engine/Render.h"
#include "game/Building.h"
#include "game/BuildingHovers.h"
#include "game/GameData.h"
#include "game/GameState.h"
#include "game/Map.h"
#include "game/Setting.h"
#include "game/StringTable.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/HUD.h"

namespace NotEnoughWindow {
namespace {

using GUI::Button;
using GUI::Textfield;
using GUI::Window;

enum LineType {
    kResource = 0, kGold = 1, kCrystals = 2, kLevel = 3, kProfession = 4, kBuildingCount = 5,
    kBuildingLevel = 6, kPopulation = 7, kItem = 8
};

// RequirementLine (100 bytes): one row, "window_uprgade_1_line_%02d" (sic).
struct Line {
    Window* root = nullptr;                // +0x00
    Window* icon = nullptr;                // +0x04 icon_60_on (takes a depth slot)
    Window* sep02 = nullptr;               // +0x08 sepparator_vertical_02
    Window* sep03 = nullptr;               // +0x0c sepparator_vertical_03
    Textfield* name = nullptr;             // +0x10 text_resource_name
    Textfield* quantity = nullptr;         // +0x14 text_quantity_resource
    Textfield* quantityShort = nullptr;    // +0x18 text_quantity_resource_not_enough
    Textfield* level = nullptr;            // +0x1c text_level
    Textfield* levelShort = nullptr;       // +0x20 text_level_not_reached
    Textfield* itemClass[5] = {};          // +0x24 text_item_class_01..05
    Textfield* itemQuantity = nullptr;     // +0x38 text_quantity_item
    Textfield* itemQuantityShort = nullptr;   // +0x3c text_quantity_item_not_enough
    Window* find = nullptr;                // +0x40 button_upgrade_window_find_one (OnFind(i))
    Textfield* findText = nullptr;         // +0x44 its text_upgrade
    Window* complete = nullptr;            // +0x48 icon_complete (takes a depth slot)
    int type = kResource;                  // +0x4c
    int resource = 0;                      // +0x50
    uint32_t buildingId = 0;               // +0x54
    uint32_t itemId = 0;                   // +0x58
    uint32_t profession = 0;               // +0x5c
    int missing = 0;                       // +0x60
};

WindowManager::FunctionalWindow* g_queue = nullptr;   // NotEnoughWindow::wnd
Window* g_root = nullptr;              // 0x62591c Window_upgrade.xml
GUI::TweenEffect* g_tween = nullptr;   // 0x625920
unsigned g_required[11] = {};          // 0x625924 per resource type
unsigned g_level = 0;                  // 0x625950
unsigned g_profession = 0;             // 0x625954
unsigned g_professionLevel = 0;        // 0x625958
std::vector<std::pair<unsigned, unsigned>> g_items;          // 0x62595c (item id, count)
unsigned g_population = 0;             // 0x625968
const char32_t* g_levelFailMessage = nullptr;   // 0x62596c
const char32_t* g_goldFailMessage = nullptr;    // 0x625970
std::vector<std::pair<unsigned, unsigned>> g_buildingCounts; // 0x625974 (building id, count)
std::vector<std::pair<unsigned, unsigned>> g_buildingLevels; // 0x625980 (building id, level)
unsigned g_exchangeLimit = 0;          // 0x62598c
Map::Building* g_upgradable = nullptr; // 0x625990
bool g_actionFlag = false;             // 0x625994
uint32_t g_produceItem = 0;            // 0x625998
uint32_t g_produceAmount = 0;          // 0x6259a0
float g_produceX = 0.f, g_produceY = 0.f;   // 0x6259a4 0x6259a8
Window* g_upgrade = nullptr;           // 0x62599c button_upgrade_window_upgrade (the action)
Textfield* g_upgradeText = nullptr;    // 0x6259ac
Textfield* g_upgrade02Text = nullptr;  // 0x6259b0
Window* g_purpose = nullptr;           // 0x6259b4 BuildingPurposeInfo root
Line g_lines[8];                       // 0x625a44
Textfield* g_header = nullptr;         // 0x625d64 text_header
Textfield* g_desc = nullptr;           // 0x625d68 text_desc
int g_missing = 0;                     // 0x625d6c
Window* g_golden = nullptr;            // 0x625d70 golden_border_box
Window* g_scaler = nullptr;            // 0x625d74 scaler_transparent
int g_extraH = 0;                      // 0x625d78
int g_buttonsY = 0;                    // 0x625d7c
int g_shiftY = 0;                      // 0x625d80
int g_buyAllPrice = 0;                 // 0x625d84
Window* g_buyAll = nullptr;            // 0x625d88 button_upgrade_window_buy_all
Window* g_buyAll02 = nullptr;          // 0x625d8c button_upgrade_window_buy_all_02
Textfield* g_buyAll02Text = nullptr;   // 0x625d90
Textfield* g_buyAll02Price = nullptr;  // 0x625d94
Window* g_upgrade02 = nullptr;         // 0x625d98 button_upgrade_window_upgrade_02
Window* g_upgrade02Lock = nullptr;     // 0x625d9c
Window* g_upgrade02Builder = nullptr;  // 0x625da0
Textfield* g_buyAllText = nullptr;     // 0x625da4
Textfield* g_buyAllPriceText = nullptr;   // 0x625da8
Window* g_upgradeLock = nullptr;       // 0x625dac need_level_lock
Window* g_upgradeBuilder = nullptr;    // 0x625db0 icon_profession_builder
Window* g_darker = nullptr;            // 0x625db4 upgrade_unlocked_holder.darker_02
Textfield* g_bonusText = nullptr;      // 0x625db8 upgrade_unlocked_holder.text_upgrade_bonus
Window* g_underline = nullptr;         // 0x625dbc scaler_limit_underline_2px
Button* g_close = nullptr;             // 0x625dc0 x_button.clickArea
unsigned g_pendingFind = 0xffffffffu;  // 0x60f2f0 the line whose Find runs after the slide-out
int g_rootW = 0, g_rootH = 0;          // 0x60f2f4 0x60f2f8 sizes at Init (SetSize rewrites origW/H)
int g_goldenW = 0, g_goldenH = 0;      // 0x60f2fc 0x60f300
int g_scalerW = 0, g_scalerH = 0;      // 0x60f304 0x60f308
float g_scale = 1.f;                   // 0x60f30c
bool g_tablet = false;                 // 0x60f310

// Table 0x601798: the crystal price per missing unit of resources 0..8.
const char* const kExchangeSettings[9] = {
    "exchange_lumber", "exchange_rocks", "exchange_food", "exchange_planks", "exchange_stones",
    "exchange_meat", "exchange_sausages", "exchange_oil", "exchange_gold"};

// Table 0x59c47c: the building producing resources 0..7, and whether the shop sells it (+0x20).
const uint32_t kResourceBuilding[8] = {17, 20, 19, 102, 101, 1006, 1008, 114};
const bool kResourceBuildingSold[8] = {false, false, true, true, true, true, true, true};

void OnTweenIn() {}   // @0x2f6f10

// @0x2f6c5c
void SetZ(float z) {
    if (!g_queue->shown) return;
    g_root->SetZ(z);
    g_root->SetPosition(g_tween->GetCenteredX(), g_tween->GetCenteredY());
}

// @0x2f77b0: the resource bar above stays usable.
bool Click(int x, int y, bool pressed) {
    if (TopCityWindow::Click(x, y, pressed)) return true;
    bool visible = g_root->visibleSelf;
    g_root->Click(x, y, pressed, false);
    return visible;
}

// @0x2f7428
bool OnBack() {
    if (!g_queue->shown) return false;
    Hide();
    return true;
}

// FUN_002f79a4: the helper arrow's callback, the building's info window as if tapped at (x, y).
GUI::Callback* BuildingClickCallback(Map::Building* b, int x, int y) {
    return new GUI::Callback([b, x, y] {
        // UNVERIFIED (3e, building info windows): BuildingHovers::OnBuildingClick(b, x, y).
        (void)b, (void)x, (void)y;
    });
}

// The arrow over a building, after centring the view on it: a tap on it opens the building.
void PointAtBuilding(Map::Building* b) {
    // UNVERIFIED: Render::CenterOn(x, y, true, true, 0.4, 0, false) animates to zoom 0.4; the port's
    // CenterOn is the instant path without the zoom.
    Render::CenterOn(b->baseX, (b->maxY + b->minY) * 0.5f);
    int sx = (int)b->minX, sy = (int)b->minY;
    Map::WorldCoordinatesToScreen(sx, sy);
    BuildingHovers::SetArrowClickCallback(BuildingClickCallback(b, sx, sy));
    BuildingHovers::ArrowAt(b->baseX, b->minY, false, false, false, false, false, false, false);
}

// @0x2f7a0c: where a resource comes from: its building, else the shop cell selling it.
void FindResourceHelp(int type, unsigned line, bool fromLine) {
    uint32_t id = 0;
    bool sold = false;
    if ((unsigned)type < 8) {
        id = kResourceBuilding[type];
        sold = kResourceBuildingSold[type];
    }
    // UNVERIFIED (3f): on a farm (Map::GetCurrentFarm) HUDWindow::ExitFarm first.
    if (GameState::GetCurrentMapID() == 0) {
        if (Map::Building* b = Map::GetBuildingWithID(id)) {
            PointAtBuilding(b);
        } else if (sold) {
            ShopWindow::Show();
            ShopWindow::OnSelectItemID(id);
            ShopWindow::ShowArrowOnItem(id);
        }
        return;
    }
    if (!fromLine) {
        g_lines[0].missing = 1;
        g_lines[0].resource = type;
        g_pendingFind = 0;
        g_lines[0].type = kResource;
    } else {
        g_pendingFind = line;
    }
    // UNVERIFIED (milestone 4): GlobalMapWindow::SwitchMap(0, false, true) travels home first.
}

// @0x2f7b98
void OnFindActual(unsigned i) {
    BuildingHovers::SetArrowVisibleWindowLimit(2);
    Line& l = g_lines[i];
    switch (l.type) {
    case kResource:
        FindResourceHelp(l.resource, i, true);
        break;
    case kGold:
        ExchangeWindow::Show();
        ExchangeWindow::OnTab(0, SWPrintf(0x80, StringTable::GetString("NO_GOLD"), {ToWideString(l.missing)}).c_str());
        break;
    case kCrystals:
        ExchangeWindow::Show();
        ExchangeWindow::OnTab(1, SWPrintf(0x80, StringTable::GetString("NO_CRYSTALS"),
                                          {ToWideString(l.missing), GameState::GetPlayerName()})
                                     .c_str());
        break;
    case kLevel:
        break;
    case kProfession:
        // UNVERIFIED (milestone 4): CraftItemWindow / LevelInfoWindow on the profession's tab.
        break;
    case kBuildingCount:
        if (GameState::GetCurrentMapID() != 0) break;   // UNVERIFIED (M4): GlobalMapWindow::SwitchMap(0)
        ShopWindow::Show();
        ShopWindow::OnSelectItemID(l.buildingId);
        ShopWindow::ShowArrowOnItem(l.buildingId);
        break;
    case kBuildingLevel: {
        if (GameState::GetCurrentMapID() != 0) break;   // UNVERIFIED (M4): GlobalMapWindow::SwitchMap(0)
        if (Map::Building* b = Map::GetUpgradeableBuildingWithID(l.buildingId)) {
            // UNVERIFIED (3f): the farm cases (ExitFarm, BottomFarmWindow::HelpWithUpgrade).
            if (b->IsOpened() && b->data->buildingClass != 0xd && (!b->HasActiveContract() || !b->contractDone)) {
                Render::CenterOn(b->baseX, (b->maxY + b->minY) * 0.5f);   // UNVERIFIED: zoom 0.4, see above
                // UNVERIFIED (3e, building info windows): BuildingHovers::OnBuildingClick(b, W/2, H/2)
                // and ShowArrowAtUpgrade.
                return;
            }
            PointAtBuilding(b);
            return;
        }
        if (Map::Building* b = Map::GetUnfinishedBuildingWithID(l.buildingId, true)) {
            PointAtBuilding(b);
            return;
        }
        ShopWindow::Show();
        ShopWindow::OnSelectItemID(l.buildingId);
        ShopWindow::ShowArrowOnItem(l.buildingId);
        break;
    }
    case kPopulation:
        if (Map::Building* b = Map::GetUnfinishedBuildingedWithPopulation()) {
            if (ShopWindow::IsVisible()) ShopWindow::Hide();
            int sx = (int)b->minX, sy = (int)b->minY;
            Map::WorldCoordinatesToScreen(sx, sy);
            BuildingHovers::SetArrowClickCallback(BuildingClickCallback(b, sx, sy));
            Render::CenterOn(b->baseX, b->minY);   // UNVERIFIED: zoom 0.4, see PointAtBuilding
            BuildingHovers::SetArrowVisibleWindowLimit(2);
            BuildingHovers::ArrowAt(b->maxX - 30.f, (b->maxY + b->minY) * 0.5f, false, true, false, false, false,
                                    false, false);
        } else {
            ShopWindow::Show();
            ShopWindow::OnTabSelect(0);
            ShopWindow::ShowArrowOnItem(0x20);
        }
        break;
    case kItem:
        // UNVERIFIED (milestone 4, Items): dropped items, mission maps, crafting, the item shop.
        break;
    }
}

// @0x2f873c: a Find tapped runs once the dialog has slid out.
void OnTweenOut() {
    WindowManager::WindowHide(false);
    g_queue->shown = false;
    g_queue->MoveWindowDown(true);
    if (g_pendingFind == 0xffffffffu) return;
    unsigned i = g_pendingFind;
    g_pendingFind = 0xffffffffu;
    OnFindActual(i);
}

// @0x2f76f8
void OnFind(unsigned i) {
    Hide();
    if (GameState::IsBossCombatActive()) {
        PopupWindow::Show(StringTable::GetString("CAN_NOT_TRAVEL"), PopupWindow::Hide, nullptr, nullptr);
        return;
    }
    BuildingHovers::Hide();
    // UNVERIFIED (milestone 4): HireTroops, GlobalMap, CampaignInfo, RaidInfo, HealSquad, Crafting,
    // CraftItem and ArenaDifficulty windows are hidden too.
    if (g_lines[i].type != kPopulation) ShopWindow::Hide();
    g_pendingFind = i;
}

// The crystal price of the missing resources 0..8: ceil(exchange_X * shortfall) each.
int ResourceCrystalPrice() {
    int sum = 0;
    for (int t = 0; t < 9; ++t) {
        int need = (int)g_required[t];
        if (need == 0 || need <= (int)GameState::GetResourceAmount(t)) continue;
        float rate = Setting(kExchangeSettings[t]).GetFloat();
        sum += (int)std::ceil(rate * (float)(unsigned)(need - (int)GameState::GetResourceAmount(t)));
    }
    return sum;
}

// "buy_all_modifier", the exchange limit and a minimum of 1.
void ApplyBuyAllModifier() {
    unsigned price = (unsigned)((float)g_buyAllPrice * Setting("buy_all_modifier").GetFloat());
    g_buyAllPrice = (int)price;
    if (g_exchangeLimit != 0 && g_exchangeLimit < price) {
        g_buyAllPrice = (int)g_exchangeLimit;
        price = g_exchangeLimit;
    }
    if ((int)price < 1) g_buyAllPrice = 1;
}

// @0x2fd2ac: the shortfall bought with crystals; the action runs when nothing else is missing.
void OnBuyAll() {
    g_buyAllPrice = ResourceCrystalPrice();
    // UNVERIFIED (milestone 4, Items): missing items add their crystal price (ItemInfo +0x30) x count.
    g_buyAllPrice = GameState::AdjustCrystalCost(g_buyAllPrice);
    if (g_buyAllPrice != 0) ApplyBuyAllModifier();
    if ((int)GameState::GetResourceAmount(GameState::kCrystal) < g_buyAllPrice) {
        ExchangeWindow::Show();
        ExchangeWindow::OnTab(1, SWPrintf(0x80, StringTable::GetString("NO_CRYSTALS"),
                                          {ToWideString(g_buyAllPrice), GameState::GetPlayerName()})
                                     .c_str());
        return;
    }
    for (int t = 0; t < 9; ++t) {
        int need = (int)g_required[t];
        if (need == 0 || (int)GameState::GetResourceAmount(t) >= need) continue;
        GameState::ChangeResourceAmount(t, need - (int)GameState::GetResourceAmount(t));
    }
    // UNVERIFIED (milestone 4, Items): missing items are added (GameState::AddItem).
    // UNVERIFIED (milestone 5): SoundsManager::PlaySound("ui_buy_with_crystals").
    GameState::ChangeResourceAmount(GameState::kCrystal, -g_buyAllPrice);
    // Billing::LogCBPurchase(0x1c, 0, price): online logging, not ported.
    if (!CheckRequirements()) {
        UpdateContents();
        return;
    }
    if (g_upgrade->onClick) g_upgrade->onClick();
    Hide();
}

// @0x2f7450
void OnBuyItem() {
    // UNVERIFIED (milestone 4, Items): buys the item to produce for crystals and drops it at
    // (g_produceX, g_produceY) (BuildingHovers::DropItem).
}

void CenterOnScreen() {
    g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::GetVerticalCenter(g_root->h));
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
        g_queue = new WindowManager::FunctionalWindow("NotEnoughWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_queue->shown; }   // @0x2f6ce8

// @0x2fd9a4
void Init() {
    g_extraH = 0;
    g_buttonsY = 0;
    g_shiftY = 0;
    g_tablet = GUI::IsTabletVersion();
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Window_upgrade.xml", "Window_upgrade.png", -1.f, 0,
                             0, 0, 0x32, false, 1.f);
    g_queue->RegisterTopWindow(g_root);
    g_scale = g_root->scale;
    g_root->SetVisibility(false);
    CenterOnScreen();
    g_rootW = g_root->origW;
    g_rootH = g_root->origH;
    g_tween = GUI::CreateTweenEffect(g_root, nullptr, OnTweenIn, OnTweenOut);
    g_close = GUI::GetWindowTyped<Button>(g_root, "x_button.clickArea");
    g_close->SetOnClick(Hide);
    g_close->SetActionSound("ui_close", 1);
    g_header = GUI::GetWindowTyped<Textfield>(g_root, "text_header");
    g_desc = GUI::GetWindowTyped<Textfield>(g_root, "text_desc");
    g_golden = GUI::GetWindowTyped<Window>(g_root, "golden_border_box");
    g_goldenW = g_golden->origW;
    g_goldenH = g_golden->origH;
    g_scaler = GUI::GetWindowTyped<Window>(g_root, "scaler_transparent");
    g_scalerW = g_scaler->origW;
    g_scalerH = g_scaler->origH;
    g_darker = GUI::GetWindowTyped<Window>(g_root, "upgrade_unlocked_holder.darker_02");
    g_bonusText = GUI::GetWindowTyped<Textfield>(g_root, "upgrade_unlocked_holder.text_upgrade_bonus");
    // BuildingPurposeInfo::LoadFrom @0x2f7850. UNVERIFIED (building upgrades): only its root is
    // loaded; the purpose blocks (Shared::House*Info) are filled for an upgradable building.
    g_purpose = GUI::GetWindowTyped<Window>(g_root, "upgrade_unlocked_holder.building_purpose_holder");
    g_underline = GUI::GetWindowTyped<Window>(g_root, "scaler_limit_underline_2px");
    for (unsigned i = 0; i < 8; ++i) {
        const unsigned n = i + 1;
        const char* line = "window_uprgade_1_line_%02d";
        Line& l = g_lines[i];
        l.root = GUI::GetWindowTypedF<Window>(g_root, line, n);
        l.icon = GUI::GetWindowTypedF<Window>(g_root, "window_uprgade_1_line_%02d.icon_60_on", n);
        l.icon->takesZ = true;
        l.sep02 = GUI::GetWindowTypedF<Window>(g_root, "window_uprgade_1_line_%02d.sepparator_vertical_02", n);
        l.sep03 = GUI::GetWindowTypedF<Window>(g_root, "window_uprgade_1_line_%02d.sepparator_vertical_03", n);
        l.name = GUI::GetWindowTypedF<Textfield>(g_root, "window_uprgade_1_line_%02d.text_resource_name", n);
        l.quantity = GUI::GetWindowTypedF<Textfield>(g_root, "window_uprgade_1_line_%02d.text_quantity_resource", n);
        l.quantityShort =
            GUI::GetWindowTypedF<Textfield>(g_root, "window_uprgade_1_line_%02d.text_quantity_resource_not_enough", n);
        l.level = GUI::GetWindowTypedF<Textfield>(g_root, "window_uprgade_1_line_%02d.text_level", n);
        l.levelShort = GUI::GetWindowTypedF<Textfield>(g_root, "window_uprgade_1_line_%02d.text_level_not_reached", n);
        for (unsigned c = 0; c < 5; ++c)
            l.itemClass[c] =
                GUI::GetWindowTypedF<Textfield>(g_root, "window_uprgade_1_line_%02d.text_item_class_%02d", n, c + 1);
        l.itemQuantity = GUI::GetWindowTypedF<Textfield>(g_root, "window_uprgade_1_line_%02d.text_quantity_item", n);
        l.itemQuantityShort =
            GUI::GetWindowTypedF<Textfield>(g_root, "window_uprgade_1_line_%02d.text_quantity_item_not_enough", n);
        l.find = GUI::GetWindowTypedF<Window>(g_root, "window_uprgade_1_line_%02d.button_upgrade_window_find_one", n);
        l.find->SetOnClick([i] { OnFind(i); });
        l.findText = GUI::GetWindowTypedF<Textfield>(
            g_root, "window_uprgade_1_line_%02d.button_upgrade_window_find_one.text_upgrade", n);
        l.complete = GUI::GetWindowTypedF<Window>(g_root, "window_uprgade_1_line_%02d.icon_complete", n);
        l.complete->takesZ = true;
        l.complete->SetTexture(IconManager::GetIcon("gold_confirm"), false);
    }
    g_upgrade = GUI::GetWindowTyped<Window>(g_root, "button_upgrade_window_upgrade");
    g_upgradeBuilder = GUI::GetWindowTyped<Window>(g_root, "button_upgrade_window_upgrade.icon_profession_builder");
    g_upgradeLock = GUI::GetWindowTyped<Window>(g_root, "button_upgrade_window_upgrade.need_level_lock");
    g_upgradeText = GUI::GetWindowTyped<Textfield>(g_root, "button_upgrade_window_upgrade.text_upgrade");
    g_buyAll = GUI::GetWindowTyped<Window>(g_root, "button_upgrade_window_buy_all");
    g_buyAll->SetOnClick(OnBuyAll);
    g_buyAllText = GUI::GetWindowTyped<Textfield>(g_root, "button_upgrade_window_buy_all.text");
    g_buyAllPriceText = GUI::GetWindowTyped<Textfield>(g_root, "button_upgrade_window_buy_all.text_price");
    g_upgrade02 = GUI::GetWindowTyped<Window>(g_root, "button_upgrade_window_upgrade_02");
    g_upgrade02Builder =
        GUI::GetWindowTyped<Window>(g_root, "button_upgrade_window_upgrade_02.icon_profession_builder");
    g_upgrade02Lock = GUI::GetWindowTyped<Window>(g_root, "button_upgrade_window_upgrade_02.need_level_lock");
    g_upgrade02Text = GUI::GetWindowTyped<Textfield>(g_root, "button_upgrade_window_upgrade_02.text_upgrade");
    g_buyAll02 = GUI::GetWindowTyped<Window>(g_root, "button_upgrade_window_buy_all_02");
    g_buyAll02->SetOnClick(OnBuyItem);
    g_buyAll02Text = GUI::GetWindowTyped<Textfield>(g_root, "button_upgrade_window_buy_all_02.text");
    g_buyAll02Price = GUI::GetWindowTyped<Textfield>(g_root, "button_upgrade_window_buy_all_02.text_price");
    g_queue->fn.back = OnBack;
    g_queue->fn.show = Show;
}

// @0x2f7810
void Deinit() {
    GUI::RemoveTweenEffect(g_tween);
    g_tween = nullptr;
    delete g_root;
    g_root = nullptr;
}

// @0x2f6cfc
void ResetRequirements() {
    for (unsigned& r : g_required) r = 0;
    g_professionLevel = 0;
    g_level = 0;
    g_items.clear();
    g_profession = 0;
    g_goldFailMessage = nullptr;
    g_population = 0;
    g_buildingCounts.clear();
    g_levelFailMessage = nullptr;
    g_buildingLevels.clear();
    g_exchangeLimit = 0;
    g_upgradable = nullptr;
    g_actionFlag = false;
    g_produceItem = 0;
    g_upgrade->SetOnClick(nullptr);
}

void SetLevelFailMessage(const char32_t* text) { g_levelFailMessage = text; }   // @0x2f6dd0
void SetGoldFailMesage(const char32_t* text) { g_goldFailMessage = text; }      // @0x2f6de4
void SetUpgradableBuilding(Map::Building* b) { g_upgradable = b; }              // @0x2f6df8

// @0x2f6e0c
void SetItemToProduce(uint32_t item, uint32_t amount, float x, float y) {
    g_produceY = y;
    g_produceItem = item;
    g_produceAmount = amount;
    g_produceX = x;
}

// @0x2f6e2c: the action button (its text on both upgrade buttons). flag: a single missing level,
// gold, crystals or people still opens their popup even with an action.
void SetActionCallback(GUI::Callback cb, const char32_t* text, bool flag) {
    g_upgrade->SetOnClick(std::move(cb));
    g_upgradeText->SetText(text);
    g_upgrade02Text->SetText(text);
    g_actionFlag = flag;
}

void AddRequirement(int type, unsigned amount) { g_required[type] = amount; }   // @0x2f6ea4
void AddLevelRequirement(unsigned level) { g_level = level; }                   // @0x2f6ebc

// @0x2f6ed0
void AddReputationRequirement(unsigned profession, unsigned level) {
    g_professionLevel = level;
    g_profession = profession;
}

void AddPopulaionRequirement(unsigned people) { g_population = people; }        // @0x2f6ee8
void SetExchangeLimit(unsigned limit) { g_exchangeLimit = limit; }              // @0x2f6efc
void AddItemRequirement(unsigned id, unsigned count) { g_items.emplace_back(id, count); }   // @0x2f87bc
void AddBuildingCountRequirement(unsigned id, unsigned count) { g_buildingCounts.emplace_back(id, count); }   // @0x2f8948
void AddBuildingLevelRequirement(unsigned id, unsigned level) { g_buildingLevels.emplace_back(id, level); }   // @0x2f8ad4

// @0x2f8c60
bool CheckRequirements() {
    g_missing = 0;
    for (int i = 0; i < 11; ++i) {
        int need = (int)g_required[i];
        if (need != 0 && (int)GameState::GetResourceAmount(i) < need) ++g_missing;
    }
    if (g_level != 0 && (unsigned)GameState::GetLevel() < g_level) ++g_missing;
    // UNVERIFIED (milestone 4): the profession level (Professions::GetPlayerProfessionLevel).
    for (auto& [id, n] : g_items)
        if ((unsigned)GameState::GetItemAmount(id, false) < n) ++g_missing;
    if (g_population != 0 &&
        GameState::GetPlayerWorkersCount() - Map::GetUsedWorkerCount() < (int)g_population)
        ++g_missing;
    for (auto& [id, n] : g_buildingCounts)
        if ((unsigned)Map::GetBuildingCount(id, false) < n) ++g_missing;
    for (auto& [id, n] : g_buildingLevels)
        if ((unsigned)Map::GetBuildingMaxUpgrade(id) < n) ++g_missing;
    return g_missing == 0;
}

// @0x2f7134
void SetDescriptionText(const char32_t* desc, const char32_t* title) {
    if (!title) {
        g_header->SetText(StringTable::GetString("REQUIREMENTS"));
    } else {
        g_header->SetText(SWPrintf(0x100, U"%s - %s", {StringTable::GetString("REQUIREMENTS"), title}).c_str());
    }
    g_desc->SetText(desc);
}

// @0x2f8ecc: one line per missing (or met) requirement, the buttons, then the layout around the
// lines. CheckRequirements has counted the missing ones.
void UpdateContents() {
    g_root->RescaleWindow(1.f);
    g_root->SetSize((unsigned)g_rootW, (unsigned)g_rootH);
    g_golden->SetSize((unsigned)g_goldenW, (unsigned)g_goldenH);
    g_scaler->SetSize((unsigned)g_scalerW, (unsigned)g_scalerH);
    g_extraH = 0;
    g_buttonsY = 0;
    g_shiftY = 0;
    for (Line& l : g_lines) {
        l.root->SetVisibility(false);
        l.icon->SetVisibility(false);
        l.sep02->SetVisibility(false);
        l.sep03->SetVisibility(false);
        l.name->SetVisibility(false);
        l.quantity->SetVisibility(false);
        l.quantityShort->SetVisibility(false);
        l.level->SetVisibility(false);
        l.levelShort->SetVisibility(false);
        for (Textfield* t : l.itemClass) t->SetVisibility(false);
        l.itemQuantity->SetVisibility(false);
        l.itemQuantityShort->SetVisibility(false);
        l.find->SetVisibility(false);
        l.complete->SetVisibility(false);
    }
    g_buyAllPrice = 0;

    unsigned n = 0;
    bool levelOk = true;
    if (g_level != 0 && (unsigned)GameState::GetLevel() < g_level) {
        Line& l = g_lines[0];
        l.root->SetVisibility(true);
        l.icon->SetVisibility(true);
        l.icon->SetTexture(IconManager::GetIcon(GameState::GetResourceMapIconName(10)), true);
        std::u32string text = SWPrintf(0x100, StringTable::GetString("SHOP_UNLOCK"), {ToWideString((int)g_level)});
        if ((unsigned)GameState::GetLevel() < g_level) {
            l.levelShort->SetVisibility(true);
            l.levelShort->SetText(text.c_str());
            l.complete->SetVisibility(true);
            l.complete->SetTexture(IconManager::GetIcon("impossible"), false);
        } else {
            l.level->SetVisibility(true);
            l.level->SetText(text.c_str());
            l.complete->SetVisibility(true);
            l.complete->SetTexture(IconManager::GetIcon("gold_confirm"), false);
        }
        n = 1;
        levelOk = false;
        l.type = kLevel;
    }
    // UNVERIFIED (milestone 4): the profession line ("REQUIRES_REPUTATION_LEVEL", type 4) and the
    // item lines (type 8; their crystal price x shortfall joins Buy all). Items::GetItemInfo is not
    // ported, so the original would skip the item lines here too.

    if (g_population != 0 && GameState::GetPlayerWorkersCount() - Map::GetUsedWorkerCount() < (int)g_population) {
        Line& l = g_lines[n];
        l.root->SetVisibility(true);
        l.icon->SetVisibility(true);
        l.icon->SetTexture(IconManager::GetIcon("icon_b_houses"), true);
        if (GameState::GetPlayerWorkersCount() - Map::GetUsedWorkerCount() < (int)g_population) {
            int need = (int)g_population + Map::GetUsedWorkerCount() - GameState::GetPlayerWorkersCount();
            l.levelShort->SetVisibility(true);
            l.levelShort->SetText(
                SWPrintf(0x100, StringTable::GetString("PEOPLE_REQUIRED"), {ToWideString(need)}).c_str());
            l.find->SetVisibility(true);
            l.findText->SetText(StringTable::GetString("PERFORM_FIND"));
        } else {
            l.level->SetVisibility(true);
            l.level->SetText(
                SWPrintf(0x100, StringTable::GetString("PEOPLE_REQUIRED"), {ToWideString((int)g_population)}).c_str());
            l.complete->SetVisibility(true);
            l.complete->SetTexture(IconManager::GetIcon("gold_confirm"), false);
        }
        l.type = kPopulation;
        ++n;
    }

    for (int t = 0; t < 11; ++t) {
        if (g_required[t] == 0) continue;
        if (n == 8) {
            std::printf("ERROR: NotEnoughWindow::UpdateContents() has too many requirements\n");
            break;
        }
        Line& l = g_lines[n];
        l.root->SetVisibility(true);
        l.icon->SetVisibility(true);
        l.icon->SetTexture(IconManager::GetIcon(GameState::GetResourceMapIconName(t)), true);
        l.sep02->SetVisibility(true);
        l.sep03->SetVisibility(false);
        l.name->SetVisibility(true);
        l.name->SetText(GameState::GetResourceGameName(t));
        int have = (int)GameState::GetResourceAmount(t);
        if (have < (int)g_required[t]) {
            l.quantityShort->SetVisibility(true);
            l.quantityShort->SetText(SWPrintf(0x100, U"%d/%d", {have, g_required[t]}).c_str());
            l.find->SetVisibility(true);
            l.findText->SetText(StringTable::GetString("PERFORM_FIND"));
            if (t < 9) {
                float rate = Setting(kExchangeSettings[t]).GetFloat();
                g_buyAllPrice +=
                    (int)std::ceil(rate * (float)(unsigned)((int)g_required[t] - (int)GameState::GetResourceAmount(t)));
            }
        } else {
            l.quantity->SetVisibility(true);
            l.quantity->SetText(ToWideString((int)g_required[t]));
            l.complete->SetVisibility(true);
            l.complete->SetTexture(IconManager::GetIcon("gold_confirm"), false);
        }
        l.type = t == GameState::kGold ? kGold : t == GameState::kCrystal ? kCrystals : kResource;
        l.resource = t;
        l.missing = (int)g_required[t] - (int)GameState::GetResourceAmount(t);
        ++n;
    }

    bool full = false;
    for (auto& [id, count] : g_buildingCounts) {
        if (n == 8) { full = true; break; }
        const GameData::BuildingData* d = GameData::GetBuilding(id);
        const char32_t* name = StringTable::GetString(d->name.c_str());
        std::u32string text = count == 1
            ? SWPrintf(0x100, StringTable::GetString("NEED_BUILDING_NAME"), {name})
            : SWPrintf(0x100, StringTable::GetString("NEED_BUILDING_NAME_MULTIPLE"), {ToWideString((int)count), name});
        if ((unsigned)Map::GetBuildingCount(id, false) < count) {
            Line& l = g_lines[n++];
            l.root->SetVisibility(true);
            l.levelShort->SetVisibility(true);
            l.levelShort->SetText(text.c_str());
            l.find->SetVisibility(true);
            l.findText->SetText(StringTable::GetString("PERFORM_FIND"));
            l.type = kBuildingCount;
            l.buildingId = id;
        }
    }
    if (full) {
        n = 8;
        std::printf("ERROR: NotEnoughWindow::UpdateContents() has too many requirements\n");
    }

    full = false;
    for (auto& [id, level] : g_buildingLevels) {
        if (n == 8) { full = true; break; }
        const GameData::BuildingData* d = GameData::GetBuilding(id);
        std::u32string text = SWPrintf(0x100, StringTable::GetString("NEED_BUILDING_LEVEL"),
                                       {StringTable::GetString(d->name.c_str()), ToWideString((int)level)});
        if ((unsigned)Map::GetBuildingMaxUpgrade(id) < level) {
            Line& l = g_lines[n++];
            l.root->SetVisibility(true);
            if (d->id - 99u < 2) {   // the castle
                l.icon->SetVisibility(true);
                l.icon->SetTexture(IconManager::GetIcon("icon_castle_60"), true);
            }
            l.levelShort->SetVisibility(true);
            l.levelShort->SetText(text.c_str());
            l.find->SetVisibility(true);
            l.findText->SetText(StringTable::GetString("PERFORM_FIND"));
            l.type = kBuildingLevel;
            l.buildingId = id;
        }
    }
    if (full) {
        n = 8;
        std::printf("ERROR: NotEnoughWindow::UpdateContents() has too many requirements\n");
    }

    if (g_buyAllPrice != 0) ApplyBuyAllModifier();

    // UNVERIFIED (milestone 4, Items): with an item to produce that has a crystal price (and something
    // missing, level reached) buy_all_02 and upgrade_02 show instead. Items::GetItemInfo is not ported.
    g_buyAll02->SetVisibility(false);
    g_upgrade02->SetVisibility(false);
    g_buyAll->SetVisibility(g_missing != 0);
    g_buyAllText->SetText(StringTable::GetString("BUY_ALL"));
    g_buyAllPriceText->SetText(ToWideString(g_buyAllPrice));
    if (g_missing == 0) {
        g_upgrade->SetVisibility(true);
        g_upgrade->SetEnabled(true);
        g_upgradeLock->SetVisibility(false);
        g_upgradeBuilder->SetVisibility(true);
    } else if (!levelOk) {
        g_upgrade->SetVisibility(true);
        g_upgrade->SetEnabled(false);
        g_upgradeLock->SetVisibility(true);
        g_upgradeBuilder->SetVisibility(false);
    } else {
        g_upgrade->SetVisibility(false);
    }

    // The lines' height: from the first line to the last shown one.
    g_extraH = n < 2 ? -10 : g_lines[n - 1].root->y - g_lines[0].root->y - 10;
    g_purpose->SetVisibility(false);
    g_darker->SetVisibility(false);
    g_bonusText->SetVisibility(false);
    if (g_upgradable) {
        // UNVERIFIED (building upgrades): "UPGRADE_WILL_UNLOCK" and the building's purpose (the
        // missions a workshop unlocks, a house's tax and workers, a storage's extra space), shown in
        // the purpose block with the darker background and the bonus text.
    }
    g_buttonsY = g_extraH;
    g_underline->SetVisibility(g_purpose->visibleSelf);
    if (g_purpose->visibleSelf) g_extraH += g_purpose->h + g_underline->h;

    g_scale = GUI::GetScaleFactor(g_root->origW, g_golden->origH + 0x50 + g_extraH, !GUI::IsTabletVersion(), 1.f);
    g_root->RescaleWindow(g_scale);
    g_tween->CenterWith(0, (int)(g_scale * -60.f), 0, 0);
    g_extraH = (int)((float)g_extraH * g_scale);
    g_buttonsY = (int)((float)g_buttonsY * g_scale);
    g_golden->SetSize((unsigned)g_golden->w, (unsigned)(g_extraH + g_golden->h));
    g_scaler->SetSize((unsigned)g_scaler->w, (unsigned)(g_scaler->h + g_buttonsY));
    g_upgrade->MoveWindow(0, g_buttonsY);
    g_buyAll->MoveWindow(0, g_buttonsY);
    g_upgrade02->MoveWindow(0, g_buttonsY);
    g_buyAll02->MoveWindow(0, g_buttonsY);
    // Without the purpose block everything below the header moves up by its height.
    g_shiftY = 0;
    if (!g_purpose->visibleSelf)
        g_shiftY = -(int)((float)(g_underline->h + g_purpose->h) + g_scale * -10.f);
    g_desc->MoveWindow(0, g_shiftY);
    g_scaler->MoveWindow(0, g_shiftY);
    for (Line& l : g_lines) l.root->MoveWindow(0, g_shiftY);
    g_upgrade->MoveWindow(0, g_shiftY);
    g_buyAll->MoveWindow(0, g_shiftY);
    g_upgrade02->MoveWindow(0, g_shiftY);
    g_buyAll02->MoveWindow(0, g_shiftY);
    g_root->SetSize((unsigned)g_root->w, (unsigned)(g_close->h + g_golden->h));
}

// @0x2fc7f8: a single missing level, gold, crystals or people (with no action, or the action's
// flag set) opens their own popup or the exchange; otherwise the dialog.
void Show() {
    BuildingHovers::HideArrow();
    auto direct = [] { return !g_upgrade->onClick || g_actionFlag; };
    if (direct() && g_missing == 1) {
        if ((unsigned)GameState::GetLevel() < g_level) {
            const char32_t* fmt = g_levelFailMessage ? g_levelFailMessage : StringTable::GetString("SHOP_UNLOCK");
            PopupWindow::Show(SWPrintf(0x80, fmt, {ToWideString((int)g_level)}).c_str(), PopupWindow::Hide, nullptr,
                              nullptr);
            return;
        }
    }
    if (direct()) {
        int gold = (int)g_required[GameState::kGold];
        if (g_missing == 1 && (int)GameState::GetResourceAmount(GameState::kGold) < gold) {
            const char32_t* fmt = g_goldFailMessage ? g_goldFailMessage : StringTable::GetString("NO_GOLD");
            int need = gold - (int)GameState::GetResourceAmount(GameState::kGold);
            ExchangeWindow::Show();
            ExchangeWindow::OnTab(0, SWPrintf(0x80, fmt, {ToWideString(need)}).c_str());
            return;
        }
        int crystals = (int)g_required[GameState::kCrystal];
        if (g_missing == 1 && (int)GameState::GetResourceAmount(GameState::kCrystal) < crystals) {
            int need = crystals - (int)GameState::GetResourceAmount(GameState::kCrystal);
            ExchangeWindow::Show();
            ExchangeWindow::OnTab(1, SWPrintf(0x80, StringTable::GetString("NO_CRYSTALS"),
                                              {ToWideString(need), GameState::GetPlayerName()})
                                         .c_str());
            return;
        }
        if (g_missing == 1 && g_population != 0 &&
            GameState::GetPlayerWorkersCount() - Map::GetUsedWorkerCount() < (int)g_population) {
            PopupWindow::Show(
                SWPrintf(0x80, StringTable::GetString("NO_PEOPLE"), {ToWideString((int)g_population)}).c_str(),
                PopupWindow::Hide, PopupWindow::Hide, nullptr);
            return;
        }
    }
    g_queue->shown = true;
    g_queue->MoveWindowOnTop(true);
    if (!g_root->visibleSelf) {
        WindowManager::WindowShow(false);
        g_root->SetVisibility(true);
        UpdateContents();
        g_tween->AnimateIn();
    } else {
        g_root->SetVisibility(true);
        UpdateContents();
    }
    g_root->PreloadTextures();   // GUI::PreloadWindow
}

// @0x2f73f4
void Hide() {
    // UNVERIFIED (milestone 4): CampaignInfoWindow and RaidInfoWindow UpdateContents.
    if (!g_root->visibleSelf) return;
    g_tween->AnimateOut();
}

// @0x2f8790
void RunLastHelpItem() {
    if (g_pendingFind == 0xffffffffu) return;
    OnFindActual(g_pendingFind);
    g_pendingFind = 0xffffffffu;
}

}  // namespace NotEnoughWindow
