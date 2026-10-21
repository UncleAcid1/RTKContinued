// RepairWindow: repairing a worn item (Window_repair.xml), from its item card's Repair button. The
// item (quality frame, name, picture), its durability bar before and after, and three methods: a
// light and a thorough repair for gold (each also lowers the item's maximum durability to what it
// restores), and a full repair for crystals. The bars: dark red is the base durability, red the
// item's current maximum, green its durability left.
// Port of RepairWindow (libkingdom.so 5.11), 0x336218..0x338258. Statics at 0x62bf8c.
#include <cmath>
#include <cstdio>

#include "engine/SoundsManager.h"
#include "engine/IconManager.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/Setting.h"
#include "game/StringTable.h"
#include "game/Tasks.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/HUD.h"
#include "windows/Windows.h"

namespace RepairWindow {
namespace {

using GUI::Textfield;
using GUI::Window;

// One repair method (0x24 bytes), "button_light_upgrade_%02d".
struct OptionInfo {
    Window* root = nullptr;             // +0x00
    Window* selected = nullptr;         // +0x04 selected_button
    Textfield* method = nullptr;        // +0x08 text_method
    Window* goldIcon = nullptr;         // +0x0c icon_gold_cost (takes a depth slot)
    Window* crystalIcon = nullptr;      // +0x10 icon_35_crystal
    Textfield* quantity = nullptr;      // +0x14 text_quantity_item: an affordable gold price
    Textfield* notEnough = nullptr;     // +0x18 text_quantity_item_not_enough: a price above the gold
    Textfield* crystalPrice = nullptr;  // +0x1c text_price_crystalv (sic)
    GUI::Button* click = nullptr;       // +0x20 clickArea
};

WindowManager::FunctionalWindow* g_wnd = nullptr;   // RepairWindow::wnd 0x62c0cc
unsigned g_option = 1;                 // 0x60f378 the chosen method (0 light, 1 thorough, 2 crystals)
float g_scale = 1.f;                   // 0x60f380
bool g_tablet = false;
Textfield* g_levelName[5] = {};        // 0x62bf8c text_item_class_01..05: the name in its quality's colour
Window* g_icon = nullptr;              // 0x62bfa0 item_repair_icon
Window* g_broken = nullptr;            // 0x62bfa4 item_class_broken
Window* g_levelFrame[5] = {};          // 0x62bfa8 item_class_01..05
Window* g_picture = nullptr;           // 0x62bfbc item_icon_holder_item (takes a depth slot)
// The durability bars (0x62bfc0 before, 0x62c068 after), each clipped to its share of the width.
struct DurabilityBar {
    Window* root = nullptr;
    Window* darkRed = nullptr;
    Textfield* darkRedText = nullptr;
    Window* red = nullptr;
    Textfield* redText = nullptr;
    GUI::ClipRect redClip = {};
    Window* green = nullptr;
    Textfield* greenText = nullptr;
    GUI::ClipRect greenClip = {};
};
DurabilityBar g_before;                // 0x62bfc0
OptionInfo g_options[3];               // 0x62bffc
DurabilityBar g_after;                 // 0x62c068
GameState::PlayerItem* g_item = nullptr;   // 0x62c0a4
Textfield* g_header = nullptr;         // 0x62c0a8
Textfield* g_desc = nullptr;           // 0x62c0ac text_desc
Textfield* g_chooseMethod = nullptr;   // 0x62c0b0
Textfield* g_afterRepair = nullptr;    // 0x62c0b4 text_after_repair
Window* g_confirm = nullptr;           // 0x62c0b8 button_confirm_repair
Textfield* g_confirmText = nullptr;    // 0x62c0bc
Window* g_root = nullptr;              // 0x62c0c0
GUI::TweenEffect* g_tween = nullptr;   // 0x62c0c4
GUI::Button* g_close = nullptr;        // 0x62c0c8 x_button.clickArea

// The light repair restores 0.5 + 20% of the missing durability, the thorough one 0.5 + 50%.
int LightAmount(const GameState::PlayerItem* it) {
    return (int)(0.5f + (float)(it->info->durability - it->f0c) * 0.2f);
}
int ThoroughAmount(const GameState::PlayerItem* it) {
    return (int)(0.5f + (float)(it->info->durability - it->f0c) * 0.5f);
}

// @0x3363a8: the crystal price of amount durability points: half the item's crystal price (or its
// gold price at the exchange rate) per its full durability, else repair_crystal_cost per
// repair_crystal_amount; at least 1, with the crystal multiplier.
int GetRepairCostCrystals(int amount, const GameState::PlayerItem* it) {
    const Items::ItemInfo* info = it->info;
    int price = (int)info->cost2;
    float perPoint;
    if (price == 0) price = (int)std::ceil(Setting("exchange_gold").GetFloat() * (float)info->cost);
    if (price != 0)
        perPoint = (float)price * 0.5f / (float)info->durability;
    else
        perPoint = Setting("repair_crystal_cost").GetFloat() / Setting("repair_crystal_amount").GetFloat();
    int cost = (int)std::ceil((float)amount * perPoint);
    if (cost == 0) cost = 1;
    return GameState::AdjustCrystalCost(cost);
}

// @0x3364c4: the gold price likewise (the crystal price over the exchange rate; else the light or
// thorough repair's cost per amount setting); at least 1.
int GetRepairCostGold(int amount, const GameState::PlayerItem* it, bool light) {
    const Items::ItemInfo* info = it->info;
    int price = (int)info->cost;
    float perPoint;
    if (price == 0) price = (int)std::ceil((float)info->cost2 / Setting("exchange_gold").GetFloat());
    if (price != 0)
        perPoint = (float)price * 0.5f / (float)info->durability;
    else if (light)
        perPoint = Setting("repair_weak_cost").GetFloat() / Setting("repair_weak_amount").GetFloat();
    else
        perPoint = Setting("repair_med_cost").GetFloat() / Setting("repair_med_amount").GetFloat();
    const int cost = (int)std::ceil((float)amount * perPoint);
    return cost == 0 ? 1 : cost;
}

// @0x336378
void OnTweenOut() {
    WindowManager::WindowHide(false);
    g_wnd->shown = false;
    g_wnd->MoveWindowDown(true);
}

// @0x33736c
void OnOption(unsigned i) {
    g_option = i;
    SetParameters(g_item);
}

// @0x337488: pays for the chosen method (or opens NotEnoughWindow) and repairs.
void OnRepair() {
    GameState::PlayerItem* it = g_item;
    unsigned cur = it->f0c, max = it->f10;
    const int lightCost = GetRepairCostGold(LightAmount(it), it, true);
    const int thoroughCost = GetRepairCostGold(ThoroughAmount(it), it, false);
    const int crystalCost = GetRepairCostCrystals((int)(it->info->durability - it->f0c), it);
    const struct { unsigned option; int type, cost; } kChecks[3] = {
        {0, 8, lightCost}, {1, 8, thoroughCost}, {2, 9, crystalCost}};
    for (const auto& c : kChecks) {
        if (g_option != c.option) continue;
        NotEnoughWindow::ResetRequirements();
        NotEnoughWindow::AddRequirement(c.type, (unsigned)c.cost);
        if (!NotEnoughWindow::CheckRequirements()) {
            NotEnoughWindow::Show();
            return;
        }
        // UNVERIFIED (milestone 6): SoundsManager::PlaySound("ui_buy_with_crystals", 1, false) for
        // the crystal repair.
    }
    // The gold repairs restore part of the missing durability (up to the current maximum, which
    // becomes the new durability); the crystal repair restores all of it.
    if (g_option == 0) {
        cur = std::min(cur + (unsigned)LightAmount(it), it->f10);
        GameState::ChangeResourceAmount(8, -lightCost);
        max = cur;
    }
    if (g_option == 1) {
        cur = std::min(cur + (unsigned)ThoroughAmount(it), it->f10);
        GameState::ChangeResourceAmount(8, -thoroughCost);
        max = cur;
    }
    if (g_option == 2) {
        cur = it->info->durability;
        GameState::ChangeResourceAmount(9, -crystalCost);
        // (Billing::LogCBPurchase(0x1f, item, cost) and FileManager::OnItemBought("item_repair",
        // ..., "crystals"): purchase statistics, online.)
        max = cur;
    }
    it->f10 = max;
    it->f0c = cur;
    SoundsManager::PlaySound("ui_repair", 1, false);
    if (GameState::IsTaskStarted(0x597)) Tasks::CompleteSubtask(0x3c, 0, 1);
    if (g_option == 2 && it->f0c != it->info->durability) SetParameters(it);
    else Hide();
    CharacterInfoWindow::UpdateContents();
    ItemShopWindow::UpdateContents();
}

// @0x337460
bool OnBack() {
    if (!g_wnd->shown) return false;
    Hide();
    return true;
}

// @0x33782c
void SetZ(float z) {
    if (!g_wnd->shown) return;
    g_root->SetZ(z);
    g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::GetVerticalCenter(g_root->h));
}

// @0x3378b4: the HUD's top bar keeps its taps.
bool Click(int x, int y, bool pressed) {
    if (TopCityWindow::Click(x, y, pressed)) return true;
    const bool visible = g_root->visibleSelf;
    g_root->Click(x, y, pressed, false);
    return visible;
}

// The bar's red and green parts, their texts, and their clip rectangles (x .. x + w, 0 .. 0x400)
// from the "before" bar's red and green parts.
void LoadBar(DurabilityBar& b, const char* name) {
    char path[160];
    b.root = GUI::GetWindowTyped<Window>(g_root, name);
    std::snprintf(path, sizeof path, "%s.durability_bar_dark_red", name);
    b.darkRed = GUI::GetWindowTyped<Window>(g_root, path);
    std::snprintf(path, sizeof path, "%s.text_bar_dark_red", name);
    b.darkRedText = GUI::GetWindowTyped<Textfield>(g_root, path);
    std::snprintf(path, sizeof path, "%s.durability_bar_red", name);
    b.red = GUI::GetWindowTyped<Window>(g_root, path);
    std::snprintf(path, sizeof path, "%s.text_bar_red", name);
    b.redText = GUI::GetWindowTyped<Textfield>(g_root, path);
    const Window* red = g_before.red ? g_before.red : b.red;
    b.redClip = {red->x, 0, red->x + red->w, 0x400};
    b.red->SetClipRect(&b.redClip);
    b.redText->SetClipRect(&b.redClip);
    std::snprintf(path, sizeof path, "%s.durability_bar_green", name);
    b.green = GUI::GetWindowTyped<Window>(g_root, path);
    std::snprintf(path, sizeof path, "%s.text_bar_green", name);
    b.greenText = GUI::GetWindowTyped<Textfield>(g_root, path);
    const Window* green = g_before.green ? g_before.green : b.green;
    b.greenClip = {green->x, 0, green->x + green->w, 0x400};
    b.green->SetClipRect(&b.greenClip);
    b.greenText->SetClipRect(&b.greenClip);
}

// The texts "cur / max" and the red and green parts' shares (red: maximum, green: left).
void FillBar(DurabilityBar& b, unsigned left, unsigned maximum, unsigned base) {
    const std::u32string text =
        SWPrintf(0x100, StringTable::GetString("REPAIR_DURABILITY"), {ToWideString((int)left), ToWideString((int)base)});
    b.darkRedText->SetText(text.c_str());
    b.redText->SetText(text.c_str());
    b.greenText->SetText(text.c_str());
    b.redClip.right = (int)((float)b.redClip.left + (float)b.red->w * (float)maximum / (float)base);
    b.greenClip.right = (int)((float)b.greenClip.left + (float)b.green->w * (float)left / (float)base);
}

}  // namespace

