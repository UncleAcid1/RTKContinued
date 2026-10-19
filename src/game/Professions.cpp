#include "game/Professions.h"

#include <pugixml.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "engine/FileManager.h"
#include "engine/IconManager.h"
#include "engine/StlSort.h"
#include "engine/Timer.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/Locations.h"
#include "game/Map.h"
#include "game/MetaData.h"
#include "game/MetaExpression.h"
#include "game/Rand48.h"
#include "game/StringTable.h"
#include "gui/WindowManager.h"
#include "windows/Windows.h"

namespace Professions {
namespace {
std::vector<Profession*> g_professions;            // 0x613ed4 (kept for the whole run)
std::vector<std::unique_ptr<std::string>> g_names; // 0x613ee4 the names' string pool
std::vector<ProfessionLevel*> g_levels;            // 0x613f10 every level of every profession
std::vector<Items::ItemInfo*> g_candidates;        // 0x613f20 GenerateItem's items
uint32_t g_seed = 0;                               // 0x613f30 the gathering rolls (Init: the time)

// The rolls' generator (inlined in the original): the C runtime's linear congruential one.
uint32_t NextRand() {
    g_seed = g_seed * 0x343fd + 0x269ec3;
    return (g_seed & 0x7fffffff) >> 16;
}

int GetInt(const Items::ItemInfo* info, int t) {
    return info->meta->FindChildData(t)->GetInt();
}

// The gathering's success percent: the item's success rate and modifier, the tool's gather chance
// (or 0.7 without a tool) and the level's success rate. (The same sum in GetItemProbability and
// GenerateItem.)
int Chance(const Items::ItemInfo* item, float tool, const ProfessionLevel* level) {
    float fail = (1.f + (float)item->successRate * -0.01f) +
                 (tool + (1.f + (float)level->successRate * -0.01f)) * 0.5f;
    return (int)(0.5f + (1.f - fail * item->successModifier) * 100.f);
}
}  // namespace

// @0x1e9448
bool Init(const char* file) {
    double t0 = Timer::GetTime();
    uint32_t size = 0;
    uint8_t* buf = FileManager::LoadFile(file, size);
    if (!buf) {
        std::printf("Professions::Init() Failed to load profession list (File not found: %s)\n", file);
        return false;
    }
    pugi::xml_document doc;
    pugi::xml_parse_result r = doc.load_buffer_inplace(buf, size, 0x74);
    if (!r) {
        std::printf("Professions::Init() Failed to load profession list (%s)\n", r.description());
        FileManager::FreeFile(buf);
        return false;
    }
    if (!doc.child("Reputations")) {
        std::printf("Professions::Init() Incorrect file format (%s)\n", file);
        FileManager::FreeFile(buf);
        return false;
    }
    g_professions.reserve(0x10);   // (FUN_001e919c: the vector's grow)
    for (pugi::xml_node n = doc.child("Reputations").child("Reputation"); n; n = n.next_sibling("Reputation")) {
        uint32_t id = n.attribute("id").as_uint();
        Profession* p = GetProfession(id);
        if (!p) {
            g_professions.push_back(new Profession());
            p = g_professions.back();
        } else {
            delete p->unlockQuests;
            p->unlockQuests = nullptr;
            p->levels.clear();
            p->objects.clear();
        }
        p->id = id;
        g_names.push_back(std::make_unique<std::string>(n.attribute("name").value()));
        p->name = g_names.back()->c_str();
        p->title = StringTable::GetString(p->name);
        char key[0x40];
        std::snprintf(key, sizeof key, "%s_60", p->name);
        p->title60 = StringTable::GetString(key);
        std::snprintf(key, sizeof key, "%s_desc", p->name);
        p->desc = StringTable::GetString(key);
        p->action = StringTable::GetStringF("%s_action", p->name);
        p->smallIcon = p->icon = IconManager::GetIcon(n.attribute("icon").value(), true);
        p->iconLarge = IconManager::GetIcon(n.attribute("icon_large").value(), true);
        switch (p->id) {
        case 1: p->smallIcon = IconManager::GetIcon("gold_16_rep_bloodspiller", false); break;
        case 2: p->smallIcon = IconManager::GetIcon("gold_16_rep_shields", false); break;
        case 7: p->smallIcon = IconManager::GetIcon("pvp_valor_16", false); break;
        case 8: p->smallIcon = IconManager::GetIcon("rep_pvp_honour_16", false); break;
        case 6: p->smallIcon = IconManager::GetIcon("prof_tamer_16", false); break;
        }
        p->type = n.attribute("is_profession").as_bool() ? kProfession : kReputation;
        p->unlockLevel = n.attribute("unlock_level").as_uint();
        const char* quests = n.attribute("unlock_quest").value();
        p->unlockQuests = quests ? ParseCustomStyleData(quests, ",", "1") : nullptr;
        p->pointItem = n.attribute("item_id").as_uint();
        for (const char* v = n.attribute("object_id").value(); v && *v;) {
            p->objects.push_back((uint32_t)std::atoi(v));
            const char* comma = std::strchr(v, ',');
            if (!comma) break;
            v = comma + 1;
        }
        p->f44 = 0;
        p->tab = n.attribute("tab_id").as_uint();
        for (pugi::xml_node it = n.child("Levels").child("Item"); it; it = it.next_sibling("Item")) {
            g_levels.push_back(new ProfessionLevel());
            ProfessionLevel* l = g_levels.back();
            p->levels.push_back(l);
            l->id = it.attribute("id").as_uint();
            l->profession = p->id;
            l->name = StringTable::GetString(it.attribute("name").value());
            l->level = it.attribute("level").as_uint();
            l->successRate = it.attribute("success_rate").as_uint();
            l->points = it.attribute("points").as_uint();
        }
        // (FUN_001e9214 with its heap fallback FUN_001e90b0, FUN_001e93a8 and the insertion pass:
        // STLport's introsort.)
        StlSort::Sort(p->levels.data(), p->levels.data() + p->levels.size(), ProfessionLevelSort);
    }
    FileManager::FreeFile(buf);
    // (The original's message says "collection list".)
    std::printf("Professions::Init() Loaded collection list in %.3fms\n", (Timer::GetTime() - t0) * 1000.0);
    g_seed = (uint32_t)Timer::GetGlobalTime();
    return true;
}

// @0x1ea098: the professions' records go back to their pools.
void Deinit() {
    for (ProfessionLevel* l : g_levels) delete l;
    g_levels.clear();
    for (Profession* p : g_professions) {
        delete p->unlockQuests;
        delete p;
    }
    g_professions.clear();
    g_names.clear();
}

bool ProfessionLevelSort(const ProfessionLevel* a, const ProfessionLevel* b) { return a->level < b->level; }

Profession* GetProfession(uint32_t id) {
    for (Profession* p : g_professions)
        if (p->id == id) return p;
    return nullptr;
}

Profession* GetProfession(const char* name) {
    for (Profession* p : g_professions)
        if (!std::strcmp(p->name, name)) return p;
    return nullptr;
}

Profession* EnumProfession(unsigned i) { return i < g_professions.size() ? g_professions[i] : nullptr; }

Profession* GetProfessionForObject(uint32_t object) {
    for (Profession* p : g_professions)
        for (uint32_t o : p->objects)
            if (o == object) return p;
    return nullptr;
}

Profession* GetProfessionForTab(uint32_t tab, ProfessionType type) {
    for (Profession* p : g_professions)
        if (p->tab == tab && p->type == type) return p;
    return nullptr;
}

Profession* GetProfessionForPointItem(uint32_t item) {
    for (Profession* p : g_professions)
        if (p->pointItem == item) return p;
    return nullptr;
}

ProfessionLevel* GetProfessionLevel(uint32_t profession, uint32_t level) {
    Profession* p = GetProfession(profession);
    if (!p) return nullptr;
    for (ProfessionLevel* l : p->levels)
        if (l->level == level) return l;
    return nullptr;
}

// The highest level whose points are reached (0: none).
uint32_t GetProfessionLevelForPoints(uint32_t profession, uint32_t points) {
    Profession* p = GetProfession(profession);
    if (!p) return 0;
    uint32_t best = 0;
    for (ProfessionLevel* l : p->levels)
        if (l->points <= points && best < l->level) best = l->level;
    return best;
}

bool IsProfessionObject(uint32_t object) {
    if (object == 0) return false;
    return GetProfessionForObject(object) != nullptr;
}

// Read from the asm: per owned item two tests in a row, first the shortest gather time among the
// tools with both a gather chance and a gather time, then the highest gather chance among the
// tools with a gather chance (both unbroken, of this profession). The tool chosen in the character
// window (CharacterInfoWindow::GetToolOverride) wins over both. Without owned, the shortest (best)
// or longest gather time of all the profession's items.
uint32_t GetProfessionTool(uint32_t profession, bool owned, GameState::PlayerItem** item, bool best) {
    Profession* p = GetProfession(profession);
    if (!p) return 0;
    Items::ItemInfo* tool = nullptr;
    if (owned) {
        for (unsigned i = 0; GameState::PlayerItem* it = GameState::EnumItems(i); ++i) {
            Items::ItemInfo* info = it->info;
            if (!info || !info->meta) continue;
            if (info->meta->FindChildData(kExpGatherChance) && info->meta->FindChildData(kExpGatherTime) &&
                info->reputationId == p->id && !it->IsBroken() &&
                (!tool || GetInt(info, kExpGatherTime) < GetInt(tool, kExpGatherTime))) {
                tool = info;
                if (item) *item = it;
            }
            if (info->meta->FindChildData(kExpGatherChance) && info->reputationId == p->id && !it->IsBroken() &&
                (!tool || GetInt(info, kExpGatherChance) > GetInt(tool, kExpGatherChance))) {
                tool = info;
                if (item) *item = it;
            }
        }
        if (uint32_t chosen = CharacterInfoWindow::GetToolOverride(p->id)) {
            if (GameState::PlayerItem* it = GameState::GetFirstItemByID(chosen, false)) {
                if (item) *item = it;
                return it->info ? it->info->id : 0;
            }
        }
    } else {
        for (unsigned i = 0; Items::ItemInfo* info = Items::EnumItemInfo(i); ++i) {
            if (!info->meta || !info->meta->FindChildData(kExpGatherChance) ||
                !info->meta->FindChildData(kExpGatherTime) || info->reputationId != p->id)
                continue;
            int t = GetInt(info, kExpGatherTime);
            if (!tool || (best ? t < GetInt(tool, kExpGatherTime) : t > GetInt(tool, kExpGatherTime))) tool = info;
        }
    }
    return tool ? tool->id : 0;
}

bool IsProfessionTool(uint32_t item) {
    Items::ItemInfo* info = Items::GetItemInfo(item);
    return info && info->meta && info->meta->FindChildData(kExpGatherChance);
}

uint32_t GetPlayerProfessionLevel(uint32_t profession) {
    return GetProfessionLevelForPoints(profession, GameState::GetProfessionPoints(profession));
}

// A level reached shows the craft success window.
void AddProfessionPoint(uint32_t profession, uint32_t points) {
    uint32_t before = GetPlayerProfessionLevel(profession);
    GameState::AddProfessionPoints(profession, points);
    if (GetPlayerProfessionLevel(profession) <= before) return;
    CraftSuccessWindow::SetParameters(GetProfession(profession));
    WindowManager::EnqueueWindow(CraftSuccessWindow::Show, 0.25f);
}

int GetItemProbability(uint32_t profession, uint32_t item) {
    ProfessionLevel* level = GetProfessionLevel(profession, GetPlayerProfessionLevel(profession));
    if (!level) level = GetProfessionLevel(profession, 1);
    Items::ItemInfo* info = Items::GetItemInfo(item);
    if (!info) return 0;
    Items::ItemInfo* tool = Items::GetItemInfo(GetProfessionTool(profession, true, nullptr, false));
    float t = tool ? 1.f + (float)GetInt(tool, kExpGatherChance) * -0.01f : 0.7f;
    int chance = Chance(info, t, level);
    return chance > 0 ? chance : 0;
}

// An item of the profession found on the current map: one of the map's items the player's level
// allows, picked by the inverse of its success modifier, then rolled against its chance (only with
// a tool).
uint32_t GenerateItem(uint32_t profession) {
    uint32_t lvl = GetPlayerProfessionLevel(profession);
    g_candidates.clear();
    MetaData* contents = Locations::GetMapNodeContents(GameState::GetCurrentMapID());
    if (!contents) return 0;
    for (unsigned i = 0; Items::ItemInfo* info = Items::EnumItemInfo(i); ++i) {
        if (info->reputationId != profession || info->reputationLevel > lvl || !info->isCraft || !info->GetIcon())
            continue;
        bool onMap = false;
        for (unsigned k = 0; k < contents->GetChildrenCount() && !onMap; ++k)
            onMap = (int)info->id == contents->GetChild(k)->GetInt();
        if (!onMap) continue;
        if (info->successModifier < 0.01f) {
            std::fprintf(stderr, "ERROR: Professions::GenerateItem() item %d has an invalid fail chance of %f\n",
                         info->id, (double)info->successModifier);   // ErrorReporter::Printf
            continue;
        }
        g_candidates.push_back(info);
    }
    ProfessionLevel* level = GetProfessionLevel(profession, lvl);
    if (!level) level = GetProfessionLevel(profession, 1);
    if (g_candidates.empty()) return 0;
    float sum = 0.f;
    for (Items::ItemInfo* c : g_candidates) sum += 1.f / c->successModifier;
    float x = ((float)(Rand48::lrand48() % 1001) / 1000.f) * sum;
    Items::ItemInfo* picked = nullptr;
    for (Items::ItemInfo* c : g_candidates) {
        if (1.f / c->successModifier < x) {
            x += -1.f / c->successModifier;
            continue;
        }
        picked = c;
        break;
    }
    if (!picked) return 0;
    Items::ItemInfo* tool = Items::GetItemInfo(GetProfessionTool(profession, true, nullptr, false));
    if (!tool) return 0;
    int chance = Chance(picked, 1.f + (float)GetInt(tool, kExpGatherChance) * -0.01f, level);
    if (chance > 0 && (int)(NextRand() % 100) < chance) return picked->id;
    return 0;
}

bool Profession::IsUnlocked() const {
    if ((uint32_t)GameState::GetLevel() < unlockLevel) return false;
    if (!unlockQuests) return true;
    for (unsigned i = 0; i < unlockQuests->GetChildrenCount(); ++i)
        if (GameState::TaskCompleted((unsigned)unlockQuests->GetChild(i)->GetInt())) return true;
    return false;
}

// The gathering object of a profession of the type for the current map: one of the unlocked
// professions (at most 16) that one of the map's items (at the player's level) belongs to, with the
// map's quality as the percent chance; its object for the map's tileset.
uint32_t GenerateProfessionObject(ProfessionType type) {
    MetaData* contents = Locations::GetMapNodeContents(GameState::GetCurrentMapID());
    if (!contents || g_professions.empty()) return 0;
    Profession* found[0x10];
    unsigned count = 0;
    for (Profession* p : g_professions) {
        uint32_t lvl = GetPlayerProfessionLevel(p->id);
        if (p->type != type || !p->IsUnlocked() || count >= 0x10) continue;
        bool any = false;
        for (unsigned k = 0; k < contents->GetChildrenCount() && !any; ++k) {
            Items::ItemInfo* info = Items::GetItemInfo((uint32_t)contents->GetChild(k)->GetInt());
            if (info && info->reputationLevel <= lvl) any = info->reputationId == p->id;
        }
        if (any) found[count++] = p;
    }
    if (count == 0) return 0;
    if (NextRand() % 100 > Locations::GetMapNodeQuality(GameState::GetCurrentMapID())) return 0;
    uint32_t r = NextRand();
    uint32_t tileset = (uint32_t)Map::GetTileset();
    Profession* p = found[r % count];
    return tileset < p->objects.size() ? p->objects[tileset] : 0;
}

}  // namespace Professions
