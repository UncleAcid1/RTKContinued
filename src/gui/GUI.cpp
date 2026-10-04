#include "gui/GUI.h"

#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <vector>

#include "engine/FileManager.h"
#include "engine/Render.h"
#include "engine/Resources.h"
#include "engine/Text.h"

namespace GUI {
namespace {

// Screen configuration (SDL_baseInit @0x18b1e0). Defaults are the original's initial values.
int g_screenW = 1024, g_screenH = 768;   // 0x60ef6c, 0x60ef70
bool g_highDPI = false;                  // 0x612424 (CommonSetFlags 1000)
bool g_smallScreen = false;              // 0x612428
float g_highDPIScale = 1.25f;            // 0x60ef38
float g_hoverScale = 1.25f;              // 0x60ef3c
float g_hudScale = 1.25f;                // 0x60ef40
float g_fontScale = 1.f;                 // 0x60ef44 (GUI::Init's float argument, 1.0 in the game)

const char* g_fontFile = "fonts/ARICYRB.ttf";
Window* g_desktop = nullptr;             // GUI::Init's template objects: SetTexture/SetFrame ignore them
Textfield* g_defaultText = nullptr;
Button* g_defaultButton = nullptr;
Window* g_capture = nullptr;

// Global registry: every registered window (index stored in +0x70) and the name table
// (1024-bucket chained hash keyed by StringHash, new entries at the head, so the newest window of a
// name is found first). GetWindow additionally requires the window's root to match.
std::vector<Window*> g_all;
std::unordered_map<std::string, std::vector<Window*>> g_byName;

void Register(Window* w) {
    g_byName[w->name].push_back(w);
    w->index = (int)g_all.size();
    g_all.push_back(w);
}

void Unregister(Window* w) {
    auto it = g_byName.find(w->name);
    if (it != g_byName.end()) {
        auto& v = it->second;
        for (size_t i = v.size(); i-- > 0;)
            if (v[i] == w) { v.erase(v.begin() + (long)i); break; }
    }
    if (w->index >= 0 && w->index < (int)g_all.size()) {   // swap-remove, as ~Window @0x17dab4
        Window* last = g_all.back();
        g_all.pop_back();
        if (last != w) { last->index = w->index; g_all[(size_t)w->index] = last; }
    }
    w->index = -1;
}

int ShaderFor(const Window* w) { return !w->enabled ? 3 : (w->alpha == 1.f ? 0 : 1); }

}  // namespace

// ---------------------------------------------------------------------------------------------
// Screen configuration

// The Android path of SDL_baseInit (no command-line switches): flags and scale factors from the
// device screen size in pixels. The original then creates the window at that size.
void SetScreenSize(int w, int h) {
    g_screenW = w;
    g_screenH = h;
    int mx = w > h ? w : h, mn = w > h ? h : w;
    if (mx < 600) {                          // @0x18bb30 small screens
        g_highDPI = true;                    // CommonSetFlags(1000, 1)
        float f = ((float)w / 800.f) * 1.25f;
        g_hoverScale = f <= 1.f ? f : 1.f;
        g_hudScale = (float)w / 800.f;
        Render::SetBaseZoomFactor((float)w / 800.f);
        g_smallScreen = true;
    }
    if (mx > 0x739 || mn >= 1000) {          // @0x18b640 large screens
        // (The original first sets base zoom 1.9 or 1.425, then overwrites it with 1.5.)
        g_highDPI = true;
        Render::SetBaseZoomFactor(1.5f);
        g_highDPIScale = 2.f;
        g_hoverScale = 2.f;
        g_hudScale = 1.6f;
        Render::SetForceLinear(true);
    }
}
int ScreenWidth() { return g_screenW; }
int ScreenHeight() { return g_screenH; }
bool IsTabletVersion() { return (g_screenH < g_screenW ? g_screenH : g_screenW) >= 600; }
bool IsHighDPIVersion() { return g_highDPI; }
bool IsSmallScreenVersion() { return g_smallScreen; }
float GetHighDPIScaleFactor() { return g_highDPIScale; }
float GetHudScaleFactor() { return g_highDPI ? g_hudScale : 1.f; }
float GetHoverScaleFactor(float normal, float scale) { return g_highDPI ? scale * g_hoverScale : normal; }
int GetVerticalCenter(int h) { return (g_screenH - h) / 2; }

// @0x176760 (the phone flag is unused in this build)
float GetScaleFactor(int w, int h, bool phone, float scale) {
    (void)phone;
    float s = g_highDPI ? g_highDPIScale : scale;
    if (s * (float)h > (float)g_screenH) s = (float)g_screenH / (float)h;
    if ((float)w * s > (float)g_screenW) s = (float)g_screenW / (float)w;
    return s;
}

Window* GetCapture() { return g_capture; }
void SetCapture(Window* w) { g_capture = w; }
void ReleaseCapture() { g_capture = nullptr; }

// ---------------------------------------------------------------------------------------------
// Window

Window::Window() = default;   // field initialisers = Window::Window @0x175b00

// @0x17d980 (partial): callback, sprite chain and registry entries are released.
// UNVERIFIED: the rest of the destructor (highlight/blocker lists, children) is not read yet;
// children are not deleted here.
Window::~Window() {
    while (sprite) sprite = Render::RemoveSprite(sprite);
    Unregister(this);
    delete sound;
}

// @0x175cc0: children (whose positions are root-relative) move with their parent.
void Window::SetPosition(int nx, int ny) {
    int16_t ox = x, oy = y;
    x = (int16_t)nx;
    y = (int16_t)ny;
    if (parent)
        for (Window* c = firstChild; c; c = c->next) c->SetPosition(nx - ox + c->x, ny - oy + c->y);
    UpdatePosition();
}

void Window::AddChild(Window* c) {   // @0x175d38
    c->parent = this;
    if (!firstChild) {
        firstChild = lastChild = c;
    } else {
        Window* l = lastChild;
        lastChild = c;
        l->next = c;
        c->prev = l;
    }
}

void Window::PrependChild(Window* c) {   // @0x175d60
    Window* f = firstChild;
    c->parent = this;
    firstChild = c;
    if (!f) {
        lastChild = c;
    } else {
        f->prev = c;
        c->next = f;
    }
}

Window* Window::GetChild(const char* n) {   // @0x178af8: matches the last name component or the full name
    for (Window* c = firstChild; c; c = c->next) {
        if (c->name.empty()) continue;
        size_t dot = c->name.rfind('.');
        if (dot != std::string::npos && c->name.compare(dot + 1, std::string::npos, n) == 0) return c;
        if (c->name == n) return c;
    }
    return nullptr;
}

void Window::SetClipRect(ClipRect* r) { clip = r; }   // @0x175d80
void Window::SetOnClick(Callback cb) { onClick = std::move(cb); }   // @0x175d88
void Window::MoveWindow(int dx, int dy) { SetPosition(dx + x, dy + y); }   // @0x175dbc

void Window::RestorePosition() {   // @0x175de0
    if (root) {
        float s = root->scale;
        y = (int16_t)(int)((float)origY * s);
        x = (int16_t)(int)((float)origX * s);
    }
    for (Window* c = firstChild; c; c = c->next) c->RestorePosition();
}

void Window::SetBorders(unsigned l, unsigned t, unsigned r, unsigned b) {   // @0x178478
    border[0] = (int16_t)l;
    border[1] = (int16_t)t;
    border[2] = (int16_t)r;
    border[3] = (int16_t)b;
    Update9Slices();
}

void Window::SetSize(unsigned nw, unsigned nh) {   // @0x178418
    float s = RootScale();
    h = (int16_t)nh;
    w = (int16_t)nw;
    origW = (int16_t)(int)((float)nw / s);
    origH = (int16_t)(int)((float)nh / s);
    Update9Slices();
}

// @0x178490
void Window::UpdatePosition() {
    if (sprite) {
        if (!sprite->next) {
            int rx = root ? root->x : 0, ry = root ? root->y : 0;
            float cx = centerSprite ? ((float)w - sprite->w) * 0.5f : 0.f;
            float cy = centerSprite ? ((float)h - sprite->h) * 0.5f : 0.f;
            float px = (float)(int)((float)(rx + x) + cx);
            float py = (float)((int)((float)(ry + y) - cy) + h);
            Render::SetPosition(sprite, px - 0.25f, py - 0.25f, z);
        } else {
            Update9Slices();
        }
    }
    for (Window* c = firstChild; c; c = c->next) c->UpdatePosition();
    if (clip) ClipWithRect(clip->left, clip->top, clip->right, clip->bottom);
}

// @0x178868: the root keeps a running depth (+0x1c); every child that draws takes the next one,
// 0.0001 below the previous, so later (deeper in the tree) windows draw on top.
void Window::SetZ(float nz) {
    if (!parent) zNext = nz;
    z = nz;
    if (!sprite) {
        if (takesZ && parent) { root->zNext -= 0.0001f; z = root->zNext; }
    } else if (!sprite->next) {
        if (parent) { root->zNext -= 0.0001f; z = root->zNext; }
        int rx = root ? root->x : 0, ry = root ? root->y : 0;
        float cx = centerSprite ? ((float)w - sprite->w) * 0.5f : 0.f;
        float cy = centerSprite ? ((float)h - sprite->h) * 0.5f : 0.f;
        float px = (float)(int)((float)(rx + x) + cx);
        float py = (float)((int)((float)(ry + y) - cy) + h);
        // UNVERIFIED: the decompile of SetZ shows no -0.25 here (unlike UpdatePosition); the
        // asm of 0x178868 still has to be checked.
        Render::SetPosition(sprite, px, py, z);
    } else {
        if (parent) { root->zNext -= 0.0001f; z = root->zNext; }
        Update9Slices();
    }
    for (Window* c = firstChild; c; c = c->next) c->SetZ(nz);
    if (!parent && (zNext < 0.f || nz > 1.f))
        std::printf("GUI: window %s z range exhausted (%f)\n", name.c_str(), zNext);
}

// @0x179094
void Window::SetVisibility(bool v) {
    visibleSelf = v;
    if (parent && v) v = parent->visible;
    visible = v;
    if (sprite) {
        if (!sprite->next) Render::SetVisibility(sprite, visible);
        else Update9Slices();
    }
    for (Window* c = firstChild; c; c = c->next) c->SetVisibility(c->visibleSelf);
    if (!visible && g_capture == this) g_capture = nullptr;
}

void Window::SetEnabled(bool e) {   // @0x1770bc
    if (enabled == e) return;
    enabled = e;
    if (sprite) Render::SetShaderType(sprite, ShaderFor(this));
    for (Window* c = firstChild; c; c = c->next) c->SetEnabled(e);
}

void Window::SetAlpha(float a, bool recursive) {   // @0x1771ac
    alpha = a;
    for (Render::Sprite* s = sprite; s; s = s->next) {
        Render::SetAlpha(s, a);
        Render::SetShaderType(s, ShaderFor(this));
    }
    if (recursive)
        for (Window* c = firstChild; c; c = c->next) c->SetAlpha(a, true);
}

// @0x179c2c: rotation shader 8 takes the centre in colour r/g and the angle in b.
// UNVERIFIED: shader type 8 (rotatedVS) is not loaded by the port yet.
void Window::SetRotation(float angle) {
    if (!sprite) return;
    sprite->color[2] = angle;
    sprite->color[0] = sprite->x + sprite->w * 0.5f;
    sprite->color[1] = sprite->h * 0.5f - sprite->y;
    Render::SetShaderType(sprite, 8);
}

void Window::SetScreenSpace(bool ss) {   // @0x179b84
    screenSpace = ss;
    if (sprite) {
        if (!sprite->next) sprite->screenSpace = ss;
        else Update9Slices();
    }
    for (Window* c = firstChild; c; c = c->next) c->SetScreenSpace(ss);
}

void Window::SetCustomShader(int type, bool recursive) {   // @0x17704c
    for (Render::Sprite* s = sprite; s; s = s->next) Render::SetShaderType(s, type);
    if (recursive)
        for (Window* c = firstChild; c; c = c->next) c->SetCustomShader(type, true);
}

void Window::SetColor(float r, float g, float b, bool recursive) {   // @0x17b488
    for (Render::Sprite* s = sprite; s; s = s->next) Render::SetColor(s, r, g, b);
    if (recursive)
        for (Window* c = firstChild; c; c = c->next) c->SetColor(r, g, b, true);
}

void Window::SetExtra(float extra, bool recursive) {   // @0x179b14 (stores the bool as the value)
    for (Render::Sprite* s = sprite; s; s = s->next) s->extra = extra;
    if (recursive)
        for (Window* c = firstChild; c; c = c->next) c->SetExtra(extra, true);
}

// @0x17b0d0
void Window::SetTexture(Render::Texture* tex, bool keepSize, unsigned tw, unsigned th, bool force,
                        unsigned frameIdx) {
    if (this == g_desktop || this == g_defaultText || this == g_defaultButton) return;
    if (texture == tex && !force && frame == frameIdx) return;
    if (!tex) {
        Render::RemoveSprite(sprite);
        texture = nullptr;
        sprite = nullptr;
        return;
    }
    texture = tex;
    centerSprite = keepSize;
    if (!sprite) {
        sprite = Render::CreateSprite(tex, Render::kLayerGUI, false, false);
    } else {
        Render::SetTexture(sprite, tex);
        Render::SetMirror(sprite, false, false);
    }
    Render::SetShaderType(sprite, ShaderFor(this));
    sprite->screenSpace = screenSpace;
    sprite->visible = visible;
    if (tw == 0 && th == 0) {
        tw = (unsigned)tex->w;
        th = (unsigned)tex->h;
    } else {
        if (tw == 0) tw = (unsigned)((float)tex->w * ((float)th / (float)tex->h));
        if (th == 0) th = (unsigned)((float)tex->h * ((float)tw / (float)tex->w));
    }
    unk2a = 0;
    unk28 = 0;
    if (keepSize) {
        float s = RootScale();
        sprite->w = (float)tw * s;
        sprite->h = (float)th * s;
    } else {
        sprite->w = (float)w;
        sprite->h = (float)h;
    }
    frame = (uint8_t)frameIdx;
    if (tex->frames >= 2) {
        Render::SetFrame(sprite, (float)(tex->h / tex->frames), (float)tex->w, frameIdx & 0xff);
        float s = RootScale();
        sprite->w *= s;
        sprite->h *= s;
    }
    int rx = root ? root->x : 0, ry = root ? root->y : 0;
    float cx = keepSize ? ((float)w - sprite->w) * 0.5f : 0.f;
    float cy = keepSize ? ((float)h - sprite->h) * 0.5f : 0.f;
    float px = (float)(int)((float)(rx + x) + cx);
    float py = (float)((int)((float)(ry + y) - cy) + h);
    Render::SetPosition(sprite, px - 0.25f, py - 0.25f, z);
}

// @0x178c44: frame f of a layout image is "<dir>/<RootSymbol>___<name>_f<f>.png" (f 0: no suffix).
void Window::SetFrame(unsigned f, bool keep) {
    if (this == g_desktop || this == g_defaultText || this == g_defaultButton || !root) return;
    std::string n = name;
    for (char& c : n) if (c == '.') c = '_';
    std::string dir = root->name;
    size_t slash = dir.rfind('/');
    if (slash != std::string::npos) dir.resize(slash);
    char buf[512];
    if (f == 0) std::snprintf(buf, sizeof buf, "%s/%s___%s.png", dir.c_str(), root->symbol.c_str(), n.c_str());
    else std::snprintf(buf, sizeof buf, "%s/%s___%s_f%d.png", dir.c_str(), root->symbol.c_str(), n.c_str(), f);
    SetTexture(Resources::GetUIImage(buf, true, false), keep, 0, 0, false, 0);
}

void Window::ReloadTextures() {}   // @0x175db8 (empty)
void Window::PreloadTextures() {}  // @0x17aed4: textures load synchronously in the port

// @0x17ae08 UNVERIFIED: mask shader (type 4) not used by the HUD; not ported.
void Window::ApplyMask(Render::Texture*) {}

// @0x17e260 UNVERIFIED: sounds are not ported yet; the sound name is kept.
void Window::SetActionSound(const char* n, int param) {
    if (!sound) sound = new ActionSound;
    sound->name = n ? n : "";
    sound->param = param;
}

void Window::RestoreSize() {   // @0x179978
    if (!root) {
        w = (int16_t)(int)((float)origW * scale);
        h = (int16_t)(int)((float)origH * scale);
        if (sprite && sprite->tex) {
            sprite->w = (float)sprite->tex->w * scale;
            sprite->h = (float)sprite->tex->h * scale;
        }
    } else {
        float s = root->scale;
        unk28 = unk2a = 0;
        w = (int16_t)(int)((float)origW * s);
        h = (int16_t)(int)((float)origH * s);
        if (sprite) {
            if (!centerSprite) {
                sprite->h = (float)h;
                sprite->w = (float)w;
            } else if (sprite->tex) {
                sprite->w = (float)sprite->tex->w * s;
                sprite->h = (float)sprite->tex->h * root->scale;
            }
        }
    }
    for (Window* c = firstChild; c; c = c->next) c->RestoreSize();
}

void Window::RescaleWindow(float ns) {   // @0x17a320
    if (!root) {
        scale = ns;
        w = (int16_t)(int)(ns * (float)origW);
        h = (int16_t)(int)(ns * (float)origH);
        if (sprite && sprite->tex) {
            sprite->w = ns * (float)sprite->tex->w;
            sprite->h = (float)sprite->tex->h * scale;
        }
    } else {
        float s = root->scale;
        unk28 = unk2a = 0;
        x = (int16_t)(int)((float)origX * s);
        y = (int16_t)(int)((float)origY * s);
        w = (int16_t)(int)((float)origW * s);
        h = (int16_t)(int)((float)origH * s);
        if (sprite) {
            if (!centerSprite) {
                sprite->h = (float)h;
                sprite->w = (float)w;
            } else if (sprite->tex) {
                sprite->w = (float)sprite->tex->w * s;
                sprite->h = (float)sprite->tex->h * root->scale;
            }
            if (texture && texture->frames > 1) {
                Render::SetFrame(sprite, (float)(texture->h / texture->frames), (float)texture->w, frame);
                sprite->w *= RootScale();
                sprite->h *= RootScale();
            }
        }
    }
    for (Window* c = firstChild; c; c = c->next) c->RescaleWindow(ns);
}

// @0x17a748 UNVERIFIED: scroll-area clipping (UV trimming) is not ported yet; the rectangle is
// passed on to children as the original does.
void Window::ClipWithRect(int l, int t, int r, int b) {
    if (root && !root->ignoreClip) {
        b -= root->y; t -= root->y; r -= root->x; l -= root->x;
    }
    for (Window* c = firstChild; c; c = c->next) {
        if (!c->clip) {
            c->ClipWithRect(l, t, r, b);
        } else {
            int cl = l < c->clip->left ? c->clip->left : l;
            int ct = t < c->clip->top ? c->clip->top : t;
            int cr = c->clip->right <= r ? c->clip->right : r;
            int cb = c->clip->bottom <= b ? c->clip->bottom : b;
            c->ClipWithRect(cl, ct, cl < cr ? cr : cl, ct < cb ? cb : ct);
        }
    }
}

// @0x17af34 (hit test). pixelHitTest needs the texture's hit mask (Render::Sprite::HasPixelAt).
// UNVERIFIED: hit masks are not built by the port's texture loader; such windows hit by rectangle.
Window* Window::GetWindowAtPosition(int px, int py, bool clickable) {
    bool inside = !(px < x || w + x < px || py < y) && py <= h + y;
    if (noInput || !inside) return nullptr;
    for (Window* c = lastChild; c; c = c->prev) {
        int cx = px, cy = py;
        if (!parent) { cx = px - x; cy = py - y; }
        if (Window* hit = c->GetWindowAtPosition(cx, cy, clickable)) return hit;
    }
    if (!visible || !enabled) return nullptr;
    if (clickable ? !onClick : !parent) return nullptr;
    return this;
}

// @0x17cc30 is ported with input in milestone 2d.
bool Window::Click(int, int, bool, bool) { return false; }

// @0x17745c: nine sprites (left, right, top, bottom edges; four corners; centre), chained through
// Sprite::next. Insets come from the layout (scaled by the root scale) and shrink proportionally
// when the window is smaller than the two insets together.
void Window::Update9Slices() {
    if (!texture || !(border[0] || border[1] || border[2] || border[3])) return;
    // (The original tests the two 32-bit words at +0x78/+0x7c, i.e. any inset non-zero.)
    Render::Sprite* old[9] = {};
    if (!sprite->next) {
        Render::RemoveSprite(sprite);
    } else {
        int i = 0;
        for (Render::Sprite* s = sprite; s && i < 9; s = s->next) old[i++] = s;
    }
    sprite = nullptr;
    const int TW = texture->w, TH = texture->h;
    int L = border[0], T = border[1], R = border[2], B = border[3];
    if (R < 0) R += TW;
    if (B < 0) B += TH;
    float s = RootScale();
    int l = (int)(s * (float)L), r = (int)(s * (float)R), t = (int)(s * (float)T), b = (int)(s * (float)B);
    if (w < r + l) {
        float k = (float)w / (float)(r + l);
        r = (int)((float)R * k);
        l = (int)((float)L * k);
    }
    if (h < b + t) {
        float k = (float)h / (float)(b + t);
        b = (int)((float)B * k);
        t = (int)((float)T * k);
    }
    float X0 = (float)x + (root ? (float)root->x : 0.f);
    float Y0 = (float)y + (root ? (float)root->y : 0.f);
    const float fTW = (float)TW, fTH = (float)TH;
    const float uL = (float)L / fTW, uR = (float)(TW - R) / fTW;   // u at the inner edges
    const float vT = (float)T / fTH, vB = (float)(TH - B) / fTH;   // v at the inner edges
    struct Slice { float u0, u1, vBot, vTop, sw, sh, px, py; };
    const float midW = (float)((w - 1) - l - r), midH = (float)((h - 1) - t - b);
    const Slice sl[9] = {
        {0.f, uL, vB, vT, (float)l, midH, 0.f, (float)(h - 1 - b)},                 // left edge
        {uR, 1.f, vB, vT, (float)r, midH, (float)(w - 1 - r), (float)(h - 1 - b)},  // right edge
        {uL, uR, vT, 0.f, midW, (float)t, (float)l, (float)t},                      // top edge
        {uL, uR, 1.f, vB, midW, (float)b, (float)l, (float)(h - 1)},                // bottom edge
        {0.f, uL, vT, 0.f, (float)l, (float)t, 0.f, (float)t},                      // top-left
        {uR, 1.f, vT, 0.f, (float)r, (float)t, (float)(w - 1 - r), (float)t},       // top-right
        {0.f, uL, 1.f, vB, (float)l, (float)b, 0.f, (float)(h - 1)},                // bottom-left
        {uR, 1.f, 1.f, vB, (float)r, (float)b, (float)(w - 1 - r), (float)(h - 1)}, // bottom-right
        {uL, uR, vB, vT, midW, midH, (float)l, (float)(h - 1 - b)},                 // centre
    };
    Render::Sprite* prev = nullptr;
    int count = unk5f ? 8 : 9;   // +0x5f: no centre piece
    for (int i = 0; i < count; ++i) {
        Render::Sprite* sp = old[i] ? old[i] : Render::CreateSprite(texture, Render::kLayerGUI, false, false);
        if (prev) prev->next = sp; else sprite = sp;
        Render::SetShaderType(sp, ShaderFor(this));
        sp->screenSpace = screenSpace;
        sp->visible = visible;
        sp->u0 = sl[i].u0; sp->u1 = sl[i].u1;
        sp->vBottom = sl[i].vBot; sp->vTop = sl[i].vTop;
        sp->w = sl[i].sw; sp->h = sl[i].sh;
        Render::SetPosition(sp, X0 + sl[i].px + 0.5f, Y0 + sl[i].py + 0.5f, z);
        prev = sp;
    }
    // With +0x5f set, a previous centre sprite stays chained after the bottom-right one, as on the
    // original (its next pointer is not reset).
}

void Button::SetOnLongTap(Callback cb) { onLongTap = std::move(cb); }

// ---------------------------------------------------------------------------------------------
// Module

void Init(const char* fontFile, bool highDPI) {   // @0x178b64
    (void)highDPI;
    g_fontFile = fontFile;
    g_desktop = new Window();
    g_defaultText = new Textfield(Render::CreateFont(g_fontFile, 10));
    g_defaultText->type = Window::kTextfield;
    g_defaultButton = new Button();
    g_defaultButton->type = Window::kButton;
    // PORT: the TextCache (0x1a0c bytes) only caches rendered strings; not ported.
}

void Deinit() {
    // UNVERIFIED: @0x17d60c also destroys every registered window; the port leaves them to exit.
}

const char* GetFontFile() { return g_fontFile; }
Textfield* DefaultTextfield() { return g_defaultText; }

void DumpTree(const Window* w, int depth) {
    for (; w; w = depth ? w->next : nullptr) {
        const Render::Sprite* s = w->sprite;
        std::printf("%*s%s [%d] pos %d,%d size %dx%d z %.4f vis %d tex %s sprite %s",
                    depth * 2, "", w->name.c_str(), w->type, w->x, w->y, w->w, w->h, w->z, w->visible,
                    w->texture ? w->texture->name.c_str() : "-", s ? "" : "-");
        if (s) std::printf("(%.2f,%.2f %.1fx%.1f z %.4f)", s->x, s->y, s->w, s->h, s->z);
        std::printf("\n");
        if (w->firstChild) DumpTree(w->firstChild, depth + 1);
        if (!depth) break;
    }
}

Window* GetWindow(Window* root, const char* n) {   // @0x17c17c
    auto it = g_byName.find(n);
    if (it == g_byName.end()) return nullptr;
    const auto& v = it->second;
    for (size_t i = v.size(); i-- > 0;)
        if (v[i]->root == root) return v[i];
    return nullptr;
}

// ---------------------------------------------------------------------------------------------
// Layout loader: GUI::RegisterBinaryUI @0x180784. File "<layout>b":
//   u32 count + count x 26-byte CompactWindowInfo: u16 name, symbol (string offsets), parent index,
//     image (1-based window index whose image is used; 0 none), s16 x, y, w, h, u16 ?,
//     u8 type (0 window, 1 button, 2 textfield), u8 border left, u8 border top, u16 right, u16 bottom
//   u32 count + 6-byte CompactTextInfo: u8 r, g, b, size, align, filter count
//   u32 count + 12-byte CompactFilterInfo: f32 strength, u8 blur, s8 dx, s8 dy, u8 knockout, u8 r, g, b, -
//   u32 size + string bytes
namespace {

struct CompactWindowInfo { uint16_t name, symbol, parent, image; int16_t x, y, w, h; uint16_t u8;
                           uint8_t type, borderL, borderT, pad; uint16_t borderR, borderB; };
static_assert(sizeof(CompactWindowInfo) == 26, "layout record");

template <class T> std::vector<T> ReadSection(const uint8_t*& p, const uint8_t* end, size_t recSize) {
    std::vector<T> v;
    if (p + 4 > end) return v;
    uint32_t n;
    std::memcpy(&n, p, 4);
    p += 4;
    if (p + (size_t)n * recSize > end) return v;
    v.resize(n);
    for (uint32_t i = 0; i < n; ++i) std::memcpy(&v[i], p + i * recSize, recSize);
    p += (size_t)n * recSize;
    return v;
}

void SetupSprite(Window* win, Render::Texture* tex, float z) {
    win->sprite = Render::CreateSprite(tex, Render::kLayerGUI, false, false);
    Render::SetShaderType(win->sprite, 0);
    win->sprite->screenSpace = true;
    win->sprite->w = (float)win->w;
    win->sprite->h = (float)win->h;
    Render::SetPosition(win->sprite, (float)win->x, (float)win->y + (float)win->h, z);
}

}  // namespace

Window* RegisterBinaryUI(const char* layout, const char* rootImage, float scale, int offsetX, int offsetY,
                         int fitExtraW, int fitExtraH, bool async, float fontScale) {
    (void)async;
    char path[512];
    std::snprintf(path, sizeof path, "%sb", layout);
    uint32_t size = 0;
    uint8_t* data = FileManager::LoadFile(path, size);   // PORT: the original caches files < 4 KiB
    if (!data) return nullptr;
    const uint8_t* p = data;
    const uint8_t* end = data + size;
    struct Text { uint8_t r, g, b, size, align, filters; };
    struct Filter { float strength; uint8_t blur; int8_t dx, dy; uint8_t knockout, r, g, b, pad; };
    auto infos = ReadSection<CompactWindowInfo>(p, end, 26);
    auto texts = ReadSection<Text>(p, end, 6);
    auto filters = ReadSection<Filter>(p, end, 12);
    uint32_t strSize = 0;
    if (p + 4 <= end) { std::memcpy(&strSize, p, 4); p += 4; }
    std::string strings((const char*)p, (size_t)(end - p) < strSize ? (size_t)(end - p) : strSize);
    auto str = [&](uint16_t off) { return off < strings.size() ? std::string(strings.c_str() + off) : std::string(); };

    Render::Texture* rootTex = Resources::GetUIImage(rootImage, true, false);
    // (falls back to IconManager::GetIcon(rootImage) on the original; icons are not ported yet)
    std::string dir = layout;
    size_t slash = dir.rfind('/');
    if (slash != std::string::npos) dir.resize(slash);

    std::vector<Window*> list;   // windows of this file, by record index
    Window* top = nullptr;
    std::string rootSymbol;
    bool hasRootSymbol = false;
    size_t textIdx = 0, filterIdx = 0;
    for (size_t i = 0; i < infos.size(); ++i) {
        const CompactWindowInfo& in = infos[i];
        Window* win = nullptr;
        if (in.type == 0) {
            win = new Window();
        } else if (in.type == 1) {
            win = new Button();
        } else if (in.type == 2 && textIdx < texts.size()) {
            const Text& t = texts[textIdx++];
            unsigned fs = (unsigned)((float)t.size * scale * fontScale * g_fontScale);
            if (IsSmallScreenVersion() && (int)fs < 9) fs = 9;
            Textfield* tf = new Textfield(Render::CreateFont(g_fontFile, fs));
            tf->SetColor((float)t.r / 255.f, (float)t.g / 255.f, (float)t.b / 255.f);
            tf->SetAlignment(t.align, 1);
            tf->fontSize = (int16_t)(int)((float)t.size * fontScale * g_fontScale);
            Render::GlowFilter* prevGlow = nullptr;
            for (unsigned k = 0; k < t.filters && filterIdx < filters.size(); ++k) {
                const Filter& f = filters[filterIdx++];
                auto* g = new Render::GlowFilter();
                if (!prevGlow) tf->glow = g; else prevGlow->next = g;
                g->blur = (int)(unsigned)((int)((float)f.blur * scale) & 0xff);
                if (f.blur && g->blur == 0) g->blur = 1;
                if (IsSmallScreenVersion() && (int)fs < 13 && g->blur > 1) g->blur = 1;
                g->r = (float)f.r / 255.f;
                g->g = (float)f.g / 255.f;
                g->b = (float)f.b / 255.f;
                g->strength = f.strength * scale;
                g->dx = f.dx;
                g->dy = f.dy;
                if (IsSmallScreenVersion() && (int)fs < 13) g->dx = g->dy = 0;
                g->knockout = f.knockout != 0;
                prevGlow = g;
            }
            win = tf;
        }
        if (!win) {   // unknown type: the original pushes a null entry and carries on
            list.push_back(nullptr);
            continue;
        }
        list.push_back(win);
        std::string symbol = str(in.symbol);
        if (in.parent == 0) win->name = str(in.name);
        else win->name = (list[in.parent] ? list[in.parent]->name : std::string()) + "." + str(in.name);
        win->scale = 1.f;
        win->symbol = symbol;
        if (i == 0) {
            if (scale < 0.f) scale = GetScaleFactor(fitExtraW + in.w, fitExtraH + in.h, !IsTabletVersion(), -scale);
            win->scale = scale;
            win->name = layout;
            win->baseX = offsetX + in.x;
            win->baseY = offsetY + in.y;
            rootSymbol = symbol;
            hasRootSymbol = true;
            top = win;
        }
        win->type = in.type;
        win->origX = in.x;
        win->origY = in.y;
        win->x = (int16_t)(int)((float)in.x * scale);
        win->y = (int16_t)(int)((float)in.y * scale);
        win->origW = in.w;
        win->origH = in.h;
        win->root = i == 0 ? nullptr : top;
        win->w = (int16_t)(int)((float)in.w * scale);
        win->h = (int16_t)(int)((float)in.h * scale);
        if (in.parent == 0) { if (i != 0) top->AddChild(win); }
        else if (list[in.parent]) list[in.parent]->AddChild(win);
        win->texture = nullptr;
        bool nineSlice = in.borderL || in.borderT || in.borderR || in.borderB;
        unsigned img = in.image;
        if (hasRootSymbol && img == list.size() && i != 0) {
            // own image: "<dir>/<RootSymbol>___<full name, dots as underscores>.png"
            std::string n = win->name;
            for (char& c : n) if (c == '.') c = '_';
            std::string file = dir + "/" + rootSymbol + "___" + n + ".png";
            if (Render::Texture* t = Resources::GetUIImage(file.c_str(), true, false)) {
                win->texture = t;
                SetupSprite(win, t, 0.001f);
            }
        } else if (img != 0 && img - 1 < list.size() && list[img - 1] && list[img - 1]->texture) {
            win->texture = list[img - 1]->texture;
            SetupSprite(win, win->texture, 0.001f);
        }
        if (nineSlice && win->texture)
            win->SetBorders(in.borderL, in.borderT, (unsigned)(win->texture->w - in.borderR),
                            (unsigned)(win->texture->h - in.borderB));
        Register(win);
    }
    FileManager::FreeFile(data);
    if (rootTex && top) {
        top->texture = rootTex;
        SetupSprite(top, rootTex, 0.001f);
    }
    return top;
}

Window* RegisterUI(const char* layout, const char* rootImage, float scale, int offsetX, int offsetY,
                   int fitExtraW, int fitExtraH, bool async, float fontScale) {
    Window* w = RegisterBinaryUI(layout, rootImage, scale, offsetX, offsetY, fitExtraW, fitExtraH, async,
                                 fontScale);
    if (!w) std::printf("GUI::RegisterUI: %s: no binary layout (XML layouts are not ported)\n", layout);
    return w;
}

}  // namespace GUI
