// ItemHoverWindow (see ItemHoverWindow.h). Offsets are the original's.
#include "hud/ItemHoverWindow.h"

#include <cstdio>
#include <cstdlib>

#include "engine/IconManager.h"
#include "engine/Render.h"
#include "game/AIState.h"
#include "game/Building.h"
#include "game/BuildingHovers.h"
#include "game/BuildingPlacement.h"
#include "game/Entity.h"
#include "game/EntityData.h"
#include "game/EntityManager.h"
#include "game/GameData.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/Map.h"
#include "game/MetaData.h"
#include "game/MetaExpression.h"
#include "game/Setting.h"
#include "game/SoldierSlots.h"
#include "game/StringTable.h"
#include "gui/GUI.h"
#include "hud/HUD.h"
#include "windows/Windows.h"

namespace {
// The hover layouts are 1.2 times larger unless both screen sides are under 800 pixels.
float LayoutScale() { return GUI::ScreenWidth() < 800 && GUI::ScreenHeight() < 800 ? 1.f : 1.2f; }

GUI::Window* Register(std::vector<GUI::Window*>& list, const char* layout, const char* image, float scale) {
    GUI::Window* w = GUI::RegisterUI(layout, image, scale, 0, 0, 0, 0, false, 1.f);
    list.push_back(w);
    return w;
}

void SetLine(ItemHoverWindow::EffectLine& l, const char32_t* text, const char* icon) {
    l.root->SetVisibility(true);
    l.text->SetText(text);
    l.icon->SetVisibility(true);
    l.icon->SetTexture(IconManager::GetIcon(icon), true);
}

// Systems of later milestones the window asks about. UNVERIFIED stand-ins until they are ported.
bool LevelInfoWindowIsVisible() { return false; }          // LevelInfoWindow::IsVisible (5)
bool RaidInfoWindowIsVisible() { return false; }           // RaidInfoWindow::IsVisible (5)
bool ChestsWindowIsVisible() { return false; }             // ChestsWindow::IsVisible (5)
bool IsChestItem(uint32_t) { return false; }               // Chests::IsChestItem (5)
bool ItemCanBeBought(uint32_t) { return false; }           // ItemShopWindow::ItemCanBeBought (5)
uint32_t GetMissionMapForItem(uint32_t) { return 0; }      // GlobalMapWindow::GetMissionMapForItem (5)
uint32_t GetMapForCraftItem(uint32_t) { return 0; }        // GlobalMapWindow::GetMapForCraftItem (5)
bool IsTaskActive(unsigned) { return false; }              // Tasks::IsTaskActive (4c)

// @0x27b958 CharacterInfoWindow::IsItemEquipped(ItemData*): in the window's ten equipment holders
// or on the belt. UNVERIFIED (milestone 5): the holders (0x619440..) are filled by the window's
// UpdateContents; the port reads the equipment bindings they show.
bool IsItemEquipped(const GameState::PlayerItem* it) {
    if (!it) return false;
    bool on = false;
    for (unsigned i = 0; i < 10; ++i)
        if (GameState::GetItemAt(i) == it) on = true;
    for (unsigned i = 0; i < GameState::GetBeltSlotCount(); ++i)
        if (GameState::GetBeltItemAt(i) == it->id) on = true;
    return on;
}

// @0x27b8c8 CharacterInfoWindow::CanTakeOnItem: not locked by level, and an equipment slot, a
// profession tool or a free (or used-up) belt slot.
bool CanTakeOnItem(const GameState::PlayerItem* it) {
    if (!it || it->info->IsLockedByLevel()) return false;
    if (it->info->slot != 0 || it->info->IsProfessionTool()) return true;
    for (unsigned i = 0; i < GameState::GetBeltSlotCount(); ++i) {
        if (GameState::GetBeltItemAt(i) == 0) return true;
        if (GameState::GetItemAmount(GameState::GetBeltItemAt(i), false) == 0) return true;
    }
    return false;
}

// @0x27b134 CharacterInfoWindow::CanTakeOffItem: everything but slot 8.
bool CanTakeOffItem(const GameState::PlayerItem* it) { return it && it->info->slot != 8; }
}  // namespace

// @0x3845b0
const char32_t* GetProbabilityString(unsigned p) {
    if (p <= 100) return StringTable::GetString("CHANCE_LOW");
    if (p > 3000) {
        if (p > 7000) return StringTable::GetString("CHANCE_VERY_HIGH");
        return StringTable::GetString("CHANCE_HIGH");
    }
    return StringTable::GetString("CHANCE_MEDIUM");
}

// @0x3849b4
void ItemPlaceContinuation() {
    PopupWindow::Hide();
    // UNVERIFIED (milestone 5): CharacterInfoWindow::Hide (the window is not ported).
    // UNVERIFIED (milestone 4c): Tasks::HelpWithMapSwitchTo(0, true, true, 0) travels home; the city
    // is the only map the port loads so far.
}

// ---- construction and layout ----

ItemHoverWindow::ItemHoverWindow() { name = "ItemHoverWindow"; }   // @0x38a7e8

ItemHoverWindow::~ItemHoverWindow() {   // @0x38a664
    for (GUI::Window* w : windows) delete w;
    windows.clear();
}

// @0x38a52c
void ItemHoverWindow::ItemButton::Create() {
    float s = LayoutScale();
    root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Button_dismiss.xml", "Button_dismiss.png",
                           GUI::GetHoverScaleFactor(s, s), 0, 0, 0, 0, false, 1.f);
    root->SetVisibility(false);
    blue = GUI::GetWindowTypedF<GUI::Window>(root, "button_belt_blue");
    green = GUI::GetWindowTypedF<GUI::Window>(root, "button_belt_green");
    icon = GUI::GetWindowTypedF<GUI::Window>(root, "icon_placeholder_35");
    icon->takesZ = true;
    text = GUI::GetWindowTypedF<GUI::Textfield>(root, "text");
    click = GUI::GetWindowTypedF<GUI::Button>(root, "clickArea");
}

