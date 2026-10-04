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

// Render::GlowFilter (0x30 bytes): one blurred outline/shadow pass, chained through next.
struct GlowFilter {
    float r = 0, g = 0, b = 0;    // +0x04 +0x08 +0x0c colour
    int blur = 0;                 // +0x10 radius in pixels (before overscale)
    float strength = 0;           // +0x14
    int dx = 0, dy = 0;           // +0x18 +0x1c offset
    bool knockout = false;        // +0x20 inner glow (inverted alpha)
    float overscale = 1.f;        // +0x24
    bool allowAsync = true;       // +0x28
    GlowFilter* next = nullptr;   // +0x2c
    int GetOffset() const;        // @0x1fae60
};

void InitFonts();                 // TTF_Init (done in SDL_baseInit @0x18b59c)
void DeinitFonts();               // @0x1fec10

// @0x1fee6c: returns the cached font for (file, size) or opens a new one. Fonts created from the
// same file share the first one's buffer.
Font* CreateFont(const char* file, unsigned size);

struct Sprite;

// A pattern image that recolours text (TextStyleManager gradients): RGBA bytes, rows of `pitch`.
struct TextPattern { int w = 0, h = 0, pitch = 0, bytesPerPixel = 4; const uint8_t* pixels = nullptr; };

enum TextStyle { kTextBold = 1, kTextItalic = 2, kTextUnderline = 4, kTextStrikethrough = 8 };

// @0x1ff2e4 Render::CreateTextInternal. Renders `text` (with \n, literal "\\n", <b> <i> <u> <s>
// tags, word wrap to `width` when `wrap`, align 0 left 1 centre 2 right 3 justify) into a new text
// sprite on `layer`, or re-uses `reuse` (its texture is replaced). The sprite has u 0..1, v 0 at
// the bottom edge field and a negative height (texture rows top-down), shader 1.
Sprite* CreateTextInternal(Font* font, const char32_t* text, float r, float g, float b, bool screenSpace,
                           bool wrap, int width, int align, GlowFilter* glow, Sprite* reuse, int layer,
                           int lineSpacing, const TextPattern* pattern, unsigned style);
// @0x2013b0 (wrapping, GUI layer) and @0x2013f4 (no wrap).
Sprite* CreateTextWrapped(Font* font, const char32_t* text, float r, float g, float b, int width,
                          int align, bool screenSpace, GlowFilter* glow, Sprite* reuse, int lineSpacing,
                          const TextPattern* pattern, unsigned style);
Sprite* CreateText(Font* font, const char32_t* text, float r, float g, float b, int align, bool screenSpace,
                   GlowFilter* glow, Sprite* reuse, int layer);

// @0x1fb110: premultiply (optional) and apply the glow chain to 32-bit RGBA pixels in place.
void ApplyImageFilters(uint8_t* px, int w, int h, int pitch, bool premultiply, const GlowFilter* glow);

// @0x1fecb0: wide text to the 16-bit form SDL_ttf takes (truncated to 0x300 characters).
size_t TextToInternal(const char32_t* text, uint16_t* out /* [0x301] */);

// @0x1fed68: true if every glyph (except CR/LF) exists in the font.
bool CanDisplayText(Font* font, const char32_t* text);

}  // namespace Render
