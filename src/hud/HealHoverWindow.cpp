// HealHoverWindow: see the header.
#include "hud/HealHoverWindow.h"

#include <cstdio>

#include "engine/Render.h"
#include "engine/Resources.h"
#include "game/AI.h"
#include "game/AIState.h"
#include "game/Entity.h"
#include "game/EntityData.h"
#include "game/EntityManager.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/MetaData.h"
#include "game/MetaExpression.h"
#include "game/Setting.h"
#include "game/SoldierSlots.h"
#include "game/Squad.h"
#include "game/StringTable.h"
#include "gui/GUI.h"
#include "hud/HUD.h"
#include "windows/Windows.h"

namespace {
// 0x5a678c: the layout slot of each cell (the cells go down the columns, the slots along the lines).
const unsigned kSlotOrder[10] = {0, 5, 1, 6, 2, 7, 3, 8, 4, 9};
}

HealHoverWindow::HealHoverWindow() {
    name = "HealHoverWindow";
    usesZRange = true;
}

// (Only the vectors are freed; the layout stays, as the original.)
HealHoverWindow::~HealHoverWindow() = default;

void HealHoverWindow::Init() {
    root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Hint_heal_warriors.xml", "Hint_heal_warriors.png",
                           GUI::GetHoverScaleFactor(1.f, 1.f), 0, 0, 0, 0, false, 1.f);
    root->SetVisibility(false);
    title = GUI::GetWindowTyped<GUI::Textfield>(root, "text_header_title");
    char name[160];
    for (unsigned i = 0; i < 10; ++i) {
        std::snprintf(name, sizeof name,
                      "heal_warrior_item_holder.masker_warrior_items.inventory_slot_holder_%02d.inventory_item_icon",
                      kSlotOrder[i] + 1);
        holders[i].LoadFrom(root, name, false);
        holders[i].root->SetOnClick([this, i] { OnItem(i); });
    }
    // The last column waits one column further right (the grid recycles its columns).
    holders[8].root->MoveWindow(holders[2].root->x - holders[0].root->x, 0);
    holders[9].root->MoveWindow(holders[2].root->x - holders[0].root->x, 0);
    scroller.Init();
    GUI::Window* mask = GUI::GetWindowTyped<GUI::Window>(root, "heal_warrior_item_holder.masker_warrior_items.mask_universal");
    scroller.track = mask;
    scroller.viewport = {mask->x, mask->y, mask->x + mask->w, mask->y + mask->h};
    for (Shared::ItemHolder& h : holders) {
        h.root->SetClipRect(&scroller.viewport);
        scroller.windows.push_back(h.root);
    }
    scrollBarBg = GUI::GetWindowTyped<GUI::Window>(root, "heal_warrior_item_holder.scroll_bar_warrior_healer");
    scroller.scrollbar =
        GUI::GetWindowTyped<GUI::Window>(root, "heal_warrior_item_holder.scroll_bar_warrior_healer.scroll_bar_tiny");
    scroller.scrollbar->SetTexture(Resources::GetDirectImage("images/scroll.png"));
    scroller.horizontal = true;
    scroller.unkA0 = 10;
    scroller.itemSize = holders[2].root->x - holders[0].root->x;
    scroller.perLine = 2;
    scroller.onScroll = [this] { UpdateContents(); };
}

void HealHoverWindow::SetZ(float z) {
    if (shownHover) root->SetZ(z);
}

bool HealHoverWindow::Click(int x, int y, bool pressed) {
    if (!shownHover) return false;
    if (scroller.Click(x, y, pressed, root) || root->Click(x, y, pressed, false)) return true;
    Hide();
    return false;
}

void HealHoverWindow::Move(int x, int y) {
    if (shownHover) scroller.Move(x, y);
}

void HealHoverWindow::Show() {
    if (shownHover) return;
    BuildingHoverWindow::Show();
    root->SetVisibility(true);
    UpdateContents();
}

void HealHoverWindow::Hide() {
    if (!shownHover) return;
    root->SetVisibility(false);
    BuildingHoverWindow::Hide();
}

void HealHoverWindow::Update(float dt) {
    if (!shownHover) return;
    scroller.Update(dt);
    scroller.UpdateScroll();
}

void HealHoverWindow::SetPosition(int x, int y) {
    BuildingHoverWindow::SetPosition(x, y);
    root->SetPosition(x, y);
}

