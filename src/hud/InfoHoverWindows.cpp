// The info windows of a tapped building (BuildingHovers::OnBuildingClick) and the upgrade dialog
// they share. Screen-space BuildingHoverWindows positioned over the building.
#include <cstdio>
#include <string>

#include "engine/IconManager.h"
#include "engine/Render.h"
#include "engine/Resources.h"
#include "engine/Timer.h"
#include "game/AIState.h"
#include "game/Building.h"
#include "game/BuildingHovers.h"
#include "game/Contracts.h"
#include "game/Entity.h"
#include "game/EntityData.h"
#include "game/EntityManager.h"
#include "game/GameData.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/Map.h"
#include "game/Setting.h"
#include "game/StringTable.h"
#include "gui/GUI.h"
#include "hud/HUD.h"
#include "hud/HoverWindows.h"
#include "windows/Windows.h"

// ---- the upgrade dialog ----

bool UpgradeBuilding(Map::Building* b, bool prepareOnly) {
    const GameData::UpgradeInfo& u = b->GetNextUpgradeInfo();
    std::u32string text;
    if (u.quest == 0 || GameState::TaskCompleted((unsigned)u.quest)) {
        if (GameState::GetPlayerWorkersCount() + Map::GetPendingWorkerCount() + u.givePopulation <=
            GameState::GetMaxWorkerCount()) {
            NotEnoughWindow::ResetRequirements();
            NotEnoughWindow::SetLevelFailMessage(StringTable::GetString("UPGRADE_LEVEL_REQ"));
            std::u32string desc = SWPrintf(0x100, StringTable::GetString("REQUIREMENT_UPGRADE"),
                                           {StringTable::GetString(b->data->name.c_str()),
                                            ToWideString(b->level + 2)});
            NotEnoughWindow::SetDescriptionText(desc.c_str(), nullptr);
            NotEnoughWindow::SetUpgradableBuilding(b);
            NotEnoughWindow::AddLevelRequirement((unsigned)u.level);
            NotEnoughWindow::AddPopulaionRequirement((unsigned)u.population);
            for (int i = 0; i < 11; ++i) NotEnoughWindow::AddRequirement(i, (unsigned)u.cost[i]);
            if (u.requiredId != 0)
                NotEnoughWindow::AddBuildingLevelRequirement((unsigned)u.requiredId, (unsigned)u.requiredCount);
            NotEnoughWindow::SetExchangeLimit((unsigned)u.exchangeLimit);
            if (prepareOnly) return true;
            NotEnoughWindow::CheckRequirements();
            NotEnoughWindow::SetActionCallback([b] { UpgradeBuildingContinuation(b); },
                                               StringTable::GetString("UPGRADE_ACTION"), false);
            BuildingHovers::Hide();
            NotEnoughWindow::Show();
            return true;
        }
        text = SWPrintf(0x100, StringTable::GetString("PEOPLE_LIMIT"), {ToWideString(GameState::GetMaxWorkerCount())});
    } else {
        // UNVERIFIED (milestone 4, Tasks): with the quest known (Tasks::GetTask) the text is
        // "HIRE_TROOPS_TASK_LOCK" with the quest's name; without one it is "UPGRADE_LOCKED".
        text = StringTable::GetString("UPGRADE_LOCKED");
    }
    PopupWindow::Show(text.c_str(), PopupWindow::Hide, nullptr, nullptr);
    return false;
}

void UpgradeBuildingContinuation(Map::Building* b) {
    if (b == Map::GetCurrentFarm()) HUDWindow::ExitFarm();
    b->Upgrade((unsigned)b->level + 1);
    NotEnoughWindow::Hide();
}

// ---- FactoryHoverWindow ----

namespace {

const Contracts::ContractMission& Mission(const Map::Building* b, unsigned i) {
    return b->data->delivery->missions[i];
}

// The soil patch number of a patch entity (-1: the building's own order).
int PatchOf(Entity* e) { return e ? e->GetAI()->GetFarmPatchNum() : -1; }

unsigned PatchCount(const Map::Building* b) { return b->data->id == 0x3ee ? 1u : 6u; }

void SetIcon(GUI::Window* w, const char* name) { w->SetTexture(IconManager::GetIcon(name), true); }

// The button centre the helper arrow points at.
void ArrowAtButton(GUI::Window* root, GUI::Window* b) {
    BuildingHovers::ArrowAt((float)(b->w + b->x + root->x), (float)(root->y + b->y + b->h / 2), false, true, false,
                            false, true, false, false);
}

}  // namespace

void FactoryHoverWindow::ProgressInfo::SetProgress(float p) {
    clip.right = (int)((float)clip.left + (float)root->w * p);
}

FactoryHoverWindow::FactoryHoverWindow() {
    name = "FactoryHoverWindow";
    progressPercent = GameState::GetSetting("progress_as_time") == 0.f;
    itemBoosts = (int)GameState::GetSetting("item_boosts") == 1;
}

FactoryHoverWindow::~FactoryHoverWindow() { delete root; }

void FactoryHoverWindow::Init() {
    root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Hint_order_item_holder.xml",
                           "Hint_order_item_holder.png", GUI::GetHoverScaleFactor(1.f, 1.f), 0, 0, 0, 0, false, 1.f);
    root->SetVisibility(false);
    header = GUI::GetWindowTyped<GUI::Textfield>(root, "header_text");
    for (unsigned i = 0; i < 5; ++i) {   // ItemHolder::LoadFrom @0x37cf0c
        char p[64];
        std::snprintf(p, sizeof p, "item_order_item_holder_%02d", i + 1);
        ItemHolder& h = items[i];
        h.root = GUI::GetWindowTypedF<GUI::Window>(root, "%s", p);
        h.ordered = GUI::GetWindowTypedF<GUI::Window>(root, "%s.item_order_ordered_item", p);
        h.icon = GUI::GetWindowTypedF<GUI::Window>(root, "%s.icon_active", p);
        h.icon->takesZ = true;
        h.lock = GUI::GetWindowTypedF<GUI::Window>(root, "%s.icon_lock_16", p);
        h.root->SetOnClick([this, i] { OnButton(i); });
        h.root->SetActionSound("ui_click", 1);
    }
    // OrderButton::LoadFrom @0x37cac0
    order = GUI::GetWindowTyped<GUI::Window>(root, "button_item_order");
    orderIcon = GUI::GetWindowTyped<GUI::Window>(root, "button_item_order.icon_35");
    orderIcon->takesZ = true;
    orderText = GUI::GetWindowTyped<GUI::Textfield>(root, "button_item_order.text_over_green_2lines");
    order->SetOnClick([this] { OnLaunchContract(); });
    // SpeedupButton::LoadFrom @0x37c9a8
    speedup = GUI::GetWindowTyped<GUI::Window>(root, "button_item_speed_up");
    speedupIcon = GUI::GetWindowTyped<GUI::Window>(root, "button_item_speed_up.icon_35");
    speedupIcon->takesZ = true;
    speedupCrystal = GUI::GetWindowTyped<GUI::Window>(root, "button_item_speed_up.icon_35_crystal");
    SetIcon(speedupCrystal, "35_crystal");
    speedupItem = GUI::GetWindowTyped<GUI::Window>(root, "button_item_speed_up.speed_up_item");
    speedupItem->takesZ = true;
    speedupText = GUI::GetWindowTyped<GUI::Textfield>(root, "button_item_speed_up.text");
    speedupPrice = GUI::GetWindowTyped<GUI::Textfield>(root, "button_item_speed_up.text_price");
    speedup->SetOnClick([this] { OnSpeedUp(false); });
    // StopButton::LoadFrom @0x37c938
    stop = GUI::GetWindowTyped<GUI::Window>(root, "button_item_stop_prod");
    stopIcon = GUI::GetWindowTyped<GUI::Window>(root, "button_item_stop_prod.icon_35");
    stopIcon->takesZ = true;
    stopText = GUI::GetWindowTyped<GUI::Textfield>(root, "button_item_stop_prod.text_give_job");
    stop->SetOnClick([this] { OnStopWithConfirm(); });
    jobText = GUI::GetWindowTyped<GUI::Textfield>(root, "text_job");
    // ProgressInfo::LoadFrom @0x37cb30
    progress.root = GUI::GetWindowTyped<GUI::Window>(root, "progress_bar_item_order");
    progress.root->MoveWindow(2, 0);
    progress.color = GUI::GetWindowTyped<GUI::Window>(root, "progress_bar_item_order.progres_bar_color");
    progress.under = GUI::GetWindowTyped<GUI::Textfield>(root, "progress_bar_item_order.text_under");
    progress.over = GUI::GetWindowTyped<GUI::Textfield>(root, "progress_bar_item_order.text_over");
    GUI::Window* pr = progress.root;
    progress.clip = {pr->x, pr->y, pr->w + pr->x, pr->h + pr->y};
    progress.color->SetClipRect(&progress.clip);
    progress.over->SetClipRect(&progress.clip);
    // OrderThreeArrowInfo @0x37ce4c, OrderThreeInfo @0x37cd8c, OrderTwoInfo @0x37ccdc
    auto loadRow = [this](Row& r, const char* p, unsigned icons, const char* const* texts, unsigned n) {
        r.root = GUI::GetWindowTypedF<GUI::Window>(root, "%s", p);
        for (unsigned i = 0; i < icons; ++i) {
            r.icon[i] = GUI::GetWindowTypedF<GUI::Window>(root, "%s.icon_25_%02d", p, i + 1);
            r.icon[i]->takesZ = true;
        }
        for (unsigned i = 0; i < n; ++i)
            r.text[i] = GUI::GetWindowTyped<GUI::Textfield>(root, (std::string(p) + "." + texts[i]).c_str());
    };
    static const char* const kArrow[] = {"text_item_from_01", "text_item_from_02", "text_item_to_01"};
    static const char* const kThree[] = {"text_item_from_01", "text_item_from_02", "text_item_from_03"};
    loadRow(resTimeRes, "order_holder_res_time_res", 3, kArrow, 3);
    loadRow(resTimeItem3, "order_holder_res_time_item_x3", 3, kThree, 3);
    loadRow(resTimeItem, "order_holder_res_time_item", 2, kThree, 2);
    // OrderEarnedInfo @0x37cc6c: one icon and one text
    earned.root = GUI::GetWindowTyped<GUI::Window>(root, "order_holder_already_earned");
    earned.icon[0] = GUI::GetWindowTyped<GUI::Window>(root, "order_holder_already_earned.icon_25_02");
    earned.icon[0]->takesZ = true;
    earned.text[0] = GUI::GetWindowTyped<GUI::Textfield>(root, "order_holder_already_earned.text_item_from_02");
    // OrderLockedInfo @0x37cc24
    locked.root = GUI::GetWindowTyped<GUI::Window>(root, "order_holder_order_locked");
    locked.text[0] = GUI::GetWindowTyped<GUI::Textfield>(root, "order_holder_order_locked.text_locked");
    for (unsigned i = 0; i < 5; ++i) selected[i] = GUI::GetWindowTypedF<GUI::Window>(root, "selected_item_%02d", i + 1);
    usesZRange = true;
}

void FactoryHoverWindow::SetZ(float z) {
    if (!shownHover) return;
    root->SetZ(z - 0.001f);
}

bool FactoryHoverWindow::Click(int x, int y, bool pressed) {
    if (!shownHover) return false;
    if (speedingUp || BuildingHovers::ClickOnArrow(x, y, pressed) || root->Click(x, y, pressed, false)) return true;
    return GameState::TutorialStep() == 0x43;
}

void FactoryHoverWindow::Show() {
    if (shownHover) return;
    BuildingHoverWindow::Show();
    root->SetVisibility(true);
    if (GameState::TutorialStep() == 0x53) ShowArrowAt(0);
    int c = building ? building->lastContract : 0;
    if (c == 0) c = GetCurrentContract();
    selectedOrder = c == 0 ? 0 : (unsigned)(c - 1);
    SetBuilding(building);
}

void FactoryHoverWindow::Hide() {
    if (!shownHover) return;
    root->SetVisibility(false);
    BuildingHoverWindow::Hide();
    ignoreCompletion = false;
    multiCount = 1;
    multiFarm = false;
}

void FactoryHoverWindow::SetPosition(int x, int y) {
    int ny = y - f30, nx = x - root->w / 2;
    FixWindowPosition(nx, ny);
    BuildingHoverWindow::SetPosition(nx, ny);
    root->SetPosition(nx, ny);
    if (GameState::TutorialStep() == 0x43) {
        BuildingHovers::SetArrowVisibleWindowLimit(1);
        BuildingHovers::SetArrowClickCallback(new GUI::Callback([this] { OnLaunchContract(); }));
        GUI::SetInteractionObjectLock(order, nullptr);
        ArrowAtButton(root, order);
    }
}

