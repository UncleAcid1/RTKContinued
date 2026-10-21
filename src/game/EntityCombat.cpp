// Entity: the fight side. Damage (OnHit, WillDie), blocks, dying and death (drops, gold and
// experience, quest steps, the dead hero's broken gear), the belt orbs' bonuses, aggro.
// Port of the combat functions of Entity (libkingdom.so 5.11, @0x14ffb4..0x15e7ac).
#include <cstdio>

#include "engine/SoundsManager.h"
#include "engine/IconManager.h"
#include "engine/Render.h"
#include "engine/Timer.h"
#include "game/AI.h"
#include "game/AIState.h"
#include "game/Animation.h"
#include "game/BuildingHovers.h"
#include "game/Combat.h"
#include "game/Entity.h"
#include "game/EntityData.h"
#include "game/EntityManager.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/Locations.h"
#include "game/Map.h"
#include "game/MetaData.h"
#include "game/MetaExpression.h"
#include "game/Projectile.h"
#include "game/PvP.h"
#include "game/Rand48.h"
#include "game/Setting.h"
#include "game/SpawnPoint.h"
#include "game/Spells.h"
#include "game/Squad.h"
#include "game/StringTable.h"
#include "game/Tasks.h"
#include "gui/GUI.h"
#include "hud/HUD.h"
#include "windows/Windows.h"

bool Entity::g_deathAnimation = false;     // 0x6119ac a boss is dying: the game waits for it
Entity* Entity::g_dyingBoss = nullptr;     // 0x6119d0
float Entity::g_dyingBossTime = 0.f;       // 0x6119d4

namespace {

// The attacker's damage before bonuses: its magic attack if it has one, else ranged, else melee.
int BaseDamage(const Entity* attacker) {
    int d = attacker->GetAttackMelee();
    if (attacker->GetAttackRanged() > 0) d = attacker->GetAttackRanged();
    if (attacker->GetAttackMagic() > 0) d = attacker->GetAttackMagic();
    return d;
}

// e's absorption against the attacker's kind of attack (0 for an attacker without any attack).
int Absorb(const Entity* e, const Entity* attacker) {
    if (attacker->GetAttackMagic() > 0) return e->GetAbsorbMagic();
    if (attacker->GetAttackRanged() > 0) return e->GetAbsorbRanged();
    if (attacker->GetAttackMelee() > 0) return e->GetAbsorbMelee();
    return 0;
}

// A drop of `count` items: on the ground next to e, or (collect: boss and arena fights) straight
// into the map's collection.
void Drop(Entity* e, uint32_t item, bool collect) {
    if (collect) GameState::AddMapItemCollectionInfo(GameState::GetCurrentMapID(), item, 1);
    else BuildingHovers::DropItem(e->worldX, e->worldY, Items::GetItemInfo(item), 0, 1);
}

int Int(const MetaData* d, unsigned i) { return d->GetChild(i)->GetInt(); }

// One line of "items_drop" / "starN_items": item=chance:count[:every]. With `every` the item drops
// on every every-th kill of this monster instead of by chance (chance out of `scale`).
void DropByChance(Entity* e, uint32_t item, int chance, int scale, int count, const MetaData* params, bool collect) {
    int every = params->GetChildrenCount() < 3 ? 0 : Int(params, 2);
    if (every == 0) {
        for (int i = 0; i < count; ++i)
            if ((int)(Entity::NextSetupRandom() % (unsigned)scale) < chance) Drop(e, item, collect);
        return;
    }
    int kills = (int)GameState::GetMonsterKilledCount(e->GetEntityData()->id);
    if (kills > 0 && (unsigned)kills % (unsigned)every == 0)
        for (int i = 0; i < count; ++i) Drop(e, item, collect);
}

// The belt's active orbs: the sum of `value` over them. With `use` each orb that counted loses a
// charge and is used up at its last (its price is added to the fight's spent money).
int OrbBonus(Entity* e, int (Items::ItemInfo::*value)() const, bool use) {
    if (!e->OrbsCanBeUsed()) return 0;
    int sum = 0;
    for (unsigned slot = 0; slot < GameState::GetBeltSlotCount(); ++slot) {
        Items::ItemInfo* info = Items::GetItemInfo(GameState::GetBeltItemAt(slot));
        if (!info || GameState::GetBeltItemActivationTimeAt(slot) == 0 || GameState::GetItemAmount(info->id, true) == 0)
            continue;
        int v = (info->*value)();
        sum += v;
        if (v == 0 || !use) continue;
        GameState::PlayerItem* it = GameState::GetFirstItemByID(GameState::GetBeltItemAt(slot), true);
        ++it->f0c;
        if (it->f0c >= (uint32_t)info->GetUseCount()) {
            if (BaseCombat* c = GameState::GetActiveCombat()) c->AddSpentMoney((int)info->cost, (int)info->cost2);
            GameState::RemoveUniqueItem(it->uniqueId);
        }
        BeltBarWindow::UpdateContents();
    }
    return sum;
}

}  // namespace

