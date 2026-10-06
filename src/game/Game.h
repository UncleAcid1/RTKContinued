// Game: the main loop's globals and exit path (libkingdom.so 5.11, free functions in Game.cpp).
#pragma once

extern bool done;          // 0x6123f8: the main loop ends
void MainExit();           // @0x184eec
// @0x188480: the "exit" answer of the back-key question. UNVERIFIED (online): with a profile that
// wants warnings, the player's city first goes to the server (LoadingInternetWindow, NET_SendSave);
// offline it is MainExit.
void ExitWithSendSave();
// @0x1885e0: the player's city from the main save: GameState::Load, the city's entities and map.
// Without a game state the original starts a new game (the hero on campaign map 0x15, the
// tutorial): UNVERIFIED (milestone 4), the port returns false and main boots its test city.
bool LoadSavedGame(bool listMaps);
// main_Loop_Init: the save file is "save.bin" (backup "save.binprev") unless a profile was played
// ("last_played_save": "save_<profile>.bin", when save_offline.bin or save_fb.bin exists).
void SetSaveNames();
// main_Exit_Func's save: outside the city tutorial the drops are collected and the game saved.
// UNVERIFIED (milestone 3e/4): the placement and movement modes and the windows it closes first.
void SaveOnExit();