void FactoryHoverWindow::FixWindowPosition(int& x, int& y) {
    if (x < 0) x = 0;
    if (y < -10) y = -10;
    if (GUI::ScreenWidth() < f2c + x) x = GUI::ScreenWidth() - f2c;
    if (GUI::ScreenHeight() < f30 + y) y = GUI::ScreenHeight() - f30;
}

void FactoryHoverWindow::OnButton(unsigned i) {
    selectedOrder = i;
    SetBuilding(building);
}

int FactoryHoverWindow::GetSpeedUpCost() {
    Map::Building* b = building;
    if (!b) return 0;
    if (entity && b->data->buildingClass == 0xd)
        return GameState::AdjustCrystalCost((int)Mission(b, (unsigned)(b->patchContract[PatchOf(entity)] - 1)).speedupCost2);
    if (b->data->buildingClass == 2 && b->contract != 0)
        return GameState::AdjustCrystalCost((int)Mission(b, (unsigned)(b->contract - 1)).speedupCost2);
    return 0;
}

int FactoryHoverWindow::GetCurrentContract() {
    int c = building->contract;
    if (entity) c = building->patchContract[PatchOf(entity)];
    if (building == Map::GetCurrentFarm() && multiFarm && !entity) c = 0;
    return c;
}

void FactoryHoverWindow::OnSpeedUpFinished() {
    Map::Building* b = building;
    if (b->data->buildingClass == 0xd) {
        int patch = PatchOf(entity);
        int c = b->patchContract[patch];
        int cost = GameState::AdjustCrystalCost((int)Mission(b, (unsigned)(c - 1)).speedupCost2);
        if (!itemBoosts) GameState::ChangeResourceAmount(GameState::kCrystal, -cost);
        else GameState::RemoveItem(boostItem, boostAmount);
        // PORT (online removed): Billing::LogCBPurchase(1, delivery id * 10 - 1 + c, cost).
        if (Map::GetCurrentFarm()) {
            b->SpeedupFarm(-1, patch);
            BottomFarmWindow::ShowPatchSpeedUp((unsigned)patch);
        }
        speedingUp = false;
    } else {
        // UNVERIFIED (tutorial): at step 0x54 the camera centres on the building (Render::CenterOn)
        // and the step moves to 0x55.
        int cost = 0;
        if (b->data->buildingClass == 2 && b->contract != 0)
            cost = (int)Mission(b, (unsigned)(b->contract - 1)).speedupCost2;
        cost = GameState::AdjustCrystalCost(cost);
        if (!itemBoosts) GameState::ChangeResourceAmount(GameState::kCrystal, -cost);
        else GameState::RemoveItem(boostItem, boostAmount);
        if (b->data->buildingClass == 2 && b->contract != 0) {
            // PORT (online removed): Billing::LogCBPurchase. UNVERIFIED (milestone 4, Tasks):
            // Tasks::CompleteSubtask(0xf, delivery id * 10 - 1 + contract).
            b->SpeedupBuilding();
            b->contractDone = true;
            b->OnContractCompleted(false);
            finishAfterSpeedup = true;
        } else {
            b->SpeedupBuilding();
        }
        speedingUp = false;
    }
    if (GameState::TutorialStep() != 0x56) BuildingHovers::HideArrow();
    BuildingHovers::Hide();
    BuildingHovers::Update(0.0, true);
}

void FactoryHoverWindow::OnSpeedUp(bool confirmed) {
    unsigned cost = (unsigned)GetSpeedUpCost();
    NotEnoughWindow::ResetRequirements();
    if (!itemBoosts) {
        NotEnoughWindow::AddRequirement(GameState::kCrystal, cost);
        if (!NotEnoughWindow::CheckRequirements()) {
            BuildingHovers::Hide();
            NotEnoughWindow::Show();
            return;
        }
        if (!confirmed) {
            ConfirmPurchaseWindow::SetParameters([this] { OnSpeedUp(true); }, cost, true);
            ConfirmPurchaseWindow::Show();
            return;
        }
    } else if (boostItem == 0) {
        std::fprintf(stderr, "ERROR: BuildProgressHoverWindow::OnSpeedUp() Cannot find a speed up item type for this case\n");
    } else {
        NotEnoughWindow::AddItemRequirement(boostItem, (unsigned)boostAmount);
        if (!NotEnoughWindow::CheckRequirements()) {
            BuildingHovers::Hide();
            // UNVERIFIED (milestone 4, Items): ItemShopWindow::Show / ShowShopItem(boostItem).
            return;
        }
    }
    // SoundsManager::PlaySound("ui_on_speedup", 1, false): sounds are milestone 5.
    if (building->data->buildingClass == 0xd) {
        BuildingHovers::Hide();
        OnSpeedUpFinished();
        return;
    }
    speedingUp = true;
    speedT = 1.f - left;
    if (GameState::TutorialStep() != 0x54) return;
    GUI::SetInteractionLock(true);
    BuildingHovers::HideArrow();
}

void FactoryHoverWindow::OnStopWithConfirm() {
    std::u32string title = SWPrintf(0x100, U"%s - %s", {StringTable::GetString(building->data->name.c_str()),
                                                         building->GetContractName()});
    PopupSelectionWindow::Show(title.c_str(), StringTable::GetString("CANCEL_CONFIRM"),
                               StringTable::GetString("HUD_OK"), IconManager::GetIcon("gold_confirm"),
                               [this] { OnStop(); }, StringTable::GetString("CANCEL"),
                               IconManager::GetIcon("red_x"), PopupSelectionWindow::Hide);
    PopupSelectionWindow::SetMentorIcon();
}

void FactoryHoverWindow::OnStop() {
    PopupSelectionWindow::Hide();
    Map::Building* b = building;
    const Contracts::Contract* del = b->data->delivery;
    bool multi = b == Map::GetCurrentFarm() && multiFarm && !entity;
    int c = GetCurrentContract();
    if (c == 0 && !multi) return;
    // The price is refunded: the running order's, or each active patch's.
    if (c != 0 && !multi) {
        const Contracts::ContractMission& m = del->missions[(size_t)c - 1];
        GameState::ChangeResourceAmount(GameState::kGold, (int)(m.price * multiCount));
        GameState::ChangeResourceAmount(m.priceResource, (int)(m.priceResourceCount * multiCount));
    } else {
        for (unsigned i = 0; i < PatchCount(b); ++i) {
            if (!b->IsSoilPatchActive(i)) continue;
            const Contracts::ContractMission& m = del->missions[(size_t)b->patchContract[i] - 1];
            GameState::ChangeResourceAmount(GameState::kGold, (int)m.price);
            GameState::ChangeResourceAmount(m.priceResource, (int)m.priceResourceCount);
        }
    }
    if (b->data->buildingClass == 0xd) {
        // A patch with a crop is cleared (state 0x13, no order).
        auto clear = [b](unsigned i) {
            AIBaseState* ai = b->patchEntities[i]->GetAI();
            if (ai->GetItem() == 0) return;
            ai->Clean();
            ai->ChangeState(0x13);
            b->resources[i] = 0x13;
            b->patchContract[i] = 0;
        };
        if (entity) clear((unsigned)PatchOf(entity));
        if (multi)
            for (unsigned i = 0; i < PatchCount(b); ++i)
                if (b->IsSoilPatchActive(i)) clear(i);
    } else {
        if (b->trainee) {
            EntityManager::RemoveEntity(b->trainee, true);
            b->trainee = nullptr;
        }
        b->contractDone = false;
        b->contract = 0;
    }
    BuildingHovers::HideArrow();
    BuildingHovers::Hide();
}

void FactoryHoverWindow::OnUpgrade(bool confirmed) {
    if (arrowShown) {
        arrowShown = false;
        BuildingHovers::HideArrow();
    }
    int activePatch = building->GetFirstActiveSoilPatch();
    if (!UpgradeBuilding(building, true)) return;
    if (!NotEnoughWindow::CheckRequirements() || !confirmed) {
        NotEnoughWindow::SetActionCallback([this] { OnUpgrade(true); }, StringTable::GetString("UPGRADE_ACTION"),
                                           false);
        NotEnoughWindow::Show();
        return;
    }
    NotEnoughWindow::Hide();
    if (activePatch == -1 && !building->HasActiveContract()) {
        BuildingHovers::Hide();
        UpgradeBuildingContinuation(building);
        return;
    }
    PopupSelectionWindow::Show(StringTable::GetString("UPGRADE_WILL_CANCEL_HEADER"),
                               StringTable::GetString("UPGRADE_WILL_CANCEL"), StringTable::GetString("HUD_OK"),
                               IconManager::GetIcon("gold_confirm"), [this] { OnStopAndUpgrade(true); },
                               StringTable::GetString("CANCEL"), IconManager::GetIcon("red_x"),
                               PopupSelectionWindow::Hide);
    PopupSelectionWindow::SetFirstGreen();
}

void FactoryHoverWindow::OnStopAndUpgrade(bool confirmed) {
    PopupSelectionWindow::Hide();
    bool savedMulti = multiFarm;
    Entity* savedEntity = entity;
    multiFarm = true;
    entity = nullptr;
    OnStop();
    multiFarm = savedMulti;
    entity = savedEntity;
    OnUpgrade(confirmed);
}

void FactoryHoverWindow::Update(float dt) {
    if (!shownHover || !building) return;
    int step = GameState::TutorialStep();
    if ((unsigned)(step - 0x53) < 2) {
        GUI::Window* b = order->visibleSelf ? order : speedup->visibleSelf ? speedup : nullptr;
        if (b) ArrowAtButton(root, b);
    }
    if (building == Map::GetCurrentFarm()) {
        arrowShown = false;
    } else {
        int x = (int)((building->minX + building->maxX) * 0.5f);
        int y = (int)((float)building->data->iconY + building->minY);
        Map::WorldCoordinatesToScreen(x, y);
        SetPosition(x, y);
        if (arrowShown) {
            if (!BuildingHovers::ArrowVisible()) {
                arrowShown = false;
            } else {
                GUI::Window* b = order->visibleSelf ? order : speedup->visibleSelf ? speedup : nullptr;
                if (b) ArrowAtButton(root, b);
            }
        }
    }
    if (speedingUp) {
        speedT += dt * 0.5f;
        if (speedT < 0.f) speedT = 0.f;
        else if (speedT > 1.f) OnSpeedUpFinished();
    }
    secondFrac += dt / (float)HUDWindow::GetDeltaTimeMultiplier();
    uint32_t now = Timer::GetGlobalTime();
    if (now == second) {
        if (secondFrac > 1.f) secondFrac = 1.f;
    } else {
        second = now;
        secondFrac = 0.f;
    }
    int c = GetCurrentContract();
    if (c == 0) {
        bool lockedOrder = (unsigned)building->level < Mission(building, selectedOrder).buildingLevel;
        progress.under->SetText(StringTable::GetString(lockedOrder ? "FACTORY_UNLOCK_TO_ORDER" : "FACTORY_READY_TO_ORDER"));
        progress.SetProgress(0.f);
        root->UpdatePosition();
        return;
    }
    left = 1.f - building->GetContractProgress(0, PatchOf(entity));
    leftNext = 1.f - building->GetContractProgress(1, PatchOf(entity));
    if (building->data->buildingClass != 0xd) {
        if (building->contractDone && !ignoreCompletion) {
            BuildingHovers::Hide();
            BuildingHovers::Update(0.0, true);
        }
    }
    if (building && building->data->buildingClass == 0xd && entity && !ignoreCompletion &&
        building->GetFarmState(PatchOf(entity)) == 6) {
        BuildingHovers::Hide();
        BuildingHovers::Update(0.0, true);
    }
    float fill;
    if (speedingUp) {
        left = 1.f - speedT;
        if (leftNext < 0.f) leftNext = 0.f;
        fill = speedT;
    } else {
        if (leftNext < 0.f) leftNext = 0.f;
        left = left + (leftNext - left) * secondFrac;
        fill = 1.f - left;
    }
    std::u32string text;
    if (!progressPercent) {
        int t = (int)(0.5f + (float)(unsigned)building->GetContractTime(PatchOf(entity)) * left);
        text = SWPrintf(0x100, StringTable::GetString("FACTORY_TIME_LEFT"),
                        {StringTable::GetNumericTimeString(t, t > 3600).c_str()});
    } else {
        text = SWPrintf(0x100, StringTable::GetString("BUILDING_BAKERY_PROGRESS"),
                        {ToWideString((int)((1.f - left) * 100.f))});
    }
    progress.under->SetText(text.c_str());
    progress.over->SetText(text.c_str());
    progress.SetProgress(fill);
    if (finishAfterSpeedup) {
        BuildingHovers::OnBuildingFinishedClick(building);
        finishAfterSpeedup = false;
    }
    root->UpdatePosition();
}

