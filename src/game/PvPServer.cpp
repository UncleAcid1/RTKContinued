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
#include "game/Rand48.h"
#include "game/SaveManager.h"
#include "game/Setting.h"
#include "game/SoldierSlots.h"
#include "game/StringTable.h"

namespace PvPServer {
namespace {

using SaveManager::PutChar;
using SaveManager::PutShort;
using SaveManager::PutUnsigned;

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

// For an equipment binding (internal numbering): one of the three best items on sale for its slot
// that the rival's level allows (not premium, not a customisation), or none.
const Items::ItemInfo* RivalItem(unsigned binding, int level) {
    const uint32_t slot = GameState::InternalItemBindingToExternal(binding);
    std::vector<const Items::ItemInfo*> fit;
    for (unsigned i = 0; const Items::ItemInfo* it = Items::EnumItemInfo(i); ++i)
        if (it->slot == slot && it->type != 0x11 && !it->premium && it->level <= (uint32_t)level && it->IsOnSale())
            fit.push_back(it);
    if (fit.empty()) return nullptr;
    std::stable_sort(fit.begin(), fit.end(), [](const Items::ItemInfo* a, const Items::ItemInfo* b) { return a->level > b->level; });
    return fit[Rand(std::min<unsigned>(3, (unsigned)fit.size()))];
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

std::vector<uint8_t> MakeRivalBlock(int level, bool male) {
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
        const Items::ItemInfo* it = (GameState::GetItemAt(b) || Rand(3) == 0) ? RivalItem(b, level) : nullptr;
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
    // Warriors like the player's squad (one fewer to one more, at most three: a PvP fight has
    // four a side with the hero); without a squad, now and then the ambush rival's warrior 0x2b.
    std::vector<const Entity*> squad;
    for (unsigned i = 0; i < SoldierSlots::GetOccupiedSlotCount(); ++i)
        if (const Entity* e = SoldierSlots::GetSoldierBySlot(i)) squad.push_back(e);
    const unsigned have = (unsigned)squad.size();
    const unsigned lo = have ? have - 1 : 0, hi = std::min(have + 1, 3u);
    const unsigned n = lo + Rand(hi - lo + 1);
    PutUnsigned(c, n);
    for (unsigned i = 0; i < n; ++i) {
        int id = 0x2b, hpMax = 0;
        if (have) {
            const Entity* e = squad[Rand(have)];
            id = (int)e->GetEntityData()->id;
            hpMax = e->GetHpMax();
        } else if (const EntityData* d = EntityFactory::GetEntityByID(id)) {
            hpMax = d->GetHpForLevel(d->level);
        }
        PutShort(c, (int16_t)id);
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
        const int level = std::max(1, playerLevel - 2 + (int)Rand(5));
        blocks.push_back(MakeRivalBlock(level, Rand(2) == 0));
    }
    for (const std::vector<uint8_t>& b : blocks) {
        PutUnsigned(out, 0xffffffff);   // UNVERIFIED: the two words before the size (not read)
        PutUnsigned(out, (uint32_t)b.size());
        PutUnsigned(out, (uint32_t)b.size());
        out.insert(out.end(), b.begin(), b.end());
    }
    return out;
}

}  // namespace PvPServer