// @0x38a9b8
void ItemHoverWindow::Init() {
    const char* dir = "../resource/kingdom_ui/1Original/";
    auto path = [dir](const char* f) { return std::string(dir) + f; };
    float s = LayoutScale();
    float hs = GUI::GetHoverScaleFactor(s, s);

    frame = Register(windows, path("Mini_hint.xml").c_str(), "Mini_hint.png", 1.f);
    frame->SetVisibility(false);
    border = GUI::GetWindowTypedF<GUI::Window>(frame, "golden_border_box");
    static const char* const kArrows[8] = {"dialog_arrow_s", "dialog_arrow_se", "dialog_arrow_e",
                                           "dialog_arrow_ne", "dialog_arrow_n", "dialog_arrow_nw",
                                           "dialog_arrow_w", "dialog_arrow_sw"};
    for (int i = 0; i < 8; ++i) arrows[i] = GUI::GetWindowTypedF<GUI::Window>(frame, kArrows[i]);
    borderW = border->w;
    borderH = border->h;

    nameWin = Register(windows, path("Hint_info_item_name.xml").c_str(), "Hint_info_item_name.png", hs);
    nameWin->SetVisibility(false);
    for (int i = 0; i < 5; ++i) classText[i] = GUI::GetWindowTypedF<GUI::Textfield>(nameWin, "text_item_class_%02d", i + 1);

    separator = Register(windows, path("Window_hints_sepparator_small.xml").c_str(),
                         "Window_hints_sepparator_small.png", hs);
    separator->SetVisibility(false);
    for (EffectLine& l : lines) {
        l.root = Register(windows, path("Hint_info_effect_1_line.xml").c_str(), "Hint_info_effect_1_line.png", hs);
        l.root->SetVisibility(false);
        l.icon = GUI::GetWindowTypedF<GUI::Window>(l.root, "icon");
        l.icon->takesZ = true;
        l.text = GUI::GetWindowTypedF<GUI::Textfield>(l.root, "text_desc");
    }

    descWin = Register(windows, path("Unit_info_hint_6_lines_small.xml").c_str(), "Unit_info_hint_6_lines_small.png", hs);
    descWin->SetVisibility(false);
    descText = GUI::GetWindowTyped<GUI::Textfield>(descWin, "description_text");

    durSeparator = Register(windows, path("Window_hints_sepparator_small.xml").c_str(),
                            "Window_hints_sepparator_small.png", hs);
    durSeparator->SetVisibility(false);

    durWin = Register(windows, path("Hint_info_item_durability_small.xml").c_str(),
                      "Hint_info_item_durability_small.png", hs);
    durWin->SetVisibility(false);
    barDarkRed = GUI::GetWindowTyped<GUI::Window>(durWin, "durability_bar_dark_red");
    textDarkRed = GUI::GetWindowTyped<GUI::Textfield>(durWin, "text_bar_dark_red");
    barRed = GUI::GetWindowTyped<GUI::Window>(durWin, "durability_bar_red");
    textRed = GUI::GetWindowTyped<GUI::Textfield>(durWin, "text_bar_red");
    clipRed = {barRed->x, 0, barRed->x + barRed->w, 0x400};
    barRed->SetClipRect(&clipRed);
    textRed->SetClipRect(&clipRed);
    barGreen = GUI::GetWindowTyped<GUI::Window>(durWin, "durability_bar_green");
    textGreen = GUI::GetWindowTyped<GUI::Textfield>(durWin, "text_bar_green");
    clipGreen = {barRed->x, 0, barRed->x + barRed->w, 0x400};   // (the red bar's extent, as the original)
    barGreen->SetClipRect(&clipGreen);
    textGreen->SetClipRect(&clipGreen);

    infoLine = Register(windows, path("Unit_info_info_line.xml").c_str(), "Unit_info_info_line.png", hs);
    infoLine->SetVisibility(false);
    infoIcon = GUI::GetWindowTypedF<GUI::Window>(infoLine, "unit_info_line_button.icon_place_holder");
    infoIcon->takesZ = true;
    infoTextRes = GUI::GetWindowTypedF<GUI::Textfield>(infoLine, "unit_info_line_button.text_res");
    infoText = GUI::GetWindowTypedF<GUI::Textfield>(infoLine, "unit_info_line_button.text");

    luckLine = Register(windows, path("Hint_info_icon_text.xml").c_str(), "Hint_info_icon_text.png", hs);
    luckLine->SetVisibility(false);
    luckIcon = GUI::GetWindowTypedF<GUI::Window>(luckLine, "icon_35");
    luckIcon->takesZ = true;
    luckText = GUI::GetWindowTypedF<GUI::Textfield>(luckLine, "text_item_from_02");

    for (ItemButton& b : buttons) {
        b.Create();
        buttonRoots.push_back(b.root);
    }
    for (GUI::Window* r : buttonRoots) windows.push_back(r);

    // The handlers (FUN_00383fa0 makes the member-pointer callbacks to the vtable slots +0x6c..+0xb4).
    buttons[kDrink].click->SetOnClick([this] { OnDrink(); });
    buttons[kEnable].click->SetOnClick([this] { OnEnable(); });
    buttons[kDisable].click->SetOnClick([this] { OnEnable(); });
    buttons[kEquip].click->SetOnClick([this] { OnEquip(); });
    buttons[kUnequip].click->SetOnClick([this] { OnUnequip(); });
    durWin->SetOnClick([this] { OnRepair(); });
    buttons[kRepair].click->SetOnClick([this] { OnRepair(); });
    buttons[kRemove].click->SetOnClick([this] { OnRemove(); });
    buttons[kImprove].click->SetOnClick([this] { OnImprove(); });
    buttons[kFind].click->SetOnClick([this] { OnFind(); });
    buttons[kShop].click->SetOnClick([this] { OnShop(); });
    buttons[kReputation].click->SetOnClick([this] { OnReputation(); });
    buttons[kOpen].click->SetOnClick([this] { OnOpen(); });
    buttons[kAskFriends].click->SetOnClick([this] { OnAskFriends(); });
    buttons[kTrain].click->SetOnClick([this] { OnTrain(); });
    buttons[kHeal].click->SetOnClick([this] { OnHeal(); });
    buttons[kMove].click->SetOnClick([this] { OnMove(); });
    buttons[kDismiss].click->SetOnClick([this] { OnDismiss(); });
    buttons[kFeed].click->SetOnClick([this] { OnFeed(); });
    buttons[kPlace].click->SetOnClick([this] { OnPlace(); });
    arrowShift = 0;
}

// @0x383478
void ItemHoverWindow::SetZ(float zz) {
    z = zz;
    for (GUI::Window* w : windows) w->SetZ(zz - 0.0001f);
    frame->SetZ(zz + 0.001f);
}

// @0x38a4c0
void ItemHoverWindow::Show() {
    if (shownHover) return;
    BuildingHoverWindow::Show();
    frame->SetVisibility(true);
    nameWin->SetVisibility(true);
    separator->SetVisibility(true);
    helpingImprove = false;
    probability = 0;
}

// @0x38a464
void ItemHoverWindow::Hide() {
    if (!shownHover) return;
    for (GUI::Window* w : windows) w->SetVisibility(false);
    BuildingHoverWindow::Hide();
}

// @0x38a308
bool ItemHoverWindow::Click(int x, int y, bool pressed) {
    if (!shownHover) return false;
    if (BuildingHovers::ClickOnArrow(x, y, pressed)) return true;
    for (GUI::Window* r : buttonRoots)
        if (r->Click(x, y, pressed, true)) return true;
    // The tutorials keep the popup open while they point at its buttons.
    int st = GameState::secondTutorial;
    if (st == 0x88 || st == 0x8c) return true;
    if (st == 0xa4) {
        if (GameState::tutorial != 0x37) return true;
    } else if ((unsigned)(GameState::tutorial - 0x14) <= 1) {
        return true;
    }
    if (GameState::IsTameTutorial() && buttons[kFeed].root->visibleSelf && buttons[kFeed].root->enabled)
        return true;
    if (GameState::secondTutorial == 0x96) return true;
    Hide();
    return false;
}

// @0x383514
void ItemHoverWindow::SetTargetRect(int left, int top, int right, int bottom, Direction dir) {
    direction = dir;
    target[0] = left; target[1] = top; target[2] = right; target[3] = bottom;
    screen[0] = 0; screen[1] = 0; screen[2] = GUI::ScreenWidth(); screen[3] = GUI::ScreenHeight();
}

