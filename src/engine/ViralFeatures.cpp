// ViralFeatures: the screenshot framed for sharing. Port of ViralFeatures::PostScreenshot,
// BlitBiliniar and ColorCorrection (libkingdom.so 5.11), 0x23ef20..0x23f5d4, and the file's static
// strings (initialiser @0x23ef24, at 0x616b30 / 0x616b48). PORT: the original works on SDL2
// surfaces (SDL_CreateRGBSurface, SDL_UpperBlit); here they are plain 32-bit pixel buffers and the
// blits are written out. WallPost (the Facebook wall posts, same address range) is online.
#include "engine/ViralFeatures.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "engine/FileManager.h"
#include "engine/Image.h"
#include "engine/Resources.h"

namespace ViralFeatures {
namespace {

const std::string g_logo = "images/Rule_The_Kingdom.png";         // 0x616b30
const std::string g_cover = "images/screenshot_frame_male.jpg";   // 0x616b48

// An SDL_Surface's w, h and pixels (32 bits per pixel, pitch w * 4).
struct Surface {
    int w = 0, h = 0;
    uint32_t* px = nullptr;
};

struct Rect {
    int x = 0, y = 0, w = 0, h = 0;
};

// @0x23ef20 ColorCorrection: returns the surface (the cover) unchanged in 5.11.
void ColorCorrection(ImageRGBA&) {}

// @0x23efac BlitBiliniar: src's srcRect (its x / y unused) stretched over dst's dstRect with
// bilinear filtering; the three colour bytes keep their places, alpha is 0xff. Pixels that fall off
// dst are skipped.
void BlitBiliniar(const Surface& src, const Rect& srcRect, Surface& dst, const Rect& dstRect) {
    const float sx = (float)srcRect.w / (float)dstRect.w;
    const float sy = (float)srcRect.h / (float)dstRect.h;
    auto byte = [](float v) { return v > 0.f ? (uint32_t)(int)v & 0xff : 0u; };
    for (int i = 0; i < dstRect.w; ++i) {
        for (int j = 0; j < dstRect.h; ++j) {
            if (i + dstRect.x >= dst.w || j + dstRect.y >= dst.h) continue;
            const float fx = (float)i * sx, fy = (float)j * sy;
            const int x0 = (int)std::floor(fx), y0 = (int)std::floor(fy);
            const int x1 = x0 + 1 < srcRect.w ? x0 + 1 : x0;
            const int y1 = y0 + 1 < srcRect.h ? y0 + 1 : y0;
            const float ax = fx - (float)x0, bx = 1.f - ax;
            const float ay = fy - (float)y0, by = 1.f - ay;
            const uint32_t p00 = src.px[x0 + y0 * src.w], p10 = src.px[x1 + y0 * src.w];
            const uint32_t p01 = src.px[x0 + y1 * src.w], p11 = src.px[x1 + y1 * src.w];
            uint32_t out = 0xff000000u;
            for (int shift = 0; shift <= 16; shift += 8) {
                auto c = [shift](uint32_t p) { return (float)((p >> shift) & 0xff); };
                const uint32_t top = byte(c(p10) * ax + c(p00) * bx);
                const uint32_t bottom = byte(c(p11) * ax + c(p01) * bx);
                out |= byte((float)bottom * ay + (float)top * by) << shift;
            }
            dst.px[(j + dstRect.y) * dst.w + i + dstRect.x] = out;
        }
    }
}

// SDL_UpperBlit of a picture at (x, y), clipped to dst. RGBA bytes blend (SDL_BLENDMODE_BLEND,
// SDL2's ALPHA_BLEND_RGBA: d + (s - d) * a / 255; the cover's colours are premultiplied, which SDL
// takes as they are); RGB bytes are copied. dst is 0xAARRGGBB with alpha 0xff, which blending keeps.
void Blit(const uint8_t* bytes, int channels, int w, int h, Surface& dst, int x, int y) {
    for (int j = 0; j < h; ++j) {
        if (y + j < 0 || y + j >= dst.h) continue;
        for (int i = 0; i < w; ++i) {
            if (x + i < 0 || x + i >= dst.w) continue;
            const uint8_t* s = bytes + ((size_t)j * w + i) * channels;
            uint32_t& d = dst.px[(y + j) * dst.w + x + i];
            if (channels == 3) {
                d = 0xff000000u | (uint32_t)s[0] << 16 | (uint32_t)s[1] << 8 | s[2];
                continue;
            }
            const int a = s[3];
            int dr = (int)(d >> 16) & 0xff, dg = (int)(d >> 8) & 0xff, db = (int)d & 0xff;
            dr = (s[0] - dr) * a / 255 + dr;
            dg = (s[1] - dg) * a / 255 + dg;
            db = (s[2] - db) * a / 255 + db;
            d = (d & 0xff000000u) | (uint32_t)dr << 16 | (uint32_t)dg << 8 | (uint32_t)db;
        }
    }
}

}  // namespace

// @0x23f2b8
void PostScreenshot(const uint32_t* pixels, int w, int h) {
    std::vector<uint32_t> out((size_t)0x32e * 0x1e0, 0xffffffffu);   // memset 0xff
    Surface picture{0x32e, 0x1e0, out.data()};
    const Surface screen{w, h, const_cast<uint32_t*>(pixels)};
    unsigned bpp = 0;
    ImageRGBA cover = Resources::GetImageData(g_cover.c_str(), bpp);
    if (!cover.ok()) {
        std::printf("ERROR: ViralFeatures::PostScreenshot() Cover data was not found\n");
        return;
    }
    if (bpp != 32) {
        std::printf("ERROR: ViralFeatures::PostScreenshot() Cover data is not 32 bits per pixel\n");
        return;
    }
    // Render::GetSurfaceFromPath @0x1fbff8: the file as IMG_LoadPNG / IMG_LoadJPG decode it (no mask).
    DecodedImage logo;
    bool hasLogo = false;
    uint32_t n = 0;
    if (uint8_t* data = FileManager::LoadFile(g_logo.c_str(), n)) {
        hasLogo = DecodeImage(data, n, logo);
        FileManager::FreeFile(data);
    }
    // The screen fills the frame's 580x360 opening at (54, 71): the full width, or the full height
    // (centred) when the screen is wider than 580:360.
    Rect to;
    to.y = 0x47;
    to.h = (int)(580.f / ((float)w / (float)h));
    if (to.h < 0x168) {
        to.h = 0x168;
        to.w = (int)(360.f / ((float)h / (float)w));
        to.x = 0x36 - (to.w - 0x244) / 2;
    } else {
        to.x = 0x36;
        to.w = 0x244;
    }
    BlitBiliniar(screen, Rect{0, 0, w, h}, picture, to);
    ColorCorrection(cover);
    Blit(cover.px.data(), 4, cover.w, cover.h, picture, 0, 0);
    // The logo's rectangle at 0x58dec8: (692, 27), 78x78 (5.11's logo is an opaque palette PNG).
    if (hasLogo && logo.channels >= 3) Blit(logo.px.data(), logo.channels, logo.w, logo.h, picture, 0x2b4, 0x1b);
    FileManager::SaveImage(picture.px, picture.w, picture.h);
}

}  // namespace ViralFeatures
