// ExchangeWindow: the treasury's exchange (Payment_screen.xml): two tabs, "Buy Gold" and
// "Buy Crystals", each with six packs, the best deal and popular banners and the packs' bonuses.
// Port of ExchangeWindow (libkingdom.so 5.11), 0x2abcbc..0x2af8bc. Statics at 0x61e814.
//
// PORT (freemium removal, see STATUS.md): the original sold every pack for real money through the
// app store (Billing::Process -> FileManager::BillingProcessAndroid, goods added by BoughtCallback
// when the store confirmed). Offline it is a two-way exchange: a gold pack costs crystals and a
// crystal pack costs gold, gold being the dearer direction. The rates are the settings
// "port_exchange_gold_per_crystal" and "port_exchange_gold_per_bought_crystal" (placeholders to
// tune). The packs' amounts, bonuses, banners and layout are the original's.
#include <cmath>
#include <cstdio>
#include <string>

#include "engine/Render.h"
#include "engine/Timer.h"
#include "engine/IconManager.h"
#include "game/GameState.h"
#include "game/Map.h"
#include "game/Setting.h"
#include "game/StringTable.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "windows/Windows.h"

namespace ExchangeWindow {
namespace {

using GUI::Button;
using GUI::Textfield;
using GUI::Window;

// TabPayment (0x14 bytes), "tab_payment_%02d". LoadFrom @0x2af80c.
struct TabPayment {
    Window* root = nullptr;          // +0x00
    Window* goldIcon = nullptr;      // +0x04 icon_gold_60_tab (takes a depth slot)
    Window* crystalIcon = nullptr;   // +0x08 icon_crystal_60 (takes a depth slot)
    Textfield* text = nullptr;       // +0x0c text_get_gold
    Button* click = nullptr;         // +0x10 clickArea
    void LoadFrom(Window* r, const char* p) {
        root = GUI::GetWindowTypedF<Window>(r, "%s", p);
        goldIcon = GUI::GetWindowTypedF<Window>(r, "%s.icon_gold_60_tab", p);
        goldIcon->takesZ = true;
        crystalIcon = GUI::GetWindowTypedF<Window>(r, "%s.icon_crystal_60", p);
        crystalIcon->takesZ = true;
        text = GUI::GetWindowTypedF<Textfield>(r, "%s.text_get_gold", p);
        click = GUI::GetWindowTypedF<Button>(r, "%s.clickArea", p);
    }
};

// BonusDeal (0x20 bytes), the banners over a pack. LoadFrom @0x2af29c.
struct BonusDeal {
    Window* root = nullptr;          // +0x00
    Window* redX1 = nullptr;         // +0x04 bonus_best_deal_red_x1
    Window* goldX1 = nullptr;        // +0x08 bonus_best_deal_gold_x1
    Textfield* textX1 = nullptr;     // +0x0c text_best_deal_x1_mobile
    Window* redX2 = nullptr;         // +0x10 bonus_best_deal_red_x2
    Window* goldX2 = nullptr;        // +0x14 bonus_best_deal_gold_x2
    Textfield* textX2a = nullptr;    // +0x18 text_best_deal_x2_01_mobile
    Textfield* textX2b = nullptr;    // +0x1c text_best_deal_x2_02_mobile
    void LoadFrom(Window* r, const char* p, float scale) {
        root = GUI::GetWindowTypedF<Window>(r, "%s", p);
        redX1 = GUI::GetWindowTypedF<Window>(r, "%s.bonus_best_deal_red_x1", p);
        goldX1 = GUI::GetWindowTypedF<Window>(r, "%s.bonus_best_deal_gold_x1", p);
        textX1 = GUI::GetWindowTypedF<Textfield>(r, "%s.text_best_deal_x1_mobile", p);
        textX1->MoveWindow(0, (int)(scale * -5.f));
        textX1->SetOverscale(1.5f);
        redX2 = GUI::GetWindowTypedF<Window>(r, "%s.bonus_best_deal_red_x2", p);
        goldX2 = GUI::GetWindowTypedF<Window>(r, "%s.bonus_best_deal_gold_x2", p);
        textX2a = GUI::GetWindowTypedF<Textfield>(r, "%s.text_best_deal_x2_01_mobile", p);
        textX2a->SetOverscale(1.5f);
        textX2b = GUI::GetWindowTypedF<Textfield>(r, "%s.text_best_deal_x2_02_mobile", p);
        textX2b->SetOverscale(1.5f);
    }
};

// One currency's half of a pack: the price, the amount (with the crossed-out amount before a
// discount) and the bonus.
struct Holder {
    Window* root = nullptr;          // buy_crystals_holder / buy_gold_holder
    Textfield* priceBefore = nullptr;   // text_price_before
    Window* crossoutPrice = nullptr; // element_crossout_price
    Textfield* price = nullptr;      // text_price
    Textfield* get1 = nullptr;       // text_get_01 (the amount before a discount)
    Window* crossout = nullptr;      // element_crossout
    Textfield* get2 = nullptr;       // text_get_02 (the amount)
    Textfield* bonus = nullptr;      // text_get_bonus ("+ %d")
    Window* icon = nullptr;          // icon_crystal / icon_16_gold
    void LoadFrom(Window* r, const char* p, const char* holder, const char* iconName, const char* tex) {
        std::string h = std::string(p) + "." + holder;
        const char* q = h.c_str();
        root = GUI::GetWindowTypedF<Window>(r, "%s", q);
        priceBefore = GUI::GetWindowTypedF<Textfield>(r, "%s.text_price_before", q);
        crossoutPrice = GUI::GetWindowTypedF<Window>(r, "%s.element_crossout_price", q);
        price = GUI::GetWindowTypedF<Textfield>(r, "%s.text_price", q);
        get1 = GUI::GetWindowTypedF<Textfield>(r, "%s.text_get_01", q);
        crossout = GUI::GetWindowTypedF<Window>(r, "%s.element_crossout", q);
        get2 = GUI::GetWindowTypedF<Textfield>(r, "%s.text_get_02", q);
        bonus = GUI::GetWindowTypedF<Textfield>(r, "%s.text_get_bonus", q);
        icon = GUI::GetWindowTypedF<Window>(r, (std::string("%s.") + iconName).c_str(), q);
        icon->SetTexture(IconManager::GetIcon(tex), true);
    }
};

// PaymentLine (0x78 bytes), "buttons_get_crystals.button_buy_currency_%02d". LoadFrom @0x2af4b4.
struct PaymentLine {
    Window* root = nullptr;          // +0x00
    Window* buttonSmall = nullptr;   // +0x04 button_active_payment_small
    Window* buttonLarge = nullptr;   // +0x08 button_active_payment_large
    Holder crystals;                 // +0x0c..+0x2c buy_crystals_holder
    Holder gold;                     // +0x30..+0x50 buy_gold_holder
    BonusDeal deal;                  // +0x54 bonus_best_deal_holder
    Button* click = nullptr;         // +0x74 clickArea
    void LoadFrom(Window* r, const char* p, float scale) {
        root = GUI::GetWindowTypedF<Window>(r, "%s", p);
        buttonSmall = GUI::GetWindowTypedF<Window>(r, "%s.button_active_payment_small", p);
        buttonLarge = GUI::GetWindowTypedF<Window>(r, "%s.button_active_payment_large", p);
        crystals.LoadFrom(r, p, "buy_crystals_holder", "icon_crystal", "35_crystal");
        gold.LoadFrom(r, p, "buy_gold_holder", "icon_16_gold", "16_gold");
        deal.LoadFrom(r, (std::string(p) + ".bonus_best_deal_holder").c_str(), scale);
        click = GUI::GetWindowTypedF<Button>(r, "%s.clickArea", p);
    }
};

// DailyBonus, the chest corner. LoadFrom @0x2af3fc.
struct DailyBonus {
    Textfield* header = nullptr;     // 0x61eb9c task_header
    Window* chestOpened = nullptr;   // 0x61eba0
    Window* chestClosed = nullptr;   // 0x61eba4
    Window* bagGold = nullptr;       // 0x61eba8 daily_bonus_bag_gold
    Window* bagCrystals = nullptr;   // 0x61ebac daily_bonus_bag_crystals
    Window* timeout = nullptr;       // 0x61ebb0 holder_chest_timeout
    Textfield* timeoutText = nullptr;   // 0x61ebb4
    void LoadFrom(Window* r) {
        header = GUI::GetWindowTyped<Textfield>(r, "task_header");
        chestOpened = GUI::GetWindowTyped<Window>(r, "icon_chest_opened");
        chestClosed = GUI::GetWindowTyped<Window>(r, "icon_chest_closed");
        bagGold = GUI::GetWindowTyped<Window>(r, "daily_bonus_bag_gold");
        bagCrystals = GUI::GetWindowTyped<Window>(r, "daily_bonus_bag_crystals");
        timeout = GUI::GetWindowTyped<Window>(r, "holder_chest_timeout");
        timeoutText = GUI::GetWindowTyped<Textfield>(r, "holder_chest_timeout.text_timeout");
    }
};

// The original's pack tables (0x595c20), 7 entries; LineToindex maps line 5 to entry 5 (6 with
// server setting 0xb6, online only).
const int kCrystalMain[7] = {1000, 500, 200, 140, 70, 15, 30};      // +0x8c
const int kCrystalBonus[7] = {125, 50, 15, 6, 3, 0, 0};             // +0x1c, discount row 0
const int kGoldMain[7] = {200000, 100000, 40000, 28000, 14000, 3000, 6000};   // +0x118
const int kGoldBonus[7] = {25000, 10000, 3000, 2000, 1000, 0, 0};   // +0xa8, discount row 0

WindowManager::FunctionalWindow* g_queue = nullptr;   // ExchangeWindow::wnd
Window* g_root = nullptr;              // 0x61e814 Payment_screen.xml
GUI::TweenEffect* g_tween = nullptr;   // 0x61e818
TabPayment g_tabs[2];                  // 0x61e81c
PaymentLine g_lines[6];                // 0x61e844
Window* g_chestInfo[6] = {};           // 0x61eb14 buttons_get_crystals.button_chest_info_%02d
int g_crystalAmounts[6][2] = {};       // 0x61eb2c (amount, bonus)
int g_goldAmounts[6][2] = {};          // 0x61eb5c (amount, bonus)
unsigned g_tab = 0;                    // 0x61eb8c 0 gold, 1 crystals
Window* g_gemHolder = nullptr;         // 0x61eb90 payment_gem_holder (discount tasks)
DailyBonus g_daily;                    // 0x61eb9c
bool g_tabSet = false;                 // 0x61ebb8
Textfield* g_header = nullptr;         // 0x61ebbc task__buy_more
uint32_t g_lastSecond = 0;             // 0x61ebc0
Button* g_close = nullptr;             // 0x61ebc8 x_button.clickArea
Window* g_golden = nullptr;            // 0x61ebcc golden_border_box
float g_scale = 1.f;                   // 0x60f20c
bool g_tablet = false;                 // 0x60f210

std::u32string Grouped(int n) { return SWPrintf(0x20, U"%$d", {n}); }

// PORT: the prices. A gold pack costs its matching crystal pack's crystals (gold per crystal),
// a crystal pack costs gold per bought crystal; bonuses come free with the bigger packs.
int GoldPackPrice(unsigned line) {
    float rate = Setting("port_exchange_gold_per_crystal").GetFloat();
    return (int)std::ceil((float)kGoldMain[line] / (rate > 0.f ? rate : 200.f));
}
int CrystalPackPrice(unsigned line) {
    float rate = Setting("port_exchange_gold_per_bought_crystal").GetFloat();
    return (int)std::ceil((float)kCrystalMain[line] * (rate > 0.f ? rate : 400.f));
}

// @0x2ac3cc: amounts in whole tens.
void MakeNeat(int& a, int& b) {
    a = (int)(std::ceil((float)a / 10.f) * 10.f);
    b = (int)(std::ceil((float)b / 10.f) * 10.f);
}

// @0x2ac434. UNVERIFIED (milestone 4, Tasks): the discount tasks 0x4d8/0x4da/0x4db raise the
// multiplier to 1.3/1.4/3.0 and pick the bonus row; offline none is active (multiplier 1.0, row 0).
void UpdateCosts() {
    const float mult = 1.f;
    for (unsigned i = 0; i < 6; ++i) {
        g_crystalAmounts[i][0] = (int)std::floor((float)kCrystalMain[i] * mult);
        g_crystalAmounts[i][1] = (int)std::floor((float)kCrystalBonus[i] * mult);
        g_goldAmounts[i][0] = (int)std::floor((float)kGoldMain[i] * mult);
        g_goldAmounts[i][1] = (int)std::floor((float)kGoldBonus[i] * mult);
        MakeNeat(g_goldAmounts[i][0], g_goldAmounts[i][1]);
    }
}

// One holder's texts: the price, the amount and its "+ bonus" placed after the amount's text.
void FillHolder(Holder& h, const std::u32string& price, int amount, int bonus) {
    h.price->SetText(price.c_str());
    h.get1->SetVisibility(false);   // no discount task: no amount before the discount
    h.crossout->SetVisibility(false);
    h.get2->SetText(ToWideString(amount));
    if (bonus < 1) {
        h.bonus->SetVisibility(false);
        return;
    }
    h.bonus->SetVisibility(true);
    h.bonus->SetText(SWPrintf(0x80, U"+ %d", {bonus}).c_str());
    h.bonus->RestorePosition();
    int dx = 0;
    if (Render::Sprite* s = h.get2->sprite)
        dx = -(int)(((float)(h.bonus->x - h.get2->x) - s->w) + 10.f);
    h.bonus->MoveWindow(dx, 0);
}

// @0x2ad1e4: the tabs' looks, the chest corner, then every pack line.
void UpdateContents() {
    if (!g_root->visibleSelf) return;
    UpdateCosts();
    g_tabs[0].root->SetFrame(g_tab == 0 ? 0x19 : 0, true);
    g_tabs[1].root->SetFrame(g_tab == 1 ? 0x19 : 0, true);
    g_tabs[0].goldIcon->SetTexture(IconManager::GetIcon("bugs"), true);
    g_tabs[0].crystalIcon->SetVisibility(false);
    g_tabs[0].text->SetText(StringTable::GetString("PAYMENT_CONVERT_GD_MOBILE"));
    g_tabs[1].goldIcon->SetVisibility(false);
    g_tabs[1].crystalIcon->SetTexture(IconManager::GetIcon("crystal_60"), true);
    g_tabs[1].text->SetText(StringTable::GetString("PAYMENT_CONVERT_CB_MOBILE"));
    // UNVERIFIED (milestone 4, Tasks): with a discount task active the gem holder shows its icon and
    // time left ("FACTORY_TIME_LEFT"); offline none is.
    g_gemHolder->SetVisibility(false);
    g_daily.chestOpened->SetVisibility(false);
    g_daily.chestClosed->SetVisibility(false);
    g_daily.bagGold->SetVisibility(g_tab == 0);
    g_daily.bagCrystals->SetVisibility(g_tab == 1);
    g_daily.header->SetVisibility(false);
    g_daily.timeout->SetVisibility(false);
    for (unsigned i = 0; i < 6; ++i) {
        PaymentLine& l = g_lines[i];
        l.buttonSmall->SetVisibility(false);
        l.buttonLarge->SetVisibility(true);
        l.gold.root->SetVisibility(g_tab == 0);
        l.crystals.root->SetVisibility(g_tab == 1);
        l.gold.crossoutPrice->SetVisibility(false);
        l.crystals.crossoutPrice->SetVisibility(false);
        // PORT: the price was "$%s" of the store price (realCost / 100, "%.2f"); it is the
        // exchange price in the other currency now.
        if (g_tab == 0) {
            std::u32string price = Grouped(GoldPackPrice(i)) + U" " + GameState::GetResourceGameName(GameState::kCrystal);
            FillHolder(l.gold, price, g_goldAmounts[i][0], g_goldAmounts[i][1]);
        } else if (g_tab == 1) {
            std::u32string price = Grouped(CrystalPackPrice(i)) + U" " + GameState::GetResourceGameName(GameState::kGold);
            FillHolder(l.crystals, price, g_crystalAmounts[i][0], g_crystalAmounts[i][1]);
        }
        l.deal.redX2->SetVisibility(i == 2);
        l.deal.redX1->SetVisibility(i == 0);
        l.deal.goldX2->SetVisibility(false);
        l.deal.goldX1->SetVisibility(false);
        l.deal.textX1->SetVisibility(l.deal.redX2->visibleSelf || l.deal.redX1->visibleSelf);
        l.deal.textX1->SetText(StringTable::GetString(i == 2 ? "PAYMENT_POPULAR" : "PAYMENT_BEST_DEAL"));
        l.deal.textX1->SetRotation(0.12f);
        l.deal.textX2a->SetVisibility(l.deal.goldX2->visibleSelf || l.deal.goldX1->visibleSelf);
        l.deal.textX2a->SetText(StringTable::GetString("PAYMENT_BEST_DEAL"));
        l.deal.textX2b->SetVisibility(l.deal.goldX2->visibleSelf || l.deal.goldX1->visibleSelf);
        l.deal.textX2b->SetText(StringTable::GetString("PAYMENT_BONUS_CHEST"));
        g_chestInfo[i]->SetVisibility(false);
    }
}

// The original's BoughtCallback @0x2ac5e0 tail: the "received" popup, then the save the
// purchase triggers.
void Received(bool crystals, int amount) {
    GameState::updated = true;
    // SaveManager::QueueSendSave and FileManager::BillingReceived: online, not ported.
    Map::SafeSave();
    std::u32string n = Grouped(amount);
    const char32_t* fmt = StringTable::GetString(crystals ? "MONEY_BOUGHT_MOBILE_TXT2" : "MONEY_BOUGHT_MOBILE_TXT");
    PopupWindow::Show(SWPrintf(0x100, fmt, {n.c_str()}).c_str(), PopupWindow::Hide, nullptr, nullptr);
}

// PORT (replaces OnPayForDollars @0x2ad020): a gold pack for crystals, confirmed first.
void OnPayForGold(unsigned line) {
    int price = GoldPackPrice(line);
    int have = (int)GameState::GetResourceAmount(GameState::kCrystal);
    if (have < price) {
        OnTab(g_tab, SWPrintf(0x80, StringTable::GetString("NO_CRYSTALS"),
                              {ToWideString(price - have), GameState::GetPlayerName()})
                         .c_str());
        return;
    }
    ConfirmPurchaseWindow::SetParameters(
        [line, price] {
            // UNVERIFIED (milestone 5): SoundsManager::PlaySound("ui_buy_with_gold").
            int gold = g_goldAmounts[line][0] + g_goldAmounts[line][1];
            GameState::ChangeResourceAmount(GameState::kCrystal, -price);
            GameState::ChangeResourceAmount(GameState::kGold, gold);
            Received(false, gold);
        },
        (unsigned)price, false);
    ConfirmPurchaseWindow::Show();
}

// PORT (replaces OnPayForCB @0x2ace84): a crystal pack for gold.
void OnPayForCrystals(unsigned line) {
    int price = CrystalPackPrice(line);
    int have = (int)GameState::GetResourceAmount(GameState::kGold);
    if (have < price) {
        OnTab(g_tab, SWPrintf(0x80, StringTable::GetString("NO_GOLD"), {ToWideString(price - have)}).c_str());
        return;
    }
    // UNVERIFIED (milestone 5): SoundsManager::PlaySound("ui_buy_with_crystals").
    int crystals = g_crystalAmounts[line][0] + g_crystalAmounts[line][1];
    GameState::ChangeResourceAmount(GameState::kGold, -price);
    GameState::AddCrystals(crystals);
    Received(true, crystals);
}

// @0x2ad0f4
void OnBuy(unsigned line) {
    if (g_tab == 0) OnPayForGold(line);
    else if (g_tab == 1) OnPayForCrystals(line);
}

// @0x2abcbc
bool Click(int x, int y, bool pressed) {
    bool visible = g_root->visibleSelf;
    g_root->Click(x, y, pressed, false);
    return visible;
}

// @0x2abd08
void SetZ(float z) {
    if (!g_queue->shown) return;
    g_root->SetZ(z);
    g_root->SetPosition(g_tween->GetCenteredX(), g_tween->GetCenteredY());
}

void OnTweenIn() {}   // @0x2ac3b4 UNVERIFIED (milestone 5): PlaySound("ui_money_win_opened")

// @0x2ac380
void OnTweenOut() {
    WindowManager::WindowHide(false);
    g_queue->shown = false;
    g_queue->MoveWindowDown(true);
    GameState::DropGamePauseState();
}

// @0x2af0bc: the window refreshes once a second (the discount timer).
void Update(float) {
    uint32_t now = Timer::GetGlobalTime();
    if (now == g_lastSecond) return;
    g_lastSecond = now;
    UpdateContents();
}

// @0x2af1c4
bool OnBack() {
    if (!g_queue->shown) return false;
    Hide();
    return true;
}

}  // namespace

WindowManager::FunctionalWindow* Queue() {
    if (!g_queue) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.deinit = Deinit;
        f.click = Click;
        f.setZ = SetZ;
        f.hide = Hide;
        g_queue = new WindowManager::FunctionalWindow("ExchangeWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_queue->shown; }   // @0x2abd94

// @0x2af8bc
void Init() {
    g_tabSet = false;
    g_tablet = GUI::IsTabletVersion();
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Payment_screen.xml", "Payment_screen.png", -1.f, 0, 0,
                             0, 0, false, 1.f);
    g_queue->RegisterTopWindow(g_root);
    g_scale = g_root->scale;
    g_root->SetVisibility(false);
    g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::GetVerticalCenter(g_root->h));
    g_tween = GUI::CreateTweenEffect(g_root, nullptr, OnTweenIn, OnTweenOut);
    g_close = GUI::GetWindowTyped<Button>(g_root, "x_button.clickArea");
    g_close->SetOnClick(Hide);
    g_close->SetActionSound("ui_close", 1);
    g_golden = GUI::GetWindowTyped<Window>(g_root, "golden_border_box");
    g_tween->CenterWith(g_golden);
    g_tween->insetB = 0;
    g_tween->insetT = (int)(g_scale * -40.f);
    for (unsigned i = 0; i < 2; ++i) {
        char name[32];
        std::snprintf(name, sizeof name, "tab_payment_%02u", i + 1);
        g_tabs[i].LoadFrom(g_root, name);
        g_tabs[i].click->SetOnClick([i] { OnTab(i, nullptr); });
        g_tabs[i].click->SetActionSound("ui_tab", 1);
    }
    // GemHolder::LoadFrom @0x2af22c: payment_gem_holder, its icon_gem_60 and text_time_left.
    g_gemHolder = GUI::GetWindowTyped<Window>(g_root, "payment_gem_holder");
    GUI::GetWindowTyped<Window>(g_root, "payment_gem_holder.icon_gem_60")->takesZ = true;
    g_header = GUI::GetWindowTyped<Textfield>(g_root, "task__buy_more");
    g_daily.LoadFrom(g_root);
    for (unsigned i = 0; i < 6; ++i) {
        char name[64];
        std::snprintf(name, sizeof name, "buttons_get_crystals.button_buy_currency_%02u", i + 1);
        g_lines[i].LoadFrom(g_root, name, g_scale);
        g_lines[i].click->SetOnClick([i] { OnBuy(i); });
        // UNVERIFIED (milestone 5): the action sound is the string at 0x5afb5c.
        g_chestInfo[i] = GUI::GetWindowTypedF<Window>(g_root, "buttons_get_crystals.button_chest_info_%02u", i + 1);
    }
    UpdateCosts();
    g_queue->hideOnOuterClick = true;
    g_queue->fn.update = Update;
    g_queue->fn.back = OnBack;
    g_queue->fn.show = Show;
}

// @0x2af1ec
void Deinit() {
    GUI::RemoveTweenEffect(g_tween);
    g_tween = nullptr;
    delete g_root;
    g_root = nullptr;
}

// @0x2af0ec
void Show() {
    g_queue->shown = true;
    g_queue->MoveWindowOnTop(true);
    if (!g_root->visibleSelf) {
        WindowManager::WindowShow(false);
        g_tween->AnimateIn();
    }
    g_root->SetVisibility(true);
    UpdateContents();
    // OnReceivedData @0x2abda8 is empty.
    if (GameState::GetCurrentLocation() == 2) GameState::RaiseGamePauseState();
    g_root->PreloadTextures();   // GUI::PreloadWindow
}

// @0x2af198
void Hide() {
    if (!g_root->visibleSelf) return;
    g_tween->AnimateOut();
}

// @0x2aed8c: the tab, and the header: the given text, or the tab's name.
void OnTab(unsigned tab, const char32_t* text) {
    if (!text && g_tab == tab && g_tabSet) return;
    g_tab = tab;
    g_tabSet = true;
    g_header->SetVisibility(true);
    if (!text) {
        const char* name = g_tab == 0 ? "PAYMENT_CONVERT_GD_MOBILE" : "PAYMENT_CONVERT_CB_MOBILE";
        g_header->SetText(SWPrintf(0x100, U"%s\n%s",
                                   {StringTable::GetString(name), StringTable::GetString("ADD_MONEY_DESC")})
                              .c_str());
    } else {
        g_header->SetText(text);
    }
    UpdateContents();
}

}  // namespace ExchangeWindow