// @0x383570 (read from the asm: the decompile drops most of the switch). The frame is placed with
// the chosen arrow at the target; a side arrow slides along the frame (arrowShift) when the frame
// is pushed back on screen. When the frame does not fit on the chosen side, the next side that
// fits (in arrow order) is tried.
void ItemHoverWindow::RecomputePosition() {
    for (GUI::Window* a : arrows) a->MoveWindow(0, -arrowShift);
    const int L = target[0], T = target[1], R = target[2], B = target[3];
    const int W = GUI::ScreenWidth(), H = GUI::ScreenHeight();
    int bw = border->w, bh = border->h;
    int dw = bw - borderW, dh = bh - borderH;   // how much the border grew past its layout size
    int fw = frame->w, fh = frame->h;
    arrowShift = 0;
    int x = 0, y = 0;
    switch (direction) {
    case kS:
        x = (L + R) / 2 + 4 - (fw - borderW) / 2 - bw / 2;
        y = T + 8 - fh - dh;
        break;
    case kSE:
        x = L + 0x18 - fw - dw;
        y = T + 0x18 - fh - dh;
        break;
    case kE:
    case kW: {
        x = direction == kE ? L + 8 - fw - dw : R - 8;
        y = -((fh - borderH) / 2) - bh / 2 + (B + T) / 2;
        if (bh / 2 + B >= H) {   // pushed up from the bottom edge
            arrowShift = bh / 2 + B - H;
            y += (H - B) - bh / 2;
        }
        int over = T - bh / 2;
        if (over <= 0) {          // pushed down from the top edge
            arrowShift = over;
            y += bh / 2 - T;
        }
        break;
    }
    case kNE:
        x = L + 0x18 - fw - dw;
        y = B - 0x18;
        break;
    case kN:
        x = (L + R) / 2 + 4 - (fw - borderW) / 2 - bw / 2;
        y = B - 8;
        break;
    case kNW:
        x = R - 0x18;
        y = B - 0x18;
        break;
    case kSW:
        x = R - 0x18;
        y = T + 0x18 - fh - dh;
        break;
    }
    for (GUI::Window* a : arrows) a->MoveWindow(0, arrowShift);
    for (int i = 0; i < 8; ++i) arrows[i]->SetVisibility(direction == i);

    // Which sides fit on the screen.
    bh = border->h;
    bw = border->w;
    int rs = (int)(frame->scale * 64.f);   // a side arrow's reach
    int above = T - bh, left = L - bw;
    bool fits[8] = {};
    fits[kS] = above > 0x20 && (L + R) / 2 - bw / 2 >= -9;
    fits[kSE] = above > 0xe && left > 0xe;
    fits[kE] = left > 0x20 && T - rs > 0 && rs + B < H;
    fits[kNE] = B + bh + 0xe < H && left > 0xe;
    fits[kN] = B + bh + 0x20 < H && (L + R) / 2 - bw / 2 >= -9;
    fits[kNW] = B + bh + 0xe < H && R + bw + 0xe < W;
    fits[kW] = R + bw + 0x20 < W && T - rs > 0 && B + rs < H;
    fits[kSW] = above > 0xe && R + bw + 0xe < W;
    if (fits[kNE] && fits[kNW] && direction == kW) fits[kNE] = false;
    bool any = false;
    for (bool f : fits) any |= f;
    if (!any) {
        if (R + bw + 0x20 < W) { fits[kW] = true; any = true; }
        else if (left > 0x20) { fits[kE] = true; any = true; }
    }
    if (fits[direction] || !any) {
        SetPosition(x, y);
        return;
    }
    int d = direction;
    do d = (d + 1) % 8; while (!fits[d]);
    direction = d;
    RecomputePosition();
}

// @0x383c24: the frame grows around the visible parts; the arrows go to its corners and edges.
void ItemHoverWindow::FinalizeSetup() {
    int h = separator->h + nameWin->h;
    for (EffectLine& l : lines) {
        if (!l.root->visibleSelf) continue;
        if (!l.text->sprite) {
            h += l.root->h;
            continue;
        }
        l.root->SetBorders(3, 3, 3, 3);
        l.root->SetSize(l.root->w, (int)std::fabs(l.text->sprite->h) - 6);
        float th = std::fabs(l.text->sprite->h);
        if (th > (float)l.root->h) h += (int)th - 0xc;   // a text taller than its line
        else h += l.root->h;
    }
    if (descWin->visibleSelf) h += descWin->h;
    if (durSeparator->visibleSelf) h += durSeparator->h + 3;
    if (durWin->visibleSelf) h += durWin->h;
    if (infoLine->visibleSelf) h += infoLine->h;
    if (luckLine->visibleSelf) h += luckLine->h;
    for (GUI::Window* r : buttonRoots)
        if (r->visibleSelf) h += r->h;
    for (GUI::Window* a : arrows) a->RestorePosition();
    border->SetSize(padRight + padLeft + nameWin->w, h + padBottom);
    int ax = border->w - borderW, ay = border->h - borderH;
    arrows[kS]->MoveWindow(ax / 2, ay);
    arrows[kSE]->MoveWindow(ax, ay);
    arrows[kE]->MoveWindow(ax, ay / 2);
    arrows[kNE]->MoveWindow(ax, 0);
    arrows[kN]->MoveWindow(ax / 2, 0);
    arrows[kNW]->MoveWindow(0, 0);
    arrows[kW]->MoveWindow(0, ay / 2);
    arrows[kSW]->MoveWindow(0, ay);
    arrowShift = 0;
}

// @0x389bdc: the parts stack downwards from the name; the buttons sit 4 px further left.
void ItemHoverWindow::SetPosition(int x, int y) {
    BuildingHoverWindow::SetPosition(x, y);
    frame->SetPosition(x, y);
    int cx = x + padLeft;
    nameWin->SetPosition(cx, y + padTop);
    int cy = y + padTop + nameWin->h;
    separator->SetPosition(cx, cy);
    cy += separator->h;
    for (EffectLine& l : lines) {
        if (!l.root->visibleSelf) break;
        l.root->SetPosition(cx, cy);
        if (!l.text->sprite) {
            cy += l.root->h;
            continue;
        }
        float th = std::fabs(l.text->sprite->h);
        int lh = l.root->h;
        if (th > (float)lh) cy += (int)th - 0xc;
        else cy += lh;
    }
    if (descWin->visibleSelf) { descWin->SetPosition(cx, cy); cy += descWin->h; }
    if (durSeparator->visibleSelf) { durSeparator->SetPosition(cx, cy + 3); cy += durSeparator->h + 3; }
    if (durWin->visibleSelf) { durWin->SetPosition(cx, cy); cy += durWin->h; }
    if (infoLine->visibleSelf) { infoLine->SetPosition(cx, cy); cy += infoLine->h; }
    if (luckLine->visibleSelf) { luckLine->SetPosition(cx, cy); cy += luckLine->h; }
    static const int kOrder[] = {kDrink, kEnable, kDisable, kRepair, kRemove, kImprove, kEquip, kUnequip,
                                 kFind, kShop, kReputation, kOpen, kAskFriends, kTrain, kHeal, kMove,
                                 kDismiss, kFeed, kPlace};
    for (int k : kOrder) {
        GUI::Window* r = buttons[k].root;
        if (k != kPlace && !r->visibleSelf) continue;
        // (the Place button is positioned even when hidden: the original's last call is unconditional)
        r->SetPosition(cx - 4, cy);
        if (k != kPlace) cy += r->h;
    }
}

// ---- contents ----

// @0x386b24
void ItemHoverWindow::SetResource(int type, bool allowFind) {
    inventory = false;
    item = nullptr;
    info = nullptr;
    resource = type;
    unit = nullptr;
    unitData = nullptr;
    healMode = false;
    for (unsigned k = 0; k < 5; ++k) classText[k]->SetVisibility(k == 0);
    classText[0]->SetText(GameState::GetResourceGameName(type));
    if (!GameState::IsCityTutorial() && allowFind) {
        ItemButton& b = buttons[kFind];
        b.root->SetVisibility(true);
        b.green->SetVisibility(false);
        b.icon->SetTexture(IconManager::GetIcon("gold_find"), true);
        b.text->SetText(StringTable::GetString("PERFORM_FIND"));
    }
    if (probability != 0) {
        luckLine->SetVisibility(true);
        luckIcon->SetTexture(IconManager::GetIcon("gold_luck"), false);
        luckText->SetText(GetProbabilityString(probability));
    }
    FinalizeSetup();
}

