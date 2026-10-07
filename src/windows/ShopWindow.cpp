// ShopWindow: the building and decoration shop behind BottomCity's Build button. A strip of cells
// (Main_buildings_container) slides up from the bottom with nine tabs; the info panel
// (Building_info_panel) on the right describes the selected item. Tapping a cell buys it and hands
// it to BuildingPlacement. Port of ShopWindow (libkingdom.so 5.11), 0x346d14..0x34d99c. Statics at
// 0x62d4cc and 0x60f3a4.
#include "windows/Windows.h"

#include <pugixml.hpp>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "engine/FileManager.h"
#include "engine/IconManager.h"
#include "engine/Render.h"
#include "engine/StlSort.h"
#include "game/Building.h"
#include "game/BuildingHovers.h"
#include "game/BuildingPlacement.h"
#include "game/Contracts.h"
#include "game/GameData.h"
#include "game/GameState.h"
#include "game/Map.h"
#include "game/Setting.h"
#include "game/StringTable.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/ContentScroller.h"
#include "hud/HUD.h"
#include "hud/Shared.h"

namespace ShopWindow {
namespace {

using GUI::Textfield;
using GUI::Window;

// One cost line of the info panel (CostPartInfo, 0x10 bytes; LoadFrom @0x34878c).
struct CostPartInfo {
    Window* root = nullptr;             // +0x00 "%s"
    Window* icon = nullptr;             // +0x04 "%s.icon" (takes a depth slot)
    Textfield* textEnough = nullptr;    // +0x08
    Textfield* textNotEnough = nullptr; // +0x0c
    void LoadFrom(Window* w, const char* prefix) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", prefix);
        icon = GUI::GetWindowTypedF<Window>(w, "%s.icon", prefix);
        icon->takesZ = true;
        textEnough = GUI::GetWindowTypedF<Textfield>(w, "%s.text_enough", prefix);
        textNotEnough = GUI::GetWindowTypedF<Textfield>(w, "%s.text_not_enough", prefix);
    }
};

// The cost block (CostInfo; LoadFrom @0x348814).
struct CostInfo {
    Window* root = nullptr;             // +0x00
    Textfield* text = nullptr;          // +0x04 text_cost
    CostPartInfo parts[6];              // +0x08 building_req_icon_01..06
    void LoadFrom(Window* w, const char* prefix) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", prefix);
        text = GUI::GetWindowTypedF<Textfield>(w, "%s.text_cost", prefix);
        char buf[64];
        for (int i = 0; i < 6; ++i) {
            std::snprintf(buf, sizeof buf, "%s.building_req_icon_%02d", prefix, i + 1);
            parts[i].LoadFrom(w, buf);
        }
    }
};

// One shop cell (BuildingInfo, 0x24 bytes): "container_build.box_%02d".
struct BuildingInfo {
    Window* root = nullptr;             // +0x00
    Window* bgDefault = nullptr;        // +0x04 bg_default
    Window* bgCrystal = nullptr;        // +0x08 back_building_crystal_item
    Window* questNew = nullptr;         // +0x0c icon_gold_quest_new (takes a depth slot)
    Window* lock = nullptr;             // +0x10 icon_lock
    Textfield* text = nullptr;          // +0x14
    Textfield* status = nullptr;        // +0x18 text_status
    Window* image = nullptr;            // +0x1c image_holder_idle (takes a depth slot)
    const GameData::BuildingData* data = nullptr;  // +0x20 (building tabs only)
};

WindowManager::FunctionalWindow* g_wnd = nullptr;   // ShopWindow::wnd
Window* g_root = nullptr;                // 0x62d4cc Main_buildings_container
Window* g_info = nullptr;                // 0x62d4d0 Building_info_panel
Textfield* g_name = nullptr;             // 0x62d4d4 name_text
Textfield* g_desc = nullptr;             // 0x62d4d8 text_desc
CostInfo g_cost;                         // 0x62d4dc
Shared::HouseLivingInfo g_living;        // 0x62d544
Shared::HouseTrainingInfo g_training;    // 0x62d554
Shared::HouseConvertingInfo g_converting;   // 0x62d560
Shared::HouseProducingInfo g_producing;  // 0x62d570
Shared::HouseDecorationInfo g_decoration;   // 0x62d5b4
Shared::HouseLivingWorkerInfo g_livingWorker;   // 0x62d5bc
Window* g_lock = nullptr;                // 0x62d5d0 icon_lock
Textfield* g_levelReq = nullptr;         // 0x62d5d4 text_level_req
Textfield* g_reqUpgrade = nullptr;       // 0x62d5d8 text_req_upgrade
Shared::ContentScroller g_scroller;      // 0x62d5dc
int g_infoOffsetX = 0;                   // 0x62d6e8
std::vector<BuildingInfo> g_cells;       // 0x62d6ec
unsigned g_cellCount = 0;                // 0x62d718 5 on small screens, else 6
int g_offsetX = 0;                       // 0x62d71c the strip's x offset (0x38 on small screens, scaled)
GUI::MovementEffect* g_rootEffect = nullptr;   // 0x62d720
GUI::MovementEffect* g_infoEffect = nullptr;   // 0x62d724
Shared::TabHolder g_tabs[9];             // 0x62d728
std::vector<GameData::BuildingData*> g_buildings[10];   // 0x62d86c per tab type
std::vector<GameData::DecorData*> g_decors[10];         // 0x62d8e4 per tab type
unsigned g_tab = 0;                      // 0x62d95c the current tab type
Window* g_close = nullptr;               // 0x62d960 button_close_building_mode
unsigned g_selected = 0;                 // 0x62d964 the selected cell
int g_arrowCallback = 0;                 // 0x62d968
Window* g_left = nullptr;                // 0x62d96c container_build.button_left
Window* g_right = nullptr;               // 0x62d970 container_build.button_right
bool g_initing = false;                  // 0x62d974
bool g_small = false;                    // 0x62d975
Window* g_backLight = nullptr;           // 0x62d978 container_build.back_light_blue
Window* g_closeArea = nullptr;           // 0x62d97c
int g_marginY = 10;                      // 0x60f3a4 (scaled by Init)
int g_infoMarginY = 2;                   // 0x60f3a8 (scaled by Init)
int g_arrowItem = -1;                    // 0x60f3ac the cell the helper arrow points at
bool g_showBarsOnHide = true;            // 0x60f3b0 cleared by a purchase
float g_scale = 1.f;                     // 0x60f3b4 the strip's layout scale