void HealHoverWindow::UpdateContents() {
    if (!shownHover || !entity) return;
    // The potions of the setting (item 0x162 only when owned).
    items.clear();
    Setting list("heal_warrior_items");
    for (unsigned i = 0; i < list.GetChildrenCount(); ++i) {
        const uint32_t id = (uint32_t)list.GetChild(i).GetInt();
        if (id != 0x162 || GameState::GetItemAmount(0x162, false) != 0) items.push_back(id);
    }
    title->SetText(ToWideString(U"%s %s %d/%d", {StringTable::GetString("PERFORM_SAVE"),
                                                StringTable::GetString(entity->GetEntityData()->name.c_str()),
                                                entity->GetHP(), entity->GetHpMax()}));
    scroller.itemCount = (unsigned)items.size();
    for (unsigned k = 0; k < 10; ++k) {
        Shared::ItemHolder& h = holders[k];
        const unsigned slot = k + scroller.firstIndex;
        h.root->SetVisibility(slot < items.size());
        if (slot >= items.size()) continue;
        Items::ItemInfo* info = Items::GetItemInfo(items[slot]);
        if (!info) continue;
        for (unsigned j = 0; j < 5; ++j) h.icon.itemClass[j]->SetVisibility(info->itemLevel == j);
        GUI::FitImageIntoWindow(h.icon.icon, info->GetIcon(), true, true);
        h.icon.redX->SetVisibility(false);
        h.icon.owned->SetVisibility(false);
        const int amount = GameState::GetItemAmount(info->id, false);
        h.icon.icon->SetEnabled(amount != 0);
        h.quantity->SetVisibility(amount != 0);
        h.quantity->SetText(ToWideString(U"x%d", {amount}));
        // The health it gives (its effect's HP part), on the green discount badge's text.
        if (info->meta)
            if (const MetaData* hp = info->meta->FindChildData(0x19))
                h.icon.discount[1].text->SetText(
                    ToWideString(U"+%d %s", {hp->GetInt(), StringTable::GetString("stats_hp_points")}));
    }
    root->UpdatePosition();
}

void HealHoverWindow::OnItem(unsigned i) {
    if (!entity) return;
    const unsigned slot = i + scroller.firstIndex;
    if (slot >= items.size()) return;
    Items::ItemInfo* info = Items::GetItemInfo(items[slot]);
    if (!info) return;
    if (GameState::GetItemAmount(info->id, false) == 0) {
        NeedItemWindow::SetParameters(info->id, nullptr);
        NeedItemWindow::Show();
        if (HireTroopsWindow::IsVisible()) HireTroopsWindow::Hide();
        if (ArenaDifficultyWindow::IsVisible()) ArenaDifficultyWindow::Hide();
        if (PlayerInfoWindow::IsVisible()) PlayerInfoWindow::Hide();
        if (PlayerCompetitionWindow::IsVisible()) PlayerCompetitionWindow::Hide();
        Hide();
        return;
    }
    if (!info->meta || !info->meta->FindChildData(0x19)) return;
    GameState::PlayerItem* item = GameState::GetFirstItemByID(info->id, false);
    if (!item) return;
    // UNVERIFIED (milestone 6): SoundsManager::PlaySound("drink_potion", 1, false).
    const bool wasDead = entity->IsDead();
    info->meta->OnApplyItemEffect(entity);
    GameState::RemoveUniqueItem(item->uniqueId);
    if (wasDead) {
        // Back on its feet: on the units' layer, into the hero's squad when it has a slot there,
        // next to the hero.
        Render::ChangeLayer(entity->GetSprite(), 8);
        Render::SortRenderLayer(8, 1);
        bool inSlots = false;
        for (unsigned s = 0; s < SoldierSlots::GetTotalSlotCount(); ++s)
            if (SoldierSlots::GetSoldierBySlot(s) == entity) inSlots = true;
        Entity* player = EntityManager::GetPlayer();
        bool inSquad = false;
        for (unsigned s = 0; s < (unsigned)player->GetSquad()->GetSoldierCount(); ++s)
            if (player->GetSquad()->GetSoldier(s) == entity) inSquad = true;
        if (inSlots) {
            if (inSquad) {
                entity->OnAddedToSquad(player->GetSquad());
                entity->SetAnimationP("idle_1", true, false, false);
            } else {
                player->GetSquad()->AddSoldier(entity);
                entity->SetAnimationP("idle_1", true, false, inSquad);
            }
            if (AI::Waypoint* wp = AI::GetRandomWaypointInRange(player->tileX, player->tileY, 2, 1, false, nullptr, false))
                EntityManager::SpawnEntityAt(entity, (unsigned)wp->x, (unsigned)wp->y, true, true);
            entity->GetAI()->Reset(false);
            Render::CenterOn(entity->worldX, entity->worldY, true, 0.4f, 0, false);
        }
        entity->UpdateRing();
    }
    if (HireTroopsWindow::IsVisible()) HireTroopsWindow::UpdateContents();
    BattleBarWindow::UpdateContents();
    if (entity->GetHP() != entity->GetHpMax()) {
        UpdateContents();
        return;
    }
    Hide();
}
