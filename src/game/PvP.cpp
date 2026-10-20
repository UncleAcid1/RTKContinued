#include "game/PvP.h"

#include <cstring>
#include <string>

#include "game/GameState.h"
#include "game/StringTable.h"

namespace PvP {
namespace {

std::vector<PlayerInfo*> g_players;   // 0x613fc8
uint32_t g_victories = 0;             // 0x613f58
uint32_t g_defeats = 0;               // 0x613f5c
uint32_t g_lastLoaded[6] = {};        // 0x613f60 when each list type was last loaded
uint32_t g_refreshTime[8] = {};       // 0x613f78 (the index is not checked, as the original)
bool g_loading[2] = {};               // 0x613f98 a list / a single player is loading (online)
uint32_t g_tutorialLastRefresh = 0;   // 0x613f9c
bool g_tutorialCompleted = false;     // 0x613fa0
uint32_t g_queuedPlayerID = 0;        // 0x613fa4 the friend attacked from a visit (online)
bool g_openWindowOnUID = false;       // 0x613fa8 set by QueuePvPWindowOpen only (dead in 5.11)
uint32_t g_myUID = 0;                 // 0x613fac the server's id for the player (online)
int g_loadedDataType = 2;             // 0x60f030

// A new record at the end of the list.
PlayerInfo* Add() {
    g_players.push_back(new PlayerInfo());
    return g_players.back();
}

// The shadows' and the ambusher's name: "PVP_PLAYER_SHADOW_NAME" (or "A").
char32_t* ShadowName() {
    const char32_t* s = StringTable::GetString("PVP_PLAYER_SHADOW_NAME");
    if (!s) s = U"A";
    const std::u32string copy(s);
    char32_t* out = new char32_t[copy.size() + 1];
    std::memcpy(out, copy.c_str(), (copy.size() + 1) * sizeof(char32_t));
    return out;
}

}  // namespace

PlayerInfo::~PlayerInfo() {
    delete[] name;
    delete[] str410;
    delete[] str414;
}

// UNVERIFIED (5t-3): +0x3c (0x613f94), zeroed here too, is not read yet; the uid stays.
void Init() {
    g_victories = 0;
    g_defeats = 0;
    for (uint32_t& t : g_lastLoaded) t = 0;
    for (uint32_t& t : g_refreshTime) t = 0;
    g_loading[0] = g_loading[1] = false;
    g_tutorialLastRefresh = 0;
    g_tutorialCompleted = false;
    g_queuedPlayerID = 0;
    g_openWindowOnUID = false;
    g_loadedDataType = 0xfe;
}

void SetLastLoadedTime(int type, uint32_t t) { g_lastLoaded[type] = t; }   // @0x1eb6e4
uint32_t GetLastLoadedTime(int type) { return g_lastLoaded[type]; }       // @0x1eb6fc

// @0x1eb714
void ResetLastLoadedTime(unsigned type) {
    if (type > 5) return;
    g_lastLoaded[type] = 0;
}

bool IsLoadingData() { return g_loading[0] || g_loading[1]; }   // @0x1eb784
int GetLoadedDataType() { return g_loadedDataType; }           // @0x1eb7a4
uint32_t GetMyUID() { return g_myUID; }                        // @0x1eb7b4
uint32_t GetRefreshTime(int type) { return g_refreshTime[type]; }          // @0x1eb7c8
void SetRefreshTime(int type, uint32_t t) { g_refreshTime[type] = t; }     // @0x1eb7e0
void SetQueuedPlayerID(uint32_t id) { g_queuedPlayerID = id; }             // @0x1eb858
uint32_t GetQueuedPlayerID() { return g_queuedPlayerID; }                  // @0x1eb86c

// @0x1ebcc8 The original then queues PlayerCompetitionWindow::Show (online) when the flag is set;
// only QueuePvPWindowOpen (dead in 5.11) sets it, so the branch never runs.
void SetMyUID(uint32_t uid) {
    g_myUID = uid;
    if (uid == 0 || !g_openWindowOnUID) return;
    g_openWindowOnUID = false;
}

// @0x1ec890 Refreshing the shadows' list (type 1) completes the PvP tutorial.
void RefreshPlayers(int type) {
    if (!g_tutorialCompleted && type == 1) SetCompletedPvPTutorial(true);
    ResetLastLoadedTime(type);
}

// @0x1ec7a8 The shadows (type 1) beaten.
int GetTutorialDeadPlayers() {
    int n = 0;
    for (unsigned i = 0; PlayerInfo* p = EnumPlayers(i); ++i)
        if (p->type == 1 && p->defeated == 1) ++n;
    return n;
}

// Records of `type` (0xff: every type) are deleted, except the shadows (gid -4..-2).
void ClearPlayerInfo(int type) {
    for (auto it = g_players.begin(); it != g_players.end();) {
        PlayerInfo* p = *it;
        if ((unsigned)(p->gid + 4) < 3 || (p->type != type && type != 0xff)) {
            ++it;
            continue;
        }
        delete p;
        it = g_players.erase(it);
    }
}

// The rival of the tutorial's ambush: level 5, her gear (0x20a, 0x205, 0x207, 0x209, 0x206,
// 0x208) broken so it gives her nothing, two warriors of entity 0x2b; 4 valor for beating her.
void CreatePvPTutorialPlayer() {
    PlayerInfo* p = Add();
    p->warriorCount = 2;
    p->gid = -1;
    p->warriors[1].f04 = true;
    p->type = 0xfd;
    p->warriors[1].hp = 5;
    p->losses = 7;
    p->equipment.items[3].broken = false;
    p->equipment.items[1].id = 0x205;
    p->warriors[1].id = 0x2b;
    p->equipment.items[6].id = 0x206;
    p->warriors[0].hp = 4;
    p->equipment.items[2].id = 0x207;
    p->wins = 0;
    p->equipment.items[7].id = 0x208;
    p->f10 = 1;
    p->equipment.items[3].id = 0x209;
    p->level = 5;
    p->equipment.items[0].id = 0x20a;
    p->male = true;
    p->equipment.items[1].broken = true;
    p->equipment.items[6].broken = true;
    p->equipment.items[2].broken = true;
    p->equipment.items[7].broken = true;
    p->equipment.items[3].f0c = 5;
    p->equipment.items[3].f08 = 5;
    p->equipment.items[0].broken = true;
    p->warriors[0].id = 0x2b;
    p->warriors[0].f08 = 4;
    p->warriors[0].f04 = true;
    p->warriors[1].f08 = 5;
    p->name = ShadowName();
    p->valor = 4;
}

// The three "shadows" of the PvP list's tutorial.
void CreatePvPTutorialPlayersRandom() {
    PlayerInfo* p = Add();
    p->gid = -2;
    p->warriors[0].f04 = true;
    p->lastFight = 0;
    p->warriors[0].hp = 4;
    p->type = 1;
    p->wins = 3;
    p->losses = 1;
    p->warriors[0].id = 0x2b;
    p->f10 = 1;
    p->level = 2;
    p->male = true;
    p->warriorCount = 1;
    p->warriors[0].f08 = 4;
    p->name = ShadowName();
    p->valor = 4;

    p = Add();
    p->warriors[1].id = 0x2b;
    p->warriors[0].id = 0x2b;
    p->gid = -3;
    p->warriors[1].f04 = true;
    p->lastFight = 0;
    p->warriors[1].hp = 5;
    p->level = 3;
    p->warriors[0].hp = 4;
    p->equipment.items[1].id = 0x5c;
    p->type = 1;
    p->equipment.items[6].id = 0x73;
    p->losses = 5;
    p->wins = 2;
    p->f10 = 2;
    p->male = true;
    p->warriorCount = 2;
    p->warriors[0].f08 = 4;
    p->warriors[0].f04 = true;
    p->warriors[1].f08 = 5;
    p->name = ShadowName();
    p->valor = 6;

    p = Add();
    p->gid = -4;
    p->warriorCount = 0;
    p->wins = 10;
    p->f10 = 1;
    p->type = 1;
    p->level = 4;
    p->lastFight = 0;
    p->losses = 2;
    p->male = false;
    p->name = ShadowName();
    p->valor = 3;
}

PlayerInfo* GetPlayerInfoByGID(int gid, int) {
    for (PlayerInfo* p : g_players)
        if (p->gid == gid) return p;
    return nullptr;
}

PlayerInfo* EnumPlayers(unsigned i) { return i < g_players.size() ? g_players[i] : nullptr; }

bool IsCompletedPvPTutorial() { return g_tutorialCompleted; }

// Only in the city. Once completed, the shadows (type 1) are done (type 200).
void SetCompletedPvPTutorial(bool done) {
    if (!GameState::IsPlayerCity()) return;
    if (done && !g_tutorialCompleted)
        for (unsigned i = 0; PlayerInfo* p = EnumPlayers(i); ++i)
            if (p->type == 1 && (unsigned)(p->gid + 4) < 3) p->type = 200;
    g_tutorialCompleted = done;
}

uint32_t GetVictoryCount() { return g_victories; }
void SetVictoryCount(uint32_t n) { g_victories = n; }
uint32_t GetDefeatCount() { return g_defeats; }
void SetDefeatCount(uint32_t n) { g_defeats = n; }   // @0x1eb75c
uint32_t GetTutorialLastRefresh() { return g_tutorialLastRefresh; }   // @0x1eb7f8
void SetTutorialLastRefresh(uint32_t t) { g_tutorialLastRefresh = t; }   // @0x1eb80c

// Shadow i is gid -2 - i.
void GetTutorialPlayerStats(unsigned i, uint32_t& lastFight, char& f08, char& wins) {
    PlayerInfo* p = GetPlayerInfoByGID(-2 - (int)i, 200);
    if (!p) return;
    lastFight = p->lastFight;
    f08 = (char)p->losses;
    wins = (char)p->wins;
}

// Only in the city; a shadow fought before counts as defeated.
void SetTutorialPlayerStats(unsigned i, uint32_t lastFight, char f08, char wins) {
    if (!GameState::IsPlayerCity()) return;
    PlayerInfo* p = GetPlayerInfoByGID(-2 - (int)i, 200);
    if (!p) return;
    p->losses = f08;
    p->wins = wins;
    p->lastFight = lastFight;
    if (lastFight != 0) p->defeated = 1;
}

void ResetTutorial() {
    g_tutorialLastRefresh = 0;
    g_tutorialCompleted = false;
}

}  // namespace PvP

namespace PlayerCompetitionWindow {
namespace {
int g_selectedGID = 0;            // 0x626368
bool g_selectionFetched = false;  // 0x62636c
uint32_t g_defeated = 0;          // 0x626370
bool g_returnWithoutWindow = false;
bool g_dataDownload = false;      // 0x626380 the opponent's data must be downloaded (online)
}  // namespace

int GetSelectedPlayerGID() { return g_selectedGID; }

// fetch: an opponent not in the list needs a download (online; the flag is never read offline).
void SetSelectedPlayerGID(int gid, bool fetch) {
    g_selectedGID = gid;
    if (fetch && !PvP::GetPlayerInfoByGID(gid)) g_dataDownload = true;
    g_selectionFetched = false;
}

void SetReturnWithoutWindow(bool on) { g_returnWithoutWindow = on; }
bool HasReturnWithoutWindow() { return g_returnWithoutWindow; }
void SetDefeatedPlayer(uint32_t spawn) { g_defeated = spawn; }
bool HasDefeatedPlayer(uint32_t spawn) { return g_defeated == spawn; }

}  // namespace PlayerCompetitionWindow
