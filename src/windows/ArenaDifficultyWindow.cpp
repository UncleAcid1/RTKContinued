// ArenaDifficultyWindow: the arena waves event's difficulty choice (event 2, "King of the Hill").
// Three flags (the difficulties), the chosen one's challenge, chance and reward strip, and the start
// button. Port of ArenaDifficultyWindow (libkingdom.so 5.11, @0x242b4c..0x245ec8, Win_arn_dif.xml;
// statics 0x616bc4).
//
// The fight itself is BaseCombat::SwitchToArenaFight and ArenaCombat (game/ArenaCombat.cpp).
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "engine/SoundsManager.h"
#include "engine/IconManager.h"
#include "engine/Render.h"
#include "engine/Resources.h"
#include "engine/Timer.h"
#include "game/BuildingHovers.h"
#include "game/AIState.h"
#include "game/Combat.h"
#include "game/Entity.h"
#include "game/EntityData.h"
#include "game/EntityManager.h"
#include "game/Items.h"
#include "game/MetaExpression.h"
#include "game/MetaData.h"
#include "game/Setting.h"
#include "game/SoldierSlots.h"
#include "game/SpawnPoint.h"
#include "game/StringTable.h"
#include "game/Tasks.h"
#include "game/GameState.h"
#include "game/Rand48.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/ContentScroller.h"
#include "hud/HUD.h"
#include "hud/ItemHoverWindow.h"
#include "hud/Shared.h"
#include "windows/Windows.h"

namespace ArenaDifficultyWindow {
namespace {

using GUI::Textfield;
using GUI::Window;

// ArenaDifficultyWindow::FlagHolder (0x64 bytes): one difficulty's flag ("arena_flag_holder_%02d"),
// its gold pile pictures and the sparkles over them.
struct FlagHolder {
    Window* root = nullptr;           // +0x00 (tapped: OnSelectDifficulty)
    Window* masked = nullptr;         // +0x04 arena_flag_masked
    Window* goldSmall = nullptr;      // +0x08 image_gold_small
    Window* goldMedium = nullptr;     // +0x0c image_gold_medium
    Window* goldLarge = nullptr;      // +0x10 image_gold_large
    Window* sparksSmall = nullptr;    // +0x14 sparks_pile_small_01 (takes a depth slot)
    Window* sparksMedium[2] = {};     // +0x18 sparks_pile_med_01 / 02 (take depth slots)
    Window* sparksLarge[4] = {};      // +0x20 sparks_pile_large_01 .. 04 (take depth slots)
    int sparkFrame[4] = {};           // +0x30 a random frame of "images/Buildings/mk/bless/bless"
    float sparkTime[4] = {};          // +0x40 a random 0 .. 9.9 s
    Window* selector = nullptr;       // +0x50 arena_flag_selector
    Textfield* day1 = nullptr;        // +0x54 text_day_01
    Textfield* day2 = nullptr;        // +0x58 text_day_02
    Textfield* req = nullptr;         // +0x5c text_req
    float f60 = 1.f;                  // +0x60

