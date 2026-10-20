// PlayerInfoWindow: a player's profile card (Window_char_info.xml): the flag with "name [level]",
// the dressed figure, the ten equipment slots (tapped: the item's card), and the stats (health and
// damage with his gear and complete sets, victories, valor rank, defeats, level). Port of
// PlayerInfoWindow (libkingdom.so 5.11, @0x32524c..0x3274f0; statics at 0x629e80..0x62a680).
//
// There are two cards: the shown player's (the right one, blue flag) and the player's own (the
// left one, red flag), which comes in beside it when SetParameters' compare flag is set (the PvP
// window's info button). The player's own items' card (the second ItemHoverWindow) is only shown
// in that mode.
// PORT (DECIDED 2026-10-20): the Visit button stays hidden (the original shows it for a compared
// player in the city outside a fight; Neighbours::LoadMap downloads his city: online). The Gift
// button shows greyed out as the original's does for a player who is not a friend (the friends'
// list, NeighboursServer: online).
#include <cstdio>

#include "engine/IconManager.h"
#include "engine/Render.h"
#include "engine/Timer.h"
#include "game/BuildingHovers.h"
#include "game/EntityData.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/MetaData.h"
#include "game/MetaExpression.h"
#include "game/PlayerEquipmentInfo.h"
#include "game/Professions.h"
#include "game/PvP.h"
#include "game/Sets.h"
#include "game/StringTable.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/HUD.h"
#include "hud/ItemHoverWindow.h"
#include "hud/Shared.h"
#include "windows/Windows.h"

namespace PlayerInfoWindow {

namespace {

using GUI::Textfield;
using GUI::Window;

// 0x6017bc: the empty slots' pictures; 0x6017e4: the slots' windows.
const char* const kSlotBackgrounds[10] = {"inv_bg_head",  "inv_bg_sword",  "inv_bg_torso", "inv_bg_foot",
                                          "inv_bg_ring",  "inv_bg_amulet", "inv_bg_armor", "inv_bg_glove",
                                          "inv_bg_belt",  "inv_bg_ring"};
const char* const kSlotNames[10] = {
    "inventory_item_icon_head",       "inventory_item_icon_left_hand", "inventory_item_icon_pants",
    "inventory_item_icon_foot",       "inventory_item_icon_jewels",    "inventory_item_icon_neck",
    "inventory_item_icon_right_hand", "inventory_item_icon_armor",     "inventory_item_icon_ring1",
    "inventory_item_icon_ring2"};

// PlayerInfoWindow::StatisticHolder (0x10 bytes): one stats line.
struct StatisticHolder {
    Window* root = nullptr;           // +0x00
    Textfield* text = nullptr;        // +0x04 text_stat
    Textfield* num = nullptr;         // +0x08 text_stat_num
    Window* icon = nullptr;           // +0x0c icon_stat (takes a depth slot)

    // @0x326da8
    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        text = GUI::GetWindowTypedF<Textfield>(w, "%s.text_stat", p);
        num = GUI::GetWindowTypedF<Textfield>(w, "%s.text_stat_num", p);
        icon = GUI::GetWindowTypedF<Window>(w, "%s.icon_stat", p);
        icon->takesZ = true;
    }
};

// PlayerInfoWindow::ItemBindInfo (0x24 bytes): one equipment slot.
struct ItemBindInfo {
    Window* root = nullptr;           // +0x00 (tapped: OnItem)
    Window* icon = nullptr;           // +0x04 item_icon_holder_bg (takes a depth slot)
    Window* selector = nullptr;       // +0x08 ...item_class_character_selector
    Window* broken = nullptr;         // +0x0c ...item_class_character_broken
    Window* itemClass[5] = {};        // +0x10 ...item_class_character_01..05

    // @0x326e30
    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        icon = GUI::GetWindowTypedF<Window>(w, "%s.item_icon_holder_bg", p);
        icon->takesZ = true;
        selector = GUI::GetWindowTypedF<Window>(w, "%s.inventory_item_character_icon_holder.item_class_character_selector", p);
        broken = GUI::GetWindowTypedF<Window>(w, "%s.inventory_item_character_icon_holder.item_class_character_broken", p);
        for (int k = 0; k != 5; ++k)
            itemClass[k] =
                GUI::GetWindowTypedF<Window>(w, "%s.inventory_item_character_icon_holder.item_class_character_%02d", p, k + 1);
    }
};

