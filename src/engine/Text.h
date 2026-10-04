// Render text: fonts (SDL_ttf 2.0.11 on FreeType 2.4.5, as linked by the original) and text sprites.
// Port of Render::CreateFont @0x1fee6c, Render::CanDisplayText @0x1fed68,
// Render::TextToInternal @0x1fecb0 and the text sprite builders (CreateTextInternal @0x1ff2e4).
#pragma once
#include <cstdint>
#include <string>

struct _TTF_Font;

namespace Render {

struct Font {                     // 0x18 bytes
    _TTF_Font* ttf = nullptr;     // +0x00
    std::string name;             // +0x04 file name the font was created from
    unsigned size = 0;            // +0x08 point size
    uint32_t dataSize = 0;        // +0x0c
    uint8_t* data = nullptr;      // +0x10 file buffer, owned by the first font created from a file
    int lineSkip = 0;             // +0x14 TTF_FontLineSkip
};

void InitFonts();                 // TTF_Init (done in SDL_baseInit @0x18b59c)
void DeinitFonts();               // @0x1fec10

// @0x1fee6c: returns the cached font for (file, size) or opens a new one. Fonts created from the
// same file share the first one's buffer.
Font* CreateFont(const char* file, unsigned size);

// @0x1fecb0: wide text to the 16-bit form SDL_ttf takes (truncated to 0x300 characters).
size_t TextToInternal(const char32_t* text, uint16_t* out /* [0x301] */);

// @0x1fed68: true if every glyph (except CR/LF) exists in the font.
bool CanDisplayText(Font* font, const char32_t* text);

}  // namespace Render
