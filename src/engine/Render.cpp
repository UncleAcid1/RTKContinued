#include "engine/Render.h"

#include <OpenGL/gl3.h>
#include <png.h>

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>

#include "engine/FileManager.h"
#include "engine/Image.h"

namespace Render {
namespace {

// ---------------------------------------------------------------------------------------------
// std::sort as compiled into the original (STLport, SGI introsort). Ported so that sprites with
// equal keys end up in the same order as on the original, which a different std::sort would not
// guarantee. Seen in Render::Update @0x1f92c0: __introsort_loop(first,last,0,2*floor(log2 n),cmp),
// then __insertion_sort over the first 16 and unguarded insertion for the rest.
// ---------------------------------------------------------------------------------------------
using Cmp = bool (*)(const Sprite*, const Sprite*);
const int kThreshold = 16;

const Sprite* Median(const Sprite* a, const Sprite* b, const Sprite* c, Cmp comp) {
    if (comp(a, b)) {
        if (comp(b, c)) return b;
        if (comp(a, c)) return c;
        return a;
    }
    if (comp(a, c)) return a;
    if (comp(b, c)) return c;
    return b;
}

Sprite** UnguardedPartition(Sprite** first, Sprite** last, const Sprite* pivot, Cmp comp) {
    for (;;) {
        while (comp(*first, pivot)) ++first;
        --last;
        while (comp(pivot, *last)) --last;
        if (!(first < last)) return first;
        std::swap(*first, *last);
        ++first;
    }
}

void AdjustHeap(Sprite** first, long hole, long len, Sprite* value, Cmp comp) {
    long top = hole, child = 2 * hole + 2;
    while (child < len) {
        if (comp(first[child], first[child - 1])) --child;
        first[hole] = first[child];
        hole = child;
        child = 2 * (child + 1);
    }
    if (child == len) {
        first[hole] = first[child - 1];
        hole = child - 1;
    }
    long parent = (hole - 1) / 2;  // __push_heap
    while (hole > top && comp(first[parent], value)) {
        first[hole] = first[parent];
        hole = parent;
        parent = (hole - 1) / 2;
    }
    first[hole] = value;
}

void HeapSortAll(Sprite** first, Sprite** last, Cmp comp) {  // partial_sort(first,last,last)
    long len = last - first;
    if (len < 2) return;
    for (long parent = (len - 2) / 2;; --parent) {  // make_heap
        AdjustHeap(first, parent, len, first[parent], comp);
        if (parent == 0) break;
    }
    while (last - first > 1) {  // sort_heap
        --last;
        Sprite* v = *last;
        *last = *first;
        AdjustHeap(first, 0, last - first, v, comp);
    }
}

void IntrosortLoop(Sprite** first, Sprite** last, int depth, Cmp comp) {
    while (last - first > kThreshold) {
        if (depth == 0) { HeapSortAll(first, last, comp); return; }
        --depth;
        Sprite** cut = UnguardedPartition(first, last,
                                          Median(*first, *(first + (last - first) / 2), *(last - 1), comp), comp);
        IntrosortLoop(cut, last, depth, comp);
        last = cut;
    }
}

void UnguardedLinearInsert(Sprite** last, Sprite* val, Cmp comp) {
    Sprite** next = last - 1;
    while (comp(val, *next)) {
        *last = *next;
        last = next;
        --next;
    }
    *last = val;
}

void InsertionSort(Sprite** first, Sprite** last, Cmp comp) {
    if (first == last) return;
    for (Sprite** i = first + 1; i != last; ++i) {
        Sprite* val = *i;
        if (comp(val, *first)) {
            std::memmove(first + 1, first, (size_t)(i - first) * sizeof(Sprite*));
            *first = val;
        } else {
            UnguardedLinearInsert(i, val, comp);
        }
    }
}

void StlportSort(Sprite** first, Sprite** last, Cmp comp) {
    if (first == last) return;
    long n = last - first;
    int lg = 0;
    for (long k = n; k != 1; k >>= 1) ++lg;
    IntrosortLoop(first, last, lg * 2, comp);
    if (n > kThreshold) {
        InsertionSort(first, first + kThreshold, comp);
        for (Sprite** i = first + kThreshold; i != last; ++i) UnguardedLinearInsert(i, *i, comp);
    } else {
        InsertionSort(first, last, comp);
    }
}

// @0x1f5c9c Render::SpriteSortZ: a before b iff a.z > b.z
bool SpriteSortZ(const Sprite* a, const Sprite* b) { return a->z > b->z; }
// @0x1f5c7c Render::SpriteSortTex: by texture identity (pointer order on the original).
// PORT: the original compares the Texture's first field; we compare our GL ids, which has the same
// effect (batching), not the same order. Only matters for layers sorted with order 0.
bool SpriteSortTex(const Sprite* a, const Sprite* b) { return a->tex->glId < b->tex->glId; }

// ---------------------------------------------------------------------------------------------
struct Layer {
    std::vector<Sprite*> sprites;  // +0x04/+0x10
    bool sortPending = false;      // +0x5d
    int sortOrder = 0;             // +0x60
};

struct ShaderProg {
    GLuint prog = 0;
    GLint uTransform = -1, uTexture = -1, uColorMask = -1;
};

Layer g_layers[kLayerCount];
std::vector<std::unique_ptr<Texture>> g_textures;
ShaderProg g_shaders[16];
GLuint g_vao = 0, g_vbo = 0;
int g_screenW = 0, g_screenH = 0;
uint32_t g_seq = 0;

// Replace the identifier `word` (whole-word) in src.
std::string ReplaceWord(const std::string& src, const char* word, const char* with) {
    std::string out;
    size_t n = std::strlen(word);
    for (size_t i = 0; i < src.size();) {
        bool startOk = i == 0 || !(std::isalnum((unsigned char)src[i - 1]) || src[i - 1] == '_');
        if (startOk && src.compare(i, n, word) == 0 &&
            (i + n >= src.size() || !(std::isalnum((unsigned char)src[i + n]) || src[i + n] == '_'))) {
            out += with;
            i += n;
        } else {
            out += src[i++];
        }
    }
    return out;
}

// The shipped shaders are GLSL ES 1.00. Desktop core profile needs GLSL 1.50: map attribute/varying,
// texture2D, gl_FragColor; precision qualifiers are legal no-ops in 1.50. Shader logic is unchanged.
std::string ToDesktopGLSL(std::string src, bool fragment) {
    src = ReplaceWord(src, "texture", "s_texture");  // uniform name clashes with texture() in 1.50
    src = ReplaceWord(src, "texture2D", "texture");
    if (fragment) {
        src = ReplaceWord(src, "varying", "in");
        src = ReplaceWord(src, "gl_FragColor", "fragColor");
        return "#version 150\nout vec4 fragColor;\n" + src;
    }
    src = ReplaceWord(src, "attribute", "in");
    src = ReplaceWord(src, "varying", "out");
    return "#version 150\n" + src;
}

GLuint Compile(GLenum type, const std::string& src, const char* name) {
    GLuint s = glCreateShader(type);
    const char* p = src.c_str();
    glShaderSource(s, 1, &p, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, sizeof log, nullptr, log);
        std::fprintf(stderr, "shader %s: %s\n", name, log);
    }
    return s;
}

bool LoadShader(int type, const char* vsName, const char* psName) {
    uint32_t vn = 0, pn = 0;
    std::string vsPath = std::string("shaders/") + vsName, psPath = std::string("shaders/") + psName;
    uint8_t* vs = FileManager::LoadFile(vsPath.c_str(), vn);
    uint8_t* ps = FileManager::LoadFile(psPath.c_str(), pn);
    if (!vs || !ps) {
        std::fprintf(stderr, "missing shader %s/%s\n", vsName, psName);
        FileManager::FreeFile(vs);
        FileManager::FreeFile(ps);
        return false;
    }
    GLuint v = Compile(GL_VERTEX_SHADER, ToDesktopGLSL(std::string((char*)vs, vn), false), vsName);
    GLuint f = Compile(GL_FRAGMENT_SHADER, ToDesktopGLSL(std::string((char*)ps, pn), true), psName);
    FileManager::FreeFile(vs);
    FileManager::FreeFile(ps);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, v);
    glAttachShader(prog, f);
    glBindAttribLocation(prog, 0, "vertex");
    glBindAttribLocation(prog, 1, "texc");
    glBindAttribLocation(prog, 2, "color");
    glBindAttribLocation(prog, 3, "extra");
    glLinkProgram(prog);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(prog, sizeof log, nullptr, log);
        std::fprintf(stderr, "link %s+%s: %s\n", vsName, psName, log);
        return false;
    }
    ShaderProg& sp = g_shaders[type];
    sp.prog = prog;
    sp.uTransform = glGetUniformLocation(prog, "transform");
    sp.uTexture = glGetUniformLocation(prog, "s_texture");
    sp.uColorMask = glGetUniformLocation(prog, "colorMask");
    glUseProgram(prog);
    if (sp.uTexture >= 0) glUniform1i(sp.uTexture, 0);
    if (sp.uColorMask >= 0) glUniform1i(sp.uColorMask, 1);
    return true;
}

}  // namespace

