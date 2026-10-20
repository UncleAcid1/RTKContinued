// HealSquadWindow: the "Heal Hero" window. It opens when the hero has fallen (Entity::OnDeath), on a
// tap while he is dead, and from his health bar. He can wait for his health to come back, heal at
// once ("Heal now!": a healing item he owns, else crystals), drink one of the healing items he
// carries, or flee to the castle. Port of HealSquadWindow (libkingdom.so 5.11, @0x2c9ee0..0x2cd3c0,
// Window_heal_warrior_small.xml; statics 0x620b18).
//
// Freemium removal (user decision 2026-10-13): the hero's health comes back much faster than on
// the original (GameState::HpRegenSeconds); the crystals "Heal now!" costs are earned in play.
#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

#include "engine/IconManager.h"
#include "engine/Render.h"
#include "engine/Timer.h"
#include "game/AIState.h"
#include "game/BuildingHovers.h"
#include "game/Combat.h"
#include "game/Entity.h"
#include "game/EntityData.h"
#include "game/EntityManager.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/Map.h"
#include "game/PlayerEquipmentInfo.h"
#include "game/MetaExpression.h"
#include "game/Rand48.h"
#include "game/SaveManager.h"
#include "game/Setting.h"
#include "game/StringTable.h"
#include "game/Tasks.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/HUD.h"
#include "hud/ItemHoverWindow.h"
#include "hud/Shared.h"
#include "windows/Windows.h"

namespace HealSquadWindow {
namespace {

using GUI::Textfield;
using GUI::Window;

// HealSquadWindow::HealthBar (0x34 bytes): the big health bar with its "+1 in ..." line.
struct HealthBar {
    Window* root = nullptr;        // +0x00
    Textfield* stat = nullptr;     // +0x04 "Health: n/max"
    Window* bar = nullptr;         // +0x08 clipped to the health left
    Textfield* plusBg = nullptr;   // +0x0c
    Textfield* plus = nullptr;     // +0x10
    Window* icon16 = nullptr;      // +0x14
    Textfield* textBg = nullptr;   // +0x18
    Textfield* text = nullptr;     // +0x1c
    Window* iconHealth = nullptr;  // +0x20
    GUI::ClipRect clip{};          // +0x24

    // @0x2ca840
    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        stat = GUI::GetWindowTypedF<Textfield>(w, "%s.text_rep_stat", p);
        bar = GUI::GetWindowTypedF<Window>(w, "%s.hp_bar_holder_win.player_hp_large_bar", p);
        plusBg = GUI::GetWindowTypedF<Textfield>(w, "%s.hp_bar_holder_win.text_plus_one_bg", p);
        plus = GUI::GetWindowTypedF<Textfield>(w, "%s.hp_bar_holder_win.text_plus_one", p);
        icon16 = GUI::GetWindowTypedF<Window>(w, "%s.hp_bar_holder_win.icon_16", p);
        icon16->SetTexture(IconManager::GetIcon("health_16", false), true);
        textBg = GUI::GetWindowTypedF<Textfield>(w, "%s.hp_bar_holder_win.text_bg", p);
        text = GUI::GetWindowTypedF<Textfield>(w, "%s.hp_bar_holder_win.text_hp", p);
        iconHealth = GUI::GetWindowTypedF<Window>(w, "%s.icon_health", p);
        iconHealth->SetTexture(IconManager::GetIcon("health_35", false), true);
        clip = {bar->x, bar->y, bar->w + bar->x, bar->h + bar->y};
        bar->SetClipRect(&clip);
        plus->SetClipRect(&clip);
        text->SetClipRect(&clip);
    }

    // @0x2caa4c: hp of max; under it the time to the next point (or "Health is full").
    void SetHealth(float hp, int max) {
        clip.right = (int)((float)bar->x + ((float)bar->w * hp) / (float)max);
        int now = (int)(hp + 0.5f);
        stat->SetText(SWPrintf(0x80, U"%s: %d/%d", {StringTable::GetString("HINT_HP"), now, max}).c_str());
        plus->SetVisibility(max != now);
        icon16->SetVisibility(max != now);
        text->SetVisibility(true);
        plusBg->SetText(U"+");
        plus->SetText(U"+");
        std::u32string line = max == now ? SWPrintf(0x80, StringTable::GetString("HEAL_SQUAD_HP_FULL"))
                                         : StringTable::GetTimeString(GameState::GetTimeTillNextHP(), false);
        textBg->SetText(line.c_str());
        text->SetText(line.c_str());
    }
};

// HealSquadWindow::HealingGoToCastleButton (0x28 bytes): a wide button, blue (two text lines and a
// flag icon) or green (with a crystal price).
struct BigButton {
    Window* root = nullptr;         // +0x00
    Window* blue = nullptr;         // +0x04
    Window* green = nullptr;        // +0x08
    Textfield* blue1 = nullptr;     // +0x0c
    Textfield* blue2 = nullptr;     // +0x10
    Window* icon = nullptr;         // +0x14
    Window* crystal = nullptr;      // +0x18
    Textfield* green1 = nullptr;    // +0x1c
    Textfield* green2 = nullptr;    // +0x20
    Textfield* price = nullptr;     // +0x24

