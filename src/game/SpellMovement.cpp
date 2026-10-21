#include "engine/SoundsManager.h"
#include "game/SpellMovement.h"

#include <cmath>
#include <cstdlib>

#include "engine/Render.h"
#include "engine/Resources.h"
#include "game/Combat.h"
#include "game/GameState.h"
#include "game/Map.h"
#include "game/Spells.h"
#include "game/StringTable.h"
#include "gui/GUI.h"
#include "hud/HUD.h"

namespace SpellMovement {
namespace {

bool g_active = false;                         // 0x630c44
int g_epicenterX = 0, g_epicenterY = 0;        // 0x630c48 0x630c4c
Render::Texture* g_red = nullptr;              // 0x630c50 the middle
Render::Texture* g_orange = nullptr;           // 0x630c54
Render::Texture* g_yellow = nullptr;           // 0x630c58
Render::Sprite* g_sprites = nullptr;           // 0x630c5c the footprint's sprites, chained by next
Spells::SpellInfo* g_spell = nullptr;          // 0x630c60
int g_selX = 0, g_selY = 0;                    // 0x630c64 0x630c68 the selected tile
bool g_dragging = false;                       // 0x630c6c
float g_baseX = 0.f, g_baseY = 0.f;            // 0x630c70 0x630c74 the selected tile's world point
float g_dragX = 0.f, g_dragY = 0.f;            // 0x630c78 0x630c7c the drag so far, in world units

// The footprints (0x5a8790, 25 bytes a spell range, 5 x 5): 0 none, 1 yellow, 2 orange, 3 the middle.
const char kFootprints[4][25] = {
    {0, 0, 0, 0, 0, 0, 2, 2, 2, 0, 0, 2, 3, 2, 0, 0, 2, 2, 2, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 0, 0, 0, 1, 2, 1, 0, 1, 2, 3, 2, 1, 0, 1, 2, 1, 0, 0, 0, 1, 0, 0},
    {0, 1, 1, 1, 0, 1, 2, 2, 2, 1, 1, 2, 3, 2, 1, 1, 2, 2, 2, 1, 0, 1, 1, 1, 0},
    {1, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 2, 3, 2, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 1},
};

// The tile that is `offset` rows and columns... see CheckArenaBounds: the arena's first tile
// (its corner) from its middle and size (mBossMapPositions [8..11]).
void ArenaCorner(int& x, int& y) {
    const int* pos = GameState::mBossMapPositions;
    x = pos[8];
    y = pos[9];
    const int w = pos[10], h = pos[11];
    int n = w / 2;
    if ((w & 1) == 0) --n;
    if (0 < n) {
        int yy = y;
        do {
            const bool odd = std::abs(yy) & 1;
            --yy;
            if (odd) ++x;
            --x;
        } while (yy != y - n);
        y -= n;
    }
    int n2 = h / 2;
    if ((h & 1) == 0) --n2;
    if (0 < n2) {
        int yy = y;
        do {
            const int next = yy - 1;
            if (yy % 2 == 1) ++x;
            yy = next;
        } while (yy != y - n2);
        y -= n2;
    }
}

// @0x3a5964: tile (px, py) is inside the arena (always, with a siege combat: GameState::g_Combat,
// dead in 5.11).
bool CheckArenaBounds(int px, int py) {
    int x, y;
    ArenaCorner(x, y);
    const int w = GameState::mBossMapPositions[10], h = GameState::mBossMapPositions[11];
    if (h <= 0) return false;
    const int end = y + h;
    for (;;) {
        if (0 < w) {
            if (x == px && y == py) return true;
            int cx = x, cy = y;
            for (;;) {
                if (cy % 2 == 1) ++cx;
                const bool last = cy == (w - 1) + y;
                ++cy;
                if (last) break;
                if (px == cx && py == cy) return true;
            }
        }
        if (std::abs(y) & 1) ++x;
        ++y;
        if (y == end) return false;
        --x;
    }
}

// @0x3a5800
void RemovePlacementInfo() {
    while (g_sprites) g_sprites = Render::RemoveSprite(g_sprites);
}

// @0x3a5708: the footprint's middle (3) in the 5 x 5 pattern: column x, row y.
void FindEpicenterPos(const char* pattern, int& x, int& y) {
    for (int row = 0; row < 5; ++row, pattern += 5)
        for (int col = 0; col < 5; ++col)
            if (pattern[col] == 3) {
                x = col;
                y = row;
                return;
            }
}

// @0x3a5ae0: the footprint's sprites on the ground. The 5 x 5 pattern runs along the map's two
// diagonals: a column steps one tile row down, a row one down and (by the row's parity) a column
// aside. Its middle is the selected tile.
void CreatePlacementInfo() {
    RemovePlacementInfo();
    const char* pattern = kFootprints[g_spell->range];
    int ex = 0, ey = 0;
    FindEpicenterPos(pattern, ex, ey);   // (the original never uses the result)
    int x = g_selX;
    if (std::abs(g_selY) & 1) ++x;
    int a = x - 1;
    int b = (std::abs(g_selY - 1) & 1) == 0 ? x - 2 : a;
    int c = (std::abs(g_selY - 1) & 1) == 0 ? a : x;
    int start = ((g_selY - 2) % 2 == 1) ? c : b;
    if ((g_selY - 3) % 2 == 1) ++start;
    int rowX = start;
    int rowY = g_selY - 4;
    for (int row = 0; row < 5; ++row) {
        int cx = rowX, cy = rowY;
        for (int col = 0; col < 5; ++col) {
            const char v = pattern[row * 5 + col];
            Render::Texture* tex = v == 2 ? g_orange : v == 3 ? g_red : v == 1 ? g_yellow : nullptr;
            if (tex) {
                Render::Sprite* s = Render::CreateSprite(tex, Render::kLayerFlat3, false, false);
                Render::SetPosition(s, ((cy & 1) ? 42.f : 0.f) + (float)cx * 84.f, (float)cy * 42.f * 0.5f, 0.3f);
                if (v == 3) {
                    g_epicenterX = cx;
                    g_epicenterY = cy;
                }
                s->next = g_sprites;
                g_sprites = s;
            }
            if (cy % 2 == 1) ++cx;
            ++cy;
        }
        if (std::abs(rowY) & 1) ++rowX;
        ++rowY;
        --rowX;
    }
    Render::SortRenderLayer(Render::kLayerFog, 1);
}

// @0x3a5ea8: the selection moves to tile (x, y) if it is in the arena.
void SetSelectionXY(int x, int y) {
    if (!CheckArenaBounds(x, y)) return;
    g_selY = y;
    g_selX = x;
    CreatePlacementInfo();
}

}  // namespace

void Init() {
    g_active = false;
    g_red = Resources::GetImage("images/Tileset/arena/tile_magic_red");
    g_orange = Resources::GetImage("images/Tileset/arena/tile_magic_orange");
    g_yellow = Resources::GetImage("images/Tileset/arena/tile_magic_yellow");
}

bool Activated() { return g_active; }

void GetEpicenterPos(int& x, int& y) {
    x = g_epicenterX;
    y = g_epicenterY;
}

void Activate(Spells::SpellInfo* spell, int x, int y) {
    if (g_active) return;
    g_selY = y;
    g_active = true;
    g_dragging = false;
    g_spell = spell;
    g_selX = x;
    // (not in the tutorial's main game, 0x80: the hint)
    if (GameState::tutorial != 0x80) HUDWindow::SetInfoText(StringTable::GetString("SPELL_MOVE"));
    CreatePlacementInfo();
    if (BaseCombat* combat = GameState::GetActiveCombat()) combat->SpellAreaSelectionStarted();
}

void Decline() {
    if (!g_active) return;
    HUDWindow::SetInfoText(nullptr);
    g_active = false;
    RemovePlacementInfo();
    if (BaseCombat* combat = GameState::GetActiveCombat()) combat->SpellAreaSelectionEnded();
    SoundsManager::PlaySound("ui_swing_out", 1, false);
}

void Accept() {
    if (!g_active) return;
    HUDWindow::SetInfoText(nullptr);
    g_active = false;
    RemovePlacementInfo();
    if (BaseCombat* combat = GameState::GetActiveCombat()) combat->QueueSpellCast(g_spell->slot, g_selX, g_selY);
    // (GameState::g_Combat's CombatManager::OnSpellCast: the siege system, dead in 5.11. Online,
    // dormant: OG::MakeRequest(0x14, 0x10, spell id, -1, 0).)
}

bool Click(int x, int y, bool pressed) {
    if (!g_active) return false;
    if (GameState::tutorial == 0x80 && GameState::secondTutorial != 0x100) return false;
    if (pressed) {
        int wx = g_selX, wy = g_selY;
        g_dragging = true;
        Map::TileCoordinatesToWorld(wx, wy);
        g_dragX = 0.f;
        g_dragY = 0.f;
        g_baseX = (float)wx + 42.f;
        g_baseY = (float)wy - 21.f;
        return true;
    }
    // A short drag is a tap: the selection goes to the tile under it.
    if (g_dragging && std::fabs(g_dragX) < 10.f && std::fabs(g_dragY) < 10.f) {
        int wx = x, wy = y;
        Map::MouseCoordinatesToWorld(wx, wy);
        Map::WorldCoordinatesToTile(wx, wy);
        SetSelectionXY(wx, wy);
    }
    g_dragging = false;
    return false;
}

bool Move(int, int, int dx, int dy) {
    if (!g_active || !g_dragging) return false;
    const float k = 2.f / Render::zoom;
    g_dragX += ((float)dx / (float)GUI::ScreenWidth()) * k;
    g_dragY += ((float)dy / (float)GUI::ScreenHeight()) * (k / Render::aspect);
    int tx = (int)(g_dragX + g_baseX), ty = (int)(g_dragY + g_baseY);
    Map::WorldCoordinatesToTile(tx, ty);
    if ((g_selX == tx && g_selY == ty) || !CheckArenaBounds(tx, ty)) return true;
    g_selX = tx;
    g_selY = ty;
    CreatePlacementInfo();
    return true;
}

}  // namespace SpellMovement
