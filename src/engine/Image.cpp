#include "engine/Image.h"

#include <png.h>
#include <cstdio>
#include <jpeglib.h>
#include <csetjmp>
#include <cstring>

namespace {

bool DecodePNG(const uint8_t* data, uint32_t size, DecodedImage& out) {
    png_image img;
    std::memset(&img, 0, sizeof img);
    img.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_memory(&img, data, size)) return false;
    bool hasAlpha = (img.format & PNG_FORMAT_FLAG_ALPHA) != 0;
    bool color = (img.format & PNG_FORMAT_FLAG_COLOR) != 0;
    if (!color && !hasAlpha) { img.format = PNG_FORMAT_GRAY; out.channels = 1; }
    else if (!hasAlpha) { img.format = PNG_FORMAT_RGB; out.channels = 3; }
    else { img.format = PNG_FORMAT_RGBA; out.channels = 4; }
    out.w = (int)img.width;
    out.h = (int)img.height;
    out.px.resize(PNG_IMAGE_SIZE(img));
    if (!png_image_finish_read(&img, nullptr, out.px.data(), 0, nullptr)) {
        png_image_free(&img);
        return false;
    }
    return true;
}

struct JpegErr {
    jpeg_error_mgr mgr;
    jmp_buf jb;
};
void JpegFail(j_common_ptr c) { longjmp(((JpegErr*)c->err)->jb, 1); }

bool DecodeJPEG(const uint8_t* data, uint32_t size, DecodedImage& out) {
    jpeg_decompress_struct cinfo;
    JpegErr err;
    cinfo.err = jpeg_std_error(&err.mgr);
    err.mgr.error_exit = JpegFail;
    if (setjmp(err.jb)) { jpeg_destroy_decompress(&cinfo); return false; }
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, data, size);
    jpeg_read_header(&cinfo, TRUE);
    cinfo.out_color_space = cinfo.num_components == 1 ? JCS_GRAYSCALE : JCS_RGB;
    jpeg_start_decompress(&cinfo);
    out.w = (int)cinfo.output_width;
    out.h = (int)cinfo.output_height;
    out.channels = cinfo.output_components;
    out.px.resize((size_t)out.w * out.h * out.channels);
    while (cinfo.output_scanline < cinfo.output_height) {
        JSAMPROW row = out.px.data() + (size_t)cinfo.output_scanline * out.w * out.channels;
        jpeg_read_scanlines(&cinfo, &row, 1);
    }
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    return true;
}

}  // namespace

bool DecodeImage(const uint8_t* data, uint32_t size, DecodedImage& out) {
    if (size >= 8 && std::memcmp(data, "\x89PNG", 4) == 0) return DecodePNG(data, size, out);
    if (size >= 2 && data[0] == 0xFF && data[1] == 0xD8) return DecodeJPEG(data, size, out);
    return false;
}

ImageRGBA MakePremultiplied(const DecodedImage& color, const DecodedImage* alpha) {
    ImageRGBA r;
    r.w = color.w;
    r.h = color.h;
    r.px.resize((size_t)r.w * r.h * 4);
    const bool useMask = alpha && alpha->w == color.w && alpha->h == color.h;
    for (int y = 0; y < r.h; ++y) {
        for (int x = 0; x < r.w; ++x) {
            size_t i = (size_t)y * r.w + x;
            const uint8_t* c = color.px.data() + i * color.channels;
            uint8_t cr, cg, cb, a;
            if (color.channels >= 3) { cr = c[0]; cg = c[1]; cb = c[2]; }
            else { cr = cg = cb = c[0]; }
            if (useMask) a = alpha->px[i * alpha->channels];        // mask: first channel
            else a = color.channels == 4 ? c[3] : 255;
            uint8_t* o = r.px.data() + i * 4;
            // (int)(c*a / 255.0f), negative-guarded as in the original
            o[0] = (uint8_t)(int)((float)(cr * a) / 255.0f);
            o[1] = (uint8_t)(int)((float)(cg * a) / 255.0f);
            o[2] = (uint8_t)(int)((float)(cb * a) / 255.0f);
            o[3] = a;
        }
    }
    return r;
}
