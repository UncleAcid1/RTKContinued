// ChestOpenedWindow: "<chest> opened" (Endscreen_cehst_opened.xml, the original's spelling): the
// banner slides down over a black band, the squad cheers, and the loot strip (FoundItemsWindow, its
// contents set by the caller) rises from the bottom. Shown by a map chest with a reward window, a
// decoration's "collect_chest" and the chests window (ChestsWindow, its "transaction mode": the
// chest was bought or opened there, the loot is given when the strip closes and the starter chest
// item is used up). Closing the strip slides the banner away, and back in the arena's map 0x70 the
// map it was entered from loads; the chests window can open again on a chest.
// Port of ChestOpenedWindow (libkingdom.so 5.11), 0x2886d0..0x28979c. Statics at 0x61ac7c.
//
// PORT (online removed): Hide's SaveManager::QueueSendSave and, for a chest not sold for crystals,
// FileManager::BillingReceived (no real money).
#include "engine/Render.h"
#include "engine/Resources.h"
#include "game/AIState.h"
#include "game/BuildingHovers.h"
#include "game/Chests.h"
#include "game/Entity.h"
#include "game/EntityManager.h"
#include "game/GameState.h"
#include "game/Map.h"
#include "game/Rand48.h"
#include "game/Squad.h"
#include "game/StringTable.h"
#include "game/Tasks.h"
#include "engine/Text.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/HUD.h"
#include "windows/Windows.h"

