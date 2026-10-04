# Exact text-rendering stack of libkingdom.so 5.11: FreeType 2.4.5 + SDL_ttf 2.0.11 (see README.md).
cmake_minimum_required(VERSION 3.20)

set(FT ${CMAKE_CURRENT_SOURCE_DIR}/freetype)
add_library(freetype245 STATIC
    ${FT}/src/base/ftsystem.c ${FT}/src/base/ftinit.c ${FT}/src/base/ftdebug.c
    ${FT}/src/base/ftbase.c ${FT}/src/base/ftbbox.c ${FT}/src/base/ftbitmap.c
    ${FT}/src/base/ftglyph.c ${FT}/src/base/ftstroke.c ${FT}/src/base/ftsynth.c
    ${FT}/src/autofit/autofit.c ${FT}/src/truetype/truetype.c ${FT}/src/type1/type1.c
    ${FT}/src/cff/cff.c ${FT}/src/cid/type1cid.c ${FT}/src/type42/type42.c
    ${FT}/src/winfonts/winfnt.c ${FT}/src/pcf/pcf.c ${FT}/src/psaux/psaux.c
    ${FT}/src/psnames/psnames.c ${FT}/src/pshinter/pshinter.c ${FT}/src/raster/raster.c
    ${FT}/src/sfnt/sfnt.c ${FT}/src/smooth/smooth.c ${FT}/src/bdf/bdf.c
    ${FT}/src/gzip/ftgzip.c ${FT}/src/lzw/ftlzw.c)
target_include_directories(freetype245 PUBLIC ${FT}/include PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/ftconfig)
target_compile_definitions(freetype245 PUBLIC DARWIN_NO_CARBON
    PRIVATE FT2_BUILD_LIBRARY "FT_CONFIG_MODULES_H=<ftmodule_rtk.h>" FT_CONFIG_OPTION_SYSTEM_ZLIB)
# PORT: system zlib instead of the bundled 2002 copy (which does not compile on macOS); it only
# serves gzip-compressed PCF fonts, which the game never loads.
find_package(ZLIB REQUIRED)
target_link_libraries(freetype245 PRIVATE ZLIB::ZLIB)
target_compile_options(freetype245 PRIVATE -w)   # 2011 code; warnings are not ours to fix
# DARWIN_NO_CARBON: no Mac resource-fork font support, as on the Android build

add_library(sdl_ttf2011 STATIC sdl_ttf/SDL_ttf.c sdl2_shim/sdl2_shim.c)
target_include_directories(sdl_ttf2011 PUBLIC sdl_ttf sdl2_shim)
target_link_libraries(sdl_ttf2011 PUBLIC freetype245)
target_compile_options(sdl_ttf2011 PRIVATE -w)
