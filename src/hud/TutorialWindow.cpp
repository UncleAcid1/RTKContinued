// TutorialWindow: the scripted opening of a new game. It has nothing on screen: it is a window
// only to be updated with the others, and each update looks at the tutorial step and does the
// step's work once.
//   Update @0x368ee8       steps 0..0x17: the first fight on campaign map 0x15
//   UpdateCity @0x36731c   steps 0x18..0x66: the first minutes in the city (ends at 0x80)
//   UpdateSecond @0x36693c GameState::secondTutorial: hints on the next maps, until level 6
// Even steps do something and move on to the next odd one; the odd ones wait for the player, and
// the code the player's action reaches (a dialog's Next, a walk's end, a window's button) sets
// the next even step.
// The two big functions are switches whose jump tables Ghidra did not follow; they were read
// from out/switch/*.c, made by tools/ghidra_scripts/SwitchDecomp.java.
// Not ported: TutorialSender's report to the server (FileManager::NotifyTutorialStep, online);
// SkipTutorialStep @0x366498 and the game bot (StartTutorialSkip, StartGameBot have no caller in
// 5.11, so their flags 0x630925 / 0x630924 are never set); the sounds (milestone 6).
#include "hud/HUD.h"

#include <cstdio>

#include "engine/Render.h"
#include "engine/SystemFuncs.h"
#include "engine/Timer.h"
#include "game/AIState.h"
#include "game/Building.h"
#include "game/BuildingHovers.h"
#include "game/Combat.h"
#include "game/Entity.h"
#include "game/EntityManager.h"
#include "game/GameData.h"
#include "game/GameState.h"
#include "game/Map.h"
#include "game/Setting.h"
#include "game/Squad.h"
#include "game/StringTable.h"
#include "game/Tasks.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "windows/Windows.h"

namespace TutorialSender {
namespace {
float g_maxTime = 0.f;   // TutorialSender::mMaxTime
float g_timer = 0.f;     // TutorialSender::mTimer
int g_currentStep = 0;   // TutorialSender::mCurrentStep
int g_lastSentStep = 0;  // TutorialSender::mLastSentStep
}  // namespace

void SetTimeLimit(float seconds) { g_maxTime = seconds; }   // @0x23ee5c

// @0x23ee70 (step 0x17 does not replace step 0x9e)
void AddTutorialStep(int step) {
    if (step != 0x17 || g_currentStep != 0x9e) g_currentStep = step;
}

// @0x23ee90: every mMaxTime seconds a new step is reported.
void Update(float dt) {
    g_timer += dt;
    if (g_timer < g_maxTime) return;
    if (g_currentStep != g_lastSentStep) {
        // (FileManager::NotifyTutorialStep(step): online)
        g_lastSentStep = g_currentStep;
    }
    g_timer = 0.f;
}
}  // namespace TutorialSender

