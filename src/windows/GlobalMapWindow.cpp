// GlobalMapWindow: the world map (World_map_holder_01.xml). The castle, the campaigns' main nodes
// and the missions' side nodes over a scrolling parchment; the hero's marker travels between them.
// Port of GlobalMapWindow (libkingdom.so 5.11), 0x2bd364..0x2c6494. Statics at 0x61f850..0x620968.
//
// UNVERIFIED (milestone 5): the windows its nodes open (RaidInfoWindow,
// SultanArrivingWindow, CampaignCompleteWindow, RaidCompleteWindow,
// CampaignPerfectedWindow, PlayerWeakWindow), the PvP travel to map 0x70 and the castle's featured
// quests are not ported yet; each call site says so. Expansions (downloaded content) are online:
// Expansions::AreEnabled() is false offline, so their branches are left out. flagDemoVersion is 0.
// Sounds and music: milestone 6 (each call site is noted).
#include <cmath>
#include <cstdio>
#include <vector>

#include "engine/IconManager.h"
#include "engine/Render.h"
#include "engine/Resources.h"
#include "engine/Timer.h"
#include "game/BuildingHovers.h"
#include "game/Combat.h"
#include "game/Entity.h"
#include "game/EntityManager.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/Locations.h"
#include "game/Map.h"
#include "game/MetaData.h"
#include "game/Rand48.h"
#include "game/SaveManager.h"
#include "game/Setting.h"
#include "game/StringTable.h"
#include "game/Tasks.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/HUD.h"
#include "hud/Shared.h"
#include "windows/Windows.h"

namespace GlobalMapWindow {

using GUI::Button;
using GUI::Textfield;
using GUI::Window;
using Locations::LocationInfo;

bool unlockAll = false;   // GlobalMapWindow::unlockAll (a debug switch; 0 in 5.11)

namespace {

WindowManager::FunctionalWindow* g_queue = nullptr;   // GlobalMapWindow::wnd 0x620970
Window* g_world = nullptr;            // GlobalMapWindow::world 0x6209e0 World_map_holder_01.xml
float g_windowScale = 1.f;            // GlobalMapWindow::windowScale
const char* const kWindowConfigs[6] = {   // GlobalMapWindow::windowConfigs 0x60f244
    "button_world_map_kingdom", "button_world_map_desert", "button_world_map_ice",
    "button_world_map_haunted_castle", "button_world_map_ice_castle", "button_world_map_arena"};
const char* const kElementNames[6] = {    // GlobalMapWindow::elementNames 0x60f25c
    "buton_comp_kingdom_castle_", "button_comp_desert", "button_comp_winter", "button_comp_haunted",
    "button_comp_ice_kingdom", "button_comp_arena_active"};

int g_arrowWindowLimit = 1;           // 0x60f22c
float g_speed = 2.f;                  // 0x60f230 the travel's duration in seconds
int g_margin = 0x100;                 // 0x60f234 (Init: 256 times the scale)
int g_worldX = 0x5f, g_worldY = 0x96; // 0x60f238 0x60f23c the map's offset in the parchment

}  // namespace

// The locations (vtables 0x6093b0, 0x609370, 0x609330, 0x6092f0, 0x6092b0; slots in this order).
struct Location {
    virtual ~Location() = default;
    virtual void Init(Window* w, unsigned index, bool side);   // +0x08 @0x2bd3d4
    virtual void Deinit() {}                                   // +0x0c @0x2bd404
    virtual void Show() { window->SetVisibility(true); }       // +0x10 @0x2bd408
    virtual void Hide() { window->SetVisibility(false); }      // +0x14 @0x2bd428
    virtual void Update() {}                                   // +0x18 @0x2bd448
    virtual void UpdatePosition() {}                           // +0x1c @0x2bd44c
    virtual uint32_t GetCampaignTask() { return 0; }           // +0x20 @0x2bd450
    virtual void ShowCompletionWindow();                       // +0x24 @0x2bf9dc
    virtual void OnClick() {}                                  // +0x28 @0x2bd458
    virtual void GetMarkerLocation(int& x, int& y);            // +0x2c @0x2bd45c
    virtual uint32_t GetCurrentMap() { return info->startMap; }   // +0x30 @0x2bd49c
    virtual bool HasMap(uint32_t map);                         // +0x34 @0x2bd4a8

    Window* window = nullptr;         // +0x04
    Window* window2 = nullptr;        // +0x08 where the helper arrow points
    bool locked = false;              // +0x0c
    LocationInfo* info = nullptr;     // +0x10
    bool mark = false;                // +0x14 GetMapForCraftItem / GetMissionMapForItem's scratch flag
};

struct CastleLocation : Location {   // 0x2c bytes
    void Init(Window* w, unsigned index, bool side) override;   // @0x2bf738
    void OnClick() override;                                    // @0x2c1c10
    void GetMarkerLocation(int& x, int& y) override;            // @0x2bd5e0

    Window* comp = nullptr;           // +0x18 "%s.<element name>"
    Window* lock = nullptr;           // +0x1c icon_lock
    Textfield* levelReq = nullptr;    // +0x20 icon_lock.text_level_req
    Button* clickArea = nullptr;      // +0x24
    Window* questNew = nullptr;       // +0x28 icon_gold_quest_new
};

struct NamedCastleLocation : CastleLocation {   // 0x34 bytes
    void Init(Window* w, unsigned index, bool side) override;   // @0x2bf8d8
    void GetMarkerLocation(int& x, int& y) override;            // @0x2bd6f0

    Window* nameHolder = nullptr;     // +0x2c bg_name_holder
    Textfield* name = nullptr;        // +0x30 bg_name_holder.text_player_name
};

struct HomeLocation : Location {     // 0x38 bytes
    void Init(Window* w, unsigned index, bool side) override;   // @0x2bf60c
    void Show() override;                                       // @0x2c0e2c
    void OnClick() override;                                    // @0x2c1914
    void GetMarkerLocation(int& x, int& y) override;            // @0x2bd818
    uint32_t GetCurrentMap() override { return 0; }             // @0x2bd870

    Window* castles[6] = {};          // +0x18 buton_comp_kingdom_castle_01..06, one per castle level
    Button* clickArea = nullptr;      // +0x30
    Textfield* name = nullptr;        // +0x34 bg_name_holder.text_player_name
};

struct CoreLocation : Location {     // 0x5c bytes; side = a mission's node (with stars)
    ~CoreLocation() override;
    void Init(Window* w, unsigned index, bool side) override;   // @0x2c0974
    void Deinit() override;                                     // @0x2bd958
    void Show() override;                                       // @0x2bfdc0
    void Hide() override;                                       // @0x2bd98c
    void Update() override;                                     // @0x2c4aac
    void UpdatePosition() override;                             // @0x2bfb88
    uint32_t GetCampaignTask() override;                        // @0x2bfa74
    void ShowCompletionWindow() override;                       // @0x2bf93c
    void OnClick() override;                                    // @0x2c192c
    void GetMarkerLocation(int& x, int& y) override;            // @0x2bd9b4
    uint32_t GetCurrentMap() override;                          // @0x2bf3b4

