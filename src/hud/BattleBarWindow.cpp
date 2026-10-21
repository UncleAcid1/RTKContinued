// BattleBarWindow: the army bar at the bottom left (Army button, warrior slots) and the quest slider
// above it. Port of BattleBarWindow (libkingdom.so 5.11), 0x24a750..0x24c2b4. Statics 0x60f0ac.
#include <cmath>
#include <cstdio>

#include "engine/SoundsManager.h"
#include "engine/IconManager.h"
#include "engine/Render.h"
#include "engine/Timer.h"
#include "game/Building.h"
#include "game/BuildingHovers.h"
#include "game/BuildingMovement.h"
#include "game/BuildingPlacement.h"
#include "game/Entity.h"
#include "game/EntityData.h"
#include "game/EntityManager.h"
#include "game/GameData.h"
#include "game/GameState.h"
#include "game/Map.h"
#include "game/Setting.h"
#include "game/SoldierSlots.h"
#include "game/Squad.h"
#include "game/Tasks.h"
#include "hud/HealHoverWindow.h"
#include "hud/ItemHoverWindow.h"
#include "game/StringTable.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/HUD.h"
#include "hud/Shared.h"
#include "windows/Windows.h"

namespace BattleBarWindow {
namespace {

using GUI::Textfield;
using GUI::Window;

WindowManager::FunctionalWindow* g_queue = nullptr;
// 0x60f0ac..0x60f0b8: bar (y offset, x) and quest slider (y offset, x), scaled by the bar's scale
int g_barDY = 0x15, g_barX = -5, g_sliderDY = -0x70, g_sliderX = -5;
bool g_tallScreen = false;   // screen height > 600
bool g_tablet = false;
float g_scale = 1.f;         // 0x60f0c0
Window* g_root = nullptr;    // "Battle_bar"
GUI::MovementEffect* g_effect = nullptr;
Window* g_bg2Slots = nullptr;
Window* g_bg = nullptr;
Window* g_armyIcon = nullptr;
Textfield* g_barName = nullptr;
Window* g_armyClick = nullptr;
Shared::BottomSlotInfo g_slots[3];
Window* g_allQuests = nullptr;
Textfield* g_allQuestsText = nullptr;
Window* g_slider = nullptr;  // "Slider_mission_scroll_bg" (Shared::TaskWindow)
GUI::MovementEffect* g_sliderEffect = nullptr;
ItemHoverWindow* g_hover = nullptr;   // 0x617a28 a soldier's card over its slot
HealHoverWindow* g_heal = nullptr;    // 0x617a30 the healing card over a wounded soldier's slot
Entity* g_hoverEntity = nullptr;      // 0x617b78 whose card shows
unsigned g_hoverSlot = 0;             // 0x617b7c
bool g_deathTutorial = false;         // 0x617a2c the helper after the first soldier's death runs
int g_lastSecond = 0;                 // 0x617b8c
// Per slot (BottomSlotInfo +0x58..+0x60): the fill glides to progressValue like the hero's bars.
struct Glide { bool started = false; float shown = 0.f, time = 0.f; } g_glide[3];

void OnSlot(unsigned i);

// @0x249ed4: the Army button. UNVERIFIED (milestone 5): HireTroopsWindow::ShowForBuilding (0x8f the
// tavern by "your_army_opens_tavern" / "always_open_tavern" or without "skip_choose_troops_window",
// else 0x8e the barracks); during the first-death helper: the barracks, with its arrow.
void OnArmy() {
    if (!g_deathTutorial) return;
    GUI::SetInteractionLock(false);
    BuildingHovers::HideArrow();
    BuildingHovers::HideWorldDialog(nullptr);
    g_deathTutorial = false;
}

// @0x24bbb8: over an edit mode or the shop the bar only swallows what lands on it.
bool Click(int x, int y, bool pressed) {
    if (!g_root) return false;
    if (BuildingMovement::Activated() || BuildingPlacement::Activated() || ShopWindow::IsVisible() ||
        BottomScreenshotWindow::IsVisible()) {
        return (g_slider && g_slider->GetWindowAtPosition(x, y, false)) || g_root->GetWindowAtPosition(x, y, false);
    }
    if (g_heal->Click(x, y, pressed) || g_hover->Click(x, y, pressed)) return true;
    if (g_slider && g_slider->Click(x, y, pressed, false)) return true;
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
        g_queue = new WindowManager::FunctionalWindow("BattleBarWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_root && g_root->visibleSelf; }

// @0x24bdd8
void Init() {
    const int W = GUI::ScreenWidth(), H = GUI::ScreenHeight();
    (void)W;
    g_sliderX = -5;
    g_barX = -5;
    g_tallScreen = 600 < H;
    Queue()->usesZRange = false;
    g_barDY = 0x15;
    g_sliderDY = -0x70;
    g_tablet = GUI::IsTabletVersion();
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Battle_bar.xml", "Battle_bar.png",
                             GUI::GetHudScaleFactor(), 0, 0, 0, 0, false, 1.f);
    if (!g_root) return;
    Queue()->RegisterTopWindow(g_root);
    g_scale = g_root->scale;
    g_root->SetVisibility(false);
    g_root->SetPosition(-g_root->w, H - g_root->h);
    g_effect = GUI::CreateMovementEffect(g_root);
    g_bg2Slots = GUI::GetWindowTyped<Window>(g_root, "battle_bar_main_bg_2_slots");
    g_bg = GUI::GetWindowTyped<Window>(g_root, "battle_bar_main_bg");
    g_armyIcon = GUI::GetWindowTyped<Window>(g_root, "button_army.icon_60");
    g_armyIcon->SetTexture(IconManager::GetIcon("bottom_bar_army", false), true);
    g_barName = GUI::GetWindowTyped<Textfield>(g_root, "text_bar_name");
    g_barName->SetText(StringTable::GetString("OPEN_WARRIOR_SCREEN"));
    g_armyClick = GUI::GetWindowTyped<GUI::Button>(g_root, "button_army.clickArea");
    g_armyClick->SetOnClick(OnArmy);
    g_armyClick->SetActionSound("", 1);
    for (int i = 0; i < 3; ++i) {
        char p[64];
        std::snprintf(p, sizeof p, "bottom_slot_warrior_%02d", i + 1);
        g_slots[i].LoadFrom(g_root, p);
        g_slots[i].iconHolder->SetTexture(IconManager::GetIcon("icon_gold_helmet", false));
        g_slots[i].damage->SetFrame(0x14, true);
        g_slots[i].holder->SetOnClick([i] { OnSlot((unsigned)i); });
    }
    g_allQuests = GUI::GetWindowTyped<Window>(g_root, "button_all_quests");
    g_allQuests->SetVisibility(false);
    g_allQuests->SetOnClick(TaskListWindow::Show);   // OnAllTasks @0x24a048 (the same code as TaskListWindow::Show)
    g_allQuestsText = GUI::GetWindowTyped<Textfield>(g_root, "button_all_quests.text_all_quests");

    g_slider = GUI::RegisterUI("../resource/kingdom_ui/1Original/Slider_mission_scroll_bg.xml",
                               "Slider_mission_scroll_bg.png", GUI::GetHudScaleFactor(), 0, 0, 0, 0, false, 1.f);
    if (g_slider) {
        // Shared::TaskWindow::LoadFrom: the quest line nothing in 5.11 shows (see ShowAllTasks). Starts hidden.
        g_slider->SetVisibility(false);
        g_slider->SetOnClick([] {});
        g_sliderEffect = GUI::CreateMovementEffect(g_slider);
    }
    Queue()->fn.update = Update;
    Queue()->fn.show = Show;
    g_barDY = (int)((float)g_barDY * g_scale);
    g_barX = (int)((float)g_barX * g_scale);
    g_sliderX = (int)((float)g_sliderX * g_scale);
    g_sliderDY = (int)((float)g_sliderDY * g_scale);
    g_hover = new ItemHoverWindow();
    g_heal = new HealHoverWindow();
}

// @0x24b8ac: shown in the city during tutorial steps 10..24 and after 54
void Show() {
    if (!g_root || !GameState::IsPlayerCity()) return;
    if (g_effect->active && g_effect->hideAtEnd) g_root->SetVisibility(false);
    int step = GameState::TutorialStep();
    if (!(29u < (unsigned)(step - 0x19) && (unsigned)(step - 10) < 0x7ffffff6u)) return;
    Queue()->MoveWindowOnTop(true);
    if (!g_root->visibleSelf) {
        const int H = GUI::ScreenHeight();
        int y = H - g_root->h;
        g_effect->Animate(-g_root->w, y, g_barX, y + g_barDY, true);
        if (g_slider) {
            int sy = (g_sliderDY + H) - g_slider->h;
            g_sliderEffect->Animate(-g_slider->w, sy, g_sliderX, sy, true);
        }
        g_root->SetVisibility(true);
        UpdateContents();
    }
    ShowAllTasks();
    g_root->PreloadTextures();   // GUI::PreloadWindow
}

// @0x24b660 BattleBarWindow::ShowAllTasks. The quest line (g_slider, a Shared::TaskWindow) would
// show instead of the button when the flag at 0x617b90 is set; nothing in 5.11 sets it.
void ShowAllTasks() {
    if (!g_root) return;   // PORT: asked before Init (as TaskHolderWindow::UpdateTasks)
    const bool show = !(GameState::tutorial < 0x66 || GameState::IsBossCombatActive() || ShopWindow::IsVisible() ||
                        Map::GetCurrentFarm());
    if (!g_allQuests->visibleSelf) {
        if (show) g_allQuests->SetVisibility(true);
    } else if (!show) {
        g_allQuests->SetVisibility(false);
    }
    if (g_slider && g_slider->visibleSelf && !show) g_slider->SetVisibility(false);
}

// @0x249b90
void HideAllTasks() {
    if (!g_root) return;   // PORT: as above
    g_allQuests->SetVisibility(false);
    if (g_slider) g_slider->SetVisibility(false);
}

// @0x24b77c
void Hide() {
    if (!g_root) return;
    if (!GameState::IsPlayerCity()) HideAllTasks();
    const int H = GUI::ScreenHeight();
    if (g_root->visibleSelf) {
        int y = (g_barDY + H) - g_root->h;
        g_effect->Animate(g_barX, y, -g_root->w, y, false);
    }
    if (g_slider && g_slider->visibleSelf) {
        int y = (g_sliderDY + H) - g_slider->h;
        g_sliderEffect->Animate(g_sliderX, y, -g_slider->w, y, false);
    }
}

// @0x24ba4c (the heal hover window takes z - 0.0001: milestone 5)
void SetZ(float) {
    if (!g_root) return;
    float z = HUDWindow::GetZ();
    if (g_slider) g_slider->SetZ(z - 0.009f);
    g_root->SetZ(z - 0.009f);
    const int H = GUI::ScreenHeight();
    if (!g_effect->active && g_root->visibleSelf) g_root->SetPosition(g_barX, (g_barDY + H) - g_root->h);
    if (g_slider && !g_sliderEffect->active && g_slider->visibleSelf)
        g_slider->SetPosition(g_sliderX, (g_sliderDY + H) - g_slider->h);
}

bool DeathTutorialActive() { return g_deathTutorial; }   // @0x249c10

// @0x249cfc: the healing card over the slot of soldier e (if it has one).
void ShowHealSoldier(Entity* e) {
    unsigned slot = ~0u;
    for (unsigned i = 0; i < SoldierSlots::GetTotalSlotCount() && slot == ~0u; ++i)
        if (SoldierSlots::GetSoldierBySlot(i) == e) slot = i;
    if (slot == ~0u) return;
    g_heal->SetEntity(e);
    g_heal->Show();
    const Window* h = g_slots[slot].holder;
    g_heal->SetPosition(g_root->x + h->x, g_root->y + h->y - g_heal->root->h);
    g_heal->MoveWindowOnTop(true);
}

// @0x249bd4
void HideHoverWindow() {
    if (g_hover && g_hover->shownHover) g_hover->Hide();
}

// @0x249dd4: the first soldier has died: the helper arrow at the Army button.
void LaunchFirstDeathHelper() {
    g_deathTutorial = true;
    BuildingHovers::SetArrowClickCallback(new GUI::Callback(OnArmy));
    BuildingHovers::SetArrowVisibleWindowLimit(1);
    Window* army = g_armyClick;
    BuildingHovers::ArrowAt((float)(g_root->x + army->x + army->w / 2), (float)(g_root->y + army->y + army->h / 2), false,
                            true, false, false, true, true, false);
    BuildingHovers::PreventArrowAutoHide();
}

namespace {

// @0x24a04c: a tap on a slot. An empty one opens the barracks (milestone 5); a soldier's or a locked
// one's card pops up over it.
void OnSlot(unsigned i) {
    if ((GameState::IsTutorial() || GameState::IsCityTutorial()) && GameState::tutorial != 0x17) return;
    // UNVERIFIED (milestone 5): HireTroopsWindow::ShowForBuilding(0x8f) when "always_open_tavern",
    // (0x8e) for an empty slot the player owns.
    Entity* e = SoldierSlots::GetSoldierBySlot(i);
    if (!e && i < GameState::GetSoldierSlotCount()) return;
    Tasks::TaskInfo* t =
        Tasks::GetTask((uint32_t)Setting(i == 1 ? "squad_slot_02_quest_unlock" : "squad_slot_03_quest_unlock").GetInt());
    std::u32string lock = t ? SWPrintf(0x100, StringTable::GetString("HIRE_TROOPS_TASK_LOCK"), {t->name})
                            : SWPrintf(0x100, StringTable::GetString("SHOP_UNLOCK_TASK"));
    SoundsManager::PlaySound("ui_click", 1, false);
    g_hover->Show();
    if (!e) g_hover->SetText(StringTable::GetString("WAR_SLOT_LOCKED"), lock.c_str());
    else g_hover->SetEntity(e, e->GetEntityData(), true, false);
    g_hoverEntity = e;
    g_hoverSlot = i;
    Window* h = g_slots[i].holder;
    int x = g_root->x + h->x, y = g_root->y + h->y;
    g_hover->SetTargetRect(x, y, x + h->w, y + h->h, ItemHoverWindow::kSW);
    g_hover->RecomputePosition();
    g_hover->SetZ(0.001f);
    Render::SortRenderLayer(0xe, 1);
}

}  // namespace

// @0x24a6a0: the card of the first slot holding a soldier of this kind (help: with the Feed hint).
void ShowSoldier(uint32_t id, bool help) {
    for (unsigned i = 0; i != 3; ++i) {
        Entity* e = SoldierSlots::GetSoldierBySlot(i);
        if (!e || e->GetEntityData()->id != id) continue;
        OnSlot(i);
        if (help) g_hover->HelpFeed(true);
    }
}

// @0x24a750: what each slot shows. A soldier: its picture, its health as the bar's fill (in
// training: the 5 seconds' progress) and how many of its kind the player has; a free slot: the wings;
// a slot not unlocked yet: the lock (the third is hidden).
void UpdateContents() {
    if (!g_root || !g_root->visibleSelf) return;
    if (!EntityManager::GetPlayer()) return;
    g_bg2Slots->noInput = true;
    g_bg->noInput = true;
    g_bg2Slots->SetVisibility(GameState::GetSoldierSlotCount() != 3);
    g_bg->SetVisibility(GameState::GetSoldierSlotCount() == 3);
    Entity* chosen[3] = {};
    for (unsigned i = 0; i != 3; ++i) {
        Shared::BottomSlotInfo& s = g_slots[i];
        s.holder->SetVisibility(true);
        s.wingsEmpty->SetVisibility(true);
        s.bgSimple->SetVisibility(false);
        s.bgDisabled->SetVisibility(false);
        s.quantityActive->SetVisibility(false);
        s.damageAlpha = 0.f;
        Entity* e = SoldierSlots::GetSoldierBySlot(i);
        uint32_t before = (uint32_t)s.unk50;
        uint32_t id = e ? e->GetEntityData()->id : 0;
        if (id == 0) {
            // (In the arena a kind whose soldiers are all dead keeps its slot.)
            if (before != 0 && GameState::IsArenaCombatActive() && SoldierSlots::GetSoldiersCountByID((int)before) == 0)
                id = before;
            else s.unk50 = 0;
        } else {
            s.unk50 = 0;
        }
        if (g_hoverEntity && i == g_hoverSlot && e != g_hoverEntity) {
            g_hover->Hide();
            g_hoverSlot = 0;
            g_hoverEntity = nullptr;
        }
        bool empty = false;
        if (id == 0 && i < GameState::GetSoldierSlotCount()) {
            // A free slot shows a kind that is in training (not one an earlier free slot shows).
            for (unsigned j = 0; j < SoldierPool::GetOccupiedSlotCount(); ++j) {
                Entity* p = SoldierPool::GetSoldierBySlot(j);
                if (!p) continue;
                bool dup = false;
                for (Entity* c : chosen)
                    if (c && c->GetEntityData()->id == p->GetEntityData()->id) dup = true;
                if (!dup && p->workType == 1) {
                    chosen[i] = p;
                    id = p->GetEntityData()->id;
                }
            }
            if (SoldierSlots::GetOccupiedSlotCount() < GameState::GetSoldierSlotCount() &&
                !SoldierSlots::GetFirstSoldierByID(id))
                e = SoldierPool::GetFirstSoldierByID(id);
            else empty = true;
        }
        if (empty || id == 0) {
            if (i < GameState::GetSoldierSlotCount()) {
                s.bgSimple->SetVisibility(true);
                s.wingsEmpty->SetVisibility(true);
                s.wingsEmpty->SetEnabled(true);
                s.soldierFine->SetVisibility(false);
                s.iconHolder->SetVisibility(true);
                s.lockIcon->SetVisibility(false);
                s.progress->SetVisibility(false);
                s.timeLeft->SetVisibility(false);
            } else if (i < 2) {
                s.bgDisabled->SetVisibility(true);
                s.wingsEmpty->SetVisibility(true);
                s.wingsEmpty->SetEnabled(false);
                s.wingsEmpty->SetAlpha(0.5f, false);
                s.soldierFine->SetVisibility(false);
                s.iconHolder->SetVisibility(true);
                s.lockIcon->SetVisibility(true);
                s.progress->SetVisibility(false);
                s.timeLeft->SetVisibility(false);
            } else {
                s.holder->SetVisibility(false);
            }
            s.unk50 = 0;
            s.progressValue = 1.f;
            continue;
        }
        int count = SoldierSlots::GetSoldiersCountByID((int)id) + SoldierPool::GetSoldiersCountByID((int)id);
        if (!e) {
            EntityData* data = EntityFactory::GetEntityByID((int)id);
            s.bgSimple->SetVisibility(true);
            s.wingsEmpty->SetVisibility(false);
            s.soldierFine->SetVisibility(true);
            s.soldierFine->SetTexture(data->side0);
            s.soldierFine->SetEnabled(false);
            s.iconHolder->SetVisibility(false);
            s.lockIcon->SetVisibility(false);
            s.progress->SetVisibility(false);
            s.quantityActive->SetVisibility(true);
            s.quantityActive->SetText(ToWideString(count));
        } else {
            s.bgSimple->SetVisibility(true);
            s.wingsEmpty->SetVisibility(false);
            s.soldierFine->SetVisibility(true);
            s.soldierFine->SetTexture(e->GetEntityData()->side0);
            s.soldierFine->SetEnabled(true);
            s.iconHolder->SetVisibility(false);
            s.lockIcon->SetVisibility(false);
            s.progress->SetVisibility(true);
            bool training = false;
            if (e->workType == 1 || e->GetWorkplace()) {
                // In training: the first 5 seconds after its building started.
                const GameData::BuildingData* bd = GameData::GetBuilding((uint32_t)e->GetEntityData()->trainingObjType);
                int start = 0;
                if (Map::Building* b = Map::GetBuildingWithID(bd->id)) {
                    start = b->IsOpened() ? (int)b->stateTime : 0;
                } else if (GameState::OfflineBuilding* ob = GameState::GetOfflineBuildingWithID(bd->id)) {
                    start = (int)ob->stateTime;
                }
                int passed = Timer::GetGlobalTime() - start;
                if (passed <= 5) {
                    training = true;
                    s.timeLeft->SetVisibility(false);
                    float f = (float)passed / 5.f;
                    s.progressValue = f <= 1.f ? (f < 0.f ? 1.f : 1.f - f) : 0.f;
                }
            }
            if (!training) {
                s.timeLeft->SetVisibility(false);
                float f = 0.f;
                float fill;
                if (!e->IsActuallyDying()) {
                    int hp = e->GetHP();
                    f = (float)(hp < 0 ? 0 : hp) / (float)e->GetHpMax();
                    fill = f <= 1.f ? (f < 0.f ? 1.f : 1.f - f) : 0.f;
                } else {
                    fill = 1.f - f;
                }
                s.progressValue = fill;
                if (f < 0.5f) s.damageAlpha = (0.5f - f) + (0.5f - f);
            }
            s.quantityActive->SetVisibility(count > 1);
            s.quantityActive->SetText(ToWideString(count));
            if (before == 0) s.unk64 = 2.f;
        }
        s.unk50 = (int)id;
    }
    g_allQuestsText->SetText(StringTable::GetString("main_menu_button_tasks"));
    // (A second text field, 0x6179c8, of the unused quest line gets the same name.)
}

// @0x24b114: a soldier of the hero's squad was hurt: its slot flashes red.
void OnDamage(Entity* e) {
    Entity* player = EntityManager::GetPlayer();
    if (!player || !g_root) return;
    BaseSquad* squad = player->GetSquad();
    bool found = false;
    for (unsigned i = 1; i <= 3; ++i) {
        if (i >= (unsigned)squad->GetSoldierCount() || squad->GetSoldier(i) != e) continue;
        Shared::BottomSlotInfo& s = g_slots[i - 1];
        found = true;
        s.damage->SetVisibility(true);
        s.damage->SetAlpha(s.damageAlpha, false);
        s.damageTime = 0.f;
    }
    if (found) UpdateContents();
}

// @0x24b1fc
void Update(float dt) {
    if (!g_root) return;
    if (g_hover->shownHover) {
        // The card stays just above the bar (under a pop-up window: just under the bar).
        float z = PopupWindow::IsVisible() ? g_root->z - 0.01f : 0.001f;
        if (z != g_hover->GetZ()) {
            g_hover->SetZ(z);
            Render::SortRenderLayer(0xe, 1);
        }
        if (WindowManager::GetShownWindowCount() != 0 && GameState::secondTutorial == 0x100) g_hover->Hide();
    }
    for (int i = 0; i != 3; ++i) {
        Shared::BottomSlotInfo& s = g_slots[i];
        Glide& g = g_glide[i];
        if (!g.started) g.shown = s.progressValue;
        g.started = true;
        g.time += dt;
        if (g.time > 0.033333f) {
            g.time = 0.f;
            g.shown = g.shown - (s.progressValue - g.shown) * -0.133975f;
        }
        s.SetFillPercent(g.shown);
        if (i == 0 && s.wingsEmpty->visibleSelf && GameState::secondTutorial == 0x100) {
            // The first free slot beckons.
            s.holder->SetAlpha(1.f + (std::cos((float)Timer::GetTime() * 8.f) + 1.f) * 0.2f, true);
        } else if (s.unk64 <= 0.f) {
            s.holder->SetAlpha(1.f, true);
        } else {
            // A soldier that just took the slot blinks for 2 seconds.
            s.holder->SetAlpha(1.f + (std::cos((float)Timer::GetTime() * 8.f) + 1.f) * 0.25f, true);
            s.unk64 -= dt;
        }
        if (s.damage->visibleSelf) {
            // The red flash springs in, then settles at the slot's damage level.
            s.damageTime += dt + dt;
            if (s.damageTime < 0.75f) {
                float f = s.damageTime / 0.75f - 1.f;
                s.damage->SetAlpha(s.damageAlpha + (1.f - s.damageAlpha) * (1.f + f * f * (1.70158f + f * 2.70158f)), false);
            } else if (s.damageTime >= 1.f) {
                s.damage->SetVisibility(s.damageAlpha != 0.f);
                s.damage->SetAlpha(s.damageAlpha, false);
            } else {
                s.damage->SetAlpha(1.f + (s.damageTime - 0.75f) * 4.f * (s.damageAlpha - 1.f), false);
            }
        }
        if (s.damageAlpha >= 0.5f) {
            s.soldierFine->SetAlpha(1.f - s.damageAlpha, false);
            s.soldierFine->SetCustomShader(10, false);
        }
    }
    if (Timer::GetGlobalTime() == g_lastSecond) return;
    g_lastSecond = Timer::GetGlobalTime();
    UpdateContents();
}

}  // namespace BattleBarWindow
