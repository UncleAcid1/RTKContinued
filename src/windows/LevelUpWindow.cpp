// LevelUpWindow: "New level!" (Lvlup.xml): the level, a greeting, and a strip of what the level
// unlocks (the shop's buildings of that level, new tasks, the crystal reward) with arrows to scroll
// it. The crystal reward ("level_up_cb", at most 5) is paid when the window closes.
// Port of LevelUpWindow (libkingdom.so 5.11), 0x2f147c..0x2f3740. Statics at 0x625468.
//
// PORT (online removed): the Facebook/Twitter share (WallPost, the ShareButtonsHolder ticks shown
// from level 3) is not offered; the button is the plain "Ok" the original shows below level 3.
#include <vector>

#include "engine/IconManager.h"
#include "engine/Render.h"
#include "game/BuildingHovers.h"
#include "game/BuildingPlacement.h"
#include "game/Entity.h"
#include "game/EntityManager.h"
#include "game/GameData.h"
#include "game/GameState.h"
#include "game/Map.h"
#include "game/Setting.h"
#include "game/StringTable.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/ContentScroller.h"
#include "hud/Shared.h"
#include "windows/Windows.h"

namespace LevelUpWindow {
namespace {

using GUI::Button;
using GUI::Textfield;
using GUI::Window;

// One cell of the unlock strip (0x10 bytes), "item_holder_%02d".
struct ItemHolder {
    Window* root = nullptr;          // +0x00
    Window* icon = nullptr;          // +0x04 icon_60_on (takes a depth slot)
    Textfield* name = nullptr;       // +0x08 text_item_class_01
    Textfield* count = nullptr;      // +0x0c text_item_count
};

// An unlock (0xc bytes): a building, the new tasks, new land, or the crystal reward.
struct Unlock {
    const GameData::BuildingData* building = nullptr;   // +0x00
    bool tasks = false;              // +0x04
    bool land = false;               // +0x05
    int crystals = 0;                // +0x08
};

WindowManager::FunctionalWindow* g_queue = nullptr;   // LevelUpWindow::wnd
Window* g_root = nullptr;              // 0x625468 Lvlup.xml
GUI::TweenEffect* g_tween = nullptr;   // 0x62546c
Shared::ContentScroller g_scroller;    // 0x625470
ItemHolder g_holders[5];               // 0x62557c
std::vector<Unlock> g_unlocks;         // 0x6255cc
Window* g_left = nullptr;              // 0x6255d8 button_navigate_left
Window* g_right = nullptr;             // 0x6255dc button_navigate_right
Shared::LargeLogoWindow g_logo;        // 0x6255e0
Window* g_chain1 = nullptr;            // 0x6255f0 chain_01
Window* g_chain2 = nullptr;            // 0x6255f4 chain_02
Textfield* g_shareText = nullptr;      // 0x6255f8 lvl_up_share_holder.share.text_upgrade
Window* g_shareIcon = nullptr;         // 0x6255fc lvl_up_share_holder.share.icon_holder
Button* g_shareClick = nullptr;        // 0x625600
Window* g_social = nullptr;            // 0x625604 ShareButtonsHolder root, lvl_up_share_social_01
Textfield* g_header = nullptr;         // 0x625618 text_header
Textfield* g_desc1 = nullptr;          // 0x62561c text_level_desc_01
Textfield* g_desc2 = nullptr;          // 0x625620 text_level_desc_02
Textfield* g_levelText = nullptr;      // 0x625624 text_level_text
Window* g_male = nullptr;              // 0x625628 char_male_tiny
Window* g_female = nullptr;            // 0x62562c char_female_tiny
Button* g_leftClick = nullptr;         // 0x625630
Button* g_rightClick = nullptr;        // 0x625634
Textfield* g_digit1 = nullptr;         // 0x625638 text_level_digit_1
Textfield* g_digit2 = nullptr;         // 0x62563c text_level_digit_2
int g_centerShift = 0;                 // 0x625640 the strip's shift when it holds fewer than 5
bool g_tablet = false;                 // 0x60f2e4
float g_scale = 1.f;                   // 0x60f2e8

// @0x2f151c / @0x2f1540: one cell along.
void OnLeft() { g_scroller.bounce += (float)g_scroller.itemSize; }
void OnRight() { g_scroller.bounce -= (float)g_scroller.itemSize; }

// @0x2f19f0. PORT: no wall posts (see the top of the file).
void OnShare() { Hide(); }

// @0x2f1a64
bool OnBack() {
    if (!g_queue->shown) return false;
    OnShare();
    return true;
}

// @0x2f1a8c
void Move(int x, int y) { g_scroller.Move(x, y); }

// @0x2f1aa8
bool Click(int x, int y, bool pressed) {
    bool r = false;
    if (g_root->visibleSelf) {
        if (g_scroller.Click(x, y, pressed, g_root)) return true;
        r = g_root->visibleSelf;
    }
    g_root->Click(x, y, pressed, false);
    return r;
}

// @0x2f147c
void SetZ(float z) {
    if (!g_queue->shown) return;
    g_root->SetZ(z);
    g_root->SetPosition(g_tween->GetCenteredX(), g_tween->GetCenteredY());
}

// @0x2f1848: the level-up is saved once the window is in.
void OnTweenIn() {
    if (GameState::tutorial == 0x34) GUI::SetInteractionLock(false);
    // FileManager::OnLevelUpEvent(level): online statistics, not ported.
    if (!g_root->visibleSelf || GameState::IsCityTutorial()) return;
    // SaveManager::QueueSendSave: online, not ported.
    Map::Save(0);
}

// @0x2f16ec: the tutorial steps move on, and the crystal reward is paid: dropped at the hero's
// feet (the view centred on them), or added straight away.
void OnTweenOut() {
    WindowManager::WindowHide(false);
    g_queue->shown = false;
    g_queue->MoveWindowDown(true);
    if (GameState::tutorial == 0x34) GameState::tutorial = 0x35;
    if (GameState::secondTutorial == 0x211) GameState::secondTutorial = 0x212;
    else if (GameState::secondTutorial == 0x218) GameState::secondTutorial = 0x219;
    GameState::DropGamePauseState();
    int cb = Setting("level_up_cb").GetInt();
    if (cb < 0) cb = 0;
    else if (cb > 4) cb = 5;
    if (GameState::tutorial != 0x35) {
        if (Entity* player = EntityManager::GetPlayer()) {   // UNVERIFIED (milestone 4): no hero yet
            for (int i = 0; i < cb; ++i)
                BuildingHovers::DropResource(player->worldX, player->worldY, GameState::kCrystal, 1, false, false);
            Render::CenterOn(player->worldX, player->worldY);   // UNVERIFIED: animated, zoom 0.4
            return;
        }
    }
    GameState::AddCrystals(cb);
}

// @0x2f18ac: the arrows follow the strip.
void Update(float dt) {
    if (!g_queue->shown) return;
    g_scroller.Update(dt);
    g_left->SetEnabled(g_scroller.CanMoveLeft());
    g_right->SetEnabled(g_scroller.CanMoveRight());
    g_left->SetVisibility(g_scroller.CanMove());
    g_right->SetVisibility(g_scroller.CanMove());
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
        f.move = Move;
        g_queue = new WindowManager::FunctionalWindow("LevelUpWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_queue->shown; }   // @0x2f1508

// @0x2f1b78
void Init() {
    g_tablet = GUI::IsTabletVersion();
    int extraH = !g_tablet ? -0xfa : (GUI::IsHighDPIVersion() ? -100 : 0);
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Lvlup.xml", "Lvlup.png", -1.f, 0, 0, 0, extraH, false,
                             1.f);
    g_queue->RegisterTopWindow(g_root);
    g_scale = g_root->scale;
    g_root->SetVisibility(false);
    g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::GetVerticalCenter(g_root->h));
    g_tween = GUI::CreateTweenEffect(g_root, nullptr, OnTweenIn, OnTweenOut);
    g_logo.LoadFrom(g_root, "logo_holder_large");
    g_logo.SetLanguage((unsigned)StringTable::GetLangID());
    g_chain1 = GUI::GetWindowTyped<Window>(g_root, "chain_01");
    g_chain2 = GUI::GetWindowTyped<Window>(g_root, "chain_02");
    if (!g_tablet) {   // phones: no logo and chains, the window sits higher
        g_logo.holder->SetVisibility(false);
        g_tween->insetT = (int)(g_scale * 125.f);
        g_chain1->SetVisibility(false);
        g_chain2->SetVisibility(false);
    }
    g_shareText = GUI::GetWindowTyped<Textfield>(g_root, "lvl_up_share_holder.share.text_upgrade");
    g_shareIcon = GUI::GetWindowTyped<Window>(g_root, "lvl_up_share_holder.share.icon_holder");
    g_shareIcon->takesZ = true;
    g_shareClick = GUI::GetWindowTyped<Button>(g_root, "lvl_up_share_holder.share.clickArea");
    g_shareClick->SetOnClick(OnShare);
    // ShareButtonsHolder::LoadFrom @0x3423d4 (its Twitter tick toggles OnTwitterToggle @0x2f3720).
    // PORT: only its root, kept hidden.
    g_social = GUI::GetWindowTyped<Window>(g_root, "lvl_up_share_social_01");
    g_header = GUI::GetWindowTyped<Textfield>(g_root, "text_header");
    g_desc1 = GUI::GetWindowTyped<Textfield>(g_root, "text_level_desc_01");
    g_desc2 = GUI::GetWindowTyped<Textfield>(g_root, "text_level_desc_02");
    g_levelText = GUI::GetWindowTyped<Textfield>(g_root, "text_level_text");
    g_male = GUI::GetWindowTyped<Window>(g_root, "char_male_tiny");
    g_female = GUI::GetWindowTyped<Window>(g_root, "char_female_tiny");
    g_left = GUI::GetWindowTyped<Window>(g_root, "button_navigate_left");
    g_right = GUI::GetWindowTyped<Window>(g_root, "button_navigate_right");
    g_leftClick = GUI::GetWindowTyped<Button>(g_root, "button_navigate_left.clickArea");
    g_leftClick->SetOnClick(OnLeft);
    g_leftClick->SetActionSound("ui_click", 1);
    g_rightClick = GUI::GetWindowTyped<Button>(g_root, "button_navigate_right.clickArea");
    g_rightClick->SetOnClick(OnRight);
    g_rightClick->SetActionSound("ui_click", 1);
    for (unsigned i = 0; i < 5; ++i) {
        ItemHolder& h = g_holders[i];
        h.root = GUI::GetWindowTypedF<Window>(g_root, "item_holder_%02d", i + 1);
        h.icon = GUI::GetWindowTypedF<Window>(g_root, "item_holder_%02d.icon_60_on", i + 1);
        h.icon->takesZ = true;
        h.name = GUI::GetWindowTypedF<Textfield>(g_root, "item_holder_%02d.text_item_class_01", i + 1);
        h.count = GUI::GetWindowTypedF<Textfield>(g_root, "item_holder_%02d.text_item_count", i + 1);
    }
    // The fifth cell waits one cell to the right of the fourth (the strip recycles them).
    g_holders[4].root->MoveWindow(g_holders[1].root->x - g_holders[0].root->x, 0);
    g_digit1 = GUI::GetWindowTyped<Textfield>(g_root, "text_level_digit_1");
    g_digit2 = GUI::GetWindowTyped<Textfield>(g_root, "text_level_digit_2");
    g_scroller.Init();
    Window* view = GUI::GetWindowTyped<Window>(g_root, "scaler_transparent");
    g_scroller.viewport = {view->x, view->y, view->x + view->w, view->y + view->h};
    for (ItemHolder& h : g_holders) {
        h.root->SetClipRect(&g_scroller.viewport);
        g_scroller.windows.push_back(h.root);
    }
    g_queue->hideOnOuterClick = true;
    g_scroller.unkA0 = 5;
    g_scroller.onScroll = UpdateContents;
    g_scroller.padEnd = (int)(g_scale * 60.f);
    g_scroller.viewport.left += 4;
    g_scroller.itemSize = g_holders[1].root->x - g_holders[0].root->x;
    g_scroller.viewport.right -= 4;
    g_scroller.viewport.bottom -= 4;
    g_queue->fn.zRange = WindowManager::ReturnSmallZRange;
    g_queue->fn.back = OnBack;
    g_queue->fn.show = Show;
    g_queue->fn.update = Update;
    g_scroller.horizontal = true;
    g_scroller.perLine = 1;
}

// @0x2f1b38
void Deinit() {
    GUI::RemoveTweenEffect(g_tween);
    g_tween = nullptr;
    delete g_root;
    g_root = nullptr;
}

// @0x2f2398: the texts, the unlocks of the new level, then the visible cells of the strip
// (centred while there are fewer than five).
void UpdateContents() {
    g_shareText->SetText(StringTable::GetString("SHARE_OK"));
    g_shareIcon->SetTexture(IconManager::GetIcon("gold_confirm"), true);
    g_header->SetText(StringTable::GetString("NEW_LEVEL"));
    g_desc1->SetText(SWPrintf(0x80, StringTable::GetString("NEW_LEVEL_DESC_2"), {GameState::GetPlayerName()}).c_str());
    g_desc2->SetText(
        SWPrintf(0x80, StringTable::GetString("NEW_LEVEL_DESC"), {ToWideString(GameState::GetLevel())}).c_str());
    g_levelText->SetText(StringTable::GetString("NEW_LEVEL_DESC_3"));
    g_male->SetVisibility(GameState::IsMalePlayer());
    g_female->SetVisibility(!GameState::IsMalePlayer());
    // PORT: the share ticks (shown from level 3 with the Facebook/Twitter states) stay hidden.
    g_social->SetVisibility(false);
    std::u32string digits = SWPrintf(0x80, U"%d", {GameState::GetLevel()});
    g_digit1->SetText(digits.c_str());
    g_digit2->SetText(digits.c_str());

    g_unlocks.clear();
    int cb = Setting("level_up_cb").GetInt();
    if (cb > 4) cb = 5;
    g_unlocks.push_back({nullptr, false, false, cb});
    for (unsigned i = 0; const GameData::BuildingData* d = GameData::EnumBuildings(i); ++i)
        if ((unsigned)d->tab <= 9 && d->buy && (int)d->level == GameState::GetLevel()) g_unlocks.push_back({d});
    // UNVERIFIED (milestone 4): Tasks::HasTasksForLevel(level) adds the "new tasks" cell.

    for (ItemHolder& h : g_holders) h.root->SetVisibility(false);
    g_scroller.itemCount = (unsigned)g_unlocks.size();
    g_left->SetEnabled(g_scroller.CanMoveLeft());
    g_right->SetEnabled(g_scroller.CanMoveRight());
    g_left->SetVisibility(g_scroller.CanMove());
    g_right->SetVisibility(g_scroller.CanMove());
    for (ItemHolder& h : g_holders) h.root->MoveWindow(-g_centerShift, 0);
    g_centerShift = 0;

    const unsigned first = g_scroller.firstIndex, count = (unsigned)g_unlocks.size();
    unsigned i = first;
    unsigned shown = 0;
    for (; i < count; ++i) {
        ItemHolder& h = g_holders[i - first];
        h.root->SetVisibility(true);
        h.count->SetVisibility(false);
        const Unlock& u = g_unlocks[i];
        if (u.building) {
            GUI::FitImageIntoWindow(h.icon, GameData::BuildingImage(u.building, 0), true, true);
            h.name->SetText(StringTable::GetString(u.building->name.c_str()));
        } else if (u.tasks) {
            h.icon->SetTexture(IconManager::GetIcon("icon_post"), true);
            h.name->SetText(StringTable::GetString("NEW_TASKS"));
        } else if (u.land) {
            h.icon->SetTexture(IconManager::GetIcon("icon_land_buy"), true);
            h.name->SetText(StringTable::GetString("NEW_LAND"));
        } else if (u.crystals != 0) {
            h.icon->SetTexture(IconManager::GetIcon("crystal_60"), true);
            h.name->SetText(StringTable::GetString("RES_COUNTRY_BUCKS"));
            h.count->SetVisibility(true);
            h.count->SetText(ToWideString(u.crystals));
        }
        if (i == first + 4) {
            shown = i + 1 - first;
            break;
        }
    }
    if (i >= count) shown = i - first;
    if (count != 0 && count < 5) {
        int cell = g_holders[1].root->x - g_holders[0].root->x;
        g_centerShift = (int)((float)(unsigned)(cell * (4 - (int)shown)) * 0.5f);
    }
    for (ItemHolder& h : g_holders) h.root->MoveWindow(g_centerShift, 0);
    g_root->UpdatePosition();
}

// @0x2f3740: deferred while anything else is up; the hero is healed, the strip starts at the
// beginning and the game pauses.
void Show() {
    if (!GameState::IsPlayerCity()) return;
    if (!g_root->visibleSelf &&
        (WindowManager::GetShownWindowCount() != 0 || BuildingPlacement::Activated() || GameState::IsPvPTutorial() ||
         GameState::IsTameTutorial())) {
        // UNVERIFIED: BuildingMovement::Activated (3e.5) and PlayerTopWindow::IsPlayerDialogVisible
        // (milestone 4) also defer it.
        WindowManager::EnqueueWindow(Show, 0.f);
        return;
    }
    // FileManager::ToggleStatEvent(2) at level 4: online statistics, not ported.
    // UNVERIFIED (milestone 4): the hero's HP is refilled and GameState::UpdatePlayerRegenerationState.
    // UNVERIFIED (milestone 5): SoundsManager::PlaySound("ui_level_up").
    bool wasShown = g_queue->shown;
    g_queue->shown = true;
    if (!g_root->visibleSelf) WindowManager::WindowShow(false);
    g_queue->MoveWindowOnTop(true);
    if (!g_root->visibleSelf) g_tween->AnimateIn();
    g_root->SetVisibility(true);
    g_scroller.firstIndex = 0;
    g_scroller.HandleMove(-g_scroller.offset);
    UpdateContents();
    // UNVERIFIED (milestone 4): HUDWindow::UpdateTasks.
    BuildingHovers::HideArrow();
    if (!wasShown) GameState::RaiseGamePauseState();
    g_root->PreloadTextures();   // GUI::PreloadWindow
    if (GameState::secondTutorial - 0x20fu < 2) GameState::secondTutorial = 0x211;
    else if (GameState::secondTutorial == 0x217) GameState::secondTutorial = 0x218;
}

// @0x2f1968
void Hide() {
    if (g_root->visibleSelf) {
        g_tween->AnimateOut();
        g_root->SetZ(g_root->z);
        Render::SortRenderLayer(Render::kLayerGUI, 1);
    }
    // UNVERIFIED (milestone 4): with GameState::GetTutorialType() 1 or 2 at tutorial step 0x34,
    // CastleTopWindow::Show.
}

}  // namespace LevelUpWindow
