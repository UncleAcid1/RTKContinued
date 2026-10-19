// The fighters' side of a fight: AIWarrior's attack, hit, block and death (the combat slots of
// AIState.h) and AIEnemy::CheckAggro, which starts a fight when the hero stops in a monster's zone.
// Port of AIWarrior @0xf2144..0xf4b78 (combat part) and AIEnemy::CheckAggro @0xea444.
#include <algorithm>
#include <cstdio>
#include <string>

#include "engine/Render.h"
#include "game/AI.h"
#include "game/AIState.h"
#include "game/Animation.h"
#include "game/BRand.h"
#include "game/BuildingHovers.h"
#include "game/Combat.h"
#include "game/Entity.h"
#include "game/EntityData.h"
#include "game/EntityManager.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/MetaData.h"
#include "game/MetaExpression.h"
#include "game/ParticlePresets.h"
#include "game/Rand48.h"
#include "game/Setting.h"
#include "game/Spells.h"
#include "game/Squad.h"
#include "game/StringTable.h"
#include "game/Tasks.h"
#include "hud/HUD.h"
#include "windows/Windows.h"

namespace {

// Where the damage numbers start: over the entity's head.
float HeadY(Entity* e) { return e->worldY - (e->GetSprite() ? e->GetSprite()->h : 0.f); }

// The damage number over e: "-n", under a "critical hit" line for a critical one.
void ShowDamage(Entity* e, int damage) {
    float x = e->worldX, y = HeadY(e);
    if (!e->CriticalHit()) {
        BuildingHovers::ShowTextHoverWithStyle(x, y, ToWideString(-damage), 3, 1.5f, 50.f, false, false);
        return;
    }
    int h = BuildingHovers::ShowTextHoverWithStyle(x, y, StringTable::GetString("damage_critical_hit"), 4, 1.5f, 50.f,
                                                   false, false);
    BuildingHovers::ShowTextHoverWithStyle(x, ((float)h + y) - 20.f, ToWideString(-damage), 5, 1.5f,
                                           50.f, false, false);
}

}  // namespace

// ------------------------------------------------------------------------------------- AIWarrior

void AIWarrior::StartedAttack(Entity* target) {
    lastHpTimer = hitTimer = deathTimer = blockTimer = f90 = f8c = f94 = 0.f;
    linked = target;
    deathDamage = 1;
    target->GetAI()->linked = entity;
    entity->OnAttack();
    target->OnDefend();
    ChangeState(2);
    target->GetAI()->ChangeState(3);
}

void AIWarrior::AddedToCombat() { entity->OnAddedToCombat(); }

void AIWarrior::RemovedFromCombat() {
    blockTimer = hitTimer = deathTimer = 0.f;
    linked = nullptr;
    // An attacker (2; an archer only while its arrow flies) or a defender (3) is free again.
    if (state == 2 ? entity->GetAttackRanged() == 0 : state == 3) state = 0;
    entity->OnRemovedFromCombat();
}

void AIWarrior::Attack(bool forceBlock) {
    Entity* target = linked;
    TurnTo(AI::GetWaypoint(target->tileX, target->tileY, false));
    char name[40] = {};
    std::snprintf(name, sizeof name, "attack%d", (int)(Rand48::lrand48() % 3) + 1);
    if (!entity->AnimExists(name)) {
        entity->SetAnimationOnce("attack1", false, false, false, true);
    } else if (!entity->SetAnimationOnce(name, false, false, false, true)) {
        entity->SetAnimationOnce("attack1", false, false, false, true);
    }
    // When the swing lands on the target.
    Animation* a = entity->GetAnimController()->anim;
    float delay = 0.1f;
    if (a) delay = a->GetDelay(false);
    if (entity->GetAttackRanged() > 0) delay = 1.4f;
    if (entity->GetAttackMagic() > 0 && a) delay = (float)a->frameCount / a->fps + 0.5f;
    entity->SetCrit(false);
    target->SetCrit(false);

    // Every attack fills the hero's fury: "rage_base" (plus his fury bonus) for his own, times a
    // setting for a soldier's or a monster's.
    int rage = Setting("rage_base").GetInt();
    Entity* player = EntityManager::GetPlayer();
    float fury;
    if (!entity->player) {
        float coeff = GameState::GetSetting(entity->f39 ? "rage_mob_attack_coeff" : "rage_warrior_coeff");
        fury = (float)rage * coeff;
    } else {
        fury = (float)(entity->GetFuryBonus() + rage);
    }
    player->AddAP((int)std::ceil(fury * entity->GetAdditiveFuryGain()));

    int blockRoll = BRand::Percent();
    int defense = target->GetDefenseMelee();
    if (entity->GetAttackRanged() > 0) defense = target->GetDefenseRanged();
    if (entity->GetAttackMagic() > 0) defense = target->GetDefenseMagic();
    int crit = entity->GetCritChance();
    // UNVERIFIED (milestone 5, the arena and PvP): the first arena events' monsters always get
    // blocked (map 0x70 before event 1 was won; an arena fight before event 2), and the PvP
    // tutorial's stances fix the outcome (BottomBattleWindow::IsInAttackStance / IsInBlockStance).
    bool noCrit = crit < BRand::Percent();
    if ((blockRoll <= defense && noCrit && GameState::tutorial == 0x80) || forceBlock) {
        target->GetAI()->Block(delay);
        return;
    }
    if (!noCrit && GameState::tutorial == 0x80) {
        entity->SetCrit(true);
        target->SetCrit(true);
    }
    if (!target->WillDie(entity)) {
        target->GetAI()->Hit(delay);
        return;
    }
    if (GameState::IsTameTutorial()) {
        entity->SetAnimationOnce("idle_1", false, false, false, true);
        delay = 0.1f;
    }
    target->GetAI()->Death(delay);
}