    Window* blueBg = nullptr;         // +0x18 button_core_node_blue_bg
    Window* lock = nullptr;           // +0x1c icon_lock
    Textfield* levelReq = nullptr;    // +0x20 icon_lock.text_level_req
    Window* goldConfirm = nullptr;    // +0x24 icon_gold_confirm
    Window* questNew = nullptr;       // +0x28 icon_gold_quest_new
    Textfield* percent = nullptr;     // +0x2c text_progress_percent (missions: the time left)
    Button* clickArea = nullptr;      // +0x30
    Window* starBronze = nullptr;     // +0x34 (side nodes only; else the dummy window)
    Window* starSilver = nullptr;     // +0x38
    Window* starGold = nullptr;       // +0x3c
    Window* starDisabled = nullptr;   // +0x40 icon_star_disabled
    Window* starInactive = nullptr;   // +0x44 icon_star_disabled_inactive
    Window* statusActive = nullptr;   // +0x48 icon_star_status_active
    Window* statusTimeout = nullptr;  // +0x4c icon_star_status_timeout
    Window* locked16 = nullptr;       // +0x50 icon_16_locked (gold_16_lock)
    Window* workHere = nullptr;       // +0x54 Button_work_here_holder.xml (the finished mission's sand clock)
    Window* workHereIcon = nullptr;   // +0x58
};

namespace {

struct Arrow {                        // 0x10 bytes (vector 0x6206d0)
    Render::Sprite* sprite = nullptr;
    float x = 0, y = 0;
    unsigned location = 0;            // index into g_locations
};

std::vector<Location*> g_locations;   // 0x6206b0 (count 0x6206bc)
Window* g_marker = nullptr;           // 0x6206c0 World_marker_holder.xml
Window* g_markerMale = nullptr;       // 0x6206c4
Window* g_markerFemale = nullptr;     // 0x6206c8
bool g_sultanMovement = false;        // 0x6206cc
bool g_preventArrowHide = false;      // 0x6206cd
bool g_sepiaActive = false;           // 0x6206ce
std::vector<Arrow> g_arrows;          // 0x6206d0
bool g_arenaEventPending = false;     // 0x6206dc
uint32_t g_arenaEvent = 0;            // 0x6206e0
NamedCastleLocation g_desert;         // 0x6206e4 [1]
NamedCastleLocation g_ice;            // 0x620718 [2]
NamedCastleLocation g_haunted;        // 0x62074c [3]
NamedCastleLocation g_arena;          // 0x620780 [5]
HomeLocation g_home;                  // 0x6207b4 [0]
CastleLocation g_iceCastle;           // 0x6207ec [4]
CoreLocation g_core[16];              // 0x61f850 the campaigns' nodes
CoreLocation g_side[24];              // 0x61fe10 the missions' nodes
CoreLocation g_start;                 // 0x620818 button_side_node_start (the tutorial's way home)
Window* g_sultanPath[7] = {};         // 0x620874 marker_sultan_path.marker_01..07
Render::Sprite* g_sepia = nullptr;    // 0x620890
Render::Texture* g_sepiaTex = nullptr;   // 0x620894
bool g_traveling = false;             // 0x620898
uint32_t g_dest = 0;                  // 0x62089c
bool g_keep = false;                  // 0x6208a0 SwitchMap's second argument on arrival
float g_t = 0.f;                      // 0x6208a4
bool g_tutorialAnim = false;          // 0x6208a8 from the start node to the castle
bool g_ambush = false;                // 0x6208a9 halfway the ambush banner, no map switch
int g_fromX = 0, g_fromY = 0, g_toX = 0, g_toY = 0;   // 0x6208ac..0x6208b8
Render::Sprite* g_pattern[4] = {};    // 0x6208bc..0x6208c8 images/WorldMap/pattern_paper_bg
Render::Sprite* g_bg = nullptr;       // 0x6208cc images/WorldMap/world_map_bg
Window* g_sultan = nullptr;           // 0x6208d0 World_marker_event_holder.xml
Textfield* g_sultanTimer = nullptr;   // 0x6208d4 timer_time
Shared::ButtonBuildingControls g_return;     // 0x6208d8
GUI::MovementEffect* g_returnEffect = nullptr;     // 0x620900
Shared::ButtonBuildingControls g_navigate;   // 0x620904
GUI::MovementEffect* g_navigateEffect = nullptr;   // 0x62092c
uint32_t g_lastBattleMap = 0;         // 0x620930
float g_velX = 0.f, g_velY = 0.f;     // 0x620934 0x620938 the fling after a drag
int g_scrollX = 0, g_scrollY = 0;     // 0x62093c 0x620940
int g_sultanDay = 0;                  // 0x620944
bool g_dragging = false;              // 0x620948
int g_dragX = 0, g_dragY = 0;         // 0x62094c 0x620950
bool g_dragMoved = false;             // 0x620954
bool g_lastUnlockAll = false;         // 0x620955
float g_decel = 0.f;                  // 0x620958 (Init: 5000)
float g_accX = 0.f, g_accY = 0.f;     // 0x62095c 0x620960
int g_lastSecond = 0;                 // 0x620964
uint32_t g_sultanSecs = 0;            // 0x620968

// The tasks of the locations without a map of their own (CoreLocation::OnClick).
const uint32_t kTasksB[4] = {0x429, 0x26a, 0x35a, 0x35c};     // 0x597774 location 0xb
const uint32_t kTasks2f[4] = {0x2f1, 0x35e, 0x360, 0x2da};    // 0x597784 location 0x2f
const uint32_t kTasks38[2] = {0x2a7, 0x484};                  // 0x597794 location 0x38

Window* WorldRoot() { return g_world; }

// The marker goes to a screen point (the window's +0x08 is SetPosition).
void PlaceSultan() {
    Window* m = g_sultanPath[g_sultanDay];
    g_sultan->SetPosition((g_world->x + m->x) - g_sultan->w / 2, (g_world->y + m->y) - g_sultan->h);
}

// @0x2bdd94
bool OnBack() { return false; }

// @0x2bf3b0: the "Navigate" button opens ChoosePathWindow.
void OnNavigate() { ChoosePathWindow::Show(); }

// @0x2bf434. UNVERIFIED (milestone 5): SultanArrivingWindow (Show and UpdateContents).
void OnSultan() {}

}  // namespace

// ---- Location ------------------------------------------------------------------------------

void Location::Init(Window* w, unsigned, bool) {
    window = w;
    w->SetVisibility(false);
    window2 = window;
}

// @0x2bf9dc: the desert campaign (0x3ea) is the only castle with a campaign of its own.
void Location::ShowCompletionWindow() {
    if (!info || info->campaign != 0x3ea) return;
    int crystals = Setting("campaign_complete_cb").GetInt();
    if (4 < crystals) crystals = 5;
    std::printf("Adding %d crystals for completing the campaign\n", crystals);
    GameState::AddCrystals(crystals);
    // UNVERIFIED (milestone 5): CampaignCompleteWindow::SetParameters(info->id) and Show.
}

void Location::GetMarkerLocation(int& x, int& y) {
    x = WorldRoot()->x + window->x;
    y = WorldRoot()->y + window->y;
}

bool Location::HasMap(uint32_t map) {
    if (!info || info->maps.empty()) return false;
    for (const Locations::MapInfo& m : info->maps)
        if (m.id == map) return true;
    return false;
}

// ---- CastleLocation ------------------------------------------------------------------------

void CastleLocation::Init(Window* w, unsigned index, bool side) {
    Location::Init(w, index, side);
    const char* cfg = kWindowConfigs[index];
    comp = GUI::GetWindowTypedF<Window>(g_world, "%s.%s", cfg, kElementNames[index]);
    lock = GUI::GetWindowTypedF<Window>(g_world, "%s.icon_lock", cfg);
    lock->SetVisibility(false);
    levelReq = GUI::GetWindowTypedF<Textfield>(g_world, "%s.icon_lock.text_level_req", cfg);
    clickArea = GUI::GetWindowTypedF<Button>(g_world, "%s.clickArea", cfg);
    clickArea->SetOnClick([this] { OnClick(); });
    questNew = GUI::GetWindowTypedF<Window>(g_world, "%s.icon_gold_quest_new", cfg);
    questNew->SetTexture(IconManager::GetIcon("gold_quest_new", false));
    questNew->SetVisibility(false);
}

// The castles' campaigns: the desert's (0x3ea) opens its campaign window, the arena (0x3ec) the
// arena; any other open castle is travelled to. Until task 0x773 (the tutorial's ambush) is done
// the hero is healed, gets item 0x5b and travels with the ambush.
void CastleLocation::OnClick() {
    if (!info) return;
    if (comp->enabled) {
        if (info->campaign == 0x3ea || info->campaign == 0x3ec) {
            if (info->campaign == 0x3ea) {
                CampaignInfoWindow::SetParameters(info, info->startMap, false);
                CampaignInfoWindow::Show();
                CampaignInfoWindow::SetAnimationOnce();
                return;
            }
        }
        if (GameState::TaskCompleted(0x773)) {
            SwitchMapAnimated(info->startMap, false, 500.f, false);
            return;
        }
        Entity* player = EntityManager::GetPlayer();
        player->SetHP(player->GetHpMax());
        GameState::UpdatePlayerRegenerationState();
        if (!SetInfoWindow::GetPlayerItemOrBetter(0x5b))
            if (GameState::PlayerItem* it = GameState::AddItem(0x5b, 1, false, false, false))
                GameState::BindItemTo(8, it);
        Map::SafeSave();
        SwitchMapAnimated(info->startMap, false, 500.f, true);
        return;
    }
    uint32_t id = info->id;
    if (id == 0x49 || id == 0x53)
        PopupWindow::Show(StringTable::GetString("LOCATION_LOCKED"), PopupWindow::Hide, nullptr, nullptr);
    else if (id == 0x10 || id == 0x19)
        PopupWindow::Show(StringTable::GetString("COMING_SOON"), PopupWindow::Hide, nullptr, nullptr);
}

void CastleLocation::GetMarkerLocation(int& x, int& y) {
    x = WorldRoot()->x + window->x;
    y = WorldRoot()->y + window->y;
}

void NamedCastleLocation::Init(Window* w, unsigned index, bool side) {
    CastleLocation::Init(w, index, side);
    nameHolder = GUI::GetWindowTypedF<Window>(g_world, "%s.bg_name_holder", kWindowConfigs[index]);
    name = GUI::GetWindowTypedF<Textfield>(g_world, "%s.bg_name_holder.text_player_name", kWindowConfigs[index]);
}

void NamedCastleLocation::GetMarkerLocation(int& x, int& y) {
    x = window->x + window->w - 0x32 + WorldRoot()->x;
    y = window->y + window->h - 0x1e + WorldRoot()->y;
}

// ---- HomeLocation --------------------------------------------------------------------------

void HomeLocation::Init(Window* w, unsigned index, bool side) {
    Location::Init(w, index, side);
    for (int i = 0; i < 6; ++i) {
        castles[i] = GUI::GetWindowTypedF<Window>(g_world, "%s.buton_comp_kingdom_castle_%02d", kWindowConfigs[index], i + 1);
        castles[i]->SetVisibility(false);
    }
    clickArea = GUI::GetWindowTypedF<Button>(g_world, "%s.clickArea", kWindowConfigs[index]);
    clickArea->SetOnClick([this] { OnClick(); });
    name = GUI::GetWindowTypedF<Textfield>(g_world, "%s.bg_name_holder.text_player_name", kWindowConfigs[index]);
}

// The castle as built: building 99's level (or its offline record's) picks the picture.
void HomeLocation::Show() {
    window->SetVisibility(true);
    int level;
    if (Map::Building* b = Map::GetBuildingWithID(99)) level = b->level;
    else if (GameState::OfflineBuilding* o = GameState::GetOfflineBuildingWithID(99)) level = (int)o->level;
    else level = 0;
    for (int i = 0; i < 6; ++i) castles[i]->SetVisibility(i == level);
}

void HomeLocation::OnClick() { SwitchMapAnimated(0, false, 500.f, false); }

void HomeLocation::GetMarkerLocation(int& x, int& y) {
    x = window->x + window->w - 0x32 + WorldRoot()->x;
    y = window->y + window->h - 0x1e + WorldRoot()->y;
}

// ---- CoreLocation --------------------------------------------------------------------------

CoreLocation::~CoreLocation() = default;

// Main nodes (side false) have the blue background, the lock and its level; side nodes the stars
// and the mission's status, and their arrow points at the node itself.
void CoreLocation::Init(Window* w, unsigned, bool side) {
    window = w;
    w->SetVisibility(false);
    const char* n = w->name.c_str();
    if (side) {
        blueBg = GUI::DummyWindow();
        lock = GUI::DummyWindow();
        lock->SetVisibility(false);
        levelReq = GUI::DefaultTextfield();
    } else {
        blueBg = GUI::GetWindowTypedF<Window>(g_world, "%s.button_core_node_blue_bg", n);
        lock = GUI::GetWindowTypedF<Window>(g_world, "%s.icon_lock", n);
        lock->SetVisibility(false);
        levelReq = GUI::GetWindowTypedF<Textfield>(g_world, "%s.icon_lock.text_level_req", n);
    }
    goldConfirm = GUI::GetWindowTypedF<Window>(g_world, "%s.icon_gold_confirm", n);
    goldConfirm->takesZ = true;
    questNew = GUI::GetWindowTypedF<Window>(g_world, "%s.icon_gold_quest_new", n);
    questNew->takesZ = true;
    percent = GUI::GetWindowTypedF<Textfield>(g_world, "%s.text_progress_percent", n);
    clickArea = GUI::GetWindowTypedF<Button>(g_world, "%s.clickArea", n);
    clickArea->SetOnClick([this] { OnClick(); });
    if (side) {
        starBronze = GUI::GetWindowTypedF<Window>(g_world, "%s.icon_star_bronze", n);
        starSilver = GUI::GetWindowTypedF<Window>(g_world, "%s.icon_star_silver", n);
        starGold = GUI::GetWindowTypedF<Window>(g_world, "%s.icon_star_gold", n);
        starDisabled = GUI::GetWindowTypedF<Window>(g_world, "%s.icon_star_disabled", n);
        starInactive = GUI::GetWindowTypedF<Window>(g_world, "%s.icon_star_disabled_inactive", n);
        statusActive = GUI::GetWindowTypedF<Window>(g_world, "%s.icon_star_status_active", n);
        statusTimeout = GUI::GetWindowTypedF<Window>(g_world, "%s.icon_star_status_timeout", n);
        locked16 = GUI::GetWindowTypedF<Window>(g_world, "%s.icon_16_locked", n);
        locked16->SetTexture(IconManager::GetIcon("gold_16_lock", false), true);
        window2 = GUI::GetWindowTypedF<Window>(g_world, "%s", n);
    } else {
        Window* dummy = GUI::DummyWindow();
        starBronze = starSilver = starGold = starDisabled = starInactive = statusActive = statusTimeout = locked16 = dummy;
        dummy->SetTexture(IconManager::GetIcon("gold_16_lock", false), true);
        window2 = GUI::GetWindowTypedF<Window>(g_world, "%s.button_core_node_blue_bg", n);
    }
    workHere = GUI::RegisterUI("../resource/kingdom_ui/1Original/Button_work_here_holder.xml",
                               "Button_work_here_holder.png", GUI::GetHudScaleFactor(), 0, 0, 0, 0, false, 1.f);
    workHere->SetVisibility(false);
    workHere->SetOnClick([this] { OnClick(); });
    GUI::GetWindowTyped<Window>(workHere, "button_work_here.button_bg_green")->SetVisibility(false);
    workHereIcon = GUI::GetWindowTypedF<Window>(workHere, "button_work_here.icon_work_here");
    workHereIcon->SetTexture(IconManager::GetSandClockIcon());
}

void CoreLocation::Deinit() {
    delete workHere;
    workHere = nullptr;
}

void CoreLocation::Hide() {
    Location::Hide();
    workHere->SetVisibility(false);
}

// How the node looks: open (the campaign's progress, the gold check when complete, "!" when never
// started), the next one to open (the lock with the level it needs), two ahead (faded) or further.
// Missions show their stars and whether the last run timed out. Nothing changes in the opening
// tutorial or for map 0xd.
void CoreLocation::Show() {
    if (GameState::IsTutorial()) return;
    if (!info) return;
    if (info->startMap == 0xd) return;
    const int count = Locations::GetLocationTaskCount(info, false);
    const int done = Locations::GetLocationTaskCompleted(info, false);
    locked = false;
    if (info->startMap == 0) locked = true;
    if (Locations::IsLocationLocked(info)) locked = true;
    if (unlockAll && info->startMap != 0) locked = false;
    // (flagDemoVersion: location 0x2f stays locked in the demo)
    bool returnTaskActive = false;
    if (info->returnTasks)
        for (unsigned i = 0; i < info->returnTasks->GetChildrenCount(); ++i)
            returnTaskActive |= Tasks::IsTaskActive((uint32_t)info->returnTasks->GetChild(i)->GetInt(), false);
    if (info->mission) {
        starDisabled->SetVisibility(locked || info->startMap == 0);
        starInactive->SetVisibility(false);
        Window* stars[3] = {starBronze, starSilver, starGold};
        for (int i = 1; i <= 3; ++i) stars[i - 1]->SetVisibility(Locations::GetMissionStarCount(info->startMap, 0) == i);
        if ((uint32_t)GameState::GetMapCompletionCount(info->startMap) < info->starsCount[0]) starBronze->SetVisibility(true);
        const uint32_t start = GameState::GetMapStartTime(info->startMap);
        statusTimeout->SetVisibility(start != 0 && start + info->timeout < (uint32_t)Timer::GetGlobalTime());
        statusActive->SetVisibility(!statusTimeout->visibleSelf);
    }
    goldConfirm->SetVisibility(false);
    questNew->SetVisibility(false);
    if (!locked || returnTaskActive) {
        window->SetVisibility(true);
        blueBg->SetEnabled(true);
        window->SetAlpha(1.f, false);
        lock->SetVisibility(false);
        locked16->SetVisibility(false);
        if (!info->mission) {
            percent->SetText(SWPrintf(0x40, U"%d%%", {(int)((float)(unsigned)done / (float)(unsigned)count * 100.f)}).c_str());
            goldConfirm->SetTexture(IconManager::GetIcon("gold_confirm", false));
            goldConfirm->SetVisibility(count == done);
        }
        if (GameState::GetMapStartTime(info->startMap) != 0) return;
        if (info->mission) return;
        questNew->SetTexture(IconManager::GetExclamaitionIcon(), true);
        questNew->SetVisibility(true);
        return;
    }
    uint32_t id = info->campaign;
    if (!info->mission) {
        if (1 < id) {
            if (!GetCoreLocation(id - 2)->locked) goto previousOpen;
            id = info->campaign;
        }
        if (id == 1) {
        previousOpen:   // the next campaign: the lock and the level it needs
            window->SetVisibility(true);
            blueBg->SetEnabled(false);
            window->SetAlpha(1.f, false);
            percent->SetText(nullptr);
            lock->SetVisibility(true);
            levelReq->SetVisibility((uint32_t)GameState::GetLevel() < info->level);
            levelReq->SetText(ToWideString((int)info->level));
            return;
        }
        if (2 < id) {
            if (!GetCoreLocation(id - 3)->locked) goto twoAhead;
            id = info->campaign;
        }
        if (id == 2) {
        twoAhead:
            window->SetVisibility(true);
            blueBg->SetEnabled(false);
            window->SetAlpha(0.5f, false);
            percent->SetText(nullptr);
            lock->SetVisibility(false);
            return;
        }
        goto further;
    }
    if (id < 2 || GetSideLocation(id - 2)->locked) {
        id = info->campaign;
        if (id != 1) {
            if (2 < id) {
                if (!GetSideLocation(id - 3)->locked) goto sideTwoAhead;
                id = info->campaign;
            }
            if (id == 2) {
            sideTwoAhead:
                window->SetVisibility(true);
                percent->SetText(nullptr);
                starBronze->SetVisibility(false);
                statusActive->SetVisibility(false);
                statusTimeout->SetVisibility(false);
                starDisabled->SetVisibility(false);
                starInactive->SetVisibility(true);
                locked16->SetVisibility(false);
                return;
            }
            goto further;
        }
    }
    // the next mission
    window->SetVisibility(true);
    percent->SetText(nullptr);
    starBronze->SetVisibility(false);
    statusActive->SetVisibility(false);
    statusTimeout->SetVisibility(false);
    starDisabled->SetVisibility(true);
    starInactive->SetVisibility(false);
    locked16->SetVisibility(true);
    return;
further:
    window->SetVisibility(true);
    if (!info->mission) {
        blueBg->SetEnabled(false);
        window->SetAlpha(0.5f, false);
    }
    percent->SetText(nullptr);
    lock->SetVisibility(false);
    locked16->SetVisibility(false);
    starBronze->SetVisibility(false);
    starSilver->SetVisibility(false);
    starGold->SetVisibility(false);
    statusActive->SetVisibility(false);
    statusTimeout->SetVisibility(false);
    starDisabled->SetVisibility(false);
    starInactive->SetVisibility(true);
}

// Before the opening tutorial ends map 0xe keeps the arrow; an open mission shows "Start", its
// time left, or the sand clock once its time is up.
void CoreLocation::Update() {
    if (!info) return;
    const uint32_t map = info->startMap;
    if (map == 0xd) return;
    if (GameState::tutorial < 0x80 && map == 0xe) {
        if (!window->visibleSelf) {
            workHere->SetVisibility(false);
            UpdatePosition();
            return;
        }
        if (WindowManager::GetShownWindowCount() == 1 && GetCurrentArrowTarget() != 6 && !g_traveling)
            ArrowAtMap(0xe, false, false);
    }
    if (window->visibleSelf && info->mission && !locked) {
        const uint32_t start = GameState::GetMapStartTime(map);
        if (start == 0) {
            percent->SetText(StringTable::GetString("START_CAMP"));
            workHere->SetVisibility(false);
        } else if (start + info->timeout < (uint32_t)Timer::GetGlobalTime()) {
            percent->SetText(U"0");
            workHere->SetVisibility(true);
        } else {
            const uint32_t left = (start + info->timeout) - (uint32_t)Timer::GetGlobalTime();
            percent->SetText(SWPrintf(0x80, U"%02d:%02d:%02d", {left / 0xe10, (left / 0x3c) % 0x3c, left % 0x3c}).c_str());
            workHere->SetVisibility(false);
        }
    } else {
        workHere->SetVisibility(false);
    }
    UpdatePosition();
}

// The last visited battle map's node pulses; the sand clock bobs over its node.
void CoreLocation::UpdatePosition() {
    if (!window->visibleSelf) return;
    if (info->maps.size() != 0) {
        bool found = false;
        for (const Locations::MapInfo& m : info->maps)
            if (m.id == g_lastBattleMap) { found = true; break; }
        if (found) {
            float c = cosf((float)Timer::GetTime() * 10.f);
            if (blueBg->texture) {
                c = c + 1.f;
                blueBg->SetAlpha(1.f + c * 0.05f, false);
                if (Render::Sprite* s = blueBg->sprite) {
                    const float scale = g_windowScale;
                    const float grow = scale * (c + c);
                    s->x = ((float)window->root->x + (float)blueBg->origX * scale) - c * scale;
                    s->h = grow + scale * (float)blueBg->texture->h;
                    s->w = grow + scale * (float)blueBg->texture->w;
                }
            }
        }
    }
    const double bob = cos(Timer::GetTime() * 10.0);
    Window* root = window->root;
    workHere->SetPosition((root->x + window->x + window->w / 2) - workHere->w / 2,
                          ((window->y + root->y - 5) - workHere->h) + (int)(bob * 5.0));
    workHere->SetZ(root->z - 0.05f);
}

// The campaign's most recently shown open task (main tasks first).
uint32_t CoreLocation::GetCampaignTask() {
    if (!info || info->maps.empty()) return 0;
    uint32_t bestTime = 0, best = 0;
    for (const Locations::MapInfo& m : info->maps) {
        const uint32_t t = Tasks::GetMapLatestUnfinishedTask(m.id, true);
        if (t != 0 && bestTime < GameState::GetTaskShowTime(t)) {
            bestTime = GameState::GetTaskShowTime(t);
            best = t;
        }
    }
    if (best == 0)
        for (const Locations::MapInfo& m : info->maps) {
            const uint32_t t = Tasks::GetMapLatestUnfinishedTask(m.id, false);
            if (t != 0 && bestTime < GameState::GetTaskShowTime(t)) {
                bestTime = GameState::GetTaskShowTime(t);
                best = t;
            }
        }
    return best;
}

void CoreLocation::ShowCompletionWindow() {
    if (!info) return;
    if (!info->mission) {
        int crystals = Setting("campaign_complete_cb").GetInt();
        if (4 < crystals) crystals = 5;
        std::printf("Adding %d crystals for completing the campaign\n", crystals);
        GameState::AddCrystals(crystals);
        // UNVERIFIED (milestone 5): CampaignCompleteWindow::SetParameters(info->id) and Show.
    } else {
        // UNVERIFIED (milestone 5): RaidCompleteWindow::SetParameters(info->id) and Show.
    }
}

// An open node opens its campaign (CampaignInfoWindow) or mission (RaidInfoWindow); the tutorial's
// start node (4000) travels at once. A closed one says why: the sultan's location while his task
// runs, the special locations by their own tasks, else "LOCATION_LOCKED".
void CoreLocation::OnClick() {
    if (!info) return;
    if (!locked) {
        bool open;
        if (!info->mission) open = blueBg->enabled || info->campaign == 4000;
        else open = !starInactive->visibleSelf && !starDisabled->visibleSelf;
        if (open) {
            if (GameState::tutorial == 0x67) GUI::SetInteractionLock(false);
            const uint32_t map = GetCurrentMap();
            if (map == 0) return;
            if (!info->mission) {
                if (info->campaign == 4000) {
                    SwitchMapAnimated(map, false, 500.f, false);
                    return;
                }
                CampaignInfoWindow::SetParameters(info, map, false);
                CampaignInfoWindow::Show();
                CampaignInfoWindow::SetAnimationOnce();
                return;
            }
            // UNVERIFIED (milestone 5): RaidInfoWindow::SetParameters(info, map, false), Show and
            // SetAnimationOnce.
            return;
        }
    }
    if (Locations::GetLocationType(info) && Tasks::IsTaskActive(0x5c9, false) && !GameState::TaskCompleted(0x5c9)) {
        // UNVERIFIED (milestone 5): SultanArrivingWindow::Show.
        return;
    }
    if (!Locations::GetLocationType(info)) {
        const uint32_t* tasks = nullptr;
        unsigned n = 0;
        if (info->id == 0xb) tasks = kTasksB, n = 4;
        else if (info->id == 0x2f) tasks = kTasks2f, n = 4;
        else if (info->id == 0x38) tasks = kTasks38, n = 2;
        for (unsigned i = 0; i < n; ++i)
            if (GameState::GetTaskBeginTime(tasks[i]) != 0 && !GameState::TaskCompleted(tasks[i])) {
                TaskInfoWindow::SetTask(tasks[i], false);
                TaskInfoWindow::Show();
                return;
            }
    }
    PopupWindow::Show(StringTable::GetString("LOCATION_LOCKED"), PopupWindow::Hide, nullptr, nullptr);
}

void CoreLocation::GetMarkerLocation(int& x, int& y) {
    x = window->x + window->w - 10 + WorldRoot()->x;
    y = window->y + window->h - 3 + WorldRoot()->y;
}

// The location's map that was started last (else its start map).
uint32_t CoreLocation::GetCurrentMap() {
    uint32_t best = info->startMap;
    for (const Locations::MapInfo& m : info->maps) {
        const uint32_t t = GameState::GetMapStartTime(m.id);
        if (t == 0) continue;
        if (GameState::GetMapStartTime(best) < t) best = m.id;
    }
    return best;
}

// ---- the window ----------------------------------------------------------------------------

void Wheel(bool) {}   // @0x2bdb7c (empty; not in the window's functions)

CoreLocation* GetCoreLocation(unsigned i) { return &g_core[i]; }   // @0x2bda0c
CoreLocation* GetSideLocation(unsigned i) { return &g_side[i]; }   // @0x2bda24

WindowManager::FunctionalWindow* Queue() {
    if (!g_queue) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.deinit = Deinit;
        f.click = Click;
        f.setZ = SetZ;
        f.hide = Hide;
        f.move = Move;
        g_queue = new WindowManager::FunctionalWindow("GlobalMapWindow", std::move(f));
    }
    return g_queue;
}