// Static FunctionalWindow("RepairWindow", Init, Deinit, Click, SetZ, Hide, Move).
WindowManager::FunctionalWindow* Queue() {
    if (!g_wnd) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.click = Click;
        f.setZ = SetZ;
        f.hide = Hide;
        g_wnd = new WindowManager::FunctionalWindow("RepairWindow", std::move(f));
    }
    return g_wnd;
}

// @0x336218
bool IsVisible() { return g_wnd->shown; }

// @0x337958
void Init() {
    g_tablet = GUI::IsTabletVersion();
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Window_repair.xml", "Window_repair.png", -1.f, 0, 0, 0,
                             0x32, false, 1.f);
    g_wnd->RegisterTopWindow(g_root);
    g_scale = g_root->scale;
    g_root->SetVisibility(false);
    g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::GetVerticalCenter(g_root->h));
    g_tween = GUI::CreateTweenEffect(g_root, nullptr, nullptr, OnTweenOut);
    g_tween->CenterWith(0, (int)(g_scale * -50.f), 0, 0);
    g_close = GUI::GetWindowTyped<GUI::Button>(g_root, "x_button.clickArea");
    g_close->SetOnClick(Hide);
    g_close->SetActionSound("ui_close", 1);
    g_header = GUI::GetWindowTyped<Textfield>(g_root, "text_header");
    for (int k = 0; k < 5; ++k) g_levelName[k] = GUI::GetWindowTypedF<Textfield>(g_root, "text_item_class_%02d", k + 1);
    g_icon = GUI::GetWindowTyped<Window>(g_root, "item_repair_icon");
    g_broken = GUI::GetWindowTyped<Window>(g_root, "item_repair_icon.item_class_broken");
    for (int k = 0; k < 5; ++k)
        g_levelFrame[k] = GUI::GetWindowTypedF<Window>(g_root, "item_repair_icon.item_class_%02d", k + 1);
    g_picture = GUI::GetWindowTyped<Window>(g_root, "item_repair_icon.item_icon_holder_item");
    g_picture->takesZ = true;
    g_desc = GUI::GetWindowTyped<Textfield>(g_root, "text_desc");
    LoadBar(g_before, "progress_bar_item_durability_before");
    g_chooseMethod = GUI::GetWindowTyped<Textfield>(g_root, "text_choose_method");
    for (unsigned i = 0; i < 3; ++i) {
        OptionInfo& o = g_options[i];
        const int n = (int)i + 1;
        o.root = GUI::GetWindowTypedF<Window>(g_root, "button_light_upgrade_%02d", n);
        o.selected = GUI::GetWindowTypedF<Window>(g_root, "button_light_upgrade_%02d.selected_button", n);
        o.method = GUI::GetWindowTypedF<Textfield>(g_root, "button_light_upgrade_%02d.text_method", n);
        o.goldIcon = GUI::GetWindowTypedF<Window>(g_root, "button_light_upgrade_%02d.icon_gold_cost", n);
        o.goldIcon->takesZ = true;
        o.crystalIcon = GUI::GetWindowTypedF<Window>(g_root, "button_light_upgrade_%02d.icon_35_crystal", n);
        o.quantity = GUI::GetWindowTypedF<Textfield>(g_root, "button_light_upgrade_%02d.text_quantity_item", n);
        o.notEnough =
            GUI::GetWindowTypedF<Textfield>(g_root, "button_light_upgrade_%02d.text_quantity_item_not_enough", n);
        o.crystalPrice = GUI::GetWindowTypedF<Textfield>(g_root, "button_light_upgrade_%02d.text_price_crystalv", n);
        o.click = GUI::GetWindowTypedF<GUI::Button>(g_root, "button_light_upgrade_%02d.clickArea", n);
        o.click->SetOnClick([i] { OnOption(i); });
    }
    g_afterRepair = GUI::GetWindowTyped<Textfield>(g_root, "text_after_repair");
    LoadBar(g_after, "progress_bar_item_durability_after");
    g_confirm = GUI::GetWindowTyped<Window>(g_root, "button_confirm_repair");
    g_confirm->SetOnClick(OnRepair);
    g_wnd->fn.back = OnBack;
    g_wnd->fn.zRange = WindowManager::ReturnSmallZRange;
    g_wnd->hideOnOuterClick = true;
    g_wnd->fn.show = Show;
    g_confirmText = GUI::GetWindowTyped<Textfield>(g_root, "button_confirm_repair.text_upgrade");
}