// @0x385340
void ItemHoverWindow::SetText(const char32_t* title, const char32_t* text) {
    inventory = false;
    item = nullptr;
    info = nullptr;
    resource = 0xb;
    unit = nullptr;
    unitData = nullptr;
    healMode = false;
    for (unsigned k = 0; k < 5; ++k) classText[k]->SetVisibility(k == 0);
    classText[0]->SetText(title);
    if (text) descText->SetText(text);
    descWin->SetVisibility(text != nullptr);
    if (probability != 0) {
        luckLine->SetVisibility(true);
        luckIcon->SetTexture(IconManager::GetIcon("gold_luck"), false);
        luckText->SetText(GetProbabilityString(probability));
    }
    FinalizeSetup();
}

// @0x386d14 (the inventory part's flags read from the asm where the decompile lost them)
void ItemHoverWindow::SetItem(const char32_t* /*unused*/, GameState::PlayerItem* it, Items::ItemInfo* inf,
                              bool inv, bool allowFind, const char32_t* sourceText, bool viewOnly,
                              bool noDescription, bool heal, bool noReputation) {
    if (!inf) return;
    inventory = inv;
    unitData = nullptr;
    resource = 0xb;
    item = it;
    info = inf;
    unit = nullptr;
    healMode = heal;
    if (GameState::IsArenaCombatActive() || GameState::IsBossCombatActive()) allowFind = false;
    for (unsigned k = 0; k < 5; ++k) classText[k]->SetVisibility(inf->itemLevel == k);
    classText[inf->itemLevel]->SetText(inf->title);

    unsigned n = 0;   // the next effect line
    // UNVERIFIED (milestone 5): the set line (Sets::GetSetForItem, also through
    // Collections::GetCollectionStart: the set's name, no icon) and the profession line
    // (Professions::GetProfession / GetProfessionLevel of reputation_id / reputation_level: the
    // level's name with the profession's icon). Neither system is ported, so neither line shows.
    bool hasProfession = false;   // Professions::GetProfession(inf->reputationId) != null

    // The item's type, with the building that makes it (a pack: its first item).
    uint32_t madeId = inf->id;
    if (inf->packId && inf->packItems && inf->packItems->GetChild(0))
        madeId = inf->packItems->GetChild(0)->GetChild(0)->GetInt();
    unsigned slot = 0;
    uint32_t producer = NeedItemWindow::GetBuildingThatProducesItem(madeId, slot);
    if (Render::Texture* tex = IconManager::GetIcon(inf->GetItemTypeIcon(producer))) {
        EffectLine& l = lines[n++];
        l.root->SetVisibility(true);
        l.text->SetText(inf->GetItemTypeName(producer));
        l.icon->SetVisibility(true);
        l.icon->SetTexture(tex, tex->w < 0x19);
    }
    if (GameState::GetLevel() < inf->level) {
        std::u32string s = SWPrintf(0x100, StringTable::GetString("SHOP_UNLOCK"), {ToWideString((int)inf->level)});
        SetLine(lines[n++], s.c_str(), "gold_16_lock");
    }
    if (producer) {
        if (const GameData::BuildingData* bd = GameData::GetBuilding(producer))
            SetLine(lines[n++], StringTable::GetString(bd->name.c_str()), "gold_16_house");
    }

    bool luck = false;
    if (inf->meta) {
        luck = inf->meta->FindChildData(kExpLuck) != nullptr;
        MetaExpression* x = inf->meta;
        if (!luck) {
            std::u32string desc;   // the original's one buffer for every line (see GenerateDescription)
            do {
                if (x->IsValidItemEffect(inf)) {
                    x->GenerateDescription(desc, 0x80, EntityManager::GetPlayer());
                    EffectLine& l = lines[n++];
                    l.root->SetVisibility(true);
                    l.text->SetAlignment(0, 0);
                    l.text->SetText(desc.c_str());
                    l.icon->SetVisibility(true);
                    l.icon->SetTexture(IconManager::GetIcon(x->GetBonusIcon(inf)), true);
                }
                x = x->GetNext();
            } while (x && n < 8);
        }
        // A GIVE_TROOPS item lists the unit's stats.
        const MetaData* troops = luck ? nullptr : inf->meta->FindChildData(kExpGiveTroops);
        if (troops) {
            if (EntityData* d = EntityFactory::GetEntityByID(troops->GetInt())) {
                static const char* const kKeys[6] = {"ENTITY_INFO_HEALTH", "ENTITY_INFO_ATTACK_MELEE",
                                                     "ENTITY_INFO_ATTACK_RANGE", "ENTITY_INFO_ATTACK_RANGE",
                                                     "ENTITY_INFO_DEFENCE_2", "ENTITY_INFO_DEFENCE"};
                const char* icons[6] = {"gold_16_health", "gold_16_attack", "gold_16_ranged_attack",
                                        d->GetSmallSpellIcon(), "gold_16_defence_melee", "gold_16_defence"};
                int values[6] = {d->hp, d->attackMelee, d->attackRanged, d->attackMagic, d->defenseMelee,
                                 d->defenseRanged};
                for (int k = 0; k < 6; ++k) {
                    if (values[k] <= 0) continue;
                    if (n >= 8) break;   // PORT: the original does not stop at the eighth line
                    SetLine(lines[n++], ToWideString(StringTable::GetString(kKeys[k]), {ToWideString(values[k])}),
                            icons[k]);
                }
            }
        }
    }

    // The description.
    if (inf->type == 0x81 || inf->type == 0x82) {   // a building or decoration item
        char key[0x80];
        bool exists;
        const GameData::DecorData* dd = inf->type == 0x82 ? GameData::GetDecoration(inf->id - 0x8000) : nullptr;
        if (dd) {
            std::snprintf(key, sizeof key, "%s_DESC", dd->name.c_str());
            exists = StringTable::StringExists(key);
            if (exists) {
                std::u32string t = StringTable::GetTimeString(dd->collectTime, true);
                std::u32string s = SWPrintf(0x100, StringTable::GetString(key), {t.c_str()});
                descText->SetText(s.c_str());
            }
        } else {
            std::snprintf(key, sizeof key, "%s_DESC_B", inf->name);
            exists = StringTable::StringExists(key);
            if (exists) descText->SetText(StringTable::GetString(key));
        }
        descWin->SetVisibility(exists);
    } else if (!noDescription) {
        char key[0x80];
        std::snprintf(key, sizeof key, "%s_DESC", inf->name);
        const char32_t* desc = nullptr;
        if (StringTable::StringNotEmpty(key)) {
            desc = StringTable::GetStringF("%s_DESC", inf->name);
            // UNVERIFIED (milestone 5): a chest item's description takes the chest's crystal price
            // (Chests::GetChestForItem / GetChestInfo +0x20); no chests yet.
            (void)IsChestItem;
        }
        const MetaData* luckData = inf->meta ? inf->meta->FindChildData(kExpLuck) : nullptr;
        if (luckData) {
            std::u32string s = SWPrintf(0x100, desc, {ToWideString(luckData->GetInt())});
            descText->SetText(s.c_str());
            descWin->SetVisibility(true);
        } else {
            descText->SetText(desc);
            descWin->SetVisibility(!inf->meta && desc && *desc);
        }
    }

    // Durability.
    bool hasDurability = it && inf->durability != 0;
    durSeparator->SetVisibility(it && inf->durability != 0);
    durWin->SetVisibility(hasDurability);
    bool broken = false;
    if (it) {
        std::u32string s = SWPrintf(0x100, StringTable::GetString("REPAIR_DURABILITY"),
                                    {ToWideString((int)it->f0c), ToWideString((int)inf->durability)});
        textDarkRed->SetText(s.c_str());
        textRed->SetText(s.c_str());
        textGreen->SetText(s.c_str());
        // (a durability of 0 divides by zero on the original too; the parts are hidden then)
        if (inf->durability) {
            clipRed.right = (int)((float)clipRed.left + (float)barRed->w * (float)it->f10 / (float)inf->durability);
            clipGreen.right = (int)((float)clipGreen.left + (float)barGreen->w * (float)it->f0c / (float)inf->durability);
        }
    }

    if (inv && it && !viewOnly) {
        broken = it->IsBroken();
        uint32_t durabilityMax = inf->durability, durabilityLeft = it->f0c;
        // UNVERIFIED (milestone 5): crafting. The source collection (Collections::
        // GetCollectionWithSource: Improve), the crafting collection (GetCraftingCollectionForItem:
        // Craft) and, for a profession of type 0, GetCollectionWithResult; without the
        // "craft_unlock_task" task and a source collection the other two are dropped; type 7 items
        // are never crafted. No collections are ported, so all three are null.
        bool source = false, craftable = false, result = false;
        if (!source && !GameState::TaskCompleted(Setting("craft_unlock_task").GetInt())) { result = false; craftable = false; }
        if (it->info->type == 7) { result = false; craftable = false; }

        int beltSlot = -1;
        bool activated = false;
        for (unsigned i = 0; i < GameState::GetBeltSlotCount() && beltSlot == -1; ++i) {
            if (GameState::GetBeltItemAt(i) == it->id) {
                beltSlot = (int)i;
                activated = GameState::GetBeltItemActivationTimeAt(i) != 0;
            }
        }
        Entity* player = EntityManager::GetPlayer();
        unsigned t = it->info->type;
        bool orb = t == 3 && it->info->CanBeActivated();

        // Drink / use: potions (type 3 that are not orbs), ropes (5), buffs (0xe).
        ItemButton& drink = buttons[kDrink];
        drink.root->SetVisibility((t == 3 && !orb) || t == 5 || t == 0xe);
        drink.root->SetEnabled(!it->info->IsLockedByLevel() && (!inf->meta || inf->meta->CanApplyItemEffect(player)));
        drink.blue->SetVisibility(false);
        drink.icon->SetTexture(IconManager::GetIcon(t == 3 ? "gold_potion" : "gold_helmet"), true);
        drink.text->SetText(StringTable::GetString(
            t == 3 && it->id != 0x250 && it->id != 0x211 ? "INVENTORY_DRINK" : "INVENTORY_USE"));

        // Turn on / off: an orb on the belt.
        ItemButton& on = buttons[kEnable];
        on.root->SetVisibility(orb && beltSlot != -1 && !activated && !heal);
        if (it->info->id == 0x15d) on.root->SetVisibility(true);   // the magnifying glass
        on.blue->SetVisibility(false);
        on.icon->SetTexture(IconManager::GetIcon("gold_confirm"), true);
        on.text->SetText(StringTable::GetString("INVENTORY_ENABLE"));
        ItemButton& off = buttons[kDisable];
        off.root->SetVisibility(orb && beltSlot != -1 && activated && !heal);
        off.green->SetVisibility(false);
        off.icon->SetTexture(IconManager::GetIcon("gold_cancel"), true);
        off.text->SetText(StringTable::GetString("INVENTORY_DISABLE"));

        // Equip / to the belt.
        bool equipped = IsItemEquipped(it);
        ItemButton& eq = buttons[kEquip];
        eq.root->SetVisibility(!equipped && (t == 3 || t == 8) && !it->IsBroken() && !heal);
        eq.root->SetEnabled(CanTakeOnItem(it));
        eq.blue->SetVisibility(false);
        eq.icon->SetTexture(IconManager::GetIcon(t == 3 ? "gold_to_belt" : "gold_equip_item"), true);
        eq.text->SetText(StringTable::GetString(t == 3 ? "INVENTORY_TO_BELT" : "INVENTORY_EQUIP"));
        ItemButton& uneq = buttons[kUnequip];
        uneq.root->SetVisibility(equipped && (t == 3 || t == 8) && !heal);
        uneq.root->SetEnabled(CanTakeOffItem(it) && !GameState::IsArenaCombatActive());
        uneq.green->SetVisibility(false);
        uneq.icon->SetTexture(IconManager::GetIcon(t == 3 ? "gold_from_belt" : "gold_unequip_item"), true);
        uneq.text->SetText(StringTable::GetString(t == 3 ? "INVENTORY_FROM_BELT" : "INVENTORY_UNEQUIP"));

        // Repair: some durability used (a profession tool only with a crystal price).
        ItemButton& rep = buttons[kRepair];
        bool repairable = durabilityMax != 0 && (int)durabilityLeft < (int)durabilityMax;
        if (repairable && it->info->IsProfessionTool() && it->info->cost2 == 0) repairable = false;
        rep.root->SetVisibility(repairable);
        rep.green->SetVisibility(false);
        rep.icon->SetTexture(IconManager::GetIcon("gold_repair"), true);
        rep.text->SetText(StringTable::GetString("INVENTORY_REPAIR"));

        // Remove: only a broken item.
        ItemButton& rem = buttons[kRemove];
        rem.root->SetVisibility(broken);
        rem.green->SetVisibility(false);
        rem.icon->SetTexture(IconManager::GetIcon("red_x"), true);
        rem.text->SetText(StringTable::GetString("INVENTORY_REMOVE"));

        // Improve (a source collection) / Craft.
        ItemButton& imp = buttons[kImprove];
        if (craftable || source || result) {
            int tut = GameState::tutorial;
            imp.root->SetVisibility(tut != 0x14 && tut != 0x15 && GameState::secondTutorial != 0xa4);
        } else {
            imp.root->SetVisibility(false);
        }
        imp.green->SetVisibility(false);
        imp.icon->SetTexture(IconManager::GetIcon(source ? "gold_upgrade" : "gold_repair"), true);
        imp.text->SetText(StringTable::GetString(source ? "INVENTORY_IMPROVE" : "PERFORM_CRAFT_ITEM"));
    }

    if (!viewOnly) {
        if (!noDescription && GameState::tutorial != 0x61) {
            // Shop: potions, type 7, broken profession tools or items not owned, when the item shop
            // sells them (or a pack with them).
            ItemButton& shop = buttons[kShop];
            unsigned t = inf->type;
            bool kind = t == 3 || t == 7 || (inf->IsProfessionTool() && broken) || !inv;
            shop.root->SetVisibility(kind && (ItemCanBeBought(inf->id) || Items::GetItemPackWithItem(inf->id)));
            shop.root->SetEnabled(!GameState::IsArenaCombatActive() &&
                                  GameState::TaskCompleted(Setting("shop_unlock_task").GetInt()));
            shop.blue->SetVisibility(false);
            shop.icon->SetTexture(IconManager::GetIcon("profession_hauler"), true);
            shop.text->SetText(StringTable::GetString("PERFORM_BUY"));
        }
        if (!noReputation && hasProfession && !LevelInfoWindowIsVisible()) {
            // UNVERIFIED (milestone 5): the Valor button for a profession of type 1 (its icon
            // +0x1c and LEVEL_INFO_HEADER_REP); no professions yet.
        }
    }

    // Find: a mission or crafting map has it, or a building makes it.
    bool onMap = (GetMissionMapForItem(inf->id) || GetMapForCraftItem(inf->id)) && !RaidInfoWindowIsVisible();
    if (!viewOnly && ((slot != 0xffffffff && allowFind) || onMap) && !GameState::IsCityTutorial() &&
        GameState::GetCurrentMapID() != 0x70) {
        ItemButton& f = buttons[kFind];
        f.root->SetVisibility(true);
        f.green->SetVisibility(false);
        f.icon->SetTexture(IconManager::GetIcon("gold_find"), true);
        f.text->SetText(StringTable::GetString("PERFORM_FIND"));
    }

    // The luck line: where it is found, or a drop chance.
    if (sourceText || probability != 0) {
        luckLine->SetVisibility(true);
        luckIcon->SetTexture(IconManager::GetIcon("gold_luck"), false);
        if (probability != 0) luckText->SetText(GetProbabilityString(probability));
        luckText->SetText(sourceText);
    }
    if ((inf->type == 0x81 || inf->type == 0x82) && inv && !viewOnly) {
        // Place a building or decoration item (in the player's city).
        ItemButton& p = buttons[kPlace];
        p.root->SetVisibility(true);
        p.root->SetEnabled(GameState::IsPlayerCity());
        p.green->SetVisibility(false);
        p.icon->SetTexture(IconManager::GetIcon("gold_build"), true);
        p.text->SetText(StringTable::GetString("PERFORM_PLACE"));
    }
    if (inv && IsChestItem(inf->id)) {
        ItemButton& o = buttons[kOpen];
        o.root->SetVisibility(true);
        o.green->SetEnabled(!GameState::IsCombatActive());
        o.icon->SetEnabled(!GameState::IsCombatActive());
        o.blue->SetVisibility(false);
        o.icon->SetTexture(IconManager::GetIcon("gold_place"), false);
        o.text->SetText(StringTable::GetString("PERFORM_OPEN"));
    }
    if (inf->id == 0x211 && IsTaskActive(0x6b4) && !ChestsWindowIsVisible() && !noDescription) {
        ItemButton& o = buttons[kOpen];
        o.root->SetVisibility(true);
        o.green->SetEnabled(true);
        o.icon->SetEnabled(true);
        o.blue->SetVisibility(false);
        o.icon->SetTexture(IconManager::GetIcon("gold_find"), false);
        o.text->SetText(StringTable::GetString("PERFORM_FIND"));
    }

    // Ask friends for it (items_to_ask, after task_friends_enable; online: dormant, see OnAskFriends).
    bool askable = false;
    Setting ask("items_to_ask");
    for (unsigned i = 0; i < ask.GetChildrenCount(); ++i)
        if ((uint32_t)ask.GetChild(i).GetInt() == inf->id) askable = true;
    bool friends = GameState::TaskCompleted(Setting("task_friends_enable").GetInt());
    if (askable && !GameState::IsCombatActive() && friends) {
        ItemButton& a = buttons[kAskFriends];
        a.root->SetVisibility(true);
        a.root->SetEnabled(inf->giftLevel <= GameState::GetLevel());
        a.blue->SetVisibility(false);
        a.icon->SetTexture(IconManager::GetIcon("gift_35"), false);
        a.text->SetText(StringTable::GetString("WEBVIEW_SEND_GIFT_BUTTON"));
    }
    FinalizeSetup();
}