bool Entity::IsDeathAnimationRunning() { return g_deathAnimation; }

void Entity::SetItemDrop(MetaData* drop) {
    delete data->itemsDrop;
    data->itemsDrop = drop;
}

void Entity::SetWarriorCost(int v) { data->warriorCost = v; }
void Entity::SetXP(int v) { GetEntityData()->xp = v; }

int Entity::GetDamage() const {
    int d = attackRanged < 1 ? attackMelee : attackRanged;
    return attackMagic < 1 ? d : attackMagic;
}

void Entity::SetAggro(int range) {
    if (!spawnPoint) spawnPoint = new SpawnPoint();
    spawnPoint->aggroRange = range;
}

int Entity::GetInitiative(int level) const {
    if (level == 0) return initiative;
    if (!player) {
        int d = std::abs(data->level - level);
        if (d == 2) return (int)((float)initiative * 1.25f);
        if (d == 3) return (int)((float)initiative * 1.5f);
        if (d > 3) return (int)((float)initiative * 1.75f);
    } else {
        int d = std::abs(GameState::GetLevel() - level);
        if (d == 2) return (int)((float)initiative * 0.75f);
        if (d == 3) return (int)((float)initiative * 0.5f);
        if (d > 3) return (int)((float)initiative * 0.3f);
    }
    return initiative;
}

void Entity::ShowGlowAnimation() {
    glow = Render::RemoveSprite(glow);
    float x = (float)(int)sprite->x, y = (float)(int)sprite->y;
    glow = Render::CreateSprite(IconManager::GetIcon("Pickup_item_glow", false), 8, false, false);
    Render::SetShaderType(glow, 8);
    Render::SetPosition(glow, x, y, sprite->z + 0.001f);
    glowAlpha = 0.f;
    glowTime = 1.25f;
    Render::SortRenderLayer(8, 1);
}

bool Entity::IsBoss() const {
    MetaExpression* m = GetMetaExpression();
    return m && m->FindChildData(0x86) != nullptr;
}

const MetaData* Entity::GetBossWaveInfo() const {
    MetaExpression* m = GetMetaExpression();
    return m ? m->FindChildData(0x86) : nullptr;
}

const MetaData* Entity::GetBossAdditionalWavesInfo() const {
    MetaExpression* m = GetMetaExpression();
    return m ? m->FindChildData(0x95) : nullptr;
}

uint32_t Entity::GetBossTaskID() const {
    MetaExpression* m = GetMetaExpression();
    if (!m) return 0;
    const MetaData* d = m->FindChildData(0x93);
    return d ? (uint32_t)d->GetInt() : 0;
}

uint32_t Entity::GetBossMapID() const {
    MetaExpression* m = GetMetaExpression();
    if (!m) return 0;
    const MetaData* d = m->FindChildData(0x8b);
    return d ? (uint32_t)d->GetInt() : 0x25;
}

void Entity::OnAggro() {
    int sx = (int)worldX, sy = (int)worldY;
    Map::WorldCoordinatesToScreen(sx, sy);
    if (sx < 0x32 || sx >= Render::ScreenWidth() - 0x31 || sy < 0x32 || sy >= Render::ScreenHeight() - 0x31)
        Render::CenterOn(worldX, worldY, true);
    std::printf("Entity::OnAggro() - %d\n", (int)data->id);
    if (workplaceDecor) {
        workplaceDecor->ReturnCost();
        workplaceDecor->RemoveWorker();
    }
    f38 = true;
    if (player) BuildingHovers::HideWorldDialog(nullptr);
}

