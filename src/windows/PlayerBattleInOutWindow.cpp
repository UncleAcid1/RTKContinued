// PlayerBattleInOutWindow: the PvP arena fight's frame. The intro: black bands close in from the
// top and bottom with gold lines, the two fighters' flags (the hero's from the left, the
// opponent's from the right, each with the dressed figure and "name [level]") meet at a "VS",
// the header ("Ambush!" in the tutorial) and "tap to continue". The outro: the flags again with a
// winner's / loser's medal over each, the valor bar, and FoundItemsWindow's loot strip with its
// buttons. A tap (intro) or the strip's buttons (outro) fade it out.
// Port of PlayerBattleInOutWindow (libkingdom.so 5.11), 0x2fe2c8..0x301ad8. Statics at 0x625e34.
// UNVERIFIED (milestone 5): the outro's valor bar progress (Professions: profession 7, its level
// points; Update @0x300338 animates it), PlayerInfoWindow (the opponent's profile, OnInfo).
// (milestone 6: SoundsManager "arena_win".) Online (dormant): PvP::ReportStateToServer.
#include <cmath>
#include <cstdlib>

#include "engine/IconManager.h"
#include "engine/Render.h"
#include "engine/Resources.h"
#include "engine/Text.h"
#include "engine/Timer.h"
#include "game/BuildingHovers.h"
#include "game/Combat.h"
#include "game/Entity.h"
#include "game/EntityData.h"
#include "game/EntityManager.h"
#include "game/GameState.h"
#include "game/Map.h"
#include "game/PlayerEquipmentInfo.h"
#include "game/PvP.h"
#include "game/Squad.h"
#include "game/StringTable.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/HUD.h"
#include "hud/Shared.h"
#include "windows/Windows.h"

