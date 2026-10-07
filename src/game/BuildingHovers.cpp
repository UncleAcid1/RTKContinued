#include "game/BuildingHovers.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "engine/IconManager.h"
#include "engine/Render.h"
#include "engine/Text.h"
#include "engine/Timer.h"
#include "game/Building.h"
#include "game/BuildingMovement.h"
#include "windows/Windows.h"
#include "game/BuildingPlacement.h"
#include "game/AIState.h"
#include "game/Contracts.h"
#include "game/Entity.h"
#include "game/EntityData.h"
#include "game/EntityManager.h"
#include "game/GameData.h"
#include "game/GameState.h"
#include "game/Map.h"
#include "game/Rand48.h"
#include "game/StringTable.h"
#include "gui/GUI.h"
#include "gui/TextStyleManager.h"
#include "gui/WindowManager.h"
#include "hud/HUD.h"
#include "hud/HoverWindows.h"

namespace BuildingHovers {

namespace {

// A dropped pickup (0x64 bytes): it falls and bounces, then waits to be tapped; after `lifetime`
// seconds it collects itself.
struct ItemDrop {
    int kind = 0;                     // +0x00 0 resource, 1 item, 2 farm food
    const void* item = nullptr;       // +0x04 ItemInfo (items: not ported yet)
    int type = 0;                     // +0x08 resource type
    unsigned amount = 0;              // +0x0c
    Render::Sprite* sprite = nullptr; // +0x10
    Render::Sprite* glow = nullptr;   // +0x14 (items)
    int subtasks[8] = {};             // +0x18 Tasks::CompleteSubtask(0xd, n) on collect
    bool bonus = false;               // +0x38
    bool animating = false;           // +0x39
    float x = 0, y = 0;               // +0x3c +0x40 sprite position (bottom-left)
    float vx = 0;                     // +0x44
    float groundY = 0;                // +0x48 where it landed first
    float vy = 0;                     // +0x4c
    float delay = 0;                  // +0x50 until it appears
    float t = 0;                      // +0x54 time since it appeared
    float glowT = 0;                  // +0x58
    float lifetime = 10.f;            // +0x5c
    int bounce = 100;                 // +0x60 how far below groundY it may fall
    void Animate(float dt);           // @0x263d30
    void Collect(bool automatic, bool showWindow);   // @0x26d218
};

// A text popup (0x10 bytes).
struct TextInfo {
    Render::Sprite* sprite = nullptr; // +0x00
    float fade = 0;                   // +0x04 seconds to fade out
    float rise = 0;                   // +0x08 pixels per second
    bool grow = false;                // +0x0c shrink towards the bottom-right instead
};

// A sprite flying from the world to the HUD (0x30 bytes).
struct ItemMove {
    Render::Sprite* sprite = nullptr; // +0x00
    float startW = 0, startH = 0;     // +0x04 +0x08
    float startX = 0, startY = 0;     // +0x0c +0x10
    float endW = 0, endH = 0;         // +0x14 +0x18
    float endX = 0, endY = 0;         // +0x1c +0x20
    float t = 0, duration = 0;        // +0x24 +0x28
    bool sound = false;               // +0x2c "building_position" on arrival
    bool fadeOut = false;             // +0x2d fades in the last quarter second
};

WindowManager::FunctionalWindow* g_wnd = nullptr;   // BuildingHovers::wnd
std::vector<HoverInfo*> g_hoverData;   // BuildingHovers::hoverData (sgl::vector, swap-removed)
std::vector<TextInfo> g_itemInfo;      // BuildingHovers::itemInfo
std::vector<ItemMove> g_itemMoves;     // BuildingHovers::itemMoves
std::vector<ItemDrop> g_itemDrops;     // BuildingHovers::itemDrops
BuildingHoverWindow* g_currentHover = nullptr;   // 0x618980 the tapped building's info window
// The info windows (Init), shown one at a time as g_currentHover.
FactoryHoverWindow* g_factoryWindow = nullptr;            // BuildingHovers::factoryWindow
BuildProgressHoverWindow* g_progressWindow = nullptr;     // BuildingHovers::progressWindow (screen space)
FarmRestoreWindow* g_farmRestoreWindow = nullptr;         // BuildingHovers::farmRestoreWindow
LivingHoverWindow* g_livingWindow = nullptr;              // BuildingHovers::livingWindow
StorageHoverWindow* g_storageWindow = nullptr;            // BuildingHovers::storageWindow
EmptyHoverWindow* g_emptyWindow = nullptr;                // BuildingHovers::emptyWindow
CastleHoverWindow* g_castleWindow = nullptr;              // BuildingHovers::castleWindow
ResourceHoverWindow* g_resourceActiveWindow = nullptr;    // BuildingHovers::resourceActiveWindow
DecorationHoverWindow* g_decorationWindow = nullptr;      // BuildingHovers::decorationWindow
bool g_skipHoverCheck = false;         // BuildingHovers::skipHoverCheck
int g_lastID = 0;                      // 0x618998
bool g_hoversVisible = true;           // 0x60f138
uint32_t g_lastTime = 0;               // 0x6189dc
bool g_updatingDrops = false;          // 0x6188e1
int g_textCounter = 0;                 // 0x6188d8 (ShowTextHover)
int g_styleCounter = 0;                // 0x6188dc (ShowTextHoverWithStyle)

// HelperArrow (0x30 bytes): the bobbing "Arrow_show" pointer (tutorial steps, building placement).
// arrows[0] always exists (Init); ArrowAt(..., newArrow) adds more, HideArrow drops them again.
// UNVERIFIED (tutorial): the tablet attention rings (+0x18, +0x1c, +0x20, +0x24) and the arrow
// following an entity (+0x28, UpdateArrow) are not ported.
struct HelperArrow {
    Render::Sprite* sprite = nullptr;  // +0x00
    bool left = false;                 // +0x04 points left (Arrow_show_left), else down
    bool mirror = false, flip = false; // +0x05 +0x06
    float x = 0.f, y = 0.f;            // +0x08 +0x0c the anchor, world space unless screenSpace
    GUI::Callback* onClick = nullptr;  // +0x10 SetArrowClickCallback (owned)
    ~HelperArrow() { delete onClick; }
};
std::vector<HelperArrow*> g_arrows;    // BuildingHovers::arrows
bool g_canHideArrow = true;            // BuildingHovers::canHideArrow
int g_arrowCallbackID = 1;             // BuildingHovers::arrowCallbackID
unsigned g_arrowVisibleWindowLimit = 0;   // BuildingHovers::arrowVisibleWindowLimit

HoverInfo* NewHover() {
    auto* h = new HoverInfo();
    g_hoverData.push_back(h);
    h->id = g_lastID++;
    return h;
}

// Where a hover window sits: a building's top centre, a decoration's top centre (its tile without
// a sprite), an entity's head.
void PositionWindow(HoverInfo* h) {
    BuildingHoverWindow* w = h->window;
    if (h->building) {
        w->SetPosition((int)((h->building->minX + h->building->maxX) * 0.5f), (int)h->building->minY);
    } else if (h->decor) {
        if (Render::Sprite* s = h->decor->sprite) {
            w->SetPosition((int)(s->x + s->w * 0.5f), (int)(s->y - s->h));
        } else {
            int x = h->decor->x, y = h->decor->y;
            Map::TileCoordinatesToWorld(x, y);
            w->SetPosition(x, y);
        }
    } else if (h->entity) {
        // Over the entity's head; talk and friend windows take the entity again.
        float wx = 0.f, wy = 0.f;
        h->entity->GetWorldPos(wx, wy);
        w->SetPosition((int)wx, (int)((wy - h->entity->GetIdleHeight()) - 5.f));
        if (h->type == kTalk || h->type == kFriendInfo) w->SetEntity(h->entity);
    }
}

void RemoveHover(size_t i) {
    HoverInfo* h = g_hoverData[i];
    if (h == (HoverInfo*)nullptr) return;
    if (h->window) h->window->RemoveWindow();
    delete h;
    g_hoverData[i] = g_hoverData.back();
    g_hoverData.pop_back();
}

}  // namespace

void HoverInfo::SetHoverType(int t) {
    if (type == t) {
        if (window) PositionWindow(this);
        return;
    }
    if (t == kNone && window) {
        window->RemoveWindow();
        type = kNone;
        window = nullptr;
        return;
    }
    if (window) window->RemoveWindow();
    type = t;
    window = nullptr;
    BuildingHoverWindow* w = nullptr;
    if (t == kAssignBuilder || t == kContractFinished || t == kCutResource || t == kHireTroops ||
        t == kFarmWater || t == kFarmReady || t == kTraining)
        w = new BubbleHoverWindow();
    else if (t == kBuildProgress)
        w = new BuildProgressHoverWindow(t == kAssignBuilder || t == kContractFinished);
    else if (t == kResourceRestore)
        w = new ResourceRestoreHoverWindow();
    else if (t == kTaxes)
        w = new TaxesHoverWindow();
    // UNVERIFIED (not ported yet): TalkHoverWindow (0xc), HealthbarHoverWindow (0xd), HealthbarTinyHoverWindow (0xe),
    // UseItemHoverWindow (0xf), BossTimeHoverWindow (0x10) come before the sleeping window;
    // PlayerNameHoverWindow (0x11), FriendInfoHoverWindow (0x13) after it.
    else if (t == kFarmSleeping || t == kDelivery)
        w = new SleepingHoverWindow();
    else
        return;
    window = w;
    w->Init();
    w->Show();
    w->SetZ(0.3f);
    w->SetBuilding(building);
    w->SetDecoration(decor);
    w->SetEntity(entity);
    if (building) building->FindBaseCoordinates();
    if (window) PositionWindow(this);
    Render::SortRenderLayer(Render::kLayerGUI, 1);
}

WindowManager::FunctionalWindow* Queue() {
    if (!g_wnd) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.deinit = Deinit;
        f.click = Click;
        f.setZ = SetZ;
        f.hide = Hide;
        g_wnd = new WindowManager::FunctionalWindow("BuildingHovers", std::move(f));
    }
    return g_wnd;
}

