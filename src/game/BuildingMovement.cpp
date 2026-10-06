#include "game/BuildingMovement.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "engine/Render.h"
#include "engine/Resources.h"
#include "game/AI.h"
#include "game/Building.h"
#include "game/BuildingHovers.h"
#include "game/GameData.h"
#include "game/GameState.h"
#include "game/HiddenObjects.h"
#include "game/Map.h"
#include "game/StringTable.h"
#include "gui/GUI.h"
#include "hud/HUD.h"
#include "hud/HoverWindows.h"
#include "windows/Windows.h"

namespace BuildingMovement {
namespace {

using Map::Building;
using Map::Decor;

enum Mode { kMove = 0, kRotate = 1, kRemove = 2, kRoad = 3, kWarehouse = 4 };
enum ActionType { kActMove = 0, kActRemove = 2, kActRoad = 3, kActWarehouse = 4, kActContinue = 5 };

// An action (0x28 bytes on the original).
struct Action {
    int type = kActMove;              // +0x00 (a removal records the mode: 2, or 4 for the warehouse)
    Building* building = nullptr;     // +0x04 the edited building
    Building* copy = nullptr;         // +0x08 its preview while moved (owned)
    Decor* decor = nullptr;           // +0x0c the edited decoration
    Decor* copyDecor = nullptr;       // +0x10 its preview while moved (owned)
    // +0x14 the red footprint tiles where the preview is blocked (a sprite chain on the original).
    std::vector<Render::Sprite*> red;
    bool invalid = false;             // +0x18 the preview is blocked
    int parent = 0;                   // +0x1c kActContinue: the action it continues
    uint8_t x = 0, y = 0;             // +0x20 +0x21 kActContinue: the position before this drag
    bool mirrored = false;            // +0x22 and the rotation
};

PlaceBuildingHoverWindow* g_hover = nullptr;   // 0x630b3c (legacy mode's controls)
bool g_active = false;                // 0x630b40
bool g_legacy = false;                // 0x630b41
std::vector<Action> g_actions;        // 0x630b44
HiddenObjects g_hidden;               // 0x630b50 decorations faded under the previews
int g_roadCost = 0;                   // 0x630b68 (roads, not ported)
int g_mode = kMove;                   // 0x630b6c
bool g_grabbed = false;               // 0x630b70 a preview follows the drag
Render::Texture* g_redTex = nullptr;  // 0x630b74 images/red_mid_02
Render::Texture* g_influenceTex = nullptr;   // 0x630b78 images/influence_mid
float g_dragX = 0.f, g_dragY = 0.f;   // 0x630b7c 0x630b80 the drag so far, world units
float g_grabX = 0.f, g_grabY = 0.f;   // 0x630b84 0x630b88 the grabbed object's tile centre
bool g_tapPending = false;            // 0x630b8c the press hit nothing
int g_pressX = 0, g_pressY = 0;       // 0x630b90 0x630b94
Building* g_removeBuilding = nullptr; // 0x630b98 waiting for the removal question
Decor* g_removeDecor = nullptr;       // 0x630b9c

// The action a continuation stands for (following kActContinue to its first move).
Action& Root(size_t i) {
    while (g_actions[i].type == kActContinue) i = (size_t)g_actions[i].parent;
    return g_actions[i];
}

void ShowSprites(Building* b, bool on) {
    for (Render::Sprite* s : b->sprites) Render::SetVisibility(s, on);
}

void RemoveRed(Action& a) {
    for (Render::Sprite* s : a.red) Render::RemoveSprite(s);
    a.red.clear();
}

// The footprint of an action's preview: its start tile and its build zone.
bool PreviewZone(const Action& a, int& sx, int& sy, int& w, int& h) {
    if (a.copy) {
        a.copy->GetStartTile(sx, sy);
        a.copy->GetBuildZone(w, h);
        return true;
    }
    if (a.copyDecor) {
        const GameData::DecorData* d = GameData::GetDecoration(a.copyDecor->id);
        a.copyDecor->GetStartTile(sx, sy);
        w = d ? d->w : 0;
        h = d ? d->h : 0;
        return true;
    }
    return false;
}

template <class Fn>
void ForFootprint(int sx, int sy, int w, int h, Fn&& fn) {   // MapObject's footprint walk
    unsigned x = (unsigned)sx, y = (unsigned)sy;
    for (int r = 0; r < h; ++r) {
        unsigned cx = x, cy = y;
        for (int c = 0; c < w; ++c) {
            fn((int)cx, (int)cy);
            unsigned odd = cy & 1;
            cy -= 1;
            if (odd) cx += 1;
        }
        if ((y & 1) == 0) x -= 1;
        y -= 1;
    }
}

}  // namespace

// @0x39d324: no other moved preview covers the tile.
bool IsFree(int x, int y, Building* b, Decor* d) {
    for (const Action& a : g_actions) {
        if (a.type != kActMove || (b && a.copy == b) || (d && a.copyDecor == d)) continue;
        int sx = 0, sy = 0, w = 0, h = 0;
        if (!PreviewZone(a, sx, sy, w, h)) continue;
        bool hit = false;
        ForFootprint(sx, sy, w, h, [&](int tx, int ty) { hit |= tx == x && ty == y; });
        if (hit) return false;
    }
    return true;
}

namespace {

// FUN_0039d57c: whether a moved preview fits, the blocked tiles marked red.
void CheckAction(Action& a) {
    RemoveRed(a);
    int sx = 0, sy = 0, w = 0, h = 0;
    a.invalid = false;
    if (!PreviewZone(a, sx, sy, w, h)) return;
    ForFootprint(sx, sy, w, h, [&](int tx, int ty) {
        Building* b = Map::GetBuilding(tx, ty);
        Decor* d = Map::GetDecoration(tx, ty);
        Decor* v = Map::GetVirtualDecoration(tx, ty);
        if (d && d->IsFake()) d = nullptr;
        // An object being moved away or removed does not block.
        for (const Action& o : g_actions)
            if (b && o.building == b && o.type == kActMove) b = nullptr;
        for (const Action& o : g_actions)
            if (b && o.building == b && (o.type == kActRemove || o.type == kActWarehouse)) b = nullptr;
        for (const Action& o : g_actions)
            if (d && o.decor == d && o.type == kActMove) d = nullptr;
        for (const Action& o : g_actions)
            if (d && o.decor == d && o.type == kActRemove) d = nullptr;
        for (const Action& o : g_actions)
            if (v && o.decor == v && o.type == kActRemove) v = nullptr;
        bool blocked = b || d || v;
        if (tx < 0 || tx >= Map::GetGridWidth() || ty < 0 || ty >= Map::GetGridHeight()) blocked = true;
        if (!IsFree(tx, ty, a.copy, a.copyDecor)) blocked = true;
        Map::Patch* p = Map::GetPatchForCoordinates(tx, ty, false);
        if (p && !p->owned) {
            a.invalid = true;
            blocked = true;
        } else {
            a.invalid |= blocked;
        }
        if (!blocked) return;
        Render::Sprite* s = Render::CreateSprite(g_redTex, Render::kLayerFog, false, false);
        float wx = (ty & 1) ? 42.f : 0.f;
        Render::SetPosition(s, wx + (float)tx * 84.f, (float)ty * 42.f * 0.5f, 0.3f);
        a.red.push_back(s);
    });
}

// The check every edit ends with: the OK button only while every moved preview fits.
void CheckAll() {
    BottomCityWindow::EnableOkButton(true);
    for (Action& a : g_actions) {
        if (a.type == kActMove) CheckAction(a);
        if (a.invalid) BottomCityWindow::EnableOkButton(false);
    }
}

// @0x39cf84: the plain decorations under the previews fade out.
void UpdateObjectVisibility() {
    g_hidden.ShowAll();
    for (const Action& a : g_actions) {
        int sx = 0, sy = 0, w = 0, h = 0;
        if (a.copy) {
            a.copy->GetStartTile(sx, sy);
            a.copy->GetBuildZone(w, h);
        } else if (a.copyDecor) {
            a.copyDecor->GetStartTile(sx, sy);
            a.copyDecor->GetBuildZone(w, h);
        } else {
            continue;
        }
        ForFootprint(sx, sy, w, h, [&](int tx, int ty) {
            Decor* d = Map::GetDecoration(tx, ty);
            if (d && d->IsFake()) g_hidden.HideObject(d, 0.2f);
        });
    }
}

// @0x39c610
void UpdateRendering() {
    Render::SortRenderLayer(Render::kLayerObjects, 0);
    Render::SortRenderLayer(3, 1);
    Render::SortRenderLayer(4, 0);
    Render::SortRenderLayer(5, 1);
    Render::SortRenderLayer(Render::kLayerObjects, 0);
    Render::SortRenderLayer(Render::kLayerObjects, 1);
}

// The grabbed object follows the drag from its tile's centre.
void Grab(int tx, int ty) {
    g_grabbed = true;
    Map::TileCoordinatesToWorld(tx, ty);
    g_dragX = 0.f;
    g_dragY = 0.f;
    g_grabX = (float)tx + 42.f;
    g_grabY = (float)ty - 21.f;
}

void FlyText(float x, float y, const char* key) {
    BuildingHovers::ShowTextHover(x, y, StringTable::GetString(key), 1.f, 1.f, 1.f, 0.f, 0.f, 0.f, 0x19, 5, 5.f, true,
                                  2.f, 50.f);
}

void Popup(const char* key) { PopupWindow::Show(StringTable::GetString(key), PopupWindow::Hide, nullptr, nullptr); }

// The removal question, Yes running RemoveOk; No declines in legacy mode.
void AskRemove(const std::u32string& text) {
    PopupWindow::Show(text.c_str(), RemoveOk, g_legacy ? Decline : PopupWindow::Hide, nullptr);
}

std::u32string RemoveQuestion(const std::string& name) {
    if (!StringTable::StringExists(name.c_str())) return StringTable::GetString("TOOL_DELETE_ASK_GENERIC");
    return SWPrintf(0x80, StringTable::GetString("TOOL_DELETE_ASK"), {StringTable::GetString(name.c_str())});
}

// Release in move/rotate mode on nothing, or in remove mode: a short tap on empty ground with no
// edit made leaves the edit mode (legacy mode only for move/rotate).
bool ReleaseOnGround(int x, int y, bool needLegacy) {
    g_grabbed = false;
    if (needLegacy && !g_legacy) return false;
    if (!g_tapPending || !g_actions.empty()) return false;
    if (std::abs(x - g_pressX) + std::abs(y - g_pressY) > 9) return false;
    Decline();
    return false;
}

}  // namespace

void Init() {
    g_active = false;
    g_redTex = Resources::GetImage("images/red_mid_02");
    g_influenceTex = Resources::GetImage("images/influence_mid");
    g_hover = new PlaceBuildingHoverWindow();
    g_hover->Init();
}

void Deinit() {
    delete g_hover;
    g_hover = nullptr;
}

bool Activated() { return g_active; }

void SetLegacyMode(bool on) { g_legacy = on; }

// The zoom the road mode applied is taken back when another mode is picked.
static void LeaveRoadZoom() {
    if (g_mode == kRoad) Render::zoom = Render::zoom / 1.6f;
}

void ToggleMovement() {
    LeaveRoadZoom();
    g_mode = kMove;
    HUDWindow::SetInfoText(StringTable::GetString("MODE_MOVE"));
    BuildingHovers::SetHoverVisiblity(false, false);
}

void ToggleDemolishion() {
    LeaveRoadZoom();
    g_mode = kRemove;
    HUDWindow::SetInfoText(StringTable::GetString("MODE_REMOVE"));
    BuildingHovers::SetHoverVisiblity(false, false);
}

void ToggleRotation() {
    if (!g_legacy) {
        g_mode = kRotate;
        HUDWindow::SetInfoText(StringTable::GetString("MODE_ROTATE"));
        BuildingHovers::SetHoverVisiblity(false, false);
        return;
    }
    // Legacy mode: the one edited object turns.
    Action* a = nullptr;
    for (Action& o : g_actions)
        if (o.copy || o.copyDecor) {
            a = &o;
            break;
        }
    if (!a) return;
    if (a->copy) {
        a->copy->ToggleMirror();
        a->copy->FindBaseCoordinates();
        a->copy->UpdateImage();
    }
    if (a->copyDecor) {
        a->copyDecor->ToggleMirror();
        a->copyDecor->UpdateImage();
    }
    Render::SortRenderLayer(Render::kLayerObjects, 1);
    CheckAll();
}

void Activate() {
    if (g_active) return;
    BuildingHovers::SetHoverVisiblity(false, false);
    TaskHolderWindow::SetTaskVisibility(false);
    g_grabbed = false;
    g_mode = kMove;
    g_active = true;
    HUDWindow::SetInfoText(StringTable::GetString("MODE_MOVE"));
    HUDWindow::SetBottomType(2);
    BottomCityWindow::EnableOkButton(true);
    BottomCityWindow::EnableUndoButton(false);
    BeltBarWindow::Hide();
    BattleBarWindow::Hide();
    g_roadCost = 0;
    g_hidden.Clear(true);
}

void UndoAction(bool quiet) {
    if (g_actions.empty()) {
        BottomCityWindow::EnableUndoButton(false);
        return;
    }
    Action& a = g_actions.back();
    if (a.type == kActMove) {
        if (a.building) {
            ShowSprites(a.building, true);
            a.building->UndoAction();
            delete a.copy;
            a.copy = nullptr;
            a.building->LinkBaseToBuilding();
        }
        if (a.decor) {
            a.decor->visible = true;
            if (a.decor->sprite) Render::SetVisibility(a.decor->sprite, true);   // PORT: see Click
            if (a.copyDecor && a.copyDecor->sprite) Render::RemoveSprite(a.copyDecor->sprite);
            delete a.copyDecor;
            a.copyDecor = nullptr;
            a.decor->UpdateMapLink();
        }
        RemoveRed(a);
    } else if (a.type == kActContinue) {
        Action& root = Root((size_t)a.parent);
        if (root.copy) {
            root.copy->x = a.x;
            root.copy->y = a.y;
            if (a.mirrored != root.copy->IsMirrored()) root.copy->ToggleMirror();
            root.copy->FindBaseCoordinates();
            root.copy->UpdateImage();
        }
        if (root.copyDecor) {
            root.copyDecor->x = a.x;
            root.copyDecor->y = a.y;
            root.copyDecor->mirrored = a.mirrored;
            root.copyDecor->UpdateImage();
        }
    } else if (a.type == kActWarehouse || a.type == kActRemove) {
        if (a.building) {
            ShowSprites(a.building, true);
            a.building->UndoAction();
            a.building->LinkBaseToBuilding();
        } else if (a.type == kActRemove && a.decor) {
            a.decor->visible = true;
            if (a.decor->sprite) Render::SetVisibility(a.decor->sprite, true);   // PORT: see Click
            a.decor->UpdateMapLink();
        }
    }
    // kActRoad: the road tiles (not ported).
    g_actions.pop_back();
    CheckAll();
    if (g_actions.empty()) BottomCityWindow::EnableUndoButton(false);
    // !quiet: FUN_0039c660(0, 1) formats the road cost ("ROAD_WORK_COST") into a local buffer only.
    (void)quiet;
}

void Decline() {
    if (!g_active) return;
    HUDWindow::SetInfoText(nullptr);
    while (!g_actions.empty()) UndoAction(true);
    UpdateRendering();
    g_active = false;
    g_hover->SetBuilding(nullptr);
    g_hover->SetDecoration(nullptr);
    g_hover->Hide();
    BeltBarWindow::Show();
    BattleBarWindow::Show();
    HUDWindow::SetBottomType(0);
    BottomCityWindow::Show();
    LeaveRoadZoom();
    BuildingHovers::SetHoverVisiblity(true, true);
    TaskHolderWindow::SetTaskVisibility(true);
    CastleTopWindow::HideTools();
    g_hidden.Clear(true);
}

bool Accept() {
    if (!g_active) return true;
    if (!g_actions.empty()) {
        // Blocked previews get an arrow; if none of them is on screen the view goes to the last one.
        bool allFit = true, anyOnScreen = false;
        float fx = 0.f, fy = 0.f;
        for (Action& a : g_actions) {
            if (!a.invalid) continue;
            if (a.copy) {
                a.copy->FindBaseCoordinates();
                fy = a.copy->minY;
                fx = a.copy->baseX;
            }
            if (a.copyDecor && a.copyDecor->sprite) {
                const Render::Sprite* s = a.copyDecor->sprite;
                fx = s->x + s->w * 0.5f;
                fy = s->y - s->h;
            }
            allFit = false;
            BuildingHovers::ArrowAt(fx, fy, false, false, false, false, false, false, true);
            int sx = (int)fx, sy = (int)fy;
            Map::WorldCoordinatesToScreen(sx, sy);
            if (sx < 0 || Render::ScreenWidth() < sx || sy < 1 || Render::ScreenHeight() < sy) allFit = false;
            else anyOnScreen = true;
        }
        if (!allFit) {
            if (anyOnScreen) return false;
            Render::CenterOn(fx, fy);   // UNVERIFIED: animated, zoom 0.4 (the port's CenterOn is instant)
            return false;
        }
    }
    HUDWindow::SetInfoText(nullptr);
    if (g_actions.empty()) {
        Decline();
        return true;
    }
    g_hidden.Clear(true);
    for (const Action& a : g_actions)
        if (a.type == kActMove && a.invalid) return false;
    NotEnoughWindow::ResetRequirements();
    if (g_roadCost > 0) NotEnoughWindow::AddRequirement(GameState::kGold, (unsigned)g_roadCost);
    if (!NotEnoughWindow::CheckRequirements()) {
        NotEnoughWindow::Show();
        return false;
    }
    // UNVERIFIED (milestone 5): SoundsManager::PlaySound("building_position").
    GameState::ChangeResourceAmount(GameState::kGold, -g_roadCost);
    GameState::updated = true;
    for (Action& a : g_actions) {
        if (a.type == kActMove) {
            if (Building* b = a.building) {
                Map::RemoveBuilding(b->x, b->y, true, true);
                b->x = a.copy->x;
                b->y = a.copy->y;
                if (a.copy->IsMirrored() != b->IsMirrored()) b->ToggleMirror();
                b->FindBaseCoordinates();
                b->UpdateImage();
                b->LinkBaseToBuilding();
                delete a.copy;
                a.copy = nullptr;
                b->OnMoved();
                b->FindBaseCoordinates();
            }
            if (Decor* d = a.decor) {
                Map::RemoveDecoration(d->x, d->y, true);
                d->x = a.copyDecor->x;
                d->y = a.copyDecor->y;
                d->visible = true;
                d->mirrored = a.copyDecor->mirrored;
                d->UpdateImage();
                d->UpdateMapLink();
                if (a.copyDecor->sprite) Render::RemoveSprite(a.copyDecor->sprite);
                delete a.copyDecor;
                a.copyDecor = nullptr;
            }
            RemoveRed(a);
        } else if (a.type == kActRemove) {
            if (!a.building) {
                if (a.decor) Map::RemoveDecoration(a.decor->x, a.decor->y, false);
            } else {
                if (a.building->IsOpened()) a.building->SetClosed(true);
                Map::RemoveBuilding(a.building->x, a.building->y, false, true);
            }
        }
        // kActWarehouse (PresentsContainer::AddToWarehouse) and kActRoad: not ported.
    }
    g_actions.clear();
    UpdateRendering();
    g_active = false;
    g_hover->SetBuilding(nullptr);
    g_hover->SetDecoration(nullptr);
    g_hover->Hide();
    BeltBarWindow::Show();
    BattleBarWindow::Show();
    HUDWindow::SetBottomType(0);
    LeaveRoadZoom();
    BuildingHovers::SetHoverVisiblity(true, true);
    BuildingHovers::Update(0.0, true);
    TaskHolderWindow::SetTaskVisibility(true);
    CastleTopWindow::HideTools();
    Map::UpdateRoadConnections(0, 0, Map::GetGridWidth() - 1, Map::GetGridHeight() - 1);
    return true;
}

void Update(float dt) {
    if (g_actions.empty()) return;
    bool allFit = true;
    for (const Action& a : g_actions)
        if (a.invalid) allFit = false;
    if (g_hover->shownHover) {
        const Action& last = g_actions.back();
        bool placed = false;
        if (last.copy) {
            int sx = (int)last.copy->baseX, sy = (int)last.copy->minY;
            Map::WorldCoordinatesToScreen(sx, sy);
            g_hover->SetPosition(sx, sy);
            placed = true;
        } else if (last.copyDecor && last.copyDecor->sprite) {
            const Render::Sprite* s = last.copyDecor->sprite;
            int sx = (int)(s->x + s->w * 0.5f), sy = (int)(s->y - s->h);
            Map::WorldCoordinatesToScreen(sx, sy);
            g_hover->SetPosition(sx, sy);
            placed = true;
        }
        if (placed && allFit) g_hover->EnableAccept();
    }
    g_hidden.Update(dt);
}

bool Click(int x, int y, bool pressed, bool moving) {
    if (!g_active) return false;
    if (!g_actions.empty()) BottomCityWindow::EnableUndoButton(true);
    if (pressed) {
        g_tapPending = false;
        g_pressY = y;
        g_pressX = x;
    }
    int tx = x, ty = y;
    Map::MouseCoordinatesToWorld(tx, ty);
    Map::WorldCoordinatesToTile(tx, ty);
    int wx = x, wy = y;
    Map::MouseCoordinatesToWorld(wx, wy);

    // The newest moved preview under the point (a continuation counts as its first move).
    int hit = -1;
    for (int i = (int)g_actions.size() - 1; i >= 0; --i) {
        if (g_actions[(size_t)i].type != kActMove && g_actions[(size_t)i].type != kActContinue) continue;
        const Action& r = Root((size_t)i);
        if (r.copy && !r.copy->sprites.empty() && r.copy->sprites.front()->visible && r.copy->Contains(wx, wy)) {
            hit = i;
            break;
        }
        if (r.copyDecor && r.copyDecor->sprite && r.copyDecor->sprite->visible && r.copyDecor->Contains(wx, wy, true)) {
            hit = i;
            break;
        }
    }

    if (g_mode == kMove || g_mode == kRotate) {
        // Legacy mode edits one object only.
        void* legacyTarget = nullptr;
        for (const Action& a : g_actions) {
            legacyTarget = a.copyDecor ? (void*)a.copyDecor : (void*)a.copy;
            if (legacyTarget) break;
        }
        if (!g_legacy) legacyTarget = nullptr;

        if (!pressed) {
            if (hit == -1) {
                // A short tap on owned ground moves the last moved preview there.
                const bool smallDrag = std::fabs(g_dragX) < 2.f && std::fabs(g_dragY) < 2.f;
                if (!moving && g_mode == kMove && !g_actions.empty() &&
                    (g_actions.back().type == kActMove || g_actions.back().type == kActContinue) && smallDrag) {
                    Map::Patch* p = Map::GetPatchForCoordinates(tx, ty, true);
                    if (p && p->owned) {
                        Action& r = Root(g_actions.size() - 1);
                        if (r.copy) {
                            r.copy->x = (uint8_t)tx;
                            r.copy->y = (uint8_t)ty;
                            r.copy->FindBaseCoordinates();
                            r.copy->UpdateImage();
                        } else if (r.copyDecor) {
                            r.copyDecor->x = (uint8_t)tx;
                            r.copyDecor->y = (uint8_t)ty;
                            r.copyDecor->UpdateImage();
                        }
                        CheckAll();
                        Render::SortRenderLayer(Render::kLayerObjects, 1);
                        return true;
                    }
                }
            } else {
                // UNVERIFIED (milestone 5): SoundsManager::PlaySound("Build", 0x14, pressed).
                Action& r = Root((size_t)hit);
                const bool smallDrag = std::fabs(g_dragX) < 2.f && std::fabs(g_dragY) < 2.f;
                if (g_mode == kRotate && smallDrag && r.type == kActMove) {
                    if (r.copy) {
                        r.copy->ToggleMirror();
                        r.copy->FindBaseCoordinates();
                        r.copy->UpdateImage();
                    } else if (r.copyDecor) {
                        r.copyDecor->ToggleMirror();
                        r.copyDecor->UpdateImage();
                    }
                    Render::SortRenderLayer(Render::kLayerObjects, 1);
                    CheckAll();
                }
            }
            return ReleaseOnGround(x, y, true);
        }

        if (hit != -1) {
            // Grabbing an older preview again: a continuation remembers where it was.
            if (hit != (int)g_actions.size() - 1) {
                const Action& h = g_actions[(size_t)hit];
                Action c;
                c.type = kActContinue;
                c.parent = hit;
                if (h.type == kActContinue) {
                    c.x = h.x;
                    c.y = h.y;
                    c.mirrored = h.mirrored;
                } else if (h.copy) {
                    c.x = h.copy->x;
                    c.y = h.copy->y;
                    c.mirrored = h.copy->IsMirrored();
                } else {
                    c.x = h.copyDecor->x;
                    c.y = h.copyDecor->y;
                    c.mirrored = h.copyDecor->mirrored;
                }
                g_actions.push_back(c);
                std::printf("BuildingMovement::Click() Created movement continuation action (%d) (parent action %d)\n",
                            (int)g_actions.size() - 1, hit);
            }
            const Action& r = Root(g_actions.size() - 1);
            const int ox = r.copy ? r.copy->x : r.copyDecor->x;
            const int oy = r.copy ? r.copy->y : r.copyDecor->y;
            Grab(ox, oy);
            return true;
        }

        Building* b = Map::GetBuildingAtCoords(wx, wy);
        if (!b) {
            Decor* d = Map::GetDecorAtCoords(wx, wy, false);
            if (!d) {
                g_dragX = 0.f;
                g_dragY = 0.f;
                g_tapPending = true;
                return false;
            }
            if (legacyTarget) return false;
            if (!d->metaText.empty()) {   // a quest decoration stays (MetaExpression +0x20)
                if (d->sprite) FlyText(d->sprite->x + d->sprite->w * 0.5f, d->sprite->y, "FLY_CANTMOVE");
                return false;
            }
            for (const Action& a : g_actions)
                if ((a.type == kActMove || a.type == kActRemove || a.type == kActWarehouse) && a.decor == d) return false;
            if (d->IsFake()) return false;
            Action a;
            a.type = kActMove;
            a.copyDecor = new Decor();
            a.copyDecor->x = d->x;
            a.copyDecor->y = d->y;
            a.copyDecor->mirrored = d->mirrored;
            a.copyDecor->id = d->id;
            a.copyDecor->UpdateImage();
            d->visible = false;
            // PORT: the port's sprites do not read the decoration's visible flag (+0x1d) each frame,
            // so its sprite is hidden (and shown again on undo) explicitly.
            if (d->sprite) Render::SetVisibility(d->sprite, false);
            a.decor = d;
            g_actions.push_back(std::move(a));
            std::printf("BuildingMovement::Click() Created movement action (%d) for decoration\n",
                        (int)g_actions.size() - 1);
            CheckAll();
            Grab(d->x, d->y);
            Render::SortRenderLayer(Render::kLayerObjects, 1);
            if (!g_legacy) return true;
            g_hover->Show();
            return true;
        }

        if (legacyTarget && legacyTarget != (void*)b) return false;
        if (b->data->buildingClass != 0xd && b->data->buildingClass == 4) return false;   // trees and rocks stay
        for (const Action& a : g_actions)
            if ((a.type == kActMove || a.type == kActRemove || a.type == kActWarehouse) && a.building == b) return false;
        Action a;
        a.type = kActMove;
        a.copy = b->Duplicate(true);
        a.copy->FindBaseCoordinates();
        a.copy->UpdateImage();
        a.building = b;
        ShowSprites(b, false);
        b->PrepareToAction();
        g_actions.push_back(std::move(a));
        std::printf("BuildingMovement::Click() Created movement action (%d) for building\n", (int)g_actions.size() - 1);
        CheckAll();
        Grab(b->x, b->y);
        Render::SortRenderLayer(Render::kLayerObjects, 1);
        if (!g_legacy) return true;
        g_hover->SetBuilding(g_actions.back().copy);
        g_hover->Show();
        return true;
    }

    if (g_mode != kRemove && g_mode != kWarehouse) return false;   // kRoad: not ported

    if (!pressed) return ReleaseOnGround(x, y, false);

    // Remove mode: what was tapped, on owned land that is bordered and visible.
    Building* b = Map::GetBuildingAtCoords(wx, wy);
    Decor* d = Map::GetDecorAtCoords(wx, wy, false);
    if (b && b->patch && (!b->patch->owned || !b->patch->bordered)) b = nullptr;
    if (d && d->patch && (!d->patch->owned || !d->patch->bordered)) d = nullptr;
    if (b && !b->sprites.empty() && !b->sprites.front()->visible) b = nullptr;
    if (d && d->sprite && !d->sprite->visible) d = nullptr;

    if (!b) {
        if (!d) {
            if (hit == -1) {
                g_tapPending = true;
                return false;
            }
            // A moved preview: remove what it stands for.
            Action& r = Root((size_t)hit);
            if (r.copy) {
                g_removeBuilding = r.copy;
                AskRemove(RemoveQuestion(r.copy->data->name));
            } else if (r.copyDecor) {
                g_removeDecor = r.copyDecor;
                const GameData::DecorData* data = GameData::GetDecoration(r.copyDecor->id);
                AskRemove(RemoveQuestion(data ? data->name : std::string()));
            }
            return true;
        }
        if (!d->metaText.empty()) {
            Popup("CANT_SELL_LAST");
            return false;
        }
        g_removeDecor = d;
        const GameData::DecorData* data = GameData::GetDecoration(d->id);
        AskRemove(RemoveQuestion(data ? data->name : std::string()));
        return true;
    }

    if (b->data->cantSellLast && Map::GetBuildingCount(b->id, false) == 1) {
        Popup("CANT_SELL_LAST");
        return false;
    }
    if (b->GetEffectOnPopulation() > 0 &&
        GameState::GetPlayerWorkersCount() - b->GetEffectOnPopulation() - Map::GetUsedWorkerCount() < 0) {
        Popup("PEOPLE_LIMIT_LOW");
        return false;
    }
    // UNVERIFIED (milestone 4): PresentsContainer::WAREHOUSE_ID is also refused; no warehouse yet.
    bool refuse = b->data->IsHQ() || !b->IsOpened();
    if (!refuse) {
        if (!b->HasActiveContract()) {
            if (b->data->buildingClass == 4) return false;
        } else if (b->data->buildingClass != 0) {
            refuse = true;
        }
    }
    if (refuse) {
        FlyText(b->baseX, b->minY, g_mode == kWarehouse ? "CANT_MOVE_TO_INV" : "FLY_CANTDELETE");
        return false;
    }
    // UNVERIFIED (milestone 4): PresentsContainer::ToggleWarehouseAnim.
    g_removeBuilding = b;
    AskRemove(RemoveQuestion(b->data->name));
    return true;
}

bool Move(int x, int y, int dx, int dy) {
    (void)x, (void)y;
    if (!g_active) return false;
    if (!g_actions.empty()) BottomCityWindow::EnableUndoButton(true);
    if (g_mode != kMove) return false;   // kRoad painting: not ported
    if (g_actions.empty()) return false;
    const int lastType = g_actions.back().type;
    if (lastType != kActMove && lastType != kActContinue) return false;
    Action& r = Root(g_actions.size() - 1);
    if (!g_grabbed) return false;
    BuildingHovers::HideArrow();
    const float k = 2.f / Render::zoom;
    const float ddx = ((float)dx / (float)Render::ScreenWidth()) * k;
    const float ddy = ((float)dy / (float)Render::ScreenHeight()) * (k / Render::aspect);
    g_dragX = g_dragX + ddx;
    g_dragY = g_dragY + ddy;
    int tx = (int)(g_dragX + g_grabX), ty = (int)(g_dragY + g_grabY);
    Map::WorldCoordinatesToTile(tx, ty);
    uint8_t* px = nullptr;
    uint8_t* py = nullptr;
    int w = 0, h = 0;
    if (r.copy) {
        px = &r.copy->x;
        py = &r.copy->y;
        r.copy->GetBuildZone(w, h);
    } else if (r.copyDecor) {
        px = &r.copyDecor->x;
        py = &r.copyDecor->y;
        r.copyDecor->GetBuildZone(w, h);
    } else {
        return true;
    }
    if ((*px == tx && *py == ty) || !Map::IsValidAreaForBuildZone(tx, ty, w, h)) return true;
    *px = (uint8_t)tx;
    *py = (uint8_t)ty;
    if (r.copy) {
        r.copy->FindBaseCoordinates();
        r.copy->UpdateImage();
    }
    if (r.copyDecor) r.copyDecor->UpdateImage();
    CheckAll();
    UpdateObjectVisibility();
    Render::SortRenderLayer(Render::kLayerObjects, 1);
    return true;
}

void RemoveOk() {
    Action a;
    a.type = g_mode;   // kActRemove (or the warehouse's 4)
    std::printf("BuildingMovement::RemoveOk() Created remove action (%d)\n", (int)g_actions.size());
    a.building = g_removeBuilding;
    a.decor = g_removeDecor;
    if (a.building) {
        ShowSprites(a.building, false);
        a.building->PrepareToAction();
    }
    if (a.decor) {
        a.decor->visible = false;
        if (a.decor->sprite) Render::SetVisibility(a.decor->sprite, false);   // PORT: see Click
    }
    g_actions.push_back(std::move(a));
    g_removeDecor = nullptr;
    g_removeBuilding = nullptr;
    if (g_legacy) Accept();
}

}  // namespace BuildingMovement
