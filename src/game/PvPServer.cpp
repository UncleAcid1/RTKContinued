#include "game/PvPServer.h"

#include <algorithm>
#include <cstring>
#include <set>
#include <string>

#include "engine/Timer.h"
#include "game/Entity.h"
#include "game/EntityData.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/PvP.h"
#include "game/Rand48.h"
#include "game/SaveManager.h"
#include "game/Setting.h"
#include "game/StringTable.h"
#include "game/Tavern.h"

namespace PvPServer {
namespace {

using SaveManager::PutChar;
using SaveManager::PutShort;
using SaveManager::PutUnsigned;

bool g_reportPending = false;
std::string g_report;

unsigned Rand(unsigned n) { return n ? (unsigned)((unsigned long)Rand48::lrand48() % n) : 0; }

void PutString(std::vector<uint8_t>& v, const std::u32string& s) {   // GameState's SaveString
    PutUnsigned(v, (uint32_t)s.size());
    for (char32_t c : s) PutUnsigned(v, (uint32_t)c);
}

void PutChunk(std::vector<uint8_t>& block, uint32_t type, const std::vector<uint8_t>& payload) {
    PutUnsigned(block, type);
    PutUnsigned(block, (uint32_t)payload.size());
    block.insert(block.end(), payload.begin(), payload.end());
}

// A first name and a surname from the workers' lists.
std::u32string RivalName(bool male) {
    const int female = male ? 0 : 1;
    std::u32string name = EntityFactory::GetNameByIdx(female, 0, EntityFactory::GetRandomNameIdx(female, 0));
    if (const char32_t* sur = EntityFactory::GetSurnameByIdx(female, 0, EntityFactory::GetRandomSurnameIdx(female, 0))) {
        if (*sur) name += U" ";
        name += sur;
    }
    return name;
}

// How hard a rival is (user's bug report 2026-10-26: rivals were all too strong, wore the best gear
// and copied the player's squad). Each rival rolls one: 25% easy, 55% fair, 20% hard.
enum Tier { kEasy, kFair, kHard };

Tier RollTier() {
    const unsigned r = Rand(100);
    return r < 25 ? kEasy : r < 80 ? kFair : kHard;
}

// For an equipment binding (internal numbering), measured against what the player wears there:
// easy, something weaker (half the time nothing); fair, the best item no newer than the player's
// (without one, now and then the plainest); hard, one of the two best the rival's level allows.
// Only items on sale, not premium, not customisations, at most the rival's level.
const Items::ItemInfo* RivalItem(unsigned binding, int level, Tier tier) {
    const uint32_t slot = GameState::InternalItemBindingToExternal(binding);
    std::vector<const Items::ItemInfo*> fit;
    for (unsigned i = 0; const Items::ItemInfo* it = Items::EnumItemInfo(i); ++i)
        if (it->slot == slot && it->type != 0x11 && !it->premium && it->level <= (uint32_t)level && it->IsOnSale())
            fit.push_back(it);
    if (fit.empty()) return nullptr;
    std::stable_sort(fit.begin(), fit.end(), [](const Items::ItemInfo* a, const Items::ItemInfo* b) { return a->level > b->level; });
    const GameState::PlayerItem* mine = GameState::GetItemAt(binding);
    const uint32_t mineLevel = mine && mine->info ? mine->info->level : 0;
    switch (tier) {
    case kHard:
        return fit[Rand(std::min<unsigned>(2, (unsigned)fit.size()))];
    case kFair:
        if (!mine) return Rand(3) == 0 ? fit.back() : nullptr;
        for (const Items::ItemInfo* it : fit)
            if (it->level <= mineLevel) return it;
        return fit.back();
    case kEasy:
    default:
        if (!mine || Rand(2) == 0) return nullptr;
        for (const Items::ItemInfo* it : fit)
            if (it->level < mineLevel) return it;
        return nullptr;
    }
}

// SelectHQWindow::OnRandomizeAppearance's rule: for each customisation binding 2..9 but 7, one
// of the customisation items on sale for it (not the other gender's) or, as one more equal
// chance, none; binding 5 never takes 0x2ef or 0x2f0.
void RivalLook(bool male, uint32_t customs[32]) {
    std::vector<const Items::ItemInfo*> all;
    for (unsigned i = 0; const Items::ItemInfo* it = Items::EnumItemInfo(i); ++i) {
        if (it->type != 0x11 || !it->IsOnSale()) continue;
        const bool m = std::strncmp(it->name, "male_", 5) == 0, f = std::strncmp(it->name, "female_", 7) == 0;
        if ((m && !male) || (f && male)) continue;
        all.push_back(it);
    }
    for (unsigned b = 2; b < 10; ++b) {
        if (b == 7) continue;
        std::vector<const Items::ItemInfo*> fit;
        for (const Items::ItemInfo* it : all)
            if (it->slot == b) fit.push_back(it);
        if (fit.empty()) continue;
        const unsigned r = Rand((unsigned)fit.size() + 1);
        if (r == fit.size() || (b == 5 && (fit[r]->id == 0x2ef || fit[r]->id == 0x2f0))) continue;
        customs[b] = fit[r]->id;
    }
}

}  // namespace

// One rival's game state block (the chunks GameState::Save writes for the player).
static std::vector<uint8_t> MakeRivalBlock(int level, bool male, Tier tier) {
    std::vector<uint8_t> block, c;
    PutUnsigned(c, male ? 1 : 0);
    PutString(c, RivalName(male));
    const char32_t* castle = StringTable::GetString("DEFAULT_CASTLE_NAMES");
    PutString(c, castle ? castle : U"");
    PutChunk(block, 0x10, c);
    // Profession 7's points as the player's own.
    c.clear();
    PutUnsigned(c, 1);
    PutUnsigned(c, 7);
    PutUnsigned(c, GameState::GetProfessionPoints(7));
    PutChunk(block, 0x41, c);
    c.clear();
    PutUnsigned(c, 0);
    PutChunk(block, 0x43, c);
    // 0x4f: the level; the gear (where the player wears something, else one time in three), whole;
    // no belt; the army; his own record (not read); the look.
    c.clear();
    PutUnsigned(c, (uint32_t)level);
    PutUnsigned(c, 10);
    for (unsigned b = 0; b < 10; ++b) {
        const Items::ItemInfo* it = RivalItem(b, level, tier);
        if (!it) {
            PutUnsigned(c, 0);
            continue;
        }
        PutUnsigned(c, b + 1);   // a unique id of his own
        PutUnsigned(c, it->id);
        PutUnsigned(c, it->durability);
        PutUnsigned(c, it->durability);
    }
    PutUnsigned(c, 0);
    // A random squad of different warriors (one of each, as the player's squad) from the tavern's
    // offers his level allows, goblins aside: easy 0-1, fair 1-2, hard 2-3 of them.
    std::vector<const Tavern::TavernInfo*> offers;
    for (unsigned i = 0; const Tavern::TavernInfo* t = Tavern::EnumTavernInfo(i); ++i) {
        if (t->type == 5 || t->level > (unsigned)level || !EntityFactory::GetEntityByID((int)t->entity)) continue;
        bool seen = false;
        for (const Tavern::TavernInfo* o : offers) seen = seen || o->entity == t->entity;
        if (!seen) offers.push_back(t);
    }
    unsigned n = (tier == kEasy ? 0 : tier == kFair ? 1 : 2) + Rand(2);
    n = std::min(n, (unsigned)offers.size());
    PutUnsigned(c, n);
    for (unsigned i = 0; i < n; ++i) {
        const unsigned k = Rand((unsigned)offers.size());
        const Tavern::TavernInfo* t = offers[k];
        offers.erase(offers.begin() + k);
        const int hpMax = t->GenerateHP();
        PutShort(c, (int16_t)t->entity);
        PutShort(c, (int16_t)hpMax);
        PutChar(c, 1);
        PutShort(c, 0);
        PutShort(c, (int16_t)hpMax);
    }
    PutUnsigned(c, 0);
    PutUnsigned(c, 0);
    uint32_t customs[32] = {};
    RivalLook(male, customs);
    PutUnsigned(c, 0x20);
    for (uint32_t id : customs) PutUnsigned(c, id);
    PutChunk(block, 0x4f, c);
    PutChunk(block, 0x13, {});
    return block;
}

std::vector<uint8_t> GetData(int type, int, const char*) {
    std::vector<uint8_t> out;
    out.push_back(0x1f);
    out.push_back('\r');
    PutUnsigned(out, (uint32_t)(int)GameState::GetSetting("pvp_refresh_time"));
    PutUnsigned(out, 0);   // DECIDED 2026-10-20: free
    const unsigned count = type >= 1 && type <= 4 ? (unsigned)(int)GameState::GetSetting("port_pvp_rival_count") : 0;
    PutUnsigned(out, count);
    const int playerLevel = GameState::GetLevel();
    const uint32_t now = (uint32_t)Timer::GetGlobalTime();
    std::vector<std::vector<uint8_t>> blocks;
    std::set<uint32_t> gids;
    for (unsigned i = 0; i < count; ++i) {
        uint32_t gid;
        do gid = 0x1000 + Rand(0x7fffe000); while (!gids.insert(gid).second);   // never 1 or -1..-4
        PutUnsigned(out, gid);
        PutUnsigned(out, now);   // never fought: his last fight at time 0
        PutUnsigned(out, 0);
        PutUnsigned(out, 0);
        PutUnsigned(out, 0);     // the player's losses against him
        PutUnsigned(out, 0);     // and wins
        PutChar(out, 0);
        // easy: 2 or 1 below the player, fair: one either side, hard: 1 to 3 above.
        const Tier tier = RollTier();
        const int offset = tier == kEasy ? -2 + (int)Rand(2) : tier == kFair ? -1 + (int)Rand(3) : 1 + (int)Rand(3);
        blocks.push_back(MakeRivalBlock(std::max(1, playerLevel + offset), Rand(2) == 0, tier));
    }
    for (const std::vector<uint8_t>& b : blocks) {
        // Two words ParsePvPData reads past without looking at them, then the size.
        PutUnsigned(out, 0xffffffff);
        PutUnsigned(out, (uint32_t)b.size());
        PutUnsigned(out, (uint32_t)b.size());
        out.insert(out.end(), b.begin(), b.end());
    }
    return out;
}

void ReportCombatState(int gid, int state) {
    g_report.clear();
    if (state == PvP::kStateWon)
        if (const PvP::PlayerInfo* p = PvP::GetPlayerInfoByGID(gid, 0xff)) g_report = std::to_string(p->valor);
    g_reportPending = true;
}

void Update() {
    if (!g_reportPending) return;
    g_reportPending = false;
    const std::string answer = g_report;
    PvP::ReportLoadFinished(true, answer.c_str(), (int)answer.size());
}

}  // namespace PvPServer
