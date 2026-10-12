// HUDWindow: the driver of the city HUD. Port of HUDWindow (libkingdom.so 5.11), 0x2c6498..0x2c9e00.
#include "hud/HUD.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "engine/IconManager.h"
#include "engine/Render.h"
#include "engine/Resources.h"
#include "game/Tasks.h"
#include "game/AIState.h"
#include "game/Building.h"
#include "game/BuildingHovers.h"
#include "game/Entity.h"
#include "game/EntityManager.h"
#include "game/GameData.h"
#include "game/Fog.h"
#include "game/GameState.h"
#include "game/Map.h"
#include "game/StringTable.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "windows/Windows.h"

namespace HUDWindow {
namespace {

using GUI::Textfield;
using GUI::Window;

// LocatorHolder (0x30 bytes, LoadFrom @0x2c9520): the arrow at the screen edge pointing at the
// player or at a task target.
struct LocatorHolder {
    Window* root = nullptr;        // +0x00
    Window* bg = nullptr;          // +0x04 player_locator_18_bg
    Window* icon = nullptr;        // +0x08 quest_helper_icon_holder.icon_60 (takes a depth slot)
    Window* maleGood = nullptr;    // +0x0c icon_char_damage_holder.locator_character_male_good
    Window* femaleGood = nullptr;  // +0x10 ...female_good
    Window* maleBad = nullptr;     // +0x14 ...male_bad
    Window* femaleBad = nullptr;   // +0x18 ...female_bad
    Window* finderOrange = nullptr;   // +0x1c quest_helper_icon_holder.button_quest_finder_orange
    Window* finderRed = nullptr;      // +0x20
    Window* finderGreen = nullptr;    // +0x24
    Window* finderBlue = nullptr;     // +0x28
    float unk2c = 0.f;                // +0x2c

    void LoadFrom(const char* xml, const char* png) {
        root = GUI::RegisterUI(xml, png, GUI::GetHudScaleFactor(), 0, 0, 0, 0, false, 1.f);
        root->SetVisibility(false);
        bg = GUI::GetWindowTyped<Window>(root, "player_locator_18_bg");
        icon = GUI::GetWindowTyped<Window>(root, "quest_helper_icon_holder.icon_60");
        icon->takesZ = true;
        maleGood = GUI::GetWindowTyped<Window>(root, "icon_char_damage_holder.locator_character_male_good");
        femaleGood = GUI::GetWindowTyped<Window>(root, "icon_char_damage_holder.locator_character_female_good");
        maleBad = GUI::GetWindowTyped<Window>(root, "icon_char_damage_holder.locator_character_male_bad");
        maleBad->SetVisibility(true);
        maleBad->takesZ = true;
        maleBad->MoveWindow(1, 0);
        femaleBad = GUI::GetWindowTyped<Window>(root, "icon_char_damage_holder.locator_character_female_bad");
        femaleBad->SetVisibility(true);
        femaleBad->takesZ = true;
        finderOrange = GUI::GetWindowTyped<Window>(root, "quest_helper_icon_holder.button_quest_finder_orange");
        finderRed = GUI::GetWindowTyped<Window>(root, "quest_helper_icon_holder.button_quest_finder_red");
        finderGreen = GUI::GetWindowTyped<Window>(root, "quest_helper_icon_holder.button_quest_finder_green");
        finderBlue = GUI::GetWindowTyped<Window>(root, "quest_helper_icon_holder.button_quest_finder_blue");
        unk2c = 0.f;
    }
};

// TimerHolder (0x10 bytes, LoadFrom @0x2c9440): the farm map timer.
struct TimerHolder {
    Window* root = nullptr;        // +0x00
    Textfield* header = nullptr;   // +0x04 timer_header
    Window* icon = nullptr;        // +0x08 icon_60_timer (the sand clock)
    Textfield* time = nullptr;     // +0x0c timer_time

