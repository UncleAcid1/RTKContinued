// The fighters' side of a fight: AIWarrior's attack, hit, block and death (the combat slots of
// AIState.h) and AIEnemy::CheckAggro, which starts a fight when the hero stops in a monster's zone.
// Port of AIWarrior @0xf2144..0xf4b78 (combat part) and AIEnemy::CheckAggro @0xea444.
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
#include "game/Rand48.h"
#include "game/Setting.h"
#include "game/Squad.h"
#include "game/StringTable.h"
#include "hud/HUD.h"

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
    // UNVERIFIED (milestone 5, spells): hit by the hero while a buff item with a spell chance
    // (type 0xe, meta 0x2f, GameState::GetItemBuffRemainingTime) runs, the spell may strike too
    // (Entity::OnAttackMagic, WillDieMagic, MagicHit / MagicDeath).
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
    // (ChestOpenedWindow::IsVisible stops it too: milestone 5.)
    AI::Waypoint* wp = IsPlayerInAggroRange();
    if (!wp) return;
    if (!entity->GetAI()->SetTarget(wp, true) && (wp->x != entity->tileX || wp->y != entity->tileY)) return;
    if (player->IsDead()) {
        if (!HealSquadWindow::IsVisible()) HealSquadWindow::Show();
        return;
    }
    // PORT (until milestone 5's BossCombat): a boss does not attack. In the original a boss with a
    // boss task (Entity::GetBossTaskID, unless TaskInfoWindow::SkipBossTask) opens the quest window
    // in its boss form (SetFightBossCallback, SetTask(id, true)), and any other becomes a boss fight
    // once the squads stand (BaseCombat::SwitchToBossFight).
    if (entity->IsBoss()) return;
    GameState::EndActiveCombat(true);
    entity->RemovePatrol();
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
