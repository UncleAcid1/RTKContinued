#include "game/BuildingPlacement.h"

#include <cstdio>
#include <vector>

#include "engine/Render.h"
#include "engine/Resources.h"
#include "engine/Timer.h"
#include "game/AI.h"
#include "game/Building.h"
#include "game/BuildingHovers.h"
#include "game/EntityManager.h"
#include "game/GameData.h"
#include "game/GameState.h"
#include "game/HiddenObjects.h"
#include "game/Map.h"
#include "game/StringTable.h"
#include "gui/GUI.h"
#include "hud/HUD.h"
#include "hud/HoverWindows.h"
#include "windows/Windows.h"

namespace BuildingPlacement {
namespace {

PlaceBuildingHoverWindow* g_hover = nullptr;   // 0x630ba0
bool g_active = false;                         // 0x630ba4
bool g_dragging = false;                       // 0x630ba5 the preview follows the pointer
Map::Building* g_building = nullptr;           // 0x630ba8 the preview (a Duplicate)
Map::Decor* g_decor = nullptr;                 // 0x630bac
std::vector<AI::Waypoint*> g_waypoints;        // 0x630bb0 (the free-spot search)
HiddenObjects g_hidden;                        // 0x630bc0 fake decorations under the footprint
int g_extraGold = 0;                           // 0x630bd8 (UpdateCost)
float g_flyTime = 0.f;                         // 0x630bdc the shop icon's flight; input waits
Render::Texture* g_red[4] = {};                // 0x630be0 R, UP, border, mid: blocked footprint
Render::Texture* g_influence[4] = {};          // 0x630bf0 the same pieces: free footprint
Render::Sprite* g_footprint = nullptr;         // 0x630c00 the footprint tiles (a sprite chain)
int g_dragDistX = 0, g_dragDistY = 0;          // 0x630c04 0x630c08 pointer travel of this drag
float g_dragX = 0.f, g_dragY = 0.f;            // 0x630c0c 0x630c10 the drag in world units
int g_grabX = 0, g_grabY = 0;                  // 0x630c14 0x630c18 the grab point minus the drag
bool g_fromPresents = false;                   // 0x630c1c a present: free and built at once
bool g_declineOnAccept = false;                // 0x630c1d
// 0x630c20 PresentsContainer::Present* and 0x630c24 (a sprite chain removed by Accept): presents
// are milestone 4; nothing sets them yet.

// @0x3a1b18
void Activate() {
    g_active = true;
    BuildingHovers::SetHoverVisiblity(false, false);
    g_extraGold = 0;
    g_hidden.Clear(true);
}

// @0x3a1fa0
void RemoveBuildingPlacementError() {
    while (g_footprint) g_footprint = Render::RemoveSprite(g_footprint);
    g_hidden.ShowAll();
}

// @0x3a20ec: one sprite per footprint tile, red where it is blocked, else the green influence
// pieces: corners (R, mirrored for the left), tips (UP, flipped for the top), borders and the
// middle. Fake decorations under it fade to 0.2.
void CreateBuildingPlacementError(bool blocked) {
    RemoveBuildingPlacementError();
    int sx = 0, sy = 0, w = 0, h = 0;
    if (g_building) {
        g_building->GetStartTile(sx, sy);
        g_building->GetBuildZone(w, h);
    } else if (g_decor) {
        g_decor->GetStartTile(sx, sy);
        const GameData::DecorData* d = GameData::GetDecoration(g_decor->id);
        w = d->w;
        h = d->h;
    } else {
        return;
    }
    for (int row = 0; row < h; ++row) {
        int x = sx, y = sy;
        bool firstRow = row == 0, lastRow = row == h - 1;
        for (int col = 0; col < w; ++col) {
            bool lastCol = col == w - 1;
            int piece;
            bool mirror, flip;
            if (w == 1 && h == 1) {
                piece = 3, mirror = false, flip = false;
            } else if (col == 0 && firstRow) {
                piece = 1, mirror = false, flip = true;
            } else if (lastCol && firstRow) {
                piece = 0, mirror = false, flip = false;
            } else if (col == 0 && lastRow) {
                piece = 0, mirror = true, flip = false;
            } else if (lastCol && lastRow) {
                piece = 1, mirror = false, flip = false;
            } else if (firstRow) {
                piece = 2, mirror = true, flip = false;
            } else if (lastRow) {
                piece = 2, mirror = false, flip = true;
            } else if (col == 0) {
                piece = 2, mirror = false, flip = false;
            } else if (lastCol) {
                piece = 2, mirror = true, flip = true;
            } else {
                piece = 3, mirror = false, flip = false;
            }
            Map::Decor* d = Map::GetDecoration(x, y);
            if (d && d->IsFake()) g_hidden.HideObject(d, 0.2f);
            Render::Sprite* s = blocked ? Render::CreateSprite(g_red[piece], Render::kLayerFog, mirror, flip)
                                        : Render::CreateSprite(g_influence[piece], Render::kLayerFlat3, mirror, flip);
            float wx = (float)x * 84.f + ((y & 1) ? 42.f : 0.f);
            Render::SetPosition(s, wx, (float)y * 42.f * 0.5f, 0.3f);
            s->next = g_footprint;
            g_footprint = s;
            if (y & 1) ++x;
            --y;
        }
        if ((sy & 1) == 0) --sx;
        --sy;
    }
}

// @0x3a24ac: rebuild the preview's image at 0.8 alpha and its footprint.
void UpdateBuildingImage() {
    bool fits;
    if (g_building) {
        g_building->FindBaseCoordinates();
        g_building->UpdateImage();
        for (Render::Sprite* s : g_building->sprites) Render::SetAlpha(s, 0.8f);
        fits = g_building->CanBePlaced(true);
    } else if (g_decor) {
        g_decor->UpdateImage();
        if (g_decor->sprite) Render::SetAlpha(g_decor->sprite, 0.8f);
        fits = g_decor->CanBePlaced(true);
    } else {
        return;
    }
    CreateBuildingPlacementError(!fits);
}

// The tile of the screen centre (BuildingBought / DecorBought).
void ScreenCentreTile(int& x, int& y) {
    x = Render::ScreenWidth() / 2;
    y = Render::ScreenHeight() / 2;
    Map::MouseCoordinatesToWorld(x, y);
    Map::WorldCoordinatesToTile(x, y);
}

// Shared by BuildingBought and DecorBought: from tile (x, y), the first waypoint within 24 tiles
// where the object fits (first with fake decorations counting as obstacles, then as free), else
// (x, y) itself; off the player's land the view jumps to the castle and the object to the new
// screen centre.
template <class Obj>
void FindSpot(Obj* o, int x, int y) {
    Map::Patch* patch = Map::GetPatchForCoordinates(x, y, true);
    Map::Building* hq = Map::GetHQ();
    o->x = (uint8_t)x;
    o->y = (uint8_t)y;
    g_waypoints.clear();
    AI::GetWaypointsInRange(g_waypoints, (unsigned)x & 0xff, (unsigned)y & 0xff, 0x18, true);
    bool found = false;
    for (int pass = 0; pass < 2 && !found; ++pass) {
        for (AI::Waypoint* wp : g_waypoints) {
            o->x = (uint8_t)wp->x;
            o->y = (uint8_t)wp->y;
            if (o->CanBePlaced(pass == 1)) {
                found = true;
                break;
            }
        }
    }
    if (!found) {
        o->x = (uint8_t)x;
        o->y = (uint8_t)y;
    }
    // (The original dereferences a missing castle when the centre has no patch.)
    if ((!patch || !patch->owned) && hq) {
        Render::CenterOn(hq->baseX, (hq->maxY + hq->minY) * 0.5f);
        ScreenCentreTile(x, y);
        o->x = (uint8_t)x;
        o->y = (uint8_t)y;
    }
}

}  // namespace

void Init() {
    static const char* red[4] = {"images/red_R_02", "images/red_UP_02", "images/red_border_02", "images/red_mid_02"};
    static const char* green[4] = {"images/influence_R", "images/influence_UP", "images/influence_border",
                                   "images/influence_mid"};
    for (int i = 0; i < 4; ++i) g_red[i] = Resources::GetImage(red[i]);
    for (int i = 0; i < 4; ++i) g_influence[i] = Resources::GetImage(green[i]);
    g_dragging = false;
    g_hover = new PlaceBuildingHoverWindow();
    g_hover->Init();
}

void Deinit() {
    delete g_hover;
    g_hover = nullptr;
}

bool Activated() { return g_active; }

bool PlacementControlsVisible() { return Activated() && g_hover->shownHover; }

bool IsInDragProcess() { return g_active && g_dragging; }

bool IsBuildingDragged() { return Activated() && (g_building || g_decor) && g_dragging; }

// The controls appear once the shop icon has landed; while they are up the arrow points at the
// preview (unless the tutorial's confirm step is ready) and confirm follows CanBePlaced.
void Update(float dt) {
    float before = g_flyTime;
    g_flyTime = before - dt;
    if (before > 0.f && g_flyTime <= 0.f) {
        g_hover->Show();
        if (g_building) Render::SetVisibility(g_building->mainSprite, true);
        if (g_decor) {
            g_decor->visible = true;
            Render::SetVisibility(g_decor->sprite, true);
        }
    }
    if (g_hover->shownHover) {
        if (g_building) {
            int sx = (int)g_building->baseX, sy = (int)g_building->minY;
            Map::WorldCoordinatesToScreen(sx, sy);
            g_building->FindBaseCoordinates();
            if (!g_building->CanBePlaced(true) || GameState::tutorial != 0x5c) {
                BuildingHovers::ArrowAt(g_building->baseX, g_building->minY, false, false, false, false, false, false,
                                        false);
                g_hover->ResetArrow();
            }
            g_hover->SetPosition(sx, sy);
        } else if (g_decor && g_decor->sprite) {
            const Render::Sprite* s = g_decor->sprite;
            int sx = (int)(s->x + s->w * 0.5f), sy = (int)(s->y - s->h);
            Map::WorldCoordinatesToScreen(sx, sy);
            g_hover->SetPosition(sx, sy);
            int ax = g_decor->x, ay = g_decor->y;
            Map::TileCoordinatesToWorld(ax, ay);
            if (const Render::Sprite* t = g_decor->sprite) {
                ax = (int)(t->x + t->w * 0.5f);
                ay = (int)(t->y - t->h);
            }
            BuildingHovers::ArrowAt((float)ax, (float)ay, false, false, false, false, false, false, false);
        }
    }
    g_hidden.Update(dt);
}

bool Click(int x, int y, bool pressed, bool moving) {
    if (!g_active) return false;
    if (g_flyTime > 0.f || GameState::tutorial == 0x5c) return true;
    if (!pressed) {
        int wx, wy;
        if (!g_dragging || g_dragDistY + g_dragDistX < 0xb) {
            // A tap: the preview jumps to the tapped tile if that is on the player's land.
            if (moving) return false;
            g_dragging = false;
            wx = x;
            wy = y;
            Map::MouseCoordinatesToWorld(wx, wy);
            Map::WorldCoordinatesToTile(wx, wy);
            uint8_t* ox = g_building ? &g_building->x : &g_decor->x;
            uint8_t* oy = g_building ? &g_building->y : &g_decor->y;
            if (*ox != wx || *oy != wy) {
                Map::Patch* p = Map::GetPatchForCoordinates(wx, wy, true);
                if (!p || !p->owned) return true;
                *ox = (uint8_t)wx;
                *oy = (uint8_t)wy;
                UpdateBuildingImage();
                Render::SortRenderLayer(Render::kLayerObjects, 1);
            }
        } else {
            // The end of a drag.
            if (g_building) {
                wx = g_building->x;
                wy = g_building->y;
            } else if (g_decor) {
                wx = g_decor->x;
                wy = g_decor->y;
            } else {
                wx = wy = 0;
            }
            g_dragging = false;
        }
        Map::TileCoordinatesToWorld(wx, wy);
        g_grabX = (int)(((float)wx + 42.f) - g_dragX);
        g_grabY = (int)(((float)wy - 21.f) - g_dragY);
        return true;
    }
    // A press on the preview (its bounds, 100 px taller) starts a drag.
    int mx = x, my = y;
    Map::MouseCoordinatesToWorld(mx, my);
    int wx = 0, wy = 0;
    if (g_building) {
        wx = g_building->x;
        wy = g_building->y;
    } else if (g_decor) {
        wx = g_decor->x;
        wy = g_decor->y;
    }
    Map::TileCoordinatesToWorld(wx, wy);
    g_grabX = (int)(((float)wx + 42.f) - g_dragX);
    g_grabY = (int)(((float)wy - 21.f) - g_dragY);
    float fx = (float)mx, fy = (float)my;
    bool hit = false;
    if (g_building) {
        const Map::Building* b = g_building;
        hit = b->minX < fx && fx < b->maxX && b->minY - 100.f < fy && fy < b->maxY;
    } else if (g_decor && g_decor->sprite) {
        const Render::Sprite* s = g_decor->sprite;
        hit = s->x < fx && fx < s->x + s->w && (s->y - s->h) - 100.f < fy && fy < s->y;
    }
    if (!hit) return false;
    g_dragDistY = 0;
    g_dragging = true;
    g_dragDistX = 0;
    return true;
}

bool Move(int x, int y, int dx, int dy) {
    if (!g_active) return false;
    if (g_flyTime > 0.f || GameState::tutorial == 0x5c) return true;
    if ((!g_building && !g_decor) || !g_dragging) return false;
    float k = 2.f / Render::zoom;
    g_dragDistX += dx < 0 ? -dx : dx;
    g_dragDistY += dy < 0 ? -dy : dy;
    g_dragX = g_dragX + ((float)dx / (float)Render::ScreenWidth()) * k;
    g_dragY = g_dragY + ((float)dy / (float)Render::ScreenHeight()) * (k / Render::aspect);
    int tx = (int)g_dragX + g_grabX, ty = (int)g_dragY + g_grabY;
    Map::WorldCoordinatesToTile(tx, ty);
    int w = 1, h = 1;
    uint8_t *ox, *oy;
    if (g_building) {
        g_building->GetBuildZone(w, h);
        ox = &g_building->x;
        oy = &g_building->y;
    } else {
        g_decor->GetBuildZone(w, h);
        ox = &g_decor->x;
        oy = &g_decor->y;
    }
    if ((*ox == tx && *oy == ty) || !Map::IsValidAreaForBuildZone(tx, ty, w, h)) return true;
    if (g_decor) {
        if (!g_decor->data) g_decor->data = GameData::GetDecoration(g_decor->id);
        Map::SetVirtualDecoration(*ox, *oy, nullptr);
        Map::UpdateRoadConnections(*ox - 2, *oy - 2, *ox + 2, *oy + 2);
    }
    *ox = (uint8_t)tx;
    *oy = (uint8_t)ty;
    UpdateBuildingImage();
    if (g_decor) {
        // A road being placed links up with its neighbours as it moves.
        if (!Map::GetDecoration(*ox, *oy) && g_decor->data && g_decor->data->road)
            Map::SetVirtualDecoration(*ox, *oy, g_decor);
        Map::UpdateRoadConnections(*ox - 2, *oy - 2, *ox + 2, *oy + 2);
    }
    Render::SortRenderLayer(Render::kLayerObjects, 1);
    return true;
}

void ToggleRotation() {
    if (g_building) g_building->ToggleMirror();
    if (g_decor) g_decor->ToggleMirror();
    UpdateBuildingImage();
    Render::SortRenderLayer(Render::kLayerObjects, 1);
}

void Decline() {
    if (!g_active) return;
    g_flyTime = 0.f;
    ShopWindow::Show();
    HUDWindow::SetInfoText(nullptr);
    RemoveBuildingPlacementError();
    delete g_building;
    g_building = nullptr;
    if (g_decor) {
        // PORT: the original frees the preview while a road preview may still be its tile's
        // virtual decoration; the port clears that tile first.
        if (Map::GetVirtualDecoration(g_decor->x, g_decor->y) == g_decor)
            Map::SetVirtualDecoration(g_decor->x, g_decor->y, nullptr);
        Render::RemoveSprite(g_decor->sprite);   // ~Decor
        delete g_decor;
    }
    g_decor = nullptr;
    g_active = false;
    g_hover->SetBuilding(nullptr);
    g_hover->SetDecoration(nullptr);
    g_hover->Hide();
    BuildingHovers::SetHoverVisiblity(true, true);
    HUDWindow::SetBottomType(0);
    g_hidden.Clear(true);
}

// The tail of BuildingBought / DecorBought: the shop icon `from` flies over 1 s onto `to` (a world
// sprite), ending at its top-left corner less the icon's height and at its zoomed size.
void FlyTo(Render::Sprite* from, const Render::Sprite* to) {
    int x = (int)to->x;
    int y = (int)(to->y - from->h / Render::GetBaseZoomFactor());
    Map::WorldCoordinatesToScreen(x, y);
    BuildingHovers::AddItemMovement(from, x, y, true, 1.f, (int)(to->w * Render::GetBaseZoomFactor()),
                                    (int)(to->h * Render::GetBaseZoomFactor()), true, false);
}

void BuildingBought(Map::Building* b, Render::Sprite* sprite, bool fromPresents) {
    // SoundsManager::PlaySound("spell_fly", 1, false): sounds are milestone 5.
    Activate();
    g_fromPresents = fromPresents;
    g_declineOnAccept = false;
    g_building = b->Duplicate(true);
    int x, y;
    ScreenCentreTile(x, y);
    if (GameState::tutorial == 0x5c) {
        // UNVERIFIED (tutorial): BuildingHovers::HideWorldDialog(nullptr) (the world dialog is not
        // ported).
        x = 0x1a;
        y = 0x11;
    }
    HUDWindow::SetInfoText(StringTable::GetString("MOVEMENT_SHORT"));
    FindSpot(g_building, x, y);
    UpdateBuildingImage();
    g_dragY = 0.f;
    g_dragX = 0.f;
    g_building->FindBaseCoordinates();
    Render::SortRenderLayer(Render::kLayerObjects, 1);
    HUDWindow::SetBottomType(1);
    g_hover->SetBuilding(g_building);
    if (!sprite) {
        g_hover->Show();
        return;
    }
    // The shop's icon flies for 1 s to the preview's main sprite, which stays hidden until it lands.
    FlyTo(sprite, g_building->mainSprite);
    Render::SetVisibility(g_building->mainSprite, false);
    g_flyTime = 1.f;
}

void DecorBought(Map::Decor* d, Render::Sprite* sprite, bool fromPresents) {
    // SoundsManager::PlaySound("spell_fly", 1, false): sounds are milestone 5.
    Activate();
    g_fromPresents = fromPresents;
    g_decor = d;
    g_declineOnAccept = false;
    int x, y;
    ScreenCentreTile(x, y);
    FindSpot(g_decor, x, y);
    UpdateBuildingImage();
    g_grabX = Render::ScreenWidth() / 2;
    g_grabY = Render::ScreenHeight() / 2;
    Map::MouseCoordinatesToWorld(g_grabX, g_grabY);
    g_dragY = 0.f;
    g_dragX = 0.f;
    Render::SortRenderLayer(Render::kLayerObjects, 1);
    HUDWindow::SetBottomType(1);
    g_hover->SetDecoration(g_decor);
    if (!sprite) {
        g_hover->Show();
        return;
    }
    FlyTo(sprite, g_decor->sprite);
    g_decor->visible = false;
    Render::SetVisibility(g_decor->sprite, false);
    g_flyTime = 1.f;
}

void UpdateCost(int extraGold, bool show) {
    g_extraGold += extraGold;
    // UNVERIFIED: the price line under the controls (an SWPrintf of the cost) is not ported.
}

void Accept() {
    if (!g_active) return;
    if (g_declineOnAccept) {
        Decline();
        return;
    }
    if (g_building && !g_building->CanBePlaced(true)) return;
    if (g_decor && !g_decor->CanBePlaced(true)) return;
    g_flyTime = 0.f;
    g_hidden.Clear(true);
    int gold, crystals;
    if (g_building) {
        gold = (int)g_building->data->gold;
        crystals = (int)g_building->data->cb;
    } else {
        const GameData::DecorData* dd = GameData::GetDecoration(g_decor->id);
        gold = dd->cost1;
        crystals = dd->cost2;
    }
    if (g_fromPresents) {
        if (g_building) GameState::RemoveItem(g_building->id + 0x4000, 1);
        if (g_decor) GameState::RemoveItem(g_decor->id + 0x8000, 1);
    }
    // SoundsManager::PlaySound("building_build_start", 1, false): sounds are milestone 5.
    NotEnoughWindow::ResetRequirements();
    if (g_building)
        for (int i = 0; i < 8; ++i) NotEnoughWindow::AddRequirement(i, (unsigned)g_building->data->cost[i]);
    NotEnoughWindow::AddRequirement(GameState::kGold, (unsigned)(gold + g_extraGold));
    NotEnoughWindow::AddRequirement(GameState::kCrystal, (unsigned)GameState::AdjustCrystalCost(crystals));
    if (!NotEnoughWindow::CheckRequirements() && !g_fromPresents) {
        NotEnoughWindow::Show();
        UpdateCost(0, true);
        return;
    }
    HUDWindow::SetInfoText(nullptr);
    if (!g_fromPresents) {
        if (g_building)
            for (int i = 0; i < 8; ++i) GameState::ChangeResourceAmount(i, -g_building->data->cost[i]);
        GameState::ChangeResourceAmount(GameState::kGold, -(gold + g_extraGold));
        GameState::ChangeResourceAmount(GameState::kCrystal, -GameState::AdjustCrystalCost(crystals));
        // Billing::LogCBPurchase(0xc, id, crystals) for a crystal price: online logging, not ported.
    }
    GameState::ChangeResourceAmount(GameState::kGold, -g_extraGold);   // (charged again, as the original)
    if (g_building) {
        Map::Building* b = g_building;
        Map::Patch* p = Map::GetPatchForCoordinates(b->x, b->y, false);
        p->buildings.emplace_back(b);
        Map::SetBuilding(b->x, b->y, b);
        b->patch = p;
        b->index = (int)p->buildings.size() - 1;
        if (!g_fromPresents) {
            b->needsBuilder = 1;
            b->buildLeft = (double)b->data->constructionTime;
            b->stateTime = Timer::GetGlobalTime();
            b->SetClosed(true);
        } else {
            b->OnBuilded();
        }
        if (b->data->parkingCount != 0) b->workers.assign(b->data->parkingCount, nullptr);
        b->FindBaseCoordinates();
        b->LinkBaseToBuilding();
        b->UpdateImage();
        // (A present can set its own build time and skip the task.)
        b->f54 = Timer::GetGlobalTime() + b->data->constructionTime;
        // UNVERIFIED (milestone 4): Tasks::CompleteSubtask(0x16, id, 1).
        BuildingHovers::RegisterBuilding(b);
        b->isCopy = false;
        b->SetUniqueID(GameState::latestUniqueID + 1);
        Render::CenterOn(b->baseX, (b->maxY + b->minY) * 0.5f);
        if (Entity* worker = EntityManager::GetFreeWorker()) {
            b->AssignWorker(worker, 0);
            BuildingHovers::Update(0.0, true);
        }
        // OG::MakeRequest(2, 1, id, 0, 0): online, not ported.
        // UNVERIFIED (3f): a farm buys its default soil patches (OnSoilPatchBuy, farmPatchDefault - 1
        // times).
    } else if (g_decor) {
        Map::Decor* d = g_decor;
        Map::Patch* p = Map::GetPatchForCoordinates(d->x, d->y, false);
        p->decors.emplace_back(d);
        d->patch = p;
        d->UpdateMapLink();
        d->UpdateImage();
        int wx = d->x, wy = d->y;
        Map::TileCoordinatesToWorld(wx, wy);
        Render::CenterOn((float)wx, (float)wy);
        d->collectStart = Timer::GetGlobalTime();
        d->data = GameData::GetDecoration(d->id);
        if (d->data && d->data->collectTime != 0) BuildingHovers::RegisterDecoration(d);
        Map::UpdateRoadConnections(d->x - 2, d->y - 2, d->x + 2, d->y + 2);
    }
    RemoveBuildingPlacementError();
    g_active = false;
    g_building = nullptr;
    g_decor = nullptr;
    Render::SortRenderLayer(Render::kLayerObjects, 1);
    g_hover->SetBuilding(nullptr);
    g_hover->SetDecoration(nullptr);
    g_hover->Hide();
    BattleBarWindow::Show();
    BeltBarWindow::Show();
    BottomCityWindow::Show();
    if (crystals != 0) Map::Save(0);
    BuildingHovers::SetHoverVisiblity(true, true);
    ShopWindow::Hide();
    if (GameState::tutorial == 0x5c) {
        BuildingHovers::HideArrow();
        GUI::SetInteractionLock(true);
        GameState::tutorial = 0x5d;
    }
}

}  // namespace BuildingPlacement