// @0x2bda40: where the hero's marker stands for `map` (the last location holding it wins).
void GetMarkerLocationForMap(uint32_t map, int& x, int& y) {
    for (Location* loc : g_locations) {
        if (!loc->info) continue;
        for (const Locations::MapInfo& m : loc->info->maps) {
            if (m.id != map) continue;
            loc->GetMarkerLocation(x, y);
            x -= g_marker->w / 2;
            y += 0x14 - g_marker->h;
        }
    }
}

void ShowSultanMovement() { g_sultanMovement = true; }                   // @0x2bdb80
void SetArrowVisibleWindowLimit(unsigned n) { g_arrowWindowLimit = (int)n; }   // @0x2bdb98
void PreventArrowHide() { g_preventArrowHide = true; }                   // @0x2bdd10

// @0x2bdbac: the map the location holding `map` would travel to.
uint32_t GetNearestMapToMap(uint32_t map) {
    for (Location* loc : g_locations)
        if (loc->info && loc->HasMap(map)) return loc->GetCurrentMap();
    return 0;
}

// @0x2bdc4c: a campaign's node (campaigns 1..999, 0x3ea) holding `map` is "clicked".
bool ShowCampaignWindowForMap(uint32_t map) {
    for (Location* loc : g_locations) {
        if (!loc->info) continue;
        const uint32_t c = loc->info->campaign;
        if (c == 0) continue;
        if ((c < 999 || c == 0x3ea || c == 999) && loc->HasMap(map)) {
            loc->OnClick();
            return true;
        }
    }
    return false;
}

