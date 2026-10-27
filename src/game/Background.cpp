#include "game/Background.h"

#include "engine/Render.h"
#include "engine/Resources.h"
#include "game/GameState.h"
#include "game/Locations.h"
#include "game/Map.h"

namespace Background {
namespace {
Render::Sprite* g_land = nullptr;     // +0x2c
Render::Sprite* g_horizon = nullptr;  // +0x04 (0x611630) the sky strip
// The mission maps' mountains and front strips (0x611634, 0x611638; CreateLand's mission-map
// branch): null elsewhere.
Render::Sprite* g_mountains = nullptr;
Render::Sprite* g_front = nullptr;
// The farm view (CreateFarm): its ground, the farm's building (and the animal farm's fence) and
// the four paper margins around it.
Render::Sprite* g_farmBg = nullptr;       // 0x61162c
Render::Sprite* g_farmBuilding = nullptr; // 0x611640
Render::Sprite* g_farmFence = nullptr;    // 0x61163c
Render::Sprite* g_farmPaper[4] = {};      // 0x611648..0x611654 top, bottom, left, right

// The mission maps' strip textures by tileset (summer, winter, desert), loaded once: the sky
// (0x611684 + 0xc * tileset), the mountains (0x611688 ...) and the front (0x61168c ...).
Render::Texture* g_stripTex[3][3] = {};
const char* const kStripFile[3][3] = {
    {"images/map_top/summer_night_top_0002_sky.jpg", "images/map_top/summer_night_top_0001_mountains.jpg",
     "images/map_top/summer_night_top_0000_front.jpg"},
    {"images/map_top/winter_night_top_0002_sky.jpg", "images/map_top/winter_night_top_0001_mountains.jpg",
     "images/map_top/winter_night_top_0000_front.jpg"},
    {"images/map_top/sunset_desert_0002_sky.jpg", "images/map_top/sunset_desert_0001_back.jpg",
     "images/map_top/Sunset_desert_0000_dunes.jpg"},
};

// One mission strip: eight tiles wide, centred on the map, at height y.
void PlaceStrip(Render::Sprite*& s, Render::Texture* t, int layer, float y) {
    if (!t) return;
    if (!s) s = Render::CreateSprite(t, layer, false, false);
    else Render::SetTexture(s, t);
    Render::SetWrapping(t, true);
    const float H = (float)t->h;
    s->u1 = 8.f;                             // +0x2c
    s->h = H;                                // +0x18
    s->vTop = 0.5f / (float)s->tex->h;       // +0x34
    s->vBottom = -0.5f / H + 1.f;            // +0x30
    s->w = (float)(t->w << 3);               // +0x14
    Render::SetPosition(s, (float)(t->w << 3) * -0.5f + (float)(unsigned)Map::GetGridWidth() * 84.f * 0.5f, y, 0.9f);
}
}  // namespace

// @0x10b9ec. PORT: the land and the top strip are made again here; the original keeps their
// sprites and swaps the texture (the same result).
void CreateLand(unsigned tileset) {
    g_mountains = Render::RemoveSprite(g_mountains);
    g_front = Render::RemoveSprite(g_front);
    if (g_land) { Render::RemoveSprite(g_land); g_land = nullptr; }
    if (g_horizon) { Render::RemoveSprite(g_horizon); g_horizon = nullptr; }

    // Background::Init: ground textures per tileset (direct images)
    static const char* ground[] = {"images/grass_01.png", "images/snow_01.png", "images/sand_01.jpg",
                                   "images/snow_01.png", "images/swamp_01.png"};
    // (Init @0x10c308 loads snow_01.png once and stores it for tilesets 1 and 3)
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

    // A mission map (or a fight entered from one) of the first three tilesets: the sky, mountains
    // and front strips (Update moves them with the camera), and no terrain shadow switch.
    bool mission = Locations::IsMissionMap((uint32_t)GameState::GetCurrentMapID());
    if (!mission && GameState::IsCombatActive()) mission = Locations::IsMissionMap(GameState::mReturnMapID);
    if (tileset < 3 && mission) {
        for (int i = 0; i < 3; ++i)
            if (!g_stripTex[tileset][i]) g_stripTex[tileset][i] = Resources::GetDirectImage(kStripFile[tileset][i]);
        PlaceStrip(g_horizon, g_stripTex[tileset][0], Render::kLayerGround, -40.5f);
        PlaceStrip(g_mountains, g_stripTex[tileset][1], Render::kLayerFlat2, -26.5f);
        PlaceStrip(g_front, g_stripTex[tileset][2], Render::kLayerFlat2, -25.5f);
        return;
    }
    // The top strip of the other maps (0x10bfa0).
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
    Render::EnableTerrainShadowing(true);
}

// @0x10b4fc: the farm's ground (images/Farm/farm_bg) at the farm patch's row, the farm's own
// building in its corner, and four tiled paper margins framing the ground. (The farm argument is
// unused.)
void CreateFarm(int) {
    // PORT: the original sets a flag at +0x48 on these textures before creating the sprites (keeping
    // them resident); the port's textures always stay loaded.
    Map::Patch* patch = Map::GetFarmPatch();
    float row = (float)patch->y * 42.f * 0.5f;
    if (Render::Texture* t = Resources::GetImage("images/Farm/farm_bg")) {
        g_farmBg = Render::CreateSprite(t, Render::kLayerGround, false, false);
        Render::SetPosition(g_farmBg, 30.f, ((float)t->h + row) - 80.f, 1.f);
    }
    uint32_t id = Map::GetCurrentFarm()->id;
    auto place = [](const char* image, int layer, float x, float y) -> Render::Sprite* {
        Render::Texture* t = Resources::GetImage(image);
        if (!t) return nullptr;
        Render::Sprite* s = Render::CreateSprite(t, layer, false, false);
        Render::SetPosition(s, x, y + (float)t->h, 1.f);
        return s;
    };
    if (id == 0x13) {
        g_farmBuilding = place("images/Buildings/farm/small/small_farm", Render::kLayerFlat3, 205.f, 70.f + row);
    } else if (id == 0x72) {
        g_farmBuilding = place("images/Buildings/farm/oil/building", Render::kLayerFlat3, 130.f, row - 85.f);
    } else if (id == 0x3ee) {
        g_farmBuilding = place("images/Buildings/farm/animal/building", Render::kLayerFlat3, 15.f, 25.f + row);
        // (the fence is placed by the building image's height, as on the original)
        if (Render::Texture* t = Resources::GetImage("images/Buildings/farm/animal/fence")) {
            g_farmFence = Render::CreateSprite(t, Render::kLayer9, false, false);
            float h = g_farmBuilding ? g_farmBuilding->tex->h : 0.f;
            Render::SetPosition(g_farmFence, 25.f, 40.f + row + h, 1.f);
        }
    }
    Render::Texture* paper = Resources::GetImage("images/WorldMap/pattern_paper_bg");
    if (!paper) return;   // (the original falls back to Render::GetImageDebugTex)
    static const float kScale[4][2] = {{8.f, 1.f}, {8.f, 1.f}, {2.f, 4.f}, {2.f, 4.f}};
    for (int i = 0; i < 4; ++i) {
        Render::Sprite* s = Render::CreateSprite(paper, Render::kLayerGUI, false, false);
        s->screenSpace = false;
        s->w *= kScale[i][0];
        s->u1 *= kScale[i][0];
        s->vBottom *= kScale[i][1];
        s->h *= kScale[i][1];
        Render::SetWrapping(s->tex, true);
        g_farmPaper[i] = s;
    }
    if (g_farmBg) {
        float x = g_farmBg->x, y = g_farmBg->y, w = g_farmBg->w, h = g_farmBg->h;
        float left = x - g_farmPaper[2]->w;
        Render::SetPosition(g_farmPaper[0], left, y - h, 1.f);
        Render::SetPosition(g_farmPaper[1], left, y + g_farmPaper[1]->h, 1.f);
        Render::SetPosition(g_farmPaper[2], left, y, 1.f);
        Render::SetPosition(g_farmPaper[3], x + w, y, 1.f);
    }
    Render::EnableTerrainShadowing(false);
}

// @0x10b238: with all three strips (mission maps) they follow the camera at different rates: the
// sky by half the camera offset, the mountains by an eighth, the front strip not at all.
void Update(double) {
    if (!g_horizon || !g_mountains || !g_front) return;
    float mid = (float)(unsigned)Map::GetGridWidth() * 84.f * 0.5f;
    Render::SetPosition(g_horizon, (float)(g_horizon->tex->w << 3) * -0.5f + mid + Render::offsetX * -0.5f, -40.5f, 0.9f);
    Render::SetPosition(g_mountains, (float)(g_mountains->tex->w << 3) * -0.5f + mid + Render::offsetX * -0.125f, -26.5f, 0.9f);
    Render::SetPosition(g_front, (float)(g_front->tex->w << 3) * -0.5f + mid, -25.5f, 0.9f);
}

// @0x10b47c. PORT: the low-memory device path (Render::FreeMemory) is left out.
void RemoveFarm() {
    g_farmBg = Render::RemoveSprite(g_farmBg);
    for (Render::Sprite*& s : g_farmPaper) s = Render::RemoveSprite(s);
    g_farmBuilding = Render::RemoveSprite(g_farmBuilding);
    g_farmFence = Render::RemoveSprite(g_farmFence);
}

// @0x10b1f0: the farm ground's left, top, right and bottom (world).
void GetFarmBounds(float& left, float& top, float& right, float& bottom) {
    if (!g_farmBg) return;
    left = g_farmBg->x;
    top = g_farmBg->y - g_farmBg->h;
    right = g_farmBg->x + g_farmBg->w;
    bottom = g_farmBg->y;
}

void SetBrokenFence(bool broken) { Render::ChangeLayer(g_farmFence, broken ? Render::kLayerFlat3 : Render::kLayer9); }

}  // namespace Background