void AIWarrior::Hit(float delay) {
    if (delay > 0.f) {
        hitTimer = delay;
        return;
    }
    if (!entity->AnimExists("gethit")) {
        AnimEnded();
        entity->AnimationEnded();
    } else {
        entity->SetAnimationOnce("gethit", false, false, false, true);
    }
    int absorbed = 0;
    int damage = entity->OnHit(linked, &absorbed);
    if (damage == 0) {
        if (absorbed != 0) {
            float x = entity->worldX, y = HeadY(entity);
            int h = BuildingHovers::ShowTextHoverWithStyle(x, y, StringTable::GetString("damage_aborbed"), 1, 1.5f,
                                                           50.f, false, false);
            // (The original prints min(absorbed, 0) here.)
            BuildingHovers::ShowTextHoverWithStyle(x, ((float)h + y) - 20.f,
                                                   ToWideString(absorbed & (absorbed >> 31)), 2, 1.5f,
                                                   50.f, false, false);
        }
    } else {
        ShowDamage(entity, damage);
        // ParticlePresets::OnCriticalHit(critical_fx, from the attacker to it) for an enemy with
        // "critical_fx", on a critical hit or outside second-tutorial step 0x100: milestone 6.
    }
    // The hero's blow may bring a spell with it: each running buff potion (type 0xe) with a
    // RANDOM_SPELL (0x2f: spell, percent chance) strikes the one he hit.
    if (!linked->player) return;
    for (unsigned i = 0; Items::ItemInfo* item = Items::EnumItemInfo(i); ++i) {
        if (item->type != 0xe || !item->meta) continue;
        const MetaData* random = item->meta->FindChildData(kExpRandomSpell);
        if (!random || GameState::GetItemBuffRemainingTime(item->id) == 0) continue;
        unsigned spellId = (unsigned)random->GetChild(0)->GetInt();
        int chance = random->GetChild(1)->GetInt();
        if (Rand48::lrand48() % 100 >= chance) continue;
        linked->OnAttackMagic(entity, 0);
        Spells::SpellInfo* spell = Spells::GetSpellInfo(spellId);
        if (!spell) continue;
        float delay = ParticlePresets::OnSpell(spell->id, entity->worldX, entity->worldY, false);
        if (spell->id == 7) entity->SetUpdateSpeedScale(0.1f);
        if (!entity->WillDieMagic(linked, spell, 1.f, false))
            entity->GetAI()->MagicHit(delay, false, 1.f, true, spell->id);
        else
            entity->GetAI()->MagicDeath(delay, false, 1.f, true, spell->id);
    }
}

void AIWarrior::Block(float delay) {
    if (delay > 0.f) {
        blockTimer = delay;
        return;
    }
    if (!entity->AnimExists("block")) {
        AnimEnded();
        entity->AnimationEnded();
    } else {
        entity->SetAnimationOnce("block", false, false, false, true);
    }
    entity->OnBlock(linked);
    BuildingHovers::ShowTextHoverWithStyle(entity->worldX, HeadY(entity), StringTable::GetString("HIT_BLOCKED"), 1,
                                           1.5f, 50.f, false, false);
    // ParticlePresets::OnCriticalHit(100, ...) when the hero blocks: milestone 6.
}