// @0x2bdd28
bool IsVisible() { return !g_sepiaActive && g_locations[0]->window->visibleSelf; }

// @0x2bdd54: the location index of the first arrow (0 without one).
unsigned GetCurrentArrowTarget() { return g_arrows.empty() ? 0 : g_arrows[0].location; }

// @0x2bdd78. UNVERIFIED (milestone 5): ArenaEventWindow shows it once the map is up.
void EnqueueArenaEventWindow(unsigned event) {
    g_arenaEvent = event;
    g_arenaEventPending = true;
}

// @0x2be2f8: the first open campaign with tasks left is "clicked".
void ExecuteFirstHelp() {
    for (Location* loc : g_locations) {
        LocationInfo* i = loc->info;
        if (!i || i->startMap == 0xb || i->startMap == 0x74 || i->startMap == 0) continue;
        if (Locations::IsLocationLocked(i) || i->mission) continue;
        const unsigned count = (unsigned)Locations::GetLocationTaskCount(i, false);
        const unsigned done = (unsigned)Locations::GetLocationTaskCompleted(i, false);
        if (count != 0 && done < count) {
            loc->OnClick();
            return;
        }
    }
}

// @0x2be3c8: the captured screen fades out in 0.3 s (a blocking loop of frames).
void AnimateSepiaOut() {
    double last = Timer::GetTime();
    double t = 0.0;
    do {
        const double now = Timer::GetTime();
        const double dt = now - last;
        last = Timer::GetTime();
        t += dt;
        if (g_sepia) {
            const float x = (float)(t / (double)0.3f);
            Render::SetAlpha(g_sepia, sqrtf(1.f - x * x) + 0.f);
        }
        Render::Update();
    } while (t < (double)0.3f);
    Timer::AddDeltaTimeCompensation((double)0.3f);
    g_sepia = Render::RemoveSprite(g_sepia);
    if (g_sepiaTex) Render::RemoveTexture(g_sepiaTex);
    g_sepiaTex = nullptr;
    g_sepiaActive = false;
}

// @0x2be4f8: the screen is captured and fades in as sepia over it in 0.3 s.
void AnimateSepiaIn() {
    g_sepiaActive = true;
    Timer::GetTime();
    Render::Update();
    Render::Update();
    g_sepia = Render::RemoveSprite(g_sepia);
    if (g_sepiaTex) Render::RemoveTexture(g_sepiaTex);
    g_sepiaTex = Render::CreateScreenTexture();
    g_sepia = Render::CreateSprite(g_sepiaTex, 0xf, false, false);
    g_sepia->screenSpace = true;
    Render::SetShaderType(g_sepia, 9);
    Render::SetPosition(g_sepia, 0.f, (float)GUI::ScreenHeight(), 0.5f);
    double last = Timer::GetTime();
    Render::SortRenderLayer(0xe, 1);
    HUDWindow::HideBlack();
    int frames = 0;
    double t = 0.0;
    do {
        const double now = Timer::GetTime();
        ++frames;
        const double dt = now - last;
        last = Timer::GetTime();
        t += dt;
        const float x = (float)(t / (double)0.3f) - 1.f;
        Render::SetAlpha(g_sepia, sqrtf(1.f - x * x) + 0.f);
        Render::Update();
    } while (t < (double)0.3f);
    std::printf("GlobalMapWindow::AnimateSepiaIn() %d frames in %f seconds\n", frames, t);
    Timer::AddDeltaTimeCompensation((double)0.3f);
}

// @0x2be6b8: the tutorial's walk from the start node to the castle (2 s; ambush: the banner
// halfway). The first argument is not used.
void ShowTutorialAnimation(float, bool ambush, float t) {
    // (SoundsManager "ui_map_mark_move": milestone 6)
    g_t = t;
    g_ambush = ambush;
    g_traveling = true;
    g_dest = 0;
    g_speed = 2.f;
    g_keep = false;
    g_tutorialAnim = true;
    g_start.window->SetVisibility(true);
    Window* s = g_start.window;
    g_fromX = (s->x + s->w - 10 + g_world->x) - g_marker->w / 2;
    g_fromY = (s->y + s->h + 0x11 + g_world->y) - g_marker->h;
    GetMarkerLocationForMap(0, g_toX, g_toY);
}

// @0x2be7c0: a map (other than `current` when there is one) whose crafting nodes drop `item`;
// `current` when only it does, else 0.
uint32_t GetMapForCraftItem(uint32_t item, uint32_t current) {
    if (g_locations.empty()) return 0;
    for (Location* loc : g_locations) {
        loc->mark = false;
        if (!loc->info || loc->info->maps.empty() || loc->locked) continue;
        if (!Locations::IsLocationLocked(loc->info)) loc->mark = true;
    }
    bool found = false;
    for (Location* loc : g_locations) {
        if (!loc->mark || loc->info->maps.empty()) continue;
        for (const Locations::MapInfo& m : loc->info->maps) {
            const MetaData* contents = m.nodeContents;
            if (!contents) continue;
            for (unsigned i = 0; i < contents->GetChildrenCount(); ++i) {
                const uint32_t id = (uint32_t)contents->GetChild(i)->GetInt();
                const Items::ItemInfo* it = id == item ? Items::GetItemInfo(id) : nullptr;
                if (it && !it->IsLockedByReputation(true)) {
                    found = true;
                    if (m.id != current) return m.id;
                }
            }
        }
    }
    return found ? current : 0;
}

// @0x2be9ec: a mission map (other than `current`) whose loot for the stars earned (then any
// higher star level) holds `item`; `current` when only it does, else 0.
uint32_t GetMissionMapForItem(uint32_t item, uint32_t current) {
    if (g_locations.empty()) return 0;
    for (Location* loc : g_locations) {
        loc->mark = false;
        LocationInfo* i = loc->info;
        if (i && i->mission && !i->maps.empty() && i->items && i->items->GetChildrenCount() != 0 && !loc->locked &&
            !Locations::IsLocationLocked(i))
            loc->mark = true;
    }
    // A star level's loot: lists whose first entry is the item id.
    auto offers = [&](const MetaData* level) {
        for (unsigned k = 0; k < level->GetChildrenCount(); ++k) {
            const uint32_t id = (uint32_t)level->GetChild(k)->GetChild(0)->GetInt();
            const Items::ItemInfo* it = id == item ? Items::GetItemInfo(id) : nullptr;
            if (it && !it->IsLockedByReputation(true)) return true;
        }
        return false;
    };
    bool found = false;
    for (Location* loc : g_locations) {
        if (!loc->mark) continue;
        LocationInfo* i = loc->info;
        unsigned stars = (unsigned)Locations::GetMissionStarCount(i->maps[0].id, 0);
        if (i->items->GetChildrenCount() <= stars) stars = i->items->GetChildrenCount() - 1;
        const uint32_t map = i->maps[0].id;
        if (offers(i->items->GetChild(stars))) {
            found = true;
            if (map != current) return map;
        }
    }
    for (Location* loc : g_locations) {
        if (!loc->mark) continue;
        LocationInfo* i = loc->info;
        unsigned stars = (unsigned)Locations::GetMissionStarCount(i->maps[0].id, 0);
        if (stars >= 3) continue;
        for (; stars != 4 && stars < i->items->GetChildrenCount(); ++stars) {
            const uint32_t map = i->maps[0].id;
            if (offers(i->items->GetChild(stars))) {
                found = true;
                if (current != map) return map;
            }
        }
    }
    return found ? current : 0;
}

// @0x2bedd0: every task of the campaign holding `map` is completed (a mission's first block of
// end tasks too).
void FinishCampaign(uint32_t map) {
    if (map == 0) return;
    for (Location* loc : g_locations) {
        LocationInfo* i = loc->info;
        if (!i || !loc->HasMap(map)) continue;
        for (const Locations::MapInfo& m : i->maps) Tasks::CompleteMapTasks(m.id);
        if (i->mission && i->endTasks->GetChildrenCount() != 0)
            if (const MetaData* block = i->endTasks->GetChild(0))
                for (unsigned k = 0; k < block->GetChildrenCount(); ++k)
                    Tasks::CompleteTask((uint32_t)block->GetChild(k)->GetInt(), true, true, false);
        Tasks::Update(true);
        HUDWindow::UpdateTasks();
        return;
    }
}