void Entity::OnAddedToCombat() {
    f36 = true;
    RemovePatrol();
    if (GameState::tutorial == 0xb || GameState::tutorial == 0xd) BuildingHovers::HideArrow();
    BuildingHovers::Update(0.f, true);
}

void Entity::OnRemovedFromCombat() {
    if (anim) anim->mult = 1.f;
    f36 = false;
    f38 = false;
    wanders = true;
}

bool Entity::OrbsCanBeUsed() {
    Entity* p = EntityManager::GetPlayer();
    if (p == this) return true;
    if (SquadUsesOrbs() && GetSquad()) return p == GetSquad()->GetOwner();
    return false;
}

int Entity::GetItemAttackHealthReturn(bool use) { return OrbBonus(this, &Items::ItemInfo::GetAttackHealthReturn, use); }
int Entity::GetItemCriticalChanceBonus(bool use) { return OrbBonus(this, &Items::ItemInfo::GetCriticalChanceBonus, use); }
int Entity::GetItemDamageAbsorption(bool use) { return OrbBonus(this, &Items::ItemInfo::GetDamageAbsorption, use); }
int Entity::GetItemDamageBoost(bool use) { return OrbBonus(this, &Items::ItemInfo::GetDamageBoost, use); }

int Entity::GetCritChance() { return critChance + f8c + GetItemCriticalChanceBonus(true); }

void Entity::OnBlock(Entity* attacker) {
    attacker->GetItemDamageBoost(true);
    if (data->id == 0xc || data->id == 0xd) SoundsManager::PlaySound("battle_block_player", 1, false);
    else if (data->finalClass == 10) SoundsManager::PlaySound("battle_block_soldier", 1, false);
    else SoundsManager::PlaySound("battle_block_monster", 1, false);
}

namespace {

// The map's SPELL_RESIST (0x79): per spell slot a percent, taken off the damage (rounded to the
// nearest). Written out in both WillDieMagic and OnHitMagic.
int SpellDamageAfterResist(int damage, uint32_t slot) {
    MetaExpression* meta = Locations::GetMapMeta(GameState::GetCurrentMapID());
    const MetaData* resist = meta ? meta->FindChildData(0x79) : nullptr;
    if (!resist) return damage;
    for (unsigned i = 0; i < resist->GetChildrenCount(); ++i) {
        if ((uint32_t)resist->GetChild(i)->GetChild(0)->GetInt() != slot) continue;
        float percent = (float)resist->GetChild(i)->GetChild(1)->GetInt();
        damage = (int)(0.5f + (float)damage * (1.f + percent * -0.01f));
    }
    return damage;
}

// The view goes to a spell's point when it is within 0x31 of the screen's edge or off it.
void ShowSpellCast(float wx, float wy) {
    int x = (int)wx, y = (int)wy;
    Map::WorldCoordinatesToScreen(x, y);
    if (x <= 0x31 || x >= Render::ScreenWidth() - 0x31 || y <= 0x31 || y >= Render::ScreenHeight() - 0x31)
        Render::CenterOn(wx, wy, true);
}

}  // namespace

bool Entity::WillDieMagic(Entity*, Spells::SpellInfo* spell, float mult, bool) {
    if (GameState::tutorial == 0x17 && IsEnemy()) return true;
    int damage = SpellDamageAfterResist((int)((float)spell->damage * mult), spell->slot);
    return hp - damage < 1;
}

int Entity::OnHitMagic(Entity*, Spells::SpellInfo* spell, int& resisted, float mult, bool) {
    int damage = (int)((float)spell->damage * mult);
    if (spell->slot == 1) SoundsManager::PlaySound("spell_cast_fire", 1, false);
    else if (spell->slot == 2) SoundsManager::PlaySound("spell_cast_lightning", 1, false);
    else if (spell->slot == 3) SoundsManager::PlaySound("spell_cast_ice", 1, false);
    int dealt = SpellDamageAfterResist(damage, spell->slot);
    resisted = damage - dealt;
    hp -= dealt;
    if (hp < 1) ConsumeHpOverlimit(false);
    Hit();
    return dealt;
}

void Entity::OnSpellCast(AI::Waypoint* target) { ShowSpellCast(target->wx, target->wy); }

void Entity::OnSpellCast(Entity* target) { ShowSpellCast(target->worldX, target->worldY); }

