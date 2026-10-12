// Map::Decor: the job on a decoration (its worker, price and action points) and what its
// expression answers. Port of the Map::Decor functions @0x130b58..0x13b99c that are not in Map.cpp.
#include "game/Map.h"
#include <algorithm>
#include <cstdlib>
#include "engine/Render.h"
#include "engine/Timer.h"
#include "game/AI.h"
#include "game/AIState.h"
#include "game/BuildingHovers.h"
#include "game/Entity.h"
#include "game/EntityManager.h"
#include "game/GameData.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/MetaData.h"
#include "game/MetaExpression.h"
#include "game/StringTable.h"

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
        // UNVERIFIED (milestone 4f): a decoration with a portal (+0x90) that is used up (f4c == 0)
        // stays open without the item; there are no portals yet.
        return true;
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

}  // namespace Map

// @0x150b50 Entity::IsDecorInRange
bool Entity::IsDecorInRange(const Map::Decor* d, int range) const { return d->IsEntityInRange(this, range); }
