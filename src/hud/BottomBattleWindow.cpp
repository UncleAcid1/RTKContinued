// BottomBattleWindow: the bar at the bottom of the campaign maps (Rage_bar.xml): three large round
// buttons (fury spells left and right, the centre one for running or, when a monster can be
// tamed, the net), the mana bars and the rage buttons.
// Port of BottomBattleWindow (libkingdom.so 5.11), 0x256e0c..0x25d8d8. Statics at 0x618224.
// PORTED (4h, for the opening tutorial's taming lesson): the window, the large buttons, the net
// (EnableNet, the centre button's net look, the throw and the catch).
// UNVERIFIED (milestone 5, the rage bar): the spells and fury (OnLeft, OnRight, OnAutoCast,
// OnSpell, the rage buttons, the mana bars and potion, HelpWithSpell, the spell drag in Click),
// the stances (OnAttack, OnBlock, OnAuto), running on the centre button and its look, the
// arena / boss branches of Update, showing the bar on every campaign map (Map::Load).
#include "hud/HUD.h"

#include <cmath>

#include "engine/IconManager.h"
#include "engine/Render.h"
#include "game/AIState.h"
#include "game/BuildingHovers.h"
#include "game/Entity.h"
#include "game/EntityData.h"
#include "game/EntityManager.h"
#include "game/Fog.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/Map.h"
#include "game/MetaData.h"
#include "game/MetaExpression.h"
#include "game/Rand48.h"
#include "game/SoldierSlots.h"
#include "game/StringTable.h"
#include "game/Tasks.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"