// PlayerInfoWindow::WindowData (0x3c0 bytes): one card.
struct WindowData {
    Window* root = nullptr;           // +0x000
    Window* close = nullptr;          // +0x004 x_button
    Textfield* name = nullptr;        // +0x008 text_player_name
    Window* flagRed = nullptr;        // +0x00c flag_pvp_long (the player's own)
    Window* flagBlue = nullptr;       // +0x010 flag_pvp_long_blue
    Shared::PlayerHolder figure;      // +0x014 character_item_holder
    Window* visit = nullptr;          // +0x1d0 button_friend_visit
    Window* visitIcon = nullptr;      // +0x1d4
    Textfield* visitText = nullptr;   // +0x1d8
    Window* gift = nullptr;           // +0x1dc button_friend_gift
    Window* giftIcon = nullptr;       // +0x1e0
    Textfield* giftText = nullptr;    // +0x1e4
    ItemBindInfo binds[10];           // +0x1e8
    Textfield* statsTitle = nullptr;  // +0x350 text_enemy_stats
    StatisticHolder stats[6];         // +0x354 pvp_stat_01..06
    GUI::TweenEffect* tween = nullptr;   // +0x3b4
    Window* itemHolder = nullptr;     // +0x3b8 Character_item_holder.xml (the figure's pieces)
    int gid = 0;                      // +0x3bc the player shown (0: the player himself)

    void Load();
    void UpdateContents();
    void Destroy();
};

WindowManager::FunctionalWindow* g_queue = nullptr;   // 0x62a610
WindowData g_own;                         // 0x629e80 the player's own card (left)
WindowData g_other;                       // 0x62a240 the shown player's card (right)
bool g_compare = false;                   // 0x62a600 the player's own card beside it
ItemHoverWindow* g_hover = nullptr;       // 0x62a604 the shown player's item
uint32_t g_lastTime = 0;                  // 0x62a608 Update's last second
ItemHoverWindow* g_ownHover = nullptr;    // 0x62a60c the player's own item

void OnItem(unsigned i);
void OnVisit();
void OnGift();

// @0x326ff8
void WindowData::Load() {
    root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Window_char_info.xml", "", -1.f, 0, 0, 0, 0, false, 1.f);
    root->SetVisibility(false);
    close = GUI::GetWindowTyped<Window>(root, "x_button");
    close->SetOnClick(Hide);
    close->SetActionSound("ui_close", 1);
    name = GUI::GetWindowTyped<Textfield>(root, "text_player_name");
    flagRed = GUI::GetWindowTyped<Window>(root, "flag_pvp_long");
    flagBlue = GUI::GetWindowTyped<Window>(root, "flag_pvp_long_blue");
    figure.LoadFrom(root, "character_item_holder");
    visit = GUI::GetWindowTyped<Window>(root, "button_friend_visit");
    visit->SetOnClick(OnVisit);
    visitIcon = GUI::GetWindowTyped<Window>(root, "button_friend_visit.icon_35");
    visitIcon->takesZ = true;
    visitText = GUI::GetWindowTyped<Textfield>(root, "button_friend_visit.text_blue");
    gift = GUI::GetWindowTyped<Window>(root, "button_friend_gift");
    gift->SetOnClick(OnGift);
    giftIcon = GUI::GetWindowTyped<Window>(root, "button_friend_gift.icon_35");
    giftIcon->takesZ = true;
    giftText = GUI::GetWindowTyped<Textfield>(root, "button_friend_gift.text_blue");
    for (unsigned i = 0; i != 10; ++i) {
        binds[i].LoadFrom(root, kSlotNames[i]);
        binds[i].root->SetOnClick([i] { OnItem(i); });
    }
    statsTitle = GUI::GetWindowTyped<Textfield>(root, "text_enemy_stats");
    for (int i = 0; i != 6; ++i) {
        char p[32];
        std::snprintf(p, sizeof p, "pvp_stat_%02d", i + 1);
        stats[i].LoadFrom(root, p);
    }
    itemHolder = GUI::RegisterUI("../resource/kingdom_icons/1Original/Character_item_holder.xml", "", 1.f, 0, 0, 0, 0,
                                 false, 1.f);
}