namespace TutorialWindow {
namespace {

WindowManager::FunctionalWindow* g_queue = nullptr;   // TutorialWindow::wnd
bool g_visible = false;        // TutorialWindow::visible
Entity* g_guide = nullptr;     // 0x6308e0 the city's first citizen, who explains it
Entity* g_farmer = nullptr;    // 0x6308e4 the farmer's wife (entity 7), who comes for the farm
Entity* g_envoy = nullptr;     // 0x6308e8 the last visitor (entity 6)
float g_f0c = 0.f;             // 0x6308ec (only reset)
bool g_decorShown = false;     // 0x6308f0 step 0x20d centred the view once
bool g_enemyShown = false;     // 0x6308f1 step 0x20e centred the view once
uint32_t g_waitUntil = 0;      // 0x6308f4 (WaitUntil; read by the game bot only)
int g_arrowCallback = 0;       // 0x630928 the id of the arrow callback set here

void Init() { g_queue->usesZRange = false; }       // @0x3661e4
bool Click(int, int, bool) { return g_visible; }   // @0x366200
void SetZ(float) {}                                // @0x366214
void Hide() { g_visible = false; }                 // @0x366218

// (Render::CenterOn's glide over 0.4 s is not in the port's renderer yet: it jumps.)
void CenterOn(float x, float y) { Render::CenterOn(x, y); }

void Say(Entity* who, const char* key, uint32_t task, bool arrow, bool next, bool setSide, bool noCenter, bool left) {
    BuildingHovers::ShowWorldDialog(who, StringTable::GetString(key), task, arrow, next, setSide, 0, noCenter, left);
}

// The helper arrow over a decoration's picture; its tap is a tap on the decoration.
void ArrowAtDecor(Map::Decor* d) {
    Render::Sprite* s = d->sprite;
    BuildingHovers::ArrowAt(s->x + s->w * 0.5f, s->y - s->h, false, false, false, false, false, false, false);
}

// FUN_003672b4 and the arrow that goes with it: a tap on the arrow is a tap on the building at
// its screen place (px, py are the world point looked up).
void ArrowAtBuilding(Map::Building* b, float px, float py) {
    int sx = (int)px, sy = (int)py;
    Map::WorldCoordinatesToScreen(sx, sy);
    BuildingHovers::SetArrowClickCallback(new GUI::Callback([b, sx, sy] { BuildingHovers::OnBuildingClick(b, sx, sy); }));
}

// FUN_003668d0: a tap on the arrow sends the hero at the entity (AIPlayer::ClickedEntity).
GUI::Callback* AttackCallback(Entity* player, Entity* e) {
    AIBaseState* ai = player->GetAI();
    return new GUI::Callback([ai, e] { ai->ClickedEntity(e); });
}

void HideTask(uint32_t id) {
    if (Tasks::TaskInfo* t = Tasks::GetTask(id)) t->dnShow = true;
}

// The barracks' building site of step 0x19: building 0x8e at (0x1a, 0x11), waiting for a builder.
void PlaceBarracksSite() {
    Map::Building* b = new Map::Building();
    b->id = 0x8e;
    b->data = GameData::GetBuilding(0x8e);
    b->x = 0x1a;
    b->y = 0x11;
    Map::Patch* p = Map::GetPatchForCoordinates(0x1a, 0x11, false);
    p->buildings.emplace_back(b);
    Map::SetBuilding(b->x, b->y, b);
    b->index = (int)p->buildings.size() - 1;
    b->patch = p;
    b->buildLeft = (double)b->data->constructionTime;
    b->stateTime = Timer::GetGlobalTime();
    b->SetClosed(true);
    if (b->data->parkingCount != 0) b->workers.assign(b->data->parkingCount, nullptr);
    b->FindBaseCoordinates();
    b->LinkBaseToBuilding();
    b->UpdateImage();
    b->f54 = Timer::GetGlobalTime() + b->data->constructionTime;
    BuildingHovers::RegisterBuilding(b);
    b->SetUniqueID(GameState::latestUniqueID + 1);
}

void CheckStartingResource(int type, const char* setting, const char* name) {
    int have = (int)GameState::GetResourceAmount(type), want = Setting(setting).GetInt();
    if (have != want)
        std::fprintf(stderr,
                     "ERROR: TutorialWindow::UpdateCity() Amount of %s after tutorial (%d) doesn't equal %s (%d)\n",
                     name, have, setting, want);
}

// @0x36693c
void UpdateSecond(float) {
    int& step = GameState::secondTutorial;
    if (GameState::lastSentStep != step) {
        GameState::lastSentStep = step;
        TutorialSender::AddTutorialStep(step);
    }
    if (step == 0x100) return;
    Entity* player = EntityManager::GetPlayer();
    BaseSquad* squad = player->GetSquad();
    // His companion speaks (the hero himself when he is alone).
    Entity* who = squad->GetSoldierCount() > 1 ? squad->GetSoldier(1) : player;
    const uint32_t map = GameState::GetCurrentMapID();
    bool beltCheck = false;
    switch (step) {
    case 0x82:
        if (map == 0xe) {
            Say(who, "TUTORIAL_DIALOG_4", 0, false, false, true, false, false);
            step = 0x84;
        }
        break;
    case 0x8a:
        beltCheck = true;
        break;
    case 0x8d:
        if (map == 0x6c) {
            Say(who, "TUTORIAL_SECOND_2", 0, false, false, true, false, false);
            if (GameState::IsCombatActive()) {
                if (Entity* e = GameState::GetActiveCombat()->GetEnemySquad()->GetSoldier(2)) {
                    GUI::SetInteractionObjectLock(e, nullptr);
                    BuildingHovers::SetArrowClickCallback(AttackCallback(player, e));
                    BuildingHovers::ArrowAt(e->worldX + 20.f, e->worldY + e->GetIdleHeight() * -0.5f, false, true, false,
                                            false, false, false, false);
                }
            }
            step = 0x8e;
        }
        break;
    case 0x8f:
        if (map == 0x6c) {
            Say(who, "TUTORIAL_SECOND_2_5", 0, false, false, true, false, false);
            step = 0x90;
        }
        break;
    case 0x90:
        if (map == 0x6c) {
            if (GameState::IsCombatActive()) {
                if (Entity* e = GameState::GetActiveCombat()->GetEnemySquad()->GetSoldier(1)) {
                    GUI::SetInteractionObjectLock(e, nullptr);
                    BuildingHovers::SetArrowClickCallback(AttackCallback(player, e));
                    BuildingHovers::ArrowAt(e->worldX, e->worldY - e->GetIdleHeight(), false, false, false, false, false,
                                            false, false);
                }
            }
            step = 0x91;
        }
        break;
    case 0x92:
        if (map == 0x91) {
            Say(who, "TUTORIAL_SECOND_3", 0, false, false, false, true, false);
            step = 0x93;
        }
        break;
    case 0x97:
        if (map == 0x91) {
            if (who != player) who->SetAttackMelee(2);
            GUI::SetInteractionLock(true);
            BottomBattleWindow::Show();
            BuildingHovers::HideWorldDialog(nullptr);
            BuildingHovers::HideArrow();
        }
        break;
    case 0x9c:
        if (map == 0x91) {
            Say(who, "TUTORIAL_SECOND_5", 0, false, false, false, false, false);
            step = 0x9d;
            if (Tasks::TaskInfo* t = Tasks::GetTask(0x235)) t->postQuest = 0;
        }
        break;
    case 0xa1:
        if (Tasks::TaskInfo* t = Tasks::GetTask(0x3f1)) PlayerTopWindow::SetParameters(t->preQuest, t->id, false, true);
        // (SettingsWindow's Facebook and Twitter switches are turned on: online.)
        step = 0xa3;
        break;
    case 0x20d:   // the arrow at the decoration on (9, 0x15)
        if (WindowManager::GetShownWindowCount() != 0) {
            if (BuildingHovers::GetArrowClickCallbackID() == g_arrowCallback) BuildingHovers::HideArrow();
        } else if (Map::Decor* d = Map::GetDecoration(9, 0x15)) {
            Render::Sprite* s = d->sprite;
            int sx = (int)(s->x + s->w * 0.5f), sy = (int)(s->y - s->h);
            Map::WorldCoordinatesToScreen(sx, sy);
            if (BuildingHovers::GetArrowClickCallbackID() != g_arrowCallback)
                g_arrowCallback = BuildingHovers::SetArrowClickCallback(
                    new GUI::Callback([d, sx, sy] { BuildingHovers::OnDecorClick(d, sx, sy); }));
            ArrowAtDecor(d);
            if (!g_decorShown) CenterOn(s->x + s->w * 0.5f, s->y + s->h * -0.5f);
            g_decorShown = true;
        }
        break;
    case 0x20e:   // the arrow at the nearest enemy of map 0xf
        if (WindowManager::GetShownWindowCount() != 0 || map != 0xf) {
            if (BuildingHovers::GetArrowClickCallbackID() == g_arrowCallback) BuildingHovers::HideArrow();
        } else if (Entity* e = EntityManager::GetNearestEnemy(player, false, false)) {
            if (BuildingHovers::GetArrowClickCallbackID() != g_arrowCallback)
                g_arrowCallback = BuildingHovers::SetArrowClickCallback(AttackCallback(player, e));
            BuildingHovers::FollowEntity(nullptr);
            BuildingHovers::ArrowAt(e->worldX + 20.f, e->worldY + e->GetIdleHeight() * -0.5f, false, true, false, false,
                                    false, false, false);
            if (!g_enemyShown) CenterOn(e->worldX, e->worldY);
            g_enemyShown = true;
        }
        break;
    default:
        break;
    }
    // Step 0x8a on map 0x6c: an orb on the belt that is not switched on gets its hint first.
    if (beltCheck && map == 0x6c) {
        if (GameState::GetBeltItemAt(0) == 0 || GameState::GetBeltItemActivationTimeAt(0) != 0) {
            step = 0x8d;
        } else {
            Say(who, "TUTORIAL_SECOND_2_ORB", 0, false, false, true, false, false);
            step = 0x8b;
        }
    }
    // The second tutorial is over at level 6. (FileManager::SendTutorialFinished2: online.)
    if ((unsigned)GameState::GetLevel() > 5) step = 0x100;
}

// @0x36731c
void UpdateCity(float dt) {
    int& step = GameState::tutorial;
    if (step == 0x80) {
        UpdateSecond(dt);
        return;
    }
    Entity* player = EntityManager::GetPlayer();
    if (GameState::lastSentStep != step) {
        GameState::lastSentStep = step;
        TutorialSender::AddTutorialStep(step);
    }
    Entity* guide = g_guide;
    switch (step) {
    case 0x18:
        if (Map::Building* hq = Map::GetHQ()) hq->stateTime = Timer::GetGlobalTime();
        Map::Save(0);
        step = 0x19;
        break;
    case 0x19: {   // the city is set up: the guide and his house, the farm's site, the barracks' site
        if (Map::Building* hq = Map::GetHQ()) hq->stateTime = Timer::GetGlobalTime();
        Render::CenterOn(2524.f, 209.f);   // (at once on the original too)
        if (Map::Building* house = Map::GetBuilding(0x1b, 10)) {
            g_guide = EntityManager::SpawnEntityAt(0, 0x18, 0x10, false, false);
            house->AssignLiver(g_guide);
            g_guide->SetHome(house);
        }
        if (Map::GetDecoration(0x1d, 0xe)) {
            Map::EnqueueBuildingPlacement(0x1d, 0xe, 0x13);
            Map::Update(0.0);
        }
        PlaceBarracksSite();
        // The tree on (0x1f, 0x14) has 20 of its wood cut and waiting.
        if (Map::Building* tree = Map::GetBuilding(0x1f, 0x14)) {
            for (int i = 0; i < 0x14; ++i)
                GameState::PlaceOrder(tree, Map::GetNearestStorage(tree, false), 1, tree->data->speedupResource, 0);
            tree->resourceLeft -= 0x14;
            tree->resources[tree->data->speedupResource] += 0x14;
            tree->UpdateResources();
        }
        step = 0x1a;
        break;
    }
    case 0x1a:   // the guide and the hero walk to the castle
        g_guide = EntityManager::GetEntityByClass(0, false, true);
        g_guide->GetAI()->SetSpeed(140.f);
        g_guide->GetAI()->WalkTo(0x1c, 0xb);
        BattleBarWindow::Hide();
        player->SetPos(0x16, 0x11);
        player->GetAI()->UpdateLastTarget();
        player->GetAI()->WalkTo(0x1b, 0xd);
        player->SetHP(player->GetHpMax());
        GameState::UpdatePlayerRegenerationState();
        EntityManager::UpdateGraphics();
        Render::SetViewportMapBounds(0x16, -0xd, 0x25, 0x1e);
        GameState::updated = true;
        if (Map::Building* storage = Map::GetBuildingWithID(0x12)) storage->HireGolbin();
        step = 0x1b;
        break;
    case 0x1d:   // "this is your castle": the arrow at the ruin on (0x21, 0xd)
        TopCityWindow::Show();
        g_guide->SetDirection(1);
        Say(g_guide, "DESC178", 0, false, false, false, true, false);
        if (Map::Decor* d = Map::GetDecoration(0x21, 0xd)) {
            GUI::SetInteractionObjectLock(d, nullptr);
            Render::Sprite* s = d->sprite;
            int sx = (int)(s->x + s->w * 0.5f), sy = (int)(s->y - s->h);
            Map::WorldCoordinatesToScreen(sx, sy);
            BuildingHovers::SetArrowClickCallback(
                new GUI::Callback([d, sx, sy] { BuildingHovers::OnDecorClick(d, sx, sy); }));
            ArrowAtDecor(d);
        }
        step = 0x1e;
        break;
    case 0x20:
        CenterOn(2716.f, 209.f);
        g_guide->SetDirection(1);
        Say(g_guide, "DESC573", 0, false, false, true, true, false);
        step = 0x21;
        break;
    case 0x22:
        g_guide->GetAI()->WalkTo(0x1d, 0x12);
        CenterOn(2478.f, 312.f);
        step = 0x23;
        break;
    case 0x24:   // the cut tree: tap it to have the wood carried
        g_guide->SetDirection(1);
        g_guide->SetWorkplace(Map::GetHQ());
        CenterOn(2478.f, 312.f);
        Say(g_guide, "DESC179", 0, false, false, true, true, true);
        if (Map::Building* b = Map::GetBuilding(0x1f, 0x14)) {
            GUI::SetInteractionObjectLock(b, nullptr);
            ArrowAtBuilding(b, b->minX, b->minY);
            BuildingHovers::ArrowAt(b->baseX, b->minY, false, false, false, false, false, false, false);
        }
        step = 0x25;
        break;
    case 0x27:
        CenterOn(2478.f, 312.f);
        BuildingHovers::HideWorldDialog(nullptr);
        Say(g_guide, "DESC182", 0, false, false, true, true, true);
        g_guide->SetWorkplace(nullptr);
        if (Map::Building* b = Map::GetBuilding(0x1f, 0x14)) {
            GUI::SetInteractionObjectLock(b, nullptr);
            ArrowAtBuilding(b, b->minX, b->minY);
            BuildingHovers::ArrowAt(b->baseX, b->minY, false, false, false, false, false, false, false);
        }
        step = 0x28;
        break;
    case 0x2a:
        g_guide->GetAI()->WalkTo(0x1c, 9);
        CenterOn(2408.f, 108.f);
        step = 0x2f;
        break;
    case 0x2c:
        g_guide->SetDirection(1);
        Say(g_guide, "DESC188", 0, false, false, true, true, true);
        if (Map::Building* b = Map::GetBuilding(0x1e, 7)) {
            GUI::SetInteractionObjectLock(b, nullptr);
            ArrowAtBuilding(b, b->minX, b->minY);
            BuildingHovers::ArrowAt(b->baseX, b->minY, false, false, false, false, false, false, false);
        }
        step = 0x2d;
        break;
    case 0x2f: {   // the view opens to the whole owned land
        g_guide->GetAI()->WalkTo(0x1c, 0xc);
        Map::UpdateOwnedAreaBorders();
        int minX, minY, maxX, maxY;
        Map::GetAreaBorders(minX, minY, maxX, maxY);
        Render::SetViewportMapBounds(minX, minY - 0xd, maxX + 1, maxY);
        step = 0x30;
        break;
    }
    case 0x30:
        if (Entity* e = EntityManager::GetEntityByClass(0x133, true, true)) Render::CenterOn(e->worldX, e->worldY);
        break;
    case 0x33:   // the first level is given
        BuildingHovers::HideArrow();
        HideTask(0xbe);
        GameState::ChangeResourceAmount(GameState::kExpAfter,
                                        (int)GameState::GetLevelXP(1) - (int)GameState::GetResourceAmount(GameState::kExpAfter));
        if (GameState::GetServerSetting(0xb4) == 1) GameState::SetTutorialType(1);
        step = 0x34;
        break;
    case 0x35:
        BuildingHovers::ShowWorldDialog(guide, StringTable::GetString("TUTORIAL_DIALOG_2"), 0, true, true, false, 0, true,
                                        false);
        CheckStartingResource(GameState::kLumber, "starting_resources_lumber", "lumber");
        CheckStartingResource(GameState::kFood, "starting_resources_food", "food");
        CheckStartingResource(GameState::kRocks, "starting_resources_rocks", "rocks");
        step = 0x36;
        break;
    case 0x37:   // the farmer's wife arrives
        if (GameState::GetTutorialType() == 1 &&
            (GameState::GetCurrentMapID() != 0 || WindowManager::GetShownWindowCount() != 0 ||
             !GameState::TaskCompleted(0x427))) {
            Tasks::TaskInfo* t = Tasks::GetTask(0xbe);
            if (t && !t->dnShow) {
                t->dnShow = true;
                HUDWindow::UpdateTasks();
            }
            break;
        }
        BuildingHovers::HideArrow();
        Map::Save(0);
        CenterOn(2441.f, 97.f);
        g_farmer = EntityManager::SpawnEntityAt(7, 0x1b, 4, false, false);
        g_farmer->GetAI()->WalkTo(0x1c, 7);
        if (Tasks::TaskInfo* t = Tasks::GetTask(0xbe)) {
            t->dnShow = true;
            HUDWindow::UpdateTasks();
        }
        HideTask(0xfe);
        HideTask(0xd5);
        HideTask(0xe6);
        HideTask(0x185);
        if (GameState::GetTutorialType() == 1) {
            if (Tasks::TaskInfo* t = Tasks::GetTask(0x429)) {
                t->doNotShowReward = true;
                t->dnShow = true;
                HUDWindow::UpdateTasks();
            }
        }
        GUI::SetInteractionLock(true);
        BattleBarWindow::Show();
        // The farm's own farmer stays unseen until she has gone.
        if (Map::Building* farm = Map::GetBuildingWithID(0x13)) {
            if (!farm->workers.empty() && farm->workers[0]) {
                farm->workers[0]->Disappear();
                farm->workers[0]->SetAlpha(0.f);
            }
        }
        step = 0x38;
        break;
    case 0x3d:
        g_farmer->GetAI()->WalkTo(0x1c, 0xb);
        CenterOn(2347.f, 255.f);
        step = 0x3e;
        break;
    case 0x3f:
        g_farmer->Disappear();
        if (Map::Building* farm = Map::GetBuildingWithID(0x13))
            if (!farm->workers.empty() && farm->workers[0]) farm->workers[0]->Appear(true, true);
        step = 0x40;
        break;
    case 0x40:   // she has faded: the farm view opens
        if (g_farmer && g_farmer->alpha <= 0.f) {
            EntityManager::RemoveEntity(g_farmer, true);
            g_farmer = nullptr;
            if (Map::Building* farm = Map::GetBuilding(0x1d, 0xe)) {
                Tasks::CompleteTask(0xbe, true, true, false);
                Map::ShowFarm(true, farm);
            }
            step = 0x41;
        }
        break;
    case 0x41:
        if (Map::Building* farm = Map::GetCurrentFarm()) {
            farm->farmer->GetAI()->Farm(0, 0, true);
            step = 0x44;
        }
        break;
    case 0x44:
        if (Map::GetCurrentFarm()) {
            BuildingHovers::ShowFarmHover(false, 0, 1);
            step = 0x45;
        }
        break;
    case 0x45:
        if (g_farmer && g_farmer->alpha <= 0.f) {
            EntityManager::RemoveEntity(g_farmer, true);
            g_farmer = nullptr;
        }
        break;
    case 0x46:   // the first soil patch is ripe
        if (Map::Building* farm = Map::GetCurrentFarm())
            if (!farm->patchEntities.empty() && farm->patchEntities[0]->GetAI()->GetState() == 0x19) step = 0x47;
        break;
    case 0x47:   // the arrow at its crop
        if (Map::Building* farm = Map::GetCurrentFarm()) {
            if (farm->patchEntities.empty()) break;
            Entity* patch = farm->patchEntities[0];
            Entity* crop = patch->GetAI()->GetItem();
            if (!crop) break;
            if (BuildingHovers::GetArrowClickCallbackID() != g_arrowCallback) {
                int sx = (int)crop->worldX, sy = (int)(crop->worldY - crop->GetIdleHeight());
                Map::WorldCoordinatesToScreen(sx, sy);
                g_arrowCallback = BuildingHovers::SetArrowClickCallback(
                    new GUI::Callback([patch, sx, sy] { BuildingHovers::OnEntityClick(patch, sx, sy); }));
            }
            GUI::SetInteractionObjectLock(patch, BottomFarmWindow::GetPlantButton());
            BuildingHovers::ArrowAt(crop->worldX, crop->worldY - crop->GetIdleHeight(), false, false, false, false, false,
                                    false, false);
        }
        break;
    case 0x4b:
        if (Map::GetCurrentFarm()) {
            if (Entity* e = EntityManager::GetEntityByClass(0xdf, false, true))
                Say(e, "TUTORIAL_FARM_AUTOMATION", 0, false, false, true, true, false);
            step = 0x48;
        }
        break;
    case 0x4c:   // the farmer harvests and plants again, for the order's price
        if (Map::GetCurrentFarm()) {
            Entity* e = EntityManager::GetEntityByClass(0xdf, false, true);
            Map::Building* farm = Map::GetCurrentFarm();
            if (e && farm) {
                GameState::ChangeResourceAmount(GameState::kGold, -(int)farm->data->delivery->missions[0].price);
                e->GetAI()->Farm(999, 0, false);
                e->GetAI()->Farm(0, 0, true);
                BuildingHovers::HideArrow();
            }
            step = 0x4e;
        }
        break;
    case 0x4e:
        if (Map::GetCurrentFarm()) {
            if (Entity* e = EntityManager::GetEntityByClass(0xdf, false, true))
                Say(e, "TUTORIAL_FARM_EXIT", 0, false, false, true, true, false);
            step = 0x4f;
        }
        break;
    case 0x52:   // the arrow at the sawmill
        if (Map::Building* b = Map::GetBuildingWithID(0x6b)) {
            ArrowAtBuilding(b, b->minX, b->minY);
            CenterOn(2288.f, 258.f);
            BuildingHovers::ArrowAt(b->baseX, b->minY, false, false, false, false, false, false, false);
            GUI::SetInteractionObjectLock(b, nullptr);
        }
        step = 0x53;
        break;
    case 0x59:
        if (Entity* e = EntityManager::GetEntityByClass(0, false, true)) {
            CenterOn(2288.f, 258.f);
            e->SetDirection(1);
            Say(e, GameState::GetTutorialType() == 2 ? "TUT_SHORT_DIALOG2" : "DESC1077", 0, false, false, true, true, false);
        }
        step = 0x5a;
        break;
    case 0x60:   // the arrow at the barracks
        if (Entity* e = EntityManager::GetEntityByClass(0, false, true)) {
            e->SetDirection(1);
            Say(e, "DESC1079", 0, false, false, true, true, true);
        }
        if (Map::Building* b = Map::GetBuildingWithID(0x8e)) {
            ArrowAtBuilding(b, b->minX, b->minY);
            CenterOn(2180.f, 258.f);
            BuildingHovers::ArrowAt(b->baseX, b->minY, false, false, false, false, false, false, false);
            GUI::SetInteractionObjectLock(b, nullptr);
        }
        step = 0x61;
        break;
    case 0x64:
        if (Entity* e = EntityManager::GetEntityByClass(0x22, true, true)) Render::CenterOn(e->worldX, e->worldY);
        break;
    case 0x66:   // the envoy
        g_envoy = EntityManager::SpawnEntityAt(6, 0x1b, 4, false, false);
        g_envoy->GetAI()->WalkTo(0x1c, 9);
        Say(g_envoy, "TUTORIAL_DIALOG_3", 0, false, false, true, true, false);
        CenterOn(g_envoy->worldX, g_envoy->worldY);
        HUDWindow::UpdateTasks();
        step = 0x67;
        break;
    default:
        break;
    }
}

// @0x368ee8
void Update(float dt) {
    if (!GameState::IsTutorial()) {
        UpdateCity(dt);
        return;
    }
    g_guide = nullptr;
    HUDWindow::RemoveTaskArrows();
    Entity* player = EntityManager::GetPlayer();
    Entity* soldier = player->GetSquad()->GetSoldier(1);   // Stoutbeard
    int& step = GameState::tutorial;
    if (GameState::lastSentStep != step) {
        GameState::lastSentStep = step;
        TutorialSender::AddTutorialStep(step);
    }
    switch (step) {
    case 0: {   // the scene and the starting purse
        Render::CenterOn(574.f, 256.f);
        soldier->SetPos(6, 8);
        soldier->SetDirection(7);
        soldier->GetAI()->UpdateLastTarget();
        player->SetDirection(7);
        EntityManager::Update(0.f);
        // SoundsManager::PlayMusic("music_battle_map", true) unless it plays: milestone 6.
        GameState::ChangeResourceAmount(GameState::kLumber, Setting("pretut_resources_lumber").GetInt());
        GameState::ChangeResourceAmount(GameState::kFood, Setting("pretut_resources_food").GetInt());
        GameState::ChangeResourceAmount(GameState::kRocks, Setting("pretut_resources_rocks").GetInt());
        GameState::ChangeResourceAmount(GameState::kGold, Setting("pretut_resources_gold").GetInt());
        GameState::AddCrystals(Setting("pretut_resources_crystals").GetInt());
        // What a game reset kept (HUDWindow::GameReset) comes back, where it is more.
        int gold = SystemFuncs::GetSetting_Int("pre_resetgame_gold", 0, "settings");
        int crystals = SystemFuncs::GetSetting_Int("pre_resetgame_crystals", 0, "settings");
        if ((gold < 1 ? crystals : gold) > 0) {
            if (Setting("pretut_resources_gold").GetInt() < gold)
                GameState::ChangeResourceAmount(GameState::kGold, gold - Setting("pretut_resources_gold").GetInt());
            if (Setting("pretut_resources_crystals").GetInt() < crystals)
                GameState::AddCrystals(crystals - Setting("pretut_resources_crystals").GetInt());
        }
        step = 1;
        break;
    }
    case 2:   // who are you: the hero's name and looks
        SelectHQWindow::SetMode(0);
        SelectHQWindow::Show();
        step = 3;
        break;
    case 4:
        PlayerTopWindow::Show();
        soldier->GetAI()->WalkTo(4, 0x11);
        soldier->f96 = true;
        step = 5;
        break;
    case 6:
        if (GameState::GetServerSetting(0xb5) == 1) {   // the short tutorial
            GameState::SetTutorialType(2);
            GameState::ChangeResourceAmount(GameState::kLumber, 5);
            GameState::ChangeResourceAmount(GameState::kRocks, 5);
            GameState::ChangeResourceAmount(GameState::kCrystal, -12);
            GameState::AddItem(0xaf, 1, false, false, false);
        }
        soldier->SetDirection(1);
        Say(soldier, GameState::GetTutorialType() == 2 ? "TUT_SHORT_DIALOG1" : "DESC322", 0, true, true, false, false, false);
        step = 7;
        break;
    case 8: {   // the arrow at the way on: the portal decoration on (6, 5)
        Map::Decor* d = Map::GetDecoration(6, 5);
        AIBaseState* ai = player->GetAI();
        Portal* portal = d->portal;
        BuildingHovers::SetArrowClickCallback(new GUI::Callback([ai, portal] { ai->ClickedPortal(portal); }));
        ArrowAtDecor(d);
        CenterOn(574.f, 143.f);
        HideTask(0x136);
        step = 9;
        break;
    }
    case 10:   // the first monster
        Say(soldier, "DESC309", 0, false, false, false, false, false);
        if (Entity* e = EntityManager::GetEntityAtXY(4, 6)) {
            GUI::SetInteractionObjectLock(e, nullptr);
            BuildingHovers::SetArrowClickCallback(AttackCallback(player, e));
            BuildingHovers::ArrowAt(e->worldX, e->worldY - e->GetIdleHeight(), false, false, false, false, false, false,
                                    false);
        }
        BattleBarWindow::Show();
        step = 0xb;
        break;
    case 0xc: {   // the villager thanks the hero by name
        BuildingHovers::Update(0.f, true);
        GameState::CompleteTask(0x76c, false);
        EntityManager::OnTaskCompleted(0x76c);
        Map::OnTaskCompleted(0x76c);
        Tasks::Update(true);
        HUDWindow::UpdateTasks();
        CenterOn(462.f, 100.f);
        std::u32string text =
            SWPrintf(0x100, StringTable::GetString("battle_tut_05_villager"), {GameState::GetPlayerName()});
        if (Entity* e = EntityManager::GetEntityAtXY(5, 4)) {
            GUI::SetInteractionObjectLock(nullptr, nullptr);
            BuildingHovers::ShowWorldDialog(e, text.c_str(), 0, true, true, false, 0, true, true);
        }
        step = 0xf;
        break;
    }
    case 0x10:
        CenterOn(412.f, 100.f);
        BuildingHovers::HideArrow();
        Say(soldier, "Tutorial_dialog_7", 0, false, false, true, true, true);
        step = 0x12;
        break;
    case 0x12: {   // the arrow at the chest on (6, 3)
        Map::Decor* d = Map::GetDecoration(6, 3);
        BuildingHovers::SetArrowClickCallback(new GUI::Callback([d] { BuildingHovers::OnDecorClick(d, 0, 0); }));
        ArrowAtDecor(d);
        GUI::SetInteractionObjectLock(d, nullptr);
        step = 0x14;
        break;
    }
    case 0x16: {
        CenterOn(478.f, 100.f);
        soldier->SetDirection(1);
        BuildingHovers::HideArrow();
        std::u32string text = SWPrintf(0x100, StringTable::GetString("TAME_TUT1"), {GameState::GetPlayerName()});
        BuildingHovers::ShowWorldDialog(soldier, text.c_str(), 0x76d, true, true, true, 0, true, false);
        step = 0x17;
        break;
    }
    case 0x17:
        if (GameState::TaskCompleted(0x76e) && !GameState::TaskCompleted(0x76f) && !BuildingHovers::IsWorldDialogVisible())
            Say(soldier, "TAME_TUT1a", 0x76f, true, true, true, true, false);
        break;
    default:
        break;
    }
}

}  // namespace

WindowManager::FunctionalWindow* Queue() {
    if (!g_queue) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.click = Click;
        f.setZ = SetZ;
        f.hide = Hide;
        f.update = Update;
        g_queue = new WindowManager::FunctionalWindow("TutorialWindow", std::move(f));
    }
    return g_queue;
}

// @0x366230 (Map::Free)
void ResetState() {
    g_f0c = 0.f;
    g_enemyShown = false;
    g_guide = nullptr;
    g_farmer = nullptr;
    g_envoy = nullptr;
    g_decorShown = false;
}

// @0x366268 (the quest help's "come back later" time; only the game bot reads it)
void WaitUntil(uint32_t time) {
    if (g_waitUntil < time) g_waitUntil = time;
}

}  // namespace TutorialWindow
