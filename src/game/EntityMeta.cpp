// The quest behaviour of entities: what an entity's MetaExpression (its spawn point's) makes it do
// when the map is set up, a task starts, a task or one of its steps is completed, an object is
// used or an event (tag) changes. Port of Entity::OnSetup @0x15a938, OnTaskStarted @0x159518,
// OnTaskCompleted @0x15a12c, OnSubTaskCompleted @0x159a4c, OnInteract @0x152fd0, OnTagUpdate
// @0x15b988 and AddPatrolPoint @0x15854c.
// Not ported: Entity::ExecutePvPSpawnMeta (0x1596d0, the PvP shadows: milestone 5), marked where
// it is called.
#include <cstring>

#include "engine/Render.h"
#include "engine/Timer.h"
#include "game/AIState.h"
#include "game/Entity.h"
#include "game/EntityData.h"
#include "game/GameState.h"
#include "game/Map.h"
#include "game/MetaData.h"
#include "game/MetaExpression.h"
#include "game/SpawnPoint.h"
#include "game/Tags.h"

namespace {

uint32_t g_setupSeed = 0;   // 0x6119c8 PrepareForSetup: the RANDOM places' generator

int Int(const MetaData* list, unsigned i) { return list->GetChild(i)->GetInt(); }
const char* Str(const MetaData* list, unsigned i) { return list->GetChild(i)->GetString(); }
bool IsNumber(const MetaData* list, unsigned i) {
    return i < list->GetChildrenCount() && list->GetChild(i)->type == MetaData::kNumber;
}
bool IsString(const MetaData* list, unsigned i) {
    return i < list->GetChildrenCount() && list->GetChild(i)->type == MetaData::kString;
}

// GOTO_AFTER_*: "place" or "place|hide|unhide|dialog": the words after the place.
struct GotoFlags { bool hide = false, unhide = false, dialog = false; };
const MetaData* GotoPlace(const MetaData* d, GotoFlags& f) {
    if (d->GetChild(0)->type != MetaData::kList) return d;
    for (unsigned i = 1; i < d->GetChildrenCount(); ++i) {
        if (d->GetChild(i)->type != MetaData::kString) continue;
        const char* w = Str(d, i);
        if (!std::strcmp(w, "hide")) { f.hide = true; f.unhide = false; }
        if (!std::strcmp(w, "unhide")) { f.unhide = true; f.hide = false; }
        if (!std::strcmp(w, "dialog")) f.dialog = true;
    }
    return d->GetChild(0);
}

}  // namespace

// @0x150534 Entity::IsEnemy
bool Entity::IsEnemy() const {
    if (!data || data->clas != 9) return false;
    return spawnPoint ? !spawnPoint->npc : true;
}

// @0x15014c Entity::GetMetaExpression
MetaExpression* Entity::GetMetaExpression() const {
    if (foreignMeta) return foreignMeta;
    return spawnPoint ? spawnPoint->meta : nullptr;
}

// @0x151230 Entity::PrepareForSetup
void Entity::PrepareForSetup() { g_setupSeed = Timer::GetGlobalTime(); }

unsigned Entity::NextSetupRandom() {
    g_setupSeed = g_setupSeed * 0x343fd + 0x269ec3;
    return (g_setupSeed & 0x7fffffff) >> 0x10;
}

// @0x15854c Entity::AddPatrolPoint
void Entity::AddPatrolPoint(const std::string& name, int x, int y, int wait, int direction,
                            const std::string& animation, int times, int pause) {
    PatrolPoint p;
    p.name = name;
    p.x = x;
    p.y = y;
    p.wait = wait;
    p.direction = direction;
    p.animation = animation;
    p.times = times;
    p.pause = pause;
    p.waitLeft = (float)wait;
    p.pauseLeft = (float)pause;
    patrol.push_back(p);
    patrolStep = 0;
    patrolIndex = 0;
}

namespace {

// UNHIDE_*: the entity is switched on; a living enemy fades in.
void Unhide(Entity* e) {
    e->SetActive(true, false);
    if (e->IsEnemy() && !e->IsDead()) {
        e->alpha = 0.f;
        e->Appear(true, true);
    }
}

// ANIM_AFTER_*: the animation becomes its idle animation ("death" is played once and held).
void SetQuestAnimation(Entity* e, const char* name) {
    e->GetAI()->SetCustomIdleAnimation(name, -1, -1);
    if (!std::strcmp(name, "death")) e->SetAnimationOnce("death", false, false, true, true);
    else e->SetAnimationP(name, true, false, false);
    e->f95 = false;
}

// STATS_REPLACE_AFTER_QUEST: the stats of another entity type at the player's level.
void ReplaceStats(Entity* e, int id) {
    const EntityData* d = EntityFactory::GetEntityByID(id);
    if (!d) return;
    int level = GameState::GetLevel();
    e->hpMax = d->GetHpForLevel(level);
    e->attackMelee = d->GetAttackMeleeForLevel(level);
    e->attackMagic = d->GetAttackMagicForLevel(level);
    e->attackRanged = d->GetAttackRangedForLevel(level);
    e->defenseMelee = d->GetDefenseMeleeForLevel(level);
    e->defenseRanged = d->GetDefenseRangedForLevel(level);
    e->defenseMagic = d->GetDefenseMagicForLevel(level);
    e->absorbMelee = e->absorbRanged = e->absorbMagic = 0;
    if (e->hpMax < e->hp) e->hp = e->hpMax;
    e->hpRate = d->hpRate;
}

}  // namespace

