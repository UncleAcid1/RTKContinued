// Professions: the professions and reputations (professions.xml, <Reputation>). A profession
// (herbalist, miner, fisherman, tamer ...) gathers items from objects on the mission maps with a
// tool; a reputation (the arena's Bloodletters and Shields of the Empire, PvP valor ...) only
// counts points. Both have seven levels, each reached at a number of points (GameState's
// profession points). Port of Professions (libkingdom.so 5.11, @0x1e7fc0..0x1ea1e0; statics
// 0x613ed4..0x613f30).
#pragma once
#include <cstdint>
#include <vector>

class MetaData;
namespace Items { struct ItemInfo; }
namespace GameState { struct PlayerItem; }
namespace Render { struct Texture; }

namespace Professions {

enum ProfessionType { kProfession = 0, kReputation = 1 };

struct ProfessionLevel {                   // 0x18 bytes, <Levels><Item>
    uint32_t id = 0;                       // +0x00 "id"
    uint32_t profession = 0;               // +0x04 the profession's id
    const char32_t* name = nullptr;        // +0x08 StringTable "name" (the rank)
    uint32_t level = 0;                    // +0x0c "level" 1..7
    uint32_t successRate = 0;              // +0x10 "success_rate" percent
    uint32_t points = 0;                   // +0x14 "points" needed
};

struct Profession {                        // 0x5c bytes, <Reputation>
    uint32_t id = 0;                       // +0x00 "id"
    const char* name = nullptr;            // +0x04 "name" (the StringTable key)
    const char32_t* title = nullptr;       // +0x08 the name's text
    const char32_t* title60 = nullptr;     // +0x0c "<name>_60"
    const char32_t* desc = nullptr;        // +0x10 "<name>_desc"
    const char32_t* action = nullptr;      // +0x14 "<name>_action"
    Render::Texture* smallIcon = nullptr;  // +0x18 "icon", or the 16 px icon of profession 1, 2, 6, 7, 8
    Render::Texture* icon = nullptr;       // +0x1c "icon"
    Render::Texture* iconLarge = nullptr;  // +0x20 "icon_large"
    ProfessionType type = kProfession;     // +0x24 not "is_profession"
    uint32_t unlockLevel = 0;              // +0x28 "unlock_level"
    MetaData* unlockQuests = nullptr;      // +0x2c "unlock_quest" (any of them completed)
    uint32_t pointItem = 0;                // +0x30 "item_id": the item that gives points
    std::vector<uint32_t> objects;         // +0x34 "object_id": the map object per tileset
    uint32_t f44 = 0;                      // +0x44
    uint32_t tab = 0;                      // +0x48 "tab_id"
    std::vector<ProfessionLevel*> levels;  // +0x4c sorted by level

    bool IsUnlocked() const;               // @0x1e8e20
};

bool Init(const char* file);               // @0x1e9448
void Deinit();                             // @0x1ea098
bool ProfessionLevelSort(const ProfessionLevel* a, const ProfessionLevel* b);   // @0x1e7fc0
Profession* GetProfession(uint32_t id);    // @0x1e7fd8
Profession* GetProfession(const char* name);   // @0x1e8dbc
Profession* EnumProfession(unsigned i);    // @0x1e803c
Profession* GetProfessionForObject(uint32_t object);   // @0x1e8060
Profession* GetProfessionForTab(uint32_t tab, ProfessionType type);   // @0x1e80e0
Profession* GetProfessionForPointItem(uint32_t item);  // @0x1e8144
ProfessionLevel* GetProfessionLevel(uint32_t profession, uint32_t level);   // @0x1e81a8
uint32_t GetProfessionLevelForPoints(uint32_t profession, uint32_t points); // @0x1e8208
bool IsProfessionObject(uint32_t object);  // @0x1e8268
// @0x1e8514 the profession's tool item id: owned (the chosen one, else the best unbroken one; item
// receives it), else the best (best) or the worst of all items.
uint32_t GetProfessionTool(uint32_t profession, bool owned, GameState::PlayerItem** item, bool best);
bool IsProfessionTool(uint32_t item);      // @0x1e8818
uint32_t GetPlayerProfessionLevel(uint32_t profession);   // @0x1e8850
void AddProfessionPoint(uint32_t profession, uint32_t points);   // @0x1e886c
int GetItemProbability(uint32_t profession, uint32_t item);      // @0x1e88c0 percent
uint32_t GenerateItem(uint32_t profession);                      // @0x1e89cc an item id, or 0
uint32_t GenerateProfessionObject(ProfessionType type);          // @0x1e8e8c an object id, or 0

}  // namespace Professions
