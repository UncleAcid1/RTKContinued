// BottomFarmWindow: the farm view's bottom bar. Back (to the city), the main action button (plant,
// gather, dig, clean up or change, whatever the farm's patches need), Upgrade and the powder Speed
// Up (or Recover for rotten patches), plus two sign posts on the farm ground to the previous and
// next farm. Port of BottomFarmWindow (libkingdom.so 5.11), 0x25fb60..0x262490. Statics at
// 0x6186b0. OnFarmWater @0x260b10 is not ported: nothing calls it in 5.11.
#include <vector>

#include "engine/IconManager.h"
#include "engine/Render.h"
#include "engine/Timer.h"
#include "game/AIState.h"
#include "game/Background.h"
#include "game/Building.h"
#include "game/BuildingHovers.h"
#include "game/Contracts.h"
#include "game/Entity.h"
#include "game/EntityManager.h"
#include "game/GameData.h"
#include "game/GameState.h"
#include "game/Map.h"
#include "game/StringTable.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/HUD.h"
#include "hud/HoverWindows.h"
#include "windows/Windows.h"

namespace BottomFarmWindow {
namespace {

using GUI::Button;
using GUI::Textfield;
using GUI::Window;

// FarmButton (0x38 bytes, LoadFrom @0x261dc4): one bottom button and its icon variants.
struct FarmButton {
    Window* root = nullptr;           // +0x00
    Window* back = nullptr;           // +0x04 button_bg_blue_back
    Window* leave = nullptr;          // +0x08 icon_leave_screen_60
    Window* water = nullptr;          // +0x0c icon_farm_water
    Window* butcher = nullptr;        // +0x10 icon_farm_butcher
    Window* collect = nullptr;        // +0x14 icon_farm_collect
    Window* dispose = nullptr;        // +0x18 icon_farm_dispose
    Window* plant = nullptr;          // +0x1c icon_farm_plant
    Window* raise = nullptr;          // +0x20 icon_farm_raise
    Window* upgrade = nullptr;        // +0x24 icon_farm_upgrade_large
    Window* speedup = nullptr;        // +0x28 icon_farm_speedup
    Textfield* price = nullptr;       // +0x2c icon_farm_speedup.text_price_crystalv
    Textfield* text = nullptr;        // +0x30
    Button* clickArea = nullptr;      // +0x34