bool Entity::WillDie(Entity* attacker) {
    int damage = attacker ? BaseDamage(attacker) : 0;
    if (f15c) damage <<= 1;
    if (!attacker) damage -= GetItemDamageAbsorption(false);
    else damage = (attacker->GetItemDamageBoost(false) + damage) - GetItemDamageAbsorption(false) - Absorb(this, attacker);
    if (damage < 0) damage = 0;
    if (player) {
        if (hp <= damage && GameState::IsTameTutorial()) damage = 0;
        if (HpOverlimitConsumable()) return (hp - damage) + ffc < 1;
    }
    return hp - damage < 1;
}

// The side effects of any lost HP: the HUD's bars, and for a monster hit outside a boss fight its
// gold and experience start to spill (a share by the HP lost, each with an even chance per hit;
// the spawn point remembers what already dropped, OnDeath gives the rest).
void Entity::Hit() {
    if (player) GameState::UpdatePlayerRegenerationState();
    BattleBarWindow::OnDamage(this);
    PlayerTopWindow::OnDamage(this);
    HUDWindow::OnDamage(this);
    bool bossFight = GameState::IsCombatActive() && GameState::GetActiveCombat()->GetType() > 2;
    if (GameState::secondTutorial <= 0x82 || GameState::IsFirstVirtualTutorial() || GameState::IsTameTutorial() ||
        bossFight || !data || data->clas != 9 || !spawnPoint || spawnPoint->npc)
        return;
    if (GetHP() >= GetHpMax()) return;
    uint32_t map = GameState::GetCurrentMapID();
    int stars = Locations::GetMissionStarCount(map, 0);
    float goldMult = Locations::GetMapGoldMult(map, (uint32_t)stars);
    float xpMult = Locations::GetMapXPMult(map, (uint32_t)stars);
    float starMult = Setting("raid_star_gold_mult").GetChild((unsigned)stars).GetFloat();
    float xpAll = (float)(int)(xpMult * (float)data->xp * 0.4f);
    float lost = (float)(GetHpMax() - GetHP()) / (float)(GetHpMax() - 1);
    float goldAll = (float)(int)(goldMult * (float)data->warriorCost * starMult * 0.4f);
    int gold = (int)(goldAll * lost - (float)spawnPoint->f40);
    int xp = (int)(xpAll * lost - (float)spawnPoint->f44);
    if (gold > 0 && Rand48::lrand48() % 100 > 0x32) {
        BuildingHovers::DropResource(worldX, worldY, 8, (unsigned)gold, false, false);
        spawnPoint->f40 += gold;
    }
    if (xp > 0 && Rand48::lrand48() % 100 > 0x32) {
        BuildingHovers::DropResource(worldX, worldY, 10, (unsigned)xp, false, false);
        spawnPoint->f44 += xp;
    }
    if (gold > 0 || xp > 0) SoundsManager::PlaySound("item_dropped_light", 1, false);
}

namespace {
bool NameHas(const EntityData* d, const char* s) { return d->name.find(s) != std::string::npos; }   // FUN_00155abc
}  // namespace

// The attacker's blow: the hero's (male / female, critical: this entity's CriticalHit), a monster's
// own sound_attack (else "battle_attack_monster"; also without an attacker), a soldier's by its name:
// "fat" or "superbeard" a morning star at random, "skinny" + "melee" a sword at random, else the
// first morning star. (PlayHitSound(bool) @0x15de7c, the same with the flag as critical, is only
// called by the OnHitByEnemy functions: dead in 5.11.)
void Entity::PlayHitSound(Entity* attacker) {
    if (!attacker) {
        SoundsManager::PlaySound("battle_attack_monster", 1, false);
        return;
    }
    const EntityData* d = attacker->GetEntityData();
    const char* name;
    if (d->id == 0xc || d->id == 0xd) {
        if (CriticalHit()) name = attacker->IsMale() ? "battle_attack_male_player_crit" : "battle_attack_female_player_crit";
        else name = attacker->IsMale() ? "battle_attack_male_player" : "battle_attack_female_player";
    } else if (d->finalClass != 10 || d->type == 0x11) {
        SoundsManager::PlaySound(d->soundAttack.empty() ? "battle_attack_monster" : d->soundAttack.c_str(), 1, false);
        return;
    } else if (NameHas(d, "fat") || NameHas(d, "superbeard")) {
        name = (Rand48::lrand48() & 1) == 0 ? "battle_attack_soldier_morningstar1" : "battle_attack_soldier_morningstar2";
    } else if (NameHas(d, "skinny") && NameHas(d, "melee")) {
        name = (Rand48::lrand48() & 1) == 0 ? "battle_attack_soldier_sword1" : "battle_attack_soldier_sword2";
    } else {
        name = "battle_attack_soldier_morningstar1";
    }
    SoundsManager::PlaySound(name, 1, false);
}

