// PlayerCompetitionWindow: the PvP opponents' window (Window_pvp.xml). Port of PlayerCompetitionWindow
// (libkingdom.so 5.11, @0x301e08..0x31ffc4). DECIDED 2026-10-20: only its PvP tab (tab 4) is ported;
// the activity, friends, gifts, top players and invite tabs stay dormant (online: their holders are
// not loaded and their tabs stay hidden).
//
// The PvP tab (pvp_part_enemy): a carousel of the opponents of the list (PvP type 1: the tutorial's
// three shadows, then the opponents the offline server makes): nine figures on a turning circle,
// the one in front the shown opponent; arrows, taps on the side figures and a horizontal drag turn
// it. Under it his squad, his record, the valor bar, Refresh (free, DECIDED 2026-10-20) and Fight:
// the window closes, the world map travels to the arena (GlobalMapWindow::ShowPvPAnimation) and the
// fight starts there. The info button opens a fought opponent's PlayerInfoWindow.
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "engine/IconManager.h"
#include "engine/Render.h"
#include "engine/Timer.h"
#include "game/BuildingHovers.h"
#include "game/Entity.h"
#include "game/EntityData.h"
#include "game/EntityManager.h"
#include "game/GameState.h"
#include "game/Professions.h"
#include "game/PvP.h"
#include "game/Setting.h"
#include "game/StringTable.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/HUD.h"
#include "hud/ItemHoverWindow.h"
#include "hud/Shared.h"
#include "windows/Windows.h"

namespace PlayerCompetitionWindow {

namespace {

using GUI::Textfield;
using GUI::Window;

// PlayerCompetitionWindow::GiNotificationHolder (0xc bytes): a tab's count bubble.
struct GiNotificationHolder {
    Window* root = nullptr;           // +0x00
    Window* bg = nullptr;             // +0x04 gi_notification_bg
    Textfield* text = nullptr;        // +0x08 text_notification

    // @0x323118
    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTyped<Window>(w, p);
        bg = GUI::GetWindowTypedF<Window>(w, "%s.gi_notification_bg", p);
        text = GUI::GetWindowTypedF<Textfield>(w, "%s.text_notification", p);
    }
};

// PlayerCompetitionWindow::IconGoldQuestNew (8 bytes)
struct IconGoldQuestNew {
    Window* root = nullptr;           // +0x00
    Window* mark = nullptr;           // +0x04 exc_mark

    // @0x3202b4
    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTyped<Window>(w, p);
        mark = GUI::GetWindowTypedF<Window>(w, "%s.exc_mark", p);
    }
};

// PlayerCompetitionWindow::TabShop (0x30 bytes): one of the six tabs (tab_01 .. tab_06).
struct TabShop {
    Window* root = nullptr;           // +0x00
    GiNotificationHolder count;       // +0x04 gi_notification_holder
    Window* lock = nullptr;           // +0x10 icon_gold_16_lock (takes a depth slot)
    IconGoldQuestNew questNew;        // +0x14 icon_gold_quest_new
    Window* icon = nullptr;           // +0x1c icon_place_holder (takes a depth slot)
    Window* active = nullptr;         // +0x20 tab_active
    Window* activeRed = nullptr;      // +0x24 tab_active_red
    Window* inactive = nullptr;       // +0x28 tab_inactive
    Window* inactiveRed = nullptr;    // +0x2c tab_inactive_red

    // @0x323174
    void LoadFrom(Window* w, const char* p) {
        std::string s = p;
        root = GUI::GetWindowTyped<Window>(w, p);
        count.LoadFrom(w, (s + ".gi_notification_holder").c_str());
        lock = GUI::GetWindowTypedF<Window>(w, "%s.icon_gold_16_lock", p);
        lock->takesZ = true;
        questNew.LoadFrom(w, (s + ".icon_gold_quest_new").c_str());
        icon = GUI::GetWindowTypedF<Window>(w, "%s.icon_place_holder", p);
        icon->takesZ = true;
        active = GUI::GetWindowTypedF<Window>(w, "%s.tab_active", p);
        activeRed = GUI::GetWindowTypedF<Window>(w, "%s.tab_active_red", p);
        inactive = GUI::GetWindowTypedF<Window>(w, "%s.tab_inactive", p);
        inactiveRed = GUI::GetWindowTypedF<Window>(w, "%s.tab_inactive_red", p);
    }
};

// PlayerCompetitionWindow::ValorBar (0x40 bytes): the player's valor (profession 7) and its bar.
struct ValorBar {
    Window* root = nullptr;           // +0x00 (tapped: OnValor)
    Window* icon = nullptr;           // +0x04 icon_rep (takes a depth slot)
    Textfield* level = nullptr;       // +0x08 text_rep_level
    Textfield* stat = nullptr;        // +0x0c text_rep_stat
    Textfield* barBg = nullptr;       // +0x10 rep_bar_holder_pvp.text_bg
    Textfield* barText = nullptr;     // +0x14 rep_bar_holder_pvp.text_rep (clipped)
    Window* bar = nullptr;            // +0x18 rep_bar_holder_pvp.pvp_valor_bar_fat (clipped)
    Window* info = nullptr;           // +0x1c pvp_info_button (tapped: OnValor)
    Window* infoIcon = nullptr;       // +0x20 icon_holder_info
    Window* shop = nullptr;           // +0x24 button_pvp_shop_small (tapped: OnValorShop)
    Window* shopIcon = nullptr;       // +0x28 its icon_16_pvp
    Textfield* shopText = nullptr;    // +0x2c its text_unlock_items
    GUI::ClipRect clip{};             // +0x30 the bar's rect

    // @0x3034e0
    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        icon = GUI::GetWindowTypedF<Window>(w, "%s.icon_rep", p);
        icon->takesZ = true;
        level = GUI::GetWindowTypedF<Textfield>(w, "%s.text_rep_level", p);
        stat = GUI::GetWindowTypedF<Textfield>(w, "%s.text_rep_stat", p);
        barBg = GUI::GetWindowTypedF<Textfield>(w, "%s.rep_bar_holder_pvp.text_bg", p);
        barText = GUI::GetWindowTypedF<Textfield>(w, "%s.rep_bar_holder_pvp.text_rep", p);
        bar = GUI::GetWindowTypedF<Window>(w, "%s.rep_bar_holder_pvp.pvp_valor_bar_fat", p);
        info = GUI::GetWindowTypedF<Window>(w, "%s.pvp_info_button", p);
        infoIcon = GUI::GetWindowTypedF<Window>(w, "%s.icon_holder_info", p);
        infoIcon->SetTexture(IconManager::GetIcon("gold_info", false), true);
        shop = GUI::GetWindowTypedF<Window>(w, "%s.button_pvp_shop_small", p);
        shopIcon = GUI::GetWindowTypedF<Window>(w, "%s.button_pvp_shop_small.icon_16_pvp", p);
        shopIcon->SetTexture(IconManager::GetIcon("pvp_valor_16", false), true);
        shopText = GUI::GetWindowTypedF<Textfield>(w, "%s.button_pvp_shop_small.text_unlock_items", p);
        clip = {bar->x, bar->y, bar->x + bar->w, bar->y + bar->h};
        bar->SetClipRect(&clip);
        barText->SetClipRect(&clip);
    }

    // @0x301eec the bar filled to part (0..1).
    void SetProgress(float part) { clip.right = (int)((float)clip.left + (float)bar->w * part); }
};