    void LoadFrom(const char* xml, const char* png) {
        root = GUI::RegisterUI(xml, png, GUI::GetHudScaleFactor(), 0, 0, 0, 0, false, 1.f);
        root->SetVisibility(false);
        icon = GUI::GetWindowTyped<Window>(root, "icon_60_timer");
        icon->SetTexture(IconManager::GetSandClockIcon(), false, 0, 0, false, 0);
        header = GUI::GetWindowTyped<Textfield>(root, "timer_header");
        time = GUI::GetWindowTyped<Textfield>(root, "timer_time");
    }
};

// Statics at 0x6209e4
WindowManager::FunctionalWindow* g_queue = nullptr;
float g_z = 0.03f;                    // 0x60f278
LocatorHolder g_playerLocator;        // +0x00
LocatorHolder g_taskLocator;          // +0x30
TimerHolder g_farmTimer;              // +0x60
Window* g_mapName = nullptr;          // +0x70 Text_map_name_popup (centred)
Render::Sprite* g_darken = nullptr;   // +0x78 full-screen sprite (created by the screen darkening)
Window* g_mapName2 = nullptr;         // +0x80 Text_map_name_popup (near the top)
Textfield* g_mapName2Text = nullptr;  // +0x84
int g_unk88 = 0;                      // +0x88
Textfield* g_mapNameText = nullptr;   // +0x94
float g_mapNameTime = 0.f;            // +0x98
float g_mapNameW = 0.f, g_mapNameH = 0.f;   // +0x9c +0xa0 the name sprite's full size (h < 0: text)
Render::Sprite* g_frameBorder = nullptr;   // +0xa4 the 8 sprites of the screen frame, chained

// The task locator's state.
bool g_locatorPending = false;        // +0x74 ShowTaskLocaterTo: shows once no window is up
bool g_locatorGoing = false;          // +0x75 set by Update's once-a-second locator of a campaign map (4f)
bool g_quickLocatorForce = false;     // +0x76 SetQuickLocatorForce (the game bot)
bool g_quickLocator = false;          // +0x7c the target is a quick task's: found at once, without the tap
int g_targetX = 0, g_targetY = 0;     // +0x8c +0x90 the target in world pixels
int g_targetTileX = 0, g_targetTileY = 0;   // +0xa8 +0xac and as a tile (0, 0: none)
GUI::Callback* g_arrowCallback = nullptr;   // +0xb0 the help arrow's callback taken over by the locator

bool FogIsVisible(int x0, int y0, int x1, int y1) { return Fog::IsVisible(x0, y0, x1, y1); }

// UNVERIFIED (milestone 4): CastleTopWindow::OnFindPlayer moves the camera to the player;
// OnFarmTimer opens the farm.
void OnFindPlayer() {}
void OnFarmTimer() {}

// @0x2c7cb8 HUDWindow::OnFindTask: the task locator was tapped (or a quick task's target was set).
// Off screen: the camera travels there; on screen: the help arrow's callback runs, or the hero
// walks to the tile, or the camera centres on the point.
// UNVERIFIED: the camera path (Map::ClearCameraPoints + AddCameraPoint(tileX, tileY, 1, 1, 0, x, y,
// 0, 1.0, true, 0.0, 0)) and the animated Render::CenterOn(x, y, true, true, 0.4, 0, false) are
// both the instant Render::CenterOn here.
void OnFindTask() {
    int tileX = g_targetTileX, tileY = g_targetTileY;
    int x = g_targetX, y = g_targetY;
    if (tileX == 0 && tileY == 0) {
        if (x != 0 || y != 0) {
            tileX = x;
            tileY = y;
            Map::WorldCoordinatesToTile(tileX, tileY);
        }
    }
    if ((x != 0 || y != 0 || tileX != 0 || tileY != 0) && x == 0 && y == 0) {
        x = tileX;
        y = tileY;
        Map::TileCoordinatesToWorld(x, y);
    }
    int sx = x, sy = y;
    Map::WorldCoordinatesToScreen(sx, sy);
    if (g_locatorGoing) {
        if (!FogIsVisible(x - 0x20, y - 0x20, x + 0x20, y + 0x20)) Render::CenterOn((float)x, (float)y);
        else Render::CenterOn((float)g_targetX, (float)g_targetY);
        g_taskLocator.root->SetVisibility(false);
        g_locatorGoing = false;
        return;
    }
    if (sx < 0 || GUI::ScreenWidth() < sx || sy < 0 || GUI::ScreenHeight() < sy) {
        g_taskLocator.root->SetVisibility(false);
        Render::CenterOn((float)x, (float)y);
    } else if (!g_arrowCallback) {
        if (g_targetTileX == 0 && g_targetTileY == 0) {
            Render::CenterOn((float)g_targetX, (float)g_targetY);
        } else {
            g_taskLocator.root->SetVisibility(false);
            if (Entity* player = EntityManager::GetPlayer()) {
                // The hero is sent as if the tile had been tapped.
                int mx = 0, my = 0;
                WindowManager::GetMousePosition(mx, my);
                int px = g_targetTileX, py = g_targetTileY;
                Map::TileCoordinatesToWorld(px, py);
                Map::WorldCoordinatesToScreen(px, py);
                WindowManager::SetMousePosition(px, py);
                player->GetAI()->ClickedTile(g_targetTileX, g_targetTileY, 0, 0);
                WindowManager::SetMousePosition(mx, my);
            }
        }
    } else if (!g_quickLocator) {
        g_taskLocator.root->SetVisibility(false);
        (*g_arrowCallback)();
        g_arrowCallback = nullptr;   // (not deleted, as the original)
    } else {
        if (!FogIsVisible(x - 0x20, y - 0x20, x + 0x20, y + 0x20)) Render::CenterOn((float)x, (float)y);
        else Render::CenterOn((float)g_targetX, (float)g_targetY);
    }
    g_targetTileY = 0;
    g_targetTileX = 0;
}

// The locator `l` on the circle around the screen's centre, towards the world point (x, y) that
// is `dist` away from the view's centre (cx, cy); its arrow turns to point there.
void PlaceLocator(LocatorHolder& l, float x, float y, int cx, int cy, float dist, float z, bool shorten) {
    const int W = GUI::ScreenWidth(), H = GUI::ScreenHeight();
    const int marginY = W < H ? 0x32 : 100, marginX = W < H ? 0x28 : 0x96;
    if (Render::Sprite* s = l.bg->sprite) {
        float radius = (float)std::min(W / 2 - marginX, H / 2 - marginY);
        if (shorten && radius + 40.f > dist) radius = (float)(int)(dist - 40.f);
        l.root->SetPosition((int)((float)(W / 2) + ((x - (float)cx) / dist) * radius + s->w * -0.5f),
                            (int)((float)(H / 2) + radius * ((y - (float)cy) / dist) + s->h * -0.5f));
        l.root->SetZ(z);
    }
    l.bg->SetRotation(-(float)std::atan2((double)((int)y - cy), (double)((int)x - cx)));
}

// The distance from the view's centre, the horizontal part scaled down by the screen's shape.
float LocatorDistance(int x, int y, int cx, int cy) {
    const float aspect = (float)GUI::ScreenWidth() / (float)GUI::ScreenHeight();
    return std::sqrt((float)((y - cy) * (y - cy)) + (float)((x - cx) * (x - cx)) / (aspect * aspect * 1.2f));
}

// @0x2c7220 HUDWindow::UpdatePlayerLocator: away from home, the hero's portrait at the screen's
// edge while he is out of view.
void UpdatePlayerLocator() {
    const int W = GUI::ScreenWidth(), H = GUI::ScreenHeight();
    int cx = W / 2, cy = H / 2;
    Map::MouseCoordinatesToWorld(cx, cy);
    Entity* player = EntityManager::GetPlayer();
    if (!player) return;
    const int x = (int)player->worldX, y = (int)player->worldY;
    int sx = x, sy = y;
    Map::WorldCoordinatesToScreen(sx, sy);
    // UNVERIFIED (milestone 4f): PlayerTopWindow::IsPlayerDialogVisible also hides it.
    if (GameState::GetCurrentMapID() == 0 || (0 < sx && sx < W && 0x32 < sy && sy <= H + 0x31) ||
        WindowManager::GetShownWindowCount() != 0 || BuildingHovers::IsWorldDialogVisible() || GameState::IsTutorial()) {
        g_playerLocator.root->SetVisibility(false);
        return;
    }
    const float dist = LocatorDistance(x, y, cx, cy);
    g_playerLocator.root->SetVisibility(true);
    g_playerLocator.maleGood->SetVisibility(GameState::IsMalePlayer());
    g_playerLocator.femaleGood->SetVisibility(!GameState::IsMalePlayer());
    PlaceLocator(g_playerLocator, player->worldX, player->worldY, cx, cy, dist, 0.01f, false);
}

// @0x2c6e04 HUDWindow::UpdateTaskLocator: the quest target's icon at the screen's edge while the
// target is under the fog (or, when travelling to it, until it is in view).
void UpdateTaskLocator() {
    if (g_locatorPending && WindowManager::GetShownWindowCount() == 0) {
        g_taskLocator.root->SetVisibility(true);
        g_locatorPending = false;
    }
    const int W = GUI::ScreenWidth(), H = GUI::ScreenHeight();
    int cx = W / 2, cy = H / 2;
    Map::MouseCoordinatesToWorld(cx, cy);
    const int x = g_targetX, y = g_targetY;
    int sx = x, sy = y;
    Map::WorldCoordinatesToScreen(sx, sy);
    const bool seen = FogIsVisible(x - 0x1e, y - 0x1e, x + 0x1e, y + 0x1e);
    // UNVERIFIED (milestone 4f): PlayerTopWindow::IsPlayerDialogVisible also hides it (both branches).
    if (!g_locatorGoing) {
        if (seen || WindowManager::GetShownWindowCount() != 0 || BuildingHovers::IsWorldDialogVisible()) {
            g_taskLocator.root->SetVisibility(false);
            return;
        }
    } else if ((0x32 < sx && sx < W - 0x32 && 0x32 < sy && sy < H - 0x32) ||
               WindowManager::GetShownWindowCount() != 0 || BuildingHovers::IsWorldDialogVisible()) {
        g_taskLocator.root->SetVisibility(false);
        g_locatorGoing = false;
        return;
    }
    const float dist = LocatorDistance(x, y, cx, cy);
    if (dist < 10.f) {
        g_taskLocator.root->SetVisibility(false);
        g_locatorGoing = false;
    }
    PlaceLocator(g_taskLocator, (float)x, (float)y, cx, cy, dist, 0.011f, true);
}

// @0x2c6498
bool Click(int x, int y, bool pressed) {
    return g_playerLocator.root->Click(x, y, pressed, false) || g_taskLocator.root->Click(x, y, pressed, false) ||
           g_farmTimer.root->Click(x, y, pressed, false);
}

}  // namespace

WindowManager::FunctionalWindow* Queue() {   // static FunctionalWindow "HUDWindow" (_INIT_ 0x2c6720)
    if (!g_queue) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.setZ = SetZ;
        f.hide = [] {};                                   // HUDWindow::Hide @0x2c655c (empty)
        f.click = Click;
        g_queue = new WindowManager::FunctionalWindow("HUDWindow", std::move(f));
    }
    return g_queue;
}

