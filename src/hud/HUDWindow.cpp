// HUDWindow: the driver of the city HUD. Port of HUDWindow (libkingdom.so 5.11), 0x2c6498..0x2c9e00.
#include "hud/HUD.h"

#include <cmath>
#include <cstdlib>

#include "engine/IconManager.h"
#include "engine/Render.h"
#include "engine/Resources.h"
#include "game/Building.h"
#include "game/BuildingHovers.h"
#include "game/EntityManager.h"
#include "game/GameData.h"
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

// UNVERIFIED (milestone 4): CastleTopWindow::OnFindPlayer and HUDWindow::OnFindTask move the camera
// to the player / the task target; OnFarmTimer opens the farm.
void OnFindPlayer() {}
void OnFindTask() {}
void OnFarmTimer() {}

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

void ExitFarm() {
    if (!Map::GetCurrentFarm()) return;
    if (GameState::GetCurrentLocation() == 1) GameState::SetCurrentLocation(0);
    // UNVERIFIED (milestone 4, Tasks): Tasks::Update(true) and UpdateTasks.
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

// @0x2c81c8. Nothing without a player entity. The map-name popup: over the first second the name
// grows from half size (on a quarter circle) and fades in; it fades out until 3 s, then hides.
// UNVERIFIED (milestones 4d/4g): the player's damage locator pulse (male/female portrait), the
// locators, the screen darkening, the mission timer, the task locator search and the HUD's return
// after the city tutorial.
void Update(float dt) {
    if (!EntityManager::GetPlayer()) return;
    Render::Sprite* s = g_mapNameText->sprite;
    if (!g_mapName->visibleSelf || !s) return;
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

}  // namespace HUDWindow
