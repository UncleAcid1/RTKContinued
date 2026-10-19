// CampaignPerfectedWindow: "Campaign perfected!" once every quest of a campaign is done
// (Endscreen_campaign_100_percent.xml): the banner slides down over a black band, the squad
// cheers, "Campaign progress 100%", and the loot strip (FoundItemsWindow, the campaign's 100%
// task reward with the "campaign_complete_full_cb" crystals first) rises. Closing the strip slides
// the banner away, the compass blinks, and the reward task is completed (its reward dropped).
// Port of CampaignPerfectedWindow (libkingdom.so 5.11), 0x275ab0..0x276934. Statics at 0x619100.
//
// PORT (online removed): OnTweenIn's SaveManager::QueueSendSave.
#include "engine/Render.h"
#include "engine/Resources.h"
#include "game/AIState.h"
#include "game/BuildingHovers.h"
#include "game/Entity.h"
#include "game/EntityManager.h"
#include "game/GameState.h"
#include "game/Locations.h"
#include "game/Map.h"
#include "game/Rand48.h"
#include "game/Setting.h"
#include "game/Squad.h"
#include "game/StringTable.h"
#include "game/Tasks.h"
#include "engine/Text.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/HUD.h"
#include "windows/Windows.h"

namespace CampaignPerfectedWindow {
namespace {

using GUI::Textfield;
using GUI::Window;

WindowManager::FunctionalWindow* g_queue = nullptr;   // CampaignPerfectedWindow::wnd 0x61912c
uint32_t g_location = 0;              // 0x619100 the campaign (location id)
Tasks::TaskInfo* g_task = nullptr;    // 0x619104 the campaign's 100% task
Window* g_root = nullptr;             // 0x619108
GUI::MovementEffect* g_effect = nullptr;   // 0x61910c
int g_bandY = 0;                      // 0x619110 the banner's y (small screens: slid up)
Render::Sprite* g_black = nullptr;    // 0x619114 the black band (alpha 0.6)
Render::Sprite* g_goldline = nullptr; // 0x619118 Endscreen_goldline_scaler.png under the band
Textfield* g_text = nullptr;          // 0x61911c "text": "<campaign> perfected"
Textfield* g_text2 = nullptr;         // 0x619120 text_02 "CAMPAIGN_PROGRESS"
Textfield* g_text3 = nullptr;         // 0x619124 text_03 "100%"
Render::Texture* g_blackTex = nullptr;   // 0x619128
bool g_tablet = false;                // 0x60f158
float g_rootScale = 1.f;              // 0x60f15c

// @0x275cfc: the band settles; the game is saved (not in the city tutorial).
void OnTweenIn() {
    const float y = g_effect->hideAtEnd ? 0.f : g_black->h;
    Render::SetPosition(g_black, 0.f, y, 0.02f);
    Render::SetPosition(g_goldline, 0.f, g_black->y + 5.f, 0.0199f);
    // UNVERIFIED (milestone 6): SoundsManager::PlaySound("ui_campaign_completed", 1, false).
    if (!g_root->visibleSelf) return;
    if (GameState::IsCityTutorial()) return;
    Map::Save(0);
}

// @0x275dc0: the band follows the banner.
void OnTweenMove() {
    if (!g_effect->hideAtEnd) Render::SetPosition(g_black, 0.f, g_black->h * g_effect->t, 0.02f);
    else Render::SetPosition(g_black, 0.f, (1.f - g_effect->t) * g_black->h, 0.02f);
    Render::SetPosition(g_goldline, 0.f, g_black->y + 5.f, 0.0199f);
    Render::SortRenderLayer(0xe, 1);
}

// @0x275b94: the banner is gone; the 100% task is completed with its reward dropped.
void OnTweenOut() {
    g_root->SetVisibility(false);
    if (g_task) Tasks::CompleteTask(g_task->id, true, true, false);
    g_task = nullptr;
}

// @0x275e80
void UpdateContents() {
    if (Locations::LocationInfo* info = Locations::GetLocationInfo(g_location))
        g_text->SetText(SWPrintf(0x100, StringTable::GetString("CAMPAIGN_PERFECTED"), {info->name}).c_str());
    g_text2->SetText(StringTable::GetString("CAMPAIGN_PROGRESS"));
    g_text3->SetText(U"100%");
}

// @0x276144: the band's texture comes back after a context loss.
void Update(float) {
    if (!g_queue->shown) return;
    if (!g_black) return;
    if (g_blackTex && g_blackTex->glId != 0) return;
    g_blackTex = Render::CreateSolidFillTexture(0.f, 0.f, 0.f, 1.f);
    Render::SetTexture(g_black, g_blackTex);
}

// @0x275bf4: the band is 37% of the screen.
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

// @0x276614: the taps go to the loot strip.
bool Click(int x, int y, bool pressed) {
    FoundItemsWindow::Click(x, y, pressed);
    return g_queue->shown;
}

// @0x276630
void Deinit() {
    GUI::RemoveMovementEffect(g_effect);
    g_effect = nullptr;
    delete g_root;
    g_root = nullptr;
    g_black = Render::RemoveSprite(g_black);
    g_goldline = Render::RemoveSprite(g_goldline);
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
        g_queue = new WindowManager::FunctionalWindow("CampaignPerfectedWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_queue->shown; }   // @0x275ab0

// @0x275ac4: the task's reward is shown here, so it is not announced again nor completed by the
// quest checks (do_not_show_reward, no_complete).
void SetParameters(uint32_t location, Tasks::TaskInfo* task) {
    g_location = location;
    g_task = task;
    if (!task) return;
    task->noComplete = true;
    task->doNotShowReward = true;
}

// @0x2761c4 (exit): closed at once, the task completed.
void ForceHide() {
    if (!g_queue->shown) return;
    FoundItemsWindow::Hide();
    WindowManager::WindowHide(false);
    g_queue->shown = false;
    g_queue->MoveWindowDown(true);
    Render::SetVisibility(g_black, false);
    Render::SetVisibility(g_goldline, false);
    OnTweenOut();
}

// @0x27653c (FoundItemsWindow's close): the banner slides back up, the window leaves the queue at
// once and the compass blinks.
void Hide() {
    if (g_root->visibleSelf) {
        const int x = g_effect->GetCenteredX();
        g_effect->Animate(x, g_bandY, g_effect->GetCenteredX(), -g_root->h, false);
    }
    WindowManager::WindowHide(false);
    g_queue->shown = false;
    g_queue->MoveWindowDown(true);
    Render::SetVisibility(g_black, false);
    Render::SetVisibility(g_goldline, false);
    CastleTopWindow::BlinkCompass();
}

// @0x27622c: the drops are collected, the loot strip rises ("TASK_COMPLETE_DESC": the task's reward
// after the "campaign_complete_full_cb" crystals), the banner slides down, the squad cheers and
// the camera glides to the hero.
void Show() {
    if (g_queue->shown) return;
    BuildingHovers::CollectAll();
    FoundItemsWindow::SetParameters(g_task, StringTable::GetString("TASK_COMPLETE_DESC"),
                                    (unsigned)Setting("campaign_complete_full_cb").GetInt());
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

// @0x276694
void Init() {
    g_blackTex = nullptr;
    g_black = nullptr;
    g_goldline = nullptr;
    g_tablet = GUI::IsTabletVersion();
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Endscreen_campaign_100_percent.xml",
                             "Endscreen_campaign_100_percent.png", GUI::GetHoverScaleFactor(1.f, 1.f), 0, 0, 0, 0, false,
                             1.f);
    g_queue->RegisterTopWindow(g_root);
    g_rootScale = g_root->scale;
    g_root->SetVisibility(false);
    g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::GetVerticalCenter(g_root->h));
    int h = GUI::ScreenHeight();
    if (h < 0x300) {
        if (h < 600) h = 600;
        g_bandY = -(int)((float)(0x300 - h) * 0.25f);
    }
    g_effect = GUI::CreateMovementEffect(g_root, OnTweenMove, OnTweenIn, OnTweenOut);
    g_text = GUI::GetWindowTyped<Textfield>(g_root, "text");
    g_text2 = GUI::GetWindowTyped<Textfield>(g_root, "text_02");
    g_text3 = GUI::GetWindowTyped<Textfield>(g_root, "text_03");
    g_queue->fn.zRange = WindowManager::ReturnSmallZRange;
    g_queue->fn.show = Show;
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

}  // namespace CampaignPerfectedWindow
