// Rule the Kingdom port: entry point (platform layer).
// Milestone 1: open a window and show a map, loaded and placed by the ported game code.
//
// Usage: rtk [--root <backup folder>] [--map N] [--seed N]
//            [--screenshot out.png --camera X Y ZOOM]   (render one frame to a PNG and exit)
#include <SDL3/SDL.h>
#include <OpenGL/gl3.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "engine/FileManager.h"
#include "engine/Render.h"
#include "engine/Resources.h"
#include "game/GameData.h"
#include "game/Map.h"

namespace {

struct Options {
    std::string root = "..";  // the backup folder that contains "Android files", "files", "_extract"
    unsigned map = 0;
    long seed = 1;            // GameState::playerSeed (per player on the original)
    std::string screenshot;
    float camX = 2520.f, camY = 300.f, zoom = 1.f;  // centre of the starting area (area 1)
    bool camSet = false;
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
        else if (a == "--camera") {
            o.camX = (float)std::atof(next());
            o.camY = (float)std::atof(next());
            o.zoom = (float)std::atof(next());
            o.camSet = true;
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
    if (!Map::Load(opt.map, opt.seed)) return 1;

    float camX = opt.camX, camY = opt.camY, zoom = opt.zoom;
    const float dpi = (float)fbw / 1280.f;  // keep one game pixel per point on Retina
    if (headless) {
        Render::SetCamera(camX, camY, zoom * dpi);
        Render::Frame();
        glFinish();
        bool ok = Render::SaveScreenshot(opt.screenshot.c_str());
        std::printf("screenshot %s: %s\n", opt.screenshot.c_str(), ok ? "ok" : "FAILED");
        return ok ? 0 : 1;
    }

    bool running = true, dragging = false;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_EVENT_QUIT: running = false; break;
                case SDL_EVENT_KEY_DOWN:
                    if (e.key.key == SDLK_ESCAPE) running = false;
                    if (e.key.key == SDLK_F12) Render::SaveScreenshot("rtk_screenshot.png");
                    break;
                case SDL_EVENT_MOUSE_BUTTON_DOWN: dragging = true; break;
                case SDL_EVENT_MOUSE_BUTTON_UP: dragging = false; break;
                case SDL_EVENT_MOUSE_MOTION:
                    if (dragging) { camX -= e.motion.xrel / zoom; camY -= e.motion.yrel / zoom; }
                    break;
                case SDL_EVENT_MOUSE_WHEEL:
                    zoom *= e.wheel.y > 0 ? 1.1f : (e.wheel.y < 0 ? 1.f / 1.1f : 1.f);
                    if (zoom < 0.25f) zoom = 0.25f;
                    if (zoom > 3.f) zoom = 3.f;
                    break;
                case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                    SDL_GetWindowSizeInPixels(win, &fbw, &fbh);
                    Render::Init(fbw, fbh);
                    break;
                default: break;
            }
        }
        Render::SetCamera(camX, camY, zoom * dpi);
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