// @0x2c9a7c: the edge locators, the farm timer and the map name popups, all hidden until needed.
void Init() {
    g_darken = nullptr;
    g_unk88 = 0;
    g_playerLocator.LoadFrom("../resource/kingdom_ui/1Original/Player_locator_large.xml", "Player_locator_large.png");
    g_playerLocator.root->SetOnClick(OnFindPlayer);
    g_playerLocator.maleBad->SetTexture(Resources::GetDirectImage("images/damage_locator_m.png"), false, 0, 0, false, 0);
    g_playerLocator.femaleBad->SetTexture(Resources::GetDirectImage("images/damage_locator_f.png"), false, 0, 0, false, 0);
    g_playerLocator.finderOrange->SetVisibility(false);
    g_playerLocator.finderRed->SetVisibility(false);
    g_playerLocator.finderGreen->SetVisibility(false);
    g_playerLocator.finderBlue->SetVisibility(false);
    g_taskLocator.LoadFrom("../resource/kingdom_ui/1Original/Player_locator_large.xml", "Player_locator_large.png");
    g_taskLocator.root->SetOnClick(OnFindTask);
    g_taskLocator.icon->SetTexture(IconManager::GetExclamaitionIcon(), true, 0, 0, false, 0);
    g_taskLocator.maleGood->SetVisibility(false);
    g_taskLocator.maleBad->SetVisibility(false);
    g_taskLocator.femaleGood->SetVisibility(false);
    g_taskLocator.femaleBad->SetVisibility(false);
    g_taskLocator.finderRed->SetVisibility(false);
    g_farmTimer.LoadFrom("../resource/kingdom_ui/1Original/Farm_map_timer.xml", "Farm_map_timer.png");
    g_farmTimer.root->SetOnClick(OnFarmTimer);
    const int W = GUI::ScreenWidth(), H = GUI::ScreenHeight();
    g_mapName = GUI::RegisterUI("../resource/kingdom_ui/1Original/Text_map_name_popup.xml", "Text_map_name_popup.png",
                                GUI::GetHoverScaleFactor(1.f, 1.f), 0, 0, 0, 0, false, 1.f);
    g_mapName->SetVisibility(false);
    g_mapName->SetPosition((W - g_mapName->w) / 2, (H - g_mapName->h) / 2);
    g_mapNameText = GUI::GetWindowTyped<Textfield>(g_mapName, "map_name_popup.text_map_name");
    g_mapName2 = GUI::RegisterUI("../resource/kingdom_ui/1Original/Text_map_name_popup.xml", "Text_map_name_popup.png",
                                 GUI::GetHoverScaleFactor(1.f, 1.f), 0, 0, 0, 0, false, 1.f);
    g_mapName2->SetVisibility(false);
    g_mapName2->SetZ(0.031f);
    g_mapName2Text = GUI::GetWindowTyped<Textfield>(g_mapName2, "map_name_popup.text_map_name");
}