bool IsDecorTab(unsigned t) { return t == 4 || t == 7 || t == 8 || t == 9; }

// @0x346d3c: tab 5 (type 5, land patches) is skipped.
unsigned TabIndexToType(unsigned i) {
    static const unsigned kTypes[9] = {0, 1, 2, 3, 4, 6, 7, 8, 9};
    return kTypes[i];
}

// @0x34bc64: types 8 and 9 hold decorations when roads are enabled.
unsigned GetTabItemCount(unsigned t) {
    if (t == 5) return 0;
    if (t == 4 || t == 7 || (t - 8 < 2 && Setting("roads_enabled").GetInt() == 1))
        return (unsigned)g_decors[t].size();
    return (unsigned)g_buildings[t].size();
}

int RootX() { return g_offsetX + (GUI::ScreenWidth() - g_root->w) / 2; }

// @0x346e30 UpdateClip / @0x346e84 OnTweenMove
void UpdateClip() {
    for (unsigned i = 0; i < g_cellCount; ++i) g_cells[i].root->UpdatePosition();
}
void OnTweenMove() { UpdateClip(); }

void FillBuildings();
void Update(float dt);

// @0x34d53c
void OnTweenIn() {
    FillBuildings();
    Render::SortRenderLayer(Render::kLayerGUI, 1);
}

// @0x346e08
void OnTweenOut() { g_root->SetVisibility(false); }

// @0x3473c4
void OnTweenOutInfo() {
    g_info->SetVisibility(false);
    g_close->SetVisibility(true);
    WindowManager::WindowHide(false);
    g_wnd->shown = false;
    g_wnd->MoveWindowDown(true);
    // UNVERIFIED (milestone 4): BattleBarWindow::ShowAllTasks (the quest lines).
    BuildingHovers::SetHoverVisiblity(true, true);
}

// @0x346d84: the arrow buttons scroll one cell.
void OnCellChange(unsigned dir) {
    if (dir == 0) g_scroller.bounce += (float)g_scroller.itemSize;
    else g_scroller.bounce -= (float)g_scroller.itemSize;
}

// @0x3472d8
void LoadDecorationRequirements(unsigned i) {
    const GameData::DecorData* d = g_decors[g_tab][i];
    NotEnoughWindow::ResetRequirements();
    NotEnoughWindow::AddLevelRequirement((unsigned)d->needLevel);
    NotEnoughWindow::AddRequirement(GameState::kGold, (unsigned)d->cost1);
    NotEnoughWindow::AddRequirement(GameState::kCrystal, (unsigned)GameState::AdjustCrystalCost(d->cost2));
}

// @0x34732c
void LoadBuildingRequirements(unsigned i) {
    const GameData::BuildingData* d = g_buildings[g_tab][i];
    NotEnoughWindow::ResetRequirements();
    NotEnoughWindow::AddLevelRequirement(d->level);
    NotEnoughWindow::AddPopulaionRequirement(d->costPopulation);
    for (int r = 0; r < 11; ++r) NotEnoughWindow::AddRequirement(r, (unsigned)d->cost[r]);
    NotEnoughWindow::AddRequirement(GameState::kGold, d->gold);
    NotEnoughWindow::AddRequirement(GameState::kCrystal, (unsigned)GameState::AdjustCrystalCost((int)d->cb));
    if (d->requiredId != 0)
        NotEnoughWindow::AddBuildingLevelRequirement((unsigned)d->requiredId, (unsigned)d->requiredCount);
    NotEnoughWindow::SetExchangeLimit(0);   // BuildingData +0xb4, never set by the loader
}

bool AtMax(const GameData::BuildingData* d) {
    int max = GameData::GetMaxBuildingCount(*d);
    return max != 0 && (unsigned)max <= (unsigned)Map::GetBuildingCount(d->id, false);
}

bool QuestDone(const GameData::BuildingData* d) {
    return d->requiresQuest == 0 || GameState::TaskCompleted((unsigned)d->requiresQuest);
}

// @0x34762c
void SetInfoLine(unsigned i, Render::Texture* icon, const char32_t* text, bool enough) {
    if (i >= 6) {
        std::puts("SetInfoLine() Too many requirements");
        return;
    }
    CostPartInfo& p = g_cost.parts[i];
    p.root->SetVisibility(true);
    p.icon->SetTexture(icon, false, 0, 0, false, 0);
    p.icon->SetEnabled(enough);
    p.textEnough->SetText(text);
    p.textNotEnough->SetText(text);
    p.textEnough->SetVisibility(enough);
    p.textNotEnough->SetVisibility(!enough);
}

Render::Texture* ResourceIcon(int type) {
    return IconManager::GetIcon(GameState::GetResourceMapIconName(type), false);
}

void SetIcon(Window* w, Render::Texture* tex) { w->SetTexture(tex, true, 0, 0, false, 0); }

void ShowCostLines(int gold, int crystals, unsigned& line) {
    if (gold != 0) {
        SetInfoLine(line++, ResourceIcon(GameState::kGold), ToWideString(gold),
                    gold <= (int)GameState::GetResourceAmount(GameState::kGold));
    }
    if (crystals != 0) {
        int c = GameState::AdjustCrystalCost(crystals);
        SetInfoLine(line++, ResourceIcon(GameState::kCrystal), ToWideString(c),
                    GameState::AdjustCrystalCost(crystals) <= (int)GameState::GetResourceAmount(GameState::kCrystal));
    }
}