// @0x385c80
void ItemHoverWindow::SetEntity(Entity* e, EntityData* data, bool train, bool manage) {
    inventory = false;
    resource = 0xb;
    item = nullptr;
    info = nullptr;
    unit = e;
    unitData = data;
    healMode = false;
    for (unsigned k = 0; k < 5; ++k) classText[k]->SetVisibility(k == 0);
    classText[0]->SetText(unitData->id == 0x1a2 ? StringTable::GetString("MARKER_SOLDIER") : unitData->title);
    static const char* const kKeys[6] = {"ENTITY_INFO_HEALTH", "ENTITY_INFO_ATTACK_MELEE",
                                         "ENTITY_INFO_ATTACK_RANGE", "ENTITY_INFO_ATTACK_RANGE",
                                         "ENTITY_INFO_DEFENCE_2", "ENTITY_INFO_DEFENCE"};
    int values[6];
    if (!e) {
        values[0] = data->hp;
        values[1] = unitData->attackMelee;
        values[2] = unitData->attackRanged;
        values[3] = unitData->attackMagic;
        values[4] = unitData->defenseMelee;
        values[5] = unitData->defenseRanged;
    } else {
        values[0] = e->GetHpMax();
        values[1] = e->HasMeleeDamage() ? e->GetAttackMelee() : 0;
        values[2] = e->GetAttackRanged();
        values[3] = e->GetAttackMagic();
        values[4] = e->GetDefenseMelee();
        values[5] = e->GetDefenseRanged();
    }
    const char* icons[6] = {"gold_16_health", "gold_16_attack", "gold_16_ranged_attack",
                            unitData->GetSmallSpellIcon(), "gold_16_defence_melee", "gold_16_defence"};
    unsigned n = 0;
    for (int k = 0; k < 6; ++k) {
        if (values[k] <= 0) continue;
        EffectLine& l = lines[n++];
        l.root->SetVisibility(true);
        std::u32string s;
        if (k == 0 && e)   // a unit's health as "now/max"
            s = SWPrintf(0x100, StringTable::GetString(kKeys[0]), {ToWideString(U"%d/%d", {e->GetHP(), values[0]})});
        else
            s = SWPrintf(0x100, StringTable::GetString(kKeys[k]), {ToWideString(values[k])});
        l.text->SetText(s.c_str());
        l.icon->SetVisibility(true);
        l.icon->SetTexture(IconManager::GetIcon(icons[k]), true);
    }
    if (train && !GameState::IsCityTutorial()) {
        ItemButton& b = buttons[kTrain];
        b.root->SetVisibility(true);
        b.green->SetVisibility(false);
        b.icon->SetTexture(IconManager::GetIcon("gold_attack"), true);
        b.text->SetText(StringTable::GetString("PERFORM_TRAIN_MORE"));
    }
    if (e && (train || manage)) {
        if (!GameState::IsCityTutorial() && e->IsEliteSoldier()) {
            ItemButton& b = buttons[kHeal];
            b.root->SetVisibility(true);
            b.root->SetEnabled(e->GetHP() < e->GetHpMax());
            b.blue->SetVisibility(false);
            b.icon->SetTexture(IconManager::GetIcon("gold_potion"), true);
            b.text->SetText(StringTable::GetString("PERFORM_SAVE"));
        }
        if (manage && !GameState::IsCityTutorial()) {
            bool inSquad = SoldierSlots::HasSoldier(e);
            Entity* first = SoldierSlots::GetFirstSoldierByID(data->id);
            ItemButton& b = buttons[kMove];
            b.root->SetVisibility(true);
            b.green->SetVisibility(false);
            b.icon->SetTexture(IconManager::GetIcon("gold_flag_02"), true);
            b.text->SetText(StringTable::GetString(inSquad ? "HIRE_TROOPS_MOVE_UP" : "HIRE_TROOPS_MOVE_DOWN"));
            bool can = false;
            if (!first) {
                // Down into the squad: a free slot and alive.
                if (!inSquad) can = SoldierSlots::GetOccupiedSlotCount() < GameState::GetSoldierSlotCount() && !e->IsDead();
            } else if (inSquad) {
                // Up into the reserve: a free reserve slot, alive (or elite), no combat, the city
                // tutorial over.
                can = SoldierPool::GetOccupiedSlotCount() < GameState::GetSoldierReserveSlots() &&
                      !(e->IsDead() && !e->IsEliteSoldier()) && !GameState::IsCombatActive() &&
                      GameState::secondTutorial == 0x100;
            }
            b.root->SetEnabled(can);
        }
    }
    if (e && manage && !GameState::IsCityTutorial()) {
        bool inSquad = SoldierSlots::HasSoldier(e);
        ItemButton& b = buttons[kDismiss];
        b.root->SetVisibility(true);
        b.green->SetVisibility(false);
        b.icon->SetTexture(IconManager::GetIcon("red_x"), true);
        b.text->SetText(StringTable::GetString("HIRE_TROOPS_DISMISS"));
        b.root->SetEnabled(e->workType != 1 && !inSquad);
    }
    if (probability != 0) {
        luckLine->SetVisibility(true);
        luckIcon->SetTexture(IconManager::GetIcon("gold_luck"), false);
        luckText->SetText(GetProbabilityString(probability));
    }
    FinalizeSetup();
}

