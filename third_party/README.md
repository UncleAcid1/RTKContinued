# Third-party code (vendored, unmodified unless noted)

The original game (libkingdom.so 5.11) statically links these exact versions; the port uses the same
ones so text renders identically. Versions were read from the binary (`TTF_Linked_Version` returns
2.0.11; `FT_New_Library` stores 2.4.5).

| Directory | Source | License |
|---|---|---|
| `freetype/` | FreeType 2.4.5, `freetype-2.4.5.tar.bz2` from download.savannah.gnu.org/releases/freetype/freetype-old/ (sha256 73e83588...45af2); only `include/`, `src/`, licenses kept | FTL (docs/FTL.TXT) |
| `sdl_ttf/` | SDL_ttf 2.0.11, github.com/libsdl-org/SDL_ttf tag `release-2.0.11` (sha256 2021ed55...c3c); `SDL_ttf.c/.h` + docs | zlib (COPYING) |
| `sdl2_shim/` | Written for the port: the few SDL 2.0 surface/RWops functions SDL_ttf.c needs, so it compiles without SDL2 | same as the port |
| `ftconfig/ftmodule_rtk.h` | Written for the port: the game's FreeType module list (2.4.5 default minus pfr) | same as the port |
