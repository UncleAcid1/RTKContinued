// GUI: window tree built from Flash-exported layouts (.xmlb), drawn as sprites on render layer 14.
// Port of the GUI namespace (libkingdom.so 5.11): GUI::Window (vtable 0x6086d8, 0x80 bytes),
// GUI::Button (0x608648, 0x88 bytes), GUI::Textfield (0x608580, 0xbc bytes), the layout loader
// GUI::RegisterBinaryUI @0x180784 and the global window registry.
//
// Coordinates are screen pixels, y down. A window's x/y are relative to its root window (the
// layout's top window); the root's own x/y are absolute. Sizes and positions are the layout's
// values multiplied by the root's scale (+0x60) and truncated.
#pragma once
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>

namespace Render { struct Texture; struct Sprite; struct Font; struct GlowFilter; struct TextPattern; }

namespace GUI {

struct ClipRect { int left, top, right, bottom; };   // compared in root-local coordinates

// GUI::Callback and its templated subclasses are type-erased function objects; std::function is
// the same thing in standard C++.
using Callback = std::function<void()>;

struct ActionSound { std::string name; int param = 0; };   // +0x6c, played on click

class Window {
public:
    enum Type { kWindow = 0, kButton = 1, kTextfield = 2 };

    Window();
    virtual ~Window();

    // Virtual interface in vtable order (byte offset of the slot in comments).
    virtual void SetPosition(int x, int y);                         // +0x08 @0x175cc0
    virtual void SetZ(float z);                                     // +0x0c @0x178868
    virtual void SetVisibility(bool visible);                       // +0x10 @0x179094
    virtual void SetTexture(Render::Texture* tex, bool keepSize = false, unsigned w = 0,
                            unsigned h = 0, bool force = false, unsigned frame = 0);  // +0x14 @0x17b0d0
    virtual void SetEnabled(bool enabled);                          // +0x18 @0x1770bc
    virtual void SetAlpha(float alpha, bool recursive = true);      // +0x1c @0x1771ac
    virtual void SetRotation(float angle);                          // +0x20 @0x179c2c
    virtual void SetScreenSpace(bool screenSpace);                  // +0x24 @0x179b84
    virtual void SetCustomShader(int type, bool recursive);         // +0x28 @0x17704c
    virtual void SetColor(float r, float g, float b, bool recursive = true);  // +0x2c @0x17b488
    virtual void SetExtra(float extra, bool recursive);             // +0x30 @0x179b14
    virtual void AddChild(Window* child);                           // +0x34 @0x175d38
    virtual void PrependChild(Window* child);                       // +0x38 @0x175d60
    virtual Window* GetChild(const char* name);                     // +0x3c @0x178af8
    virtual Window* GetWindowAtPosition(int x, int y, bool clickable); // +0x40 @0x17af34
    virtual void UpdatePosition();                                  // +0x44 @0x178490
    virtual void ClipWithRect(int left, int top, int right, int bottom);  // +0x48 @0x17a748
    virtual void SetClipRect(ClipRect* rect);                       // +0x4c @0x175d80
    virtual void SetOnClick(Callback cb);                           // +0x50 @0x175d88
    virtual void SetActionSound(const char* name, int param);       // +0x54 @0x17e260
    virtual bool Click(int x, int y, bool pressed, bool force);     // +0x58 @0x17cc30
    virtual void SetFrame(unsigned frame, bool keepSize);           // +0x5c @0x178c44
    virtual void ReloadTextures();                                  // +0x60 @0x175db8
    virtual void PreloadTextures();                                 // +0x64 @0x17aed4
    virtual void ApplyMask(Render::Texture* mask);                  // +0x68 @0x17ae08
    virtual void SetBorders(unsigned left, unsigned top, unsigned right, unsigned bottom);  // +0x6c @0x178478
    virtual void SetSize(unsigned w, unsigned h);                   // +0x70 @0x178418
    virtual void MoveWindow(int dx, int dy);                        // +0x74 @0x175dbc
    virtual void RestorePosition();                                 // +0x78 @0x175de0
    virtual void RestoreSize();                                     // +0x7c @0x179978
    virtual void RescaleWindow(float scale);                        // +0x80 @0x17a320

    void Update9Slices();                                           // @0x17745c

    float RootScale() const { return root ? root->scale : scale; }