// ---- the actions ----

// @0x385a7c
void ItemHoverWindow::OnDrink() {
    if (!item) return;
    if (healMode) {
        HealSquadWindow::UseItem(item->id);
        Hide();
        return;
    }
    BuildingHovers::HideArrow();
    Entity* player = EntityManager::GetPlayer();
    Items::ItemInfo* inf = item->info;
    if (inf->meta && !inf->meta->CanApplyItemEffect(player)) return;
    // UNVERIFIED (milestone 6): SoundsManager::PlaySound("rope_use" for type 5, "ui_task_complete"
    // for 0xe, else "drink_potion", 1, false).
    if (inf->type == 0xe) {
        if (inf->meta && inf->meta->FindChildData(kExpBuffTime))
            GameState::ActivateItemBuff(item->id, inf->meta->FindChildData(kExpBuffTime)->GetInt());
        // UNVERIFIED (milestone 5): AIPlayer::PlayBuffAnimation (vfunc +0x158: (0, 1, 1, 1)) and
        // CharacterInfoWindow::ShowBuffAnimation(this).
    } else if (inf->meta) {
        inf->meta->OnApplyItemEffect(player);
    }
    uint32_t uid = item->uniqueId;
    if (GameState::secondTutorial == 0x96) GameState::secondTutorial = 0x97;
    GameState::RemoveUniqueItem(uid);
    CharacterInfoWindow::UpdateContents();
    Hide();
}

