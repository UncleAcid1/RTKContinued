// Image decoding: PNG/JPEG via libpng/libjpeg (the libraries the original links), producing
// premultiplied RGBA exactly as Render::GetImageData @0x1fbb80:
//   colour image + separate 8-bit alpha image ("<name>_.png"):  c' = (int)(c * a / 255.0f), A = a
//   32-bit image without mask: same premultiplication with its own alpha.
#pragma once
#include <cstdint>
#include <vector>

struct ImageRGBA {
    int w = 0, h = 0;
    std::vector<uint8_t> px;  // premultiplied RGBA8, row-major, top row first
    bool ok() const { return w > 0 && h > 0; }
};

struct DecodedImage {
    int w = 0, h = 0, channels = 0;  // 1 (gray), 3 (RGB) or 4 (RGBA)
    std::vector<uint8_t> px;
};

bool DecodeImage(const uint8_t* data, uint32_t size, DecodedImage& out);
// Merge colour + optional alpha mask into premultiplied RGBA (GetImageData semantics).
ImageRGBA MakePremultiplied(const DecodedImage& color, const DecodedImage* alpha);