void FactoryHoverWindow::OnLaunchContract() {
    Map::Building* b = building;
    unsigned sel = selectedOrder;
    const Contracts::Contract* del = b->data->delivery;
    const Contracts::ContractMission& m = del->missions[sel];
    if ((unsigned)b->level < m.buildingLevel) {
        OnUpgrade(false);
        return;
    }
    bool multi = b == Map::GetCurrentFarm() && multiFarm && !entity;
    int c = GetCurrentContract();
    if (c == (int)sel + 1) {
        OnSpeedUp(false);
        return;
    }
    // Changing a running order (or replanting every patch): the new order must be affordable with
    // the old one's price back; then the old one is refunded and stopped.
    if (c != 0 || multi) {
        NotEnoughWindow::ResetRequirements();
        NotEnoughWindow::SetDescriptionText(StringTable::GetString("REQUIREMENT_MISSION"), nullptr);
        // Below the order's level, an order that makes an item names it (with its drop subtask and
        // the building's point).
        if ((unsigned)GameState::GetLevel() < m.playerLevel && m.item)
            NotEnoughWindow::SetItemToProduce(m.item->id, sel + del->id * 10, b->baseX, b->minY);
        NotEnoughWindow::AddRequirement(m.priceResource, m.priceResourceCount * multiCount);
        NotEnoughWindow::AddRequirement(GameState::kGold, m.price * multiCount);
        NotEnoughWindow::AddLevelRequirement(m.playerLevel);
        if (!NotEnoughWindow::CheckRequirements()) {
            std::u32string t = SWPrintf(0x100, StringTable::GetString("FACTORY_ORDER"), {m.title});
            NotEnoughWindow::SetActionCallback([this] { OnLaunchContract(); }, t.c_str(), false);
            NotEnoughWindow::Show();
            return;
        }
        if (!multi) {
            const Contracts::ContractMission& old = del->missions[(size_t)c - 1];
            GameState::ChangeResourceAmount(GameState::kGold, (int)(old.price * multiCount));
            GameState::ChangeResourceAmount(old.priceResource, (int)(old.priceResourceCount * multiCount));
        } else {
            for (unsigned i = 0; i < PatchCount(b); ++i) {
                if (!b->IsSoilPatchActive(i)) continue;
                const Contracts::ContractMission& old = del->missions[(size_t)b->patchContract[i] - 1];
                GameState::ChangeResourceAmount(GameState::kGold, (int)old.price);
                GameState::ChangeResourceAmount(old.priceResource, (int)old.priceResourceCount);
            }
        }
        if (b->data->buildingClass == 0xd) {
            // The patch's crop is dropped and it goes back to planting (state 0x12).
            auto replant = [b](unsigned i) {
                AIBaseState* ai = b->patchEntities[i]->GetAI();
                if (ai->GetItem() == 0) return;
                ai->Clean();
                ai->ChangeState(0x12);
                b->resources[i] = 0x12;
            };
            if (entity) replant((unsigned)PatchOf(entity));
            if (multi)
                for (unsigned i = 0; i < PatchCount(b); ++i)
                    if (b->IsSoilPatchActive(i)) replant(i);
        } else {
            if (b->trainee) {
                EntityManager::RemoveEntity(b->trainee, true);
                b->trainee = nullptr;
            }
            b->contractDone = false;
            b->contract = 0;
        }
    }
    NotEnoughWindow::ResetRequirements();
    NotEnoughWindow::SetDescriptionText(StringTable::GetString("REQUIREMENT_MISSION"), nullptr);
    NotEnoughWindow::AddRequirement(m.priceResource, m.priceResourceCount * multiCount);
    NotEnoughWindow::AddRequirement(GameState::kGold, m.price * multiCount);
    NotEnoughWindow::AddLevelRequirement(m.playerLevel);
    if ((unsigned)GameState::GetLevel() < m.playerLevel && m.item)
        NotEnoughWindow::SetItemToProduce(m.item->id, sel + del->id * 10, b->baseX, b->minY);
    if (!NotEnoughWindow::CheckRequirements()) {
        std::u32string t = SWPrintf(0x100, StringTable::GetString("FACTORY_ORDER"), {m.title});
        NotEnoughWindow::SetActionCallback([this] { OnLaunchContract(); }, t.c_str(), false);
        NotEnoughWindow::Show();
        return;
    }
    // SoundsManager::PlaySound("ui_make_order", 1, false): sounds are milestone 5.
    GameState::ChangeResourceAmount(GameState::kGold, -(int)(m.price * multiCount));
    GameState::ChangeResourceAmount(m.priceResource, -(int)(m.priceResourceCount * multiCount));
    if (b == Map::GetCurrentFarm()) {
        if (multiFarm) {
            for (unsigned i = 0; i < PatchCount(b); ++i)
                if (b->IsSoilPatchEmpty(i)) b->farmer->GetAI()->Farm((int)sel, (int)i, true);
            multiFarm = false;
            multiCount = 1;
            BuildingHovers::Hide();
        } else {
            b->farmer->GetAI()->Farm((int)sel, PatchOf(entity), true);
            BuildingHovers::Hide();
        }
    } else {
        b->LaunchContract(sel, -1);
        if (!arrowShown) {
            BuildingHovers::Hide();
        } else if (!GameState::IsCityTutorial()) {
            SetBuilding(building);
        }
    }
    if (GameState::TutorialStep() == 0x43) {
        GameState::tutorial = 0x44;
        BuildingHovers::SetArrowVisibleWindowLimit(0);
    }
    BuildingHovers::HideArrow();
    // UNVERIFIED (tutorial): at step 0x53 the tutorial opens the building at (0x1c, 0x13), moves to
    // 0x54 and points the arrow at its speed-up button.
}

void FactoryHoverWindow::ShowArrowAt(unsigned i) {
    if (!shownHover) return;
    selectedOrder = i;
    SetBuilding(building);
    BuildingHovers::SetArrowVisibleWindowLimit(3);
    // UNVERIFIED (milestone 4): TaskInfoWindow::BlockArrowLimitChangeOnce.
    if (order->visibleSelf) {
        BuildingHovers::SetArrowClickCallback(new GUI::Callback([this] { OnLaunchContract(); }));
        ArrowAtButton(root, order);
    } else if (speedup->visibleSelf) {
        BuildingHovers::SetArrowClickCallback(new GUI::Callback([this] { OnSpeedUp(false); }));
        ArrowAtButton(root, speedup);
    }
    if (GameState::TutorialStep() == 0x53) GUI::SetInteractionObjectLock(order, nullptr);
    arrowShown = true;
}

void FactoryHoverWindow::ShowArrowAtUpgrade() {
    if (!building) return;
    const std::vector<Contracts::ContractMission>& ms = building->data->delivery->missions;
    for (unsigned i = 0; i < ms.size(); ++i) {
        if ((unsigned)building->level < ms[i].buildingLevel) {
            ShowArrowAt(i);
            return;
        }
    }
}

void FactoryHoverWindow::SetBuilding(Map::Building* b) {
    itemBoosts = (int)GameState::GetSetting("item_boosts") == 1;
    BuildingHoverWindow::SetBuilding(b);
    f2c = root->w;
    f30 = root->h;
    if (!building) return;
    const Contracts::Contract* del = building->data->delivery;
    int current = GetCurrentContract();
    bool multi = building == Map::GetCurrentFarm() && multiFarm && !entity;
    const char32_t* name = StringTable::GetString(building->data->name.c_str());
    header->SetText(SWPrintf(0x100, U"%s", {name, building->level + 1}).c_str());
    if (current == 0) {
        jobText->SetText(StringTable::GetString("FACTORY_SELECT"));
    } else {
        jobText->SetText(SWPrintf(0x100, StringTable::GetString("FACTORY_PREPARING"),
                                  {del->missions[(size_t)current - 1].title}).c_str());
    }
    const Contracts::ContractMission& m = del->missions[selectedOrder];
    auto showRows = [this](Row* shown) {
        for (Row* r : {&resTimeRes, &resTimeItem3, &resTimeItem, &earned, &locked}) r->root->SetVisibility(r == shown);
    };
    bool running = true;   // the speed-up and stop buttons
    if ((unsigned)building->level < m.buildingLevel) {
        showRows(&locked);
        locked.text[0]->SetText(StringTable::GetString("FACTORY_ORDER_LOCKED"));
        order->SetVisibility(true);
        speedup->SetVisibility(false);
        stop->SetVisibility(false);
        SetIcon(orderIcon, "gold_upgrade");
        orderText->SetText(SWPrintf(0x100, StringTable::GetString("FACTORY_UNLOCK"), {name, m.title}).c_str());
        running = false;
    } else {
        int reward = (int)((m.rewardGold != 0 ? m.rewardGold : m.rewardResourceCount) * multiCount);
        int rewardType = m.rewardGold != 0 ? GameState::kGold : m.rewardResource;
        int time = m.timePlant;
        for (int t : m.growTimes) time += t;
        int price = (int)m.price, priceRes = (int)m.priceResourceCount;
        if (m.rewardGold != 0 && m.rewardResourceCount != 0)
            std::fprintf(stderr, "ERROR: FactoryHoverWindow::SetBuilding() Cannot show two rewards in mission %d in contract %d\n",
                         (int)selectedOrder, (int)del->id);
        bool other = current != (int)selectedOrder + 1;
        if (reward == 0 || other) {
            int count = (price > 0) + (priceRes > 0) + (reward > 0) + (time > 0);
            std::u32string timeText = StringTable::GetTimeString(time, false);
            if (count == 4) {
                std::fprintf(stderr, "ERROR: FactoryHoverWindow::SetBuilding() Cannot show three requirements and a reward in mission %d in contract %d\n",
                             (int)selectedOrder, (int)del->id);
            } else if (count == 3 && reward == 0) {
                showRows(&resTimeItem3);
                SetIcon(resTimeItem3.icon[0], "Icon_16_time");
                resTimeItem3.text[0]->SetText(timeText.c_str());
                SetIcon(resTimeItem3.icon[1], GameState::GetResourceIconName(GameState::kGold));
                resTimeItem3.text[1]->SetText(ToWideString(price));
                SetIcon(resTimeItem3.icon[2], GameState::GetResourceIconName(m.priceResource));
                resTimeItem3.text[2]->SetText(ToWideString(priceRes));
            } else if (count == 3) {
                showRows(&resTimeRes);
                SetIcon(resTimeRes.icon[0], "Icon_16_time");
                resTimeRes.text[0]->SetText(timeText.c_str());
                SetIcon(resTimeRes.icon[1], GameState::GetResourceIconName(price != 0 ? GameState::kGold : m.priceResource));
                resTimeRes.text[1]->SetText(ToWideString(price != 0 ? price : priceRes));
                SetIcon(resTimeRes.icon[2], GameState::GetResourceIconName(rewardType));
                resTimeRes.text[2]->SetText(ToWideString(reward));
            } else if (count == 2) {
                if (time == 0)
                    std::fprintf(stderr, "ERROR: FactoryHoverWindow::SetBuilding() Time was expected in mission %d in contract %d\n",
                                 (int)selectedOrder, (int)del->id);
                if (price == 0)
                    std::fprintf(stderr, "ERROR: FactoryHoverWindow::SetBuilding() Reward was expected in mission %d in contract %d\n",
                                 (int)selectedOrder, (int)del->id);
                showRows(&resTimeItem);
                SetIcon(resTimeItem.icon[0], "Icon_16_time");
                resTimeItem.text[0]->SetText(timeText.c_str());
                SetIcon(resTimeItem.icon[1], GameState::GetResourceIconName(GameState::kGold));
                resTimeItem.text[1]->SetText(ToWideString(price));
            }
            if (other) {
                order->SetVisibility(true);
                speedup->SetVisibility(false);
                stop->SetVisibility(false);
                if (current == 0 && !multi) {
                    SetIcon(orderIcon, "gold_flag_02");
                    orderText->SetText(SWPrintf(0x100, StringTable::GetString("FACTORY_ORDER"), {m.title}).c_str());
                } else {
                    SetIcon(orderIcon, "gold_refresh");
                    orderText->SetText(SWPrintf(0x100, StringTable::GetString("FACTORY_CHANGE_TO"), {m.title}).c_str());
                }
                running = false;
            }
        } else {
            showRows(&earned);
            char key[64];
            std::snprintf(key, sizeof key, "RES_COUNT_%s", GameState::GetResourceName(rewardType));
            std::u32string forms = StringTable::GetCountableString(StringTable::GetString(key), reward);
            std::u32string amount = SWPrintf(0x40, forms.c_str(), {reward});
            std::u32string timeText = StringTable::GetTimeString(time, false);
            earned.text[0]->SetText(SWPrintf(0x100, StringTable::GetString("FACTORY_ALREADY_EARNED"),
                                             {amount.c_str(), timeText.c_str()}).c_str());
            SetIcon(earned.icon[0], GameState::GetResourceIconName(rewardType));
        }
    }
    if (running) {
        order->SetVisibility(false);
        speedup->SetVisibility(true);
        stop->SetVisibility(true);
        bool canStop = GameState::GetLevel() >= 3 &&
                       (GameState::GetTutorialType() != 1 || GameState::secondTutorial == 0x100);
        stop->SetEnabled(canStop);
        SetIcon(speedupIcon, "gold_confirm");
        speedupText->SetText(StringTable::GetString("FACTORY_COMPLETE_NOW"));
        speedupCrystal->SetVisibility(!itemBoosts);
        speedupItem->SetVisibility(itemBoosts);
        SetIcon(speedupItem, "hourglass_speed_35");
        boostItem = 0x269;
        boostAmount = Items::GetAmountToBoostTime(
            0x269, (uint32_t)(int)((float)(unsigned)b->GetContractTime(PatchOf(entity)) *
                                   (1.f - b->GetContractProgress(0, PatchOf(entity)))));
        speedupPrice->SetText(ToWideString(itemBoosts ? boostAmount : GameState::AdjustCrystalCost((int)m.speedupCost2)));
        SetIcon(stopIcon, "impossible");
        stopText->SetText(StringTable::GetString("FACTORY_STOP"));
    }
    for (unsigned i = 0; i < 5 && i < del->missions.size(); ++i) {
        const Contracts::ContractMission& mm = del->missions[i];
        bool unlocked = mm.buildingLevel <= (unsigned)building->level;
        items[i].ordered->SetVisibility(current == (int)i + 1);
        items[i].icon->SetTexture(Resources::GetDecoration(mm.icon.c_str()), true);
        items[i].icon->SetEnabled(unlocked);
        items[i].lock->SetVisibility(!unlocked);
    }
    for (unsigned i = 0; i < 5; ++i) selected[i]->SetVisibility(selectedOrder == i);
}

