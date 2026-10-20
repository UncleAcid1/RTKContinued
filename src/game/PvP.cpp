#include "game/PvP.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <string>

#include "engine/FileManager.h"
#include "engine/Timer.h"
#include "game/EntityData.h"
#include "game/GameState.h"
#include "game/Professions.h"
#include "game/Rand48.h"
#include "game/SaveManager.h"
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

struct CachedData {                   // Cache<PvP::SaveCache>::CachedData
    SaveCache cache;                  // +0x00
    uint32_t time = 0;                // +0x20
    bool valid = false;               // +0x24
};
std::map<std::string, CachedData> g_cache;   // 0x613fb0

std::string CachePath(const std::string& key) { return FileManager::GetStoragePath() + key; }

std::string CacheKey(int type) {
    char key[0x40] = {};
    std::snprintf(key, sizeof key, "pc%d", type);
    return key;
}

char32_t* Copy(const char32_t* s) {
    size_t n = 0;
    while (s[n]) ++n;
    char32_t* out = new char32_t[n + 1];
    std::memcpy(out, s, (n + 1) * sizeof(char32_t));
    return out;
}

// A u32 count, then the characters as u32s (GameState's SaveString).
char32_t* LoadString(SaveManager::Reader& r) {
    uint32_t n = r.u32();
    char32_t* out = new char32_t[n + 1];
    for (uint32_t i = 0; i < n; ++i) out[i] = (char32_t)r.u32();
    out[n] = 0;
    return out;
}

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


// The original reads the opponent's chunks in any order; others are skipped.
bool LoadPlayerInfo(const PvPSave& save, int type, const char32_t* name) {
    PlayerInfo* p = Add();
    p->gid = save.gid;
    p->type = type;
    p->lastFight = save.lastFight;
    // A shadow beaten in the last five hours stays beaten.
    if (g_tutorialCompleted && type == 1 && save.lastFight + 18000u > (uint32_t)Timer::GetGlobalTime())
        p->defeated = type;
    if (name) p->str414 = Copy(name);
    SaveManager::SaveBlock block;
    block.mapId = 0xffffffff;
    block.data = save.data;
    block.ParseDataIntoChunks();
    for (;;) {
        const uint32_t id = block.NextChunkID();
        if (id == 0x13) return true;
        if (id != 0x10 && id != 0x41 && id != 0x43 && id != 0x4f) {
            block.SkipChunk(false);
            continue;
        }
        SaveManager::Reader r(block.GetChunk());
        if (id == 0x10) {
            p->male = r.u32() != 0;
            char32_t* own = LoadString(r);
            if (!name) {
                p->name = own;
            } else {
                p->name = Copy(name);
                delete[] p->str414;
                p->str414 = own;
            }
            p->str410 = LoadString(r);
        } else if (id == 0x41) {   // the profession points: only profession 7's level is kept
            const uint32_t n = r.u32();
            for (uint32_t i = 0; i < n; ++i) {
                const uint32_t prof = r.u32();
                const uint32_t points = r.u32();
                if (prof == 7) p->f10 = (int)Professions::GetProfessionLevelForPoints(prof, points);
            }
        } else if (id == 0x43) {
            p->f418 = (int)r.u32();
        } else {   // 0x4f
            p->level = (int)r.u32();
            // The gear: at most ten bindings are read (the original then reads on from there).
            const uint32_t n = r.u32();
            for (uint32_t i = 0; i < n && i < 10; ++i) {
                PlayerEquipmentInfo::ItemInfo& it = p->equipment.items[i];
                it.uniqueId = r.u32();
                if (it.uniqueId == 0) continue;
                it.id = r.u32();
                it.f08 = r.u32();
                it.f0c = r.u32();
            }
            const uint32_t belt = r.u32();
            for (uint32_t i = 0; i < belt; ++i)
                if (r.u32() != 0) r.u32();
            // The army: the warriors (not the hero, class 5) marked in the save.
            const uint32_t army = r.u32();
            for (uint32_t i = 0; i < army; ++i) {
                const int16_t eid = r.s16();
                const EntityData* d = EntityFactory::GetEntityByID(eid);
                const int16_t hp = r.s16();
                const uint8_t flag = r.u8();
                r.s16();
                const int16_t hpMax = r.s16();
                // PORT: d null-checked, and no more than ten warriors (the original checks neither).
                if (!d || d->clas == 5 || !flag || p->warriorCount >= 10) continue;
                PlayerInfo::WarriorInfo& w = p->warriors[p->warriorCount++];
                w.id = (uint32_t)eid;
                w.f08 = hp;
                w.hp = hpMax;
                w.f04 = true;
            }
            // His own victories and defeats are read, then the record against the player replaces them.
            p->losses = (int)r.u32();
            p->wins = (int)r.u32();
            p->wins = save.wins;
            p->losses = save.losses;
            const uint32_t customs = r.u32();
            if (customs < 0x400 && r.HasData((size_t)customs * 4))
                for (uint32_t i = 0; i < customs; ++i) {
                    const uint32_t v = r.u32();
                    if (i < 32) p->equipment.customs[i].id = v;
                }
        }
        block.EndChunkLoading(r);
    }
}

