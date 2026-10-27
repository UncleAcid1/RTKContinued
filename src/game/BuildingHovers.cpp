#include "engine/SoundsManager.h"
#include "game/BuildingHovers.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "engine/IconManager.h"
#include "engine/Render.h"
#include "engine/Resources.h"
#include "engine/Text.h"
#include "engine/Timer.h"
#include "game/Building.h"
#include "game/BuildingMovement.h"
#include "windows/Windows.h"
#include "game/BuildingPlacement.h"
#include "game/AIState.h"
#include "game/Contracts.h"
#include "game/Combat.h"
#include "game/Fog.h"
#include "game/Chests.h"
#include "game/Entity.h"
#include "game/EntityData.h"
#include "game/EntityManager.h"
#include "game/GameData.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/Map.h"
#include "game/MetaData.h"
#include "game/MetaExpression.h"
#include "game/Professions.h"
#include "game/Rand48.h"
#include "game/Setting.h"
#include "game/SoldierSlots.h"
#include "game/StringTable.h"
#include "gui/GUI.h"
#include "gui/TextStyleManager.h"
#include "gui/WindowManager.h"
#include "hud/HUD.h"
#include "hud/HoverWindows.h"
#include "game/Tasks.h"

namespace BuildingHovers {

namespace {

// A dropped pickup (0x64 bytes): it falls and bounces, then waits to be tapped; after `lifetime`
// seconds it collects itself.
struct ItemDrop {
    int kind = 0;                     // +0x00 0 resource, 1 item, 2 farm food
    Items::ItemInfo* item = nullptr;  // +0x04 (items)
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
    // @0x263660: falls from (x, y) (on other maps kept above the bottom edge, on the farm below
    // y 3000) with horizontal speed vx after delay seconds; bounce h (2/3 and vx / 3 on small
    // screens before the city tutorial's end).
    void StartAnimation(float x, float y, float vx, float delay, int h);
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
bool g_rubyDropped = false;            // 0x6188e0 a ruby lies on the ground (CheckForSpecialDrops)
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
bool g_skipCombatCheck = false;        // BuildingHovers::skipCombatCheck
int g_lastID = 0;                      // 0x618998
bool g_hoversVisible = true;           // 0x60f138
WorldHintHoverWindow* g_worldDialog = nullptr;   // BuildingHovers::worldDialogHoverWindow
uint32_t g_lastTime = 0;               // 0x6189dc
bool g_updatingDrops = false;          // 0x6188e1
int g_textCounter = 0;                 // 0x6188d8 (ShowTextHover)
int g_styleCounter = 0;                // 0x6188dc (ShowTextHoverWithStyle)

// HelperArrow (0x30 bytes): the bobbing "Arrow_show" pointer (tutorial steps, building placement).
// arrows[0] always exists (Init); ArrowAt(..., newArrow) adds more, HideArrow drops them again.
struct HelperArrow {
    Render::Sprite* sprite = nullptr;  // +0x00
    bool left = false;                 // +0x04 points left (Arrow_show_left), else down
    bool mirror = false, flip = false; // +0x05 +0x06
    float x = 0.f, y = 0.f;            // +0x08 +0x0c the anchor, world space unless screenSpace
    GUI::Callback* onClick = nullptr;  // +0x10 SetArrowClickCallback (owned)
    Render::Sprite* ring1 = nullptr;   // +0x18 the attention rings round the target (tablets)
    Render::Sprite* ring2 = nullptr;   // +0x1c
    float ringTime = 0.f;              // +0x20 their pulse, 0..2 s
    bool itemArrow = false;            // +0x24 it waits for the item dropped on the emperor's map (0xb)
    Entity* entity = nullptr;          // +0x28 the entity it follows (FollowEntity)
    Map::Decor* decor = nullptr;       // +0x2c the decoration it points at (FollowDecor)
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
    else if (t == kTalk)
        w = new TalkHoverWindow();
    else if (t == kHealthbarTiny)
        w = new HealthbarTinyHoverWindow();
    // (HealthbarHoverWindow, 0xd, is never asked for in 5.11; FriendInfoHoverWindow, 0x13, is
    // online.)
    else if (t == kUseItem)
        w = new UseItemHoverWindow();
    else if (t == kBossTime)
        w = new BossTimeHoverWindow();
    else if (t == kFarmSleeping || t == kDelivery)
        w = new SleepingHoverWindow();
    else if (t == kPlayerName)
        w = new PlayerNameHoverWindow();
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
    // (wndScale 0x60f134, 1.5, becomes 1 below a 320 px screen height here; nothing in 5.11 reads it.)
    g_arrows.push_back(new HelperArrow());
    // (PersonHoverWindow and FarmGrowHoverWindow are created here too, but nothing in 5.11 shows
    // them: not ported.)
    g_emptyWindow = new EmptyHoverWindow();
    g_storageWindow = new StorageHoverWindow();
    g_resourceActiveWindow = new ResourceHoverWindow();
    g_livingWindow = new LivingHoverWindow();
    g_factoryWindow = new FactoryHoverWindow();
    g_progressWindow = new BuildProgressHoverWindow(true);
    g_castleWindow = new CastleHoverWindow();
    g_farmRestoreWindow = new FarmRestoreWindow();
    g_decorationWindow = new DecorationHoverWindow();
    g_worldDialog = new WorldHintHoverWindow();
    Queue()->fn.back = OnBack;   // (the Back key)
}

// ---- the helper arrow ----

bool ArrowVisible() {
    for (HelperArrow* a : g_arrows)
        if (a->sprite && a->sprite->visible) return true;
    return false;
}

bool CanAutoHideArrow() { return g_canHideArrow; }

void MoveRectIntoView(int x0, int y0, int x1, int y1, bool full) {
    int sx0 = x0, sy0 = y0, sx1 = x1, sy1 = y1;
    Map::WorldCoordinatesToScreen(sx0, sy0);
    Map::WorldCoordinatesToScreen(sx1, sy1);
    const int W = GUI::ScreenWidth(), H = GUI::ScreenHeight();
    const int left = full ? 0 : 0x32, top = full ? 0 : 0x4b, right = full ? W : W - 0x32, bottom = full ? H : H - 0x32;
    int ltx = left, lty = top, rbx = right, rby = bottom;   // the margins' corners in the world
    Map::MouseCoordinatesToWorld(ltx, lty);
    Map::MouseCoordinatesToWorld(rbx, rby);
    int dx = left > sx0 ? x0 - ltx : 0;
    if (right < sx1) dx = x1 - rbx;
    int dy = top > sy0 ? y0 - lty : 0;
    if (bottom < sy1) dy = y1 - rby;
    if (dx != 0 || dy != 0) Render::CenterOn((float)dx - Render::offsetX, (float)dy + Render::offsetY, true);
}

void PreventArrowAutoHide() { g_canHideArrow = false; }

GUI::Callback* GetClickCallback() { return g_arrows.back()->onClick; }

int GetArrowClickCallbackID() { return g_arrowCallbackID; }

void SetArrowVisibleWindowLimit(unsigned limit) { g_arrowVisibleWindowLimit = limit; }

// @0x262fe8: the next info window may open over other windows (OnBuildingClick's check is skipped once).
void AllowWindowOverHoverOnce() { g_skipHoverCheck = true; }

// @0x263bd0: the arrow at the hurry button of b's build progress hover.
void ShowArrowAtSpeedup(Map::Building* b) {
    if (g_hoverData.empty()) return;
    for (HoverInfo* h : g_hoverData) {
        if (h->building != b) continue;
        if (h->type == 2) static_cast<BuildProgressHoverWindow*>(h->window)->HelpWithSpeedUp();
        return;
    }
}

// @0x263c44: the arrow at the Improve button of the open info window.
void ShowArrowAtUpgrade() {
    if (g_castleWindow->shownHover) g_castleWindow->ShowArrowAtUpgrade();
    if (g_storageWindow->shownHover) g_storageWindow->ShowArrowAtUpgrade();
    if (g_livingWindow->shownHover) g_livingWindow->ShowArrowAtUpgrade();
    if (g_factoryWindow->shownHover) g_factoryWindow->ShowArrowAtUpgrade();
}

// @0x263cfc: the arrow at the factory window's order i (ignoreCompletion: its Complete Now is ignored).
void ShowArrowAtFactoryContract(unsigned i, bool ignoreCompletion) {
    g_factoryWindow->ShowArrowAt(i);
    if (ignoreCompletion) g_factoryWindow->IgnoreCompletion();
}

int SetArrowClickCallback(GUI::Callback* cb) {
    HelperArrow* a = g_arrows.back();
    delete a->onClick;
    a->onClick = cb;
    ++g_arrowCallbackID;
    std::printf("Arrow callbacks created: %d\n", g_arrowCallbackID);
    return g_arrowCallbackID;
}

GUI::Callback* HijackArrowCallback() {
    HelperArrow* a = g_arrows.back();
    GUI::Callback* cb = a->onClick;
    a->onClick = nullptr;
    return cb;
}

// Without a callback the hero is sent to the arrow's tile.
void ExecuteArrowCallback() {
    HelperArrow* a = nullptr;
    for (HelperArrow* o : g_arrows) {
        if (o->sprite && o->sprite->visible) {
            a = o;
            break;
        }
    }
    if (!a) return;
    Render::SetVisibility(a->sprite, false);
    if (!a->onClick) {
        int x = (int)a->x, y = (int)a->y;
        Map::WorldCoordinatesToTile(x, y);
        EntityManager::GetPlayer()->GetAI()->ClickedTile(x, y, 0, 0);
        return;
    }
    for (HelperArrow* o : g_arrows)
        if (o && o->sprite) Render::SetVisibility(o->sprite, false);
    a->decor = nullptr;
    a->entity = nullptr;
    (*a->onClick)();
    ++g_arrowCallbackID;
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
        SoundsManager::PlaySound("ui_arrow_click", 1, false);
        for (HelperArrow* o : g_arrows)
            if (o && o->sprite) Render::SetVisibility(o->sprite, false);
        a->decor = nullptr;
        a->itemArrow = false;
        a->entity = nullptr;
        (*a->onClick)();
        ++g_arrowCallbackID;
        return true;
    }
    return false;
}

void HideArrow() {
    for (size_t i = 1; i < g_arrows.size(); ++i) {
        HelperArrow* a = g_arrows[i];
        if (!a) continue;
        a->sprite = Render::RemoveSprite(a->sprite);
        a->ring1 = Render::RemoveSprite(a->ring1);
        a->ring2 = Render::RemoveSprite(a->ring2);
        delete a;
    }
    g_arrows.resize(1);
    HelperArrow* a = g_arrows[0];
    if (a->sprite) Render::SetVisibility(a->sprite, false);
    if (a->ring1) Render::SetVisibility(a->ring1, false);
    if (a->ring2) Render::SetVisibility(a->ring2, false);
    delete a->onClick;
    a->onClick = nullptr;
    a->decor = nullptr;
    a->itemArrow = false;
    a->entity = nullptr;
    ++g_arrowCallbackID;
}

// The rings go transparent (alpha 0) whatever a is.
void SetArrowAlpha(float a) {
    for (HelperArrow* h : g_arrows) {
        if (!h->sprite) continue;
        Render::SetShaderType(h->sprite, a != 1.f ? 1 : 0);
        Render::SetAlpha(h->sprite, a);
        if (h->ring1) Render::SetAlpha(h->ring1, 0.f);
        if (h->ring2) Render::SetAlpha(h->ring2, 0.f);
    }
}

// (Part of Update @0x26e460) the attention rings round the arrow's point pulse every 2 s: the first
// grows from nothing with the time, fades in over the first second, stays, fades out from 1.25 s;
// the second starts at 0.3 s at 1.5 times the size. Both follow the arrow's visibility.
void UpdateArrowRings(float dt) {
    for (HelperArrow* a : g_arrows) {
        if (!a->ring1 || !a->ring1->visible || !a->sprite) continue;
        Render::Sprite* s = a->sprite;
        Render::SetVisibility(a->ring1, s->visible);
        Render::SetVisibility(a->ring2, s->visible);
        float px = a->x, py = a->y;
        if (!a->left) {
            px += s->w * 0.5f;
            if (a->flip) py -= s->h;
        } else {
            py += s->h * -0.5f;
            if (a->mirror) px += s->w;
        }
        if (s->screenSpace) {
            int ix = (int)px, iy = (int)py;
            Map::MouseCoordinatesToWorld(ix, iy);
            px = (float)ix;
            py = (float)iy;
        }
        float t = a->ringTime;
        Render::Sprite* r = a->ring1;
        if (t < 1.f) {
            Render::SetShaderType(r, 1);
            Render::SetAlpha(r, 1.f - std::sqrt(1.f - t * t));
        } else if (t < 1.25f) {
            Render::SetShaderType(r, 0);
            Render::SetAlpha(r, 1.f);
        } else if (t < 2.f) {
            Render::SetShaderType(r, 1);
            const float u = (t - 1.25f) / 0.75f - 1.f;
            Render::SetAlpha(r, 1.f - std::sqrt(1.f - u * u));
        }
        t = a->ringTime;
        r->w = (float)r->tex->w * t * GUI::GetHoverScaleFactor(1.f, 1.f);
        r->h = (float)r->tex->h * t * GUI::GetHoverScaleFactor(1.f, 1.f);
        Render::SetPosition(r, px + r->w * -0.5f, py + r->h * 0.5f, 0.5f);
        r = a->ring2;
        t = a->ringTime;
        float k = -1.f;
        if (t < 0.3f) {
            Render::SetShaderType(r, 1);
            Render::SetAlpha(r, 0.f);
        } else if (t < 1.f) {
            k = (t - 0.3f) / 0.7f;
            Render::SetShaderType(r, 1);
            Render::SetAlpha(r, 1.f - std::sqrt(1.f - k * k));
        } else if (t < 2.f) {
            const float u = t - 2.f;
            Render::SetShaderType(r, 1);
            Render::SetAlpha(r, 1.f - std::sqrt(1.f - u * u));
            k = t;
        }
        if (k >= 0.f) {
            r->h = k * (float)r->tex->h * 1.5f;
            r->w = (float)r->tex->w * k * 1.5f;
        }
        r->w *= GUI::GetHoverScaleFactor(1.f, 1.f);
        r->h *= GUI::GetHoverScaleFactor(1.f, 1.f);
        Render::SetPosition(r, px + r->w * -0.5f, py + r->h * 0.5f, 0.5f);
        a->ringTime += dt;
        if (a->ringTime > 2.f) a->ringTime = 0.f;
    }
}

void MoveWindowDown() {
    for (HoverInfo* h : g_hoverData)
        if (h->window) h->window->MoveWindowDown(false);
    g_wnd->MoveWindowDown(true);
}

// The arrows bob 8 px at 15 rad/s: down-pointing ones vertically, left-pointing ones sideways.
void UpdateArrow() {
    for (HelperArrow* a : g_arrows) {
        Render::Sprite* s = a->sprite;
        if (!s || !s->visible) continue;
        // An arrow that follows somebody (FollowEntity) keeps over their head; when they die it
        // goes, with its callback.
        if (Entity* e = a->entity) {
            if (e->IsDead()) {
                Render::SetVisibility(s, false);
                delete a->onClick;
                a->onClick = nullptr;
                continue;
            }
            float ex = 0.f, ey = 0.f;
            e->GetWorldPos(ex, ey);
            a->x = ex;
            a->y = (ey - (e->GetSprite() ? e->GetSprite()->h : 0.f)) - 20.f;
            if (a->left) {
                if (a->mirror) a->x = ex - s->w;
                a->y += s->h * 0.5f;
            } else {
                if (a->flip) a->y = s->h + ey;
                a->x += s->w * -0.5f;
            }
        }
        double bob = std::cos(Timer::GetTime() * 15.0) * 8.0;
        if (!a->left) Render::SetPosition(s, a->x, (float)((double)a->y + bob), s->z);
        else Render::SetPosition(s, (float)((double)a->x + bob), a->y, s->z);
    }
}

void ArrowAt(float x, float y, bool hide, bool left, bool mirror, bool flip, bool screenSpace, bool tablet,
             bool newArrow) {
    // The attention rings: asked for (or second-tutorial steps 0x86, 0x87), on a tablet.
    const bool rings = (tablet || (unsigned)(GameState::secondTutorial - 0x86) < 2) && GUI::IsTabletVersion();
    if (hide) {
        HideArrow();
        return;
    }
    if (newArrow) g_arrows.push_back(new HelperArrow());
    HelperArrow* a = g_arrows.back();
    if (!a->sprite) a->sprite = Render::CreateSprite(IconManager::GetIcon("Arrow_show"), Render::kLayer15, false, false);
    if (rings && !a->ring1) {
        Render::Texture* t = Resources::GetDirectImage("images/attention_ring2.jpg");
        a->ring1 = Render::CreateSprite(t, Render::kLayer15, false, false);
        a->ring2 = Render::CreateSprite(t, Render::kLayer15, false, false);
    }
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
    a->decor = nullptr;
    a->entity = nullptr;
    if (rings) {
        if (!a->ring1->visible) a->ringTime = 0.f;
        Render::SetVisibility(a->ring1, !GameState::IsPaused());
        Render::SetVisibility(a->ring2, !GameState::IsPaused());
    }
    g_canHideArrow = true;
    // After the tutorial, in a boss or arena fight, a world arrow off screen brings the camera to
    // it.
    if (!screenSpace && GameState::secondTutorial == 0x100 && GameState::IsCombatActive() &&
        GameState::GetActiveCombat()->GetType() > 2) {
        int sx = (int)a->x, sy = (int)a->y;
        Map::WorldCoordinatesToScreen(sx, sy);
        const bool onScreen = sx > -1 && (float)sx + s->w <= (float)GUI::ScreenWidth() && (float)sy - s->h >= 0.f &&
                              sy <= GUI::ScreenHeight();
        if (!onScreen) Render::CenterOn(a->x, a->y, true);
    }
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

// @0x2635c0: the Back key closes the tapped building's info window (unlike Hide, whether or not it
// is shown, and without bringing the HUD back).
bool OnBack() {
    if (!g_currentHover) return false;
    WindowManager::WindowHide(false);
    g_wnd->shown = false;
    g_currentHover->Hide();
    g_currentHover = nullptr;
    return true;
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
        HideWorldDialog(nullptr);
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

// The tutorial's end as its last drop is picked up in the short tutorial (type 1): both
// tutorials are over. (FileManager::SendTutorialFinished2: online.)
static void AfterDropCollected() {
    int& step = GameState::tutorial;
    if (step == 0x15) {
        if (!g_itemDrops.empty()) step = 0x14;
    } else if (step == 0x57) {
        HideArrow();
        Tasks::Update(true);
        HUDWindow::UpdateTasks();
        if (!g_itemDrops.empty()) {
            step = 0x56;
        } else if (GameState::GetTutorialType() == 1) {
            GUI::SetInteractionLock(false);
            step = 0x80;
            GameState::secondTutorial = 0x100;
            TutorialSender::AddTutorialStep(0x100);
            GameState::lastSentStep = 0x100;
        } else {
            step = 0x58;
        }
    }
}

// @0x26e1a0: the drop of that item (the last such; 0: the first drop) is picked up as by a tap,
// and the tutorial steps that wait for it move on.
void CollectOne(uint32_t item) {
    if (!g_itemDrops.empty()) {
        size_t at = 0;
        if (item != 0)
            for (size_t i = 0; i < g_itemDrops.size(); ++i)
                if (g_itemDrops[i].item && g_itemDrops[i].item->id == item) at = i;
        g_itemDrops[at].Collect(false, true);
        g_itemDrops[at].sprite = Render::RemoveSprite(g_itemDrops[at].sprite);
        g_itemDrops[at].glow = Render::RemoveSprite(g_itemDrops[at].glow);
        g_itemDrops[at] = g_itemDrops.back();
        g_itemDrops.pop_back();
    }
    if (GameState::tutorial == 0x49) {
        HideWorldDialog(nullptr);
        HideArrow();
    }
    AfterDropCollected();
    int& second = GameState::secondTutorial;
    if (second == 0x85) {
        HideWorldDialog(nullptr);
        HideArrow();
        second = g_itemDrops.empty() ? 0x86 : 0x84;
        return;
    }
    if (second != 0x94) return;
    GUI::SetInteractionLock(true);
    HideArrow();
    second = 0x95;
    BeltBarWindow::UpdateContents();
}

// FUN_002643e8: the arrow callback that picks up one drop.
static GUI::Callback* CollectOneCallback(uint32_t item) {
    return new GUI::Callback([item] { CollectOne(item); });
}

static void ArrowAtDrop(const ItemDrop& d) {
    ArrowAt(d.sprite->x + d.sprite->w * 0.5f, d.sprite->y - d.sprite->h, false, false, false, false, false, false, false);
}

// BuildingHovers::Update's first part: the tutorial steps that wait for a drop to be picked up
// keep the drops from being collected by themselves (128 s to live) and point the arrow at one.
static void UpdateTutorialDrops() {
    auto keep = [] { for (ItemDrop& d : g_itemDrops) d.lifetime = 128.f; };
    int& step = GameState::tutorial;
    if (step == 0x48 || step == 0x49) {
        keep();
        if (!g_itemDrops.empty()) ArrowAtDrop(g_itemDrops[0]);
        if (step == 0x48 && !g_itemDrops.empty()) {
            SetArrowClickCallback(CollectOneCallback(0));
            GUI::SetInteractionObjectLock(g_itemDrops[0].sprite, nullptr);
            step = 0x49;
        }
    } else if (step == 0x21) {
        // The castle's first taxes: after 28 s the arrow's tap takes them all.
        if (!g_itemDrops.empty()) {
            if (g_itemDrops[0].lifetime < 100.f) SetArrowClickCallback(new GUI::Callback(CollectAll));
            ArrowAtDrop(g_itemDrops[0]);
            keep();
        }
    } else {
        if (step == 0x14 || step == 0x15) {
            keep();
            // The key (item 0x254) is picked up last.
            if (!g_itemDrops.empty()) {
                const ItemDrop& last = g_itemDrops.back();
                if (!last.item || last.item->id != 0x254) {
                    for (size_t i = 0; i < g_itemDrops.size(); ++i) {
                        if (!g_itemDrops[i].item || g_itemDrops[i].item->id != 0x254) continue;
                        std::swap(g_itemDrops[i], g_itemDrops.back());
                        break;
                    }
                }
            }
            if (!g_itemDrops.empty() && !CharacterInfoWindow::IsVisible()) {
                GUI::SetInteractionObjectLock(g_itemDrops[0].sprite, nullptr);
                ArrowAtDrop(g_itemDrops[0]);
            }
            if (step == 0x14 && !g_itemDrops.empty()) {
                SetArrowClickCallback(CollectOneCallback(0));
                step = 0x15;
            }
        }
        if (step == 0x56 || step == 0x57) {
            keep();
            if (!g_itemDrops.empty()) ArrowAtDrop(g_itemDrops[0]);
            if (step == 0x56 && !g_itemDrops.empty()) {
                SetArrowClickCallback(CollectOneCallback(0));
                step = 0x57;
            }
        }
    }
    int& second = GameState::secondTutorial;
    if (second >= 0x83 && second <= 0x85) {
        keep();
        if (second != 0x83 && !g_itemDrops.empty()) {
            ArrowAtDrop(g_itemDrops[0]);
            PreventArrowAutoHide();
        }
        if (second == 0x84 && !g_itemDrops.empty()) {
            SetArrowClickCallback(CollectOneCallback(0));
            GUI::SetInteractionObjectLock(g_itemDrops[0].sprite, g_itemDrops.back().sprite);
            second = 0x85;
        }
    } else if (second >= 0x8a && second <= 0x9e) {
        keep();
        if ((second == 0x93 || second == 0x94) && !g_itemDrops.empty() && !g_itemDrops[0].animating)
            CollectOne(g_itemDrops[0].item->id);
        if (second == 0x93 && !g_itemDrops.empty()) {
            GUI::SetInteractionObjectLock(g_itemDrops[0].sprite, nullptr);
            second = 0x94;
        }
    } else if (second == 0x9f || second == 0xa0) {
        // The dropped item 0x6b gets the arrow.
        for (size_t i = 0; i < g_itemDrops.size(); ++i) {
            if (!g_itemDrops[i].item || g_itemDrops[i].item->id != 0x6b) continue;
            g_itemDrops[i].lifetime = 128.f;
            ArrowAtDrop(g_itemDrops[i]);
            if (second == 0x9f) {
                SetArrowClickCallback(CollectOneCallback(0x6b));
                GUI::SetInteractionObjectLock(g_itemDrops[i].sprite, nullptr);
                second = 0xa0;
            }
        }
    }
    // The emperor's map: its one dropped item keeps its arrow (DropItem sets it up).
    if (GameState::GetCurrentMapID() == 0xb && WindowManager::GetShownWindowCount() == 0 && g_itemDrops.size() == 1) {
        g_itemDrops[0].lifetime = 128.f;
        if (!g_arrows.empty() && g_arrows[0]->onClick && g_arrows[0]->itemArrow) ArrowAtDrop(g_itemDrops[0]);
    }
    if (GameState::IsTameTutorial()) {
        keep();
        if (!g_itemDrops.empty()) {
            GUI::SetInteractionObjectLock(g_itemDrops[0].sprite, nullptr);
            if (!ArrowVisible()) SetArrowClickCallback(CollectOneCallback(0));
            ArrowAtDrop(g_itemDrops[0]);
        }
    }
}

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
    // The storage, resource, living and factory info windows drop it.
    for (BuildingHoverWindow* w : {(BuildingHoverWindow*)g_storageWindow, (BuildingHoverWindow*)g_resourceActiveWindow,
                                   (BuildingHoverWindow*)g_livingWindow, (BuildingHoverWindow*)g_factoryWindow})
        if (w && w->building == b) w->SetBuilding(nullptr);
}

void UnregisterDecoration(Map::Decor* d) {
    for (size_t i = 0; i < g_hoverData.size(); ++i) {
        if (g_hoverData[i]->decor != d) continue;
        RemoveHover(i);   // only the first match
        break;
    }
    for (HelperArrow* a : g_arrows) {
        if (a->decor != d) continue;
        a->decor = nullptr;
        if (a->sprite) Render::SetVisibility(a->sprite, false);
    }
}

void UnregisterEntity(Entity* e) {
    for (size_t i = 0; i < g_hoverData.size();) {
        if (g_hoverData[i]->entity != e) { ++i; continue; }
        RemoveHover(i);
        ++i;
    }
    if (g_worldDialog && g_worldDialog->entity == e) {
        g_worldDialog->Hide();
        g_worldDialog->SetEntity(nullptr);
    }
    for (HelperArrow* a : g_arrows)
        if (a->entity == e) a->entity = nullptr;
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
            // (During a city battle, GameState::g_Combat, damaged buildings show the tiny health bar
            // (0xe) instead: the siege system, dead in 5.11.)
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
                } else if (cls == 0xc && SoldierSlots::GetOccupiedSlotCount() == 0 && !GameState::IsCityTutorial()) {
                    // A training ground calls for soldiers while the squad's slots are empty.
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
                // (In a city battle (g_Combat: the siege system, dead in 5.11) a damaged decoration
                // would show the health bar (0xe) instead.)
                // A decoration being searched by a worker shows the progress; one that wants an item
                // the cogs.
                if (d->meta && d->WorkerIsWorking() && !d->GetPlayerSearchAnimation(nullptr)) {
                    h->SetHoverType(kBuildProgress);
                    goto checked;
                }
                if (d->ShowUseItemHover() && d->sprite && d->sprite->visible) {
                    h->SetHoverType(kUseItem);
                    goto checked;
                }
                const GameData::DecorData* dd = d->data;
                if (dd && dd->collectTime != 0 && GameState::GetCurrentMapID() == 0 && d->patch->owned &&
                    Timer::GetGlobalTime() > dd->collectTime + d->collectStart) {
                    h->SetHoverType(kTaxes);
                    goto checked;
                }
            }
            if (e && !e->IsDisappearing() && e->IsAppeared()) {
                // Somebody with a talk task (not done yet) or a special talk shows the talk button.
                if (e->GetTalkTask() != 0) {
                    if (GameState::TaskCompleted(e->GetTalkTask())) {
                        e->AssignTalkTask(0);
                    } else {
                        Map::Patch* p = GameState::GetCurrentMapID() == 0
                                            ? Map::GetPatchForCoordinates(e->tileX, e->tileY, false)
                                            : nullptr;
                        h->SetHoverType(p && !p->owned ? kNone : kTalk);
                        e->AssignSpecialTalk(0);
                    }
                    goto checked;
                }
                if (e->GetSpecialTalk() != 0) {
                    h->SetHoverType(kTalk);
                    goto checked;
                }
                // The guide (spawn point 0x1d7) offers the travel choice once task 0x43f has begun.
                if (e->GetSpawnPointID() == 0x1d7 && !IsWorldDialogVisible()) {
                    e->AssignSpecialTalk(e->GetTalkTask() == 0 && GameState::GetTaskBeginTime(0x43f) != 0 ? 0x1d7 : 0);
                    goto checked;
                }
                // The health bar: everyone in a PvP arena fight (type 6); a monster that is fighting
                // or wounded, if the fog does not hide it (in the arenas: always).
                BaseCombat* combat = GameState::GetActiveCombat();
                if ((e->IsEnemy() || e->IsPlayer()) && e->IsActive() && !e->IsDead() && combat && combat->GetType() == 6) {
                    h->SetHoverType(kHealthbarTiny);
                    goto checked;
                }
                // (While GameState::g_Combat is set, the siege, every living entity has one.)
                if (e->IsEnemy() &&
                    (e->f36 || (e->GetHP() != 0 && e->GetHP() != e->GetHpMax() && e->IsActive()))) {
                    if (combat && (combat->GetType() == 5 || combat->GetType() == 6)) {
                        h->SetHoverType(kHealthbarTiny);
                    } else if (Render::Sprite* sp = e->GetSprite()) {
                        h->SetHoverType(Fog::IsVisible((int)sp->x, (int)(sp->y - sp->h), (int)(sp->x + sp->w), (int)sp->y)
                                            ? kHealthbarTiny
                                            : kNone);
                    }
                    goto checked;
                }
                // A boss that is coming back shows its timer once a fight against it has started.
                if (e->IsBoss() && e->IsActive() && !e->IsDead() && e->GetEntityData()->restoreTime != 0) {
                    if (GameState::GetBossFightStartTime((uint32_t)GameState::GetCurrentMapID(), e->GetSpawnPointID()) != 0)
                        h->SetHoverType(kBossTime);
                    goto checked;
                }
                // A ripe soil patch on the farm view shows its crop.
                if (e->GetAI() && e->GetAI()->GetState() == 0x19 && Map::GetCurrentFarm()) {
                    h->SetHoverType(kFarmReady);
                    goto checked;
                }
                // A PvP rival on the map (an entity with a GUID) shows his name while the fog
                // does not hide him.
                if (e->GetGUID() != 0 && e->IsActive() && !e->IsDead()) {
                    if (Render::Sprite* sp = e->GetSprite())
                        h->SetHoverType(Fog::IsVisible((int)sp->x, (int)(sp->y - sp->h), (int)(sp->x + sp->w), (int)sp->y)
                                            ? kPlayerName
                                            : kNone);
                    goto checked;
                }
            }
            h->SetHoverType(kNone);
        } else {
            // (Visiting a friend's city, their goblins (class 0x16) show 0x13: online.)
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
        // The second tutorial's orb (step 0x94) is brought into view where it lands.
        if (GameState::secondTutorial == 0x94) {
            int sx = (int)(x + sprite->w * 0.5f), sy = (int)(y + sprite->h * -0.5f);
            Map::WorldCoordinatesToScreen(sx, sy);
            if (!(0x4a < sx && sx < Render::ScreenWidth() - 0x4a && 0x4a < sy && sy < Render::ScreenHeight() - 0x4a))
                Render::CenterOn(x + sprite->w * 0.5f, y - 180.f, true);
        }
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

void ItemDrop::Collect(bool automatic, bool showWindow) {
    for (int& s : subtasks) {
        if (s != 0) Tasks::CompleteSubtask(Tasks::kDeliveryMissionEnd, (uint32_t)s, 1);
        s = 0;
    }
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
        if (GameState::tutorial == 0x21) {
            HideArrow();
            GameState::tutorial = 0x22;
        }
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
        SoundsManager::PlaySound(type == GameState::kGold ? "collect_gold" : "collect_resource", 1, false);
        // Off the city the map's loot list counts it (FoundItemsWindow shows it at the end).
        if (GameState::GetCurrentMapID() != 0)
            GameState::AddMapResourceCollectionInfo((uint32_t)GameState::GetCurrentMapID(), type, (int)amount);
    }
    if (kind == 1) {
        uint32_t id = item->id;
        // The starter chest 0x2b6 (with showWindow: shown in the chests window, 0x3f with the setting
        // starter_chest_b, else 0x35) starts its task 0x75b over, as featured now.
        if (id == 0x2b6) {
            if (showWindow) {
                ChestsWindow::Show();
                ChestsWindow::SetStarterChestMode(false, Setting("starter_chest_b").GetInt() ? 0x3f : 0x35);
            }
            GameState::ResetTask(0x75b);
            GameState::StartTask(0x75b);
            GameState::SetFeaturedTaskClickTime(0x75b, (uint32_t)Timer::GetGlobalTime());
            HUDWindow::UpdateTasks();
        }
        bool timesOnly = id - 0x16cu < 2;
        if (timesOnly) g_rubyDropped = false;
        if (Professions::Profession* p = Professions::GetProfessionForPointItem(id)) {
            // A profession's point item: its points (a tap on a profession's shows them at the belt).
            if (!automatic && p->type == Professions::kProfession) {
                int sx = (int)sprite->x, sy = (int)sprite->y;
                Map::WorldCoordinatesToScreen(sx, sy);
                BeltBarWindow::ShowProfessionPoint((float)sx, (float)sy, p, GameState::GetProfessionPoints(p->id));
                automatic = true;   // the crafting bar takes the picture, it does not fly to the belt
            }
            Professions::AddProfessionPoint(p->id, amount);
        } else if (id == 0x26a) {
            EntityManager::GetPlayer()->AddHP((int)amount);
        } else {
            GameState::AddItem(id, (int)amount, false, timesOnly, false);
            BeltBarWindow::AutoBind(id);
            BeltBarWindow::UpdateContents();
        }
        Render::Sprite* s = nullptr;
        if (!automatic) {
            sprite->w *= GUI::GetHudScaleFactor();
            sprite->h *= GUI::GetHudScaleFactor();
            s = sprite;
        }
        OnCollect(id, s, false, false, 0xb, timesOnly);
        if (amount < 2) {
            ShowTextHoverWithStyle(x, y, item->title, (int)item->itemLevel + 0xe, 2.f, 50.f, false, false);
        } else {
            std::u32string text = SWPrintf(0x100, U"+%d %s", {amount, item->title});
            ShowTextHoverWithStyle(x, y, text.c_str(), (int)item->itemLevel + 0xe, 2.f, 50.f, false, false);
        }
        SoundsManager::PlaySound("collect_item", 1, false);
        // Away from the city the map's loot list counts it (not item 0x88 in the city tutorial).
        if (GameState::GetCurrentMapID() != 0 && !(GameState::IsCityTutorial() && item->id == 0x88))
            GameState::AddMapItemCollectionInfo((uint32_t)GameState::GetCurrentMapID(), item->id, (int)amount);
        if (item->id == 0x6b && GameState::secondTutorial == 0xa0) {
            HideArrow();
            GameState::secondTutorial = 0xa1;
        }
        if (item->id == 0x88 && GameState::secondTutorial == 0x94) GUI::SetInteractionLock(true);
        if (GameState::GetCurrentMapID() == 0xb) HideArrow();
        return;
    }
    if (kind != 2) return;
    // (HideWorldDialog(nullptr) first: there is no world dialog yet)
    GameState::ChangeResourceAmount(type, (int)amount);
    OnCollect(0, automatic ? nullptr : sprite, false, false, 0xb, false);
    std::u32string text = SWPrintf(0x100, U"+%d %s", {amount, GameState::GetResourceGameName(type)});
    ShowTextHover(x, y - (float)sprite->tex->h, text.c_str(), 1.f, 1.f, 1.f, 0.f, 0.f, 0.f, 0x19, 5, 5.f, true,
                  2.f, 50.f);
    SoundsManager::PlaySound("collect_farm_item", 1, false);
    if (GameState::tutorial == 0x49) GameState::tutorial = 0x4c;
}

void DropFarmFood(float x, float y, int type, unsigned amount, Render::Texture* tex, unsigned dropId) {
    if (!tex) {
        std::puts("BuildingHovers::DropFarmFood() Cannot drop item - no image");
        Tasks::CompleteSubtask(Tasks::kDeliveryMissionEnd, dropId, 1);
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
    SoundsManager::PlaySound("farm_item_dropped", 1, false);
    if (GameState::tutorial == 0x48) GUI::SetInteractionObjectLock(g_itemDrops.back().sprite, nullptr);
}

// @0x26fafc: with quest 0x4d7 (or 0x4d9) running, a drop may come with a ruby (0x16c, or 0x16d),
// at "ruby_find_chance" percent; one at a time (picking it up allows the next).
void CheckForSpecialDrops(float x, float y) {
    for (const auto& [task, id] : {std::pair<uint32_t, uint32_t>{0x4d7, 0x16c}, {0x4d9, 0x16d}}) {
        if (!Tasks::IsTaskActive(task, true) || g_rubyDropped) continue;
        const long roll = Rand48::lrand48();
        if (!((float)(roll % 100) < GameState::GetSetting("ruby_find_chance"))) continue;
        g_rubyDropped = true;
        Items::ItemInfo* info = Items::GetItemInfo(id);
        g_itemDrops.emplace_back();
        ItemDrop& d = g_itemDrops.back();
        d.kind = 1;
        d.item = info;
        d.amount = 1;
        d.sprite = Render::CreateSprite(info->GetDropIcon(), 0xc, false, false);
        long r = Rand48::lrand48();
        float vx;
        if (!GameState::IsTutorial()) vx = (float)(r % 0x3c) - 30.f;
        else vx = (float)(Rand48::lrand48() % 0x28) - 10.f;
        const float hw = (float)(info->GetDropIcon()->w / 2), hh = (float)(info->GetDropIcon()->h / 2);
        d.StartAnimation(x - hw, y - hh, vx, 0.f, (int)(Rand48::lrand48() % 0x28) + 0x5a);
        d.glow = Render::CreateSprite(IconManager::GetIcon("Pickup_item_glow"), 0xb, false, false);
        Render::SetShaderType(d.glow, 8);
        Render::SetVisibility(d.glow, false);
    }
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
    SoundsManager::PlaySound("item_dropped_light", 1, false);
    CheckForSpecialDrops(x, y);
    if (GameState::tutorial == 0x14) GUI::SetInteractionLock(false);
    if (GameState::tutorial == 0x21 && type == GameState::kGold)
        GUI::SetInteractionObjectLock(g_itemDrops.back().sprite, nullptr);
}

void ItemDrop::StartAnimation(float px, float py, float pvx, float pdelay, int h) {
    if (GameState::GetCurrentMapID() != 0) {
        float limit = ((float)(unsigned)Map::GetGridHeight() * 42.f * 0.5f - 42.f) + (float)h * -1.5f;
        if (py > limit) py = limit;
    }
    if (Map::GetCurrentFarm() && py > 3000.f) py = 2995.f;
    vx = pvx;
    delay = pdelay;
    x = px;
    y = py;
    bounce = h;
    animating = true;
    if (GUI::IsSmallScreenVersion() && GameState::SecondTutorialStep() != 0x100) {
        bounce = (int)((float)h / 1.5f);
        vx /= 3.f;
    }
    groundY = py;
    vy = 0.f;
    Render::SetPosition(sprite, px, py, 0.2f);
    Render::SetVisibility(sprite, false);
}

void DropItem(float x, float y, Items::ItemInfo* info, unsigned subtask, unsigned amount) {
    if (!info) {
        std::puts("BuildingHovers::DropItem() Cannot drop item - no info");
    } else {
        if (GameState::IsTameTutorial() && info->id == 0x62) return;
        if (info->type == 0x10 && Setting("item_boosts").GetInt() == 0) return;
        if (info->IsLimited() && (amount = (unsigned)info->ApplyLimitToAmount((int)amount)) == 0) return;
        // (GameState::GetServerSetting(0xb3) is 0 offline; with it items 0x38..0x3c went straight in)
        if (info->GetDropIcon()) {
            if (info->activatedByQuest != 0 && !GameState::TaskCompleted(info->activatedByQuest)) return;
            if (info->id == 0x86 && (GameState::SecondTutorialStep() == 0x81 || GameState::IsFirstVirtualTutorial()))
                GameState::secondTutorial = 0x82;
            g_itemDrops.emplace_back();
            ItemDrop& d = g_itemDrops.back();
            d.kind = 1;
            d.amount = amount;
            d.item = info;
            d.sprite = Render::CreateSprite(info->GetDropIcon(), 0xc, false, false);
            long r = Rand48::lrand48();
            float vx;
            if (!GameState::IsTutorial()) vx = (float)(r % 0x3c) - 30.f;
            else vx = (float)(Rand48::lrand48() % 0x28) - 10.f;
            float hw = (float)(info->GetDropIcon()->w / 2), hh = (float)(info->GetDropIcon()->h / 2);
            d.StartAnimation(x - hw, y - hh, vx, 0.f, (int)(Rand48::lrand48() % 0x28) + 0x5a);
            for (int& s : d.subtasks)
                if (s == 0) {
                    s = (int)subtask;
                    break;
                }
            d.glow = Render::CreateSprite(IconManager::GetIcon("Pickup_item_glow"), 0xb, false, false);
            Render::SetShaderType(d.glow, 8);
            Render::SetVisibility(d.glow, false);
            CheckForSpecialDrops(x, y);
            if (info->type == 8) SoundsManager::PlaySound("chest_opened", 1, false);
            else SoundsManager::PlaySound(info->itemLevel != 0 ? "item_dropped_special" : "item_dropped_light", 1, false);
            if (GameState::tutorial == 0x56) GUI::SetInteractionObjectLock(g_itemDrops.back().sprite, nullptr);
            // The emperor's map: the arrow's tap picks the item up (see UpdateTutorialDrops).
            if (GameState::GetCurrentMapID() == 0xb) {
                SetArrowClickCallback(CollectOneCallback(info->id));
                if (!g_arrows.empty()) g_arrows[0]->itemArrow = true;
            }
            return;
        }
        std::puts("BuildingHovers::DropItem() Cannot drop item - no image");
        GameState::AddItem(info->id, 1, false, false, false);
    }
    if (subtask != 0) Tasks::CompleteSubtask(Tasks::kDeliveryMissionEnd, subtask, 1);
}

bool HasDroppedItem(unsigned id, float& x, float& y) {
    for (ItemDrop& d : g_itemDrops) {
        if (id != 0 && !(d.item && d.item->id == id)) continue;
        if (!d.sprite) continue;
        x = d.sprite->x + d.sprite->w * 0.5f;
        y = d.sprite->y - d.sprite->h;
        return true;
    }
    return false;
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

void OnCollect(unsigned item, Render::Sprite* sprite, bool screenSpace, bool glow, int type, bool noBelt) {
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
    else {
        if (Professions::GetProfessionForPointItem(item)) {
            // A profession's point item flies to the crafting bar's badge.
            m.endX = GUI::GetHudScaleFactor() * 235.f;
            m.endY = sprite->h;
        } else if ((item == 0xaf && GameState::tutorial == 0x15) || GameState::tutorial == 0x57) {
            // The tutorial's piece flies to the hero's portrait.
            m.endX = 18.f;
            m.endY = sprite->h + 18.f;
        } else if (!noBelt) {
            int slot = -1;
            for (unsigned i = 0; i < GameState::GetBeltSlotCount() && slot == -1; ++i)
                if (GameState::GetBeltItemAt(i) == item) slot = (int)i;
            if (GameState::SecondTutorialStep() == 0x94) slot = 1;
            int tx = 0, ty = 0;
            // (step 0x85, or no belt slot: the open character window's worn belt, else the
            // inventory button)
            if ((GameState::SecondTutorialStep() == 0x85 || slot == -1) &&
                !CharacterInfoWindow::CanMoveIntoEquippedBelt(tx, ty)) {
                BeltBarWindow::GetAdjustedSideButtonLocation(tx, ty, (int)sprite->w, (int)sprite->h);
                m.endX = (float)tx;
                m.endY = (float)(int)((float)ty + m.sprite->h);
                // Over the open character, item shop, craft item or PvP window it draws on layer 0xf.
                if (CharacterInfoWindow::IsVisible() || ItemShopWindow::IsVisible() || CraftItemWindow::IsVisible() ||
                    PlayerCompetitionWindow::IsVisible())
                    Render::ChangeLayer(m.sprite, 0xf);
            } else if (GameState::SecondTutorialStep() == 0x85 || slot == -1) {
                m.endX = (float)tx;
                m.endY = (float)(int)((float)ty + m.sprite->h);
                Render::ChangeLayer(m.sprite, 0xf);
            } else {
                BeltBarWindow::GetBeltItemLocation(tx, ty, slot);
                m.endX = (float)tx;
                m.endY = (float)(int)((float)ty + m.sprite->h);
                Render::ChangeLayer(m.sprite, 0xf);
            }
        } else {
            m.endX = (float)(int)((sprite->w * -0.5f + (float)Render::ScreenWidth() * 0.5f) - 40.f);
            m.endY = (float)(int)(18.f + sprite->h * 0.5f);
        }
    }
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
    // The chest's loot of the opening tutorial: the resource bar comes with the first piece, the
    // belt with the net (item 0x254), which starts the taming lesson (step 0x17).
    if (item != 0xaf && GameState::tutorial == 0x15) TopCityWindow::Show();
    if (item == 0x254 && GameState::tutorial == 0x15) {
        BeltBarWindow::Show();
        GameState::tutorial = 0x17;
    }
    if (GameState::IsTameTutorial() && item == 0x254) {
        BottomBattleWindow::Show();
        Entity* wolf = EntityManager::GetEntityAtXY(7, 0x10);
        if (GameState::tutorial != 0x17) wolf = EntityManager::GetEntityAtXY(5, 0xb);
        if (wolf) {
            GUI::SetInteractionObjectLock(wolf, nullptr);
            AIBaseState* ai = EntityManager::GetPlayer()->GetAI();
            SetArrowClickCallback(new GUI::Callback([ai, wolf] { ai->ClickedEntity(wolf); }));
            ArrowAt(wolf->worldX, wolf->worldY - wolf->GetIdleHeight(), false, false, false, false, false, false, false);
            Render::CenterOn(wolf->worldX - 100.f, wolf->worldY, true);
        }
        if (GameState::tutorial == 0x17) PlayerTopWindow::SetParameters(StringTable::GetString("DESC1911"), 0, false, true);
    }
    // Item 0x21d picked up on map 0x52 before task 0x5a5 is done: the character window on it.
    if (item == 0x21d && !GameState::TaskCompleted(0x5a5) && GameState::GetCurrentMapID() == 0x52) {
        CharacterInfoWindow::Show();
        CharacterInfoWindow::ShowInventoryItem(0x21d, 0, nullptr);
    }
}

// ---- text popups ----

void ShowTextHover(float x, float y, const char32_t* text, float r, float g, float b, float glowR,
                   float glowG, float glowB, int fontSize, int glowBlur, float glowStrength,
                   bool linear, float fade, float rise) {
    if (!text) {
        std::puts("BuildingHovers::ShowTextHover() text is NULL");
        return;
    }
    std::u32string plain = StringTable::GetUndecoratedString(text, 0x100);
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
    ti.sprite = Render::CreateText(font, plain.c_str(), r, g, b, 1, false, &glow, nullptr, 0xc);
    g_itemInfo.push_back(ti);
    Render::Sprite* s = ti.sprite;
    float z = 0.3f + (float)g_textCounter * -0.0001f;
    Render::SetPosition(s, x + s->w * -0.5f, y, z);
    if (s->x + s->w > (float)(unsigned)Map::GetGridWidth() * 84.f) Render::SetPosition(s, x - s->w, y, z);
    if (s->x < 0.f) Render::SetPosition(s, 0.f, y, z);
    if (linear && s->tex) Render::SetMagnifyFilter(s->tex, false);
}

// @0x269cec BuildingHovers::ShowIconHover
void ShowIconHover(float x, float y, Render::Texture* tex) {
    if (!tex) {
        std::puts("BuildingHovers::ShowIconHover() tex is NULL");
        return;
    }
    TextInfo ti;
    ti.fade = 2.f;
    ti.rise = 50.f;
    ti.grow = false;
    ti.sprite = Render::CreateSprite(tex, 0xc, false, false);
    g_itemInfo.push_back(ti);
    Render::SetPosition(ti.sprite, x + ti.sprite->w * -0.5f, y, 0.3f);
    Render::SetShaderType(ti.sprite, 1);
    if (ti.sprite->tex) Render::SetMagnifyFilter(ti.sprite->tex, false);
}

// @0x263218 BuildingHovers::FollowDecor
void FollowDecor(Map::Decor* d) { g_arrows.back()->decor = d; }

// @0x2631fc BuildingHovers::FollowEntity (UpdateArrow keeps the arrow over the entity)
void FollowEntity(Entity* e) { g_arrows.back()->entity = e; }

// ---- the world dialog ----

bool IsWorldDialogVisible() { return g_worldDialog && g_worldDialog->shownHover; }

void SetWorldDialogOffset(int x, int y) { g_worldDialog->SetOffset(x, y); }

// @0x268ec8 BuildingHovers::ShowWorldDialog
void ShowWorldDialog(Entity* e, const char32_t* text, uint32_t task, bool arrow, bool nextButton, bool setSide,
                     uint32_t nextTask, bool noCenter, bool sideLeft) {
    if (g_worldDialog->shownHover) {
        if (g_worldDialog->task == task) return;
        std::puts("BuildingHovers::ShowWorldDialog() Closing previous dialog");
        g_worldDialog->OnClose();
        g_worldDialog->Hide();
    }
    if (task == 0x775 || task == 0x777) arrow = true;
    // The speaker's voice, as Entity::OnTalkTo's (here also while a world dialog shows).
    if (e->GetEntityData()->soundCharType == 2) SoundsManager::PlaySound("troll_attack", 1, true);
    else SoundsManager::PlaySound(e->GetTalkSoundName().c_str(), 1, true);
    g_worldDialog->Show();
    g_worldDialog->SetEntity(e);
    // (PORT: a task without a pre-quest text says nothing)
    g_worldDialog->SetParameters(StringTable::GetUndecoratedString(text ? text : U"", 0x200).c_str(), task, nextTask,
                                 e->GetSpawnPointID());
    g_worldDialog->Update(0.f);
    if (!noCenter) g_worldDialog->CenterIfOffScreen();
    if (arrow) g_worldDialog->ShowArrow();
    if (!nextButton) g_worldDialog->HideNextButton();
    if (setSide) g_worldDialog->SetDefaultSideLeft(sideLeft);
    UpdateHovers();
    // Task 0x427's dialog (before task 0x429 is done): a sawmill (0x6b) or farm (0x13, its first
    // patch) running its second order switches to the first; the second's costs come back and
    // the first's are paid.
    if (task == 0x427 && !GameState::TaskCompleted(0x429)) {
        auto swap = [](Map::Building* b, int& slot) {
            if (slot != 2) return;
            slot = 1;
            const Contracts::Contract* c = b->data->delivery;
            if (!c) return;
            const Contracts::ContractMission& first = c->missions[0];
            const Contracts::ContractMission& second = c->missions[1];
            GameState::ChangeResourceAmount(second.priceResource, (int)second.priceResourceCount);
            GameState::ChangeResourceAmount(GameState::kGold, (int)second.price);
            GameState::ChangeResourceAmount(first.priceResource, -(int)first.priceResourceCount);
            GameState::ChangeResourceAmount(GameState::kGold, -(int)first.price);
        };
        if (Map::Building* b = Map::GetBuildingWithID(0x6b)) swap(b, b->contract);
        if (Map::Building* b = Map::GetBuildingWithID(0x13)) swap(b, b->patchContract[0]);
    }
}

// @0x263a6c BuildingHovers::HideWorldDialog
void HideWorldDialog(Entity* e) {
    if (g_worldDialog->shownHover && (g_worldDialog->entity == e || !e)) {
        g_worldDialog->OnClose();
        g_worldDialog->Hide();
    }
    if (e == EntityManager::GetPlayer() || !e) PlayerTopWindow::HideDialog();
}

// @0x263a1c BuildingHovers::HideWorldDialogByTask
void HideWorldDialogByTask(uint32_t task) {
    if (!g_worldDialog->shownHover || g_worldDialog->task != task) return;
    g_worldDialog->OnClose();
    g_worldDialog->Hide();
}

// @0x266c60 BuildingHovers::HideArrowOnDecor
void HideArrowOnDecor(Map::Decor* d) {
    for (HelperArrow* a : g_arrows)
        if (a->decor == d) {
            HideArrow();
            return;
        }
}

// @0x263000 BuildingHovers::AllowDecorClickInBattleOnce
void AllowDecorClickInBattleOnce() { g_skipCombatCheck = true; }

int ShowTextHoverWithStyle(float x, float y, const char32_t* text, int style, float fade, float rise,
                           bool topLayer, bool screenSpace) {
    if (!text) {
        std::puts("BuildingHovers::ShowTextHoverWithStyle() text is NULL");
        return 0;
    }
    std::u32string plain = StringTable::GetUndecoratedString(text, 0x100);
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
    Render::Sprite* s = Render::CreateText(font, plain.c_str(), r, g, b, 1, false, glow, nullptr, topLayer ? 0xf : 0xd);
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
    UpdateTutorialDrops();
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
            if (m.sound) SoundsManager::PlaySound("building_position", 1, false);
            m.sprite = Render::RemoveSprite(m.sprite);
            Render::SortRenderLayer(Render::kLayer15, 1);
            g_itemMoves[i] = g_itemMoves.back();
            g_itemMoves.pop_back();
            if (GameState::secondTutorial == 0x95) BeltBarWindow::UpdateContents();
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
    UpdateArrowRings(fdt);
    g_worldDialog->Update(dt);
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
    HUDWindow::UpdateTasks();
    HUDWindow::SetBottomType(3);
    Map::ShowFarm(true, b);
}

bool Click(int x, int y, bool pressed) {
    if (!GameState::IsPlayerCity() || BuildingMovement::Activated() || BuildingPlacement::Activated() ||
        ShopWindow::IsVisible())
        return false;
    if (ClickOnArrow(x, y, pressed)) return true;
    if (ArrowVisible() && !GameState::IsTutorial() && GUI::CanInteractWith(nullptr) && !pressed) HideArrow();
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
                if (GameState::tutorial == 0x49) HideArrow();
                AfterDropCollected();
                if (GameState::secondTutorial == 0x85) {
                    HideWorldDialog(nullptr);
                    HideArrow();
                    if (!g_itemDrops.empty()) {
                        GameState::secondTutorial = 0x84;
                    } else {
                        GameState::secondTutorial = 0x86;
                        BeltBarWindow::UpdateContents();
                    }
                }
                if (GameState::secondTutorial == 0x94) {
                    GUI::SetInteractionLock(true);
                    HideArrow();
                    HideWorldDialog(nullptr);
                    GameState::secondTutorial = 0x95;
                    BeltBarWindow::UpdateContents();
                }
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
                    HireTroopsWindow::ShowForBuilding(GameState::tutorial == 0x61 ? 0x8fu : b->id, false);
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
                        SoundsManager::PlaySound("building_build_start", 1, true);
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
    // (In a city battle, g_Combat, a building with an active hover is tapped too: dead in 5.11.)
    if (b && !pressed && !BuildingHasActiveHover(b) && OnBuildingClick(b, x, y)) return true;
    if (d && !pressed) return OnDecorClick(d, x, y);
    return false;
}

// @0x26a3b0: a tapped decoration. A locked one, or one with a timed job away from home, goes to
// the hero; a job is checked for its price and given to the hero (away) or a worker (at home);
// Decor::OnClick handles the rest; in the city one that pays taxes shows its info window.
bool OnDecorClick(Map::Decor* d, int x, int y) {
    if (GameState::tutorial == 0x13) HideWorldDialog(nullptr);
    if (GameState::tutorial == 0x14) HideArrow();
    if (GameState::tutorial == 0x3a) {
        HideWorldDialog(nullptr);
        HideArrow();
    }
    Entity* player = EntityManager::GetPlayer();
    bool passes = g_skipCombatCheck;
    g_skipCombatCheck = false;
    if (GameState::IsCombatActive() && !passes) return true;
    if (GameState::GetCurrentMapID() != 0 || !d->patch || d->patch->owned) {
        if (d->IsLocked()) {
            player->GetAI()->ClickedDecoration(d);
            return true;
        }
        if (d->f4c != 0) {
            if (d->WorkerAssigned()) return true;
            NotEnoughWindow::ResetRequirements();
            NotEnoughWindow::SetDescriptionText(StringTable::GetString("REQUIREMENT_GOBLIN"), nullptr);
            for (int i = 0; i < 11; ++i)
                if (d->cost[i] != 0) NotEnoughWindow::AddRequirement(i, (unsigned)d->cost[i]);
            if (!NotEnoughWindow::CheckRequirements()) {
                NotEnoughWindow::SetActionCallback([d, x, y] { OnDecorClick(d, x, y); }, nullptr, false);
                NotEnoughWindow::Show();
                return true;
            }
            if (GameState::GetCurrentMapID() != 0) {
                if (d->f50 != 0) {
                    player->GetAI()->ClickedDecoration(d);
                    return true;
                }
                if (player->GetWorkplaceDecoration() && player->GetWorkplaceDecoration() != d)
                    player->GetAI()->TryToInterruptJob();
                if (player->GetWorkplaceDecoration()) return true;
                // A CRAFT node takes its profession's tool's GATHER_TIME, else 2 s.
                unsigned seconds = 2;
                if (d->meta && d->meta->FindChildData(kExpCraft))
                    if (Professions::Profession* p = Professions::GetProfessionForObject(d->id)) {
                        Items::ItemInfo* tool = Items::GetItemInfo(Professions::GetProfessionTool(p->id, true, nullptr, false));
                        if (tool && tool->meta)
                            if (const MetaData* t = tool->meta->FindChildData(kExpGatherTime)) seconds = (unsigned)t->GetInt();
                    }
                d->f50 = seconds;
                d->AssignWorker(player);
                return true;
            }
            if (Entity* w = EntityManager::GetFreeWorker()) {
                d->AssignWorker(w);
                return true;
            }
            if (EntityManager::GetFirstBusyWorker()) {
                SoundsManager::PlaySound("ui_show_busyworkers", 1, false);
                PopupSelectionWindow::Show(StringTable::GetString("FREE_UP_WORKER"),
                                           StringTable::GetString("NO_FREE_WORKER_DESC"),
                                           StringTable::GetString("FIRE_PERSON"),
                                           IconManager::GetIcon("icon_profession_builder"),
                                           [d] { OnFreeWorkerAssignDecoration(d); },
                                           StringTable::GetString("PERFORM_BUILD"), IconManager::GetIcon("gold_build"),
                                           OnFreeWorkerBuild);
                PopupSelectionWindow::SetMentorIcon();
                return true;
            }
            if (d->f4c != 0 && d->WorkerAssigned() && GameState::GetCurrentMapID() == 0) {
                Render::Sprite* s = d->sprite;
                ShowTextHover(s->x + s->w * 0.5f, s->y - s->h, StringTable::GetString("BUILDER_COMING"), 1.f, 1.f,
                              1.f, 0.f, 0.f, 0.f, 0x19, 5, 5.f, true, 2.f, 50.f);
                return true;
            }
        }
        if (d->OnClick()) return true;
    } else {
        return true;   // a decoration on land the player does not own
    }
    const GameData::DecorData* data = d->GetData();
    BuildingHoverWindow* w = g_currentHover;
    if (data && data->collectTime != 0 && GameState::GetCurrentMapID() == 0 && d->patch && d->patch->owned) {
        for (HoverInfo* h : g_hoverData)
            if (h->window && h->decor == d) return false;
        w = g_currentHover = g_decorationWindow;
    }
    if (!w) {
        // Without a window the tap goes on to the entities: when (x, y) is (0, 0), at the
        // decoration's tile.
        if (x != 0 || y != 0) return false;
        int wx = d->x, wy = d->y;
        Map::TileCoordinatesToWorld(wx, wy);
        EntityManager::OnClick(wx, wy);
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
        // On campaign maps (location 2) a live, active entity's tap is not taken here.
        if (GameState::GetCurrentLocation() == 2 && e->IsActive() && !e->IsDead()) return 0;
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
    // (In a city battle CombatManager::OnBuildingClicked takes the tap: the siege system, dead in 5.11.)
    if (GameState::tutorial == 0x3a) {
        HideArrow();
        HideWorldDialog(nullptr);
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
            HireTroopsWindow::ShowForBuilding(b->id, false);
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
            // A tavern-class building: its own tab, or always the tavern's with "always_open_tavern".
            if (GameState::tutorial != 0x61 && Setting("always_open_tavern").GetInt() == 1)
                HireTroopsWindow::ShowForBuilding(0x8f, false);
            else
                HireTroopsWindow::ShowForBuilding(b->id, false);
        }
    }
    // (Off the city only the hover already chosen, if any, shows.)
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
    if (b->data->buildingClass != 4) SoundsManager::PlaySound("ui_click_building", 1, true);
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
    if (Map::Decor* job = w->GetWorkplaceDecoration()) job->RemoveWorker();
    b->AssignWorker(w, 0);
}

// @0x263af8 BuildingHovers::OnFreeWorkerAssignDecoration
void OnFreeWorkerAssignDecoration(Map::Decor* d) {
    PopupSelectionWindow::Hide();
    if (Entity* w = EntityManager::GetFreeWorker()) {
        d->AssignWorker(w);
        return;
    }
    Entity* w = EntityManager::GetFirstBusyWorker();
    if (!w) return;
    if (Map::Building* job = w->GetWorkplace()) job->RemoveWorker(w);
    if (Map::Decor* job = w->GetWorkplaceDecoration()) job->RemoveWorker();
    d->AssignWorker(w);
}

namespace {
// "No free worker": move a busy one (OnFreeWorkerAssign) or build a house (OnFreeWorkerBuild).
void AskToFreeWorker(Map::Building* b, Render::Texture* icon) {
    SoundsManager::PlaySound("ui_show_busyworkers", 1, false);
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
    // An order making an item drops it, its subtask contract - 1 + delivery id * 10.
    unsigned subtask = b->contract - 1 + b->data->delivery->id * 10;
    if (m.item) DropItem(b->baseX, b->minY, m.item, subtask, 1);
    else Tasks::CompleteSubtask(Tasks::kDeliveryMissionEnd, subtask, 1);
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

// A decoration's taxes: its chest's contents (ChestOpenedWindow), gold and XP drop at its sprite.
void CreateDecorationDrop(Map::Decor* d) {
    if (!d || !d->data || !d->sprite) return;
    if (Chests::ChestInfo* chest = Chests::GetChestInfo(d->data->collectChest)) {
        Chests::DropItem drops[0x20];
        const unsigned n = chest->GenerateContents(drops, 0x20, false);
        FoundItemsWindow::ClearCollected();
        FoundItemsWindow::SetHeader(StringTable::GetString("CAMPAIGN_INFO_FOUND"));
        for (unsigned i = 0; i < n; ++i) {
            if (drops[i].type == 0xb) FoundItemsWindow::AddCollectedItem(drops[i].item, drops[i].amount);
            else FoundItemsWindow::AddCollectedResource((int)drops[i].type, drops[i].amount);
        }
        ChestOpenedWindow::SetName(chest->name);
        ChestOpenedWindow::Show();
    }
    Render::Sprite* s = d->sprite;
    if (d->data->collectMoney != 0)
        DropResource(s->x + s->w * 0.5f, s->y + s->h * -0.5f, GameState::kGold, d->data->collectMoney, false, false);
    if (d->data->collectExp != 0)
        DropResource(s->x + s->w * 0.5f, s->y + s->h * -0.5f, GameState::kExpAfter, d->data->collectExp, false, false);
}

}  // namespace BuildingHovers
