#include "game/EntityData.h"

#include <pugixml.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

#include "engine/FileManager.h"
#include "engine/Render.h"
#include "engine/Resources.h"
#include "engine/Splitter.h"
#include "engine/Timer.h"
#include "game/GameState.h"
#include "game/MetaData.h"
#include "game/Rand48.h"
#include "game/StringTable.h"

namespace {

std::map<int, EntityData*> g_byId;    // EntityFactory+0x70: the map...
std::vector<EntityData*> g_ordered;   // ...and its +0x18 vector, in file order

// EntityFactory+0x00..+0x68: WORKER_MALE_NAMES, FARMER_MALE_NAMES, WORKER_MALE_SURNAMES,
// FARMER_MALE_SURNAMES, WORKER_FEMALE_NAMES, FARMER_FEMALE_NAMES, WORKER_FEMALE_SURNAMES,
// FARMER_FEMALE_SURNAMES, WORKER_IDLE.
std::vector<std::u32string> g_names[9];

// @0x16021c: "n" fills every level with n; "a[-b]:n,..." fills levels a..b with n, and the levels
// after the highest range get that range's value.
void ParseLevelScaleInfo(int16_t* out, unsigned count, const char* s) {
    if (!std::strchr(s, ':')) {
        for (unsigned i = 0; i < count; ++i) out[i] = (int16_t)std::atoi(s);
        return;
    }
    if (!s || !*s) return;
    unsigned top = 0, topValue = 0;
    do {
        unsigned from = (unsigned)std::atoi(s);
        while ((unsigned char)(*s - '0') < 10) ++s;
        unsigned to = from;
        if (*s == '-') to = (unsigned)std::atoi(++s);
        const char* colon = std::strchr(s, ':');
        unsigned value = (unsigned)std::atoi(colon + 1);
        s = std::strchr(colon + 1, ',');
        if (s) ++s;
        for (unsigned i = from; (int)i <= (int)to; ++i)
            if (i < count) out[i] = (int16_t)value;
        if ((int)top < (int)to) {
            topValue = value;
            top = to;
        }
    } while (s && *s);
    if (top != 0)
        for (unsigned i = top; (int)i < 0x28; ++i) out[i] = (int16_t)topValue;
}

}  // namespace

EntityData::~EntityData() { Destroy(); }

void EntityData::Destroy() {
    levelScale.reset();
    pool.reset();
    delete itemsDrop;
    for (MetaData*& m : starItems) delete m;
    delete bossMinionsLevels;
    delete bossMinions;
    delete particlePoints;
}

void EntityData::Clean() {
    name.clear();
    soundAttack.clear();
    soundDeath.clear();
    spawnTime = 0;
    id = 0;
    hp = 1;
    xp = 0;
    clas = finalClass = monsterType = vType = 0;
    offX = offY = boffX = boffY = slots = iconY = 0;
    enemyClass = 0;
    female = 0;
    soundCharType = 0;
    trainTime = 1;
    speedupCost = warriorCost = warriorCost2 = reviveCost = 0;
    hpRate = attackMelee = attackRanged = 0;
    attackMagic = defenseMelee = defenseRanged = defenseMagic = level = 0;
    critChance = initiative = 0;
    successModifier = 0.f;
    successRate = 0;
    speed = 0.f;
    frames = 0;
    offSpeed = 0.f;
    itemsDrop = nullptr;
    starItems[0] = starItems[1] = starItems[2] = nullptr;
    bossMinionsLevels = bossMinions = nullptr;
    side0 = nullptr;
    corpseDropId = corpseDropCount = 0;
    criticalFx = spellFx = 0;
    trainingObjType = trainingObjCount = 0;
    particlePoints = nullptr;
    reputationId = reputationLevel = 0;
}

Animation* EntityData::GetAnimation(const char* n) const {
    for (Animation* a : anims)
        if (a->name == n) return a;
    return nullptr;
}

bool EntityData::AnimationExists(const char* n) const { return GetAnimation(n) != nullptr; }

int EntityData::GetHpForLevel(int lvl) const { return levelScale ? levelScale->row[0][lvl] : hp; }
int EntityData::GetAttackMeleeForLevel(int lvl) const { return levelScale ? levelScale->row[1][lvl] : attackMelee; }
int EntityData::GetAttackRangedForLevel(int lvl) const { return levelScale ? levelScale->row[2][lvl] : attackRanged; }
int EntityData::GetAttackMagicForLevel(int lvl) const { return levelScale ? levelScale->row[3][lvl] : attackMagic; }
int EntityData::GetDefenseMeleeForLevel(int lvl) const { return levelScale ? levelScale->row[4][lvl] : defenseMelee; }
int EntityData::GetDefenseRangedForLevel(int lvl) const { return levelScale ? levelScale->row[5][lvl] : defenseRanged; }
int EntityData::GetDefenseMagicForLevel(int lvl) const { return levelScale ? levelScale->row[6][lvl] : defenseMagic; }