// @0x159518 Entity::OnTaskStarted
void Entity::OnTaskStarted(uint32_t task) {
    for (MetaExpression* e = GetMetaExpression(); e; e = e->GetNext()) {
        const MetaData* d = e->GetDataWithType(kExpArenaSpawn);
        if (!d || d->type != MetaData::kList || d->GetChildrenCount() < 2 || (uint32_t)Int(d, 1) != task) continue;
        if (!IsDead()) {
            // UNVERIFIED (milestone 5): the arena's entity for this type
            // (EntityFactory::GetEntityForArena(Int(d, 0), 0), 0x1610fc): when there is one the
            // entity becomes it (SwitchEntityData 0x1594e4), its spawn point takes the type and
            // its HP at the player's level and is alive again, then SetActive(true, false) and
            // it fades in (alpha 0, Appear(true, false)).
            continue;
        }
        if (spawnPoint && spawnPoint->deathTime == 0) spawnPoint->deathTime = GetCurrentTime();
        if (!active) {
            SetActive(true, false);
            Render::SetVisibility(sprite, false);
        }
    }
}

// @0x159a4c Entity::OnSubTaskCompleted
void Entity::OnSubTaskCompleted(uint32_t subtask) {
    for (MetaExpression* e = GetMetaExpression(); e; e = e->GetNext()) {
        const MetaData* d;
        if ((d = e->GetDataWithType(kExpUnhideSubQuest)) && (uint32_t)Int(d, 1) == subtask) Unhide(this);
        if ((d = e->GetDataWithType(kExpUnhideAfterSubQuest)) && (uint32_t)Int(d, 1) == subtask) Unhide(this);
        if ((d = e->GetDataWithType(kExpAnimAfterSubQuest)) && (uint32_t)Int(d, 1) == subtask) {
            SetQuestAnimation(this, Str(d, 2));
            GetAI()->UpdateLastTarget();
            int direction = IsNumber(d, 3) ? Int(d, 3) : -1;
            f94 = true;
            // (where it stands, or where its patrol ends)
            int x = patrol.empty() ? tileX : patrol.back().x;
            int y = patrol.empty() ? tileY : patrol.back().y;
            AddPatrolPoint("", x, y, 0, direction, Str(d, 2), 0, 0);
        }
        if ((d = e->GetDataWithType(kExpGotoAfterSubQuest))) {
            GotoFlags f;
            d = GotoPlace(d, f);
            if ((uint32_t)Int(d, 1) == subtask) {
                f95 = f.hide;
                f96 = f.dialog;
                if (f.unhide) SetActive(true, false);
                GetAI()->UpdateLastTarget();
                int direction = IsNumber(d, 4) ? Int(d, 4) : -1;
                f94 = true;
                AddPatrolPoint("", Int(d, 2), Int(d, 3), 0, direction, "", 0, 0);
            }
        }
        // UNVERIFIED (milestone 5): PVP_SPAWN without a GUID: ExecutePvPSpawnMeta.
    }
}