void Init() {
    // UNVERIFIED: wndScale is 1.5 below a 320 px screen height (the arrows' scale).
    g_arrows.push_back(new HelperArrow());
    // UNVERIFIED: PersonHoverWindow (only shown on campaign maps: milestone 4) and the world dialog
    // (WorldHintHoverWindow) are created here too. FarmGrowHoverWindow is created as well but
    // nothing in 5.11 shows it, so it is not ported.
    g_emptyWindow = new EmptyHoverWindow();
    g_storageWindow = new StorageHoverWindow();
    g_resourceActiveWindow = new ResourceHoverWindow();
    g_livingWindow = new LivingHoverWindow();
    g_factoryWindow = new FactoryHoverWindow();
    g_progressWindow = new BuildProgressHoverWindow(true);
    g_castleWindow = new CastleHoverWindow();
    g_farmRestoreWindow = new FarmRestoreWindow();
    g_decorationWindow = new DecorationHoverWindow();
    // UNVERIFIED (world dialog): the queue's back function becomes BuildingHovers::OnBack.
}

// ---- the helper arrow ----

bool ArrowVisible() {
    for (HelperArrow* a : g_arrows)
        if (a->sprite && a->sprite->visible) return true;
    return false;
}

bool CanAutoHideArrow() { return g_canHideArrow; }

int GetArrowClickCallbackID() { return g_arrowCallbackID; }

void SetArrowVisibleWindowLimit(unsigned limit) { g_arrowVisibleWindowLimit = limit; }

int SetArrowClickCallback(GUI::Callback* cb) {
    HelperArrow* a = g_arrows.back();
    delete a->onClick;
    a->onClick = cb;
    ++g_arrowCallbackID;
    std::printf("Arrow callbacks created: %d\n", g_arrowCallbackID);
    return g_arrowCallbackID;
}

bool ClickOnArrow(int x, int y, bool pressed) {
    for (HelperArrow* a : g_arrows) {
        if (!a->onClick) {
            if (!a->sprite) continue;
            Render::SetVisibility(a->sprite, false);   // an arrow without a callback goes away
        }
        Render::Sprite* s = a->sprite;
        if (!s || !s->visible) continue;
        int px = x, py = y;
        if (!s->screenSpace) Map::MouseCoordinatesToWorld(px, py);
        if ((float)px < s->x || !((float)px < s->x + s->w)) continue;
        if ((float)py < s->y - s->h || !((float)py < s->y)) continue;
        if (pressed) return true;
        // SoundsManager::PlaySound("ui_arrow_click", 1, false): sounds are milestone 5.
        for (HelperArrow* o : g_arrows)
            if (o && o->sprite) Render::SetVisibility(o->sprite, false);
        (*a->onClick)();
        ++g_arrowCallbackID;
        return true;
    }
    return false;
}

void HideArrow() {
    for (size_t i = 1; i < g_arrows.size(); ++i) {
        if (!g_arrows[i]) continue;
        Render::RemoveSprite(g_arrows[i]->sprite);
        delete g_arrows[i];
    }
    g_arrows.resize(1);
    HelperArrow* a = g_arrows[0];
    if (a->sprite) Render::SetVisibility(a->sprite, false);
    ++g_arrowCallbackID;
}

// The arrows bob 8 px at 15 rad/s: down-pointing ones vertically, left-pointing ones sideways.
void UpdateArrow() {
    for (HelperArrow* a : g_arrows) {
        Render::Sprite* s = a->sprite;
        if (!s || !s->visible) continue;
        double bob = std::cos(Timer::GetTime() * 15.0) * 8.0;
        if (!a->left) Render::SetPosition(s, a->x, (float)((double)a->y + bob), s->z);
        else Render::SetPosition(s, (float)((double)a->x + bob), a->y, s->z);
    }
}

void ArrowAt(float x, float y, bool hide, bool left, bool mirror, bool flip, bool screenSpace, bool tablet,
             bool newArrow) {
    if (hide) {
        HideArrow();
        return;
    }
    if (newArrow) g_arrows.push_back(new HelperArrow());
    HelperArrow* a = g_arrows.back();
    if (!a->sprite) a->sprite = Render::CreateSprite(IconManager::GetIcon("Arrow_show"), Render::kLayer15, false, false);
    Render::Sprite* s = a->sprite;
    Render::SetTexture(s, IconManager::GetIcon(left ? "Arrow_show_left" : "Arrow_show"));
    Render::SetMirror(s, mirror, flip);
    float w = (float)s->tex->w, h = (float)s->tex->h;
    if (screenSpace) {
        s->w = w * GUI::GetHoverScaleFactor(1.f, 1.f);
        s->h = h * GUI::GetHoverScaleFactor(1.f, 1.f);
    } else {
        s->w = w;
        s->h = h * 1.f;
    }
    a->left = left;
    a->mirror = mirror;
    a->flip = flip;
    a->x = x;
    a->y = y;
    if (left) {
        if (mirror) a->x = x - s->w;
        a->y = y + s->h * 0.5f;
    } else {
        if (flip) a->y = s->h + y;
        a->x = x + s->w * -0.5f;
    }
    s->screenSpace = screenSpace;
    Render::SetVisibility(s, true);
    // UNVERIFIED (tutorial): the tablet attention rings (tablet, or second tutorial 0x86/0x87 on a
    // tablet) and, after the tutorial (0x100) in a combat with more than two sides, centring the
    // camera on an arrow off screen.
    g_canHideArrow = true;
    UpdateArrow();
}

void Deinit() {
    if (g_currentHover) {
        g_currentHover->Hide();
        g_currentHover = nullptr;
    }
    for (TextInfo& ti : g_itemInfo) Render::RemoveSprite(ti.sprite);
    g_itemInfo.clear();
    for (ItemMove& m : g_itemMoves) Render::RemoveSprite(m.sprite);
    g_itemMoves.clear();
    for (ItemDrop& d : g_itemDrops) Render::RemoveSprite(d.sprite);
    g_itemDrops.clear();
}

void Show() {
    if (!GameState::IsPlayerCity()) return;
    if (g_currentHover) {
        if (g_currentHover->shownHover) Hide();
        if (g_currentHover) g_currentHover->Show();
    }
    Update(0.0, true);
}

void Hide() {
    if (!g_currentHover || !g_currentHover->shownHover) return;
    WindowManager::WindowHide(false);
    g_wnd->shown = false;
    g_currentHover->Hide();
    if (WindowManager::GetShownWindowCount() == 0 && !IsWorldDialogVisible()) HUDWindow::Show();
    g_currentHover = nullptr;
}

bool IsHoverVisible() { return g_currentHover && g_currentHover->shownHover; }

bool SetHoverWindowPosition(void* w, int x, int y) {
    if (g_currentHover != w) return false;
    Map::WorldCoordinatesToScreen(x, y);
    g_currentHover->SetPosition(x, y);
    return true;
}

