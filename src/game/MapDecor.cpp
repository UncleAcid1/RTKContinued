// Map::Decor: the job on a decoration (its worker, price and action points) and what its
// expression answers. Port of the Map::Decor functions @0x130b58..0x13b99c that are not in Map.cpp.
#include "game/Map.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "engine/Render.h"
#include "engine/Timer.h"
#include "game/AI.h"
#include "game/AIState.h"
#include "game/BuildingHovers.h"
#include "game/Entity.h"
#include "game/EntityData.h"
#include "game/EntityManager.h"
#include "game/GameData.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/Locations.h"
#include "game/Setting.h"
#include "game/SpawnPoint.h"
#include "game/MetaData.h"
#include "game/MetaExpression.h"
#include "game/Rand48.h"
#include "game/Squad.h"
#include "game/StringTable.h"
#include "game/Tasks.h"
#include "hud/HUD.h"

namespace Map {

namespace {

// @0x130b58 / @0x130b70 (the same code twice): the tile to the left on the row above or below y.
int LeftOnNextRow(int x, int y) { return ((std::abs(y) & 1) ? x + 1 : x) - 1; }

// A list of x:y pairs from child `from` on names the tile.
bool ListHasTile(const MetaData* list, unsigned from, int x, int y) {
    for (unsigned i = from; i < list->GetChildrenCount(); ++i) {
        const MetaData* t = list->GetChild(i);
        if (t->GetChild(0)->GetInt() == x && t->GetChild(1)->GetInt() == y) return true;
    }
    return false;
}

}  // namespace

// @0x130b88 Map::Decor::AddExtraDecor
void Decor::AddExtraDecor(Render::Sprite* s) {
    s->next = extraDecors;
    extraDecors = s;
}

// @0x13297c Map::Decor::RemoveExtraDecors
void Decor::RemoveExtraDecors() {
    while (extraDecors) {
        Render::Sprite* next = extraDecors->next;
        Render::RemoveSprite(extraDecors);
        extraDecors = next;
    }
}

// @0x130b98 Map::Decor::HasEntity
bool Decor::HasEntity(const Entity* e) const {
    if (worker == e) return true;
    for (const Entity* p = particles; p; p = p->nextParticle)
        if (p == e) return true;
    return false;
}

// @0x130be8 Map::Decor::FindBaseCoordinates
void Decor::FindBaseCoordinates() {
    // UNVERIFIED: an id that is a building's uses that building's data here (GetData gives null).
    if (!GetData()) return;
    float X, Y;
    GetBasePoint(X, Y);
    baseX = minX = maxX = X;
    baseY = maxY = minY = Y;
    for (const Render::Sprite* s = sprite; s; s = s->next) {
        if (s->x < minX) minX = s->x;
        if (maxX < s->x + s->w) maxX = s->x + s->w;
        if (s->y - s->h < minY) minY = s->y - s->h;
        if (maxY < s->y) maxY = s->y;
    }
}

// @0x130f58 Map::Decor::GetCenterWorldXY
void Decor::GetCenterWorldXY(int& x, int& y) const {
    x = (int)(sprite->x + sprite->w) / 2;
    y = (int)(sprite->y - sprite->h) / 2;
}

// @0x130fa0 Map::Decor::IsEntityInRange
bool Decor::IsEntityInRange(const Entity* e, int range) const {
    if (range < 1) range = 1;
    if (!data) return false;   // PORT: the original reads the null data
    int depth = mirrored ? data->w : data->h;
    int width = mirrored ? data->h : data->w;
    int lowX = linearX - (depth - 1);
    int off = (width & 1) ? width / 2 : 0;
    int dx = 0;
    if (e->linearX < lowX) dx = lowX - e->linearX;
    else if (e->linearX > linearX) dx = e->linearX - linearX;
    int dy = 0;
    if (e->linearY < linearY - off) dy = std::abs(e->linearY - linearY + off);
    else if (e->linearY > linearY + width / 2) dy = e->linearY - (linearY + width / 2);
    return dy <= range && dx <= range;
}

// @0x1311e8 Map::Decor::WorkerIsWorking
bool Decor::WorkerIsWorking() const { return worker && worker->GetAI()->IsWorking(); }

// @0x1312a4 Map::Decor::WorkFinished
bool Decor::WorkFinished() const { return f50 + collectStart < Timer::GetGlobalTime(); }

// @0x131658 Map::Decor::GetReplaceWithTarget
uint32_t Decor::GetReplaceWithTarget() const {
    const MetaData* d = meta ? meta->FindChildData(kExpReplaceObj) : nullptr;
    return d ? (uint32_t)d->GetInt() : 0;
}

// @0x1316e8 Map::Decor::GetName
const char32_t* Decor::GetName() const {
    // UNVERIFIED (milestone 5): a chest (CHEST_DROP / CHEST_DROP_TYPE) has the name of the chest for
    // the player's level (Chests::GetChestForLevel 0x12b974 / GetChestForLevelByType 0x12d29c).
    if (const GameData::DecorData* d = GameData::GetDecoration(id)) return StringTable::GetString(d->name.c_str());
    if (const GameData::BuildingData* b = GameData::GetBuilding(id)) return StringTable::GetString(b->name.c_str());
    return nullptr;
}

// @0x1317a4 Map::Decor::WorkEnded
void Decor::WorkEnded() { BuildingHovers::Update(0.f, true); }

// @0x1317b4 Map::Decor::HasTriggerTileAt
bool Decor::HasTriggerTileAt(int x, int y) const {
    if (!meta) return false;
    const MetaData* d;
    // The quest and teleport lists start with what they trigger; their tiles follow.
    for (int type : {kExpStartQuestTile, kExpEndQuestTile, kExpEndSubQuestTile, kExpTeleportTile})
        if ((d = meta->FindChildData(type)) && ListHasTile(d, 1, x, y)) return true;
    if ((d = meta->FindChildData(kExpAp)) && ListHasTile(d, 0, x, y)) return true;
    if ((d = meta->FindChildData(kExpTp)) && d->type == MetaData::kList)
        return d->GetChild(0)->GetInt() == x && d->GetChild(1)->GetInt() == y;
    return false;
}

// @0x131aa0 Map::Decor::HasBlockTileAt
bool Decor::HasBlockTileAt(int x, int y) const {
    const MetaData* d = meta ? meta->FindChildData(kExpBt) : nullptr;
    if (!d) return false;
    for (unsigned i = 0; i < d->GetChildrenCount(); ++i) {
        const MetaData* t = d->GetChild(i);
        unsigned k = t->GetChild(0)->type == MetaData::kString ? 1 : 0;   // after its message
        if (t->GetChild(k)->GetInt() == x && t->GetChild(k + 1)->GetInt() == y) return true;
    }
    return false;
}

// @0x131b48 Map::Decor::GetActionPointCount
unsigned Decor::GetActionPointCount() const {
    if (!meta) return 0;
    if (const MetaData* d = meta->FindChildData(kExpAp)) return d->GetChildrenCount();
    if (meta->FindChildData(kExpTp)) return 1;
    return meta->FindChildData(kExpCraft) ? 1 : 0;
}

// @0x131ba4 Map::Decor::GetActionPoint
void Decor::GetActionPoint(int& ax, int& ay, int i) const {
    ax = ay = 0;
    if (!meta) return;
    if (const MetaData* d = meta->FindChildData(kExpAp)) {
        if ((int)d->GetChildrenCount() <= i) return;
        ax = d->GetChild((unsigned)i)->GetChild(0)->GetInt();
        ay = d->GetChild((unsigned)i)->GetChild(1)->GetInt();
        return;
    }
    if (const MetaData* d = meta->FindChildData(kExpTp)) {
        if (d->type != MetaData::kList) return;
        ax = d->GetChild(0)->GetInt();
        ay = d->GetChild(1)->GetInt();
        return;
    }
    int right = (y & 1) ? x + 1 : x;
    AI::Waypoint* wp;
    if (meta->FindChildData(kExpCraft)) {
        wp = AI::GetWaypoint(LeftOnNextRow(x, y), y + 1, false);
        if (!wp) wp = AI::GetWaypoint(right, y - 1, false);
        if (!wp) wp = AI::GetWaypoint(right, y + 1, false);
    } else if (meta->FindChildData(kExpChestDropType)) {
        wp = AI::GetWaypoint(right, y + 1, false);
        if (!wp) wp = AI::GetWaypoint(LeftOnNextRow(x, y), y + 1, false);
        if (!wp) wp = AI::GetWaypoint(right, y - 1, false);
    } else {
        return;
    }
    if (!wp) wp = AI::GetWaypoint(LeftOnNextRow(x, y), y - 1, false);
    if (wp) {
        ax = wp->x;
        ay = wp->y;
    } else {
        ax = x;
        ay = y;
    }
}

// @0x131e28 Map::Decor::GetGoblinSearchAnimation
const char* Decor::GetGoblinSearchAnimation() const {
    const MetaData* d = meta ? meta->FindChildData(kExpGoblin) : nullptr;
    return d ? d->GetString() : nullptr;
}

// @0x131e58 Map::Decor::GetHintMessage
const char32_t* Decor::GetHintMessage() const {
    const MetaData* d = meta ? meta->FindChildData(kExpHint) : nullptr;
    return d ? StringTable::GetString(d->GetString()) : nullptr;
}

// @0x131e90 Map::Decor::GetBlockMessage
const char32_t* Decor::GetBlockMessage() const {
    const MetaData* d = meta ? meta->FindChildData(kExpBt) : nullptr;
    if (!d || d->GetChildrenCount() == 0) return nullptr;
    const MetaData* first = d->GetChild(0)->GetChild(0);
    return first->type == MetaData::kString ? StringTable::GetString(first->GetString()) : nullptr;
}

// @0x131f74 Map::Decor::GetProgressMessage
const char32_t* Decor::GetProgressMessage() const {
    if (!meta) return nullptr;
    if (const MetaData* d = meta->FindChildData(kExpProgressMsg)) return StringTable::GetString(d->GetString());
    // UNVERIFIED (milestone 5): a CRAFT node shows its profession's text
    // (Professions::GetProfessionForObject 0x1e8060, +0x14).
    return nullptr;
}

// @0x132040 Map::Decor::GetRequiredItem
uint32_t Decor::GetRequiredItem() const {
    if (!meta) return 0;
    const MetaData* d;
    if ((d = meta->FindChildData(kExpReqUnlockQuest)) && !GameState::TaskCompleted((unsigned)d->GetInt())) return 0;
    if ((d = meta->FindChildData(kExpReqUnlockSubQuest)) &&
        !GameState::SubTaskCompleted((unsigned)d->GetChild(0)->GetInt(), (unsigned)d->GetChild(1)->GetInt()))
        return 0;
    if ((d = meta->FindChildData(kExpReqUnlockItem))) return (uint32_t)d->GetInt();
    // UNVERIFIED (milestone 5): a CRAFT node without its profession's tool equipped asks for the
    // tool (Professions::GetProfessionTool 0x1e8514); a chest for the key of the chest for the
    // player's level (Chests::GetChestForLevel 0x12b974 / GetChestForLevelByType 0x12d29c, +0x14).
    return 0;
}

// @0x1321b8 Map::Decor::IsLocked
bool Decor::IsLocked() const {
    if (!meta) return false;
    const MetaData* d;
    if ((d = meta->FindChildData(kExpReqUnlockQuest)) && !GameState::TaskCompleted((unsigned)d->GetInt())) return true;
    if ((d = meta->FindChildData(kExpReqUnlockSubQuest)) &&
        !GameState::SubTaskCompleted((unsigned)d->GetChild(0)->GetInt(), (unsigned)d->GetChild(1)->GetInt()))
        return true;
    if ((d = meta->FindChildData(kExpReqUnlockItem)) && GameState::GetItemAmount((uint32_t)d->GetInt(), false) == 0) {
        // (a used-up decoration with a portal stays open without the item)
        if (!portal || f4c != 0) return true;
    }
    // UNVERIFIED (milestone 5): a CRAFT node without its profession's tool, and a chest whose key
    // the player lacks, are locked too (see GetRequiredItem).
    return false;
}

// @0x13234c Map::Decor::GetMessage
std::u32string Decor::GetMessage(unsigned size) const {
    if (!meta) return {};
    const MetaData* d;
    if ((d = meta->FindChildData(kExpMessage)))
        return SWPrintf(size, U"%s", {StringTable::GetString(d->GetString())});
    if ((d = meta->FindChildData(kExpReqUnlockQuest)) && d->type == MetaData::kNumber &&
        !GameState::TaskCompleted((unsigned)d->GetInt()))
        return {};
    if ((d = meta->FindChildData(kExpReqUnlockSubQuest)) &&
        !GameState::SubTaskCompleted((unsigned)d->GetChild(0)->GetInt(), (unsigned)d->GetChild(1)->GetInt()))
        return {};
    if ((d = meta->FindChildData(kExpReqUnlockItem)) && d->type == MetaData::kNumber &&
        GameState::GetItemAmount((uint32_t)d->GetInt(), false) == 0) {
        if (const Items::ItemInfo* item = Items::GetItemInfo((uint32_t)d->GetInt()))
            return SWPrintf(size, StringTable::GetString("MSG_MISSING"), {StringTable::GetString(item->name)});
    }
    return {};
}

// @0x132ad8 Map::Decor::GetPlayerSearchAnimation
const char* Decor::GetPlayerSearchAnimation(unsigned* time) const {
    const MetaData* d = meta ? meta->FindChildData(kExpInteractAnim) : nullptr;
    if (!d) return nullptr;
    if (d->type == MetaData::kList && d->GetChildrenCount() > 1 && d->GetChild(0)->type == MetaData::kString &&
        d->GetChild(1)->type == MetaData::kNumber) {
        if (time) *time = (unsigned)d->GetChild(1)->GetInt();
        return d->GetChild(0)->GetString();
    }
    return nullptr;   // ErrorReporter: "ERROR: meta INTERACT_ANIM is incorrect"
}

// @0x132b8c Map::Decor::ReturnCost
void Decor::ReturnCost() {
    for (int i = 0; i < 11; ++i)
        if (cost[i] != 0) GameState::ChangeResourceAmount(i, cost[i]);
}

// @0x1336f0 Map::Decor::AssignWorker
void Decor::AssignWorker(Entity* e) {
    worker = e;
    e->SetWorkplaceDecoration(this);
    if (worker->GetAI()->AssignToJob(this)) return;
    if (worker) worker->SetWorkplaceDecoration(nullptr);
    worker = nullptr;
}

// @0x133748 Map::Decor::RemoveWorker
void Decor::RemoveWorker() {
    if (!worker) return;
    if (worker == EntityManager::GetPlayer()) f50 = 0;
    worker->GetAI()->RemoveFromJob();
    worker->SetWorkplaceDecoration(nullptr);
    // UNVERIFIED (milestone 4f): a goblin (not the hero) on the current map also leaves the
    // offline goblins (GameState::RemoveOfflineGoblin 0x194bd4).
    worker = nullptr;
}

// @0x134b94 Map::Decor::RemoveGoblinWorker
void Decor::RemoveGoblinWorker() {
    if (worker && !worker->IsPlayer()) worker = nullptr;
}

// @0x133dc0 Map::Decor::CanDropItem
bool Decor::CanDropItem(uint32_t item) const {
    if (!meta) return false;
    if (const MetaData* d = meta->FindChildData(kExpDrop)) {
        for (unsigned i = 0; i < d->GetChildrenCount(); ++i) {
            const MetaData* what = d->GetChild(i)->GetChild(0);
            if (what->type == MetaData::kNumber && (uint32_t)what->GetInt() == item) return true;
        }
    }
    if (const MetaData* d = meta->FindChildData(kExpDropAfterQuest)) {
        for (unsigned i = 0; i < d->GetChildrenCount(); ++i) {
            const MetaData* what = d->GetChild(i)->GetChild(1);
            if (what->type == MetaData::kNumber && (uint32_t)what->GetInt() == item) return true;
        }
    }
    // UNVERIFIED (milestone 5): a CRAFT node drops its profession's items that the map allows
    // (Locations::CanCraftNodeDropItem 0x1b3f10), a chest what the chest for the player's level
    // can hold (Chests::ChestInfo::GenerateContents 0x12a648).
    return false;
}

// @0x13a388 Map::Decor::RemoveAttacker
void Decor::RemoveAttacker(Entity* e) {
    if (hp < 1) return;
    attackers.erase(std::remove(attackers.begin(), attackers.end(), e), attackers.end());
}

// @0x13b99c Map::Decor::GetCenterPoint
void Decor::GetCenterPoint(int& cx, int& cy) const {
    cx = x;
    cy = y;
    int depth = 0;
    if (const GameData::DecorData* d = GameData::GetDecoration(id)) depth = mirrored ? d->w : d->h;
    else if (const GameData::BuildingData* b = GameData::GetBuilding(id)) depth = mirrored ? b->w : b->h;
    for (int i = 0; i < depth / 2; ++i) {
        cx = LeftOnNextRow(cx, cy);
        --cy;
    }
    TileCoordinatesToWorld(cx, cy);
}

// @0x1329ac BaseAnimController<Map::Decor>::Update
void DecorAnim::Update(float dt) {
    if (!texture) return;
    if (dt > 8.f) dt = 8.f;
    if (decor && decor->sprite) {
        if (texture != decor->sprite->tex) {
            Render::SetTexture(decor->sprite, texture);
            shownFrame = -1;
        }
        if (shownFrame != frame)
            Render::SetFrame(decor->sprite, (float)Render::GetFrameHeight(texture),
                             (float)Render::GetFrameWidth(texture), frame);
        shownFrame = frame;
    }
    if (paused) return;
    timer += dt;
    while (timer > 0.f) {
        if (++frame >= texture->frames) frame = 0;
        timer += 1.5f / (-1.f / texture->frameTime);
    }
}

// @0x133c44 Map::Decor::RemoveParticles
void Decor::RemoveParticles() {
    while (particles) {
        Entity* next = particles->nextParticle;
        EntityManager::RemoveEntity(particles, true);
        particles = next;
    }
}

// @0x135260 Map::Decor::CreateParticles
void Decor::CreateParticles(const char* name, unsigned count, bool randomStart) {
    if (!data || data->particlePoints.empty() || !sprite) return;
    for (const std::pair<int, int>& point : data->particlePoints) {
        Entity* e = EntityManager::SpawnEntityAt(std::string(name), 0x1e, 8, false, false);
        if (!e) break;
        e->SetWorldPos((float)point.first + sprite->x, (float)point.second + sprite->y - sprite->h);
        e->SetCustomZ(sprite->z - 1e-05f);
        if (!e->GetEntityData()->anims.empty())
            e->SetAnimationP(e->GetEntityData()->anims[0]->name.c_str(), randomStart, false, false);
        // UNVERIFIED (milestone 6): Entity::SetParticlePlayCount(count) (0x15124c) when count != 0.
        (void)count;
        if (particles) e->nextParticle = particles;
        particles = e;
    }
}

// @0x135550 Map::Decor::UpdateDamageParticles
void Decor::UpdateDamageParticles() {
    if (!data || data->particlePoints.empty()) return;
    if ((float)hp < (float)hpMax * 0.33f) {
        if (!particles || particles->GetEntityData()->id != 0xfa) return;   // the smoke becomes fire
        RemoveParticles();
        CreateParticles("fire_1", 0, false);
    } else if ((float)hp < (float)hpMax * 0.66f && !particles) {
        CreateParticles("production_smoke", 0, false);
    }
}

// @0x1356ec Map::Decor::ReceiveDamage
void Decor::ReceiveDamage(int damage) {
    hp -= damage;
    UpdateDamageParticles();
}

// @0x1337d4 Map::Decor::TeleportPlayerTile
void Decor::TeleportPlayerTile(int tx, int ty) {
    Entity* player = EntityManager::GetPlayer();
    if (player->tileX == tx && player->tileY == ty) return;
    BaseSquad* squad = player->GetSquad();
    for (unsigned i = 0; i < (unsigned)squad->GetSoldierCount(); ++i) {
        squad->GetSoldier(i)->SetPos(tx, ty);
        squad->GetSoldier(i)->GetAI()->Reset(true);
    }
    Entity::UpdateSafePos(player);
    player->GetSquad()->MoveTo(AI::GetWaypoint(tx, ty, false));
    TileCoordinatesToWorld(tx, ty);
    Render::CenterOn((float)tx, (float)ty);   // (animated over 0.4 s on the original)
}

// @0x13391c Map::Decor::TeleportAction
bool Decor::TeleportAction(unsigned i) {
    if (GameState::secondTutorial != 0x100) return false;
    const MetaData* d = meta->FindChildData(kExpPortal);
    if (!d || !EntityManager::GetPlayer()) return false;
    if (i >= d->GetChildrenCount()) i = 0;
    TeleportPlayerTile(d->GetChild(i)->GetChild(0)->GetInt(), d->GetChild(i)->GetChild(1)->GetInt());
    return true;
}

// @0x1339cc Map::Decor::TeleportPlayerMap
void Decor::TeleportPlayerMap(int map) {
    // UNVERIFIED (milestone 4f): unless a map load is queued (Map::IsMapLoadQueued 0x1b5fb8),
    // Map::EnqueueLoadMap(map, false, true, 0) (0x1b5f7c).
    (void)map;
}

// @0x1339f8 Map::Decor::HandleTeleportTile
void Decor::HandleTeleportTile(const MetaData* d) {
    Entity* player = EntityManager::GetPlayer();
    if (!ListHasTile(d, 1, player->tileX, player->tileY) || IsLocked()) return;
    const MetaData* to = d->GetChild(0);
    if (to->type == MetaData::kNumber) {
        // (The original also looks for a tile near the hero that is not one of these tiles, to
        // come back to; its loop runs over an empty range and never finds one.)
        TeleportPlayerMap(to->GetInt());
    } else if (to->type == MetaData::kList && to->GetChildrenCount() == 2) {
        // Both numbers are read from the pair's first (as the original does).
        TeleportPlayerTile(to->GetChild(0)->GetInt(), to->GetChild(0)->GetInt());
    }
}

namespace {

// The hover text of a decoration's message: half its image to the right of its tile, above it.
void MessageHover(const Decor* d, const char32_t* text) {
    int wx = d->x, wy = d->y;
    TileCoordinatesToWorld(wx, wy);
    float dx = d->sprite ? d->sprite->w * 0.5f : 0.f, dy = d->sprite ? d->sprite->h : 0.f;
    BuildingHovers::ShowTextHover((float)wx + dx, (float)wy - dy, text, 1.f, 1.f, 1.f, 0.f, 0.f, 0.f, 0x19, 5,
                                  5.f, true, 2.f, 50.f);
}

// "<map><id>" as one decimal number: how quests name a decoration type on a map.
uint32_t MapObjectKey(int map, uint32_t id) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%d%d", map, (int)id);
    return (uint32_t)std::atoi(buf);
}

// DROP's entries: "name:amount[:percent]" gives the resource (to: the player directly, else a drop
// at the world point); "item:count[:percent]" rolls each item.
void GiveDrops(const Decor* d, const MetaData* list, int wx, int wy, bool direct) {
    // On a mission map gold (8) is scaled by the map's multiplier for its stars and
    // "raid_star_gold_mult", experience (10) by the map's.
    uint32_t map = GameState::GetCurrentMapID();
    bool mission = Locations::IsMissionMap(map);
    uint32_t stars = mission ? (uint32_t)Locations::GetMissionStarCount(map, 0) : 0;
    float goldMult = Locations::GetMapGoldMult(map, stars), xpMult = Locations::GetMapXPMult(map, stars);
    for (unsigned i = 0; i < list->GetChildrenCount(); ++i) {
        const MetaData* c = list->GetChild(i);
        int percent = c->GetChildrenCount() < 3 ? 100 : c->GetChild(2)->GetInt();
        if (c->GetChild(0)->type == MetaData::kString) {
            int type = GameState::StringToResourceType(c->GetChild(0)->GetString());
            int amount = c->GetChild(1)->GetInt();
            if (direct) {
                GameState::ChangeResourceAmount(type, percent * amount / 100);
                std::u32string text =
                    SWPrintf(0x80, U"+%d %s", {percent * amount / 100, GameState::GetResourceGameName(type)});
                BuildingHovers::ShowTextHover((float)wx, (float)wy - (d->sprite ? d->sprite->h : 0.f), text.c_str(),
                                              1.f, 1.f, 1.f, 0.f, 0.f, 0.f, 0x19, 5, 5.f, true, 2.f, 50.f);
            } else {
                if (type == 8 && mission)
                    amount = (int)(goldMult * (float)amount * Setting("raid_star_gold_mult").GetChild(stars).GetFloat());
                else if (type == 10 && mission)
                    amount = (int)(xpMult * (float)amount);
                BuildingHovers::DropResource((float)wx, (float)wy, type, (unsigned)(percent * amount / 100), false,
                                             false);
            }
        }
        if (c->GetChild(0)->type == MetaData::kNumber) {
            for (int n = 0; n < c->GetChild(1)->GetInt(); ++n)
                if (lrand48() % 100 < percent)
                    BuildingHovers::DropItem((float)wx, (float)wy,
                                             Items::GetItemInfo((uint32_t)c->GetChild(0)->GetInt()), 0, 1);
        }
    }
}

// What both ends of a job tell the quests and the world: the steps "click object" (0x23) and
// "interact with place" (0x22); in the city also as map 47.
void CompleteInteractSteps(const Decor* d) {
    int map = (int)GameState::GetCurrentMapID();
    Tasks::CompleteSubtask(0x23, MapObjectKey(map, d->id), 1);
    if (map == 0) Tasks::CompleteSubtask(0x23, MapObjectKey(0x2f, d->id), 1);
    Tasks::CompleteSubtask(0x22, (uint32_t)d->y + (uint32_t)d->x * 0x100 + (uint32_t)map * 0x10000, 1);
    if (map == 0) Tasks::CompleteSubtask(0x22, (uint32_t)d->y + 0x2f0000 + (uint32_t)d->x * 0x100, 1);
}

void TellInteract(const Decor* d) {
    uint32_t map = GameState::GetCurrentMapID();
    uint32_t key = (uint32_t)d->y + (uint32_t)d->x * 0x100 + (map == 0 ? 0x2f0000 : map << 16);
    EntityManager::OnInteract(key);
    OnInteract(key);
    BuildingHovers::Update(0.f, true);
}

// PARTICLE_* values: name[:count[:fixed start]].
void ParticlesFrom(Decor* d, const MetaData* p) {
    d->RemoveParticles();
    if (!d->data || d->data->particlePoints.empty()) return;
    const char* name = p->GetChild(0)->GetString();
    if (std::strcmp(name, "none") == 0) return;
    unsigned count = p->GetChildrenCount() < 2 ? 0 : (unsigned)p->GetChild(1)->GetInt();
    bool randomStart = p->GetChildrenCount() < 3 || p->GetChild(2)->GetInt() == 0;
    d->CreateParticles(name, count, randomStart);
}

}  // namespace