// @0x15a12c Entity::OnTaskCompleted
void Entity::OnTaskCompleted(uint32_t task) {
    for (MetaExpression* e = GetMetaExpression(); e; e = e->GetNext()) {
        const MetaData* d;
        if ((d = e->GetDataWithType(kExpUnhideQuest)) && (uint32_t)d->GetInt() == task) Unhide(this);
        if ((d = e->GetDataWithType(kExpUnhideAfterQuest)) && (uint32_t)d->GetInt() == task) Unhide(this);
        if ((d = e->GetDataWithType(kExpAnimFinishQuest)) && (uint32_t)Int(d, 0) == task) {
            f94 = true;
            AddPatrolPoint("", tileX, tileY, 0, -1, Str(d, 1), 1, 0);
        }
        if ((d = e->GetDataWithType(kExpAnimAfterQuest)) && (uint32_t)Int(d, 0) == task)
            SetQuestAnimation(this, Str(d, 1));
        if ((d = e->GetDataWithType(kExpGotoAfterQuest))) {
            GotoFlags f;
            d = GotoPlace(d, f);
            if ((uint32_t)Int(d, 0) == task) {
                f95 = f.hide;
                f96 = f.dialog;
                if (f.unhide) SetActive(true, false);
                GetAI()->UpdateLastTarget();
                int direction = IsNumber(d, 4) ? Int(d, 4) : -1;
                f94 = true;
                AddPatrolPoint("", Int(d, 1), Int(d, 2), 0, direction, "", 0, 0);
                if (IsString(d, 3)) GetAI()->SetCustomIdleAnimation(Str(d, 3), -1, -1);
            }
        }
        if ((d = e->GetDataWithType(kExpHideQuest)) && (uint32_t)d->GetInt() == task) SetActive(false, true);
        if ((d = e->GetDataWithType(kExpStatsReplaceAfterQuest)) && (uint32_t)Int(d, 0) == task)
            ReplaceStats(this, Int(d, 1));
        // UNVERIFIED (milestone 5): PVP_SPAWN without a GUID: ExecutePvPSpawnMeta.
    }
}