void AIWarrior::Death(float delay) {
    if (delay > 0.f) {
        lastHpTimer = delay;
        deathTimer = delay;
        deathDamage = entity->GetHP();
        if (entity->IsEnemy() && !GameState::IsBossCombatActive()) BottomBattleWindow::EnableNet(entity->GetSpawnPointID());
        return;
    }
    if (!entity->IsDisappearing() && entity->IsAppeared()) {
        ShowDamage(entity, deathDamage);
        // ParticlePresets::OnCriticalHit(critical_fx, ...) for an enemy with "critical_fx":
        // milestone 6.
    }
    ChangeState(4);
    entity->SetAnimationOnce("death", false, false, true, true);
    entity->OnDying(linked);
}

void AIWarrior::ProjectileLanded(int kind) {
    if (kind == 0) Hit(0.f);
    else if (kind == 1) Death(0.f);
    else if (kind == 2) Block(0.f);
    lastHpTimer = 0.f;
    deathDamage = 1;
    hitTimer = blockTimer = deathTimer = 0.f;
    linked->GetAI()->ChangeState(0);
}

// ------------------------------------------------------------------------------- AIWarrior: spells

namespace {

// The area spells' 5x5 diamonds by "range" 0..3 (0x5791a4, 25 bytes each, row by row): 0 not hit,
// 1 a fifth of the damage, 2 half, 3 all.
const unsigned char kAreaPatterns[4][25] = {
    {0, 0, 0, 0, 0, 0, 2, 2, 2, 0, 0, 2, 3, 2, 0, 0, 2, 2, 2, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 0, 0, 0, 1, 2, 1, 0, 1, 2, 3, 2, 1, 0, 1, 2, 1, 0, 0, 0, 1, 0, 0},
    {0, 1, 1, 1, 0, 1, 2, 2, 2, 1, 1, 2, 3, 2, 1, 1, 2, 2, 2, 1, 0, 1, 1, 1, 0},
    {1, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 2, 3, 2, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 1},
};

// The styles of a spell's two hover texts (its name's, its damage's) by its slot: fire 8 / 9,
// lightning 10 / 11, arcane 12 / 13, else (no spell) 4 / 9.
void SpellStyles(const Spells::SpellInfo* spell, int& nameStyle, int& numberStyle) {
    nameStyle = 4;
    numberStyle = 9;
    if (!spell) return;
    if (spell->slot == 1) nameStyle = 8, numberStyle = 9;
    else if (spell->slot == 2) nameStyle = 10, numberStyle = 11;
    else if (spell->slot == 3) nameStyle = 12, numberStyle = 13;
}

// The spell's name over e (a missing spell shows "damage_critical_hit", as the original does).
const char32_t* SpellTitle(const Spells::SpellInfo* spell) {
    return StringTable::GetString(spell ? spell->name : "damage_critical_hit");
}

// The spell's name over e's head and its damage under it.
void ShowSpellDamage(Entity* e, const std::u32string& title, int nameStyle, int damage, int numberStyle) {
    float x = e->worldX, y = HeadY(e);
    int h = BuildingHovers::ShowTextHoverWithStyle(x, y, title.c_str(), nameStyle, 1.5f, 50.f, false, false);
    BuildingHovers::ShowTextHoverWithStyle(x, ((float)h + y) - 20.f, ToWideString(-damage), numberStyle, 1.5f,
                                           50.f, false, false);
}

}  // namespace

void AIWarrior::StartedMagicAttackArea(AI::Waypoint* wp) {
    lastHpTimer = hitTimer = deathTimer = blockTimer = f90 = f8c = f94 = 0.f;
    f7c = 0;
    f78 = f79 = false;
    f74 = 1.f;
    epicenter = wp;
    deathDamage = 1;
}

void AIWarrior::StartedMagicAttack(Entity* target) {
    lastHpTimer = hitTimer = deathTimer = blockTimer = f90 = f8c = f94 = 0.f;
    linked = target;
    deathDamage = 1;
    target->GetAI()->linked = entity;
    target->GetAI()->ChangeState(3);
}

void AIWarrior::FindEpicenterPos(const char* pattern, int& x, int& y) {
    for (int row = 0; row < 5; ++row, pattern += 5)
        for (int col = 0; col < 5; ++col)
            if (pattern[col] == 2) {
                x = col;
                y = row;
                return;
            }
}

