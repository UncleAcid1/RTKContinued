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
    Callback onLongTap;           // +0x80/+0x84
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
    virtual void SetEditable(bool editable);                        // +0xac @0x176100
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
    bool editable = false;        // +0x9a
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
template <class T> T* GetWindowTyped(Window* root, const char* name) {
    return dynamic_cast<T*>(GetWindow(root, name));
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

}  // namespace GUI