// PlayerCompetitionWindow::StatisticHolder (0x10 bytes): one line of the opponent's record.
struct StatisticHolder {
    Window* root = nullptr;           // +0x00
    Textfield* text = nullptr;        // +0x04 text_stat
    Textfield* number = nullptr;      // +0x08 text_stat_num
    Window* icon = nullptr;           // +0x0c icon_stat (takes a depth slot)

    // @0x303458
    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        text = GUI::GetWindowTypedF<Textfield>(w, "%s.text_stat", p);
        number = GUI::GetWindowTypedF<Textfield>(w, "%s.text_stat_num", p);
        icon = GUI::GetWindowTypedF<Window>(w, "%s.icon_stat", p);
        icon->takesZ = true;
    }
};

// One of the carousel's figures: Shared::PlayerHolder (0x1bc bytes in the original, its last three
// floats kept here).
struct Figure {
    Shared::PlayerHolder holder;
    float offset = 0.f;               // +0x004 its angle from the centre (degrees)
    float bodyScale = 1.f;            // +0x1b0 its body's width / the centre figure's
    float f1b4 = 1.f;                 // +0x1b4 its depth scale relative to its body's
    float f1b8 = 1.f;                 // +0x1b8
};

// PlayerCompetitionWindow::EnemyHolder (pvp_part_enemy, @0x62739c): the PvP tab.
struct EnemyHolder {
    Window* root = nullptr;           // +0x0000
    Window* selectorBg = nullptr;     // +0x0004 pvp_selector_bg_arena
    ValorBar valor;                   // +0x0008 pvp_valor_bar
    Window* paramBack = nullptr;      // +0x0048 param_back
    Window* timerIcon = nullptr;      // +0x004c icon_timer (takes a depth slot)
    Textfield* statNum = nullptr;     // +0x0050 text_stat_num
    Window* refresh = nullptr;        // +0x0054 button_pvp_refresh (tapped: OnRefresh(false))
    Textfield* refreshText = nullptr; // +0x0058 its text_gold
    Textfield* refreshPrice = nullptr;    // +0x005c its text_price
    Window* refreshFree = nullptr;    // +0x0060 pvp_refresh_opps (tapped: OnRefreshFree)
    Textfield* refreshFreeText = nullptr; // +0x0064 its text_button
    Window* refreshFreeIcon = nullptr;    // +0x0068 its icon_35 (takes a depth slot)
    Window* refreshCrystal = nullptr; // +0x006c button_pvp_refresh.icon_35_crystal
    Window* info = nullptr;           // +0x0070 button_info_round (tapped: OnInfo)
    Window* infoIcon = nullptr;       // +0x0074 its icon_placeholder_35 ("gold_info")
    Textfield* infoText = nullptr;    // +0x0078 its text
    Window* backdrop = nullptr;       // +0x007c backdrop_01
    Textfield* day1 = nullptr;        // +0x0080 text_day_01
    Textfield* day2 = nullptr;        // +0x0084 text_day_02
    Textfield* error = nullptr;       // +0x0088 text_error
    Window* left = nullptr;           // +0x008c button_navigate_left (tapped: OnLeft)
    Window* right = nullptr;          // +0x0090 button_navigate_right (tapped: OnRight)
    Window* mask = nullptr;           // +0x0094 mask_selector
    GUI::ClipRect figureClip{};       // +0x0098 the mask's rect (1024 deeper): clips the figures
    Textfield* name1 = nullptr;       // +0x00a8 text_name_01 (centred)
    Textfield* name2 = nullptr;       // +0x00ac text_name_02 (centred)
    Figure figures[9];                // +0x00b0 pvp_char_01 .. 09 in the order kFigureOrder
    Textfield* squadTitle = nullptr;  // +0x104c text_enemy_squad
    Textfield* squadFront = nullptr;  // +0x1050 text_squad_front
    Shared::WarriorSlotInfo slots[3]; // +0x1054 slot_warrior_01 .. 03 (tapped: OnWarrior(i))
    Shared::ButtonRaidStart start;    // +0x12e8 button_pvp_start (tapped: OnFight)
    Window* vicDef = nullptr;         // +0x132c pvp_vic_def_holder
    Window* vicDefIcon = nullptr;     // +0x1330 its icon_vic_def (takes a depth slot)
    Textfield* victory = nullptr;     // +0x1334 its text_victory
    Textfield* defeat = nullptr;      // +0x1338 its text_defeat
    Textfield* vicDefExp = nullptr;   // +0x133c its text_vic_def_exp
    Window* statsBg = nullptr;        // +0x1340 sliced_crem_bg_02
    Textfield* statsTitle = nullptr;  // +0x1344 text_enemy_stats
    StatisticHolder stats[4];         // +0x1348 pvp_stat_01 .. 04
    Window* separator = nullptr;      // +0x1388 sepparator_01
    // (+0x138c a Shared::ContentScroller: initialised, never used by the PvP tab)
    Window* dragArea = nullptr;       // +0x140c mask_selector again
    GUI::ClipRect dragRect{};         // +0x1410 its rect: a press inside starts a drag

