// SystemFuncs: the platform layer the game calls into (settings, device id, web pages ...).
// Port of SystemFuncs (libkingdom.so 5.11), which forwards to Java on Android.
#pragma once
#include <cstdint>
#include <string>

namespace SystemFuncs {

// Settings stored per file (Android SharedPreferences "MyCountry_mute", ...; a null file is the
// default one): Int @0x3d7fac / @0x3d7ea0, Bool @0x3d81c4 / @0x3d7d94, String @0x3d9148 / @0x3d82dc
// (get / set). UNVERIFIED (milestone 6): the port keeps them in memory; persistence is not ported yet.
int GetSetting_Int(const char* key, int def, const char* file = nullptr);
void SetSetting_Int(const char* key, int value, const char* file = nullptr);
bool GetSetting_Bool(const char* key, bool def, const char* file = nullptr);
void SetSetting_Bool(const char* key, bool value, const char* file = nullptr);
std::string GetSetting_String(const char* key, const char* def, const char* file = nullptr);
void SetSetting_String(const char* key, const char* value, const char* file = nullptr);

// @0x3d7664: the device id shown in the settings window. UNVERIFIED (offline port): empty.
std::string getGUID();
// @0x3da2d0: the Android id the online state chunk keeps. UNVERIFIED (offline port): empty.
std::string getSID();

// @0x3d7954: Google Play Games sign-in. Offline port: never signed in.
inline bool signedInToGPGS() { return false; }

// @0x3d93b8: gzip (the 10-byte header 1f 8b 08 00 00 00 00 00 00 00, raw deflate at the default
// level, CRC-32 and size). @0x3db14c: the reverse (CGZIP2AT, which passes data without the gzip
// magic through unchanged). Both return a new[] buffer with a 0 after the data; size is in/out.
uint8_t* GZIP_Compress(const void* data, uint32_t& size);
uint8_t* GZIP_Decompress(const void* data, uint32_t& size);

}  // namespace SystemFuncs