// @0x134948 Map::Decor::OnClick
bool Decor::OnClick() {
    // UNVERIFIED (milestone 4g): in a combat a real decoration's tap goes to
    // CombatManager::OnDecorClicked (0x12e494).
    if (meta) {
        const MetaData* d;
        if ((d = meta->FindChildData(kExpReqUnlockQuest)) && !GameState::TaskCompleted((unsigned)d->GetInt())) return false;
        if ((d = meta->FindChildData(kExpReqUnlockSubQuest)) &&
            !GameState::SubTaskCompleted((unsigned)d->GetChild(0)->GetInt(), (unsigned)d->GetChild(1)->GetInt()))
            return false;
        // (Without its item only a used-up decoration with a portal goes on, to the portal.)
        if ((d = meta->FindChildData(kExpReqUnlockItem)) && GameState::GetItemAmount((uint32_t)d->GetInt(), false) == 0)
            return portal && f4c == 0 ? PortalAction() : false;
        if (f4c != 0) return false;
        if (portal) return PortalAction();
        if (meta->FindChildData(kExpPortal)) {
            if (Entity* player = EntityManager::GetPlayer()) {
                player->GetAI()->ClickedDecoration(this);
                return true;
            }
        }
        if ((d = meta->FindChildData(kExpMessage)) && sprite)
            MessageHover(this, StringTable::GetString(d->GetString()));
    }
    return portal ? PortalAction() : false;
}

