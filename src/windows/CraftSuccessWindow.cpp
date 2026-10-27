// CraftSuccessWindow: the small "you got it" card (Window_craft_success.xml): a crafted item, a
// spell, a profession's new level, a new battle slot or a new unit, with the item class's text
// underlined and its glow turning behind the icon.
// Port of CraftSuccessWindow (libkingdom.so 5.11), 0x2a38fc..0x2a50ac. Statics at 0x61dfe8.
//
// PORT (online removed): the Facebook/Twitter share (OnShare's WallPost::PostForEvent from level 3,
// the share ticks icon_social_03 and their toggles OnTwitterToggle (0x2a4634) / OnFacebookToggle
// (0x2a4644)) is not offered; the share button is the plain "Ok" and the ticks stay hidden.
// The sound: 6c-2 (with a profession).
#include "engine/SoundsManager.h"
#include "engine/IconManager.h"
#include "engine/Text.h"
#include "engine/Timer.h"
#include "game/BuildingHovers.h"
#include "game/Entity.h"
#include "game/EntityData.h"
#include "game/EntityManager.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/Professions.h"
#include "game/Spells.h"
#include "game/StringTable.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "windows/Windows.h"

namespace CraftSuccessWindow {
namespace {

using GUI::Button;
using GUI::Textfield;
using GUI::Window;

WindowManager::FunctionalWindow* g_queue = nullptr;   // CraftSuccessWindow::wnd 0x61e068
Window* g_root = nullptr;                      // 0x61dfe8 Window_craft_success.xml
GameState::PlayerItem* g_item = nullptr;       // 0x61dfec the crafted item (ItemData)
Spells::SpellInfo* g_spell = nullptr;          // 0x61dff0
Professions::Profession* g_profession = nullptr;   // 0x61dff4 the profession that gained a level
bool g_warriorSlot = false;                    // 0x61dff8 a new battle slot
EntityData* g_entity = nullptr;                // 0x61dffc the unit learned
Textfield* g_text[5] = {};                     // 0x61e000 success_text_holder.text_item_class_01..05
Window* g_underline[5] = {};                   // 0x61e014 craft_underline_image_holder.<class>
Window* g_glow[5] = {};                        // 0x61e028 item_pickup_holder.<class>
Window* g_icon = nullptr;                      // 0x61e03c icon_60
Textfield* g_okText = nullptr;                 // 0x61e040 crafted_item_confirm.share.text_upgrade
Window* g_okIcon = nullptr;                    // 0x61e044 crafted_item_confirm.share.icon_holder
Window* g_social = nullptr;                    // 0x61e048 crafted_item_confirm.icon_social_03
Window* g_share = nullptr;                     // 0x61e04c crafted_item_confirm.share
Window* g_confirm = nullptr;                   // 0x61e050 crafted_item_confirm
Window* g_twitter = nullptr;                   // 0x61e054 ...icon_social_twitter
Window* g_twitterOff = nullptr;                // 0x61e058 ...icon_social_twitter_off
Window* g_tick = nullptr;                      // 0x61e05c ...social_share_tickbox
GUI::TweenEffect* g_tween = nullptr;           // 0x61e060
Button* g_click = nullptr;                     // 0x61e064 crafted_item_confirm.share.clickArea
bool g_tablet = false;                         // 0x60f1f4
float g_scale = 1.f;                           // 0x60f1f8

// The class names of the underline and glow windows (0x601700, 0x601714). The orange glow
// really is "pickup_item_glow".
const char* const kUnderlines[5] = {"craft_underline_common", "craft_underline_green", "craft_underline_blue",
                                    "craft_underline_orange", "craft_underline_purple"};
const char* const kGlows[5] = {"pickup_item_glow_common", "pickup_item_glow_green", "pickup_item_glow_blue",
                               "pickup_item_glow", "pickup_item_glow_purple"};

// @0x2a3b28
void OnTweenOut() {
    WindowManager::WindowHide(false);
    g_queue->shown = false;
    g_queue->MoveWindowDown(true);
    GameState::DropGamePauseState();
}

// @0x2a3b5c: the card's text and icon sit at the class of what it shows (a spell's slot, an
// item's level, 3 for a profession, 2 for a battle slot or a unit); that class is underlined and
// its glow shown.
void UpdateContents() {
    unsigned selected = 0;
    if (!g_item && !g_spell && !g_profession && !g_warriorSlot && !g_entity) return;
    if (g_spell) {
        selected = g_spell->slot;
        // The icons of the three schools are items 0x15f (1), 0x161 (2) and 0x160 (3).
        const uint32_t school = selected == 1 ? 0x15f : selected == 2 ? 0x161 : selected == 3 ? 0x160 : 0;
        if (school)
            if (Items::ItemInfo* info = Items::GetItemInfo(school)) g_icon->SetTexture(info->GetDropIcon(), true);
        g_text[selected]->SetText(StringTable::GetString(g_spell->name));
    }
    if (g_item) {
        selected = g_item->info->itemLevel;
        g_icon->SetTexture(g_item->info->GetDropIcon(), true);
        g_text[selected]->SetText(g_item->info->title);
    }
    if (g_profession) {
        g_icon->SetTexture(g_profession->iconLarge, true);
        const uint32_t id = g_profession->id;
        if (Professions::ProfessionLevel* rank =
                Professions::GetProfessionLevel(id, Professions::GetPlayerProfessionLevel(id))) {
            // (the original builds this text twice, for a profession and for a reputation, alike)
            g_text[3]->SetText(SWPrintf(0x80, U"%s\n%s %d - %s",
                                        {g_profession->title, StringTable::GetString("ARENA_EVENT_RANK"),
                                         (int)rank->level, rank->name}).c_str());
        }
        selected = 3;
    }
    if (g_warriorSlot) {
        selected = 2;
        g_text[2]->SetText(StringTable::GetString("NEW_BATTLE_SLOT"));
        g_icon->SetTexture(IconManager::GetIcon("leadership_flag_120"), true);
    }
    if (g_entity) {
        selected = 2;
        g_text[2]->SetText(StringTable::GetString("DESC1821"));
        g_icon->SetTexture(g_entity->side0, true);
    }
    for (unsigned i = 0; i < 5; ++i) g_text[i]->SetVisibility(i == selected);
    for (unsigned i = 0; i < 5; ++i) g_underline[i]->SetVisibility(i == selected);
    for (unsigned i = 0; i < 5; ++i) g_glow[i]->SetVisibility(i == selected);
    g_okText->SetText(StringTable::GetString("SHARE_OK"));
    g_okIcon->SetTexture(IconManager::GetIcon("gold_confirm"), true);
    // PORT: the share ticks (shown for a card without a battle slot or a unit, from the Twitter /
    // Facebook toggles) stay hidden, so the button is centred as without them.
    g_social->SetVisibility(false);
    g_share->RestorePosition();
    if (!g_social->visibleSelf) g_share->MoveWindow((g_confirm->w - g_share->w) / 2, 0);
}

// @0x2a45d8: the glows turn.
void Update(float) {
    if (!g_root->visibleSelf) return;
    for (Window* glow : g_glow) glow->SetRotation(-(float)Timer::GetTime());
}

// @0x2a4634 / @0x2a4644: PORT (online removed), see the top.
void OnTwitterToggle() {}
[[maybe_unused]] void OnFacebookToggle() {}

// @0x2a4968. PORT: no wall post (see the top of the file); the card only closes.
void OnShare() { Hide(); }

// @0x2a4a64
bool OnBack() {
    if (!g_queue->shown) return false;
    OnShare();
    return true;
}

// @0x2a4a8c
void SetZ(float z) {
    if (g_queue->shown) g_root->SetZ(z);
    if (g_tween->active) return;
    g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::GetVerticalCenter(g_root->h));
}

// @0x2a38fc
bool Click(int x, int y, bool pressed) {
    const bool visible = g_root->visibleSelf;
    g_root->Click(x, y, pressed, false);
    return visible;
}

// @0x2a4b30
void Deinit() {
    GUI::RemoveTweenEffect(g_tween);
    g_tween = nullptr;
    delete g_root;
    g_root = nullptr;
}

}  // namespace