    // Fields, named after their use; offsets are those of the original object.
    std::string name;             // +0x04 full dotted name ("panel.button.text"); root: the layout path
    int type = kWindow;           // +0x08
    std::string symbol;           // +0x0c Flash symbol (class) name; root: its own symbol
    int16_t origX = 0, origY = 0; // +0x10 +0x12 layout position (unscaled)
    int16_t x = 0, y = 0;         // +0x14 +0x16 scaled position (relative to the root, see above)
    float z = 0.1f;               // +0x18 sprite depth
    float zNext = 0.1f;           // +0x1c root only: next free depth, handed out downwards by SetZ
    int16_t w = 0, h = 0;         // +0x20 +0x22 scaled size
    int16_t origW = 0, origH = 0; // +0x24 +0x26 layout size (unscaled)
    int16_t unk28 = 0, unk2a = 0; // +0x28 +0x2a
    bool noInput = false;         // +0x2c never hit by clicks/hit tests
    bool pixelHitTest = false;    // +0x2d hit only where the sprite has opaque pixels
    bool ignoreClip = false;      // +0x2e
    uint8_t frame = 0;            // +0x2f frame index passed to SetTexture
    Window* next = nullptr;       // +0x30 sibling list
    Window* prev = nullptr;       // +0x34
    Window* firstChild = nullptr; // +0x38
    Window* lastChild = nullptr;  // +0x3c
    Window* parent = nullptr;     // +0x40
    Window* root = nullptr;       // +0x44 top window of the layout (nullptr for the root itself)
    Render::Texture* texture = nullptr;  // +0x48
    Render::Sprite* sprite = nullptr;    // +0x4c (first of the chain when 9-sliced)
    int baseX = 0, baseY = 0;     // +0x50 +0x54 root: layout origin + RegisterUI offset
    bool centerSprite = false;    // +0x58 sprite centred in the window (SetTexture keepSize)
    bool enabled = true;          // +0x59
    bool visibleSelf = true;      // +0x5a requested visibility
    bool visible = true;          // +0x5b effective visibility (requested && parent visible)
    bool clickHandled = false;    // +0x5c root: a child consumed the current click
    bool takesZ = false;          // +0x5d takes a depth slot even without a sprite
    bool screenSpace = true;      // +0x5e drawn with the GUI transform
    bool unk5f = false;           // +0x5f
    float scale = 1.f;            // +0x60 root: layout scale factor
    float alpha = 1.f;            // +0x64
    Callback onClick;             // +0x68
    ActionSound* sound = nullptr; // +0x6c
    int index = -1;               // +0x70 position in the global window list
    ClipRect* clip = nullptr;     // +0x74
    int16_t border[4] = {0, 0, 0, 0};  // +0x78 9-slice insets: left, top, right, bottom
};

class Button : public Window {   // 0x88 bytes
public:
    Button() { type = kButton; }
    virtual void SetOnLongTap(Callback cb);  // +0x84 @0x175f08
    Callback onLongTap;           // +0x80 (OnLongTapStart)
    Callback onLongTapAbort;      // +0x84 (OnLongTapAbort)
};

class Textfield : public Window {  // 0xbc bytes
public:
    enum Align { kLeft = 0, kCenter = 1, kRight = 2 };
    explicit Textfield(Render::Font* font);
    ~Textfield() override;
    // Window overrides (Textfield versions in the vtable)
    void SetZ(float z) override;                                    // @0x1796d0
    void SetVisibility(bool visible) override;                      // @0x178ab8
    void SetEnabled(bool enabled) override;                         // @0x177008
    void SetAlpha(float alpha, bool recursive = true) override;     // @0x177144
    void SetRotation(float angle) override;                         // @0x179e5c
    void SetScreenSpace(bool screenSpace) override;                 // @0x179bfc
    void SetCustomShader(int type, bool recursive) override;        // @0x176ff8
    void UpdatePosition() override;                                 // @0x179c84
    void ClipWithRect(int left, int top, int right, int bottom) override;  // @0x179728
    bool Click(int x, int y, bool pressed, bool force) override;    // @0x17d04c
    void ReloadTextures() override;                                 // @0x179544
    void RescaleWindow(float scale) override;                       // @0x17a574
    // Textfield interface (vtable +0x84...)
    virtual void SetAlignment(int horizontal, int vertical);        // +0x84 @0x1760c4
    virtual void SetText(const char32_t* text, void* asyncTag = nullptr, Render::Font* font = nullptr);  // +0x88 @0x17b9b8
    virtual const char32_t* GetText();                              // +0x8c @0x1760e0
    virtual void SetFont(Render::Font* font);                       // +0x90 @0x17741c
    virtual void SetColor(float r, float g, float b);               // +0x94 @0x17a160
    virtual void SetWorldOverscale(bool on);                        // +0x98 @0x179360
    virtual void SetOverscale(float overscale);                     // +0x9c @0x179160
    virtual void SetStyle(bool async);                              // +0xa0 @0x1760e8
    virtual void SetPattern(const Render::TextPattern* pattern);    // +0xa4 @0x1760f0
    virtual void SetFontStyle(unsigned style);                      // +0xa8 @0x1760f8
    virtual void SetEditable(unsigned maxLength);                   // +0xac @0x176100 (0: not editable)
    virtual void SetOnEdit(Callback cb);                            // +0xb0 @0x176108
    virtual bool IsInputFocused();                                  // +0xb4 @0x176110
    virtual void SetAsyncUpdate(bool on);                           // +0xb8 @0x176130