// The purpose line of a building (the class-specific part of OnItemInfo).
void ShowPurpose(const GameData::BuildingData* d) {
    const unsigned cls = d->buildingClass;
    if (cls == 0 && d->givePopulation == 0) {
        g_living.root->SetVisibility(true);
        g_living.time->SetText(StringTable::GetTimeString(d->collectTime, false).c_str());
        SetIcon(g_living.icon, ResourceIcon(GameState::kGold));
        g_living.tax->SetText(ToWideString(d->collectMoney));
        return;
    }
    if (cls == 0xc || cls == 7) {
        g_training.root->SetVisibility(true);
        const char* text;
        const char* icon;
        switch (d->id) {
            case 0x12: text = "BUILDING_HIRE_GOBLINS"; icon = "goblin_60"; break;
            case 0x8e: text = "BUILDING_HIRE_WARRIORS"; icon = "army_warrior_60"; break;
            case 0x69: text = "BUILDING_HIRE_ARCHERS"; icon = "army_archer_60"; break;
            case 0x93: text = "BUILDING_HIRE_WIZARDS"; icon = "army_mage_60"; break;
            default: return;
        }
        g_training.text->SetText(StringTable::GetString(text));
        SetIcon(g_training.icon, IconManager::GetIcon(icon, false));
        return;
    }
    const Contracts::Contract* c = d->delivery;
    bool producing = (cls == 2 || cls == 0xd) && c;
    if (cls == 2 && c && !c->missions[0].item) {
        g_converting.root->SetVisibility(true);
        g_converting.text->SetText(StringTable::GetString("BUILDING_CONVERING"));
        SetIcon(g_converting.from, ResourceIcon(c->missions[0].priceResource));
        SetIcon(g_converting.to, ResourceIcon(c->missions[0].rewardResource));
        return;
    }
    if (producing) {
        g_producing.root->SetVisibility(true);
        g_producing.text->SetText(StringTable::GetString("BUILDING_PRODUCING"));
        for (unsigned i = 0; i < 5; ++i) {
            if (i >= c->missions.size()) continue;
            const Contracts::ContractMission& m = c->missions[i];
            // UNVERIFIED (Items): an item mission puts the item's image in the holder
            // (Items::ItemInfo::PutImageInHolder); items are not ported, so .item is always null.
            if (!m.item && !m.icon.empty()) SetIcon(g_producing.icons[i], IconManager::GetIcon(m.icon.c_str(), false));
        }
        for (Window* w : g_producing.confirms) w->SetVisibility(false);
        for (Window* w : g_producing.locks) w->SetVisibility(false);
        return;
    }
    if (cls == 0) {
        g_livingWorker.root->SetVisibility(true);
        g_livingWorker.time->SetText(StringTable::GetTimeString(d->collectTime, false).c_str());
        SetIcon(g_livingWorker.icon, ResourceIcon(GameState::kGold));
        g_livingWorker.tax->SetText(ToWideString((int)d->givePopulation * 5 + d->collectMoney));
        g_livingWorker.workers->SetText(ToWideString((int)d->givePopulation));
    }
}

// @0x34900c: fills the info panel for cell i (the second argument is unused).
void OnItemInfo(unsigned i, bool) {
    g_cost.root->SetVisibility(false);
    for (CostPartInfo& p : g_cost.parts) p.root->SetVisibility(false);
    g_cost.text->SetText(StringTable::GetString("HIRE_TROOPS_COST"));
    g_living.root->SetVisibility(false);
    g_training.root->SetVisibility(false);
    g_converting.root->SetVisibility(false);
    g_producing.root->SetVisibility(false);
    g_decoration.root->SetVisibility(false);
    g_livingWorker.root->SetVisibility(false);
    g_reqUpgrade->SetVisibility(false);
    char key[0x80];
    int needLevel;
    if (g_tab == 5) return;   // land patches (FillPatches): never shown, GetTabItemCount(5) is 0
    if (IsDecorTab(g_tab)) {
        const GameData::DecorData* d = g_decors[g_tab][i + g_scroller.firstIndex];
        g_name->SetText(StringTable::GetString(d->name.c_str()));
        std::snprintf(key, sizeof key, "%s_DESC_B", d->name.c_str());
        g_desc->SetText(StringTable::GetString(key));
        g_decoration.root->SetVisibility(true);
        g_decoration.text->SetText(StringTable::GetString("BUILDING_DECORATION"));
        g_cost.root->SetVisibility(true);
        unsigned line = 0;
        ShowCostLines(d->cost1, d->cost2, line);
        needLevel = d->needLevel;
        g_lock->SetVisibility(GameState::GetLevel() < needLevel);
        g_levelReq->SetVisibility(GameState::GetLevel() < needLevel);
    } else {
        const unsigned index = i + g_scroller.firstIndex;
        if (index >= g_buildings[g_tab].size()) return;
        const GameData::BuildingData* d = g_buildings[g_tab][index];
        g_name->SetText(StringTable::GetString(d->name.c_str()));
        std::snprintf(key, sizeof key, "%s_DESC_B", d->name.c_str());
        g_desc->SetText(StringTable::GetString(key));
        ShowPurpose(d);
        if (d->requiredId != 0) {
            const GameData::BuildingData* req = GameData::GetBuilding((uint32_t)d->requiredId);
            bool met = Map::GetBuildingCount(req->id, false) != 0 &&
                       (unsigned)d->requiredCount <= (unsigned)Map::GetBuildingMaxUpgrade((uint32_t)d->requiredId);
            // The original also formats W_NEED_STUFF with the level here and never uses the result.
            if (!met) {
                g_reqUpgrade->SetVisibility(true);
                g_reqUpgrade->SetText(SWPrintf(0x100, StringTable::GetString("NEED_BUILDING_LEVEL"),
                                               {StringTable::GetString(req->name.c_str()),
                                                ToWideString(d->requiredCount)})
                                          .c_str());
            }
        }
        if (!g_reqUpgrade->visibleSelf) {
            g_cost.root->SetVisibility(true);
            unsigned line = 0;
            ShowCostLines((int)d->gold, (int)d->cb, line);
            for (int r = 0; r < 11; ++r) {
                if (d->cost[r] == 0) continue;
                SetInfoLine(line++, ResourceIcon(r), ToWideString(d->cost[r]),
                            d->cost[r] <= (int)GameState::GetResourceAmount(r));
            }
            if ((unsigned)(GameState::GetPlayerWorkersCount() - Map::GetUsedWorkerCount()) < d->costPopulation)
                SetInfoLine(line, IconManager::GetIcon("b_houses", false), ToWideString((int)d->costPopulation), true);
        }
        needLevel = (int)d->level;
        g_lock->SetVisibility((unsigned)GameState::GetLevel() < d->level);
        g_levelReq->SetVisibility((unsigned)GameState::GetLevel() < d->level);
    }
    g_levelReq->SetText(ToWideString(needLevel));
}

