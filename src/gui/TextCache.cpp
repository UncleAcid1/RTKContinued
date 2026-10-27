#include "gui/TextCache.h"

#include "engine/Render.h"
#include "engine/Text.h"
#include "engine/Timer.h"

namespace GUI {
namespace {

// StringHash over the wide text (the same 0x1505 * 0x21 hash as the char version @0x22d498).
uint32_t TextHash(const char32_t* s) {
    uint32_t h = 0x1505;
    for (; *s; ++s) h = h * 0x21 + (uint32_t)*s;
    return h;
}

// The glow chains match when both are empty or every filter's settings match in turn.
bool SameGlow(const Render::GlowFilter* a, const Render::GlowFilter* b) {
    for (;;) {
        if (!a && !b) return true;
        if (!a || !b) return false;
        if (a->r != b->r || a->g != b->g || a->b != b->b || a->blur != b->blur || a->strength != b->strength ||
            a->dx != b->dx || a->dy != b->dy || a->knockout != b->knockout)
            return false;
        a = a->next;
        b = b->next;
    }
}

}  // namespace

void TextCache::Clear(Entry& e) {
    if (e.tex) Render::RemoveTexture(e.tex);
    e.tex = nullptr;
    e.text.clear();
}

void TextCache::InvalidateCache() {
    for (unsigned i = 0; i < count; ++i) Clear(entries[i]);
    count = 0;
    freeCount = 0;
    next = 0;
}

// The entries made with this filter (a Textfield deleting its filters): their stored filter would
// be read by the next GetTextureFor.
void TextCache::InvalidateEntriesWithGlow(Render::GlowFilter* glow) {
    for (unsigned i = 0; i < count; ++i)
        if (entries[i].glow == glow) Clear(entries[i]);
}

Render::Texture* TextCache::GetTextureFor(Render::Font* font, const char32_t* text, float r, float g, float b,
                                          unsigned width, int align, Render::GlowFilter* glow,
                                          int8_t lineSpacing) {
    for (unsigned i = 0; i < count; ++i) {
        Entry& e = entries[i];
        if (!e.tex || e.font != font || e.r != r || e.g != g || e.b != b || e.width != width ||
            e.align != align || e.lineSpacing != lineSpacing || !SameGlow(e.glow, glow))
            continue;
        if (TextHash(text) != e.hash || e.text != text) continue;
        Render::Texture* tex = e.tex;
        e.tex = nullptr;
        e.text.clear();
        freeSlots[freeCount++] = i;
        return tex;
    }
    return nullptr;
}

void TextCache::AddTextureFor(Render::Texture* tex, Render::Font* font, const char32_t* text, float r, float g,
                              float b, unsigned width, int align, Render::GlowFilter* glow, int8_t lineSpacing) {
    unsigned slot;
    if (freeCount == 0) slot = next++ & 0x7f;
    else slot = freeSlots[--freeCount];
    if (count <= slot) count = slot + 1;
    Entry& e = entries[slot];
    e.time = Timer::GetGlobalTime();
    e.hash = TextHash(text);
    Clear(e);
    e.tex = tex;
    e.text = text;
    e.lineSpacing = lineSpacing;
    e.r = r;
    e.g = g;
    e.font = font;
    e.glow = glow;
    e.b = b;
    e.width = width;
    e.align = align;
}

}  // namespace GUI
