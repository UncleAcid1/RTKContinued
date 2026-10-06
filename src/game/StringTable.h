// StringTable: localized UI strings (LocalizedStrings*.xml).
// Port of StringTable (libkingdom.so 5.11): Init @0x22cae0, GetString @0x228ee8,
// StringExists @0x22c980, SetLanguage @0x228b7c.
//
// File format: <ts><t i="XX_KEY">text</t>...</ts>, where XX is a language code. Keys are matched
// case-insensitively. Strings are wide (wchar_t, 32-bit on Android); the port uses char32_t.
#pragma once
#include <initializer_list>
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

// StringArgument (0x10 bytes: type +0x00, value +0x08): one argument of SWPrintf.
struct StringArgument {
    enum Type { kNone = 0, kInt = 1, kLong = 2, kDouble = 3, kUtf8 = 4, kWide = 5 };
    int type = kNone;
    int i = 0;
    double d = 0.0;
    const char* s = nullptr;
    const char32_t* w = nullptr;
    StringArgument() = default;
    StringArgument(int v) : type(kInt), i(v) {}
    StringArgument(unsigned v) : type(kInt), i((int)v) {}
    StringArgument(double v) : type(kDouble), d(v) {}
    StringArgument(const char* v) : type(kUtf8), s(v) {}
    StringArgument(const char32_t* v) : type(kWide), w(v) {}
};

// @0x22d84c SWPrintf(buf, size, format, up to 13 arguments): the game's wide printf. Specifiers:
// %% ; %[+][0n|.n][$]d / u (0n: at least n digits; .n only applies to %f; $: groups of three split by ","); %[.n]f;
// %s (wide); %S (UTF-8); %c. Its checks follow each other, so a matched specifier's next character
// is tested as the next specifier ("%ds" takes two arguments). A wrong argument type prints an
// error and ends the text; at `size` characters the text is cut (size - 1 kept).
std::u32string SWPrintf(unsigned size, const char32_t* format, std::initializer_list<StringArgument> args = {});
// @0x22e0f8: L"%d" into one of 16 rotating buffers (the pointer stays valid for 15 more calls).
const char32_t* ToWideString(int n);