// @0x131fd8 Map::Decor::PortalAction: the hero goes to the portal (not during tutorial steps 10..0x16).
bool Decor::PortalAction() {
    if ((unsigned)(GameState::tutorial - 10) <= 0xc || !portal) return false;
    Entity* player = EntityManager::GetPlayer();
    if (!player) return false;
    player->GetAI()->ClickedPortal(portal);
    return true;
}

// @0x133c80 Map::Decor::ShowUseItemHover
bool Decor::ShowUseItemHover() const {
    if (!visible || !meta || f4c == 0) return false;
    const MetaData* d;
    if ((d = meta->FindChildData(kExpReqUnlockQuest)) && !GameState::TaskCompleted((unsigned)d->GetInt())) return false;
    if ((d = meta->FindChildData(kExpReqUnlockSubQuest)) &&
        !GameState::SubTaskCompleted((unsigned)d->GetChild(0)->GetInt(), (unsigned)d->GetChild(1)->GetInt()))
        return false;
    if (MetaExpression* icon = meta->FindByType(kExpInteractIcon)) {
        d = icon->GetData();
        if (d && d->GetChildrenCount() != 0 && d->GetChild(0)->type == MetaData::kString)
            return std::strcmp(d->GetChild(0)->GetString(), "none") != 0;
        return true;
    }
    if ((d = meta->FindChildData(kExpReqUnlockItem))) return GameState::GetItemAmount((uint32_t)d->GetInt(), false) != 0;
    return false;
}