namespace EntityFactory {

EntityData* GetEntityByID(int id) {
    auto it = g_byId.find(id);
    return it != g_byId.end() ? it->second : nullptr;
}

EntityData* EnumEntity(unsigned i) { return i < g_ordered.size() ? g_ordered[i] : nullptr; }

int GetEntityIDByName(const std::string& n) {
    for (auto& [id, d] : g_byId)
        if (d->name == n) return id;
    return -1;
}

namespace {
std::vector<std::u32string>& NameList(int female, int farmer) { return g_names[(female ? 4 : 0) + (farmer ? 1 : 0)]; }
std::vector<std::u32string>& SurnameList(int female, int farmer) { return g_names[(female ? 6 : 2) + (farmer ? 1 : 0)]; }
}  // namespace

int GetRandomNameIdx(int female, int farmer) {
    return (int)((unsigned)Rand48::lrand48() % (unsigned)NameList(female, farmer).size());
}

int GetRandomSurnameIdx(int female, int farmer) {
    return (int)((unsigned)Rand48::lrand48() % (unsigned)SurnameList(female, farmer).size());
}

const char32_t* GetNameByIdx(int female, int farmer, int idx) {
    auto& v = NameList(female, farmer);
    if ((unsigned)idx >= v.size()) return v[(size_t)GetRandomNameIdx(female, farmer)].c_str();
    return v[(size_t)idx].c_str();
}

const char32_t* GetSurnameByIdx(int female, int farmer, int idx) {
    auto& v = SurnameList(female, farmer);
    if ((unsigned)idx >= v.size()) return v[(size_t)GetRandomSurnameIdx(female, farmer)].c_str();
    return v[(size_t)idx].c_str();
}

void Init() {
    static const char* const keys[9] = {
        "WORKER_MALE_NAMES",   "FARMER_MALE_NAMES",   "WORKER_MALE_SURNAMES",   "FARMER_MALE_SURNAMES",
        "WORKER_FEMALE_NAMES", "FARMER_FEMALE_NAMES", "WORKER_FEMALE_SURNAMES", "FARMER_FEMALE_SURNAMES",
        "WORKER_IDLE"};
    for (int k = 0; k < 9; ++k) {
        const char32_t* s = StringTable::GetString(keys[k]);
        std::u32string all = s ? s : U"";
        // WSplitter(s, L","): every part, empty ones included
        size_t start = 0;
        for (;;) {
            size_t comma = all.find(U',', start);
            g_names[k].push_back(all.substr(start, comma - start));
            if (comma == std::u32string::npos) break;
            start = comma + 1;
        }
    }
}

bool LoadData(const char* file) {
    double t0 = Timer::GetTime();
    uint32_t size = 0;
    uint8_t* buf = FileManager::LoadFile(file, size);
    if (!buf) {
        std::printf("EntityFactory::LoadData() Failed to load person data (File not found: %s)\n", file);
        return false;
    }
    pugi::xml_document doc;
    pugi::xml_parse_result r = doc.load_buffer_inplace(buf, size, 0x74);
    if (!r) {
        std::printf("EntityFactory::LoadData() Failed to parse XML file (%s)\n", r.description());
        FileManager::FreeFile(buf);
        return false;
    }
    if (!doc.child("XPersons")) {
        std::printf("EntityFactory::LoadData() Incorrect file format (%s)\n", file);
        FileManager::FreeFile(buf);
        return false;
    }
    for (pugi::xml_node x = doc.child("XPersons").child("XPerson"); x; x = x.next_sibling("XPerson")) {
        int id = (int)x.attribute("id").as_uint();
        EntityData* d = GetEntityByID(id);
        if (!d) {
            d = new EntityData;
            g_byId[id] = d;
            g_ordered.push_back(d);
        } else {
            d->Destroy();
            d->Clean();
        }
        d->id = (uint32_t)id;
        d->type = x.attribute("type").as_uint();
        d->parentId = x.attribute("parent_id").as_uint();
        d->name = x.attribute("name").value();
        d->title = StringTable::GetString(d->name.c_str());
        d->spawnTime = x.attribute("char_spawntime").as_uint();
        if (x.attribute("char_gender").as_uint() == 0) d->female = 1;
        d->soundCharType = x.attribute("sound_char_type").as_uint();
        d->trainTime = x.attribute("char_traintime").as_int();
        d->speedupCost = x.attribute("char_speedupcost").as_int();
        d->warriorCost = x.attribute("warrior_cost").as_int();
        d->warriorCost2 = x.attribute("warrior_cost2").as_int();
        d->reviveCost = x.attribute("char_revivecost").as_int();

        char tmp[256];
        std::snprintf(tmp, sizeof tmp, "%s", x.attribute("char_resourcescost").value());
        d->resourceCost = 0;
        d->resourceCostType = 2;
        if (char* eq = std::strchr(tmp, '=')) {
            *eq = 0;
            d->resourceCostType = GameState::StringToResourceType(tmp);
            d->resourceCost = std::atoi(eq + 1);
        }
        const char* ic = x.attribute("char_item_cost").value();
        d->itemCost = d->itemCostId = 0;
        if (const char* eq = std::strchr(ic, '=')) {
            d->itemCostId = std::atoi(ic);
            d->itemCost = std::atoi(eq + 1);
        }
        d->hp = x.attribute("hp").as_int();
        d->hpRate = x.attribute("hp_rate").as_int();
        d->critChance = x.attribute("char_critchance").as_int();
        d->initiative = x.attribute("initiative").as_int();
        d->attackMelee = x.attribute("attact_melee").as_int();
        d->attackRanged = x.attribute("attact_ranged").as_int();
        d->attackMagic = x.attribute("attack_magic").as_int();
        d->defenseMelee = x.attribute("defense_melee").as_int();
        d->defenseRanged = x.attribute("defense_ranged").as_int();
        d->defenseMagic = x.attribute("defense_magic").as_int();
        d->restoreTime = x.attribute("restore_time").as_int();
        d->soundAttack = x.attribute("sound_attack").value();
        d->soundDeath = x.attribute("sound_death").value();
        d->level = x.attribute("level").as_int();
        d->xp = x.attribute("xp").as_uint();

        if (std::strchr(x.attribute("hp").value(), ':')) {
            d->levelScale = std::make_unique<EntityData::LevelTable>();
            auto& t = d->levelScale->row;
            ParseLevelScaleInfo(t[0], 0x28, x.attribute("hp").value());
            ParseLevelScaleInfo(t[1], 0x28, x.attribute("attact_melee").value());
            ParseLevelScaleInfo(t[2], 0x28, x.attribute("attact_ranged").value());
            ParseLevelScaleInfo(t[4], 0x28, x.attribute("defense_melee").value());
            ParseLevelScaleInfo(t[5], 0x28, x.attribute("defense_ranged").value());
        }
        if (std::strchr(x.attribute("char_hp_pool").value(), ':')) {
            d->pool = std::make_unique<EntityData::LevelTable>();
            auto& t = d->pool->row;
            ParseLevelScaleInfo(t[0], 0x28, x.attribute("char_hp_pool").value());
            ParseLevelScaleInfo(t[1], 0x28, x.attribute("char_attack_melee_pool").value());
            ParseLevelScaleInfo(t[2], 0x28, x.attribute("char_attack_ranged_pool").value());
            ParseLevelScaleInfo(t[3], 0x28, x.attribute("char_defense_melee_pool").value());
            ParseLevelScaleInfo(t[4], 0x28, x.attribute("char_defense_ranged_pool").value());
            ParseLevelScaleInfo(t[5], 0x28, x.attribute("char_defense_magic_pool").value());
            ParseLevelScaleInfo(t[6], 0x28, x.attribute("char_critchance_pool").value());
        }
        d->monsterType = x.attribute("monster_type").as_int();
        d->enemyClass = x.attribute("enemy_class").as_int();

        pugi::xml_node xd = x.child("XData");
        d->clas = xd.attribute("clas").as_int();
        if ((unsigned)(id - 0xc) < 2) d->clas = 5;
        switch (id) {
        case 0x1f: case 0x22: case 0x23: case 0x24: case 0x25: case 0x31: case 0x161: case 0x1a2:
        case 0x195: case 0x1cf: case 0x198: case 0x197: case 0x208: case 0x20e: case 0x216:
            d->clas = 10;
            break;
        }
        if (id == 0x30) d->clas = 0;
        if (id == 0x133) d->clas = 0x10;
        if (id == 0x186) d->clas = 0x14;
        int fc = d->clas;
        if (d->type == 10) {
            if (fc == 8) fc = d->clas = 10;
        } else if (d->type == 9) {
            if (fc == 8) fc = d->clas = 9;
        } else if (d->type == 0x11 && fc == 9) {
            fc = d->clas = 10;
        }
        d->finalClass = fc;
        d->vType = xd.attribute("v_type").as_int();
        d->offX = xd.attribute("off_x").as_int();
        d->offY = xd.attribute("off_y").as_int();
        d->boffX = xd.attribute("boff_xx").as_int();
        d->boffY = xd.attribute("boff_y").as_int();
        d->slots = xd.attribute("slots").as_int();
        d->iconY = x.attribute("icon_y").as_int();

        pugi::xml_node xg = x.child("XGraphics");
        d->frames = xg.attribute("frames").as_int();
        d->speed = xg.attribute("speed").as_float();
        d->offSpeed = xg.attribute("off_speed").as_float();
        d->side0 = Resources::GetImage(xg.attribute("side0").value());
        // (the original also sets the side0 texture's +0x48 flag)

        std::vector<std::string> entries = SplitterParse(xg.attribute("anims").value(), ";");
        d->anims.reserve(d->anims.size() + entries.size());
        for (const std::string& e : entries) {
            Animation* a = new Animation;   // (a pool of 0x80 per block on the original)
            std::vector<std::string> f = SplitterParse(e.c_str(), "|");
            auto field = [&](size_t i) -> const char* { return i < f.size() ? f[i].c_str() : ""; };
            a->path = field(0);
            a->fps = (float)std::strtod(field(1), nullptr);
            a->loop = std::atoi(field(2)) == 1;
            if (*field(3)) {
                std::vector<std::string> o = SplitterParse(field(3), ",");
                auto v = [&](size_t i) { return i < o.size() ? std::atoi(o[i].c_str()) : 0; };
                a->offX = v(0);
                a->offY = v(1);
                if (o.size() == 2) {
                    a->off2X = v(0);
                    a->off2Y = v(1);
                } else if (o.size() >= 3) {
                    a->off2X = v(2);
                    a->off2Y = v(3);
                }
            }
            if (*field(4)) {
                std::vector<std::string> w = SplitterParse(field(4), ",");
                a->delay[0] = std::atoi(w[0].c_str()) / 2;
                a->delay[1] = (w.size() > 1 ? std::atoi(w[1].c_str()) : 0) / 2;
            }
            a->name = field(5);
            // (ids 0x285/0x286: a shadow glow filter set at +0x30, not ported)
            d->anims.push_back(a);
        }

        const char* p = x.attribute("items_drop").value();
        if (*p) d->itemsDrop = ParseCustomStyleData(p, ",=|", "110");
        for (int i = 0; i < 3; ++i) {
            std::snprintf(tmp, sizeof tmp, "star%d_items", i + 1);
            p = x.child("Farm").attribute(tmp).value();
            if (*p) d->starItems[i] = ParseCustomStyleData(p, ",=|", "110");
        }
        p = x.attribute("boss_minions").value();
        if (*p) {
            if (std::strchr(p, ':'))
                d->bossMinionsLevels = ParseCustomStyleData(p, ":", "1");
            else if (std::strchr(p, ','))
                d->bossMinions = ParseCustomStyleData(p, ",", "1");
            else
                d->bossMinions = ParseInteger(p);
        }
        d->unlockQuest = x.attribute("char_unlock_quest").as_uint();
        const char* tr = x.attribute("training_obj_type").value();
        d->trainingObjType = std::atoi(tr);
        if (const char* eq = std::strchr(tr, '=')) d->trainingObjCount = std::atoi(eq + 1);
        const char* cd = x.attribute("corpse_drop").value();
        d->corpseDropCount = d->corpseDropId = 0;
        if (const char* colon = std::strchr(cd, ':')) {
            d->corpseDropId = std::atoi(cd);
            d->corpseDropCount = std::atoi(colon + 1);
        }
        d->criticalFx = x.attribute("critical_fx").as_uint();
        d->spellFx = x.attribute("spell_fx").as_uint();
        if (d->trainingObjType != 0 && d->trainingObjCount != 0 && d->level > 1 && d->level < 5) {
            // SWPrintf(L"%s +%d", title, level - 1) into the wide string pool
            std::u32string t = d->title ? d->title : U"";
            t += U" +";
            for (char c : std::to_string(d->level - 1)) t += (char32_t)c;
            d->ownTitle = t;
            d->title = d->ownTitle.c_str();
        }
        p = x.attribute("particle_points").value();
        d->particlePoints = *p ? ParseCustomStyleData(p, "|;,", "111") : nullptr;
        d->reputationId = x.attribute("reputation_id").as_uint();
        d->reputationLevel = x.attribute("reputation_level").as_uint();
        d->successRate = x.attribute("success_rate").as_int();
        d->successModifier = x.attribute("success_modifier").as_float();
    }
    // Unnamed entries inherit their parent's name and title.
    for (auto& [id, d] : g_byId) {
        if (d && d->name.empty() && d->parentId != 0) {
            if (EntityData* parent = GetEntityByID((int)d->parentId)) {
                if (d != parent) d->name = parent->name;
                d->title = parent->title;
            }
        }
    }
    FileManager::FreeFile(buf);
    std::printf("EntityFactory::LoadData() Loaded person data %.3fms\n", (Timer::GetTime() - t0) * 1000.0);
    return true;
}

}  // namespace EntityFactory