void ShowFarmHover(bool fake, unsigned patch, unsigned count) {
    if (GameState::tutorial == 0x42) {
        GameState::tutorial = 0x43;
        // UNVERIFIED (tutorial): HideWorldDialog(nullptr) (the world dialog is not ported).
        HideArrow();
    }
    Map::Building* farm = Map::GetCurrentFarm();
    if (!farm) return;
    if (g_currentHover) {
        WindowManager::WindowHide(false);
        g_wnd->shown = false;
        g_currentHover->Hide();
        g_currentHover = nullptr;
    }
    const int p = (int)patch;
    if (fake) {
        g_currentHover = g_progressWindow;
        g_progressWindow->FakeSpeedup();
    } else if (patch == 0xffffffffu) {
        g_currentHover = g_factoryWindow;
        g_factoryWindow->StartMultipleFarmContracts(count);
    } else if (farm->GetFarmState(p) == 1) {
        g_currentHover = g_farmRestoreWindow;
    } else if (farm->GetFarmState(p) == 6 || farm->patchEntities[patch]->GetAI()->GetState() == 0x13) {
        // A ripe or dirty patch: the farmer goes there; the progress box follows the work.
        g_currentHover = g_progressWindow;
        if (GameState::tutorial == 0x47) {
            GUI::SetInteractionObjectLock(nullptr, nullptr);
            GUI::SetInteractionLock(true);
            HideArrow();
            GameState::tutorial = 0x4a;
        }
        farm->farmer->GetAI()->Farm(0, p, false);
    } else if (farm->GetFarmState(p) != 0 && farm->patchEntities[patch]->GetAI()->GetState() == 0x12) {
        g_currentHover = g_progressWindow;
    } else if (farm->GetFarmState(p) == 0) {
        g_currentHover = g_factoryWindow;
        if (count > 1) g_factoryWindow->StartMultipleFarmContracts(count);
    } else {
        g_currentHover = g_factoryWindow;
    }
    BuildingHoverWindow* w = g_currentHover;
    if (!w) return;
    if (patch == 0xffffffffu) {
        w->SetEntity(nullptr);
        g_currentHover->SetBuilding(farm);
    } else {
        if (w == g_factoryWindow) w->SetEntity(farm->patchEntities[patch]);
        farm->lastContract = 0;
        g_currentHover->SetBuilding(farm);
        g_currentHover->SetEntity(farm->patchEntities[patch]);
    }
    w = g_currentHover;
    if (w == g_factoryWindow || w == g_farmRestoreWindow) {
        w->SetPosition(GUI::ScreenWidth() / 2, GUI::ScreenHeight() - 100);
    } else {
        Entity* f = farm->farmer;
        int sx = (int)f->worldX, sy = (int)(f->worldY - f->GetSprite()->h);
        Map::WorldCoordinatesToScreen(sx, sy);
        w->SetPosition(sx, sy);
        f->SetHoverWindowPositionHandling(w);
    }
    g_currentHover->Show();
    g_currentHover->MoveWindowOnTop(true);
    g_wnd->shown = true;
    WindowManager::WindowShow(false);
}

void ConvertDroppedFoodToResource(Map::Building* b) {
    unsigned amounts[8] = {};
    std::vector<unsigned> subtasks;
    for (size_t i = 0; i < g_itemDrops.size();) {
        ItemDrop& d = g_itemDrops[i];
        if (d.kind != 2) {
            ++i;
            continue;
        }
        amounts[d.type] += d.amount;
        for (int s : d.subtasks)
            if (s != 0) subtasks.push_back((unsigned)s);
        d.sprite = Render::RemoveSprite(d.sprite);
        d.glow = Render::RemoveSprite(d.glow);
        d = g_itemDrops.back();   // the last one takes its place (and is looked at next)
        g_itemDrops.pop_back();
    }
    for (int r = 0; r < 8; ++r)
        if (amounts[r] != 0) DropResource(b->baseX, b->minY, r, amounts[r], false, false);
    for (unsigned id : subtasks) AddDeliveryContractToFinishOnLastItem(id);
}
void ScheduleUpdate() { g_lastTime = 0; }

// @0x263164: the last drop completes the harvested order's task when collected (its first free
// subtask slot).
void AddDeliveryContractToFinishOnLastItem(unsigned id) {
    if (g_itemDrops.empty()) return;
    for (int& s : g_itemDrops.back().subtasks) {
        if (s == 0) {
            s = (int)id;
            return;
        }
    }
}
bool HasDroppedItems() { return !g_itemDrops.empty(); }

void CollectAll() {
    for (size_t i = 0; i < g_itemDrops.size(); ++i) {
        g_itemDrops[i].Collect(true, false);
        g_itemDrops[i].sprite = Render::RemoveSprite(g_itemDrops[i].sprite);
        g_itemDrops[i].glow = Render::RemoveSprite(g_itemDrops[i].glow);
    }
    g_itemDrops.clear();
}
bool IsItemMoving() { return !g_itemMoves.empty(); }

void SetHoverVisiblity(bool visible, bool) {
    g_hoversVisible = visible;
    for (HoverInfo* h : g_hoverData) {
        if (!h->window) continue;
        if (g_hoversVisible) h->window->Show();
        else h->window->Hide();
    }
}

bool BuildingHasActiveHover(Map::Building* b) {
    for (HoverInfo* h : g_hoverData)
        if (h->building == b && h->window && h->type != kTraining && h->type != kDelivery) return true;
    return false;
}

void ActivateBuildingHover(Map::Building* b) {
    for (HoverInfo* h : g_hoverData)
        if (h->building == b && h->window) h->window->Activate(true);
}

void RegisterBuilding(Map::Building* b) { NewHover()->building = b; }
void RegisterDecoration(Map::Decor* d) { NewHover()->decor = d; }

void SafeRegisterEntity(Entity* e) {
    for (HoverInfo* h : g_hoverData)
        if (h->entity == e) return;
    NewHover()->entity = e;
}

void UnregisterBuilding(Map::Building* b) {
    for (size_t i = 0; i < g_hoverData.size();) {
        if (g_hoverData[i]->building != b) { ++i; continue; }
        RemoveHover(i);
        ++i;   // as the original: the swapped-in record is not checked
    }
    // UNVERIFIED (milestone 3e): the storage, resource, living and factory info windows drop b.
}

void UnregisterDecoration(Map::Decor* d) {
    for (size_t i = 0; i < g_hoverData.size(); ++i) {
        if (g_hoverData[i]->decor != d) continue;
        RemoveHover(i);   // only the first match
        break;
    }
    // UNVERIFIED (tutorial): arrows pointing at d are hidden.
}

void UnregisterEntity(Entity* e) {
    for (size_t i = 0; i < g_hoverData.size();) {
        if (g_hoverData[i]->entity != e) { ++i; continue; }
        RemoveHover(i);
        ++i;
    }
    // UNVERIFIED (milestone 3e / tutorial): the person window, the world dialog and the arrows drop e.
}

// ---- hover types ----

void UpdateHovers() {
    if (!Map::IsLoaded()) return;
    for (size_t i = 0; i < g_hoverData.size(); ++i) {
        HoverInfo* h = g_hoverData[i];
        Map::Building* b = h->building;
        Entity* e = h->entity;
        Map::Decor* d = h->decor;
        if (b && GameState::GetCurrentMapID() == 0 && GameState::IsPlayerCity()) {
            // UNVERIFIED (milestone 4): during a city battle (g_Combat) damaged buildings show the
            // tiny health bar (0xe) instead.
            int step = GameState::TutorialStep();
            if (b->needsBuilder == 0 && b->upgrading == 0) {
            idle:
                unsigned cls = b->data->buildingClass;
                if ((cls == 0 || cls == 9) && b->GetReadyGoldAmount() != 0) {
                    h->SetHoverType(kTaxes);
                } else if (b->IsOpened() && cls == 2 && b->HasActiveContract() && b->contractDone) {
                    h->SetHoverType(kContractFinished);
                } else if (cls == 4) {
                    if (b->resourceState == 2) h->SetHoverType(kCutResource);
                    else if (b->resourceState == 0) h->SetHoverType(kResourceRestore);
                    else h->SetHoverType(kNone);
                } else if (cls == 0xc && b->HasActiveContract() && !b->contractDone) {
                    h->SetHoverType(kBuildProgress);
                } else if (cls == 0xd) {
                    // A ready crop, a rotten first patch, or an idle farm with its farmer asleep.
                    if (b->GetFirstReadySoilPath() != -1) h->SetHoverType(kFarmReady);
                    else if (b->GetFarmState(0) == 1) h->SetHoverType(kFarmWater);
                    else if (!b->HasActiveContract() && !b->livers.empty() && b->livers[0] &&
                             b->livers[0]->IsAppeared())
                        h->SetHoverType(kFarmSleeping);
                    else h->SetHoverType(kNone);
                } else if (cls == 2) {
                    if (b->data->delivery && !b->HasActiveContract()) h->SetHoverType(kDelivery);
                    else h->SetHoverType(kNone);
                } else if (cls == 0xc && !GameState::IsCityTutorial()) {
                    // UNVERIFIED (milestone 4): only with no soldier slot occupied
                    // (SoldierSlots::GetOccupiedSlotCount() == 0); there are no soldiers yet.
                    h->SetHoverType(kTraining);
                } else {
                    h->SetHoverType(kNone);
                }
            } else if (!b->BuilderIsWorking() && step > 0x5c) {
                h->SetHoverType(kAssignBuilder);
            } else if ((b->needsBuilder == 0 && b->upgrading == 0) || (b->id == 0x8e && step < 0x59)) {
                goto idle;
            } else {
                h->SetHoverType(kBuildProgress);
            }
        } else if (GameState::IsPlayerCity()) {
            if (d) {
                // UNVERIFIED (milestone 4): the battle health bar, decorations being searched by a
                // worker (2) and the use-item hover (0xf) need the decoration jobs and items.
                const GameData::DecorData* dd = d->data;
                if (dd && dd->collectTime != 0 && GameState::GetCurrentMapID() == 0 && d->patch->owned &&
                    Timer::GetGlobalTime() > dd->collectTime + d->collectStart) {
                    h->SetHoverType(kTaxes);
                    goto checked;
                }
            }
            if (e && !e->IsDisappearing() && e->IsAppeared()) {
                // UNVERIFIED (milestone 4): talk tasks (0xc), the 0x1d7 special talk, battle health
                // bars (0xe), boss timers (0x10) and player names (0x11) need quests, combat and
                // other players; they are checked first on the original.
                // A ripe soil patch on the farm view shows its crop.
                if (e->GetAI() && e->GetAI()->GetState() == 0x19 && Map::GetCurrentFarm()) {
                    h->SetHoverType(kFarmReady);
                    goto checked;
                }
            }
            h->SetHoverType(kNone);
        } else {
            // UNVERIFIED (milestone 4+): visiting a friend, their goblins (class 0x16) show 0x13.
        }
    checked:
        BuildingHoverWindow* w = g_hoverData[i]->window;
        if (w && !g_hoversVisible && w->IsVisible()) w->Hide();
    }
}