// @0x2c76e0: a 4-pixel frame around the screen from frame_game_border.png: left, right, top and
// bottom edges, then the four corners, chained in that order (screen space, depth 0).
void UpdateFrameBorder() {
    while (g_frameBorder) g_frameBorder = Render::RemoveSprite(g_frameBorder);
    Render::Texture* tex = Resources::GetUIImage("frame_game_border.png", true, false);
    if (!tex) return;
    const float b = 4.f;
    const float W = (float)GUI::ScreenWidth(), H = (float)GUI::ScreenHeight();
    const float tw = (float)tex->w, th = (float)tex->h;
    const float uIn = b / tw, uOut = (tw - b) / tw, vIn = b / th, vOut = (th - b) / th;
    struct Part { float x, y, w, h; int u0, u1, vB, vT; };   // -1: keep CreateSprite's value
    // u0/u1: 0 = uOut, 1 = uIn; vB/vT: 0 = vIn, 1 = vOut
    const Part parts[8] = {
        {0, H - b, b, H - 2 * b, -1, 1, 0, 1},          // left
        {W - b, H - b, b, H - 2 * b, 0, -1, 0, 1},      // right
        {b, b, W - 2 * b, b, 0, 1, 0, -1},              // top
        {b, H, W - 2 * b, b, 0, 1, -1, 1},              // bottom
        {0, b, b, b, -1, 1, 0, -1},                     // top left
        {W - b, b, b, b, 0, -1, 0, -1},                 // top right
        {0, H, b, b, -1, 1, -1, 1},                     // bottom left
        {W - b, H, b, b, 0, -1, -1, 1},                 // bottom right
    };
    Render::Sprite* prev = nullptr;
    for (const Part& p : parts) {
        Render::Sprite* s = Render::CreateSprite(tex, Render::kLayer15, false, false);
        s->w = p.w;
        s->h = p.h;
        s->screenSpace = true;
        if (p.u0 >= 0) s->u0 = p.u0 ? uIn : uOut;
        if (p.u1 >= 0) s->u1 = p.u1 ? uIn : uOut;
        if (p.vB >= 0) s->vBottom = p.vB ? vOut : vIn;
        if (p.vT >= 0) s->vTop = p.vT ? vOut : vIn;
        Render::SetPosition(s, p.x, p.y, 0.f);
        if (prev) prev->next = s;
        else g_frameBorder = s;
        prev = s;
    }
}

