// EntityData: one character type from persons.xml (workers, the hero, soldiers, monsters, farm
// animals, storage piles), and EntityFactory, which loads and owns them.
// Port of EntityData (@0x15eaa4..0x15f5e8), EntityFactory::LoadData @0x162828, EntityFactory::Init
// @0x161a1c (the name lists), GetEntityByID @0x15f7b4, EnumEntity @0x1608a8,
// GetEntityIDByName @0x1605c8.
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "game/Animation.h"

namespace Render { struct Texture; }
class MetaData;

namespace Tavern { struct TavernInfo; }

struct EntityData {              // 0x160 bytes
    // Per-level values (ParseLevelScaleInfo), 40 levels per row.
    struct LevelTable { int16_t row[7][40] = {}; };   // 0x230 bytes

    uint32_t id = 0;             // +0x00
    unsigned type = 0;           // +0x04 "type"
    unsigned parentId = 0;       // +0x08 "parent_id"
    std::string name;            // +0x0c "name"
    const char32_t* title = nullptr;  // +0x24 StringTable name
    int enemyClass = 0;          // +0x28 "enemy_class"
    std::string soundAttack;     // +0x2c "sound_attack" (GetSoundAttack @0x15ed9c)
    std::string soundDeath;      // +0x44 "sound_death" (GetSoundDeath @0x15ed84)
    unsigned spawnTime = 0;      // +0x5c "char_spawntime"
    int trainTime = 1;           // +0x60 "char_traintime"
    int speedupCost = 0;         // +0x64 "char_speedupcost"
    int warriorCost = 0;         // +0x68 "warrior_cost"
    int warriorCost2 = 0;        // +0x6c "warrior_cost2"
    int reviveCost = 0;          // +0x70 "char_revivecost"
    int resourceCostType = 2;    // +0x74 "char_resourcescost" TYPE=n (no '=': 2, n = 0)
    int resourceCost = 0;        // +0x78
    int itemCostId = 0, itemCost = 0;  // +0x7c +0x80 "char_item_cost" id=n
    int hp = 1;                  // +0x84 "hp"
    int hpRate = 0;              // +0x88 "hp_rate"
    int attackMelee = 0;         // +0x8c "attact_melee"
    int attackRanged = 0;        // +0x90 "attact_ranged"
    int critChance = 0;          // +0x94 "char_critchance"
    int initiative = 0;          // +0x98 "initiative"
    int attackMagic = 0;         // +0x9c "attack_magic"
    int defenseMelee = 0;        // +0xa0 "defense_melee"
    int defenseRanged = 0;       // +0xa4 "defense_ranged"
    int defenseMagic = 0;        // +0xa8 "defense_magic"
    int level = 0;               // +0xac "level"
    int restoreTime = 0;         // +0xb0 "restore_time"
    int corpseDropId = 0, corpseDropCount = 0;  // +0xb4 +0xb8 "corpse_drop" a:b
    // +0xbc rows hp, melee, ranged, magic, def melee, def ranged, def magic (when "hp" has a ':';
    // the magic rows stay 0)
    std::unique_ptr<LevelTable> levelScale;
    // +0xc0 the *_pool attributes: hp, melee, ranged, def melee, def ranged, def magic, crit
    std::unique_ptr<LevelTable> pool;
    int female = 0;              // +0xc4 1 when "char_gender" is 0
    unsigned soundCharType = 0;  // +0xc8 "sound_char_type"
    unsigned xp = 0;             // +0xcc "xp"
    int clas = 0;                // +0xd0 XData "clas", with the per-id overrides
    int finalClass = 0;          // +0xd4
    int monsterType = 0;         // +0xd8 "monster_type"
    int vType = 0;               // +0xdc XData "v_type"
    int offX = 0, offY = 0;      // +0xe0 +0xe4 XData "off_x", "off_y"
    int boffX = 0, boffY = 0;    // +0xe8 +0xec XData "boff_xx", "boff_y"
    int slots = 0;               // +0xf0 XData "slots"
    int iconY = 0;               // +0xf4 "icon_y"
    int frames = 0;              // +0xf8 XGraphics "frames"
    float speed = 0.f;           // +0xfc XGraphics "speed"
    float offSpeed = 0.f;        // +0x100 XGraphics "off_speed"
    MetaData* itemsDrop = nullptr;    // +0x104 "items_drop" (",=|", "110")
    MetaData* starItems[3] = {};      // +0x108 Farm "star1_items".."star3_items"
    MetaData* bossMinionsLevels = nullptr;  // +0x114 "boss_minions" with ':' (":", "1")
    MetaData* bossMinions = nullptr;  // +0x118 "boss_minions" n or a,b,...
    Render::Texture* side0 = nullptr; // +0x11c XGraphics "side0"
    std::vector<Animation*> anims;    // +0x120 XGraphics "anims"
    unsigned unlockQuest = 0;    // +0x12c "char_unlock_quest"
    int trainingObjType = 0, trainingObjCount = 0;  // +0x130 +0x134 "training_obj_type" a=b
    unsigned criticalFx = 0;     // +0x138 "critical_fx"
    unsigned spellFx = 0;        // +0x13c "spell_fx"
    MetaData* particlePoints = nullptr;  // +0x14c "particle_points" ("|;,", "111")
    unsigned reputationId = 0;   // +0x150 "reputation_id"
    unsigned reputationLevel = 0;  // +0x154 "reputation_level"
    int successRate = 0;         // +0x158 "success_rate"
    float successModifier = 0.f; // +0x15c "success_modifier"