void UpdateHoverPositions() {
    for (HoverInfo* h : g_hoverData) {
        if (!h->window) continue;
        int t = h->type;
        if (t != kTalk && t != kHealthbar && t != kHealthbarTiny && t != kBossTime && t != kPlayerName) continue;
        PositionWindow(h);
    }
}

// ---- drops ----

void ItemDrop::Animate(float dt) {
    if (dt > 0.05f) dt = 0.05f;
    if (glow && delay < 0.f) {
        Render::SetVisibility(glow, true);
        Render::SetPosition(glow, x + glow->w * -0.5f + sprite->w * 0.5f, y + glow->h * 0.5f + sprite->h * -0.5f, 0.3f);
        glow->color[0] = glow->x + glow->w * 0.5f;   // rotation centre
        glow->color[1] = glow->h * 0.5f - glow->y;
        glow->color[2] = glowT / 2.125f;             // angle
        if (glowT < 1.f) Render::SetAlpha(glow, glowT);
        glowT += dt * 4.25f;
        if ((double)glowT > 4.14) Render::SetAlpha(glow, glowT <= 5.14f ? 5.14f - glowT : 0.f);
    }
    if (!animating) return;
    delay -= dt;
    if (delay < 0.f && !sprite->visible) {
        Render::SetVisibility(sprite, true);
        glowT = 0.f;
        t = 0.f;
    }
    if (delay > 0.f) return;
    t += dt;
    if (t > 0.6f && y - groundY > (float)bounce) {
        // UNVERIFIED (tutorial): at second-tutorial step 0x94 the camera centres on the drop.
        animating = false;
    }
    float s = dt * 10.f;
    vy += s * 60.f;
    y += s * vy;
    x += s * vx;
    int sx = (int)x, sy = (int)y;
    if (y - groundY > (float)bounce) vy = -70.f;
    Map::WorldCoordinatesToScreen(sx, sy);
    if (vx > 0.f && (float)sx + sprite->w > (float)(Render::ScreenWidth() - 0x4b)) vx = -vx;
    if (vx < 0.f && sx < 0x4b) vx = -vx;
    if (vx > 0.f && x > (float)(unsigned)Map::GetGridWidth() * 84.f - 84.f) vx = -vx;
    if (vx < 0.f && x < 84.f) vx = -vx;
    if (vy > 0.f && GameState::GetCurrentMapID() != 0 &&
        y > (float)(unsigned)Map::GetGridHeight() * 42.f * 0.5f - 42.f)
        vy = -vy;
    if (vy > 0.f && Map::GetCurrentFarm() && y > 3100.f) vy = -vy;   // the farm ground's edge
    Render::SetPosition(sprite, x, y, 0.2f);
}

void ItemDrop::Collect(bool automatic, bool) {
    // UNVERIFIED (milestone 4, Tasks): each non-zero subtask completes Tasks::CompleteSubtask(0xd, n).
    for (int& s : subtasks) s = 0;
    if (kind == 0) {
        if (type == GameState::kCrystal) GameState::AddCrystals((int)amount);
        else GameState::ChangeResourceAmount(type, (int)amount);
        Render::Sprite* s = nullptr;
        if (!automatic) {
            sprite->w *= GUI::GetHudScaleFactor();
            sprite->h *= GUI::GetHudScaleFactor();
            s = sprite;
        }
        OnCollect(0, s, false, false, type, false);
        // UNVERIFIED (tutorial): step 0x21 hides the arrow and moves to 0x22.
        std::u32string text;
        if (bonus) {
            text = StringTable::GetString("ITEM_BONUS");   // "%s +%%d %%s"
            text += U" +";
        } else {
            text = U"+";
        }
        std::string n = std::to_string(amount);
        text.append(n.begin(), n.end());
        text += U' ';
        if (const char32_t* name = GameState::GetResourceGameName(type)) text += name;
        int style = type == GameState::kGold ? TextStyleManager::kGold
                  : type == GameState::kExpAfter ? TextStyleManager::kXp : TextStyleManager::kIce;
        ShowTextHoverWithStyle(x, y, text.c_str(), style, 2.f, 50.f, false, false);
        // SoundsManager::PlaySound(type == GOLD ? "collect_gold" : "collect_resource", 1, false):
        // sounds are not ported yet.
        // UNVERIFIED (milestone 4): on other maps GameState::AddMapResourceCollectionInfo.
    }
    // UNVERIFIED (Items): kind 1 (items, profession points, chests).
    if (kind != 2) return;
    // (HideWorldDialog(nullptr) first: there is no world dialog yet)
    GameState::ChangeResourceAmount(type, (int)amount);
    OnCollect(0, automatic ? nullptr : sprite, false, false, 0xb, false);
    // UNVERIFIED: the text's format (the decompile shows only its "+"; the city drops' "+%d %s").
    std::u32string text = U"+";
    std::string n = std::to_string(amount);
    text.append(n.begin(), n.end());
    text += U' ';
    if (const char32_t* name = GameState::GetResourceGameName(type)) text += name;
    ShowTextHover(x, y - (float)sprite->tex->h, text.c_str(), 1.f, 1.f, 1.f, 0.f, 0.f, 0.f, 0x19, 5, 5.f, true,
                  2.f, 50.f);
    // SoundsManager::PlaySound("collect_farm_item", 1, false): sounds are not ported yet.
    if (GameState::tutorial == 0x49) GameState::tutorial = 0x4c;
}

void DropFarmFood(float x, float y, int type, unsigned amount, Render::Texture* tex, unsigned dropId) {
    if (!tex) {
        std::puts("BuildingHovers::DropFarmFood() Cannot drop item - no image");
        // UNVERIFIED (milestone 4, Tasks): Tasks::CompleteSubtask(0xd, dropId, 1).
        return;
    }
    g_itemDrops.emplace_back();
    ItemDrop& d = g_itemDrops.back();
    d.kind = 2;
    d.item = nullptr;
    d.type = type;
    d.amount = amount;
    d.sprite = Render::CreateSprite(tex, 0xc, false, false);
    y -= (float)(tex->h / 2);
    float sx = x - (float)(tex->w / 2);
    float delay = (float)(Rand48::lrand48() % 10) * 0.04f;
    int h = (int)(Rand48::lrand48() % 0x28) + 0x5a;
    if (GameState::GetCurrentMapID() != 0) {
        float limit = ((float)(unsigned)Map::GetGridHeight() * 42.f * 0.5f - 42.f) + (float)h * -1.5f;
        if (y > limit) y = limit;
    }
    if (Map::GetCurrentFarm() && y > 3000.f) y = 2995.f;
    d.delay = delay;
    d.x = sx;
    d.y = y;
    d.bounce = h;
    d.animating = true;
    d.vx = 30.f;
    if (GUI::IsSmallScreenVersion() && GameState::SecondTutorialStep() != 0x100) {
        d.bounce = (int)((float)h / 1.5f);
        d.vx /= 3.f;
    }
    d.vy = 0.f;
    d.groundY = y;
    Render::SetPosition(d.sprite, sx, y, 0.2f);
    Render::SetVisibility(d.sprite, false);
    for (int& s : d.subtasks)
        if (s == 0) s = (int)dropId;
    // SoundsManager::PlaySound("farm_item_dropped", 1, false): sounds are not ported yet.
    // UNVERIFIED (tutorial): step 0x48 locks the interaction to the drop's sprite.
}