// @0x132bcc Map::Decor::WorkStarted
void Decor::WorkStarted() {
    uint32_t now = Timer::GetGlobalTime();
    collectStart = now;
    if (worker && !worker->IsPlayer()) {
        worker->SetEndWorkTS((int)(now + f50));
        // UNVERIFIED (milestone 4f): GameState::RefreshOfflineGoblins (0x19c874).
    }
    // UNVERIFIED (milestone 6): the hero's start sound: SOUND, else the type's sound (DecorData
    // +0x60), else "gather_grass" / "gather_ore" for a herb / ore node and
    // "interact_(fe)male_player_interact".
    for (int i = 0; i < 11; ++i) {
        if (cost[i] == 0) continue;
        GameState::ChangeResourceAmount(i, -cost[i]);
        std::u32string text = SWPrintf(0x80, U"-%d %s", {cost[i], GameState::GetResourceGameName(i)});
        int wx = x, wy = y;
        TileCoordinatesToWorld(wx, wy);
        BuildingHovers::ShowTextHover((float)wx, (float)wy, text.c_str(), 1.f, 1.f, 1.f, 0.f, 0.f, 0.f, 0x19, 5,
                                      5.f, true, 2.f, 50.f);
    }
}

// @0x133058 Map::Decor::UpdateOfflineState
void Decor::UpdateOfflineState() {
    if (f50 + collectStart >= Timer::GetGlobalTime()) return;
    if ((int)f4c < 1) f4c = 0;
    else if (--f4c != 0) return;
    int wx = x, wy = y;
    TileCoordinatesToWorld(wx, wy);
    CompleteInteractSteps(this);
    if (const MetaData* d = meta ? meta->FindChildData(kExpDrop) : nullptr) GiveDrops(this, d, wx, wy, true);
    TellInteract(this);
}