    // @0x303734
    void LoadFrom(Window* w, const char* p) {
        // The figures pvp_char_NN by index: 8 is the big centre one (pvp_char_05).
        static const int kFigureOrder[9] = {0, 1, 2, 3, 5, 6, 7, 8, 4};
        char q[128];
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        selectorBg = GUI::GetWindowTypedF<Window>(w, "%s.pvp_selector_bg_arena", p);
        std::snprintf(q, sizeof q, "%s.pvp_valor_bar", p);
        valor.LoadFrom(w, q);
        paramBack = GUI::GetWindowTypedF<Window>(w, "%s.param_back", p);
        timerIcon = GUI::GetWindowTypedF<Window>(w, "%s.icon_timer", p);
        timerIcon->takesZ = true;
        statNum = GUI::GetWindowTypedF<Textfield>(w, "%s.text_stat_num", p);
        refresh = GUI::GetWindowTypedF<Window>(w, "%s.button_pvp_refresh", p);
        refreshText = GUI::GetWindowTypedF<Textfield>(w, "%s.button_pvp_refresh.text_gold", p);
        refreshPrice = GUI::GetWindowTypedF<Textfield>(w, "%s.button_pvp_refresh.text_price", p);
        refreshFree = GUI::GetWindowTypedF<Window>(w, "%s.pvp_refresh_opps", p);
        refreshFreeText = GUI::GetWindowTypedF<Textfield>(w, "%s.pvp_refresh_opps.text_button", p);
        refreshFreeIcon = GUI::GetWindowTypedF<Window>(w, "%s.pvp_refresh_opps.icon_35", p);
        refreshFreeIcon->takesZ = true;
        refreshCrystal = GUI::GetWindowTypedF<Window>(w, "%s.button_pvp_refresh.icon_35_crystal", p);
        info = GUI::GetWindowTypedF<Window>(w, "%s.button_info_round", p);
        infoIcon = GUI::GetWindowTypedF<Window>(w, "%s.button_info_round.icon_placeholder_35", p);
        infoIcon->SetTexture(IconManager::GetIcon("gold_info", false), true);
        infoText = GUI::GetWindowTypedF<Textfield>(w, "%s.button_info_round.text", p);
        backdrop = GUI::GetWindowTypedF<Window>(w, "%s.backdrop_01", p);
        day1 = GUI::GetWindowTypedF<Textfield>(w, "%s.text_day_01", p);
        day2 = GUI::GetWindowTypedF<Textfield>(w, "%s.text_day_02", p);
        error = GUI::GetWindowTypedF<Textfield>(w, "%s.text_error", p);
        left = GUI::GetWindowTypedF<Window>(w, "%s.button_navigate_left", p);
        right = GUI::GetWindowTypedF<Window>(w, "%s.button_navigate_right", p);
        mask = GUI::GetWindowTypedF<Window>(w, "%s.mask_selector", p);
        figureClip = {mask->x, mask->y, mask->x + mask->w, mask->y + mask->h + 0x400};
        name1 = GUI::GetWindowTypedF<Textfield>(w, "%s.text_name_01", p);
        name1->SetAlignment(1, 1);
        name2 = GUI::GetWindowTypedF<Textfield>(w, "%s.text_name_02", p);
        name2->SetAlignment(1, 1);
        for (unsigned i = 0; i != 9; ++i) {
            std::snprintf(q, sizeof q, "%s.pvp_char_%02d", p, kFigureOrder[i] + 1);
            figures[i].holder.LoadFrom(w, q);
            figures[i].holder.root->SetClipRect(&figureClip);
        }
        // The first figure goes one spacing further left, the centre one a spacing right.
        const int spacing = figures[2].holder.root->x - figures[1].holder.root->x;
        figures[0].holder.root->MoveWindow(-spacing, 0);
        figures[8].holder.root->MoveWindow(spacing, 0);
        dragArea = mask;
        dragRect = {mask->x, mask->y, mask->x + mask->w, mask->y + mask->h};
        // The eight small figures wear the centre one's bodies, scaled by their widths.
        const Shared::PlayerHolder& centre = figures[8].holder;
        for (unsigned i = 0; i != 8; ++i) {
            Figure& f = figures[i];
            f.f1b4 = 1.f;
            f.f1b8 = 1.f;
            f.bodyScale = (float)f.holder.body->w / (float)centre.body->w;
            f.holder.root->SetVisibility(false);
            f.holder.body->SetTexture(centre.body->texture, false);
            f.holder.bodyFemale->SetTexture(centre.bodyFemale->texture, false);
        }
        squadTitle = GUI::GetWindowTypedF<Textfield>(w, "%s.text_enemy_squad", p);
        squadFront = GUI::GetWindowTypedF<Textfield>(w, "%s.text_squad_front", p);
        for (unsigned i = 0; i != 3; ++i) {
            std::snprintf(q, sizeof q, "%s.slot_warrior_%02d", p, i + 1);
            slots[i].LoadFrom(w, q);
        }
        std::snprintf(q, sizeof q, "%s.button_pvp_start", p);
        start.LoadFrom(w, q);
        vicDef = GUI::GetWindowTypedF<Window>(w, "%s.pvp_vic_def_holder", p);
        vicDefIcon = GUI::GetWindowTypedF<Window>(w, "%s.pvp_vic_def_holder.icon_vic_def", p);
        vicDefIcon->takesZ = true;
        victory = GUI::GetWindowTypedF<Textfield>(w, "%s.pvp_vic_def_holder.text_victory", p);
        defeat = GUI::GetWindowTypedF<Textfield>(w, "%s.pvp_vic_def_holder.text_defeat", p);
        vicDefExp = GUI::GetWindowTypedF<Textfield>(w, "%s.pvp_vic_def_holder.text_vic_def_exp", p);
        statsBg = GUI::GetWindowTypedF<Window>(w, "%s.sliced_crem_bg_02", p);
        statsTitle = GUI::GetWindowTypedF<Textfield>(w, "%s.text_enemy_stats", p);
        for (unsigned i = 0; i != 4; ++i) {
            std::snprintf(q, sizeof q, "%s.pvp_stat_%02d", p, i + 1);
            stats[i].LoadFrom(w, q);
        }
        separator = GUI::GetWindowTypedF<Window>(w, "%s.sepparator_01", p);
    }
};

WindowManager::FunctionalWindow* g_queue = nullptr;   // 0x619e10
Window* g_root = nullptr;                 // 0x627398
GUI::TweenEffect* g_tween = nullptr;      // 0x628838
Window* g_close = nullptr;                // 0x629de0 x_button
TabShop g_tabs[6];                        // 0x629c34
Window* g_header = nullptr;               // 0x629de4 header (tabs 0 .. 2)
Textfield* g_headerText = nullptr;        // 0x629de8 text_player_name
Window* g_header2 = nullptr;              // 0x629dec header_02 (tabs 3 .. 5)
Textfield* g_title = nullptr;             // 0x629ddc text_player_name_02
EnemyHolder g_enemy;                      // 0x62739c pvp_part_enemy
std::vector<Window*> g_socialHolders;     // PORT: the social tabs' holders' roots, only to hide them
ItemHoverWindow* g_hover = nullptr;       // 0x626388 a squad warrior's hint
Window* g_itemHolder = nullptr;           // 0x629c2c Character_item_holder.xml (the figures' pieces)
unsigned g_tab = 0;                       // 0x626364 the tab shown
int g_selectedGID = 0;                    // 0x626368
bool g_selectionFetched = false;          // 0x62636c
uint32_t g_defeated = 0;                  // 0x626370
int g_steps = 0;                          // 0x626374 the figures the carousel still has to turn (+: left)
unsigned g_centre = 4;                    // 0x626378 the list's offset of the shown opponent
bool g_queued = false;                    // 0x62637c a fight was chosen: the arena on closing
bool g_dataDownload = false;              // 0x626380 the opponent's data must be downloaded (online)
int g_mode = 1;                           // 0x60f320 the list type shown (PvP::PvPPlayerType)
int g_lastTab = -1;                       // 0x60f324
float g_scale = 1.f;                      // 0x60f32c the root's scale
float g_stepWidth = 10.f;                 // 0x60f334 a figure's turn in carousel units
float g_stepSpeed = 40.f;                 // 0x60f338 the turning speed without a fling
int g_offset = 0;                         // 0x6287cc the carousel's position in the list
bool g_pressed = false;                   // 0x6287e4
bool g_dragging = false;                  // 0x6287e5
int g_dragX = 0;                          // 0x6287e8
float g_velocity = 0.f;                   // 0x629c24 a fling's speed
float g_subStep = 0.f;                    // 0x629c28 the turn within the current figure (-W..W)
bool g_loadFailed[6] = {};                // 0x629d9c per list type: its last load failed
std::vector<PvP::PlayerInfo*> g_list;     // 0x629da4 the opponents of the type shown
unsigned g_paidRefresh = 0;               // 0x629df0 the crystals of the last paid refresh
int g_fightGID = 0;                       // 0x629df4 the opponent OnFight takes (0: the shown one)
bool g_629dfc = false;                    // 0x629dfc (Show clears it)
float g_time = 0.f;                       // 0x629e08 the window's time
int g_lastSecond = 0;                     // Update's last wall-clock second
bool g_returnWithoutWindow = false;

// The opponent the carousel shows (null: an empty list).
PvP::PlayerInfo* ShownPlayer() {
    if (g_list.empty()) return nullptr;
    return g_list[(unsigned)(g_centre + g_offset) % g_list.size()];
}

// @0x302658: a tab's look: its icon raised while active, the count bubble.
void SetTabMode(TabShop& tab, bool active, unsigned count) {
    tab.icon->RestorePosition();
    if (active && tab.icon->parent) tab.icon->MoveWindow(0, (int)(tab.icon->parent->scale * -4.f));
    tab.icon->UpdatePosition();
    tab.active->SetVisibility(active);
    tab.inactive->SetVisibility(!active);
    tab.count.root->SetVisibility(count != 0);
    tab.count.text->SetText(ToWideString((int)count));
}

