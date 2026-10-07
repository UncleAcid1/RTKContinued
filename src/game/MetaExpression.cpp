#include "game/MetaExpression.h"

#include <cstdio>

#include "game/Entity.h"
#include "game/EntityData.h"
#include "game/EntityManager.h"
#include "game/GameState.h"
#include "game/MetaData.h"
#include "game/SoldierSlots.h"
#include "game/Squad.h"

namespace {

enum Arg { kNone, kInt, kStr, kColon, kColonWrap, kWall, kWallWrap, kSemicolon, kSemicolonWrap };

struct Keyword {
    const char* name;
    bool prefix;   // ParseType's third argument: no '=' or ',' needed after the name
    int type;
    Arg arg;
};

// ParseSingle's keywords in the original's order (the first match wins). The second "CAM" (0x65)
// is never reached, as on the original.
const Keyword kKeywords[] = {
    {"REPLACE_OBJ", false, 0x35, kInt},
    {"REPLACE_AFTER_QUEST", false, 0x36, kColon},
    {"REPLACE_AFTER_SUB_QUEST", false, 0x37, kColon},
    {"REPLACE_AFTER_ITEM", false, 0x38, kColon},
    {"REPLACE_AFTER_INTERACT", false, 0x39, kColon},
    {"MESSAGE", false, 0x3a, kStr},
    {"INTERACT_MESSAGE", false, 0x3b, kStr},
    {"MSG_BEFORE_QUEST", false, 0x3c, kColon},
    {"MSG_AFTER_QUEST", false, 0x3d, kColon},
    {"DROP", false, 0x3e, kWallWrap},
    {"PROGRESS_MSG", false, 0x3f, kStr},
    {"REQ_UNLOCK_QUEST", false, 0x40, kInt},
    {"REQ_UNLOCK_SUB_QUEST", false, 0x41, kColonWrap},
    {"REQ_UNLOCK_ITEM", false, 0x42, kInt},
    {"KILL", false, 0x43, kColon},
    {"UNHIDE_QUEST", false, 0x44, kInt},
    {"UNHIDE_SUB_QUEST", false, 0x45, kColon},
    {"UNHIDE_ITEM", false, 0x46, kInt},
    {"UNHIDE_INTERACT", false, 0x47, kInt},
    {"HIDE_QUEST", false, 0x48, kInt},
    {"HIDE_SUB_QUEST", false, 0x49, kColon},
    {"HIDE_ITEM", false, 0x4a, kInt},
    {"HIDE_INTERACT", false, 0x4b, kInt},
    {"UNHIDE_AFTER_QUEST", false, 0x4c, kInt},
    {"UNHIDE_AFTER_SUB_QUEST", false, 0x4d, kColon},
    {"UNHIDE_AFTER_INTERACT", false, 0x4e, kInt},
    {"GIVE_ITEM", false, 0x4f, kInt},
    {"TP", false, 0x50, kColon},
    {"AP", false, 0x51, kWallWrap},
    {"BT", false, 0x52, kWallWrap},
    {"NO_FADE", false, 0x53, kInt},
    {"CAM", false, 0x54, kWall},
    {"TRANSFER_TO", false, 0x55, kInt},
    {"GOBLIN", false, 0x56, kStr},
    {"PORTAL", false, 0x57, kWallWrap},
    {"START_QUEST_TILE", false, 0x58, kWallWrap},
    {"END_QUEST_TILE", false, 0x59, kWallWrap},
    {"END_SUB_QUEST_TILE", false, 0x5a, kWallWrap},
    {"DISABLE_AFTER_QUEST", false, 0x5b, kInt},
    {"DISABLE_AFTER_SUB_QUEST", false, 0x5c, kColon},
    {"SOUND", false, 0x5e, kColonWrap},
    {"SOUND_PORTAL", false, 0x5f, kColonWrap},
    {"PARTICLE_IDLE", false, 0x60, kStr},
    {"PARTICLE_AFTER_INTERACT", false, 0x61, kColonWrap},
    {"PARTICLE_TRAP", false, 0x62, kColonWrap},
    {"PARTICLE_AFTER_QUEST", false, 0x63, kColonWrap},
    {"PARTICLE_AFTER_SUB_QUEST", false, 0x64, kColonWrap},
    {"CAM", false, 0x65, kWallWrap},
    {"CAM_AFTER_INTERACT", false, 0x66, kWallWrap},
    {"CAM_AFTER_QUEST", false, 0x67, kWallWrap},
    {"CAM_AFTER_SUB_QUEST", false, 0x68, kWallWrap},
    {"SHAKE_AFTER_INTERACT", false, 0x69, kColonWrap},
    {"SHAKE_AFTER_QUEST", false, 0x6a, kColonWrap},
    {"SHAKE_AFTER_SUB_QUEST", false, 0x6b, kColonWrap},
    {"INTERACT_ICON", true, 0x6c, kStr},
    {"INTERACT_ANIM", false, 0x6d, kColonWrap},
    {"CRAFT", true, 0x6e, kColonWrap},
    {"TRAP_DAMAGE", false, 0x6f, kColonWrap},
    {"TRAP_SOUND", false, 0x70, kColonWrap},
    {"TRAP_PATTERN", false, 0x71, kColonWrap},
    {"TRAP_ANIMATED", false, 0x72, kColonWrap},
    {"CHEST_DROP", false, 0x73, kColonWrap},
    {"CHEST_DROP_TYPE", false, 0x74, kInt},
    {"SPELL_AFTER_QUEST", false, 0x75, kColonWrap},
    {"SPELL_AFTER_SUB_QUEST", false, 0x76, kColonWrap},
    {"SPELL_AFTER_INTERACT", false, 0x77, kColonWrap},
    {"TELEPORT_TILE", false, 0x78, kWallWrap},
    {"SPELL_RESIST", false, 0x79, kWallWrap},
    {"HINT", false, 0x7a, kStr},
    {"GOTO_AFTER_QUEST", false, 0x7b, kWall},
    {"GOTO_AFTER_SUB_QUEST", false, 0x7c, kWall},
    {"HINT_DEAD", false, 0x7d, kStr},
    {"ANIM_FINISH_QUEST", false, 0x7e, kColon},
    {"ANIM_AFTER_QUEST", false, 0x7f, kColon},
    {"ANIM_AFTER_SUB_QUEST", false, 0x80, kColon},
    {"DIRECTION", false, 0x81, kInt},
    {"QUEST_FINISH_MSG", false, 0x82, kInt},
    {"ANIM_IDLE", false, 0x83, kSemicolon},
    {"PATROL", false, 0x84, kWallWrap},
    {"PATROL_TIMES", false, 0x85, kInt},
    {"BOSS", false, 0x86, kSemicolonWrap},
    {"DROP_ONCE", false, 0x87, kNone},
    {"GROUP", false, 0x88, kInt},
    {"START_QUEST", false, 0x89, kInt},
    {"END_QUEST", false, 0x8a, kInt},
    {"MAP_ID", false, 0x8b, kInt},
    {"GIVE_QUEST", false, 0x8c, kColon},
    {"FOG", false, 0x8d, kWall},
    {"DROP_AFTER_QUEST", false, 0x90, kWallWrap},
    {"DROP_BEFORE_QUEST", false, 0x91, kWallWrap},
    {"RANDOM", false, 0x92, kWallWrap},
    {"BOSS_TASK", false, 0x93, kInt},
    {"STATS_REPLACE_AFTER_QUEST", false, 0x94, kColon},
    {"ARENA_SPAWN", false, 0x96, kColon},
    {"MOB_SPAWN", false, 0x97, kColonWrap},
    {"PVP_SPAWN", false, 0x98, kColonWrap},
    {"UNHIDE_TAG", false, 0x99, kStr},
    {"UPGRADE", false, 0x9a, kInt},
    {"GROW", false, 0x9b, kStr},
    {"INITIATIVE", false, 0x5, kInt},
    {"HP_MAX", false, 0x6, kInt},
    {"HEALTH_PC", false, 0x7, kInt},
    {"MELEE_ATTACK", false, 0x8, kInt},
    {"MELEE_DEFENSE", false, 0x9, kInt},
    {"MELEE_DEFENCE", false, 0x9, kInt},
    {"MELEE_ABSORB", false, 0xa, kInt},
    {"MELEE_DAMAGE_PC", false, 0xb, kInt},
    {"RANGED_ATTACK", false, 0xc, kInt},
    {"RANGED_DEFENSE", false, 0xd, kInt},
    {"RANGED_DEFENCE", false, 0xd, kInt},
    {"RANGED_ABSORB", false, 0xe, kInt},
    {"RANGED_DAMAGE_PC", false, 0xf, kInt},
    {"MAGIC_ATTACK", false, 0x10, kInt},
    {"MAGIC_DEFENSE", false, 0x11, kInt},
    {"MAGIC_DEFENCE", false, 0x11, kInt},
    {"MAGIC_ABSORB", false, 0x12, kInt},
    {"MAGIC_DAMAGE_PC", false, 0x13, kInt},
    {"ALL_DEFENSE", false, 0x14, kInt},
    {"CRIT_CHANCE", false, 0x15, kInt},
    {"FURY_BONUS", false, 0x16, kInt},
    {"PARTY", false, 0x17, kNone},
    {"GOLD", false, 0x18, kInt},
    {"HP", false, 0x19, kInt},
    {"HP_REGEN", false, 0x22, kInt},
    {"DMG", false, 0x1a, kInt},
    {"ENERGY", false, 0x9c, kInt},
    {"MANA", false, 0x1b, kInt},
    {"ORB_ATTACK", false, 0x1c, kColonWrap},
    {"ORB_DEFENSE", false, 0x1d, kColonWrap},
    {"ORB_DEFENCE", false, 0x1d, kColonWrap},
    {"ORB_CRITICAL", false, 0x1e, kColonWrap},
    {"ORB_VAMPIRE", false, 0x1f, kColonWrap},
    {"ORB_PHOENIX", false, 0x20, kColonWrap},
    {"SPEED", false, 0x21, kInt},
    {"BELT_SLOTS", false, 0x23, kInt},
    {"BELT_SIZE", false, 0x24, kInt},
    {"GATHER_CHANCE", false, 0x25, kInt},
    {"GATHER_TIME", false, 0x26, kInt},
    {"GIVE_TROOPS", false, 0x27, kInt},
    {"LIMIT_QUANTITY", false, 0x28, kInt},
    {"LUCK", false, 0x29, kInt},
    {"SPEED_GOBLIN", false, 0x2a, kInt},
    {"SPEED_BUILD", false, 0x2b, kInt},
    {"SPEED_TIME", false, 0x2c, kInt},
    {"RESTORE_HP", false, 0x2d, kInt},
    {"BUFF_TIME", false, 0x2e, kInt},
    {"RANDOM_SPELL", false, 0x2f, kColon},
    {"FOUND_IN_PVP", false, 0x30, kInt},
    {"ADD_CUSTOM", false, 0x31, kWallWrap},
    {"HIDE_BIND", false, 0x32, kColonWrap},
    {"HIDE_CUSTOM", false, 0x33, kColonWrap},
    {"KEEP_HAIR", false, 0x34, kInt},
    {"BAT", false, 0x8e, kWall},
    {"BOSS_STR", false, 0x8f, kStr},
};

// bionic's ctype classes used here (_N 4, _S 8), ASCII only.
bool IsDigit(unsigned char c) { return c - '0' < 10u; }
bool IsSpace(unsigned char c) { return c == ' ' || (c >= '\t' && c <= '\r'); }

// PORT: ParseSingle repeats this call in each keyword's branch.
MetaData* ParseArg(const char*& p, Arg a) {
    switch (a) {
    case kInt: return ParseInteger(p);
    case kStr: return ParseString(p);
    case kColon: return ParseColonDividedList(p, false);
    case kColonWrap: return ParseColonDividedList(p, true);
    case kWall: return ParseWallDividedList(p, false);
    case kWallWrap: return ParseWallDividedList(p, true);
    case kSemicolon: return ParseSemicolonDividedList(p, false);
    case kSemicolonWrap: return ParseSemicolonDividedList(p, true);
    default: return nullptr;
    }
}

}  // namespace