void AIWarrior::MagicAttack(unsigned slot) {
    TurnTo(AI::GetWaypoint(linked->tileX, linked->tileY, false));
    entity->OnAttackMagic(linked, slot);
    Spells::SpellInfo* spell = Spells::GetBestSpellForSlot(slot, (uint32_t)entity->GetAP(), true);
    if (!spell) {
        // ErrorReporter::Printf
        std::printf("ERROR: AIWarrior::MagicAttack() Spell from slot %d was queued, but there is no AP (%d) to cast it\n",
                    (int)slot, entity->GetAP());
        return;
    }
    float delay = ParticlePresets::OnSpell(spell->id, linked->worldX, linked->worldY, false);
    if (spell->id == 7) linked->SetUpdateSpeedScale(0.1f);
    Tasks::CompleteSubtask(Tasks::kCastSpell, spell->id, 1);
    AIBaseState* target = linked->GetAI();
    if (linked->GetDefenseMagic() < BRand::Percent()) {
        if (!linked->WillDieMagic(entity, spell, 1.f, false))
            target->MagicHit(delay, true, 1.f, false, spell->id);
        else
            target->MagicDeath(delay, true, 1.f, false, spell->id);
    } else {
        target->MagicBlock(delay, true, 1.f, false, spell->id);
    }
}

void AIWarrior::MagicAttackArea(unsigned slot) {
    if (!epicenter) return;
    TurnTo(epicenter);
    Spells::SpellInfo* spell = Spells::GetBestSpellForSlot(slot, (uint32_t)entity->GetAP(), true);
    if (!spell) return;
    // (Spell 7 plays its effect on each monster instead, below.)
    float delay = 0.f;
    if (spell->id != 7) delay = ParticlePresets::OnSpell(spell->id, epicenter->wx, epicenter->wy, false);
    Tasks::CompleteSubtask(Tasks::kCastSpell, spell->id, 1);
    entity->OnAttackMagic(nullptr, slot);
    const unsigned char* pattern = kAreaPatterns[spell->range];
    int ex = 0, ey = 0;
    FindEpicenterPos((const char*)pattern, ex, ey);   // (the original never uses the result)

    // The diamond's tiles on the staggered grid: five columns of five tiles, each column starting
    // one row lower than the last; the first sits 4 rows above the epicenter. Odd rows are half a
    // tile to the right, so x steps up after an odd row.
    auto odd = [](int n) { return ((n < 0 ? -n : n) & 1) != 0; };
    int y0 = epicenter->y;
    int base = epicenter->x + (odd(y0) ? 1 : 0);
    int start = base - 1, alt = base - 2;
    if (odd(y0 - 1)) {
        start = base;
        alt = base - 1;
    }
    int rowX = (y0 - 2) % 2 != 1 ? alt : start;
    if ((y0 - 3) % 2 == 1) ++rowX;
    int rowY = y0 - 4;
    for (int row = 0; row < 5; ++row) {
        int x = rowX, y = rowY;
        for (int col = 0; col < 5; ++col, ++y) {
            AI::Waypoint* wp = AI::GetWaypoint(x, y, false);
            Entity* target = EntityManager::GetEntityAtXY(x, y);
            unsigned char share = pattern[row * 5 + col];
            if (target && wp && target->IsEnemy() && share != 0) {
                float mult = share == 2 ? 0.5f : share == 1 ? 0.2f : 1.f;
                AIBaseState* ai = target->GetAI();
                ai->linked = entity;
                ai->ChangeState(3);
                if (spell->id == 7) {
                    target->SetUpdateSpeedScale(0.1f);
                    delay = ParticlePresets::OnSpell(spell->id, target->worldX, target->worldY, false);
                }
                // UNVERIFIED (milestone 5, the PvP tutorial): outside tutorial 0x80 the spell
                // always lands.
                if (target->GetDefenseMagic() < BRand::Percent() || GameState::tutorial != 0x80) {
                    if (!target->WillDieMagic(entity, spell, mult, false))
                        ai->MagicHit(delay, false, mult, false, spell->id);
                    else
                        ai->MagicDeath(delay, false, mult, false, spell->id);
                } else {
                    ai->MagicBlock(delay, false, 1.f, false, spell->id);
                }
            }
            if (y % 2 == 1) ++x;
        }
        if (odd(rowY)) ++rowX;
        --rowX;
        ++rowY;
    }
}

void AIWarrior::MagicAttackAreaFinished(unsigned slot) {
    if (Spells::SpellInfo* spell = Spells::GetBestSpellForSlot(slot, (uint32_t)entity->GetAP(), true))
        entity->AddAP(-(int)spell->cost);
}