// Shader table from Render::InitMain @0x1f7bd4 (types 0..15). Only types used so far are loaded;
// the rest are recorded here for completeness.
//   0 main/main  1 text/text  2 main/main  3 text/grayscale  4 mask/mask  5 ring/ring
//   6 boss_intro  7 ?/ghostly  8 rotated/text  9 ?/sepia  10 ?/grayscaleTransition  11 color/color
//   12 overlay/overlay  13 rotatedYVS/?  14 scaledVS/?  15 buildingAnimate/buildingAnimate
//   UNVERIFIED: the VS/PS that stay in the sprintf buffers for types 7, 9, 10, 13, 14.
bool Init(int screenW, int screenH) {
    g_screenW = screenW;
    g_screenH = screenH;
    glGenVertexArrays(1, &g_vao);
    glBindVertexArray(g_vao);
    glGenBuffers(1, &g_vbo);
    bool ok = LoadShader(0, "mainVS.txt", "mainPS.txt");
    ok = LoadShader(2, "mainVS.txt", "mainPS.txt") && ok;
    ok = LoadShader(1, "textVS.txt", "textPS.txt") && ok;       // GUI with alpha, text
    ok = LoadShader(3, "textVS.txt", "grayscalePS.txt") && ok;  // disabled GUI
    ok = LoadShader(8, "rotatedVS.txt", "textPS.txt") && ok;    // turned about color.xy by color.z
    // Render::Update: no depth test, premultiplied alpha blending (ONE, ONE_MINUS_SRC_ALPHA).
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    return ok;
}

