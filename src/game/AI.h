// AI: the waypoint graph entities walk on, one waypoint per walkable tile with links to its eight
// neighbours, and the static helpers that search it. Port of the AI namespace (libkingdom.so 5.11,
// @0xe1648..0xe8050).
//
// The city (map 0) gets a waypoint on every tile (Map::CreateRoadAI); other maps get one per tile
// of their walkable list. A waypoint's weight is its walking cost: 1, 1000 under a building or a
// layer-0 city decoration, 0.1 on a road decoration, 10 under a decoration with meta type 0x6f.
// ClassifyWaypoints numbers the connected parts; paths are only searched within one part.
//
// The farm view has its own small grid (GetWaypoint(x, y, true), CreateFarmWaypoints), linked
// diagonally only.
#pragma once
#include <vector>

namespace Render { struct Sprite; }
namespace Map { struct Building; struct Decor; }
class Entity;

namespace AI {

struct Waypoint {                // 0x4c bytes
    int x = -1, y = -1;          // +0x00 +0x04 tile
    float wx = 0.f, wy = 0.f;    // +0x08 +0x0c world position
    unsigned index = 0;          // +0x10 slot in the waypoint list
    // +0x14.. neighbours: up-left, down-right, down-left, up-right (the diagonal tile steps), then
    // left (x-1), down (y+2), right (x+1), up (y-2)
    Waypoint* n[8] = {};
    float cost = 0.f;            // +0x34 search cost
    Waypoint* parent = nullptr;  // +0x38 search parent
    unsigned part = 0;           // +0x3c connected part (ClassifyWaypoints), 1-based
    Render::Sprite* debug = nullptr;  // +0x40 (debug overlay, not ported)
    float weight = 1.f;          // +0x44
    int marker = 0;              // +0x48 GetWaypointsInRange visit marker
};

extern bool allowWaypointLink;   // 0x60ef0c (1)
extern float weightCost;         // AI::mWeigthCost 0x60ef10 (1.41): factor for the left/down/right/up steps

Waypoint* GetWaypoint(int x, int y, bool farm);   // @0xe169c
void CreateMapWaypoint(int x, int y);             // @0xe7c78
void RemoveMapWaypoint(int x, int y);             // @0xe28e8
void LinkAdjacentWaypoints(Waypoint* wp);         // @0xe17e0 (both directions, no block check)
void LinkAdjacentWaypoints();                     // @0xe33a4 every waypoint to its unblocked neighbours
void ClassifyWaypoints();                         // @0xe1740
void FreeWaypoints();                             // @0xe26f0
bool IsValidWaypoint(const Waypoint* wp);         // @0xe19cc
int GetNextWaypointMarker();                      // @0xe19ac
const std::vector<Waypoint*>& GetWaypoints();     // the list (0x610af8)
const std::vector<Waypoint*>& GetFarmWaypoints(); // the farm's list (0x610b08)
void CreateFarmWaypoints(int x, int y);           // @0xe7a08
void LinkAdjacentFarmWaypoints();                 // @0xe35f4
void RemoveFarmWaypoints();                       // @0xe192c

// The shared search list (0x610b38) and GetNearest @0xe1b7c (lowest cost; 0 for 0 or 1 entries).
std::vector<Waypoint*>& SearchList();
int GetNearest(const std::vector<Waypoint*>& v);

// @0xe2a68 / @0xe2d70 / @0xe2bd8: the nearest of the four diagonal neighbours of tile (x, y) to the
// world point (wx, wy); dir gets 1, 3, 5 or 7 (the direction to it) or -1.
Waypoint* GetNearestWP(int x, int y, float wx, float wy, int& dir);
Waypoint* GetNearestWPZeroWeight(int x, int y, float wx, float wy, int& dir);   // weight <= 1 only
Waypoint* GetNearestFreeWP(int x, int y, float wx, float wy, int& dir, Entity* self);  // no other entity
// @0xe457c: a random waypoint on the ring around a w x h zone whose corner is (x, y); one with
// weight 1 if there is any. Without corners if noCorners.
Waypoint* GetWaypointNearBuildZone(int x, int y, int w, int h, bool noCorners);
Waypoint* GetWaypointNearBuilding(Map::Building* b, bool noCorners);    // @0xe4a80
Waypoint* GetWaypointNearDecoration(Map::Decor* d, bool noCorners);     // @0xe4bc4
// @0xe7f68: breadth-first from (x, y) for `range` steps, appended to out.
void GetWaypointsInRange(std::vector<Waypoint*>& out, unsigned x, unsigned y, unsigned range, bool all);
Waypoint* GetRandomRoadWaypoint();                // @0xe3ac0 (any waypoint)
Waypoint* GetRandomFreeWP(int w, int h, int x, int y);   // @0xe396c
// @0xe388c: up to 64 tries at a random tile of the w x h grid rectangle at (x, y) that has a
// waypoint and no decoration, building, entity or entity heading there.
Waypoint* GetRandomFreeWPRect(int w, int h, int x, int y);
// @0xe4cdc: a random waypoint in the (2 range + 1)-tile square around (x, y) (in tile steps), leaving
// out the inner (2 inner + 1) square (inner 0: the centre tile). weightOne: any waypoint of weight 1; else one
// without an entity or building. With reachable and e, only those e's AI finds a path to (SetTarget
// is called on e's AI for each). Null if none.
Waypoint* GetRandomWaypointInRange(int x, int y, int range, int inner, bool weightOne, Entity* e, bool reachable);

}  // namespace AI
