#include "game/Background.h"

#include "engine/Render.h"
#include "engine/Resources.h"
#include "game/Map.h"

namespace Background {
namespace {
Render::Sprite* g_land = nullptr;     // +0x2c
Render::Sprite* g_horizon = nullptr;  // +0x04
}  // namespace

void CreateLand(unsigned tileset) {
    if (g_land) { Render::RemoveSprite(g_land); g_land = nullptr; }
    if (g_horizon) { Render::RemoveSprite(g_horizon); g_horizon = nullptr; }

    // Background::Init: ground textures per tileset (direct images)
    static const char* ground[] = {"images/grass_01.png", "images/snow_01.png", "images/sand_01.jpg",
                                   "images/snow_01.png", "images/swamp_01.png"};
    // UNVERIFIED: index 3 (Init stores five textures at +0x44..+0x54; the 4th path is not resolved yet)
    if (tileset < 5) {
        if (Render::Texture* t = Resources::GetDirectImage(ground[tileset])) {
            g_land = Render::CreateSprite(t, Render::kLayerGround, false, false);
            Render::SetWrapping(t, true);
            int nx = (Map::GetGridWidth() + 0x30) * 0x54 / t->w;
            int ny = (Map::GetGridHeight() + 0x3e) * 0x15 / t->h;
            g_land->u1 = (float)nx;                 // +0x2c
            g_land->w = (float)(t->w * nx);         // +0x14
            g_land->vBottom = (float)ny;            // +0x30
            g_land->h = (float)(t->h * ny);         // +0x18
            float x = (float)-t->w * 20.f;
            float y = (float)(t->h * ny) + (float)t->h * -2.f;
            Render::SetPosition(g_land, x, y, 0.9f);
            Render::SetShaderType(g_land, 2);
        }
    }

    // Horizon strip for non-mission maps (0x10bfa0). Mission maps use images/map_top/* (not ported yet).
    static const char* top[] = {"images/Tileset/summer/top", "images/Tileset/winter/winter_top",
                                "images/Tileset/desert/desert_top", "images/Tileset/winter/winter_top",
                                "images/Tileset/swamp/swamp_top"};
    if (tileset < 5) {
        if (Render::Texture* t = Resources::GetImage(top[tileset])) {
            g_horizon = Render::CreateSprite(t, Render::kLayerGround, false, false);
            Render::SetWrapping(t, true);
            float H = (float)t->h;
            g_horizon->u1 = 6.0f;                   // +0x2c
            g_horizon->h = H;                       // +0x18
            g_horizon->vTop = 0.5f / H;             // +0x34
            g_horizon->vBottom = -0.5f / H + 1.0f;  // +0x30
            g_horizon->w = (float)(t->w * 6);       // +0x14
            float x = (float)(t->w * 6) * -0.5f + (float)Map::GetGridWidth() * 84.f * 0.5f;
            Render::SetPosition(g_horizon, x, -40.5f, 0.9f);
            Render::SetShaderType(g_horizon, 2);
        }
    }
}

}  // namespace Background
