// PlayerProfileManager: the player's profile (offline or Facebook) and its start-up state machine.
// Online, ported dormant (project decision 2026-10-07): the state machine (Start, ChangeStateTo, the
// Connect* / Check* / On* callbacks, Synchronize, ResetGame) runs on NeighboursServer, SocNets and
// CMyServer, which the port does not have (port_plan "online"); the port's start loads the local
// save directly (main_Loop_Init). Ported are the state, the flags the server's settings set and the
// queries, so the save keeps the player type and a later server can drive the rest.
#pragma once

#include <string>

namespace PlayerProfileManager {

// PORT: the switch the online features wait behind. Off, FBEnabled answers false (Facebook is never
// signed in), as every ported caller already assumes.
constexpr bool kOnline = false;

typedef void (*StateChangedCallback)(int state);
typedef void (*ActionCallback)(int action);

int GetCurrentState();                               // @0x3be31c
void ResetNoConnectionWarning();                     // @0x3be330
void SetCanPlayWithoutInternet(bool can);            // @0x3be348
bool CanPlayWithoutInternet();                       // @0x3bf308
bool CanShowOfflineButtons();                        // @0x3be35c
void SetCanShowOfflineButtons(bool can);             // @0x3be370
void SetPlayerType(unsigned type);                   // @0x3be388
unsigned GetPlayerType();                            // @0x3be39c
bool HandshakeFailed();                              // @0x3be3b0
void SetFbButtonState(bool enabled);                 // @0x3be3c4
void SetOfflineButtonState(bool enabled);            // @0x3be3d8
bool IsFbButtonEnabled();                            // @0x3be3ec
bool IsOfflineButtonEnabled();                       // @0x3be400
bool FBEnabled();                                    // @0x3be414
bool WarningsEnabled();                              // @0x3be458
bool OfflineModeByDefault();                         // @0x3be488
std::string GetLastPlayedProfile();                  // @0x3be4d8 "offline" / "fb"
bool HasFBSave();                                    // @0x3be508
bool HasOfflineSave();                               // @0x3be518
void AddStateChangedCallback(StateChangedCallback cb);   // @0x3be234 (one callback: replaces)
void AddActionFinishedCallback(ActionCallback cb);       // @0x3be248
void AddActionFailedCallback(ActionCallback cb);         // @0x3be25c

}  // namespace PlayerProfileManager
