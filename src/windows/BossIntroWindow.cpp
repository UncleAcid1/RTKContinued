// BossIntroWindow: the banner before a boss, an arena wave or the tutorial's ambush
// (Endscreen_boss_start.xml), fading in and out; "Tap to continue" closes it. With a reputation
// set (the arena's defeat screens) it slides down over a black band with the reputation bar.
// Port of BossIntroWindow (libkingdom.so 5.11), 0x251770..0x2539f0. Statics at 0x617ea8.
//
// The reputation screens (SetReputation, from ArenaCombat: an arena event lost) show the event's
// loot (FoundItemsWindow) and the reputation bar over a black band, and end the event.
// UNVERIFIED (milestone 6): their sound "arena_lose" and SoundsManager::StopMusic.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "engine/Render.h"
#include "engine/Resources.h"
#include "engine/Timer.h"
#include "game/Combat.h"
#include "game/Entity.h"
#include "game/EntityManager.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/Map.h"
#include "game/Professions.h"
#include "game/PvP.h"
#include "game/Rand48.h"
#include "game/Setting.h"
#include "game/AIState.h"
#include "game/Squad.h"
#include "game/StringTable.h"
#include "game/Tasks.h"
#include "engine/Text.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/HUD.h"
#include "hud/Shared.h"
#include "windows/Windows.h"

