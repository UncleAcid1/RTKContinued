// Map::SpawnPoint: where a campaign map places an entity. Loaded by Map::LoadSpawnPoints
// (milestone 4f); until then only the fields the entities' quest reactions use are here.
#pragma once
#include <cstdint>

class MetaExpression;

struct SpawnPoint {
    uint32_t entityId = 0;           // +0x04 the EntityData id
    int x = 0, y = 0;                // +0x08 +0x0c tile
    bool npc = false;                // +0x10 (Entity::IsNPC)
    MetaExpression* meta = nullptr;  // +0x2c its entity's quest behaviour
    int hp = 0;                      // +0x34
    int hpMax = 0;                   // +0x3c
    uint32_t deathTime = 0;          // +0x4c when its entity died (0: alive)
    int f50 = 0;                     // +0x50 (compared with MOB_SPAWN's first number)
};
