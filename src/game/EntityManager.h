// EntityManager: the entities in play, their creation, per-frame update and lookups.
// Port of EntityManager (libkingdom.so 5.11, @0x1654a8..0x168530). On the original it is an App
// member (App::GetEntityManager); its +0x18 is the EntityFactory, +0x1c the player.
#pragma once
#include <string>

class Entity;
namespace Map { struct Building; }
struct SpawnPoint;

namespace EntityManager {

// @0x167cac: a new entity of type id (0x135 if there is none, after an error). Workers (class 0)
// and farmers (class 2) get random names; a class-5 entity becomes the player unless noPlayer.
// temporary (+0x3a) entities are removed by Clean(false).
Entity* CreateEntity(int id, bool noPlayer, bool temporary);
// @0x165600: place e at a tile on the current map and activate it; fade it in if appear.
Entity* SpawnEntityAt(Entity* e, unsigned x, unsigned y, bool appear, bool glow);
Entity* SpawnEntityAt(int id, unsigned x, unsigned y, bool appear, bool glow);   // @0x167f1c
Entity* SpawnEntityAt(const std::string& name, unsigned x, unsigned y, bool appear, bool glow);  // @0x167fc0
void AddEntity(Entity* e);                       // @0x168488
void RemoveEntity(Entity* e, bool any);          // @0x165794 (without any: temporary ones only)
void DestroyEntity(Entity* e);                   // @0x1656f8
void Clean(bool keepPlayer);                     // @0x167890

void Update(float dt);                           // @0x1676b4
void UpdateGraphics();                           // @0x166868

Entity* EnumEntities(unsigned i);                // @0x167c38
int GetEntityCount(bool permanentOnly);          // @0x167c54
Entity* GetPlayer();                             // @0x165500
void ResetOrders(Map::Building* b);              // @0x1669dc every AI forgets its orders to b
Entity* GetEntityAtXY(int x, int y);             // @0x16613c (active, alive)
// @0x16716c: the entity whose sprite box (Entity::Contains) holds the world point, the nearest
// (lowest z) first among living non-NPCs, then living ones, then any active one.
Entity* GetEntityAtWorldXY(int x, int y);
// @0x166594: an active worker (class 0, not an NPC) with neither a workplace nor a decoration job.
Entity* GetFreeWorker();
// @0x16632c: a worker (class 0) working at a building (not a decoration).
Entity* GetFirstBusyWorker();
int GetEntityCountAtXY(int x, int y);            // @0x1668dc
unsigned GetEntityCountByID(unsigned id);        // @0x1667c8
// @0x165d30: an active, living non-player entity whose AI's last target (or patrol end) is (x, y).
Entity* GetEntityWithTargetPositionAtXY(int x, int y);

}  // namespace EntityManager