void Shutdown() {
    for (auto& l : g_layers) {
        for (Sprite* s : l.sprites) delete s;
        l.sprites.clear();
    }
    for (auto& t : g_textures) glDeleteTextures(1, &t->glId);
    g_textures.clear();
}

Texture* CreateTexture(const ImageRGBA& img, const std::string& name, int frames) {
    auto t = std::make_unique<Texture>();
    t->w = img.w;
    t->h = img.h;
    t->name = name;
    t->frames = frames;
    glGenTextures(1, &t->glId);
    glBindTexture(GL_TEXTURE_2D, t->glId);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (img.h < 0x801 || frames < 2) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img.w, img.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, img.px.data());
    } else {
        int colH = (int)((unsigned)img.h / (unsigned)frames) * ((frames + 1) >> 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img.w * 2, colH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, img.w, colH, GL_RGBA, GL_UNSIGNED_BYTE, img.px.data());
        glTexSubImage2D(GL_TEXTURE_2D, 0, img.w, 0, img.w, img.h - colH, GL_RGBA, GL_UNSIGNED_BYTE,
                        img.px.data() + (size_t)colH * img.w * 4);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    g_textures.push_back(std::move(t));
    return g_textures.back().get();
}

void SetWrapping(Texture* t, bool wrap) {
    if (!t) return;
    t->wrap = wrap;
    glBindTexture(GL_TEXTURE_2D, t->glId);
    GLint m = wrap ? GL_REPEAT : GL_CLAMP_TO_EDGE;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, m);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, m);
}