int GetRandomPlayerGID(int type, bool) {
    std::vector<PlayerInfo*> pool;
    for (unsigned i = 0; PlayerInfo* p = EnumPlayers(i); ++i)
        if (p->type == type && p->defeated != 1) pool.push_back(p);
    if (pool.empty()) return 0;
    return pool[(size_t)((unsigned long)Rand48::lrand48() % pool.size())]->gid;
}

void ClearAllCache() {
    Cache::Remove("pc1");
    Cache::Remove("pc2");
    Cache::Remove("pc3");
    Cache::Remove("pc4");
}

void RemoveCachedData(int type, bool hard) {
    const std::string key = CacheKey(type);
    if (hard) Cache::Remove(key);
    else g_cache[key].valid = false;
}

// The records follow EnumPlayers' order of that type; more players than the list had are reported.
void UpdateCachedData(int type) {
    const std::string key = CacheKey(type);
    SaveCache c;
    if (!Cache::Get(key, c) || !Cache::IsValid(key)) return;
    c.lastFight.assign(c.count, 0);
    c.losses.assign(c.count, 0);
    c.wins.assign(c.count, 0);
    c.defeated.assign(c.count, 0);
    uint32_t n = 0;
    for (unsigned i = 0; PlayerInfo* p = EnumPlayers(i); ++i) {
        if (p->type != type) continue;
        if (n >= c.count) {
            ++n;
            std::fprintf(stderr, "ERROR: PvP::UpdateCachedData() Type %d, PvP::EnumPlayers returned entry %d, "
                         "but there is only %d entries in the array\n", type, n, c.count + 1);   // ErrorReporter::Printf
            continue;
        }
        c.lastFight[n] = p->lastFight;
        c.losses[n] = (uint32_t)p->losses;
        c.wins[n] = (uint32_t)p->wins;
        c.defeated[n] = (uint32_t)p->defeated;
        ++n;
    }
    Cache::Set(key, c, c.time);
    Cache::Save(key);
}

namespace Cache {

void Set(const std::string& key, const SaveCache& c, uint32_t time) { g_cache[key] = CachedData{c, time, true}; }

// The file: the text's length and the time, the text, the count, then the four arrays (native
// 32-bit words). Always true, even when the file cannot be written.
bool Save(const std::string& key) {
    FILE* f = std::fopen(CachePath(key).c_str(), "wb");
    if (!f) return true;
    const SaveCache& c = g_cache[key].cache;
    const uint32_t size = (uint32_t)c.data.size();
    std::fwrite(&size, 1, 4, f);
    std::fwrite(&c.time, 1, 4, f);
    std::fwrite(c.data.data(), 1, size, f);
    std::fwrite(&c.count, 1, 4, f);
    for (const std::vector<uint32_t>* a : {&c.lastFight, &c.losses, &c.wins, &c.defeated})
        std::fwrite(a->data(), 1, (size_t)c.count * 4, f);
    std::fclose(f);
    return true;
}

void Free(const std::string& key) { g_cache.erase(key); }

// A list read from its file is cached (time 0).
bool Get(const std::string& key, SaveCache& out) {
    auto it = g_cache.find(key);
    if (it != g_cache.end()) {
        out = it->second.cache;
        return true;
    }
    FILE* f = std::fopen(CachePath(key).c_str(), "rb");
    if (!f) return false;
    SaveCache c;
    uint32_t size = 0;
    std::fread(&size, 1, 4, f);
    std::fread(&c.time, 1, 4, f);
    c.data.assign(size, '\0');
    std::fread(&c.data[0], 1, size, f);
    std::fread(&c.count, 1, 4, f);
    for (std::vector<uint32_t>* a : {&c.lastFight, &c.losses, &c.wins, &c.defeated}) {
        a->assign(c.count, 0);   // PORT: a short file leaves zeros
        std::fread(a->data(), 1, (size_t)c.count * 4, f);
    }
    std::fclose(f);
    Set(key, c, 0);
    out = c;
    return true;
}

bool IsValid(const std::string& key) { return g_cache[key].valid; }

void Remove(const std::string& key) {
    Free(key);
    std::remove(CachePath(key).c_str());
}

}  // namespace Cache

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