// @0x385768: an orb on the belt turns on (BeltBarWindow::OnActivateSlot); the magnifying glass
// (0x15d) is used up and points at a map object that drops something.
void ItemHoverWindow::OnEnable() {
    if (!item) return;
    if (item->id == 0x15d) {
        Hide();
        if (GameState::GetCurrentMapID() == 0) {
            PopupWindow::Show(StringTable::GetString("MAGNIFYING_GLASS_TOWN"), PopupWindow::Hide, nullptr, nullptr);
            return;
        }
        // UNVERIFIED (milestone 5, with CharacterInfoWindow): away from the city: CharacterInfoWindow::Hide, the glass is
        // used up (RemoveUniqueItem), Map::GetDecorationWithAnyDrop -> "MAGNIFYING_GLASS_NONE" or
        // the camera / arrow on that decoration (Fog::IsVisible, Map::AddCameraPoint). Only the city
        // is loaded so far.
        return;
    }
    int slot = -1;
    for (unsigned i = 0; i < GameState::GetBeltSlotCount(); ++i) {
        if (slot != -1) break;
        if (GameState::GetBeltItemAt(i) == item->id) slot = (int)i;
    }
    if (slot == -1) return;
    BuildingHovers::HideArrow();
    BeltBarWindow::OnActivateSlot((unsigned)slot);
    CharacterInfoWindow::UpdateContents();
    Hide();
}

// @0x385728
void ItemHoverWindow::OnEquip() {
    if (!item || !inventory) return;
    BuildingHovers::HideArrow();
    // UNVERIFIED (milestone 5): CharacterInfoWindow::OnEquip(true) (the inventory window equips its
    // selected item).
    Hide();
}

// @0x3856d0
void ItemHoverWindow::OnUnequip() {
    if (!item || !inventory) return;
    BuildingHovers::HideArrow();
    // UNVERIFIED (milestone 5): CharacterInfoWindow::ShowInventoryItem(item->id, 5, item) and
    // OnEquip(false); ItemShopWindow::UpdateContents.
    Hide();
}

// @0x385654
void ItemHoverWindow::OnRepair() {
    if (!item || !inventory) return;
    if (item->f0c == item->info->durability) return;
    if (item->info->IsProfessionTool() && item->info->cost2 == 0) return;
    BuildingHovers::HideArrow();
    // UNVERIFIED (milestone 5): RepairWindow::SetParameters(item) and Show.
    Hide();
}

// @0x3854b4: asks first (PopupSelectionWindow), except for a profession tool.
void ItemHoverWindow::OnRemove() {
    if (!item || !inventory) return;
    BuildingHovers::HideArrow();
    if (!item->info->IsProfessionTool()) {
        PopupSelectionWindow::Show(StringTable::GetString(""), StringTable::GetString("INVENTORY_REMOVE_CONFIRM"),
                                   StringTable::GetString("HUD_OK"), IconManager::GetIcon("gold_confirm"),
                                   [this] { OnRemoveActual(); }, StringTable::GetString("CANCEL"),
                                   IconManager::GetIcon("red_x"), [] { PopupSelectionWindow::Hide(); });
    } else {
        OnRemoveActual();
    }
    Hide();
}

// @0x385314
void ItemHoverWindow::OnRemoveActual() {
    GameState::RemoveUniqueItem(item->uniqueId);
    // UNVERIFIED (milestone 5): ItemShopWindow::UpdateContents.
    if (PopupSelectionWindow::IsVisible()) PopupSelectionWindow::Hide();
    CharacterInfoWindow::UpdateContents();
}

// @0x385270
void ItemHoverWindow::OnImprove() {
    if (!item || !inventory) return;
    BuildingHovers::HideArrow();
    // UNVERIFIED (milestone 5): a source collection -> CharacterInfoWindow::OnImprove(item) (and
    // CraftingWindow::HelpImprove when it is open); else the crafting / result collection ->
    // CraftItemWindow::Show + ShowCollection. No collections yet.
    Hide();
}

// @0x385030
void ItemHoverWindow::OnFind() {
    if (GameState::IsBossCombatActive() || GameState::IsArenaCombatActive()) {
        PopupWindow::Show(StringTable::GetString("CAN_NOT_TRAVEL"), PopupWindow::Hide, nullptr, nullptr);
        return;
    }
    if (!info && resource == 0xb) return;
    BuildingHovers::HideArrow();
    // UNVERIFIED (milestones 4d/5): the windows the popup may sit over are hidden first:
    // CharacterInfoWindow, ItemShopWindow, HireTroopsWindow, CraftItemWindow, LevelInfoWindow,
    // ArenaDifficultyWindow, CampaignInfoWindow, PlayerInfoWindow, PlayerCompetitionWindow.
    if (!info) {
        if (resource == 8) {
            ExchangeWindow::Show();
            ExchangeWindow::OnTab(0, nullptr);
        } else if (resource == 9) {
            ExchangeWindow::Show();
            ExchangeWindow::OnTab(1, nullptr);
        } else if (resource != 0xb) {
            BuildingHovers::SetArrowVisibleWindowLimit(2);
            NotEnoughWindow::FindResourceHelp(resource, 0, false);
        }
    } else {
        uint32_t id = info->id;
        if (info->packId && info->packItems && info->packItems->GetChild(0))
            id = info->packItems->GetChild(0)->GetChild(0)->GetInt();
        // UNVERIFIED (milestone 5): a mission map with the item (GlobalMapWindow::CenterOnMap and the
        // RaidInfoWindow for its location) or a crafting map (GlobalMapWindow::ArrowAtMap).
        if (!GetMissionMapForItem(id) && !GetMapForCraftItem(id)) NeedItemWindow::OnProduce(id);
    }
    // UNVERIFIED (milestone 4g): in the city, HealSquadWindow::Hide.
    Hide();
}

// @0x384f6c
void ItemHoverWindow::OnShop() {
    if (!info) return;
    // UNVERIFIED (milestone 5): PlayerCompetitionWindow / PlayerInfoWindow hidden when both show;
    // the item shop on the item (ItemShopWindow::ShowShopItem, mode 3 for 0x250) or on a pack with it
    // (ShowShopPack). The item shop is not ported.
    BuildingHovers::HideArrow();
    Hide();
}