WindowManager::FunctionalWindow* Queue() {
    if (!g_queue) {   // FUN_002a3a28
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.deinit = Deinit;
        f.click = Click;
        f.setZ = SetZ;
        f.hide = Hide;
        g_queue = new WindowManager::FunctionalWindow("CraftSuccessWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_queue->shown; }   // @0x2a3948

// @0x2a395c
void SetParameters(GameState::PlayerItem* item) {
    g_entity = nullptr;
    g_item = item;
    g_spell = nullptr;
    g_profession = nullptr;
    g_warriorSlot = false;
}

// @0x2a3980
void SetParameters(Spells::SpellInfo* spell) {
    g_entity = nullptr;
    g_spell = spell;
    g_item = nullptr;
    g_profession = nullptr;
    g_warriorSlot = false;
}

// @0x2a39a8
void SetParameters(Professions::Profession* profession) {
    g_entity = nullptr;
    g_profession = profession;
    g_item = nullptr;
    g_spell = nullptr;
    g_warriorSlot = false;
}

// @0x2a39d0
void SetParameters(WarriorSlotMode) {
    g_entity = nullptr;
    g_warriorSlot = true;
    g_item = nullptr;
    g_spell = nullptr;
    g_profession = nullptr;
}

// @0x2a39fc
void SetParameters(EntityData* entity) {
    g_entity = entity;
    g_warriorSlot = false;
    g_item = nullptr;
    g_spell = nullptr;
    g_profession = nullptr;
}

// @0x2a4654
void Show() {
    g_queue->shown = true;
    g_queue->MoveWindowOnTop(true);
    if (!g_root->visibleSelf) {
        WindowManager::WindowShow(false);
        g_tween->AnimateIn();
        if (GameState::GetCurrentLocation() == 2) GameState::RaiseGamePauseState();
        if (g_profession) SoundsManager::PlaySound("ui_level_up", 1, false);
    }
    g_root->SetVisibility(true);
    UpdateContents();
    g_root->PreloadTextures();   // GUI::PreloadWindow
}

// @0x2a4718: the card slides out; what it showed continues: an item opens the character window
// on it, a profession's first level opens its recipes (or, for a reputation, its levels, with
// that rank's item dropped at the hero's feet), a new unit opens the hire window on it.
void Hide() {
    if (g_root->visibleSelf) g_tween->AnimateOut();
    if (g_item) {
        CharacterInfoWindow::Show();
        if (GameState::TaskCompleted(0x393)) CharacterInfoWindow::ShowInventoryItem(g_item->info->id, 0, g_item);
        g_item = nullptr;
    }
    g_spell = nullptr;
    if (g_profession) {
        if (Professions::GetPlayerProfessionLevel(g_profession->id) == 1) {
            if (g_profession->type != Professions::kReputation) {
                CraftItemWindow::Show();
                CraftItemWindow::OnTab(0);
                CraftItemWindow::OnListItem(g_profession->tab - 1);
            } else if (GameState::GetCurrentMapID() != 0x70) {
                LevelInfoWindow::Show();
                LevelInfoWindow::OnTab(g_profession->tab, false);
            }
        }
        if (g_profession && g_profession->type == Professions::kReputation && g_profession->id != 7) {
            // The reputation item of the rank just reached (kind 3), dropped twice.
            uint32_t found = 0;
            for (unsigned i = 0; Items::ItemInfo* info = Items::EnumItemInfo(i); ++i)
                if (info->reputationId == g_profession->id &&
                    info->reputationLevel == Professions::GetPlayerProfessionLevel(info->reputationId) &&
                    info->type == 3) {
                    found = info->id;
                    break;
                }
            if (Items::ItemInfo* info = Items::GetItemInfo(found))
                if (Entity* player = EntityManager::GetPlayer()) {
                    BuildingHovers::DropItem(player->worldX, player->worldY, info, 0, 1);
                    BuildingHovers::DropItem(player->worldX, player->worldY, info, 0, 1);
                }
        }
    }
    g_profession = nullptr;
    EntityData* entity = g_entity;
    g_warriorSlot = false;
    if (entity) {
        HireTroopsWindow::Show();
        HireTroopsWindow::ShowForBuilding(0x8e, false);
        HireTroopsWindow::ShowEntity(entity->id);
        HireTroopsWindow::ShowArrow();
    }
    g_entity = nullptr;
}

// @0x2a4b70
void Init() {
    g_tablet = GUI::IsTabletVersion();
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Window_craft_success.xml", "Window_craft_success.png",
                             -1.f, 0, 0, 0, 0, false, 1.f);
    g_queue->RegisterTopWindow(g_root);
    g_scale = g_root->scale;
    g_root->SetVisibility(false);
    g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::GetVerticalCenter(g_root->h));
    g_tween = GUI::CreateTweenEffect(g_root, nullptr, nullptr, OnTweenOut);
    for (int i = 0; i < 5; ++i)
        g_text[i] = GUI::GetWindowTypedF<Textfield>(g_root, "success_text_holder.text_item_class_%02d", i + 1);
    for (int i = 0; i < 5; ++i)
        g_underline[i] = GUI::GetWindowTypedF<Window>(g_root, "craft_underline_image_holder.%s", kUnderlines[i]);
    for (int i = 0; i < 5; ++i)
        g_glow[i] = GUI::GetWindowTypedF<Window>(g_root, "item_pickup_holder.%s", kGlows[i]);
    g_icon = GUI::GetWindowTyped<Window>(g_root, "icon_60");
    g_icon->takesZ = true;
    g_confirm = GUI::GetWindowTyped<Window>(g_root, "crafted_item_confirm");
    g_share = GUI::GetWindowTyped<Window>(g_root, "crafted_item_confirm.share");
    g_okText = GUI::GetWindowTyped<Textfield>(g_root, "crafted_item_confirm.share.text_upgrade");
    g_okIcon = GUI::GetWindowTyped<Window>(g_root, "crafted_item_confirm.share.icon_holder");
    g_okIcon->takesZ = true;
    g_click = GUI::GetWindowTyped<Button>(g_root, "crafted_item_confirm.share.clickArea");
    g_click->SetOnClick(OnShare);
    g_social = GUI::GetWindowTyped<Window>(g_root, "crafted_item_confirm.icon_social_03");
    g_twitter = GUI::GetWindowTyped<Window>(g_root, "crafted_item_confirm.icon_social_03.icon_social_twitter");
    g_twitterOff = GUI::GetWindowTyped<Window>(g_root, "crafted_item_confirm.icon_social_03.icon_social_twitter_off");
    g_tick = GUI::GetWindowTyped<Window>(g_root, "crafted_item_confirm.icon_social_03.social_share_tickbox");
    g_social->SetOnClick(OnTwitterToggle);
    g_queue->fn.update = Update;
    g_queue->fn.isVisible = IsVisible;
    g_queue->fn.zRange = WindowManager::ReturnSmallZRange;
    g_queue->fn.back = OnBack;
    g_queue->fn.show = Show;
}

}  // namespace CraftSuccessWindow
