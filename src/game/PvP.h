// PvP: the opponents' records (PvP::PlayerInfo) and the PvP counters. Port of the offline part of
// PvP (libkingdom.so 5.11, 0x1eb5d0..0x1f2a88; statics at 0x613f58..0x613fd0). The opponents
// came from the server (online, dormant); the tutorial's fake rivals are made here.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "game/PlayerEquipmentInfo.h"

namespace PvP {

struct PlayerInfo {                    // 0x428 bytes (constructor @0x1eb9cc)
    struct WarriorInfo {               // 0x10 bytes (@0x1eb628)
        uint32_t id = 0;               // +0x00 entity id
        bool f04 = false;              // +0x04 (true for every warrior loaded)
        int f08 = 0;                   // +0x08 HP (the save's)
        int hp = 1;                    // +0x0c max HP (the save's)
    };
    int gid = 0;                       // +0x000 (-1: the tutorial's ambusher, -2..-4: its shadows)
    uint32_t lastFight = 0;            // +0x004 when last beaten (global time)
    int losses = 0;                    // +0x008 times the hero lost to him
    int wins = 0;                      // +0x00c
    int f10 = 0;                       // +0x010 the level of profession 7 (chunk 0x41)
    int level = 0;                     // +0x014
    bool male = false;                 // +0x018
    int type = 0;                      // +0x01c PvPPlayerType (0xfd the ambusher, 1 shadows, 200 done)
    PlayerEquipmentInfo equipment;     // +0x020 (items[10], customs[32])
    int warriorCount = 0;              // +0x368
    WarriorInfo warriors[10];          // +0x36c
    char32_t* name = nullptr;          // +0x40c the name shown
    char32_t* str410 = nullptr;        // +0x410 the castle's name
    char32_t* str414 = nullptr;        // +0x414 the save's own name when another one is shown
    int f418 = 0;                      // +0x418 the save's chunk 0x43
    int defeated = 0;                  // +0x41c
    int f420 = 0;                      // +0x420 (counts the hero's defeats too)
    uint32_t valor = 0;                // +0x424 valor for beating him
    ~PlayerInfo();                     // @0x1eb94c
};

// One opponent as the server sent it (0x24 bytes): his game state block (the chunks
// GameState::Save writes: 0x10 name, 0x41 professions, 0x43, 0x4f level / gear / belt / army /
// record / customisations) and his record against the player. +0x10, +0x14 and +0x20 are read
// from the answer but nothing reads them.
struct PvPSave {
    std::vector<uint8_t> data;         // +0x00
    uint32_t f04 = 0;                  // +0x04 (always 0)
    int gid = 0;                       // +0x08
    uint32_t lastFight = 0;            // +0x0c
    uint32_t f10 = 0;                  // +0x10
    uint32_t f14 = 0;                  // +0x14
    int losses = 0;                    // +0x18
    int wins = 0;                      // +0x1c
    uint8_t f20 = 0;                   // +0x20
};

// A list as it was loaded (Cache<PvP::SaveCache>, kept in memory and in the storage file "pc<type>"):
// the server's answer and, per opponent of that type in list order, his record.
struct SaveCache {                     // 0x20 bytes
    std::string data;                  // +0x00 its length, +0x1c the text
    uint32_t time = 0;                 // +0x04
    uint32_t count = 0;                // +0x08 the arrays' length
    std::vector<uint32_t> lastFight;   // +0x0c PlayerInfo +0x004
    std::vector<uint32_t> losses;      // +0x10 PlayerInfo +0x008
    std::vector<uint32_t> wins;        // +0x14 PlayerInfo +0x00c
    std::vector<uint32_t> defeated;    // +0x18 PlayerInfo +0x41c
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
// The list's loading state (the lists came from the server; 5t-3 fills them locally).
void SetLastLoadedTime(int type, uint32_t t);    // @0x1eb6e4
uint32_t GetLastLoadedTime(int type);            // @0x1eb6fc
void ResetLastLoadedTime(unsigned type);         // @0x1eb714
bool IsLoadingData();                            // @0x1eb784
int GetLoadedDataType();                         // @0x1eb7a4
uint32_t GetRefreshTime(int type);               // @0x1eb7c8
void SetRefreshTime(int type, uint32_t t);       // @0x1eb7e0
void RefreshPlayers(int type);                   // @0x1ec890
int GetTutorialDeadPlayers();                    // @0x1ec7a8
// The lists (type 1 the PvP tab's opponents and the tutorial's shadows, 2 / 3 the other tabs'
// lists, 4 the rivals met on the maps, 5 one player looked up): from the cache while it is fresh,
// else asked for. The server is replaced by PvPServer (offline, DECIDED 2026-10-18); its answer
// goes through PvPDataLoadFinished as the server's did. The callback gets whether a list came.
void LoadPvPData(int type, void (*done)(bool), int cost, const char* uid);   // @0x1f2a88
void PvPDataLoadFinished(bool unused, const uint8_t* data, int size);       // @0x1f131c
// A new record from an opponent's save (the name, when given, is shown instead of his own).
bool LoadPlayerInfo(const PvPSave& save, int type, const char32_t* name);   // @0x1ecc08
int GetRandomPlayerGID(int type, bool unused);   // @0x1ed9fc a random one of `type` not beaten
// The lists' cache ("pc<type>"): RemoveCachedData marks one stale (hard: deletes it and its file),
// UpdateCachedData writes the records of `type` back into it.
void ClearAllCache();                            // @0x1f249c
void RemoveCachedData(int type, bool hard);      // @0x1f2634
void UpdateCachedData(int type);                 // @0x1f1ac8
namespace Cache {
void Set(const std::string& key, const SaveCache& c, uint32_t time);   // @0x1ef788
bool Save(const std::string& key);               // @0x1f011c
void Free(const std::string& key);               // @0x1f0500 (memory only)
bool Get(const std::string& key, SaveCache& out);   // @0x1f09bc (memory, else the file)
bool IsValid(const std::string& key);            // @0x1f0fd0
void Remove(const std::string& key);             // @0x1f1ef8 (memory and file)
}
// The server's side (online, dormant).
uint32_t GetMyUID();                             // @0x1eb7b4
void SetMyUID(uint32_t uid);                     // @0x1ebcc8
void SetQueuedPlayerID(uint32_t id);             // @0x1eb858
uint32_t GetQueuedPlayerID();                    // @0x1eb86c

}  // namespace PvP

namespace GameState {
// @0x19b3d0 The server's PvP answer: "\x1f" or "?\r", then big-endian: the seconds until the next
// refresh, the crystal cost of this refresh, the count; per opponent 25 bytes (gid, seconds since
// the last fight, two words, losses, wins, a byte); per opponent a block (two words, the size, the
// game state block); optionally per opponent a UTF-8 name (u16 length). False for a bad answer.
bool ParsePvPData(const uint8_t* data, uint32_t size, std::vector<PvP::PvPSave>& saves,
                  uint32_t& refreshDelay, uint32_t& cost, std::vector<char32_t*>& names);
}

// The PvP opponents' window (online, milestone 5): only the selection the fights read.
namespace PlayerCompetitionWindow {
int GetSelectedPlayerGID();                      // @0x301f2c
void SetSelectedPlayerGID(int gid, bool fetch);  // @0x30255c
void SetReturnWithoutWindow(bool on);            // @0x301f40
bool HasReturnWithoutWindow();                   // @0x301f54
void SetDefeatedPlayer(uint32_t spawn);          // @0x301f68
bool HasDefeatedPlayer(uint32_t spawn);          // @0x301f7c
}