    std::u32string ownTitle;     // PORT: storage for a title built by LoadData ("%s +%d")
    // PORT: a PartialClone shares its original's itemsDrop; the original's destructor would free
    // it twice, so a clone leaves it alone.
    bool sharesItemsDrop = false;

    EntityData() = default;
    ~EntityData();               // @0x15f5e8
    // @0x15f198: a member copy. (The original copies the level tables' pointers too; its only
    // caller, PartialClone, clears them right after: the clone keeps none.)
    EntityData& operator=(const EntityData& o);
    // @0x15f594: a copy for one entity (Entity::SetCustomEntityData) without the level tables, the
    // star items, the boss minions and the particle points.
    EntityData* PartialClone() const;
    void Destroy();              // @0x15edb4 frees the tables and MetaData
    void Clean();                // @0x15ee94 back to the defaults above
    Animation* GetAnimation(const char* name) const;   // @0x15f13c
    bool AnimationExists(const char* name) const;      // @0x15f0e4
    // The *ForLevel getters read the level table when "hp" had per-level values, else the plain value.
    // @0x15ebf0: "gold_16_lightning" (spell_fx 2), "gold_16_ice" (3), else "gold_16_mages".
    const char* GetSmallSpellIcon() const {
        return spellFx == 2 ? "gold_16_lightning" : spellFx == 3 ? "gold_16_ice" : "gold_16_mages";
    }
    int GetHpForLevel(int level) const;                // @0x15eb3c
    int GetAttackMeleeForLevel(int level) const;       // @0x15eb54
    int GetAttackRangedForLevel(int level) const;      // @0x15eb6c
    int GetAttackMagicForLevel(int level) const;       // @0x15eb84 (row 3 is never filled)
    int GetDefenseMeleeForLevel(int level) const;      // @0x15eb9c
    int GetDefenseRangedForLevel(int level) const;     // @0x15ebb8
    int GetDefenseMagicForLevel(int level) const;      // @0x15ebd4 (row 6 is never filled)
    // The arena waves' stats per player level (the *_pool rows; 0 without them).
    int GetWavesHpForLevel(int level) const;           // @0x15eaa4
    int GetWavesAttackMeleeForLevel(int level) const;  // @0x15eab8
    int GetWavesAttackRangedForLevel(int level) const; // @0x15eacc
    int GetWavesDefenseMeleeForLevel(int level) const; // @0x15eae0
    int GetWavesDefenseRangedForLevel(int level) const;   // @0x15eaf4
    int GetWavesDefenseMagicForLevel(int level) const; // @0x15eb0c
    int GetWavesCritChanceForLevel(int level) const;   // @0x15eb24
    // @0x15ec34: 100, or 100 - the sale's percentage (+0x140) while the server's transaction time
    // is inside the sale (+0x144..+0x148). Online only (dormant): no sale is ever set offline.
    int GetDiscountSize() const { return 100; }
    // @0x15ecfc / @0x15ec74: the hiring price in gold ("warrior_cost") / crystals
    // ("warrior_cost2"); unless `full`, with the sale taken off (price * discount / 100).
    // A tavern offer's own prices (TavernInfo +0x24 / +0x28, when not 0) replace the unit's.
    unsigned GetCostGold(bool full, const Tavern::TavernInfo* tavern) const;
    unsigned GetCostCrystals(bool full, const Tavern::TavernInfo* tavern) const;
};

namespace EntityFactory {

bool LoadData(const char* file);                  // @0x162828
void Init();                                      // @0x161a1c InitNames: the worker/farmer name lists (Init @0x162820 only branches to it)
EntityData* GetEntityByID(int id);                // @0x15f7b4
void GetEntitiesForBuilding(unsigned building, unsigned* out, unsigned count);   // @0x160680
EntityData* GetTamedEntityForID(uint32_t id);    // @0x160720
// @0x1610fc: a monster (type 0x10) of the arena's kind (monster_type == arena), by id order: the
// pick-th % count one, a random one for pick 0; none when there is none.
EntityData* GetEntityForArena(unsigned arena, unsigned pick);
EntityData* EnumEntity(unsigned i);               // @0x1608a8 (file order)
int GetEntityIDByName(const std::string& name);   // @0x1605c8 (-1 if none)
// The worker/farmer name lists (female: EntityData::female, farmer: 1 for farmers). An index out of
// range picks a random one.
int GetRandomNameIdx(int female, int farmer);     // @0x1615f4
int GetRandomSurnameIdx(int female, int farmer);  // @0x16174c
const char32_t* GetNameByIdx(int female, int farmer, int idx);      // @0x161694 (EntityManager's @0x1655f8 forwards here)
const char32_t* GetSurnameByIdx(int female, int farmer, int idx);   // @0x1617ec (EntityManager's @0x1655f0 forwards here)

}  // namespace EntityFactory
