// Game dialogs (the windows beyond the HUD). Each is a set of static functions behind a
// WindowManager::FunctionalWindow, as on the original.
#pragma once

namespace WindowManager { class FunctionalWindow; }

namespace CityRenameWindow {
WindowManager::FunctionalWindow* Queue();   // static FunctionalWindow "CityRenameWindow" (_INIT_ 0x298ffc)
void Init();                                // @0x29949c
void Deinit();                              // @0x29945c
void Show();                                // @0x299248
void Hide();                                // @0x2992ec
}

namespace PopupWindow {
using Callback = void (*)();
WindowManager::FunctionalWindow* Queue();   // static FunctionalWindow "PopupWindow" (_INIT_ 0x32d02c)
void Init();                                // @0x32d9b0
void Deinit();                              // @0x32d964
// @0x32d630: OK only -> one centred button; OK and/or Cancel -> two; onExit adds the close button.
// okText / cancelText are StringTable keys replacing "HUD_OK" / "CANCEL".
void Show(const char32_t* text, Callback onOk, Callback onCancel, Callback onExit, const char* okText = nullptr,
          const char* cancelText = nullptr);
void Hide();                                // @0x32d79c
bool IsVisible();                           // @0x32d000
void ForceOnTop();                          // @0x32d014
}

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

// The shop (3e.3). UNVERIFIED stand-ins until it is ported: the shop is never open.
namespace ShopWindow {
bool IsVisible();                           // @0x346d14
void Show();                                // @0x34d73c
void Hide();                                // @0x3479b8
// @0x346dd4: the left edge of the shop's info panel (its offset 0x62d6e8 + the screen width - the
// panel's width).
int GetInfoPanelX();
}

// The "not enough resources" dialog (3e.4). The requirements are ported; the dialog is not yet.
namespace NotEnoughWindow {
void ResetRequirements();                   // @0x2f6cfc
void AddRequirement(int type, unsigned amount);   // @0x2f6ea4 a resource amount needed
// @0x2f8c60: true when nothing is missing. UNVERIFIED (3e.4): the level, profession, item, worker,
// building and upgrade requirements (other Add* functions, not ported) are always met.
bool CheckRequirements();
void Show();                                // @0x2fc7f8 UNVERIFIED stand-in (3e.4): prints the shortfall
}