void DropResource(float x, float y, int type, unsigned amount, bool collectNow, bool bonus) {
    if (amount == 0) return;
    do {
        g_itemDrops.emplace_back();
        ItemDrop& d = g_itemDrops.back();
        d.kind = 0;
        d.type = type;
        d.bonus = bonus;
        if (type == GameState::kGold && !collectNow) {
            d.amount = amount > 99 ? 100 : amount;
            if (amount < 11) {
                d.sprite = Render::CreateSprite(IconManager::GetIcon("gold_pile_small"), 0xc, false, false);
                amount = 0;
            } else {
                d.sprite = Render::CreateSprite(IconManager::GetIcon("gold_pile"), 0xc, false, false);
                amount = amount < 0x65 ? 0 : amount - 100;
            }
        } else {
            d.amount = amount;
            d.sprite = Render::CreateSprite(IconManager::GetIcon(GameState::GetResourceMapIconName(type)), 0xc, false, false);
            amount = 0;
        }
        float sy = y + d.sprite->h * -0.5f;
        float sx = x + d.sprite->w * -0.5f;
        float vx = (float)(Rand48::lrand48() % 0x3c) - 30.f;
        int h = (int)(Rand48::lrand48() % 0x28) + 0x5a;
        if (GameState::GetCurrentMapID() != 0) {
            float limit = ((float)(unsigned)Map::GetGridHeight() * 42.f * 0.5f - 42.f) + (float)h * -1.5f;
            if (sy > limit) sy = limit;
        }
        if (Map::GetCurrentFarm() && sy > 3000.f) sy = 2995.f;
        d.vx = vx;
        d.x = sx;
        d.y = sy;
        d.bounce = h;
        d.animating = true;
        d.delay = 0.f;
        if (GUI::IsSmallScreenVersion() && GameState::SecondTutorialStep() != 0x100) {
            d.vx /= 3.f;
            d.bounce = (int)((float)h / 1.5f);
        }
        d.vy = 0.f;
        d.groundY = sy;
        Render::SetPosition(d.sprite, sx, sy, 0.2f);
        Render::SetVisibility(d.sprite, false);
        if (collectNow) {
            ItemDrop& c = g_itemDrops.back();
            c.Collect(false, true);
            Render::RemoveSprite(c.sprite);
            Render::RemoveSprite(c.glow);
            g_itemDrops.pop_back();
        }
    } while (amount != 0);
    // SoundsManager::PlaySound("item_dropped_light", 1, false): sounds are not ported yet.
    // CheckForSpecialDrops @0x26fafc: ruby finds while the quests 0x4d7/0x4d9 are active
    // (Tasks::IsTaskActive). UNVERIFIED (milestone 4): quests are not ported, so it drops nothing.
    // UNVERIFIED (tutorial): steps 0x14 (unlock) and 0x21 (lock on the gold drop).
}

void AddItemMovement(Render::Sprite* sprite, int x, int y, bool screenSpace, float duration, int w, int h,
                     bool topLayer, bool fadeOut) {
    if (!sprite) return;
    g_itemMoves.emplace_back();
    ItemMove& m = g_itemMoves.back();
    m.sprite = Render::CreateSprite(sprite->tex, 0xc, false, false);
    int sx = (int)sprite->x, sy = (int)sprite->y;
    m.sprite->screenSpace = true;
    m.sprite->w = sprite->w;
    m.sprite->h = sprite->h;
    if (!screenSpace) Map::WorldCoordinatesToScreen(sx, sy);
    Render::SetPosition(m.sprite, (float)sx, (float)sy, 0.05f);
    Render::ChangeLayer(m.sprite, topLayer ? 0xd : 0xf);
    m.startW = sprite->w;
    m.startH = sprite->h;
    m.startX = (float)sx;
    m.startY = (float)sy;
    m.endW = w == -1 ? sprite->w : (float)w;
    m.endH = h == -1 ? sprite->h : (float)h;
    m.endX = (float)x;
    m.endY = (float)y + sprite->h;
    m.sound = topLayer;
    m.t = 0.f;
    m.fadeOut = fadeOut;
    m.duration = duration;
}

void OnCollect(unsigned item, Render::Sprite* sprite, bool screenSpace, bool glow, int type, bool) {
    if (!sprite) return;
    g_itemMoves.emplace_back();
    if (glow) g_itemMoves.emplace_back();
    ItemMove& m = g_itemMoves[g_itemMoves.size() - (glow ? 2 : 1)];
    float scale = screenSpace ? 1.f : Render::GetBaseZoomFactor();
    m.sprite = Render::CreateSprite(sprite->tex, 0xc, false, false);
    m.sprite->screenSpace = true;
    m.sprite->w = scale * sprite->w;
    m.sprite->h = scale * sprite->h;
    int sx = (int)sprite->x, sy = (int)sprite->y;
    if (!screenSpace) Map::WorldCoordinatesToScreen(sx, sy);
    Render::SetPosition(m.sprite, (float)sx, (float)sy, 0.05f);
    m.startW = scale * sprite->w;
    m.startH = scale * sprite->h;
    m.startX = (float)sx;
    m.startY = (float)sy;
    m.endW = sprite->w;
    m.endH = sprite->h;
    m.fadeOut = false;
    m.sound = false;
    if (item == 0) {
        if (type == GameState::kExpAfter) {
            m.endX = 18.f;
            m.endY = sprite->h + 18.f;
        } else {
            m.endX = sprite->w * -0.5f + (float)Render::ScreenWidth() * 0.5f;
            m.endY = sprite->h + 0.f;
        }
    }
    // UNVERIFIED (Items): items fly to the belt, the character window or a profession badge.
    m.t = 0.f;
    m.duration = 1.25f;
    if (glow) {
        ItemMove& g = g_itemMoves.back();
        g.sprite = Render::CreateSprite(IconManager::GetIcon("Pickup_item_glow"), 0xf, false, false);
        g.sprite->screenSpace = true;
        Render::SetShaderType(g.sprite, 8);
        Render::SetPosition(g.sprite, (float)sx, (float)sy, 0.07f);
        g.startW = g.sprite->w;
        g.startH = g.sprite->h;
        g.startX = m.startX + (m.startW - g.startW) * 0.5f;
        g.startY = m.startY + (m.startH - g.startH) * -0.5f;
        g.endW = g.sprite->w;
        g.endH = g.sprite->h;
        g.sound = false;
        g.t = 0.f;
        g.duration = 1.25f;
        g.fadeOut = false;
        g.endX = m.endX + (m.endW - g.endW) * 0.5f;
        g.endY = m.endY + (m.endH - g.endH) * -0.5f;
        Render::SortRenderLayer(Render::kLayer15, 1);
    }
    // UNVERIFIED (tutorial, items): steps 0x15/0x17 and the tame tutorial react to item 0xaf/0x254.
}

// ---- text popups ----

void ShowTextHover(float x, float y, const char32_t* text, float r, float g, float b, float glowR,
                   float glowG, float glowB, int fontSize, int glowBlur, float glowStrength,
                   bool nearest, float fade, float rise) {
    if (!text) {
        std::puts("BuildingHovers::ShowTextHover() text is NULL");
        return;
    }
    // StringTable::GetUndecoratedString: these texts carry no decorations.
    Render::GlowFilter glow;
    glow.r = glowR;
    glow.g = glowG;
    glow.b = glowB;
    glow.blur = glowBlur;
    glow.strength = glowStrength;
    g_textCounter = (g_textCounter + 1) % 100;
    TextInfo ti;
    ti.fade = fade;
    ti.rise = rise;
    ti.grow = false;
    Render::Font* font = Render::CreateFont(GUI::GetFontFile(), (unsigned)fontSize);
    ti.sprite = Render::CreateText(font, text, r, g, b, 1, false, &glow, nullptr, 0xc);
    g_itemInfo.push_back(ti);
    Render::Sprite* s = ti.sprite;
    float z = 0.3f + (float)g_textCounter * -0.0001f;
    Render::SetPosition(s, x + s->w * -0.5f, y, z);
    if (s->x + s->w > (float)(unsigned)Map::GetGridWidth() * 84.f) Render::SetPosition(s, x - s->w, y, z);
    if (s->x < 0.f) Render::SetPosition(s, 0.f, y, z);
    // UNVERIFIED: `nearest` sets the text texture's magnify filter to nearest (Texture::SetMagnifyFilter).
    (void)nearest;
}

