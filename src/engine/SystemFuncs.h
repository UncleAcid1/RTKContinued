// SystemFuncs: the platform layer the game calls into (settings, device id, web pages ...).
// Port of SystemFuncs (libkingdom.so 5.11), which forwards to Java on Android.
#pragma once
#include <string>

namespace SystemFuncs {

// @0x3d7fac / @0x3d7ea0: integer settings, stored per file (Android SharedPreferences "MyCountry_mute",
// ...). UNVERIFIED (milestone 6): the port keeps them in memory; persistence is not ported yet.
int GetSetting_Int(const char* key, int def, const char* file = nullptr);
void SetSetting_Int(const char* key, int value, const char* file = nullptr);

// @0x3d7664: the device id shown in the settings window. UNVERIFIED (offline port): empty.
std::string getGUID();

// @0x3d7954: Google Play Games sign-in. Offline port: never signed in.
inline bool signedInToGPGS() { return false; }

}  // namespace SystemFuncs