// @0x33661c: the item, its bars, the three prices; an item at its current maximum can only be
// repaired with crystals.
void SetParameters(GameState::PlayerItem* it) {
    g_item = it;
    const Items::ItemInfo* info = it->info;
    g_header->SetText(StringTable::GetString("REPAIR_HEADER"));
    for (unsigned k = 0; k < 5; ++k) g_levelName[k]->SetVisibility(info->itemLevel == k);
    g_levelName[info->itemLevel]->SetText(info->title);
    g_broken->SetVisibility(it->IsBroken());
    for (unsigned k = 0; k < 5; ++k) g_levelFrame[k]->SetVisibility(info->itemLevel == k && !it->IsBroken());
    const_cast<Items::ItemInfo*>(info)->PutImageInHolder(g_picture);
    g_desc->SetText(StringTable::GetString("REPAIR_DESCRIPTION"));
    FillBar(g_before, it->f0c, it->f10, info->durability);
    g_chooseMethod->SetText(StringTable::GetString("REPAIR_CHOOSE_METHOD"));
    g_options[0].goldIcon->SetTexture(IconManager::GetIcon(GameState::GetResourceMapIconName(8), false));
    g_options[0].crystalIcon->SetVisibility(false);
    g_options[1].goldIcon->SetTexture(IconManager::GetIcon(GameState::GetResourceMapIconName(8), false));
    g_options[1].crystalIcon->SetVisibility(false);
    g_options[2].goldIcon->SetVisibility(false);
    const int gold = (int)GameState::GetResourceAmount(8);
    const int costs[2] = {GetRepairCostGold(LightAmount(it), it, true), GetRepairCostGold(ThoroughAmount(it), it, false)};
    for (unsigned i = 0; i < 2; ++i) {
        OptionInfo& o = g_options[i];
        o.quantity->SetText(ToWideString(costs[i]));
        o.quantity->SetVisibility(costs[i] <= gold);
        o.notEnough->SetText(ToWideString(costs[i]));
        o.notEnough->SetVisibility(gold < costs[i]);
    }
    g_options[2].crystalPrice->SetText(ToWideString(GetRepairCostCrystals((int)(info->durability - it->f0c), it)));
    g_options[0].method->SetText(StringTable::GetString("REPAIR_OPTION_1"));
    g_options[1].method->SetText(StringTable::GetString("REPAIR_OPTION_2"));
    g_options[2].method->SetText(StringTable::GetString("REPAIR_OPTION_3"));
    g_options[0].root->SetEnabled(it->f0c != it->f10 && LightAmount(it) != 0);
    g_options[1].root->SetEnabled(it->f0c != it->f10);
    g_options[2].root->SetEnabled(it->f0c != info->durability);
    if (it->f0c == it->f10) g_option = 2;
    for (unsigned i = 0; i < 3; ++i) g_options[i].selected->SetVisibility(g_option == i);
    // After the chosen repair (the red part keeps the current maximum).
    g_afterRepair->SetText(StringTable::GetString("REPAIR_AFTER_REPAIR"));
    unsigned after = it->f0c;
    if (g_option == 0) after = std::min(it->f0c + (unsigned)LightAmount(it), it->f10);
    else if (g_option == 1) after = std::min(it->f0c + (unsigned)ThoroughAmount(it), it->f10);
    else if (g_option == 2) after = info->durability;
    FillBar(g_after, after, it->f10, info->durability);
    g_confirm->SetEnabled(g_option != (unsigned)-1);
    g_confirmText->SetText(StringTable::GetString("INVENTORY_REPAIR"));
    g_root->UpdatePosition();
}

// @0x337390
void Show() {
    g_wnd->shown = true;
    g_wnd->MoveWindowOnTop(true);
    if (!g_root->visibleSelf) {
        WindowManager::WindowShow(false);
        g_tween->AnimateIn();
    }
    g_root->SetVisibility(true);
    g_root->PreloadTextures();   // GUI::PreloadWindow
}

// @0x33740c: the next opening starts on the thorough repair.
void Hide() {
    if (g_root->visibleSelf) g_tween->AnimateOut();
    g_option = 1;
    g_item = nullptr;
}

}  // namespace RepairWindow