// @0x3253dc: the card of gid (the player himself when no record has it): the flag, the figure,
// the slots, and the stats: his hero's health and melee damage at his level with his unbroken gear,
// customisations and complete sets.
void WindowData::UpdateContents() {
    PvP::PlayerInfo* info = PvP::GetPlayerInfoByGID(gid, 0xff);
    const bool other = info != nullptr;
    const char32_t* playerName = other ? info->name : GameState::GetPlayerName();
    const bool male = other ? info->male : GameState::IsMalePlayer();
    const int level = other ? info->level : GameState::GetLevel();
    name->SetText(SWPrintf(0x100, U"%s [%d]", {playerName, level}).c_str());
    flagRed->SetVisibility(!other);
    flagBlue->SetVisibility(other);
    figure.body->SetVisibility(male);
    figure.bodyFemale->SetVisibility(!male);
    const EntityData* hero = EntityFactory::GetEntityByID(male ? 0xc : 0xd);
    int values[11] = {};
    values[0] = hero->GetHpForLevel(level);
    values[1] = hero->GetAttackMeleeForLevel(level);
    PvP::PlayerEquipmentInfo equip;
    if (!other) equip.GetPlayerEquipment();
    else equip = info->equipment;
    for (unsigned i = 0; i != 10; ++i) {
        ItemBindInfo& b = binds[i];
        Items::ItemInfo* item = equip.GetItemAt((int)i);
        const bool broken = equip.IsItemBrokenAt((int)i);
        if (!item) {
            b.icon->SetTexture(IconManager::GetIcon(kSlotBackgrounds[i], false));
            b.selector->SetVisibility(false);
            b.broken->SetVisibility(false);
            for (Window* w : b.itemClass) w->SetVisibility(false);
            continue;
        }
        b.icon->SetVisibility(item->GetIcon() != nullptr);
        if (item->GetIcon()) item->PutImageInHolder(b.icon);
        if (item->meta && !broken) item->meta->OnApplyItemEffect(values, true);
        b.selector->SetVisibility(false);
        b.broken->SetVisibility(false);
        for (unsigned k = 0; k != 5; ++k) b.itemClass[k]->SetVisibility(item->itemLevel == k);
    }
    for (int i = 1; i != 10; ++i) {
        Items::ItemInfo* c = equip.GetCustomizationAt(i);
        const bool broken = equip.IsCustomizationBrokenAt(i);
        if (c && c->meta && !broken) c->meta->OnApplyItemEffect(values, true);
    }
    Sets::FindPlayerSetState(equip);
    for (unsigned i = 0; Sets::Set* s = Sets::EnumSet(i); ++i)
        for (int k = 0; k != 3; ++k)
            if (s->bonusSlots[k] && s->covered[k] == s->bonusSlots[k]->GetChildrenCount() && s->bonus[k])
                s->bonus[k]->OnApplyItemEffect(values, true);
    figure.PlacePlayerEquipment(equip, (float)figure.root->x, (float)figure.root->y, itemHolder, male, 1.f, 0.f, 0.f,
                                0.f, false);
    // PORT (DECIDED 2026-10-20): hidden. The original shows it for a compared player (other &&
    // g_compare) in the player's own city outside a fight.
    visit->SetVisibility(false);
    visitIcon->SetTexture(IconManager::GetIcon("leadership_flag_60", false));
    visitText->SetText(StringTable::GetString("WEBVIEW_VISIT"));
    if (!other) {
        gift->SetVisibility(false);
        gift->SetEnabled(false);
    } else {
        gift->SetVisibility(g_compare && GameState::IsPlayerCity() && !GameState::IsCombatActive());
        // Enabled for a friend who can get a gift, while gifts are left to send
        // (NeighboursServer::GetMyFriendByGID / CanSendGiftTo: online; offline nobody is a friend).
        gift->SetEnabled(false);
    }
    giftIcon->SetTexture(IconManager::GetIcon("gift_35", false));
    giftText->SetText(StringTable::GetString("GIFT_SEND"));
    if (PlayerCompetitionWindow::GetSelectedPlayerGID() == 1) values[0] = 5;   // the PvP tutorial's first shadow
    statsTitle->SetText(SWPrintf(0x100, StringTable::GetString("STATS_HEADER"), {playerName}).c_str());
    static const char* const kIcons[6] = {"health_35", "army_warrior_40", "pvp_won_35", "pvp_valor_35", "pvp_lost_35", "xp"};
    for (int i = 0; i != 6; ++i) stats[i].icon->SetTexture(IconManager::GetIcon(kIcons[i], false), i != 5);
    static const char* const kTexts[6] = {"INVENTORY_STAT_HEALTH", "INVENTORY_STAT_DAMAGE", "PVP_VICTORIES",
                                          "PVP_RANK",              "PVP_DEFEATS",           "PVP_LEVEL"};
    for (int i = 0; i != 6; ++i) stats[i].text->SetText(StringTable::GetString(kTexts[i]));
    stats[0].num->SetText(ToWideString(values[0]));
    stats[1].num->SetText(ToWideString(values[1]));
    stats[2].num->SetText(ToWideString(other ? info->losses : (int)PvP::GetVictoryCount()));
    const uint32_t valor = other ? (uint32_t)info->f10 : Professions::GetPlayerProfessionLevel(7);
    if (Professions::ProfessionLevel* rank = Professions::GetProfessionLevel(7, valor))
        stats[3].num->SetText(rank->name);
    else
        stats[3].num->SetText(StringTable::GetString("RANK_NONE"));
    stats[4].num->SetText(ToWideString(other ? info->wins : (int)PvP::GetDefeatCount()));
    stats[5].num->SetText(ToWideString(level));
}

