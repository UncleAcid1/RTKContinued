// PlayerTopWindow: the player panel at the top left (portrait, level, reputation/XP/HP bars).
// Port of PlayerTopWindow (libkingdom.so 5.11), 0x3274f4..0x32a2a4. Statics at 0x62a680.
#include <cmath>
#include <vector>

#include "engine/IconManager.h"
#include "engine/Render.h"
#include "game/BuildingHovers.h"
#include "game/BuildingMovement.h"
#include "game/BuildingPlacement.h"
#include "game/Entity.h"
#include "game/EntityManager.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/MetaExpression.h"
#include "game/Setting.h"
#include "game/StringTable.h"
#include "windows/Windows.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/HUD.h"

namespace PlayerTopWindow {
namespace {

using GUI::Button;
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
int g_restoreHideX = 0, g_restoreHideY = 0;   // 0x62a714 0x62a718 the popup tucked under the panel
int g_restoreShowX = 0, g_restoreShowY = 0;   // 0x62a71c 0x62a720
bool g_legend = false;                // 0x62a680 the bars show their names instead of numbers
float g_lastRep = 0.f, g_lastXp = 0.f, g_lastHp = 0.f;   // 0x62a684 0x62a730 0x62a690 the fractions shown
int g_lastOverlimit = 0;              // 0x62a74c
float g_badTime = 0.f;                // 0x62a6ec the hurt portrait's flash
bool g_cityKnown = false;             // 0x62a6f8
bool g_wasPlayerCity = true;          // 0x60f340
float g_smoothTime = 0.f;             // 0x60f344
float g_rep = 0.f, g_xp = 0.f, g_hp = 0.f;   // 0x62a6fc 0x62a700 0x62a704 the values the bars glide to
// Items on their way into the portrait (RaiseAndMoveIntoPlayer): a potion works when it arrives.
struct Incoming { uint32_t item; float time; };
std::vector<Incoming> g_incoming;     // 0x62a6c4
float g_savedZ = 0.f;                 // 0x62a6dc the panel's depth before it was raised

float Clamp01(float f) { return f >= 1.f ? 1.f : (f < 0.f ? 0.f : f); }

void SetBoth(Textfield* a, Textfield* b, const char32_t* text) {
    a->SetText(text);
    b->SetText(text);
}

// @0x3274f4
void OnToggleLegend() {
    g_legend = !g_legend;
    if (!g_legend) g_lastRep = 0.f;
}

// @0x327adc: the heal window, for the bar and the "+1 HP" popup.
void OnHealth() {
    if (GameState::IsTutorial() || GameState::IsCityTutorial() || !GameState::IsPlayerCity()) return;
    Entity* player = EntityManager::GetPlayer();
    if (!player || player->IsActuallyDying()) return;
    HealSquadWindow::Show();
    HealSquadWindow::ResetRecentDeaths();
}

// @0x327b30: the portrait. "character_icon_opens_heal" 1: the heal window, else the character's.
void OnCharacter() {
    if (Setting("character_icon_opens_heal").GetInt() == 1) {
        Entity* player = EntityManager::GetPlayer();
        if (player && !player->IsActuallyDying()) {
            HealSquadWindow::Show();
            HealSquadWindow::ResetRecentDeaths();
        }
        return;
    }
    // UNVERIFIED (milestone 5): CharacterInfoWindow::Show.
}

// @0x327538
void OnRestoreOut() { g_hpRestore->SetVisibility(false); }

// @0x329758: over an edit mode or the shop the panel only swallows presses on it.
bool Click(int x, int y, bool pressed) {
    if (!g_root) return false;
    if (BuildingMovement::Activated() || BuildingPlacement::Activated() || ShopWindow::IsVisible()) {
        // (BottomScreenshotWindow, not ported, is never visible)
        bool over = g_root->GetWindowAtPosition(x, y, false) != nullptr;
        if ((BuildingMovement::Activated() || BuildingPlacement::Activated()) && !pressed) over = false;
        return over;
    }
    // UNVERIFIED (4g, PlayerHintHoverWindow): while the hero's dialog shows on map 0x70 a tap on
    // the helper arrow comes first, and the dialog gets the taps the panel does not take.
    Entity* player = EntityManager::GetPlayer();
    if (player && player->GetHP() != player->GetHpMax() && g_hpRestore && g_hpRestore->Click(x, y, pressed, false))
        return true;
    return g_root->Click(x, y, pressed, false);
}

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

