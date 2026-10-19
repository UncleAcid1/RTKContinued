// CampaignCompleteWindow: "Campaign complete!" at a campaign's end (Endscreen_campaign_end.xml): the
// banner slides down over a black band, the squad cheers, and the loot strip (FoundItemsWindow,
// "You found", with the "campaign_complete_cb" crystals first) rises from the bottom. Closing the
// strip slides the banner away and the hero travels home on the world map; after the tutorial's
// campaign (location 0xb) "where to next" opens once the city has loaded.
// Port of CampaignCompleteWindow (libkingdom.so 5.11), 0x271db4..0x272b4c (Show is 0x272460..
// 0x272770: Ghidra cut it at 0x272598 into a bogus "_INIT_70"). Statics at 0x618a34.
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
#include "engine/Text.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "windows/Windows.h"

namespace CampaignCompleteWindow {
namespace {

using GUI::Textfield;
using GUI::Window;

WindowManager::FunctionalWindow* g_queue = nullptr;   // CampaignCompleteWindow::wnd 0x618a58
uint32_t g_location = 0;              // 0x618a34 the campaign (location id)
Window* g_root = nullptr;             // 0x618a38
GUI::MovementEffect* g_effect = nullptr;   // 0x618a3c
int g_bandY = 0;                      // 0x618a40 the banner's y (small screens: slid up)
Render::Sprite* g_black = nullptr;    // 0x618a44 the black band (alpha 0.6)
Render::Sprite* g_goldline = nullptr; // 0x618a48 Endscreen_goldline_scaler.png under the band
Textfield* g_text = nullptr;          // 0x618a4c text_01
Textfield* g_text2 = nullptr;         // 0x618a50 text_02 (the same text)
Render::Texture* g_blackTex = nullptr;   // 0x618a54
bool g_tablet = false;                // 0x60f144
float g_rootScale = 1.f;              // 0x60f148

// @0x271fe0: the band settles; the game is saved (not in the city tutorial).
void OnTweenIn() {
    const float y = g_effect->hideAtEnd ? 0.f : g_black->h;
    Render::SetPosition(g_black, 0.f, y, 0.02f);
    Render::SetPosition(g_goldline, 0.f, g_black->y + 5.f, 0.0199f);
    // UNVERIFIED (milestone 6): SoundsManager::PlaySound("ui_campaign_completed", 1, false).
    if (!g_root->visibleSelf) return;
    if (GameState::IsCityTutorial()) return;
    Map::Save(0);
}

// @0x2720a4: the band follows the banner.
void OnTweenMove() {
    if (!g_effect->hideAtEnd) Render::SetPosition(g_black, 0.f, g_black->h * g_effect->t, 0.02f);
    else Render::SetPosition(g_black, 0.f, (1.f - g_effect->t) * g_black->h, 0.02f);
    Render::SetPosition(g_goldline, 0.f, g_black->y + 5.f, 0.0199f);
    Render::SortRenderLayer(0xe, 1);
}

// @0x271e88: the banner is gone; the hero travels home on the world map.
void OnTweenOut() {
    g_root->SetVisibility(false);
    GlobalMapWindow::Show();
    GlobalMapWindow::CenterOnMap(0);
    GlobalMapWindow::SwitchMapAnimated(0, false, 500.f, false);
}

// @0x272164: "<campaign> completed" in both texts.
void UpdateContents() {
    Locations::LocationInfo* info = Locations::GetLocationInfo(g_location);
    if (!info) return;
    const std::u32string text = SWPrintf(0x100, StringTable::GetString("CAMPAIGN_COMPLETED"), {info->name});
    g_text->SetText(text.c_str());
    g_text2->SetText(text.c_str());
}

// @0x2723e0: the band's texture comes back after a context loss.
void Update(float) {
    if (!g_queue->shown) return;
    if (!g_black) return;
    if (g_blackTex && g_blackTex->glId != 0) return;
    g_blackTex = Render::CreateSolidFillTexture(0.f, 0.f, 0.f, 1.f);
    Render::SetTexture(g_black, g_blackTex);
}

// @0x271ed8: the band is 37% of the screen.
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

// @0x272844: the taps go to the loot strip.
bool Click(int x, int y, bool pressed) {
    FoundItemsWindow::Click(x, y, pressed);
    return g_queue->shown;
}

// @0x272860
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
        g_queue = new WindowManager::FunctionalWindow("CampaignCompleteWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_queue->shown; }                            // @0x271db4
void SetParameters(uint32_t location) { g_location = location; }      // @0x271dc8

// @0x272770 (FoundItemsWindow's close): the banner slides back up and the window leaves the queue
// at once; after the tutorial's campaign "where to next" opens with the next map.
void Hide() {
    if (g_root->visibleSelf) {
        const int x = g_effect->GetCenteredX();
        g_effect->Animate(x, g_bandY, g_effect->GetCenteredX(), -g_root->h, false);
    }
    WindowManager::WindowHide(false);
    g_queue->shown = false;
    g_queue->MoveWindowDown(true);
    if (g_location == 0xb) ChoosePathWindow::EnqueueShowOnLoad();
}

// @0x272460: the drops are collected, the loot strip rises (the campaign's finds after the
// "campaign_complete_cb" crystals), the banner slides down, the squad cheers and the camera glides
// to the hero.
void Show() {
    if (g_queue->shown) return;
    BuildingHovers::CollectAll();
    // PORT (DECIDED user 2026-10-18): a map holding a chest key gives it with a 1/4 chance.
    if (const uint32_t key = Map::RollHeldChestKey(4, false)) {
        GameState::AddItem(key, 1, false, false, false);
        GameState::AddMapItemCollectionInfo((uint32_t)GameState::GetCurrentMapID(), key, 1);
    }
    FoundItemsWindow::SetParameters(Locations::GetLocationInfo(g_location),
                                    StringTable::GetString("CAMPAIGN_YOU_FOUND"),
                                    (unsigned)Setting("campaign_complete_cb").GetInt());
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

// @0x2728c4
void Init() {
    g_blackTex = nullptr;
    g_black = nullptr;
    g_goldline = nullptr;
    g_tablet = GUI::IsTabletVersion();
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Endscreen_campaign_end.xml", "Endscreen_campaign_end.png",
                             GUI::GetHoverScaleFactor(1.f, 1.f), 0, 0, 0, 0, false, 1.f);
    g_queue->RegisterTopWindow(g_root);
    g_rootScale = g_root->scale;
    g_root->SetVisibility(false);
    g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::GetVerticalCenter(g_root->h));
    int h = GUI::ScreenHeight();
    if (h < 0x300) {
        if (h < 600) h = 600;
        g_bandY = -(int)((float)(0x300 - h) * 0.8f);
    }
    g_effect = GUI::CreateMovementEffect(g_root, OnTweenMove, OnTweenIn, OnTweenOut);
    g_text = GUI::GetWindowTyped<Textfield>(g_root, "text_01");
    g_text2 = GUI::GetWindowTyped<Textfield>(g_root, "text_02");
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

}  // namespace CampaignCompleteWindow