void SetInfoText(const char32_t* text) {
    // UNVERIFIED (milestone 4): 30 px lower while ArenaTurnWindow is visible.
    g_mapName2->SetPosition((GUI::ScreenWidth() - g_mapName2->w) / 2, g_mapName2->h + 0x50);
    g_mapName2->SetVisibility(text != nullptr);
    if (text) g_mapName2Text->SetText(text);
}

void SetBottomType(int type) {
    if (type == 3) {
        BottomFarmWindow::Show();
        BottomCityWindow::Hide();
        Render::SortRenderLayer(Render::kLayerGUI, 1);
        return;
    }
    BottomFarmWindow::Hide();
    // UNVERIFIED (milestone 4): GlobalMapWindow::IsVisible also keeps the city bar hidden.
    if (type == 0 && GameState::GetCurrentMapID() == 0 && !ShopWindow::IsVisible()) {
        if (!BottomCityWindow::IsVisible()) BottomCityWindow::Show();
    } else {
        BottomCityWindow::Hide();
    }
    Render::SortRenderLayer(Render::kLayerGUI, 1);
}

void EnterFarm() {
    BattleBarWindow::Hide();
    BeltBarWindow::Hide();
    // UNVERIFIED (milestone 4): PlayerTopWindow::HideDialog (the player's dialog).
    BottomFarmWindow::Show();
    Map::Building* farm = Map::GetCurrentFarm();
    if (!farm) return;
    const char32_t* name = StringTable::GetString(farm->data->name.c_str());
    if (!GUI::IsTabletVersion()) ShowMapName(name);
    else SetInfoText(name);
}

