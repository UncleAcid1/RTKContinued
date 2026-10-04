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
    int frames = 0;        // +0x4c  frame count of a one-file animation sheet (0/1 = static)
    bool wrap = false;     // Texture::SetWrapping
    std::string name;
};

enum RenderLayer : int {  // indices used by the original (RenderLayer enum values)
    kLayerGround = 0, kLayerGroundDecal = 1, kLayerFlat2 = 2, kLayerFlat3 = 3,
    kLayerRings = 7, kLayerObjects = 8, kLayer9 = 9, kLayerFog = 11, kLayerGUI = 14, kLayer15 = 15,
    kLayerCount = 16
};

struct Sprite {
    int index = 0;               // +0x00 slot in its layer
    bool visible = true;         // +0x04
    float x = 0, y = 0, z = 0;   // +0x08 +0x0c +0x10  (x,y) = bottom-left corner, world/screen units
    float w = 0, h = 0;          // +0x14 +0x18
    float extra = 0;             // +0x1c vertex attribute 3
    Texture* tex = nullptr;      // +0x20
    bool screenSpace = false;    // +0x25 selects the GUI shader instance (Shader::Set(bool))
    float u0 = 0, u1 = 1;        // +0x28 +0x2c
    float vBottom = 1, vTop = 0; // +0x30 +0x34
    float color[4] = {1, 1, 1, 1};  // +0x38..+0x44
    int shaderType = 0;          // +0x54 (GetShader(type))
    Texture* colorMask = nullptr;   // +0x58
    int layer = 0;               // +0x60
    uint32_t seq = 0;            // PORT: creation order, keeps insertion order explicit
};

bool Init(int screenW, int screenH);        // after a GL context exists
void Shutdown();

Texture* CreateTexture(const ImageRGBA& img, const std::string& name);
void SetWrapping(Texture* t, bool wrap);    // @Render::Texture::SetWrapping

// @0x1fe66c Render::CreateSprite(Texture*, RenderLayer, bool mirror, bool flip)
Sprite* CreateSprite(Texture* tex, int layer, bool mirror, bool flip);
void RemoveSprite(Sprite* s);               // @0x1fe240
void SetPosition(Sprite* s, float x, float y, float z);  // @0x1fdf04
void SetFrame(Sprite* s, int frameHeight, int frame);    // @0x1fe940 (one-file vertical sheets)
void SetShaderType(Sprite* s, int type);    // @0x1fe580
void SortRenderLayer(int layer, int order); // @0x1fc29c  order 1 = SpriteSortZ, 0 = SpriteSortTex

// Camera/world transform: shader instance +0x24 gets (camX+0.25, camY+0.25, scaleX, scaleY);
// GUI instance +0x10 gets (-W/2, H/2, 2/W, 2/H)  (UpdateShaderUniforms @0x1f6e10).
void SetCamera(float centerX, float centerY, float zoom);
void Frame();                               // sort dirty layers, build vertices, draw all layers
bool SaveScreenshot(const char* pngPath);   // PORT: verification aid
size_t SpriteCount();

}  // namespace Render
