#include "game/Items.h"

#include <pugixml.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

#include "engine/FileManager.h"
#include "engine/IconManager.h"
#include "engine/Resources.h"
#include "engine/Timer.h"
#include "game/EntityData.h"
#include "game/GameData.h"
#include "game/GameState.h"
#include "game/MetaData.h"
#include "game/MetaExpression.h"
#include "game/Rand48.h"
#include "game/Setting.h"
#include "game/StringTable.h"
#include "game/Tags.h"
#include "gui/GUI.h"

namespace Items {
namespace {

std::vector<ItemInfo*> g_items;                       // 0x613580 every ItemInfo, in load order
std::unordered_map<uint32_t, ItemInfo*> g_byId;       // HashMap<unsigned, ItemInfo*, 1024> 0x61356c
uint32_t g_transactionTime = 0;                       // 0x613558 (SetTransactionTime: the server)
std::deque<std::string> g_names;                      // FUN_001ad790's string pool (0x613590)
std::deque<std::u32string> g_titles;                  // FUN_001ade84's wide string pool (0x61359c)

const char* InternName(const char* s) {   // @0x1ad790
    g_names.emplace_back(s);
    return g_names.back().c_str();
}

const char32_t* InternTitle(const std::u32string& s) {   // @0x1ade84
    g_titles.push_back(s);
    return g_titles.back().c_str();
}

// The quality word of item_level 1..4 ("item_class_<armor|weapon|ring>_<green|blue|orange|purple>",
// 0x601644) and its grammatical form per slot (0x5831e8, slots 1..10).
const char* const kClassNames[12] = {
    "item_class_armor_green",  "item_class_weapon_green",  "item_class_ring_green",
    "item_class_armor_blue",   "item_class_weapon_blue",   "item_class_ring_blue",
    "item_class_armor_orange", "item_class_weapon_orange", "item_class_ring_orange",
    "item_class_armor_purple", "item_class_weapon_purple", "item_class_ring_purple",
};
const int kSlotDeclination[10] = {0, 1, 0, 0, 1, 2, 2, 0, 3, 3};

ItemInfo* NewItem() {   // FUN_001ae8c0 + push_back
    ItemInfo* info = new ItemInfo;
    g_items.push_back(info);
    return info;
}

// Init / InitPacks reuse a known id's record: its expression and pack list go first.
void ResetItem(ItemInfo* info) {
    delete info->meta;
    delete info->packItems;
}

}  // namespace

int ItemInfo::GetDiscountSize(uint32_t t) const {
    if ((t & discountType) != 0 && discountFrom < g_transactionTime && g_transactionTime < discountTo)
        return 100 - (int)discount;
    return 100;
}

uint32_t ItemInfo::GetCostGold(bool full) const {
    if (full) return cost;
    if (cost == 0) return 0;
    uint32_t c = cost * (uint32_t)GetDiscountSize(1) / 100;
    return c ? c : 1;
}

uint32_t ItemInfo::GetCostCrystals(bool full) const {
    if (full) return cost2;
    if (cost2 == 0) return 0;
    uint32_t c = cost2 * (uint32_t)GetDiscountSize(2) / 100;
    return c ? c : 1;
}

uint32_t ItemInfo::GetCostItem(bool full) const {
    if (full) return costItem ? costItemCount : 0;
    if (costItem == 0 || costItemCount == 0) return 0;
    uint32_t c = costItemCount * (uint32_t)GetDiscountSize(4) / 100;
    return c ? c : 1;
}

bool ItemInfo::IsLimited() const { return meta && meta->FindByType(kExpLimitQuantity); }

bool ItemInfo::CanBeActivated() const {
    if (!meta) return false;
    return meta->FindByType(kExpOrbAttack) || meta->FindByType(kExpOrbDefense) ||
           meta->FindByType(kExpOrbCritical) || meta->FindByType(kExpOrbVampire);
}

int ItemInfo::GetItemCountLimit() const {
    if (meta)
        if (const MetaData* d = meta->FindChildData(kExpLimitQuantity)) return d->GetInt();
    return 10000;
}

int ItemInfo::GetAttackHealthReturn() const {
    if (meta)
        if (const MetaData* d = meta->FindChildData(kExpOrbVampire)) return d->GetChild(0)->GetInt();
    return 0;
}

int ItemInfo::GetCriticalChanceBonus() const {
    if (meta)
        if (const MetaData* d = meta->FindChildData(kExpOrbCritical)) return d->GetChild(0)->GetInt();
    return 0;
}

int ItemInfo::GetDamageAbsorption() const {
    if (meta)
        if (const MetaData* d = meta->FindChildData(kExpOrbDefense)) return d->GetChild(0)->GetInt();
    return 0;
}

int ItemInfo::GetDamageBoost() const {
    if (!meta) return 0;
    if (const MetaData* d = meta->FindChildData(kExpOrbAttack)) return d->GetChild(0)->GetInt();
    if (const MetaData* d = meta->FindChildData(kExpOrbVampire)) return d->GetChild(1)->GetInt();
    return 0;
}

// UNVERIFIED (milestone 5): Professions::GetProfession is not ported (no professions), so no item is
// a profession tool yet. The original: GATHER_CHANCE and GATHER_TIME, and the reputation_id
// profession's +0x24 is 0 or 1.
bool ItemInfo::IsProfessionTool() const { return false; }

const char32_t* ItemInfo::GetItemTypeName(uint32_t context) const {
    uint32_t s = slot == 0 ? 10 : GameState::ExternalItemBindingToInternal(slot);
    switch (type) {
    case 7: return StringTable::GetString("ITEM_TYPE_GEMS");
    case 6: return StringTable::GetString(context == 0x6b ? "REC_FOOD" : "ITEM_TYPE_MISC");
    case 3: return StringTable::GetString("ITEM_TYPE_ORB");
    case 4: return StringTable::GetString("ITEM_TYPE_POTION");
    case 5: return StringTable::GetString("ITEM_TYPE_SCROLL");
    case 1: return StringTable::GetString("ITEM_TYPE_QUEST");
    case 2: return StringTable::GetString("ITEM_TYPE_MISSION");
    case 9: return StringTable::GetString("ITEM_TYPE_MAP_OBJECTS");
    case 0xe: return StringTable::GetString("ITEM_TYPE_BUFFS");
    case 8:
        switch (s) {
        case 1: return StringTable::GetString("ITEM_TYPE_SWORD");
        case 6: return StringTable::GetString("ITEM_TYPE_SHIELD");
        case 4: case 9: return StringTable::GetString("ITEM_TYPE_RING");
        case 5: return StringTable::GetString("ITEM_TYPE_NECKLACE");
        case 0: return StringTable::GetString("ITEM_TYPE_HEAD");
        case 2: return StringTable::GetString("ITEM_TYPE_TORSO");
        case 3: return StringTable::GetString("ITEM_TYPE_LEGS");
        case 7: return StringTable::GetString("ITEM_TYPE_GLOWES");
        case 8: return StringTable::GetString("ITEM_TYPE_BELT");
        }
        break;
    }
    return U"";
}

const char* ItemInfo::GetItemTypeIcon(uint32_t context) const {
    uint32_t s = slot == 0 ? 10 : GameState::ExternalItemBindingToInternal(slot);
    switch (type) {
    case 7: return "tab_shop_gems";
    case 6: return context == 0x6b ? "16_food" : "tab_shop_misc";
    case 3: case 4: case 5: return "tab_inv_cons";
    case 1: case 2: case 9: return "tab_inv_quest";
    case 0xe: return "gold_16_lightning";
    case 8:
        if (s == 1) return "tab_shop_weapons";
        if (s == 6) return "tab_shop_shields";
        if (s == 4 || s == 5 || s == 9) return "tab_shop_accessories";
        return "tab_shop_armor";
    }
    return "";
}

Render::Texture* ItemInfo::GetBuffIcon(int size) const {
    char buf[256];   // 0x6135b8
    std::snprintf(buf, sizeof buf, "%s_%s", name, size < 0x14 ? "16" : "24");
    return IconManager::GetIcon(buf, false);
}

// UNVERIFIED: the original also sets +0x48 of the texture to 1 (see Background.cpp), here and in
// GetIcon / GetDropIcon.
void ItemInfo::PrepareItemImage() {
    if (icon) return;
    Render::Texture* t = nullptr;
    if (type == 0x81) {
        const GameData::BuildingData* b = GameData::GetBuilding(id - 0x4000);
        if (!b || b->parts.empty()) return;
        t = GameData::PartImage(&b->parts[0]);
    } else if (type == 0x82) {
        const GameData::DecorData* d = GameData::GetDecoration(id - 0x8000);
        if (!d) return;
        t = GameData::DecorImage(d);
    } else {
        return;
    }
    icon = t;
    iconMissing = false;
}

Render::Texture* ItemInfo::GetIcon() {
    if (iconMissing) return nullptr;
    if (icon) return icon;
    if (iconName && (icon = Resources::GetDecoration(iconName))) return icon;
    iconMissing = true;
    return nullptr;
}

void ItemInfo::PutImageInHolder(GUI::Window* w) {
    if (!GetIcon()) PrepareItemImage();
    Render::Texture* t;
    if (!GetIcon() && type == 0x11) {
        t = IconManager::GetIcon("customisation_60", false);
        iconMissing = false;
        icon = t;
    } else {
        t = icon;
    }
    if (t) GUI::FitImageIntoWindow(w, t, true, true);
}

Render::Texture* ItemInfo::GetDropIcon() {
    if (dropIconMissing) return nullptr;
    if (dropIcon) return dropIcon;
    if (iconName) {
        char buf[128];
        std::snprintf(buf, sizeof buf, "%s_drop", iconName);
        if ((dropIcon = Resources::GetDecoration(buf))) return dropIcon;
    }
    if (!(dropIcon = GetIcon())) dropIconMissing = true;
    return dropIcon;
}

int ItemInfo::ApplyLimitToAmount(int amount) const {
    int have = GameState::GetItemAmount(id, false);
    int limit = GetItemCountLimit();
    if (have >= limit) return 0;
    return have + amount > limit ? limit - have : amount;
}

// UNVERIFIED (milestone 5): Professions (GetProfession, GetPlayerProfessionLevel) are not ported;
// no item is locked by reputation yet.
bool ItemInfo::IsLockedByReputation(bool any) const { return false; }

bool ItemInfo::IsLockedByLevel() const { return (uint32_t)GameState::GetLevel() < level; }

bool ItemInfo::IsLocked() const { return IsLockedByLevel() || IsLockedByReputation(false); }

bool ItemInfo::IsOnSale() const {
    if (!sellable) return false;
    return !tag || tag->IsActive();
}

int ItemInfo::GetUseCount() const {
    if (!meta) return 0;
    const MetaData* d = meta->GetDataWithType(kExpOrbAttack);
    if (!d) d = meta->GetDataWithType(kExpOrbDefense);
    if (!d) d = meta->GetDataWithType(kExpOrbCritical);
    if (d) return d->GetChildrenCount() > 1 ? d->GetChild(1)->GetInt() : 1;
    if ((d = meta->GetDataWithType(kExpOrbVampire)))
        return d->GetChildrenCount() > 2 ? d->GetChild(2)->GetInt() : 2;
    return 0;
}

bool Init(const char* file) {
    double t0 = Timer::GetTime();
    uint32_t size = 0;
    uint8_t* buf = FileManager::LoadFile(file, size);
    if (!buf) {
        std::printf("Items::Init() Failed to load item list (File not found: %s)\n", file);
        return false;
    }
    pugi::xml_document doc;
    pugi::xml_parse_result r = doc.load_buffer_inplace(buf, size, 0x74);
    if (!r) {
        std::printf("Items::Init() Failed to load item list (%s)\n", r.description());
        FileManager::FreeFile(buf);
        return false;
    }
    if (!doc.child("Items")) {
        std::printf("Items::Init() Incorrect file format (%s)\n", file);
        FileManager::FreeFile(buf);
        return false;
    }
    if (g_items.capacity() < 0x80) g_items.reserve(0x80);
    for (pugi::xml_node n = doc.child("Items").child("Item"); n; n = n.next_sibling("Item")) {
        uint32_t id = n.attribute("item_id").as_uint();
        ItemInfo* s = GetItemInfo(id);
        if (!s) {
            s = NewItem();
            g_byId[id] = s;
        } else {
            ResetItem(s);
        }
        *s = ItemInfo();
        s->id = id;
        s->name = InternName(n.attribute("name").value());
        s->title = StringTable::GetString(s->name);
        if (std::strstr(n.attribute("icon").value(), ".png"))
            std::fprintf(stderr, "Item %d has an incorrect image name '%s'\n", s->id, n.attribute("icon").value());
        if (*n.attribute("icon").value()) s->iconName = InternName(n.attribute("icon").value());
        s->type = n.attribute("type").as_uint();
        s->shopTab = n.attribute("shop_tab").as_uint();
        s->slot = n.attribute("slot").as_uint();
        s->sellable = n.attribute("sellable").as_bool();
        s->premium = n.attribute("premium").as_bool();
        s->cost = n.attribute("cost").as_uint();
        s->cost2 = n.attribute("cost2").as_uint();
        const char* ci = n.attribute("cost_items").value();
        if (*ci) {
            s->costItem = (uint32_t)std::atoi(ci);
            if (const char* eq = std::strchr(ci, '=')) s->costItemCount = (uint32_t)std::atoi(eq + 1);
        }
        s->level = n.attribute("level").as_uint();
        s->reputationId = n.attribute("reputation_id").as_uint();
        s->reputationLevel = n.attribute("reputation_level").as_uint();
        s->successRate = n.attribute("success_rate").as_uint();
        s->successModifier = n.attribute("success_modifier").as_float();
        s->isCraft = n.attribute("is_craft").as_bool();
        s->activatedByQuest = n.attribute("activated_by_quest").as_uint();
        s->weight = n.attribute("weight").as_uint();
        s->itemLevel = n.attribute("item_level").as_uint();
        s->sortId = n.attribute("sort_id").as_uint();
        s->remains = n.attribute("remains").as_bool();
        const char* ll = n.attribute("level_limit").value();
        s->levelLimitMin = s->levelLimitMax = (int16_t)std::atoi(ll);
        if (const char* dash = std::strchr(ll, '-')) s->levelLimitMax = (int16_t)std::atoi(dash + 1);
        const char* bonus = n.attribute("bonus").value();
        if (std::strlen(bonus) > 1) s->meta = new MetaExpression(InternName(bonus), false);
        s->durability = n.attribute("durability").as_uint();
        s->deathPenalty = n.attribute("death_penalty").as_uint();
        s->tag = Tags::GetTag(n.attribute("tags").value());
        if (s->slot - 100 < 2) {
            std::fprintf(stderr, "ERROR: Items::Init() Item %d has incorrect slot number: %d\n", s->id, s->slot);
            s->slot = 0;
        }
        if (s->itemLevel >= 5) {
            if (s->itemLevel != 5)
                std::fprintf(stderr, "ERROR: Items::Init() Item %d has unsupported level: %d\n", s->id, s->itemLevel);
            s->itemLevel = 4;
        }
        // Equipment of item_level 1..4 is named "<quality> <name>", the quality word in the form
        // its slot needs.
        if (s->itemLevel != 0 && s->slot != 0 && s->type != 0x11) {
            int kind = s->slot == 3 ? 1 : (s->slot == 2 || s->slot == 9 || s->slot == 10) ? 2 : 0;
            std::u32string word = StringTable::GetDeclinationString(
                StringTable::GetString(kClassNames[(s->itemLevel - 1) * 3 + kind]), 0x40,
                kSlotDeclination[s->slot - 1]);
            s->title = InternTitle(SWPrintf(0x100, U"%s %s", {word.c_str(), s->title}));
        }
        // A "%s" in the name takes the GIVE_TROOPS unit's name; that unit's side0 image is the icon.
        if (s->meta) {
            const char32_t* pc = nullptr;
            if (s->title)
                pc = std::char_traits<char32_t>::find(s->title, std::char_traits<char32_t>::length(s->title), U'%');
            if (const MetaData* troops = s->meta->FindChildData(kExpGiveTroops)) {
                if (EntityData* e = EntityFactory::GetEntityByID(troops->GetInt())) {
                    if (pc && pc[1] == U's') s->title = InternTitle(SWPrintf(0x100, s->title, {e->title}));
                    s->icon = e->side0;
                }
            }
        }
    }
    FileManager::FreeFile(buf);
    std::printf("Items::Init() Loaded item list in %.3fms\n", (Timer::GetTime() - t0) * 1000.0);
    return true;
}

bool InitPacks(const char* file) {
    double t0 = Timer::GetTime();
    uint32_t size = 0;
    uint8_t* buf = FileManager::LoadFile(file, size);
    if (!buf) {
        std::printf("Items::InitPacks() Failed to load item pack list (File not found: %s)\n", file);
        return false;
    }
    pugi::xml_document doc;
    pugi::xml_parse_result r = doc.load_buffer_inplace(buf, size, 0x74);
    if (!r) {
        std::printf("Items::InitPacks() Failed to load item pack list (%s)\n", r.description());
        FileManager::FreeFile(buf);
        return false;
    }
    if (!doc.child("Packs")) {
        std::printf("Items::InitPacks() Incorrect file format (%s)\n", file);
        FileManager::FreeFile(buf);
        return false;
    }
    if (g_items.capacity() < 0x80) g_items.reserve(0x80);
    for (pugi::xml_node n = doc.child("Packs").child("Pack"); n; n = n.next_sibling("Pack")) {
        if (!n.attribute("active").as_bool()) continue;
        ItemInfo* s = GetItemPackInfo(n.attribute("id").as_uint());
        if (!s) s = NewItem();
        else ResetItem(s);
        *s = ItemInfo();
        s->id = 0x2000;
        s->name = InternName(n.attribute("name").value());
        s->title = StringTable::GetString(s->name);
        if (*n.attribute("icon").value()) s->iconName = InternName(n.attribute("icon").value());
        s->type = 0x80;
        s->sellable = true;
        s->premium = n.attribute("premium").as_bool();
        s->cost = n.attribute("cost").as_uint();
        s->cost2 = n.attribute("cost2").as_uint();
        s->level = n.attribute("level").as_uint();
        s->reputationId = n.attribute("reputation_id").as_uint();
        s->reputationLevel = n.attribute("reputation_level").as_uint();
        s->sortId = n.attribute("sort_id").as_uint();
        s->tag = Tags::GetTag(n.attribute("tags").value());
        s->packId = n.attribute("id").as_uint();
        s->packItems = new MetaData(MetaData::kList);
        for (pugi::xml_node i = n.child("Item"); i; i = i.next_sibling("Item")) {
            MetaData* pair = new MetaData(MetaData::kList);
            MetaData* itemId = new MetaData(MetaData::kNumber);
            itemId->intValue = (int)i.attribute("id").as_uint();   // (+0x18 only, as on the original)
            pair->AppendChild(itemId);
            MetaData* count = new MetaData(MetaData::kNumber);
            count->intValue = (int)i.attribute("count").as_uint();
            pair->AppendChild(count);
            s->packItems->AppendChild(pair);
        }
    }
    FileManager::FreeFile(buf);
    std::printf("Items::InitPacks() Loaded item pack list in %.3fms\n", (Timer::GetTime() - t0) * 1000.0);
    return true;
}

void InitSpecial() {
    Setting locks("gift_lvl_lock");
    for (unsigned i = 0; i < locks.GetChildrenCount(); ++i) {
        ItemInfo* s = GetItemInfo((uint32_t)locks.GetChild(i).GetChild(0).GetInt());
        if (s) s->giftLevel = (uint32_t)locks.GetChild(i).GetChild(1).GetInt();
    }
}

void Deinit() {
    for (ItemInfo* s : g_items) {
        delete s->meta;
        delete s->packItems;
        delete s;
    }
    g_items.clear();
    g_byId.clear();
    g_names.clear();
    g_titles.clear();
}

ItemInfo* GetItemInfo(uint32_t id) {
    auto it = g_byId.find(id);
    return it == g_byId.end() ? nullptr : it->second;
}

ItemInfo* GetItemInfo(const char* name) {
    for (ItemInfo* s : g_items)
        if (std::strcmp(s->name, name) == 0) return s;
    return nullptr;
}

ItemInfo* GetItemPackInfo(uint32_t packId) {
    for (ItemInfo* s : g_items)
        if (s->packId == packId) return s;
    return nullptr;
}

ItemInfo* GetItemPackWithItem(uint32_t id) {
    for (ItemInfo* s : g_items) {
        if (s->packId == 0 || !s->packItems) continue;
        for (unsigned i = 0; i < s->packItems->GetChildrenCount(); ++i)
            if ((uint32_t)s->packItems->GetChild(i)->GetChild(0)->GetInt() == id) return s;
    }
    return nullptr;
}

ItemInfo* EnumItemInfo(unsigned i) { return i < g_items.size() ? g_items[i] : nullptr; }

ItemInfo* GetRandomItemForSlot(uint32_t slot) {
    std::vector<ItemInfo*> found;
    for (ItemInfo* s : g_items)
        if (s->slot == slot && s->type != 0x11) found.push_back(s);
    if (found.empty()) return nullptr;
    return found[(uint32_t)Rand48::lrand48() % found.size()];
}

void AddDecorationAsItem(uint32_t decorId) {
    const GameData::DecorData* d = GameData::GetDecoration(decorId);
    if (!d) return;
    uint32_t id = d->id + 0x8000;
    ItemInfo* s = GetItemInfo(id);
    if (!s) {
        s = NewItem();
        g_byId[id] = s;
    }
    *s = ItemInfo();
    s->id = id;
    s->name = d->name.c_str();
    s->title = StringTable::GetString(s->name);
    s->type = 0x82;
    s->cost = (uint32_t)d->cost1;
    s->cost2 = (uint32_t)d->cost2;
    s->icon = s->dropIcon = d->image;
}

void AddBuildingAsItem(uint32_t buildingId) {
    const GameData::BuildingData* b = GameData::GetBuilding(buildingId);
    if (!b) return;
    uint32_t id = b->id + 0x4000;
    ItemInfo* s = GetItemInfo(id);
    if (!s) {
        s = NewItem();
        g_byId[id] = s;
    }
    *s = ItemInfo();
    s->id = id;
    s->name = b->name.c_str();
    s->title = StringTable::GetString(s->name);
    s->type = 0x81;
    s->icon = s->dropIcon = b->parts.empty() ? nullptr : b->parts[0].image;
    s->cost = b->gold;
    s->cost2 = b->cb;
}

uint32_t GetPlayersBestNet() {
    uint32_t best = 0;
    int bestChance = 0;
    for (unsigned i = 0; GameState::PlayerItem* it = GameState::EnumItems(i); ++i) {
        const ItemInfo* s = (const ItemInfo*)it->info;
        if (!s || !s->meta || s->meta->FindByType(kExpGatherTime)) continue;
        const MetaData* chance = s->meta->FindChildData(kExpGatherChance);
        if (!chance || chance->GetInt() <= bestChance) continue;
        best = s->id;
        bestChance = chance->GetInt();
    }
    return best;
}

int GetAmountToBoostTime(uint32_t id, uint32_t seconds) {
    ItemInfo* s = GetItemInfo(id);
    if (!s) return 1;
    int type = id == 0x263 ? kExpSpeedGoblin : id == 0x269 ? kExpSpeedTime : kExpSpeedBuild;
    if (s->meta)
        if (const MetaData* d = s->meta->FindChildData(type)) {
            uint32_t n = (seconds - 1 + (uint32_t)d->GetInt()) / (uint32_t)d->GetInt();
            if (n) return (int)n;
        }
    return 1;
}

}  // namespace Items
