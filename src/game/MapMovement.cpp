#include "game/MapMovement.h"

#include <cmath>
#include <cstdlib>

#include "engine/Render.h"
#include "game/GameState.h"
#include "game/Map.h"
#include "gui/GUI.h"
#include "hud/HUD.h"

namespace MapMovement {

namespace {
bool g_focus;            // +0x00
bool g_active;           // +0x01
unsigned g_moved;        // +0x04 pixels travelled since the press
float g_velX, g_velY;    // +0x08 +0x0c
float g_decel;           // +0x10
float g_remX, g_remY;    // +0x14 +0x18 movement not applied yet (under a pixel)

// Shift the view by a screen-pixel delta (the same code in Move and Update).
void Shift(float dx, float dy) {
    float k = 2.f / Render::zoom;
    Render::offsetX += dx / (float)Render::ScreenWidth() * k;
    Render::offsetY -= dy / (float)Render::ScreenHeight() * (k / Render::aspect);
}
}  // namespace

void Init() {
    g_decel = 6000.f;
    g_velX = g_velY = 0.f;
    g_moved = 0;
    g_remX = g_remY = 0.f;
    g_focus = false;
    g_active = false;
}

void Deinit() {}
void RemoveFocus() { g_focus = false; }
void StopDrag() { g_active = false; }
bool HasFocus() { return g_focus; }
bool IsActive() { return g_active; }

bool Move(int x, int y, int dx, int dy) {
    (void)x; (void)y;
    if (!g_focus) return false;
    g_moved += (unsigned)std::abs(dx) + (unsigned)std::abs(dy);
    if (g_moved <= 0x14) return true;
    g_active = true;
    Shift((float)dx, (float)dy);
    Map::InterruptCamera();
    Render::ApplyViewportLimit();
    if (GameState::TutorialStep() == 0x80) {
        // UNVERIFIED: BuildingHovers arrow auto-hide (step 0x60efd0 != 0x83, !IsCityTutorial,
        // CanAutoHideArrow -> HideArrow) is not ported.
    }
    return true;
}

bool Click(int x, int y, bool pressed, bool rightButton) {
    (void)x; (void)y;
    if (rightButton) {
        g_focus = pressed;
        return false;
    }
    if (pressed) {
        g_velY = g_velX = 0.f;
        g_moved = 0;
        g_focus = true;
        g_active = false;
        return true;
    }
    if (!g_focus || !g_active) return false;
    float vx = GUI::GetMouseSpeed(true);
    float vy = GUI::GetMouseSpeed(false);
    if (vx < -2000.f) vx = -2000.f;
    else if (vx > 2000.f) vx = 2000.f;
    if (vy < -2000.f) vy = -2000.f;
    else if (vy > 2000.f) vy = 2000.f;
    g_velX = vx;
    g_velY = vy;
    return true;
}

// Each velocity component moves toward 0 by its share of decel*dt, stopping at 0.
void Update(float dt) {
    dt = dt / (float)HUDWindow::GetDeltaTimeMultiplier();
    float vx = g_velX, vy = g_velY;
    float speed = std::sqrt(vy * vy + vx * vx);
    float step = dt * g_decel;
    if (vx != 0.f) {
        float d = step * std::fabs(vx / speed);
        if (vx < 0.f) { vx += d; if (vx > 0.f) vx = 0.f; }
        else if (vx > 0.f) { vx -= d; if (vx < 0.f) vx = 0.f; }
    }
    g_velX = vx;
    if (vy != 0.f) {
        float d = std::fabs(vy / speed) * step;
        if (vy < 0.f) { vy += d; if (vy > 0.f) vy = 0.f; }
        else if (vy > 0.f) { vy -= d; if (vy < 0.f) vy = 0.f; }
    }
    float rx = g_remX + dt * vx;
    g_velY = vy;
    float ry = g_remY + dt * vy;
    g_remX = rx;
    g_remY = ry;
    int ix = (int)rx, iy = (int)ry;
    if (ix != 0 || iy != 0) {
        Shift((float)ix, (float)iy);
        g_remX = rx - (float)ix;
        g_remY = ry - (float)iy;
    }
}

}  // namespace MapMovement