// @0x1fe66c. Without an atlas: size = texture size, u = 0..1 (swapped when mirrored),
// v: +0x30 = 1 (bottom), +0x34 = 0 (top), swapped when flipped. Colours 1.0, shader GetShader(0).
Sprite* CreateSprite(Texture* tex, int layer, bool mirror, bool flip) {
    if (!tex || layer < 0 || layer >= kLayerCount) return nullptr;
    auto* s = new Sprite;
    s->tex = tex;
    s->layer = layer;
    s->w = (float)tex->w;
    s->h = (float)tex->h;
    s->u0 = mirror ? 1.f : 0.f;
    s->u1 = mirror ? 0.f : 1.f;
    s->vBottom = flip ? 0.f : 1.f;
    s->vTop = flip ? 1.f : 0.f;
    s->seq = g_seq++;
    Layer& l = g_layers[layer];
    s->index = (int)l.sprites.size();
    l.sprites.push_back(s);
    return s;
}

Sprite* RemoveSprite(Sprite* s) {
    if (!s) return nullptr;
    Sprite* next = s->next;
    auto& v = g_layers[s->layer].sprites;
    for (size_t i = 0; i < v.size(); ++i) {
        if (v[i] == s) {
            v.erase(v.begin() + (long)i);
            break;
        }
    }
    if (s->isText && s->tex) RemoveTexture(s->tex);   // Layer::RemoveSprite @0x1fc9b8
    delete s;
    return next;
}

void SetPosition(Sprite* s, float x, float y, float z) {
    if (!s) return;
    s->x = x;
    s->y = y;
    s->z = z;
}

// @0x1fe940, case without per-frame textures and sheet height <= 2048 (or < 2 frames):
// u stays 0..1 (mirror honoured), v range selects the frame slice of height frameHeight.
void SetFrame(Sprite* s, int frameHeight, int frame) {
    if (!s || !s->tex) return;
    Texture* t = s->tex;
    float H = (float)t->h;
    s->h = (float)frameHeight;
    if (t->h <= 2048 || t->frames < 2) {
        s->vBottom = ((float)frameHeight + (float)frame * frameHeight) / H;
        s->vTop = ((float)frame * frameHeight) / H;
    } else {  // two columns: first ceil(n/2) frames in the left half
        int perCol = (t->frames + 1) >> 1;
        int col = frame < perCol ? 0 : 1;
        int row = frame % perCol;
        float fh = (float)(t->h / t->frames * perCol);
        s->u0 = col ? 0.5f : 0.f;
        s->u1 = col ? 1.f : 0.5f;
        s->vBottom = ((float)frameHeight + (float)row * frameHeight) / fh;
        s->vTop = ((float)row * frameHeight) / fh;
    }
}

void SetShaderType(Sprite* s, int type) {
    if (s) s->shaderType = type;
}

// @0x1fe940 (no per-frame texture chain, no atlas): size = (w, h); v range = frame slice of height h;
// u = 0..1, swapped when mirrorFrames. Sheets taller than 2048 with 2+ frames use two columns.
void SetFrame(Sprite* s, float h, float w, int frame) {
    if (!s || !s->tex) return;
    Texture* t = s->tex;
    if (s->h == h && s->w == w && s->frame1 == frame + 1) return;
    s->h = h;
    s->w = w;
    s->frame1 = (int16_t)(frame + 1);
    if (t->h <= 0x800 || t->frames < 2) {
        float f = (float)frame, H = (float)t->h;
        s->vBottom = 0.f + (1.f - 0.f) * ((h + f * h) / H);
        s->vTop = 0.f + ((f * h) / H) * (1.f - 0.f);
        s->u0 = s->mirrorFrames ? 1.f : 0.f;
        s->u1 = s->mirrorFrames ? 0.f : 1.f;
    } else {
        int perCol = (t->frames + 1) >> 1;
        float lo = 0.f, hi = 0.5f;
        if (frame >= perCol) { lo = 0.5f; hi = 1.f; }
        s->u0 = s->mirrorFrames ? hi : lo;
        s->u1 = s->mirrorFrames ? lo : hi;
        float row = (float)(frame % perCol);
        float colH = (float)(t->h / t->frames * perCol);
        s->vBottom = (h + row * h) / colH;
        s->vTop = (row * h) / colH;
    }
}

void SetVisibility(Sprite* s, bool v) { if (s) s->visible = v; }

