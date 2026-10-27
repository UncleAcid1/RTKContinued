// PvPServer: the offline stand-in for the PvP server (PORT, DECIDED 2026-10-18: arena / PvP is
// offline; opponents are generated rivals, ranks and valor kept locally, room left for a server).
// It answers CMyServer::NET_PvPGetData's request (pvp_saves.php) with the answer a server sent,
// in GameState::ParsePvPData's format, so the original code (PvP::PvPDataLoadFinished, the
// lists' cache, LoadPlayerInfo) runs unchanged; a real server could answer instead.
#pragma once
#include <cstdint>
#include <vector>

namespace PvPServer {

// A list of rivals of `type` (1..4) scaled to the player: names from the game's name lists; each
// rolls easy / fair / hard, which sets his level around the player's, his gear against the
// player's own and a random squad of tavern warriors; a random look (SelectHQWindow's rule). Type 5 (one
// player looked up by uid, the social tabs: dormant) gets an empty list. DECIDED 2026-10-20: the
// answer's cost is 0, refreshing is free. The refresh delay is the setting "pvp_refresh_time".
std::vector<uint8_t> GetData(int type, int cost, const char* uid);

// NET_PvPReportCombatState's answer, delivered by Update: for a win, the opponent's valor
// (PlayerInfo +0x424, worked out by ReportStateToServer: kept locally, DECIDED 2026-10-18); for the
// other states an empty answer.
void ReportCombatState(int gid, int state);
// Each frame (Tick): a pending answer goes to PvP::ReportLoadFinished.
void Update();


}  // namespace PvPServer
