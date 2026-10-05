// Game dialogs (the windows beyond the HUD). Each is a set of static functions behind a
// WindowManager::FunctionalWindow, as on the original.
#pragma once

namespace WindowManager { class FunctionalWindow; }

namespace SettingsWindow {
WindowManager::FunctionalWindow* Queue();   // static FunctionalWindow "SettingsWindow" (_INIT_ 0x33def8)
void Init();                                // @0x33fae0
void Deinit();                              // @0x33e4bc
void Show();                                // @0x33fa5c
void Hide();                                // @0x33e1dc
bool IsVisible();                           // @0x33de04
void UpdateContents();                      // @0x33ea94
void EnableSync(bool on);                   // @0x33f944
int GetSoundMutedState();                   // @0x33de18
void SetSoundMutedState(int muted);         // @0x33de28
int GetMusicMutedState();                   // @0x33de3c
void SetMusicMutedState(int muted);         // @0x33de50
int GetNotificationState();                 // @0x33de78
void SetNotificationState(int on);          // @0x33de64
}