    float TextZoom() const;       // (world overscale ? base zoom : 1) * overscale
    void BuildSprite(Render::Sprite* reuse, bool setShader);

    std::u32string text;          // +0x80 (nullptr on the original = empty here)
    bool hasText = false;
    void* asyncTag = nullptr;     // +0x84
    int alignH = kLeft;           // +0x88
    int alignV = 0;               // +0x8c
    const Render::TextPattern* pattern = nullptr;  // +0x90 SetPattern
    unsigned style = 0;           // +0x94 SetFontStyle: initial TTF style bits
    int16_t fontSize = 0;         // +0x98 layout font size * font scale
    uint8_t maxLength = 0;        // +0x9a editable when non-zero (SetEditable)
    bool hideSprite = false;      // +0x9b
    float color[3] = {0, 0, 0};   // +0x9c +0xa0 +0xa4
    float overscale = 1.f;        // +0xa8
    Render::Font* font = nullptr; // +0xac
    Render::GlowFilter* glow = nullptr;  // +0xb0 chain of filters
    bool async = false;           // +0xb4 SetStyle (passed to the text builder as its async flag)
    bool worldOverscale = false;  // +0xb5
    int8_t lineSpacing = 0;       // +0xb6 extra pixels between lines
    Callback onEdit;              // +0xb8
};

// GUI::AnimationEffect (0x3c bytes): the base of the window animations. Every effect sits in one
// list (CreateMovementEffect / CreateTweenEffect) that UpdateAnimation steps each frame.
class AnimationEffect {
public:
    using Fn = std::function<void()>;
    AnimationEffect(Fn onUpdate, Fn onShown, Fn onHidden)   // @0x182f30
        : onUpdate(std::move(onUpdate)), onShown(std::move(onShown)), onHidden(std::move(onHidden)) {}
    virtual ~AnimationEffect() = default;
    virtual void Update(float) {}                        // +0x08 @0x175afc
    virtual int GetCenteredX() const;                    // +0x0c @0x17621c
    virtual int GetCenteredY() const;                    // +0x10 @0x176254
    void CenterWith(int l, int t, int r, int b) { insetL = l; insetT = t; insetR = r; insetB = b; }  // @0x17628c
    void CenterWith(const Window* w);                    // @0x1762a4

    Window* window = nullptr;     // +0x04
    Fn onUpdate;                  // +0x08 after every step
    Fn onShown;                   // +0x0c arrival when shown
    Fn onHidden;                  // +0x10 arrival when hidden
    bool active = false;          // +0x14
    bool hideAtEnd = false;       // +0x15
    float t = 0.f;                // +0x18 progress 0..1
    float duration = 0.2f;        // +0x1c seconds
    bool blocksInput = true;      // +0x20 (IsAnyAnimationActive)
    int insetL = 0, insetT = 0, insetR = 0, insetB = 0;   // +0x24..+0x30 centring margins
    std::string swingIn = "ui_swing_in", swingOut = "ui_swing_out";   // +0x34 +0x38 sounds
};

// GUI::MovementEffect (0x4c bytes): slides a window from one position to another in `duration`
// seconds. Created by CreateMovementEffect.
class MovementEffect : public AnimationEffect {
public:
    using AnimationEffect::AnimationEffect;
    void Update(float dt) override;                      // +0x08 @0x176548
    // @0x176510: from (fx, fy) to (tx, ty); hideAtEnd = !show (the window is hidden on arrival).
    void Animate(int fx, int fy, int tx, int ty, bool show);

    int fromX = 0, fromY = 0, toX = 0, toY = 0;          // +0x3c..+0x48
};

// GUI::TweenEffect (0x58 bytes): a dialog dropping in from above the screen to its centred position
// (AnimateIn) and falling out of the bottom (AnimateOut), in `duration` (0.25) seconds. x moves from
// fromX to midX until t reaches `split`, then on to toX; y moves linearly over the whole time.
class TweenEffect : public AnimationEffect {
public:
    TweenEffect(Fn onUpdate, Fn onShown, Fn onHidden)    // @0x183230
        : AnimationEffect(std::move(onUpdate), std::move(onShown), std::move(onHidden)) { duration = 0.25f; }
    void Update(float dt) override;                      // +0x08 @0x176350
    void AnimateIn();                                    // @0x17877c
    void AnimateOut();                                   // @0x1786c4