// @0x15a938 Entity::OnSetup
void Entity::OnSetup() {
    bool shown = true;   // no HIDE / UNHIDE part has switched it off
    for (MetaExpression* e = GetMetaExpression(); e; e = e->GetNext()) {
        const MetaData* d;
        if ((d = e->GetDataWithType(kExpDirection))) SetDirection(d->GetInt());
        if ((d = e->GetDataWithType(kExpAnimIdle))) {
            if (d->type == MetaData::kList) {   // name:times:seconds
                int times = Int(d, 1);
                float time = (float)Int(d, 2);
                for (int i = 0; i < times; ++i) EnqueueAnimation(Str(d, 0), time);
            } else if (!IsDead()) {
                SetAnimationP(d->GetString(), true, false, false);
            }
        }
        if ((d = e->GetDataWithType(kExpPatrol))) {
            // each point: [name:]x:y[:wait][:direction][:animation][:times][:pause]
            for (unsigned i = 0; i < d->GetChildrenCount(); ++i) {
                const MetaData* p = d->GetChild(i);
                std::string name, animation;
                unsigned k = 0;
                if (p->GetChild(0)->type == MetaData::kString) name = Str(p, k++);
                int x = Int(p, k), y = Int(p, k + 1);
                k += 2;
                int wait = IsNumber(p, k) ? Int(p, k++) : 0;
                int direction = IsNumber(p, k) ? Int(p, k++) : -1;
                if (IsString(p, k)) animation = Str(p, k++);
                int times = 1;
                if (IsNumber(p, k)) {
                    times = Int(p, k++);
                    if (times == 0) times = 1;
                }
                int pause = IsNumber(p, k) ? Int(p, k) : 0;
                AddPatrolPoint(name, x, y, wait, direction, animation, times, pause);
            }
            f96 = true;
        }
        if ((d = e->GetDataWithType(kExpHideQuest)) && GameState::TaskCompleted((unsigned)d->GetInt())) {
            shown = false;
            SetActive(false, true);
        }
        if ((d = e->GetDataWithType(kExpUnhideQuest)) && !GameState::TaskCompleted((unsigned)d->GetInt())) {
            SetActive(false, true);
            shown = false;
        }
        if ((d = e->GetDataWithType(kExpUnhideAfterQuest)) && GameState::TaskCompleted((unsigned)d->GetInt())) {
            shown = true;
            SetActive(true, false);
        }
        if ((d = e->GetDataWithType(kExpUnhideSubQuest)) &&
            !GameState::SubTaskCompleted((unsigned)Int(d, 0), (unsigned)Int(d, 1))) {
            SetActive(false, true);
            shown = false;
        }
        if ((d = e->GetDataWithType(kExpUnhideAfterSubQuest)) &&
            GameState::SubTaskCompleted((unsigned)Int(d, 0), (unsigned)Int(d, 1))) {
            shown = true;
            SetActive(true, false);
        }
        if ((d = e->GetDataWithType(kExpUnhideInteract))) {
            Map::Decor* o = Map::GetDecorationToInteract((uint32_t)d->GetInt());
            if (o && o->f4c != 0) {   // the object has not been used yet
                shown = false;
                SetActive(false, true);
            }
        }
        if ((d = e->GetDataWithType(kExpAnimAfterQuest)) && GameState::TaskCompleted((unsigned)Int(d, 0))) {
            bool death = !std::strcmp(Str(d, 1), "death");
            if (death) {   // lying dead
                SetAnimationOnce("death", false, false, true, true);
                SetAnimationFrame(GetAnimationFrameCount() - 1);
                Render::ChangeLayer(GetSprite(), 6);
                SetHP(0);
            } else if (!IsDead()) {
                SetAnimationP(Str(d, 1), true, false, false);
            }
        }
        if ((d = e->GetDataWithType(kExpGotoAfterQuest))) {
            GotoFlags f;
            d = GotoPlace(d, f);
            if (GameState::TaskCompleted((unsigned)Int(d, 0))) {
                SetPos(Int(d, 1), Int(d, 2));
                GetAI()->UpdateLastTarget();
                if (!IsDead()) {
                    if (IsString(d, 3)) {
                        GetAI()->SetCustomIdleAnimation(Str(d, 3), -1, -1);
                        SetAnimationP(Str(d, 3), true, false, false);
                    }
                    if (IsNumber(d, 4)) SetDirection(Int(d, 4));
                }
                if (f.hide) {
                    shown = false;
                    SetActive(false, true);
                }
                if (f.unhide) {
                    shown = true;
                    SetActive(true, false);
                }
            }
        }
        if ((d = e->GetDataWithType(kExpAnimAfterSubQuest)) &&
            GameState::SubTaskCompleted((unsigned)Int(d, 0), (unsigned)Int(d, 1)) && !IsDead()) {
            SetAnimationP(Str(d, 2), true, false, false);
            if (IsNumber(d, 3)) SetDirection(Int(d, 3));
        }
        if ((d = e->GetDataWithType(kExpGotoAfterSubQuest))) {
            GotoFlags f;
            d = GotoPlace(d, f);
            if (GameState::SubTaskCompleted((unsigned)Int(d, 0), (unsigned)Int(d, 1))) {
                SetPos(Int(d, 2), Int(d, 3));
                GetAI()->UpdateLastTarget();
                if (IsNumber(d, 4)) SetDirection(Int(d, 4));
                if (f.hide) {
                    shown = false;
                    SetActive(false, true);
                }
                if (f.unhide) {
                    shown = true;
                    SetActive(true, false);
                }
            }
        }
        // RANDOM (x:y|x:y|...): the first time on this map it stands at one of the places.
        if ((d = e->GetDataWithType(kExpRandom)) && GameState::GetMapStartTime(GameState::GetCurrentMapID()) == 0) {
            const MetaData* place = d->GetChild(NextSetupRandom() % d->GetChildrenCount());
            SetPos(Int(place, 0), Int(place, 1));
            GetAI()->UpdateLastTarget();
        }
        if ((d = e->GetDataWithType(kExpStatsReplaceAfterQuest)) && GameState::TaskCompleted((unsigned)Int(d, 0)))
            ReplaceStats(this, Int(d, 1));
        if ((d = e->GetDataWithType(kExpArenaSpawn)) && d->type == MetaData::kList && d->GetChildrenCount() > 1) {
            SetActive(GameState::IsTaskStarted((unsigned)Int(d, 1)), true);
            if (sprite && IsDead()) Render::SetVisibility(sprite, false);
        }
        if ((d = e->GetDataWithType(kExpMobSpawn)) && spawnPoint && d->type == MetaData::kList &&
            d->GetChildrenCount() != 0) {
            int limit = Int(d, 0);
            bool on = limit < 1 || spawnPoint->f50 < limit;
            if (!shown) on = false;
            SetActive(on, true);
            if (sprite && IsDead()) Render::SetVisibility(sprite, false);
        }
        // UNVERIFIED (milestone 5): PVP_SPAWN: ExecutePvPSpawnMeta.
        if ((d = e->GetDataWithType(kExpUnhideTag))) {
            Tags::Tag* tag = Tags::GetTag(d->GetString());
            if (tag && !tag->IsActive()) {
                SetActive(false, true);
                shown = false;
            }
        }
    }
}

// @0x152fd0 Entity::OnInteract
void Entity::OnInteract(uint32_t object) {
    for (MetaExpression* e = GetMetaExpression(); e; e = e->GetNext()) {
        const MetaData* d = e->GetDataWithType(kExpUnhideInteract);
        if (d && (uint32_t)d->GetInt() == object) Unhide(this);
    }
}

// @0x15b988 Entity::OnTagUpdate: an event's entities come and go with it.
void Entity::OnTagUpdate(Tags::Tag* tag) {
    for (MetaExpression* e = GetMetaExpression(); e; e = e->GetNext()) {
        const MetaData* d = e->GetDataWithType(kExpUnhideTag);
        if (!d) continue;
        Tags::Tag* mine = Tags::GetTag(d->GetString());
        if (mine != tag || !tag) continue;   // PORT: an unknown tag is skipped (the original reads it)
        if (mine->IsActive() && !active) {
            SetActive(true, false);
            Appear(false, true);
            OnSetup();
        }
        if (!tag->IsActive() && active) OnSetup();
    }
}