void SetColor(Sprite* s, float r, float g, float b) {
    if (!s) return;
    s->color[0] = r; s->color[1] = g; s->color[2] = b;
}

void SetAlpha(Sprite* s, float a) {
    if (!s) return;
    for (float& c : s->alpha) c = a;
}

// @0x1fdfc8 without atlas: u 0..1 (swapped if mirror), v bottom 1 / top 0 (swapped if flip)
void SetMirror(Sprite* s, bool mirror, bool flip) {
    if (!s) return;
    s->u0 = mirror ? 1.f : 0.f;
    s->u1 = mirror ? 0.f : 1.f;
    s->vBottom = flip ? 0.f : 1.f;
    s->vTop = flip ? 1.f : 0.f;
}

void SetTexture(Sprite* s, Texture* t) {
    if (!s || s->tex == t) return;
    s->tex = t;
    s->frame1 = 0;
}

void SetFrameMirror(Sprite* s, bool mirror) {
    if (!s || s->mirrorFrames == mirror) return;
    s->mirrorFrames = mirror;
    s->u0 = mirror ? 1.f : 0.f;
    s->u1 = mirror ? 0.f : 1.f;
}

bool HasPixelAt(const Sprite* s, float x, float y) {
    float u = (x - s->x) / s->w;
    if (!(0.f <= u && u <= 1.f)) return false;
    float h = std::fabs(s->h);
    float v = (y - (s->y - h)) / h;
    if (!(0.f <= v && v <= 1.f)) return false;
    const Texture* t = s->tex;
    if (!t || t->pixelMask.empty()) return true;
    v = s->vTop + (s->vBottom - s->vTop) * v;
    u = s->u0 + (s->u1 - s->u0) * u;
    // (Atlas sub-images rescale u, v to the part first; the port has no atlases.)
    unsigned col = (unsigned)(int)((float)(t->w - 1) * u);
    unsigned row = (unsigned)(int)((float)(t->h - 1) * v);
    unsigned pitch = (((unsigned)t->w + 1) >> 1) + 7 >> 3;
    return (t->pixelMask[(col >> 4) + pitch * (row >> 1)] >> ((col >> 1) & 7)) & 1;
}

std::vector<uint8_t> GeneratePixelMask(const uint8_t* rgba, int w, int h) {
    unsigned pitch = (((unsigned)w + 1) >> 1) + 7 >> 3;
    std::vector<uint8_t> m((((unsigned)h + 1) >> 1) * pitch, 0);
    for (int y = 0; y < h; y += 2) {
        for (int x = 0, bit = 0; x < w; x += 2, ++bit) {
            unsigned sum = 0;
            for (int r = y; r < y + 2 && r < h; ++r) {
                sum += rgba[((size_t)r * w + x) * 4 + 3];
                if (x + 1 < w) sum += rgba[((size_t)r * w + x + 1) * 4 + 3];
            }
            if (sum > 0x80) m[(size_t)(y >> 1) * pitch + (bit >> 3)] |= (uint8_t)(1 << (bit & 7));
        }
    }
    return m;
}

int GetFrameWidth(const Texture* t) { return t->w; }

int GetFrameHeight(const Texture* t) { return t->frames != 0 ? t->h / t->frames : t->h; }

// The sprite's slot in the old layer gets that layer's last sprite. (The port finds the slot by
// search: sprite indices are not kept up to date.)
void ChangeLayer(Sprite* s, int layer) {
    if (!s || s->layer == layer || layer < 0 || layer >= kLayerCount) return;
    auto& from = g_layers[s->layer].sprites;
    for (size_t i = 0; i < from.size(); ++i) {
        if (from[i] != s) continue;
        g_layers[s->layer].sortPending = true;
        from[i] = from.back();
        from.pop_back();
        s->layer = layer;
        s->index = (int)g_layers[layer].sprites.size();
        g_layers[layer].sprites.push_back(s);
        g_layers[layer].sortPending = true;
        return;
    }
}