void ShowMapName(const char32_t* name) {
    if (GameState::tutorial < 9 || g_mapName->visibleSelf) return;
    // UNVERIFIED (milestone 4): nothing during a PvP battle (GameState::IsPvPCombatActive).
    g_mapName->SetVisibility(true);
    g_mapNameText->SetText(name ? name : U" ");
    if (g_mapNameText->sprite && g_mapNameText->sprite->tex) {
        g_mapNameTime = 0.f;
        g_mapNameText->SetAlpha(0.f, false);
        Render::Texture* t = g_mapNameText->sprite->tex;
        g_mapNameW = (float)t->w;
        g_mapNameH = (float)-t->h;
    }
    // UNVERIFIED (milestone 5): "emperor_map_enter" on map 0xb, "farm_map_enter" on mission maps.
}

// @0x2c7ba8 HUDWindow::UpdateTasks
void UpdateTasks() { TaskHolderWindow::UpdateTasks(); }
// @0x2c7bac HUDWindow::RemoveTaskArrows
void RemoveTaskArrows() { TaskHolderWindow::RemoveTaskArrows(); }

void ExitFarm() {
    if (!Map::GetCurrentFarm()) return;
    if (GameState::GetCurrentLocation() == 1) GameState::SetCurrentLocation(0);
    Tasks::Update(true);
    UpdateTasks();
    SetBottomType(0);
    Map::ShowFarm(false, nullptr);
    BuildingHovers::Hide();
    BattleBarWindow::Show();
    BeltBarWindow::Show();
    CastleTopWindow::Show();
    BottomFarmWindow::Hide();
    SetInfoText(nullptr);
}

