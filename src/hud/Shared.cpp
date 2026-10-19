#include "hud/Shared.h"

#include "engine/Render.h"

#include "engine/IconManager.h"
#include "engine/Timer.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/MetaData.h"
#include "game/MetaExpression.h"
#include "gui/GUI.h"

#include <cmath>
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
void EndscreenProgressBar::LoadFrom(Window* r, const char* p, const char* holder, const char* fillName) {
    root = GUI::GetWindowTypedF<Window>(r, "%s", p);
    bg = GUI::GetWindowTypedF<Window>(r, "%s.raid_progress_bar_bg", p);
    bg->SetBorders((unsigned)bg->border[0], 1, (unsigned)bg->border[2], (unsigned)bg->border[3]);
    textUnder = GUI::GetWindowTypedF<Textfield>(r, "%s.%s.text_progress_under_02", p, holder);
    textOver = GUI::GetWindowTypedF<Textfield>(r, "%s.%s.text_progress_over_02", p, holder);
    fill = GUI::GetWindowTypedF<Window>(r, "%s.%s.%s", p, holder, fillName);
    endLeft = GUI::GetWindowTypedF<Window>(r, "%s.raid_progress_bar_end_left", p);
    endRight = GUI::GetWindowTypedF<Window>(r, "%s.raid_progress_bar_end_right", p);
    hanger = GUI::GetWindowTypedF<Window>(r, "%s.decor_bottom_hanger", p);
    clip.bottom = 0x800;
    clip.top = 0;
    clip.left = fill->x;
    clip.right = fill->w + fill->x;
    textOver->SetClipRect(&clip);
    fill->SetClipRect(&clip);
}

// @0x343bd8
void ProgressBarLarge::LoadFrom(Window* r, const char* p) {
    root = GUI::GetWindowTypedF<Window>(r, "%s", p);
    textUnder = GUI::GetWindowTypedF<Textfield>(r, "%s.text_progress_under_01", p);
    textOver = GUI::GetWindowTypedF<Textfield>(r, "%s.progress_bar_profession_orange.text_progress_over_01", p);
    fill = GUI::GetWindowTypedF<Window>(r, "%s.progress_bar_profession_orange.progress_bar_large_orange", p);
    lock = GUI::GetWindowTypedF<Window>(r, "%s.lock_01", p);
    clip.bottom = 0x800;
    clip.top = 0;
    clip.left = fill->x;
    clip.right = fill->w + fill->x;
    textOver->SetClipRect(&clip);
    fill->SetClipRect(&clip);
}

// @0x340650
void ProgressBarLarge::SetProgress(float f) { clip.right = (int)((float)clip.left + (float)fill->w * f); }

void EndscreenProgressBar::Rescale(float scale) {
    const int w = root->w;
    root->RestorePosition();
    const int W = GUI::ScreenWidth();
    const int off = (int)(scale * 5.f);
    root->MoveWindow((w - W) / 2 - off, 0);
    const float wide = scale * 12.f;
    bg->SetSize((unsigned)((int)wide + W), (unsigned)bg->h);
    fill->SetSize((unsigned)W, (unsigned)fill->h);
    fill->SetTexture(fill->texture, false, 0, 0, true, 0);
    clip.right = W;
    clip.left = (w - W) / 2;
    endRight->MoveWindow((W - w) + (int)wide, 0);
    hanger->MoveWindow(off + (W - w) / 2, 0);
    textUnder->MoveWindow(off + (W - w) / 2, 0);
    textOver->MoveWindow(off + (W - w) / 2, 0);
}

void EndscreenProgressBar::SetProgress(float f) { clip.right = (int)((float)clip.left + (float)fill->w * f); }

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