void OnItemSelect(unsigned i);

GUI::Callback SelectCallback(unsigned i) {
    return [i] { OnItemSelect(i); };
}

// @0x34748c: the "not enough" dialog for the selected cell.
void OnShowRequirements() {
    if (g_tab == 5) return;
    const unsigned index = g_selected + g_scroller.firstIndex;
    if (IsDecorTab(g_tab)) {
        LoadDecorationRequirements(index);
    } else {
        const GameData::BuildingData* d = g_buildings[g_tab][index];
        if (AtMax(d) || !QuestDone(d)) return;
        LoadBuildingRequirements(index);
    }
    if (NotEnoughWindow::CheckRequirements()) return;
    NotEnoughWindow::SetDescriptionText(StringTable::GetString("REQUIREMENT_BUILD"), nullptr);
    NotEnoughWindow::SetActionCallback(SelectCallback(g_selected), StringTable::GetString("PERFORM_BUY"), false);
    NotEnoughWindow::Show();
}

void PopupText(const std::u32string& text) {
    PopupWindow::Show(text.c_str(), PopupWindow::Hide, nullptr, nullptr);
}

// @0x347c4c: buy the selected cell's item and start placing it.
void OnBuy() {
    NotEnoughWindow::Hide();
    // Tab 5 (land patches) never shows, so its branch (centre on the area, arrow at it) is not ported.
    if (g_tab == 5) return;
    const unsigned index = g_selected + g_scroller.firstIndex;
    Render::Sprite* icon = g_cells[g_selected].image->sprite;
    if (IsDecorTab(g_tab)) {
        LoadDecorationRequirements(index);
        if (!NotEnoughWindow::CheckRequirements()) {
            OnShowRequirements();
            return;
        }
        auto* decor = new Map::Decor();
        g_showBarsOnHide = false;
        decor->id = g_decors[g_tab][index]->id;
        Hide();
        BuildingPlacement::DecorBought(decor, icon, false);
        return;
    }
    GameData::BuildingData* d = g_buildings[g_tab][index];
    if (AtMax(d)) return;
    if (!QuestDone(d)) {
        // UNVERIFIED (milestone 4): with Tasks::GetTask (the first locked task of its chain) the
        // text is HIRE_TROOPS_TASK_LOCK; Tasks are not ported, so it is always SHOP_UNLOCK_TASK.
        PopupText(SWPrintf(0x100, StringTable::GetString("SHOP_UNLOCK_TASK")));
        return;
    }
    if ((unsigned)GameState::GetMaxWorkerCount() <
        (unsigned)(GameState::GetPlayerWorkersCount() + Map::GetPendingWorkerCount() + (int)d->givePopulation)) {
        PopupText(SWPrintf(0x100, StringTable::GetString("PEOPLE_LIMIT"), {ToWideString(GameState::GetMaxWorkerCount())}));
        return;
    }
    LoadBuildingRequirements(index);
    if (!NotEnoughWindow::CheckRequirements()) {
        OnShowRequirements();
        return;
    }
    if (GameState::tutorial == 0x5b) {
        GUI::SetInteractionLock(false);
        BuildingHovers::HideArrow();
        GameState::tutorial = 0x5c;
    }
    if (g_arrowItem != -1) {
        g_arrowItem = -1;
        BuildingHovers::HideArrow();
        BuildingHovers::SetArrowVisibleWindowLimit(0);
    }
    auto* b = new Map::Building();
    b->baseX = b->maxY = 0.f;
    b->data = d;
    b->id = d->id;
    b->SetOpened();
    g_showBarsOnHide = false;
    Hide();
    BuildingPlacement::BuildingBought(b, icon, false);
    delete b;
}

// @0x34a31c: a tap on a cell buys it straight away.
void OnItemSelect(unsigned i) {
    if (BuildingPlacement::Activated()) BuildingPlacement::Decline();
    g_selected = i;
    OnItemInfo(i, true);
    OnBuy();
}

}  // namespace

// @0x34d570
void OnTabSelect(unsigned i) {
    const unsigned type = TabIndexToType(i);
    if (g_tab != type) {
        g_scroller.firstIndex = 0;
        g_scroller.HandleMove(-g_scroller.offset);
        g_selected = 0;
    }
    g_tab = type;
    for (unsigned t = 0; t < 9; ++t) g_tabs[t].SetMode(TabIndexToType(t) == g_tab, false);
    for (unsigned c = 0; c < g_cellCount; ++c) g_cells[c].image->SetTexture(nullptr, false, 0, 0, false, 0);
    OnItemInfo(0, false);
    FillBuildings();
}