float GetZ() { return g_z; }

// @0x2c909c
void Show() {
    Queue()->MoveWindowOnTop(true);
    int loc = GameState::GetCurrentLocation();
    if (loc != 1) BattleBarWindow::Show();
    if (GameState::GetCurrentLocation() != 1) BeltBarWindow::Show();
    PlayerTopWindow::Show();
    if (GameState::GetCurrentLocation() != 3) TopCityWindow::Show();
    CastleTopWindow::Show();
    // BottomBattleWindow (location 2) belongs to campaign maps (milestone 4).
    if (GameState::GetCurrentLocation() == 0) BottomCityWindow::Show();
    else BottomCityWindow::Hide();
    if (GameState::IsPlayerCity()) TaskHolderWindow::Show();
    else TaskHolderWindow::Hide();
}

// @0x2c9158: the HUD's own windows, the frame border, then the panels in this order.
void SetZ(float z) {
    g_z = z;
    g_mapName->SetZ(0.001f);
    // (+0x1e while ArenaTurnWindow is visible: arena maps, milestone 4)
    g_mapName2->SetPosition((GUI::ScreenWidth() - g_mapName2->w) / 2, g_mapName2->h + 0x50);
    g_playerLocator.root->SetZ(0.01f);
    g_farmTimer.root->SetZ(z);
    UpdateFrameBorder();
    if (g_darken) {
        g_darken->w = (float)GUI::ScreenWidth();
        g_darken->h = (float)GUI::ScreenHeight();
        Render::SetPosition(g_darken, 0.f, g_darken->h, WindowManager::GetTopWindowRange() + 0.0001f);
    }
    PlayerTopWindow::SetZ(z);
    CastleTopWindow::SetZ(z);
    BeltBarWindow::SetZ(z);
    BattleBarWindow::SetZ(z);
    // BottomBattleWindow::SetZ (campaign maps)
    TaskHolderWindow::SetZ(z);
    BottomCityWindow::SetZ(z);
    TopCityWindow::SetZ(z);
}

namespace {
// (HUDWindow::Update) the map-name popup's zoom and fade.
void UpdateMapName(Render::Sprite* s, float dt) {
    const float W = (float)GUI::ScreenWidth(), H = (float)GUI::ScreenHeight();
    if (g_mapNameTime < 1.f) {
        float halfW = g_mapNameW * 0.5f;
        s->w = halfW - (g_mapNameW - halfW) * (std::sqrt(1.f - g_mapNameTime * g_mapNameTime) - 1.f);
        float halfH = g_mapNameH * 0.5f;
        s->h = halfH - (g_mapNameH - halfH) * (std::sqrt(1.f - g_mapNameTime * g_mapNameTime) - 1.f);
        Render::SetAlpha(s, 0.f - (std::sqrt(1.f - g_mapNameTime * g_mapNameTime) - 1.f));
        Render::SetPosition(s, (W - s->w) * 0.5f, (H + s->h) * 0.5f, s->z);
    } else if (g_mapNameTime < 3.f) {
        s->w = g_mapNameW;
        s->h = g_mapNameH;
        Render::SetAlpha(s, 1.f - (g_mapNameTime - 1.f) * 0.5f);
        Render::SetPosition(s, (W - s->w) * 0.5f, (H + s->h) * 0.5f, s->z);
    } else {
        s->w = g_mapNameW;
        s->h = g_mapNameH;
        g_mapName->SetVisibility(false);
    }
    g_mapNameTime += dt;
}
}  // namespace

