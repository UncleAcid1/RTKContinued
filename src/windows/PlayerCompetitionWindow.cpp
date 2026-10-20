// PlayerCompetitionWindow: the PvP opponents' window (Window_pvp.xml). Port of PlayerCompetitionWindow
// (libkingdom.so 5.11, @0x301e08..0x31ffc4). DECIDED 2026-10-20: only its PvP tab (tab 4) is ported;
// the activity, friends, gifts, top players and invite tabs stay dormant.
//
// IN PROGRESS (5t-5, see docs/5t5_notes.md): only the selection the fights read is here so far.
#include "game/PvP.h"
#include "windows/Windows.h"

namespace PlayerCompetitionWindow {
namespace {
int g_selectedGID = 0;            // 0x626368
bool g_selectionFetched = false;  // 0x62636c
uint32_t g_defeated = 0;          // 0x626370
bool g_returnWithoutWindow = false;
bool g_dataDownload = false;      // 0x626380 the opponent's data must be downloaded (online)
}  // namespace

int GetSelectedPlayerGID() { return g_selectedGID; }

// fetch: an opponent not in the list needs a download (online; the flag is never read offline).
void SetSelectedPlayerGID(int gid, bool fetch) {
    g_selectedGID = gid;
    if (fetch && !PvP::GetPlayerInfoByGID(gid)) g_dataDownload = true;
    g_selectionFetched = false;
}

void SetReturnWithoutWindow(bool on) { g_returnWithoutWindow = on; }
bool HasReturnWithoutWindow() { return g_returnWithoutWindow; }
void SetDefeatedPlayer(uint32_t spawn) { g_defeated = spawn; }
bool HasDefeatedPlayer(uint32_t spawn) { return g_defeated == spawn; }

}  // namespace PlayerCompetitionWindow
