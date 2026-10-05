// TextInput: typing into the text field being edited with a keyboard. Port of TextInput
// (libkingdom.so 5.11), 0x184ecc..0x1871a0; globals enabled 0x6124d0, text 0x6124d4 (256 wchar_t),
// currLength 0x6128d4. Every change is passed on through FileManager::EndTextInput (final only on
// Return).
//
// PORT: on Android FileManager::BeginTextInput opens a Java text dialog and TextInput::Enable is
// never called; the Mac port types into the field with this keyboard path instead (see
// FileManager::BeginTextInput).
#pragma once

namespace TextInput {

extern bool enabled;                 // 0x6124d0
void Enable(const char32_t* text);   // @0x1870e0
void Disable();                      // @0x184ecc
void OnReturn();                     // @0x1854b0
void OnBackspace();                  // @0x1854d8
void OnCharacter(char32_t c);        // @0x185518

}  // namespace TextInput
