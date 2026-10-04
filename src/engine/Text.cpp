#include "engine/Text.h"

#include <cstdio>
#include <cstring>
#include <vector>

#include "SDL_ttf.h"
#include "engine/FileManager.h"

namespace Render {
namespace {
std::vector<Font*> g_fonts;   // @0x1fee80 sgl::vector<Render::Font*>
}

void InitFonts() { TTF_Init(); }

// @0x1fec10
void DeinitFonts() {
    for (Font* f : g_fonts) {
        if (f->ttf) TTF_CloseFont(f->ttf);
        FileManager::FreeFile(f->data);
        delete f;
    }
    g_fonts.clear();
}

// @0x1fee6c
Font* CreateFont(const char* file, unsigned size) {
    for (Font* f : g_fonts)
        if (f->name == file && f->size == size) return f;
    // A font of the same file that owns its buffer lends it to the new one.
    uint8_t* shared = nullptr;
    uint32_t sharedSize = 0;
    for (Font* f : g_fonts) {
        if (f->name == file && f->data) { shared = f->data; sharedSize = f->dataSize; break; }
    }
    Font* font = new Font();
    g_fonts.push_back(font);   // pushed before opening, as in the original
    uint8_t* data = shared;
    uint32_t dataSize = sharedSize;
    if (!shared) data = FileManager::LoadFile(file, dataSize);
    font->ttf = data ? TTF_OpenFontRW(SDL_RWFromMem(data, (int)dataSize), 0, (int)size) : nullptr;
    if (!font->ttf) {
        std::printf("Render::CreateFont: cannot open %s: %s\n", file, TTF_GetError());
        if (!shared) FileManager::FreeFile(data);
        return nullptr;   // PORT: the original also leaves the empty entry in its list
    }
    font->name = file;
    font->size = size;
    TTF_SetFontHinting(font->ttf, TTF_HINTING_LIGHT);   // 1
    font->lineSkip = TTF_FontLineSkip(font->ttf);
    if (shared) {
        font->dataSize = 0;
        font->data = nullptr;
    } else {
        font->dataSize = dataSize;
        font->data = data;
    }
    return font;
}

// @0x1fecb0
size_t TextToInternal(const char32_t* text, uint16_t* out) {
    size_t n = 0;
    while (text[n] && n < 0x300) { out[n] = (uint16_t)text[n]; ++n; }
    if (n >= 0x300) std::printf("Render::TextToInternal: text too long (%zu)\n", n);
    out[n] = 0;
    return n;
}

// @0x1fed68
bool CanDisplayText(Font* font, const char32_t* text) {
    uint16_t buf[0x301];
    size_t n = TextToInternal(text, buf);
    for (size_t i = 0; i < n; ++i) {
        if (buf[i] == 0xd || buf[i] == 0xa) continue;
        if (!TTF_GlyphIsProvided(font->ttf, buf[i])) return false;
    }
    return true;
}

}  // namespace Render