// ---- BaseHoverWindow ----

BaseHoverWindow::BaseHoverWindow() {
    name = "BaseHoverWindow";
    f2c = 1;
    f30 = 1;
}

BaseHoverWindow::~BaseHoverWindow() {
    delete root;
    delete footer;
}

void BaseHoverWindow::InitBase(bool footerScreenSpace) {
    root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Unit_info_hint.xml", "Unit_info_hint.png", 1.f, 0, 0,
                           0, 0, false, 1.f);
    root->SetVisibility(false);
    border = GUI::GetWindowTyped<GUI::Window>(root, "golden_border_box");
    borderW = border->w;
    footer = GUI::RegisterUI("../resource/kingdom_ui/1Original/Hint_window_footer.xml", "Hint_window_footer.png", 1.f,
                             0, 0, 0, 0, false, 1.f);
    footer->SetVisibility(false);
    footer->SetScreenSpace(footerScreenSpace);
}

void BaseHoverWindow::SetZ(float z) {
    root->SetZ(z + 0.001f);
    footer->SetZ(z + 0.001f);
}

bool BaseHoverWindow::Click(int x, int y, bool) {
    if (!shownHover) return false;
    return root->x < x && x < root->w + root->x && root->y < y && y <= root->y + root->h + 0x13;
}

// The flag is set first, so BuildingHoverWindow::Show returns at once (the window is not moved on
// top), as on the original.
void BaseHoverWindow::Show() {
    if (shownHover) return;
    shownHover = true;
    BuildingHoverWindow::Show();
    root->SetVisibility(true);
    footer->SetVisibility(true);
}

void BaseHoverWindow::Hide() {
    if (!shownHover) return;
    shownHover = false;
    BuildingHoverWindow::Hide();
    root->SetVisibility(false);
    footer->SetVisibility(false);
}

void BaseHoverWindow::SetPosition(int x, int y) {
    root->SetPosition(x, y);
    footer->SetPosition(x + (border->w - borderW) / 2, f30 - 2 + y);
}

void BaseHoverWindow::SetSize(int w, int h) {
    f2c = w;
    f30 = h;
    root->SetSize((unsigned)w, (unsigned)h);
    border->SetSize((unsigned)w, (unsigned)h);
}

void BaseHoverWindow::FixWindowPosition(int& x, int& y) {
    if (x < 0) x = 0;
    if (y < -GetPaddingTop()) y = -GetPaddingTop();
    if (GUI::ScreenWidth() < f2c + x) x = GUI::ScreenWidth() - f2c;
    if (GUI::ScreenHeight() < f30 + y) y = GUI::ScreenHeight() - f30;
}

// ---- LivingHoverWindow ----

namespace {
// CallbackCenterOnWorker::Perform @0x3754ec: the arrow over a resident and the camera on them.
void CenterOnWorker(Entity* e) {
    if (!e) return;
    BuildingHovers::Hide();
    float x = 0.f, y = 0.f;
    e->GetWorldPos(x, y);
    if (!e->IsActive()) return;
    if (Render::Sprite* s = e->GetSprite()) {
        BuildingHovers::ArrowAt(x, (y - s->h) - 20.f, false, false, false, false, false, false, false);
        // UNVERIFIED (tutorial arrows): BuildingHovers::FollowEntity(e) keeps the arrow over them.
    }
    // UNVERIFIED: the original animates the camera there at zoom 0.4 (Render::CenterOn's animated path).
    Render::CenterOn(x, y);
}
}  // namespace

LivingHoverWindow::LivingHoverWindow() { name = "LivingHoverWindow"; }

LivingHoverWindow::~LivingHoverWindow() {
    delete header;
    delete upgrade;
    delete separator;
    delete taxes;
    delete progress;
    delete separator2;
    delete hereLives;
    for (Resident& r : residents) delete r.root;
}

unsigned LivingHoverWindow::GetResidentCount() {
    if (!building) return 0;
    unsigned n = (unsigned)building->GetResidentCount();
    return n < 5 ? n : 4;
}

void LivingHoverWindow::OnUpgrade() {
    if (arrowShown) {
        BuildingHovers::HideArrow();
        arrowShown = false;
    }
    UpgradeBuilding(building, false);
}

void LivingHoverWindow::ShowArrowAtUpgrade() {
    if (!building) return;
    BuildingHovers::SetArrowVisibleWindowLimit(2);
    // UNVERIFIED (milestone 4): TaskInfoWindow::BlockArrowLimitChangeOnce.
    BuildingHovers::SetArrowClickCallback(new GUI::Callback([this] { OnUpgrade(); }));
    BuildingHovers::ArrowAt((float)(upgrade->w + upgrade->x), (float)(upgrade->y + upgrade->h / 2), false, true, false,
                            false, true, false, false);
    arrowShown = true;
}

void LivingHoverWindow::Init() {
    InitBase(true);
    float hs = GUI::GetHoverScaleFactor(1.f, 1.f);
    auto load = [hs](const char* xml, const char* png) {
        std::string path = std::string("../resource/kingdom_ui/1Original/") + xml;
        GUI::Window* w = GUI::RegisterUI(path.c_str(), png, hs, 0, 0, 0, 0, false, 1.f);
        w->SetVisibility(false);
        return w;
    };
    header = load("Unit_info_hint_header.xml", "Unit_info_hint_header.png");
    headerText = GUI::GetWindowTyped<GUI::Textfield>(header, "header_text");
    upgrade = load("Button_house_upgrade.xml", "Button_house_upgrade.png");
    upgradeText = GUI::GetWindowTyped<GUI::Textfield>(upgrade, "text_upgrade");
    upgradeClick = GUI::GetWindowTyped<GUI::Button>(upgrade, "clickArea");
    separator = load("Window_hints_sepparator.xml", "Window_hints_sepparator.png");
    taxes = load("Hint_taxes_earned_holder.xml", "Hint_taxes_earned_holder.png");
    taxesIcon = GUI::GetWindowTyped<GUI::Window>(taxes, "icon_25_02");
    taxesIcon->takesZ = true;
    taxesText = GUI::GetWindowTyped<GUI::Textfield>(taxes, "text_item_from_02");
    progress = load("Progress_bar_mobile.xml", "Progress_bar_mobile.png");
    bar = GUI::GetWindowTyped<GUI::Window>(progress, "unit_info_progress_bar");
    bar->SetFrame(1, true);
    fill = GUI::DuplicateWindow(bar, true, false, nullptr);
    fill->SetFrame(100, false);
    bar->PrependChild(fill);
    under = GUI::GetWindowTyped<GUI::Textfield>(progress, "unit_info_progress_bar.text_under");
    over = GUI::GetWindowTyped<GUI::Textfield>(progress, "unit_info_progress_bar.text_over");
    separator2 = load("Window_hints_sepparator.xml", "Window_hints_sepparator.png");
    hereLives = load("Sepparator_here_works.xml", "Sepparator_here_works.png");
    GUI::GetWindowTyped<GUI::Textfield>(hereLives, "text_here_works")->SetText(StringTable::GetString("BUILDING_HERE_LIVES"));
    for (Resident& r : residents) {
        r.root = load("Button_house_pick_small.xml", "Button_house_pick_small.png");
        r.bg = GUI::GetWindowTyped<GUI::Window>(r.root, "green_button_bg_compact");
        r.icon = GUI::GetWindowTyped<GUI::Window>(r.root, "icon_gold_find");
        r.icon->takesZ = true;
        r.job = GUI::GetWindowTyped<GUI::Textfield>(r.root, "text_job");
        r.name = GUI::GetWindowTyped<GUI::Textfield>(r.root, "text_name");
        r.clickArea = GUI::GetWindowTyped<GUI::Button>(r.root, "clickArea");
    }
}

// Every part, top to bottom: the header, the Improve button, a separator, the taxes, the progress
// bar, a separator, "Living here:" and the resident rows.
#define LIVING_PARTS {header, upgrade, separator, taxes, progress, separator2, hereLives}

void LivingHoverWindow::SetZ(float z) {
    BaseHoverWindow::SetZ(z);
    for (GUI::Window* w : LIVING_PARTS) w->SetZ(z - 0.0001f);
    for (unsigned i = 0; i < GetResidentCount(); ++i) residents[i].root->SetZ(z - 0.0001f);
}

bool LivingHoverWindow::Click(int x, int y, bool pressed) {
    if (header->Click(x, y, pressed, false) || upgrade->Click(x, y, pressed, false) ||
        progress->Click(x, y, pressed, false))
        return true;
    for (unsigned i = 0; i < GetResidentCount(); ++i)
        if (residents[i].root->Click(x, y, pressed, false)) return true;
    return BaseHoverWindow::Click(x, y, pressed);
}

void LivingHoverWindow::Show() {
    if (shownHover) return;
    BaseHoverWindow::Show();
    for (GUI::Window* w : LIVING_PARTS) w->SetVisibility(true);
    for (unsigned i = 0; i < GetResidentCount(); ++i) residents[i].root->SetVisibility(true);
}

void LivingHoverWindow::Hide() {
    if (!shownHover) return;
    for (GUI::Window* w : LIVING_PARTS) w->SetVisibility(false);
    for (unsigned i = 0; i < GetResidentCount(); ++i) residents[i].root->SetVisibility(false);
    BaseHoverWindow::Hide();
}

void LivingHoverWindow::SetPosition(int x, int y) {
    int nx = x - root->w / 2, ny = y - f30;
    FixWindowPosition(nx, ny);
    BaseHoverWindow::SetPosition(nx, ny);
    int left = nx + padding + GetPaddingLeft();
    ny += GetPaddingTop();
    header->SetPosition(left, ny);
    ny += header->h;
    upgrade->SetPosition(left - 4, ny);
    ny += upgrade->h;
    separator->SetPosition(left, ny);
    ny += separator->h;
    taxes->SetPosition(left + 8, ny);
    ny += taxes->h;
    progress->SetPosition(left, ny);
    ny += progress->h;
    separator2->SetPosition(left, ny);
    ny += separator2->h;
    hereLives->SetPosition(left, ny);
    ny += hereLives->h;
    for (unsigned i = 0; i < GetResidentCount(); ++i) {
        residents[i].root->SetPosition(left, ny);
        ny += residents[i].root->h;
    }
}

void LivingHoverWindow::Update(float) {
    if (!building || !shownHover) return;
    if (arrowShown && !BuildingHovers::ArrowVisible()) arrowShown = false;
    int x = (int)((building->minX + building->maxX) * 0.5f);
    int y = (int)((float)building->data->iconY + building->minY);
    Map::WorldCoordinatesToScreen(x, y);
    SetPosition(x, y);
    if (arrowShown)
        BuildingHovers::ArrowAt((float)(upgrade->w + upgrade->x), (float)(upgrade->y + upgrade->h / 2), false, true,
                                false, false, true, false, false);
    int collect = building->data->collectTime;
    int elapsed = (int)(Timer::GetGlobalTime() - building->stateTime);
    if (elapsed >= collect) elapsed = collect;
    std::u32string text = SWPrintf(0x100, StringTable::GetString("HOUSE_INCOME"),
                                   {StringTable::GetTimeString(collect - elapsed, false).c_str()});
    under->SetText(text.c_str());
    over->SetText(text.c_str());
    if (bar->sprite && fill->sprite) {
        float p = (float)elapsed / (float)(unsigned)collect;
        if (p > 1.f) p = 1.f;
        float f = 0.125f + p * 0.75f;
        fill->sprite->u1 = f;
        fill->sprite->w = bar->sprite->w * f;
        over->ClipWithRect(fill->x, 0, (int)((float)fill->x + (float)bar->w * f), 0x200);
    }
}

