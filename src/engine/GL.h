// PORT: OpenGL for the port's renderer (a 3.2 core context). macOS: Apple's header. Linux and
// Windows: SDL's GL 1.x header, and the newer functions the renderer calls fetched from the driver
// once the context exists (GL::Load), since Windows' opengl32 only has GL 1.1.
#pragma once

#if defined(__APPLE__)
#include <OpenGL/gl3.h>
namespace GL {
inline bool Load() { return true; }
}
#else
#include <SDL3/SDL_opengl.h>

#define RTK_GL_FUNCTIONS(X)                                              \
    X(PFNGLACTIVETEXTUREPROC, glActiveTexture)                           \
    X(PFNGLATTACHSHADERPROC, glAttachShader)                             \
    X(PFNGLBINDATTRIBLOCATIONPROC, glBindAttribLocation)                 \
    X(PFNGLBINDBUFFERPROC, glBindBuffer)                                 \
    X(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray)                       \
    X(PFNGLBLENDEQUATIONPROC, glBlendEquation)                           \
    X(PFNGLBUFFERDATAPROC, glBufferData)                                 \
    X(PFNGLCOMPILESHADERPROC, glCompileShader)                           \
    X(PFNGLCREATEPROGRAMPROC, glCreateProgram)                           \
    X(PFNGLCREATESHADERPROC, glCreateShader)                             \
    X(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray)       \
    X(PFNGLGENBUFFERSPROC, glGenBuffers)                                 \
    X(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays)                       \
    X(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog)                   \
    X(PFNGLGETPROGRAMIVPROC, glGetProgramiv)                             \
    X(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog)                     \
    X(PFNGLGETSHADERIVPROC, glGetShaderiv)                               \
    X(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation)                 \
    X(PFNGLLINKPROGRAMPROC, glLinkProgram)                               \
    X(PFNGLSHADERSOURCEPROC, glShaderSource)                             \
    X(PFNGLUNIFORM1IPROC, glUniform1i)                                   \
    X(PFNGLUNIFORM4FPROC, glUniform4f)                                   \
    X(PFNGLUSEPROGRAMPROC, glUseProgram)                                 \
    X(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer)

// The pointers are rtk_gl*; the gl* names below stand for them (the arguments only meet # and ##,
// so the list itself is not renamed).
#define RTK_GL_DECLARE(type, name) extern type rtk_##name;
RTK_GL_FUNCTIONS(RTK_GL_DECLARE)
#undef RTK_GL_DECLARE
#define glActiveTexture rtk_glActiveTexture
#define glAttachShader rtk_glAttachShader
#define glBindAttribLocation rtk_glBindAttribLocation
#define glBindBuffer rtk_glBindBuffer
#define glBindVertexArray rtk_glBindVertexArray
#define glBlendEquation rtk_glBlendEquation
#define glBufferData rtk_glBufferData
#define glCompileShader rtk_glCompileShader
#define glCreateProgram rtk_glCreateProgram
#define glCreateShader rtk_glCreateShader
#define glEnableVertexAttribArray rtk_glEnableVertexAttribArray
#define glGenBuffers rtk_glGenBuffers
#define glGenVertexArrays rtk_glGenVertexArrays
#define glGetProgramInfoLog rtk_glGetProgramInfoLog
#define glGetProgramiv rtk_glGetProgramiv
#define glGetShaderInfoLog rtk_glGetShaderInfoLog
#define glGetShaderiv rtk_glGetShaderiv
#define glGetUniformLocation rtk_glGetUniformLocation
#define glLinkProgram rtk_glLinkProgram
#define glShaderSource rtk_glShaderSource
#define glUniform1i rtk_glUniform1i
#define glUniform4f rtk_glUniform4f
#define glUseProgram rtk_glUseProgram
#define glVertexAttribPointer rtk_glVertexAttribPointer

namespace GL {
// Fetches the functions above from the current context; false (with a message) if one is missing.
bool Load();
}  // namespace GL
#endif
