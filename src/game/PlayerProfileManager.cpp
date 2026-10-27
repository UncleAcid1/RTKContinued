#include "game/PlayerProfileManager.h"

#include "engine/FileManager.h"
#include "engine/SystemFuncs.h"
#include "game/StringTable.h"

namespace PlayerProfileManager {
namespace {
int g_currentState = 0;                       // 0x630ce0 mCurrentState (ChangeStateTo: online)
int g_warningCount = 0;                       // 0x630cdc mWarningCount (ShowNoConnectionWarning: online)
bool g_handshakeFailed = false;               // 0x630cd8 mHandshakeFailed
unsigned g_playerType = 0;                    // 0x630ce8 mPlayerType (saved in chunk 0x68)
ActionCallback g_actionFinished = nullptr;    // 0x630cec mActionFinishedCallback
ActionCallback g_actionFailed = nullptr;      // 0x630cf0 mActionFailedCallback
StateChangedCallback g_stateChanged = nullptr;   // 0x630cf4 mStateChangedCallback
bool g_canShowOfflineButtons = true;          // 0x60f444 mCanShowOfflineButtons
bool g_canPlayWithoutInternet = true;         // 0x60f445 mCanPlayWithoutInternet
bool g_fbButtonEnabled = true;                // 0x60f446 mFbButtonEnabled
bool g_offlineButtonEnabled = true;           // 0x60f447 mOfflineButtonEnabled
}  // namespace

int GetCurrentState() { return g_currentState; }
void ResetNoConnectionWarning() { g_warningCount = 0; }
void SetCanPlayWithoutInternet(bool can) { g_canPlayWithoutInternet = can; }

// A Facebook player whose last profile was Facebook must stay online.
bool CanPlayWithoutInternet() {
    bool can = g_canPlayWithoutInternet;
    if (GetLastPlayedProfile() == "fb" && g_playerType == 1) can = false;
    return can;
}

bool CanShowOfflineButtons() { return g_canShowOfflineButtons; }
// As the original: the argument is ignored, the flag is set.
void SetCanShowOfflineButtons(bool) { g_canShowOfflineButtons = true; }
void SetPlayerType(unsigned type) { g_playerType = type; }
unsigned GetPlayerType() { return g_playerType; }
bool HandshakeFailed() { return g_handshakeFailed; }
void SetFbButtonState(bool enabled) { g_fbButtonEnabled = enabled; }
void SetOfflineButtonState(bool enabled) { g_offlineButtonEnabled = enabled; }
bool IsFbButtonEnabled() { return g_fbButtonEnabled; }
bool IsOfflineButtonEnabled() { return g_offlineButtonEnabled; }

// Facebook is on except for languages 7 and 8, where the server's "fb_enabled" decides.
bool FBEnabled() {
    if (!kOnline) return false;   // PORT: offline, never
    const int lang = StringTable::GetLangID();
    if (lang != 8 && lang != 7) return true;
    return SystemFuncs::GetSetting_Int("fb_enabled", 0) != 0;
}

bool WarningsEnabled() { return SystemFuncs::GetSetting_Int("warnings_enabled", 0, "MyCountry_flags") != 0; }

// Languages 7 and 8 start offline; the others unless the server shows the starting window.
bool OfflineModeByDefault() {
    const int lang = StringTable::GetLangID();
    if (lang == 8 || lang == 7) return true;
    return SystemFuncs::GetSetting_Int("show_starting_win", 0, "MyCountry_flags") == 0;
}

std::string GetLastPlayedProfile() { return SystemFuncs::GetSetting_String("last_played_save", ""); }
bool HasFBSave() { return FileManager::SaveExists("save_fb.bin"); }
bool HasOfflineSave() { return FileManager::SaveExists("save_offline.bin"); }
void AddStateChangedCallback(StateChangedCallback cb) { g_stateChanged = cb; }
void AddActionFinishedCallback(ActionCallback cb) { g_actionFinished = cb; }
void AddActionFailedCallback(ActionCallback cb) { g_actionFailed = cb; }

}  // namespace PlayerProfileManager
