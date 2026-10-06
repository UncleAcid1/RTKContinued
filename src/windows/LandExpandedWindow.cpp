// LandExpandedWindow: "Land expanded!" after a piece of land (or a farm's soil patch, 3f) was
// bought (Window_expanded_land.xml): the area's name and an Ok button.
// Port of LandExpandedWindow (libkingdom.so 5.11), 0x2e7368..0x2e79f4. Statics at 0x623dd8.
//
// PORT (online removed): the Facebook/Twitter share (WallPost from level 3, the
// ShareButtonsHolder ticks) is not offered; Ok only closes the window.
#include "engine/IconManager.h"
#include "game/GameState.h"
#include "game/Setting.h"
#include "game/StringTable.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "windows/Windows.h"

namespace LandExpandedWindow {
namespace {

using GUI::Button;
using GUI::Textfield;
using GUI::Window;

WindowManager::FunctionalWindow* g_queue = nullptr;   // LandExpandedWindow::wnd
Window* g_root = nullptr;              // 0x623dd8 Window_expanded_land.xml
uint32_t g_area = 0;                   // 0x623ddc
uint32_t g_farm = 0;                   // 0x623de0 the farm's building id (3f)
int g_patch = 0;                       // 0x623de4
Window* g_landImage = nullptr;         // 0x623de8 image_land_expand
Window* g_farmImage = nullptr;         // 0x623dec image_farm_expand
Textfield* g_title = nullptr;          // 0x623df0 text_land
Textfield* g_purchased = nullptr;      // 0x623df4 text_you_have_purchased
Textfield* g_name = nullptr;           // 0x623df8 text_expand_name
Textfield* g_okText = nullptr;         // 0x623dfc lvl_up_share_holder.share.text_upgrade
Window* g_okIcon = nullptr;            // 0x623e00 lvl_up_share_holder.share.icon_holder
Window* g_social = nullptr;            // 0x623e04 ShareButtonsHolder root, lvl_up_share_social_01
GUI::TweenEffect* g_tween = nullptr;   // 0x623e18
Button* g_ok = nullptr;                // 0x623e1c lvl_up_share_holder.share.clickArea
bool g_tablet = false;                 // 0x60f2cc
float g_scale = 1.f;                   // 0x60f2d0

// @0x2e7368
bool Click(int x, int y, bool pressed) {
    bool visible = g_root->visibleSelf;
    g_root->Click(x, y, pressed, false);
    return visible;
}

// @0x2e7944
void SetZ(float z) {
    g_root->SetZ(z);
    g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::GetVerticalCenter(g_root->h));
}

void OnTweenIn() {}   // @0x2e74e0 UNVERIFIED (milestone 5): PlaySound("ui_task_complete")

// @0x2e74ac
void OnTweenOut() {
    GameState::DropGamePauseState();
    WindowManager::WindowHide(false);
    g_queue->shown = false;
    g_queue->MoveWindowDown(true);
}

// @0x2e781c. PORT: no wall post (see the top of the file).
void OnShare() { Hide(); }

// @0x2e791c
bool OnBack() {
    if (!g_queue->shown) return false;
    OnShare();
    return true;
}

// @0x2e74f8
void UpdateContents() {
    g_landImage->SetVisibility(g_area != 0);
    g_farmImage->SetVisibility(g_farm != 0);
    if (g_area != 0) {
        g_title->SetText(StringTable::GetString("LAND_EXPANDED"));
        g_purchased->SetText(StringTable::GetString("LAND_PURCHASED"));
        GameState::AreaInfo info;
        if (GameState::GetAreaInfo(g_area, info)) g_name->SetText(info.name);
    }
    if (g_farm != 0) {
        g_title->SetText(StringTable::GetString("FARM_EXPANDED"));
        g_purchased->SetText(StringTable::GetString("FARM_PURCHASED"));
        g_name->SetText(U" ");
    }
    g_okText->SetText(StringTable::GetString("SHARE_OK"));
    g_okIcon->SetTexture(IconManager::GetIcon("gold_confirm"), true);
    // PORT: the share ticks (ShareButtonsHolder::SetFacebook/SetTwitter) stay hidden. Their empty
    // backdrop beside Ok is part of the window's background image, so it still shows.
    g_social->SetVisibility(false);
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
        g_queue = new WindowManager::FunctionalWindow("LandExpandedWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_queue->shown; }   // @0x2e73b4

// @0x2e79f4
void Init() {
    g_tablet = GUI::IsTabletVersion();
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Window_expanded_land.xml", "Window_expanded_land.png",
                             -1.f, 0, 0, -100, -300, false, 1.f);
    g_queue->RegisterTopWindow(g_root);
    g_scale = g_root->scale;
    g_root->SetVisibility(false);
    g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::GetVerticalCenter(g_root->h));
    g_tween = GUI::CreateTweenEffect(g_root, nullptr, OnTweenIn, OnTweenOut);
    g_title = GUI::GetWindowTyped<Textfield>(g_root, "text_land");
    g_purchased = GUI::GetWindowTyped<Textfield>(g_root, "text_you_have_purchased");
    g_purchased->SetAlignment(1, 0);
    g_name = GUI::GetWindowTyped<Textfield>(g_root, "text_expand_name");
    g_landImage = GUI::GetWindowTyped<Window>(g_root, "image_land_expand");
    g_farmImage = GUI::GetWindowTyped<Window>(g_root, "image_farm_expand");
    g_okText = GUI::GetWindowTyped<Textfield>(g_root, "lvl_up_share_holder.share.text_upgrade");
    g_okIcon = GUI::GetWindowTyped<Window>(g_root, "lvl_up_share_holder.share.icon_holder");
    g_okIcon->takesZ = true;
    g_ok = GUI::GetWindowTyped<Button>(g_root, "lvl_up_share_holder.share.clickArea");
    g_ok->SetOnClick(OnShare);
    // ShareButtonsHolder::LoadFrom @0x3423d4 (its tick toggles OnTwitterToggle @0x2e7758). PORT:
    // only its root, kept hidden.
    g_social = GUI::GetWindowTyped<Window>(g_root, "lvl_up_share_social_01");
    g_queue->hideOnOuterClick = true;
    g_queue->fn.zRange = WindowManager::ReturnSmallZRange;
    g_queue->fn.back = OnBack;
    g_queue->fn.show = Show;
}

// @0x2e79b4
void Deinit() {
    GUI::RemoveTweenEffect(g_tween);
    g_tween = nullptr;
    delete g_root;
    g_root = nullptr;
}

// @0x2e73c8
void SetAreaParameters(uint32_t areaId) {
    g_area = areaId;
    g_farm = 0;
}

// @0x2e73e0
void SetPatchParameters(uint32_t farmId, int patch) {
    g_patch = patch;
    g_area = 0;
    g_farm = farmId;
}

// @0x2e7778
void Show() {
    g_queue->shown = true;
    g_queue->MoveWindowOnTop(true);
    if (!g_root->visibleSelf) {
        WindowManager::WindowShow(false);
        g_tween->AnimateIn();
    }
    g_root->SetVisibility(true);
    UpdateContents();
    g_root->PreloadTextures();   // GUI::PreloadWindow
    GameState::RaiseGamePauseState();
}

// @0x2e77f8
void Hide() {
    if (!g_root->visibleSelf) return;
    g_tween->AnimateOut();
}

}  // namespace LandExpandedWindow
