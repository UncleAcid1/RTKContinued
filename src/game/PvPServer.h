// PvPServer: the offline stand-in for the PvP server (PORT, DECIDED 2026-10-18: arena / PvP is
// offline; opponents are generated rivals, ranks and valor kept locally, room left for a server).
// It answers CMyServer::NET_PvPGetData's request (pvp_saves.php) with the answer a server sent,
// in GameState::ParsePvPData's format, so the original code (PvP::PvPDataLoadFinished, the
// lists' cache, LoadPlayerInfo) runs unchanged; a real server could answer instead.
#pragma once
#include <cstdint>
#include <vector>

namespace PvPServer {

// A list of rivals of `type` (1..4) scaled to the player: names from the game's name lists, his
// level give or take two, gear for the bindings he wears (items on sale he could wear at the
// rival's level), warriors like his squad, a random look (SelectHQWindow's rule). Type 5 (one
// player looked up by uid, the social tabs: dormant) gets an empty list. DECIDED 2026-10-20: the
// answer's cost is 0, refreshing is free. The refresh delay is the setting "pvp_refresh_time".
std::vector<uint8_t> GetData(int type, int cost, const char* uid);

// One rival's game state block (the chunks GameState::Save writes for the player).
std::vector<uint8_t> MakeRivalBlock(int level, bool male);

}  // namespace PvPServer