    int fromX = 0, fromY = 0, toX = 0, toY = 0;          // +0x3c..+0x48
    int midX = 0;                                        // +0x4c
    float split = 0.f;                                   // +0x54
};

MovementEffect* CreateMovementEffect(Window* w, MovementEffect::Fn onUpdate = nullptr,
                                     MovementEffect::Fn onShown = nullptr,
                                     MovementEffect::Fn onHidden = nullptr);   // @0x1830a4
void RemoveMovementEffect(MovementEffect* e);                                  // @0x17c538
TweenEffect* CreateTweenEffect(Window* w, TweenEffect::Fn onUpdate = nullptr, TweenEffect::Fn onShown = nullptr,
                               TweenEffect::Fn onHidden = nullptr);            // @0x183278
void RemoveTweenEffect(TweenEffect* e);                                        // @0x17c4a0
void UpdateAnimation(float dt);                                                // @0x17c648 (movement part)
bool IsAnyAnimationActive();                                                   // @0x17c43c

// --- module API -------------------------------------------------------------------------------
// @0x178b64 Init(fontFile, ..., highDPI): desktop window, default textfield/button, text cache.
void Init(const char* fontFile, bool highDPI);
void Deinit();                                                     // @0x17d60c
const char* GetFontFile();

// @0x180784 / @0x183400: load "<layout>.xmlb" (falls back to the XML layout, not ported: every
// layout the game uses exists as .xmlb). scale < 0: fit the layout to the screen (GetScaleFactor).
Window* RegisterUI(const char* layout, const char* rootImage, float scale, int offsetX, int offsetY,
                   int fitExtraW, int fitExtraH, bool async, float fontScale);

Window* GetWindow(Window* root, const char* name);                 // @0x17c17c
void DumpTree(const Window* w, int depth = 0);   // PORT: debugging aid (prints the window tree)
// GetWindowTyped / GetWindowTypedF (@0x2432f8, @0x243420...): on a miss (or a window of another
// type) they print a message and return GUI::Init's template window of that type, never null.
Window* GetWindowTyped(Window* root, const char* name, int type);
Window* DummyWindow();   // GUI::dummyWindow, the template window (GUI::Init's desktop window)
template <class T> T* GetWindowTyped(Window* root, const char* name);
template <> inline Window* GetWindowTyped<Window>(Window* root, const char* name) { return GetWindowTyped(root, name, -1); }
template <> inline Textfield* GetWindowTyped<Textfield>(Window* root, const char* name) {
    return static_cast<Textfield*>(GetWindowTyped(root, name, Window::kTextfield));
}
template <> inline Button* GetWindowTyped<Button>(Window* root, const char* name) {
    return static_cast<Button*>(GetWindowTyped(root, name, Window::kButton));
}
template <class T, class... A> T* GetWindowTypedF(Window* root, const char* fmt, A... args) {
    char buf[0x100];
    std::snprintf(buf, sizeof buf, fmt, args...);
    return GetWindowTyped<T>(root, buf);
}

// Screen configuration (set by SDL_baseInit @0x18b1e0 from the device screen).
void SetScreenSize(int w, int h);
int ScreenWidth();
int ScreenHeight();
bool IsTabletVersion();                                            // @0x176698
bool IsHighDPIVersion();                                           // @0x1766e0
bool IsSmallScreenVersion();                                       // @0x1767ec
float GetScaleFactor(int w, int h, bool phone, float scale);       // @0x176760
float GetHighDPIScaleFactor();                                     // @0x176184
float GetHoverScaleFactor(float normal, float scale);              // @0x176728 HighDPI ? scale*hover : normal
float GetHudScaleFactor();                                         // @0x176700
int GetVerticalCenter(int h);                                      // @0x1761fc

// Input
Window* GetCapture();                                              // @0x17698c
void SetCapture(Window* w);                                        // @0x1769a0
void ReleaseCapture();                                             // @0x1769b4
bool CanInteractWith(void* object);                                // @0x176864
void SetInteractionLock(bool lock);                                // @0x176844
void SetInteractionObjectLock(void* a, void* b);                   // @0x17680c
void SetOutOfBandInteractions(void* a, void* b);                   // @0x17682c
void OnLongTapStart();                                             // @0x1768fc
void OnLongTapAbort();                                             // @0x176944
extern Textfield* g_inputField;                                    // the field receiving text input
void SetTargetHighlight(Window* w, float target, bool pressed);    // @0x17c950
void OnMouseMove(int x, int y, bool recordOnly);                   // @0x17d180
void OnMouseClick(int x, int y, bool pressed);                     // @0x17d2ac (the tap ring)
float GetMouseSpeed(bool horizontal);                              // @0x176e80 pixels per second

}  // namespace GUI