    // @0x2ca704
    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        blue = GUI::GetWindowTypedF<Window>(w, "%s.button_blue", p);
        green = GUI::GetWindowTypedF<Window>(w, "%s.button_bg_green", p);
        blue1 = GUI::GetWindowTypedF<Textfield>(w, "%s.text_button_blue_01", p);
        blue2 = GUI::GetWindowTypedF<Textfield>(w, "%s.text_button_blue_02", p);
        icon = GUI::GetWindowTypedF<Window>(w, "%s.icon_60_flag", p);
        icon->takesZ = true;
        crystal = GUI::GetWindowTypedF<Window>(w, "%s.icon_crystal", p);
        crystal->takesZ = true;
        green1 = GUI::GetWindowTypedF<Textfield>(w, "%s.text_button_green_01", p);
        green2 = GUI::GetWindowTypedF<Textfield>(w, "%s.text_button_green_02", p);
        price = GUI::GetWindowTypedF<Textfield>(w, "%s.text_price_crystal", p);
    }
};

// A healing item's icon on its way from its slot to the hero's figure (0x24 bytes).
struct Flying {
    Render::Sprite* sprite;   // +0x00
    float fromX, fromY;       // +0x04
    float toX, toY;           // +0x0c
    float time, duration;     // +0x14 +0x18
    uint32_t item;            // +0x1c
    bool used;                // +0x20
};

WindowManager::FunctionalWindow* g_queue = nullptr;
Window* g_root = nullptr;                 // 0x620b1c
GUI::TweenEffect* g_tween = nullptr;      // 0x620fec
Entity* g_attacker = nullptr;             // 0x620b18 who felled him (the arrow points back at it)
std::function<void()> g_postAction;       // 0x620b20 run when the window closes with him alive
int g_deaths[4] = {};                     // 0x620b24..0x620b30 when he last died (the newest last)
HealthBar g_health;                       // 0x620b34
Shared::PlayerHolder g_figure;            // 0x620b68 the hero's dressed figure
Window* g_itemHolder = nullptr;           // 0x620ff8 Character_item_holder.xml (the figure's item cell)
Shared::ItemHolder g_holders[5];          // 0x620d24
std::vector<Items::ItemInfo*> g_items;    // 0x620fcc the healing items offered
std::vector<Flying> g_flying;             // 0x620fd8
int g_broken = 0;                         // 0x620fe8 worn items that are broken (counted on Show)
Window* g_buffHolder = nullptr;           // 0x620ff0
Shared::BuffHolderManager* g_buffs = nullptr;   // the hero's running potions
ItemHoverWindow* g_hover = nullptr;       // 0x620ff4
Window* g_close = nullptr;                // 0x620ffc
Textfield* g_name = nullptr;              // 0x621000
Textfield* g_title = nullptr;             // 0x621004
Textfield* g_inInventory = nullptr;       // 0x621008
BigButton g_healNow;                      // 0x62100c
BigButton g_flee;                         // 0x621034
Window* g_shop = nullptr;                 // 0x62105c
Textfield* g_shopText = nullptr;          // 0x621060
Window* g_repair = nullptr;               // 0x621064
Textfield* g_repairText = nullptr;        // 0x621068
float g_time = 0.f;                       // 0x62106c
bool g_glideStarted = false;              // 0x621070
float g_shownHp = 0.f, g_glideTime = 0.f; // 0x621074 0x621078
float g_flash = 0.f;                      // 0x62107c the figure's green flash after a potion
int g_lastSecond = 0;                     // 0x621080

// The item's amount, less the ones already flying to the figure.
int Available(const Items::ItemInfo* info, bool onlyUnused) {
    int n = GameState::GetItemAmount(info->id, false);
    for (const Flying& f : g_flying)
        if (f.item == info->id && !(onlyUnused && f.used)) --n;
    return n;
}