// @0x326cec
void WindowData::Destroy() {
    GUI::RemoveTweenEffect(tween);
    tween = nullptr;
    delete itemHolder;
    itemHolder = nullptr;
    delete root;
    root = nullptr;
}

void Move(int, int) {}   // @0x32524c

// @0x325264
float ZRange() { return g_queue->shown ? 0.03f : 0.001f; }

void OnTweenIn() {}   // @0x326404

// @0x3253a8
void OnTweenOut() {
    WindowManager::WindowHide(false);
    g_queue->shown = false;
    g_queue->MoveWindowDown(true);
    GameState::DropGamePauseState();
}

// @0x326684 a slot tapped: the shown player's item there (not in city state 1), and with the
// compare card (or in city state 1) the player's own item there, each pointing at its slot (the
// left column's slots from the west, the right column's from the east).
void OnItem(unsigned i) {
    PvP::PlayerInfo* info = PvP::GetPlayerInfoByGID(g_other.gid, 0xff);
    if (GameState::GetCurrentCity() != 1 && info) {
        if (Items::ItemInfo* item = Items::GetItemInfo(info->equipment.items[i].id)) {
            g_hover->Show();
            g_hover->SetItem(StringTable::GetString("INVENTORY_EQUIPPED_ITEM"), nullptr, item, false, false, nullptr,
                             false, false, false, g_other.gid == -1);
            const Window* s = g_other.binds[i].root;
            const int x = g_other.root->x + s->x, y = g_other.root->y + s->y;
            g_hover->SetTargetRect(x, y, x + s->w, y + s->h, i < 5 ? ItemHoverWindow::kW : ItemHoverWindow::kE);
            g_hover->RecomputePosition();
            g_hover->SetZ(0.001f);
            Render::SortRenderLayer(Render::kLayerGUI, 1);
        }
    }
    GameState::PlayerItem* own = GameState::GetItemAt(i);
    Items::ItemInfo* item = own ? own->info : nullptr;
    if (!g_compare && GameState::GetCurrentCity() != 1) return;
    if (!item) return;
    g_ownHover->Show();
    g_ownHover->SetItem(StringTable::GetString("INVENTORY_EQUIPPED_ITEM"), nullptr, item, true, false, nullptr, false,
                        false, false, false);
    int bx = 0, by = 0;
    BeltBarWindow::GetAdjustedSideButtonLocation(bx, by, 10, 10);   // (unused)
    const Window* s = g_own.binds[i].root;
    const int x = g_own.root->x + s->x, y = g_own.root->y + s->y;
    g_ownHover->SetTargetRect(x, y, x + s->w, y + s->h, i < 5 ? ItemHoverWindow::kW : ItemHoverWindow::kE);
    g_ownHover->RecomputePosition();
    g_ownHover->SetZ(0.001f);
    Render::SortRenderLayer(Render::kLayerGUI, 1);
}

// @0x326b98 (Neighbours::LoadMap(gid, false), the player's city: online; the button stays hidden)
void OnVisit() {
    if (GlobalMapWindow::IsVisible()) GlobalMapWindow::Hide();
    Hide();
    if (PlayerCompetitionWindow::IsVisible()) PlayerCompetitionWindow::Hide();
}

// @0x326ef0: for a friend, the PvP window's gifts tab with him selected (NeighboursServer, the
// friends' list: online; offline there is no friend, so it only closes).
void OnGift() { Hide(); }

// @0x326b18: an item's card first.
bool OnBack() {
    if (!g_queue->shown) return false;
    if (g_hover->shownHover) {
        g_hover->Hide();
        if (g_ownHover->shownHover) g_ownHover->Hide();
        return true;
    }
    Hide();
    return true;
}