void OnLeft() { ++g_offset; }                                  // @0x301f9c
void OnRight() { --g_offset; }                                 // @0x301fb8
void OnPerson(unsigned i) { g_steps += (int)g_centre - (int)i; }   // @0x301fd4 the carousel turns to figure i

// @0x301ffc
float ZRange() { return g_queue->shown ? 0.2f : 0.001f; }

// @0x302bc8 the valor's page in LevelInfoWindow.
void OnValor() {
    LevelInfoWindow::Show();
    if (const Professions::Profession* p = Professions::GetProfession(7)) LevelInfoWindow::OnTab(p->tab, true);
}

// @0x302cd0 the item shop's valor tab.
void OnValorShop() {
    ItemShopWindow::Show();
    ItemShopWindow::ShowShopTab(7);
}

// @0x302a90 the hint of the shown opponent's warrior in slot i.
void OnWarrior(unsigned i) {
    Shared::WarriorSlotInfo& s = g_enemy.slots[i];
    EntityData* d = EntityFactory::GetEntityByID(s.soldier);
    if (!d) return;
    // UNVERIFIED (milestone 6): SoundsManager::PlaySound("ui_click", 1, false).
    g_hover->Show();
    g_hover->SetEntity(nullptr, d, false, false);
    const int x = g_root->x + s.root->x;
    const int y = g_root->y + s.root->y;
    g_hover->SetTargetRect(x, y, x + s.root->w, y + s.root->h, ItemHoverWindow::kS);
    g_hover->RecomputePosition();
    g_hover->SetZ(0.001f);
    Render::SortRenderLayer(0xe, 1);
}

// @0x308d28 the shown opponent's profile, once fought (beaten or won against).
void OnInfo() {
    PvP::PlayerInfo* p = ShownPlayer();
    if (!p) return;
    if (p->defeated != 0) {
        PlayerInfoWindow::SetParameters(p->gid, true);
        PlayerInfoWindow::Show();
        return;
    }
    PopupWindow::Show(StringTable::GetString("PVP_BATTLE_TO_SHOW_INFO"), PopupWindow::Hide, nullptr, nullptr);
}

// @0x309148 the shown opponent (or g_fightGID's) becomes the selected one, his valor is worked out
// (once the PvP tutorial is done) and the window closes: OnTweenOut sends the hero to the arena. A
// dead hero is healed first (HealSquadWindow), then tries again. (The original's PlayerWeakWindow
// check of an empty squad or belt first: removed, user 2026-10-18.)
void OnFight() {
    BeltBarWindow::UpdateBeltItems();
    PvP::PlayerInfo* p = g_fightGID == 0 ? ShownPlayer() : PvP::GetPlayerInfoByGID(g_fightGID);
    if (p) {
        if (PvP::IsCompletedPvPTutorial()) p->valor = (uint32_t)p->GetValorForVictory();
        g_selectedGID = p->gid;
        g_fightGID = 0;
    }
    if (EntityManager::GetPlayer()->IsDead()) {
        HealSquadWindow::Show();
        HealSquadWindow::ResetRecentDeaths();
        HealSquadWindow::SetHealPostAction([] { OnFight(); });
        return;
    }
    g_queued = true;
    Hide();
}

// @0x309310 a list's load ended (ok: it came). The tutorial's shadow 1 is selected while unbeaten;
// a looked-up player (type 5) is shown or fought. Offline the loads never fail.
void DataDownloadCallback(bool ok) {
    if (!ok) {
        if (g_tab == 4) {
            // (LoadingInternetWindow::Hide: online)
            PopupWindow::Show(StringTable::GetString("FB_ONLINE_NOTICE"), PopupWindow::Hide, nullptr, nullptr);
            g_loadFailed[g_mode] = true;
        }
        PopupWindow::Show(StringTable::GetString("FB_ONLINE_NOTICE"), PopupWindow::Hide, nullptr, nullptr);
        return;
    }
    PvP::PlayerInfo* shadow = PvP::GetPlayerInfoByGID(1);
    if (shadow && shadow->defeated != 1 && g_selectedGID == 0 && shadow->type == 1) g_selectedGID = 1;
    RefreshCurrentView();
    g_loadFailed[g_mode] = false;
    if (g_tab != 0 && g_tab != 3 && g_tab != 2 && PvP::GetLoadedDataType() != 5) return;
    if (PvP::GetRandomPlayerGID(5, true) == 0) return;
    if (g_fightGID != 0) {
        OnFight();
    } else {
        PlayerInfoWindow::SetParameters(PvP::GetRandomPlayerGID(5, true), true);
        PlayerInfoWindow::Show();
    }
    // (LoadingInternetWindow::Hide: online)
}

// @0x31d300 the list is asked for again (free). (GameState::SetPvPRefreshTime and the loading
// window: online.)
void OnRefreshFree() {
    if (PvP::IsLoadingData()) return;
    PvP::RemoveCachedData(g_mode, g_loadFailed[g_mode]);
    PvP::RefreshPlayers(g_mode);
    RefreshCurrentView();
    UpdateContents();
}

// @0x31d3b0 the early refresh for "pvp_refresh_cost" crystals (0 offline, DECIDED 2026-10-20),
// confirmed first (pay = false).
void OnRefresh(bool pay) {
    if (PvP::IsLoadingData()) return;
    const unsigned cost = (unsigned)Setting("pvp_refresh_cost").GetInt();
    NotEnoughWindow::ResetRequirements();
    NotEnoughWindow::AddRequirement(GameState::kCrystal, cost);
    if (!NotEnoughWindow::CheckRequirements()) {
        NotEnoughWindow::Show();
        return;
    }
    if (!pay) {
        ConfirmPurchaseWindow::SetParameters([] { OnRefresh(true); }, cost, true);
        ConfirmPurchaseWindow::Show();
        return;
    }
    PvP::RemoveCachedData(g_mode, g_loadFailed[g_mode]);
    g_paidRefresh = cost;
    PvP::RefreshPlayers(g_mode);
    RefreshCurrentView();
    UpdateContents();
}

// @0x302e94
bool OnBack() {
    if (!g_queue->shown) return false;
    g_selectedGID = 0;
    if (g_hover->shownHover) g_hover->Hide();
    else Hide();
    return true;
}

// @0x31d7b4 (the invite tab's return to the last tab: online)
void OnClose() { OnBack(); }

// @0x302f08: the hint and the arrow first; a press inside the carousel starts a drag, its release
// flings the carousel (over 400 px/s, at most 2000) or settles it on the nearer figure. (The social
// tabs' scrollers: online.)
bool Click(int x, int y, bool pressed) {
    if (!g_root->visibleSelf) {
        g_root->Click(x, y, pressed, false);
        return false;
    }
    if (BuildingHovers::ClickOnArrow(x, y, pressed)) return true;
    if (!pressed) BuildingHovers::HideArrow();
    if (g_hover->Click(x, y, pressed)) return true;
    if (pressed) {
        if (g_enemy.root->visible) {
            const EnemyHolder& e = g_enemy;
            if (g_root->x + e.dragRect.left <= x && x <= g_root->x + e.dragRect.right &&
                g_root->y + e.dragRect.top <= y && y <= g_root->y + e.dragRect.bottom) {
                g_dragX = x;
                g_pressed = true;
            }
        }
    } else {
        g_pressed = false;
        if (g_dragging) {
            g_dragging = false;
            const float v = GUI::GetMouseSpeed(true) / g_scale;
            g_velocity = v;
            if (v < -2000.f || v > 2000.f) {
                g_velocity = v < -2000.f ? -2000.f : 2000.f;
                g_steps = 0;
                return true;
            }
            if (std::fabs(v) > 400.f) {
                g_steps = 0;
                return true;
            }
            g_velocity = 0.f;
            if (g_subStep > 0.f) ++g_steps;
            if (g_subStep < 0.f) --g_steps;
            return true;
        }
    }
    const bool visible = g_root->visibleSelf;
    g_root->Click(x, y, pressed, false);
    return visible;
}