// @0x13577c Map::Decor::Update
void Decor::Update(double dt) {
    if (anim) anim->Update((float)dt);
    if (WorkerIsWorking()) {
        uint32_t now = Timer::GetGlobalTime();
        // The tutorial's two jobs take half their time.
        if ((GameState::tutorial == 0x1f || GameState::tutorial == 0x3b) && f50 / 2 < now - collectStart)
            collectStart = now - f50 / 2;
        if (now == f50 + collectStart && !worker->IsPlayer() && !Render::CenteringInProgress() && !IsCameraMoving()) {
            // A goblin about to finish off screen (within 49 px of an edge) is shown.
            int sx = (int)worker->worldX, sy = (int)worker->worldY;
            WorldCoordinatesToScreen(sx, sy);
            if (sx < 0x32 || sx >= Render::ScreenWidth() - 0x31 || sy < 0x32 || sy >= Render::ScreenHeight() - 0x31)
                Render::CenterOn(worker->worldX, worker->worldY);   // (animated over 0.4 s on the original)
        }
        if (Timer::GetGlobalTime() > f50 + collectStart) {
            RemoveWorker();
            // UNVERIFIED (milestone 6): "chest_opened" for a type whose name contains "chest".
            bool usedUp = true;
            if ((int)f4c < 1) f4c = 0;
            else usedUp = --f4c == 0;
            if (usedUp) Finish();
        }
    }
    Entity* player = EntityManager::GetPlayer();
    const MetaData* d;
    // PORT: without the hero (the original reads a null player) the tiles are skipped.
    if (visible && meta && player && (d = meta->FindChildData(kExpStartQuestTile)) &&
        ListHasTile(d, 1, player->tileX, player->tileY)) {
        GameState::StartTask((unsigned)d->GetChild(0)->GetInt());
        HUDWindow::UpdateTasks();
        Tasks::EnqueueUpdate();
    }
    if (visible && meta && player && (d = meta->FindChildData(kExpEndQuestTile)) &&
        ListHasTile(d, 1, player->tileX, player->tileY)) {
        uint32_t task = (uint32_t)d->GetChild(0)->GetInt();
        Tasks::TaskInfo* t = GameState::TaskCompleted(task) ? nullptr : Tasks::GetTask(task);
        if (t && (!t->HasTimeout() || t->GetRemainingTime() != 0)) {
            for (const Tasks::TaskSubtask& s : t->subtasks)
                if (s.id != 0) Tasks::CompleteSubtaskDirect(s.id, 0x400);
            Tasks::CompleteTask(task, true, true, false);
        }
    }
    if (visible && meta && player && (d = meta->FindChildData(kExpEndSubQuestTile)) &&
        ListHasTile(d, 1, player->tileX, player->tileY)) {
        const MetaData* step = d->GetChild(0);
        if (!GameState::SubTaskCompleted((unsigned)step->GetChild(0)->GetInt(), (unsigned)step->GetChild(1)->GetInt()))
            Tasks::CompleteSubtaskDirect((uint32_t)step->GetChild(1)->GetInt(), 0x400);
    }
    if (visible && meta && player && (d = meta->FindChildData(kExpTeleportTile))) HandleTeleportTile(d);
    // UNVERIFIED (milestone 4g): traps. TRAP_DAMAGE hurts the entity standing on it once per its
    // delay while the trap is on (trapOn, not in a fight, not a reserve soldier): PARTICLE_TRAP,
    // TRAP_SOUND, "gethit" or death, the damage in a popup; collectStart is the last hit.
    // TRAP_PATTERN switches trapOn by the map time and fades its particles (or plays
    // TRAP_ANIMATED's two animations).
    if (DecorationVisibilityUpdateAllowed() && sprite) {
        // UNVERIFIED (milestone 4f): and Fog::IsVisible (0x173888) of the sprite's rectangle.
        sprite->visible = visible;
    }
    if (particles && particles->GetSprite() && particles->GetSprite()->visible != visible)
        for (Entity* p = particles; p; p = p->nextParticle)
            if (p->GetSprite()) p->GetSprite()->visible = visible;
    // (A debug overlay, off: the footprint as a bar with the id and depth, sprites +0xa4.)
}