namespace BottomBattleWindow {
namespace {

using GUI::Textfield;
using GUI::Window;

// ButtonFuryLarge (0x1c bytes): a large round button. LoadFrom @0x, SetColor @0x.
struct ButtonFuryLarge {
    Window* root = nullptr;      // +0x00 "%s"
    Window* active = nullptr;    // +0x04 fury_button_active_state (the glow)
    Window* bg[4] = {};          // +0x08 button_bg_orange, _blue, _green_circle, _red
    Window* icon = nullptr;      // +0x18 icon_60
    void LoadFrom(Window* r, const char* name) {
        root = GUI::GetWindowTypedF<Window>(r, "%s", name);
        active = GUI::GetWindowTypedF<Window>(r, "%s.fury_button_active_state", name);
        bg[0] = GUI::GetWindowTypedF<Window>(r, "%s.button_bg_orange", name);
        bg[1] = GUI::GetWindowTypedF<Window>(r, "%s.button_bg_blue", name);
        bg[2] = GUI::GetWindowTypedF<Window>(r, "%s.button_bg_green_circle", name);
        bg[3] = GUI::GetWindowTypedF<Window>(r, "%s.button_bg_red", name);
        icon = GUI::GetWindowTypedF<Window>(r, "%s.icon_60", name);
        icon->takesZ = true;
    }
    // Only background `c` shows.
    void SetColor(int c) {
        for (int i = 0; i < 4; ++i) bg[i]->SetVisibility(c == i);
    }
};

WindowManager::FunctionalWindow* g_queue = nullptr;   // BottomBattleWindow::wnd
Window* g_root = nullptr;              // 0x618224 Rage_bar.xml
ButtonFuryLarge g_left;                // 0x618228 button_fury_large_01
ButtonFuryLarge g_right;               // 0x618244 button_fury_large_02
ButtonFuryLarge g_centre;              // 0x618380 button_fury_large_03
float g_pulse = 0.f;                   // 0x618378 the glows' clock
uint32_t g_netTarget = 0;              // 0x61836c the spawn point of the monster the net may catch
bool g_tameNext = false;               // 0x61837c (debug: TameNext throws at once)
uint32_t g_netTask = 0;                // 0x6183f0 a task the throw completes
int g_netArrowCallback = 0;            // 0x6183f4
Render::Sprite* g_net = nullptr;       // 0x618414 the thrown net
float g_netT = 0.f;                    // 0x618418 its flight, 0..1
float g_netFromX = 0.f, g_netFromY = 0.f;   // 0x61841c 0x618420
float g_netToX = 0.f, g_netToY = 0.f;       // 0x618424 0x618428
float g_netTimer = 0.f;                // 0x61842c how long the dying monster can still be caught
Textfield* g_stance = nullptr;         // 0x618458 text_stance
GUI::MovementEffect* g_effect = nullptr;   // 0x618430
int g_offsetX = 0;                     // 0x60f10c
int g_offsetY = 0x17;                  // 0x60f110
float g_scale = 1.f;                   // 0x60f11c

void OnTweenIn() {}                                    // @0x
void OnTweenOut() { g_root->SetVisibility(false); }    // @0x

// The net lands: in the taming lesson always a catch, else by the net's chance (meta 0x25)
// times the tamed form's success rate, out of 10000.
void NetLanded() {
    g_net = Render::RemoveSprite(g_net);
    const Items::ItemInfo* net = Items::GetItemInfo(Items::GetPlayersBestNet());
    const MetaData* chance = net && net->meta ? net->meta->FindChildData(0x25) : nullptr;
    int odds = chance ? chance->GetInt() : 0;
    Entity* e = EntityManager::GetEntityBySpawnPoint(g_netTarget);
    EntityData* tamed = nullptr;
    if (!e) {
        odds = 0;
    } else {
        tamed = EntityFactory::GetTamedEntityForID(e->GetEntityData()->id);
        odds = tamed ? tamed->successRate * odds : 0;
    }
    long roll = Rand48::lrand48();
    if (GameState::tutorial == 0x17 || GameState::IsTameTutorial() || roll % 10000 < odds) {
        if (!e) return;
        g_netTarget = 0;
        e->Disappear();
        e->SetAlpha(0.f);
        if (uint32_t n = Items::GetPlayersBestNet()) GameState::RemoveItem(n, 1);
        Tasks::CompleteSubtask(Tasks::kTameEnemyClass, (uint32_t)e->GetEntityData()->enemyClass, 1);
        if (tamed) {
            if (SoldierPool::GetOccupiedSlotCount() < GameState::GetSoldierReserveSlots()) {
                Entity* s = EntityManager::CreateEntity((int)tamed->id, false, true);
                GameState::HireSoldier(s, true, 2);
                s->SetPos(e->tileX, e->tileY);
                s->GetAI()->UpdateLastTarget();
                s->SetHP(s->GetHpMax() / 2);
            } else {
                // The reserve is full: the item that holds this animal (meta 0x27) drops instead.
                const Items::ItemInfo* holder = nullptr;
                for (unsigned i = 0; const Items::ItemInfo* it = Items::EnumItemInfo(i); ++i) {
                    const MetaData* d = it->meta ? it->meta->FindChildData(0x27) : nullptr;
                    if (d && (uint32_t)d->GetInt() == tamed->id) {
                        holder = it;
                        break;
                    }
                }
                if (holder) BuildingHovers::DropItem(e->worldX, e->worldY, const_cast<Items::ItemInfo*>(holder), 0, 1);
                else std::fprintf(stderr, "ERROR: BottomBattleWindow::Update() no item holds tamed entity %u\n", tamed->id);
            }
            BattleBarWindow::UpdateContents();
        }
        if (GameState::IsTameTutorial()) {
            // The lesson is over: the arrow at the way out, the portal on (10, 0x12).
            BuildingHovers::HideWorldDialog(nullptr);
            BuildingHovers::HideArrow();
            BattleBarWindow::UpdateContents();
            if (Map::Decor* d = Map::GetDecoration(10, 0x12)) {
                AIBaseState* ai = EntityManager::GetPlayer()->GetAI();
                Portal* portal = d->portal;
                BuildingHovers::SetArrowClickCallback(new GUI::Callback([ai, portal] { ai->ClickedPortal(portal); }));
                Render::Sprite* sp = d->sprite;
                BuildingHovers::ArrowAt(sp->x + sp->w * 0.5f, sp->y - sp->h, false, false, false, false, false, false, false);
                if (GameState::secondTutorial == 0x100) GUI::SetInteractionLock(false);
                else GUI::SetInteractionObjectLock(d->portal, d);
                Fog::Open(sp->x + sp->w * 0.5f, sp->y - sp->h, 200.f, 0.3f);
            }
            Hide();
            Tasks::CompleteTask(0x771, true, true, false);
        }
    } else {
        if (e)
            BuildingHovers::ShowTextHoverWithStyle(e->worldX, e->worldY, StringTable::GetString("effect_text_resist"), 1, 2.f,
                                                   50.f, false, false);
        if (uint32_t n = Items::GetPlayersBestNet()) GameState::RemoveItem(n, 1);
        g_netTarget = 0;
    }
}

// @0x259c70 (the parts listed at the top of the file)
void Update(float dt) {
    if (!g_root->visibleSelf) return;
    g_pulse += dt;
    // The catch window closes; in the taming lesson it stays open.
    if (GameState::tutorial == 0x17 || GameState::IsTameTutorial()) {
        if (g_netTarget == 0x1f) g_netTarget = 0;
    } else {
        g_netTimer -= dt;
    }
    if (g_netTimer < 0.f && !g_net) g_netTarget = 0;
    // The centre button offers the net while there is a target, a net and a tamed form.
    uint32_t net = 0;
    bool offer = false;
    if (g_netTarget != 0) {
        net = Items::GetPlayersBestNet();
        if (!net) {
            g_netTimer = 0.f;
        } else {
            offer = true;
            if (!g_net && !(g_netTimer < 0.f)) {
                Entity* e = EntityManager::GetEntityBySpawnPoint(g_netTarget);
                if (e && !EntityFactory::GetTamedEntityForID(e->GetEntityData()->id)) g_netTimer = 0.f;
            }
        }
    }
    if (offer && !g_net) {
        g_centre.SetColor(1);
        g_centre.active->SetVisibility(true);
        g_centre.active->SetAlpha(0.5f + std::cos(g_pulse * 8.f) * 0.6f, false);
        if (Items::ItemInfo* info = Items::GetItemInfo(net)) {
            g_centre.icon->SetTexture(info->GetIcon(), true);
            // UNVERIFIED (milestone 5, the mask shader): the icon is masked round
            // (button_large_round_mask.png, GUI::Window::ApplyMask).
        }
        g_stance->SetText(StringTable::GetString("TAME_HINT"));
    }
    // UNVERIFIED (milestone 5): otherwise the run / walk look (gold_run / gold_walk, STANCE_MODE).
    // The flying net: from the hero to the monster in 2/3 s, growing from 0.2 to its full size.
    if (g_net) {
        if (g_netT < 1.f) {
            Render::SetAlpha(g_net, 1.f);
            const float t = g_netT, k = 0.2f + t * 0.8f;
            g_net->h = (float)g_net->tex->h * k;
            g_net->w = (float)g_net->tex->w * k;
            Render::SetPosition(g_net, g_netFromX + t * (g_netToX - g_netFromX) + g_net->w * -0.5f,
                                g_netFromY + t * (g_netToY - g_netFromY) + g_net->h * -0.5f, 0.2f);
            // Shader 8 turns it about its middle (color.xy) by color.z.
            g_net->color[0] = g_net->x + g_net->w * 0.5f;
            g_net->color[1] = g_net->h * 0.5f - g_net->y;
            g_net->color[2] = t * -3.5f;
            g_netT = t + dt * 1.5f;
        } else {
            NetLanded();
        }
    }
}

// @0x259564: the spells' drag is milestone 5.
bool Click(int x, int y, bool pressed) { return g_root->Click(x, y, pressed, false); }

}  // namespace

WindowManager::FunctionalWindow* Queue() {
    if (!g_queue) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.click = Click;
        f.setZ = SetZ;
        f.hide = Hide;
        g_queue = new WindowManager::FunctionalWindow("BottomBattleWindow", std::move(f));
    }
    return g_queue;
}