// @0x31df1c: a press moved 11 px or more turns into a drag; the drag turns the carousel by 0.05
// units a pixel, a figure every g_stepWidth. (The social scrollers: online.)
void Move(int x, int y) {
    (void)y;
    if (g_pressed && std::abs(x - g_dragX) >= 11) {
        g_dragging = true;
        g_velocity = 0.f;
        g_steps = 0;
    } else if (!g_dragging) {
        return;
    }
    g_subStep += (float)(x - g_dragX) * 0.05f;
    if (g_subStep > g_stepWidth) {
        g_subStep -= g_stepWidth;
        --g_offset;
    }
    if (g_subStep < -g_stepWidth) {
        g_subStep += g_stepWidth;
        ++g_offset;
    }
    g_dragX = x;
    UpdateContents();
}

// @0x31ba54: the carousel's motion: a fling slows down by 2000 px/s² and settles on the nearer
// figure; queued turns (arrows, figure taps) run at a speed that follows their count. The contents
// are refreshed once a wall-clock second. (The social tabs' search fields: online.)
void Update(float dt) {
    g_time += dt;
    if (PvP::IsLoadingData() || !g_queue->shown) return;
    float v = g_velocity;
    bool moved = false;
    if (std::fabs(v) > 400.f || (v != 0.f && g_steps == 0)) {
        float p = g_subStep + v * 0.05f * dt;
        g_subStep = p;
        if (p > g_stepWidth) {
            p -= g_stepWidth;
            --g_offset;
            g_subStep = p;
        }
        if (p < -g_stepWidth) {
            ++g_offset;
            g_subStep = p + g_stepWidth;
        }
        bool settle;
        if (v > 0.f) {
            v += dt * -2000.f;
            g_velocity = v;
            if (v < 0.f) {
                g_velocity = 0.f;
                settle = true;
            } else {
                settle = std::fabs(v) <= 400.f;
            }
        } else if (v < 0.f) {
            v += dt * 2000.f;
            g_velocity = v;
            if (v >= 0.f) {
                g_velocity = 0.f;
                settle = true;
            } else {
                settle = std::fabs(v) <= 400.f;
            }
        } else {
            settle = true;
        }
        if (settle) {
            if (g_subStep > 0.f) ++g_steps;
            if (g_subStep < 0.f) --g_steps;
        }
        UpdateContents();
        moved = true;
    }
    if (g_steps != 0 || !moved) {
        const int steps = g_steps;
        if (steps != 0) {
            const float target = std::fabs((float)steps * 40.f);
            if (target > g_stepSpeed) {
                g_stepSpeed += dt * 40.f;
                if (g_stepSpeed > target) g_stepSpeed = target;
            } else if (target < g_stepSpeed) {
                g_stepSpeed += dt * -40.f;
                if (g_stepSpeed < target) g_stepSpeed = target;
            }
            const float speed = g_velocity != 0.f ? std::fabs(g_velocity * 0.05f) : g_stepSpeed;
            if (steps > 0) {
                g_subStep += speed * dt;
                if (g_subStep > g_stepWidth) {
                    g_steps = steps - 1;
                    --g_offset;
                    g_velocity = 0.f;
                    g_subStep = g_steps == 0 ? 0.f : g_subStep - g_stepWidth;
                }
            } else {
                g_subStep -= speed * dt;
                if (g_subStep < -g_stepWidth) {
                    g_steps = steps + 1;
                    ++g_offset;
                    g_velocity = 0.f;
                    g_subStep = g_steps == 0 ? 0.f : g_subStep + g_stepWidth;
                }
            }
            UpdateContents();
        }
    }
    if (Timer::GetGlobalTime() == g_lastSecond) return;
    g_lastSecond = Timer::GetGlobalTime();
    UpdateContents();
}

// @0x31de7c
void SetZ(float z) {
    if (!g_queue->shown) return;
    g_root->SetZ(z);
    if (!g_tween->active) g_root->SetPosition(g_tween->GetCenteredX(), g_tween->GetCenteredY());
    UpdateContents();
}

// @0x31b738: once the three shadows of the PvP tutorial are beaten, the tutorial is done and the
// real opponents come in ten seconds. (The loading window while a list loads: online.)
void OnTweenIn() {
    if (!PvP::IsCompletedPvPTutorial() && PvP::GetTutorialDeadPlayers() == (int)PvP::GetTutorialPlayerCount()) {
        PvP::SetCompletedPvPTutorial(true);
        PvP::SetRefreshTime(1, (uint32_t)Timer::GetGlobalTime() + 10);
        PvP::SetLastLoadedTime(1, (uint32_t)Timer::GetGlobalTime());
    }
    UpdateContents();
}

// @0x3025ac: with a fight chosen (and the hero alive) the world map opens (unless it is up or the
// hero is on map 0x74) and travels to the arena, where GlobalMapWindow starts the fight.
void OnTweenOut() {
    WindowManager::WindowHide(false);
    g_queue->shown = false;
    g_queue->MoveWindowDown(true);
    GameState::DropGamePauseState();
    if (!g_queued || g_selectedGID == 0) return;
    if (!PvP::GetPlayerInfoByGID(g_selectedGID, g_mode)) return;
    if (EntityManager::GetPlayer()->IsDead()) return;
    if (!GlobalMapWindow::IsVisible() && GameState::GetCurrentMapID() != 0x74) GlobalMapWindow::Show();
    GlobalMapWindow::ShowPvPAnimation();
}

}  // namespace

