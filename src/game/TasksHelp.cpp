// The quest "help": what a step's action button does (the shop on the right building, an arrow at
// a building, an area or an order ...). Port of Tasks::TaskSubtask::OnHelp @0x2381d8 (its switch
// follows the jump table at 0x238238, read from the binary), HelpWithLocation @0x22f53c,
// HelpWithMapSwitchTo @0x235964 (+ Continuation @0x235e24). RunLastSubTaskHelp is in Tasks.cpp.
//
// A step type whose target is not ported yet is marked in its case with the step that brings it:
// decorations' jobs 4e, spawn points / the campaign maps / fog 4f, enemies 4g, the item shop, the
// hire, craft, character, set, heal and level windows and the world map: milestone 5.
#include <cstdio>

#include "engine/Render.h"
#include "game/Building.h"
#include "game/BuildingHovers.h"
#include "game/BuildingMovement.h"
#include "game/Contracts.h"
#include "game/Entity.h"
#include "game/EntityManager.h"
#include "game/GameData.h"
#include "game/AIState.h"
#include "game/Fog.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/Locations.h"
#include "game/Map.h"
#include "game/MetaData.h"
#include "game/Sets.h"
#include "game/Setting.h"
#include "game/StringTable.h"
#include "game/Tasks.h"
#include "gui/GUI.h"
#include "hud/HUD.h"
#include "windows/Windows.h"

