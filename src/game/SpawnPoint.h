// SpawnPoint (@0x224f84, 0x54 bytes): where a campaign map places an entity, with what the save keeps
// about that entity (HP, death time). Portal (@0x1e5de8, 0x20 bytes): a decoration that leads to
// another map. Both are loaded by Map::LoadSpawnPoints / LoadPortals (src/game/MapSpawn.cpp).
#pragma once
#include <cstdint>

class MetaExpression;

struct SpawnPoint {
    uint32_t id = 0;                 // +0x00 unique on its map (Entity::GetSpawnPointID)
    uint32_t entityId = 0;           // +0x04 the EntityData id
    int x = 0, y = 0;                // +0x08 +0x0c tile
    bool npc = false;                // +0x10 (Entity::IsNPC)
    int clas = 0;                    // +0x14 the entity's class (5 or 10 with entity 0xc: the hero's own)
    int f18 = 0;                     // +0x18
    int f1c = 0;                     // +0x1c
    bool f20 = false;                // +0x20
    int f24 = 0;                     // +0x24 (0 for a wandering player shadow)
    int initiative = 0;              // +0x28 (Entity +0x130)
    MetaExpression* meta = nullptr;  // +0x2c its entity's quest behaviour
    bool hasHp = false;              // +0x30 chunk 0x14 was saved
    int hp = 0;                      // +0x34
    bool hasHpMax = false;           // +0x38 chunk 0x25 was saved
    int hpMax = 0;                   // +0x3c
    int f40 = 0, f44 = 0;            // +0x40 +0x44 (chunk 0x28)
    int corpseTaken = 0;             // +0x48 (chunk 0x42) the dead entity's corpse item is gone (it is switched off)
    uint32_t deathTime = 0;          // +0x4c (chunk 0x44) when its entity died (0: alive)
    int f50 = 0;                     // +0x50 (chunk 0x45; compared with MOB_SPAWN's first number)

    SpawnPoint() = default;
    SpawnPoint(const SpawnPoint&) = delete;
    SpawnPoint& operator=(const SpawnPoint&) = delete;
    ~SpawnPoint();                   // @0x22503c
};

struct Portal {
    int map = 0;                     // +0x00 the map it leads to
    int f04 = 0;                     // +0x04
    int f08 = 0, f0c = 0;            // +0x08 +0x0c
    int x = 0, y = 0;                // +0x10 +0x14 its decoration's tile
    int walkX = 0, walkY = 0;        // +0x18 +0x1c where the hero walks to use it
};