// @0x2bef14: each location's campaign task, up to `max` (Tasks::Update's list, priority 5).
unsigned GetCampaignTasks(Tasks::TaskInfo** out, unsigned max) {
    unsigned n = 0;
    for (Location* loc : g_locations) {
        const uint32_t t = loc->GetCampaignTask();
        if (t == 0 || max <= n) continue;
        out[n++] = Tasks::GetTask(t);
    }
    return n;
}

// @0x2befa4: `task` completes its location: among the blocks of end tasks that hold it, the one with
// the most begun tasks; true when `task` finished every task of that block.
bool LocationHasBeenCompletedByTask(const MetaData* endTasks, uint32_t task) {
    if (!endTasks) return false;
    unsigned best = 0xffffffff, bestBegun = 0;
    for (unsigned b = 0; b < endTasks->GetChildrenCount(); ++b) {
        const MetaData* block = endTasks->GetChild(b);
        unsigned begun = 0;
        bool has = false;
        for (unsigned k = 0; k < block->GetChildrenCount(); ++k) {
            const uint32_t id = (uint32_t)block->GetChild(k)->GetInt();
            const bool completed = GameState::TaskCompleted(id);
            if (id == task) has = true;
            if (completed) break;
            if (GameState::GetTaskBeginTime(id) != 0) ++begun;
        }
        if (has && (bestBegun < begun || best == 0xffffffff)) {
            best = b;
            bestBegun = begun;
        }
    }
    if (best == 0xffffffff) return false;
    const MetaData* block = endTasks->GetChild(best);
    int completedCount = 0;
    bool has = false;
    for (unsigned k = 0; k < block->GetChildrenCount(); ++k) {
        const uint32_t id = (uint32_t)block->GetChild(k)->GetInt();
        const bool completed = GameState::TaskCompleted(id);
        if (id == task) has = true;
        if (!completed && GameState::GetTaskBeginTime(id) != 0) break;
        ++completedCount;
    }
    if (has && completedCount != 0) return (int)block->GetChildrenCount() == completedCount;
    return false;
}

// @0x2bf168 (Tasks::CompleteTask): a finished location shows its completion window; a campaign
// whose tasks are all done pays its 100% reward once.
void OnTaskCompleted(uint32_t task) {
    for (Location* loc : g_locations) {
        LocationInfo* i = loc->info;
        if (!i) continue;
        if (i->mission && i->endTasks) {
            if ((uint32_t)GameState::GetCurrentMapID() == i->startMap && LocationHasBeenCompletedByTask(i->endTasks, task)) {
                loc->ShowCompletionWindow();
                return;
            }
        }
        const uint32_t c = i->campaign;
        if (c == 0 || !(c == 0x3ea || c < 1000) || i->mission) continue;
        if (LocationHasBeenCompletedByTask(i->endTasks, task)) {
            loc->ShowCompletionWindow();
            return;
        }
        if (Locations::GetLocationTaskCount(i, false) == 0 || GameState::GetCampaignCompletionStatus(i->id)) continue;
        if (Locations::GetLocationTaskCompleted(i, false) != Locations::GetLocationTaskCount(i, false)) continue;
        GameState::SetCampaignCompletionStatus(i->id);
        Tasks::TaskInfo* t = Tasks::GetCampaignCompletionTask(i->id);
        if (t && !GameState::TaskCompleted(t->id) && !GameState::IsCombatActive() && GameState::GetCurrentMapID() != 0x70) {
            int crystals = Setting("campaign_complete_full_cb").GetInt();
            if (4 < crystals) crystals = 5;
            std::printf("Adding %d crystals for 100%% completion of campaign %d\n", crystals, (int)i->id);
            GameState::AddCrystals(crystals);
            // (SoundsManager "ui_100_percents": milestone 6)
            // UNVERIFIED (milestone 5): CampaignPerfectedWindow::SetParameters(i->id, t) and Show.
        }
    }
}

// @0x2bf438
void Deinit() {
    for (Location* loc : g_locations) loc->Deinit();
    delete g_world;
    g_world = nullptr;
    for (Render::Sprite*& s : g_pattern) s = Render::RemoveSprite(s);
    g_bg = Render::RemoveSprite(g_bg);
    delete g_marker;
    g_marker = nullptr;
    delete g_sultan;
    g_sultan = nullptr;
    delete g_return.root;
    g_return.root = nullptr;
    GUI::RemoveMovementEffect(g_returnEffect);
    g_returnEffect = nullptr;
    delete g_navigate.root;
    g_navigate.root = nullptr;
    GUI::RemoveMovementEffect(g_navigateEffect);
    g_navigateEffect = nullptr;
    g_locations.clear();
}

// @0x2c0eb0
void HideArrow() {
    for (Arrow& a : g_arrows) a.sprite = Render::RemoveSprite(a.sprite);
    g_arrows.clear();
}

// @0x2c0f14: the arrow's location is "clicked" (true); without an arrow, whether the hero travels.
bool ExecuteArrowCallback() {
    if (!g_arrows.empty()) {
        const unsigned loc = g_arrows[0].location;
        HideArrow();
        g_locations[loc]->OnClick();
        return true;
    }
    return g_traveling;
}

// @0x2c0f70: back to the map the hero is on (not while a started mission's result waits).
void Hide() {
    std::printf("saint -> hiding");
    const uint32_t cur = (uint32_t)GameState::GetCurrentMapID();
    if (LocationInfo* l = Locations::GetLocationWithMap(cur))
        if (l->mission && GameState::GetMapStartTime(l->startMap) == 0 && GameState::GetMapEndTime(l->startMap) != 0) return;
    g_velY = 0.f;
    g_velX = 0.f;
    if (!g_locations[0]->window->visibleSelf) return;
    // (SoundsManager: back in the city "music_city" plays unless it is playing: milestone 6)
    if (g_world->sprite) Render::SetVisibility(g_world->sprite, false);
    for (Location* loc : g_locations) loc->Hide();
    for (Render::Sprite* s : g_pattern) Render::SetVisibility(s, false);
    Render::SetVisibility(g_bg, false);
    g_marker->SetVisibility(false);
    g_sultan->SetVisibility(false);
    Render::minLayer = 0;
    Render::clearColor = true;
    WindowManager::WindowHide(false);
    g_queue->shown = false;
    g_queue->MoveWindowDown(true);
    HUDWindow::SetBottomType(0);
    if (GameState::GetCurrentMapID() == 0 || GameState::GetCurrentMapID() == 0xb) BottomBattleWindow::Hide();
    else BottomBattleWindow::Show();
    GameState::SetCurrentLocation(GameState::GetCurrentMapID() == 0 ? 0 : 2);
    if (GameState::GetCurrentLocation() == 0) {
        if (!BottomCityWindow::IsVisible()) BottomCityWindow::Show();
    } else {
        BottomCityWindow::Hide();
    }
    // UNVERIFIED (milestone 5): PlayerWeakWindow's advice. On map 0x6d (once the shop task,
    // Setting "shop_unlock_task", is done) when the hero is hurt with fewer than 3 belt potions
    // below level 5; on another map of a location on the world map when no boss fight, no soldier
    // in the slots, the camera still, not maps 0, 0x74, 0xd, 0xb, and secondTutorial 0x100; each
    // map once (PlayerWeakWindow::HasBeenShownOnMap).
    HideArrow();
    HUDWindow::UpdateFrameBorder();
    if (const char32_t* name = Locations::GetMapName((uint32_t)GameState::GetCurrentMapID())) HUDWindow::ShowMapName(name);
    GameState::ResetGamePauseState();
    const int W = GUI::ScreenWidth(), H = GUI::ScreenHeight();
    const int rx = W / 2 - g_return.root->w;
    g_returnEffect->Animate(rx, H - g_return.root->h, rx, H, false);
    g_navigateEffect->Animate(W / 2, H - g_navigate.root->h, W / 2, H, false);
    g_ambush = false;
    g_tutorialAnim = false;
}

// @0x2c1440 the "Go home" button. UNVERIFIED (milestone 5): on a started mission's map whose result
// waits, RaidInfoWindow (SetParameters(location, its start map, false), Show) instead.
void OnReturn() {
    std::printf("saint -> onreturn");
    if (LocationInfo* l = Locations::GetLocationWithMap((uint32_t)GameState::GetCurrentMapID()))
        if (l->mission && GameState::GetMapStartTime(l->startMap) == 0 && GameState::GetMapEndTime(l->startMap) != 0) return;
    Hide();
    TaskHolderWindow::UpdateTasks();
}

// @0x2c14b4: load `map` (before the tutorial's end only map 0xe, or 0 / 0xf in tutorial type 1).
// keep: no save of the old map and a reload of the same map; animated: the sepia over the switch.
void SwitchMap(uint32_t map, bool keep, bool animated) {
    std::printf("saint -> switching map to %d\n", (int)map);
    int t = GameState::tutorial;
    if (t < 0x80 && map != 0xe) {
        if (GameState::GetTutorialType() != 1) return;
        if (map != 0xf && map != 0) return;
        t = GameState::tutorial;
    }
    if (t == 0x67)
        if (Entity* e = EntityManager::GetEntityByClass(6, false, true)) EntityManager::RemoveEntity(e, true);
    if (map == (uint32_t)GameState::GetCurrentMapID()) animated = false;
    else if (animated && !Render::IsLowPerformanceDevice()) AnimateSepiaIn();
    BuildingHovers::CollectAll();
    if (!keep) Map::Save(0);
    if ((uint32_t)GameState::GetCurrentMapID() != map || keep) {
        if (BaseCombat* c = GameState::GetActiveCombat()) c->EndCombat();
        const bool cached = SaveManager::GetMainSave()->HasMapInCache(map);
        SaveManager::SaveBlock* block = SaveManager::GetMapData(map);
        if (!block) {
            std::printf("ERROR: map %d cannot be found and cannot be loaded from server\n", (int)map);
        } else {
            GameState::SetCurrentMapID(map);
            Map::Load(block, (uint32_t)Rand48::lrand48(), false, cached);
            Map::UpdateOfflineResources();
            Tasks::CompleteSubtask(Tasks::kEnterMap, map, 1);
            Tasks::Update(true);
        }
    }
    if (!animated) {
        Hide();
    } else {
        HUDWindow::SetBottomType(0);
        if (GameState::GetCurrentMapID() == 0 || GameState::GetCurrentMapID() == 0xb) BottomBattleWindow::Hide();
        else BottomBattleWindow::Show();
        if (!Render::IsLowPerformanceDevice()) AnimateSepiaOut();
    }
    // UNVERIFIED (milestone 5): back in the city CastleTopWindow::ActivateFeatureHolders.
}

// @0x2c1734: the marker travels to `map`'s location at `pixelsPerSecond` (at most 1.5 s), then
// SwitchMap(map, keep) (ambush: the banner halfway and no switch).
void SwitchMapAnimated(uint32_t map, bool keep, float pixelsPerSecond, bool ambush) {
    std::printf("saint: switch map to %d", (int)map);
    if (!(0x7f < GameState::tutorial || map == 0xe || (GameState::GetTutorialType() == 1 && (map == 0xf || map == 0))))
        return;
    HideArrow();
    if (map == (uint32_t)GameState::GetCurrentMapID()) {
        Hide();
        TaskHolderWindow::UpdateTasks();
        return;
    }
    // (SoundsManager "ui_map_mark_move": milestone 6)
    g_keep = keep;
    g_traveling = true;
    g_ambush = ambush;
    g_t = 0.f;
    g_dest = map;
    g_tutorialAnim = false;
    GetMarkerLocationForMap((uint32_t)GameState::GetCurrentMapID(), g_fromX, g_fromY);
    GetMarkerLocationForMap(map, g_toX, g_toY);
    const int dx = g_fromX - g_toX, dy = g_fromY - g_toY;
    const float d = sqrtf((float)(dy * dy + dx * dx));
    g_speed = d / pixelsPerSecond;
    if (1.5f < d / pixelsPerSecond) g_speed = 1.5f;
}