MetaExpression::MetaExpression(const char* text, bool ownsText) : ownsText(ownsText), text(text) {
    Parse(text);
}

MetaExpression::MetaExpression(MetaExpression*, const char* text) : text(text) {
    Parse(text);
}

MetaExpression::~MetaExpression() {
    delete data;
    delete next;
    if (ownsText) delete[] text;
}

MetaExpression* MetaExpression::FindByType(int t) {
    for (MetaExpression* e = this; e; e = e->next)
        if (e->data && e->data->type == t) return e;
    return nullptr;
}

const MetaData* MetaExpression::GetDataWithType(int t) const {
    if (data && data->type == t) return data->GetChild(0);
    return nullptr;
}

const MetaData* MetaExpression::FindChildData(int t) {
    MetaExpression* e = FindByType(t);
    if (e && e->GetData() && e->GetData()->GetChildrenCount() != 0) return e->GetData()->GetChild(0);
    return nullptr;
}

bool MetaExpression::ParseSingle(const char*& p) {
    for (const Keyword& k : kKeywords) {
        if (!ParseType(p, k.name, k.prefix)) continue;
        data = new MetaData(k.type);
        if (k.arg == kNone) return true;
        MetaData* value = ParseArg(p, k.arg);
        if (k.type == kExpInteractIcon) {   // (no child when there is no name)
            if (value) data->AppendChild(value);
            return true;
        }
        data->AppendChild(value);
        if (k.type == kExpTp) {
            // "TP=a:b,<digits>": the number after the comma belongs to TP.
            if (*p == ',' && IsDigit((unsigned char)p[1])) {
                ++p;
                while (IsDigit((unsigned char)*p)) ++p;
            }
            return true;
        }
        // The orbs' value lists are checked (the node stays either way).
        int count = 0;
        const char* error = nullptr;
        switch (k.type) {
        case kExpOrbAttack:
            count = 2;
            error = "MetaExpression::Parse() Incorrect syntax for meta ORB_ATTACK (must be ORB_ATTACK=attack:charges)\n";
            break;
        case kExpOrbDefense:
            count = 2;
            error = "MetaExpression::Parse() Incorrect syntax for meta ORB_DEFENSE (must be ORB_DEFENSE=defense:charges)\n";
            break;
        case kExpOrbCritical:
            count = 2;
            error = "MetaExpression::Parse() Incorrect syntax for meta ORB_CRITICAL (must be ORB_CRITICAL=bonus:charges)\n";
            break;
        case kExpOrbVampire:
            count = 3;
            error = "MetaExpression::Parse() Incorrect syntax for meta ORB_VAMPIRE (must be ORB_VAMPIRE=hp_healed:attack:charges)\n";
            break;
        case kExpOrbPhoenix:
            count = 1;
            error = "MetaExpression::Parse() Incorrect syntax for meta ORB_PHOENIX (must be ORB_PHOENIX=1)\n";
            break;
        }
        if (error) {
            const MetaData* list = data->GetChild(0);
            if (list->type != MetaData::kList || list->GetChildrenCount() != (unsigned)count)
                std::fprintf(stderr, "%s", error);   // ErrorReporter::Printf
        }
        return true;
    }
    return false;
}