WindowManager::FunctionalWindow* Queue() {
    if (!g_queue) {   // static FunctionalWindow "PlayerCompetitionWindow"
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.click = Click;
        f.move = Move;
        f.setZ = SetZ;
        f.hide = Hide;
        g_queue = new WindowManager::FunctionalWindow("PlayerCompetitionWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_queue && g_queue->shown; }   // @0x301f18

// @0x301ea0 the list type, with its tab (2: the friends', 1: the PvP tab).
void SetViewMode(int type) {
    g_mode = type;
    if (type == 2) g_tab = 1;
    else if (type == 1) g_tab = 4;
}

void SetCurrentView(int type) { g_mode = type; }   // @0x302050
int GetCurrentView() { return g_mode; }            // @0x302040
bool CombatQueued() { return g_queued; }           // @0x30202c

// @0x306120: the PvP tab's parts (the social tabs' holders: online, not loaded).
void Init() {
    for (bool& f : g_loadFailed) f = false;
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Window_pvp.xml", "", -1.5f, 0, 0, 0, 0, false, 1.f);
    if (!g_root) return;
    Queue()->RegisterTopWindow(g_root);
    g_scale = g_root->scale;
    g_root->SetVisibility(false);
    g_tween = GUI::CreateTweenEffect(g_root, nullptr, OnTweenIn, OnTweenOut);
    g_tween->insetB = (int)(g_root->scale * -90.f);
    g_close = GUI::GetWindowTyped<Window>(g_root, "x_button");
    g_close->SetOnClick(OnClose);
    g_close->SetActionSound("ui_close", 1);
    for (unsigned i = 0; i != 6; ++i) {
        char p[16];
        std::snprintf(p, sizeof p, "tab_%02d", i + 1);
        TabShop& t = g_tabs[i];
        t.LoadFrom(g_root, p);
        t.root->MoveWindow(0, 1);
        t.root->SetOnClick([i] { OnTab(i); });
        t.root->SetActionSound("ui_tab", 1);
    }
    static const char* const kTabIcons[6] = {"activity_list_35", "friend_list_35", "pvp_won_35",
                                             "gift_35",          "pvp_list_35",    "add_person"};
    // (the original sets them in the order 0, 1, 3, 4, 2, 5)
    for (unsigned i = 0; i != 6; ++i) g_tabs[i].icon->SetTexture(IconManager::GetIcon(kTabIcons[i], false), true);
    g_header = GUI::GetWindowTyped<Window>(g_root, "header");
    g_headerText = GUI::GetWindowTyped<Textfield>(g_root, "text_player_name");
    g_header2 = GUI::GetWindowTyped<Window>(g_root, "header_02");
    g_title = GUI::GetWindowTyped<Textfield>(g_root, "text_player_name_02");
    // (the social tabs' holders: online; only their roots are kept, hidden by UpdateContents)
    for (const char* n : {"webview_activity_holder", "webview_friends_holder", "webview_friend_selector",
                          "webview_leaderboard_holder", "webview_friend_inviter"})
        g_socialHolders.push_back(GUI::GetWindowTyped<Window>(g_root, n));
    g_enemy.LoadFrom(g_root, "pvp_part_enemy");
    EnemyHolder& e = g_enemy;
    e.start.gold->SetVisibility(false);
    e.valor.root->SetOnClick(OnValor);
    e.valor.info->SetOnClick(OnValor);
    e.valor.shop->SetOnClick(OnValorShop);
    e.refresh->SetOnClick([] { OnRefresh(false); });
    e.refresh->SetVisibility(false);
    e.refreshFree->SetOnClick(OnRefreshFree);
    e.info->SetOnClick(OnInfo);
    e.info->SetVisibility(false);
    e.left->SetOnClick(OnLeft);
    e.right->SetOnClick(OnRight);
    g_centre = 4;
    for (unsigned i = 0; i != 9; ++i)
        if (i != g_centre) e.figures[i].holder.root->SetOnClick([i] { OnPerson(i); });
    for (unsigned i = 0; i != 3; ++i) {
        Shared::WarriorSlotInfo& s = e.slots[i];
        s.root->SetOnClick([i] { OnWarrior(i); });
        s.root->SetActionSound("ui_click", 1);
        s.root->SetVisibility(false);
        s.hpIcon->SetVisibility(false);
    }
    e.start.root->SetOnClick(OnFight);
    e.start.root->SetVisibility(false);
    // (the social tabs' buttons and scrollers: online)
    g_queue->fn.show = Show;
    g_queue->fn.back = OnBack;
    g_queue->fn.update = Update;
    g_queue->fn.zRange = ZRange;
    g_queue->hideOnOuterClick = true;
    g_stepWidth = 10.f;
    g_hover = new ItemHoverWindow();
    g_itemHolder = GUI::RegisterUI("../resource/kingdom_icons/1Original/Character_item_holder.xml", "", 1.f, 0, 0, 0, 0,
                                   false, 1.f);
    g_list.clear();
    g_selectedGID = 0;
    g_tab = 0;
    g_paidRefresh = 0;
}

// @0x31daf8: on the list type's tab (1 .. 3, else 1). The tutorial's shadow 1 is selected while
// unbeaten. (The Facebook profile check: online.)
void Show() {
    if (g_mode - 1u > 2) g_mode = 1;
    g_629dfc = false;
    g_queue->shown = true;
    g_queue->MoveWindowOnTop(true);
    if (!g_root->visibleSelf) {
        WindowManager::WindowShow(false);
        g_tween->AnimateIn();
        GameState::RaiseGamePauseState();
    }
    g_root->SetVisibility(true);
    g_queued = false;
    PvP::PlayerInfo* shadow = PvP::GetPlayerInfoByGID(1);
    if (shadow && shadow->defeated != 1 && g_selectedGID == 0 && shadow->type == 1) g_selectedGID = 1;
    // (the social scrollers' ScrollToTop: online)
    RefreshCurrentView();
    UpdateContents();
    g_root->PreloadTextures();   // GUI::PreloadWindow
}

// @0x302d00 (the social notifications' reset: online)
void Hide() {
    if (g_root && g_root->visibleSelf) g_tween->AnimateOut();
}

// @0x31d514: tab 4 shows the PvP list (type 1), tab 1 the friends' (2), the others type 5. (The
// social tabs' bookkeeping: online.)
void OnTab(unsigned tab) {
    if (PvP::IsLoadingData() && (tab == 1 || tab == 4)) return;
    if (g_tab == tab) return;
    if (tab == 1) g_mode = 2;
    else if (tab == 4) g_mode = 1;
    else g_mode = 5;
    if (tab < 3) g_lastTab = (int)tab;
    g_tab = tab;
    RefreshCurrentView();
    UpdateContents();
}

// @0x305874: the view's fixed texts and looks, everything hidden that UpdateContents shows; the
// list of the type shown, and the carousel set on the selected opponent.
void RefreshCurrentView() {
    EnemyHolder& e = g_enemy;
    if (g_tab == 4) g_title->SetText(StringTable::GetString("PVP_TITLE_ALT"));
    for (Figure& f : e.figures) f.holder.root->SetVisibility(false);
    e.squadTitle->SetText(StringTable::GetString(g_tab == 1 ? "PVP_SQUAD_FRIEND" : "PVP_SQUAD"));
    e.start.root->SetVisibility(false);
    e.start.icon->SetTexture(IconManager::GetIcon("gold_attack", false), true);
    e.start.lock->SetVisibility(false);
    e.start.crystalIcon->SetVisibility(false);
    e.start.text->SetText(StringTable::GetString("PVP_FIGHT"));
    e.vicDef->SetVisibility(false);
    e.statsTitle->SetText(StringTable::GetString("PVP_STATS"));
    e.refreshFreeText->SetText(StringTable::GetString("PVP_REFRESH"));
    static const char* const kStatIcons[4] = {"pvp_won_35", "pvp_lost_35", "pvp_valor_35", "xp"};
    static const char* const kStatTexts[4] = {"PVP_VICTORIES", "PVP_DEFEATS", "PVP_RANK", "PVP_LEVEL"};
    for (StatisticHolder& s : e.stats) s.root->SetVisibility(true);
    for (unsigned i = 0; i != 4; ++i) e.stats[i].icon->SetTexture(IconManager::GetIcon(kStatIcons[i], false));
    for (unsigned i = 0; i != 4; ++i) e.stats[i].text->SetText(StringTable::GetString(kStatTexts[i]));
    for (StatisticHolder& s : e.stats) s.number->SetVisibility(false);
    e.refresh->SetVisibility(false);
    e.refreshFree->SetVisibility(false);
    e.refreshText->SetVisibility(false);
    e.refreshPrice->SetVisibility(false);
    e.name1->SetVisibility(false);
    e.name2->SetVisibility(false);
    e.paramBack->SetVisibility(false);
    e.timerIcon->SetVisibility(false);
    e.statNum->SetVisibility(false);
    for (Shared::WarriorSlotInfo& s : e.slots) {
        s.root->RestorePosition();
        s.root->SetVisibility(false);
        s.soldier = 0;
    }
    e.vicDef->SetVisibility(false);
    e.victory->SetVisibility(false);
    e.defeat->SetVisibility(false);
    e.info->SetVisibility(false);
    g_list.clear();
    for (unsigned i = 0;; ++i) {
        PvP::PlayerInfo* p = PvP::EnumPlayers(i);
        if (!p) break;
        if (p->type == g_mode) g_list.push_back(p);
    }
    for (unsigned i = 0; i != g_list.size(); ++i) {
        if (g_list[i]->gid == g_selectedGID) {
            g_offset = (int)i - (int)g_centre;
            break;
        }
    }
}

// @0x30a720 (69 KB, all tabs; the PvP tab here): the tabs' looks, the headers and holders of the
// tab shown, then the tab's contents.
void UpdateContents() {
    for (unsigned i = 0; i != 6; ++i) SetTabMode(g_tabs[i], g_tab == i, 0);   // (tab 0's count: online)
    // (the friends' registration and the gift rotation: online)
    const bool friends = GameState::TaskCompleted((uint32_t)Setting("task_friends_enable").GetInt());
    const bool pvp = GameState::TaskCompleted(0x74d);
    g_close->SetVisibility(true);
    g_header->SetVisibility(g_tab <= 2);
    g_headerText->SetVisibility(g_header->visibleSelf);
    g_header2->SetVisibility(g_tab - 3u <= 2);
    g_title->SetVisibility(g_header2->visibleSelf);
    // (the social tabs' holders, shown on their tabs: online, hidden)
    for (Window* w : g_socialHolders) w->SetVisibility(false);
    g_enemy.root->SetVisibility(g_tab == 4);
    for (TabShop& t : g_tabs) t.root->SetVisibility(true);
    // Only the activity, friends and top players' tabs show, over their own header.
    for (unsigned i = 0; i != 3; ++i) g_tabs[i].root->SetVisibility(g_header->visibleSelf);
    for (unsigned i = 3; i != 6; ++i) g_tabs[i].root->SetVisibility(false);
    if (GameState::IsPlayerCity()) {
        g_tabs[0].root->SetEnabled(friends);
        g_tabs[1].root->SetEnabled(friends);
        g_tabs[3].root->SetEnabled(friends);
        g_tabs[4].root->SetEnabled(pvp);
        g_tabs[2].root->SetEnabled(true);
        g_tabs[5].root->SetEnabled(friends);
    } else {
        g_tabs[0].root->SetEnabled(false);
        g_tabs[1].root->SetEnabled(false);
        g_tabs[3].root->SetEnabled(true);
        g_tabs[4].root->SetEnabled(false);
        g_tabs[2].root->SetEnabled(false);
    }
    if (g_tab != 4) return;   // (the other tabs: online)
    // (the loading window: online)
    if (PvP::GetLastLoadedTime(g_mode) == 0 || g_dataDownload) {
        PvP::LoadPvPData(g_mode, DataDownloadCallback, (int)g_paidRefresh, nullptr);
        g_paidRefresh = 0;
        g_dataDownload = false;
    }
    EnemyHolder& e = g_enemy;
    // @0x30ac2c the valor bar: rank, points towards the next rank.
    const uint32_t level = Professions::GetPlayerProfessionLevel(7);
    const Professions::ProfessionLevel* cur = Professions::GetProfessionLevel(7, level);
    const Professions::ProfessionLevel* next = Professions::GetProfessionLevel(7, level + 1);
    e.valor.icon->SetTexture(IconManager::GetIcon("rep_pvp_valor_60", false), true);
    e.valor.level->SetVisibility(level != 0);
    e.valor.level->SetText(ToWideString((int)level));
    e.valor.shopText->SetText(StringTable::GetString("GO_TO_SHOP"));
    e.valor.stat->SetText(SWPrintf(0x100, StringTable::GetString("PVP_VALOR_NAME"),
                                   {cur ? cur->name : StringTable::GetString("RANK_NONE")})
                              .c_str());
    const uint32_t points = GameState::GetProfessionPoints(7);
    const uint32_t nextPoints = next ? next->points : 1;
    const std::u32string bar =
        SWPrintf(0x100, U"%s: %d/%d", {StringTable::GetString("HINT_VALOR"), (int)points, (int)nextPoints});
    e.valor.barBg->SetText(bar.c_str());
    e.valor.barText->SetText(bar.c_str());
    const uint32_t from = cur ? cur->points : 0;
    e.valor.SetProgress((float)(points - from) / (float)(nextPoints - from));
    e.valor.root->UpdatePosition();
    g_root->UpdatePosition();
    if (PvP::IsLoadingData()) return;   // (the loading window: online)
    if (PvP::GetLastLoadedTime(g_mode) == 0 || g_dataDownload) {
        PvP::LoadPvPData(g_mode, DataDownloadCallback, (int)g_paidRefresh, nullptr);
        g_paidRefresh = 0;
        g_dataDownload = false;
        return;
    }
    // @0x311a3c the list's controls: the refresh timer and buttons (list 1), the record and Fight
    // once there are opponents. (List 2, the friends', and its invite button: online.)
    const bool pvpList = g_mode == 1;
    const bool any = !g_list.empty();
    e.refresh->SetVisibility(pvpList);
    e.refreshFree->SetVisibility(g_mode == 2);
    e.refreshText->SetVisibility(pvpList);
    e.refreshPrice->SetVisibility(pvpList);
    e.paramBack->SetVisibility(pvpList);
    e.timerIcon->SetVisibility(pvpList);
    e.statNum->SetVisibility(pvpList);
    for (StatisticHolder& s : e.stats) s.number->SetVisibility(any);
    e.start.root->SetVisibility(any);
    e.name1->SetVisibility(any);
    e.name2->SetVisibility(any);
    const int left = (int)PvP::GetRefreshTime(g_mode) - Timer::GetGlobalTime();
    e.paramBack->SetVisibility(pvpList);
    e.timerIcon->SetVisibility(pvpList);
    e.timerIcon->SetTexture(IconManager::GetSandClockIcon(), false);
    e.statNum->SetVisibility(pvpList);
    e.statNum->SetText(SWPrintf(0x100, StringTable::GetString("PVP_REFRESH_IN"),
                                {StringTable::GetTimeString(left < 1 ? 1 : left, false).c_str()})
                           .c_str());
    e.refresh->SetVisibility(pvpList);
    e.refreshText->SetText(SWPrintf(0x100, U"%s:", {StringTable::GetString("PVP_REFRESH")}).c_str());
    e.refreshPrice->SetText(ToWideString(Setting("pvp_refresh_cost").GetInt()));
    e.refreshFree->SetVisibility(g_mode == 2 || g_loadFailed[g_mode]);
    e.refreshFreeText->SetText(StringTable::GetString(g_mode == 2 ? "PVP_INVITE_FRIENDS" : "PVP_REFRESH"));
    e.refreshFreeIcon->SetTexture(IconManager::GetIcon(g_mode == 2 ? "add_person" : "gold_refresh", false), true);
    e.backdrop->SetVisibility(false);
    e.day1->SetVisibility(false);
    e.day2->SetVisibility(false);
    e.error->SetVisibility(false);
    if (left <= 0 && pvpList) g_dataDownload = true;   // the list is due: asked for again
    // The carousel: figure i shows the opponent at (i + g_offset) in the list, on a circle; its
    // angle (i - g_centre) * g_stepWidth + g_subStep degrees moves it sideways (sin) and its depth
    // (cos) scales it (never below figure 2's) and lowers it (at most 250 px).
    const Window* mask = e.mask;
    const float minScale = e.figures[2].bodyScale;
    for (unsigned i = 0; i != 9; ++i) {
        Figure& f = e.figures[i];
        Shared::PlayerHolder& h = f.holder;
        PvP::PlayerInfo* p = g_list.empty() ? nullptr : g_list[(unsigned)(i + g_offset) % g_list.size()];
        if (!p) {
            h.root->SetVisibility(false);
            continue;
        }
        h.root->SetVisibility(true);
        f.offset = (float)((int)i - (int)g_centre) * g_stepWidth;
        const float angle = (f.offset + g_subStep) / 180.f * 3.1415927f;
        const float c = std::cos(angle);
        const float s = std::sin(angle);
        float depth = (c - 0.95f) / 0.05f;
        if (minScale > depth) depth = minScale;
        f.f1b4 = depth / f.bodyScale;
        float dy = g_scale * 75.f * (depth - 0.705f);
        if (dy < 0.f) dy = 0.f;
        else if (dy > 250.f) dy = 250.f;
        const float x = (float)mask->x + (float)mask->w * 0.5f + (float)mask->w * s + (float)h.root->w * -0.5f;
        const float y = (float)mask->y + (float)mask->h * 0.5f + dy + (float)h.root->h * -0.5f;
        h.root->SetPosition((int)x, (int)y);
        h.body->SetVisibility(p->male);
        h.bodyFemale->SetVisibility(!p->male);
        const float scale = f.bodyScale * f.f1b4;
        const float dx = (float)h.root->w * -0.5f * (f.f1b4 - 1.f);
        const float dyRoot = (float)h.root->h * -0.5f * (f.f1b4 - 1.f);
        // Each body picture at its scaled place in the figure, its sprite set to match.
        auto placeBody = [&](Window* body, int bx, int by) {
            const float w = (float)body->texture->w * scale * g_scale;
            const float hgt = g_scale * (float)body->texture->h * scale;
            const float px = (float)bx * scale / f.bodyScale + x + dx;
            const float py = (float)by * scale / f.bodyScale + y + dyRoot;
            body->SetSize((unsigned)(int)w, (unsigned)(int)hgt);
            body->SetPosition((int)px, (int)py);
            body->SetTexture(body->texture, false, 0, 0, true, 0);
            Render::Sprite* sp = body->sprite;
            sp->h = hgt;
            sp->w = w;
            sp->x = (float)g_root->x + px + 0.5f;
            sp->y = (float)g_root->y + py + hgt + 0.5f;
        };
        placeBody(h.body, h.bodyX, h.bodyY);
        placeBody(h.bodyFemale, h.bodyFemaleX, h.bodyFemaleY);
        h.PlacePlayerEquipment(p->equipment, x, y, g_itemHolder, p->male, scale, dx, dyRoot,
                               (depth - minScale) * -0.007f, false);
        // A shadow of the PvP tutorial already fought, and a beaten opponent, cannot be picked.
        bool enabled = true;   // (list 2: a friend fought within "friend_fight_timeout": online)
        if (pvpList && !PvP::IsCompletedPvPTutorial() && p->lastFight != 0) enabled = false;
        if (p->defeated == 1) enabled = false;
        h.root->SetEnabled(enabled);
    }
    // The shown opponent: his squad (centred), name, record, rank and level, and Fight or his
    // defeat.
    if (PvP::PlayerInfo* p = ShownPlayer()) {
        unsigned count = 0;
        for (unsigned k = 0; k != 3; ++k) {
            Shared::WarriorSlotInfo& s = e.slots[k];
            s.root->RestorePosition();
            EntityData* d = p->warriors[k].id ? EntityFactory::GetEntityByID(p->warriors[k].id) : nullptr;
            if (!d) {
                s.root->SetVisibility(false);
                s.soldier = 0;
                continue;
            }
            ++count;
            s.root->SetVisibility(true);
            s.soldierFine->SetTexture(d->side0, true);
            s.lockIcon->SetVisibility(false);
            s.soldier = d->id;
        }
        const int spacing = e.slots[1].root->x - e.slots[0].root->x;
        for (Shared::WarriorSlotInfo& s : e.slots) s.root->MoveWindow(spacing * (3 - (int)count) >> 1, 0);
        e.squadFront->SetVisibility(count == 0);
        if (count == 0) e.squadFront->SetText(StringTable::GetString("PVP_NO_WARRIORS"));
        const std::u32string name = SWPrintf(0x100, U"%s [%d]", {p->name, p->level});
        e.name1->SetText(name.c_str());
        e.name2->SetText(name.c_str());
        e.statsBg->SetVisibility(g_tab != 1);
        e.statsTitle->SetVisibility(g_tab != 1);
        const bool record = g_tab != 1 && p->defeated == 0;
        for (StatisticHolder& s : e.stats) s.root->SetVisibility(record);
        e.separator->SetVisibility(record);
        e.stats[0].number->SetText(ToWideString(p->losses));
        e.stats[1].number->SetText(ToWideString(p->wins));
        if (const Professions::ProfessionLevel* rank = Professions::GetProfessionLevel(7, (uint32_t)p->f10))
            e.stats[2].number->SetText(rank->name);
        else
            e.stats[2].number->SetText(StringTable::GetString("RANK_NONE"));
        e.stats[3].number->SetText(ToWideString(p->level));
        if (p->defeated == 1) {   // (list 2's fought friends: online)
            e.start.root->SetVisibility(false);
            e.vicDef->SetVisibility(true);
            e.vicDefIcon->SetTexture(IconManager::GetIcon("pvp_lost_60", false), true);
            e.victory->SetVisibility(false);
            e.defeat->SetVisibility(true);
            e.defeat->SetText(StringTable::GetString("PVP_KILLED"));
            e.vicDefExp->SetText(StringTable::GetString("PVP_KILLED_DESC"));
        } else {
            e.start.root->SetVisibility(true);
            e.vicDef->SetVisibility(false);
        }
        e.info->SetVisibility(p->defeated != 0 && g_mode != 2);
        e.infoText->SetText(StringTable::GetString("PVP_INFO"));
    }
    // The pictures reaching out of the carousel are clipped to it.
    const GUI::ClipRect& r = e.figureClip;
    auto clip = [&](Window* w) {
        if (w->x < mask->x || mask->x + mask->w < w->x + w->w) w->ClipWithRect(r.left, r.top, r.right, r.bottom);
    };
    for (Figure& f : e.figures) {
        clip(f.holder.body);
        clip(f.holder.bodyFemale);
        for (Shared::PlayerHolder::Layer& l : f.holder.layers) {
            clip(l.male);
            clip(l.femaleWindow);
        }
    }
}

int GetSelectedPlayerGID() { return g_selectedGID; }   // @0x301f2c

// @0x30255c fetch: an opponent not in the list needs a download (online; the flag is never read
// offline).
void SetSelectedPlayerGID(int gid, bool fetch) {
    g_selectedGID = gid;
    if (fetch && !PvP::GetPlayerInfoByGID(gid)) g_dataDownload = true;
    g_selectionFetched = false;
}

void SetReturnWithoutWindow(bool on) { g_returnWithoutWindow = on; }   // @0x301f40
bool HasReturnWithoutWindow() { return g_returnWithoutWindow; }        // @0x301f54
void SetDefeatedPlayer(uint32_t spawn) { g_defeated = spawn; }         // @0x301f68
bool HasDefeatedPlayer(uint32_t spawn) { return g_defeated == spawn; } // @0x301f7c

}  // namespace PlayerCompetitionWindow
