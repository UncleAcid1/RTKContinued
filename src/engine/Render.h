// Render: textures, sprites, render layers and their GL drawing.
// Port of the Render namespace (libkingdom.so 5.11). Field names follow the original struct offsets.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct ImageRGBA;

namespace Render {

struct Texture {
    unsigned glId = 0;
    int w = 0, h = 0;      // +0x04, +0x08 (pixel size of the full image / sheet)
    float frameTime = 0.f; // +0x0c seconds per frame (set by the users of animated textures)
    Texture* next = nullptr;  // +0x14 the next frame of an animated texture chain (loops)
    int frames = 0;        // +0x4c  frame count of a one-file animation sheet (0/1 = static)
    std::vector<uint8_t> pixelMask;   // +0x50 GeneratePixelMask (empty: none), for HasPixelAt
    bool wrap = false;     // Texture::SetWrapping
    std::string name;
};

enum RenderLayer : int {  // indices used by the original (RenderLayer enum values)
    kLayerGround = 0, kLayerGroundDecal = 1, kLayerFlat2 = 2, kLayerFlat3 = 3,
    kLayerRings = 7, kLayerObjects = 8, kLayer9 = 9, kLayerFog = 11, kLayerGUI = 14, kLayer15 = 15,
    kLayerCount = 16
};

struct Sprite {                  // 0x68 bytes on the original
    int index = 0;               // +0x00 slot in its layer
    bool visible = true;         // +0x04
    bool mirrorFrames = false;   // +0x06 SetFrame keeps u mirrored
    float x = 0, y = 0, z = 0;   // +0x08 +0x0c +0x10  (x,y) = bottom-left corner, world/screen units
    float w = 0, h = 0;          // +0x14 +0x18
    float extra = 0;             // +0x1c vertex attribute 3
    Texture* tex = nullptr;      // +0x20
    bool isText = false;         // +0x24 texture owned by the sprite (text sprites)
    bool screenSpace = false;    // +0x25 selects the GUI shader instance (Shader::Set(bool))
    int16_t frame1 = 0;          // +0x26 current frame + 1 (SetFrame)
    float u0 = 0, u1 = 1;        // +0x28 +0x2c
    float vBottom = 1, vTop = 0; // +0x30 +0x34
    float color[3] = {1, 1, 1};  // +0x38..+0x40 (rotation shaders reuse them as centre/angle)
    float alpha[4] = {1, 1, 1, 1};  // +0x44 +0x48 +0x4c +0x50 per corner: BL, BR, TL, TR
    int shaderType = 0;          // +0x54 (GetShader(type))
    Texture* colorMask = nullptr;   // +0x58
    float animTime = 0.f;        // +0x5c frame-chain timer (UpdateAnimatedSprites)
    int layer = 0;               // +0x60
    Sprite* next = nullptr;      // +0x64 chain of sprites drawn for one GUI window (9-slices)
    uint32_t seq = 0;            // PORT: creation order, keeps insertion order explicit
};

bool Init(int screenW, int screenH);        // after a GL context exists
void Shutdown();

// @0x202014 (no atlas): frames is the one-file animation's frame count. A sheet taller than 2048
// with 2+ frames is uploaded as two columns (the first (frames + 1) / 2 frames on the left), as
// SetFrame expects; w and h stay the image's.
Texture* CreateTexture(const ImageRGBA& img, const std::string& name, int frames = 0);
void SetWrapping(Texture* t, bool wrap);    // @Render::Texture::SetWrapping

// @0x1fe66c Render::CreateSprite(Texture*, RenderLayer, bool mirror, bool flip)
Sprite* CreateSprite(Texture* tex, int layer, bool mirror, bool flip);
Sprite* RemoveSprite(Sprite* s);            // @0x1fe240, returns s->next
void SetPosition(Sprite* s, float x, float y, float z);  // @0x1fdf04
void SetFrame(Sprite* s, int frameHeight, int frame);    // @0x1fe940 (one-file vertical sheets)
// @0x1fe940 Sprite::SetFrame(h, w, frame), the general form (sets size and v range of the frame).
void SetFrame(Sprite* s, float h, float w, int frame);
void SetShaderType(Sprite* s, int type);    // @0x1fe580
void SetVisibility(Sprite* s, bool v);      // @0x1fdfac
void SetColor(Sprite* s, float r, float g, float b);  // @0x1fde78
void SetAlpha(Sprite* s, float a);          // @0x1fde3c (all four corners)
void SetMirror(Sprite* s, bool mirror, bool flip);    // @0x1fdfc8
void SetTexture(Sprite* s, Texture* t);     // @0x1fe8cc
// @0x1fddac (no atlas): u = 0..1, swapped when mirrored; does nothing if already in that state.
void SetFrameMirror(Sprite* s, bool mirror);
// @0x1fe090 Sprite::HasPixelAt: (x, y) inside the sprite and, if its texture has a pixel mask, on
// an opaque mask cell.
bool HasPixelAt(const Sprite* s, float x, float y);
// @0x1fb9d4: one bit per 2x2 pixel block whose summed alpha is over 0x80 (rows of
// ((w + 1) / 2 + 7) / 8 bytes). CreateTexture builds it for 32-bit images when asked.
std::vector<uint8_t> GeneratePixelMask(const uint8_t* rgba, int w, int h);
int GetFrameWidth(const Texture* t);        // @0x201464
int GetFrameHeight(const Texture* t);       // @0x20146c h / frames for a one-file sheet
void ChangeLayer(Sprite* s, int layer);     // @0x1fc55c
// Texture from raw RGBA bytes (text). nearest: GL_NEAREST filters (text textures unless ForceLinear).
Texture* CreateTextureRGBA(int w, int h, const uint8_t* rgba, bool nearest, const std::string& name);
void RemoveTexture(Texture* t);
void SortRenderLayer(int layer, int order); // @0x1fc29c  order 1 = SpriteSortZ, 0 = SpriteSortTex

// Camera/world transform (UpdateShaderUniforms @0x1f6e10): the world shaders' instance +0x24 gets
// (offsetX+0.25, offsetY+0.25, zoom, zoom*aspect); the GUI's +0x10 gets (-W/2, H/2, 2/W, 2/H).
// offsetX is minus the world x at the screen centre, offsetY the world y there; zoom is clip units per
// world pixel horizontally (GetDefaultZoom = baseZoomFactor / (W/2)), aspect = W/H.
extern float offsetX, offsetY;   // 0x614040 0x614044
extern float zoom;               // 0x60f040 (0.001 until main_Loop_Init)
extern float aspect;             // 0x60f044
void SetViewportMapBounds(int minX, int minY, int maxX, int maxY);   // @0x1f5ce4 (tiles)
void ApplyViewportLimit();       // @0x1f5d08 keeps the view inside the map bounds, clamps the zoom
float GetDefaultZoom();          // @0x1f6084
// @0x1f60c0 CenterOn(x, y, _, animate=false, ...): the instant path. UNVERIFIED: the animated path
// (camera tween in Render::Update) is not ported; its callers come with milestones 3-4.
void CenterOn(float x, float y);
void SetForceLinear(bool on);   // @0x201514
bool GetForceLinear();          // @0x201528
void SetBaseZoomFactor(float z);   // @0x1f61c0 (world zoom chosen from the screen size)
float GetBaseZoomFactor();         // @0x1f61d4
int ScreenWidth();
int ScreenHeight();
void Frame();                               // sort dirty layers, build vertices, draw all layers
// @0x1fe378 (Render::Update, before drawing): sprites whose texture is a frame chain step to the
// next texture every frameTime seconds (a timer past 12.5 frame times restarts at 0).
void UpdateAnimatedSprites(float dt);
void RegisterAnimatedSprite(Sprite* s);     // @0x1fe5d4 (CreateSprite, SetTexture)
void UnregisterAnimatedSprite(Sprite* s);   // @0x1fe2f4 (SetTexture, Layer::RemoveSprite/Free)
bool SaveScreenshot(const char* pngPath);   // PORT: verification aid
size_t SpriteCount();

}  // namespace Render