// A part that is no keyword (or a '#' comment) is skipped to the next comma; after a comma the next
// node parses the rest. Only spaces may follow the last part.
void MetaExpression::Parse(const char* p) {
    if (!ParseSingle(p)) {
        if (*p != '#') {
            std::fprintf(stderr, "ERROR: Incorrect meta expression type (stopped at '%s')\n", p);
            if (*p == ',' || *p == 0) goto parsed;
        }
        do ++p;
        while (*p != ',' && *p != 0);
    }
parsed:
    if (*p == ',') {
        ++p;
        if (*p == 0)
            std::fprintf(stderr, "Warning: MetaExpression::Parse() Expression ends with a trailing comma\n");
        else
            next = new MetaExpression(this, p);
    } else if (*p != 0) {
        while (IsSpace((unsigned char)*p)) {
            ++p;
            if (*p == 0) return;
        }
        std::fprintf(stderr, "ERROR MetaExpression::Parse() Metadata was not fully parsed (stopped at '%s')\n", p);
    }
}

namespace {
int Value(const MetaData* d) { return d->GetChild(0)->GetInt(); }   // PORT: GetChild(0)->GetInt()

// HP_MAX and HEALTH_PC change only the player or a hero entity (0xc, 0xd).
bool IsHero(Entity* e) {
    if (e->player) return true;
    EntityData* d = e->GetEntityData();
    return d && (d->id == 0xc || d->id == 0xd);
}

// (int)(0.5 + base * percent * 0.01)
int Percent(int base, const MetaData* d) { return (int)(0.5f + (float)(base * Value(d)) * 0.01f); }
}  // namespace

