#ifndef RTK_SDL2_SHIM_ENDIAN_H
#define RTK_SDL2_SHIM_ENDIAN_H
#include "SDL.h"
static __inline__ Uint16 SDL_Swap16(Uint16 x) { return (Uint16)((x << 8) | (x >> 8)); }
#endif