    void LoadFrom(Window* r, const char* name) {
        const char* e = "%s.hud_bottom_button_elements.";
        std::string p = std::string(e);
        root = GUI::GetWindowTypedF<Window>(r, "%s", name);
        back = GUI::GetWindowTypedF<Window>(r, (p + "button_bg_blue_back").c_str(), name);
        leave = GUI::GetWindowTypedF<Window>(r, (p + "icon_leave_screen_60").c_str(), name);
        water = GUI::GetWindowTypedF<Window>(r, (p + "icon_farm_water").c_str(), name);
        butcher = GUI::GetWindowTypedF<Window>(r, (p + "icon_farm_butcher").c_str(), name);
        collect = GUI::GetWindowTypedF<Window>(r, (p + "icon_farm_collect").c_str(), name);
        dispose = GUI::GetWindowTypedF<Window>(r, (p + "icon_farm_dispose").c_str(), name);
        plant = GUI::GetWindowTypedF<Window>(r, (p + "icon_farm_plant").c_str(), name);
        raise = GUI::GetWindowTypedF<Window>(r, (p + "icon_farm_raise").c_str(), name);
        upgrade = GUI::GetWindowTypedF<Window>(r, (p + "icon_farm_upgrade_large").c_str(), name);
        speedup = GUI::GetWindowTypedF<Window>(r, (p + "icon_farm_speedup").c_str(), name);
        price = GUI::GetWindowTypedF<Textfield>(r, (p + "icon_farm_speedup.text_price_crystalv").c_str(), name);
        text = GUI::GetWindowTypedF<Textfield>(r, "%s.text", name);
        clickArea = GUI::GetWindowTypedF<Button>(r, "%s.clickArea", name);
    }
};

WindowManager::FunctionalWindow* g_queue = nullptr;   // BottomFarmWindow::wnd
Window* g_root = nullptr;                             // 0x6186b0 Farm_buttons_container.xml
FarmButton g_plant;                                   // 0x6186b4 button_plant
FarmButton g_back;                                    // 0x6186ec button_back
FarmButton g_upgrade;                                 // 0x618724 button_upgrade
FarmButton g_speedup;                                 // 0x61875c button_speedup
PatchProgressHoverWindow* g_patchWindows[6] = {};     // 0x618794
bool g_speedUpPending = false;                        // 0x6187ac (waits for the next second)
int g_speedUpTime = 0;                                // 0x6187b0
int g_arrowCallback = 0;                              // 0x6187b4
GUI::MovementEffect* g_effect = nullptr;              // 0x6187b8
int g_offsetX = 0, g_offsetY = 0;                     // 0x6187bc 0x6187c0
Window* g_signLeft = nullptr;                         // 0x6187c4 Sign_post_narrow_left.xml
Window* g_signRight = nullptr;                        // 0x6187c8 Sign_post_narrow_right.xml
Window* g_signLeftIcon = nullptr;                     // 0x6187cc
Window* g_signRightIcon = nullptr;                    // 0x6187d0
bool g_tablet = false;                                // 0x60f124
float g_scale = 1.f;                                  // 0x60f128

unsigned PatchCount(const Map::Building* b) { return b->data->id == 0x3ee ? 1u : 6u; }

const Contracts::ContractMission& Mission(const Map::Building* b, int contract) {
    return b->data->delivery->missions[(size_t)contract - 1];
}

// The helper arrow over a button (screen space).
void ArrowAtButton(const FarmButton& b) {
    BuildingHovers::ArrowAt((float)(b.root->x + g_root->x + b.root->w / 2), (float)(b.root->y + g_root->y), false,
                            false, false, false, true, false, false);
}

// The patch's progress box in the powder speed-up mode, over the patch.
void ShowPatchWindow(Map::Building* farm, unsigned p) {
    PatchProgressHoverWindow* w = g_patchWindows[p];
    Entity* e = farm->patchEntities[p];
    w->SetBuilding(farm);
    w->SetEntity(e);
    w->SetPosition((int)e->worldX, (int)e->worldY - 0x28);
    w->Show();
    w->SetFullSpeedUpMode();
    w->MoveWindowOnTop(true);
}

unsigned GetPatchCountInSpeedUp() {   // @0x25fe30
    Map::Building* farm = Map::GetCurrentFarm();
    if (!farm) return 0;
    unsigned n = 0;
    for (unsigned i = 0; i < PatchCount(farm); ++i)
        if (g_patchWindows[i]->fullSpeedUp) ++n;
    return n;
}

// @0x25fe90 / @0x25fef0: the next / previous opened farm's view.
void OnFarmRight() {
    if (BuildingHovers::IsHoverVisible()) BuildingHovers::Hide();
    Map::Building* next = Map::GetFarmAfter(Map::GetCurrentFarm());
    if (!next || next == Map::GetCurrentFarm()) return;
    Map::ShowFarm(false, nullptr);
    Map::ShowFarm(true, next);
}

void OnFarmLeft() {
    if (BuildingHovers::IsHoverVisible()) BuildingHovers::Hide();
    Map::Building* prev = Map::GetFarmBefore(Map::GetCurrentFarm());
    if (!prev || prev == Map::GetCurrentFarm()) return;
    Map::ShowFarm(false, nullptr);
    Map::ShowFarm(true, prev);
}

// @0x25ff50: the powder for every growing patch, for the sum of their orders' speed-up prices.
void OnFarmSpeedUpActual() {
    if (GameState::secondTutorial == 0x100) GUI::SetInteractionLock(false);
    if (GameState::tutorial == 0x45) {
        // UNVERIFIED (tutorial): BuildingHovers::HideWorldDialog(nullptr).
        BuildingHovers::HideArrow();
        GameState::tutorial = 0x46;
    }
    Map::Building* farm = Map::GetCurrentFarm();
    if (!farm) return;
    const unsigned n = PatchCount(farm);
    int cost = 0;
    for (unsigned i = 0; i < n; ++i)
        if (farm->IsSoilPatchActive(i) && farm->patchContract[i] != 0)
            cost += (int)Mission(farm, farm->patchContract[i]).speedupCost2;
    cost = GameState::AdjustCrystalCost(cost);
    NotEnoughWindow::ResetRequirements();
    NotEnoughWindow::AddRequirement(GameState::kCrystal, (unsigned)cost);
    if (!NotEnoughWindow::CheckRequirements()) {
        NotEnoughWindow::Show();
        return;
    }
    // UNVERIFIED (milestone 5): SoundsManager::PlaySound("ui_on_speedup").
    GameState::ChangeResourceAmount(GameState::kCrystal, -cost);
    // PORT (online removed): Billing::LogCBPurchase(0x16, farm id, cost).
    for (unsigned i = 0; i < n; ++i) {
        if (!farm->IsSoilPatchActive(i)) continue;
        farm->SpeedupFarm(-1, (int)i);
        ShowPatchWindow(farm, i);
    }
}

// @0x260248: rotten patches (and nothing growing): Recover all for 4 crystals each, confirmed
// first; otherwise the powder speed-up, which waits for the next second when the secondary
// tutorial is done (the GUI is locked meanwhile).
void OnFarmSpeedUp(bool confirmed) {
    Map::Building* farm = Map::GetCurrentFarm();
    if (!farm || farm->GetFirstActiveSoilPatch() != -1 || farm->GetFirstRottenSoilPatch() == -1) {
        if (GameState::secondTutorial != 0x100) {
            OnFarmSpeedUpActual();
            return;
        }
        GUI::SetInteractionLock(true);
        g_speedUpPending = true;
        g_speedUpTime = Timer::GetGlobalTime();
        return;
    }
    const unsigned n = PatchCount(farm);
    int cost = 0;
    for (unsigned i = 0; i < n; ++i)
        if (farm->IsSoilPatchRotten(i)) cost += 4;
    unsigned price = (unsigned)GameState::AdjustCrystalCost(cost);
    NotEnoughWindow::ResetRequirements();
    NotEnoughWindow::AddRequirement(GameState::kCrystal, price);
    if (!NotEnoughWindow::CheckRequirements()) {
        NotEnoughWindow::Show();
        return;
    }
    if (!confirmed) {
        ConfirmPurchaseWindow::SetParameters([] { OnFarmSpeedUp(true); }, price, true);
        ConfirmPurchaseWindow::Show();
        return;
    }
    GameState::ChangeResourceAmount(GameState::kCrystal, -(int)price);
    // PORT (online removed): Billing::LogCBPurchase(0x15, farm id, price).
    // UNVERIFIED (milestone 5): SoundsManager::PlaySound("ui_on_speedup").
    for (unsigned i = 0; i < n; ++i)
        if (farm->IsSoilPatchRotten(i)) farm->RestoreFarm(i);
}

void OnFarmUpgrade(bool confirmed);

// @0x260598: the planted orders are refunded and their patches left dirty, then the upgrade.
void OnFarmStopAndUpgrade(bool confirmed) {
    PopupSelectionWindow::Hide();
    if (Map::Building* farm = Map::GetCurrentFarm()) {
        for (unsigned i = 0; i < PatchCount(farm); ++i) {
            AIBaseState* ai = farm->patchEntities[i]->GetAI();
            if (!ai->GetItem()) continue;
            if (int c = farm->patchContract[i]) {
                const Contracts::ContractMission& m = Mission(farm, c);
                GameState::ChangeResourceAmount(GameState::kGold, (int)m.price);
                GameState::ChangeResourceAmount(m.priceResource, (int)m.priceResourceCount);
            }
            farm->patchEntities[i]->GetAI()->Clean();
            farm->patchEntities[i]->GetAI()->ChangeState(0x13);
            farm->resources[i] = 0x13;
            farm->patchContract[i] = 0;
        }
    }
    OnFarmUpgrade(confirmed);
}

// @0x2603d8: the upgrade's requirements; with orders growing, a question first (they are lost).
void OnFarmUpgrade(bool confirmed) {
    Map::Building* farm = Map::GetCurrentFarm();
    if (!farm) return;
    int active = farm->GetFirstActiveSoilPatch();
    if (!UpgradeBuilding(Map::GetCurrentFarm(), true)) return;
    if (!NotEnoughWindow::CheckRequirements() || !confirmed) {
        NotEnoughWindow::SetActionCallback([] { OnFarmUpgrade(true); }, StringTable::GetString("UPGRADE_ACTION"),
                                           false);
        NotEnoughWindow::Show();
        return;
    }
    NotEnoughWindow::Hide();
    if (active == -1) {
        UpgradeBuildingContinuation(farm);
        return;
    }
    PopupSelectionWindow::Show(StringTable::GetString("UPGRADE_WILL_CANCEL_HEADER"),
                               StringTable::GetString("UPGRADE_WILL_CANCEL"), StringTable::GetString("HUD_OK"),
                               IconManager::GetIcon("gold_confirm"), [] { OnFarmStopAndUpgrade(true); },
                               StringTable::GetString("CANCEL"), IconManager::GetIcon("gold_cancel"),
                               PopupSelectionWindow::Hide);
    PopupSelectionWindow::SetFirstGreen();
}

// @0x2606b8: what the main button does: plant on the empty patches (the order panel), else gather
// every ripe crop (if it fits in the storage), dig the dirty patches, clear the rotten ones, or
// change a growing order.
void OnFarmPlant() {
    Map::Building* farm = Map::GetCurrentFarm();
    const unsigned n = PatchCount(farm);
    if (farm->GetFirstEmptySoilPatch() != -1) {
        unsigned empty = 0;
        for (unsigned i = 0; i < n; ++i)
            if (farm->IsSoilPatchEmpty(i)) ++empty;
        BuildingHovers::ShowFarmHover(false, (unsigned)farm->GetFirstEmptySoilPatch(), empty);
        return;
    }
    if (farm->GetFirstReadySoilPath() != -1) {
        unsigned amounts[8] = {};
        for (unsigned i = 0; i < n; ++i) {
            if (!farm->IsSoilPatchReady(i) || farm->patchContract[i] == 0) continue;
            const Contracts::ContractMission& m = Mission(farm, farm->patchContract[i]);
            amounts[m.rewardResource] += m.rewardResourceCount;
        }
        for (int r = 0; r < 8; ++r) {
            if (amounts[r] == 0 ||
                (unsigned)GameState::resourceAmountMax > (unsigned)GameState::GetResourceAmount(r) + amounts[r])
                continue;
            float x = (Map::GetFarmWorldX() + (float)(GUI::ScreenWidth() / 2)) - 100.f;
            float y = (Map::GetFarmWorldY() - (float)(GUI::ScreenHeight() / 2)) + 300.f;
            BuildingHovers::ShowTextHover(x, y, StringTable::GetString("WORK_RES_NO_SPACE"), 1.f, 1.f, 1.f, 0.f,
                                          0.f, 0.f, 0x19, 5, 5.f, true, 2.f, 50.f);
            return;
        }
        if (GameState::tutorial == 0x47) {
            GUI::SetInteractionObjectLock(nullptr, nullptr);
            GUI::SetInteractionLock(true);
            BuildingHovers::HideArrow();
            GameState::tutorial = 0x4a;
        }
        for (unsigned i = 0; i < n; ++i)
            if (farm->IsSoilPatchReady(i)) farm->farmer->GetAI()->Farm(0, (int)i, false);
        return;
    }
    if (farm->GetFirstDirtySoilPatch() != -1) {
        for (unsigned i = 0; i < n; ++i)
            if (farm->IsSoilPatchDirty(i)) farm->farmer->GetAI()->Farm(0, (int)i, false);
        return;
    }
    if (farm->GetFirstRottenSoilPatch() != -1) {
        for (unsigned i = 0; i < n; ++i) {
            if (!farm->IsSoilPatchRotten(i)) continue;
            farm->CleanFarm(i);
            farm->farmer->GetAI()->Farm(0, (int)i, false);
            farm->farmer->GetAI()->Farm(0, (int)i, false);   // (queued twice, as the original)
        }
        return;
    }
    if (farm->GetFirstActiveSoilPatch() == -1) {
        BuildingHovers::ShowFarmHover(false, 0, 1);
        return;
    }
    unsigned growing = 0;
    for (unsigned i = 0; i < n; ++i)
        if (farm->IsSoilPatchActive(i)) ++growing;
    BuildingHovers::ShowFarmHover(false, farm->resourceLeft != 0 ? 0xffffffffu : 0u, growing);
}

bool OnBack() {   // @0x260d20
    if (!g_root->visibleSelf) return false;
    OnFarmBack();
    return true;
}

// @0x260d48: the tutorial arrows, then the buttons' looks from the patches' states.
void Update(float) {
    if (!g_root->visibleSelf) return;
    if (g_speedUpPending && Timer::GetGlobalTime() != g_speedUpTime) {
        g_speedUpPending = false;
        OnFarmSpeedUpActual();
    }
    if (GameState::tutorial == 0x42 && g_root->visibleSelf) {
        if (BuildingHovers::GetArrowClickCallbackID() != g_arrowCallback)
            g_arrowCallback = BuildingHovers::SetArrowClickCallback(new GUI::Callback(OnFarmPlant));
        GUI::SetInteractionObjectLock(g_plant.clickArea, nullptr);
        ArrowAtButton(g_plant);
    }
    if (GameState::tutorial == 0x45 && g_speedup.root->enabled && g_root->visibleSelf) {
        Map::Building* farm = Map::GetCurrentFarm();
        if (farm && !farm->IsFarmSpeedUp(0)) {
            if (BuildingHovers::GetArrowClickCallbackID() != g_arrowCallback)
                g_arrowCallback = BuildingHovers::SetArrowClickCallback(new GUI::Callback([] { OnFarmSpeedUp(true); }));
            GUI::SetInteractionObjectLock(g_speedup.clickArea, nullptr);
            BuildingHovers::SetArrowVisibleWindowLimit(1);
            ArrowAtButton(g_speedup);
        }
    }
    if (GameState::tutorial == 0x4f) {
        if (BuildingHovers::GetArrowClickCallbackID() != g_arrowCallback)
            g_arrowCallback = BuildingHovers::SetArrowClickCallback(new GUI::Callback(OnFarmBack));
        GUI::SetInteractionObjectLock(g_back.clickArea, nullptr);
        ArrowAtButton(g_back);
        GameState::tutorial = 0x50;
    }

    Map::Building* farm = Map::GetCurrentFarm();
    if (!farm) return;
    const unsigned n = PatchCount(farm);
    // Speed Up is for growing patches with no powder already working.
    bool canSpeedUp = farm->GetFirstActiveSoilPatch() + 1 != 0;
    for (unsigned i = 0; i < n; ++i)
        if (farm->IsFarmSpeedUp((int)i)) canSpeedUp = false;
    const int empty = farm->GetFirstEmptySoilPatch();
    const int active = farm->GetFirstActiveSoilPatch();
    const int ready = farm->GetFirstReadySoilPath();
    const int dirty = farm->GetFirstDirtySoilPatch();
    const int rotten = farm->GetFirstRottenSoilPatch();
    const bool animals = farm->id == 0x3ee;
    g_back.text->SetText(StringTable::GetString("INVENTORY_BACK"));
    g_plant.root->SetEnabled(true);
    for (Window* w : {g_plant.butcher, g_plant.collect, g_plant.dispose, g_plant.plant, g_plant.raise})
        w->SetVisibility(false);
    Window* icon = nullptr;
    if (empty != -1) {
        g_plant.text->SetText(StringTable::GetString(animals ? "PERFORM_GROW" : "PERFORM_PLANT"));
        icon = animals ? g_plant.raise : g_plant.plant;
    } else if (ready != -1) {
        g_plant.text->SetText(StringTable::GetString(animals ? "ANIMAL_GATHER" : "PERFORM_GATHER"));
        icon = animals ? g_plant.butcher : g_plant.collect;
    } else if (dirty != -1) {
        g_plant.text->SetText(StringTable::GetString("PERFORM_DIG"));
        icon = g_plant.dispose;
    } else if (rotten != -1) {
        g_plant.text->SetText(StringTable::GetString(animals ? "ANIMAL_CLEAN_UP" : "FARM_CLEAN_UP"));
        icon = g_plant.dispose;
    } else if (active == -1) {
        g_plant.root->SetEnabled(false);
        icon = animals ? g_plant.raise : g_plant.plant;
    } else {
        g_plant.text->SetText(StringTable::GetString(animals ? "ANIMAL_CHANGE" : "FARM_CHANGE"));
        icon = animals ? g_plant.raise : g_plant.plant;
    }
    icon->SetVisibility(true);
    g_upgrade.root->SetEnabled((unsigned)farm->level < farm->data->GetMaxUpgradeCount() && GameState::tutorial > 0x50);
    g_upgrade.text->SetText(StringTable::GetString("PERFORM_UPGRADE"));
    g_speedup.root->SetEnabled(true);
    if (!canSpeedUp) {
        if (rotten == -1) {
            g_speedup.root->SetEnabled(false);
            g_speedup.price->SetVisibility(false);
            g_speedup.text->SetText(StringTable::GetString("PERFORM_SPEEDUP"));
        } else {
            int cost = 0;
            for (unsigned i = 0; i < n; ++i)
                if (farm->IsSoilPatchRotten(i)) cost += 4;
            g_speedup.price->SetVisibility(true);
            g_speedup.price->SetText(ToWideString(GameState::AdjustCrystalCost(cost)));
            g_speedup.text->SetText(StringTable::GetString(animals ? "ANIMAL_RECOVER_UP" : "FARM_RECOVER_UP"));
        }
        return;
    }
    int cost = 0;
    for (unsigned i = 0; i < n; ++i)
        if (farm->IsSoilPatchActive(i) && farm->patchContract[i] != 0)
            cost += (int)Mission(farm, farm->patchContract[i]).speedupCost2;
    if (cost == 0) {
        std::printf("ERROR: BottomFarmWindow::Update() Farm patch %d is active but doesn't have an active "
                    "contract\n", active);
    } else {
        g_speedup.price->SetVisibility(true);
        g_speedup.price->SetText(ToWideString(GameState::AdjustCrystalCost(cost)));
    }
    g_speedup.text->SetText(StringTable::GetString("PERFORM_SPEEDUP"));
}

// @0x261a4c: the bar at the bottom centre (below the screen while hidden), the sign posts in the
// farm ground's lower corners.
void SetZ(float z) {
    g_root->SetZ(z);
    const int W = GUI::ScreenWidth(), H = GUI::ScreenHeight();
    if (!g_root->visibleSelf) {
        g_root->SetPosition((W - g_root->w) / 2, H);
    } else {
        int y = g_effect->hideAtEnd ? H : g_offsetY + H - g_root->h;
        g_root->SetPosition((W - g_root->w) / 2, y);
    }
    float left = 0.f, top = 0.f, right = 0.f, bottom = 0.f;
    Background::GetFarmBounds(left, top, right, bottom);
    g_signLeft->SetZ(0.8f);
    g_signLeft->SetPosition((int)left + 100, ((int)bottom - 100) - g_signLeft->h);
    g_signRight->SetZ(0.8f);
    g_signRight->SetPosition(((int)right - 100) - g_signRight->w, ((int)bottom - 100) - g_signRight->h);
}

// @0x261c38: the bar first (nothing while a powder speed-up runs), then the sign posts.
bool Click(int x, int y, bool pressed) {
    if (GetPatchCountInSpeedUp() != 0 || g_root->Click(x, y, pressed, true)) return true;
    int wx = x, wy = y;
    Map::MouseCoordinatesToWorld(wx, wy);
    if (g_signLeft->Click(wx, wy, pressed, false)) return true;
    return g_signRight->Click(wx, wy, pressed, false);
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
        g_queue = new WindowManager::FunctionalWindow("BottomFarmWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_root->visibleSelf; }

void Init() {
    g_offsetX = 0;
    g_offsetY = 0;
    g_tablet = GUI::IsTabletVersion();
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Farm_buttons_container.xml",
                             "Farm_buttons_container.png", GUI::GetHudScaleFactor(), 0, 0, 0, 0, false, 1.f);
    g_queue->RegisterTopWindow(g_root);
    g_scale = g_root->scale;
    g_root->SetVisibility(false);
    g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::ScreenHeight());
    g_back.LoadFrom(g_root, "button_back");
    g_back.clickArea->SetOnClick(OnFarmBack);
    g_back.clickArea->SetActionSound("ui_click", 1);
    g_plant.LoadFrom(g_root, "button_plant");
    g_plant.clickArea->SetOnClick(OnFarmPlant);
    g_plant.clickArea->SetActionSound("ui_click", 1);
    g_upgrade.LoadFrom(g_root, "button_upgrade");
    g_upgrade.clickArea->SetOnClick([] { OnFarmUpgrade(false); });
    g_speedup.LoadFrom(g_root, "button_speedup");
    g_speedup.clickArea->SetOnClick([] { OnFarmSpeedUp(false); });
    if (GUI::IsSmallScreenVersion()) {
        g_speedup.text->MoveWindow(-5, 0);
        g_speedup.text->SetSize((unsigned)(g_speedup.text->w + 10), (unsigned)g_speedup.text->h);
    }
    g_effect = GUI::CreateMovementEffect(g_root);
    g_signLeft = GUI::RegisterUI("../resource/kingdom_ui/1Original/Sign_post_narrow_left.xml",
                                 "Sign_post_narrow_left.png", 1.f, 0, 0, 0, 0, false, 1.f);
    g_signLeft->SetVisibility(false);
    g_signLeft->SetOnClick(OnFarmLeft);
    g_signLeft->SetScreenSpace(false);
    g_signLeftIcon = GUI::GetWindowTyped<Window>(g_signLeft, "icon");
    g_signLeftIcon->SetTexture(IconManager::GetIcon("gold_arrow_left"), true);
    g_signRight = GUI::RegisterUI("../resource/kingdom_ui/1Original/Sign_post_narrow_right.xml",
                                  "Sign_post_narrow_right.png", 1.f, 0, 0, 0, 0, false, 1.f);
    g_signRight->SetVisibility(false);
    g_signRight->SetOnClick(OnFarmRight);
    g_signRight->SetScreenSpace(false);
    g_signRightIcon = GUI::GetWindowTyped<Window>(g_signRight, "icon");
    g_signRightIcon->SetTexture(IconManager::GetIcon("gold_arrow_right"), true);
    g_queue->fn.update = Update;
    g_queue->fn.show = Show;
    g_queue->fn.zRange = WindowManager::ReturnSmallZRange;
    g_queue->fn.back = OnBack;
    for (PatchProgressHoverWindow*& w : g_patchWindows) w = new PatchProgressHoverWindow();
}

void Deinit() {
    GUI::RemoveMovementEffect(g_effect);
    g_effect = nullptr;
    delete g_root;
    g_root = nullptr;
    delete g_signLeft;
    g_signLeft = nullptr;
    delete g_signRight;
    g_signRight = nullptr;
    for (PatchProgressHoverWindow*& w : g_patchWindows) {
        delete w;
        w = nullptr;
    }
}

// @0x2618e0: slides up; the sign posts show where there is another farm.
void Show() {
    g_offsetY = 0;
    g_queue->MoveWindowOnTop(true);
    if (!g_root->visibleSelf) {
        int x = (GUI::ScreenWidth() - g_root->w) / 2;
        g_effect->Animate(x, GUI::ScreenHeight(), x + g_offsetX, (GUI::ScreenHeight() + g_offsetY) - g_root->h, true);
    }
    g_root->SetVisibility(true);
    Map::Building* before = Map::GetFarmBefore(Map::GetCurrentFarm());
    bool other = before && before != Map::GetCurrentFarm();
    g_signLeft->SetVisibility(other);
    g_signRight->SetVisibility(other);
    // (GUI::PreloadWindow: the textures load now rather than on first draw)
}

// @0x2617f4: slides down; the sign posts and the patch boxes go.
void Hide() {
    if (g_root->visibleSelf) {
        int x = (GUI::ScreenWidth() - g_root->w) / 2;
        g_effect->Animate(x + g_offsetX, (GUI::ScreenHeight() + g_offsetY) - g_root->h, x, GUI::ScreenHeight(), false);
    }
    g_signLeft->SetVisibility(false);
    g_signRight->SetVisibility(false);
    for (PatchProgressHoverWindow* w : g_patchWindows) w->Hide();
}

void HelpWithUpgrade() {
    BuildingHovers::SetArrowClickCallback(g_upgrade.root->enabled ? new GUI::Callback([] { OnFarmUpgrade(false); })
                                                                   : nullptr);
    ArrowAtButton(g_upgrade);
}

void ShowPatchSpeedUp(unsigned patch) {
    if (Map::Building* farm = Map::GetCurrentFarm()) ShowPatchWindow(farm, patch);
}

// @0x260cc0: the crops lying on the farm go along as resources; back to the city.
void OnFarmBack() {
    if (GameState::tutorial == 0x50) GameState::tutorial = 0x51;
    if (Map::GetCurrentFarm()) BuildingHovers::ConvertDroppedFoodToResource(Map::GetCurrentFarm());
    BuildingHovers::HideArrow();
    HUDWindow::ExitFarm();
    EntityManager::Update(0.f);   // (the original passes the manager pointer as the float)
    BuildingHovers::Update(0.0, true);
}

}  // namespace BottomFarmWindow