void LivingHoverWindow::SetBuilding(Map::Building* b) {
    BaseHoverWindow::SetBuilding(b);
    if (!building) return;
    const char32_t* bname = StringTable::GetString(building->data->name.c_str());
    headerText->SetText(SWPrintf(0x100, U"%s", {bname, building->level + 1}).c_str());
    upgrade->SetEnabled((unsigned)building->level < building->data->GetMaxUpgradeCount());
    upgradeText->SetText(SWPrintf(0x100, StringTable::GetString(upgrade->enabled ? "BUILDING_UPGRADE" : "HOUSE_INFO_LEVEL"),
                                  {ToWideString(building->level + 2)}).c_str());
    upgradeClick->SetOnClick([this] { OnUpgrade(); });
    for (unsigned i = 0; i < GetResidentCount(); ++i) {
        Resident& r = residents[i];
        Entity* e = building->livers[i];
        r.icon->SetTexture(IconManager::GetIcon(e->IsActive() ? "gold_find" : "gold_profile"), true);
        const char* job = e->GetEntityData()->clas == 2 ? "SPAWN_FARMER" : "SPAWN_WORKER";
        r.job->SetText(StringTable::GetGenderString(StringTable::GetString(job), e->IsMale()).c_str());
        r.name->SetText(SWPrintf(0x100, U"%s %s", {e->firstName, e->surname}).c_str());
        r.clickArea->SetOnClick([e] { CenterOnWorker(e); });
        r.clickArea->SetActionSound("ui_click", 1);
    }
    taxesIcon->SetTexture(IconManager::GetIcon(GameState::GetResourceIconName(GameState::kGold)), false);
    int gold = building->GetResidentCount() * 5 + building->data->collectMoney;
    char key[64];
    std::snprintf(key, sizeof key, "RES_COUNT_%s", GameState::GetResourceName(GameState::kGold));
    std::u32string forms = StringTable::GetCountableString(StringTable::GetString(key), gold);
    std::u32string amount = SWPrintf(0x40, forms.c_str(), {gold});
    std::u32string time = StringTable::GetTimeString(building->data->collectTime, false);
    taxesText->SetText(SWPrintf(0x100, StringTable::GetString("FACTORY_ALREADY_EARNED"),
                                {amount.c_str(), time.c_str()}).c_str());
    SetSize(header->w + padding * 2,
            residents[0].root->h * (int)GetResidentCount() + 8 + taxes->h + header->h + upgrade->h + progress->h +
                hereLives->h + separator->h * 2 + GetPaddingTop());
    Update(0.f);
}

// ---- StorageHoverWindow ----

StorageHoverWindow::StorageHoverWindow() { name = "StorageHoverWindow"; }

StorageHoverWindow::~StorageHoverWindow() {
    for (GUI::Window* w : {header, upgrade, separator, info, separator2, hire, hireCrystals}) delete w;
    for (Line& l : lines) delete l.root;
}

unsigned StorageHoverWindow::GetResourceCount() {
    if (!building) return 0;
    if ((unsigned)building->GetResidentCount() >= 9) return 8;
    unsigned n = 0;
    for (int i = 0; i < 8; ++i)
        if (building->resources[i] > 0) ++n;
    return n;
}

void StorageHoverWindow::OnUpgrade() {
    if (arrowShown) {
        BuildingHovers::HideArrow();
        arrowShown = false;
    }
    UpgradeBuilding(building, false);
}

void StorageHoverWindow::ShowArrowAtUpgrade() {
    if (!building) return;
    BuildingHovers::SetArrowVisibleWindowLimit(2);
    // UNVERIFIED (milestone 4): TaskInfoWindow::BlockArrowLimitChangeOnce.
    BuildingHovers::SetArrowClickCallback(new GUI::Callback([this] { OnUpgrade(); }));
    BuildingHovers::ArrowAt((float)(upgrade->w + upgrade->x), (float)(upgrade->y + upgrade->h / 2), false, true, false,
                            false, true, false, false);
    arrowShown = true;
}

// Only the tutorial's step 0x80 keeps the window over the storage.
void StorageHoverWindow::Update(float) {
    if (!building || !shownHover || GameState::tutorial != 0x80) return;
    if (arrowShown && !BuildingHovers::ArrowVisible()) arrowShown = false;
    int x = (int)((building->minX + building->maxX) * 0.5f);
    int y = (int)((float)building->data->iconY + building->minY);
    Map::WorldCoordinatesToScreen(x, y);
    SetPosition(x, y);
    if (arrowShown)
        BuildingHovers::ArrowAt((float)(upgrade->w + upgrade->x), (float)(upgrade->y + upgrade->h / 2), false, true,
                                false, false, true, false, false);
}

void StorageHoverWindow::OnHireGoblinCrystals(bool confirmed) {
    unsigned cost = (unsigned)GameState::AdjustCrystalCost(goblinCost[(size_t)GameState::GetGoblinCount()].crystals);
    NotEnoughWindow::ResetRequirements();
    NotEnoughWindow::AddRequirement(GameState::kCrystal, cost);
    if (!NotEnoughWindow::CheckRequirements()) {
        BuildingHovers::Hide();
        NotEnoughWindow::Show();
        return;
    }
    if (!confirmed) {
        ConfirmPurchaseWindow::SetParameters([this] { OnHireGoblinCrystals(true); }, cost, true);
        ConfirmPurchaseWindow::Show();
        return;
    }
    // SoundsManager::PlaySound("ui_buy_with_crystals", 1, false): sounds are milestone 5.
    GameState::ChangeResourceAmount(GameState::kCrystal, -(int)cost);
    // PORT (online removed): Billing::LogCBPurchase(0x14, id, cost).
    building->HireGolbin();
    BuildingHovers::Hide();
}

void StorageHoverWindow::OnHireGoblinGold() {
    if (GameState::tutorial == 0x2e) {
        // SoundsManager::PlaySound("ui_buy_with_gold", 1, false): sounds are milestone 5.
        GameState::ChangeResourceAmount(GameState::kGold, -100);
        building->HireGolbin();
        // UNVERIFIED (tutorial): BuildingHovers::HideWorldDialog.
        BuildingHovers::Hide();
        GameState::tutorial = 0x2f;
        BuildingHovers::SetArrowVisibleWindowLimit(0);
        BuildingHovers::HideArrow();
        return;
    }
    const GoblinCost& c = goblinCost[(size_t)GameState::GetGoblinCount()];
    NotEnoughWindow::ResetRequirements();
    NotEnoughWindow::AddRequirement(GameState::kGold, (unsigned)c.gold);
    NotEnoughWindow::AddLevelRequirement((unsigned)c.level);
    if (NotEnoughWindow::CheckRequirements()) {
        // SoundsManager::PlaySound("ui_buy_with_gold", 1, false): sounds are milestone 5.
        GameState::ChangeResourceAmount(GameState::kGold, -c.gold);
        building->HireGolbin();
        BuildingHovers::Hide();
        return;
    }
    BuildingHovers::Hide();
    NotEnoughWindow::Show();
}

void StorageHoverWindow::Init() {
    usesZRange = true;
    InitBase(true);
    float hs = GUI::GetHoverScaleFactor(1.f, 1.f);
    auto load = [hs](const char* xml, const char* png) {
        std::string path = std::string("../resource/kingdom_ui/1Original/") + xml;
        GUI::Window* w = GUI::RegisterUI(path.c_str(), png, hs, 0, 0, 0, 0, false, 1.f);
        w->SetVisibility(false);
        return w;
    };
    info = load("Info_line_storage.xml", "Info_line_storage.png");
    space = GUI::GetWindowTyped<GUI::Textfield>(info, "text_space");
    goblins = GUI::GetWindowTyped<GUI::Textfield>(info, "text_goblins");
    for (Line& l : lines) {
        l.root = load("Unit_info_info_line.xml", "Unit_info_info_line.png");
        l.name = GUI::GetWindowTyped<GUI::Textfield>(l.root, "unit_info_line_button.text_res");
        l.normal = GUI::GetWindowTyped<GUI::Textfield>(l.root, "unit_info_line_button.text");
        l.nearFull = GUI::GetWindowTyped<GUI::Textfield>(l.root, "unit_info_line_button.text_80_percent");
        l.full = GUI::GetWindowTyped<GUI::Textfield>(l.root, "unit_info_line_button.text_full");
        l.icon = GUI::GetWindowTyped<GUI::Window>(l.root, "unit_info_line_button.icon_place_holder");
        l.icon->takesZ = true;
    }
    upgrade = load("Button_house_upgrade.xml", "Button_house_upgrade.png");
    upgradeText = GUI::GetWindowTyped<GUI::Textfield>(upgrade, "text_upgrade");
    upgradeClick = GUI::GetWindowTyped<GUI::Button>(upgrade, "clickArea");
    hire = load("Buy_goblin_holder.xml", "Buy_goblin_holder.png");
    hire->SetOnClick([this] { OnHireGoblinGold(); });
    hireText = GUI::GetWindowTyped<GUI::Textfield>(hire, "button_hire_goblin.text_hire_regular");
    hirePrice = GUI::GetWindowTyped<GUI::Textfield>(hire, "button_hire_goblin.text_price_regular");
    hireIcon = GUI::GetWindowTyped<GUI::Window>(hire, "button_hire_goblin.icon_bottom_bar_shop");
    hireLocked = GUI::GetWindowTyped<GUI::Window>(hire, "button_hire_goblin.locked_goblin");
    GUI::GetWindowTyped<GUI::Window>(hire, "button_hire_goblin_crystal")->SetVisibility(false);
    hireCrystals = load("Buy_goblin_holder.xml", "Buy_goblin_holder.png");
    hireCrystals->SetOnClick([this] { OnHireGoblinCrystals(false); });
    hireCrystalsText = GUI::GetWindowTyped<GUI::Textfield>(hireCrystals, "button_hire_goblin_crystal.text_hire_premium");
    hireCrystalsPrice = GUI::GetWindowTyped<GUI::Textfield>(hireCrystals, "button_hire_goblin_crystal.text_price");
    GUI::GetWindowTyped<GUI::Window>(hireCrystals, "button_hire_goblin")->SetVisibility(false);
    header = load("Unit_info_hint_header.xml", "Unit_info_hint_header.png");
    headerText = GUI::GetWindowTyped<GUI::Textfield>(header, "header_text");
    separator = load("Window_hints_sepparator.xml", "Window_hints_sepparator.png");
    separator2 = load("Window_hints_sepparator.xml", "Window_hints_sepparator.png");
    // goblin_cost "gold|crystals|level;...": the price of the next goblin by how many there are.
    // [0] is 100 gold (its crystals and level are not set on the original; 0 here).
    Setting costs("goblin_cost");
    goblinCostCount = costs.GetChildrenCount();
    goblinCost.assign(goblinCostCount + 1, GoblinCost());
    goblinCost[0].gold = 100;
    for (unsigned i = 0; i < goblinCostCount; ++i) {
        Setting c = costs.GetChild(i);
        goblinCost[i + 1] = {(int)c.GetChild(0).GetFloat(), (int)c.GetChild(1).GetFloat(), (int)c.GetChild(2).GetFloat()};
    }
}

void StorageHoverWindow::SetZ(float z) {
    BaseHoverWindow::SetZ(z);
    float pz = z - 0.0001f;
    for (GUI::Window* w : {header, info, separator, separator2}) w->SetZ(pz);
    for (unsigned i = 0; i < GetResourceCount(); ++i) lines[i].root->SetZ(pz);
    for (GUI::Window* w : {upgrade, hire, hireCrystals}) w->SetZ(pz);
}

bool StorageHoverWindow::Click(int x, int y, bool pressed) {
    if (!shownHover) return false;
    if (BuildingHovers::ClickOnArrow(x, y, pressed) || header->Click(x, y, pressed, false)) return true;
    for (unsigned i = 0; i < GetResourceCount(); ++i)
        if (lines[i].root->Click(x, y, pressed, false)) return true;
    if (upgrade->Click(x, y, pressed, false) || hire->Click(x, y, pressed, false) ||
        hireCrystals->Click(x, y, pressed, false) || GameState::tutorial == 0x2e)
        return true;
    return BaseHoverWindow::Click(x, y, pressed);
}

void StorageHoverWindow::Hide() {
    if (!shownHover) return;
    for (GUI::Window* w : {header, info, separator, separator2}) w->SetVisibility(false);
    for (Line& l : lines) l.root->SetVisibility(false);
    for (GUI::Window* w : {upgrade, hire, hireCrystals}) w->SetVisibility(false);
    BaseHoverWindow::Hide();
}

