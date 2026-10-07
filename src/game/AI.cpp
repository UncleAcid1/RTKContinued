#include "game/AI.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>

#include "engine/Render.h"
#include "game/AIState.h"
#include "game/Entity.h"
#include "game/EntityManager.h"
#include "game/GameData.h"
#include "game/GameState.h"
#include "game/Map.h"
#include "game/Rand48.h"

namespace AI {

bool allowWaypointLink = true;
float weightCost = 1.41f;

namespace {

std::vector<Waypoint*> g_list;        // 0x610af8 (count 0x610b04)
std::vector<Waypoint*> g_grid;        // 0x610af4, width 0x60ef04, height 0x60ef00
int g_gridW = 0, g_gridH = 0;
std::vector<Waypoint*> g_search;      // 0x610b38
std::vector<Waypoint*> g_nearWeighted, g_nearAll;   // 0x610b48, 0x610b54
int g_marker = 1;                     // 0x60ef08 (data: starts at 1)
// The farm view's waypoints: their list (0x610b08, count 0x610b14) and grid (0x610af0; its size
// 0x60eefc x 0x60eef8 is set on first use and kept, as are the cells, which the next farm visit
// overwrites).
std::vector<Waypoint*> g_farmList;
std::vector<Waypoint*> g_farmGrid;
int g_farmGridW = 0, g_farmGridH = 0;
// Freed farm waypoints stay allocated for reuse, as in the original's pool: the farm's entities are
// removed after its waypoints, and their AI may still point at them.
std::vector<std::unique_ptr<Waypoint>> g_farmPool;
std::vector<Waypoint*> g_farmFree;

void ClassifyWaypoint(Waypoint* wp, unsigned part) {   // @0xe1648
    wp->part = part;
    for (Waypoint* n : wp->n)
        if (n && n->part == 0) ClassifyWaypoint(n, part);
}

// The four diagonal neighbours of (x, y) in GetNearestWP's order: up-left, down-right, up-right, down-left.
void DiagonalNeighbours(int x, int y, Waypoint* out[4]) {
    if ((y & 1) == 0) {
        out[0] = GetWaypoint(x - 1, y - 1, false);
        out[1] = GetWaypoint(x, y + 1, false);
        out[2] = GetWaypoint(x, y - 1, false);
        x -= 1;
    } else {
        out[0] = GetWaypoint(x, y - 1, false);
        out[1] = GetWaypoint(x + 1, y + 1, false);
        out[2] = GetWaypoint(x + 1, y - 1, false);
    }
    out[3] = GetWaypoint(x, y + 1, false);
}

int NearestDirection(Waypoint* best, Waypoint* const w[4]) {
    int dir = best == w[3] ? 3 : -1;
    if (best == w[1]) dir = 5;
    if (best == w[2]) dir = 7;
    if (best == w[0]) dir = 1;
    return dir;
}

template <class Accept>
Waypoint* NearestOf(int x, int y, float wx, float wy, int& dir, Accept&& accept) {
    Waypoint* w[4];
    DiagonalNeighbours(x, y, w);
    Waypoint* best = nullptr;
    float bestD = 100000.f;
    for (Waypoint* c : w) {
        if (!c || !accept(c)) continue;
        float dy = c->wy - wy, dx = c->wx - wx;
        float d = std::sqrt(dy * dy + dx * dx);
        if (d < bestD) {
            best = c;
            bestD = d;
        }
    }
    dir = NearestDirection(best, w);
    return best;
}

Waypoint* NewWaypoint() {
    // (a pool of 0x400 per block on the original)
    Waypoint* wp = new Waypoint;
    *wp = Waypoint{};
    return wp;
}

void AddToList(Waypoint* wp) {
    wp->index = (unsigned)g_list.size();
    g_list.push_back(wp);
}

}  // namespace

Waypoint* GetWaypoint(int x, int y, bool farm) {
    if (farm) {
        if (g_farmGrid.empty() || (x | y) < 0 || y >= g_farmGridH || x >= g_farmGridW) return nullptr;
        return g_farmGrid[(size_t)(g_farmGridW * y + x)];
    }
    if (g_grid.empty() || (x | y) < 0 || y >= g_gridH || x >= g_gridW) return nullptr;
    return g_grid[(size_t)(g_gridW * y + x)];
}

const std::vector<Waypoint*>& GetFarmWaypoints() { return g_farmList; }

// @0xe7a08: a farm tile's waypoint (unblocked, weight 1) at its world point.
void CreateFarmWaypoints(int x, int y) {
    Map::SetBlock(x, y, false);
    Waypoint* wp;
    if (!g_farmFree.empty()) {
        wp = g_farmFree.back();
        g_farmFree.pop_back();
    } else {
        g_farmPool.push_back(std::make_unique<Waypoint>());
        wp = g_farmPool.back().get();
    }
    *wp = Waypoint{};
    wp->weight = 1.f;
    wp->x = x;
    wp->y = y;
    wp->wx = (float)(int)((float)x * 196.f + ((y & 1) ? 98.f : 0.f) + 98.f + Map::GetFarmWorldX());
    wp->wy = (float)(int)(((float)y * 98.f * 0.5f - 49.f) + Map::GetFarmWorldY());
    wp->index = (unsigned)g_farmList.size();
    g_farmList.push_back(wp);
    if (g_farmGrid.empty()) {
        g_farmGridW = Map::GetFarmGridWidth();
        g_farmGridH = Map::GetFarmGridHeight();
        g_farmGrid.assign((size_t)(g_farmGridW * g_farmGridH), nullptr);
    }
    g_farmGrid[(size_t)(g_farmGridW * y + x)] = wp;
}

// @0xe35f4: the farm waypoints get their diagonal neighbours only (no straight steps).
void LinkAdjacentFarmWaypoints() {
    for (Waypoint* wp : g_farmList) {
        int x = wp->x, y = wp->y;
        bool odd = (y & 1) != 0;
        auto link = [&](int i, int tx, int ty) {
            if (!Map::GetBlock(tx, ty)) wp->n[i] = GetWaypoint(tx, ty, true);
        };
        link(0, odd ? x : x - 1, y - 1);
        link(3, odd ? x + 1 : x, y - 1);
        link(2, odd ? x : x - 1, y + 1);
        link(1, odd ? x + 1 : x, y + 1);
    }
    for (Waypoint* wp : g_farmList) wp->part = 0;
    Render::SortRenderLayer(9, false);
}

// @0xe192c: the farm waypoints go back to the pool (their grid cells stay as they were).
void RemoveFarmWaypoints() {
    for (Waypoint* wp : g_farmList) g_farmFree.push_back(wp);
    g_farmList.clear();
}

const std::vector<Waypoint*>& GetWaypoints() { return g_list; }
std::vector<Waypoint*>& SearchList() { return g_search; }

void CreateMapWaypoint(int x, int y) {
    Map::SetBlock(x, y, false);
    Waypoint* wp = NewWaypoint();
    wp->weight = 1.f;
    if (Map::GetBuilding(x, y)) wp->weight = 1000.f;
    if (Map::Decor* d = Map::GetDecoration(x, y)) {
        if (Map::GetMapID() == 0) {
            if (d->data) {
                if (d->data->layer == 0) wp->weight = 1000.f;
                if (d->data->isRoad) wp->weight = 0.1f;
            }
        } else if (d->data && d->data->isRoad) {
            wp->weight = 0.1f;
        }
        // UNVERIFIED: a decoration whose MetaExpression has type 0x6f sets weight 10; decoration
        // meta expressions are not ported yet.
    }
    wp->x = x;
    wp->y = y;
    wp->wy = (float)y * 42.f * 0.5f - 21.f;
    wp->wx = ((y & 1) ? 42.f : 0.f) + (float)x * 84.f + 42.f;
    AddToList(wp);
    if (g_grid.empty()) {
        g_gridW = Map::GetGridWidth();
        g_gridH = Map::GetGridHeight();
        g_grid.assign((size_t)g_gridW * g_gridH, nullptr);
    }
    g_grid[(size_t)(g_gridW * y + x)] = wp;
}

void RemoveMapWaypoint(int x, int y) {
    Waypoint* wp = GetWaypoint(x, y, false);
    if (!wp) return;
    g_grid[(size_t)(g_gridW * y + x)] = nullptr;
    if (wp->n[2]) wp->n[2]->n[3] = nullptr;
    if (wp->n[3]) wp->n[3]->n[2] = nullptr;
    if (wp->n[0]) wp->n[0]->n[1] = nullptr;
    if (wp->n[1]) wp->n[1]->n[0] = nullptr;
    if (wp->n[4]) wp->n[4]->n[6] = nullptr;
    if (wp->n[5]) wp->n[5]->n[7] = nullptr;
    if (wp->n[6]) wp->n[6]->n[4] = nullptr;
    if (wp->n[7]) wp->n[7]->n[5] = nullptr;
    // swap-remove from the list
    Waypoint* last = g_list.back();
    last->index = wp->index;
    g_list[wp->index] = last;
    g_list.pop_back();
    for (BaseAI* ai : BaseAI::All()) ai->OnWaypointRemove(wp);
    delete wp;
}

void LinkAdjacentWaypoints(Waypoint* wp) {
    int x = wp->x, y = wp->y;
    bool odd = (y & 1) != 0;
    wp->n[0] = GetWaypoint(odd ? x : x - 1, y - 1, false);
    wp->n[3] = GetWaypoint(odd ? x + 1 : x, y - 1, false);
    wp->n[2] = GetWaypoint(odd ? x : x - 1, y + 1, false);
    wp->n[1] = GetWaypoint(odd ? x + 1 : x, y + 1, false);
    wp->n[4] = GetWaypoint(x - 1, y, false);
    wp->n[5] = GetWaypoint(x, y + 2, false);
    wp->n[6] = GetWaypoint(x + 1, y, false);
    wp->n[7] = GetWaypoint(x, y - 2, false);
    if (wp->n[2]) wp->n[2]->n[3] = wp;
    if (wp->n[3]) wp->n[3]->n[2] = wp;
    if (wp->n[0]) wp->n[0]->n[1] = wp;
    if (wp->n[1]) wp->n[1]->n[0] = wp;
    if (wp->n[4]) wp->n[4]->n[6] = wp;
    if (wp->n[5]) wp->n[5]->n[7] = wp;
    if (wp->n[6]) wp->n[6]->n[4] = wp;
    if (wp->n[7]) wp->n[7]->n[5] = wp;
}

void LinkAdjacentWaypoints() {
    if (!allowWaypointLink) return;
    for (Waypoint* wp : g_list) {
        int x = wp->x, y = wp->y;
        bool odd = (y & 1) != 0;
        auto link = [&](int i, int tx, int ty) {
            if (!Map::GetBlock(tx, ty)) wp->n[i] = GetWaypoint(tx, ty, false);
        };
        link(0, odd ? x : x - 1, y - 1);
        link(3, odd ? x + 1 : x, y - 1);
        link(2, odd ? x : x - 1, y + 1);
        link(1, odd ? x + 1 : x, y + 1);
        link(4, x - 1, y);
        link(5, x, y + 2);
        link(6, x + 1, y);
        link(7, x, y - 2);
        // (CreateWaypointDebugOutput: the debug overlay is off)
    }
    ClassifyWaypoints();
    Render::SortRenderLayer(9, true);
}

void ClassifyWaypoints() {
    if (g_list.empty()) return;
    for (Waypoint* wp : g_list) wp->part = 0;
    unsigned part = 1;
    for (Waypoint* wp : g_list)
        if (wp->part == 0) ClassifyWaypoint(wp, part++);
}

void FreeWaypoints() {
    std::printf("AI::FreeWaypoints() Freeing %d waypoints\n", (int)g_list.size());
    for (Waypoint* wp : g_list) delete wp;
    g_list.clear();
    for (BaseAI* ai : BaseAI::All()) ai->OnMapRemove();
    // (the farm waypoints and the two search grids are freed here too)
    g_grid.clear();
    g_gridW = g_gridH = 0;
}

bool IsValidWaypoint(const Waypoint* wp) {
    for (Waypoint* w : g_list)
        if (w == wp) return true;
    for (Waypoint* w : g_farmList)
        if (w == wp) return true;
    return false;
}

int GetNextWaypointMarker() { return g_marker++; }

int GetNearest(const std::vector<Waypoint*>& v) {
    if (v.size() < 2) return 0;
    int best = 0;
    float c = v[0]->cost;
    for (size_t i = 1; i < v.size(); ++i) {
        if (v[i]->cost < c) {
            best = (int)i;
            c = v[i]->cost;
        }
    }
    return best;
}

Waypoint* GetNearestWP(int x, int y, float wx, float wy, int& dir) {
    return NearestOf(x, y, wx, wy, dir, [](Waypoint*) { return true; });
}

Waypoint* GetNearestWPZeroWeight(int x, int y, float wx, float wy, int& dir) {
    return NearestOf(x, y, wx, wy, dir, [](Waypoint* c) { return c->weight <= 1.f; });
}

Waypoint* GetNearestFreeWP(int x, int y, float wx, float wy, int& dir, Entity* self) {
    return NearestOf(x, y, wx, wy, dir, [&](Waypoint* c) {
        Entity* e = EntityManager::GetEntityAtXY(c->x, c->y);
        return e == self || e == nullptr;
    });
}

Waypoint* GetWaypointNearBuildZone(int x, int y, int w, int h, bool noCorners) {
    g_nearWeighted.clear();
    g_nearAll.clear();
    int H = h + 1, W = w + 1;
    for (int r = 0; r <= H; ++r) {
        int cx = x;
        for (int c = 0; c <= W; ++c) {
            int cy = y - c;
            bool corner = (c == 0 && (r == 0 || r == H)) || (c == W && (r == 0 || r == H));
            if (!noCorners || !corner) {
                if (Waypoint* wp = GetWaypoint(cx, cy, false)) {
                    if ((int)wp->weight == 1) g_nearWeighted.push_back(wp);
                    g_nearAll.push_back(wp);
                }
            }
            if (cy % 2 == 1) ++cx;
        }
        int nx = (std::abs(y) & 1) ? x + 1 : x;
        y -= 1;
        x = nx - 1;
    }
    if (!g_nearWeighted.empty()) return g_nearWeighted[(unsigned)Rand48::lrand48() % g_nearWeighted.size()];
    if (!g_nearAll.empty()) return g_nearAll[(unsigned)Rand48::lrand48() % g_nearAll.size()];
    return nullptr;
}

// The tile under a w x h zone's corner the ring starts from (shared by the two callers below).
static Waypoint* NearZone(int bx, int by, int w, int h, bool noCorners) {
    unsigned x = (unsigned)bx, y = (unsigned)by + 1;
    if (by & 1) x += 1;
    int half = w / 2;
    if (half >= 0) {
        unsigned end = (unsigned)by + 2 + (unsigned)half;
        while (y != end) {
            if (y & 1) x += 1;
            x -= 1;
            ++y;
        }
    }
    return GetWaypointNearBuildZone((int)x, (int)y, w, h, noCorners);
}

Waypoint* GetWaypointNearBuilding(Map::Building* b, bool noCorners) {
    const GameData::BuildingData* d = GameData::GetBuilding(b->id);
    if (!d) return nullptr;
    int w = b->IsMirrored() ? d->h : d->w;
    int h = b->IsMirrored() ? d->w : d->h;
    Waypoint* wp = NearZone(b->x, b->y, w, h, noCorners);
    if (!wp)
        std::printf("AI::GetWaypointNearBuilding() Cannot find any waypoints near the building at %dx%d\n", b->x, b->y);
    return wp;
}

Waypoint* GetWaypointNearDecoration(Map::Decor* dc, bool noCorners) {
    const GameData::DecorData* d = GameData::GetDecoration(dc->id);
    if (!d) return nullptr;
    int w = dc->mirrored ? d->h : d->w;
    int h = dc->mirrored ? d->w : d->h;
    Waypoint* wp = NearZone(dc->x, dc->y, w, h, noCorners);
    if (!wp)
        std::printf("AI::GetWaypointNearBuilding() Cannot find any waypoints near the decor at %dx%d\n", dc->x, dc->y);
    return wp;
}

void GetWaypointsInRange(std::vector<Waypoint*>& out, unsigned x, unsigned y, unsigned range, bool all) {
    int marker = GetNextWaypointMarker();
    if (Waypoint* wp = GetWaypoint((int)x, (int)y, false)) {
        out.push_back(wp);
        wp->marker = marker;
    }
    size_t prev = out.size();
    if (prev != 0 && range != 0) {
        size_t i = 0;
        for (;;) {
            size_t end = prev;
            for (; i < prev; ++i) {
                for (Waypoint* n : out[i]->n) {
                    // UNVERIFIED: without `all`, neighbours under a decoration with meta type 0x6f
                    // are skipped; decoration meta expressions are not ported yet.
                    if (n && n->marker != marker) {
                        out.push_back(n);
                        n->marker = marker;
                    }
                }
            }
            if (out.size() == prev || --range == 0) break;
            prev = out.size();
            i = end;
        }
    }
    (void)all;
}

Waypoint* GetRandomRoadWaypoint() {
    if (g_list.empty()) return nullptr;
    return g_list[(unsigned)Rand48::lrand48() % g_list.size()];
}

Waypoint* GetRandomFreeWPRect(int w, int h, int x, int y) {
    for (int tries = 0x40; tries != 0; --tries) {
        int ry = (int)(Rand48::lrand48() % h);
        int rx = (int)(Rand48::lrand48() % w);
        // (the original indexes the grid directly: g_grid[W * (ry + y) + x + rx])
        long i = (long)g_gridW * (ry + y) + x + rx;
        Waypoint* wp = (i >= 0 && (size_t)i < g_grid.size()) ? g_grid[(size_t)i] : nullptr;
        if (wp && !Map::GetDecoration(wp->x, wp->y) && !Map::GetBuilding(wp->x, wp->y) &&
            !EntityManager::GetEntityAtXY(wp->x, wp->y) && !EntityManager::GetEntityWithTargetPositionAtXY(wp->x, wp->y))
            return wp;
    }
    return nullptr;
}

Waypoint* GetRandomFreeWP(int w, int h, int x, int y) {
    for (int attempts = 0x40;;) {
        int ry = (int)(Rand48::lrand48() % h);
        int rx = (int)(Rand48::lrand48() % w);
        Waypoint* wp = nullptr;
        bool retry = true;
        if (h > 0) {
            int rowX = x;
            for (int row = y;;) {
                int cx = rowX, cy = row;
                if (w > 0 && rx != 0) {
                    int k = row;
                    do {
                        cy = k + 1;
                        if (k % 2 == 1) ++cx;
                        k = cy;
                    } while (cy != w + row && cy != row + rx);
                }
                if (row == y + ry) {
                    wp = GetWaypoint(cx, cy, false);
                    if (GameState::GetCurrentMapID() == 0x70 || !wp ||
                        (!Map::GetDecoration(wp->x, wp->y) && !Map::GetBuilding(wp->x, wp->y))) {
                        retry = wp == nullptr;
                    } else {
                        wp = nullptr;
                    }
                    break;
                }
                if (std::abs(row) & 1) ++rowX;
                ++row;
                if (row == y + h) break;
                --rowX;
            }
        }
        if (--attempts < 1) retry = false;
        if (!retry) return wp;
    }
}

Waypoint* GetRandomWaypointInRange(int x, int y, int range, int inner, bool weightOne, Entity* e, bool reachable) {
    static std::vector<Waypoint*> found;   // 0x610b60
    int size = range * 2 + 1;
    int innerSize = inner != 0 ? inner * 2 + 1 : 0;
    found.clear();
    // From (x, y) to the square's top corner: `range` steps up-left, then `range` steps up-right.
    int half = size / 2;
    if (half != 0) {
        int end = y - half;
        do {
            if (std::abs(y) & 1) ++x;
            --y;
            --x;
        } while (y != end);
        end = y - half;
        do {
            if (y % 2 == 1) ++x;
            --y;
        } while (y != end);
    }
    int lo = (size - innerSize) / 2, hi = range * 2 - lo;   // the inner square left out
    for (int row = 0; row < size; ++row) {
        int cx = x, cy = y;
        for (int col = 0; col < size; ++col) {
            if (!(lo <= col && col <= hi && lo <= row && row <= hi)) {
                Waypoint* wp = GetWaypoint(cx, cy, false);
                bool ok;
                if (weightOne) ok = wp && wp->weight == 1.f;
                else ok = wp && !EntityManager::GetEntityAtXY(wp->x, wp->y) && !Map::GetBuilding(wp->x, wp->y);
                if (ok && reachable && e) ok = e->GetAI()->SetTarget(wp, true);
                if (ok) found.push_back(wp);
            }
            if (cy % 2 == 1) ++cx;
            ++cy;
        }
        if (std::abs(y) & 1) ++x;
        --x;
        ++y;
    }
    if (found.empty()) return nullptr;
    return found[(size_t)((unsigned long)Rand48::lrand48() % found.size())];
}

}  // namespace AI