    g_levelButton = GUI::GetWindowTyped<Window>(g_root, "button_player_box_level");
    g_levelButton->SetOnClick([] {});   // UNVERIFIED (milestone 5): LevelInfoWindow::Show
    g_level = GUI::GetWindowTyped<Textfield>(g_root, "button_player_box_level.player_level");
    g_repBg = GUI::GetWindowTyped<Textfield>(g_root, "rep_bar_holder.text_bg");
    g_xpBg = GUI::GetWindowTyped<Textfield>(g_root, "xp_bar_holder.text_bg");
    g_hpBg = GUI::GetWindowTyped<Textfield>(g_root, "hp_bar_holder.text_bg");
    GUI::GetWindowTyped<Window>(g_root, "rep_bar_holder.icon_16")
        ->SetTexture(IconManager::GetIcon("pvp_valor_16", false), true);
    g_repBar = GUI::GetWindowTyped<Window>(g_root, "rep_bar_holder.player_rep_bar");
    GUI::GetWindowTyped<Window>(g_root, "rep_bar_holder")->SetOnClick(OnToggleLegend);
    g_xpBar = GUI::GetWindowTyped<Window>(g_root, "xp_bar_holder.player_xp_bar");
    GUI::GetWindowTyped<Window>(g_root, "xp_bar_holder")->SetOnClick(OnToggleLegend);
    g_hpBar = GUI::GetWindowTyped<Window>(g_root, "hp_bar_holder.player_hp_bar");
    GUI::GetWindowTyped<Window>(g_root, "hp_bar_holder")->SetOnClick(OnHealth);
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
    // The portrait (@0x3299b4: GetWindowTyped<Button> of its clickArea, falling back to GUI::Init's
    // template windows).
    GUI::GetWindowTyped<Button>(g_root, "button_player_box_picture.clickArea")->SetOnClick(OnCharacter);
    GUI::GetWindowTyped<Button>(g_root, "button_player_box_picture.clickArea")->SetActionSound("", 1);
    g_maleBad = GUI::GetWindowTyped<Window>(g_root, "button_player_box_picture.icon_char_damage_holder.locator_character_male_bad");
    g_maleBad->SetVisibility(false);
    g_femaleBad = GUI::GetWindowTyped<Window>(g_root, "button_player_box_picture.icon_char_damage_holder.locator_character_female_bad");
    g_femaleBad->SetVisibility(false);
    g_dialogPopup = GUI::GetWindowTyped<Window>(g_root, "dialog_popup_player");
    g_dialogPopup->SetEnabled(false);
    Window* restoreAt = GUI::GetWindowTyped<Window>(g_root, "hp_hero_restore");