void StorageHoverWindow::Show() {
    if (shownHover) return;
    BaseHoverWindow::Show();
    for (GUI::Window* w : {header, info, separator, separator2}) w->SetVisibility(true);
    bool tutorialHire = GameState::tutorial == 0x2d;
    for (unsigned i = 0; i < GetResourceCount(); ++i) lines[i].root->SetVisibility(!tutorialHire);
    upgrade->SetVisibility(!tutorialHire);
    // PORT: the price row is read only within the table (the original reads past it with more
    // goblins than "goblin_cost" lists).
    size_t n = (size_t)GameState::GetGoblinCount();
    const GoblinCost& c = goblinCost[n < goblinCost.size() ? n : goblinCost.size() - 1];
    hirePrice->SetText(SWPrintf(0x100, U"%d", {c.gold}).c_str());
    // UNVERIFIED (tutorial): at step 0x2d the price text is formatted a second time.
    hireCrystalsPrice->SetText(ToWideString(GameState::AdjustCrystalCost(c.crystals)));
    if (tutorialHire) {
        GameState::tutorial = 0x2e;
        SetSize(header->w + padding * 2,
                header->h + info->h + 10 + hire->h + separator->h * 2 + GetPaddingTop());
        int x = (int)((building->minX + building->maxX) * 0.5f);
        int y = (int)((float)building->data->iconY + building->minY);
        Map::WorldCoordinatesToScreen(x, y);
        SetPosition(x, y);
    }
}

void StorageHoverWindow::SetPosition(int x, int y) {
    int nx = x - root->w / 2, ny = y - f30;
    FixWindowPosition(nx, ny);
    BaseHoverWindow::SetPosition(nx, ny);
    int left = nx + padding + GetPaddingLeft();
    ny += GetPaddingTop();
    bool tutorialHire = GameState::tutorial == 0x2e;
    header->SetPosition(left, ny);
    ny += header->h;
    if (upgrade->visibleSelf || !tutorialHire) {
        upgrade->SetPosition(left, ny);
        ny += upgrade->h;
    }
    info->SetPosition(left, ny);
    ny += info->h;
    separator->SetPosition(left, ny);
    ny += separator->h;
    for (unsigned i = 0; i < GetResourceCount(); ++i) {
        if (!lines[i].root->visibleSelf && tutorialHire) continue;
        lines[i].root->SetPosition(left, ny);
        ny += lines[i].root->h + 2;
    }
    separator2->SetPosition(left, ny);
    ny += separator2->h;
    hire->SetPosition(left, ny);
    if (hire->visibleSelf) ny += hire->h;
    hireCrystals->SetPosition(left, ny);
    if (!tutorialHire) return;
    BuildingHovers::SetArrowVisibleWindowLimit(1);
    GUI::SetInteractionObjectLock(hire, nullptr);
    BuildingHovers::SetArrowClickCallback(new GUI::Callback([this] { OnHireGoblinGold(); }));
    BuildingHovers::ArrowAt((float)(hire->w + hire->x), (float)(hire->y + hire->h / 2), false, true, false, false,
                            true, false, false);
}

void StorageHoverWindow::SetBuilding(Map::Building* b) {
    BaseHoverWindow::SetBuilding(b);
    if (!building) return;
    int max = GameState::resourceAmountMax;
    headerText->SetText(SWPrintf(0x100, U"%s", {StringTable::GetString(building->data->name.c_str()), building->level + 1}).c_str());
    space->SetText(SWPrintf(0x100, StringTable::GetString("STORAGE_SPACE"), {ToWideString(max)}).c_str());
    goblins->SetText(SWPrintf(0x100, StringTable::GetString("STORAGE_GOBLINS"), {ToWideString(GameState::GetGoblinCount())}).c_str());
    upgrade->SetEnabled((unsigned)building->level < building->data->GetMaxUpgradeCount());
    upgradeText->SetText(SWPrintf(0x100, StringTable::GetString(upgrade->enabled ? "BUILDING_UPGRADE" : "HOUSE_INFO_LEVEL"),
                                  {ToWideString(building->level + 2)}).c_str());
    upgradeClick->SetOnClick([this] { OnUpgrade(); });
    // A line per resource held: white, yellow from 80% of the space, red when full.
    unsigned row = 0;
    for (int r = 0; r < 8 && row < 8; ++r) {
        int amount = building->resources[r];
        if (amount <= 0) continue;
        Line& l = lines[row++];
        l.name->SetText(GameState::GetResourceGameName(r));
        l.icon->SetTexture(IconManager::GetIcon(GameState::GetResourceIconName(r)), false);
        std::u32string t = SWPrintf(0x100, U"%d/%d", {amount, max});
        int nearFull = (int)((double)max * 0.8);
        l.normal->SetVisibility(amount < nearFull);
        if (l.normal->visibleSelf) l.normal->SetText(t.c_str());
        l.nearFull->SetVisibility(nearFull <= amount && amount < max);
        if (l.nearFull->visibleSelf) l.nearFull->SetText(t.c_str());
        l.full->SetVisibility(max <= amount);
        if (l.full->visibleSelf) l.full->SetText(t.c_str());
    }
    hire->SetVisibility(false);
    hireCrystals->SetVisibility(false);
    hireCrystalsText->SetText(StringTable::GetString("SPAWN_GOB"));
    unsigned count = (unsigned)GameState::GetGoblinCount();
    if (count < goblinCostCount) {
        const GoblinCost& c = goblinCost[count];
        hireLocked->SetVisibility(GameState::GetLevel() < c.level);
        bool open = !hireLocked->visibleSelf;
        hire->SetEnabled(open);
        hireIcon->SetVisibility(open);
        hirePrice->SetVisibility(open);
        if (open) hireText->SetText(StringTable::GetString("SPAWN_GOB"));
        else hireText->SetText(SWPrintf(0x100, StringTable::GetString("SPAWN_GOB_NEED_LEVEL"), {ToWideString(c.level)}).c_str());
    }
    int h = lines[0].root->h * (int)GetResourceCount() + info->h + 10 + upgrade->h + header->h + separator->h * 2 +
            GetPaddingTop();
    if (hire->visibleSelf) h += hire->h;
    if (hireCrystals->visibleSelf) h += hireCrystals->h;
    SetSize(header->w + padding * 2, h);
}

// ---- CastleHoverWindow ----

CastleHoverWindow::CastleHoverWindow() { name = "CastleHoverWindow"; }

CastleHoverWindow::~CastleHoverWindow() {
    for (GUI::Window* w : {header, rename, upgrade, separator, subjects, lines[0], lines[1], lines[2]}) delete w;
}

void CastleHoverWindow::OnUpgrade() {
    if (arrowShown) {
        BuildingHovers::HideArrow();
        arrowShown = false;
    }
    UpgradeBuilding(building, false);
}

// @0x37566c
static void OnCastleRename() {
    BuildingHovers::Hide();
    CityRenameWindow::Show();
}

void CastleHoverWindow::ShowArrowAtUpgrade() {
    if (!building) return;
    BuildingHovers::SetArrowVisibleWindowLimit(2);
    // UNVERIFIED (milestone 4): TaskInfoWindow::BlockArrowLimitChangeOnce.
    BuildingHovers::SetArrowClickCallback(new GUI::Callback([this] { OnUpgrade(); }));
    BuildingHovers::ArrowAt((float)(upgrade->w + upgrade->x), (float)(upgrade->y + upgrade->h / 2), false, true, false,
                            false, true, false, false);
    arrowShown = true;
}

void CastleHoverWindow::Update(float) {
    if (!building || !shownHover) return;
    if (arrowShown && !BuildingHovers::ArrowVisible()) arrowShown = false;
    int x = (int)((building->minX + building->maxX) * 0.5f);
    int y = (int)((float)building->data->iconY + building->minY);
    Map::WorldCoordinatesToScreen(x, y);
    SetPosition(x, y);
    if (arrowShown)
        BuildingHovers::ArrowAt((float)(upgrade->w + upgrade->x), (float)(upgrade->y + upgrade->h / 2), false, true,
                                false, false, true, false, false);
}

void CastleHoverWindow::Init() {
    InitBase(true);
    float hs = GUI::GetHoverScaleFactor(1.f, 1.f);
    auto load = [hs](const char* xml, const char* png) {
        std::string path = std::string("../resource/kingdom_ui/1Original/") + xml;
        GUI::Window* w = GUI::RegisterUI(path.c_str(), png, hs, 0, 0, 0, 0, false, 1.f);
        w->SetVisibility(false);
        return w;
    };
    header = load("Unit_info_hint_header.xml", "Unit_info_hint_header.png");
    headerText = GUI::GetWindowTyped<GUI::Textfield>(header, "header_text");
    rename = load("Button_house_upgrade.xml", "Button_house_upgrade.png");
    renameIcon = GUI::GetWindowTyped<GUI::Window>(rename, "icon_profession_builder");
    renameIcon->SetTexture(IconManager::GetIcon("gold_crown"), true);
    renameText = GUI::GetWindowTyped<GUI::Textfield>(rename, "text_upgrade");
    upgrade = load("Button_house_upgrade.xml", "Button_house_upgrade.png");
    upgradeText = GUI::GetWindowTyped<GUI::Textfield>(upgrade, "text_upgrade");
    separator = load("Window_hints_sepparator.xml", "Window_hints_sepparator.png");
    subjects = load("Sepparator_here_works.xml", "Sepparator_here_works.png");
    subjectsText = GUI::GetWindowTyped<GUI::Textfield>(subjects, "text_here_works");
    for (int i = 0; i < 3; ++i) {
        lines[i] = load("Info_1_line_castle.xml", "Info_1_line_castle.png");
        labels[i] = GUI::GetWindowTyped<GUI::Textfield>(lines[i], "text_res");
        values[i] = GUI::GetWindowTyped<GUI::Textfield>(lines[i], "text");
    }
    SetSize(upgrade->w + padding * 2, header->h + rename->h + 10 + upgrade->h + separator->h + subjects->h +
                                          lines[0]->h * 3 + GetPaddingTop());
}

#define CASTLE_PARTS {header, rename, upgrade, separator, subjects, lines[0], lines[1], lines[2]}

void CastleHoverWindow::SetZ(float z) {
    BaseHoverWindow::SetZ(z);
    for (GUI::Window* w : CASTLE_PARTS) w->SetZ(z - 0.0001f);
}

bool CastleHoverWindow::Click(int x, int y, bool pressed) {
    if (!shownHover) return false;
    if (header->Click(x, y, pressed, false) || rename->Click(x, y, pressed, false) ||
        upgrade->Click(x, y, pressed, false))
        return true;
    return BaseHoverWindow::Click(x, y, pressed);
}

void CastleHoverWindow::Show() {
    if (shownHover) return;
    BaseHoverWindow::Show();
    for (GUI::Window* w : CASTLE_PARTS) w->SetVisibility(true);
}

void CastleHoverWindow::Hide() {
    if (!shownHover) return;
    for (GUI::Window* w : CASTLE_PARTS) w->SetVisibility(false);
    BaseHoverWindow::Hide();
}

void CastleHoverWindow::SetPosition(int x, int y) {
    int nx = x - root->w / 2, ny = y + 0x32 - f30;
    FixWindowPosition(nx, ny);
    BaseHoverWindow::SetPosition(nx, ny);
    int left = nx + padding + GetPaddingLeft();
    ny += GetPaddingTop();
    header->SetPosition(left, ny);
    ny += header->h;
    rename->SetPosition(left, ny);
    ny += rename->h + 3;
    upgrade->SetPosition(left, ny);
    ny += upgrade->h + 3;
    separator->SetPosition(left, ny);
    ny += separator->h;
    subjects->SetPosition(left, ny);
    ny += subjects->h;
    for (GUI::Window* l : lines) {
        l->SetPosition(left, ny);
        ny += l->h;
    }
}

void CastleHoverWindow::SetBuilding(Map::Building* b) {
    BaseHoverWindow::SetBuilding(b);
    if (!building) return;
    headerText->SetText(SWPrintf(0x100, U"%s", {StringTable::GetString(building->data->name.c_str()), building->level + 1}).c_str());
    renameText->SetText(StringTable::GetString("CASTLE_GIVE_NAME"));
    rename->SetOnClick(OnCastleRename);
    upgrade->SetEnabled((unsigned)building->level < building->data->GetMaxUpgradeCount());
    upgradeText->SetText(SWPrintf(0x100, StringTable::GetString(upgrade->enabled ? "BUILDING_UPGRADE" : "HOUSE_INFO_LEVEL"),
                                  {ToWideString(building->level + 2)}).c_str());
    upgrade->SetOnClick([this] { OnUpgrade(); });
    subjectsText->SetText(StringTable::GetString("CASTLE_SUBJECTS"));
    int used = Map::GetUsedWorkerCount();
    labels[0]->SetText(StringTable::GetString("CASTLE_MAX_WORKERS"));
    values[0]->SetText(ToWideString(GameState::GetMaxWorkerCount()));
    labels[1]->SetText(StringTable::GetString("CASTLE_USED_WORKERS"));
    values[1]->SetText(ToWideString(used));
    labels[2]->SetText(StringTable::GetString("CASTLE_FREE_WORKERS"));
    values[2]->SetText(ToWideString(GameState::GetPlayerWorkersCount() - used));
}