Texture* CreateTextureRGBA(int w, int h, const uint8_t* rgba, bool nearest, const std::string& name) {
    auto t = std::make_unique<Texture>();
    t->w = w;
    t->h = h;
    t->name = name;
    glGenTextures(1, &t->glId);
    glBindTexture(GL_TEXTURE_2D, t->glId);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    GLint f = nearest ? GL_NEAREST : GL_LINEAR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, f);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, f);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    g_textures.push_back(std::move(t));
    return g_textures.back().get();
}

void RemoveTexture(Texture* t) {
    for (size_t i = 0; i < g_textures.size(); ++i) {
        if (g_textures[i].get() == t) {
            glDeleteTextures(1, &t->glId);
            g_textures.erase(g_textures.begin() + (long)i);
            return;
        }
    }
}

namespace { bool g_forceLinear = false; float g_baseZoom = 1.f; }
void SetForceLinear(bool on) { g_forceLinear = on; }
bool GetForceLinear() { return g_forceLinear; }
void SetBaseZoomFactor(float z) { g_baseZoom = z; }
float GetBaseZoomFactor() { return g_baseZoom; }

int ScreenWidth() { return g_screenW; }
int ScreenHeight() { return g_screenH; }

void SortRenderLayer(int layer, int order) {
    g_layers[layer].sortOrder = order;
    g_layers[layer].sortPending = true;
}

float offsetX = 0, offsetY = 0, zoom = 0.001f, aspect = 1.f;
namespace { int g_boundMinX, g_boundMaxX, g_boundMinY, g_boundMaxY; }   // 0x613fe0..0x613fec

void SetViewportMapBounds(int minX, int minY, int maxX, int maxY) {
    g_boundMinX = minX;
    g_boundMinY = minY;
    g_boundMaxX = maxX;
    g_boundMaxY = maxY;
}

// Transcribed from the asm (the decompile mixes up the branches). The view may show world x in
// [minX*84-8, maxX*84+8] and world y in [21*minY, 21*maxY] (a map smaller than the screen is
// centred); the zoom is limited so the map fills the screen and half the screen spans at least 240 px.
void ApplyViewportLimit() {
    float k = g_baseZoom;
    float W = (float)g_screenW, H = (float)g_screenH;
    float mapW = 16.f + (float)(g_boundMaxX - g_boundMinX) * 84.f;
    float mapH = (float)(g_boundMaxY - g_boundMinY) * 42.f * 0.5f;
    bool narrowX = mapW * k < W;
    if (narrowX) mapW = W;
    float scaleY = zoom * aspect;
    bool centreY = false;
    float padY = 0.f;
    if (k * mapH < H) {
        padY = (H - k * mapH) * 0.5f;
        centreY = g_boundMinY > 0;
        mapH = H;
    }
    float left = (float)g_boundMinX * 84.f - 8.f;
    float right = 8.f + (float)g_boundMaxX * 84.f;
    float halfW = 1.f / zoom, nhalfW = -1.f / zoom;
    if (narrowX) {
        offsetX = (halfW - right) * 0.5f + (nhalfW - left) * 0.5f;
    } else {
        float x = offsetX;
        if (nhalfW - x < left) offsetX = nhalfW - left;
        if (halfW - x > right) offsetX = halfW - right;
    }
    float top = (float)g_boundMinY * -21.f;
    float halfH = 1.f / scaleY;
    if (centreY) {
        offsetY = (-1.f / scaleY + (0.f + (float)g_boundMaxY * 21.f)) * 0.5f + (halfH - top) * 0.5f;
    } else {
        float y = offsetY;
        if (halfH - y > top) offsetY = y = halfH - top;
        float bottom = -(padY + (float)g_boundMaxY * 21.f);
        if (-1.f / scaleY - y < bottom) offsetY = -1.f / scaleY - bottom;
    }
    float z = zoom;
    if (halfW + halfW > mapW) {
        zoom = z = 1.f / (mapW * 0.5f);
        scaleY = z * aspect;
    }
    if (1.f / scaleY + 1.f / scaleY > mapH) zoom = z = 1.f / (mapH * 0.5f) / aspect;
    if (1.0 / (double)z < 240.0) zoom = 0.0041666668f;
}

float GetDefaultZoom() { return g_baseZoom / (float)(g_screenW / 2); }

void CenterOn(float x, float y) {
    float z = GetDefaultZoom();
    offsetX = -x;
    offsetY = y;
    zoom = z;
}