    g_hpRestore = GUI::RegisterUI("../resource/kingdom_ui/1Original/Hp_hero_restore.xml", "Hp_hero_restore.png",
                                  GUI::GetHudScaleFactor(), 0, 0, 0, 0, false, 1.f);
    if (g_hpRestore) {
        g_hpRestore->SetVisibility(false);
        g_hpRestore->SetOnClick(OnHealth);
        g_hpRestoreEffect = GUI::CreateMovementEffect(g_hpRestore, nullptr, nullptr, OnRestoreOut);
        g_hpRestore1 = GUI::GetWindowTyped<Textfield>(g_hpRestore, "text_hp_1");
        g_hpRestore2 = GUI::GetWindowTyped<Textfield>(g_hpRestore, "text_hp_2");
    }
    // The popup slides out from under the panel: 28 units above its place, then 25 down.
    g_restoreHideX = g_restoreShowX = restoreAt->x;
    g_restoreHideY = (int)(g_rootScale * -28.f) + restoreAt->y;
    g_restoreShowY = g_restoreHideY + (int)(g_rootScale * 25.f);
    // UNVERIFIED: the small-screen adjustments of text_hp_2. The PlayerHintHoverWindow (the hero's
    // own dialog) is created here: 4g's last HUD part.
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

// @0x327814: the hurt portrait flashes in.
void OnDamage(Entity* e) {
    if (e != EntityManager::GetPlayer() || !g_root) return;
    (GameState::IsMalePlayer() ? g_maleBad : g_femaleBad)->SetVisibility(true);
    g_badTime = 0.f;
}

void RefreshHP() { g_lastHp = 0.f; }   // @0x327564 the HP text is written again

// @0x3276c4: `sprite` (an item's icon) flies into the portrait; item `item` takes effect when it
// gets there. The panel is raised to depth z while anything is on its way.
void RaiseAndMoveIntoPlayer(float z, Render::Sprite* sprite, uint32_t item) {
    if (g_incoming.empty()) {
        g_savedZ = g_root->z;
        g_root->SetZ(z);
        Render::SortRenderLayer(0xe, 1);
    }
    g_incoming.push_back({item, 0.f});
    BuildingHovers::AddItemMovement(sprite, 0, 0, true, 1.f, -1, -1, false, true);
    Render::RemoveSprite(sprite);
}

// @0x327bf0
void Update(float dt) {
    Entity* player = GameState::IsPlayerCity() ? EntityManager::GetPlayer() : GameState::GetFriendPlayer();
    if (!player || !g_root) return;

    // The hurt portrait: springs in over 3/8 s, fades over the next 1/8 s (it stays while he is dead).
    Window* bad = GameState::IsMalePlayer() ? g_maleBad : g_femaleBad;
    if (bad->visibleSelf) {
        g_badTime += dt + dt;
        if (g_badTime < 0.75f) {
            float f = g_badTime / 0.75f - 1.f;
            bad->SetAlpha(1.f + f * f * (1.70158f + f * 2.70158f) + 0.f, false);
        } else if (g_badTime < 1.f && player->GetHP() != 0) {
            bad->SetAlpha(1.f - (g_badTime - 0.75f) * 4.f, false);
        } else if (player->GetHP() != 0) {
            bad->SetVisibility(false);
        }
    }

    // The bars glide to the real values (30 steps a second, 13.4% of the way each).
    // UNVERIFIED (milestone 5, Professions): the valor bar's value is profession 7's points
    // (GameState::GetProfessionPoints) between its level thresholds; it stays empty.
    if (g_wasPlayerCity != GameState::IsPlayerCity() || !g_cityKnown) {
        g_wasPlayerCity = GameState::IsPlayerCity();
        g_rep = 0.f;
        g_xp = (float)(int)GameState::GetResourceAmount(10);
        g_hp = (float)player->GetHP();
    }
    g_cityKnown = true;
    g_smoothTime += dt;
    if (g_smoothTime > 0.0333333f) {
        g_smoothTime = 0.f;
        g_xp = g_xp - ((float)(int)GameState::GetResourceAmount(10) - g_xp) * -0.133975f;
        float hp = 0.f;
        if (!player->IsActuallyDying()) hp = (float)(GameState::IsPlayerCity() ? player->GetHP() : player->GetHpMax());
        g_hp = g_hp - (hp - g_hp) * -0.133975f;
    }
    g_maleGood->SetVisibility(GameState::IsMalePlayer());
    g_femaleGood->SetVisibility(!GameState::IsMalePlayer());

    // The "+1 HP in ..." popup while he is hurt.
    int hpNow = player->GetHP();
    if (!GameState::IsPvPCombatActive()) hpNow += player->GetHpOverlimit();
    if (!GameState::IsPlayerCity()) hpNow = player->GetHpMax();
    if (hpNow < player->GetHpMax() && !GameState::IsTutorial() && !GameState::IsArenaCombatActive() &&
        GameState::GetCurrentMapID() != 0x70) {
        if (!g_hpRestore->visibleSelf) {
            g_hpRestore->SetVisibility(true);
            g_hpRestoreEffect->Animate(g_restoreHideX, g_restoreHideY, g_restoreShowX, g_restoreShowY, true);
        } else {
            g_hpRestore1->SetText(U"+");
            g_hpRestore2->SetText(StringTable::GetTimeString(GameState::GetTimeTillNextHP(), false).c_str());
        }
    } else if (g_hpRestore->visibleSelf && !g_hpRestoreEffect->active) {
        g_hpRestoreEffect->Animate(g_restoreShowX, g_restoreShowY, g_restoreHideX, g_restoreHideY, false);
    }

    g_level->SetText(SWPrintf(0x100, U"%d", {GameState::GetLevel()}).c_str());
    float fRep = 0.f;
    unsigned level = (unsigned)GameState::GetLevel();
    float fXp = Clamp01((g_xp - (float)GameState::GetLevelXP(level - 1)) /
                        (float)(GameState::GetLevelXP(level) - GameState::GetLevelXP(level - 1)));
    float fHp = Clamp01(g_hp / (float)player->GetHpMax());
    if (g_lastRep != fRep || g_lastXp != fXp || g_lastHp != fHp) g_legend = false;
    bool write = true;
    if (!GameState::IsCombatActive()) {
        if (g_legend) {
            SetBoth(g_repBg, g_repText, StringTable::GetString("HINT_VALOR"));
            SetBoth(g_xpBg, g_xpText, StringTable::GetString("HINT_XP"));
            SetBoth(g_hpBg, g_hpText, StringTable::GetString("HINT_HP"));
            write = false;
        }
    } else {
        g_lastRep = 0.f;
        g_legend = false;
    }
    if (write) {
        if (g_lastRep != fRep || g_lastXp != fXp || g_lastHp != fHp || player->GetHpOverlimit() != g_lastOverlimit) {
            SetBoth(g_repBg, g_repText, SWPrintf(0x100, U"%d/%d", {(int)(g_rep + 0.5f), 0}).c_str());
            g_repClip.right = (int)((float)g_repClip.left + (float)g_repBar->w * fRep);
            SetBoth(g_xpBg, g_xpText,
                    SWPrintf(0x100, U"%d/%d", {(int)(g_xp + 0.5f), (int)GameState::GetLevelXP(level)}).c_str());
            g_xpClip.right = (int)((float)g_xpClip.left + (float)g_xpBar->w * fXp);
            std::u32string hp =
                player->GetHpOverlimit() == 0
                    ? SWPrintf(0x100, U"%d/%d", {(int)(g_hp + 0.5f), player->GetHpMax()})
                    : SWPrintf(0x100, U"%d/%d + %d", {(int)(g_hp + 0.5f), player->GetHpMax(), player->GetHpOverlimit()});
            SetBoth(g_hpBg, g_hpText, hp.c_str());
            g_hpClip.right = (int)((float)g_hpClip.left + (float)g_hpBar->w * fHp);
            g_root->UpdatePosition();
        }
        g_lastRep = fRep;
        g_lastXp = fXp;
        g_lastHp = fHp;
        g_lastOverlimit = player->GetHpOverlimit();
    }

    // Items flying into the portrait: after a second the item works (a potion is drunk), after
    // two its entry goes; the panel returns to its depth with the last.
    float glow = 1.f;
    for (size_t i = 0; i < g_incoming.size();) {
        Incoming& in = g_incoming[i];
        in.time += dt;
        if (in.time >= 1.f && in.time < 2.f) {
            if (Items::ItemInfo* info = Items::GetItemInfo(in.item)) {
                // SoundsManager::PlaySound("drink_potion"): milestone 6.
                Entity::SetupPlayerHPOverlimit(true, true);
                if (info->meta) info->meta->OnApplyItemEffect(EntityManager::GetPlayer());
                Entity::SetupPlayerHPOverlimit(false, true);
                in.item = 0;
                // A healing item (HP, 0x19) makes the HP bar flare.
                if (info->meta && info->meta->FindChildData(0x19)) {
                    float f = 1.f + std::sin((in.time - 1.f) * 3.14159f) * 0.3f;
                    if (glow < f) glow = f;
                }
            }
        }
        if (in.time < 2.f) {
            ++i;
            continue;
        }
        g_incoming[i] = g_incoming.back();
        g_incoming.pop_back();
        if (g_incoming.empty()) {
            g_root->SetZ(g_savedZ);
            Render::SortRenderLayer(0xe, 1);
        }
    }
    g_hpBar->SetAlpha(glow, true);
}

}  // namespace PlayerTopWindow