// ---- EmptyHoverWindow ----

void EmptyHoverWindow::Init() {
    InitBase(true);
    header = GUI::RegisterUI("../resource/kingdom_ui/1Original/Unit_info_hint_header.xml", "Unit_info_hint_header.png",
                             1.f, 0, 0, 0, 0, false, 1.f);
    header->SetVisibility(false);
    headerText = GUI::GetWindowTyped<GUI::Textfield>(header, "header_text");
    SetSize(header->w, header->h + 10 + GetPaddingTop());
}

void EmptyHoverWindow::SetZ(float z) {
    BaseHoverWindow::SetZ(z);
    header->SetZ(z - 0.0001f);
}

bool EmptyHoverWindow::Click(int x, int y, bool pressed) {
    if (!shownHover) return false;
    if (header->Click(x, y, pressed, false)) return true;
    return BaseHoverWindow::Click(x, y, pressed);
}

void EmptyHoverWindow::Show() {
    if (shownHover) return;
    BaseHoverWindow::Show();
    header->SetVisibility(true);
}

void EmptyHoverWindow::Hide() {
    if (!shownHover) return;
    header->SetVisibility(false);
    BaseHoverWindow::Hide();
}

void EmptyHoverWindow::SetPosition(int x, int y) {
    FixWindowPosition(x, y);
    BaseHoverWindow::SetPosition(x, y);
    header->SetPosition(x + GetPaddingLeft(), y + GetPaddingTop());
}

void EmptyHoverWindow::SetBuilding(Map::Building* b) {
    BaseHoverWindow::SetBuilding(b);
    if (!building) return;
    headerText->SetText(StringTable::GetString(building->data->name.c_str()));
}

// ---- ResourceHoverWindow ----

ResourceHoverWindow::ResourceHoverWindow() {
    name = "ResourceHoverWindow";
    usesZRange = true;
}

ResourceHoverWindow::~ResourceHoverWindow() {
    for (GUI::Window* w : {header, separator, holder, progress, worksHere, separator2, workerHolder}) delete w;
}

void ResourceHoverWindow::OnBoostFinished() {
    boosting = false;
    if ((unsigned)building->resourceLeft < 2) {
        BuildingHovers::Hide();
        return;
    }
    int cost = GameState::AdjustCrystalCost(building->data->speedupCb);
    GameState::ChangeResourceAmount(GameState::kCrystal, -cost);
    // PORT (online removed): Billing::LogCBPurchase(2, id, cost).
    building->SpeedupBuilding();
    BuildingHovers::Hide();
    if (GameState::tutorial != 0x29) return;
    GameState::tutorial = 0x2a;
    BuildingHovers::SetArrowVisibleWindowLimit(0);
    BuildingHovers::HideArrow();
}

void ResourceHoverWindow::OnBoost() {
    unsigned cost = (unsigned)GameState::AdjustCrystalCost(building->data->speedupCb);
    NotEnoughWindow::ResetRequirements();
    NotEnoughWindow::AddRequirement(GameState::kCrystal, cost);
    if (!NotEnoughWindow::CheckRequirements()) {
        BuildingHovers::Hide();
        NotEnoughWindow::Show();
        return;
    }
    // SoundsManager::PlaySound("ui_on_speedup", 1, false): sounds are milestone 5.
    boosting = true;
    boostT = 0.f;
    // UNVERIFIED (tutorial): at step 0x29 BuildingHovers::HideWorldDialog.
}

void ResourceHoverWindow::OnCollect() {
    bool full = false;
    int amount = 0;
    building->CollectResources(full, amount);
    std::u32string text;
    if (!full) {
        // SoundsManager::PlaySound("collect_resource", 1, false): sounds are milestone 5.
        text = SWPrintf(0x100, U"+%d %s", {amount, GameState::GetResourceGameName(building->data->produceResource)});
    } else {
        text = StringTable::GetString("WORK_RES_NO_SPACE");
    }
    BuildingHovers::ShowTextHover(building->baseX, building->baseY, text.c_str(), 1.f, 1.f, 1.f, 0.f, 0.f, 0.f, 0x19, 5,
                                  5.f, true, 2.f, 50.f);
    BuildingHovers::Hide();
}

void ResourceHoverWindow::Init() {
    InitBase(true);
    float hs = GUI::GetHoverScaleFactor(1.f, 1.f);
    auto load = [hs](const char* xml, const char* png) {
        std::string path = std::string("../resource/kingdom_ui/1Original/") + xml;
        GUI::Window* w = GUI::RegisterUI(path.c_str(), png, hs, 0, 0, 0, 0, false, 1.f);
        w->SetVisibility(false);
        return w;
    };
    header = load("Hint_shop_header.xml", "Hint_shop_header.png");
    headerText = GUI::GetWindowTyped<GUI::Textfield>(header, "header_text");
    headerDesc = GUI::GetWindowTyped<GUI::Textfield>(header, "header_desc");
    separator = load("Window_hints_sepparator.xml", "Window_hints_sepparator.png");
    holder = load("Collect_hurry_holder.xml", "Collect_hurry_holder.png");
    collect = GUI::GetWindowTyped<GUI::Window>(holder, "button_collect_large");
    collectText = GUI::GetWindowTyped<GUI::Textfield>(holder, "button_collect_large.text");
    collectCount = GUI::GetWindowTyped<GUI::Textfield>(holder, "button_collect_large.text_header");
    collectClick = GUI::GetWindowTyped<GUI::Button>(holder, "button_collect_large.clickArea");
    collectClick->SetOnClick([this] { OnCollect(); });
    collectIcon = GUI::GetWindowTyped<GUI::Window>(holder, "button_collect_large.icon_holder_button_supply");
    collectIcon->takesZ = true;
    boost = GUI::GetWindowTyped<GUI::Window>(holder, "button_boost_small_small");
    boostText = GUI::GetWindowTyped<GUI::Textfield>(holder, "button_boost_small_small.text");
    boostPrice = GUI::GetWindowTyped<GUI::Textfield>(holder, "button_boost_small_small.text_for");
    boostAmount = GUI::GetWindowTyped<GUI::Textfield>(holder, "button_boost_small_small.text_quantity");
    GUI::GetWindowTyped<GUI::Window>(holder, "button_boost_small_small.icon_holder_arrow")
        ->SetTexture(IconManager::GetIcon("gold_arrow_right_green"), false);
    boostClick = GUI::GetWindowTyped<GUI::Button>(holder, "button_boost_small_small.clickArea");
    boostClick->SetOnClick([this] { OnBoost(); });
    boostIcon = GUI::GetWindowTyped<GUI::Window>(holder, "button_boost_small_small.icon_holder_button_mill_res");
    boostIcon->takesZ = true;
    GUI::GetWindowTyped<GUI::Window>(holder, "button_boost_small_small.icon_holder_button_mill_crystal")
        ->SetTexture(IconManager::GetIcon("Icon_16_crystal"), false);
    progress = load("Progress_bar_mobile.xml", "Progress_bar_mobile.png");
    bar = GUI::GetWindowTyped<GUI::Window>(progress, "unit_info_progress_bar");
    bar->SetFrame(1, true);
    fill = GUI::DuplicateWindow(bar, true, false, nullptr);
    fill->SetFrame(100, false);
    bar->PrependChild(fill);
    under = GUI::GetWindowTyped<GUI::Textfield>(progress, "unit_info_progress_bar.text_under");
    over = GUI::GetWindowTyped<GUI::Textfield>(progress, "unit_info_progress_bar.text_over");
    worksHere = load("Info_line_works_here_hint.xml", "Info_line_works_here_hint.png");
    GUI::GetWindowTyped<GUI::Textfield>(worksHere, "text_01")->SetText(StringTable::GetString("RESOURCE_WORKERS"));
    GUI::GetWindowTyped<GUI::Textfield>(worksHere, "text_02")->SetText(StringTable::GetString("RESOURCE_REMOVE"));
    separator2 = load("Window_hints_sepparator.xml", "Window_hints_sepparator.png");
    workerHolder = load("Button_working_worker.xml", "Button_working_worker.png");
    worker = GUI::GetWindowTyped<GUI::Window>(workerHolder, "button_worker");
    workerName = GUI::GetWindowTyped<GUI::Textfield>(workerHolder, "button_worker.text_give_job");
    workerIcon = GUI::GetWindowTyped<GUI::Window>(workerHolder, "button_worker.icon_placeholder_35");
    workerClick = GUI::GetWindowTyped<GUI::Button>(workerHolder, "button_worker.clickArea");
    fire = GUI::GetWindowTyped<GUI::Window>(workerHolder, "button_fire");
    fireClick = GUI::GetWindowTyped<GUI::Button>(workerHolder, "button_fire.clickArea");
    SetSize(header->w, header->h + holder->h + 5 + progress->h + worksHere->h + workerHolder->h + separator->h * 2 +
                           GetPaddingTop());
}

#define RESOURCE_PARTS {header, separator, holder, progress, worksHere, separator2, workerHolder}

void ResourceHoverWindow::SetZ(float z) {
    BaseHoverWindow::SetZ(z);
    for (GUI::Window* w : RESOURCE_PARTS) w->SetZ(z - 0.0001f);
}

bool ResourceHoverWindow::Click(int x, int y, bool pressed) {
    if (boosting) return true;
    if (header->Click(x, y, pressed, false) || holder->Click(x, y, pressed, false) ||
        progress->Click(x, y, pressed, false) || workerHolder->Click(x, y, pressed, false) ||
        BuildingHovers::ClickOnArrow(x, y, pressed) || GameState::tutorial == 0x29)
        return true;
    return BaseHoverWindow::Click(x, y, pressed);
}

void ResourceHoverWindow::Hide() {
    if (!shownHover) return;
    for (GUI::Window* w : RESOURCE_PARTS) w->SetVisibility(false);
    BaseHoverWindow::Hide();
}

void ResourceHoverWindow::Show() {
    if (shownHover) return;
    BaseHoverWindow::Show();
    for (GUI::Window* w : RESOURCE_PARTS) w->SetVisibility(true);
    if (GameState::tutorial == 0x28) {
        GameState::tutorial = 0x29;
        BuildingHovers::SetArrowVisibleWindowLimit(1);
        GUI::SetInteractionObjectLock(boostClick, nullptr);
        BuildingHovers::SetArrowClickCallback(new GUI::Callback([this] { OnBoost(); }));
        BuildingHovers::ArrowAt((float)(holder->w + holder->x), (float)(holder->y + holder->h / 2), false, true, false,
                                false, true, false, false);
    }
    // SoundsManager::PlaySound("work_lumber_started" / "work_stone_started", 1, true): milestone 5.
}

void ResourceHoverWindow::SetPosition(int x, int y) {
    int nx = x - root->w / 2, ny = y - f30;
    FixWindowPosition(nx, ny);
    BaseHoverWindow::SetPosition(nx, ny);
    int left = nx + GetPaddingLeft();
    ny += GetPaddingTop();
    for (GUI::Window* w : RESOURCE_PARTS) {
        w->SetPosition(left, ny);
        ny += w->h;
    }
}

void ResourceHoverWindow::Update(float dt) {
    if (boosting) {
        boostT += dt * 0.5f;
        if (boostT > 1.f) OnBoostFinished();
    }
    if (!building || !shownHover) return;
    int x = (int)((building->minX + building->maxX) * 0.5f);
    int y = (int)((float)building->data->iconY + building->minY);
    Map::WorldCoordinatesToScreen(x, y);
    SetPosition(x, y);
    if (!shownHover || !building->WorkerAssigned(0)) {
        BuildingHovers::Hide();
        return;
    }
    int type = building->data->produceResource;
    // The speed-up hands over up to speedupAmount units, all but the last one left.
    unsigned left = (unsigned)building->resourceLeft;
    unsigned share = left < 2 ? 0 : left - 1;
    if (left >= 2 && (unsigned)building->data->speedupAmount <= share) share = (unsigned)building->data->speedupAmount;
    int amount = GameState::AdjustCrystalCost((int)share);
    boost->SetEnabled(amount != 0 && building->GetGatheredResCount() <= 0x31);
    boostAmount->SetText(SWPrintf(0x100, U"%d", {amount}).c_str());
    collectCount->SetText(SWPrintf(0x100, U"%d", {building->resources[type]}).c_str());
    char key[64];
    std::snprintf(key, sizeof key, "RES_COUNT_%s", GameState::GetResourceName(type));
    std::u32string forms = StringTable::GetCountableString(StringTable::GetString(key), building->resourceLeft);
    std::u32string count = SWPrintf(0x80, forms.c_str(), {building->resourceLeft});
    headerDesc->SetText(SWPrintf(0x100, StringTable::GetString("BUILDING_COLLECT_DESC"), {count.c_str()}).c_str());
    if (!boosting) {
        std::u32string time =
            StringTable::GetTimeString(building->data->collectTime - (int)building->gatherAcc, true);
        std::snprintf(key, sizeof key, "RES_%s_NEXT", GameState::GetResourceName(type));
        std::u32string t = SWPrintf(0x100, U"%s %s", {StringTable::GetString(key), time.c_str()});
        under->SetText(t.c_str());
        over->SetText(t.c_str());
    } else {
        under->SetText(StringTable::GetString("RESOURCE_HURRY"));
        over->SetText(StringTable::GetString("RESOURCE_HURRY"));
    }
    collect->SetEnabled(building->resources[type] > 0);
    if (bar->sprite && fill->sprite) {
        float p = boosting ? boostT : (float)(building->gatherAcc / (double)(unsigned)building->data->collectTime);
        float f = 0.125f + p * 0.75f;
        fill->sprite->u1 = f;
        fill->sprite->w = bar->sprite->w * f;
        over->ClipWithRect(fill->x, 0, (int)((float)fill->x + (float)bar->w * f), 0x200);
    }
}