// @0x25d4a8
void Init() {
    g_offsetY = 0x17;
    g_offsetX = 0;
    g_queue->usesZRange = false;
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Rage_bar.xml", "Rage_bar.png", GUI::GetHudScaleFactor(), 0,
                             0, 0, 0, false, 1.f);
    g_queue->RegisterTopWindow(g_root);
    g_scale = g_root->scale;
    g_root->SetVisibility(false);
    g_root->SetPosition(-g_root->w, 0);
    g_effect = GUI::CreateMovementEffect(g_root, nullptr, OnTweenIn, OnTweenOut);
    // UNVERIFIED (milestone 5): rage_bar_bg's tap (OnManaBar), the left button's OnLeft, the mana
    // bars (ManaProgress) and the rage buttons.
    g_left.LoadFrom(g_root, "button_fury_large_01");
    g_right.LoadFrom(g_root, "button_fury_large_02");
    g_centre.LoadFrom(g_root, "button_fury_large_03");
    g_centre.root->SetOnClick(OnCenter);
    g_stance = GUI::GetWindowTyped<Textfield>(g_root, "text_stance");
    g_queue->fn.zRange = WindowManager::ReturnSmallZRange;
    g_queue->fn.update = Update;
    g_queue->fn.show = Show;
    g_offsetY = (int)(g_scale * (float)g_offsetY);
    g_offsetX = (int)((float)g_offsetX * g_scale);
}

bool IsVisible() { return g_root && g_root->visibleSelf; }   // @0x

// @0x25cfd8: from the taming lesson on; it slides up from the bottom, centred.
void Show() {
    if (GameState::tutorial <= 0x16 || !g_root) return;
    // (second-tutorial step 0x97: BeltBarWindow::RunRageAnimation, milestone 5)
    if (g_effect->active && g_effect->hideAtEnd) g_root->SetVisibility(false);
    g_queue->MoveWindowOnTop(true);
    if (!g_root->visibleSelf) {
        int x = (GUI::ScreenWidth() - g_root->w) / 2;
        g_effect->Animate(x, GUI::ScreenHeight(), x + g_offsetX, GUI::ScreenHeight() + g_offsetY - g_root->h, true);
    }
    g_root->SetVisibility(true);
    g_root->PreloadTextures();   // GUI::PreloadWindow
    // (the rage buttons' and spells' state is reset here: milestone 5)
    Update(0.f);
}