    // @0x243638
    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        masked = GUI::GetWindowTypedF<Window>(w, "%s.arena_flag_masked", p);
        goldSmall = GUI::GetWindowTypedF<Window>(w, "%s.image_gold_small", p);
        goldMedium = GUI::GetWindowTypedF<Window>(w, "%s.image_gold_medium", p);
        goldLarge = GUI::GetWindowTypedF<Window>(w, "%s.image_gold_large", p);
        sparksSmall = GUI::GetWindowTypedF<Window>(w, "%s.sparks_pile_small_%02d", p, 1);
        sparksSmall->takesZ = true;
        for (int k = 0; k != 2; ++k) {
            sparksMedium[k] = GUI::GetWindowTypedF<Window>(w, "%s.sparks_pile_med_%02d", p, k + 1);
            sparksMedium[k]->takesZ = true;
        }
        for (int k = 0; k != 4; ++k) {
            sparksLarge[k] = GUI::GetWindowTypedF<Window>(w, "%s.sparks_pile_large_%02d", p, k + 1);
            sparksLarge[k]->takesZ = true;
        }
        for (int& f : sparkFrame)
            f = (int)Rand48::lrand48() % Resources::GetFrameCount("images/Buildings/mk/bless/bless");
        for (float& t : sparkTime) t = (float)((int)Rand48::lrand48() % 100) / 10.f;
        selector = GUI::GetWindowTypedF<Window>(w, "%s.arena_flag_selector", p);
        day1 = GUI::GetWindowTypedF<Textfield>(w, "%s.text_day_01", p);
        day2 = GUI::GetWindowTypedF<Textfield>(w, "%s.text_day_02", p);
        req = GUI::GetWindowTypedF<Textfield>(w, "%s.text_req", p);
        f60 = 1.f;
    }
};

WindowManager::FunctionalWindow* g_queue = nullptr;   // 0x6171dc
Window* g_root = nullptr;                 // 0x616bc4
GUI::TweenEffect* g_tween = nullptr;      // 0x616bc8
FlagHolder g_flags[3];                    // 0x616bcc
Shared::ItemHolder g_rewards[6];          // 0x616cf8 the reward strip's slots (tapped: OnItem)
Window* g_rewardBar = nullptr;            // 0x6170a0 scroll_bar_wave_rew
Shared::ContentScroller g_scroller;       // 0x617028 the reward strip
ItemHoverWindow* g_hover = nullptr;       // 0x617164
Textfield* g_title = nullptr;             // 0x617168
Window* g_help = nullptr;                 // 0x61716c button_help
Textfield* g_titleDesc = nullptr;         // 0x617170
Textfield* g_challenge = nullptr;         // 0x617174 dif_exp.text_challenge
Window* g_backdrop = nullptr;             // 0x617178 dif_exp.backdrop_02
Textfield* g_challengeDesc = nullptr;     // 0x61717c dif_exp.text_challenge_desc
Textfield* g_chance = nullptr;            // 0x617180 dif_exp.text_chance
Textfield* g_challengeExpl = nullptr;     // 0x617184 text_challenge_expl
Shared::ButtonRaidStart g_start;          // 0x617188 button_collect_bonus (its button: OnStartEvent)
uint32_t g_openTime = 0;                  // 0x6171cc when the choice was first opened (record 0x400)
Window* g_close = nullptr;                // 0x6171d8 x_button.clickArea
std::vector<Items::ItemInfo*> g_items;    // 0x617134 the rewards shown in the strip
std::vector<int> g_counts;                // 0x617144 their amounts
std::vector<int> g_chances;               // 0x617154 their chances (percent)
float g_time = 0.f;                       // 0x6171d0 time shown
int g_lastSecond = 0;                     // 0x6171d4 Update's last wall-clock second
int g_difficulty = -1;                    // 0x60f094 the chosen difficulty (-1: none)
float g_scale = 1.f;                      // 0x60f098
bool g_tablet = false;                    // 0x60f09c

// (@0x242ee0 makes the GUI callback objects of these taps: std::function here.)

// @0x244ed8: a flag is chosen; the reward strip goes back to its start.
void OnSelectDifficulty(unsigned i) {
    g_difficulty = (int)i;
    g_scroller.firstIndex = 0;
    g_scroller.HandleMove(-g_scroller.offset);
    UpdateContents();
}

// @0x242f58: a reward's card, pointing at its slot.
void OnItem(unsigned i) {
    const unsigned n = i + g_scroller.firstIndex;
    if (n >= g_items.size() || !g_items[n]) return;
    SoundsManager::PlaySound("ui_click", 1, false);
    g_hover->Show();
    g_hover->SetItem(StringTable::GetString("BELT_ITEM"), nullptr, g_items[n], false, false, nullptr, false, false,
                     false, false);
    const Window* w = g_rewards[i].root;
    const int x = g_root->x + w->x, y = g_root->y + w->y;
    g_hover->SetTargetRect(x, y, x + w->w, y + w->h, ItemHoverWindow::kSE);
    g_hover->RecomputePosition();
    g_hover->SetZ(0.001f);
    Render::SortRenderLayer(0xe, 1);
}

// @0x2430bc
void OnHelp() {
    PopupWindow::Show(StringTable::GetString("ARENA_DIFFICULTY_HELP"), PopupWindow::Hide, nullptr, nullptr);
}

// @0x242f24: the warning's "go to the tavern": the hire window's second tab.
void OnTavern() {
    HireTroopsWindow::Show();
    HireTroopsWindow::OnTab(1, false);
    PopupSelectionWindow::Hide();
}

// @0x242f40: the warning's "go to the shop": the item shop's fourth tab.
void OnItemShop() {
    ItemShopWindow::Show();
    ItemShopWindow::OnTab(3);
    PopupSelectionWindow::Hide();
}

// The waves' boss at spawn point 0x598 fights the player's squad in the arena.
void StartFight(Entity* boss) {
    GameState::SetCombat(new AggroCombat());
    boss->CreateSquad();
    boss->GetAI()->UpdateLastTarget();
    GameState::GetActiveCombat()->BeginCombat(EntityManager::GetPlayer()->GetSquad(), boss->GetSquad());
    GameState::GetActiveCombat()->ForceBossFight(5, 0);
}
// @0x244ad0: while shown, the strip scrolls, the item card keeps above the window (under a popup
// message), the chosen flag's gold piles brighten to 1.05 and the others fade to 0.9 (in 0.75 s),
// the flags' sparkles run at 5 frames a second; once a wall-clock second the contents refresh.
void Update(float dt) {
    if (!g_queue->shown) return;
    g_scroller.Update(dt);
    if (g_hover->shownHover) {
        const float z = PopupWindow::IsVisible() ? g_root->z - 0.01f : 0.001f;
        if (g_hover->GetZ() != z) {
            g_hover->SetZ(z);
            Render::SortRenderLayer(0xe, 1);
        }
    }
    if (g_queue->shown) {
        g_time += dt;
        for (int i = 0; i != 3; ++i) {
            FlagHolder& f = g_flags[i];
            if (i == g_difficulty) {
                if (f.f60 < 1.05f) {
                    f.f60 += dt / 0.75f;
                    if (f.f60 > 1.05f) f.f60 = 1.05f;
                }
            } else if (f.f60 > 0.9f) {
                f.f60 -= dt / 0.75f;
                if (f.f60 < 0.9f) f.f60 = 0.9f;
            }
            f.goldSmall->SetAlpha(f.f60, true);
            f.goldMedium->SetAlpha(f.f60, true);
            f.goldLarge->SetAlpha(f.f60, true);
            const unsigned count = i == 0 ? 1 : (i == 1 ? 2 : 4);
            Window* const* sparks = i == 0 ? &f.sparksSmall : (i == 1 ? f.sparksMedium : f.sparksLarge);
            for (unsigned k = 0; k != count; ++k) {
                const int frames = Resources::GetFrameCount("images/Buildings/mk/bless/bless");
                Render::Sprite* s = sparks[k]->sprite;
                if (frames == 0 || !s) continue;
                f.sparkTime[k] += dt;
                while (f.sparkTime[k] > 0.2f) {
                    f.sparkTime[k] -= 0.2f;
                    if (++f.sparkFrame[k] >= frames) f.sparkFrame[k] = 0;
                }
                Render::SetFrame(s, (float)(s->tex->h / frames), (float)s->tex->w, f.sparkFrame[k]);
                Render::SetMagnifyFilter(s->tex, true);
                s->w *= g_scale;
                s->h *= g_scale;
            }
        }
    }
    if (Timer::GetGlobalTime() == g_lastSecond) return;
    g_lastSecond = Timer::GetGlobalTime();
    UpdateContents();
}

// @0x2431a4: the arrow, the reward strip, the item card, then the window.
bool Click(int x, int y, bool pressed) {
    if (g_root->visibleSelf) {
        if (BuildingHovers::ClickOnArrow(x, y, pressed)) return true;
        if (!pressed) BuildingHovers::HideArrow();
        if (g_scroller.Click(x, y, pressed, g_root)) return true;
    }
    if (g_hover->Click(x, y, pressed)) return true;
    const bool visible = g_root->visibleSelf;
    g_root->Click(x, y, pressed, false);
    return visible;
}

// @0x242b4c
void SetZ(float z) {
    if (!g_queue->shown) return;
    g_root->SetZ(z);
    g_root->SetPosition(g_tween->GetCenteredX(), g_tween->GetCenteredY());
}

// @0x242bec
float ZRange() { return g_queue->shown ? 0.021f : 0.001f; }

// @0x243184: the reward strip follows a drag.
void Move(int x, int y) { g_scroller.Move(x, y); }

void OnTweenIn() {}   // @0x244acc (empty)

// @0x242e98
void OnTweenOut() {
    WindowManager::WindowHide(false);
    g_queue->shown = false;
    g_queue->MoveWindowDown(true);
    GameState::DropGamePauseState();
    g_difficulty = -1;
}

// @0x243128: the item card closes first.
bool OnBack() {
    if (!g_queue->shown) return false;
    if (g_hover->shownHover) g_hover->Hide();
    else Hide();
    return true;
}

}  // namespace