// @0x326be0: the helper arrow, the items' cards, then the cards.
bool Click(int x, int y, bool pressed) {
    if (g_other.root->visibleSelf) {
        if (BuildingHovers::ClickOnArrow(x, y, pressed)) return true;
        if (!pressed) BuildingHovers::HideArrow();
    }
    if (g_hover->Click(x, y, pressed)) return true;
    if (g_ownHover->Click(x, y, pressed)) return true;
    const bool r = g_other.root->visibleSelf;
    g_own.root->Click(x, y, pressed, false);
    g_other.root->Click(x, y, pressed, false);
    return r;
}

// @0x326408
void SetZ(float z) {
    if (IsVisible()) {
        g_own.root->SetZ(z);
        g_other.root->SetZ(z);
    }
    if (!g_own.tween->active) g_own.root->SetPosition(g_own.tween->GetCenteredX(), g_own.tween->GetCenteredY());
    if (!g_other.tween->active) g_other.root->SetPosition(g_other.tween->GetCenteredX(), g_other.tween->GetCenteredY());
    if (IsVisible()) UpdateContents();
}

// @0x326544: the item's card stays over a popup; the cards are refreshed once a second.
void Update(float) {
    if (!g_queue->shown) return;
    if (g_hover->shownHover) {
        const float z = g_hover->GetZ();
        auto target = [] {
            return PopupWindow::IsVisible() ? (g_other.root->z - g_queue->ZRange()) + 0.001f : 0.001f;
        };
        if (target() != z) {
            g_hover->SetZ(target());
            Render::SortRenderLayer(Render::kLayerGUI, 1);
        }
    }
    if ((uint32_t)Timer::GetGlobalTime() == g_lastTime) return;
    g_lastTime = (uint32_t)Timer::GetGlobalTime();
    UpdateContents();
}

// @0x326d40
void Deinit() {
    g_own.Destroy();
    g_other.Destroy();
    delete g_hover;
    g_hover = nullptr;
    delete g_ownHover;
    g_ownHover = nullptr;
}

}  // namespace

WindowManager::FunctionalWindow* Queue() {
    if (!g_queue) {   // static FunctionalWindow "PlayerInfoWindow" (FUN_003252b8)
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.deinit = Deinit;
        f.click = Click;
        f.setZ = SetZ;
        f.hide = Hide;
        f.move = Move;
        g_queue = new WindowManager::FunctionalWindow("PlayerInfoWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_queue && g_queue->shown; }   // @0x325250

// @0x325294
void SetParameters(int gid, bool compare) {
    g_compare = compare;
    g_own.gid = 0;
    g_other.gid = gid;
}

// @0x3273f0
void Init() {
    g_own.Load();
    g_other.Load();
    g_own.tween = GUI::CreateTweenEffect(g_own.root, nullptr, nullptr, nullptr);
    g_other.tween = GUI::CreateTweenEffect(g_other.root, nullptr, OnTweenIn, OnTweenOut);
    g_queue->fn.zRange = ZRange;
    g_queue->fn.show = Show;
    g_queue->fn.update = Update;
    g_queue->fn.back = OnBack;
    g_hover = new ItemHoverWindow();
    g_ownHover = new ItemHoverWindow();
}

// @0x32696c: the shown player's card; with the compare flag both, side by side (the player's own
// on the left, the shown one moved right by its width; 69 px apart).
void Show() {
    g_queue->shown = true;
    g_queue->MoveWindowOnTop(true);
    if (!g_other.root->visibleSelf) {
        WindowManager::WindowShow(false);
        if (!g_compare) {
            g_other.tween->insetL = 0;
            g_own.tween->insetR = 0;
        } else {
            g_other.tween->insetL = -g_other.root->w;
            g_own.tween->insetR = g_own.root->w - (int)(g_own.root->scale * 69.f);
        }
        g_own.tween->AnimateIn();
        g_other.tween->AnimateIn();
        GameState::RaiseGamePauseState();
    }
    if (g_compare) g_own.root->SetVisibility(true);
    g_other.root->SetVisibility(true);
    UpdateContents();
    g_own.root->PreloadTextures();     // GUI::PreloadWindow
    g_other.root->PreloadTextures();
}

// @0x326a9c
void Hide() {
    g_hover->Hide();
    g_ownHover->Hide();
    if (g_own.root->visibleSelf) g_own.tween->AnimateOut();
    if (g_other.root->visibleSelf) g_other.tween->AnimateOut();
}

// @0x3263c8 (the player's own card has no close button)
void UpdateContents() {
    g_own.UpdateContents();
    g_other.UpdateContents();
    g_own.close->SetVisibility(false);
}

}  // namespace PlayerInfoWindow
