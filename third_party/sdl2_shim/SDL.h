/* Minimal SDL 2.0 surface/RWops API so SDL_ttf 2.0.11 compiles unmodified without SDL2.
 * Only what SDL_ttf.c uses. Semantics follow SDL 2.0.1 (surface pitch, FillRect, RWops seek/read).
 * PORT: the original game linked SDL2 + SDL_ttf 2.0.11 statically; the port uses SDL3 for the
 * platform layer and this shim only for the text rasteriser. */
#ifndef RTK_SDL2_SHIM_H
#define RTK_SDL2_SHIM_H
#include <stddef.h>
#include <stdint.h>
#include "begin_code.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t Uint8;   typedef int8_t Sint8;
typedef uint16_t Uint16; typedef int16_t Sint16;
typedef uint32_t Uint32; typedef int32_t Sint32;

typedef struct SDL_version { Uint8 major, minor, patch; } SDL_version;
typedef struct SDL_Color { Uint8 r, g, b, a; } SDL_Color;
typedef struct SDL_Palette { int ncolors; SDL_Color *colors; Uint32 version; int refcount; } SDL_Palette;
typedef struct SDL_PixelFormat { Uint32 format; SDL_Palette *palette; Uint8 BitsPerPixel, BytesPerPixel; } SDL_PixelFormat;
typedef struct SDL_Rect { int x, y, w, h; } SDL_Rect;
typedef struct SDL_Surface {
    Uint32 flags; SDL_PixelFormat *format; int w, h; int pitch; void *pixels;
} SDL_Surface;

#define SDL_SWSURFACE 0
#define SDL_SRCCOLORKEY 0x00001000
SDL_Surface *SDL_CreateRGBSurface(Uint32 flags, int w, int h, int depth,
                                  Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask);
#define SDL_AllocSurface SDL_CreateRGBSurface
void SDL_FreeSurface(SDL_Surface *s);
int SDL_FillRect(SDL_Surface *dst, const SDL_Rect *rect, Uint32 color);
int SDL_SetColorKey(SDL_Surface *s, int flag, Uint32 key);

#define RW_SEEK_SET 0
#define RW_SEEK_CUR 1
#define RW_SEEK_END 2
typedef struct SDL_RWops { const Uint8 *base, *here, *stop; } SDL_RWops;  /* memory RWops only */
SDL_RWops *SDL_RWFromMem(const void *mem, int size);
SDL_RWops *SDL_RWFromFile(const char *file, const char *mode);  /* not supported: returns NULL */
long SDL_RWseek(SDL_RWops *ctx, long offset, int whence);
long SDL_RWtell(SDL_RWops *ctx);
size_t SDL_RWread(SDL_RWops *ctx, void *ptr, size_t size, size_t maxnum);
int SDL_RWclose(SDL_RWops *ctx);

int SDL_SetError(const char *fmt, ...);
const char *SDL_GetError(void);

#ifdef __cplusplus
}
#endif
#include "close_code.h"
#endif
