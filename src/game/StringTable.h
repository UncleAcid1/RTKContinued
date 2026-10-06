// StringTable: localized UI strings (LocalizedStrings*.xml).
// Port of StringTable (libkingdom.so 5.11): Init @0x22cae0, GetString @0x228ee8,
// StringExists @0x22c980, SetLanguage @0x228b7c.
//
// File format: <ts><t i="XX_KEY">text</t>...</ts>, where XX is a language code. Keys are matched
// case-insensitively. Strings are wide (wchar_t, 32-bit on Android); the port uses char32_t.
#pragma once
#include <string>

namespace StringTable {

// @0x22cae0 Init(file, dryRun, renameToCurrentLanguage). dryRun only parses. With
// renameToCurrentLanguage the first two characters of every key become the current language code.
bool Init(const char* file, bool dryRun, bool renameToCurrentLanguage);
void Deinit();                                   // @0x228c98

void SetLanguage(const char* code, int langId);  // @0x228b7c  e.g. ("EN", 0)
int GetLangID();                                 // @0x2277bc
const char* GetLanguage();

// @0x228ee8: "<lang>_<key>" if present and non-empty, else "EN_<key>", else nullptr.
// (The original's debug placeholder for missing keys, enabled by a command-line switch through
// SetEmptyNameReplacement, is not ported.)
const char32_t* GetString(const char* key);
bool StringExists(const char* key);              // @0x22c980
// @0x22b1a0: a duration with the units of "TIME_STR" ("h,m,s"): "HH:MM h" from an hour, "MM:SS m"
// from a minute, else "S s"; compact gives "H h" / "M m" instead. (The original writes into a
// caller buffer; callers pass 0x20 characters.)
std::u32string GetTimeString(int seconds, bool compact);
// @0x22a30c (the non-compact form): "00:SS", "MM:SS", "HH:MM:SS", from a day "D days HH:MM:SS".
// UNVERIFIED: the compact form (second argument true) is not ported; it formats as false.
std::u32string GetNumericTimeString(int seconds, bool compact);
// @0x228478: the plural form of "{one|few|many}" for n (the Slavic rule: n%10 == 1 and not
// 11 -> one; n%10 in 2..4 and not 12..14 -> few; else many).
std::u32string GetCountableString(const char32_t* forms, int n);

// UTF-8 to wide, using the decoder inlined in Init (invalid bytes are skipped).
std::u32string DecodeUtf8(const char* s);

}  // namespace StringTable