void ResourceHoverWindow::SetBuilding(Map::Building* b) {
    BaseHoverWindow::SetBuilding(b);
    if (!building) return;
    int type = building->data->produceResource;
    headerText->SetText(StringTable::GetString(building->data->name.c_str()));
    collectText->SetText(StringTable::GetString("BUILDING_COLLECT_BIG"));
    collectIcon->SetTexture(IconManager::GetIcon(GameState::GetResourceIconName(type)), false);
    boostIcon->SetTexture(IconManager::GetIcon(GameState::GetResourceIconName(type)), false);
    boostText->SetText(StringTable::GetString("BUILDING_BOOST"));
    boostPrice->SetText(ToWideString(GameState::AdjustCrystalCost(building->data->speedupCb)));
    workerIcon->SetTexture(IconManager::GetIcon(GameState::GetResourceWorkerIconName(type)), false);
    workerClick->SetEnabled(!building->workers.empty());
    if (!building->workers.empty()) {
        Entity* e = building->workers[0];
        workerClick->SetOnClick([e] { CenterOnWorker(e); });
        workerClick->SetActionSound("ui_click", 1);
        if (e) workerName->SetText(SWPrintf(0x100, U"%s %s", {e->firstName, e->surname}).c_str());
    }
    Map::Building* bb = building;
    // CallbackRemoveWorker::Perform @0x3754a4
    fireClick->SetOnClick([bb] {
        if (bb->WorkerAssigned(0)) bb->RemoveWorker(bb->workers[0]);
        BuildingHovers::Hide();
    });
    fireClick->SetActionSound("ui_click", 1);
    Update(0.f);
}

// ---- DecorationHoverWindow ----

DecorationHoverWindow::~DecorationHoverWindow() {
    delete header;
    delete desc;
    delete progress;
}

void DecorationHoverWindow::Init() {
    InitBase(true);
    float hs = GUI::GetHoverScaleFactor(1.f, 1.f);
    auto load = [hs](const char* xml, const char* png) {
        std::string path = std::string("../resource/kingdom_ui/1Original/") + xml;
        GUI::Window* w = GUI::RegisterUI(path.c_str(), png, hs, 0, 0, 0, 0, false, 1.f);
        w->SetVisibility(false);
        return w;
    };
    header = load("Unit_info_hint_header.xml", "Unit_info_hint_header.png");
    headerText = GUI::GetWindowTyped<GUI::Textfield>(header, "header_text");
    desc = load("Unit_info_hint_6_lines_small.xml", "Unit_info_hint_6_lines_small.png");
    descText = GUI::GetWindowTyped<GUI::Textfield>(desc, "description_text");
    progress = load("Progress_bar_mobile.xml", "Progress_bar_mobile.png");
    bar = GUI::GetWindowTyped<GUI::Window>(progress, "unit_info_progress_bar");
    bar->SetFrame(1, true);
    fill = GUI::DuplicateWindow(bar, true, false, nullptr);
    fill->SetFrame(100, false);
    bar->PrependChild(fill);
    under = GUI::GetWindowTyped<GUI::Textfield>(progress, "unit_info_progress_bar.text_under");
    over = GUI::GetWindowTyped<GUI::Textfield>(progress, "unit_info_progress_bar.text_over");
}

void DecorationHoverWindow::SetZ(float z) {
    BaseHoverWindow::SetZ(z);
    for (GUI::Window* w : {header, desc, progress}) w->SetZ(z - 0.0001f);
}

bool DecorationHoverWindow::Click(int x, int y, bool pressed) {
    if (header->Click(x, y, pressed, false) || progress->Click(x, y, pressed, false)) return true;
    return BaseHoverWindow::Click(x, y, pressed);
}

void DecorationHoverWindow::Show() {
    if (shownHover) return;
    BaseHoverWindow::Show();
    for (GUI::Window* w : {header, desc, progress}) w->SetVisibility(true);
}

void DecorationHoverWindow::Hide() {
    if (!shownHover) return;
    for (GUI::Window* w : {header, desc, progress}) w->SetVisibility(false);
    BaseHoverWindow::Hide();
}

void DecorationHoverWindow::SetPosition(int x, int y) {
    int nx = x - root->w / 2, ny = y - f30;
    FixWindowPosition(nx, ny);
    BaseHoverWindow::SetPosition(nx, ny);
    int left = nx + padding + GetPaddingLeft();
    ny += GetPaddingTop();
    header->SetPosition(left, ny);
    ny += header->h;
    desc->SetPosition(left + (int)(GUI::GetHoverScaleFactor(1.f, 1.f) * 25.f), ny);
    ny += desc->h;
    progress->SetPosition(left, ny);
}

void DecorationHoverWindow::Update(float) {
    if (!decor || !decor->data || !shownHover) return;
    int collect = (int)decor->data->collectTime;
    int elapsed = (int)(Timer::GetGlobalTime() - decor->collectStart);
    if (elapsed >= collect) elapsed = collect;
    std::u32string text = SWPrintf(0x100, StringTable::GetString("HOUSE_INCOME"),
                                   {StringTable::GetTimeString(collect - elapsed, false).c_str()});
    under->SetText(text.c_str());
    over->SetText(text.c_str());
    if (bar->sprite && fill->sprite) {
        float p = (float)elapsed / (float)(unsigned)collect;
        if (p > 1.f) p = 1.f;
        float f = 0.125f + p * 0.75f;
        fill->sprite->u1 = f;
        fill->sprite->w = bar->sprite->w * f;
        over->ClipWithRect(fill->x, 0, (int)((float)fill->x + (float)bar->w * f), 0x200);
    }
}

void DecorationHoverWindow::SetDecoration(Map::Decor* d) {
    BuildingHoverWindow::SetDecoration(d);
    if (!decor || !decor->data) return;
    headerText->SetText(StringTable::GetString(decor->data->name.c_str()));
    char key[64];
    std::snprintf(key, sizeof key, "%s_DESC", decor->data->name.c_str());
    if (StringTable::StringExists(key)) {
        std::u32string time = StringTable::GetTimeString((int)decor->data->collectTime, true);
        descText->SetText(SWPrintf(0x100, StringTable::GetString(key), {time.c_str()}).c_str());
    }
    SetSize(header->w + padding * 2, header->h + desc->h + 8 + progress->h + GetPaddingTop());
    Update(0.f);
}

// ---- FarmRestoreWindow ----

FarmRestoreWindow::~FarmRestoreWindow() {
    delete header;
    delete restore;
    delete clean;
}

void FarmRestoreWindow::Init() {
    InitBase(true);
    float hs = GUI::GetHoverScaleFactor(1.f, 1.f);
    header = GUI::RegisterUI("../resource/kingdom_ui/1Original/Unit_info_hint_header.xml", "Unit_info_hint_header.png",
                             hs, 0, 0, 0, 0, false, 1.f);
    header->SetVisibility(false);
    headerText = GUI::GetWindowTyped<GUI::Textfield>(header, "header_text");
    restore = GUI::RegisterUI("../resource/kingdom_ui/1Original/Button_house_boost.xml", "Button_house_boost.png",
                              GUI::GetHoverScaleFactor(1.f, 1.f), 0, 0, 0, 0, false, 1.f);
    restore->SetVisibility(false);
    restorePrice = GUI::GetWindowTyped<GUI::Textfield>(restore, "text_price");
    restoreClick = GUI::GetWindowTyped<GUI::Button>(restore, "clickArea");
    restoreClick->SetOnClick([this] { OnRestore(); });
    clean = GUI::RegisterUI("../resource/kingdom_ui/1Original/Button_house_upgrade.xml", "Button_house_upgrade.png",
                            GUI::GetHoverScaleFactor(1.f, 1.f), 0, 0, 0, 0, false, 1.f);
    clean->SetVisibility(false);
    cleanText = GUI::GetWindowTyped<GUI::Textfield>(clean, "text_upgrade");
    GUI::GetWindowTyped<GUI::Window>(clean, "icon_profession_builder")->SetTexture(IconManager::GetIcon("gold_cancel"));
    cleanClick = GUI::GetWindowTyped<GUI::Button>(clean, "clickArea");
    cleanClick->SetOnClick([this] { OnCleanUp(); });
    SetSize(header->w + padding * 2, header->h + restore->h + 10 + clean->h + GetPaddingTop());
}

void FarmRestoreWindow::SetZ(float z) {
    BaseHoverWindow::SetZ(z);
    header->SetZ(z - 0.0001f);
    restore->SetZ(z - 0.0001f);
    clean->SetZ(z - 0.0001f);
}

bool FarmRestoreWindow::Click(int x, int y, bool pressed) {
    if (!shownHover) return false;
    if (header->Click(x, y, pressed, false) || restore->Click(x, y, pressed, false) ||
        clean->Click(x, y, pressed, false))
        return true;
    return BaseHoverWindow::Click(x, y, pressed);
}

void FarmRestoreWindow::Show() {
    if (shownHover) return;
    BaseHoverWindow::Show();
    header->SetVisibility(true);
    restore->SetVisibility(true);
    clean->SetVisibility(true);
}

void FarmRestoreWindow::Hide() {
    if (!shownHover) return;
    header->SetVisibility(false);
    restore->SetVisibility(false);
    clean->SetVisibility(false);
    BaseHoverWindow::Hide();
}

void FarmRestoreWindow::SetPosition(int x, int y) {
    int nx = x - f2c / 2, ny = y - f30;
    FixWindowPosition(nx, ny);
    BaseHoverWindow::SetPosition(nx, ny);
    ny += GetPaddingTop();
    header->SetPosition(padding + nx + GetPaddingLeft(), ny);
    ny += 3 + header->h;
    restore->SetPosition(padding + nx + GetPaddingLeft(), ny);
    ny += restore->h;
    clean->SetPosition(nx + padding - 3 + GetPaddingLeft(), ny);
}

void FarmRestoreWindow::SetBuilding(Map::Building* b) {
    BaseHoverWindow::SetBuilding(b);
    if (!building || !building->HasActiveContract()) return;
    restorePrice->SetText(ToWideString(GameState::AdjustCrystalCost(4)));
    bool animals = building->id == 0x3ee;
    GUI::GetWindowTyped<GUI::Textfield>(restore, "text")
        ->SetText(StringTable::GetString(animals ? "ANIMAL_RECOVER_UP" : "FARM_RECOVER_UP"));
    cleanText->SetText(StringTable::GetString(animals ? "ANIMAL_CLEAN_UP" : "FARM_CLEAN_UP"));
}

void FarmRestoreWindow::SetEntity(Entity* e) {
    BaseHoverWindow::SetEntity(e);
    if (!e) return;
    int c = building->patchContract[e->GetAI()->GetFarmPatchNum()];
    headerText->SetText(building->data->delivery->missions[(size_t)c - 1].title);
}

// The crop comes back (its rot timer restarts). The original checks for the 4 crystals but never
// takes them; kept as it is.
void FarmRestoreWindow::OnRestore() {
    unsigned p = (unsigned)entity->GetAI()->GetFarmPatchNum();
    NotEnoughWindow::ResetRequirements();
    NotEnoughWindow::AddRequirement(GameState::kCrystal, (unsigned)GameState::AdjustCrystalCost(4));
    if (NotEnoughWindow::CheckRequirements()) {
        // UNVERIFIED (milestone 5): SoundsManager::PlaySound("ui_buy_with_crystals").
        building->RestoreFarm(p);
        BuildingHovers::Hide();
        return;
    }
    BuildingHovers::Hide();
    NotEnoughWindow::Show();
}

// The farmer goes to clear the patch.
void FarmRestoreWindow::OnCleanUp() {
    unsigned p = (unsigned)entity->GetAI()->GetFarmPatchNum();
    building->CleanFarm(p);
    building->farmer->GetAI()->Farm(0, (int)p, false);
    BuildingHovers::Hide();
}