// @0x3439cc
void ButtonRaidStart::LoadFrom(Window* r, const char* p) {
    root = GUI::GetWindowTypedF<Window>(r, "%s", p);
    active = GUI::GetWindowTypedF<Window>(r, "%s.button_active", p);
    activeGreen = GUI::GetWindowTypedF<Window>(r, "%s.button_active_green", p);
    simpleRed = GUI::GetWindowTypedF<Window>(r, "%s.button_simple_red", p);
    gold = GUI::GetWindowTypedF<Window>(r, "%s.button_gold", p);
    inactive = GUI::GetWindowTypedF<Window>(r, "%s.button_inactive", p);
    inactive->SetVisibility(false);
    crystalIcon = GUI::GetWindowTypedF<Window>(r, "%s.icon_35_crystal", p);
    crystalIcon->takesZ = true;
    lock = GUI::GetWindowTypedF<Window>(r, "%s.icon_locked_campaign", p);
    text = GUI::GetWindowTypedF<Textfield>(r, "%s.text_start_raid", p);
    textCentered = GUI::GetWindowTypedF<Textfield>(r, "%s.text_start_raid_centered", p);
    textGreen = GUI::GetWindowTypedF<Textfield>(r, "%s.text_start_raid_green", p);
    textCenteredGreen = GUI::GetWindowTypedF<Textfield>(r, "%s.text_start_raid_centered_green", p);
    pvpRed = GUI::GetWindowTypedF<Textfield>(r, "%s.text_pvp_red", p);
    pvpRedCentered = GUI::GetWindowTypedF<Textfield>(r, "%s.text_pvp_red_centered", p);
    goldText = GUI::GetWindowTypedF<Textfield>(r, "%s.text_gold", p);
    price = GUI::GetWindowTypedF<Textfield>(r, "%s.text_price", p);
    icon = GUI::GetWindowTypedF<Window>(r, "%s.icon_start_raid", p);
    icon->takesZ = true;
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

// @0x345ab0
void WarriorSlotInfo::LoadFrom(Window* r, const char* p) {
    root = GUI::GetWindowTypedF<Window>(r, "%s", p);
    lowerPart = GUI::GetWindowTypedF<Window>(r, "%s.bottom_bar_lower_part", p);
    lowerPartDisabled = GUI::GetWindowTypedF<Window>(r, "%s.bottom_bar_lower_part_disabled", p);
    lowerPartDisabled->SetVisibility(false);
    bgSimple = GUI::GetWindowTypedF<Window>(r, "%s.bottom_bar_bg_simple", p);
    bgRed = GUI::GetWindowTypedF<Window>(r, "%s.bottom_bar_bg_red", p);
    bgDisabled = GUI::GetWindowTypedF<Window>(r, "%s.bottom_bar_bg_disabled", p);
    bgDisabled->SetVisibility(false);
    wingsEmpty = GUI::GetWindowTypedF<Window>(r, "%s.bottom_bar_icon_soldiers_wings.icon_wings_empty_slot", p);
    wingsEmpty->SetTexture(IconManager::GetIcon("inv_wings", false), false);
    soldierFine = GUI::GetWindowTypedF<Window>(r, "%s.bottom_bar_icon_soldiers_wings.icon_soldier_fine", p);
    soldierFine->takesZ = true;
    timeLeft = GUI::GetWindowTypedF<Textfield>(r, "%s.text_training_time_left", p);
    lockIcon = GUI::GetWindowTypedF<Window>(r, "%s.icon_holder_16x16", p);
    lockIcon->SetTexture(IconManager::GetIcon("gold_16_lock", false), true);
    golden = GUI::GetWindowTypedF<Window>(r, "%s.slot_golden_borderv", p);
    static const char* const kDiscounts[4] = {"blue", "green", "purple", "red"};
    char name[300];
    for (int i = 0; i < 4; ++i) {
        std::snprintf(name, sizeof name, "%s.discount_label_holder.discount_label_%s", p, kDiscounts[i]);
        discount[i].LoadFrom(r, name);
    }
    hpIcon = GUI::GetWindowTypedF<Window>(r, "%s.unit_info_hp_bar.icon_gold_16_health", p);
    if (!hpIcon->texture) hpIcon->SetTexture(IconManager::GetIcon("gold_16_health", false), true);
    // Each bar: the empty frame, and a copy at its full frame drawn under it, cut at the clip's
    // right edge.
    hpBar = GUI::GetWindowTypedF<Window>(r, "%s.unit_info_hp_bar", p);
    hpBar->SetFrame(1, true);
    hpFill = GUI::DuplicateWindow(hpBar, true, false, nullptr);
    hpFill->SetFrame(100, false);
    hpBar->PrependChild(hpFill);
    hpText2 = GUI::GetWindowTypedF<Textfield>(r, "%s.unit_info_hp_bar.text_hp_bar_2", p);
    hpText = GUI::GetWindowTypedF<Textfield>(r, "%s.unit_info_hp_bar.text_hp_bar", p);
    hpClip = {hpFill->x, 0, hpFill->x + hpFill->w, 0x400};
    hpFill->SetClipRect(&hpClip);
    hpText->SetClipRect(&hpClip);
    trainingType = GUI::GetWindowTypedF<Window>(r, "%s.unit_info_training_bar.unit_type_in_training", p);
    trainingType->takesZ = true;
    trainingBar = GUI::GetWindowTypedF<Window>(r, "%s.unit_info_training_bar", p);
    trainingBar->SetFrame(1, true);
    trainingFill = GUI::DuplicateWindow(trainingBar, true, false, nullptr);
    trainingFill->SetFrame(100, false);
    trainingBar->PrependChild(trainingFill);
    trainingText2 = GUI::GetWindowTypedF<Textfield>(r, "%s.unit_info_training_bar.text_training_bar_2", p);
    trainingText = GUI::GetWindowTypedF<Textfield>(r, "%s.unit_info_training_bar.text_training_bar", p);
    trainingClip = {trainingFill->x, 0, trainingFill->x + trainingFill->w, 0x400};
    trainingFill->SetClipRect(&trainingClip);
    trainingText->SetClipRect(&trainingClip);
    manageSlot = GUI::GetWindowTypedF<Textfield>(r, "%s.text_manage_slot", p);
    slotIsActive = GUI::GetWindowTypedF<Textfield>(r, "%s.text_slot_is_active", p);
    noMore = GUI::GetWindowTypedF<Textfield>(r, "%s.text_no_more", p);
    locked = GUI::GetWindowTypedF<Textfield>(r, "%s.text_locked", p);
    gold = GUI::GetWindowTypedF<Textfield>(r, "%s.text_gold", p);
    quantityNone = GUI::GetWindowTypedF<Textfield>(r, "%s.text_quantity_none", p);
    quantity = GUI::GetWindowTypedF<Textfield>(r, "%s.text_quantity", p);
    quantityActive = GUI::GetWindowTypedF<Textfield>(r, "%s.text_quantity_active", p);
    noDrop = false;
    returnSprite = nullptr;
    dragSprite = nullptr;
    pressed = false;
    dragging = false;
    pressY = 0;
    pressX = 0;
    soldier = 0;
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

void BottomSlotInfo::SetFillPercent(float f) {
    float h = (float)(unsigned)progressH;
    float keep = 1.f - f;
    progress->h = (int16_t)(int)(h * keep);
    progress->SetPosition(progress->x, (int)((float)(unsigned)progressY + h * f));
    if (progress->sprite) progress->sprite->h = keep * h;
}

// ---- BuffHolder / BuffHolderManager ----

void BuffHolder::LoadFrom(Window* parent, const char* prefix, int size) {
    root = prefix ? GUI::GetWindowTypedF<Window>(parent, "%s", prefix) : parent;
    const char* name = size == 0x10 ? "icon_holder_16x16_pure" : size == 0x18 ? "icon_holder_24" : "icon_holder_60";
    if (size != 0x10 && size != 0x18 && size != 0x3c) {
        icon = root;   // UNVERIFIED: the original takes a template window for any other size
    } else if (prefix) {
        icon = GUI::GetWindowTypedF<Window>(parent, "%s.%s", prefix, name);
    } else {
        icon = GUI::GetWindowTypedF<Window>(parent, "%s", name);
    }
    icon->takesZ = true;
    if (prefix) {
        effect = GUI::GetWindowTypedF<Window>(parent, "%s.effect_square_black", prefix);
        clickArea = GUI::GetWindowTypedF<GUI::Button>(parent, "%s.clickArea", prefix);
    } else {
        effect = GUI::GetWindowTypedF<Window>(parent, "effect_square_black");
        clickArea = GUI::GetWindowTypedF<GUI::Button>(parent, "clickArea");
    }
}

BuffHolderManager::BuffHolderManager(int size, int columns, int dirX, int dirY)
    : size(size), columns(columns), dirX(dirX), dirY(dirY) {
    const char* layout = size == 0x10   ? "../resource/kingdom_ui/1Original/Buff_holder_16.xml"
                         : size == 0x18 ? "../resource/kingdom_ui/1Original/Buff_holder_24.xml"
                                        : "../resource/kingdom_ui/1Original/Buff_holder_60.xml";
    if (size != 0x10 && size != 0x18 && size != 0x3c)
        std::printf("ERROR: BuffHolderManager is initialized with incorrect size of %d\n", size);
    for (int i = 0; i < 10; ++i) {
        Slot& s = slots[i];
        Window* w = GUI::RegisterUI(layout, "", 1.f, 0, 0, 0, 0, false, 1.f);
        s.holder.LoadFrom(w, nullptr, size);
        s.holder.root->SetAlpha(0.f, true);
        s.holder.root->SetVisibility(false);
        s.order = i;
    }
    // The clock's mask: the angle round the middle of a 24 x 24 square, 0 .. 255 once round.
    uint8_t px[24 * 24 * 4];
    for (int row = 0; row < 24; ++row) {
        for (int col = 0; col < 24; ++col) {
            float angle = std::atan2(2.f * ((float)col / 23.f) - 1.f, 2.f * ((float)row / 23.f) - 1.f);
            float f = 1.f + (angle / 3.14159274f) * -0.5f + 0.5f;
            f = f - std::floor(f);
            uint8_t v = 0;
            if (!(f < 0.f)) v = f <= 1.f ? (uint8_t)(int)(f * 255.f) : 255;
            uint8_t* p = px + (row * 24 + col) * 4;
            p[0] = p[1] = p[2] = p[3] = v;
        }
    }
    mask = Render::CreateCustomTexture(px, 24, 24);
}

void BuffHolderManager::SetPosition(int nx, int ny) {
    if (x == nx && y == ny) return;
    x = nx;
    y = ny;
    dirty = true;
}

void BuffHolderManager::SetZ(float z) {
    for (Slot& s : slots) s.holder.root->SetZ(z);
}

void BuffHolderManager::SetVisibility(bool v) {
    visible = v;
    if (v) return;
    for (Slot& s : slots) s.holder.root->SetVisibility(false);
}

namespace {
// `v` moves towards `target` by at most `step`.
float Approach(float v, float target, float step) {
    if (v == target) return v;
    if (v < target) return v + step < target ? v + step : target;
    return v - step > target ? v - step : target;
}
}  // namespace

void BuffHolderManager::Update(float dt) {
    for (Slot& s : slots) {
        Window* w = s.holder.root;
        const int line = s.order / columns, place = s.order % columns;
        const int cellH = w->h + 4, cellW = w->w + 4;
        int tx = 0, ty = 0;
        if (dirX != 0) {
            tx = (dirX < 1 ? ~line : line) * cellW;
            ty = place * cellH;
        }
        if (dirY != 0) {
            tx = place * cellW;
            ty = (dirY < 1 ? ~line : line) * cellH;
        }
        s.targetX = (float)tx;
        s.targetY = (float)ty;
        if (s.curX != s.targetX || s.curY != s.targetY || dirty) {
            s.curX = Approach(s.curX, s.targetX, dt * 100.f);
            s.curY = Approach(s.curY, s.targetY, dt * 100.f);
            w->SetPosition((int)((float)x + s.curX), (int)((float)y + s.curY));
        }
        // Fades in while it runs, out when it is over; hidden when gone.
        w->SetAlpha(Approach(w->alpha, s.item == 0 ? 0.f : 1.f, dt * 4.f), true);
        if (w->alpha == 0.f) {
            w->SetVisibility(false);
        } else {
            w->SetVisibility(visible);
            if (Render::Sprite* sp = s.holder.effect->sprite) {
                float ratio = (float)s.remaining / (float)s.total;
                if (ratio > 1.f) ratio = 1.f;
                sp->extra = 1.f - ratio;
                Render::SetShaderType(sp, 5);
            }
        }
    }
    dirty = false;
    // Once a second the holders follow the buffs that run.
    if (lastTime == Timer::GetGlobalTime()) return;
    lastTime = Timer::GetGlobalTime();
    int used = 0;
    for (Slot& s : slots)
        if (s.item != 0) ++used;
    bool freed = false;
    for (unsigned i = 0; Items::ItemInfo* info = Items::EnumItemInfo(i); ++i) {
        if (info->type != 0xe) continue;
        const int left = (int)GameState::GetItemBuffRemainingTime(info->id);
        if (left < 1) {
            for (Slot& s : slots)
                if (s.item == info->id) {
                    s.item = 0;
                    freed = true;
                }
            continue;
        }
        Slot* free = nullptr;
        bool known = false;
        for (Slot& s : slots)
            if (s.item == info->id) {
                s.remaining = left;
                known = true;
                break;
            }
        if (known) continue;
        for (Slot& s : slots)
            if (s.item == 0) {
                free = &s;
                break;
            }
        if (!free) continue;
        free->order = used++;
        free->item = info->id;
        if (info->meta)
            if (const MetaData* d = info->meta->FindChildData(0x2e)) free->total = d->GetInt();
        free->remaining = left;
        free->holder.root->SetAlpha(0.f, false);
        free->holder.icon->SetTexture(info->GetBuffIcon(size), false, 0, 0, false, 0);
        if (!free->holder.effect->sprite) continue;
        Render::SetColorMask(free->holder.effect->sprite, mask);
    }
    if (freed) {
        int n = 0;
        for (Slot& s : slots)
            if (s.item != 0) s.order = n++;
    }
}

}  // namespace Shared