int ShowTextHoverWithStyle(float x, float y, const char32_t* text, int style, float fade, float rise,
                           bool topLayer, bool screenSpace) {
    if (!text) {
        std::puts("BuildingHovers::ShowTextHoverWithStyle() text is NULL");
        return 0;
    }
    int size = 0x14;
    float r = 0, g = 0, b = 0;
    Render::GlowFilter* glow = TextStyleManager::GetTextStyle((TextStyleManager::TextStyle)style, size, r, g, b);
    g_styleCounter = (g_styleCounter + 1) % 100;
    TextInfo ti;
    ti.fade = fade;
    ti.rise = rise;
    ti.grow = false;
    // (The original keeps a cache of up to 0x20 short texts' textures, TextCache; a speed-up only.)
    Render::Font* font = Render::CreateFont(GUI::GetFontFile(), (unsigned)size);
    Render::Sprite* s = Render::CreateText(font, text, r, g, b, 1, false, glow, nullptr, topLayer ? 0xf : 0xd);
    ti.sprite = s;
    g_itemInfo.push_back(ti);
    float c = (float)g_styleCounter;
    if (!screenSpace) {
        s->w = s->w / Render::GetBaseZoomFactor();
        s->h = s->h / Render::GetBaseZoomFactor();
        Render::SetPosition(s, x + s->w * -0.5f, y + s->h * 0.5f, 0.5f + c * -0.001f);
        if (s->x + s->w > (float)(unsigned)Map::GetGridWidth() * 84.f)
            Render::SetPosition(s, x - s->w, y, 0.3f + c * -0.0001f);
    } else {
        Render::SetPosition(s, x + s->w * -0.5f, y + s->h * 0.5f, 0.5f + c * -0.001f);
        int sx = (int)x, sy = (int)y;
        Map::WorldCoordinatesToScreen(sx, sy);
        s->screenSpace = true;
        Render::SetPosition(s, (float)sx + s->w * -0.5f, (float)sy + s->h * 0.5f, 0.5f + c * -0.001f);
        float sw = (float)Render::ScreenWidth();
        if (s->w + s->x > sw) Render::SetPosition(s, sw - s->w, s->y, 0.3f + c * -0.0001f);
    }
    if (s->x < 0.f) Render::SetPosition(s, 0.f, y, 0.3f + c * -0.0001f);
    int d = (s->tex ? s->tex->h : 0) - (glow ? glow->GetOffset() : 0);
    return d < 0 ? -d : d;
}

// ---- the tick ----

void Update(double dtIn, bool force) {
    double dt = dtIn > 0.5 ? 0.5 : dtIn;
    // UNVERIFIED (tutorial): the city tutorial steps 0x48/0x49, 0x21, 0x14/0x15, 0x56/0x57, the
    // second tutorial 0x83..0x85, 0x8a..0x9e and 0x9f/0xa0, map 0xb and the tame tutorial keep the
    // drops alive (lifetime 128) and point the arrow at them.
    float fdt = (float)dt;
    if (!GameState::IsPaused() && !g_updatingDrops) {
        g_updatingDrops = true;
        for (size_t i = 0; i < g_itemDrops.size();) {
            g_itemDrops[i].Animate(fdt);
            ItemDrop& d = g_itemDrops[i];
            d.lifetime -= fdt;
            if (!(d.lifetime < 0.f)) { ++i; continue; }
            d.Collect(true, true);
            g_itemDrops[i].sprite = Render::RemoveSprite(g_itemDrops[i].sprite);
            g_itemDrops[i].glow = Render::RemoveSprite(g_itemDrops[i].glow);
            Render::SortRenderLayer(Render::kLayerGUI, 1);
            g_itemDrops[i] = g_itemDrops.back();
            g_itemDrops.pop_back();
        }
        g_updatingDrops = false;
    }
    for (size_t i = 0; i < g_itemInfo.size();) {
        TextInfo& ti = g_itemInfo[i];
        Render::Sprite* s = ti.sprite;
        if (!ti.grow) {
            Render::SetPosition(s, s->x, s->y - fdt * ti.rise, s->z);
            Render::SetAlpha(s, s->alpha[2] - fdt / ti.fade);
        } else {
            float w = s->w;
            float d = fdt * -30.f, m = fdt * 15.f;
            s->h = s->h + d / (w / s->h);
            s->w = w + d;
            Render::SetPosition(s, m + s->x, m + s->y, s->z);
            Render::SetAlpha(s, s->alpha[2] + fdt * -0.5f);
        }
        if (!(s->alpha[2] < 0.f)) { ++i; continue; }
        Render::RemoveSprite(s);
        Render::SortRenderLayer(Render::kLayerGUI, 1);
        g_itemInfo[i] = g_itemInfo.back();
        g_itemInfo.pop_back();
    }
    for (size_t i = 0; i < g_itemMoves.size();) {
        ItemMove& m = g_itemMoves[i];
        Render::Sprite* s = m.sprite;
        auto ease = [&] {
            float u = m.t / m.duration - 1.f;
            return std::sqrt(1.f - u * u);
        };
        Render::SetPosition(s, m.startX + (m.endX - m.startX) * ease(), m.startY + (m.endY - m.startY) * ease(), s->z);
        s->w = m.startW + (m.endW - m.startW) * ease();
        s->h = m.startH + (m.endH - m.startH) * ease();
        if (s->shaderType == 8) {
            s->color[0] = s->x + s->w * 0.5f;   // rotation centre and angle
            s->color[1] = s->h * 0.5f - s->y;
            s->color[2] = m.t;
            if (m.t > 1.f) Render::SetAlpha(s, (m.duration - m.t) / (m.duration - 1.f));
        } else if (m.fadeOut && m.duration - m.t < 0.25f) {
            Render::SetShaderType(s, 1);
            Render::SetAlpha(s, (m.duration - m.t) * 4.f);
        }
        if (m.t > m.duration) {
            // if (m.sound) SoundsManager::PlaySound("building_position", 1, false): not ported yet.
            m.sprite = Render::RemoveSprite(m.sprite);
            Render::SortRenderLayer(Render::kLayer15, 1);
            g_itemMoves[i] = g_itemMoves.back();
            g_itemMoves.pop_back();
            // UNVERIFIED (tutorial): at second-tutorial step 0x95 BeltBarWindow::UpdateContents.
        } else {
            m.t = fdt + m.t;
            ++i;
        }
    }
    UpdateArrow();
    if (GameState::IsPaused()) return;
    if (WindowManager::GetShownWindowCount() > g_arrowVisibleWindowLimit && ArrowVisible() &&
        !BuildingMovement::Activated() && !BuildingPlacement::Activated() && !GameState::IsTutorial())
        HideArrow();
    // UNVERIFIED (tutorial): the tablet attention rings' animation.
    // UNVERIFIED: the world dialog's Update (not ported).
    UpdateHoverPositions();
    if (Timer::GetGlobalTime() != g_lastTime || force) {
        g_lastTime = Timer::GetGlobalTime();
        UpdateHovers();
    }
}

// ---- the city tap ----

// The farm view opens (OnBuildingClick and Click share these steps).
static void EnterFarmView(Map::Building* b) {
    GameState::SetCurrentLocation(1);
    // UNVERIFIED (milestone 4, Tasks): HUDWindow::UpdateTasks.
    HUDWindow::SetBottomType(3);
    Map::ShowFarm(true, b);
}

