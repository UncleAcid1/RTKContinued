/* Implementation of the minimal SDL 2.0 API in SDL.h (see there). */
#include "SDL.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char g_error[256];

int SDL_SetError(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); vsnprintf(g_error, sizeof g_error, fmt, ap); va_end(ap);
    return -1;
}
const char *SDL_GetError(void) { return g_error; }

/* SDL 2.0 SDL_CalculatePitch: bytes per row rounded up to 4 */
static int CalculatePitch(int w, int depth) {
    int pitch = w * ((depth + 7) / 8);
    if (depth < 8) pitch = (w * depth + 7) / 8;
    return (pitch + 3) & ~3;
}

SDL_Surface *SDL_CreateRGBSurface(Uint32 flags, int w, int h, int depth,
                                  Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask) {
    (void)Rmask; (void)Gmask; (void)Bmask; (void)Amask;
    SDL_Surface *s = (SDL_Surface *)calloc(1, sizeof *s);
    if (!s) return NULL;
    s->format = (SDL_PixelFormat *)calloc(1, sizeof *s->format);
    s->format->BitsPerPixel = (Uint8)depth;
    s->format->BytesPerPixel = (Uint8)((depth + 7) / 8);
    if (depth == 8) {  /* SDL_AllocFormat gives indexed surfaces a 256-colour palette */
        s->format->palette = (SDL_Palette *)calloc(1, sizeof(SDL_Palette));
        s->format->palette->ncolors = 256;
        s->format->palette->colors = (SDL_Color *)calloc(256, sizeof(SDL_Color));
    }
    s->flags = flags; s->w = w; s->h = h;
    s->pitch = CalculatePitch(w, depth);
    s->pixels = calloc((size_t)s->pitch * (size_t)(h > 0 ? h : 1), 1);  /* SDL zeroes new pixels */
    return s;
}

void SDL_FreeSurface(SDL_Surface *s) {
    if (!s) return;
    if (s->format) {
        if (s->format->palette) { free(s->format->palette->colors); free(s->format->palette); }
        free(s->format);
    }
    free(s->pixels); free(s);
}

int SDL_FillRect(SDL_Surface *dst, const SDL_Rect *rect, Uint32 color) {
    int x0 = 0, y0 = 0, x1 = dst->w, y1 = dst->h, x, y;
    if (rect) { x0 = rect->x; y0 = rect->y; x1 = rect->x + rect->w; y1 = rect->y + rect->h; }
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > dst->w) x1 = dst->w;
    if (y1 > dst->h) y1 = dst->h;
    for (y = y0; y < y1; ++y) {
        Uint8 *row = (Uint8 *)dst->pixels + y * dst->pitch;
        for (x = x0; x < x1; ++x) {
            switch (dst->format->BytesPerPixel) {
                case 1: row[x] = (Uint8)color; break;
                case 2: ((Uint16 *)row)[x] = (Uint16)color; break;
                default: ((Uint32 *)row)[x] = color; break;
            }
        }
    }
    return 0;
}

int SDL_SetColorKey(SDL_Surface *s, int flag, Uint32 key) { (void)s; (void)flag; (void)key; return 0; }

SDL_RWops *SDL_RWFromMem(const void *mem, int size) {
    SDL_RWops *rw = (SDL_RWops *)malloc(sizeof *rw);
    rw->base = rw->here = (const Uint8 *)mem;
    rw->stop = rw->base + size;
    return rw;
}
SDL_RWops *SDL_RWFromFile(const char *file, const char *mode) {
    (void)file; (void)mode; SDL_SetError("SDL_RWFromFile is not supported by the shim"); return NULL;
}
long SDL_RWseek(SDL_RWops *c, long offset, int whence) {  /* SDL 2.0 mem_seek */
    const Uint8 *p;
    switch (whence) {
        case RW_SEEK_SET: p = c->base + offset; break;
        case RW_SEEK_CUR: p = c->here + offset; break;
        case RW_SEEK_END: p = c->stop + offset; break;
        default: return SDL_SetError("Unknown value for 'whence'");
    }
    if (p < c->base) p = c->base;
    if (p > c->stop) p = c->stop;
    c->here = p;
    return (long)(c->here - c->base);
}
long SDL_RWtell(SDL_RWops *c) { return (long)(c->here - c->base); }
size_t SDL_RWread(SDL_RWops *c, void *ptr, size_t size, size_t maxnum) {  /* SDL 2.0 mem_read */
    size_t total = maxnum * size, avail;
    if (maxnum == 0 || size == 0 || total / maxnum != size) return 0;
    avail = (size_t)(c->stop - c->here);
    if (total > avail) total = avail;
    memcpy(ptr, c->here, total);
    c->here += total;
    return total / size;
}
int SDL_RWclose(SDL_RWops *c) { free(c); return 0; }