// @0x2c1e08: scroll by (dx, dy) within the parchment (centred when it is narrower than the
// screen); the sprites, the map, the marker (moveMarker), the sultan and the arrows follow.
void HandleMove(int dx, int dy, bool moveMarker) {
    const int margin = g_margin;
    const int oldX = g_scrollX, oldY = g_scrollY;
    const int half = margin / 2;
    int x = dx + oldX, y = dy + oldY;
    g_scrollX = x;
    if (half < x) g_scrollX = x = half;
    g_scrollY = y;
    if (half < y) g_scrollY = y = half;
    const float bgW = g_bg->w, W = (float)GUI::ScreenWidth(), fHalf = (float)half;
    const float limX = (bgW - W) + fHalf;
    const float bgH = g_bg->h;
    if ((float)x < -limX) g_scrollX = -(int)limX;
    float fy = (float)y;
    const float H = (float)GUI::ScreenHeight();
    const float limY = fHalf + (bgH - H);
    if (fy < -limY) {
        const int v = -(int)limY;
        fy = (float)v;
        g_scrollY = v;
    }
    const float fMargin = (float)margin;
    int sx = g_scrollX;
    if (W > bgW + fMargin) g_scrollX = sx = (int)(W - bgW) / 2;
    Render::Sprite* p0 = g_pattern[0];
    Render::SetPosition(p0, (float)sx - g_pattern[2]->w, (fy + p0->h) - fMargin, p0->z);
    Render::Sprite* p1 = g_pattern[1];
    Render::SetPosition(p1, (float)g_scrollX - g_pattern[2]->w, (float)g_scrollY + p1->h + g_bg->h, p1->z);
    Render::Sprite* p2 = g_pattern[2];
    Render::SetPosition(p2, (float)g_scrollX - p2->w, (float)g_scrollY + p2->h, p2->z);
    Render::Sprite* p3 = g_pattern[3];
    Render::SetPosition(p3, (float)g_scrollX + g_bg->w, (float)g_scrollY + p3->h, p3->z);
    Render::SetPosition(g_bg, (float)g_scrollX, (float)g_scrollY + g_bg->h, g_bg->z);
    g_world->SetPosition(g_worldX + g_scrollX, g_worldY + g_scrollY);
    if (moveMarker && !g_tutorialAnim && !g_ambush) {
        int mx = 0, my = 0;
        GetMarkerLocationForMap((uint32_t)GameState::GetCurrentMapID(), mx, my);
        if (mx != 0 || my != 0) g_marker->SetPosition(mx, my);
    }
    if (!g_sultanMovement) PlaceSultan();
    const float ddx = (float)(oldX - g_scrollX), ddy = (float)(oldY - g_scrollY);
    for (Arrow& a : g_arrows) {
        a.x -= ddx;
        a.y -= ddy;
    }
}

// @0x2c227c: a drag scrolls once it went 10 pixels (only with no dialog over the map).
void Move(int x, int y) {
    if (g_traveling) return;
    if (1 < WindowManager::GetShownWindowCount()) {
        g_dragging = false;
        return;
    }
    if (!g_dragging) return;
    const int dx = x - g_dragX, dy = y - g_dragY;
    if (!g_dragMoved && std::abs(dy) + std::abs(dx) < 10) return;
    g_dragX = x;
    g_dragY = y;
    g_dragMoved = true;
    if (!g_traveling) HandleMove(dx, dy, true);
}

// @0x2c2324: the map scrolls by the arrows' average less the screen's centre (y negated).
void CenterOnArrows() {
    if (g_arrows.empty()) return;
    float x = g_arrows[0].x, y = g_arrows[0].y;
    unsigned n = 1;
    if (1 < g_arrows.size())
        for (; n != g_arrows.size(); ++n) {
            y += g_arrows[n].y;
            x += g_arrows[n].x;
        }
    const float fn = (float)n;
    HandleMove((int)(x / fn - (float)(GUI::ScreenWidth() / 2)), -(int)(y / fn - (float)(GUI::ScreenHeight() / 2)), true);
}

// @0x2c23f4: an arrow takes the tap first (its location is clicked on release); a press starts a
// drag, a release after a drag flings the map at the mouse's speed (within +-2000 pixels/s).
// While travelling every tap is swallowed.
bool Click(int x, int y, bool pressed) {
    if (g_traveling) return true;
    if (!g_locations[0]->window->visibleSelf) return false;
    const float fx = (float)x, fy = (float)y;
    for (const Arrow& a : g_arrows) {
        Render::Sprite* s = a.sprite;
        if (fx > s->x && fx < s->x + s->w && fy > s->y - s->h && fy < s->y) {
            if (pressed) return true;
            const unsigned loc = a.location;
            HideArrow();
            g_locations[loc]->OnClick();
            return true;
        }
    }
    if (pressed) {
        g_velY = 0.f;
        g_dragX = x;
        g_dragging = true;
        g_dragY = y;
        g_velX = 0.f;
    } else {
        g_dragging = false;
        if (g_dragMoved) {
            g_dragMoved = false;
            float vx = GUI::GetMouseSpeed(true);
            const float vy = GUI::GetMouseSpeed(false);
            if (vx < -2000.f) vx = -2000.f;
            else if (vx > 2000.f) vx = 2000.f;
            g_velX = vx;
            g_velY = vy < -2000.f ? -2000.f : (vy > 2000.f ? 2000.f : vy);
            return true;
        }
    }
    for (CoreLocation& s : g_side)
        if (s.workHere->Click(x, y, pressed, false)) return true;
    if (g_sultan->Click(x, y, pressed, false)) return true;
    if (g_return.root->Click(x, y, pressed, false)) return true;
    if (g_navigate.root->Click(x, y, pressed, false)) return true;
    g_world->Click(x, y, pressed, false);
    return true;
}

// @0x2c2728: the arrows bob, the locations update, the marker travels (and the ambush banner shows
// halfway), the map glides on after a fling and the sultan's caravan moves day by day.
void Update(float dt) {
    if (unlockAll != g_lastUnlockAll) Show();
    g_lastUnlockAll = unlockAll;
    if (WindowManager::GetShownWindowCount() != 0 && (unsigned)WindowManager::GetShownWindowCount() < (unsigned)g_arrowWindowLimit)
        g_arrowWindowLimit = WindowManager::GetShownWindowCount();
    if (!g_arrows.empty() && (unsigned)g_arrowWindowLimit < (unsigned)WindowManager::GetShownWindowCount()) HideArrow();
    for (Arrow& a : g_arrows) {
        a.sprite->x = a.x;
        a.sprite->y = (float)((double)a.y + cos(Timer::GetTime() * 15.0) * 8.0);
        a.sprite->z = HUDWindow::GetZ();
    }
    for (Location* loc : g_locations) loc->Update();
    g_home.name->SetText(GameState::GetCastleName());
    if (g_traveling) {
        const float f = g_t / g_speed;
        bool arrived = false;
        if (f <= 0.5f || !g_ambush) {
            if (!(f < 1.f)) {
                arrived = true;
            } else {
                g_marker->SetPosition((int)((float)g_fromX + f * (float)(g_toX - g_fromX)),
                                      (int)((float)g_fromY + f * (float)(g_toY - g_fromY)));
                if (GUI::ScreenWidth() < 0x800) {
                    g_scrollX = (int)(((float)g_scrollX - (float)(g_marker->x + g_marker->w / 2)) + (float)(GUI::ScreenWidth() / 2));
                    g_scrollY = (int)(((float)g_scrollY - (float)g_marker->y) + (float)(GUI::ScreenHeight() / 2));
                }
                HandleMove(0, 0, false);
                if (!g_tutorialAnim) {
                    GetMarkerLocationForMap((uint32_t)GameState::GetCurrentMapID(), g_fromX, g_fromY);
                    GetMarkerLocationForMap(g_dest == 0x70 ? 0x74 : g_dest, g_toX, g_toY);
                } else {
                    Window* s = g_start.window;
                    g_fromX = (s->x + g_world->x - 10 + s->w) - g_marker->w / 2;
                    g_fromY = (s->y + g_world->y + 0x11 + s->h) - g_marker->h;
                    GetMarkerLocationForMap(0, g_toX, g_toY);
                }
            }
        } else {
            g_traveling = false;
            BossIntroWindow::SetAmbush();
            BossIntroWindow::SetBossName(StringTable::GetString("TUT_AMBUSH"));
            BossIntroWindow::Show();
            // (SoundsManager "battle_boss_intro": milestone 6)
            arrived = true;
        }
        if (arrived) {
            if (!g_ambush) g_marker->SetPosition(g_toX, g_toY);
            g_start.window->SetVisibility(false);
            g_traveling = false;
            if (g_dest == 0x70) {
                // UNVERIFIED (milestone 5): the PvP fight against PlayerCompetitionWindow's chosen
                // player (a hero entity 0xc / 0xd, an AggroCombat switched to the PvP arena
                // (ForceBossFight(6, 0x70))), then Map::Update(0).
            } else if (!g_ambush) {
                SwitchMap(g_dest, g_keep, false);
            }
        }
        g_t += dt;
    }
    if (GameState::IsPaused()) {
        if (!g_queue->shown) return;
        if (!HealSquadWindow::IsVisible()) GameState::UpdateHPRegen(dt);
    }
    if (!g_queue->shown) return;
    float vx = g_velX;
    const float vy0 = g_velY;
    const float len = sqrtf(vy0 * vy0 + vx * vx);
    if (vx != 0.f) {
        const float d = dt * g_decel * std::fabs(vx / len);
        if (vx < 0.f) {
            vx += d;
            if (vx > 0.f) vx = 0.f;
        } else {
            vx -= d;
            if (vx < 0.f) vx = 0.f;
        }
    }
    g_velX = vx;
    float vy = g_velY;
    if (vy != 0.f) {
        const float d = dt * g_decel * std::fabs(vy0 / len);
        if (vy < 0.f) {
            vy += d;
            if (vy > 0.f) vy = 0.f;
        } else {
            vy -= d;
            if (vy < 0.f) vy = 0.f;
        }
    }
    g_velY = vy;
    if (vx != 0.f || vy != 0.f) {
        g_accX += dt * vx;
        g_accY += dt * vy;
        const int ix = (int)g_accX, iy = (int)g_accY;
        if (ix != 0 || iy != 0) {
            HandleMove(ix, iy, true);
            g_accX -= (float)(int)g_accX;
            g_accY -= (float)(int)g_accY;
        }
    }
    if (!g_queue->shown) return;
    if (Timer::GetGlobalTime() == g_lastSecond && !g_sultanMovement) return;
    g_lastSecond = Timer::GetGlobalTime();
    // The sultan's caravan (task 0x5c9): seven days on the road from event 0x5dd's start, after
    // which the task completes.
    if (GameState::GetTaskBeginTime(0x5c9) != 0 && !GameState::TaskCompleted(0x5c9)) {
        if (GameState::GetEventStartTime(0x5dd) == 0) {
            GameState::SetEventStartTime(0x5dd, (uint32_t)Timer::GetGlobalTime());
        } else {
            const int elapsed = Timer::GetGlobalTime() - (int)GameState::GetEventStartTime(0x5dd);
            if (elapsed < Setting("sultan_move_time").GetInt()) {
                const int moveTime = Setting("sultan_move_time").GetInt();
                if (!g_sultanMovement) {
                    g_sultanDay = moveTime / -86400 + 7 + (int)((unsigned)elapsed / 86400u);
                    g_sultanSecs = (unsigned)elapsed % 86400u;
                }
            } else {
                Tasks::CompleteTask(0x5c9, true, false, false);
            }
        }
    }
    g_sultan->SetVisibility((GameState::GetTaskBeginTime(0x5c9) != 0 && !GameState::TaskCompleted(0x5c9)) || g_sultanMovement);
    if (!g_sultan->visibleSelf) return;
    const std::u32string days = StringTable::GetCountableString(StringTable::GetString("COUNT_DAYS"), 7 - g_sultanDay);
    g_sultanTimer->SetText(SWPrintf(0x20, U"%d %s", {7 - g_sultanDay, days.c_str()}).c_str());
    if (!g_sultanMovement) {
        PlaceSultan();
        return;
    }
    // ShowSultanMovement: a day of travel per g_speed seconds along a Catmull-Rom curve through
    // the path's markers; on the seventh day it stops (going to map 0xb: the hero's next spawn
    // is tile (3, 0x1a)).
    const int moveTime = Setting("sultan_move_time").GetInt();
    g_sultanSecs += (uint32_t)(int)(((float)moveTime / g_speed) * dt);
    if (0x15180 < g_sultanSecs) {
        g_sultanSecs -= 0x15180;
        if (++g_sultanDay == 7) {
            g_sultanDay = 6;
            if (g_dest == 0xb) Map::PlayerNextSpawn(3, 0x1a);
            g_sultanMovement = false;
        }
    }
    const float t = (float)g_sultanSecs / 86400.f;
    const unsigned day = (unsigned)g_sultanDay;
    const int wx = g_world->x, wy = g_world->y;
    Window* w0 = day != 0 ? g_sultanPath[day - 1] : g_sultanPath[0];
    Window* w1 = g_sultanPath[day];
    int x2, y2, x3, y3;
    if (day < 6) {
        Window* w2 = g_sultanPath[day + 1];
        x2 = wx + w2->x;
        y2 = w2->y + wy;
        Window* w3 = day == 5 ? g_sultanPath[6] : g_sultanPath[day + 2];
        x3 = wx + w3->x;
        y3 = w3->y + wy;
    } else {
        x3 = x2 = wx + g_sultanPath[6]->x;
        y3 = y2 = g_sultanPath[6]->y + wy;
    }
    const float x0 = (float)(wx + w0->x), y0 = (float)(wy + w0->y);
    const float x1 = (float)(wx + w1->x), y1 = (float)(w1->y + wy);
    const float t2 = t * t, t3 = t2 * t;
    const float cx = (x1 + x1 + (float)(x2 - (wx + w0->x)) * t + ((x0 + x0 + x1 * -5.f + (float)x2 * 4.f) - (float)x3) * t2 +
                      ((float)x3 + -x0 + x1 * 3.f + (float)x2 * -3.f) * t3) * 0.5f;
    const float cy = (y1 + y1 + (float)(y2 - (wy + w0->y)) * t + ((y0 + y0 + y1 * -5.f + (float)y2 * 4.f) - (float)y3) * t2 +
                      ((float)y3 + -y0 + y1 * 3.f + (float)y2 * -3.f) * t3) * 0.5f;
    g_sultan->SetPosition((int)cx - g_sultan->w / 2, (int)cy - g_sultan->h);
}