namespace {

// @0x34bdcc
void FillDecorations() {
    const unsigned count = GetTabItemCount(g_tab) - g_scroller.firstIndex;
    const unsigned shown = count < g_cellCount ? count : g_cellCount;
    for (unsigned c = 0; c < g_cellCount; ++c) g_cells[c].root->SetVisibility(false);
    for (unsigned k = 0; k < shown; ++k) {
        BuildingInfo& cell = g_cells[k];
        cell.root->SetVisibility(true);
        const GameData::DecorData* d = g_decors[g_tab][k + g_scroller.firstIndex];
        cell.text->SetText(StringTable::GetString(d->name.c_str()));
        cell.questNew->SetVisibility(false);
        const bool crystal = d->cost2 != 0 && d->cost1 == 0;
        cell.bgDefault->SetVisibility(!crystal);
        cell.bgCrystal->SetVisibility(crystal);
        GUI::FitImageIntoWindow(cell.image, GameData::DecorImage(d), true, true);
        LoadDecorationRequirements(k + g_scroller.firstIndex);
        bool enabled;
        if (GameState::GetLevel() < d->needLevel) {
            cell.lock->SetVisibility(true);
            std::u32string s = SWPrintf(0x20, StringTable::GetString("SHOP_UNLOCK"), {ToWideString(d->needLevel)});
            cell.status->SetVisibility(true);
            cell.status->SetText(s.c_str());
            enabled = false;
        } else if (!NotEnoughWindow::CheckRequirements()) {
            cell.lock->SetVisibility(false);
            cell.status->SetVisibility(true);
            cell.status->SetText(StringTable::GetString("NO_RESOURCES"));
            enabled = false;
        } else {
            cell.lock->SetVisibility(false);
            cell.status->SetVisibility(false);
            enabled = true;
        }
        cell.image->SetEnabled(enabled);
    }
    Render::SortRenderLayer(Render::kLayerGUI, 1);
}

// @0x34c958
void FillBuildings() {
    const unsigned total = GetTabItemCount(g_tab);
    g_scroller.itemCount = total;
    if (g_tab == 5) {   // FillPatches: unreachable (no tab has type 5)
        UpdateClip();
        return;
    }
    if (IsDecorTab(g_tab)) {
        FillDecorations();
        UpdateClip();
        return;
    }
    const unsigned count = total - g_scroller.firstIndex;
    const unsigned shown = count < g_cellCount ? count : g_cellCount;
    for (unsigned c = 0; c < g_cellCount; ++c) {
        g_cells[c].root->SetVisibility(false);
        g_cells[c].data = nullptr;
    }
    for (unsigned k = 0; k < shown; ++k) {
        BuildingInfo& cell = g_cells[k];
        cell.root->SetVisibility(true);
        const GameData::BuildingData* d = g_buildings[g_tab][k + g_scroller.firstIndex];
        cell.data = d;
        cell.text->SetText(StringTable::GetString(d->name.c_str()));
        cell.questNew->SetVisibility(false);
        cell.bgDefault->SetVisibility(d->cb == 0);   // (1 - cb when cb <= 1, else 0)
        cell.bgCrystal->SetVisibility(d->cb != 0);
        const unsigned built = (unsigned)Map::GetBuildingCount(d->id, false);
        const unsigned max = (unsigned)GameData::GetMaxBuildingCount(*d);
        const bool atMax = max != 0 && max <= built;
        LoadBuildingRequirements(k + g_scroller.firstIndex);
        bool enabled = false;
        if (atMax) {
            cell.lock->SetVisibility(false);
            cell.status->SetVisibility(true);
            cell.status->SetText(SWPrintf(0x20, U"%d/%d", {built, max}).c_str());
        } else if ((unsigned)GameState::GetLevel() < d->level) {
            cell.lock->SetVisibility(true);
            std::u32string s = SWPrintf(0x20, StringTable::GetString("SHOP_UNLOCK"), {ToWideString((int)d->level)});
            cell.status->SetVisibility(true);
            cell.status->SetText(s.c_str());
        } else if (!NotEnoughWindow::CheckRequirements()) {
            cell.lock->SetVisibility(false);
            cell.status->SetVisibility(true);
            cell.status->SetText(StringTable::GetString("NO_RESOURCES"));
        } else if (QuestDone(d)) {
            cell.lock->SetVisibility(false);
            if (max < 0x400) {
                cell.status->SetVisibility(true);
                cell.status->SetText(SWPrintf(0x20, U"%d/%d", {built, max}).c_str());
            } else {
                cell.status->SetVisibility(false);
            }
            enabled = true;
        } else {
            cell.lock->SetVisibility(true);
            // UNVERIFIED (milestone 4): with Tasks::GetTask(requiresQuest) the status shows
            // SHOP_UNLOCK_TASK; Tasks are not ported, so the status is left as it was.
        }
        cell.image->SetEnabled(enabled);
        GUI::FitImageIntoWindow(cell.image, GameData::BuildingImage(d, 0), true, true);
    }
    UpdateClip();
    Render::SortRenderLayer(Render::kLayerGUI, 1);
}

// @0x34d554
void UpdateContents() {
    if (g_root->visibleSelf) FillBuildings();
}

// The helper arrow over a cell (screen space, at the top centre of the cell).
void ArrowAtCell(const BuildingInfo& cell, unsigned index) {
    if (BuildingHovers::GetArrowClickCallbackID() != g_arrowCallback)
        g_arrowCallback = BuildingHovers::SetArrowClickCallback(new GUI::Callback(SelectCallback(index)));
    BuildingHovers::ArrowAt((float)(g_root->x + cell.root->x + cell.root->w / 2), (float)(g_root->y + cell.root->y),
                            false, false, false, false, true, false, false);
}

// @0x34776c
void Update(float dt) {
    if (g_root->visibleSelf && GameState::tutorial == 0x5b) {
        GUI::SetInteractionObjectLock(g_cells[0].root, nullptr);
        ArrowAtCell(g_cells[0], 0);
    }
    if (g_arrowItem != -1) {
        BuildingHovers::SetArrowVisibleWindowLimit(1);
        ArrowAtCell(g_cells[g_arrowItem], (unsigned)g_arrowItem);
    }
    if (g_root->visibleSelf) {
        g_scroller.Update(dt);
        g_left->SetEnabled(g_scroller.CanMoveLeft());
        g_right->SetEnabled(g_scroller.CanMoveRight());
    }
}

// @0x346e88
void SetZ(float z) {
    if (!g_wnd->shown) return;
    const int H = GUI::ScreenHeight();
    g_root->SetZ(z);
    g_root->SetPosition(RootX(), (g_rootEffect->hideAtEnd ? H : H - g_root->h) + g_marginY);
    g_info->SetZ(z - 0.001f);
    g_info->SetPosition(GetInfoPanelX(), (g_infoEffect->hideAtEnd ? H : H - g_info->h) + g_infoMarginY);
    UpdateClip();
}

// @0x34862c
bool Click(int x, int y, bool pressed) {
    if (BuildingHovers::ClickOnArrow(x, y, pressed)) return true;
    if (g_arrowItem != -1 && !pressed) {
        g_arrowItem = -1;
        BuildingHovers::HideArrow();
    }
    if (g_root->visibleSelf && GameState::tutorial != 0x5b && g_scroller.Click(x, y, pressed, g_root)) return true;
    if (g_info->Click(x, y, pressed, false) || g_root->Click(x, y, pressed, false)) return true;
    if (g_root->visibleSelf && GameState::SecondTutorialStep() == 0x100 && !BuildingPlacement::Activated()) {
        Hide();
        return true;
    }
    return false;
}

// @0x347750
void Move(int x, int y) { g_scroller.Move(x, y); }

// @0x347bec
bool OnBack() {
    if (GameState::IsCityTutorial() || !g_wnd->shown) return false;
    if (!BuildingPlacement::Activated()) {
        Hide();
        return true;
    }
    if (BuildingPlacement::PlacementControlsVisible()) {
        BuildingPlacement::Decline();
        return true;
    }
    return false;
}

// Init: the tab icons from shoptabs.xml (Xtab id -> tab index, icon name -> UI icon).
void LoadTabIcons() {
    static const unsigned kTabIndex[11] = {0, 1, 2, 3, 4, 10, 5, 6, 7, 8, 9};
    // PORT: the sandbox server's res_files_sandbox copy is not used offline.
    const char* file = "../resource/res_files/1Original/shoptabs.xml";
    uint32_t size = 0;
    uint8_t* buf = FileManager::LoadFile(file, size);
    if (!buf) {
        std::puts("ShopWindow::Init() Failed to load shoptabs list (File not found)");
        return;
    }
    pugi::xml_document doc;
    pugi::xml_parse_result r = doc.load_buffer_inplace(buf, size, 0x74);
    if (!r) std::printf("ShopWindow::Init() Failed to load shoptabs list (%s)\n", r.description());
    for (pugi::xml_node t = doc.child("XShopTabs").child("Xtab"); t; t = t.next_sibling("Xtab")) {
        unsigned id = t.attribute("id").as_uint();
        if (id >= 11 || kTabIndex[id] >= 9) continue;
        const char* v = t.attribute("icon").value();
        const char* icon = !std::strcmp(v, "BuildUI/Production") ? "Icon_b_production"
                         : !std::strcmp(v, "BuildUI/Farm")       ? "Icon_b_farm"
                         : !std::strcmp(v, "BuildUI/Master")     ? "Icon_b_masters"
                         : !std::strcmp(v, "BuildUI/Decor")      ? "Icon_b_decor"
                         : !std::strcmp(v, "BuildUI/Expand")     ? "Icon_b_expand"
                         : !std::strcmp(v, "BuildUI/War")        ? "Icon_b_war"
                         : !std::strcmp(v, "BuildUI/Artist")     ? "Icon_b_artists"
                                                                 : "b_houses";
        g_tabs[kTabIndex[id]].icon->SetTexture(IconManager::GetIcon(icon, false), false, 0, 0, false, 0);
    }
    FileManager::FreeFile(buf);
}

// Init: the shop lists, each sorted three times by level (as the original does).
void BuildLists() {
    int visid = 500;
    for (unsigned i = 0; GameData::BuildingData* b = GameData::EnumBuildings(i); ++i) {
        if ((unsigned)b->tab > 9 || !b->buy) continue;
        g_buildings[b->tab].push_back(b);
        if (b->visid == 0) b->visid = visid++;
    }
    for (auto& list : g_buildings)
        for (int pass = 0; pass < 3; ++pass)
            StlSort::Sort(list.data(), list.data() + list.size(),
                          [](const GameData::BuildingData* a, const GameData::BuildingData* b) { return a->level < b->level; });
    visid = 500;
    for (unsigned i = 0; GameData::DecorData* d = GameData::EnumDecors(i); ++i) {
        if ((unsigned)d->tab > 9 || !d->buy) continue;
        g_decors[d->tab].push_back(d);
        if (d->visid == 0) d->visid = visid++;
    }
    for (auto& list : g_decors)
        for (int pass = 0; pass < 3; ++pass)
            StlSort::Sort(list.data(), list.data() + list.size(),
                          [](const GameData::DecorData* a, const GameData::DecorData* b) { return a->needLevel < b->needLevel; });
}

}  // namespace

