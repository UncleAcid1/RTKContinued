#include "hud/Shared.h"

#include "engine/IconManager.h"
#include "game/Items.h"
#include "gui/GUI.h"

#include <cstdio>
#include <string>

namespace Shared {

using GUI::Textfield;
using GUI::Window;

// @0x3445f0
void BottomSlotInfo::LoadFrom(Window* root, const char* p) {
    holder = GUI::GetWindowTypedF<Window>(root, "%s", p);
    bgSimple = GUI::GetWindowTypedF<Window>(root, "%s.bottom_bar_bg_simple", p);
    bgGreen = GUI::GetWindowTypedF<Window>(root, "%s.bottom_bar_bg_green", p);
    bgRed = GUI::GetWindowTypedF<Window>(root, "%s.bottom_bar_bg_red", p);
    bgDisabled = GUI::GetWindowTypedF<Window>(root, "%s.bottom_bar_bg_disabled", p);
    wingsEmpty = GUI::GetWindowTypedF<Window>(root, "%s.bottom_bar_icon_soldiers_wings.icon_wings_empty_slot", p);
    wingsEmpty->SetTexture(IconManager::GetIcon("inv_wings", false), true);
    soldierFine = GUI::GetWindowTypedF<Window>(root, "%s.bottom_bar_icon_soldiers_wings.icon_soldier_fine", p);
    soldierFine->takesZ = true;
    iconHolder = GUI::GetWindowTypedF<Window>(root, "%s.icon_holder_belt_warrior", p);
    iconHolder->takesZ = true;
    progress = GUI::GetWindowTypedF<Window>(root, "%s.item_proress_bar", p);
    progress->SetFrame(1, true);
    progress->centerSprite = false;
    progressH = progress->h;
    progressY = progress->y;
    timeLeft = GUI::GetWindowTypedF<Textfield>(root, "%s.text_training_time_left", p);
    damage = GUI::GetWindowTypedF<Window>(root, "%s.damage_effect_square_red", p);
    damageTime = 1.f;   // DAT_00344868
    damageAlpha = 0.f;  // DAT_0034486c
    golden = GUI::GetWindowTypedF<Window>(root, "%s.slot_golden_borderv", p);
    lockIcon = GUI::GetWindowTypedF<Window>(root, "%s.icon_holder_16x16", p);
    lockIcon->SetTexture(IconManager::GetIcon("gold_16_lock", false), true);
    quantityNone = GUI::GetWindowTypedF<Textfield>(root, "%s.text_quantity_none", p);
    quantity = GUI::GetWindowTypedF<Textfield>(root, "%s.text_quantity", p);
    quantityActive = GUI::GetWindowTypedF<Textfield>(root, "%s.text_quantity_active", p);
    unk64 = 0.f;
    unk50 = 0;
    progressValue = 1.f;
}

// @0x343634
void ButtonBuildingControls::LoadFrom(Window* r, const char* p) {
    if (!p) {
        root = r;
        red = GUI::GetWindowTyped<Window>(r, "button_large_round_sp_red");
        green = GUI::GetWindowTyped<Window>(r, "button_large_round_sp_green");
        blue = GUI::GetWindowTyped<Window>(r, "button_bg_blue_back");
        icon = GUI::GetWindowTyped<Window>(r, "icon_60");
        icon->takesZ = true;
        text = GUI::GetWindowTyped<Textfield>(r, "text");
        counter = GUI::GetWindowTyped<Textfield>(r, "text_counter_red");
        clickArea = GUI::GetWindowTyped<GUI::Button>(r, "clickArea");
    } else {
        root = GUI::GetWindowTypedF<Window>(r, "%s", p);
        red = GUI::GetWindowTypedF<Window>(r, "%s.button_large_round_sp_red", p);
        green = GUI::GetWindowTypedF<Window>(r, "%s.button_large_round_sp_green", p);
        blue = GUI::GetWindowTypedF<Window>(r, "%s.button_bg_blue_back", p);
        icon = GUI::GetWindowTypedF<Window>(r, "%s.icon_60", p);
        icon->takesZ = true;
        text = GUI::GetWindowTypedF<Textfield>(r, "%s.text", p);
        counter = GUI::GetWindowTypedF<Textfield>(r, "%s.text_counter_red", p);
        clickArea = GUI::GetWindowTypedF<GUI::Button>(r, "%s.clickArea", p);
    }
    unk28 = 0;
}

void ButtonTripleInfo::LoadFrom(Window* r, const char* p) {
    root = GUI::GetWindowTypedF<Window>(r, "%s", p);
    blue = GUI::GetWindowTypedF<Window>(r, "%s.button_active_large", p);
    green = GUI::GetWindowTypedF<Window>(r, "%s.button_active_green", p);
    gold = GUI::GetWindowTypedF<Window>(r, "%s.button_gold", p);
    inactive = GUI::GetWindowTypedF<Window>(r, "%s.button_inactive", p);
    inactive->SetVisibility(false);
    text = GUI::GetWindowTypedF<Textfield>(r, "%s.text", p);
    icon = GUI::GetWindowTypedF<Window>(r, "%s.icon_build", p);
    icon->takesZ = true;
    textGold = GUI::GetWindowTypedF<Textfield>(r, "%s.text_gold", p);
    textPrice = GUI::GetWindowTypedF<Textfield>(r, "%s.text_price", p);
    crystal = GUI::GetWindowTypedF<Window>(r, "%s.icon_35_crystal", p);
}

namespace {
void ShowColored(ButtonTripleInfo& b, bool blue, const char32_t* text, Render::Texture* icon) {
    b.blue->SetVisibility(blue);
    b.green->SetVisibility(!blue);
    b.gold->SetVisibility(false);
    b.text->SetVisibility(true);
    b.icon->SetVisibility(true);
    b.textGold->SetVisibility(false);
    b.textPrice->SetVisibility(false);
    b.crystal->SetVisibility(false);
    b.text->SetText(text);
    b.icon->SetTexture(icon, true, 0, 0, false, 0);
}
}  // namespace

void ButtonTripleInfo::ShowBlue(const char32_t* t, Render::Texture* i) { ShowColored(*this, true, t, i); }
void ButtonTripleInfo::ShowGreen(const char32_t* t, Render::Texture* i) { ShowColored(*this, false, t, i); }

void SmallLogoWindow::LoadFrom(Window* r, const char* p) {
    holder = GUI::GetWindowTypedF<Window>(r, "%s", p);
    eng = GUI::GetWindowTypedF<Window>(r, "%s.logo_small_eng", p);
    lv = GUI::GetWindowTypedF<Window>(r, "%s.logo_small_lv", p);
    rus = GUI::GetWindowTypedF<Window>(r, "%s.logo_small_rus", p);
}

void SmallLogoWindow::SetLanguage(unsigned langId) {
    eng->SetVisibility(false);
    lv->SetVisibility(false);
    rus->SetVisibility(false);
    if (langId == 1) rus->SetVisibility(true);
    else if (langId != 3) eng->SetVisibility(true);
    else lv->SetVisibility(true);
}

void LargeLogoWindow::LoadFrom(Window* r, const char* p) {
    holder = GUI::GetWindowTypedF<Window>(r, "%s", p);
    eng = GUI::GetWindowTypedF<Window>(r, "%s.logo_large_eng", p);
    lv = GUI::GetWindowTypedF<Window>(r, "%s.logo_large_lv", p);
    rus = GUI::GetWindowTypedF<Window>(r, "%s.logo_large_rus", p);
}

void LargeLogoWindow::SetLanguage(unsigned langId) {
    eng->SetVisibility(false);
    lv->SetVisibility(false);
    rus->SetVisibility(false);
    if (langId == 1) rus->SetVisibility(true);
    else if (langId != 3) eng->SetVisibility(true);
    else lv->SetVisibility(true);
}

void TabHolder::LoadFrom(Window* r, const char* p, bool withLock) {
    root = GUI::GetWindowTypedF<Window>(r, "%s", p);
    active = GUI::GetWindowTypedF<Window>(r, "%s.tab_active", p);
    inactive = GUI::GetWindowTypedF<Window>(r, "%s.tab_inactive", p);
    activeRed = GUI::GetWindowTypedF<Window>(r, "%s.tab_active_red", p);
    inactiveRed = GUI::GetWindowTypedF<Window>(r, "%s.tab_inactive_red", p);
    locked = withLock ? GUI::GetWindowTypedF<Window>(r, "%s.tab_locked", p) : GUI::DummyWindow();
    locked->SetVisibility(false);
    icon = GUI::GetWindowTypedF<Window>(r, "%s.icon_place_holder", p);
    icon->takesZ = true;
    lockIcon = GUI::GetWindowTypedF<Window>(r, "%s.icon_gold_16_lock", p);
    unk20 = 0;
    lockIcon->takesZ = true;
}

void TabHolder::SetMode(bool isActive, bool red) {
    icon->RestorePosition();
    if (isActive) {
        if (icon->root) icon->MoveWindow(0, (int)(icon->root->scale * -4.f));
        icon->UpdatePosition();
        active->SetVisibility(!red);
        inactive->SetVisibility(false);
        activeRed->SetVisibility(red);
        inactiveRed->SetVisibility(false);
    } else {
        icon->UpdatePosition();
        active->SetVisibility(false);
        inactive->SetVisibility(!red);
        activeRed->SetVisibility(false);
        inactiveRed->SetVisibility(red);
    }
}

void HouseLivingInfo::LoadFrom(Window* r, const char* p) {
    root = GUI::GetWindowTypedF<Window>(r, "%s", p);
    time = GUI::GetWindowTypedF<Textfield>(r, "%s.text_time", p);
    icon = GUI::GetWindowTypedF<Window>(r, "%s.icon_60_converted_to", p);
    icon->takesZ = true;
    tax = GUI::GetWindowTypedF<Textfield>(r, "%s.text_tax", p);
}

void HouseTrainingInfo::LoadFrom(Window* r, const char* p) {
    root = GUI::GetWindowTypedF<Window>(r, "%s", p);
    icon = GUI::GetWindowTypedF<Window>(r, "%s.icon_60", p);
    icon->takesZ = true;
    text = GUI::GetWindowTypedF<Textfield>(r, "%s.text_hired", p);
}

void HouseConvertingInfo::LoadFrom(Window* r, const char* p) {
    root = GUI::GetWindowTypedF<Window>(r, "%s", p);
    text = GUI::GetWindowTypedF<Textfield>(r, "%s.text_converts", p);
    from = GUI::GetWindowTypedF<Window>(r, "%s.icon_60_converted_from", p);
    from->takesZ = true;
    to = GUI::GetWindowTypedF<Window>(r, "%s.icon_60_converted_to", p);
    to->takesZ = true;
}

void HouseProducingInfo::LoadFrom(Window* r, const char* p) {
    root = GUI::GetWindowTypedF<Window>(r, "%s", p);
    text = GUI::GetWindowTypedF<Textfield>(r, "%s.text_converts", p);
    for (int i = 0; i < 5; ++i) {
        icons[i] = GUI::GetWindowTypedF<Window>(r, "%s.upgrade_unlocked_item_holder_%02d.icon_active_01", p, i + 1);
        icons[i]->takesZ = true;
    }
    for (int i = 0; i < 5; ++i)
        confirms[i] = GUI::GetWindowTypedF<Window>(r, "%s.upgrade_unlocked_item_holder_%02d.icon_gold_confirm", p, i + 1);
    for (int i = 0; i < 5; ++i)
        locks[i] = GUI::GetWindowTypedF<Window>(r, "%s.upgrade_unlocked_item_holder_%02d.icon_lock", p, i + 1);
}

void HouseDecorationInfo::LoadFrom(Window* r, const char* p) {
    root = GUI::GetWindowTypedF<Window>(r, "%s", p);
    text = GUI::GetWindowTypedF<Textfield>(r, "%s.text_decor", p);
}

void HouseLivingWorkerInfo::LoadFrom(Window* r, const char* p) {
    root = GUI::GetWindowTypedF<Window>(r, "%s", p);
    time = GUI::GetWindowTypedF<Textfield>(r, "%s.text_time", p);
    icon = GUI::GetWindowTypedF<Window>(r, "%s.icon_60_converted_to", p);
    icon->takesZ = true;
    tax = GUI::GetWindowTypedF<Textfield>(r, "%s.text_tax", p);
    workers = GUI::GetWindowTypedF<Textfield>(r, "%s.text_worker", p);
}

// @0x343984
void DiscountInfo::LoadFrom(Window* r, const char* p) {
    root = GUI::GetWindowTypedF<Window>(r, "%s", p);
    text = GUI::GetWindowTypedF<Textfield>(r, "%s.text_discount", p);
}

// @0x3448b8
void ItemIconHolder::LoadFrom(Window* r, const char* p, bool withRepOverlay) {
    root = GUI::GetWindowTypedF<Window>(r, "%s", p);
    locked = GUI::GetWindowTypedF<Window>(r, "%s.item_class_locked", p);
    broken = GUI::GetWindowTypedF<Window>(r, "%s.item_class_broken", p);
    repOverlay = withRepOverlay ? GUI::GetWindowTypedF<Window>(r, "%s.shop_rep_item_overlay", p) : GUI::DummyWindow();
    for (int i = 0; i < 5; ++i) itemClass[i] = GUI::GetWindowTypedF<Window>(r, "%s.item_class_%02d", p, i + 1);
    icon = GUI::GetWindowTypedF<Window>(r, "%s.item_icon_holder_item", p);
    icon->takesZ = true;
    static const char* const kLabels[4] = {"blue", "green", "purple", "red"};
    char name[0x100];
    for (int i = 0; i < 4; ++i) {
        std::snprintf(name, sizeof name, "%s.discount_label_holder.discount_label_%s", p, kLabels[i]);
        discount[i].LoadFrom(r, name);
    }
    lock = GUI::GetWindowTypedF<Window>(r, "%s.lock", p);
    lockLevel = GUI::GetWindowTypedF<Textfield>(r, "%s.lock.text_level_req", p);
    redX = GUI::GetWindowTypedF<Window>(r, "%s.icon_red_x", p);
    if (!redX->texture) redX->SetTexture(IconManager::GetIcon("red_x"), true);
    owned = GUI::GetWindowTypedF<Window>(r, "%s.icon_gold_owned", p);
    if (!owned->texture) owned->SetTexture(IconManager::GetIcon("gold_confirm"), true);
    recommended = GUI::GetWindowTypedF<Window>(r, "%s.recomended_item", p);
    state = GUI::GetWindowTypedF<Window>(r, "%s.icon_16_state", p);
    lockSmall = GUI::GetWindowTypedF<Window>(r, "%s.lock_small", p);
    lockSmall->takesZ = true;
    repProf = GUI::GetWindowTypedF<Window>(r, "%s.icon_16_rep_prof", p);
    repProf->takesZ = true;
    levelReq = GUI::GetWindowTypedF<Textfield>(r, "%s.text_level_req", p);
    itemId = 0;
}

// @0x3459ec
void ItemIconHolder::SetItem(Items::ItemInfo* info) {
    if (!info) {
        itemId = 0;
        return;
    }
    itemId = info->id;
    for (unsigned i = 0; i < 5; ++i) itemClass[i]->SetVisibility(info->itemLevel == i);
    info->PutImageInHolder(icon);
    redX->SetVisibility(false);
    owned->SetVisibility(false);
    recommended->SetVisibility(false);
    state->SetVisibility(false);
}

// @0x344c5c
void ItemHolder::LoadFrom(Window* r, const char* p, bool repOverlay) {
    root = GUI::GetWindowTypedF<Window>(r, "%s", p);
    selected = GUI::GetWindowTypedF<Window>(r, "%s.selected_item", p);
    emptyBg = GUI::GetWindowTypedF<Window>(r, "%s.item_icon_inventory_empty_bg", p);
    char name[0x100];
    std::snprintf(name, sizeof name, "%s.inventory_item_icon_holder", p);
    icon.LoadFrom(r, name, repOverlay);
    quantity = GUI::GetWindowTypedF<Textfield>(r, "%s.text_quantity", p);
    quantityRed = GUI::GetWindowTypedF<Textfield>(r, "%s.text_quantity_red", p);
    lockTiny = GUI::GetWindowTypedF<Window>(r, "%s.icon_lock_tiny", p);
}

// @0x342ef0 Shared::TaskWindow::LoadFrom
void TaskWindow::LoadFrom(GUI::Window* r, const char* prefix) {
    std::string pre = prefix ? std::string(prefix) + "." : std::string();
    auto win = [&](const char* n) { return GUI::GetWindowTyped<GUI::Window>(r, (pre + n).c_str()); };
    auto text = [&](const char* n) { return GUI::GetWindowTyped<GUI::Textfield>(r, (pre + n).c_str()); };
    auto slotted = [&](const char* n) {
        GUI::Window* w = win(n);
        w->takesZ = true;
        return w;
    };
    root = prefix ? GUI::GetWindowTyped<GUI::Window>(r, prefix) : r;
    bg = win("button_scroll_bg");
    bgBattle = win("button_scroll_bg_battle");
    bgBattleSide = win("button_scroll_bg_battle_side");
    campaign = win("button_campaign_quest");
    desert = win("button_desert_quest");
    arena = win("button_arena_quest");
    campaignPercent = text("button_campaign_quest.text_percent");
    desertPercent = text("button_desert_quest.text_percent");
    campaignDone = win("button_campaign_100_quest");
    raid = win("button_raid_quest");
    quick = win("quick_task_bg_generic");
    quickBlue = win("quick_task_bg_blue");
    quickGreen = win("quick_task_bg_green");
    quickOrange = win("quick_task_bg_orange");
    quickRed = win("quick_task_bg_red");
    allQuestsBg = win("button_bg_all_quests");
    allQuestsText = text("text_all_quests");
    allQuestsIcon = win("icon_all_quests");
    icon = slotted("button_scroll_bg.icon");
    iconBattle = slotted("button_scroll_bg_battle.icon");
    iconBattleSide = slotted("button_scroll_bg_battle_side.icon");
    iconQuick = slotted("icon_all_quests_quick_task");
    iconNew = slotted("icon_gold_quest_new");
    iconAll = slotted("icon_all_quests");
    timeText = text("text_goblin_time_2");
    status = text("text_status");
    clickArea = GUI::GetWindowTyped<GUI::Button>(r, (pre + "clickArea").c_str());
    taskId = 0;
    slot = target = 0;
    isNew = false;
    animating = false;
    bannerTimer = 0.f;
}

// @0x3408dc Shared::TaskWindow::Animate
void TaskWindow::Animate(int x0, int y0, int x1, int y1) {
    animating = true;
    animTime = 0.f;
    animDuration = 0.5f;
    fromX = x0;
    fromY = y0;
    toX = x1;
    toY = y1;
    animMode = 0;
}

}  // namespace Shared