// @0x2c37b8: everything at depth z (the parchment just behind it) and the scroll limits applied.
void SetZ(float z) {
    const bool buttons = g_queue->shown && GameState::tutorial != 0x17;
    g_return.root->SetVisibility(buttons);
    g_navigate.root->SetVisibility(buttons);
    if (!g_queue->shown) return;
    const int margin = g_margin;
    const float W = (float)GUI::ScreenWidth(), H = (float)GUI::ScreenHeight();
    const int half = margin / 2;
    int x = g_scrollX;
    if (half < x) g_scrollX = x = half;
    int y = g_scrollY;
    if (half < y) g_scrollY = y = half;
    const float bgW = g_bg->w, fHalf = (float)half;
    const float limX = (bgW - W) + fHalf;
    if ((float)x < -limX) g_scrollX = -(int)limX;
    float fy = (float)y;
    const float limY = fHalf + (g_bg->h - H);
    if (fy < -limY) {
        const int v = -(int)limY;
        fy = (float)v;
        g_scrollY = v;
    }
    const float fMargin = (float)margin;
    int sx = g_scrollX;
    if (W > bgW + fMargin) g_scrollX = sx = (int)(W - bgW) / 2;
    const float z0 = z - 0.0001f, z1 = z0 - 0.0001f, z2 = z1 - 0.0001f, z3 = z2 - 0.0001f, zBg = z3 - 0.001f;
    Render::Sprite* p0 = g_pattern[0];
    Render::SetPosition(p0, (float)sx - g_pattern[2]->w, (fy + p0->h) - fMargin, z0);
    Render::Sprite* p1 = g_pattern[1];
    Render::SetPosition(p1, (float)g_scrollX - g_pattern[2]->w, (float)g_scrollY + p1->h + g_bg->h, z1);
    Render::Sprite* p2 = g_pattern[2];
    Render::SetPosition(p2, (float)g_scrollX - p2->w, (float)g_scrollY + p2->h, z2);
    Render::Sprite* p3 = g_pattern[3];
    Render::SetPosition(p3, (float)g_scrollX + g_bg->w, (float)g_scrollY + p3->h, z3);
    Render::SetPosition(g_bg, (float)g_scrollX, (float)g_scrollY + g_bg->h, zBg);
    g_world->SetZ(zBg);
    g_world->SetPosition(g_worldX + g_scrollX, g_worldY + g_scrollY);
    if (!g_tutorialAnim && !g_ambush) {
        int mx = 0, my = 0;
        GetMarkerLocationForMap((uint32_t)GameState::GetCurrentMapID(), mx, my);
        g_marker->SetZ(zBg - 0.051f);
        if (mx != 0 || my != 0) g_marker->SetPosition(mx, my);
    }
    g_markerMale->SetVisibility(GameState::IsMalePlayer());
    g_markerFemale->SetVisibility(!GameState::IsMalePlayer());
    g_sultan->SetZ(zBg - 0.051f);
    if (!g_sultanMovement) PlaceSultan();
    const int sw = GUI::ScreenWidth(), sh = GUI::ScreenHeight();
    g_return.root->SetPosition(sw / 2 - g_return.root->w, sh - g_return.root->h);
    const float zButtons = zBg - 0.052f;
    g_return.root->SetZ(zButtons);
    g_navigate.root->SetPosition(sw / 2, sh - g_navigate.root->h);
    g_navigate.root->SetZ(zButtons);
    Render::SortRenderLayer(0xe, 1);
    Update(0.f);
}

// @0x2c3dc8: on small screens (narrower than 0x800) the sultan's marker is centred.
void CenterOnSultan() {
    if (g_sepiaActive) return;
    if (GUI::ScreenWidth() < 0x800) {
        g_scrollX = (int)(((float)g_scrollX - (float)(g_sultan->x + g_sultan->w / 2)) + (float)(GUI::ScreenWidth() / 2));
        g_scrollY = (int)(((float)g_scrollY - (float)g_sultan->y) + (float)(GUI::ScreenHeight() / 2));
    }
    SetZ(g_world->z);
}

// @0x2c3ed0: on small screens the location holding `map` is centred.
void CenterOnMap(uint32_t map) {
    if (g_sepiaActive) return;
    for (Location* loc : g_locations) {
        if (!loc->HasMap(map)) continue;
        Window* w = loc->window;
        if (GUI::ScreenWidth() < 0x800) {
            const float wy = (float)(w->y + g_world->y);
            const float wx = (float)(w->x + g_world->x + w->w / 2);
            g_scrollX = (int)(((float)g_scrollX - wx) + (float)(GUI::ScreenWidth() / 2));
            g_scrollY = (int)(((float)g_scrollY - wy) + (float)(GUI::ScreenHeight() / 2));
        }
        SetZ(g_world->z);
        return;
    }
}

// @0x2c4038: the marker travels to the arena (map 0x74) for a PvP fight (map 0x70).
void ShowPvPAnimation() {
    // (SoundsManager "ui_map_mark_move": milestone 6)
    g_ambush = false;
    g_keep = false;
    g_tutorialAnim = false;
    g_traveling = true;
    g_dest = 0x70;
    g_t = 0.f;
    g_speed = 2.f;
    CenterOnMap(0x74);
    GetMarkerLocationForMap((uint32_t)GameState::GetCurrentMapID(), g_fromX, g_fromY);
    GetMarkerLocationForMap(0x74, g_toX, g_toY);
    const int dx = g_fromX - g_toX, dy = g_fromY - g_toY;
    g_speed = sqrtf((float)(dy * dy + dx * dx)) / 500.f;
    if (1.5f < g_speed) g_speed = 1.5f;
}

// @0x2c414c: the bobbing helper arrow over the location holding `map` (centred first: center).
void ArrowAtMap(uint32_t map, bool keep, bool center) {
    if (center) CenterOnMap(map);
    if (!keep) HideArrow();
    for (unsigned i = 0; i < g_locations.size(); ++i) {   // (the bound reads 0x620600, UNVERIFIED: the count)
        if (!g_locations[i]->HasMap(map)) continue;
        g_arrows.push_back(Arrow{});
        Arrow& a = g_arrows.back();
        a.sprite = Render::CreateSprite(IconManager::GetIcon("Arrow_show", false), 0xe, false, false);
        a.sprite->w = (float)a.sprite->tex->w * g_windowScale;
        a.sprite->h = g_windowScale * (float)a.sprite->tex->h;
        Window* w2 = g_locations[i]->window2;
        a.x = (float)(g_world->x + w2->x + w2->w / 2) + a.sprite->w * -0.5f;
        a.y = (float)(g_world->y + w2->y);
        Render::SetPosition(a.sprite, a.x, a.y, HUDWindow::GetZ());
        a.sprite->screenSpace = true;
        Render::SetVisibility(a.sprite, true);
        a.location = i;
        return;
    }
}