namespace PlayerBattleInOutWindow {
namespace {

using GUI::Textfield;
using GUI::Window;

float g_scale = 1.f;        // 0x60f314 the layouts' scale
float g_duration = 0.4f;    // 0x60f318 the fades' length
bool g_tablet = false;      // 0x60f31c

Window* Register(const char* xml, const char* png) {
    Window* r = GUI::RegisterUI(xml, png, g_scale, 0, 0, 0, 0, false, 1.f);
    g_scale = r->scale;
    r->SetVisibility(false);
    return r;
}

// The header over the outro (Endscreen_pvp_valor.xml).
struct OutroHeaderWindow {
    Window* root = nullptr;          // +0x00
    Window* bossBg = nullptr;        // +0x04 endscreen_boss_start_bg (a loss)
    Window* valorBg = nullptr;       // +0x08 endscreen_pvp_valor_bg (the intro, a win)
    Textfield* text = nullptr;       // +0x0c "defeat"
    Textfield* text1 = nullptr;      // +0x10 "victory"
    Textfield* text2 = nullptr;      // +0x14 the intro's header
    Shared::EndscreenProgressBar bar;   // +0x18 the valor
    // @0x30186c
    void Load() {
        root = Register("../resource/kingdom_ui/1Original/Endscreen_pvp_valor.xml", "Endscreen_pvp_valor.png");
        bossBg = GUI::GetWindowTypedF<Window>(root, "endscreen_boss_start_bg");
        valorBg = GUI::GetWindowTypedF<Window>(root, "endscreen_pvp_valor_bg");
        text = GUI::GetWindowTypedF<Textfield>(root, "text");
        text1 = GUI::GetWindowTypedF<Textfield>(root, "text_01");
        text2 = GUI::GetWindowTypedF<Textfield>(root, "text_02");
        bar.LoadFrom(root, "endscreen_progress_bar", "progress_bar_end_purple", "progress_bar_large_purple");
        Rescale();
    }
    void Rescale() { bar.Rescale(g_scale); }   // @0x2fe2c8 (the bar's Rescale, inlined)
};

// A medal over a flag (Pvp_winner_holder.xml): a coloured circle and the won / lost icon.
struct WinnerHolder {
    Window* root = nullptr;          // +0x00
    Window* circles[4] = {};         // +0x04 purple, orange, blue, red
    Window* icon = nullptr;          // +0x14 icon_60
    // @0x301580
    void Load() {
        root = Register("../resource/kingdom_ui/1Original/Pvp_winner_holder.xml", "Pvp_winner_holder.png");
        circles[0] = GUI::GetWindowTypedF<Window>(root, "luck_circle_holder.luck_circle_purple");
        circles[1] = GUI::GetWindowTypedF<Window>(root, "luck_circle_holder.luck_circle_orange");
        circles[2] = GUI::GetWindowTypedF<Window>(root, "luck_circle_holder.luck_circle_blue");
        circles[3] = GUI::GetWindowTypedF<Window>(root, "luck_circle_holder.luck_circle_red");
        icon = GUI::GetWindowTypedF<Window>(root, "icon_60");
        icon->takesZ = true;
    }
    // @0x2fe4d0
    void SetBackground(int type) {
        for (int i = 0; i < 4; ++i) circles[i]->SetVisibility(type == i);
    }
};

// A fighter's flag (Pvp_flag_holder.xml) with the dressed figure and the name.
struct FlagWindow {
    Window* root = nullptr;          // +0x00
    Window* longRed = nullptr;       // +0x04 flag_pvp_long_red
    Window* longBlue = nullptr;      // +0x08 flag_pvp_long_blue
    Window* redGlow = nullptr;       // +0x0c pvp_red_glow
    Window* whiteGlow = nullptr;     // +0x10 pvp_white_glow
    Shared::PlayerHolder holder;     // +0x14 pvp_char_05
    Textfield* name1 = nullptr;      // +0x1d0 text_name_01 (the outro)
    Textfield* name2 = nullptr;      // +0x1d4 text_name_02
    Textfield* nameTop1 = nullptr;   // +0x1d8 text_name_top_01 (the intro)
    Textfield* nameTop2 = nullptr;   // +0x1dc text_name_top_02
    Window* info = nullptr;          // +0x1e0 button_info_tiny
    // @0x3019a0
    void Load() {
        root = Register("../resource/kingdom_ui/1Original/Pvp_flag_holder.xml", "Pvp_flag_holder.png");
        longRed = GUI::GetWindowTypedF<Window>(root, "flag_pvp_long_red");
        longBlue = GUI::GetWindowTypedF<Window>(root, "flag_pvp_long_blue");
        redGlow = GUI::GetWindowTypedF<Window>(root, "pvp_red_glow");
        whiteGlow = GUI::GetWindowTypedF<Window>(root, "pvp_white_glow");
        holder.LoadFrom(root, "pvp_char_05");
        name1 = GUI::GetWindowTypedF<Textfield>(root, "text_name_01");
        name2 = GUI::GetWindowTypedF<Textfield>(root, "text_name_02");
        nameTop1 = GUI::GetWindowTypedF<Textfield>(root, "text_name_top_01");
        nameTop2 = GUI::GetWindowTypedF<Textfield>(root, "text_name_top_02");
        info = GUI::GetWindowTypedF<Window>(root, "button_info_tiny");
    }
};

// The "VS" between the flags (Pvp_vs_letter_holder.xml); the layout's places are kept.
struct VersusWindow {
    Window* root = nullptr;          // +0x00
    Window* glow = nullptr;          // +0x04 pvp_red_glow
    int glowX = 0, glowY = 0, glowW = 0, glowH = 0;   // +0x08..+0x14
    Window* v = nullptr;             // +0x18 pvp_vs_v
    int vX = 0, vY = 0;              // +0x1c +0x20
    Window* s = nullptr;             // +0x24 pvp_vs_s
    int sX = 0, sY = 0;              // +0x28 +0x2c
    // @0x301680
    void Load() {
        root = Register("../resource/kingdom_ui/1Original/Pvp_vs_letter_holder.xml", "Pvp_vs_letter_holder.png");
        glow = GUI::GetWindowTypedF<Window>(root, "pvp_red_glow");
        glowX = glow->x;
        glowY = glow->y;
        glowW = glow->w;
        glowH = glow->h;
        v = GUI::GetWindowTypedF<Window>(root, "pvp_vs_v");
        vX = v->x;
        vY = v->y;
        s = GUI::GetWindowTypedF<Window>(root, "pvp_vs_s");
        sX = s->x;
        sY = s->y;
    }
};

// "Tap to continue" with a white glow (Text_tap_to_continue.xml).
struct TapToContinue {
    Window* root = nullptr;          // +0x00
    Textfield* text = nullptr;       // +0x04 text_tap_to_continue
    Textfield* copy = nullptr;       // +0x08 the text without the glow, over it
    // @0x301788
    void Load() {
        root = Register("../resource/kingdom_ui/1Original/Text_tap_to_continue.xml", "Text_tap_to_continue.png");
        text = GUI::GetWindowTypedF<Textfield>(root, "text_tap_to_continue");
        copy = static_cast<Textfield*>(GUI::DuplicateWindow(text, false, false, nullptr));
        if (Render::GlowFilter* g = text->glow) {
            g->b = 1.f;
            g->r = 1.f;
            g->g = 1.f;
            g->blur = 0x19;
            g->strength = 5.f;
        }
    }
};

WindowManager::FunctionalWindow* g_queue = nullptr;   // PlayerBattleInOutWindow::wnd 0x6262f4
OutroHeaderWindow g_header;          // 0x625e34
FlagWindow g_flag1;                  // 0x625e7c the hero's
bool g_animating = false;            // 0x626060 a fade runs
bool g_fadingIn = false;             // 0x626061 the fade is the intro's
float g_timer = 0.f;                 // 0x626064
bool g_intro = false;                // 0x626068 the intro (else the outro)
uint32_t g_gid = 0;                  // 0x62606c the opponent (-1: the tutorial's ambush)
bool g_won = false;                  // 0x626070 the opponent was beaten
bool g_restart = false;              // 0x626071 ScheduleRestart: the fight again after the fade
WinnerHolder g_medal1;               // 0x626074
WinnerHolder g_medal2;               // 0x62608c
FlagWindow g_flag2;                  // 0x6260a4 the opponent's
Render::Sprite* g_blackTop = nullptr;      // 0x626288
Render::Sprite* g_goldTop = nullptr;       // 0x62628c
Render::Sprite* g_blackBottom = nullptr;   // 0x626290
Render::Sprite* g_goldBottom = nullptr;    // 0x626294
VersusWindow g_vs;                   // 0x626298
TapToContinue g_tap;                 // 0x6262c8
Window* g_itemHolder = nullptr;      // 0x6262d4 Character_item_holder.xml (the figures' item cell)
const void* g_profession = nullptr;  // 0x6262d8 Professions::ProfessionInfo* (7, milestone 5)
uint32_t g_points = 0;               // 0x6262dc its points before the fight
uint32_t g_target = 0;               // 0x6262e0 and after (with the fight's valor)
float g_barTimer = 0.f;              // 0x6262e4
float g_time = 0.f;                  // 0x6262e8 the glows' and the tap text's clock
int g_lastSecond = 0;                // 0x6262ec
Render::Texture* g_blackTex = nullptr;   // 0x6262f0

int ScreenW() { return GUI::ScreenWidth(); }
int ScreenH() { return GUI::ScreenHeight(); }

// @0x2fe5e0
float ZRange() { return g_queue->shown ? 0.03f : 0.001f; }

void Move(int, int) {}   // @0x2fe510

// @0x2ff298 UNVERIFIED (milestone 5): PlayerInfoWindow::SetParameters(g_gid, false) and Show.
void OnInfo() {}

// @0x2fe610
bool OnBack() {
    if (!g_queue->shown) return false;
    Hide();
    return true;
}

// The bands' sizes: down to the hero's flag's long piece, and 30 units shorter below.
void SizeBands() {
    const int W = ScreenW();
    g_blackTop->w = (float)W;
    g_blackTop->h = (float)((ScreenH() - g_flag1.root->h) / 2) + (float)g_flag1.longRed->y + g_scale * 30.f;
    g_blackBottom->h = g_blackTop->h + g_scale * -30.f;
    g_goldTop->w = (float)(W + 0x100);
    g_blackBottom->w = (float)W;
    g_goldBottom->w = (float)(W + 0x100);
}

// The bands at t (0 open, 1 closed), the gold lines along their edges, the tap text in the lower.
void PlaceBands(float t) {
    const float z = g_flag1.root->z;
    Render::SetPosition(g_blackTop, 0.f, t * g_blackTop->h, z + 0.0001f);
    Render::SetPosition(g_goldTop, -128.f, g_blackTop->y + 5.f, 0.0199f);
    Render::SetPosition(g_blackBottom, 0.f, (float)ScreenH() + (1.f - t) * g_blackBottom->h, z + 0.0001f);
    Render::SetPosition(g_goldBottom, -128.f, (g_blackBottom->y - g_blackBottom->h) + 5.f, z);
    Window* r = g_tap.root;
    r->SetPosition((ScreenW() - r->w) / 2,
                   (int)((g_blackBottom->y + g_blackBottom->h * -0.5f) - (float)(r->h / 2)));
}

// @0x2fea20: t from 0 to 1: the header comes down with the band, the flags slide in from the
// sides, the "VS" letters from above and below, its glow grows.
void SetupIntroFrame(float t) {
    const int W = ScreenW(), H = ScreenH();
    SizeBands();
    float top = g_blackTop->h;
    float lift = 0.f;
    if (g_intro) lift = top / -3.f;
    float y = top * t - (float)g_header.bar.hanger->y;
    if (g_intro) y += lift;
    Window* hr = g_header.root;
    const float hh = (float)hr->h;
    hr->SetPosition((W - hr->w) / 2, (int)((y + hh) * t - hh));
    hr->SetAlpha(1.f, true);
    g_flag1.root->SetAlpha(t, true);
    g_flag2.root->SetAlpha(t, true);

    const float x1 = (float)((W - g_vs.root->w) / 2) * 0.6f - (float)(g_flag1.root->w / 2);
    const float x2 = ((float)W - x1) - (float)g_flag2.root->w;
    const int medalY = g_header.bar.root->y + hr->y + (int)(g_scale * 35.f);
    Window* m = g_medal1.root;
    m->SetPosition((int)x1 + (g_flag1.root->w - m->w) / 2, medalY - m->h);
    m = g_medal2.root;
    m->SetPosition((int)x2 + (g_flag2.root->w - m->w) / 2, medalY - m->h);
    const int dy = (int)(g_scale * 30.f);
    Window* f = g_flag1.root;
    float from = (float)-f->w;
    f->SetPosition((int)(from + t * (x1 - from)), dy + (H - f->h) / 2);
    f = g_flag2.root;
    from = (float)W;
    f->SetPosition((int)(from + t * (x2 - from)), dy + (H - f->h) / 2);
    g_vs.root->SetPosition((W - g_vs.root->w) / 2, dy + (H - g_vs.root->h) / 2);

    const float gw = 1.f + t * ((float)g_vs.glowW - 1.f), gh = 1.f + t * ((float)g_vs.glowH - 1.f);
    g_vs.glow->SetSize((unsigned)(int)gw, (unsigned)(int)gh);
    if (Render::Sprite* sp = g_vs.glow->sprite) {
        sp->w = gw;
        sp->h = gh;
    }
    g_vs.glow->SetPosition((int)((float)g_vs.glowW - gw) / 2, (int)((float)g_vs.glowH - gh) / 2);
    g_vs.glow->SetAlpha(std::sqrt(1.f - (t - 1.f) * (t - 1.f)) + 0.f, false);
    float ly = (float)g_vs.vY, from2 = ly + g_scale * -100.f;
    g_vs.v->SetPosition(g_vs.vX, (int)(from2 + t * (ly - from2)));
    g_vs.v->SetAlpha(t, true);
    ly = (float)g_vs.sY;
    from2 = ly + g_scale * 100.f;
    g_vs.s->SetPosition(g_vs.sX, (int)(from2 + t * (ly - from2)));
    g_vs.s->SetAlpha(t, true);
    PlaceBands(t);
}

// @0x2fe770: t from 1 to 0: everything fades and the bands open.
void SetupOutroFrame(float t) {
    SizeBands();
    g_header.root->SetAlpha(t, true);
    g_flag1.root->SetAlpha(t, true);
    g_flag2.root->SetAlpha(t, true);
    g_vs.glow->SetAlpha(t, false);
    g_vs.v->SetAlpha(t, true);
    g_vs.s->SetAlpha(t, true);
    PlaceBands(t);
}

// The fade-out's end: all hidden; then the fight again (ScheduleRestart), or after the outro the
// fight's end (a loss: back to the map it came from), or after a real intro now and then a
// (random) early start.
void Finish() {
    g_timer = g_duration;
    g_animating = false;
    for (Window* w : {g_header.root, g_medal1.root, g_flag1.root, g_vs.root, g_medal2.root, g_flag2.root, g_tap.root})
        w->SetVisibility(false);
    for (Render::Sprite* s : {g_blackTop, g_goldTop, g_blackBottom, g_goldBottom}) Render::SetVisibility(s, false);
    g_queue->shown = false;
    WindowManager::WindowHide(false);
    if (g_restart) {
        g_restart = false;
        g_intro = true;
        Show();
    } else if (!g_intro) {
        if (BaseCombat* c = GameState::GetActiveCombat()) c->PostCombat();
        if (!g_won) Map::EnqueueLoadMap((int)GameState::mReturnMapID, true, false, 0.f);
    } else if (!GameState::IsPvPTutorial() && (lrand48() & 1) == 0) {
        if (BaseCombat* c = GameState::GetActiveCombat()) c->Continue(10);
    }
}

// @0x300338
void Update(float dt) {
    if (!g_queue->shown) return;
    g_header.root->SetVisibility(true);
    g_header.bar.root->SetVisibility(!g_intro);
    g_header.valorBg->SetVisibility(g_intro || g_won);
    g_header.bossBg->SetVisibility(!g_intro && !g_won);
    g_medal1.root->SetVisibility(!g_intro);
    g_flag1.root->SetVisibility(true);
    g_vs.root->SetVisibility(g_intro);
    g_medal2.root->SetVisibility(!g_intro);
    g_flag2.root->SetVisibility(true);
    g_tap.root->SetVisibility(g_intro);
    Render::SetVisibility(g_blackTop, true);
    Render::SetVisibility(g_goldTop, g_intro);
    Render::SetVisibility(g_blackBottom, true);
    Render::SetVisibility(g_goldBottom, true);
    g_duration = 0.4f;
    if ((float)HUDWindow::GetDeltaTimeMultiplier() * 0.1999f < dt) dt = 0.01f;
    g_time += dt;
    bool animating = false;
    if (g_animating) {
        animating = true;
        if (g_fadingIn) {
            g_timer += dt;
            if (g_timer >= g_duration) {
                g_timer = g_duration;
                g_animating = false;
            }
            SetupIntroFrame(g_timer / g_duration);
        } else {
            g_timer += dt + dt;
            if (g_timer >= g_duration) Finish();
            SetupOutroFrame(1.f - g_timer / g_duration);
        }
    }
    g_vs.glow->SetRotation(g_time * -1.5f);
    g_flag1.whiteGlow->SetRotation(g_time * -0.25f);
    g_flag2.redGlow->SetRotation(g_time * 0.25f);
    g_tap.text->SetAlpha(std::fabs(std::cos(g_time * 3.f) * 0.7f), false);
    if (!g_intro && g_profession) {
        // UNVERIFIED (milestone 5): the valor bar fills from the points before the fight to after
        // (g_barTimer from -0.5, at half speed) towards the next level of profession 7, with
        // "ARENA_WAVE_GAINED" / "PVP_WAVE_GAINED_NEXT" on it, and pulses once full.
        g_barTimer += dt;
        (void)g_points;
        (void)g_target;
    }
    const int now = Timer::GetGlobalTime();
    if (now != g_lastSecond) {
        g_lastSecond = now;
        if (!animating) UpdateContents();
    }
}

// @0x301400
void Deinit() {
    delete g_itemHolder;
    g_itemHolder = nullptr;
    for (Window** w : {&g_header.root, &g_medal1.root, &g_flag1.root, &g_vs.root, &g_medal2.root, &g_flag2.root,
                       &g_tap.root}) {
        delete *w;
        *w = nullptr;
    }
    g_blackTop = Render::RemoveSprite(g_blackTop);
    g_goldTop = Render::RemoveSprite(g_goldTop);
    g_blackBottom = Render::RemoveSprite(g_blackBottom);
    g_goldBottom = Render::RemoveSprite(g_goldBottom);
}

// @0x301b10: the layouts' scale is 1 (a narrower screen than 1024: its share of 1024, a lower one
// than 720: of 600 if smaller).
void Init() {
    g_goldBottom = nullptr;
    g_blackTex = nullptr;
    g_blackTop = nullptr;
    g_goldTop = nullptr;
    g_blackBottom = nullptr;
    g_tablet = GUI::IsTabletVersion();
    g_scale = 1.f;
    if (ScreenW() < 0x400) g_scale = (float)ScreenW() * (1.f / 1024.f);
    if (ScreenH() < 0x2d0 && (float)ScreenH() / 600.f < g_scale) g_scale = (float)ScreenH() / 600.f;
    g_header.Load();
    g_medal1.Load();
    g_flag1.Load();
    g_vs.Load();
    g_medal2.Load();
    g_flag2.Load();
    g_flag2.info->SetOnClick(OnInfo);
    g_tap.Load();
    g_itemHolder = GUI::RegisterUI("../resource/kingdom_icons/1Original/Character_item_holder.xml", "", 1.f, 0, 0, 0, 0,
                                   false, 1.f);
    g_queue->fn.zRange = ZRange;
    g_queue->fn.show = Show;
    g_queue->fn.update = Update;
    g_queue->fn.back = OnBack;
    g_blackTex = Render::CreateSolidFillTexture(0.f, 0.f, 0.f, 1.f);
    g_blackTop = Render::CreateSprite(g_blackTex, Render::kLayerGUI, false, false);
    g_blackTop->screenSpace = true;
    Render::SetShaderType(g_blackTop, 1);
    Render::SetVisibility(g_blackTop, false);
    Render::SetAlpha(g_blackTop, 1.f);
    g_goldTop = Render::CreateSprite(Resources::GetUIImage("Endscreen_goldline_scaler.png", false, false),
                                     Render::kLayerGUI, false, false);
    g_goldTop->screenSpace = true;
    Render::SetVisibility(g_goldTop, false);
    g_blackBottom = Render::CreateSprite(g_blackTex, Render::kLayerGUI, false, false);
    g_blackBottom->screenSpace = true;
    Render::SetShaderType(g_blackBottom, 1);
    Render::SetVisibility(g_blackBottom, false);
    Render::SetAlpha(g_blackBottom, 1.f);
    g_goldBottom = Render::CreateSprite(Resources::GetUIImage("Endscreen_goldline_scaler.png", false, false),
                                        Render::kLayerGUI, false, false);
    g_goldBottom->screenSpace = true;
    Render::SetVisibility(g_goldBottom, false);
}

}  // namespace

WindowManager::FunctionalWindow* Queue() {
    if (!g_queue) {   // FUN_002fe640 (static init)
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.deinit = Deinit;
        f.click = Click;
        f.setZ = SetZ;
        f.hide = Hide;
        f.move = Move;
        g_queue = new WindowManager::FunctionalWindow("PlayerBattleInOutWindow", std::move(f));
    }
    return g_queue;
}

float GetZ() { return g_flag1.root->z; }                // @0x2fe514
bool IsVisible() { return g_queue->shown; }             // @0x2fe558
void SetModeIntro() { g_intro = true; }                 // @0x2fe56c
void SetModeOutro() { g_intro = false; }                // @0x2fe584
bool EnemyDefeated() { return g_won; }                  // @0x2fe5b4
void ScheduleRestart() { g_restart = true; }            // @0x2fe5c8

// @0x2fe59c
void SetParameters(uint32_t gid, bool won) {
    g_won = won;
    g_gid = gid;
}

// @0x2fe52c: the fade-out.
void Hide() {
    g_timer = 0.f;
    g_animating = true;
    g_fadingIn = false;
}

// @0x2ffd88: back from the fight (the tutorial's ambush: home, with its task done).
void ScheduleReturn() {
    if (g_gid != (uint32_t)-1) {
        Map::EnqueueLoadMap((int)GameState::mReturnMapID, false, false, 0.f);
        return;
    }
    if (!GameState::TaskCompleted(0x773)) GameState::CompleteTask(0x773, false);
    Map::EnqueueLoadMap(0, false, false, 0.5f);
}

// @0x2ffd5c
void OnContinue() {
    if (g_animating) return;
    if (!g_intro) {
        FoundItemsWindow::OnShare();
        return;
    }
    Hide();
}

// @0x3012f8: a tap fades the intro out (the tutorial reports step 0x5f4); the opponent's flag
// (its profile button) takes clicks.
bool Click(int x, int y, bool pressed) {
    bool r = false;
    if (g_queue->shown) {
        if (BuildingHovers::ClickOnArrow(x, y, pressed)) return true;
        if (!pressed) BuildingHovers::HideArrow();
        r = g_queue->shown;
        if (r && g_intro && !g_animating && !pressed) {
            Hide();
            if (GameState::tutorial == 0x17) TutorialSender::AddTutorialStep(0x5f4);
        }
    }
    if (g_flag2.root->Click(x, y, pressed, false)) return true;
    return r;
}

// @0x2ff148
void SetZ(float z) {
    if (!IsVisible()) return;
    g_header.root->SetZ(z);
    g_medal1.root->SetZ(z - 0.001f);
    g_flag1.root->SetZ(z);
    g_vs.root->SetZ(z);
    g_medal2.root->SetZ(z - 0.001f);
    g_flag2.root->SetZ(z);
    g_tap.root->SetZ(z);
    if (!g_animating) g_header.Rescale();
    if (g_fadingIn) {
        if (!g_animating) SetupIntroFrame(1.f);
        return;
    }
    if (!g_animating) SetupOutroFrame(1.f);
}

// @0x2ff2bc: the texts, the medals (the beaten one's flag greyed), each flag's "name [level]"
// (the hero's from the game, the opponent's from its PvP info) and figure.
void UpdateContents() {
    HUDWindow::HideMapName();
    g_header.text->SetText(StringTable::GetString("ARENA_WAVE_DEFEAT"));
    g_header.text1->SetText(StringTable::GetString("PVP_VICTORY"));
    g_header.text2->SetText(StringTable::GetString(g_gid == (uint32_t)-1 ? "TUT_AMBUSH" : "PVP_INTRO_HEADER"));
    g_header.text->SetVisibility(!g_intro && !g_won);
    g_header.text1->SetVisibility(!g_intro && g_won);
    g_header.text2->SetVisibility(g_intro);
    g_flag1.longRed->SetVisibility(false);
    g_flag2.longBlue->SetVisibility(false);
    g_flag1.whiteGlow->SetVisibility(!g_intro && g_won);
    g_flag1.redGlow->SetVisibility(false);
    g_flag2.whiteGlow->SetVisibility(false);
    g_flag2.redGlow->SetVisibility(!g_intro && !g_won);
    const bool heroUp = g_intro || g_won, rivalUp = g_intro || !g_won;
    g_flag1.longBlue->SetEnabled(heroUp);
    g_flag1.longRed->SetEnabled(heroUp);
    g_flag1.holder.root->SetEnabled(heroUp);
    g_flag2.longBlue->SetEnabled(rivalUp);
    g_flag2.longRed->SetEnabled(rivalUp);
    g_flag2.holder.body->SetEnabled(rivalUp);
    g_flag2.holder.bodyFemale->SetEnabled(rivalUp);
    for (Shared::PlayerHolder::Layer& l : g_flag2.holder.layers) {
        l.male->SetEnabled(rivalUp);
        l.femaleWindow->SetEnabled(rivalUp);
    }
    g_medal1.SetBackground(g_won ? 1 : 3);
    g_medal1.icon->SetTexture(IconManager::GetIcon(g_won ? "pvp_won_60" : "pvp_lost_60"), true);
    g_medal2.SetBackground(g_won ? 3 : 1);
    g_medal2.icon->SetTexture(IconManager::GetIcon(g_won ? "pvp_lost_60" : "pvp_won_60"), true);
    for (int i = 0; i < 2; ++i) {
        FlagWindow& f = i == 0 ? g_flag1 : g_flag2;
        PvP::PlayerInfo* info = i == 0 ? nullptr : PvP::GetPlayerInfoByGID(g_gid, 0xff);
        const char32_t* name;
        bool male;
        int level;
        if (!info) {
            name = GameState::GetPlayerName();
            male = GameState::IsMalePlayer();
            level = (int)GameState::GetLevel();
        } else {
            name = info->name;
            male = info->male;
            level = (int)info->level;
        }
        const std::u32string text = SWPrintf(0x100, U"%s [%d]", {name, level});
        f.name1->SetVisibility(!g_intro);
        f.name2->SetVisibility(!g_intro);
        if (!g_intro) {
            f.name1->SetText(text.c_str());
            f.name2->SetText(text.c_str());
        }
        f.nameTop1->SetVisibility(g_intro);
        f.nameTop2->SetVisibility(g_intro);
        if (g_intro) {
            f.nameTop1->SetText(text.c_str());
            f.nameTop2->SetText(text.c_str());
            f.info->SetVisibility(false);
        } else {
            f.info->SetVisibility(i == 1);
        }
        f.holder.body->SetVisibility(male);
        f.holder.bodyFemale->SetVisibility(!male);
        PvP::PlayerEquipmentInfo own;
        if (!info) own.GetPlayerEquipment();
        f.holder.PlacePlayerEquipment(info ? info->equipment : own, (float)f.holder.root->x, (float)f.holder.root->y,
                                      g_itemHolder, male, 1.f, 0.f, 0.f, 0.f, true);
    }
    g_tap.text->SetText(StringTable::GetString("TAP_TO_CONTINUE"));
    g_tap.copy->SetText(StringTable::GetString("TAP_TO_CONTINUE"));
}

// @0x2ffdfc: the bands at their open places, the fade-in starts. The outro shows the fight's loot
// (FoundItemsWindow) over it, turns the hero's camera on and cheers the winners.
void Show() {
    const bool was = g_queue->shown;
    g_queue->shown = true;
    g_queue->MoveWindowOnTop(true);
    if (!was) WindowManager::WindowShow(false);
    g_timer = 0.f;
    g_fadingIn = true;
    g_animating = true;
    Render::SetPosition(g_blackTop, 0.f, 0.f, 0.02f);
    Render::SetPosition(g_goldTop, 0.f, 0.f, 0.0199f);
    Render::SetPosition(g_blackBottom, 0.f, (float)ScreenH() + g_blackBottom->h, 0.027f);
    Render::SetPosition(g_goldBottom, 0.f, (float)ScreenH(), 0.0269f);
    {
        Timer::TimeCompensationBlock block;
        if (g_intro) {
            g_flag2.holder.root->SetOnClick(nullptr);
        } else {
            FoundItemsWindow::SetParameters((uint32_t)GameState::GetCurrentMapID(), 0,
                                            StringTable::GetString(g_won ? "ARENA_WAVE_FOUND" : "ARENA_WAVE_DEFEAT_HEADER"));
            FoundItemsWindow::Show();
            FoundItemsWindow::SetZ(GetZ());
            // UNVERIFIED (milestone 5): g_profession = Professions::GetProfession(7) and g_points
            // its points (GameState::GetProfessionPoints); without professions there is no bar.
            g_profession = nullptr;
            // (milestone 6: SoundsManager "arena_win" on a win.)
            g_target = (uint32_t)GameState::GetMapItemCollectionInfo((uint32_t)GameState::GetCurrentMapID(), 0x249) +
                       g_points;
            g_barTimer = -0.5f;
            Entity* player = EntityManager::GetPlayer();
            if (BaseSquad* sq = player ? player->GetSquad() : nullptr) {
                for (unsigned i = 0; i < (unsigned)sq->GetSoldierCount(); ++i) {
                    Entity* e = sq->GetSoldier(i);
                    if (!e) continue;
                    if (!e->IsDead() && g_won) {
                        e->SetDirection((lrand48() & 1) == 0 ? 1 : 7);
                        e->SetAnimationP(e->player ? "bat_vict" : "victory", true, false, false);
                    }
                    // (animated over 0.4 s on the original: Render::CenterOn's tween is not ported)
                    if (e->player) Render::CenterOn(e->worldX, e->worldY - e->GetIdleHeight());
                }
            }
            if (!g_won && GameState::IsCombatActive()) {
                if (BaseSquad* sq = GameState::GetActiveCombat()->GetEnemySquad()) {
                    for (unsigned i = 0; i < (unsigned)sq->GetSoldierCount(); ++i) {
                        Entity* e = sq->GetSoldier(i);
                        if (!e || e->IsDead()) continue;
                        e->SetDirection((lrand48() & 1) == 0 ? 1 : 7);
                        const uint32_t id = e->GetEntityData()->id;
                        e->SetAnimationP(id == 0xc || id == 0xd ? "bat_vict" : "victory", true, false, false);
                    }
                }
            }
            g_flag2.holder.root->SetOnClick(OnInfo);
        }
        UpdateContents();
    }
    for (Window* w : {g_header.root, g_medal1.root, g_flag1.root, g_vs.root, g_medal2.root, g_flag2.root})
        w->PreloadTextures();   // GUI::PreloadWindow
    if (GameState::tutorial == 0x17 && g_intro) GUI::SetInteractionLock(false);
    if (g_gid == (uint32_t)-1) Render::SetViewportMapBounds(4, -1, 0xf, 0x1e);
}

}  // namespace PlayerBattleInOutWindow