void StandUp(Entity* player) {
    player->SetAnimationP("idle_1", true, false, false);
    Render::ChangeLayer(player->GetSprite(), 8);
    Render::SortRenderLayer(8, 1);
}

// @0x2cc838: the item arrived: he drinks it.
void OnUseItem(uint32_t id) {
    Entity* player = EntityManager::GetPlayer();
    Items::ItemInfo* info = Items::GetItemInfo(id);
    if (!info || GameState::GetItemAmount(info->id, false) == 0) return;
    if (player->GetHP() == player->GetHpMax()) return;
    if (player->IsDead()) StandUp(player);
    // SoundsManager::PlaySound("drink_potion"): milestone 6.
    info->meta->OnApplyItemEffect(player);
    GameState::RemoveItem(info->id, 1);
    UpdateContents();
}

}  // namespace

// @0x2cc24c: one of item `id` flies from its slot to the hero's figure.
void UseItem(uint32_t id) {
    Items::ItemInfo* info = Items::GetItemInfo(id);
    if (!info) return;
    int n = Available(info, false);
    unsigned slot = 0;
    for (unsigned i = 0; i != 5; ++i)
        if (i < g_items.size() && g_items[i] == info) {
            slot = i;
            break;
        }
    if (n < 1) return;
    Window* icon = g_holders[slot].icon.icon;
    Render::Sprite* s = Render::CreateSprite(icon->texture, 0xf, false, false);
    s->screenSpace = true;
    Flying f{};
    f.sprite = s;
    f.fromX = (float)(icon->x + g_root->x);
    f.fromY = (float)(icon->y + g_root->y);
    f.toX = (float)(g_figure.root->x + g_root->x) + (float)g_figure.root->w * 0.5f + (float)icon->w * -0.5f;
    f.toY = (float)(g_figure.root->y + g_root->y) + (float)g_figure.root->h * 0.5f + (float)icon->h * -0.5f;
    f.time = 0.f;
    f.duration = 0.75f;
    f.item = info->id;
    f.used = false;
    Render::SetPosition(s, f.fromX, f.fromY, 0.01f);
    Render::SetShaderType(s, 1);
    g_flying.push_back(f);
    UpdateContents();
}

