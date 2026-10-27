// GUI::TextCache (0x1a0c bytes, made by GUI::Init at 0x6121f8): the textures of up to 0x80 short
// texts (under 0x40 characters) a Textfield has just replaced, kept so that showing the same text
// again (a counter going back and forth, a tab title) re-uses the texture instead of rendering it.
#pragma once

#include <cstdint>
#include <string>

namespace Render { struct Texture; struct Font; struct GlowFilter; }

namespace GUI {

class TextCache {
public:
    void InvalidateCache();                                   // @0x17a030
    void InvalidateEntriesWithGlow(Render::GlowFilter* glow); // @0x17a0d8
    // @0x17b51c: the cached texture for these settings (it leaves the cache), or null.
    Render::Texture* GetTextureFor(Render::Font* font, const char32_t* text, float r, float g, float b,
                                   unsigned width, int align, Render::GlowFilter* glow, int8_t lineSpacing);
    // @0x17b82c: keeps `tex` (the cache owns it from here) for these settings.
    void AddTextureFor(Render::Texture* tex, Render::Font* font, const char32_t* text, float r, float g,
                       float b, unsigned width, int align, Render::GlowFilter* glow, int8_t lineSpacing);

private:
    struct Entry {                        // 0x30 bytes
        int time = 0;                     // +0x00 Timer::GetGlobalTime when added
        uint32_t hash = 0;                // +0x04 the text's hash
        Render::Texture* tex = nullptr;   // +0x08
        std::u32string text;              // +0x0c (nullptr on the original = empty here)
        Render::Font* font = nullptr;     // +0x10
        Render::GlowFilter* glow = nullptr;  // +0x14
        float r = 0, g = 0, b = 0;        // +0x18 +0x1c +0x20
        unsigned width = 0;               // +0x24
        int align = 0;                    // +0x28
        int8_t lineSpacing = 0;           // +0x2c
    };
    void Clear(Entry& e);
    Entry entries[0x80];                  // +0x0000
    unsigned freeSlots[0x80] = {};        // +0x1800 slots emptied by GetTextureFor, re-used first
    unsigned count = 0;                   // +0x1a00 the slots in use are below this
    unsigned next = 0;                    // +0x1a04 the next slot (round robin, & 0x7f)
    unsigned freeCount = 0;               // +0x1a08
};

TextCache* GetTextCache();   // GUI's cache (0x6121f8), null before GUI::Init and after Deinit

}  // namespace GUI