bool Click(int x, int y, bool pressed) {
    if (!GameState::IsPlayerCity() || BuildingMovement::Activated() || BuildingPlacement::Activated() ||
        ShopWindow::IsVisible())
        return false;
    // UNVERIFIED (tutorial): ClickOnArrow, and hiding the arrow on a release.
    if (g_currentHover) {
        if (g_currentHover->Click(x, y, pressed)) return true;
        int step = GameState::TutorialStep();
        if (g_currentHover && !pressed && step != 0x45 && step != 0x53 && step != 0x54) {
            WindowManager::WindowHide(false);
            g_wnd->shown = pressed;
            g_currentHover->Hide();
            g_currentHover = nullptr;
        }
    }
    int tx = x, ty = y;
    Map::MouseCoordinatesToWorld(tx, ty);
    Map::WorldCoordinatesToTile(tx, ty);
    if (!g_itemDrops.empty() && !pressed) {
        for (size_t i = 0; i < g_itemDrops.size(); ++i) {
            Render::Sprite* s = g_itemDrops[i].sprite;
            int top = (int)(s->y - s->h), right = (int)(s->x + s->w), bottom = (int)s->y, left = (int)s->x;
            Map::WorldCoordinatesToScreen(left, top);
            Map::WorldCoordinatesToScreen(right, bottom);
            if (left <= x && x <= right && top <= y && y <= bottom && GUI::CanInteractWith(s)) {
                ItemDrop& d = g_itemDrops[i];
                d.Collect(false, true);
                Render::RemoveSprite(d.sprite);
                Render::RemoveSprite(d.glow);
                g_itemDrops[i] = g_itemDrops.back();
                g_itemDrops.pop_back();
                // UNVERIFIED (tutorial): steps 0x49, 0x15, 0x57, 0x85 and 0x94 advance here.
                Hide();
                UpdateHovers();
                return true;
            }
        }
    }
    int wx = x, wy = y;
    Map::MouseCoordinatesToWorld(wx, wy);
    Map::Building* b = Map::GetBuildingAtCoords(wx, wy);
    Entity* e = EntityManager::GetEntityAtWorldXY(wx, wy);
    Map::Decor* d = Map::GetDecorAtCoords(wx, wy, false);
    if (b && !GUI::CanInteractWith(b)) b = nullptr;
    if (e && !GUI::CanInteractWith(e)) e = nullptr;
    if (d && !GUI::CanInteractWith(d)) d = nullptr;
    if (!g_hoverData.empty()) {
        for (size_t i = 0; i < g_hoverData.size(); ++i) {
            HoverInfo* h = g_hoverData[i];
            if (h->window && h->type != kFarmWater && h->type != kDelivery && h->window->Click(x, y, pressed))
                return true;
        }
        if (!pressed) {
            for (size_t i = 0; i < g_hoverData.size(); ++i) {
                HoverInfo* h = g_hoverData[i];
                if (h->window && h->type != kFarmWater && h->type != kTraining)
                    h->window->Activate((b && h->building == b) || (d && h->decor == d));
                h = g_hoverData[i];
                if (h->building != b) continue;
                int t = h->type;
                if (t == kAssignBuilder) { OnBuildingAssignBuilder(b); return true; }
                if (t == kContractFinished) { OnBuildingFinishedClick(b); return true; }
                if (t == kCutResource) {
                    b->ResetResource();
                    UpdateHovers();
                    return true;
                }
                if (t == kHireTroops) {
                    // UNVERIFIED (milestone 4): HireTroopsWindow::ShowForBuilding(tutorial 0x61 ? 0x8f : id).
                    return true;
                }
                if (b) {
                    if (t == kFarmWater || t == kFarmReady || t == kFarmSleeping) {
                        if (GameState::GetTutorialType() == 1 && GameState::secondTutorial != 0x100 &&
                            (unsigned)(GameState::tutorial - 0x3a) > 0x17)
                            return true;
                        EnterFarmView(b);
                        return true;
                    }
                    if (t == kBuildProgress && b->BuilderAssigned()) {
                        // SoundsManager::PlaySound("building_build_start", 1, true): not ported yet.
                        return true;
                    }
                }
            }
        }
    }
    if (e && !pressed) {
        int r = OnEntityClick(e, x, y);
        if (r != 2) return r != 0;
    }
    // UNVERIFIED (milestone 4): in a city battle (g_Combat) a building with an active hover is tapped too.
    if (b && !pressed && !BuildingHasActiveHover(b) && OnBuildingClick(b, x, y)) return true;
    if (d && !pressed) return OnDecorClick(d, x, y);
    return false;
}

// @0x26a3b0: a tapped decoration. In the city one that pays taxes shows its info window.
bool OnDecorClick(Map::Decor* d, int x, int y) {
    int step = GameState::tutorial;
    // UNVERIFIED (tutorial): steps 0x13 and 0x3a hide the world dialog.
    if (step == 0x14 || step == 0x3a) HideArrow();
    // UNVERIFIED (milestone 4): in a combat only skipCombatCheck lets a tap through.
    if (GameState::GetCurrentMapID() == 0 && d->patch && !d->patch->owned) return true;
    // UNVERIFIED (milestone 4): a quest-locked decoration (Decor::IsLocked: its MetaExpression) goes to
    // the hero (ClickedDecoration); a decoration job (+0x4c: its resource needs, "REQUIREMENT_GOBLIN",
    // the hero or a worker sent, the "FREE_UP_WORKER" popup); Decor::OnClick (quest text, portals).
    // Without a MetaExpression, a job or a portal none of these apply.
    if (d->f4c != 0) return true;
    const GameData::DecorData* data = d->GetData();
    BuildingHoverWindow* w = g_currentHover;
    if (data && data->collectTime != 0 && GameState::GetCurrentMapID() == 0 && d->patch && d->patch->owned) {
        for (HoverInfo* h : g_hoverData)
            if (h->window && h->decor == d) return false;
        w = g_currentHover = g_decorationWindow;
    }
    if (!w) {
        // UNVERIFIED (milestone 4): a tap the city does not take goes to EntityManager::OnClick at the
        // decoration's tile when (x, y) is (0, 0).
        (void)x;
        (void)y;
        return false;
    }
    bool wasShown = g_wnd->shown;
    g_wnd->shown = true;
    g_wnd->MoveWindowOnTop(true);
    w->SetEntity(nullptr);
    w->SetDecoration(d);
    w->SetBuilding(nullptr);
    Render::Sprite* s = d->sprite;
    int sx = (int)(s->x + s->w * 0.5f), sy = (int)(s->y - s->h);
    Map::WorldCoordinatesToScreen(sx, sy);
    w->SetPosition(sx, sy);
    w->Show();
    w->MoveWindowOnTop(true);
    if (!wasShown) WindowManager::WindowShow(false);
    return true;
}

// @0x26aafc: a tapped entity. A tree's or rock's pile is collected; a farm patch shows its window
// (3f). Returns 0 (not taken), 1 (taken) or 2 (no window: the tap goes on to buildings).
int OnEntityClick(Entity* e, int x, int y) {
    if (!GameState::IsPlayerCity()) return 1;
    int cls = e->GetEntityData()->clas;
    if (cls == 1) {
        if (Map::Building* home = e->GetHome()) {
            int amount = 0;
            bool full = false;
            home->CollectResources(full, amount);
            std::u32string text = full ? std::u32string(StringTable::GetString("WORK_RES_NO_SPACE"))
                                       : SWPrintf(0x100, U"+%d %s", {amount, GameState::GetResourceGameName(home->data->produceResource)});
            ShowTextHover(home->baseX, home->baseY, text.c_str(), 1.f, 1.f, 1.f, 0.f, 0.f, 0.f, 0x19, 5, 5.f, true, 2.f,
                          50.f);
            return 1;
        }
    } else if (cls == 3 || cls == 4 || cls == 7) {
        // A soil patch: for sale -> the buy window; locked -> a note; a crop past its rot time that
        // would not fit in the storage -> a note; else its farm window.
        AIBaseState* ai = e->GetAI();
        if (ai && ai->GetState() == 0x1b) {
            LandWindow::SetPatchParameters();
            LandWindow::Show();
            return 1;
        }
        auto note = [&](const char* key) {
            float wx = 0.f, wy = 0.f;
            e->GetWorldPos(wx, wy);
            ShowTextHover(wx, wy, StringTable::GetString(key), 1.f, 1.f, 1.f, 0.f, 0.f, 0.f, 0x19, 5, 5.f, true, 2.f,
                          50.f);
        };
        if (ai && ai->GetState() == 0x14) {
            note("SOIL_PATCH_LOCKED");
            return 1;
        }
        if (ai) {
            Map::Building* farm = Map::GetCurrentFarm();
            unsigned p = (unsigned)ai->GetFarmPatchNum();
            if (farm->GetFarmState((int)p) == 6) {
                const Contracts::ContractMission& m = farm->data->delivery->missions[(size_t)farm->patchContract[p] - 1];
                if (GameState::resourceAmountMax <=
                    (int)GameState::GetResourceAmount(m.rewardResource) + (int)m.rewardResourceCount) {
                    note("WORK_RES_NO_SPACE");
                    return 1;
                }
            }
            ShowFarmHover(false, (unsigned)ai->GetFarmPatchNum(), 1);
            WindowManager::WindowHide(false);
            g_wnd->shown = false;
        }
    }
    if (!g_currentHover) {
        // UNVERIFIED (milestone 4): on campaign maps (location 2) a live, active entity takes the tap.
        return 2;
    }
    g_wnd->shown = true;
    g_wnd->MoveWindowOnTop(true);
    g_currentHover->SetEntity(e);
    g_currentHover->SetPosition(x, y);
    g_currentHover->Show();
    g_currentHover->MoveWindowOnTop(true);
    WindowManager::WindowShow(false);
    return 1;
}