namespace Tasks {
namespace {

// The arrow's tap opens the building's window (BuildingHovers::OnBuildingClick at its place on
// screen), and the camera goes there.
void ArrowAtBuilding(Map::Building* b) {
    int sx = (int)b->minX, sy = (int)b->minY;
    Map::WorldCoordinatesToScreen(sx, sy);
    BuildingHovers::SetArrowClickCallback(new GUI::Callback([b, sx, sy] { BuildingHovers::OnBuildingClick(b, sx, sy); }));
    HelpWithLocation((int)b->baseX, (int)b->minY, 0, 0);
}

// The top centre of a decoration's sprite (its tile without one).
void DecorPoint(const Map::Decor* d, int& x, int& y) {
    x = d->x;
    y = d->y;
    Map::TileCoordinatesToWorld(x, y);
    if (d->sprite) {
        x = (int)(d->sprite->x + d->sprite->w * 0.5f);
        y = (int)(d->sprite->y - d->sprite->h);
    }
}

void ArrowAtDecor(Map::Decor* d) {
    int x, y;
    DecorPoint(d, x, y);
    BuildingHovers::SetArrowClickCallback(new GUI::Callback([d] { BuildingHovers::OnDecorClick(d, 0, 0); }));
    HelpWithLocation(x, y, 0, 0);
    BuildingHovers::FollowDecor(d);
}

// The arrow over an entity's head; its tap sends the hero to it (AIPlayer::ClickedEntity).
void ArrowAtEntity(Entity* e) {
    float wx = 0.f, wy = 0.f;
    e->GetWorldPos(wx, wy);
    BuildingHovers::SetArrowClickCallback(
        new GUI::Callback([e] { EntityManager::GetPlayer()->GetAI()->ClickedEntity(e); }));
    float h = e->GetSprite() ? e->GetSprite()->h : 0.f;
    HelpWithLocation((int)wx, (int)((wy - h) - 10.f), 0, 0);
    BuildingHovers::FollowEntity(e);
}

void ShopOn(uint32_t building) {
    ShopWindow::Show();
    ShopWindow::OnSelectItemID(building);
    ShopWindow::ShowArrowOnItem(building);
}

// The area's sign: the camera goes to the middle of the area and the arrow's tap buys it.
void ArrowAtArea(const GameState::AreaInfo& a) {
    float x = 30.f + (float)(a.x + a.w / 2) * 84.f;
    float y = (float)(a.y + a.h / 2) * 42.f * 0.5f - 90.f;
    Render::CenterOn(x, y, true);
    int sx = (int)x, sy = (int)(y + 90.f);
    Map::WorldCoordinatesToScreen(sx, sy);
    BuildingHovers::SetArrowClickCallback(new GUI::Callback([sx, sy] { Map::ClickToBuyArea(sx, sy); }));
    HelpWithLocation((int)x, (int)y, 0, 0);
}

}  // namespace

// @0x22f53c Tasks::HelpWithLocation: the camera goes to the world point and the arrow points at it.
void HelpWithLocation(int x, int y, int tileX, int tileY) {
    // A point still under the fog gets the locator at the screen's edge instead.
    if (!Fog::IsVisible(x - 0x1e, y - 0x1e, x + 0x1e, y + 0x1e)) {
        HUDWindow::ShowTaskLocaterTo(x, y, tileX, tileY);
        return;
    }
    Render::CenterOn((float)x, (float)y, true);
    BuildingHovers::ArrowAt((float)x, (float)y, false, false, false, false, false, false, false);
}

// @0x235e24 Tasks::HelpWithMapSwitchToContinuation
void HelpWithMapSwitchToContinuation() {
    PopupWindow::Hide();
    HelpWithMapSwitchTo(0, true, true, 0);
}

// @0x235964 Tasks::HelpWithMapSwitchTo: the way to another map (item: what is looked for there).
void HelpWithMapSwitchTo(uint32_t map, bool arrow, bool force, uint32_t item) {
    if (GameState::IsCombatActive()) {
        PopupWindow::Show(StringTable::GetString("CAN_NOT_TRAVEL"), PopupWindow::Hide, nullptr, nullptr);
        return;
    }
    if (!force && NormalizeMapID(map) == 0 && GameState::GetCurrentMapID() != 0xb) {
        // Home: asked first.
        std::u32string text = SWPrintf(0x100, StringTable::GetString("TRAVEL_HOME"), {GameState::GetCastleName()});
        PopupWindow::Show(text.c_str(), HelpWithMapSwitchToContinuation, PopupWindow::Hide, nullptr);
        return;
    }
    // A portal to the map on this map: the camera goes to it and the arrow points at it.
    if (Map::Decor* d = Map::GetPortalToMap(map)) {
        int x, y;
        DecorPoint(d, x, y);
        Render::CenterOn((float)x, (float)y, true);
        if (arrow) {
            BuildingHovers::SetArrowVisibleWindowLimit(1);
            BuildingHovers::SetArrowClickCallback(new GUI::Callback([d] { BuildingHovers::OnDecorClick(d, 0, 0); }));
            BuildingHovers::ArrowAt((float)x, (float)y, false, false, false, false, false, false, false);
            BuildingHovers::FollowDecor(d);
        }
        return;
    }
    // Else the world map: home (map 0) is travelled to at once (the help runs again there);
    // another map gets the world map with its arrow (map 0x2f: home's), or, with an item, the
    // map centred and its mission / campaign window showing the item.
    if (map == 0) {
        EnableLastSubTaskHelp();
        GlobalMapWindow::CenterOnMap((uint32_t)GameState::GetCurrentMapID());
        GlobalMapWindow::SwitchMap(0, false, true);
        GlobalMapWindow::PreventArrowHide();
        GlobalMapWindow::Show();
        GlobalMapWindow::Hide();
        return;
    }
    GlobalMapWindow::Show();
    GlobalMapWindow::SetArrowVisibleWindowLimit(2);
    if (item == 0) {
        GlobalMapWindow::ArrowAtMap(map == 0x2f ? 0 : map, false, true);
    } else {
        GlobalMapWindow::CenterOnMap(map);
        if (Locations::LocationInfo* loc = Locations::GetLocationWithMap(map)) {
            if (loc->mission) {
                RaidInfoWindow::SetParameters(loc, map, false);
                RaidInfoWindow::Show();
                RaidInfoWindow::SetAnimationOnce();
                RaidInfoWindow::ShowItem(item);
            } else {
                CampaignInfoWindow::SetParameters(loc, map, false);
                CampaignInfoWindow::Show();
                CampaignInfoWindow::ShowItem(item);
                CampaignInfoWindow::UpdateContents();
            }
        }
    }
    HUDWindow::UpdateTasks();
}

// @0x2381d8 Tasks::TaskSubtask::OnHelp. arrow: an arrow may be shown. True when it helped.
bool TaskSubtask::OnHelp(bool arrow) {
    // The step's own map and place come first.
    if (assistMap == 0x74 && (type == kKillEnemyClass || type == kDealDamage || type == kWinInRow)) {
        // UNVERIFIED (milestone 5): CastleTopWindow::HelpWithPvP.
        return true;
    }
    if (assistMap != 0 && GameState::GetCurrentMapID() != NormalizeMapID(assistMap)) {
        HelpWithMapSwitchTo(NormalizeMapID(assistMap), arrow, false, 0);
        return true;
    }
    if (assistX != 0 || assistY != 0) {
        int x = (int)assistX, y = (int)assistY;
        Map::TileCoordinatesToWorld(x, y);
        HelpWithLocation(x, y, (int)assistX, (int)assistY);
        // UNVERIFIED (milestone 5, with that quest's map): step 0x36c also points an arrow at each of its decorations
        // still to be used (the tile list 0x58c6ec).
        return true;
    }
    const bool city = GameState::GetCurrentMapID() == 0;
    switch (type) {
    case kBuildHouse: {
        if (!city) {
            HelpWithMapSwitchTo(0, arrow, false, 0);
            return true;
        }
        Map::Building* b = Map::GetUnfinishedBuildingWithID(value, false);
        if (b) {
            if (arrow) ArrowAtBuilding(b);
            return true;
        }
        // Before tutorial step 0x80 the ruin that becomes this building gets the arrow instead.
        if (GameState::tutorial < 0x80)
            if (Map::Decor* ruin = Map::GetDecorationThatIsReplacedWith(value)) {
                if (arrow) ArrowAtDecor(ruin);
                return true;
            }
        ShopOn(value);
        return true;
    }
    case kBuildDecor:
        if (!city) {
            HelpWithMapSwitchTo(0, arrow, false, 0);
            return true;
        }
        BuildingMovement::Activate();
        // (BuildingMovement::ToggleRoadType(value): dead in 5.11, see port_plan.txt)
        return true;
    case kUpgradeHouse: {
        if (!city) {
            HelpWithMapSwitchTo(0, arrow, false, 0);
            return true;
        }
        Map::Building* b = Map::GetUnfinishedBuildingWithID(value, true);
        if (b) {   // being upgraded: its speed-up
            if (arrow) {
                BuildingHovers::ShowArrowAtSpeedup(b);
                Render::CenterOn(b->baseX, b->minY, true);
            }
            return true;
        }
        b = Map::GetUpgradeableBuildingWithID(value);
        if (!b) {
            ShopOn(value);
            return true;
        }
        if (b == Map::GetCurrentFarm()) {
            BottomFarmWindow::HelpWithUpgrade();
            return true;
        }
        if (b->IsOpened() && b->data->buildingClass != 0xd && (!b->HasActiveContract() || !b->contractDone)) {
            if (b->data->buildingClass == 0xc) {
                // UNVERIFIED (milestone 5): HireTroopsWindow::ShowForBuilding(b->id, true).
                return true;
            }
            Render::CenterOn(b->baseX, b->minY - 100.f);
            BuildingHovers::AllowWindowOverHoverOnce();
            BuildingHovers::OnBuildingClick(b, GUI::ScreenWidth() / 2, GUI::ScreenHeight() / 2);
            BuildingHovers::ShowArrowAtUpgrade();
            return true;
        }
        if (arrow) ArrowAtBuilding(b);
        return true;
    }
    case kIncreaseLand: {
        if (!city) {
            HelpWithMapSwitchTo(0, arrow, false, 0);
            return true;
        }
        GameState::AreaInfo a;
        if (!GameState::GetAreaInfo(value, a)) {
            std::fprintf(stderr, "Tasks::TaskSubtask::OnHelp() SUBTASK_INCREASE_LAND cannot find area %d\n", value);
            return true;
        }
        ArrowAtArea(a);
        return true;
    }
    case kIncreaseLandAny: {
        if (!city) {
            HelpWithMapSwitchTo(0, arrow, false, 0);
            return true;
        }
        GameState::AreaInfo a;
        if (!GameState::GetAreaInfo(Map::GetNextAreaID(), a)) {
            std::fprintf(stderr, "Tasks::TaskSubtask::OnHelp() SUBTASK_INCREASE_LAND_ANY cannot find any areas\n");
            return true;
        }
        ArrowAtArea(a);
        return true;
    }
    case kBallance:   // what is missing, with the ways to get it
        NotEnoughWindow::ResetRequirements();
        NotEnoughWindow::AddRequirement(value == 0 ? GameState::kGold : value == 1 ? GameState::kCrystal : (int)value - 2,
                                        (unsigned)amount);
        NotEnoughWindow::Show();
        return true;
    case kDeliveryMissionStart:
    case kDeliveryMissionEnd: {
        if (!city) {
            HelpWithMapSwitchTo(0, arrow, false, 0);
            return true;
        }
        if (GlobalMapWindow::IsVisible()) GlobalMapWindow::Hide();
        const Contracts::Contract* c = Contracts::GetContract(value / 10);
        if (!c) return false;
        Map::Building* b = Map::GetBuildingWithContract(c, type == kDeliveryMissionEnd);
        if (!b) {   // not built yet: the shop
            const GameData::BuildingData* d = GameData::GetBuildingDataWithContract(c);
            if (!d) return false;
            ShopOn(d->id);
            return true;
        }
        if (b == Map::GetCurrentFarm()) {
            // UNVERIFIED (3f detail): a patch growing this crop gets the arrow on its plant (its
            // tap harvests it); a ripe farm whose crop would not fit shows "WORK_RES_NO_SPACE".
            BuildingHovers::ShowFarmHover(false, 0, 1);
            BuildingHovers::ShowArrowAtFactoryContract(value % 10, false);
            return true;
        }
        if (Map::GetCurrentFarm()) HUDWindow::ExitFarm();
        if (b->IsOpened() && b->data->buildingClass != 0xd) {
            bool open = !b->HasActiveContract() || !b->contractDone;
            // (workshop 0x6b: also when it is making another order than this one)
            if (!open && b->id == 0x6b && b->GetMissionID() != (int)(value % 10)) open = true;
            if (open) {
                if (b->id == 0x6b) BuildingHovers::AllowWindowOverHoverOnce();
                Render::CenterOn(b->baseX, b->minY - 100.f);
                BuildingHovers::OnBuildingClick(b, GUI::ScreenWidth() / 2, GUI::ScreenHeight() / 2);
                BuildingHovers::ShowArrowAtFactoryContract(value % 10, b->id == 0x6b);
                return true;
            }
        }
        if (arrow) {
            // (a finished order: the arrow's tap collects it; the port's building click does both)
            ArrowAtBuilding(b);
        }
        return true;
    }
    case kDeliveryBuyCar:
        if (!city) {
            HelpWithMapSwitchTo(0, arrow, false, 0);
            return true;
        }
        return false;
    case kDeliveryMissionIncSpeed: {
        if (!city) {
            HelpWithMapSwitchTo(0, arrow, false, 0);
            return true;
        }
        const Contracts::Contract* c = Contracts::GetContract(value / 10);
        Map::Building* b = c ? Map::GetBuildingWithContract(c, false) : nullptr;
        if (!b) return false;
        if (arrow) ArrowAtBuilding(b);
        return true;
    }
    case kBuildAny:
        if (city) ShopWindow::Show();
        else HelpWithMapSwitchTo(0, arrow, false, 0);
        return true;
    case kUpgradeAny: {
        if (!city) {
            HelpWithMapSwitchTo(0, arrow, false, 0);
            return true;
        }
        Map::Building* b = Map::GetUpgradeableBuilding();
        if (!b) {
            ShopWindow::OnTabSelect(0);
            ShopWindow::Show();
            return true;
        }
        if (arrow) ArrowAtBuilding(b);
        return true;
    }
    case kStartBuildingObject:
        if (city) ShopOn(value);
        else HelpWithMapSwitchTo(0, arrow, false, 0);
        return true;
    case kBuildAndNotFinishObject: {
        if (!city) {
            HelpWithMapSwitchTo(0, arrow, false, 0);
            return true;
        }
        Map::Building* b = Map::GetUnfinishedBuildingWithID(value, false);
        if (!b) {
            ShopOn(value);
            return true;
        }
        if (arrow) ArrowAtBuilding(b);
        return true;
    }
    case kGiveJobTask:
        std::fprintf(stderr, "Tasks::TaskSubtask::OnHelp() SUBTASK_ASSIGN_JOB unsupported (%d)\n", value);
        return false;
    case kSellOnMarketTask:
        std::fprintf(stderr, "Tasks::TaskSubtask::OnHelp() SUBTASK_SELL_RES unsupported (%d)\n", value);
        return false;
    case kTrainWorker:
        std::fprintf(stderr, "Tasks::TaskSubtask::OnHelp() SUBTASK_TRAIN_WORKER unsupported (%d)\n", value);
        return false;
    case kEnterMap:
        HelpWithMapSwitchTo(value, arrow, false, 0);
        return true;
    case kTrainWorkerInBuilding: {
        if (city) {
            Map::Building* b = Map::GetBuildingWithID(value);
            if (!b) {
                ShopOn(value);
                return true;
            }
            if (b->data->buildingClass != 0xc && b->id != 0x12) {
                int sx = (int)b->minX, sy = (int)b->minY;
                Map::WorldCoordinatesToScreen(sx, sy);
                BuildingHovers::SetArrowClickCallback(
                    new GUI::Callback([b, sx, sy] { BuildingHovers::OnBuildingClick(b, sx, sy); }));
                HelpWithLocation((int)b->baseX, (int)b->minY, 0, 0);
                return true;
            }
        }
        // UNVERIFIED (milestone 5): HireTroopsWindow::ShowForBuilding(value, false).
        return true;
    }
    case kBuyFarmPatch: {
        if (!city) {
            HelpWithMapSwitchTo(0, arrow, false, 0);
            return true;
        }
        Map::Building* farm = Map::GetFarmWithPatchToBuy(value);
        if (!farm) {
            ShopOn(value);
            return true;
        }
        if (!arrow) return true;
        if (farm != Map::GetCurrentFarm()) {
            ArrowAtBuilding(farm);
            return true;
        }
        int patch = farm->GetNextPatchToBuy();
        Entity* e = patch != -1 ? farm->patchEntities[(size_t)patch] : nullptr;
        if (e) {
            float x = 0.f, y = 0.f;
            e->GetWorldPos(x, y);
            // UNVERIFIED (3f detail): the arrow's tap is the tap on that patch.
            BuildingHovers::ArrowAt(x, y - e->GetIdleHeight(), false, false, false, false, false, false, false);
        }
        return true;
    }
    case kGetItem:
    case kBallanceItem: {
        if (value - 0x16c < 2) {   // the pyramids
            PopupWindow::Show(StringTable::GetString("HELP_FIND_RUBY"), PopupWindow::Hide, nullptr, nullptr);
            return true;
        }
        float x = 0.f, y = 0.f;
        if (BuildingHovers::HasDroppedItem(value, x, y)) {   // it lies here: pick everything up
            if (arrow) {
                BuildingHovers::SetArrowClickCallback(new GUI::Callback([] { BuildingHovers::CollectAll(); }));
                HelpWithLocation((int)x, (int)y, 0, 0);
            }
            return true;
        }
        if (Entity* e = EntityManager::GetEntityWithDrop(value)) {   // somebody who drops it
            if (arrow) {
                if (GlobalMapWindow::IsVisible()) GlobalMapWindow::Hide();
                ArrowAtEntity(e);
            }
            return true;
        }
        if (Map::Decor* d = Map::GetDecorationWithDrop(value)) {   // a decoration that drops it
            if (arrow) {
                if (GlobalMapWindow::IsVisible()) GlobalMapWindow::Hide();
                ArrowAtDecor(d);
            }
            return true;
        }
        const Items::ItemInfo* info = Items::GetItemInfo(value);
        if (info && (info->type == 6 || info->type == 9)) {   // made in a building
            unsigned slot = 0;
            if (NeedItemWindow::GetBuildingThatProducesItem(value, slot) != 0) {
                NeedItemWindow::OnProduce(value);
                return true;
            }
            NeedItemWindow::SetParameters(value, nullptr);
            NeedItemWindow::Show();
            return true;
        }
        // UNVERIFIED (milestone 5): else the map where it is found or its craft resources grow
        // (GlobalMapWindow::GetMissionMapForItem / GetMapForCraftItem; NeedItemWindow's find mode
        // when the shop also sells it; "CRAFT_VISIT_LATER_HERB" / "_MINER" on this very map),
        // the crafting windows for a craftable item, else the item shop.
        std::fprintf(stderr, "Tasks::TaskSubtask::OnHelp() cannot help with %s %d\n",
                     type == kGetItem ? "SUBTASK_GET_ITEM" : "SUBTASK_BALLANCE_ITEM", value);
        return false;
    }
    case kMasteryLevel:
        HelpWithMapSwitchTo(value / 10, arrow, false, 0);
        return true;
    case kCastSpell:
        if (city) PopupWindow::Show(StringTable::GetString("CAN_NOT_CAST"), PopupWindow::Hide, nullptr, nullptr);
        // UNVERIFIED (milestone 5, spells): else BottomBattleWindow::HelpWithSpell(value, false).
        return true;
    case kBuyRealMoney:   // PORT: the exchange (the original opens its real-money tab 0)
        ExchangeWindow::Show();
        return true;
    case kInteractBuilding:      // the arrow at the decoration to use
    case kInteractBuildingAny: { // the arrow at a decoration of the type to click
        Map::Decor* d = type == kInteractBuilding ? Map::GetDecorationToInteract(value) : Map::GetDecorationToClick(value);
        if (!d) {
            std::fprintf(stderr, "Tasks::TaskSubtask::OnHelp() cannot help with %s not on the current map (%d)\n",
                         type == kInteractBuilding ? "SUBTASK_BATTLE_INTERACT_BUILDING" : "SUBTASK_TASK_CLICK_BUILDING_ANY",
                         value);
            return false;
        }
        if (arrow) ArrowAtDecor(d);
        return true;
    }
    case kTalkToPerson: {   // the arrow at the person (its tap: walk there and talk)
        Entity* e = EntityManager::GetEntityBySpawnPoint(value);
        if (!e) {
            std::fprintf(stderr,
                         "Tasks::TaskSubtask::OnHelp() cannot help with SUBTASK_TALK_TO_PERSON - no spawn point %d\n",
                         (int)value);
            return false;
        }
        if (arrow) ArrowAtEntity(e);
        return true;
    }
    case kCompleteCompaign: {   // the arrow at whoever has something to say
        Entity* e = EntityManager::GetEntityWithTalkTask();
        if (!e) return false;
        if (arrow) ArrowAtEntity(e);
        return true;
    }
    // The arrow at an enemy (its tap: the hero attacks it).
    case kKillMob: {
        Entity* e = EntityManager::GetNearestEnemyTo(EntityManager::GetPlayer(), value);
        if (!e) {
            std::fprintf(stderr,
                         "Tasks::TaskSubtask::OnHelp() cannot help with SUBTASK_BATTLE_KILL_MOB - no mob is alive with class %d\n",
                         (int)value);
            return false;
        }
        if (arrow) ArrowAtEntity(e);
        return true;
    }
    case kKillAllOnMap: {
        Entity* e = EntityManager::GetNearestEnemy(EntityManager::GetPlayer(), false, false);
        if (!e) return false;
        if (arrow) ArrowAtEntity(e);
        return true;
    }
    case kKillEnemyClass:
    case kTameEnemyClass: {   // an enemy of the class here, else the map where one lives
        // UNVERIFIED (milestone 5, taming): a tame step without a net (Items::GetPlayersBestNet)
        // opens the item shop on item 0x24f instead.
        Entity* e = EntityManager::GetNearestEnemyWithClassTo(EntityManager::GetPlayer(), value);
        if (!e) {
            uint32_t map = Locations::GetMapWithEntityOfClass(value, false);
            if (map == 0) {
                if (Locations::GetMapWithEntityOfClass(value, true) != 0) {
                    PopupWindow::Show(StringTable::GetString("SERVER_ERROR"), PopupWindow::Hide, nullptr, nullptr);
                    return true;
                }
            } else if (map != 0x74) {
                HelpWithMapSwitchTo(map, true, false, 0);
                return true;
            }
            std::fprintf(stderr, "Tasks::TaskSubtask::OnHelp() cannot help with %s - no mob is alive with class %d\n",
                         type == kTameEnemyClass ? "tame_mob_class" : "kill_mob_class", (int)value);
            return false;
        }
        if (arrow) ArrowAtEntity(e);
        return true;
    }
    case kHealHero:
        HealSquadWindow::Show();
        HealSquadWindow::ResetRecentDeaths();
        return true;
    case kEquip: {   // the character window on the item, or the arrow at it lying on the ground
        float x = 0.f, y = 0.f;
        if (GameState::GetItemAmount(value, false) == 0 && BuildingHovers::HasDroppedItem(value, x, y)) {
            BuildingHovers::SetArrowClickCallback(new GUI::Callback([] { BuildingHovers::CollectAll(); }));
            HelpWithLocation((int)x, (int)y, 0, 0);
            return true;
        }
        CharacterInfoWindow::Show();
        CharacterInfoWindow::ShowInventoryTab(0);
        CharacterInfoWindow::ShowInventoryItem(value, 1, nullptr);
        return true;
    }
    case kCollectSet: {   // the set window at a missing piece (a set already complete counts at once)
        if (Sets::Set* set = Sets::GetSet(value / 10)) {
            Sets::FindInventorySetState();
            unsigned complete = 0;
            for (unsigned k = 0; k != 3; ++k)
                if (set->bonusSlots[k] && set->covered[k] == set->bonusSlots[k]->GetChildrenCount()) ++complete;
            if (value % 10 <= complete) {
                CompleteSubtask(kCollectSet, complete + set->id * 10, 1);
                return true;
            }
        }
        SetInfoWindow::SelectSet(value / 10);
        SetInfoWindow::Show();
        SetInfoWindow::ShowNextItemHelp();
        return true;
    }
    case kImproveItem:   // the character window on the item
        CharacterInfoWindow::Show();
        CharacterInfoWindow::ShowInventoryTab(0);
        CharacterInfoWindow::ShowInventoryItem(value, 2, nullptr);
        return true;
    case kRepairItem:    // the character window on the first broken piece worn
        CharacterInfoWindow::Show();
        CharacterInfoWindow::ShowInventoryTab(0);
        for (unsigned b = 0; b != 10; ++b) {
            GameState::PlayerItem* it = GameState::GetItemAt(b);
            if (it && it->IsBroken()) {
                CharacterInfoWindow::ShowInventoryItem(it->id, 3, nullptr);
                return true;
            }
        }
        std::fprintf(stderr, "Tasks::TaskSubtask::OnHelp() cannot help with SUBTASK_REPAIR_ITEM - none of the equiped "
                             "items are broken\n");
        return true;
    case kBalanceLevel:   // the levels window on that level
        LevelInfoWindow::Show();
        LevelInfoWindow::OnTab(0, true);
        LevelInfoWindow::OnLevel((unsigned)value);
        return true;
    // UNVERIFIED (milestone 5): other windows.
    case kBuyOnMarketTask:     // the item shop on the item (CastleTopWindow::OnBuyItem for steps 0x42e / 0x42f)
    case kTrainAnyWorker:      // HireTroopsWindow::ShowForBuilding(0x8e)
    case kMakeScreenshot:      // CastleTopWindow::HelpWithScreenshot
    case kCraftItem:           // CraftItemWindow on the item
    case kDealDamage:          // CastleTopWindow::HelpWithPvP
    case kWinInRow:            // CastleTopWindow::HelpWithPvP
    case kOpenWeb:             // HelpWindow::Show(value) (online: the help web view)
        std::printf("Tasks::TaskSubtask::OnHelp() step %u (type %d): its help is not ported yet\n", id, type);
        return true;
    // (online, not ported: 0x33 the FAQ, 0x34 rate the game, 0x38 like the game: the step is
    // counted at once and the page opens.)
    case kVisitFbFaq:
    case kWriteComment:
    case kLikeGame:
    // (these do nothing but count as helped)
    case kIncreaseSpeed:
    case kGetLevel:
    case kVisitFbGroup:
    case kDailyBonus:
    case kWatchVideo:
    case kInviteFriend:
        return true;
    default:
        return false;
    }
}

}  // namespace Tasks
