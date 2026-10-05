// GUI::Textfield (vtable 0x608580, 0xbc bytes). Text images come from Render::CreateTextInternal.
#include "gui/GUI.h"

#include "engine/FileManager.h"
#include "engine/Render.h"
#include "engine/Text.h"

namespace GUI {
namespace {

int ShaderFor(const Window* w) { return !w->enabled ? 3 : (w->alpha == 1.f ? 0 : 1); }
int AlignArg(int a) { return (a != 0 && a != 1) ? 2 : a; }

}  // namespace

// @0x175f8c (the other fields keep their zero/one defaults; +0xb4 comes from a global that is 0)
Textfield::Textfield(Render::Font* f) : font(f) { type = kTextfield; }

Textfield::~Textfield() {
    for (Render::GlowFilter* g = glow; g;) {
        Render::GlowFilter* n = g->next;
        delete g;
        g = n;
    }
}

float Textfield::TextZoom() const {
    return (worldOverscale ? Render::GetBaseZoomFactor() : 1.f) * overscale;
}

// Builds the text sprite for the current text and settings (the common tail of SetText, SetColor,
// SetOverscale, SetWorldOverscale, RescaleWindow and ReloadTextures).
void Textfield::BuildSprite(Render::Sprite* reuse, bool setShader) {
    float zoom = TextZoom();
    sprite = Render::CreateTextWrapped(font, text.c_str(), color[0], color[1], color[2], (int)((float)w * zoom),
                                       AlignArg(alignH), true, glow, reuse, lineSpacing, pattern, style);
    if (setShader) Render::SetShaderType(sprite, ShaderFor(this));
    sprite->screenSpace = screenSpace;
    sprite->w = (float)sprite->tex->w / zoom;
    sprite->h = (float)(-sprite->tex->h) / zoom;
}

// @0x17b9b8
void Textfield::SetText(const char32_t* t, void* tag, Render::Font* fontOverride) {
    extern Textfield* DefaultTextfield();
    if (this == DefaultTextfield()) return;
    bool tagChanged = false;
    if (asyncTag != tag) {
        asyncTag = tag;
        tagChanged = tag != nullptr;
    }
    if (t && hasText && text == t) return;
    if (!t || !*t) {
        text.clear();
        hasText = false;
        sprite = Render::RemoveSprite(sprite);
    } else {
        text = t;
        hasText = true;
    }
    // PORT: short strings go through GUI::TextCache on the original, which renders them with the
    // same CreateTextInternal call; the cache only saves work.
    Render::Sprite* reuse = nullptr;
    if (hasText || tagChanged) {
        reuse = sprite;
    } else if (!tag || sprite) {
        UpdatePosition();
        return;
    }
    Render::Font* f = fontOverride ? fontOverride : font;
    float zoom = TextZoom();
    sprite = Render::CreateTextWrapped(f, text.c_str(), color[0], color[1], color[2], (int)((float)w * zoom),
                                       AlignArg(alignH), true, glow, reuse, lineSpacing, pattern, style);
    Render::SetShaderType(sprite, ShaderFor(this));
    if (!reuse) Render::SortRenderLayer(Render::kLayerGUI, 1);
    sprite->screenSpace = screenSpace;
    sprite->w = (float)sprite->tex->w / zoom;
    sprite->h = (float)(-sprite->tex->h) / zoom;
    UpdatePosition();
}

const char32_t* Textfield::GetText() { return hasText ? text.c_str() : nullptr; }   // @0x1760e0

// @0x17a160
void Textfield::SetColor(float r, float g, float b) {
    if (color[0] == r && color[1] == g && color[2] == b) return;
    color[0] = r;
    color[1] = g;
    color[2] = b;
    sprite = Render::RemoveSprite(sprite);
    if (hasText) {
        BuildSprite(nullptr, true);
        Render::SortRenderLayer(Render::kLayerGUI, 1);
    }
    UpdatePosition();
}

// @0x17741c: re-set the text with the new font
void Textfield::SetFont(Render::Font* f) {
    sprite = Render::RemoveSprite(sprite);
    std::u32string t = text;
    bool had = hasText;
    font = f;
    text.clear();
    hasText = false;
    SetText(had ? t.c_str() : nullptr, nullptr, nullptr);
}

// Font size for the current scale (SetOverscale/SetWorldOverscale/RescaleWindow).
static unsigned FontSizeFor(const Textfield* t, float zoom) {
    unsigned s = (unsigned)((float)t->fontSize * t->root->scale * zoom);
    if (IsSmallScreenVersion() && (int)s < 9) s = 9;
    return s;
}

// @0x179160
void Textfield::SetOverscale(float o) {
    if (overscale == o) return;
    overscale = o;
    if (!root) return;
    for (Render::GlowFilter* g = glow; g; g = g->next) g->overscale = o;   // GlowFilter::SetOverscale
    font = Render::CreateFont(GetFontFile(), FontSizeFor(this, (worldOverscale ? Render::GetBaseZoomFactor() : 1.f) * o));
    if (hasText) BuildSprite(sprite, true);
    UpdatePosition();
}

// @0x179360. (The original multiplies the font size by the bool itself, so turning world
// overscale off asks for a size-0 font.)
void Textfield::SetWorldOverscale(bool on) {
    worldOverscale = on;
    if (!root) return;
    float zoom = (on ? Render::GetBaseZoomFactor() : 1.f) * (float)on;
    font = Render::CreateFont(GetFontFile(), FontSizeFor(this, zoom));
    if (hasText) {
        float z = (worldOverscale ? Render::GetBaseZoomFactor() : 1.f) * (float)on;
        sprite = Render::CreateTextWrapped(font, text.c_str(), color[0], color[1], color[2], (int)((float)w * z),
                                           AlignArg(alignH), true, glow, sprite, lineSpacing, pattern, style);
        Render::SetShaderType(sprite, ShaderFor(this));
        sprite->screenSpace = screenSpace;
        sprite->w = (float)sprite->tex->w / z;
        sprite->h = (float)(-sprite->tex->h) / z;
    }
    UpdatePosition();
}

// @0x17a574
void Textfield::RescaleWindow(float s) {
    Window::RescaleWindow(s);
    if (!root) return;
    font = Render::CreateFont(GetFontFile(), FontSizeFor(this, (worldOverscale ? Render::GetBaseZoomFactor() : 1.f) * overscale));
    if (hasText) BuildSprite(sprite, true);
    UpdatePosition();
}

// @0x179544
void Textfield::ReloadTextures() {
    sprite = Render::RemoveSprite(sprite);
    if (hasText) BuildSprite(nullptr, false);
    UpdatePosition();
    if (!sprite) return;
    Render::SetShaderType(sprite, ShaderFor(this));
    sprite->screenSpace = screenSpace;
}

// @0x1796d0
void Textfield::SetZ(float nz) {
    z = nz;
    if (parent) {
        root->zNext -= 0.0001f;
        z = root->zNext;
    }
    if (sprite) sprite->z = z;
}

// @0x178ab8
void Textfield::SetVisibility(bool v) {
    visibleSelf = v;
    if (parent && v) v = parent->visible;
    visible = v;
    if (sprite) Render::SetVisibility(sprite, visible && !hideSprite);
}

void Textfield::SetEnabled(bool e) {   // @0x177008
    enabled = e;
    if (sprite) Render::SetShaderType(sprite, ShaderFor(this));
}

void Textfield::SetAlpha(float a, bool) {   // @0x177144
    alpha = a;
    if (!sprite) return;
    Render::SetAlpha(sprite, a);
    Render::SetShaderType(sprite, ShaderFor(this));
}

// @0x179e5c UNVERIFIED: rotation shader (8) not loaded yet.
void Textfield::SetRotation(float angle) {
    if (!sprite) return;
    sprite->color[2] = angle;
    sprite->color[0] = sprite->x + sprite->w * 0.5f;
    sprite->color[1] = sprite->h * 0.5f - sprite->y;
    Render::SetShaderType(sprite, 8);
}

void Textfield::SetScreenSpace(bool ss) {   // @0x179bfc (the field itself is not stored)
    if (sprite) sprite->screenSpace = ss;
}

void Textfield::SetCustomShader(int type, bool) {   // @0x176ff8
    if (sprite) Render::SetShaderType(sprite, type);
}

// @0x179c84: the text image (which has the glow border around it) is placed by the alignment
// inside the field; the sprite's position is its top-left corner (negative height).
void Textfield::UpdatePosition() {
    int off = glow ? glow->GetOffset() : 0;
    if (sprite) {
        float zoom = TextZoom();
        int o = (int)((float)off / zoom);
        int tw = (int)((float)sprite->tex->w / zoom), th = (int)((float)sprite->tex->h / zoom);
        int rx = root ? root->x : 0, ry = root ? root->y : 0;
        int px = rx + x - o;
        if (alignH == kCenter) px += (o * 2 - tw + w) / 2;
        else if (alignH == kRight) px += o * 2 - tw + w;
        int py = ry + y;
        if (alignV == 1) py += (h - th) / 2;
        else if (alignV == 2) py = py + h + o - th;
        else if (alignV == 0) py -= o;
        Render::SetPosition(sprite, (float)px, (float)py, z);
        Render::SetVisibility(sprite, visible && !hideSprite);
        if (sprite->shaderType == 8) SetRotation(sprite->color[2]);
    }
    if (clip) ClipWithRect(clip->left, clip->top, clip->right, clip->bottom);
}

// @0x179728: hide the text outside the rectangle, or trim it (position, size and uv).
void Textfield::ClipWithRect(int l, int t, int r, int b) {
    if (root && !root->ignoreClip) {
        b += root->y; t += root->y; r += root->x; l += root->x;
    }
    if (!sprite) return;
    float zoom = TextZoom();
    float sy = sprite->y;
    float fw = (float)sprite->tex->w / zoom;
    float fh = -(float)sprite->tex->h / zoom;
    sprite->w = fw;
    sprite->h = fh;
    int sx = (int)sprite->x;
    Render::SetVisibility(sprite, visible);
    hideSprite = false;
    if (sx <= r) {
        int iw = (int)fw, right = iw + sx;
        int top = (int)sy;
        if (l <= right && top <= b) {
            int ih = (int)fh, bottom = top - ih;   // fh < 0: bottom edge is below the top
            if (t <= bottom) {
                if (right <= r && l <= sx && bottom <= b && t <= top) {
                    sprite->vBottom = 0.f; sprite->u0 = 0.f;
                    sprite->vTop = 1.f; sprite->u1 = 1.f;
                    return;
                }
                if (t < top) t = top;
                if (l < sx) l = sx;
                int nh = b < bottom ? b - t : bottom - t;
                int nw = r < right ? r - l : right - l;
                float fl = (float)l, fnw = (float)nw, ft = (float)t, fnh = (float)nh;
                sprite->x = fl;
                sprite->w = fnw;
                sprite->y = ft;
                sprite->h = -fnh;
                float x0 = (float)sx, y0 = (float)top, ww = (float)iw, hh = (float)(-ih);
                sprite->u1 = ((fl + fnw) - x0) / ww;
                sprite->vTop = ((ft + fnh) - y0) / hh;
                sprite->u0 = (fl - x0) / ww;
                sprite->vBottom = (ft - y0) / hh;
                return;
            }
        }
    }
    Render::SetVisibility(sprite, false);
    hideSprite = true;
}

namespace {
// TextfieldCallback::Callback @0x179ed4: the text input result goes into the edited field, cut to
// its maxLength, with line breaks turned into spaces; then the field's onEdit; a final result ends
// the editing.
void OnTextInput(const char32_t* text, bool final) {
    Textfield* f = g_inputField;
    if (!f) return;
    char32_t buf[0x101];
    const char32_t* out = buf;
    if (!text) {
        out = nullptr;
    } else {
        size_t len = 0;
        while (text[len]) ++len;
        size_t n = len > f->maxLength ? f->maxLength : len;
        for (size_t i = 0; i < n; ++i) buf[i] = (text[i] == 0xd || text[i] == 10) ? U' ' : text[i];
        buf[n] = 0;
    }
    f->SetText(out);
    if (g_inputField->onEdit) g_inputField->onEdit();
    if (final) g_inputField = nullptr;
}
}  // namespace

// @0x17d04c: an editable field takes the click (a visible, enabled field even without force) and
// on release starts text input; a click elsewhere ends it. Then the Window behaviour.
bool Textfield::Click(int px, int py, bool pressed, bool force) {
    if (maxLength) {
        if (!visible) force = false;
        else if (enabled) force = true;
        if (x <= px && px <= w + x && y <= py && py <= h + y && force) {
            if (root) root->clickHandled = true;
            if (!pressed) {
                g_inputField = this;
                FileManager::BeginTextInput(OnTextInput, hasText ? text.c_str() : nullptr);
            }
            return true;
        }
        if (g_inputField == this) {
            FileManager::AbortTextInput();
            g_inputField = nullptr;
        }
    }
    return Window::Click(px, py, pressed, force);
}

void Textfield::SetAlignment(int horizontal, int vertical) {   // @0x1760c4
    alignH = horizontal;
    alignV = vertical;
    UpdatePosition();
}

void Textfield::SetStyle(bool a) { async = a; }                            // @0x1760e8
void Textfield::SetPattern(const Render::TextPattern* p) { pattern = p; }  // @0x1760f0
void Textfield::SetFontStyle(unsigned s) { style = s; }                    // @0x1760f8
void Textfield::SetEditable(unsigned n) { maxLength = (uint8_t)n; }          // @0x176100
void Textfield::SetOnEdit(Callback cb) { onEdit = std::move(cb); }         // @0x176108
bool Textfield::IsInputFocused() { return g_inputField == this; }   // @0x176110
void Textfield::SetAsyncUpdate(bool on) { if (glow) glow->allowAsync = on; }   // @0x176130

}  // namespace GUI