// @0x26726c: the tapped building's info window, placed over it. Returns whether the tap was taken.
bool OnBuildingClick(Map::Building* b, int, int) {
    // UNVERIFIED (milestone 4): in a city battle CombatManager::OnBuildingClicked takes the tap.
    if (GameState::tutorial == 0x3a) {
        HideArrow();
        // UNVERIFIED (tutorial): HideWorldDialog.
        GUI::SetInteractionLock(true);
        GameState::tutorial = 0x3d;
        return false;
    }
    if (!g_skipHoverCheck) {
        for (HoverInfo* h : g_hoverData)
            if (h->window && h->building == b && h->type != kFarmSleeping && h->type != kFarmReady &&
                h->type != kTraining && h->type != kDelivery)
                return true;
    }
    g_skipHoverCheck = false;
    bool tutorialFarmLock = GameState::GetTutorialType() == 1 && GameState::secondTutorial != 0x100;
    if (GameState::GetCurrentMapID() == 0) {
        if (b->patch && !b->patch->owned) return true;
        int cls = b->data->buildingClass;
        if (cls == 0xd && b->IsOpened()) {
            if (tutorialFarmLock && (unsigned)(GameState::tutorial - 0x3a) > 0x17) return true;
            EnterFarmView(b);   // (then on to the window tail, as on the original)
        } else if (cls == 4 && b->WorkerAssigned(0)) {
            g_currentHover = g_resourceActiveWindow;
        } else if (b->IsOpened() && b->id == 0x12) {
            g_currentHover = g_storageWindow;
        } else if (b->IsOpened() && b->id == 0x81) {
            g_currentHover = g_emptyWindow;
        } else if (b->IsOpened() && b->id == 99) {
            g_currentHover = g_castleWindow;
        } else if (b->IsOpened() && b->id == 0x8f) {
            // UNVERIFIED (milestone 4): HireTroopsWindow::ShowForBuilding(id, false).
        } else if (cls == 4) {
            OnResourceAssign(b);
        } else if (b->IsOpened() && cls == 0) {
            g_currentHover = g_livingWindow;
        } else if (b->IsOpened() && cls == 2 && b->HasActiveContract() && !b->contractDone) {
            g_currentHover = g_factoryWindow;
        } else if (b->IsOpened() && cls == 2) {
            if (tutorialFarmLock && (unsigned)(GameState::tutorial - 0x3a) > 0x1b) return true;
            g_currentHover = g_factoryWindow;
        } else if (b->IsOpened() && cls == 0xc) {
            // UNVERIFIED (milestone 4): the tavern's HireTroopsWindow (Setting "always_open_tavern"
            // == 1 outside tutorial step 0x61: building 0x8f's).
        }
    }
    // UNVERIFIED (milestone 4): on other maps the campaign buildings' windows.
    BuildingHoverWindow* w = g_currentHover;
    if (!w) return false;
    bool wasShown = g_wnd->shown;
    g_wnd->shown = true;
    g_wnd->MoveWindowOnTop(true);
    w->SetEntity(nullptr);
    w->SetDecoration(nullptr);
    w->SetBuilding(b);
    int sx = (int)((b->minX + b->maxX) * 0.5f);
    int sy = (int)((float)b->data->iconY + b->minY);
    Map::WorldCoordinatesToScreen(sx, sy);
    w->SetPosition(sx, sy);
    w->Show();
    w->MoveWindowOnTop(true);
    if (w == g_progressWindow) w->Activate(true);
    // SoundsManager::PlaySound("ui_click_building", 1, true) (not for class 4): milestone 5.
    if (wasShown) return true;
    WindowManager::WindowShow(false);
    return true;
}

// ---- hover actions ----

// @0x263ae4
void OnFreeWorkerBuild() {
    PopupSelectionWindow::Hide();
    ShopWindow::ShowBestOfTab(0);
}

// @0x263b60: a free worker, else the first busy one taken off its job, goes to b.
void OnFreeWorkerAssign(Map::Building* b) {
    PopupSelectionWindow::Hide();
    if (Entity* w = EntityManager::GetFreeWorker()) {
        b->AssignWorker(w, 0);
        return;
    }
    Entity* w = EntityManager::GetFirstBusyWorker();
    if (!w) return;
    if (Map::Building* job = w->GetWorkplace()) job->RemoveWorker(w);
    // UNVERIFIED (milestone 4): a worker on a decoration job leaves it (Decor::RemoveWorker).
    b->AssignWorker(w, 0);
}

namespace {
// "No free worker": move a busy one (OnFreeWorkerAssign) or build a house (OnFreeWorkerBuild).
void AskToFreeWorker(Map::Building* b, Render::Texture* icon) {
    // SoundsManager::PlaySound("ui_show_busyworkers", 1, false): sounds are milestone 5.
    PopupSelectionWindow::Show(StringTable::GetString("FREE_UP_WORKER"), StringTable::GetString("NO_FREE_WORKER_DESC"),
                               StringTable::GetString("FIRE_PERSON"), icon, [b] { OnFreeWorkerAssign(b); },
                               StringTable::GetString("PERFORM_BUILD"), IconManager::GetIcon("gold_build"),
                               OnFreeWorkerBuild);
    PopupSelectionWindow::SetMentorIcon();
}
}  // namespace

// @0x266a24: a tap on a tree or rock without a worker: a cut stump resets, else a worker goes.
void OnResourceAssign(Map::Building* b) {
    if (b->data->buildingClass == 4 && !b->WorkerAssigned(0)) {
        if (b->resourceLeft == 0 && b->resourceState == 2) {
            b->ResetResource();
        } else if (Entity* w = EntityManager::GetFreeWorker()) {
            b->AssignWorker(w, 0);
        } else if (EntityManager::GetFirstBusyWorker()) {
            const char* icon = b->data->id == 0x11 ? "icon_profession_lumberjack"
                               : b->data->id == 0x14 ? "icon_profession_miner" : "add_person";
            AskToFreeWorker(b, IconManager::GetIcon(icon));
        }
    }
    if (GameState::tutorial == 0x25) {
        HideArrow();
        GUI::SetInteractionLock(true);
        GameState::tutorial = 0x26;
    }
}

void OnBuildingAssignBuilder(Map::Building* b) {
    if (!b->BuilderAssigned()) {
        if (Entity* w = EntityManager::GetFreeWorker()) b->AssignWorker(w, 0);
        else if (EntityManager::GetFirstBusyWorker()) AskToFreeWorker(b, IconManager::GetIcon("icon_profession_builder"));
        UpdateHovers();
        return;
    }
    ShowTextHover(b->baseX, b->baseY, StringTable::GetString("BUILDER_COMING"), 1.f, 1.f, 1.f, 0.f, 0.f, 0.f,
                  0x19, 5, 5.f, true, 2.f, 50.f);
}

// A finished order: its rewards drop at the building and the same order starts again if affordable.
void OnBuildingFinishedClick(Map::Building* b) {
    if (!b->HasActiveContract() || !b->contractDone) return;
    const Contracts::ContractMission& m = b->data->delivery->missions[(size_t)b->GetMissionID()];
    if (m.rewardResource != GameState::kGold &&
        (int)(GameState::GetResourceAmount(m.rewardResource) + m.rewardResourceCount) > GameState::resourceAmountMax) {
        ShowTextHover(b->baseX, b->minY, StringTable::GetString("WORK_RES_NO_SPACE"), 1.f, 1.f, 1.f, 0.f, 0.f,
                      0.f, 0x19, 5, 5.f, true, 2.f, 50.f);
        return;
    }
    // UNVERIFIED (Items, Tasks): an item reward drops (DropItem); without one the order's subtask
    // completes (Tasks::CompleteSubtask(0xd, contract - 1 + delivery id * 10)).
    int step = GameState::TutorialStep();
    if (step != 0x58 && step != 0x56) {
        DropResource(b->baseX, b->minY, m.rewardResource, m.rewardResourceCount, false, false);
        DropResource(b->baseX, b->minY, GameState::kGold, m.rewardGold, false, false);
        DropResource(b->baseX, b->minY, GameState::kExpAfter, m.rewardXp, false, false);
    }
    if ((int)GameState::GetResourceAmount(GameState::kGold) - (int)m.price < 0 ||
        (int)GameState::GetResourceAmount(m.priceResource) - (int)m.priceResourceCount < 0) {
        b->contract = 0;
    } else {
        GameState::ChangeResourceAmount(GameState::kGold, -(int)m.price);
        GameState::ChangeResourceAmount(m.priceResource, -(int)m.priceResourceCount);
        b->LaunchContract((unsigned)(b->contract - 1), -1);
    }
    UpdateHovers();
}

// A decoration's taxes: its chest (Chests, not ported yet), gold and XP drop at its sprite.
void CreateDecorationDrop(Map::Decor* d) {
    if (!d || !d->data || !d->sprite) return;
    // UNVERIFIED (milestone 4): a "collect_chest" opens ChestOpenedWindow with the chest's contents.
    Render::Sprite* s = d->sprite;
    if (d->data->collectMoney != 0)
        DropResource(s->x + s->w * 0.5f, s->y + s->h * -0.5f, GameState::kGold, d->data->collectMoney, false, false);
    if (d->data->collectExp != 0)
        DropResource(s->x + s->w * 0.5f, s->y + s->h * -0.5f, GameState::kExpAfter, d->data->collectExp, false, false);
}

}  // namespace BuildingHovers