namespace {

// @0x2cc644 "Heal now!": a healing item (0x211, else 0x250) if he has one, else for crystals (the
// hero's "char_revivecost"; confirmed: after the purchase prompt).
void OnHealPlayer(bool confirmed) {
    Entity* player = EntityManager::GetPlayer();
    if (player->GetHP() != player->GetHpMax()) {
        if (GameState::GetItemAmount(0x250, false) != 0 || GameState::GetItemAmount(0x211, false) != 0) {
            UseItem(GameState::GetItemAmount(0x211, false) == 0 ? 0x250 : 0x211);
            return;
        }
        int cost = GameState::AdjustCrystalCost(player->GetEntityData()->reviveCost);
        NotEnoughWindow::ResetRequirements();
        NotEnoughWindow::AddRequirement(9, (unsigned)cost);
        if (!NotEnoughWindow::CheckRequirements()) {
            NotEnoughWindow::Show();
            return;
        }
        if (!confirmed) {
            ConfirmPurchaseWindow::SetParameters([] { OnHealPlayer(true); }, (unsigned)cost, true);
            ConfirmPurchaseWindow::Show();
            return;
        }
        // SoundsManager::PlaySound("ui_buy_with_crystals"): milestone 6. (Billing::LogCBPurchase:
        // purchase statistics, removed.)
        GameState::ChangeResourceAmount(9, -cost);
        if (player->IsDead()) player->SetAnimationP("idle_1", true, false, false);
        player->SetHP(player->GetHpMax());
        if (GameState::IsTaskStarted(0x2f9)) Tasks::CompleteSubtask(Tasks::kHealHero, 1, 1);
        GameState::UpdatePlayerRegenerationState();
        Render::ChangeLayer(player->GetSprite(), 8);
        Render::SortRenderLayer(8, 1);
    }
    Hide();
}

// @0x2cb1f8: a tap on a healing item: its card, with the Drink button when he has one.
void OnItem(unsigned i) {
    if (i >= g_items.size() || !g_items[i]) return;
    Items::ItemInfo* info = g_items[i];
    int n = Available(info, false);
    GameState::PlayerItem* item = n < 1 ? nullptr : GameState::GetFirstItemByID(info->id, false);
    // SoundsManager::PlaySound("ui_click"): milestone 6.
    g_hover->Show();
    g_hover->SetItem(StringTable::GetString("BELT_ITEM"), item, info, n > 0, false, nullptr, false, false, true, false);
    Window* h = g_holders[i].root;
    int x = g_root->x + h->x, y = g_root->y + h->y;
    g_hover->SetTargetRect(x, y, x + h->w, y + h->h, ItemHoverWindow::kSE);
    g_hover->RecomputePosition();
    g_hover->SetZ(0.001f);
    Render::SortRenderLayer(0xe, 1);
}

// @0x2cb048 "Flee to castle": hurt and away from home, he gives up the fight and the map.
void OnHome() {
    g_attacker = nullptr;
    Entity* player = EntityManager::GetPlayer();
    Hide();
    if (player->GetHP() == player->GetHpMax()) return;
    g_postAction = nullptr;
    if (player->GetHP() == 0) {
        GameState::ResetTask(0x2f9);
        GameState::StartTask(0x2f9);
    }
    if (GameState::GetCurrentMapID() == 0) return;
    if (GameState::IsCombatActive()) GameState::EndActiveCombat(true);
    Map::SafeSave();
    const bool cached = SaveManager::GetMainSave()->HasMapInCache(0);
    SaveManager::SaveBlock* city = SaveManager::GetMapData(0);
    GameState::SetCurrentMapID(0);
    Map::Load(city, (uint32_t)Rand48::lrand48(), false, cached);
    Map::UpdateOfflineResources();
    Tasks::Update(true);
    TopCityWindow::Show();
    BottomBattleWindow::Hide();
    BattleBarWindow::Show();
    BeltBarWindow::Show();
    HUDWindow::ShowMapName(GameState::GetCastleName());
    GameState::SetCurrentLocation(GameState::GetCurrentMapID() == 0 ? 0 : 2);
}

// @0x2ca494: the shop on the healing potions: 0x250 in tab 3, then 0x162.
void OnShop() {
    ItemShopWindow::Show();
    ItemShopWindow::ShowShopItem(0x250, 0, false, 3);
    ItemShopWindow::ShowShopItem(0x162, 0, false, (unsigned)-1);
}

// @0x2ca440: the character window on its first tab, at the first broken piece worn.
void OnRepair() {
    CharacterInfoWindow::Show();
    CharacterInfoWindow::ShowInventoryTab(0);
    for (unsigned b = 0; b != 10; ++b) {
        GameState::PlayerItem* it = GameState::GetItemAt(b);
        if (it && it->IsBroken()) {
            CharacterInfoWindow::ShowInventoryItem(it->id, 3, it);
            return;
        }
    }
}

// @0x2c9f90 (Move @0x2c9ef8 is empty and never registered.)
float ZRange() { return g_queue->shown ? 0.02f : 0.0001f; }

// @0x2ca358. UNVERIFIED (milestone 5, PlayerWeakWindow): the "too weak" advice on maps 0x71, 0x73,
// 0x75 below level 12, or after the third death within 5 minutes (the death times are kept).
void OnTweenIn() {
    // SoundsManager::PlaySound("ui_heal_opened"): milestone 6.
}

// @0x2ca174
void OnTweenOut() {
    WindowManager::WindowHide(false);
    g_queue->shown = false;
    g_queue->MoveWindowDown(true);
    g_buffs->SetVisibility(false);
    GameState::DropGamePauseState();
    Entity* player = EntityManager::GetPlayer();
    if (g_attacker) {
        // The helper arrow at whoever felled him: a tap takes the fight up again.
        BuildingHovers::SetArrowVisibleWindowLimit(0);
        AIBaseState* ai = player->GetAI();
        Entity* a = g_attacker;
        BuildingHovers::SetArrowClickCallback(new GUI::Callback([ai, a] { ai->ClickedEntity(a); }));
        BuildingHovers::ArrowAt(a->worldX, a->worldY - a->GetIdleHeight(), false, false, false, false, false, false,
                                false);
    }
    // The first broken gear starts the repair quest.
    if (g_broken != 0 && !GameState::IsTaskStarted(0x597)) {
        GameState::ResetTask(0x597);
        GameState::StartTask(0x597);
    }
    if (g_postAction) {
        if (player && player->GetHP() != 0) g_postAction();
        g_postAction = nullptr;
    }
}

// @0x2ca564
bool Click(int x, int y, bool pressed) {
    if (!g_root || !g_root->visibleSelf) return false;
    if (BuildingHovers::ClickOnArrow(x, y, pressed)) return true;
    if (!pressed && GameState::tutorial != 0x61) BuildingHovers::HideArrow();
    if (TopCityWindow::Queue()->fn.click && TopCityWindow::Queue()->fn.click(x, y, pressed)) return true;
    if (g_hover->Click(x, y, pressed)) return true;
    bool visible = g_root->visibleSelf;
    g_root->Click(x, y, pressed, false);
    return visible;
}

// @0x2cb19c
bool OnBack() {
    if (!g_queue->shown) return false;
    if (g_hover->shownHover) g_hover->Hide();
    else OnHome();
    return true;
}

// @0x2ca4c8
void SetZ(float z) {
    if (!g_queue->shown) return;
    g_root->SetZ(z);
    g_root->SetPosition(g_tween->GetCenteredX(), g_tween->GetCenteredY());
    g_buffs->SetZ(g_buffHolder->z);
}

void Update(float dt);

}  // namespace