size_t SpriteCount() {
    size_t n = 0;
    for (auto& l : g_layers) n += l.sprites.size();
    return n;
}

void Frame() {
    glViewport(0, 0, g_screenW, g_screenH);
    glClearColor(0.635f, 0.667f, 0.176f, 1.0f);  // Render::Update clear colour (0x3f22a2a3,...)
    glClear(GL_COLOR_BUFFER_BIT);
    glBindVertexArray(g_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    for (int i = 0; i < 4; ++i) glEnableVertexAttribArray((GLuint)i);
    const GLsizei stride = 0x28;  // pos3 uv2 rgba4 extra1
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)0xc);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)0x14);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, (void*)0x24);

    std::vector<float> verts;
    for (int li = 0; li < kLayerCount; ++li) {
        Layer& L = g_layers[li];
        if (L.sortPending && !L.sprites.empty()) {
            StlportSort(L.sprites.data(), L.sprites.data() + L.sprites.size(),
                        L.sortOrder ? SpriteSortZ : SpriteSortTex);
        }
        // Layer::Prepare @0x1fd028: 6 vertices per sprite; GUI sprites get a +0.25 offset.
        // Vertex y is negated (y-up clip space): bottom edge at -y, top edge at h - y.
        Texture* cur = nullptr;
        int curShader = -1;
        bool curScreen = false;
        verts.clear();
        auto flush = [&]() {
            if (verts.empty() || !cur) return;
            ShaderProg& sp = g_shaders[curShader] .prog ? g_shaders[curShader] : g_shaders[0];
            glUseProgram(sp.prog);
            if (curScreen)
                glUniform4f(sp.uTransform, -g_screenW * 0.5f, g_screenH * 0.5f, 2.f / g_screenW, 2.f / g_screenH);
            else
                glUniform4f(sp.uTransform, offsetX + 0.25f, offsetY + 0.25f, zoom, zoom * aspect);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, cur->glId);
            glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts.size() * sizeof(float)), verts.data(), GL_STREAM_DRAW);
            glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(verts.size() / 10));
            verts.clear();
        };
        for (Sprite* s : L.sprites) {
            if (!s->visible || !s->tex) continue;
            if (s->tex != cur || s->shaderType != curShader || s->screenSpace != curScreen) {
                flush();
                cur = s->tex;
                curShader = s->shaderType;
                curScreen = s->screenSpace;
            }
            // Layer::Prepare @0x1fd028: screen-space sprites get +0.25 on x and y; y is negated.
            float x = s->x, y = s->y;
            if (s->screenSpace) { x += 0.25f; y += 0.25f; }
            float xl = x, xr = x + s->w, yb = -y, yt = s->h - y;
            const float* c = s->color;
            auto V = [&](float px, float py, float u, float v, float a) {
                float d[10] = {px, py, s->z, u, v, c[0], c[1], c[2], a, s->extra};
                verts.insert(verts.end(), d, d + 10);
            };
            V(xl, yb, s->u0, s->vBottom, s->alpha[0]);
            V(xr, yb, s->u1, s->vBottom, s->alpha[1]);
            V(xl, yt, s->u0, s->vTop, s->alpha[2]);
            V(xr, yb, s->u1, s->vBottom, s->alpha[1]);
            V(xl, yt, s->u0, s->vTop, s->alpha[2]);
            V(xr, yt, s->u1, s->vTop, s->alpha[3]);
        }
        flush();
    }
}

bool SaveScreenshot(const char* path) {
    std::vector<uint8_t> px((size_t)g_screenW * g_screenH * 4);
    glReadPixels(0, 0, g_screenW, g_screenH, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    png_image img;
    std::memset(&img, 0, sizeof img);
    img.version = PNG_IMAGE_VERSION;
    img.width = (png_uint_32)g_screenW;
    img.height = (png_uint_32)g_screenH;
    img.format = PNG_FORMAT_RGBA;
    // bottom-up rows: negative stride flips
    return png_image_write_to_file(&img, path, 0, px.data(),
                                   -(png_int_32)(g_screenW * 4), nullptr) != 0;
}

}  // namespace Render
