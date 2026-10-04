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

// UTF-8 to wide, using the decoder inlined in Init (invalid bytes are skipped).
std::u32string DecodeUtf8(const char* s);

}  // namespace StringTable