int Entity::OnHit(Entity* attacker, int* absorbed) {
    PlayHitSound(attacker);
    // (The arena bot and the combat test keep a non-enemy at 50 HP: CombatTestBot and the console's
    // debug combat, dead in 5.11.)
    int damage = attacker ? BaseDamage(attacker) : 0;
    if (f15c) damage <<= 1;
    if (!attacker) {
        int a = GetItemDamageAbsorption(true);
        damage -= a;
        *absorbed = a;
    } else {
        if (int back = attacker->GetItemAttackHealthReturn(false)) attacker->AddHP(back);
        int boost = attacker->GetItemDamageBoost(true);
        *absorbed = GetItemDamageAbsorption(true);
        damage = (boost + damage) - *absorbed;
        int a = Absorb(this, attacker);
        *absorbed += a;
        damage -= a;
    }
    if (player && damage >= hp && GameState::IsTameTutorial()) return 0;
    if (damage < 1) return 0;
    if (IsEnemy() && GameState::IsPvPCombatActive()) Tasks::CompleteSubtask(Tasks::kDealDamage, 1, damage);
    // In a boss or arena fight of the second tutorial (steps 0x8a..0x9f) soldiers take no damage.
    if (GameState::IsCombatActive() && GameState::GetActiveCombat()->GetType() > 2 &&
        GameState::secondTutorial > 0x89 && GameState::secondTutorial < 0xa0 && data && data->clas == 10)
        damage = 0;
    hp -= damage;
    if (hp < 1) {
        ConsumeHpOverlimit(false);
        if (hp < 1) hp = 1;
    }
    Hit();
    return damage;
}

void Entity::OnDying(Entity* attacker) {
    // The death sound: the hero's, a soldier's (final class 10: fat / skinny, male / female) or the
    // monster's sound_death, else "battle_death_monster". (PlayDeathSound @0x15d664 is the same with
    // type 0x11 counted as a monster; only Entity::Die calls it: dead in 5.11.)
    if (data->id == 0xc || data->id == 0xd) {
        SoundsManager::PlaySound(IsMale() ? "battle_death_male_player" : "battle_death_female_player", 1, false);
    } else if (data->finalClass == 10) {
        SoundsManager::PlaySound(IsFat() ? (IsMale() ? "battle_death_male_soldier_fat" : "battle_death_female_soldier_fat")
                                         : (IsMale() ? "battle_death_male_soldier_skinny" : "battle_death_female_soldier_skinny"),
                                 1, false);
    } else {
        SoundsManager::PlaySound(data->soundDeath.empty() ? "battle_death_monster" : data->soundDeath.c_str(), 1, false);
    }
    if ((data->clas == 10 || player) && GameState::IsCombatActive()) GameState::GetActiveCombat()->Pause(this);
    if (IsBoss()) {
        Render::CenterOn(worldX, worldY, true);
        g_dyingBossTime = 0.f;
        g_dyingBoss = this;
        g_deathAnimation = true;
        GUI::SetInteractionLock(true);
    }
    if (IsEnemy()) {
        dying = true;
        speedScale = 1.f;
    }
    actuallyDying = true;
    // An enemy killed by magic of an attacker with "spell_fx" dissolves
    // (Particles::DesintegrateImageParticleSystem, its sprite goes transparent): milestones 5-6.
    (void)attacker;
}

