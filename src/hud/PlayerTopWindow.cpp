// PlayerTopWindow: the player panel at the top left (portrait, level, reputation/XP/HP bars).
// Port of PlayerTopWindow (libkingdom.so 5.11), 0x3274f4..0x32a2a4. Statics at 0x62a680.
#include "engine/IconManager.h"
#include "engine/Render.h"
#include "game/GameState.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/HUD.h"

namespace PlayerTopWindow {
namespace {

using GUI::Textfield;
using GUI::Window;

WindowManager::FunctionalWindow* g_queue = nullptr;
Window* g_root = nullptr;             // +0x58 "Player_pic"
GUI::MovementEffect* g_effect = nullptr;   // +0x74
int g_x = -1, g_y = -4;               // 0x60f33c = (int)-hud, +0x70 = (int)(hud * -4)
float g_rootScale = 1.f;              // 0x60f34c
bool g_tablet = false;                // 0x60f348
Window* g_levelButton = nullptr;
Textfield* g_level = nullptr;
Textfield* g_repBg = nullptr;
Textfield* g_xpBg = nullptr;
Textfield* g_hpBg = nullptr;
Window* g_repBar = nullptr;
Window* g_xpBar = nullptr;
Window* g_hpBar = nullptr;
Textfield* g_repText = nullptr;
Textfield* g_xpText = nullptr;
Textfield* g_hpText = nullptr;
GUI::ClipRect g_repClip, g_xpClip, g_hpClip;   // the bars are clipped to show progress
Window* g_maleGood = nullptr;
Window* g_femaleGood = nullptr;
Window* g_maleBad = nullptr;
Window* g_femaleBad = nullptr;
Window* g_dialogPopup = nullptr;
Window* g_hpRestore = nullptr;        // "Hp_hero_restore" (shown when the hero heals)
GUI::MovementEffect* g_hpRestoreEffect = nullptr;
Textfield* g_hpRestore1 = nullptr;
Textfield* g_hpRestore2 = nullptr;

// @0x329758. UNVERIFIED (later milestones): BuildingMovement/BuildingPlacement, the shop and the
// screenshot window make the panel only absorb clicks over it; the tutorial arrow on map 0x70, the
// heal window while the hero is hurt (+0x0c, the hero is at full health without entities) and
// the PlayerHintHoverWindow (+0x08) are not ported yet.
bool Click(int x, int y, bool pressed) { return g_root && g_root->Click(x, y, pressed, false); }

}  // namespace

WindowManager::FunctionalWindow* Queue() {
    if (!g_queue) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.setZ = SetZ;
        f.hide = Hide;
        f.click = Click;
        f.update = Update;
        f.show = Show;
        g_queue = new WindowManager::FunctionalWindow("PlayerTopWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_root && g_root->visibleSelf; }

// @0x329a74
void Init() {
    const float hud = GUI::GetHudScaleFactor();
    g_x = (int)-hud;
    Queue()->usesZRange = false;   // FunctionalWindow +0x08
    g_y = (int)(GUI::GetHudScaleFactor() * -4.f);
    g_tablet = GUI::IsTabletVersion();
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Player_pic.xml", "Player_pic.png",
                             GUI::GetHudScaleFactor(), 0, 0, 0, 0, false, 1.f);
    if (!g_root) return;
    g_rootScale = g_root->scale;
    g_root->SetVisibility(false);
    g_root->SetPosition(-g_root->w, 0);
    g_effect = GUI::CreateMovementEffect(g_root);

    // UNVERIFIED (milestone 2d/4): the click callbacks (level info, character window, heal) are
    // installed as no-ops until those windows exist.
    g_levelButton = GUI::GetWindowTyped<Window>(g_root, "button_player_box_level");
    g_levelButton->SetOnClick([] {});
    g_level = GUI::GetWindowTyped<Textfield>(g_root, "button_player_box_level.player_level");
    g_repBg = GUI::GetWindowTyped<Textfield>(g_root, "rep_bar_holder.text_bg");
    g_xpBg = GUI::GetWindowTyped<Textfield>(g_root, "xp_bar_holder.text_bg");
    g_hpBg = GUI::GetWindowTyped<Textfield>(g_root, "hp_bar_holder.text_bg");
    GUI::GetWindowTyped<Window>(g_root, "rep_bar_holder.icon_16")
        ->SetTexture(IconManager::GetIcon("pvp_valor_16", false), true);
    g_repBar = GUI::GetWindowTyped<Window>(g_root, "rep_bar_holder.player_rep_bar");
    GUI::GetWindowTyped<Window>(g_root, "rep_bar_holder")->SetOnClick([] {});
    g_xpBar = GUI::GetWindowTyped<Window>(g_root, "xp_bar_holder.player_xp_bar");
    GUI::GetWindowTyped<Window>(g_root, "xp_bar_holder")->SetOnClick([] {});
    g_hpBar = GUI::GetWindowTyped<Window>(g_root, "hp_bar_holder.player_hp_bar");
    GUI::GetWindowTyped<Window>(g_root, "hp_bar_holder")->SetOnClick([] {});
    g_repText = GUI::GetWindowTyped<Textfield>(g_root, "rep_bar_holder.text_rep");
    g_xpText = GUI::GetWindowTyped<Textfield>(g_root, "xp_bar_holder.text_xp");
    g_hpText = GUI::GetWindowTyped<Textfield>(g_root, "hp_bar_holder.text_hp");
    // Each bar and its text are clipped to a rectangle whose right edge Update moves with the value
    // (left = bar x, top = 0, right = bar x + w, bottom = 0x400 here).
    g_repClip = {g_repBar->x, 0, g_repBar->w + g_repBar->x, 0x400};
    g_xpClip = {g_xpBar->x, 0, g_xpBar->w + g_xpBar->x, 0x400};
    g_hpClip = {g_hpBar->x, 0, g_hpBar->w + g_hpBar->x, 0x400};
    g_repBar->SetClipRect(&g_repClip);
    g_repText->SetClipRect(&g_repClip);
    g_xpBar->SetClipRect(&g_xpClip);
    g_xpText->SetClipRect(&g_xpClip);
    g_hpBar->SetClipRect(&g_hpClip);
    g_hpText->SetClipRect(&g_hpClip);
    g_maleGood = GUI::GetWindowTyped<Window>(g_root, "button_player_box_picture.icon_char_damage_holder.locator_character_male_good");
    g_femaleGood = GUI::GetWindowTyped<Window>(g_root, "button_player_box_picture.icon_char_damage_holder.locator_character_female_good");
    g_maleBad = GUI::GetWindowTyped<Window>(g_root, "button_player_box_picture.icon_char_damage_holder.locator_character_male_bad");
    g_maleBad->SetVisibility(false);
    g_femaleBad = GUI::GetWindowTyped<Window>(g_root, "button_player_box_picture.icon_char_damage_holder.locator_character_female_bad");
    g_femaleBad->SetVisibility(false);
    g_dialogPopup = GUI::GetWindowTyped<Window>(g_root, "dialog_popup_player");
    g_dialogPopup->SetEnabled(false);
    GUI::GetWindowTyped<Window>(g_root, "hp_hero_restore");

    g_hpRestore = GUI::RegisterUI("../resource/kingdom_ui/1Original/Hp_hero_restore.xml", "Hp_hero_restore.png",
                                  GUI::GetHudScaleFactor(), 0, 0, 0, 0, false, 1.f);
    if (g_hpRestore) {
        g_hpRestore->SetVisibility(false);
        g_hpRestore->SetOnClick([] {});
        g_hpRestoreEffect = GUI::CreateMovementEffect(g_hpRestore, nullptr, nullptr, [] {});   // OnRestoreOut
        g_hpRestore1 = GUI::GetWindowTyped<Textfield>(g_hpRestore, "text_hp_1");
        g_hpRestore2 = GUI::GetWindowTyped<Textfield>(g_hpRestore, "text_hp_2");
    }
    // UNVERIFIED: the small-screen adjustments, the restore window's slide offsets
    // (0x60f34c * -28 / 25) and the PlayerHintHoverWindow are ported with the hero (milestone 4).
}

// @0x32954c: slide in from the left
void Show() {
    if (GameState::TutorialStep() <= 3 || !g_root) return;
    if (g_effect->active && g_effect->hideAtEnd) g_root->SetVisibility(false);
    Queue()->MoveWindowOnTop(true);
    if (!g_root->visibleSelf) g_effect->Animate(-g_root->w, g_y, g_x, g_y, true);
    g_root->SetVisibility(true);
}

// @0x327b98
void Hide() {
    if (g_root && g_root->visibleSelf) g_effect->Animate(g_x, g_y, -g_root->w, g_y, false);
}

// @0x32964c (the hint hover window's depth and position follow with milestone 4)
void SetZ(float) {
    if (!g_root) return;
    g_root->SetZ(HUDWindow::GetZ());
    if (g_hpRestore) g_hpRestore->SetZ(HUDWindow::GetZ() + 0.002f);
    if (g_root->visibleSelf) g_root->SetPosition(g_x, g_y);
}

// @0x327bf0: everything here reads the player entity (EntityManager::GetPlayer); without one the
// original returns at once, and so does the port until entities exist (milestone 4).
void Update(float) {}

}  // namespace PlayerTopWindow
