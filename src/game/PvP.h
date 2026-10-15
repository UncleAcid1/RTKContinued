// PvP: the opponents' records (PvP::PlayerInfo) and the PvP counters. Port of the offline part of
// PvP (libkingdom.so 5.11, 0x1eb5d0..0x1f2a88; statics at 0x613f58..0x613fd0). The opponents
// came from the server (online, dormant); the tutorial's fake rivals are made here.
#pragma once
#include <cstdint>
#include <vector>

#include "game/PlayerEquipmentInfo.h"

namespace PvP {

struct PlayerInfo {                    // 0x428 bytes (constructor @0x1eb9cc)
    struct WarriorInfo {               // 0x10 bytes (@0x1eb628)
        uint32_t id = 0;               // +0x00 entity id
        bool f04 = false;              // +0x04
        int f08 = 0;                   // +0x08
        int hp = 1;                    // +0x0c
    };
    int gid = 0;                       // +0x000 (-1: the tutorial's ambusher, -2..-4: its shadows)
    uint32_t lastFight = 0;            // +0x004 when last beaten (global time)
    int losses = 0;                    // +0x008 times the hero lost to him
    int wins = 0;                      // +0x00c
    int f10 = 0;                       // +0x010
    int level = 0;                     // +0x014
    bool male = false;                 // +0x018
    int type = 0;                      // +0x01c PvPPlayerType (0xfd the ambusher, 1 shadows, 200 done)
    PlayerEquipmentInfo equipment;     // +0x020 (items[10], customs[32])
    int warriorCount = 0;              // +0x368
    WarriorInfo warriors[10];          // +0x36c
    char32_t* name = nullptr;          // +0x40c
    char32_t* str410 = nullptr;        // +0x410
    char32_t* str414 = nullptr;        // +0x414
    int f418 = 0;                      // +0x418
    int defeated = 0;                  // +0x41c
    int f420 = 0;                      // +0x420 (counts the hero's defeats too)
    uint32_t valor = 0;                // +0x424 valor for beating him
    ~PlayerInfo();                     // @0x1eb94c
};

void Init();                                     // @0x1eb660
void ClearPlayerInfo(int type);                  // @0x1ebb64 (0xff: all; the shadows stay)
void CreatePvPTutorialPlayer();                  // @0x1eef64 the ambusher (gid -1)
void CreatePvPTutorialPlayersRandom();           // @0x1ef1bc the three shadows (gid -2..-4)
PlayerInfo* GetPlayerInfoByGID(int gid, int type = 0xff);   // @0x1ec368 (the type is not checked)
PlayerInfo* EnumPlayers(unsigned i);             // @0x1ec780
bool IsCompletedPvPTutorial();                   // @0x1eb828
void SetCompletedPvPTutorial(bool done);         // @0x1ec7fc
uint32_t GetVictoryCount();                      // @0x1eb74c
void SetVictoryCount(uint32_t n);                // @0x1eb738
uint32_t GetDefeatCount();                       // @0x1eb770
void SetDefeatCount(uint32_t n);                 // @0x1eb75c
uint32_t GetTutorialLastRefresh();               // @0x1eb7f8
void SetTutorialLastRefresh(uint32_t t);         // @0x1eb80c
inline unsigned GetTutorialPlayerCount() { return 3; }   // @0x1eb820
void GetTutorialPlayerStats(unsigned i, uint32_t& lastFight, char& losses, char& wins);   // @0x1ec3d0
void SetTutorialPlayerStats(unsigned i, uint32_t lastFight, char losses, char wins);      // @0x1ec41c
void ResetTutorial();                            // @0x1eb83c

}  // namespace PvP

// The PvP opponents' window (online, milestone 5): only the selection the fights read.
namespace PlayerCompetitionWindow {
int GetSelectedPlayerGID();                      // @0x301f2c
void SetSelectedPlayerGID(int gid, bool fetch);  // @0x30255c
void SetReturnWithoutWindow(bool on);            // @0x301f40
bool HasReturnWithoutWindow();                   // @0x301f54
void SetDefeatedPlayer(uint32_t spawn);          // @0x301f68
bool HasDefeatedPlayer(uint32_t spawn);          // @0x301f7c
}