// Static FunctionalWindow("ShopWindow", Init, Deinit, Click, SetZ, Hide, Move) @0x346ff4
WindowManager::FunctionalWindow* Queue() {
    if (!g_wnd) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.deinit = Deinit;
        f.click = Click;
        f.setZ = SetZ;
        f.hide = Hide;
        f.move = Move;
        g_wnd = new WindowManager::FunctionalWindow("ShopWindow", std::move(f));
    }
    return g_wnd;
}

// @0x346d14
bool IsVisible() { return g_root->visibleSelf || g_info->visibleSelf; }

// @0x346dd4
int GetInfoPanelX() { return g_infoOffsetX + GUI::ScreenWidth() - g_info->w; }

// @0x34a358
void Init() {
    g_initing = true;
    g_infoMarginY = 2;
    g_infoOffsetX = 0;
    g_marginY = 10;
    g_offsetX = 0;
    const int W = GUI::ScreenWidth();
    g_small = GUI::IsHighDPIVersion() ? W < 1300 : W < 1000;
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Main_buildings_container.xml",
                             "Main_buildings_container.png", -1.f, 0, 0, g_small ? -0x92 : -0x22, 0, false, 1.f);
    g_wnd->RegisterTopWindow(g_root);
    g_scale = g_root->scale;
    g_root->SetVisibility(false);
    g_root->SetPosition((W - g_root->w) / 2, GUI::ScreenHeight() - g_root->h);
    if (g_small) g_offsetX = 0x38;
    g_backLight = GUI::GetWindowTyped<Window>(g_root, "container_build.back_light_blue");
    g_left = GUI::GetWindowTyped<Window>(g_root, "container_build.button_left");
    g_left->SetOnClick([] { OnCellChange(0); });
    g_left->SetActionSound("ui_click", 1);
    g_right = GUI::GetWindowTyped<Window>(g_root, "container_build.button_right");
    g_right->SetOnClick([] { OnCellChange(1); });
    g_right->SetActionSound("ui_click", 1);
    char name[64];
    for (unsigned i = 0; i < 9; ++i) {
        std::snprintf(name, sizeof name, "container_build.tab_%02d", i + 1);
        g_tabs[i].LoadFrom(g_root, name, true);
        g_tabs[i].root->SetOnClick([i] { OnTabSelect(i); });
        g_tabs[i].root->SetActionSound("ui_tab", 1);
    }
    LoadTabIcons();
    g_cellCount = g_small ? 5 : 6;
    g_cells.assign(g_cellCount, BuildingInfo());
    for (unsigned i = 0; i < g_cellCount; ++i) {
        BuildingInfo& c = g_cells[i];
        const unsigned n = i + 1;
        c.root = GUI::GetWindowTypedF<Window>(g_root, "container_build.box_%02d", n);
        c.root->SetOnClick(SelectCallback(i));
        c.bgDefault = GUI::GetWindowTypedF<Window>(g_root, "container_build.box_%02d.bg_default", n);
        c.bgCrystal = GUI::GetWindowTypedF<Window>(g_root, "container_build.box_%02d.back_building_crystal_item", n);
        c.questNew = GUI::GetWindowTypedF<Window>(g_root, "container_build.box_%02d.icon_gold_quest_new", n);
        c.questNew->takesZ = true;
        c.lock = GUI::GetWindowTypedF<Window>(g_root, "container_build.box_%02d.icon_lock", n);
        c.text = GUI::GetWindowTypedF<Textfield>(g_root, "container_build.box_%02d.text", n);
        c.status = GUI::GetWindowTypedF<Textfield>(g_root, "container_build.box_%02d.text_status", n);
        c.status->MoveWindow(0, -3);
        c.image = GUI::GetWindowTypedF<Window>(g_root, "container_build.box_%02d.image_holder_idle", n);
        c.data = nullptr;
        c.image->takesZ = true;
    }
    for (unsigned n = g_cellCount + 1; n <= 6; ++n)
        GUI::GetWindowTypedF<Window>(g_root, "container_build.box_%02d", n)->SetVisibility(false);

    g_info = GUI::RegisterUI("../resource/kingdom_ui/1Original/Building_info_panel.xml", "Building_info_panel.png",
                             g_scale, 0, 0, 0, 0, false, 1.f);
    g_wnd->RegisterTopWindow(g_info);
    g_info->SetVisibility(false);
    g_name = GUI::GetWindowTyped<Textfield>(g_info, "name_text");
    g_desc = GUI::GetWindowTyped<Textfield>(g_info, "text_desc");
    g_cost.LoadFrom(g_info, "building_menu_cost_holder");
    g_living.LoadFrom(g_info, "building_purpose_holder.purpose_living_house");
    g_training.LoadFrom(g_info, "building_purpose_holder.purpose_hire");
    g_converting.LoadFrom(g_info, "building_purpose_holder.purpose_converting");
    g_producing.LoadFrom(g_info, "building_purpose_holder.purpose_producing");
    g_decoration.LoadFrom(g_info, "building_purpose_holder.purpose_decoration");
    g_livingWorker.LoadFrom(g_info, "building_purpose_holder.purpose_living_house_worker");
    g_lock = GUI::GetWindowTyped<Window>(g_info, "icon_lock");
    g_levelReq = GUI::GetWindowTyped<Textfield>(g_info, "text_level_req");
    g_reqUpgrade = GUI::GetWindowTyped<Textfield>(g_info, "text_req_upgrade");
    g_close = GUI::GetWindowTyped<Window>(g_info, "button_close_building_mode");
    g_closeArea = GUI::GetWindowTyped<GUI::Button>(g_info, "button_close_building_mode.clickArea");
    g_closeArea->SetOnClick(Hide);
    g_closeArea->SetActionSound("ui_swing_out", 1);
    g_close->MoveWindow(-(int)(g_scale * 24.f), 0);
    for (unsigned n = 7; n <= 9; ++n)
        GUI::GetWindowTypedF<Window>(g_root, "container_build.box_%02d", n)->SetVisibility(false);

    BuildLists();
    g_selected = 0;
    g_tab = 0;
    g_initing = false;
    g_rootEffect = GUI::CreateMovementEffect(g_root, OnTweenMove, OnTweenIn, OnTweenOut);
    g_infoEffect = GUI::CreateMovementEffect(g_info, nullptr, nullptr, OnTweenOutInfo);

    g_scroller.Init();
    Window* mask = GUI::GetWindowTyped<Window>(g_root, "container_build.building_menu_mask");
    g_scroller.viewport = {mask->x, mask->y, mask->x + mask->w, mask->y + mask->h};
    g_scroller.track = mask;
    for (unsigned i = 0; i < g_cellCount; ++i) {
        g_cells[i].root->SetClipRect(&g_scroller.viewport);
        g_scroller.windows.push_back(g_cells[i].root);
    }
    g_scroller.perLine = 1;
    g_scroller.horizontal = true;
    g_scroller.itemSize = g_cells[1].root->x - g_cells[0].root->x;
    g_scroller.unkA0 = (int)g_cellCount;
    g_scroller.onScroll = UpdateContents;
    g_scroller.padEnd = (int)(g_scale * 10.f);
    g_backLight->SetSize(
        (unsigned)(g_backLight->w - (int)((float)g_scroller.itemSize * (g_small ? 4.275f : 3.275f))),
        (unsigned)g_backLight->h);
    if (g_small) {
        g_right->MoveWindow(-g_scroller.itemSize, 0);
        g_scroller.viewport.right -= g_scroller.itemSize;
    }
    g_wnd->fn.show = Show;
    g_wnd->fn.update = Update;
    g_wnd->fn.zRange = WindowManager::ReturnMediumZRange;
    g_wnd->fn.back = OnBack;
    g_offsetX = (int)((float)g_offsetX * g_scale);
    g_marginY = (int)((float)g_marginY * g_scale);
    g_infoMarginY = (int)((float)g_infoMarginY * g_scale);
}

