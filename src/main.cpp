// Rule the Kingdom port: entry point (platform layer).
// Milestone 1: open a window and show a map, loaded and placed by the ported game code.
//
// Usage: rtk [--root <backup folder>] [--map N] [--seed N]
//            [--screenshot out.png --camera X Y ZOOM]   (render one frame to a PNG and exit;
//                                    the view centred on world X,Y at ZOOM times the default zoom)
//            [--click X Y]... [--press X Y] [--drag X1 Y1 X2 Y2]...
//                                    (headless left-button input in pixels, applied in order)
#include <SDL3/SDL.h>
#include <OpenGL/gl3.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "engine/FileManager.h"
#include "engine/Render.h"
#include "engine/Resources.h"
#include "engine/Text.h"
#include "game/StringTable.h"
#include "game/GameState.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/HUD.h"
#include "game/GameData.h"
#include "game/Map.h"
#include "game/MapMovement.h"

namespace {

struct Options {
    std::string root = "..";  // the backup folder that contains "Android files", "files", "_extract"
    unsigned map = 0;
    long seed = 1;            // GameState::playerSeed (per player on the original)
    std::string screenshot;
    float camX = 2520.f, camY = 300.f, zoom = 1.f;  // centre of the starting area (area 1)
    enum InputType { kClick, kPress, kDrag };
    struct Input { InputType type; int x, y, x2, y2; };
    std::vector<Input> inputs;   // headless input, applied before the screenshot
};

Options Parse(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--root") o.root = next();
        else if (a == "--map") o.map = (unsigned)std::atoi(next());
        else if (a == "--seed") o.seed = std::atol(next());
        else if (a == "--screenshot") o.screenshot = next();
        else if (a == "--click" || a == "--press" || a == "--drag") {
            Options::Input in = {a == "--click" ? Options::kClick : a == "--press" ? Options::kPress : Options::kDrag,
                                 0, 0, 0, 0};
            in.x = std::atoi(next());
            in.y = std::atoi(next());
            if (in.type == Options::kDrag) {
                in.x2 = std::atoi(next());
                in.y2 = std::atoi(next());
            }
            o.inputs.push_back(in);
        }
        else if (a == "--camera") {
            o.camX = (float)std::atof(next());
            o.camY = (float)std::atof(next());
            o.zoom = (float)std::atof(next());
        }
    }
    return o;
}

