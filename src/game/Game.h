// Game: the main loop's globals and exit path (libkingdom.so 5.11, free functions in Game.cpp).
#pragma once

extern bool done;          // 0x6123f8: the main loop ends
void MainExit();           // @0x184eec
// @0x188480: the "exit" answer of the back-key question. UNVERIFIED (online): with a profile that
// wants warnings, the player's city first goes to the server (LoadingInternetWindow, NET_SendSave);
// offline it is MainExit.
void ExitWithSendSave();