namespace BossIntroWindow {
namespace {

using GUI::Textfield;
using GUI::Window;

WindowManager::FunctionalWindow* g_queue = nullptr;   // BossIntroWindow::wnd 0x617f2c
Professions::Profession* g_reputation = nullptr;   // 0x617ea8 the reputation screen's (null: the banner)
bool g_ambush = false;                // 0x617eac
Textfield* g_text3 = nullptr;         // 0x617eb0 the name in an ambush
Textfield* g_text2 = nullptr;         // 0x617eb4 the boss's name
int g_wave = 0, g_totalWaves = 0;     // 0x617eb8 0x617ebc
Shared::EndscreenProgressBar g_bar;   // 0x617ec0 the reputation bar
Window* g_root = nullptr;             // 0x617ef0
Render::Sprite* g_black = nullptr;    // 0x617ef4 the black band (reputation screens)
Render::Sprite* g_goldline = nullptr; // 0x617ef8 Endscreen_goldline_scaler.png under the band
GUI::MovementEffect* g_effect = nullptr;   // 0x617efc
Textfield* g_text = nullptr;          // 0x617f00 the fight's kind ("Boss fight", the arena wave)
Textfield* g_tap = nullptr;           // 0x617f04 text_tap_to_continue
Window* g_tapCopy = nullptr;          // 0x617f08 (a copy of it, detached)
bool g_fading = false;                // 0x617f0c
float g_fadeT = 0.f;                  // 0x617f10
Render::Texture* g_blackTex = nullptr;   // 0x617f14
float g_shownTime = 0.f;              // 0x617f18 (while no dialog is over it)
float g_pulse = 0.f;                  // 0x617f1c "Tap to continue" pulses
uint32_t g_startPoints = 0;           // 0x617f20 the reputation's points before the event's loot
uint32_t g_endPoints = 0;             // 0x617f24 and with it
int g_bandY = 0;                      // 0x617f28 small screens: the slid-down window's y
bool g_fadeIn = false;                // 0x60f0dc
float g_rootScale = 1.f;              // 0x60f0e0
bool g_tablet = false;                // 0x60f0e4

// @0x253410: Back closes it after a second.
bool OnBack() {
    std::puts("BossIntroWindow::OnBack");
    if (g_queue->shown && 1.f < g_fadeT) {
        Hide();
        return true;
    }
    return false;
}

// @0x2518f8 (the slide out ends). After a reputation screen the arena's event ends (reputation 1:
// event 2's times, task 0x5b9 reset, the combat's PostCombat; reputation 2: event 1's times, tasks
// 0x5b1..0x5b4, 0x5b7 reset, the combat ended) and the hero returns to GameState's return map.
void OnTweenOut() {
    g_root->SetVisibility(false);
    Render::SetVisibility(g_black, false);
    Render::SetVisibility(g_goldline, false);
    if (g_reputation) {
        uint32_t id = g_reputation->id;
        if (id == 1) {
            GameState::SetEventStartTime(2, 0);
            GameState::SetEventStartTime(0x400, 0);
            GameState::SetEventEndTime(2, (uint32_t)Timer::GetGlobalTime());
            if (Tasks::TaskInfo* t = Tasks::GetTask(0x5b9)) t->dnShow = t->dnShowDefault;
            GameState::ResetTask(0x5b9);
            if (BaseCombat* c = GameState::GetActiveCombat()) c->PostCombat();
            id = g_reputation->id;
        }
        if (id == 2) {
            GameState::SetEventStartTime(1, 0);
            GameState::SetEventEndTime(1, (uint32_t)Timer::GetGlobalTime());
            if (Tasks::TaskInfo* t = Tasks::GetTask(0x5b1)) t->dnShow = t->dnShowDefault;
            GameState::ResetTask(0x5b1);
            GameState::SetTaskBeginTime(0x5b1, 1);
            GameState::ResetTask(0x5b2);
            GameState::ResetTask(0x5b3);
            GameState::ResetTask(0x5b4);
            GameState::ResetTask(0x5b7);
            if (GameState::IsCombatActive()) GameState::EndActiveCombat(true);
        }
        Map::EnqueueLoadMap((int)GameState::mReturnMapID, true, false, 0.f);
    }
    if (GameState::GetCurrentMapID() != 0 && GameState::GetCurrentMapID() != 0xb) BottomBattleWindow::Show();
    g_reputation = nullptr;
}

// @0x251ab0
void OnTweenIn() {
    const float y = g_effect->hideAtEnd ? 0.f : g_black->h;
    Render::SetPosition(g_black, 0.f, y, 0.02f);
    Render::SetPosition(g_goldline, 0.f, g_black->y + 5.f, 0.0199f);
}

// @0x251b34: the band follows the window.
void OnTweenMove() {
    if (!g_effect->hideAtEnd) Render::SetPosition(g_black, 0.f, g_black->h * g_effect->t, 0.02f);
    else Render::SetPosition(g_black, 0.f, (1.f - g_effect->t) * g_black->h, 0.02f);
    Render::SetPosition(g_goldline, 0.f, g_black->y + 5.f, 0.0199f);
    Render::SortRenderLayer(0xe, 1);
}

// @0x251c10: the fight's kind: a boss ("boss_fight_skelet", the PvP arena "MAP_ARENA") during a
// fight that is not an arena one, else the arena wave ("<difficulty> <wave>/<waves>").
void UpdateContents() {
    BaseCombat* c = GameState::GetActiveCombat();
    if (c && c->GetType() != 5 && c->GetType() != 6) {
        g_text->SetText(StringTable::GetString(GameState::GetCurrentMapID() == 0x70 ? "MAP_ARENA" : "boss_fight_skelet"));
    } else {
        const char32_t* diff = StringTable::GetStringF("ARENA_WAVE_DIFF_%d", (int)GameState::GetEventDifficulty(2) + 1);
        g_text->SetText(SWPrintf(0x80, U"%s %d/%d", {diff, g_wave, g_totalWaves}).c_str());
    }
    if (g_reputation) g_text3->SetText(StringTable::GetString("ARENA_WAVE_DEFEAT"));
    g_tap->SetVisibility(g_reputation == nullptr);
    g_tap->SetText(StringTable::GetString("TAP_TO_CONTINUE"));
    g_tapCopy->SetVisibility(g_reputation == nullptr);
    static_cast<Textfield*>(g_tapCopy)->SetText(StringTable::GetString("TAP_TO_CONTINUE"));
    g_bar.root->SetVisibility(g_reputation != nullptr);
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
        g_queue = new WindowManager::FunctionalWindow("BossIntroWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_queue->shown; }                                     // @0x251770
bool IsBlackVisible() { return IsVisible() && g_reputation != nullptr; }        // @0x251784

// @0x2517ac
void SetBossName(const char32_t* name) { (g_ambush ? g_text3 : g_text2)->SetText(name); }

void SetWave(int wave, int total) {   // @0x2517ec
    g_totalWaves = total;
    g_wave = wave;
}
int GetTotalWaves() { return g_totalWaves; }   // @0x251804
void SetAmbush() { g_ambush = true; }          // @0x251818
void SetReputation(uint32_t id) { g_reputation = Professions::GetProfession(id); }   // @0x251bf4

// @0x252028: the fade (in: 1 s; out: 1 s, then the ambush's fight starts), "Tap to continue"
// pulsing, the texts by kind.
void Update(float dt) {
    if (g_fading) {
        g_fadeT += dt;
        if (g_fadeIn) {
            if (!(g_fadeT < 1.f)) {
                g_root->SetAlpha(1.f, true);
                g_fading = false;
                return;
            }
            g_root->SetAlpha(g_fadeT, true);
        } else if (g_fadeT < 1.f) {
            g_root->SetAlpha(1.f - g_fadeT, true);
        } else {
            if (g_ambush) {
                // The tutorial's ambush: the rival (PvP player -1) is of the other gender, two
                // levels above the hero, with random looks (customisations of type 0x11) for slots 2..6, 8, 9
                // (not hair 0x2ef / 0x2f0 in slot 5); an aggro fight with her hero turns into the
                // PvP arena fight on map 0x91.
                PlayerCompetitionWindow::SetSelectedPlayerGID(-1, false);
                PlayerCompetitionWindow::SetReturnWithoutWindow(true);
                if (PvP::PlayerInfo* rival = PvP::GetPlayerInfoByGID(-1, 0xff)) {
                    rival->male = !GameState::IsMalePlayer();
                    rival->level = GameState::GetLevel() + 2;
                    uint32_t seed = (uint32_t)Timer::GetGlobalTime();
                    for (unsigned slot = 1; slot <= 9; ++slot) {
                        if (slot == 7 || slot == 1) continue;
                        std::vector<Items::ItemInfo*> looks;
                        for (unsigned i = 0; Items::ItemInfo* it = Items::EnumItemInfo(i); ++i) {
                            if (it->type != 0x11 || it->slot != slot) continue;
                            const bool maleLook = std::memcmp(it->name, "male_", 5) == 0;
                            const bool femaleLook = std::memcmp(it->name, "female_", 7) == 0;
                            if (maleLook && !rival->male) continue;
                            if (femaleLook && rival->male) continue;
                            looks.push_back(it);
                        }
                        rival->equipment.customs[slot].id = 0;
                        if (looks.empty()) continue;
                        seed = seed * 0x343fd + 0x269ec3;
                        const unsigned pick = ((seed & 0x7fffffff) >> 16) % (unsigned)(looks.size() + 1);
                        if (slot == 5 && pick < looks.size() && (looks[pick]->id == 0x2ef || looks[pick]->id == 0x2f0))
                            continue;
                        if (pick != looks.size()) rival->equipment.customs[slot].id = looks[pick]->id;
                    }
                    if (Entity* hero = EntityManager::CreateEntity(rival->male ? 0xc : 0xd, true, true)) {
                        GameState::SetPvPTutorial(true);
                        GameState::SetCombat(new AggroCombat());
                        hero->CreateSquad();
                        hero->GetAI()->UpdateLastTarget();
                        hero->player = false;
                        GameState::GetActiveCombat()->BeginCombat(EntityManager::GetPlayer()->GetSquad(), hero->GetSquad());
                        GameState::GetActiveCombat()->ForceBossFight(6, 0x91);
                        GameState::DropGamePauseState();
                    }
                }
                g_ambush = false;
            }
            g_root->SetAlpha(1.f, true);
            g_root->SetVisibility(false);
            g_fading = false;
            if (GameState::GetCurrentMapID() == 0x70 && !GameState::TaskCompleted(0x5d0)) {
                PlayerTopWindow::SetParameters(StringTable::GetString("DESC1488"), 0x5d0, true, false);
                PlayerTopWindow::Show();
                PlayerTopWindow::ShowArrowAndLock();
            }
            return;
        }
    }
    if (!g_queue->shown) return;
    if (!g_black) return;
    if (!g_blackTex || g_blackTex->glId == 0) {
        g_blackTex = Render::CreateSolidFillTexture(0.f, 0.f, 0.f, 1.f);
        Render::SetTexture(g_black, g_blackTex);
    }
    if (WindowManager::GetShownWindowCount() < 2) g_shownTime += dt;
    g_pulse += dt;
    g_tap->SetAlpha(std::fabs(cosf(g_pulse * 3.f) * 0.7f), false);
    if (g_reputation) {
        // The reputation bar fills in 2 s towards the next rank: "gained x / y", or "next rank".
        float t = g_shownTime * 0.5f;
        t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
        const uint32_t level = Professions::GetPlayerProfessionLevel(g_reputation->id);
        Professions::ProfessionLevel* next = Professions::GetProfessionLevel(g_reputation->id, level + 1);
        if (!next) next = Professions::GetProfessionLevel(g_reputation->id, level);
        if (!next) return;
        const float from = (float)g_startPoints;
        const float value = from + t * ((float)std::min(next->points, g_endPoints) - from);
        Professions::ProfessionLevel* cur = Professions::GetProfessionLevel(g_reputation->id, level);
        const int shown = (int)(value + 0.5f);
        const int prev = cur ? (int)cur->points : 0;   // (the original reads an unset value without one)
        std::u32string text;
        if ((int)next->points <= shown)
            text = SWPrintf(0x100, StringTable::GetString("ARENA_WAVE_GAINED_NEXT"), {g_reputation->title});
        else
            text = SWPrintf(0x100, StringTable::GetString("ARENA_WAVE_GAINED"),
                            {g_reputation->title60, ToWideString(shown), ToWideString((int)next->points)});
        g_bar.textUnder->SetText(text.c_str());
        g_bar.textOver->SetText(text.c_str());
        if (prev == (int)next->points) return;
        g_bar.SetProgress((value - (float)(uint32_t)prev) / (float)(uint32_t)((int)next->points - prev));
        g_root->UpdatePosition();
    }
    g_text->SetVisibility(!g_ambush);
    g_text2->SetVisibility(!g_ambush);
    g_text3->SetVisibility(g_ambush);
}

// @0x252c3c: in an arena fight the squad cheers; the banner fades in at the screen's centre.
// A reputation screen: the event's loot (with the defeat's consolation: event 2's lost wave items
// and part of the gold spent, event 1's boss items) in FoundItemsWindow, the reputation's points
// before / after it, and the band slides in.
void Show() {
    if (g_queue->shown) return;
    g_queue->shown = true;
    if (ArenaTurnWindow::IsVisible()) ArenaTurnWindow::Hide();
    if (g_reputation) {
        g_startPoints = g_endPoints = GameState::GetProfessionPoints(g_reputation->id);
        uint32_t id = g_reputation->id;
        if (id == 1) {
            const unsigned diff = GameState::GetEventDifficulty(2);
            const int base = Setting("wave_lost_base").GetChild(diff).GetInt();
            const int perWave = Setting("wave_lost_per_wave").GetChild(diff).GetInt();
            if (GameState::GetEventCompletionCount(2) != 0) {
                GameState::AddMapItemCollectionInfo(0x70, 0x1ba, (g_wave - 1) * perWave + base);
                if (BaseCombat* c = GameState::GetActiveCombat()) {
                    const float back = std::min(Setting("arena_dropback_defeat").GetFloat(), 1.f);
                    GameState::AddMapResourceCollectionInfo(0x70, GameState::kGold, (int)((float)c->GetSpentGold() * back));
                }
            }
            id = g_reputation->id;
        }
        if (id == 2 && GameState::GetEventCompletionCount(1) != 0)
            GameState::AddMapItemCollectionInfo(0x70, 0x1bb, Setting("arena_boss_dropback").GetInt());
        FoundItemsWindow::ClearCollected();
        for (int t = 0; t < 11; ++t)
            if (int n = GameState::GetMapResourceCollectionInfo((uint32_t)GameState::GetCurrentMapID(), t))
                FoundItemsWindow::AddCollectedResource(t, n);
        for (unsigned i = 0; Items::ItemInfo* info = Items::EnumItemInfo(i); ++i) {
            const int n = GameState::GetMapItemCollectionInfo((uint32_t)GameState::GetCurrentMapID(), info->id);
            if (n == 0 || info->id == 0x2000) continue;
            FoundItemsWindow::AddCollectedItem(info->id, n);
            if (Professions::GetProfessionForPointItem(info->id) == g_reputation) g_endPoints += (uint32_t)n;
        }
        FoundItemsWindow::Show();
        FoundItemsWindow::SetHeader(StringTable::GetString("ARENA_WAVE_DEFEAT_HEADER"));
        // UNVERIFIED (milestone 6): SoundsManager::PlaySound("arena_lose", 1, false), StopMusic.
        if (Entity* player = EntityManager::GetPlayer())
            Render::CenterOn(player->worldX, player->worldY - player->GetIdleHeight(), true, 0.4f, 0, false);
    }
    if (GameState::IsArenaCombatActive() && g_reputation == nullptr &&
        (GameState::GetEventCompletionCount(2) == 0 || g_wave != 1)) {
        Entity* player = EntityManager::GetPlayer();
        if (player)
            if (BaseSquad* squad = player->GetSquad())
                for (unsigned i = 0; i < (unsigned)squad->GetSoldierCount(); ++i) {
                    Entity* e = squad->GetSoldier(i);
                    if (!e) continue;
                    e->SetDirection((Rand48::lrand48() & 1) == 0 ? 1 : 7);
                    e->SetAnimationP(e->player ? "bat_vict" : "victory", true, false, false);
                }
    }
    g_queue->MoveWindowOnTop(true);
    WindowManager::WindowShow(false);
    if (!g_reputation) {
        g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::GetVerticalCenter(g_root->h));
    } else {
        // The banner slides down over the black band.
        g_effect->Animate(g_effect->GetCenteredX(), -g_root->h, g_effect->GetCenteredX(), g_bandY, true);
        g_root->SetPosition(g_effect->GetCenteredX(), -g_root->h);
        g_root->SetVisibility(true);
        Render::SetVisibility(g_black, true);
        g_black->w = (float)GUI::ScreenWidth();
        g_black->h = (float)GUI::ScreenHeight() * 0.37f;
        Render::SetPosition(g_black, 0.f, 0.f, 0.02f);
        Render::SetVisibility(g_goldline, true);
        g_goldline->w = (float)GUI::ScreenWidth();
        Render::SetPosition(g_goldline, 0.f, 0.f, 0.0199f);
        g_shownTime = -0.5f;
    }
    if (!g_reputation) {
        g_fading = true;
        g_fadeT = 0.f;
        g_fadeIn = true;
    }
    UpdateContents();
    g_root->SetVisibility(true);
    if (!g_reputation) {
        g_root->SetAlpha(0.f, true);
        GameState::RaiseGamePauseState();
    }
}

// @0x2532e0: the banner fades out (a reputation screen slides up instead).
void Hide() {
    if (!g_reputation) {
        g_fading = true;
        g_fadeT = 0.f;
        g_fadeIn = false;
    } else {
        if (g_root->visibleSelf)
            g_effect->Animate(g_effect->GetCenteredX(), g_bandY, g_effect->GetCenteredX(), -g_root->h, false);
        Entity* player = EntityManager::GetPlayer();
        if (player && player->GetSprite()) Render::ChangeLayer(player->GetSprite(), Render::kLayerObjects);
    }
    WindowManager::WindowHide(false);
    g_queue->shown = false;
    g_queue->MoveWindowDown(true);
    if (!g_reputation) GameState::DropGamePauseState();
    BeltBarWindow::Show();
}

// @0x253474
void SetZ(float z) {
    g_root->SetZ(z);
    g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::GetVerticalCenter(g_root->h));
    if (g_effect->active) return;
    if (!g_queue->shown) return;
    const float h = (float)GUI::ScreenHeight() * 0.37f;
    g_black->h = h;
    g_black->w = (float)GUI::ScreenWidth();
    Render::SetPosition(g_black, 0.f, h, 0.02f);
    g_goldline->w = (float)GUI::ScreenWidth();
    Render::SetPosition(g_goldline, 0.f, g_black->y + 5.f, 0.0199f);
    g_bar.Rescale(g_rootScale);
}

// @0x25360c: a release after a second closes it (tutorial step 0x17: reported as step 0x590).
bool Click(int x, int y, bool pressed) {
    // (a reputation screen's taps go to the loot strip)
    if (g_reputation) {
        FoundItemsWindow::Click(x, y, pressed);
        return g_queue->shown;
    }
    if (!g_queue->shown) return false;
    if (!pressed && 1.f < g_fadeT) {
        Hide();
        if (GameState::tutorial == 0x17) TutorialSender::AddTutorialStep(0x590);
        return true;
    }
    return true;
}

// @0x2536bc
void Deinit() {
    GUI::RemoveMovementEffect(g_effect);
    g_effect = nullptr;
    delete g_root;
    g_root = nullptr;
    g_black = Render::RemoveSprite(g_black);
    g_goldline = Render::RemoveSprite(g_goldline);
}

// @0x253720
void Init() {
    g_blackTex = nullptr;
    g_black = nullptr;
    g_goldline = nullptr;
    g_ambush = false;
    g_tablet = GUI::IsTabletVersion();
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Endscreen_boss_start.xml", "Endscreen_boss_start.png", -1.f,
                             0, 0, 0, 0, false, 1.f);
    g_queue->RegisterTopWindow(g_root);
    g_rootScale = g_root->scale;
    g_root->SetVisibility(false);
    int h = GUI::ScreenHeight();
    if (h < 0x300) {
        if (h < 600) h = 600;
        g_bandY = -(int)((float)(0x300 - h) * 0.55f);
    }
    g_effect = GUI::CreateMovementEffect(g_root, OnTweenMove, OnTweenIn, OnTweenOut);
    g_text = GUI::GetWindowTyped<Textfield>(g_root, "text");
    g_text2 = GUI::GetWindowTyped<Textfield>(g_root, "text2");
    g_text3 = GUI::GetWindowTyped<Textfield>(g_root, "text3");
    g_tap = GUI::GetWindowTyped<Textfield>(g_root, "text_tap_to_continue");
    g_tapCopy = GUI::DuplicateWindow(g_tap, false, false, nullptr);
    if (Render::GlowFilter* glow = g_tap->glow) {
        glow->b = 0.f;
        glow->g = 0.f;
        glow->blur = 0x19;
        glow->r = 0.698f;
        glow->strength = 5.f;
    }
    g_bar.LoadFrom(g_root, "endscreen_progress_bar", "progress_bar_end_orange", "progress_bar_large_orange");
    g_bar.Rescale(g_rootScale);
    g_queue->fn.update = Update;
    g_queue->fn.show = Show;
    g_queue->fn.zRange = WindowManager::ReturnSmallZRange;
    g_queue->fn.back = OnBack;
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

}  // namespace BossIntroWindow
