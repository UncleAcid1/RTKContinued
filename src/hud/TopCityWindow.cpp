// TopCityWindow: the resource bar at the top centre (gold, crystals, resources, population).
// Port of TopCityWindow (libkingdom.so 5.11), 0x364c88..0x366190.
#include <string>

#include "engine/Render.h"
#include "game/GameState.h"
#include "game/Map.h"
#include "game/StringTable.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/ContentScroller.h"
#include "hud/HUD.h"

namespace TopCityWindow {
namespace {

using GUI::Textfield;
using GUI::Window;

struct ResourceBarGold {          // LoadFrom @0x365b88
    Window* holder = nullptr;     // "%s"
    Window* buyHanger = nullptr;  // "%s.res_bar_buy_hanger"
    Textfield* buyText = nullptr; // "%s.text_buy_large"
    Window* goldBg = nullptr;     // "%s.hud_resbar_gold"
    Textfield* gold = nullptr;    // "%s.text_gold_large"
    Textfield* crystals = nullptr;// "%s.text_crystals_large"
};
struct ResourceSlot { Window* icon; Window* iconInactive; Textfield* text; Textfield* textLimit; int unk10; };
struct ResourceBarResources {     // LoadFrom @0x365ca4 (statics at 0x630668)
    Window* holder = nullptr;     // +0x00
    ResourceSlot slot[8] = {};    // +0x04
    Shared::ContentScroller scroller;   // +0xa4 two pages of four resources
};
struct ResourceBarPeople { Window* holder; Textfield* people; Textfield* peopleLimit; };   // @0x365c40

// Resource names in the layout (tables at 0x6018b8: icon names, then text names)
const char* kIconNames[8] = {"lumber", "stone", "food", "planks", "cut_stone", "meat", "sausage", "oil"};
const char* kTextNames[8] = {"lumber", "rocks", "food", "planks", "stones", "meat", "sausage", "oil"};

WindowManager::FunctionalWindow* g_queue = nullptr;
Window* g_root = nullptr;
GUI::MovementEffect* g_effect = nullptr;
ResourceBarGold g_gold;
ResourceBarResources g_res;
ResourceBarPeople g_people;
int g_offsetX = 25, g_offsetY = -2;   // 0x60f42c / 0x60f430 (15 / 2 on small screens)
bool g_tablet = false;                // 0x60f434
bool g_lastPlayerCity = false;
bool g_valuesValid = false;
uint32_t g_values[10] = {};           // resource amounts read when the city changes
int g_shown[10] = {};                 // counters animated towards the amounts
float g_timer = 1.f;                  // 0x60f428
bool g_buyTextPending = true;         // sets "PERFORM_BUY" on the first update

std::u32string ToWide(long long v) {
    std::string s = std::to_string(v);
    return std::u32string(s.begin(), s.end());
}

// @0x365908. The leftmost 30 pixels of the bar never take clicks; while a dialog is open over the
// bar (bar at depth 0.01) its lowest 40 pixels do not either. UNVERIFIED (later milestones): with
// the shop or screenshot window open or BuildingMovement/BuildingPlacement active the bar only
// absorbs clicks over it, and a visible tutorial arrow takes the click first.
bool Click(int x, int y, bool pressed) {
    if (!g_root) return false;
    if (g_root->visibleSelf) {
        // UNVERIFIED: a visible tutorial arrow (BuildingHovers::ArrowVisible) takes the click first.
        if (g_res.scroller.Click(x, y, pressed, g_root)) return true;
    }
    bool shrunk = false;
    if (g_root->z == 0.01f && WindowManager::GetShownWindowCount() != 0) {
        g_root->h -= 0x28;
        shrunk = true;
    }
    bool r = false;
    if (x >= g_root->x + 0x1e) r = g_root->Click(x, y, pressed, false);
    if (shrunk) g_root->h += 0x28;
    return r;
}

// @0x3658ec
void Move(int x, int y) { g_res.scroller.Move(x, y); }

// @0x364c9c: a tap on the resources scrolls to the other page.
void OnResources() {
    Shared::ContentScroller& s = g_res.scroller;
    if (s.offset < -0x8b) s.bounce += (float)(s.itemSize + 0x19);
    else s.bounce += (float)g_res.slot[0].icon->w * -3.f;
}

}  // namespace

// The FunctionalWindow is a static object in the original (_INIT_ 0x364d40)
WindowManager::FunctionalWindow* Queue() {
    if (!g_queue) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.setZ = SetZ;
        f.hide = Hide;
        f.click = Click;
        f.move = Move;
        g_queue = new WindowManager::FunctionalWindow("TopCityWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return Queue()->shown; }   // @0x364c88

// @0x365eb0
void Init() {
    if (GUI::IsSmallScreenVersion()) {
        g_offsetY = 2;
        g_offsetX = 0xf;
    }
    g_tablet = GUI::IsTabletVersion();
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Resource_bar_large.xml", "Resource_bar_large.png",
                             GUI::GetHudScaleFactor(), 0, 0, 0, 0, false, 1.f);
    if (!g_root) return;
    Queue()->RegisterTopWindow(g_root);
    g_root->SetVisibility(false);
    g_root->SetPosition(-g_root->w, 0);

    const char* p = "hud_button_resbar_gold";
    g_gold.holder = GUI::GetWindowTypedF<Window>(g_root, "%s", p);
    g_gold.buyHanger = GUI::GetWindowTypedF<Window>(g_root, "%s.res_bar_buy_hanger", p);
    g_gold.buyText = GUI::GetWindowTypedF<Textfield>(g_root, "%s.text_buy_large", p);
    g_gold.goldBg = GUI::GetWindowTypedF<Window>(g_root, "%s.hud_resbar_gold", p);
    g_gold.gold = GUI::GetWindowTypedF<Textfield>(g_root, "%s.text_gold_large", p);
    g_gold.crystals = GUI::GetWindowTypedF<Textfield>(g_root, "%s.text_crystals_large", p);
    // OnCB (exchange window) on both, with the "ui_click" sound. UNVERIFIED: windows not ported.
    g_gold.goldBg->SetOnClick([] {});
    g_gold.goldBg->SetActionSound("ui_click", 1);
    g_gold.buyHanger->SetOnClick([] {});
    g_gold.buyHanger->SetActionSound("ui_click", 1);

    Window* mask = GUI::GetWindow(g_root, "mask_resources");
    if (!mask) mask = GUI::GetWindowTyped<Window>(g_root, "mask_resources");
    const char* r = "hud_res_holder";
    g_res.holder = GUI::GetWindowTypedF<Window>(g_root, "%s", r);
    for (int i = 0; i < 8; ++i) {
        g_res.slot[i].icon = GUI::GetWindowTypedF<Window>(g_root, "%s.res_bar_%s", r, kIconNames[i]);
        g_res.slot[i].iconInactive = GUI::GetWindowTypedF<Window>(g_root, "%s.res_bar_%s_inactive", r, kIconNames[i]);
        g_res.slot[i].text = GUI::GetWindowTypedF<Textfield>(g_root, "%s.text_%s", r, kTextNames[i]);
        g_res.slot[i].textLimit = GUI::GetWindowTypedF<Textfield>(g_root, "%s.text_%s_limit", r, kTextNames[i]);
    }
    // The holder is clipped to the mask rectangle (the scroller's viewport) and scrolls inside it,
    // two pages wide, without recycling.
    Shared::ContentScroller& sc = g_res.scroller;
    sc.Init();
    sc.track = mask;
    sc.viewport = {mask->x, mask->y, mask->w + mask->x, mask->h + mask->y};
    g_res.holder->SetClipRect(&sc.viewport);
    sc.windows.push_back(g_res.holder);
    sc.noRecycle = true;
    sc.itemCount = 2;
    sc.horizontal = true;
    sc.itemSize = (g_res.slot[7].icon->x + 0x14) - g_res.slot[0].icon->x;
    sc.unkA0 = 2;
    sc.perLine = 1;
    g_res.holder->SetOnClick(OnResources);

    const char* q = "hud_resbar_population";
    g_people.holder = GUI::GetWindowTypedF<Window>(g_root, "%s", q);
    g_people.people = GUI::GetWindowTypedF<Textfield>(g_root, "%s.text_people", q);
    g_people.peopleLimit = GUI::GetWindowTypedF<Textfield>(g_root, "%s.text_people_limit", q);

    g_effect = GUI::CreateMovementEffect(g_root, nullptr, nullptr, [] {   // OnTweenOut @0x364cfc
        Queue()->shown = false;
        g_root->SetVisibility(false);
    });
    Queue()->fn.update = Update;
    Queue()->fn.show = Show;
    Queue()->fn.zRange = [] { return WindowManager::ReturnSmallZRange(); };
    g_effect->insetR -= 0x7d;
}

// @0x365650: slide down from above the screen
void Show() {
    if (!GameState::IsPlayerCity() || GameState::TutorialStep() <= 0x11 || !g_root) return;
    if (g_effect->active && g_effect->hideAtEnd) g_root->SetVisibility(false);
    Queue()->shown = true;
    Queue()->MoveWindowOnTop(true);
    if (!g_root->visibleSelf) {
        int x = g_effect->GetCenteredX() + g_offsetX;
        g_effect->Animate(x, -g_root->h, g_effect->GetCenteredX() + g_offsetX, g_offsetY, true);
    }
    g_root->SetVisibility(true);
}

// @0x36558c: slide back up
void Hide() {
    if (!g_root) return;
    if (g_effect->active && g_effect->hideAtEnd) return;
    if (!g_root->visibleSelf) return;
    int x = g_effect->GetCenteredX() + g_offsetX;
    g_effect->Animate(x, g_offsetY, g_effect->GetCenteredX() + g_offsetX, -g_root->h, false);
}

// @0x3657a4. UNVERIFIED: the dialog-dependent depths (0.01 while the shop, crafting, exchange ...
// windows are open) need those windows; the HUD depth is used.
void SetZ(float) {
    if (!g_root) return;
    g_root->SetZ(HUDWindow::GetZ());
    if (g_root->visibleSelf) g_root->SetPosition(g_effect->GetCenteredX() + g_offsetX, g_offsetY);
}

// @0x364f28
void Update(float dt) {
    if (!g_root) return;
    bool city = GameState::IsPlayerCity();
    if (g_lastPlayerCity != city) {
        g_valuesValid = false;
        g_lastPlayerCity = city;
    }
    if (!g_valuesValid) {
        for (int i = 0; i < 10; ++i) g_values[i] = GameState::GetResourceAmount(i);
    }
    g_valuesValid = true;
    if (Queue()->shown) g_res.scroller.Update(dt);
    g_timer += dt;
    if (1.f / 30.f < g_timer) {
        g_timer = 0.f;
        for (int i = 0; i < 10; ++i)
            g_shown[i] = HUDWindow::UpdateNumberToTarget(g_shown[i], (int)GameState::GetResourceAmount(i));
        if (g_root->visibleSelf) {
            g_gold.gold->SetText(ToWide(g_shown[GameState::kGold]).c_str());
            g_gold.crystals->SetText(ToWide(g_shown[GameState::kCrystal]).c_str());
            for (int i = 0; i < 8; ++i) {
                ResourceSlot& s = g_res.slot[i];
                s.text->SetText(ToWide(g_shown[i]).c_str());
                s.icon->SetVisibility(g_shown[i] != 0);
                s.iconInactive->SetVisibility(g_shown[i] == 0);
                // UNVERIFIED: Map::GetTotalStorageLimit (milestone 3); 1000 below tutorial step 0x18.
                int limit = 1000;
                s.textLimit->SetVisibility(limit <= g_shown[i]);
                s.textLimit->SetText(ToWide(g_shown[i]).c_str());
            }
            int workers = GameState::GetPlayerWorkersCount() + Map::GetPendingWorkerCount();
            int maxWorkers = GameState::GetMaxWorkerCount();
            std::string t = std::to_string(workers) + "/" + std::to_string(maxWorkers);   // L"%d/%d"
            std::u32string w(t.begin(), t.end());
            g_people.people->SetVisibility((unsigned)workers < (unsigned)maxWorkers);
            g_people.people->SetText(w.c_str());
            g_people.peopleLimit->SetVisibility((unsigned)maxWorkers <= (unsigned)workers);
            g_people.peopleLimit->SetText(w.c_str());
            g_root->UpdatePosition();
        }
    }
    bool buyVisible = !(g_root->z == 0.01f && WindowManager::GetShownWindowCount() != 0);
    g_gold.buyText->SetVisibility(buyVisible);
    g_gold.buyHanger->SetVisibility(buyVisible);
    if (g_buyTextPending) {
        g_buyTextPending = false;
        g_gold.buyText->SetText(StringTable::GetString("PERFORM_BUY"));
    }
}

}  // namespace TopCityWindow