// The part of Update that runs when the last use of the decoration ends.
void Decor::Finish() {
    BuildingHovers::HideArrowOnDecor(this);
    if (GameState::tutorial == 0x13) GameState::tutorial = 0x16;
    int wx = x, wy = y;
    TileCoordinatesToWorld(wx, wy);
    CompleteInteractSteps(this);
    if (meta) {
        const MetaData* d;
        // UNVERIFIED (milestone 5): a chest (CHEST_DROP / CHEST_DROP_TYPE) drops the contents of
        // the chest for the player's level, or shows them in ChestOpenedWindow, and takes its key.
        if (!(d = meta->FindChildData(kExpReqUnlockItem))) {
            if (Tasks::TaskInfo* t = Tasks::GetTask(GetStartQuestID())) {
                GameState::StartTask(t->id);
                Tasks::Update(true);
                BuildingHovers::Update(0.f, true);
                HUDWindow::UpdateTasks();
            }
        } else {
            uint32_t item = (uint32_t)d->GetInt();
            if (GameState::GetItemAmount(item, false) != 0) GameState::RemoveItem(item, 1);
            Items::ItemInfo* info = sprite ? Items::GetItemInfo(item) : nullptr;
            if (info)
                BuildingHovers::ShowIconHover(sprite->x + sprite->w * 0.5f, sprite->y + sprite->h * -0.5f,
                                              info->GetIcon());
        }
        if ((d = meta->FindChildData(kExpDrop))) GiveDrops(this, d, wx, wy, false);
        if ((d = meta->FindChildData(kExpDropAfterQuest))) {
            for (unsigned i = 0; i < d->GetChildrenCount(); ++i) {
                const MetaData* c = d->GetChild(i);
                if (!GameState::TaskCompleted((unsigned)c->GetChild(0)->GetInt())) continue;
                for (int n = 0; n < c->GetChild(2)->GetInt(); ++n)
                    BuildingHovers::DropItem((float)wx, (float)wy,
                                             Items::GetItemInfo((uint32_t)c->GetChild(1)->GetInt()), 0, 1);
            }
        }
        if ((d = meta->FindChildData(kExpParticleAfterInteract))) ParticlesFrom(this, d);
        if ((d = meta->FindChildData(kExpReplaceObj))) {
            uint32_t to = (uint32_t)d->GetInt();
            if (GameState::GetCurrentMapID() == 0 && GameData::GetBuilding(to)) {
                EnqueueBuildingPlacement(x, y, to);
            } else {
                id = to;
                data = GameData::GetDecoration(to);
                UpdateImage();
                Render::SortRenderLayer(8, 1);
            }
            RemoveBlockedTiles();
        }
        if (meta->FindChildData(kExpPortal)) TeleportAction(0);
        // UNVERIFIED (milestone 6): SOUND.
        bool shown = false;
        auto textAndTask = [](const MetaData* m) {
            return m->type == MetaData::kList && m->GetChildrenCount() > 1 &&
                   m->GetChild(0)->type == MetaData::kString && m->GetChild(1)->type == MetaData::kNumber;
        };
        if ((d = meta->FindChildData(kExpMsgBeforeQuest)) && textAndTask(d) &&
            !GameState::TaskCompleted((unsigned)d->GetChild(1)->GetInt())) {
            MessageHover(this, StringTable::GetString(d->GetChild(0)->GetString()));
            shown = true;
        }
        if ((d = meta->FindChildData(kExpMsgAfterQuest)) && textAndTask(d) && !shown &&
            GameState::TaskCompleted((unsigned)d->GetChild(1)->GetInt())) {
            MessageHover(this, StringTable::GetString(d->GetChild(0)->GetString()));
            shown = true;
        }
        if ((d = meta->FindChildData(kExpInteractMessage)) && d->type == MetaData::kString && !shown)
            MessageHover(this, StringTable::GetString(d->GetString()));
        // UNVERIFIED (milestone 4f): CAM_AFTER_INTERACT (Map::AddCameraCinematic 0x1b81a8 and an
        // AddCameraPoint 0x1c08cc per entry) and SHAKE_AFTER_INTERACT (AddCameraShake 0x1b7438).
        // UNVERIFIED (milestone 5): CRAFT: the profession's item drops (Professions::GenerateItem)
        // or "SEARCH_NOTHING", a profession point, the tool's wear ("DURABILITY_PENALTY").
    }
    TellInteract(this);
}

}  // namespace Map

// @0x150b50 Entity::IsDecorInRange
bool Entity::IsDecorInRange(const Map::Decor* d, int range) const { return d->IsEntityInRange(this, range); }
