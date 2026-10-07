// BaseCombat: a fight between the player's squad and an enemy squad (milestone 4g). Only the virtual
// functions that already-ported code calls are declared, under their vtable slots; no combat is
// created yet, so GameState::GetActiveCombat is always null. Port of BaseCombat (@0x10c3cc..0x110bd0;
// slots read from AggroCombat's vtable).
#pragma once

class Entity;

class BaseCombat {
public:
    virtual ~BaseCombat() = default;                 // +0x00
    virtual void AddGoodGuy(Entity* e) = 0;          // +0x2c @0x110998
    virtual void AddBadGuy(Entity* e) = 0;           // +0x30 @0x110bd0
    virtual int GetState() = 0;                      // +0x4c @0x10c4ac
    virtual int GetType() = 0;                       // +0x50 @0x10c4a4 (5, 6: arena types)
    virtual int GetBadiesCount() = 0;                // +0x54 @0x10d70c
};