// @0x384f10
void ItemHoverWindow::OnReputation() {
    if (!info) return;
    BuildingHovers::HideArrow();
    // UNVERIFIED (milestone 5): LevelInfoWindow on the item's profession tab and level.
    Hide();
}

// @0x384d14
void ItemHoverWindow::OnOpen() {
    if (!info) return;
    // UNVERIFIED (milestone 5): 0x211 opens ChestsWindow (chest 3); otherwise, out of combat, the
    // starter chest (0x2b6, ChestsWindow::SetStarterChestMode by "starter_chest_b") or a daily chest
    // (on the original a server request: GameState::SendDailyChestRequest with
    // ChestsWindow::GetBestLuckItem, then LoadingInternetWindow). In combat: "WEBVIEW_NOTNOW".
    if (info->id != 0x211 && GameState::IsCombatActive())
        PopupWindow::Show(StringTable::GetString("WEBVIEW_NOTNOW"), PopupWindow::Hide, nullptr, nullptr);
    Hide();
}

// @0x384cdc: online (the friends hub, PlayerCompetitionWindow tab 3): dormant.
void ItemHoverWindow::OnAskFriends() {
    if (!info) return;
    Hide();
}

// @0x384c6c
void ItemHoverWindow::OnTrain() {
    if (!unitData) return;
    // UNVERIFIED (milestone 5): HireTroopsWindow for the unit's building (EntityData +0x130) or its
    // tavern tab (Tavern::GetForEntity), then ShowEntity(unit id).
    Hide();
}

// @0x384bd4
void ItemHoverWindow::OnHeal() {
    if (!unit) return;
    if (GameState::IsArenaCombatActive()) {
        PopupWindow::Show(StringTable::GetString("CAN_NOT_OPEN_IN_ARENA"), PopupWindow::Hide, nullptr, nullptr);
        return;
    }
    // UNVERIFIED (milestones 4g/5): HireTroopsWindow::ShowHealSoldier when it is open, else
    // BattleBarWindow::ShowHealSoldier(unit).
    Hide();
}

// @0x384bb4 / @0x384b94: HireTroopsWindow::OnHintMove / OnHintDismiss (milestone 5: UNVERIFIED).
void ItemHoverWindow::OnMove() { Hide(); }
void ItemHoverWindow::OnDismiss() { Hide(); }

// @0x3849d8: the tame tutorial on map 0xd feeds the wolf.
void ItemHoverWindow::OnFeed() {
    if (GameState::GetCurrentMapID() == 0xd && !GameState::TaskCompleted(0x771)) {
        unit->AddHP(unit->GetHpMax() - unit->GetHP());
        // UNVERIFIED (milestone 4h): BuildingHovers::HideWorldDialog(null), HideArrow,
        // BattleBarWindow::UpdateContents, the arrow on decoration (10, 0x12) with its callback
        // (the player's AI +0x58) and Fog::Open there, Tasks::CompleteTask(0x771).
        Hide();
    }
    Hide();
}

// @0x384620: a building or decoration item goes to BuildingPlacement (only in the city; elsewhere
// the player is asked to travel home first).
void ItemHoverWindow::OnPlace() {
    if (GameState::IsBossCombatActive() || GameState::IsArenaCombatActive()) {
        PopupWindow::Show(StringTable::GetString("CAN_NOT_TRAVEL"), PopupWindow::Hide, nullptr, nullptr);
        return;
    }
    if (GameState::GetCurrentMapID() != 0) {
        std::u32string s = SWPrintf(0x100, StringTable::GetString("TRAVEL_HOME"), {GameState::GetCastleName()});
        PopupWindow::Show(s.c_str(), ItemPlaceContinuation, PopupWindow::Hide, nullptr);
        return;
    }
    if (!info) return;
    if (info->type == 0x81) {
        Map::Building* b = new Map::Building();
        b->id = info->id - 0x4000;
        b->baseY = 0.f;
        b->baseX = 0.f;
        b->data = GameData::GetBuilding(b->id);
        b->SetOpened();
        BuildingPlacement::BuildingBought(b, nullptr, true);
        delete b;   // (the original's vfunc +4, the deleting destructor: BuildingBought keeps a copy)
    }
    if (info->type == 0x82) {
        Map::Decor* d = new Map::Decor();
        d->id = info->id - 0x8000;
        BuildingPlacement::DecorBought(d, nullptr, true);
    }
    // UNVERIFIED (milestone 5): CharacterInfoWindow::Hide.
    Hide();
}

// ---- the tutorials' arrows ----

namespace {
void ArrowAtButton(ItemHoverWindow::ItemButton& b, bool lock, std::function<void()> cb, bool left) {
    BuildingHovers::SetArrowClickCallback(new GUI::Callback(std::move(cb)));
    if (lock) GUI::SetInteractionObjectLock(b.click, nullptr);
    BuildingHovers::SetArrowVisibleWindowLimit(2);
    GUI::Window* r = b.root;
    float x = left ? (float)r->x : (float)(r->w + r->x);
    BuildingHovers::ArrowAt(x, (float)(r->y + r->h / 2), false, true, left, false, true, false, false);
}
}  // namespace

// @0x384004
void ItemHoverWindow::HelpDrink(bool lock) {
    if (buttons[kDrink].root->visibleSelf && buttons[kDrink].root->enabled) {
        ArrowAtButton(buttons[kDrink], lock, [this] { OnDrink(); }, false);
        return;
    }
    std::fprintf(stderr, "ERROR: ItemHoverWindow::HelpDrink() Cannot help - player can't drink current item\n");
}

// @0x3840f8
void ItemHoverWindow::HelpFeed(bool lock) {
    if (buttons[kFeed].root->visibleSelf && buttons[kFeed].root->enabled) {
        ArrowAtButton(buttons[kFeed], lock, [this] { OnFeed(); }, false);
        return;
    }
    std::fprintf(stderr, "ERROR: ItemHoverWindow::HelpFeed() Cannot help - entity cannot be fed\n");
}

// @0x3841f0
void ItemHoverWindow::HelpRepair(bool lock) {
    if (buttons[kRepair].root->visibleSelf) {
        ArrowAtButton(buttons[kRepair], lock, [this] { OnRepair(); }, false);
        return;
    }
    std::fprintf(stderr, "ERROR: ItemHoverWindow::HelpRepair() Cannot help - item cannot be repaired\n");
}

// @0x3842dc
void ItemHoverWindow::HelpImprove(bool lock) {
    if (buttons[kImprove].root->visibleSelf) {
        ArrowAtButton(buttons[kImprove], lock, [this] { OnImprove(); }, false);
        helpingImprove = true;
        return;
    }
    std::fprintf(stderr, "ERROR: ItemHoverWindow::HelpImprove() Cannot help - item cannot be improved\n");
}

// @0x3843cc
void ItemHoverWindow::HelpEquip(bool lock) {
    if (buttons[kEquip].root->visibleSelf) {
        ArrowAtButton(buttons[kEquip], lock, [this] { OnEquip(); }, false);
        return;
    }
    std::fprintf(stderr, "ERROR: ItemHoverWindow::HelpEquip() Cannot help - item cannot be equipped\n");
}

// @0x3844b8 (the arrow at the button's left edge, mirrored)
void ItemHoverWindow::HelpEnable(bool lock) {
    if (buttons[kEnable].root->visibleSelf) {
        ArrowAtButton(buttons[kEnable], lock, [this] { OnEnable(); }, true);
        return;
    }
    std::fprintf(stderr, "ERROR: ItemHoverWindow::HelpEnable() Cannot help - item cannot be turned on\n");
}
