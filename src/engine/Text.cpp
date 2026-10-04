#include "engine/Text.h"

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <vector>

#include "SDL_ttf.h"
#include "engine/FileManager.h"
#include "engine/Render.h"
#include "game/StringTable.h"

namespace Render {
namespace {
std::vector<Font*> g_fonts;   // @0x1fee80 sgl::vector<Render::Font*>
}

void InitFonts() { TTF_Init(); }

// @0x1fae60: border needed around the text for the whole filter chain.
int GlowFilter::GetOffset() const {
    int off = 0;
    for (const GlowFilter* f = this; f; f = f->next) {
        if (!f->knockout) off += (int)((float)f->blur * f->overscale) * 2;
        int ax = f->dx < 0 ? -f->dx : f->dx, ay = f->dy < 0 ? -f->dy : f->dy;
        off += ax > ay ? ax : ay;
    }
    return off;
}

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

// =============================================================================================
// Text sprites: Render::CreateTextInternal @0x1ff2e4
// =============================================================================================
namespace Render {
namespace {

struct TextPart {          // Render::TextPart, 0x24 bytes
    int start = 0;         // +0x00 first character in the buffer
    int end = 0;           // +0x04 one past the last character
    int x = 0, y = 0;      // +0x08 +0x0c position in the text image
    int w = 0, h = 0;      // +0x10 +0x14 TTF_SizeUNICODE
    unsigned style = 0;    // +0x18 TTF style bits
    int spaceAfter = 0;    // +0x1c gap to the next part (a removed space when wrapping)
    bool lineEnd = false;  // +0x20 a line ends after this part
    bool wrapped = false;  // +0x21 the line end comes from word wrap
};

uint16_t g_buf[0x301];             // TextToInternal's global buffer
std::vector<TextPart> g_parts;     // @0x1ff98c
unsigned g_currentStyle = 0;       // @0x2006d4: last style passed to TTF_SetFontStyle (any font)

bool IsStyleTag(uint16_t c) { return c == 'b' || c == 'i' || c == 'u' || c == 's'; }

}  // namespace

Sprite* CreateTextInternal(Font* font, const char32_t* text, float r, float g, float b, bool screenSpace,
                           bool wrap, int width, int align, GlowFilter* glow, Sprite* reuse, int layer,
                           int lineSpacing, const TextPattern* pattern, unsigned style) {
    const bool perCharacter = StringTable::GetLangID() == 7 || StringTable::GetLangID() == 8;   // JP/KO: break anywhere
    const int n = (int)TextToInternal(text, g_buf);
    int bold = style & 1, italic = (style & 3) >> 1, underline = (style & 7) >> 2, strike = (style & 0xf) >> 3;
    g_parts.clear();
    int minx, maxx, miny, maxy, spaceAdvance = 0;
    TTF_GlyphMetrics(font->ttf, ' ', &minx, &maxx, &miny, &maxy, &spaceAdvance);
    auto bits = [&] {
        return (unsigned)((bold ? 1 : 0) | (italic ? 2 : 0) | (underline ? 4 : 0) | (strike ? 8 : 0));
    };
    // The part's characters are measured with the font's current style (styles are only applied
    // when rendering), as on the original.
    auto addPart = [&](int start, int end, bool lineEnd, int space) {
        TextPart p;
        p.start = start;
        p.end = end;
        p.style = bits();
        p.spaceAfter = space;
        p.lineEnd = lineEnd;
        TTF_SizeUNICODE(font->ttf, &g_buf[start], &p.w, &p.h);
        g_parts.push_back(p);
    };

    int seg = 0, i = 0;
    while (i < n) {
        uint16_t c = g_buf[i];
        if (c == '\n') {
            g_buf[i] = 0;
            addPart(seg, i, true, 0);
            seg = ++i;
            continue;
        }
        if (c == '\\' && g_buf[i + 1] == 'n') {   // a literal backslash-n in the string table
            g_buf[i] = 0;
            addPart(seg, i, true, 0);
            i += 2;
            seg = i;
            continue;
        }
        if (c == ' ' && wrap) {
            g_buf[i] = 0;
            addPart(seg, i, false, spaceAdvance);
            seg = ++i;
            continue;
        }
        if (i + 2 < n && c == '<' && IsStyleTag(g_buf[i + 1]) && g_buf[i + 2] == '>') {
            g_buf[i] = 0;
            addPart(seg, i, false, 0);
            uint16_t t = g_buf[i + 1];
            if (t == 'b') ++bold; else if (t == 'i') ++italic; else if (t == 'u') ++underline; else ++strike;
            i += 3;
            seg = i;
            continue;
        }
        if (i + 3 < n && c == '<' && g_buf[i + 1] == '/' && IsStyleTag(g_buf[i + 2]) && g_buf[i + 3] == '>') {
            g_buf[i] = 0;
            addPart(seg, i, false, 0);
            uint16_t t = g_buf[i + 2];
            if (t == 'b') --bold; else if (t == 'i') --italic; else if (t == 'u') --underline; else --strike;
            i += 4;
            seg = i;
            continue;
        }
        ++i;
        if (perCharacter) {
            uint16_t keep = g_buf[i];
            g_buf[i] = 0;
            addPart(seg, i, false, 0);
            g_buf[i] = keep;
            seg = i;
        }
    }
    addPart(seg, n, true, 0);

    const int off = glow ? glow->GetOffset() : 0;
    const int count = (int)g_parts.size();

    // Line widths (and word wrap).
    int maxW = 0, totalH = 0;
    {
        int lineW = 0;
        for (int k = 0; k < count; ++k) {
            TextPart& p = g_parts[(size_t)k];
            int gap = k ? g_parts[(size_t)k - 1].spaceAfter : 0;
            if (wrap && width < lineW + p.w + gap) {
                if (k) {
                    TextPart& q = g_parts[(size_t)k - 1];
                    q.lineEnd = true;
                    q.wrapped = true;
                    if (align == 3 && maxW < width) maxW = width;
                    q.spaceAfter = 0;
                    lineW = 0;   // (+ the now-zero gap)
                } else {
                    lineW = 0;
                }
            } else if (k) {
                lineW += gap;
            }
            lineW += p.w;
            if (maxW < lineW) maxW = lineW;
            if (p.lineEnd) lineW = 0;
        }
    }

    // Placement: alignment per line; justify spreads the free space over the gaps of wrapped lines.
    {
        int lineW = 0, x = 0, y = 0, lineStart = 0;
        float step = 0.f, frac = 0.f;
        for (int k = 0; k < count; ++k) {
            TextPart& p = g_parts[(size_t)k];
            bool place = true;
            if (lineW == 0) {
                step = 0.f;
                bool simple = true;
                if (lineStart < count) {
                    const TextPart& s0 = g_parts[(size_t)lineStart];
                    lineW = s0.w + s0.spaceAfter;
                    if (!s0.lineEnd) {
                        int e = lineStart;
                        for (;;) {
                            ++e;
                            if (e >= count) break;
                            lineW += g_parts[(size_t)e].spaceAfter + g_parts[(size_t)e].w;
                            if (g_parts[(size_t)e].lineEnd) break;
                        }
                        if (align == 3 && e != lineStart) {
                            simple = false;
                            if (e < count && g_parts[(size_t)e].wrapped) {
                                step = (float)(unsigned)(maxW - lineW) / (float)(unsigned)(e - lineStart);
                                frac = 0.f;
                            }
                        }
                    }
                }
                if (simple && align == 1) {
                    p.y = y;
                    p.x = (int)((unsigned)(maxW - lineW) >> 1);
                    x = p.x + p.w + p.spaceAfter;
                    place = false;
                } else {
                    x = align == 2 ? maxW - lineW : 0;
                }
            }
            if (place) {
                p.x = x;
                p.y = y;
                x += p.w + p.spaceAfter;
                if (step != 0.f) {
                    int t = (int)(step + frac);
                    x += t;
                    frac = (step + frac) - (float)t;
                }
            }
            if (totalH < y + p.h) totalH = y + p.h;
            if (p.lineEnd) {
                if (step != 0.f && p.wrapped) p.x = maxW - p.w;
                lineW = 0;
                x = 0;
                y += lineSpacing + font->lineSkip;
                lineStart = k + 1;
            }
        }
    }

    // Render each part into one RGBA image (bytes R, G, B, A).
    const int W = maxW + off * 2, H = totalH + off * 2;
    std::vector<uint8_t> img((size_t)(W > 0 ? W : 0) * (size_t)(H > 0 ? H : 0) * 4, 0);
    const int pitch = W * 4;
    {
        float cr = r * 255.f, cg = g * 255.f, cb = b * 255.f;
        // The colour goes to SDL_ttf with red and blue exchanged, so that the ARGB8888 surface's
        // bytes come out as R, G, B, A for the GL_RGBA upload.
        SDL_Color fg;
        fg.r = (uint8_t)((0.f < cb) * (char)(int)cb);
        fg.g = (uint8_t)((0.f < cg) * (char)(int)cg);
        fg.b = (uint8_t)((0.f < cr) * (char)(int)cr);
        fg.a = 0;
        for (int k = 0; k < count; ++k) {
            const TextPart& p = g_parts[(size_t)k];
            if (p.style != g_currentStyle) TTF_SetFontStyle(font->ttf, (int)p.style);
            g_currentStyle = p.style;
            uint16_t keep = g_buf[p.end];
            g_buf[p.end] = 0;
            SDL_Surface* s = TTF_RenderUNICODE_Blended(font->ttf, &g_buf[p.start], fg);
            g_buf[p.end] = keep;
            if (!s) continue;
            uint8_t* src = (uint8_t*)s->pixels;
            if (pattern) {   // recolour from the pattern, keeping the glyph alpha
                for (int row = 0; row < s->h; ++row) {
                    int pr = (row * pattern->h / s->h) % pattern->h;
                    for (int col = 0; col < s->w; ++col) {
                        uint8_t* d = src + s->pitch * row + col * 4;
                        if (!d[3]) continue;
                        const uint8_t* q = pattern->pixels + pattern->bytesPerPixel * (col % pattern->w) + pattern->pitch * pr;
                        d[0] = (uint8_t)((unsigned)q[0] * d[3] / 0xff);
                        d[1] = (uint8_t)((unsigned)q[1] * d[3] / 0xff);
                        d[2] = (uint8_t)((unsigned)q[2] * d[3] / 0xff);
                    }
                }
            }
            if (lineSpacing < 0 && k != 0) {   // overlapping lines: copy only covered pixels
                for (int row = 0; row < s->h; ++row)
                    for (int col = 0; col < s->w; ++col) {
                        const uint8_t* q = src + s->pitch * row + col * 4;
                        if (!q[3]) continue;
                        uint8_t* d = img.data() + (size_t)(off + row + p.y) * pitch + (size_t)(off + p.x) * 4 + col * 4;
                        d[0] = q[0]; d[1] = q[1]; d[2] = q[2]; d[3] = q[3];
                    }
            } else {
                for (int row = 0; row < s->h; ++row)
                    std::memcpy(img.data() + (size_t)(off + row + p.y) * pitch + (size_t)(off + p.x) * 4,
                                src + s->pitch * row, (size_t)s->w * 4);
            }
            SDL_FreeSurface(s);
        }
    }
    // Premultiply and apply the glow chain. (Strong glows were computed asynchronously on the
    // original, AsyncImageLoader::AddFilterRequest, with the same filter; the result is identical.)
    ApplyImageFilters(img.data(), W, H, pitch, true, glow);

    // Text textures use GL_NEAREST unless ForceLinear (set on large screens).
    Texture* tex = CreateTextureRGBA(W, H, img.data(), !GetForceLinear(), "text");
    Sprite* s = reuse;
    if (s) {
        if (s->tex) RemoveTexture(s->tex);
        SetTexture(s, tex);
    } else {
        s = CreateSprite(tex, layer, false, false);
        SetShaderType(s, 1);
    }
    s->tex = tex;
    s->u0 = 0.f; s->u1 = 1.f;
    s->vBottom = 0.f; s->vTop = 1.f;
    for (float& c : s->color) c = 1.f;
    for (float& a : s->alpha) a = 1.f;
    s->isText = true;
    s->screenSpace = screenSpace;
    s->w = (float)W;
    s->h = -(float)H;
    s->next = nullptr;
    s->visible = true;
    return s;
}

Sprite* CreateTextWrapped(Font* font, const char32_t* text, float r, float g, float b, int width, int align,
                          bool screenSpace, GlowFilter* glow, Sprite* reuse, int lineSpacing,
                          const TextPattern* pattern, unsigned style) {
    return CreateTextInternal(font, text, r, g, b, screenSpace, true, width, align, glow, reuse, 14,
                              lineSpacing, pattern, style);
}

Sprite* CreateText(Font* font, const char32_t* text, float r, float g, float b, int align, bool screenSpace,
                   GlowFilter* glow, Sprite* reuse, int layer) {
    return CreateTextInternal(font, text, r, g, b, screenSpace, false, 0, align, glow, reuse, layer, 0,
                              nullptr, 0);
}

// @0x1fb110
void ApplyImageFilters(uint8_t* px, int w, int h, int pitch, bool premultiply, const GlowFilter* glow) {
    if (premultiply) {
        for (int y = 0; y < h; ++y) {
            uint8_t* p = px + (size_t)y * pitch;
            for (int x = 0; x < w; ++x, p += 4) {
                float a = (float)p[3] / 255.f;
                p[0] = (uint8_t)(int)((float)p[0] * a);
                p[1] = (uint8_t)(int)((float)p[1] * a);
                p[2] = (uint8_t)(int)((float)p[2] * a);
            }
        }
    }
    static std::vector<int> src, tmp;          // FilterContext buffers
    static int tables[65][256];                // per-tap lookup: v * weight (16.16)
    const int n = w * h;
    for (const GlowFilter* f = glow; f; f = f->next) {
        if ((int)src.size() < n) { src.assign((size_t)n, 0); tmp.assign((size_t)n, 0); }
        std::fill(tmp.begin(), tmp.begin() + n, 0);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) src[(size_t)(y * w + x)] = px[(size_t)y * pitch + x * 4 + 3];
        if (f->knockout)
            for (int i = 0; i < n; ++i) src[(size_t)i] = 0xff - src[(size_t)i];
        int r = (int)((float)f->blur * f->overscale);
        float weights[65];
        float sigma;
        if (r < 0x21) {
            sigma = (float)r / 5.f;
            if (sigma < 0.9f) sigma = 0.9f;
        } else {
            r = 0x20;
            sigma = 6.4f;
        }
        float sum = 0.f;
        if (r >= 0) {
            double s = (double)sigma;
            double norm = 1.0 / (s * s * 6.2831853);
            for (int k = -r; k <= r; ++k) {
                double e = std::pow(2.718281828, (double)(float)(-(k * k)) / (s * (s + s)));
                float wk = (float)(norm * e);
                sum += wk;
                weights[k + r] = wk;
            }
            for (int k = 0; k <= 2 * r; ++k) weights[k] /= sum;
        }
        const int taps = r * 2 + 1;
        for (int k = 0; k < taps; ++k) {
            int step = (int)(weights[k] * 65536.f), acc = 0;
            for (int v = 0; v < 256; ++v) { tables[k][v] = acc; acc += step; }
        }
        // Vertical pass into tmp, shifted horizontally by dx.
        const int dx = f->dx, dy = f->dy;
        const int x0 = dx > 0 ? dx : 0, x1 = dx < 1 ? w + dx : w;
        for (int y = r; y < h - r; ++y)
            for (int x = x0; x < x1; ++x) {
                int acc = 0;
                for (int k = 0; k < taps; ++k) acc += tables[k][src[(size_t)((y - r + k) * w + (x - dx))]];
                tmp[(size_t)(y * w + x)] = acc >> 16;
            }
        // Horizontal pass, shifted vertically by dy, blended under (or, knockout, over) the image.
        const float strength = f->overscale * f->overscale * f->strength * 0.00390625f;
        const int y0 = dy > 0 ? dy : 0, y1 = dy < 1 ? h + dy : h;
        for (int y = y0; y < y1; ++y) {
            uint8_t* p = px + (size_t)y * pitch + r * 4;
            for (int x = r; x < w - r; ++x, p += 4) {
                int acc = 0;
                for (int k = 0; k < taps; ++k) acc += tables[k][tmp[(size_t)((y - dy) * w + (x - r + k))]];
                float v = (float)(acc >> 16) * strength;
                if (v <= 1.f) {
                    if (!(v > 0.f)) continue;
                } else {
                    v = 1.f;
                }
                float R = (float)p[0], G = (float)p[1], B = (float)p[2];
                float a = (float)p[3] / 255.f;
                float nr, ng, nb;
                if (!f->knockout) {
                    float inv = 1.f - a;
                    a = a + v * inv;
                    ng = (G / 255.f + v * f->g * inv) * 255.f;
                    nr = (R / 255.f + inv * v * f->r) * 255.f;
                    nb = (B / 255.f + v * f->b * inv) * 255.f;
                } else {
                    float inv = 1.f - v;
                    ng = (v * f->g + (G / 255.f) * inv) * a * 255.f;
                    nr = (v * f->r + inv * (R / 255.f)) * a * 255.f;
                    nb = (v * f->b + (B / 255.f) * inv) * a * 255.f;
                }
                p[0] = (uint8_t)((0.f < nr) * (char)(int)nr);
                p[1] = (uint8_t)((0.f < ng) * (char)(int)ng);
                p[2] = (uint8_t)((0.f < nb) * (char)(int)nb);
                float na = a * 255.f;
                p[3] = (uint8_t)((0.f < na) * (char)(int)na);
            }
        }
    }
}

}  // namespace Render
