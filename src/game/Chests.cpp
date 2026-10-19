#include "game/Chests.h"

#include <pugixml.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "engine/FileManager.h"
#include "engine/StlSort.h"
#include "engine/Timer.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/MetaData.h"
#include "game/Setting.h"
#include "game/StringTable.h"

namespace Chests {
namespace {
std::vector<ChestInfo*> g_chests;      // 0x611834 (kept for the whole run)
std::vector<ChestInfo*> g_candidates;  // 0x611840 the GetChestFor* / GetRandomChestByType picks
uint32_t g_seed = 0;                   // 0x61184c (Init: the time)

// The rolls' generator (inlined in the original): the C runtime's linear congruential one.
uint32_t NextRand() {
    g_seed = g_seed * 0x343fd + 0x269ec3;
    return (g_seed & 0x7fffffff) >> 16;
}

// "a" or "a-b" (atoi of each side; "a" gives a for both).
void ParseRange(const char* v, uint32_t& lo, uint32_t& hi) {
    lo = (uint32_t)std::atoi(v);
    const char* dash = std::strchr(v, '-');
    hi = dash ? (uint32_t)std::atoi(dash + 1) : lo;
}

ChestItem& ItemAt(ChestInfo* c, uint32_t sub) {
    if (c->items.size() <= sub) c->items.resize(sub + 1);   // (sgl::vector::grow @0x12a548)
    return c->items[sub];
}

// GetChestForLevel / GetChestForLevelByType's end: of the candidates only those of best's level
// stay (the original removes the others by moving the last one into their place); with ignoreKey
// a candidate whose key differs from best's is reported. One of them at random.
ChestInfo* PickOfLevel(const ChestInfo* best, bool ignoreKey, const char* error) {
    for (size_t i = 0; i < g_candidates.size();) {
        if (g_candidates[i]->level == best->level) {
            ++i;
        } else {
            g_candidates[i] = g_candidates.back();
            g_candidates.pop_back();
        }
    }
    if (ignoreKey)
        for (ChestInfo* c : g_candidates)
            if (c->requiredItem != best->requiredItem) std::fprintf(stderr, "%s\n", error);   // ErrorReporter::Printf
    return g_candidates[NextRand() % g_candidates.size()];
}

// The candidates' common checks: the chest's quest done and, unless ignoreKey, its key owned.
bool Available(const ChestInfo* c, bool ignoreKey) {
    if (c->requiredQuest && !GameState::TaskCompleted(c->requiredQuest)) return false;
    if (!ignoreKey && c->requiredItem && GameState::GetItemAmount(c->requiredItem, false) == 0) return false;
    return true;
}

// The nearer of best and c to level (best on a tie).
const ChestInfo* Nearer(const ChestInfo* best, const ChestInfo* c, uint32_t level) {
    if (!best) return c;
    return std::abs((int)level - (int)c->level) >= std::abs((int)level - (int)best->level) ? best : c;
}
}  // namespace

bool operator<(const ChestItem& a, const ChestItem& b) { return b.probability < a.probability; }

// @0x12a478
bool ChestInfo::SoldForCrystals() const {
    if (cents != 0) return false;
    return crystals != 0;
}

// @0x12a648
unsigned ChestInfo::GenerateContents(DropItem* out, unsigned max, bool all) {
    unsigned n = 0;
    if (resources) {
        for (unsigned i = 0; i < resources->GetChildrenCount(); ++i) {
            const MetaData* r = resources->GetChild(i);
            if (n >= max) continue;
            if (r->GetChild(0)->type == MetaData::kString) {
                int amount = r->GetChild(1)->GetInt();
                out[n++] = {(uint32_t)GameState::StringToResourceType(r->GetChild(0)->GetString()), 0, 0, amount,
                            (uint32_t)amount, (uint32_t)amount, 10000};
            } else if (r->GetChild(0)->type == MetaData::kNumber) {
                uint32_t id = (uint32_t)r->GetChild(0)->GetInt();
                int amount = r->GetChild(1)->GetInt();
                int given = amount;
                Items::ItemInfo* info = Items::GetItemInfo(id);
                if (info && info->IsLimited() && (given = info->ApplyLimitToAmount(amount)) == 0) continue;
                out[n++] = {0xb, id, 0, given, (uint32_t)amount, (uint32_t)amount, 10000};
            }
        }
    }
    unsigned count = (unsigned)items.size();
    if (count == 0) return n;
    for (ChestItem& it : items) it.picked = false;
    unsigned drops = countMin;
    if (countMin != countMax) drops = countMin + NextRand() % (countMax + 1 - countMin);
    // all: every item once, in turn, whatever its probability; else "drops" items picked at random,
    // each kept by its probability. (As the original, the loop only ends when that many were given:
    // a max below them would never end; its callers pass enough room.)
    unsigned left = all ? count : drops;
    if (!all && drops == 0) return n;
    for (unsigned turn = 0;; ++turn) {
        count = (unsigned)items.size();
        unsigned idx = all ? turn % count : NextRand() % count;
        // Once every item had its turn, they all may come again.
        unsigned picked = 0;
        for (const ChestItem& it : items)
            if (it.picked) ++picked;
        if (picked == count)
            for (ChestItem& it : items) it.picked = false;
        ChestItem& e = items[idx];
        if (!e.picked) {
            e.picked = true;
            if ((all || (int)(NextRand() % 10000) < e.probability) && max > n) {
                const ChestItem& c = items[idx % items.size()];
                uint32_t item = c.item, object = 0;
                if (item > 0x8000) {
                    object = item - 0x8000;
                    item = 0;
                }
                // (As the original: the highest amount itself is never rolled.)
                int amount = (int)c.amountMin;
                if (c.amountMax > c.amountMin) amount = (int)(c.amountMin + NextRand() % (c.amountMax - c.amountMin));
                Items::ItemInfo* info = Items::GetItemInfo(item);
                if (!info || !info->IsLimited() || (amount = info->ApplyLimitToAmount(amount)) != 0) {
                    --left;
                    out[n++] = {0xb, item, object, amount, c.amountMin, c.amountMax, c.probability};
                    if (left == 0) return n;
                    continue;
                }
            }
        }
        if (left == 0) return n;
    }
}

// @0x12be18
bool Init(const char* file) {
    double t0 = Timer::GetTime();
    uint32_t size = 0;
    uint8_t* buf = FileManager::LoadFile(file, size);
    if (!buf) {
        std::printf("Chests::Init() Failed to load chest list (File not found: %s)\n", file);
        return false;
    }
    pugi::xml_document doc;
    pugi::xml_parse_result r = doc.load_buffer_inplace(buf, size, 0x74);
    if (!r) {
        std::printf("Chests::Init() Failed to load chest list (%s)\n", r.description());
        FileManager::FreeFile(buf);
        return false;
    }
    if (!doc.child("Chests")) {
        std::printf("Chests::Init() Incorrect file format (%s)\n", file);
        FileManager::FreeFile(buf);
        return false;
    }
    if (g_chests.capacity() < 0x40) g_chests.reserve(0x40);
    for (pugi::xml_node n = doc.child("Chests").child("Chest"); n; n = n.next_sibling("Chest")) {
        uint32_t id = n.attribute("id").as_uint();
        ChestInfo* c = GetChestInfo(id);
        if (!c) {
            g_chests.push_back(new ChestInfo());
            c = g_chests.back();
        } else {
            delete c->resources;
            c->items.clear();
            c->resources = nullptr;
            delete[] c->sku;
            c->sku = nullptr;
        }
        c->id = id;
        c->active = n.attribute("active").as_bool();
        c->type = n.attribute("type").as_uint();
        c->name = StringTable::GetString(n.attribute("name").value());
        c->desc = StringTable::GetStringF("%s_desc", n.attribute("name").value());
        c->requiredItem = n.attribute("item_id").as_uint();
        c->requiredQuest = n.attribute("task_id").as_uint();
        ParseRange(n.attribute("items_count").value(), c->countMin, c->countMax);
        c->level = n.attribute("level").as_uint();
        c->rewardWindow = n.attribute("reward_window").as_bool();
        c->rerollPrice = n.attribute("reroll_price").as_uint();
        const char* res = n.attribute("resources").value();
        c->resources = *res ? ParseCustomStyleData(res, ",=", "11") : nullptr;
        for (pugi::xml_node it = n.child("Items").child("Item"); it; it = it.next_sibling("Item")) {
            c->items.emplace_back();
            ChestItem& e = c->items.back();
            e.id = it.attribute("id").as_uint();
            e.item = it.attribute("item_id").as_uint();
            ParseRange(it.attribute("amount").value(), e.amountMin, e.amountMax);
            e.probability = (int)(it.attribute("probability").as_float() * 100.f);
            if (e.probability == 0) {
                std::fprintf(stderr, "ERROR: Chests::Init() Chest %d item %d (%d) has incorrect probability of 0%%\n",
                             c->id, e.id, e.item);   // ErrorReporter::Printf
                e.probability = 1;
            }
        }
        for (pugi::xml_node it = n.child("Objects").child("Object"); it; it = it.next_sibling("Object")) {
            c->items.emplace_back();
            ChestItem& e = c->items.back();
            e.id = it.attribute("id").as_uint();
            e.item = it.attribute("obj_type").as_uint() + 0x8000;
            ParseRange(it.attribute("amount").value(), e.amountMin, e.amountMax);
            e.probability = (int)(it.attribute("probability").as_float() * 100.f);
            if (e.probability == 0) {
                std::fprintf(stderr,
                             "ERROR: Chests::Init() Chest %d object %d (%d) has incorrect probability of 0%%\n", c->id,
                             e.id, e.item);   // ErrorReporter::Printf
                e.probability = 1;
            }
        }
        if (c->countMin > c->countMax) {
            std::fprintf(stderr, "ERROR: Chests::Init() Chest %d item count minumum is larger than maximum! (%d-%d)\n",
                         c->id, c->countMin, c->countMax);   // ErrorReporter::Printf
            c->countMin = c->countMax;
        }
        if (c->countMax > c->items.size()) {
            std::fprintf(stderr, "ERROR: Chests::Init() Chest %d drops %d items, but has only %d items in the list\n",
                         c->id, c->countMax, (unsigned)c->items.size());   // ErrorReporter::Printf
            c->countMax = (uint32_t)c->items.size();
            if (c->countMin > c->countMax) c->countMin = c->countMax;
        }
        c->sku = nullptr;
        c->cents = 0;
        c->crystals = n.attribute("cb_price").as_uint();
        c->chestItem = 0;
        // (std::sort's introsort with its insertion pass, as STLport.)
        StlSort::Sort(c->items.data(), c->items.data() + c->items.size(),
                      [](const ChestItem& a, const ChestItem& b) { return a < b; });
    }
    FileManager::FreeFile(buf);
    std::printf("Chests::Init() Loaded chest list in %.3fms\n", (Timer::GetTime() - t0) * 1000.0);
    g_seed = (uint32_t)Timer::GetGlobalTime();
    // The store's products.
    ChangeChestAttribute(0x1b, 0, "sku", "com.gameinsight.kingdom.chest_architect");
    ChangeChestAttribute(0x1c, 0, "sku", "com.gameinsight.kingdom.chest_warrior");
    ChangeChestAttribute(0x1d, 0, "sku", "com.gameinsight.kingdom.chest_hunter");
    ChangeChestAttribute(0x1e, 0, "sku", "com.gameinsight.kingdom.chest_collector");
    ChangeChestAttribute(0x35, 0, "sku", "com.gameinsight.kingdom.chest_start_a");
    ChangeChestAttribute(0x3f, 0, "sku", "com.gameinsight.kingdom.chest_start_b");
    ChangeChestAttribute(0x51, 0, "sku", "com.gameinsight.kingdom.chest_imperial_1");
    ChangeChestAttribute(0x52, 0, "sku", "com.gameinsight.kingdom.chest_imperial_2");
    ChangeChestAttribute(0x53, 0, "sku", "com.gameinsight.kingdom.chest_imperial_3");
    ChangeChestAttribute(0x57, 0, "sku", "com.gameinsight.kingdom.chest_imperial_1_c");
    ChangeChestAttribute(0x58, 0, "sku", "com.gameinsight.kingdom.chest_imperial_2_c");
    ChangeChestAttribute(0x59, 0, "sku", "com.gameinsight.kingdom.chest_imperial_3_c");
    // PORT (no real money, user 2026-10-19): the original gives the first nine their price in
    // cents here (0x1b "299", 0x1c "1499", 0x1d "699", 0x1e "499", 0x35 "699", 0x3f "499",
    // 0x51..0x53 "999"), which makes them real-money purchases; without it they are sold for their
    // crystals ("cb_price") as the rest.
    static const struct { uint32_t chest, item; } kChestItems[] = {
        {0x1b, 0x2f4}, {0x1c, 0x2f5}, {0x1d, 0x2f6}, {0x1e, 0x2f7}, {0x35, 0x2b6}, {0x3f, 0x2b6},
        {0x51, 0x2fb}, {0x52, 0x2fb}, {0x53, 0x2fb}, {0x57, 0x2fb}, {0x58, 0x2fb}, {0x59, 0x2fb}};
    for (const auto& ci : kChestItems)
        if (ChestInfo* c = GetChestInfo(ci.chest)) c->chestItem = ci.item;
    return true;
}

// @0x12b89c
void Deinit() {
    for (ChestInfo* c : g_chests) {
        delete c->resources;
        delete[] c->sku;
        delete c;
    }
    g_chests.clear();
}

// @0x12b404
ChestInfo* GetChestInfo(uint32_t id) {
    for (ChestInfo* c : g_chests)
        if (c->id == id) return c;
    return nullptr;
}

// @0x12b270
uint32_t GetChestForItem(uint32_t item) {
    if (item == 0x2fb)   // (the imperial chests: six share it; a data check of the original)
        std::fprintf(stderr, "ERROR: Chests::GetChestForItem() There are multiple chests for item %d\n",
                     item);   // ErrorReporter::Printf
    for (ChestInfo* c : g_chests) {
        // Of the two starter chests the one of "starter_chest_b".
        if ((c->id == 0x3f || c->id == 0x35) && c->id != (Setting("starter_chest_b").GetInt() ? 0x3fu : 0x35u))
            continue;
        if (c->chestItem == item) return c->id;
    }
    return 0;
}

// @0x12b3a4
bool IsChestItem(uint32_t item) {
    for (ChestInfo* c : g_chests)
        if (c->chestItem == item) return true;
    return false;
}

// @0x12b830
uint32_t GetItemForChest(uint32_t id) {
    ChestInfo* c = GetChestInfo(id);
    return c ? c->chestItem : 0;
}

// @0x12b46c
void ChangeChestAttribute(uint32_t id, uint32_t sub, const char* name, const char* value) {
    ChestInfo* c = GetChestInfo(id);
    if (!c || sub >= 0x20) return;
    uint32_t v = (uint32_t)std::atoi(value);
    if (!std::strcmp(name, "type")) {
        c->type = v;
    } else if (!std::strcmp(name, "req_item")) {
        c->requiredItem = v;
    } else if (!std::strcmp(name, "req_quest")) {
        c->requiredQuest = v;
    } else if (!std::strcmp(name, "count")) {
        if (v > 0x1f) v = 0x20;
        c->countMin = c->countMax = v;
    } else if (!std::strcmp(name, "level")) {
        c->level = v;
    } else if (!std::strcmp(name, "resources")) {
        delete c->resources;
        const char* p = value;
        c->resources = ParseCustomStyleData(p, ",=", "11");
    } else if (!std::strcmp(name, "item_id")) {
        ItemAt(c, sub).item = v;
    } else if (!std::strcmp(name, "item_min")) {
        ItemAt(c, sub).amountMin = v;
    } else if (!std::strcmp(name, "item_max")) {
        ItemAt(c, sub).amountMax = v;
    } else if (!std::strcmp(name, "item_prob")) {
        ItemAt(c, sub).probability = (int)v;
    } else if (!std::strcmp(name, "active")) {
        c->active = v != 0;
    } else if (!std::strcmp(name, "sku")) {
        delete[] c->sku;
        c->sku = new char[std::strlen(value) + 1];
        std::strcpy(c->sku, value);
    } else if (!std::strcmp(name, "cents")) {
        c->cents = v;
    } else if (!std::strcmp(name, "chest_item")) {
        c->chestItem = v;
    } else if (!std::strcmp(name, "reroll")) {
        c->rerollPrice = v;
    } else if (!std::strcmp(name, "crystals")) {
        c->crystals = v;
    }
}

// @0x12b974
ChestInfo* GetChestForLevel(const MetaData* ids, uint32_t level, bool ignoreKey) {
    if (!ids || ids->type != MetaData::kList) return nullptr;
    g_candidates.clear();
    const ChestInfo* best = nullptr;
    for (ChestInfo* c : g_chests) {
        if (!c->active) continue;
        bool listed = false;
        for (unsigned i = 0; i < ids->GetChildrenCount() && !listed; ++i) listed = (int)c->id == ids->GetChild(i)->GetInt();
        if (!listed || !Available(c, ignoreKey)) continue;
        g_candidates.push_back(c);
        best = Nearer(best, c, level);
    }
    if (!best) return nullptr;
    return PickOfLevel(best, ignoreKey,
                       "ERROR: Chests::GetChestForLevel() Multiple chests are available with different item "
                       "requirements");
}

// @0x12cfb0
ChestInfo* GetRandomChestByType(ChestType type) {
    g_candidates.clear();
    for (ChestInfo* c : g_chests)
        if (c->active && c->type == type && Available(c, false)) g_candidates.push_back(c);
    if (g_candidates.empty()) return nullptr;
    return g_candidates[NextRand() % g_candidates.size()];
}

// @0x12d29c
ChestInfo* GetChestForLevelByType(ChestType type, uint32_t level, bool ignoreKey) {
    g_candidates.clear();
    const ChestInfo* best = nullptr;
    for (ChestInfo* c : g_chests) {
        if (!c->active || c->type != type || !Available(c, ignoreKey)) continue;
        g_candidates.push_back(c);
        best = Nearer(best, c, level);
    }
    if (!best) return nullptr;
    return PickOfLevel(best, ignoreKey,
                       "ERROR: Chests::GetChestForLevelByType() Multiple chests are available with different item "
                       "requirements");
}

}  // namespace Chests