// @0x2c448c: the world map opens over the current map (layers below 0xe are not drawn). At step
// 0x67 of the tutorial the arrow points at map 0xe's node and only it takes taps.
void Show() {
    if (GameState::GetCurrentMapID() == 0 && Map::GetCurrentFarm()) HUDWindow::ExitFarm();
    // (SoundsManager: "music_battle_map" unless playing, "ui_map_opened": milestone 6)
    const bool wasShown = g_queue->shown;
    g_queue->shown = true;
    g_queue->MoveWindowOnTop(true);
    if (!g_locations[0]->window->visibleSelf) WindowManager::WindowShow(false);
    for (Location* loc : g_locations) loc->Show();
    g_desert.comp->SetEnabled(!Locations::IsLocationLocked(g_desert.info) || unlockAll);
    g_desert.nameHolder->SetEnabled(g_desert.comp->enabled);
    g_arena.comp->SetEnabled(!Locations::IsLocationLocked(g_arena.info) || unlockAll);
    g_arena.nameHolder->SetEnabled(g_arena.comp->enabled);
    for (Render::Sprite* s : g_pattern) Render::SetVisibility(s, true);
    Render::SetVisibility(g_bg, true);
    g_marker->SetVisibility(true);
    g_start.window->SetVisibility(!GameState::TaskCompleted(0x771));
    Render::minLayer = 0xe;
    Render::clearColor = false;
    CastleTopWindow::HideTools();
    if (!g_preventArrowHide) {
        HUDWindow::Show();
        HUDWindow::SetBottomType(4);
    }
    BottomBattleWindow::Hide();
    GameState::SetCurrentLocation(3);
    HUDWindow::UpdateFrameBorder();
    if (!g_preventArrowHide) BuildingHovers::HideArrow();
    g_preventArrowHide = false;
    if (GameState::tutorial == 0x67) {
        ArrowAtMap(0xe, false, true);
        const unsigned loc = g_arrows.empty() ? 0 : g_arrows[0].location;
        // (+0x30 of map 0xe's location: a core node's click area)
        GUI::SetInteractionObjectLock(static_cast<CoreLocation*>(g_locations[loc])->clickArea, nullptr);
    }
    if (!wasShown) GameState::RaiseGamePauseState();
    g_sultanMovement = false;
    if (GameState::tutorial != 0x17) {
        g_return.root->SetVisibility(true);
        const int W = GUI::ScreenWidth(), H = GUI::ScreenHeight();
        const int rx = W / 2 - g_return.root->w;
        g_returnEffect->Animate(rx, H, rx, H - g_return.root->h, true);
        g_navigate.root->SetVisibility(true);
        g_navigateEffect->Animate(W / 2, H, W / 2, H - g_return.root->h, true);
    }
    g_return.text->SetText(StringTable::GetString("BATTLE_GO_HOME"));
    g_navigate.text->SetText(StringTable::GetString("CONTROL_BATTLE_ATTACK"));
    g_lastBattleMap = Locations::GetLastVisitedBattleMap();
}

// @0x2c4944: the campaign holding `map` starts over (from the city, with the map open): its maps'
// saves, tasks, boss fights and timers are reset.
void ResetCampaign(uint32_t map) {
    if (map == 0) return;
    SwitchMap(0, false, false);
    Show();
    for (Location* loc : g_locations) {
        if (!loc->info || !loc->HasMap(map)) continue;
        for (const Locations::MapInfo& m : loc->info->maps) {
            SaveManager::GetMainSave()->ResetMap(m.id);
            Tasks::ResetMap(m.id);
            GameState::ResetBossFightsOnMap(m.id);
            GameState::SetMapStartTime(m.id, 0);
            GameState::SetMapEndTime(m.id, 0);
        }
        Tasks::Update(true);
        HUDWindow::UpdateTasks();
        return;
    }
}

// @0x2c4f00
void Init() {
    g_windowScale = GUI::IsHighDPIVersion() ? GUI::GetHighDPIScaleFactor() : 1.f;
    int worldX;
    if (!GUI::IsHighDPIVersion() || 599 < GUI::ScreenHeight()) {
        worldX = (int)(g_windowScale * 95.f);
    } else {
        worldX = 0x47;
        g_windowScale = 0.75f;
    }
    const float patternScale[4][2] = {{14.f, 1.f}, {14.f, 1.f}, {4.f, 4.f}, {4.f, 4.f}};   // 0x59779c
    g_worldX = worldX;
    g_worldY = (int)(g_windowScale * 150.f);
    g_margin = (int)(g_windowScale * 256.f);
    for (int i = 0; i < 4; ++i) {
        Render::Sprite* s = Render::CreateSprite(Resources::GetImage("images/WorldMap/pattern_paper_bg"), 0xe, false, false);
        g_pattern[i] = s;
        s->screenSpace = true;
        const float sx = patternScale[i][0], sy = patternScale[i][1];
        s->w = s->w * sx * g_windowScale;
        s->h = s->h * sy * g_windowScale;
        s->u1 *= sx;
        s->vBottom *= sy;
        Render::SetWrapping(s->tex, true);
        Render::SetVisibility(s, false);
    }
    // (Texture::ForceLoad and the textures' unload flag: the port's textures load at once)
    g_bg = Render::CreateSprite(Resources::GetImage("images/WorldMap/world_map_bg"), 0xe, false, false);
    g_bg->screenSpace = true;
    if (g_bg->tex) {
        g_bg->w *= g_windowScale;
        g_bg->h *= g_windowScale;
    }
    Render::SetVisibility(g_bg, false);
    g_world = GUI::RegisterUI("../resource/kingdom_ui/1Original/World_map_holder_01.xml", "World_map_holder_01.png",
                              g_windowScale, 0, 0, 0, 0, false, 1.f);
    g_queue->fn.zRange = WindowManager::ReturnLargeZRange;
    g_queue->fn.back = OnBack;
    g_locations.clear();
    Location* castles[6] = {&g_home, &g_desert, &g_ice, &g_haunted, &g_iceCastle, &g_arena};
    for (Location* c : castles) g_locations.push_back(c);
    for (unsigned i = 0; i < 6; ++i) g_locations[i]->Init(GUI::GetWindowTyped<Window>(g_world, kWindowConfigs[i]), i, false);
    for (int i = 0; i < 16; ++i) {
        g_core[i].Init(GUI::GetWindowTypedF<Window>(g_world, "button_main_node_%02d", i + 1), 0, false);
        g_locations.push_back(&g_core[i]);
    }
    for (int i = 0; i < 24; ++i) {
        g_side[i].Init(GUI::GetWindowTypedF<Window>(g_world, "button_side_node_%02d", i + 1), 0, true);
        g_locations.push_back(&g_side[i]);
    }
    g_start.Init(GUI::GetWindowTyped<Window>(g_world, "button_side_node_start"), 0, true);
    g_start.starSilver->SetVisibility(false);
    g_start.starGold->SetVisibility(false);
    g_start.statusTimeout->SetVisibility(false);
    g_start.starDisabled->SetVisibility(false);
    g_start.starInactive->SetVisibility(false);
    g_start.locked16->SetVisibility(false);
    g_locations.push_back(&g_start);
    g_queue->fn.update = Update;
    g_queue->fn.show = Show;
    // Each location of locations.xml goes to its node: the castles by campaign (1000 haunted,
    // 0x3e9 ice castle, 0x3ea desert, 0x3eb ice, 0x3ec arena), campaigns 1..999 to the main or
    // (missions) side nodes, the city (0x400) home, 4000 the tutorial's start node. A mission's
    // clear tasks not started yet are hidden from the lists.
    for (unsigned i = 0;; ++i) {
        LocationInfo* l = Locations::EnumLocationInfo(i);
        if (!l) break;
        const uint32_t c = l->campaign;
        Location* loc = nullptr;
        if (c == 1000) loc = g_locations[3];
        else if (c == 0x3e9) loc = g_locations[4];
        else if (c == 0x3ea) loc = g_locations[1];
        else if (c == 0x3eb) loc = g_locations[2];
        else if (c == 0x3ec) loc = g_locations[5];
        else if (c - 1 < 999) loc = l->mission ? (Location*)&g_side[c - 1] : (Location*)&g_core[c - 1];
        else if (l->id == 0x400) loc = &g_home;
        else if (c == 4000) loc = &g_start;
        if (!loc) continue;
        loc->info = l;
        l->onGlobalMap = true;
        if (l->mission)
            for (int t : l->clearTasks)
                if (!GameState::IsTaskStarted((unsigned)t))
                    if (Tasks::TaskInfo* ti = Tasks::GetTask((uint32_t)t)) ti->dnShow = true;
    }
    g_desert.name->SetText(StringTable::GetString("world_node_desert"));
    g_ice.name->SetText(StringTable::GetString("world_node_winter"));
    g_haunted.name->SetText(StringTable::GetString("world_node_emperor"));
    g_arena.name->SetText(StringTable::GetString("MAP_ARENA"));
    g_desert.comp->SetEnabled(false);
    g_ice.comp->SetEnabled(false);
    g_iceCastle.comp->SetEnabled(false);
    g_arena.comp->SetEnabled(false);
    g_marker = GUI::RegisterUI("../resource/kingdom_ui/1Original/World_marker_holder.xml", "World_marker_holder.png",
                               g_windowScale, 0, 0, 0, 0, false, 1.f);
    g_marker->SetVisibility(false);
    g_markerMale = GUI::GetWindowTyped<Window>(g_marker, "world_marker_male");
    g_markerFemale = GUI::GetWindowTyped<Window>(g_marker, "world_marker_female");
    g_sultan = GUI::RegisterUI("../resource/kingdom_ui/1Original/World_marker_event_holder.xml",
                               "World_marker_event_holder.png", g_windowScale, 0, 0, 0, 0, false, 1.f);
    g_sultan->SetVisibility(false);
    g_sultan->SetOnClick(OnSultan);
    g_sultanTimer = GUI::GetWindowTyped<Textfield>(g_sultan, "timer_time");
    for (int i = 0; i < 7; ++i) g_sultanPath[i] = GUI::GetWindowTypedF<Window>(g_world, "marker_sultan_path.marker_%02d", i + 1);
    Window* r = GUI::RegisterUI("../resource/kingdom_ui/1Original/Button_building_controls.xml", "Button_building_controls.png",
                                GUI::GetHudScaleFactor(), 0, 0, 0, 0, false, 1.f);
    g_return.LoadFrom(r, nullptr);
    g_return.root->SetVisibility(false);
    g_return.root->SetOnClick(OnReturn);
    g_return.green->SetVisibility(false);
    g_return.red->SetVisibility(false);
    g_return.icon->SetTexture(IconManager::GetIcon("leave_screen_60", false), true);
    g_returnEffect = GUI::CreateMovementEffect(g_return.root);
    Window* n = GUI::RegisterUI("../resource/kingdom_ui/1Original/Button_building_controls.xml", "Button_building_controls.png",
                                GUI::GetHudScaleFactor(), 0, 0, 0, 0, false, 1.f);
    g_navigate.LoadFrom(n, nullptr);
    g_navigate.root->SetVisibility(false);
    g_navigate.root->SetOnClick(OnNavigate);
    g_navigate.green->SetVisibility(false);
    g_navigate.red->SetVisibility(false);
    g_navigate.icon->SetTexture(IconManager::GetIcon("gold_map_60", false), true);
    g_navigateEffect = GUI::CreateMovementEffect(g_navigate.root);
    g_accY = 0.f;
    g_decel = 5000.f;
    g_velX = 0.f;
    g_velY = 0.f;
    g_accX = 0.f;
}

// @0x2c6234: a bobbing arrow at a point of the map (keep: the others stay).
void ArrowAt(int x, int y, bool keep) {
    if (!keep) HideArrow();
    g_arrows.push_back(Arrow{});
    Arrow& a = g_arrows.back();
    a.sprite = Render::CreateSprite(IconManager::GetIcon("Arrow_show", false), 0xe, false, false);
    a.x = (float)(x + g_world->x);
    a.y = (float)(y + g_world->y);
    a.sprite->screenSpace = true;
    Render::SetVisibility(a.sprite, true);
}

}  // namespace GlobalMapWindow