void AIWarrior::MagicHit(float delay, bool payMana, float mult, bool flag, unsigned spellId) {
    if (delay > 0.f) {
        f90 = delay;
        f78 = payMana;
        f79 = flag;
        f74 = mult;
        f7c = (int)spellId;
        return;
    }
    entity->SetUpdateSpeedScale(1.f);
    if (!entity->AnimExists("gethit")) {
        AnimEnded();
        entity->AnimationEnded();
    } else {
        entity->SetAnimationOnce("gethit", false, false, false, true);
    }
    Spells::SpellInfo* spell = Spells::GetSpellInfo(spellId);
    int resisted = 0;
    int damage = entity->OnHitMagic(linked, spell, resisted, mult, flag);
    if (spell && payMana) linked->AddAP(-(int)spell->cost);
    int nameStyle, numberStyle;
    SpellStyles(spell, nameStyle, numberStyle);
    // A spell the map resists is titled "weakened <spell>".
    std::u32string title = SpellTitle(spell);
    if (resisted != 0) title = std::u32string(StringTable::GetString("effect_text_weakened")) + U' ' + title;
    ShowSpellDamage(entity, title, nameStyle, damage, numberStyle);
}

void AIWarrior::MagicBlock(float delay, bool payMana, float mult, bool flag, unsigned spellId) {
    if (delay > 0.f) {
        f94 = delay;
        f78 = payMana;
        f79 = flag;
        f74 = mult;
        f7c = (int)spellId;
        return;
    }
    Spells::SpellInfo* spell = Spells::GetSpellInfo(spellId);
    if (spell && payMana) linked->AddAP(-(int)spell->cost);
    entity->SetAnimationOnce("block", false, false, false, true);
    entity->OnBlockMagic(linked);
    BuildingHovers::ShowTextHoverWithStyle(entity->worldX, HeadY(entity), StringTable::GetString("EFFECT_TEXT_RESIST"),
                                           1, 1.5f, 50.f, false, false);
}

void AIWarrior::MagicDeath(float delay, bool payMana, float mult, bool flag, unsigned spellId) {
    if (delay > 0.f) {
        f7c = (int)spellId;
        f78 = payMana;
        f74 = mult;
        f79 = flag;
        f8c = delay;
        deathDamage = entity->GetHP();
        return;
    }
    Spells::SpellInfo* spell = Spells::GetSpellInfo(spellId);
    if (spell && payMana) linked->AddAP(-(int)spell->cost);
    int nameStyle, numberStyle;
    SpellStyles(spell, nameStyle, numberStyle);
    // (The damage shown is the HP it had when the spell was cast.)
    ShowSpellDamage(entity, SpellTitle(spell), nameStyle, std::max(deathDamage, entity->GetHP()), numberStyle);
    ChangeState(4);
    entity->SetAnimationOnce("death", false, false, true, true);
    entity->OnDying(linked);
}

// --------------------------------------------------------------------------------------- AIEnemy

void AIEnemy::CheckAggro() {
    if (entity->IsDead()) return;
    Entity* player = EntityManager::GetPlayer();
    if (GameState::IsCombatActive()) {
        BaseCombat* c = GameState::GetActiveCombat();
        if (c->GetType() == 2) return;
        if (c->HasActivePair()) return;
    }
    if (player->IsRetreating()) return;
    if (ChestOpenedWindow::IsVisible()) return;
    AI::Waypoint* wp = IsPlayerInAggroRange();
    if (!wp) return;
    if (!entity->GetAI()->SetTarget(wp, true) && (wp->x != entity->tileX || wp->y != entity->tileY)) return;
    if (player->IsDead()) {
        if (!HealSquadWindow::IsVisible()) HealSquadWindow::Show();
        return;
    }
    // A boss with a boss quest opens the quest window in its boss form (its Fight button starts the
    // fight, TaskInfoWindow::OnFight); any other becomes a boss fight once the squads stand
    // (BaseCombat::SwitchToBossFight).
    GameState::EndActiveCombat(true);
    entity->RemovePatrol();
    if (entity->IsBoss() && entity->GetBossTaskID() != 0 && !TaskInfoWindow::SkipBossTask()) {
        player->OnAggro();
        TaskInfoWindow::SetFightBossCallback(entity);
        TaskInfoWindow::SetTask(entity->GetBossTaskID(), true);
        TaskInfoWindow::Show();
        return;
    }
    player->OnAggro();
    if (GameState::secondTutorial == 0x20e && entity->IsEnemy()) {
        BuildingHovers::HideArrow();
        GameState::secondTutorial = 0x20f;
    }
    GameState::SetCombat(new AggroCombat());
    entity->CreateSquad();
    entity->GetAI()->UpdateLastTarget();
    GameState::GetActiveCombat()->BeginCombat(player->GetSquad(), entity->GetSquad());
}