bool RegisterSources(const std::string& root) {
    bool ok = FileManager::RegisterAndroidData(root + "/Android files/rule-the-kingdom-5-11-multi-android/assets/data");
    if (!ok) {
        std::fprintf(stderr, "Cannot read the 5.11 APK data under %s\n", root.c_str());
        return false;
    }
    FileManager::RegisterBlobFile(root + "/files/main.27.kbf");   // expansion packs, later wins
    FileManager::RegisterBlobFile(root + "/files/patch.33.kbf");
    FileManager::RegisterWin8Fallback(root + "/_extract/win5/Assets");  // PORT: fallback only
    std::printf("FileManager: %zu files registered\n", FileManager::EntryCount());
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    Options opt = Parse(argc, argv);
    if (!RegisterSources(opt.root)) return 1;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    const bool headless = !opt.screenshot.empty();
    SDL_Window* win = SDL_CreateWindow("Rule the Kingdom", 1280, 800,
                                       SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | (headless ? SDL_WINDOW_HIDDEN : 0));
    if (!win) { std::fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError()); return 1; }
    SDL_GLContext gl = SDL_GL_CreateContext(win);
    if (!gl) { std::fprintf(stderr, "GL context: %s\n", SDL_GetError()); return 1; }
    SDL_GL_SetSwapInterval(1);
    int fbw = 0, fbh = 0;
    SDL_GetWindowSizeInPixels(win, &fbw, &fbh);
    std::printf("GL %s, framebuffer %dx%d\n", (const char*)glGetString(GL_VERSION), fbw, fbh);

    if (!Render::Init(fbw, fbh) || !Resources::Init() || !GameData::Load()) return 1;
    Render::aspect = (float)fbw / (float)fbh;   // main_Loop_Func on repositionWindows / a resize
    // SDL_baseInit: the device screen decides the UI configuration; the port's "device screen" is
    // the window's framebuffer in pixels.
    GUI::SetScreenSize(fbw, fbh);
    Render::InitFonts();
    StringTable::SetLanguage("EN", 0);
    // UNVERIFIED: Game::LoadLanguageTable's file selection is not ported yet; English only.
    StringTable::Init("../resource/res_files/1Original/LocalizedStringsEN.xml", false, false);
    GUI::Init("fonts/ARICYRB.ttf", false);
    GameState::SetCurrentMapID(opt.map);
    if (!Map::Load(opt.map, opt.seed)) return 1;
    // The game's windows: their static FunctionalWindow objects are constructed in the binary's
    // static-initialiser order (by source file), then WindowQueue::InitWindows runs their Init.
    GameState::Reset();
    BattleBarWindow::Queue();
    BeltBarWindow::Queue();
    BottomCityWindow::Queue();
    CastleTopWindow::Queue();
    HUDWindow::Queue();
    PlayerTopWindow::Queue();
    TaskHolderWindow::Queue();
    TopCityWindow::Queue();
    // main_Loop_Init: the desktop window goes last (the bottom of the queue), then InitWindows.
    WindowManager::g_desktopWindow = new WindowManager::DesktopWindow();
    WindowManager::InitWindows();
    HUDWindow::Show();
    Render::SortRenderLayer(Render::kLayerGUI, 1);

    MapMovement::Init();
    Render::zoom = Render::GetDefaultZoom();
    // UNVERIFIED stand-in: Map::Load centres the view on the player entity (milestone 4).
    Render::CenterOn(opt.camX, opt.camY);
    Render::zoom *= opt.zoom;

    // One game tick, in the order of the game's Update (@0x186770) when no map load is running:
    // WindowManager::Update (delayed callbacks, none yet), the window queue, HUDWindow::Update,
    // GUI animations, the movement controllers, then Render::Update's camera step (the camera tween,
    // not ported, and ApplyViewportLimit); its drawing is Render::Frame.
    auto tick = [](float dt) {
        WindowManager::ProcessUpdate(dt);
        HUDWindow::Update(dt);
        GUI::UpdateAnimation(dt);
        MapMovement::Update(dt);
        Render::ApplyViewportLimit();
    };
    // Mouse input as in Game::main_Loop_Func. Coordinates are framebuffer pixels (the original scales
    // SDL's by a float factor). Not ported (UNVERIFIED): BuildingHovers, Spell/BuildingMovement,
    // BuildingPlacement, the world click (EntityManager, buildings, buying areas), long taps, the
    // touch/pinch path (Game::touchDown) and EditorConsole/Editor.
    int lastX = 0x96, lastY = 0x96;   // 0x60ef50: the previous pointer position
    bool firstMove = true;            // only the first motion event of a frame reaches ProcessMove
    auto mouseDown = [&](int x, int y, bool left) {
        WindowManager::SetMousePosition(x, y);
        GUI::OnMouseMove(x, y, true);
        if (left) {
            if (!Map::IsCameraMoving() && WindowManager::ProcessClick(x, y, true) != WindowManager::g_desktopWindow) {
                // UNVERIFIED: the tutorial arrow (BuildingHovers::ClickOnArrow / HideArrow) at step 0x80.
            } else {
                MapMovement::Click(x, y, true, false);
            }
        } else {
            MapMovement::Click(x, y, true, true);
        }
    };
    auto mouseUp = [&](int x, int y, bool left) {
        if (MapMovement::IsActive()) MapMovement::Click(x, y, false, false);
        else GUI::OnMouseClick(x, y, false);
        if (!left) {
            MapMovement::Click(x, y, false, true);
            return;
        }
        if (Map::IsCameraMoving()) {
            MapMovement::RemoveFocus();
            return;
        }
        if (!(MapMovement::HasFocus() && MapMovement::IsActive())) WindowManager::ProcessClick(x, y, false);
        MapMovement::RemoveFocus();
        // A click that reaches the desktop window goes to the world: milestones 3-4.
    };
    auto mouseMove = [&](int x, int y) {
        int dx = x - lastX, dy = y - lastY;
        lastX = x;
        lastY = y;
        if (std::abs(dx) + std::abs(dy) > 400) return;   // a jump (a new touch), not a move
        // UNVERIFIED: ShopWindow::IsVisible / BuildingHovers::IsHoverVisible also keep the drag.
        if (WindowManager::GetShownWindowCount() != 0) {
            MapMovement::RemoveFocus();
            MapMovement::StopDrag();
        }
        MapMovement::Move(x, y, dx, dy);
        if (!firstMove) {
            GUI::OnMouseMove(x, y, true);
        } else {
            WindowManager::ProcessMove(x, y);
            GUI::OnMouseMove(x, y, false);
            firstMove = false;
        }
    };
    // The mouse wheel zooms the map when no window is shown and no farm is visited (milestone 3);
    // otherwise WindowQueue::ProcessWheel (UNVERIFIED, not ported). The original reads the event's
    // first wheel field; the port uses the vertical wheel.
    auto wheel = [&](int amount) {
        if (WindowManager::GetShownWindowCount() != 0) return;
        if (amount > 0) Render::zoom = Render::zoom * 1.1f;
        if (amount < 0) Render::zoom = Render::zoom / 1.1f;
        Render::ApplyViewportLimit();
    };
    if (headless) {
        const float dt = 1.f / 30.f;
        for (int i = 0; i < 60; ++i) tick(dt);   // let the HUD slide in and count up
        for (const Options::Input& in : opt.inputs) {
            firstMove = true;
            mouseMove(in.x, in.y);
            mouseDown(in.x, in.y, true);
            for (int i = 0; i < 3; ++i) tick(dt);
            if (in.type == Options::kPress) break;
            int ex = in.x, ey = in.y;
            if (in.type == Options::kDrag) {
                const int steps = 10;   // one motion event per frame
                for (int k = 1; k <= steps; ++k) {
                    firstMove = true;
                    ex = in.x + (in.x2 - in.x) * k / steps;
                    ey = in.y + (in.y2 - in.y) * k / steps;
                    mouseMove(ex, ey);
                    tick(dt);
                }
            }
            mouseUp(ex, ey, true);
            for (int i = 0; i < 30; ++i) tick(dt);
        }
        Render::SortRenderLayer(Render::kLayerGUI, 1);
        Render::Frame();
        glFinish();
        bool ok = Render::SaveScreenshot(opt.screenshot.c_str());
        std::printf("screenshot %s: %s\n", opt.screenshot.c_str(), ok ? "ok" : "FAILED");
        return ok ? 0 : 1;
    }

    bool running = true;
    auto toPixels = [&](float wx, float wy, int& px, int& py) {
        int ww = 0, wh = 0;
        SDL_GetWindowSize(win, &ww, &wh);
        float k = ww > 0 ? (float)fbw / (float)ww : 1.f;
        px = (int)(wx * k);
        py = (int)(wy * k);
    };
    uint64_t lastTicks = SDL_GetTicks();
    while (running) {
        SDL_Event e;
        firstMove = true;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_EVENT_QUIT: running = false; break;
                case SDL_EVENT_KEY_DOWN:
                    if (e.key.key == SDLK_ESCAPE) running = false;
                    if (e.key.key == SDLK_F12) Render::SaveScreenshot("rtk_screenshot.png");
                    break;
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                case SDL_EVENT_MOUSE_BUTTON_UP: {
                    if (e.button.button != SDL_BUTTON_LEFT && e.button.button != SDL_BUTTON_RIGHT) break;
                    int x, y;
                    toPixels(e.button.x, e.button.y, x, y);
                    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) mouseDown(x, y, e.button.button == SDL_BUTTON_LEFT);
                    else mouseUp(x, y, e.button.button == SDL_BUTTON_LEFT);
                    break;
                }
                case SDL_EVENT_MOUSE_MOTION: {
                    int x, y;
                    toPixels(e.motion.x, e.motion.y, x, y);
                    mouseMove(x, y);
                    break;
                }
                case SDL_EVENT_MOUSE_WHEEL:
                    wheel(e.wheel.y > 0 ? 1 : (e.wheel.y < 0 ? -1 : 0));
                    break;
                case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                    // main_Loop_Func's SDL_WINDOWEVENT_RESIZED: new screen size, aspect, default zoom.
                    SDL_GetWindowSizeInPixels(win, &fbw, &fbh);
                    Render::Init(fbw, fbh);
                    Render::aspect = (float)fbw / (float)fbh;
                    Render::zoom = Render::GetDefaultZoom();
                    break;
                default: break;
            }
        }
        uint64_t now = SDL_GetTicks();
        float dt = (float)(now - lastTicks) / 1000.f;
        lastTicks = now;
        tick(dt > 0.1f ? 0.1f : dt);
        Render::Frame();
        SDL_GL_SwapWindow(win);
    }
    Map::Free();
    Render::Shutdown();
    SDL_GL_DestroyContext(gl);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