// @0x259338
void Hide() {
    if (!g_root) return;
    if (g_root->visibleSelf) {
        int x = (GUI::ScreenWidth() - g_root->w) / 2;
        g_effect->Animate(x + g_offsetX, GUI::ScreenHeight() + g_offsetY - g_root->h, x, GUI::ScreenHeight(), false);
    }
}

// @0x
void SetZ(float) {
    g_root->SetZ(HUDWindow::GetZ());
    if (g_effect->active) return;
    if (!g_root->visibleSelf) {
        g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::ScreenHeight());
        return;
    }
    g_root->SetPosition(g_offsetX + (GUI::ScreenWidth() - g_root->w) / 2, g_offsetY + GUI::ScreenHeight() - g_root->h);
}

// @0x258f30: a monster dies: if it has a tamed form (and that form's quest is open) the net may
// catch it for a moment (0.5 to 1 s). The taming lesson's boar (spawn point 0x1e, or 0x6e9)
// gets the hero's hint and the arrow at the centre button.
void EnableNet(uint32_t spawn) {
    g_netTarget = 0;
    Entity* e = EntityManager::GetEntityBySpawnPoint(spawn);
    if (e) {
        EntityData* tamed = EntityFactory::GetTamedEntityForID(e->GetEntityData()->id);
        if (tamed) {
            if (spawn == 0x1e || spawn == 0x6e9) {
                Render::CenterOn(616.f, 260.f);   // (animated over 0.4 s on the original)
                PlayerTopWindow::SetParameters(StringTable::GetString("TAME_TUT2"), 0, false, true);
                g_netTask = 0;
                g_netArrowCallback = BuildingHovers::SetArrowClickCallback(new GUI::Callback(OnCenter));
                GUI::SetInteractionObjectLock(g_centre.root, nullptr);
                BuildingHovers::ArrowAt((float)(g_centre.root->x + g_root->x + g_centre.root->w / 2),
                                        (float)(g_centre.root->y + g_root->y), false, false, false, false, true, false,
                                        false);
                BuildingHovers::PreventArrowAutoHide();
            }
            g_netTarget = spawn;
            g_netTimer = 0.5f + ((float)(Rand48::lrand48() % 1000) / 1000.f) * 0.5f;
            if (tamed->unlockQuest != 0) {
                Tasks::TaskInfo* t = Tasks::GetTask(tamed->unlockQuest);
                if (!t || t->IsLockedByTask()) g_netTarget = 0;
            }
        }
    }
    if (HUDWindow::GetDebugState() && g_tameNext && g_netTarget != 0) OnCenter();
    g_tameNext = false;
}

void TameNext() { g_tameNext = true; }   // @0x25710c

uint32_t GetNetTargetSpawnID() { return g_root && g_root->visibleSelf ? g_netTarget : 0; }   // @0x257090

// @0x2588b4: with a net target the net is thrown at it and the hero cheers.
// UNVERIFIED (milestone 5): without one, a search job is hurried with fury (search_to_fury_mult)
// or the hero starts / stops running (or "SPELL_NO_MANA").
void OnCenter() {
    Entity* player = EntityManager::GetPlayer();
    if (g_netTarget == 0) return;
    if (g_netTask != 0) Tasks::CompleteTask(g_netTask, true, true, false);
    if (GameState::IsTameTutorial()) {
        // (step 0x17: TutorialSender::AddTutorialStep(0x4c8))
        if (GameState::tutorial == 0x17) TutorialSender::AddTutorialStep(0x4c8);
        BuildingHovers::HideArrow();
        GUI::SetInteractionLock(true);
    }
    Entity* e = EntityManager::GetEntityBySpawnPoint(g_netTarget);
    Items::ItemInfo* info = Items::GetItemInfo(Items::GetPlayersBestNet());
    if (info && e) {
        g_net = Render::RemoveSprite(g_net);
        g_net = Render::CreateSprite(info->GetDropIcon(), 0xe, false, false);
        Render::SetShaderType(g_net, 8);
        g_netT = 0.f;
        g_netFromX = player->worldX;
        g_netFromY = player->worldY + player->GetIdleHeight() * -0.5f;
        g_netToX = e->worldX;
        g_netToY = e->worldY;
    }
    player->SetAnimationOnce("bat_vict", false, false, false, true);
}

}  // namespace BottomBattleWindow