// @0x348f30
void Deinit() {
    delete g_root;
    g_root = nullptr;
    delete g_info;
    g_info = nullptr;
    GUI::RemoveMovementEffect(g_rootEffect);
    g_rootEffect = nullptr;
    GUI::RemoveMovementEffect(g_infoEffect);
    g_infoEffect = nullptr;
    for (auto& l : g_buildings) l.clear();
    for (auto& l : g_decors) l.clear();
}

// @0x34d73c
void Show() {
    if (Map::GetCurrentFarm()) HUDWindow::ExitFarm();
    if (!g_wnd->shown) WindowManager::WindowShow(false);
    // UNVERIFIED (milestone 4): BattleBarWindow::HideAllTasks (the quest lines).
    BuildingHovers::SetHoverVisiblity(false, false);
    g_wnd->shown = true;
    g_wnd->MoveWindowOnTop(true);
    if (!g_root->visibleSelf) {
        const int W = GUI::ScreenWidth(), H = GUI::ScreenHeight();
        const int x = RootX();
        g_rootEffect->Animate(x, H, x, H + g_marginY - g_root->h, true);
        if (!g_info->visibleSelf) {
            const int ix = g_infoOffsetX + W - g_info->w;
            g_infoEffect->Animate(ix, H, ix, H + g_infoMarginY - g_info->h, true);
        }
        // HUDWindow::ShowMainUI(false) is a no-op.
        BattleBarWindow::Hide();
        BeltBarWindow::Hide();
        BottomCityWindow::Hide();
    }
    g_root->SetVisibility(true);
    g_info->SetVisibility(true);
    g_close->SetVisibility(true);
    for (unsigned i = 0; i < 9; ++i) {
        g_tabs[i].SetMode(TabIndexToType(i) == g_tab, false);
        g_tabs[i].root->SetVisibility(GetTabItemCount(TabIndexToType(i)) != 0);
    }
    FillBuildings();
    Update(0.f);
    OnItemInfo(g_selected, false);
    g_root->PreloadTextures();   // GUI::PreloadWindow
    // SoundsManager::PlaySound("ui_swing_in", 1, false): sounds are milestone 5.
}