void Entity::OnDeath(Entity* attacker) {
    if (IsEnemy()) dying = false;
    actuallyDying = false;
    // The overkill damage: how badly the dead hero's gear breaks.
    int penalty = attacker ? BaseDamage(attacker) : 1;
    if (f15c) penalty <<= 1;
    if (!attacker) {
        penalty -= GetItemDamageAbsorption(true);
    } else {
        if (int back = attacker->GetItemAttackHealthReturn(false)) attacker->AddHP(back);
        int boost = attacker->GetItemDamageBoost(true);
        penalty = (boost + penalty) - GetItemDamageAbsorption(true) - Absorb(this, attacker);
    }
    hp = 0;
    if (player) GameState::UpdatePlayerRegenerationState();
    BattleBarWindow::OnDamage(this);
    PlayerTopWindow::OnDamage(this);
    HUDWindow::OnDamage(this);
    Render::ChangeLayer(sprite, 6);

    // In a boss or arena fight the drops go to the map's collection (shown when the fight ends).
    bool collect = false, theBoss = false;
    if (GameState::IsCombatActive() && GameState::GetActiveCombat()->GetType() >= 3) {
        collect = true;
        theBoss = this == GameState::GetActiveCombat()->GetEnemySquad()->GetOwner();
    }
    if (!theBoss) {
        Tasks::CompleteSubtask(Tasks::kKillMob, data->id, 1);
        Tasks::CompleteSubtask(Tasks::kKillSpawn, GetSpawnPointID(), 1);
        if (!player && !(GetSquad() && GetSquad()->GetOwner() && GetSquad()->GetOwner()->player))
            Tasks::CompleteSubtask(Tasks::kKillEnemyClass, (uint32_t)data->enemyClass, 1);
    }

    // Its quest drops: (task, item, count) once the task is done (0x90), else until it is done
    // (0x91), else the plain (item, count) list (0x3e).
    if (MetaExpression* meta = GetMetaExpression()) {
        bool dropped = false;
        for (int kind : {0x90, 0x91}) {
            const MetaData* d = dropped ? nullptr : meta->FindChildData(kind);
            if (!d) continue;
            for (unsigned i = 0; i < d->GetChildrenCount(); ++i) {
                const MetaData* c = d->GetChild(i);
                if (GameState::TaskCompleted((unsigned)Int(c, 0)) != (kind == 0x90)) continue;
                for (int n = 0; n < Int(c, 2); ++n) Drop(this, (uint32_t)Int(c, 1), collect);
                dropped = true;
            }
        }
        const MetaData* d = dropped ? nullptr : meta->FindChildData(0x3e);
        if (d) {
            for (unsigned i = 0; i < d->GetChildrenCount(); ++i) {
                const MetaData* c = d->GetChild(i);
                if (c->GetChild(0)->type != 1) continue;
                for (int n = 0; n < Int(c, 1); ++n) Drop(this, (uint32_t)Int(c, 0), collect);
            }
        }
    }

    // "items_drop": item=chance:count[:every], the chance in percent with two decimals.
    if (const MetaData* d = data->itemsDrop) {
        for (unsigned i = 0; i < d->GetChildrenCount(); ++i) {
            const MetaData* c = d->GetChild(i);
            if (c->GetChild(0)->type != 1) {
                std::printf("ERROR: Entity::OnDeath() Entity %d has incorrect item drop configuration - no item ID\n",
                            (int)data->id);
                continue;
            }
            const MetaData* p = c->GetChild(1);
            if (p->GetChild(0)->type != 1 || p->GetChild(1)->type != 1) {
                std::printf("ERROR: Entity::OnDeath() Entity %d has incorrect item drop configuration\n", (int)data->id);
                continue;
            }
            int chance = (int)(0.5f + p->GetChild(0)->GetFloat() * 100.f);
            DropByChance(this, (uint32_t)Int(c, 0), chance, 10000, Int(p, 1), p, collect);
        }
    }

    // The mission's star drops ("star1_items".. by the map's stars; the arena: by its difficulty).
    uint32_t map = collect ? GameState::mReturnMapID : GameState::GetCurrentMapID();
    uint32_t stars = (uint32_t)Locations::GetMissionStarCount(map, 0);
    if (GameState::IsArenaCombatActive()) stars = GameState::GetEventDifficulty(2);
    if (const MetaData* d = data->starItems[stars > 1 ? 2 : stars]) {
        for (unsigned i = 0; i < d->GetChildrenCount(); ++i) {
            const MetaData* c = d->GetChild(i);
            const MetaData* p = c->GetChild(1);
            DropByChance(this, (uint32_t)Int(c, 0), Int(p, 0), 100, Int(p, 1), p, collect);
        }
    }

    // A monster's gold ("warrior_cost") and experience, less what its hits already spilled.
    if (data && data->clas == 9 && (!spawnPoint || !spawnPoint->npc)) {
        GameState::MonsterKilled(data->id);
        float goldMult = Locations::GetMapGoldMult(map, stars);
        float xpMult = Locations::GetMapXPMult(map, stars);
        int gold = (int)(goldMult * (float)data->warriorCost * Setting("raid_star_gold_mult").GetChild(stars).GetFloat());
        if (collect) {
            GameState::AddMapResourceCollectionInfo(GameState::GetCurrentMapID(), 8, gold);
            GameState::AddMapResourceCollectionInfo(GameState::GetCurrentMapID(), 10, (int)data->xp);
        } else if (GameState::secondTutorial < 0x83 || GameState::IsVirtualTutorial() || GameState::IsTameTutorial()) {
            GameState::ChangeResourceAmount(8, data->warriorCost);
            GameState::ChangeResourceAmount(10, (int)data->xp);
        } else if (spawnPoint) {
            gold -= spawnPoint->f40;
            if (gold > 0) BuildingHovers::DropResource(worldX, worldY, 8, (unsigned)gold, false, false);
            int xp = (int)(xpMult * (float)data->xp) - spawnPoint->f44;
            if (xp > 0) BuildingHovers::DropResource(worldX, worldY, 10, (unsigned)xp, false, false);
            if (gold > 0 || xp > 0) SoundsManager::PlaySound("item_dropped_light", 1, false);
        }
    }

    if (player) {
        // Each of the ten worn items may lose "death_penalty" durability: the chance is the overkill
        // (capped to the table) times "repair_chance_to_break"[overkill] percent.
        if (penalty < 0) penalty = 0;
        for (unsigned slot = 0; slot < 10; ++slot) {
            Setting table("repair_chance_to_break");
            if ((int)table.GetChildrenCount() <= penalty) penalty = (int)table.GetChildrenCount() - 1;
            int roll = (int)(NextSetupRandom() % 100);
            int chance = table.GetChild((unsigned)penalty).GetInt();
            if (roll >= penalty * chance) continue;
            GameState::PlayerItem* it = GameState::GetItemAt(slot);
            if (!it || it->f0c == 0) continue;
            uint32_t loss = it->info->deathPenalty;
            it->f0c = loss < it->f0c ? it->f0c - loss : 0;
        }
        CharacterInfoWindow::UpdateContents();
        // In the PvP arena the opponent's record counts the defeat and the outro shows, with a heal
        // of 60% of the hero's HP (at most the HP he came with) as the loot; the combat pauses and,
        // for the hero, hears of it. UNVERIFIED (milestone 5): map 0x70's BossIntroWindow
        // (reputation 1 in the arena, 2 outside) does the same with the reputation screen.
        // A real opponent hears of it (PvP::ReportStateToServer, state 3).
        if (GameState::IsPvPCombatActive()) {
            if (PvP::PlayerInfo* p = PvP::GetPlayerInfoByGID(PlayerCompetitionWindow::GetSelectedPlayerGID(), 0xff)) {
                SoundsManager::PlaySound("arena_lose", 1, false);
                ++p->losses;
                ++p->f420;
                p->defeated = 2;
                PvP::SetDefeatCount(PvP::GetDefeatCount() + 1);
                if ((unsigned)(p->gid + 4) > 3)
                    PvP::ReportStateToServer(PlayerCompetitionWindow::GetSelectedPlayerGID(), PvP::kStateLost);
            }
            const int heal = (int)((float)EntityManager::GetPlayer()->GetHpMax() * 0.6f);
            GameState::AddMapItemCollectionInfo((uint32_t)GameState::GetCurrentMapID(), 0x26a,
                                                std::min(heal, GameState::GetStartingArenaHP()));
            PlayerBattleInOutWindow::SetModeOutro();
            PlayerBattleInOutWindow::SetParameters(PlayerCompetitionWindow::GetSelectedPlayerGID(), false);
            PlayerBattleInOutWindow::Show();
            GameState::GetActiveCombat()->Pause(nullptr);
            if (player) GameState::GetActiveCombat()->PlayerDied();
        } else if (attacker) {
            HealSquadWindow::Show();
            HealSquadWindow::SetAttacker(attacker);
        }
        greyed = true;
    }

    if (data->clas == 10) {
        // A dead soldier leaves the squad (not in the PvP arena, type 6); an ordinary one is gone
        // for good.
        if (!GameState::IsCombatActive() || GameState::GetActiveCombat()->GetType() != 6) {
            EntityManager::GetPlayer()->GetSquad()->RemoveSoldier(this);
            if (!IsEliteSoldier()) temporary = true;
        }
        if (GameState::IsCombatActive() && GameState::IsTaskStarted(0x3d3) && !GameState::IsArenaCombatActive())
            BuildingHovers::ShowTextHoverWithStyle(worldX, worldY, StringTable::GetString("WARRIOR_DIED"), 0xe, 4.f,
                                                   25.f, true, false);
        // The first soldier lost: the "hire soldiers" quest and its hint.
        if (!GameState::TaskCompleted(0x3d3) && !GameState::IsArenaCombatActive()) {
            GameState::StartTask(0x3d3);
            Tasks::Update(true);
            HUDWindow::UpdateTasks();
            HUDWindow::RemoveTaskArrows();
            BattleBarWindow::LaunchFirstDeathHelper();
            BuildingHovers::ShowWorldDialog(EntityManager::GetPlayer(), StringTable::GetString("hint_first_warrior_death"),
                                            0, false, false, false, 0, false, false);
            if (!GameState::IsTaskStarted(0x4d7) && GameState::GetTaskBeginTime(0x4d7) == 0 &&
                GameState::GetTaskEndTime(0x4d7) == 0 && !Tasks::IsTaskActive(0x4db, true) &&
                !Tasks::IsTaskActive(0x4d9, true) && !Tasks::IsTaskActive(0x4da, false))
                GameState::StartTask(0x4d7);
        }
    }

    if (!EntityManager::GetEnemy()) Tasks::CompleteSubtask(Tasks::kKillAllOnMap, GameState::GetCurrentMapID(), 1);
    if (spawnPoint) {
        ++spawnPoint->f50;
        spawnPoint->deathTime = (uint32_t)Timer::GetGlobalTime();
    }
    uint32_t spawnId = GetSpawnPointID();
    if ((spawnId == 0x2d5 || (spawnId == 0x450 && GameState::GetMapCompletionCount(0x6d) == 0)) &&
        GameState::GetItemRecentTime(0x2b6) == 0)
        BuildingHovers::DropItem(worldX, worldY, Items::GetItemInfo(0x2b6), 0, 1);

    // The tutorials' fights.
    if (GameState::tutorial == 0xb) GameState::tutorial = 0xc;
    else if (GameState::tutorial == 0xd) GameState::tutorial = 0xe;
    int& st = GameState::secondTutorial;
    if (st == 0x8e) {
        st = 0x8f;
    } else if (st == 0x91) {
        BuildingHovers::DropItem(worldX, worldY, Items::GetItemInfo(0x88), 0, 1);
        st = 0x92;
    } else if (st == 0x204) {
        st = 0x205;
    } else if (st == 0x207) {
        Entity* hero = EntityManager::GetPlayer();
        if (Entity* next = EntityManager::GetNearestEnemy(hero, false, false)) {
            AIBaseState* heroAI = hero->GetAI();
            BuildingHovers::SetArrowClickCallback(new GUI::Callback([heroAI, next] { heroAI->ClickedEntity(next); }));
            BuildingHovers::ArrowAt(next->worldX + 20.f, next->worldY + next->GetIdleHeight() * -0.5f, false, true, false,
                                    false, false, false, false);
        }
        st = 0x208;
    } else if (st == 0x20f) {
        st = 0x210;
    }
}

// @0x15da30: its arrow flies to `target`; when it lands the target's AI gets ProjectileLanded(kind).
void Entity::OnProjectileFired(Entity* target, int kind) {
    delete projectile;
    projectile = nullptr;
    if (GetAttackRanged() > 0) {
        projectile = new Projectile(0, kind);
        projectile->SetOwner(this);
        projectile->MoveTo(target);
    }
    ai->ChangeState(0x23);   // (also without a ranged attack: the asm's ble skips only the arrow)
    // A "skinny" "archer" twangs one of two bows.
    if (NameHas(data, "skinny") && NameHas(data, "archer"))
        SoundsManager::PlaySound((Rand48::lrand48() & 1) == 0 ? "battle_attack_soldier_bow1" : "battle_attack_soldier_bow2", 1, false);
}

// @0x150c70: its own arrow has landed. (An AISiegeAttacker is told too: dead in 5.11.)
void Entity::ProjectileLanded() { ChangeState(0); }