WindowManager::FunctionalWindow* Queue() {
    if (!g_queue) {   // FUN_00242c44 (static init)
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.click = Click;
        f.setZ = SetZ;
        f.hide = Hide;
        f.move = Move;
        g_queue = new WindowManager::FunctionalWindow("ArenaDifficultyWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_queue && g_queue->shown; }   // @0x242bd8

// @0x245670
void Init() {
    g_tablet = GUI::IsTabletVersion();
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Win_arn_dif.xml", "Win_arn_dif.png", -1.f, 0, 0, 0, 0,
                             false, 1.f);
    if (!g_root) return;
    Queue()->RegisterTopWindow(g_root);
    g_scale = g_root->scale;
    g_root->SetVisibility(false);
    g_tween = GUI::CreateTweenEffect(g_root, nullptr, OnTweenIn, OnTweenOut);
    g_close = GUI::GetWindowTyped<Window>(g_root, "x_button.clickArea");
    g_close->SetOnClick(Hide);
    g_close->SetActionSound("ui_close", 1);
    g_help = GUI::GetWindowTyped<Window>(g_root, "button_help");
    g_help->SetOnClick(OnHelp);
    g_title = GUI::GetWindowTyped<Textfield>(g_root, "text_title");
    g_titleDesc = GUI::GetWindowTyped<Textfield>(g_root, "text_title_desc");
    for (unsigned i = 0; i != 3; ++i) {
        char p[64];
        std::snprintf(p, sizeof p, "arena_flag_holder_%02d", i + 1);
        g_flags[i].LoadFrom(g_root, p);
        g_flags[i].root->SetOnClick([i] { OnSelectDifficulty(i); });
        g_flags[i].root->SetActionSound("ui_click", 1);
    }
    g_challenge = GUI::GetWindowTyped<Textfield>(g_root, "dif_exp.text_challenge");
    g_backdrop = GUI::GetWindowTyped<Window>(g_root, "dif_exp.backdrop_02");
    g_challengeDesc = GUI::GetWindowTyped<Textfield>(g_root, "dif_exp.text_challenge_desc");
    g_chance = GUI::GetWindowTyped<Textfield>(g_root, "dif_exp.text_chance");
    for (unsigned i = 0; i != 6; ++i) {
        // (The first slot is named "slot__01" in the data, the others "slot_02" ...)
        char p[128];
        std::snprintf(p, sizeof p, "dif_exp.wave_reward.rew_hold.slot_%s%02d.inventory_item_icon", i == 0 ? "_" : "",
                      i + 1);
        g_rewards[i].LoadFrom(g_root, p, false);
        g_rewards[i].root->SetOnClick([i] { OnItem(i); });
    }
    const int step = g_rewards[1].root->x - g_rewards[0].root->x;
    g_rewards[5].root->MoveWindow(step, 0);
    g_scroller.Init();
    Window* mask = GUI::GetWindowTyped<Window>(g_root, "dif_exp.wave_reward.rew_hold.mask_universal");
    g_scroller.viewport = {mask->x, mask->y, mask->x + mask->w, mask->y + mask->h};
    g_scroller.track = mask;
    for (Shared::ItemHolder& r : g_rewards) {
        r.root->SetClipRect(&g_scroller.viewport);
        g_scroller.windows.push_back(r.root);
    }
    g_rewardBar = GUI::GetWindowTyped<Window>(g_root, "dif_exp.wave_reward.scroll_bar_wave_rew");
    g_scroller.scrollbar = GUI::GetWindowTyped<Window>(g_root, "dif_exp.wave_reward.scroll_bar_wave_rew.scroll_bar_tiny");
    g_rewardBar->MoveWindow(0, -(int)(g_scale * 5.f));
    g_scroller.unkA0 = 6;
    g_scroller.itemSize = g_rewards[1].root->x - g_rewards[0].root->x;
    g_scroller.onScroll = UpdateContents;
    g_scroller.horizontal = true;
    g_scroller.perLine = 1;
    g_challengeExpl = GUI::GetWindowTyped<Textfield>(g_root, "text_challenge_expl");
    g_start.LoadFrom(g_root, "button_collect_bonus");
    g_start.active->SetOnClick(OnStartEvent);
    g_queue->hideOnOuterClick = true;
    g_queue->fn.zRange = ZRange;
    g_queue->fn.update = Update;
    g_queue->fn.back = OnBack;
    g_queue->fn.show = Show;
    g_hover = new ItemHoverWindow();
}

// @0x244f1c: the game pauses while it is up; the first opening is recorded (record 0x400), and
// before event 2 was ever won no difficulty is chosen.
void Show() {
    g_queue->shown = true;
    g_queue->MoveWindowOnTop(true);
    if (!g_root->visibleSelf) {
        WindowManager::WindowShow(false);
        g_tween->AnimateIn();
        GameState::RaiseGamePauseState();
    }
    g_root->SetVisibility(true);
    if (GameState::GetEventStartTime(0x400) == 0) {
        g_openTime = (uint32_t)Timer::GetGlobalTime();
        GameState::SetEventStartTime(0x400, g_openTime);
    } else {
        g_openTime = GameState::GetEventStartTime(0x400);
    }
    if (GameState::GetEventCompletionCount(2) == 0) g_difficulty = 0;
    UpdateContents();
    g_root->PreloadTextures();   // GUI::PreloadWindow
}

// @0x243104
void Hide() {
    if (!g_root || !g_root->visibleSelf) return;
    g_tween->AnimateOut();
}

// The rewards of one drop list (each child: [item, [amount, chance]]; unknown items left out).
void AddRewards(const MetaData* list) {
    if (!list) return;
    for (unsigned k = 0; k < list->GetChildrenCount(); ++k) {
        Items::ItemInfo* info = Items::GetItemInfo((uint32_t)list->GetChild(k)->GetChild(0)->GetInt());
        if (!info) continue;
        g_items.push_back(info);
        g_counts.push_back(list->GetChild(k)->GetChild(1)->GetChild(0)->GetInt());
        g_chances.push_back(list->GetChild(k)->GetChild(1)->GetChild(1)->GetInt());
    }
}

// @0x243894: the title and the event's text; medium and hard grey until the event was won once;
// the chosen flag's selector; each flag's sparkles get their sheet and names; the chosen
// difficulty's challenge ("arena_diff_desc"'s three numbers in ARENA_DIFFICULTY_EASY_DESC for every
// difficulty, as the original), chance and start button, or "select a difficulty"; the reward strip:
// the drops of an arena-4 monster chosen by the opening time (its items_drop, then the chosen
// difficulty's starN_items), each with its quality frame, icon, amount and a high / medium / low
// chance label (above 20, above 10, else).
void UpdateContents() {
    g_title->SetText(StringTable::GetString("ARENA_DIFFICULTY_TITLE"));
    g_help->SetVisibility(false);
    g_titleDesc->SetText(ArenaEventWindow::GetEventDescription(2));
    g_flags[1].root->SetEnabled(GameState::GetEventCompletionCount(2) != 0);
    g_flags[2].root->SetEnabled(GameState::GetEventCompletionCount(2) != 0);
    for (int i = 0; i != 3; ++i) {
        FlagHolder& f = g_flags[i];
        f.selector->SetVisibility(i == g_difficulty);
        if (i == 0) {
            f.sparksSmall->SetTexture(Resources::GetImage("images/Buildings/mk/bless/bless"), false);
        } else if (i == 1) {
            for (Window* w : f.sparksMedium) w->SetTexture(Resources::GetImage("images/Buildings/mk/bless/bless"), true);
        } else {
            for (Window* w : f.sparksLarge) w->SetTexture(Resources::GetImage("images/Buildings/mk/bless/bless"), true);
        }
    }
    static const char* const kNames[3] = {"ARENA_DIFFICULTY_EASY", "ARENA_DIFFICULTY_MEDIUM", "ARENA_DIFFICULTY_HARD"};
    for (int i = 0; i != 3; ++i) {
        g_flags[i].day1->SetText(StringTable::GetString(kNames[i]));
        g_flags[i].day2->SetText(StringTable::GetString(kNames[i]));
    }
    const bool chosen = g_difficulty != -1;
    g_challenge->SetVisibility(chosen);
    g_backdrop->SetVisibility(chosen);
    g_challengeDesc->SetVisibility(chosen);
    g_challenge->SetText(StringTable::GetString("ARENA_DIFFICULTY_CHALLENGE"));
    const Setting desc = Setting("arena_diff_desc").GetChild(chosen ? (unsigned)g_difficulty : 0);
    const char32_t* format = StringTable::GetString("ARENA_DIFFICULTY_EASY_DESC");
    const std::u32string a = ToWideString(desc.GetChild(0).GetInt());
    const std::u32string b = ToWideString(desc.GetChild(1).GetInt());
    const std::u32string c = ToWideString(desc.GetChild(2).GetInt());
    g_challengeDesc->SetText(SWPrintf(0x100, format, {a.c_str(), b.c_str(), c.c_str()}).c_str());
    g_chance->SetVisibility(chosen);
    g_chance->SetText(StringTable::GetString("ARENA_DIFFICULTY_CHANCE"));
    g_challengeExpl->SetVisibility(!chosen);
    g_challengeExpl->SetText(StringTable::GetString("ARENA_DIFFICULTY_SELECT"));
    g_start.active->SetEnabled(chosen);
    g_start.text->SetEnabled(chosen);
    g_start.text->SetText(StringTable::GetString("ARENA_DIFFICULTY_START"));
    g_start.icon->SetTexture(IconManager::GetIcon("gold_attack", false), true);
    g_scroller.itemCount = 0;
    g_scroller.UpdateScroll();
    g_items.clear();
    g_counts.clear();
    g_chances.clear();
    if (EntityData* e = EntityFactory::GetEntityForArena(4, g_openTime)) {
        AddRewards(e->itemsDrop);
        if (chosen) AddRewards(e->starItems[g_difficulty]);
    }
    g_scroller.itemCount = (unsigned)g_items.size();
    g_scroller.UpdateScroll();
    for (unsigned i = 0; i != 6; ++i) {
        Shared::ItemHolder& h = g_rewards[i];
        h.root->SetVisibility(chosen && i + g_scroller.firstIndex < g_items.size());
        if (!h.root->visibleSelf) continue;
        const unsigned n = i + g_scroller.firstIndex;
        const int count = g_counts[n];
        const int chance = g_chances[n];
        Items::ItemInfo* info = g_items[n];
        h.icon.locked->SetVisibility(false);
        for (unsigned k = 0; k != 5; ++k) h.icon.itemClass[k]->SetVisibility(info->itemLevel == k);
        h.icon.icon->SetTexture(info->GetIcon(), true);
        const int label = chance > 20 ? 0 : (chance > 10 ? 1 : 2);   // the green, purple or red label
        for (int k = 0; k != 4; ++k) h.icon.discount[k].root->SetVisibility(k - 1 == label);
        h.icon.discount[1].text->SetText(StringTable::GetString("PROBABILITY_HIGH"));
        h.icon.discount[2].text->SetText(StringTable::GetString("PROBABILITY_MEDIUM"));
        h.icon.discount[3].text->SetText(StringTable::GetString("PROBABILITY_LOW"));
        h.quantity->SetVisibility(count > 1);
        h.quantity->SetText(ToWideString(count));
        h.icon.redX->SetVisibility(false);
        h.icon.owned->SetVisibility(false);
    }
    g_root->UpdatePosition();
}

// @0x24500c Fight!: without a soldier or with 4 belt items or fewer the player is sent to the
// tavern and / or the shop first. Otherwise a new event starts (its start = the choice's opening
// time, the chosen difficulty, quest 0x5b9 afresh) and the boss at spawn point 0x598 becomes an
// arena-4 monster (0x225 the very first time, outside PvP) at the player's level, on map 112; the
// fight begins (the hero's AP topped up to 100). A running event's fight goes on with its boss.
void OnStartEvent() {
    BeltBarWindow::UpdateBeltItems();
    const int soldiers = (int)SoldierSlots::GetOccupiedSlotCount();
    int orbs = 0;
    GameState::SetBeltItemMode(true);
    for (unsigned i = 0; i < GameState::GetBeltSlotCount(); ++i)
        orbs += GameState::GetItemAmount(GameState::GetBeltItemAt(i), true);
    GameState::SetBeltItemMode(false);
    if (soldiers <= 0 || orbs <= 4) {
        const char32_t* title = StringTable::GetString("CAN_NOT_ENTER_ARENA_TITLE");
        const char32_t* text = StringTable::GetString("CAN_NOT_ENTER_ARENA");
        if (soldiers > 0)
            PopupSelectionWindow::Show(title, text, StringTable::GetString("GO_TO_SHOP"),
                                       IconManager::GetIcon("Icon_bottom_bar_shop", false), OnItemShop, nullptr,
                                       nullptr, nullptr);
        else if (orbs > 4)
            PopupSelectionWindow::Show(title, text, StringTable::GetString("GO_TO_TAVERN"),
                                       IconManager::GetIcon("bottom_bar_army", false), OnTavern, nullptr, nullptr,
                                       nullptr);
        else
            PopupSelectionWindow::Show(title, text, StringTable::GetString("GO_TO_SHOP"),
                                       IconManager::GetIcon("Icon_bottom_bar_shop", false), OnItemShop,
                                       StringTable::GetString("GO_TO_TAVERN"),
                                       IconManager::GetIcon("bottom_bar_army", false), OnTavern);
        return;
    }
    Entity* boss = nullptr;
    if (GameState::GetEventStartTime(2) == 0) {
        GameState::SetEventStartTime(2, g_openTime);
        GameState::SetEventEndTime(2, 0);
        GameState::SetEventDifficulty(2, (uint32_t)g_difficulty);
        GameState::ResetTask(0x5b9);
        if (Tasks::TaskInfo* t = Tasks::GetTask(0x5b9)) t->dnShow = t->dnShowDefault;
        boss = EntityManager::GetEntityBySpawnPoint(0x598);
        if (boss) {
            EntityData* d;
            if (GameState::GetEventCompletionCount(2) == 0 && !GameState::IsPvPCombatActive()) {
                EntityFactory::GetEntityForArena(4, g_openTime);
                d = EntityFactory::GetEntityByID(0x225);
            } else {
                d = EntityFactory::GetEntityForArena(4, g_openTime);
            }
            if (d) {
                boss->SwitchEntityData(d);
                if (SpawnPoint* sp = boss->GetSpawnPoint()) {
                    sp->deathTime = 0;
                    sp->entityId = d->id;
                    sp->hp = d->GetHpForLevel(GameState::GetLevel());
                    sp->hpMax = d->GetHpForLevel(GameState::GetLevel());
                    if (d->bossMinionsLevels) {
                        delete sp->meta;
                        char* text = new char[0x80];
                        std::memcpy(text, "MAP_ID=112", 0xb);
                        sp->meta = new MetaExpression(text, true);
                    }
                }
            }
            Tasks::Update(true);
            StartFight(boss);
            Entity* player = EntityManager::GetPlayer();
            if (player->GetAP() < 100) player->SetAP(100);
        }
    } else {
        boss = EntityManager::GetEntityBySpawnPoint(0x598);
        if (boss) StartFight(boss);
    }
    if (boss) BeltBarWindow::Hide();
    UpdateContents();
    Hide();
}

}  // namespace ArenaDifficultyWindow