// @0x2c81c8. Nothing without a player entity. The map-name popup: over the first second the name
// grows from half size (on a quarter circle) and fades in; it fades out until 3 s, then hides.
// Last, the HUD's return: with no window shown after the city tutorial (0x80), outside the second
// tutorial's steps 0x93..0x96 and the tame tutorial, a HUD that is not at the top of the queue
// comes back: the hovers go to the bottom and the panels are shown again.
// UNVERIFIED (milestone 4g): the player's damage locator pulse (male/female portrait).
void Update(float dt) {
    if (!EntityManager::GetPlayer()) return;
    Render::Sprite* s = g_mapNameText->sprite;
    if (g_mapName->visibleSelf && s) UpdateMapName(s, dt);
    UpdatePlayerLocator();
    UpdateTaskLocator();
    // UNVERIFIED (milestones 4f/4g): UpdateScreenDarkening, the mission timer, and once a second on
    // a campaign map without a help arrow the locator of the first open quest step there
    // (SetTaskLocaterIcon + the step's help); then Render::SortRenderLayer(GUI).
    // UNVERIFIED (milestone 4g): not while PlayerTopWindow::IsPlayerDialogVisible; LoadingInternetWindow
    // is online.
    if (WindowManager::GetShownWindowCount() == 0 && GameState::tutorial == 0x80 &&
        3u < (unsigned)(GameState::secondTutorial - 0x93) && !GameState::IsTameTutorial() &&
        WindowManager::Head() != Queue()) {
        BuildingHovers::MoveWindowDown();
        TaskHolderWindow::Show();
        CastleTopWindow::Show();
        TopCityWindow::Show();   // (not while ArenaTurnWindow is visible: milestone 5)
        Queue()->MoveWindowOnTop(true);
    }
}

// @0x2c6560: step size grows with the distance to the target.
int UpdateNumberToTarget(int cur, int target) {
    if (target < cur) {
        int d = std::abs(target - cur);
        if (10000 < d) return cur - (int)((float)d * 0.2f);
        if (2000 < d) return cur - 0x3cb;
        if (500 < d) return cur - 0xb3;
        if (d < 0x51) return d < 0xb ? cur - 1 : cur - 7;
        return cur - 0x2b;
    }
    if (target <= cur) return cur;
    int d = std::abs(target - cur);
    if (10000 < d) return cur + (int)((float)d * 0.2f);
    if (2000 < d) return cur + 0x3cb;
    if (d < 0x1f5) {
        if (d < 0x51) return d < 0xb ? cur + 1 : cur + 7;
        return cur + 0x2b;
    }
    return cur + 0xb3;
}

// @0x2c66bc
void HideLocators() {
    if (!g_playerLocator.root) return;   // PORT: asked before Init
    g_playerLocator.root->SetVisibility(false);
    g_taskLocator.root->SetVisibility(false);
    g_locatorGoing = false;
    g_locatorPending = false;
}

// @0x2c670c
void SetQuickLocatorForce(bool force) { g_quickLocatorForce = force; }

// @0x2c680c HUDWindow::SetTaskLocaterIcon: the quest's picture in the task locator, on the quest
// kind's ring (orange: battle, green: battle side quest, blue: the rest).
void SetTaskLocaterIcon(Render::Texture* icon, bool battle, bool side, bool quick) {
    g_taskLocator.icon->SetTexture(icon, true, 0, 0, false, 0);
    g_taskLocator.icon->ApplyMask(Resources::GetUIImage("mask_icon_holder_60_round.png", false, false));
    g_taskLocator.finderOrange->SetVisibility(battle && !side);
    g_taskLocator.finderGreen->SetVisibility(battle && side);
    g_taskLocator.finderBlue->SetVisibility(!battle);
    g_quickLocator = quick ? true : g_quickLocatorForce;
}

// @0x2c814c HUDWindow::ShowTaskLocaterTo: the task locator points at the world point (and tile);
// it takes the current help arrow's callback, which a tap on the locator then runs.
void ShowTaskLocaterTo(int x, int y, int tileX, int tileY) {
    g_targetTileY = tileY;
    g_locatorPending = true;
    g_targetX = x;
    g_targetY = y;
    g_targetTileX = tileX;
    g_locatorGoing = false;
    delete g_arrowCallback;
    g_arrowCallback = BuildingHovers::HijackArrowCallback();
    if (!g_quickLocator) return;
    g_locatorPending = false;
    OnFindTask();
}

}  // namespace HUDWindow
