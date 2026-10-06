// LandWindow: buying a piece of land (Land_win.xml), opened by tapping its for-sale sign: the area's
// name, what grows on it (its trees and rocks, or the list of what stands there), and two buy
// buttons, gold (blue) and crystals (gold), with the level it needs. After the purchase
// LandExpandedWindow celebrates it. The farm's soil patches use the same window (3f).
// Port of LandWindow (libkingdom.so 5.11), 0x2e7d8c..0x2e8cc0. Statics at 0x623e90.
#include <cstdio>
#include <string>

#include "engine/IconManager.h"
#include "game/GameData.h"
#include "game/GameState.h"
#include "game/Map.h"
#include "game/Setting.h"
#include "game/StringTable.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/HUD.h"
#include "hud/Shared.h"
#include "windows/Windows.h"

namespace LandWindow {
namespace {

using GUI::Button;
using GUI::Textfield;
using GUI::Window;

WindowManager::FunctionalWindow* g_queue = nullptr;   // LandWindow::wnd
Window* g_icons[2] = {};               // 0x623e90 icon_lumberjack, icon_miner
Textfield* g_texts[2] = {};            // 0x623e98 wood, rocks
Textfield* g_crystalText = nullptr;    // 0x623ea0 button_shop_buy_gold.text
Textfield* g_crystalPrice = nullptr;   // 0x623ea4 button_shop_buy_gold.text_price
int g_crystals = 0;                    // 0x623ea8 the crystal price ("c2")
Textfield* g_goldText = nullptr;       // 0x623eac button_shop_buy_blue.text
Textfield* g_goldPrice = nullptr;      // 0x623eb0 button_shop_buy_blue.text_price
int g_gold = 0;                        // 0x623eb4 the gold price ("c")
Window* g_locked = nullptr;            // 0x623eb8 button_locked (level not reached)
int g_level = 0;                       // 0x623ebc the level it needs ("l")
Textfield* g_lockedText = nullptr;     // 0x623ec0 button_locked.text_level_req
Window* g_goldButton = nullptr;        // 0x623ec4 button_shop_buy_blue (pays gold)
Window* g_crystalButton = nullptr;     // 0x623ec8 button_shop_buy_gold (pays crystals)
Window* g_landImage = nullptr;         // 0x623ecc image_land_expand
Window* g_farmImage = nullptr;         // 0x623ed0 image_farm_expand
Textfield* g_header = nullptr;         // 0x623ed4 text_01
Textfield* g_list = nullptr;           // 0x623ed8 farm (the list of what stands there)
Window* g_root = nullptr;              // 0x623edc Land_win.xml
GUI::TweenEffect* g_tween = nullptr;   // 0x623ee0
GameState::AreaInfo g_area;            // 0x623ee4
Button* g_close = nullptr;             // 0x623f90 x_button.clickArea
Shared::LargeLogoWindow g_logo;        // 0x623f94
bool g_tablet = false;                 // 0x60f2d4
float g_scale = 1.f;                   // 0x60f2d8

// @0x2e7e4c
void OnTweenOut() {
    GameState::DropGamePauseState();
    WindowManager::WindowHide(false);
    g_queue->shown = false;
    g_queue->MoveWindowDown(true);
}

// @0x2e8774
void SetZ(float z) {
    g_root->SetZ(z);
    g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::GetVerticalCenter(g_root->h));
}

// @0x2e87e4: the resource bar above stays usable.
bool Click(int x, int y, bool pressed) {
    if (TopCityWindow::Click(x, y, pressed)) return true;
    bool visible = g_root->visibleSelf;
    g_root->Click(x, y, pressed, false);
    return visible;
}

// @0x2e8394
bool OnBack() {
    if (!g_queue->shown) return false;
    Hide();
    return true;
}

// @0x2e8588: 0 pays gold (and needs the level), 1 pays crystals; then the area is ours.
void OnBuyArea(unsigned withCrystals) {
    NotEnoughWindow::ResetRequirements();
    int crystals = GameState::AdjustCrystalCost(g_crystals);
    if (withCrystals == 0) {
        if (g_gold > 0) NotEnoughWindow::AddRequirement(GameState::kGold, (unsigned)g_gold);
        NotEnoughWindow::AddLevelRequirement((unsigned)g_level);
    } else if (g_crystals > 0) {
        NotEnoughWindow::AddRequirement(GameState::kCrystal, (unsigned)crystals);
    }
    if (!NotEnoughWindow::CheckRequirements()) {
        NotEnoughWindow::Show();
        return;
    }
    // FileManager::OnItemBought("land_expansion", ...): online statistics, not ported.
    // UNVERIFIED (milestone 4): Tasks::CompleteSubtask(6, area, 1) and (0x3b, 0, 1).
    if (withCrystals == 0) {
        // UNVERIFIED (milestone 5): SoundsManager::PlaySound("ui_buy_with_gold").
        GameState::ChangeResourceAmount(GameState::kGold, -g_gold);
        Map::BuyArea(g_area.id);
        Hide();
    } else {
        // UNVERIFIED (milestone 5): SoundsManager::PlaySound("ui_buy_with_crystals").
        GameState::ChangeResourceAmount(GameState::kCrystal, -crystals);
        // Billing::LogCBPurchase(8, area, price): online logging, not ported.
        Map::BuyArea(g_area.id);
        Hide();
        Map::Save(0);
    }
    LandExpandedWindow::SetAreaParameters(g_area.id);
    LandExpandedWindow::Show();
}

// The buttons' texts and prices and the level lock (the tail of SetAreaParameters and of
// SetPatchParameters).
void FillButtons() {
    g_crystalText->SetText(StringTable::GetString("ITEM_SHOP_BUY"));
    g_crystalPrice->SetText(ToWideString(GameState::AdjustCrystalCost(g_crystals)));
    g_goldText->SetText(StringTable::GetString("ITEM_SHOP_BUY"));
    g_goldPrice->SetText(ToWideString(g_gold));
    g_locked->SetVisibility(GameState::GetLevel() < g_level);
    g_lockedText->SetText(ToWideString(g_level));
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
        g_queue = new WindowManager::FunctionalWindow("LandWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_queue->shown; }   // @0x2e7d8c

// @0x2e8888
void Init() {
    g_tablet = GUI::IsTabletVersion();
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Land_win.xml", "Land_win.png", -1.f, 0, 0, 0, 0, false,
                             1.f);
    g_queue->RegisterTopWindow(g_root);
    g_scale = g_root->scale;
    g_root->SetVisibility(false);
    g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::GetVerticalCenter(g_root->h));
    g_tween = GUI::CreateTweenEffect(g_root, nullptr, nullptr, OnTweenOut);
    g_close = GUI::GetWindowTyped<Button>(g_root, "x_button.clickArea");
    g_close->SetOnClick(Hide);
    g_close->SetActionSound("ui_close", 1);
    g_logo.LoadFrom(g_root, "logo_holder_large");
    g_logo.SetLanguage((unsigned)StringTable::GetLangID());
    g_header = GUI::GetWindowTyped<Textfield>(g_root, "text_01");
    g_texts[0] = GUI::GetWindowTyped<Textfield>(g_root, "wood");
    g_texts[1] = GUI::GetWindowTyped<Textfield>(g_root, "rocks");
    g_icons[0] = GUI::GetWindowTyped<Window>(g_root, "icon_lumberjack");
    g_icons[1] = GUI::GetWindowTyped<Window>(g_root, "icon_miner");
    g_list = GUI::GetWindowTyped<Textfield>(g_root, "farm");
    g_crystalButton = GUI::GetWindowTyped<Window>(g_root, "button_shop_buy_gold");
    g_crystalText = GUI::GetWindowTyped<Textfield>(g_root, "button_shop_buy_gold.text");
    g_crystalPrice = GUI::GetWindowTyped<Textfield>(g_root, "button_shop_buy_gold.text_price");
    g_goldButton = GUI::GetWindowTyped<Window>(g_root, "button_shop_buy_blue");
    g_goldText = GUI::GetWindowTyped<Textfield>(g_root, "button_shop_buy_blue.text");
    g_goldPrice = GUI::GetWindowTyped<Textfield>(g_root, "button_shop_buy_blue.text_price");
    g_locked = GUI::GetWindowTyped<Window>(g_root, "button_locked");
    g_lockedText = GUI::GetWindowTyped<Textfield>(g_root, "button_locked.text_level_req");
    g_landImage = GUI::GetWindowTyped<Window>(g_root, "image_land_expand");
    g_farmImage = GUI::GetWindowTyped<Window>(g_root, "image_farm_expand");
    g_queue->fn.zRange = WindowManager::ReturnSmallZRange;
    g_queue->fn.back = OnBack;
    g_queue->hideOnOuterClick = true;
    g_queue->fn.show = Show;
}

// @0x2e8848
void Deinit() {
    GUI::RemoveTweenEffect(g_tween);
    g_tween = nullptr;
    delete g_root;
    g_root = nullptr;
}

// @0x2e8cc0: the area's prices and level, its name, and what is on it: a line per building
// ("%dx %s" for several). With nothing but trees and rocks the lumberjack and miner lines show
// how much wood and stone it holds instead.
void SetAreaParameters(uint32_t areaId) {
    if (!GameState::GetAreaInfo(areaId, g_area))
        std::printf("LandWindow::SetParameters() Area ID %d has no info\n", (int)areaId);
    g_gold = g_area.c;
    g_crystals = g_area.c2;
    g_level = (int)g_area.level;
    g_header->SetText(g_area.name);
    std::u32string list;
    int others = 0, trees = 0, rocks = 0;
    for (int i = 0; i < (int)g_area.defObjCount; ++i) {
        const GameData::BuildingData* d = GameData::GetBuilding((uint32_t)g_area.defObjs[i].id);
        if (!d) continue;
        const int n = g_area.defObjs[i].count;
        const unsigned room = 0x100 - (unsigned)list.size();
        auto line = [&] {
            const char32_t* name = StringTable::GetString(d->name.c_str());
            return n < 2 ? SWPrintf(room, U"%s\n", {name}) : SWPrintf(room, U"%dx %s\n", {n, name});
        };
        if (d->buildingClass != 4) {
            list += line();
            ++others;
            continue;
        }
        if (d->produceResource == 0) {   // a tree
            list += line();
            trees += n;
        }
        if (d->produceResource == 1) {   // a rock
            list += line();
            rocks += n;
        }
    }
    const bool onlyNature = others == 0;
    const bool showTrees = trees != 0 && onlyNature, showRocks = rocks != 0 && onlyNature;
    for (int i = 0; i < 2; ++i) {
        g_icons[i]->SetVisibility(false);
        g_texts[i]->SetVisibility(false);
    }
    if (showTrees) {
        g_icons[0]->SetTexture(IconManager::GetIcon("profession_lumberjack"), true);
        std::u32string f = StringTable::GetCountableString(StringTable::GetString("BUY_LAND_TREE"), trees);
        g_texts[0]->SetText(SWPrintf(0x80, f.c_str(), {trees}).c_str());
        g_icons[0]->SetVisibility(true);
        g_texts[0]->SetVisibility(true);
    }
    const int slot = showTrees ? 1 : 0;
    if (showRocks) {
        g_icons[slot]->SetTexture(IconManager::GetIcon("profession_miner"), true);
        std::u32string f = StringTable::GetCountableString(StringTable::GetString("BUY_LAND_ROCK"), rocks);
        g_texts[slot]->SetText(SWPrintf(0x80, f.c_str(), {rocks}).c_str());
        g_icons[slot]->SetVisibility(true);
        g_texts[slot]->SetVisibility(true);
        g_list->SetVisibility(false);
    } else if (others != 0) {
        g_list->SetVisibility(true);
        g_list->SetText(list.c_str());
    } else {
        g_list->SetVisibility(false);
    }
    FillButtons();
    g_goldButton->SetOnClick([] { OnBuyArea(0); });
    g_crystalButton->SetOnClick([] { OnBuyArea(1); });
    g_landImage->SetVisibility(true);
    g_farmImage->SetVisibility(false);
}

// @0x2e82f0
void Show() {
    g_queue->shown = true;
    g_queue->MoveWindowOnTop(true);
    if (!g_root->visibleSelf) {
        WindowManager::WindowShow(false);
        g_tween->AnimateIn();
    }
    g_root->SetVisibility(true);
    g_root->PreloadTextures();   // GUI::PreloadWindow
    GameState::RaiseGamePauseState();
}

// @0x2e8370
void Hide() {
    if (!g_root->visibleSelf) return;
    g_tween->AnimateOut();
}

}  // namespace LandWindow
