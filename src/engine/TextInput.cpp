#include "engine/TextInput.h"

#include "engine/FileManager.h"

namespace TextInput {

bool enabled = false;
namespace {
char32_t g_text[0x100];   // 0x6124d4
unsigned g_length = 0;    // 0x6128d4
}  // namespace

void Enable(const char32_t* text) {
    enabled = true;
    if (!text) {
        g_text[0] = 0;
        g_length = 0;
        return;
    }
    // WCSCpy without a bound on the original; the port stops at the buffer's end.
    unsigned n = 0;
    while (text[n] && n < 0xff) {
        g_text[n] = text[n];
        ++n;
    }
    g_text[n] = 0;
    g_length = n;
}

void Disable() { enabled = false; }

void OnReturn() {
    enabled = false;
    FileManager::EndTextInput(g_text, true);
}

void OnBackspace() {
    if (g_length == 0) return;
    --g_length;
    g_text[g_length] = 0;
    FileManager::EndTextInput(g_text, false);
}

void OnCharacter(char32_t c) {
    unsigned n = g_length;
    if (n > 0xfe) return;
    g_length = n + 1;
    g_text[n] = c;
    g_text[n + 1] = 0;
    FileManager::EndTextInput(g_text, false);
}

}  // namespace TextInput