// @0x3479b8
void Hide() {
    const int W = GUI::ScreenWidth(), H = GUI::ScreenHeight();
    if (!g_root->visibleSelf) {
        if (g_info->visibleSelf) {
            const int ix = g_infoOffsetX + W - g_info->w;
            g_infoEffect->Animate(ix, H + g_infoMarginY - g_info->h, ix, H, false);
        }
        if (!g_root->visibleSelf) return;
    }
    const int x = RootX();
    g_rootEffect->Animate(x, H + g_marginY - g_root->h, x, H, false);
    if (!g_showBarsOnHide) {
        g_close->SetVisibility(false);
    } else {
        const int ix = g_infoOffsetX + W - g_info->w;
        g_infoEffect->Animate(ix, H + g_infoMarginY - g_info->h, ix, H, false);
        BattleBarWindow::Show();
        BeltBarWindow::Show();
        BottomCityWindow::Show();
    }
    g_showBarsOnHide = true;
    Update(0.f);
    if (BuildingPlacement::Activated()) BuildingPlacement::Decline();
    if (g_arrowItem != -1) {
        g_arrowItem = -1;
        BuildingHovers::HideArrow();
        BuildingHovers::SetArrowVisibleWindowLimit(0);
    }
    g_arrowCallback = 0;
    BuildingHovers::SetArrowVisibleWindowLimit(0);
}

// @0x34d67c: opens the tab holding building `id` and scrolls to it.
void OnSelectItemID(unsigned id) {
    unsigned tab = 0;
    for (unsigned t = 0; t < 9; ++t) {
        const auto& list = g_buildings[t];
        for (unsigned k = 0; k < list.size(); ++k) {
            if (list[k]->id != id) continue;
            OnTabSelect(tab);
            g_scroller.ScrollIntoView(k);
            FillBuildings();
            return;
        }
        if (GetTabItemCount(t) != 0) ++tab;
    }
}

// @0x34bd24: the helper arrow on the visible cell showing building `id`.
void ShowArrowOnItem(unsigned id) {
    const unsigned count = GetTabItemCount(g_tab) - g_scroller.firstIndex;
    const unsigned shown = count < g_cellCount ? count : g_cellCount;
    BuildingHovers::SetArrowVisibleWindowLimit(2);
    for (unsigned k = 0; k < shown; ++k) {
        if (!g_cells[k].data || g_cells[k].data->id != id) continue;
        g_arrowItem = (int)k;
        OnItemInfo(k, false);
    }
}

// @0x34d99c: opens the shop on the highest-level building of tab type t (the first of equals).
void ShowBestOfTab(unsigned t) {
    const GameData::BuildingData* best = nullptr;
    for (const GameData::BuildingData* b : g_buildings[t])
        if (!best || best->level < b->level) best = b;
    Show();
    if (best) OnSelectItemID(best->id);
}

}  // namespace ShopWindow