namespace ChestOpenedWindow {
namespace {

using GUI::Textfield;
using GUI::Window;

WindowManager::FunctionalWindow* g_queue = nullptr;   // ChestOpenedWindow::wnd 0x61aca4
const char32_t* g_name = nullptr;     // 0x61ac7c the chest's name (none: "npc_chest")
bool g_transaction = false;           // 0x61ac80 opened in the chests window
uint32_t g_chest = 0;                 // 0x61ac84 that chest
Window* g_root = nullptr;             // 0x61ac88
GUI::MovementEffect* g_effect = nullptr;   // 0x61ac8c
int g_bandY = 0;                      // 0x61ac90 the banner's y (small screens: slid up)
Render::Sprite* g_black = nullptr;    // 0x61ac94 the black band (alpha 0.6)
Render::Sprite* g_goldline = nullptr; // 0x61ac98 Endscreen_goldline_scaler.png under the band
Textfield* g_text = nullptr;          // 0x61ac9c "text"
Render::Texture* g_blackTex = nullptr;   // 0x61aca0
uint32_t g_returnToChest = 0xffffffff;   // 0x60f1ac the chests window's chest to open again
bool g_tablet = false;                // 0x60f1b0
float g_rootScale = 1.f;              // 0x60f1b4

// @0x288978: the band settles.
void OnTweenIn() {
    const float y = g_effect->hideAtEnd ? 0.f : g_black->h;
    Render::SetPosition(g_black, 0.f, y, 0.02f);
    Render::SetPosition(g_goldline, 0.f, g_black->y + 5.f, 0.0199f);
    // UNVERIFIED (milestone 6): SoundsManager::PlaySound("ui_campaign_completed", 1, false).
}

// @0x288a14: the band follows the banner.
void OnTweenMove() {
    if (!g_effect->hideAtEnd) Render::SetPosition(g_black, 0.f, g_black->h * g_effect->t, 0.02f);
    else Render::SetPosition(g_black, 0.f, (1.f - g_effect->t) * g_black->h, 0.02f);
    Render::SetPosition(g_goldline, 0.f, g_black->y + 5.f, 0.0199f);
    Render::SortRenderLayer(0xe, 1);
}

// @0x2887d4: the banner is gone; in the arena's map the map it was entered from loads; the chests
// window opens again on its chest.
void OnTweenOut() {
    g_root->SetVisibility(false);
    if (GameState::GetCurrentMapID() == 0x70) Map::EnqueueLoadMap((int)GameState::mReturnMapID, false, false, 0.f);
    g_name = nullptr;
    if (g_returnToChest == 0xffffffff) return;
    // UNVERIFIED (5p-3): ChestsWindow::Show and SetChest(g_returnToChest).
    g_returnToChest = 0xffffffff;
}

// @0x288ad4
void UpdateContents() {
    const char32_t* name = g_name ? g_name : StringTable::GetString("npc_chest");
    g_text->SetText(SWPrintf(0x100, StringTable::GetString("PAYMENT_DAILY_CHEST_COLLECTED"), {name}).c_str());
}

// @0x288d6c: the band's texture comes back after a context loss.
void Update(float) {
    if (!g_queue->shown) return;
    if (!g_black) return;
    if (g_blackTex && g_blackTex->glId != 0) return;
    g_blackTex = Render::CreateSolidFillTexture(0.f, 0.f, 0.f, 1.f);
    Render::SetTexture(g_black, g_blackTex);
}

// @0x288870: the band is 37% of the screen.
void SetZ(float z) {
    if (!g_queue->shown) return;
    g_root->SetZ(z);
    g_root->SetPosition(g_effect->GetCenteredX(), g_bandY);
    g_black->w = (float)GUI::ScreenWidth();
    g_black->h = (float)GUI::ScreenHeight() * 0.37f;
    Render::SetPosition(g_black, 0.f, g_black->h, 0.02f);
    g_goldline->w = (float)GUI::ScreenWidth();
    Render::SetPosition(g_goldline, 0.f, g_black->y + 5.f, 0.0199f);
}

// @0x2893f0: the taps go to the loot strip.
bool Click(int x, int y, bool pressed) {
    FoundItemsWindow::Click(x, y, pressed);
    return g_queue->shown;
}

// @0x28940c
void Deinit() {
    GUI::RemoveMovementEffect(g_effect);
    g_effect = nullptr;
    delete g_root;
    g_root = nullptr;
    g_black = Render::RemoveSprite(g_black);
    g_goldline = Render::RemoveSprite(g_goldline);
}

// Hide / ForceHide's end (inlined twice in the original): the window leaves the queue; a chest of
// the chests window gives its loot, and the starter chest item is used up (its quest 0x75b starts
// over).
void Close() {
    WindowManager::WindowHide(false);
    g_queue->shown = false;
    g_queue->MoveWindowDown(true);
    Render::SetVisibility(g_black, false);
    Render::SetVisibility(g_goldline, false);
    if (g_transaction) {
        FoundItemsWindow::GiveCollectedToPlayer(false, nullptr, true);
        if (g_chest && Chests::GetItemForChest(g_chest) == 0x2b6 && GameState::GetItemAmount(0x2b6, false)) {
            GameState::RemoveItem(0x2b6, 1);
            GameState::ResetTask(0x75b);
            if (Tasks::TaskInfo* t = Tasks::GetTask(0x75b)) t->dnShow = t->dnShowDefault;
            HUDWindow::UpdateTasks();
        }
        Map::SafeSave();
    }
    g_chest = 0;
    g_transaction = false;
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
        g_queue = new WindowManager::FunctionalWindow("ChestOpenedWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_queue->shown; }                       // @0x2886d0
void SetName(const char32_t* name) { g_name = name; }             // @0x2886e4
// @0x2886f8
void SetChestTransactionMode(uint32_t chest) {
    g_chest = chest;
    g_transaction = true;
}
void SetReturnToChestWindow(uint32_t chest) { g_returnToChest = chest; }   // @0x288714

// @0x288dec: closed at once (no slide).
void ForceHide() {
    if (!g_queue->shown) return;
    FoundItemsWindow::Hide();
    Close();
    OnTweenOut();
}

// @0x288f20 (FoundItemsWindow's close): the loot is dropped for the player (not in the arena's
// map, not a chest of the chests window: that one is given) and the banner slides back up.
void Hide() {
    if (g_root->visibleSelf) {
        if (GameState::GetCurrentMapID() != 0x70 && !g_transaction)
            FoundItemsWindow::GiveCollectedToPlayer(true, nullptr, true);
        const int x = g_effect->GetCenteredX();
        g_effect->Animate(x, g_bandY, g_effect->GetCenteredX(), -g_root->h, false);
    }
    Close();
}

// @0x2890f4: the banner slides down, the drops are collected, the loot strip rises ("tap to
// continue"), the squad cheers and the camera glides to the hero.
void Show() {
    if (g_queue->shown) return;
    if (GameState::GetCurrentMapID() == 0x70) GUI::SetInteractionLock(false);
    BuildingHovers::CollectAll();
    FoundItemsWindow::HideOk();
    FoundItemsWindow::Show();
    g_queue->shown = true;
    g_queue->MoveWindowOnTop(true);
    WindowManager::WindowShow(false);
    const int x = g_effect->GetCenteredX();
    g_effect->Animate(x, -g_root->h, g_effect->GetCenteredX(), g_bandY, true);
    g_root->SetPosition(g_effect->GetCenteredX(), -g_root->h);
    g_root->SetVisibility(true);
    Render::SetVisibility(g_black, true);
    g_black->w = (float)GUI::ScreenWidth();
    g_black->h = (float)GUI::ScreenHeight() * 0.37f;
    Render::SetPosition(g_black, 0.f, 0.f, 0.02f);
    Render::SetVisibility(g_goldline, true);
    g_goldline->w = (float)GUI::ScreenWidth();
    Render::SetPosition(g_goldline, 0.f, 0.f, 0.0199f);
    if (Entity* player = EntityManager::GetPlayer())
        if (BaseSquad* squad = player->GetSquad())
            for (unsigned i = 0; i < (unsigned)squad->GetSoldierCount(); ++i) {
                Entity* e = squad->GetSoldier(i);
                if (!e) continue;
                e->GetAI()->StopMovement();
                e->SetDirection((Rand48::lrand48() & 1) == 0 ? 1 : 7);
                e->SetAnimationP(e->player ? "bat_vict" : "victory", true, false, false);
                if (e->player) Render::CenterOn(e->worldX, e->worldY - e->GetIdleHeight(), true, 0.4f, 0, false);
            }
    UpdateContents();
    g_root->PreloadTextures();   // GUI::PreloadWindow
}

// @0x289470
void Init() {
    g_blackTex = nullptr;
    g_black = nullptr;
    g_goldline = nullptr;
    g_transaction = false;
    g_chest = 0;
    g_tablet = GUI::IsTabletVersion();
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Endscreen_cehst_opened.xml", "Endscreen_cehst_opened.png",
                             GUI::GetHoverScaleFactor(1.f, 1.f), 0, 0, 0, 0, false, 1.f);
    g_queue->RegisterTopWindow(g_root);
    g_rootScale = g_root->scale;
    g_root->SetVisibility(false);
    g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::GetVerticalCenter(g_root->h));
    int h = GUI::ScreenHeight();
    if (h < 0x300) {
        if (h < 600) h = 600;
        g_bandY = -(int)((float)(0x300 - h) * 0.55f);
    }
    g_effect = GUI::CreateMovementEffect(g_root, OnTweenMove, OnTweenIn, OnTweenOut);
    g_text = GUI::GetWindowTyped<Textfield>(g_root, "text");
    g_queue->fn.show = Show;
    g_queue->fn.zRange = WindowManager::ReturnSmallZRange;
    g_queue->fn.update = Update;
    g_blackTex = Render::CreateSolidFillTexture(0.f, 0.f, 0.f, 1.f);
    g_black = Render::CreateSprite(g_blackTex, 0xe, false, false);
    g_black->screenSpace = true;
    Render::SetShaderType(g_black, 1);
    Render::SetVisibility(g_black, false);
    Render::SetAlpha(g_black, 0.6f);
    g_goldline = Render::CreateSprite(Resources::GetUIImage("Endscreen_goldline_scaler.png", false, false), 0xe, false, false);
    g_goldline->screenSpace = true;
    Render::SetVisibility(g_goldline, false);
}

}  // namespace ChestOpenedWindow
