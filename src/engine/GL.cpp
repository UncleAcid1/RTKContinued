#include "engine/GL.h"

#if !defined(__APPLE__)
#include <SDL3/SDL_video.h>

#include <cstdio>

#define RTK_GL_DEFINE(type, name) type rtk_##name = nullptr;
RTK_GL_FUNCTIONS(RTK_GL_DEFINE)
#undef RTK_GL_DEFINE

namespace GL {

bool Load() {
    bool ok = true;
#define RTK_GL_LOAD(type, name)                                                 \
    rtk_##name = reinterpret_cast<type>(SDL_GL_GetProcAddress(#name));          \
    if (!rtk_##name) { std::fprintf(stderr, "OpenGL: %s is missing\n", #name); ok = false; }
    RTK_GL_FUNCTIONS(RTK_GL_LOAD)
#undef RTK_GL_LOAD
    return ok;
}
}  // namespace GL
#endif
