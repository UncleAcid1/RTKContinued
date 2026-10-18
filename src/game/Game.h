// Game: the main loop's globals and exit path (libkingdom.so 5.11, free functions in Game.cpp).
#pragma once

#include <string>

extern bool done;          // 0x6123f8: the main loop ends
extern bool gameLoaded;    // 0x6123f6: main_Loop_Init finished (RepeatedMain)
// 0x6123ff: leave without saving (the save was just written or replaced). 0x6123fb: start again
// after main_Exit_Func (SDL_main's loop; PORT: main starts the program again).
extern bool forceExitNoSave, restartGameAfterExit;
extern bool canPlayMusic;  // 0x6123f5: set once the sound system is up
extern std::string gMusicName;   // the music turnMusic starts ("music_battle_map")
// The language (0 EN, 1 RU, 2 DE, 3 LV, 4 ES, 5 IT, 6 FR, 7 KO, 8 JA): the one in use
// (0x6124a0), the system's when the game has it (0x61249c, Java's mcSetAvailableLanguage) and
// whether the player has chosen one (0x6124a4, Java's mcSetLanguage).
extern unsigned languageID, availableLanguageID;
extern bool languageHasBeenSelected;
// 0x6123f9: the app is in the background (Android's SDLMain.mcPauseEvent; the port: SDL's
// background/foreground events). main_Loop_Func then only sleeps and counts the time paused.
extern bool paused;
// 0x6123fc: Android's render surface is suspended (blocking frame loops stop). PORT: never set.
extern bool renderSuspendEvent;
extern float pauseTime;    // 0x61236c: seconds paused, given to Map::OfflineUpdate on resume
void MainExit();           // @0x184eec
void MainExitNoSave();     // @0x184f04: leave without saving and start again
inline void OnShowRestartGame() {}   // @0x184ca0 (empty in 5.11)
// @0x185668: the app is in front again (after loading, a reload, the render restore): the
// feature holders wake up and the daily bonus window may open.
void OnResume();
void turnMusic(bool on);   // @0x187674
// @0x187060 / @0x186fe0: the answers of the "your language is available" question
// (main_Loop_Func): take the system's language (saved, then the game starts again) or keep the
// current one; either way the question is not asked again.
void LanguageSelect();
void LanguageDecline();
// main_Loop_Func's question itself (a part of 0x188a40): asked once the city tutorial is over, no
// window is up and the system's language is not the one in use.
void AskLanguageQuestion();

namespace FileManager {
void SetPreferredLanguade(unsigned id);   // @0x16aeb0 "selected_language" in "MyCountry_flags"
void RestartGame();                       // @0x16b0a4
}

namespace Game {
inline void splashLoop() {}               // @0x184f3c (empty in 5.11: no splash screens)
// @0x18c080: one string table from res_files/1Original (and, online, from each active content
// pack's folder). `letter` is the loading screen's debug letter and `progress` its bar (the
// loading screen is milestone 7).
bool LoadLanguageTable(const char* file, const char* letter, double progress);
// @0x18cc4c: one data file through its loader, the same way; a pack's copy of the file is loaded
// on top. failStep / okStep are the load-sampling steps reported (analytics, not ported).
bool LoadSystemConfiguration(bool (*loader)(const char*), const char* file, const char* letter, const char* name,
                             double progress, unsigned failStep, unsigned okStep);
bool LoadEntityFactoryData(const char* file);   // @0x18564c
}
// @0x188480: the "exit" answer of the back-key question. UNVERIFIED (online): with a profile that
// wants warnings, the player's city first goes to the server (LoadingInternetWindow, NET_SendSave);
// offline it is MainExit.
void ExitWithSendSave();
// @0x1885e0: the player's city from the main save: GameState::Load, the city's entities and map.
// Without a game state a new game starts (the hero and Stoutbeard on campaign map 0x15, the
// tutorial) and it returns false.
bool LoadSavedGame(bool listMaps);
// main_Loop_Init: the save file is "save.bin" (backup "save.binprev") unless a profile was played
// ("last_played_save": "save_<profile>.bin", when save_offline.bin or save_fb.bin exists).
void SetSaveNames();
// @0x185964 (the game's Update, once per second of global time): each tag whose IsActive changed
// is passed to the entities (EntityManager::OnTagUpdate), then the HUD's tasks are refreshed.
void UpdateTagState();