WindowManager::FunctionalWindow* Queue() {
    if (!g_queue) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.click = Click;
        f.setZ = SetZ;
        f.hide = Hide;
        g_queue = new WindowManager::FunctionalWindow("HealSquadWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_root && g_root->visibleSelf; }           // @0x2c9f10
void SetAttacker(Entity* e) { g_attacker = e; }                      // @0x2c9efc
void SetHealPostAction(std::function<void()> cb) { g_postAction = std::move(cb); }   // @0x2c9f28
void ResetRecentDeaths() { g_deaths[0] = g_deaths[1] = g_deaths[2] = g_deaths[3] = 0; }   // @0x2c9f6c

// @0x2cd3c0
void Init() {
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Window_heal_warrior_small.xml",
                             "Window_heal_warrior_small.png", -1.f, 0, 0, 0, 0, false, 1.f);
    if (!g_root) return;
    Queue()->RegisterTopWindow(g_root);
    g_root->SetVisibility(false);
    g_close = GUI::GetWindowTyped<Window>(g_root, "x_button");
    g_close->SetOnClick(Hide);
    g_close->SetActionSound("ui_close", 1);
    g_tween = GUI::CreateTweenEffect(g_root, nullptr, OnTweenIn, OnTweenOut);
    g_name = GUI::GetWindowTypedF<Textfield>(g_root, "text_player_name");
    g_health.LoadFrom(g_root, "win_health_bar");
    g_figure.LoadFrom(g_root, "character_item_holder");
    g_inInventory = GUI::GetWindowTypedF<Textfield>(g_root, "health_button_holder.text_in_inventory");
    for (int i = 0; i != 5; ++i) {
        char p[128];
        std::snprintf(p, sizeof p,
                      "health_button_holder.healing_use_item_%02d.inventory_slot_holder_01.inventory_item_icon", i + 1);
        g_holders[i].LoadFrom(g_root, p, false);
        g_holders[i].root->SetOnClick([i] { OnItem((unsigned)i); });
    }
    g_holders[4].root->MoveWindow(0, g_holders[1].root->y - g_holders[0].root->y);
    g_shop = GUI::GetWindowTypedF<Window>(g_root, "health_button_holder.healing_shop_button");
    g_shop->SetOnClick(OnShop);
    GUI::GetWindowTypedF<Window>(g_root, "health_button_holder.healing_shop_button.icon_60_flag")
        ->SetTexture(IconManager::GetIcon("Icon_bottom_bar_shop", false), true);
    g_shopText = GUI::GetWindowTypedF<Textfield>(g_root, "health_button_holder.healing_shop_button.text_button_blue_01");
    g_title = GUI::GetWindowTypedF<Textfield>(g_root, "text_heL_hero");
    g_healNow.LoadFrom(g_root, "healing_heal_now");
    g_healNow.root->SetOnClick([] { OnHealPlayer(false); });
    g_flee.LoadFrom(g_root, "healing_go_to_castle");
    g_flee.root->SetOnClick(OnHome);
    g_repair = GUI::GetWindowTypedF<Window>(g_root, "button_hp_items_broken");
    g_repair->SetOnClick(OnRepair);
    GUI::GetWindowTypedF<Window>(g_root, "button_hp_items_broken.icon_35")
        ->SetTexture(IconManager::GetIcon("gold_repair", false), true);
    g_repairText = GUI::GetWindowTypedF<Textfield>(g_root, "button_hp_items_broken.text_repairs");
    g_buffHolder = GUI::GetWindowTypedF<Window>(g_root, "buff_holder_01");
    g_buffHolder->takesZ = true;
    g_buffs = new Shared::BuffHolderManager(0x18, 1, 0, 1);
    g_itemHolder = GUI::RegisterUI("../resource/kingdom_icons/1Original/Character_item_holder.xml", "", 1.f, 0, 0, 0, 0,
                                   false, 1.f);
    g_queue->fn.show = Show;
    g_queue->fn.update = Update;
    g_queue->fn.zRange = ZRange;
    g_queue->fn.back = OnBack;
    g_hover = new ItemHoverWindow();
    ResetRecentDeaths();
}

// @0x2cd1d8
void Show() {
    if (GameState::IsArenaCombatActive() || GameState::GetCurrentMapID() == 0x70) {
        PopupWindow::Show(StringTable::GetString("CAN_NOT_OPEN_IN_ARENA"), PopupWindow::Hide, nullptr, nullptr, nullptr,
                          nullptr);
        return;
    }
    GameState::RaiseGamePauseState();
    const bool wasShown = g_root->visibleSelf;
    if (!wasShown) WindowManager::WindowShow(false);
    g_root->SetVisibility(true);
    g_buffs->SetVisibility(true);
    g_queue->shown = true;
    g_queue->MoveWindowOnTop(true);
    if (!wasShown) g_tween->AnimateIn();
    g_broken = 0;
    g_glideTime = 0.f;
    g_glideStarted = false;
    g_shownHp = 0.f;
    g_attacker = nullptr;
    for (unsigned i = 0; i != 10; ++i) {
        GameState::PlayerItem* it = GameState::GetItemAt(i);
        if (it && it->IsBroken()) ++g_broken;
    }
    Update(0.f);
    g_root->PreloadTextures();
    Entity* player = EntityManager::GetPlayer();
    if (player && player->IsDead()) {
        g_deaths[0] = g_deaths[1];
        g_deaths[1] = g_deaths[2];
        g_deaths[2] = g_deaths[3];
        g_deaths[3] = Timer::GetGlobalTime();
    }
}

// @0x2cafa0
void Hide() {
    if (!g_root || !g_root->visibleSelf) return;
    g_tween->AnimateOut();
    for (Flying& f : g_flying) Render::RemoveSprite(f.sprite);
    g_flying.clear();
}

// @0x2cb3ec
void UpdateContents() {
    Entity* player = EntityManager::GetPlayer();
    // He cannot simply close it while dead away from home (the emperor's map 0xb and 0x74 excepted).
    g_close->SetVisibility(!(GameState::GetCurrentLocation() != 0 && GameState::GetCurrentMapID() != 0xb &&
                             GameState::GetCurrentMapID() != 0x74 && player->GetHP() == 0));
    g_name->SetText(StringTable::GetString("HEAL_SQUAD_HEADER"));
    g_figure.body->SetVisibility(GameState::IsMalePlayer());
    g_figure.bodyFemale->SetVisibility(!GameState::IsMalePlayer());
    // The health that came back while he lay dead in a fight puts him on his feet.
    if (int hp = GameState::GetVirtualHP()) {
        if (player->IsDead()) StandUp(player);
        player->AddHP(hp);
        GameState::ResetVirtualHP();
    }
    bool broken = false;
    for (unsigned i = 0; i != 10; ++i) {
        GameState::PlayerItem* it = GameState::GetItemAt(i);
        if (it) broken = it->IsBroken() || broken;
    }
    PvP::PlayerEquipmentInfo own;
    own.GetPlayerEquipment();
    g_figure.PlacePlayerEquipment(own, (float)g_figure.root->x, (float)g_figure.root->y, g_itemHolder,
                                  GameState::IsMalePlayer(), 1.f, 0.f, 0.f, 0.f, true);

    // The healing items offered: "heal_player_items" (the "_c" list once he has item 0x211 or
    // quest 0x6b4 runs).
    g_items.clear();
    Setting list(GameState::GetItemAmount(0x211, false) == 0 && !Tasks::IsTaskActive(0x6b4, true) ? "heal_player_items"
                                                                                                  : "heal_player_items_c");
    for (unsigned i = 0; i < list.GetChildrenCount(); ++i)
        g_items.push_back(Items::GetItemInfo((uint32_t)list.GetChild(i).GetInt()));
    g_title->SetText(StringTable::GetString("HEAL_SQUAD_HEADER"));
    g_inInventory->SetText(StringTable::GetString("HEAL_SCREEN_IN_INVENTORY"));
    for (unsigned i = 0; i != 5; ++i) {
        Shared::ItemHolder& h = g_holders[i];
        h.root->SetVisibility(i < g_items.size());
        Items::ItemInfo* info = i < g_items.size() ? g_items[i] : nullptr;
        if (!info) {
            h.root->SetVisibility(false);
            continue;
        }
        int n = Available(info, true);
        if (n < 0) n = 0;
        h.icon.redX->SetVisibility(false);
        h.icon.owned->SetVisibility(false);
        h.icon.locked->SetVisibility(n == 0 && info->itemLevel == 0);
        h.icon.broken->SetVisibility(false);
        for (unsigned k = 0; k != 5; ++k)
            h.icon.itemClass[k]->SetVisibility(info->itemLevel == k && (k != 0 || n != 0));
        h.icon.icon->SetTexture(info->GetIcon(), true);
        h.icon.icon->SetEnabled(n != 0);
        h.quantity->SetVisibility(true);
        h.quantity->SetText(ToWideString(n));
        h.quantityRed->SetVisibility(false);
        h.icon.repOverlay->SetVisibility(info->reputationId != 0);
    }

    // "Heal now!": for crystals, or with the healing item he owns.
    if (GameState::GetItemAmount(0x250, false) == 0 && GameState::GetItemAmount(0x211, false) == 0) {
        g_healNow.price->SetVisibility(true);
        g_healNow.crystal->SetVisibility(true);
        g_healNow.green1->SetText(StringTable::GetString("HEAL_SCREEN_NOW"));
        g_healNow.icon->SetTexture(IconManager::GetIcon("health_60", false), true);
        g_healNow.green2->SetText(StringTable::GetString("HEAL_SCREEN_FOR"));
        g_healNow.price->SetText(ToWideString(GameState::AdjustCrystalCost(player->GetEntityData()->reviveCost)));
        g_healNow.crystal->SetTexture(IconManager::GetIcon("35_crystal", false), true);
    } else {
        Items::ItemInfo* info = Items::GetItemInfo(GameState::GetItemAmount(0x211, false) == 0 ? 0x250 : 0x211);
        g_healNow.price->SetVisibility(false);
        g_healNow.crystal->SetVisibility(false);
        g_healNow.green1->SetText(StringTable::GetString("HEAL_SCREEN_NOW"));
        g_healNow.icon->SetTexture(info->GetIcon(), true);
        g_healNow.green2->SetText(info->title);
    }
    // "Flee to castle", with how long the wait for full health is.
    g_flee.icon->SetTexture(IconManager::GetIcon("white_flag_60", false), true);
    if (player->GetHP() == player->GetHpMax()) {
        g_flee.blue2->SetText(StringTable::GetString("HEAL_SQUAD_HP_FULL"));
    } else {
        int missing = ((player->GetHpMax() - 1) - GameState::GetVirtualHP()) - player->GetHP();
        int wait = GameState::GetTimeTillNextHP();
        // PORT: the rate is GameState::HpRegenSeconds (the original: Entity::GetHPRegeneration).
        if (missing >= 1) wait = (int)((float)missing * GameState::HpRegenSeconds(player)) + wait;
        else if (missing != 0) wait = 0;
        g_flee.blue2->SetText(SWPrintf(0x100, StringTable::GetString("HEAL_SCREEN_WAIT"),
                                       {StringTable::GetNumericTimeString(wait, false).c_str()})
                                  .c_str());
    }
    g_flee.blue1->SetText(StringTable::GetString("HEAL_SCREEN_FLEE"));
    g_shop->SetVisibility(GameState::TaskCompleted((unsigned)Setting("shop_unlock_task").GetInt()));
    g_shopText->SetText(StringTable::GetString("GO_TO_SHOP"));
    g_repair->SetVisibility(broken);
    g_repairText->SetText(StringTable::GetString("HEAL_SQUAD_REPAIRS"));
    g_root->UpdatePosition();
}

namespace {

// @0x2cc930
void Update(float dt) {
    if (!g_queue->shown) return;
    if (g_hover->shownHover) {
        float z = PopupWindow::IsVisible() ? g_root->z - 0.01f : 0.001f;
        if (z != g_hover->GetZ()) {
            g_hover->SetZ(z);
            Render::SortRenderLayer(0xe, 1);
        }
    }
    g_buffs->SetPosition(g_buffHolder->x + g_root->x, g_buffHolder->y + g_root->y);
    g_buffs->Update(dt);
    g_time += dt;
    Entity* player = EntityManager::GetPlayer();
    // The bar glides to his health; the window closes by itself when it becomes full.
    float before = g_shownHp;
    float hp = (float)player->GetHP();
    if (!g_glideStarted) g_shownHp = hp;
    g_glideStarted = true;
    g_glideTime += dt;
    if (g_glideTime > 0.0333333f) {
        g_glideTime = 0.f;
        g_shownHp = g_shownHp - (hp - g_shownHp) * -0.133975f;
    }
    g_health.SetHealth(g_shownHp, player->GetHpMax());
    if (g_shownHp != before) {
        g_health.root->UpdatePosition();
        if ((int)(before + 0.5f) != player->GetHpMax() && (int)(g_shownHp + 0.5f) == player->GetHpMax()) Hide();
    }
    // The figure flashes green when a potion reaches it; its broken pieces pulse red (a mended
    // one's red fades).
    g_flash += dt * -3.f;
    for (Window* fig : {g_figure.body, g_figure.bodyFemale}) {
        if (g_flash <= 0.f) {
            fig->SetCustomShader(0, false);
        } else {
            fig->SetCustomShader(0xc, false);
            fig->SetColor(0.f, std::sin((1.f - g_flash) * 3.14159f) * 0.2f, 0.f, false);
        }
    }
    for (unsigned i = 0; i < 10; ++i) {
        GameState::PlayerItem* it = GameState::GetItemAt(i);
        if (!it) continue;
        Window* w = GameState::IsMalePlayer() ? g_figure.layers[i].male : g_figure.layers[i].femaleWindow;
        const float red = 0.15f + std::cos(g_time * 6.f) * 0.15f;
        if (g_flash > 0.f) {
            w->SetCustomShader(0xc, false);
            w->SetColor(0.f, std::sin((1.f - g_flash) * 3.14159f) * 0.2f, 0.f, false);
        } else if (it->IsBroken()) {
            w->SetCustomShader(0xc, false);
            w->SetColor(red, 0.f, 0.f, false);
        } else if (w->sprite && red < w->sprite->color[0]) {
            w->SetColor(red, 0.f, 0.f, false);
        }
    }
    for (size_t i = 0; i < g_flying.size();) {
        Flying& f = g_flying[i];
        f.time += dt;
        float k = f.time / f.duration;
        bool arrived = k > 1.f;
        if (arrived) k = 1.f;
        Render::SetPosition(f.sprite, f.fromX + k * (f.toX - f.fromX), f.fromY + k * (f.toY - f.fromY), 0.01f);
        // It fades over the last 15% of the way. UNVERIFIED: the alpha (the decompile lost
        // Sprite::SetAlpha's argument); a linear fade is used.
        if (arrived || k > 0.85f) Render::SetAlpha(f.sprite, (1.f - k) / 0.15f);
        if (!arrived) {
            // (Another window on top: the item is dropped.)
            if (WindowManager::GetShownWindowCount() > 1) {
                Render::RemoveSprite(f.sprite);
                g_flying[i] = g_flying.back();
                g_flying.pop_back();
            } else {
                ++i;
            }
            continue;
        }
        if (g_flash < 0.f) g_flash = 1.f;
        else if (g_flash < 0.5f) g_flash = 1.f - g_flash;
        f.used = true;
        uint32_t item = f.item;
        Render::RemoveSprite(f.sprite);
        g_flying[i] = g_flying.back();
        g_flying.pop_back();
        OnUseItem(item);
    }
    // The fifth slot beckons while the "_c" list shows (he has item 0x211 or quest 0x6b4 runs).
    if (GameState::GetItemAmount(0x211, false) == 0 && !Tasks::IsTaskActive(0x6b4, true))
        g_holders[4].root->SetAlpha(1.f, true);
    else g_holders[4].root->SetAlpha(1.2f + (float)std::cos((double)g_time * 8.0) * 0.2f, true);
    // The window pauses the game; his health keeps coming back all the same.
    if (GameState::IsPaused()) GameState::UpdateHPRegen(dt);
    if (Timer::GetGlobalTime() != g_lastSecond) {
        g_lastSecond = Timer::GetGlobalTime();
        UpdateContents();
    }
}

}  // namespace

}  // namespace HealSquadWindow