bool MetaExpression::CanApplyItemEffect(Entity* e) {
    if (!data) return true;
    switch (data->type) {
    case kExpHp:
    case kExpRestoreHp:
        return e->GetHP() != e->GetHpMax();
    case kExpMana:
        return e->GetAP() != e->GetApMax();
    case kExpOrbPhoenix: {
        bool hurt = e->GetHP() != e->GetHpMax();
        BaseSquad* s = e->GetSquad();
        if (!s) return hurt;
        for (unsigned i = 1; i < (unsigned)s->GetSoldierCount(); ++i) {
            Entity* m = s->GetSoldier(i);
            if (m->GetHP() != m->GetHpMax()) hurt = true;
        }
        return hurt;
    }
    case kExpGiveTroops:
        return SoldierPool::GetOccupiedSlotCount() < SoldierPool::GetTotalSlotsFree();
    }
    return true;
}

void MetaExpression::OnApplyItemEffect(Entity* e) {
    for (MetaExpression* x = this; x && x->data; x = x->next) {
        const MetaData* d = x->data;
        switch (d->type) {
        case kExpInitiative: e->AddInitiative(Value(d)); break;
        case kExpHpMax:
            if (IsHero(e)) e->AddHpMax(Value(d));
            break;
        case kExpMeleeAttack: e->AddAttackMelee(Value(d)); break;
        case kExpMeleeDefense: e->AddDefenseMelee(Value(d)); break;
        case kExpMeleeAbsorb: e->AddAbsorbMelee(Value(d)); break;
        case kExpRangedAttack: e->AddAttackRanged(Value(d)); break;
        case kExpRangedDefense: e->AddDefenseRanged(Value(d)); break;
        case kExpRangedAbsorb: e->AddAbsorbRanged(Value(d)); break;
        case kExpMagicAttack: e->AddAttackMagic(Value(d)); break;
        case kExpMagicDefense: e->AddDefenseMagic(Value(d)); break;
        case kExpMagicAbsorb: e->AddAbsorbMagic(Value(d)); break;
        case kExpAllDefense:
            e->AddDefenseMelee(Value(d));
            e->AddDefenseRanged(Value(d));
            e->AddDefenseMagic(Value(d));
            break;
        case kExpCritChance: e->AddCritChance(Value(d)); break;
        case kExpFuryBonus: e->AddFuryBonus(Value(d)); break;
        case kExpParty: {
            // The rest goes to the squad's soldiers, and the parsing of this entity ends.
            BaseSquad* s = e->GetSquad();
            if (!s) return;
            for (unsigned i = 1; i < (unsigned)e->GetSquad()->GetSoldierCount(); ++i) {
                if (!x->next) break;
                if (Entity* m = e->GetSquad()->GetSoldier(i)) x->next->OnApplyItemEffect(m);
            }
            return;
        }
        case kExpGold:
            std::puts("MetaExpression::OnApplyItemEffect() ITEM_ADD_GOLD_DROP is not implemented");
            break;
        case kExpHp: e->AddHP(Value(d)); break;
        case kExpDmg:
            std::puts("MetaExpression::OnApplyItemEffect() ITEM_ADD_DAMAGE_BOOST is not implemented");
            break;
        case kExpMana: e->AddAP(Value(d)); break;
        case kExpOrbPhoenix: {
            Entity* player = EntityManager::GetPlayer();
            if (!player || !player->GetSquad()) break;
            player->GetSquad()->HealSquad();
            if (GameState::IsTaskStarted(0x2f9)) {
                // UNVERIFIED (milestone 4c): Tasks::CompleteSubtask(0x27, 1, 1).
            }
            GameState::UpdatePlayerRegenerationState();
            break;
        }
        case kExpHpRegen: e->SetHPRegeneration(Value(d)); break;
        case kExpBeltSlots: GameState::SetBeltSlotCount((uint32_t)Value(d)); break;
        case kExpBeltSize: GameState::SetBeltSize((uint32_t)Value(d)); break;
        case kExpGiveTroops:
            // UNVERIFIED (milestone 5, hiring): EntityManager::CreateEntity(id, false, true) and
            // GameState::HireSoldier(entity, true, 2).
            break;
        case kExpLuck:
            if (e->GetLuck() < Value(d)) e->SetLuck(Value(d));
            break;
        case kExpRestoreHp: e->AddHP(e->GetHpMax() - e->GetHP()); break;
        }
    }
}

void MetaExpression::OnApplyItemBuffEffect(Entity* e) {
    for (MetaExpression* x = this; x && x->data; x = x->next) {
        const MetaData* d = x->data;
        switch (d->type) {
        case kExpHealthPc: {
            if (!IsHero(e)) break;
            bool full = e->GetHP() == e->GetHpMax();
            e->AddHpMax(Percent(e->GetHpMax(), d));
            if (full) e->AddHP(Percent(e->GetHpMax(), d));
            break;
        }
        case kExpMeleeDamagePc: e->AddAttackMelee(Percent(e->GetAttackMelee(), d)); break;
        case kExpRangedDamagePc: e->AddAttackRanged(Percent(e->GetAttackRanged(), d)); break;
        case kExpMagicDamagePc: e->AddAttackMagic(Percent(e->GetAttackMagic(), d)); break;
        }
    }
}
