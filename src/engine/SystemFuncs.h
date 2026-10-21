// SystemFuncs: the platform layer the game calls into (settings, device id, web pages ...).
// Port of SystemFuncs (libkingdom.so 5.11), which forwards to Java on Android.
#pragma once
#include <cstdint>
#include <string>

namespace SystemFuncs {

// @0x3d9e8c: clears the platform layer's state (the temporary settings and the pending text
// results). PORT: also reads the stored settings back (see below).
void Init();

// Settings stored per file (Android SharedPreferences "MyCountry_mute", ...; a null file is the
// default one): Int @0x3d7fac / @0x3d7ea0, Bool @0x3d81c4 / @0x3d7d94, String @0x3d9148 / @0x3d82dc
// (get / set). PORT: Android keeps them in the app's preferences; the port keeps them in
// "settings.txt" in the storage folder (FileManager::GetStoragePath), read by Init and written on
// every change.
int GetSetting_Int(const char* key, int def, const char* file = nullptr);
void SetSetting_Int(const char* key, int value, const char* file = nullptr);
bool GetSetting_Bool(const char* key, bool def, const char* file = nullptr);
void SetSetting_Bool(const char* key, bool value, const char* file = nullptr);
std::string GetSetting_String(const char* key, const char* def, const char* file = nullptr);
void SetSetting_String(const char* key, const char* value, const char* file = nullptr);

// @0x3da2f0: the language the app runs in, as a language id (0 EN, 1 RU, 2 DE, 3 LV, 4 ES, 5 IT,
// 6 FR, 7 KO, 8 JA), from Java's jni_getCurrentLocale. PORT (UNVERIFIED (milestone 7): the system
// language on the Mac; the Java side is not decompiled): the language chosen earlier ("selected_language" in "MyCountry_flags", which
// FileManager::SetPreferredLanguade writes), English when none was chosen yet.
int getCurrentLanguage();
// PORT: what Java reports through SDLMain.mcSetLanguage / mcSetAvailableLanguage (UNVERIFIED
// (milestone 7), as above): whether a language was chosen, and the system's language as a language id (English when
// the game does not have it).
bool HasSelectedLanguage();
int GetSystemLanguage();

// @0x3d7664: the device id shown in the settings window. UNVERIFIED (milestone 7, the Mac's id): empty.
std::string getGUID();
// @0x3da2d0: the Android id the online state chunk keeps. PORT (online, dormant): empty.
std::string getSID();

// @0x3d7954: Google Play Games sign-in. Offline port: never signed in.
inline bool signedInToGPGS() { return false; }

// @0x3d93b8: gzip (the 10-byte header 1f 8b 08 00 00 00 00 00 00 00, raw deflate at the default
// level, CRC-32 and size). @0x3db14c: the reverse (CGZIP2AT, which passes data without the gzip
// magic through unchanged). Both return a new[] buffer with a 0 after the data; size is in/out.
uint8_t* GZIP_Compress(const void* data, uint32_t& size);
uint8_t* GZIP_Decompress(const void* data, uint32_t& size);

}  // namespace SystemFuncs
