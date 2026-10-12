#include "game/Fog.h"

#include <cstdio>
#include <memory>
#include <vector>

#include "engine/Render.h"
#include "engine/Resources.h"
#include "game/SaveManager.h"
#include "gui/GUI.h"

namespace Fog {

float scale = 1.f;   // 0x60ef34

namespace {

struct BlockInfo {                 // 0x28 bytes
    float target = 1.f;            // +0x00 the alpha it fades to (1: covered)
    float alpha = 1.f;             // +0x04 the alpha it shows
    Render::Sprite* sprite = nullptr;   // +0x08
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0; // +0x0c..+0x18 its world box
    int col = 0, row = 0;          // +0x1c +0x20
    int updateIndex = -1;          // +0x24 its place in the fading list when it was added (-1: never)
};

struct QuadNode {                  // 0x1c bytes
    bool leaf = false;             // +0x00 one block
    QuadNode* child[4] = {};       // +0x04..+0x10
    uint8_t col = 0, row = 0;      // +0x14 +0x15
    uint8_t w = 0, h = 0;          // +0x16 +0x17 in blocks
    // +0x18: what draws it: a leaf's is its block; an inner node has its own while it is still
    // covered as a whole.
    BlockInfo* info = nullptr;
};

int g_cols = 0, g_rows = 0;                // 0x611f14 0x611f18
std::vector<BlockInfo> g_blocks;           // 0x611f1c
int g_originX = 0, g_originY = 0;          // 0x611f20 0x611f24 (-84, -84)
int g_cell = 0;                            // 0x611f28 (42)
Render::Texture* g_image = nullptr;        // 0x611f2c images/fog.jpg
Render::Texture* g_imageDesert = nullptr;  // 0x611f30 images/fog_desert.jpg
Render::Sprite* g_rim[6] = {};             // 0x611f54..0x611f68 the cover beside and below the map
std::vector<BlockInfo*> g_fading;          // 0x611f6c (count 0x611f78)
std::unique_ptr<float[]> g_stored;         // 0x611f7c StoreFogState's targets
int g_spriteCount = 0;                     // 0x611f80
QuadNode* g_root = nullptr;                // 0x611f84
Render::Texture* g_texture = nullptr;      // 0x611f88 the image in use
unsigned g_tileset = 0;                    // 0x611f8c
int g_width = 0, g_height = 0;             // 0x611f90 0x611f94 in pixels
bool g_render = true;                      // 0x60ef30

// (The original takes nodes and inner infos from pools, 0x611f34 and 0x611f44.)

// The sprite's part of the wrapping fog image: the world box as texture coordinates.
void SetBox(Render::Sprite* s, float x0, float y0, float x1, float y1) {
    float tw = (float)g_texture->w, th = (float)g_texture->h;
    s->u0 = x0 / tw;
    s->vTop = y0 / th;
    s->u1 = x1 / tw;
    s->vBottom = y1 / th;
}

// @0x174c44 Fog::FillQuadNode: the node splits in four until `level` reaches `size` (one block).
void FillQuadNode(QuadNode* n, int level, int size) {
    if (level >= size) {
        n->info = nullptr;
        n->leaf = true;
        for (QuadNode*& c : n->child) c = nullptr;
        return;
    }
    n->leaf = false;
    for (QuadNode*& c : n->child) {
        c = new QuadNode;
        n->info = nullptr;
    }
    uint8_t half = (uint8_t)(size / (level * 2));
    n->child[0]->col = n->col;
    n->child[0]->row = n->row;
    n->child[1]->row = n->row;
    n->child[1]->col = (uint8_t)(half + n->col);
    n->child[2]->row = (uint8_t)(half + n->row);
    n->child[2]->col = n->col;
    n->child[3]->row = (uint8_t)(half + n->row);
    n->child[3]->col = (uint8_t)(half + n->col);
    for (QuadNode*& c : n->child) {
        if (g_cols < (int)c->col || g_rows < (int)c->row) {   // off the grid
            delete c;
            c = nullptr;
        } else {
            FillQuadNode(c, level << 1, size);
            c->w = half;
            c->h = half;
            if (g_cols < (int)half + (int)c->col) c->w = (uint8_t)(g_cols - c->col);
            if (g_rows < (int)half + (int)c->row) c->h = (uint8_t)(g_rows - c->row);
        }
        n->info = nullptr;
    }
}

// @0x173f74 Fog::ClearQuadNode
void ClearQuadNode(QuadNode* n) {
    if (n->info && n->info->sprite) {
        n->info->sprite = Render::RemoveSprite(n->info->sprite);
        --g_spriteCount;
    }
    for (QuadNode*& c : n->child) {
        if (!c) continue;
        ClearQuadNode(c);
        delete c;
        c = nullptr;
    }
    if (n->info && !n->leaf) delete n->info;   // (a leaf's is a block)
}

// @0x1741ac Fog::AddBlockToUpdate: the block starts fading (once).
void AddBlockToUpdate(BlockInfo* b) {
    if (b->updateIndex != -1) return;
    if (b->sprite) Render::SetShaderType(b->sprite, 1);
    b->updateIndex = (int)g_fading.size();
    g_fading.push_back(b);
}

// @0x1742fc Fog::OpenTile: the nodes over the block split down to it (each part gets its own
// sprite), and the block takes the target if it is lower. In the first and last column only row 0
// opens.
void OpenTile(QuadNode* n, int col, int row, float target) {
    if (n->leaf) {
        if ((col == 0 || g_cols - 1 == col) && row != 0) return;
        if ((int)n->col == col && (int)n->row == row) {
            if (target < n->info->target) n->info->target = target;
            AddBlockToUpdate(n->info);
        }
        return;
    }
    if (col < (int)n->col || row < (int)n->row || col >= (int)n->w + (int)n->col || row >= (int)n->h + (int)n->row)
        return;
    if (n->info) {
        n->info->sprite = Render::RemoveSprite(n->info->sprite);
        --g_spriteCount;
        delete n->info;
        n->info = nullptr;
        for (QuadNode*& c : n->child) {
            if (!c) continue;
            if (!c->leaf) {
                c->info = new BlockInfo;
            } else {
                int i = g_cols * c->row + c->col;
                if (i < g_rows * g_cols) c->info = &g_blocks[(size_t)i];
            }
            BlockInfo* b = c->info;
            if (!b) {
                delete c;
                c = nullptr;
                continue;
            }
            b->alpha = 1.f;
            b->x0 = g_cell * c->col + g_originX;
            b->y0 = g_cell * c->row + g_originY;
            b->x1 = g_cell * (c->w + c->col) + g_originX;
            b->y1 = g_cell * (c->h + c->row) + g_originY;
            ++g_spriteCount;
            b->sprite = Render::CreateSprite(g_texture, Render::kLayerFog, false, false);
            b->sprite->w = (float)(b->x1 - b->x0) * scale;
            b->sprite->h = scale * (float)(b->y1 - b->y0);
            SetBox(b->sprite, (float)b->x0, (float)b->y0, (float)b->x1, (float)b->y1);
            Render::SetPosition(b->sprite, scale * (float)b->x0, b->sprite->h + scale * (float)b->y0, 0.01f);
            Render::SetVisibility(b->sprite, g_render);
        }
    }
    for (QuadNode* c : n->child)
        if (c) OpenTile(c, col, row, target);
}

// @0x17426c Fog::ToggleFogRender(QuadNode*)
void ToggleFogRender(QuadNode* n) {
    if (n->info && n->info->sprite) Render::SetVisibility(n->info->sprite, g_render);
    for (QuadNode* c : n->child)
        if (c) ToggleFogRender(c);
}

}  // namespace

// @0x174118 Fog::Init
void Init() {
    if (!g_image) g_image = Resources::GetDirectImage("images/fog.jpg");
    if (!g_imageDesert) g_imageDesert = Resources::GetDirectImage("images/fog_desert.jpg");
    Render::SetWrapping(g_image, true);
    Render::SetWrapping(g_imageDesert, true);
    g_texture = g_image;
}

// @0x17404c Fog::Deinit
void Deinit(bool forgetImages) {
    g_fading.clear();
    if (g_root) {
        ClearQuadNode(g_root);
        delete g_root;
    }
    g_root = nullptr;
    g_blocks.clear();
    for (Render::Sprite*& s : g_rim) s = Render::RemoveSprite(s);
    if (!forgetImages) return;
    g_image = nullptr;
    g_texture = nullptr;
    g_imageDesert = nullptr;
}

// @0x174e94 Fog::Create
void Create(unsigned w, unsigned h, unsigned tileset) {
    Deinit(false);
    g_width = (int)w * 0x54;
    g_cell = 0x2a;
    g_tileset = tileset;
    g_height = (int)h * 0x2a;
    if (g_width % 0x2a != 0) g_width = g_width + 0x2a - g_width % 0x2a;
    g_originY = -0x54;
    g_originX = -0x54;
    g_texture = tileset == 2 ? g_imageDesert : g_image;
    if (g_height % 0x2a != 0) g_height = g_height + 0x2a - g_height % 0x2a;
    g_cols = g_width / g_cell;
    g_rows = g_height / g_cell;
    if (g_cols > 0xff || g_rows > 0xff)
        std::fprintf(stderr, "ERROR: Fog::Create() Fog grid size %dx%d is too large for fog coordinate unit\n", g_cols,
                     g_rows);
    g_blocks.assign((size_t)(g_rows * g_cols), BlockInfo{});
    for (int row = 0; row < g_rows; ++row)
        for (int col = 0; col < g_cols; ++col) {
            BlockInfo& b = g_blocks[(size_t)(row * g_cols + col)];
            b.target = 1.f;
            b.alpha = 1.f;
            b.col = col;
            b.x0 = col * g_cell;
            b.x1 = (col + 1) * g_cell;
            b.updateIndex = -1;
            b.sprite = nullptr;
            b.row = row;
            b.y0 = row * g_cell;
            b.y1 = (row + 1) * g_cell;
        }
    // The tree: as deep as the larger side needs.
    int side = (g_cols < g_rows ? g_rows : g_cols) - 1;
    unsigned bits = 0;
    for (; side != 0; side /= 2) ++bits;
    g_root = new QuadNode;
    g_root->col = 0;
    g_root->row = 0;
    g_root->w = (uint8_t)g_cols;
    g_root->h = (uint8_t)g_rows;
    g_root->info = nullptr;
    g_root->leaf = false;
    FillQuadNode(g_root, 1, 1 << (bits & 0xff));
    // All of it under one sprite. (As the original: its size is the box's far corner, not its extent.)
    BlockInfo* b = g_root->info = new BlockInfo;
    b->target = 1.f;
    b->alpha = 1.f;
    b->x0 = g_cell * g_root->col + g_originX;
    b->y0 = g_root->row * g_cell + g_originY;
    b->x1 = (g_root->w + g_root->col) * g_cell + g_originX;
    b->y1 = (g_root->h + g_root->row) * g_cell + g_originY;
    ++g_spriteCount;
    b->sprite = Render::CreateSprite(g_texture, Render::kLayerFog, false, false);
    SetBox(b->sprite, (float)b->x0, (float)b->y0, (float)b->x1, (float)b->y1);
    b->sprite->h = scale * (float)b->y1;
    b->sprite->w = (float)b->x1 * scale;
    Render::SetPosition(b->sprite, scale * (float)b->x0, scale * (float)b->y1 + scale * (float)b->y0, 0.01f);

    // The cover around the map: 1536 pixels to the left and right (a fading strip beside the top
    // block row, then the rest down to 19 blocks below the map) and, on a tall screen with a short
    // map, below it.
    auto rim = [](int i, float w, float h, float x0, float y0, float x1, float y1, float y, bool fade) {
        Render::Sprite* s = g_rim[i] = Render::CreateSprite(g_texture, Render::kLayerFog, false, false);
        s->w = w;
        s->h = h;
        SetBox(s, x0, y0, x1, y1);
        Render::SetPosition(s, x0 * scale, y, 0.01f);
        if (fade) {
            s->alpha[2] = 0.f;
            s->alpha[3] = 0.f;
            Render::SetShaderType(s, 1);
        }
    };
    const float cell = (float)g_cell, ox = (float)g_originX, oy = (float)g_originY;
    const float right = (float)(g_cols * g_cell + g_originX);
    float rh = cell * scale;
    rim(0, scale * 1536.f, rh, (float)(g_originX - 0x600), oy, ox, (float)(g_originY + g_cell), rh + scale * oy, true);
    rh = (float)(g_cell * (g_rows + 0x13)) * scale;
    rim(1, scale * 1536.f, rh, (float)(g_originX - 0x600), (float)(g_originY + g_cell), ox,
        (float)(g_cell * (g_rows + 0x14) + g_originY), rh + scale * (float)(g_originY + g_cell), false);
    rh = cell * scale;
    rim(2, scale * 1536.f, rh, right, oy, (float)(g_originX + 0x600 + g_cols * g_cell), (float)(g_originY + g_cell),
        rh + scale * oy, true);
    rh = (float)(g_cell * (g_rows + 0x13)) * scale;
    rim(3, scale * 1536.f, rh, right, (float)(g_originY + g_cell), (float)(g_originX + 0x600 + g_cols * g_cell),
        (float)(g_cell * (g_rows + 0x14) + g_originY), rh + scale * (float)(g_originY + g_cell), false);
    if (GUI::ScreenHeight() > 0x400 && g_rows < 0x20) {
        float top = (float)(g_cell * (g_rows - 1) + g_originY);
        rim(4, (float)(g_cols * g_cell) * scale, cell * scale, ox, (float)(g_cell * (g_rows - 2) + g_originY), right, top,
            scale * top, true);
        rh = (float)(g_cell * 0x13) * scale;
        rim(5, (float)(g_cols * g_cell) * scale, rh, ox, top, right, (float)(g_cell * (g_rows + 0x12) + g_originY),
            rh + scale * top, false);
    }
}

// @0x17361c
float GetBlockAlpha(int col, int row) {
    if ((row | col) < 0 || col >= g_cols || row >= g_rows) return 0.f;
    return g_blocks[(size_t)(g_cols * row + col)].alpha;
}

// @0x173668
void FillZero() {
    for (BlockInfo& b : g_blocks) b.target = 0.f;
}

// @0x1736d8
void FillOne(int x0, int y0, int x1, int y1) {
    int ax = x0 - g_originX, ay = y0 + (-0xa8 - g_originY);
    int bx = x1 - g_originX, by = y1 + (-0xa8 - g_originY);
    int c0 = ax / g_cell, r0 = ay / g_cell, c1 = bx / g_cell, r1 = by / g_cell;
    float radius = (float)(bx - ax) * 0.5f, ry = (float)(by - ay) * 0.5f;
    if (radius <= ry) radius = ry;
    float cx = (float)(bx + ax) * 0.5f, cy = (float)(by + ay) * 0.5f;
    for (int c = c0; c <= c1; ++c)
        for (int r = r0; r <= r1; ++r) {
            if ((unsigned)c >= (unsigned)g_cols || (unsigned)r >= (unsigned)g_rows) continue;
            float half = (float)g_cell * 0.5f;
            float dy = (half + (float)(g_cell * r)) - cy;
            float dx = ((float)(g_cell * c) + half) - cx;
            if (radius * radius > dy * dy + dx * dx) g_blocks[(size_t)(g_cols * r + c)].target = 1.f;
        }
}

// @0x173888
bool IsVisible(int x0, int y0, int x1, int y1) {
    if (!g_render || g_cell == 0) return true;
    int c0 = (x0 - g_originX) / g_cell, r0 = (y0 - g_originY) / g_cell;
    int c1 = (x1 - g_originX) / g_cell, r1 = (y1 - g_originY) / g_cell;
    for (int c = c0; c <= c1; ++c)
        for (int r = r0; r <= r1; ++r) {
            if ((unsigned)c >= (unsigned)g_cols || (unsigned)r >= (unsigned)g_rows) continue;
            if (g_blocks[(size_t)(g_cols * r + c)].target < 1.f) return true;
        }
    return false;
}

// @0x173b24
void StoreFogState() {
    g_stored.reset(new float[(size_t)(g_cols * g_rows)]);
    for (int i = 0; i < g_cols * g_rows; ++i) g_stored[(size_t)i] = g_blocks[(size_t)i].target;
}

// @0x1759c8
void RestoreFogState() {
    Create((unsigned)(g_cols / 2), (unsigned)g_rows, g_tileset);
    if (!g_stored) {
        std::fprintf(stderr, "ERROR: Fog::RestoreFogState() restore failed - no restore info\n");
        return;
    }
    for (int row = 0; row < g_rows; ++row)
        for (int col = 0; col < g_cols; ++col) {
            float t = g_stored[(size_t)(row * g_cols + col)];
            if (t < 1.f) OpenTile(g_root, col, row, t);
        }
    g_stored.reset();
}

// @0x173bd0
void SaveFog() {
    SaveManager::BeginChunk(SaveManager::kFogV2);
    SaveManager::SaveUnsigned((uint32_t)g_cols);
    SaveManager::SaveUnsigned((uint32_t)g_rows);
    for (int row = 0; row < g_rows; ++row)
        for (int col = 0; col < g_cols; ++col)
            SaveManager::SaveShort((int16_t)(int)(g_blocks[(size_t)(row * g_cols + col)].target * 1024.f));
    SaveManager::EndChunk();
}

// @0x174770
void LoadFog(SaveManager::SaveBlock* block) {
    SaveManager::Reader r(block->GetChunk());
    int w = (int)r.u32(), h = (int)r.u32();
    int opened = 0;
    if (w == g_cols && h == g_rows) {
        for (int row = 0; row < g_rows; ++row)
            for (int col = 0; col < g_cols; ++col) {
                unsigned v = (uint16_t)r.s16();
                if (v > 0x3ff) continue;   // still covered
                ++opened;
                OpenTile(g_root, col, row, (float)v * (1.f / 1024.f));
            }
    } else {
        std::fprintf(stderr, "ERROR: Fog::LoadFog() Save fog size is %dx%d, but map size is %dx%d\n", w, h, g_cols,
                     g_rows);
    }
    block->EndChunkLoading(r);
    std::printf("Fog::LoadFog() Opened %d nodes out of %d, %d fog sprites are active\n", opened, g_cols * g_rows,
                g_spriteCount);
}

// @0x173c88 Fog::Update: the fading blocks lose 2 alpha a second down to their target; a block's
// sprite takes each corner's alpha from the four blocks meeting there and goes when all are 0.
void Update(float dt) {
    for (BlockInfo* b : g_fading) {
        b->alpha += dt * -2.f;
        if (b->alpha < b->target) b->alpha = b->target;
    }
    for (size_t i = 0; i < g_fading.size();) {
        BlockInfo* b = g_fading[i];
        if (!b->sprite) {
            g_fading[i] = g_fading.back();
            g_fading.pop_back();
            continue;
        }
        int c = b->col, r = b->row;
        auto max4 = [](float a, float b2, float c2, float d) {
            if (a < b2) a = b2;
            if (a < c2) a = c2;
            if (a < d) a = d;
            return a;
        };
        float nw = GetBlockAlpha(c - 1, r - 1), n = GetBlockAlpha(c, r - 1), ne = GetBlockAlpha(c + 1, r - 1);
        float w = GetBlockAlpha(c - 1, r), me = GetBlockAlpha(c, r), e = GetBlockAlpha(c + 1, r);
        float sw = GetBlockAlpha(c - 1, r + 1), s = GetBlockAlpha(c, r + 1), se = GetBlockAlpha(c + 1, r + 1);
        float tl = max4(nw, n, w, me), tr = max4(n, ne, me, e), bl = max4(w, me, sw, s), br = max4(me, e, s, se);
        b->sprite->alpha[2] = tl;
        b->sprite->alpha[3] = tr;
        b->sprite->alpha[0] = bl;
        b->sprite->alpha[1] = br;
        if (max4(tl, tr, bl, br) == 0.f) {
            --g_spriteCount;
            Render::RemoveSprite(b->sprite);
            b->sprite = nullptr;
            g_fading[i] = g_fading.back();
            g_fading.pop_back();
        } else {
            ++i;
        }
    }
}

// @0x1742c8
void ToggleFogRender() {
    g_render = !g_render;
    if (g_root) ToggleFogRender(g_root);
}

// @0x1748fc
void Open(float x, float y, float radius, float edge) {
    if (!(radius > 0.f)) return;
    float fx = x - (float)g_originX, fy = y - (float)g_originY;
    int c0 = (int)(fx - radius) / g_cell, r0 = (int)(fy - radius) / g_cell;
    int c1 = (int)(fx + radius) / g_cell, r1 = (int)(fy + radius) / g_cell;
    for (int c = c0; c <= c1; ++c)
        for (int r = r0; r <= r1; ++r) {
            if ((unsigned)c >= (unsigned)g_cols || (unsigned)r >= (unsigned)g_rows) continue;
            float half = (float)g_cell * 0.5f;
            float dy = (half + (float)(g_cell * r)) - fy;
            float dx = ((float)(g_cell * c) + half) - fx;
            float d2 = dy * dy + dx * dx, r2 = radius * radius;
            if (!(r2 > d2)) continue;
            float a = (d2 / r2 - edge) / (1.f - edge);
            OpenTile(g_root, c, r, a < 0.f ? 0.f : a);
        }
}

// @0x174aa4
void Open(int col, int row) { OpenTile(g_root, col, row, 0.f); }

// @0x174ac4 Fog::OpenZero: every block whose target is below 1 opens to it; the map's rim opens fully.
void OpenZero() {
    for (int col = 0; col < g_cols; ++col)
        for (int row = 0; row < g_rows; ++row) {
            float t = g_blocks[(size_t)(g_cols * row + col)].target;
            if (t < 1.f) OpenTile(g_root, col, row, t);
            if (col == 0 || row == 0 || g_cols - 1 == col || g_rows - 1 == row) OpenTile(g_root, col, row, 0.f);
        }
    std::printf("Fog::OpenZero() %d fog sprites are active\n", g_spriteCount);
}

}  // namespace Fog
