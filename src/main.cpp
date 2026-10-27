// Rule the Kingdom port: entry point (platform layer).
// Milestone 1: open a window and show a map, loaded and placed by the ported game code.
//
// Usage: rtk [--root <backup folder>] [--map N] [--seed N] [--second-tutorial N]
//            [--screenshot out.png --camera X Y ZOOM]   (render one frame to a PNG and exit;
//                                    the view centred on world X,Y at ZOOM times the default zoom)
//            [--click X Y]... [--press X Y] [--drag X1 Y1 X2 Y2]... [--type TEXT]... [--key SCANCODE]...
//                                    (headless input, applied in order: left-button input in
//                                    pixels, typed UTF-8 text, a key press by SDL scancode)
//            [--show city_rename]            (PORT test aid: open a window not reachable yet)
//            [--spawn ID X Y]... [--walk X Y] [--frames N]
//                                    (PORT test aid: spawn entities at tiles after the map loads;
//                                    the last one walks to a tile; N more frames before the shot)
//            [--dump-entities]               (PORT test aid: print every entity's state at the end)
//            [--storage DIR]                 (the save folder; default: the user's application
//                                    support folder for the game)
//            [--save]                        (PORT test aid: save (Map::SafeSave) before the shot)
//            [--quit]                        (PORT test aid: a headless run also runs the exit,
//                                    main_Exit_Func, which saves)
//            [--lang=xx_XX]                  (the original's switch: en_EN ru_RU de_DE lv_LV es_ES
//                                    it_IT fr_FR ko_KR ja_JA)
//            [--system-lang N]               (PORT test aid: the system's language id, 0..8)
//            [--empty_strings]               (the original's switch: empty texts show their key)
//            [--fps N] [--fullscreen | --windowed]   (PORT: the display options for this run, see
//                                    DisplayOptions below)
#include <SDL3/SDL.h>
#include <OpenGL/gl3.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "engine/ErrorReporter.h"
#include "engine/FileManager.h"
#include "engine/IntroVideo.h"
#include "engine/Render.h"
#include "engine/Resources.h"
#include "engine/SystemFuncs.h"
#include "engine/SystemNotifications.h"
#include "game/Tavern.h"
#include "game/Notifications.h"
#include "game/Tags.h"
#include "game/Particles.h"
#include "game/AI.h"
#include "engine/Text.h"
#include "engine/TextInput.h"
#include "game/StringTable.h"
#include "game/Chests.h"
#include "game/PresentsContainer.h"
#include "game/Collections.h"
#include "game/Contracts.h"
#include "engine/Timer.h"
#include "game/Combat.h"
#include "game/Projectile.h"
#include "game/Entity.h"
#include "game/EntityData.h"
#include "game/AIState.h"
#include "game/BuildingHovers.h"
#include "game/BuildingMovement.h"
#include "game/BuildingPlacement.h"
#include "game/SpellMovement.h"
#include "game/Animation.h"
#include "game/EntityData.h"
#include "game/EntityManager.h"
#include "game/GameState.h"
#include "game/LoadSampling.h"
#include "game/PvP.h"
#include "game/PvPServer.h"
#include "game/Items.h"
#include "game/Locations.h"
#include "game/Spells.h"
#include "game/Setting.h"
#include "game/Professions.h"
#include "game/Sets.h"
#include "gui/GUI.h"
#include "engine/SoundsManager.h"
#include "gui/TextStyleManager.h"
#include "gui/WindowManager.h"
#include "hud/HUD.h"
#include "windows/Windows.h"
#include "game/Game.h"
#include "game/GameData.h"
#include "game/MetaExpression.h"
#include "game/MetaData.h"
#include "game/Map.h"
#include "game/Override.h"
#include "game/MapMovement.h"
#include "game/SaveManager.h"
#include "game/SoldierSlots.h"
#include "game/Tasks.h"
#include "game/Squad.h"

namespace Game { bool main_Loop_Func(); }   // below

namespace {

struct Options {
    std::string root;         // PORT test aid: a folder with the APK (or its "Android files" folder)
                              // (empty: FindGameApk looks beside the game)
    std::string apk;          // the 5.11 APK in use (FindGameApk)
    unsigned map = 0;
    bool mapGiven = false;    // PORT test aid: without a save, start on that map past the tutorial
    long seed = 1;            // GameState::playerSeed of the test city (no save)
    std::string storage;      // the save folder (FileManager::GetStoragePath)
    bool save = false;        // PORT test aid: save at the end of a headless run
    std::string screenshot;
    float camX = 0.f, camY = 0.f, zoom = 1.f;   // PORT test aid: a view other than Map::Load's
    bool camera = false;
    int secondTutorial = -1;     // PORT test aid: GameState::secondTutorial for the test city (0x100: the
                                 // finished city tutorial, which hero taps in the city need)
    enum InputType { kClick, kPress, kDrag, kType, kKey, kMove };
    struct Input { InputType type; int x, y, x2, y2; std::string text; };
    std::vector<Input> inputs;   // headless input, applied before the screenshot
    std::string show;            // a window to open after the HUD (test aid)
    struct Spawn { int id, x, y; };
    std::vector<Spawn> spawns;   // PORT test aid: entities to spawn (no saves/new game yet)
    int walkX = -1, walkY = -1;  // PORT test aid: the last spawned entity walks here
    int frames = 0;              // extra headless frames before the screenshot
    bool dumpEntities = false;   // PORT test aid
    bool quit = false;           // PORT test aid: a headless run ends through main_Exit_Func
    int lang = -1;               // "--lang=": the language id (SDL_baseInit)
    int systemLang = -1;         // PORT test aid: the system's language id
    int fps = -1;                // --fps: the frame-rate cap for this run (0: the display's rate)
    int fullscreen = -1;         // --fullscreen 1 / --windowed 0 for this run
};

Options Parse(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--root") o.root = next();
        else if (a == "--map") { o.map = (unsigned)std::atoi(next()); o.mapGiven = true; }
        else if (a == "--seed") o.seed = std::atol(next());
        else if (a == "--second-tutorial") o.secondTutorial = std::atoi(next());
        else if (a == "--screenshot") o.screenshot = next();
        else if (a == "--click" || a == "--press" || a == "--drag") {
            Options::Input in = {a == "--click" ? Options::kClick : a == "--press" ? Options::kPress : Options::kDrag,
                                 0, 0, 0, 0, {}};
            in.x = std::atoi(next());
            in.y = std::atoi(next());
            if (in.type == Options::kDrag) {
                in.x2 = std::atoi(next());
                in.y2 = std::atoi(next());
            }
            o.inputs.push_back(in);
        }
        else if (a == "--move") {   // PORT test aid: the pointer glides there, no button held
            Options::Input in = {Options::kMove, 0, 0, 0, 0, {}};
            in.x = std::atoi(next());
            in.y = std::atoi(next());
            o.inputs.push_back(in);
        }
        else if (a == "--show") o.show = next();
        else if (a == "--spawn") {
            Options::Spawn sp;
            sp.id = std::atoi(next());
            sp.x = std::atoi(next());
            sp.y = std::atoi(next());
            o.spawns.push_back(sp);
        }
        else if (a == "--walk") {
            o.walkX = std::atoi(next());
            o.walkY = std::atoi(next());
        }
        else if (a == "--frames") o.frames = std::atoi(next());
        else if (a == "--dump-entities") o.dumpEntities = true;
        else if (a == "--storage") o.storage = next();
        else if (a == "--save") o.save = true;
        else if (a == "--quit") o.quit = true;
        else if (a == "--system-lang") o.systemLang = std::atoi(next());
        else if (a == "--fps") o.fps = std::atoi(next());
        else if (a == "--fullscreen") o.fullscreen = 1;
        else if (a == "--windowed") o.fullscreen = 0;
        else if (a == "--empty_strings") StringTable::SetEmptyNameReplacement();   // (SDL_baseInit)
        else if (a.rfind("--lang=", 0) == 0) {
            // SDL_baseInit (0x18b1e0): the language by locale name
            static const char* const kLocales[9] = {"en_EN", "ru_RU", "de_DE", "lv_LV", "es_ES",
                                                    "it_IT", "fr_FR", "ko_KR", "ja_JA"};
            for (int k = 0; k < 9; ++k)
                if (a.substr(7) == kLocales[k]) o.lang = k;
        }
        else if (a == "--type") {
            o.inputs.push_back({Options::kType, 0, 0, 0, 0, next()});
        }
        else if (a == "--key") {
            o.inputs.push_back({Options::kKey, std::atoi(next()), 0, 0, 0, {}});
        }
        else if (a == "--camera") {
            o.camX = (float)std::atof(next());
            o.camY = (float)std::atof(next());
            o.zoom = (float)std::atof(next());
            o.camera = true;
        }
    }
    return o;
}

// @0x171aa8 FileManager::Init: the main and patch packs (CheckExpansion), then the packs the server
// listed (the "active_packs" setting: online, dormant). PORT: the files' sources are the player's
// 5.11 APK (read in place), then the two packs the server once sent (main.27 and patch.33, later
// wins), then the files only the Windows 8 build had (win8_supplement.kbf, made by
// tools/make_supplement_pack.py); the three packs ship with the game (FindGameData).
bool RegisterSources(const std::string& apk, const std::string& data) {
    if (!FileManager::RegisterApk(apk)) {
        std::fprintf(stderr, "Cannot read the game data in %s\n", apk.c_str());
        return false;
    }
    if (!FileManager::RegisterBlobFile(data + "main.27.kbf", true, 27) ||
        !FileManager::RegisterBlobFile(data + "patch.33.kbf", false, 33) ||
        !FileManager::RegisterBlobFile(data + "win8_supplement.kbf", false, 0)) {
        std::fprintf(stderr, "Cannot read the game's packs in %s\n", data.c_str());
        return false;
    }
    std::printf("FileManager: %zu files registered (%s)\n", FileManager::EntryCount(), apk.c_str());
    return true;
}

bool IsFile(const std::string& path) {
    SDL_PathInfo info;
    return SDL_GetPathInfo(path.c_str(), &info) && info.type == SDL_PATHTYPE_FILE;
}

// PORT: the folder of the packs that ship with the game: "data" beside the program (the .app's
// Contents/Resources, or the Linux / Windows program's folder), else the source tree's gamedata
// folder (a build in port/build).
std::string FindGameData() {
    std::vector<std::string> candidates;
    if (const char* base = SDL_GetBasePath()) {
        candidates.push_back(std::string(base) + "data/");
        candidates.push_back(std::string(base) + "../gamedata/");
    }
    candidates.push_back(RTK_SOURCE_DIR "/gamedata/");
    for (const std::string& dir : candidates)
        if (IsFile(dir + "main.27.kbf")) return dir;
    return "";
}

// PORT: the folders an APK is looked for in: the program's folder and the ones above it (the folder
// that holds "Rule the Kingdom.app"), the working folder, and each one's "Android files".
std::vector<std::string> ApkFolders(const std::string& root) {
    std::vector<std::string> dirs;
    if (!root.empty()) dirs = {root};
    if (const char* base = SDL_GetBasePath()) {
        std::string dir = base;
        for (int i = 0; i < 6; ++i) {
            while (dir.size() > 1 && (dir.back() == '/' || dir.back() == '\\')) dir.pop_back();
            dirs.push_back(dir);
            const size_t slash = dir.find_last_of("/\\");
            if (slash == std::string::npos || slash == 0) break;
            dir.resize(slash);
        }
    }
    if (root.empty()) dirs.push_back(".");
    std::vector<std::string> all;
    for (const std::string& d : dirs) {
        all.push_back(d);
        all.push_back(d + "/Android files");
    }
    return all;
}

// The newest Rule the Kingdom APK in `dirs` (the largest data index), or "".
std::string FindApkIn(const std::vector<std::string>& dirs) {
    std::string best;
    uint32_t bestSize = 0;
    for (const std::string& dir : dirs) {
        int count = 0;
        char** names = SDL_GlobDirectory(dir.c_str(), "*.apk", SDL_GLOB_CASEINSENSITIVE, &count);
        for (int i = 0; names && i < count; ++i) {
            const std::string path = dir + "/" + names[i];
            const uint32_t size = FileManager::GameApkIndexSize(path);
            if (size > bestSize) { bestSize = size; best = path; }
        }
        SDL_free(names);
    }
    return best;
}

std::string PickGameApk(const char* message) {
    if (!SDL_Init(SDL_INIT_VIDEO)) return "";
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Rule the Kingdom", message, nullptr);
    struct Pick { std::string path; bool done = false; } pick;
    static const SDL_DialogFileFilter kFilter[] = {{"Android app (APK)", "apk"}};
    SDL_ShowOpenFileDialog(
        [](void* data, const char* const* list, int) {
            Pick* p = (Pick*)data;
            if (list && list[0]) p->path = list[0];
            p->done = true;
        },
        &pick, nullptr, kFilter, 1, nullptr, false);
    while (!pick.done) {
        SDL_Event e;
        SDL_WaitEventTimeout(&e, 100);
    }
    return pick.path;
}

// PORT: the player's Rule the Kingdom 5.11 APK, the one file the game needs from the player. With
// --root DIR: the newest APK in DIR or DIR/Android files (the test runs' backup folder). Else: the
// one remembered in the storage folder (game_apk.txt), else the newest APK beside the game (the
// folder that holds the app or the program); else the player picks it (not in a headless run), and
// it is remembered.
std::string FindGameApk(const std::string& root, const std::string& storage, bool ask) {
    if (!root.empty()) return FindApkIn(ApkFolders(root));
    const std::string memo = storage + "game_apk.txt";
    if (FILE* f = std::fopen(memo.c_str(), "rb")) {
        char buf[4096];
        size_t n = std::fread(buf, 1, sizeof buf, f);
        std::fclose(f);
        std::string path(buf, n);
        while (!path.empty() && (path.back() == '\n' || path.back() == '\r')) path.pop_back();
        if (FileManager::GameApkIndexSize(path)) return path;
    }
    std::string apk = FindApkIn(ApkFolders(""));
    if (!apk.empty() || !ask) return apk;
    const char* message = "Put your Rule the Kingdom 5.11 APK (the Android game file) in the same folder as "
                          "the game, or choose it now.";
    for (;;) {
        apk = PickGameApk(message);
        if (apk.empty()) return "";
        if (FileManager::GameApkIndexSize(apk)) {
            if (FILE* f = std::fopen(memo.c_str(), "wb")) {
                std::fputs(apk.c_str(), f);
                std::fclose(f);
            }
            return apk;
        }
        message = "That file is not the Rule the Kingdom APK. Choose the game's .apk file (version 5.11).";
    }
}

// PORT: Java's ShowVideoSplash plays the APK's assets/LOGO.mp4; the desktop player needs a file, so
// it is copied from the APK into the storage folder once.
std::string IntroVideoPath(const std::string& apk) {
    const std::string path = FileManager::GetStoragePath() + "LOGO.mp4";
    if (IsFile(path)) return path;
    std::vector<uint8_t> film;
    if (!FileManager::ExtractFromApk(apk, "assets/LOGO.mp4", film)) return "";
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return "";
    const bool ok = std::fwrite(film.data(), 1, film.size(), f) == film.size();
    std::fclose(f);
    if (!ok) std::remove(path.c_str());
    return ok ? path : "";
}

Options opt;
SDL_Window* win = nullptr;
SDL_GLContext gl = nullptr;
int fbw = 0, fbh = 0;      // SCREEN_WIDTH / SCREEN_HEIGHT: the framebuffer in pixels
bool headless = false;

// main_Loop_Func's state. Coordinates are framebuffer pixels (the original scales SDL's by a float
// factor).
int lastX = 0x96, lastY = 0x96;   // 0x60ef50: the previous pointer position
bool held = false;                // 0x6122ec: a button is down
float longTapTime = 0.f;          // 0x6122f0: seconds since the press or release
float wheelSum = 0.f;             // PORT: the trackpad's scroll fractions
bool longTapStarted = false;      // 0x6122f4: GUI::OnLongTapStart ran for this press
// Game::mActionToDo / mActionValue: a request to open a screen, set by CommonSetAction (Java's
// mcAddAction, coupons, server flags, blog commands). Nothing sets it offline, so the switch in
// AfterEvents is dormant (DECIDED, user 2026-10-19).
int mActionToDo = 0;              // 0x6124a8
int mActionValue = 0;             // 0x6124ac
float edgeX = 0.f, edgeY = 0.f;   // 0x612314 0x612318: edge-scroll drag not yet passed on
bool firstMove = true;            // only the first motion event of a frame reaches ProcessMove
uint64_t lastTicks = 0;           // Game::time: when the previous frame started
// PORT debug aid: RTK_TRACE_INPUT=1 prints what each button press and release reaches.
bool traceInput = false;

const char* QueueName(WindowManager::WindowQueue* q) {
    if (!q) return "none";
    if (q == WindowManager::g_desktopWindow) return "desktop(world)";
    if (q == CastleTopWindow::Queue()) return "CastleTop(right buttons)";
    if (q == SettingsWindow::Queue()) return "Settings";
    if (q == BuildingHovers::Queue()) return "BuildingHovers";
    if (q == HUDWindow::Queue()) return "HUD";
    return "other window";
}

// @0x186770 Update: one game tick, in the original's order when no map load is running:
// GameState::Update, WindowManager::Update (the windows waiting to be shown), the window queue, HUDWindow::Update,
// GUI animations, the movement controllers, BuildingHovers, then when the game is not paused the
// entities (Entity::SetCurrentTime, EntityManager::Update) and Map::Update, then Render::Update's
// camera step (the CenterOn glide and ApplyViewportLimit), UpdateTagState, UpdatePaymentItems and
// DestroyPendingWindows; its drawing is Render::Frame.
void Tick(float dt) {
    ErrorReporter::Update(dt);
    PvPServer::Update();   // PORT: the offline PvP server's replies (the original's network callbacks)
    if (!GameState::IsPaused()) GameState::UpdateHPRegen(dt);
    GameState::Update(dt);
    GameState::UpdateOfflineBuildings(dt);
    GameState::UpdateOfflineGoblins(dt);
    WindowManager::Update(dt);
    WindowManager::ProcessUpdate(dt);
    HUDWindow::Update(dt);
    GUI::UpdateAnimation(dt);
    BuildingMovement::Update(dt);
    BuildingPlacement::Update(dt);
    MapMovement::Update(dt);
    TutorialSender::Update(dt);
    if (!GameState::IsTutorial()) PresentsContainer::Update(dt);
    BuildingHovers::Update(dt, false);
    if (!GameState::IsPaused()) {
        Tasks::Update(false);
        TaskInfoWindow::Update(dt);   // (besides the window queue's own call, as the original)
    }
    if (!GameState::IsPaused()) {
        Entity::SetCurrentTime((uint32_t)Timer::GetGlobalTime());
        EntityManager::Update(dt);
        // The running fight; a finished one is deleted here, and 10 s later the map's music returns
        // if the fight music still plays (0x612924, a double).
        static double musicReturn = 0.0;
        if (GameState::IsCombatActive()) {
            if (!GameState::GetActiveCombat()->IsCompleted()) {
                GameState::GetActiveCombat()->Update(dt);
            } else {
                GameState::EndActiveCombat(false);
                musicReturn = 10.0;
            }
        }
        if (musicReturn > 0.0) musicReturn -= dt;
        if (musicReturn < 0.0) {
            musicReturn = 0.0;
            if (SoundsManager::IsMusicPlaying("music_fight") && !GameState::IsCombatActive()) {
                if (Map::GetTileset() == 0) SoundsManager::PlayMusic("music_battle_map", true);
                else if (Map::GetTileset() == 1) SoundsManager::PlayMusic("music_winter", true);
                else if (Map::GetTileset() == 2) SoundsManager::PlayMusic("desert/sand_theme", true);
            }
        }
        Map::Update(dt);
    }
    for (float& t : GameState::dialogueTimer)
        if (t < 20.f) t += dt;
    // (With Game::renderBackground 0x6123cc set, Render::Update draws the loading background under the
    // frame (UpdateProgress at renderBackgroundZ 0.2, then HideProgress); only Game::showBackground
    // sets it, which nothing calls in 5.11.)
    Render::UpdateCamera(dt);
    Render::ApplyViewportLimit();
    Render::UpdateAnimatedSprites(dt);   // (Render::Update, before its drawing)
    if (!GameState::IsPaused()) Particles::Update(dt);   // @0x186c68 (after Render::Update)
    // (EditorConsole::Update and, in its debug mode, Editor::Update: the admin mode, dead in 5.11.)
    UpdateTagState();
    GameState::UpdatePaymentItems(dt);
    WindowManager::DestroyPendingWindows();
}

// Mouse input as in Game::main_Loop_Func. The two-finger path (Game::touchDown, the fingers list):
// its pinch is the trackpad's (Pinch below); its two-finger pan is the trackpad's scroll, which SDL
// sends as the wheel (it zooms, as the mouse wheel). Left out:
// EditorConsole::IsVisible / Click / Move / OnScroll* and Editor::OnMouseOver (the admin mode, dead in 5.11: never shown).
void MouseDown(int x, int y, bool left) {
    longTapTime = 0.f;
    lastX = x;
    lastY = y;
    held = true;
    longTapStarted = false;
    WindowManager::SetMousePosition(x, y);
    GUI::OnMouseMove(x, y, true);
    if (left) {
        WindowManager::WindowQueue* down = Map::IsCameraMoving() ? nullptr : WindowManager::ProcessClick(x, y, true);
        if (traceInput)
            std::printf("TRACE down %d,%d -> %s (mapMove active %d focus %d)\n", x, y, QueueName(down),
                        MapMovement::IsActive(), MapMovement::HasFocus());
        if (down && down != WindowManager::g_desktopWindow) {
            // A tap on a window with none shown (the HUD's) after the tutorials: a help arrow that
            // may auto-hide goes, unless the tap was on it.
            if (WindowManager::GetShownWindowCount() == 0 && GameState::TutorialStep() == 0x80 &&
                !BuildingHovers::ClickOnArrow(x, y, true) && GameState::secondTutorial != 0x83 &&
                !GameState::IsCityTutorial() && BuildingHovers::CanAutoHideArrow())
                BuildingHovers::HideArrow();
        } else if (!(SpellMovement::Activated() && SpellMovement::Click(x, y, true)) &&
                   !(BuildingMovement::Activated() && BuildingMovement::Click(x, y, true, false)) &&
                   !(BuildingPlacement::Activated() && BuildingPlacement::Click(x, y, true, false))) {
            MapMovement::Click(x, y, true, false);
        }
    } else {
        MapMovement::Click(x, y, true, true);
    }
}

void MouseUp(int x, int y, bool left) {
    if (MapMovement::IsActive()) MapMovement::Click(x, y, false, false);
    else GUI::OnMouseClick(x, y, false);
    held = false;
    if (longTapStarted) GUI::OnLongTapAbort();
    longTapTime = 0.f;
    longTapStarted = false;
    if (!left) {
        MapMovement::Click(x, y, false, true);
        return;
    }
    if (BuildingPlacement::IsInDragProcess()) {
        BuildingPlacement::Click(x, y, false, false);
        return;
    }
    if (Map::IsCameraMoving()) {
        MapMovement::RemoveFocus();
        return;
    }
    WindowManager::WindowQueue* hit = WindowManager::g_desktopWindow;
    if (!(MapMovement::HasFocus() && MapMovement::IsActive())) hit = WindowManager::ProcessClick(x, y, false);
    if (traceInput)
        std::printf("TRACE up %d,%d -> %s (mapMove active %d focus %d)\n", x, y, QueueName(hit),
                    MapMovement::IsActive(), MapMovement::HasFocus());
    MapMovement::RemoveFocus();
    if (hit != WindowManager::g_desktopWindow) return;
    if (SpellMovement::Activated() && SpellMovement::Click(x, y, false)) return;
    if (BuildingMovement::Activated() && BuildingMovement::Click(x, y, false, MapMovement::IsActive())) return;
    if (BuildingPlacement::Activated() && BuildingPlacement::Click(x, y, false, MapMovement::IsActive())) return;
    // A for-sale sign opens the land window (not during a drag or a placement).
    if (!MapMovement::IsActive() && !SpellMovement::Activated() && !BuildingMovement::Activated() &&
        !BuildingPlacement::Activated() && Map::ClickToBuyArea(x, y))
        return;
    // The rest goes to the world: unless the map was dragged, to the player's AI (a character
    // or a tile). (The original then prints the building and decoration there: debug output.)
    int wx = x, wy = y;
    Map::MouseCoordinatesToWorld(wx, wy);
    std::printf("clicked tile at: (x: %d, y: %d) world pos\n", wx, wy);
    if (!MapMovement::IsActive()) EntityManager::OnClick(wx, wy);
}

void MouseMove(int x, int y) {
    int dx = x - lastX, dy = y - lastY;
    lastX = x;
    lastY = y;
    if (std::abs(dx) + std::abs(dy) > 400) return;   // a jump (a new touch), not a move
    if (WindowManager::GetShownWindowCount() != 0 && !ShopWindow::IsVisible() && !BuildingHovers::IsHoverVisible()) {
        MapMovement::RemoveFocus();
        MapMovement::StopDrag();
    }
    MapMovement::Move(x, y, dx, dy);
    if (SpellMovement::Activated() && SpellMovement::Move(x, y, dx, dy)) return;
    if (BuildingMovement::Activated() && BuildingMovement::Move(x, y, dx, dy)) return;
    if (BuildingPlacement::Activated() && BuildingPlacement::Move(x, y, dx, dy)) return;
    if (!firstMove) {
        GUI::OnMouseMove(x, y, true);
    } else {
        WindowManager::ProcessMove(x, y);
        GUI::OnMouseMove(x, y, false);
        firstMove = false;
    }
}

// main_Loop_Func after the events: a preview dragged within 100 px of a screen edge scrolls the
// view 300 world units a second that way and is moved along (whole pixels; the rest is kept).
void EdgeScroll(double frameDt) {
    if (!BuildingPlacement::IsBuildingDragged() || !held) return;
    const float viewW = Render::GetScreenViewWidth(), viewH = Render::GetScreenViewHeight();
    const float speed = (float)(frameDt * 300.0);
    const int W = Render::ScreenWidth(), H = Render::ScreenHeight();
    if (lastX + 99 >= W) {
        Render::offsetX -= speed;
        edgeX = ((float)W * speed) / viewW + edgeX;
    }
    if (lastX <= 99) {
        Render::offsetX += speed;
        edgeX = edgeX - ((float)W * speed) / viewW;
    }
    if (lastY + 99 >= H) {
        Render::offsetY += speed;
        edgeY = ((float)H * speed) / viewH + edgeY;
    }
    if (lastY <= 99) {
        Render::offsetY -= speed;
        edgeY = edgeY - ((float)H * speed) / viewH;
    }
    if (std::fabs(edgeX) > 1.f) BuildingPlacement::Move(lastX, lastY, (int)edgeX, std::fabs(edgeY) > 1.f ? (int)edgeY : 0);
    else if (std::fabs(edgeY) > 1.f) BuildingPlacement::Move(lastX, lastY, 0, (int)edgeY);
    else return;
    if (edgeX > 1.f) edgeX -= (float)(int)edgeX;
    if (edgeX < -1.f) edgeX += (float)(int)(-edgeX);
    if (edgeY > 1.f) edgeY -= (float)(int)edgeY;
    if (edgeY < -1.f) edgeY += (float)(int)(-edgeY);
}

// The mouse wheel (not on a farm): it zooms the map by 1.1 when no window is shown, else it goes
// to the windows. The original reads the event's first wheel field; the port uses the vertical wheel.
void Wheel(int amount) {
    if (Map::GetCurrentFarm()) return;
    if (WindowManager::GetShownWindowCount() != 0) {
        WindowManager::ProcessWheel(amount > 0);
        return;
    }
    if (amount > 0) Render::zoom = Render::zoom * 1.1f;
    if (amount < 0) Render::zoom = Render::zoom / 1.1f;
    Render::ApplyViewportLimit();
}

// The two-finger pinch: with no farm and no window shown (and no LoadingInternetWindow: online) the
// zoom changes by the fingers' distance, zoom / (last distance / distance), with no viewport limit.
// PORT: the trackpad's pinch (SDL's scale is distance / last distance).
void Pinch(float scale) {
    if (scale <= 0.f || Map::GetCurrentFarm() || WindowManager::GetShownWindowCount() != 0) return;
    Render::zoom = Render::zoom / (1.f / scale);
}

// Key presses (SDL_KEYDOWN in main_Loop_Func, by scancode). While a text field is edited they go
// to TextInput; otherwise volume up/down (SoundsManager::SetVolume_Master(+-1): Android's media
// volume through Java; PORT: the Mac's volume keys work on the system volume, so not handled), the
// menu key (and V) opens the settings and the back key (and Y) goes to the windows' Back.
void KeyDown(int scancode) {
    if (TextInput::enabled) {
        if (scancode == SDL_SCANCODE_BACKSPACE) TextInput::OnBackspace();
        else if (scancode == SDL_SCANCODE_RETURN || scancode == SDL_SCANCODE_KP_ENTER) TextInput::OnReturn();
        // Characters: the original converts the keysym with a US layout (SDLKeyToNative 0x187158).
        // PORT: they come from SDL text events instead (TextInputUtf8 below), so every keyboard
        // layout and input method works, as the Android text dialog did.
        return;
    }
    if (scancode == SDL_SCANCODE_VOLUMEUP || scancode == SDL_SCANCODE_VOLUMEDOWN) return;
    if (scancode == SDL_SCANCODE_MENU || scancode == SDL_SCANCODE_V) {
        if (GameState::SecondTutorialStep() == 0x100 && WindowManager::GetShownWindowCount() == 0 &&
            !BuildingHovers::IsWorldDialogVisible() && !PlayerTopWindow::IsPlayerDialogVisible() &&
            !SettingsWindow::IsVisible())
            SettingsWindow::Show();
        return;
    }
    if (scancode == SDL_SCANCODE_AC_BACK || scancode == SDL_SCANCODE_Y) {
        // Not taken by a window (and no death animation or camera move): an open
        // popup closes, else the exit question.
        if (WindowManager::ProcessBack() || Entity::IsDeathAnimationRunning() || Map::IsCameraMoving()) return;
        if (!GUI::IsAnyAnimationActive() && !GUI::CanInteractWith(nullptr) && PopupWindow::IsVisible()) {
            PopupWindow::Hide();
            return;
        }
        if (GUI::IsAnyAnimationActive()) return;
        if (RaidCompleteWindow::IsVisible() || CampaignCompleteWindow::IsVisible() ||
            CampaignPerfectedWindow::IsVisible() || BossOutroWindow::IsVisible())
            return;
        PopupWindow::Show(StringTable::GetString("MOBILE_EXIT_CONFIRM"), ExitWithSendSave, PopupWindow::Hide,
                          nullptr);
    }
}

void TextInputUtf8(const char* utf8) {
    if (!TextInput::enabled) return;
    const unsigned char* p = (const unsigned char*)utf8;
    while (*p) {
        char32_t c = *p++;
        int extra = c >= 0xf0 ? 3 : c >= 0xe0 ? 2 : c >= 0xc0 ? 1 : 0;
        if (extra) c &= 0x3f >> extra;
        for (; extra > 0 && (*p & 0xc0) == 0x80; --extra) c = (c << 6) | (*p++ & 0x3f);
        TextInput::OnCharacter(c);
    }
}

// main_Loop_Func between the events and the game's Update (frameDt: the frame's real time).
void AfterEvents(double frameDt) {
    EdgeScroll(frameDt);
    // (Render::FreeMemory when the textures pass 40 / 50 MB: the port keeps what it loads.
    // Bonus::Enqueue / Give, FileManager::ShowOffers: online gifts and offers.)
    // The gifts queued by the server become presents (the queue stays empty offline); a save follows.
    if (PresentsContainer::UnqueueStartPresents() && !GameState::IsCityTutorial()) Map::Save(0);
    // desktop_window (the world) is kept at the bottom of the window queue, under anything moved
    // down (BuildingHovers::MoveWindowDown).
    if (WindowManager::g_desktopWindow && WindowManager::g_desktopWindow->next)
        WindowManager::g_desktopWindow->MoveWindowDown(true);
    // (androidTextInputResult: the Java text dialog's answer; the port types into the field)
    // A press held for more than 0.2 s is a long tap.
    longTapTime = (float)frameDt + longTapTime;
    if (held && 0.2f < longTapTime && !longTapStarted) {
        GUI::OnLongTapStart();
        longTapStarted = true;
    }
    // (GameState::UpdateServerFlags: online)
    // The social requests (Expansions::IsRequiredUpdateRunning and FacebookSyncWindow, which hold
    // them back, are never on offline): 8 opened the Facebook login (SystemFuncs::LoginToFB,
    // online), 9 nothing, 13 / 14 PlayerProfileManager::OnAFB (online).
    if (mActionToDo == 8) {
        std::printf("FBB: AFB_SOCIAL_CONNECT_WINDOW");
        mActionToDo = 0;
    } else if (mActionToDo == 9 || (unsigned)(mActionToDo - 0xd) < 2u) {
        mActionToDo = 0;
    }
    // With no window open and outside the city tutorial, the request opens its screen and is done.
    if (WindowManager::GetShownWindowCount() == 0 && mActionToDo != 0 && !GameState::IsCityTutorial()) {
        switch (mActionToDo) {
            case 1: Map::PlayerDefaultSpawn(); break;
            case 2:
                ShopWindow::Show();
                ShopWindow::OnSelectItemID((unsigned)mActionValue);
                ShopWindow::ShowArrowOnItem((unsigned)mActionValue);
                break;
            case 3:
                ShopWindow::Show();
                ShopWindow::OnTabSelect((unsigned)mActionValue);
                break;
            case 5:
                ExchangeWindow::Show();
                ExchangeWindow::OnTab((unsigned)mActionValue, nullptr);
                break;
            case 6:
                ItemShopWindow::Show();
                ItemShopWindow::OnTab((unsigned)mActionValue);
                break;
            case 7:
                ItemShopWindow::Show();
                ItemShopWindow::ShowArrowOnItem((uint32_t)mActionValue);
                break;
            case 11: ChestsWindow::Show(); break;
            // 10 PlayerCompetitionWindow::Show + OnTab(4); 12 the friends' gift reminder (with
            // task "task_friends_enable" done, outside a farm: NeighboursServer and the settings
            // "last_gift_send" / "last_gift_remind"); 15 PlayerCompetitionWindow's Facebook tab (5,
            // after RefreshServer) for a registered player signed in to "fb": all online.
            default: break;
        }
        mActionToDo = 0;
    }
    AskLanguageQuestion();
    // More than 2 s in the background goes to the buildings in one Map::OfflineUpdate.
    if (2.f < pauseTime) {
        Map::OfflineUpdate((double)pauseTime);
        pauseTime = 0.f;
    }
}

// PORT test aid (--screenshot): the frames and inputs of a run without a window, then the picture.
int RunHeadless() {
    const float dt = 1.f / 30.f;
    auto frame = [&]() {
        AfterEvents(dt);
        Tick(dt);
    };
    Override::Apply(Override::kAll);   // PORT: main_Loop_Func's, which the headless frames do not run
    for (int i = 0; i < 60; ++i) frame();   // let the HUD slide in and count up
    if (opt.show == "city_rename") {
        CityRenameWindow::Show();
        for (int i = 0; i < 30; ++i) frame();
    }
    for (const Options::Input& in : opt.inputs) {
        if (in.type == Options::kType || in.type == Options::kKey) {
            if (in.type == Options::kType) TextInputUtf8(in.text.c_str());
            else KeyDown(in.x);
            for (int i = 0; i < 30; ++i) frame();
            continue;
        }
        if (in.type == Options::kMove) {
            // 20 motion events, one per frame, from the last pointer position.
            const int sx = lastX, sy = lastY, steps = 20;
            for (int k = 1; k <= steps; ++k) {
                firstMove = true;
                MouseMove(sx + (in.x - sx) * k / steps, sy + (in.y - sy) * k / steps);
                frame();
            }
            continue;
        }
        firstMove = true;
        MouseMove(in.x, in.y);
        MouseDown(in.x, in.y, true);
        for (int i = 0; i < 3; ++i) frame();
        if (in.type == Options::kPress) break;
        int ex = in.x, ey = in.y;
        if (in.type == Options::kDrag) {
            const int steps = 10;   // one motion event per frame
            for (int k = 1; k <= steps; ++k) {
                firstMove = true;
                ex = in.x + (in.x2 - in.x) * k / steps;
                ey = in.y + (in.y2 - in.y) * k / steps;
                MouseMove(ex, ey);
                frame();
            }
        }
        MouseUp(ex, ey, true);
        for (int i = 0; i < 30; ++i) frame();
    }
    for (int i = 0; i < opt.frames; ++i) frame();
    if (opt.save) Map::SafeSave();
    for (unsigned i = 0; opt.dumpEntities && EntityManager::EnumEntities(i); ++i) {
        Entity* e = EntityManager::EnumEntities(i);
        AnimationController* ac = e->GetAnimController();
        Map::Building* w = e->GetWorkplace();
        std::printf("entity %u: id %u tile %d,%d world %.1f,%.1f state %d anim %s dir %d path %zu work %s\n", i,
                    e->data->id, e->tileX, e->tileY, e->worldX, e->worldY, e->GetAI() ? e->GetAI()->GetState() : -1,
                    ac && ac->anim ? ac->anim->name.c_str() : "-", e->GetDirection(),
                    e->GetAI() ? e->GetAI()->path.size() : 0,
                    w ? (std::to_string(w->data->id) + "@" + std::to_string(w->x) + "," + std::to_string(w->y)).c_str() : "-");
    }
    Render::SortRenderLayer(Render::kLayerGUI, 1);
    Render::Frame();
    glFinish();
    bool ok = Render::SaveScreenshot(opt.screenshot.c_str());
    std::printf("screenshot %s: %s\n", opt.screenshot.c_str(), ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}

// PORT (user 2026-10-18): the desktop's display options, kept in settings.txt (file "port"). The
// frame-rate cap ("fps_cap": 30, 60 or 0 for the display's rate; 60 by default, the phones' rate:
// the port otherwise runs at the display's 120 Hz) waits after the buffer swap; on a display of the
// cap's rate or slower vsync alone paces the frames. "fullscreen" is the window's state. F10 cycles
// the cap; F11, Cmd+Ctrl+F or Alt+Enter switch fullscreen (as the window's green button does).
// Headless test runs are never capped and never fullscreen.
namespace DisplayOptions {
int fpsCap = 60;
uint64_t nextFrame = 0;   // SDL_GetTicksNS of the next capped frame

void Load() {
    fpsCap = opt.fps >= 0 ? opt.fps : SystemFuncs::GetSetting_Int("fps_cap", 60, "port");
}

void Present() {
    SDL_GL_SwapWindow(win);
    if (headless || fpsCap <= 0) return;
    const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(win));
    if (mode && mode->refresh_rate > 0.f && mode->refresh_rate <= (float)fpsCap + 1.f) return;
    const uint64_t period = 1000000000ull / (uint64_t)fpsCap;
    const uint64_t now = SDL_GetTicksNS();
    if (now < nextFrame) {
        SDL_DelayPrecise(nextFrame - now);
        nextFrame += period;
    } else if (now - nextFrame < period) {
        nextFrame += period;   // a little late: keep the rhythm
    } else {
        nextFrame = now + period;
    }
}

void CycleCap() {
    fpsCap = fpsCap == 30 ? 60 : (fpsCap == 60 ? 0 : 30);
    SystemFuncs::SetSetting_Int("fps_cap", fpsCap, "port");
    std::printf("frame-rate cap: %s\n", fpsCap ? std::to_string(fpsCap).c_str() : "the display's rate");
}

void ToggleFullscreen() {
    SDL_SetWindowFullscreen(win, (SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN) == 0);
}

// The window entered or left fullscreen (a key, the green button or the system).
void OnFullscreen(bool on) {
    if (!headless) SystemFuncs::SetSetting_Bool("fullscreen", on, "port");
}
}  // namespace DisplayOptions

// The platform start (SDL_baseInit 0x18b1e0, whose command line is read by Parse): SDL, the
// window and its GL context, the renderer and the fonts. PORT: SDL3 and desktop OpenGL; the
// device's screen is the window's framebuffer.
int SDL_baseInit() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    // PORT: on a Retina display the framebuffer has the screen's real pixels (twice the window's
    // size), so the original's large-screen setup applies (SetScreenSize: HighDPI, GUI scale 2.0, base
    // zoom 1.5), as on a Retina tablet. The headless test runs keep 1280x800 pixels (their taps).
    const bool retina = !headless || std::getenv("RTK_RETINA");
    win = SDL_CreateWindow("Rule the Kingdom", 1280, 800,
                           SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | (headless ? SDL_WINDOW_HIDDEN : 0) |
                               (retina ? SDL_WINDOW_HIGH_PIXEL_DENSITY : 0));
    if (!win) { std::fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError()); return 1; }
    gl = SDL_GL_CreateContext(win);
    if (!gl) { std::fprintf(stderr, "GL context: %s\n", SDL_GetError()); return 1; }
    // PORT: vsync, except in headless runs (a hidden window's swap can wait for a display that sleeps).
    SDL_GL_SetSwapInterval(headless ? 0 : 1);
    SDL_GetWindowSizeInPixels(win, &fbw, &fbh);
    std::printf("GL %s, framebuffer %dx%d\n", (const char*)glGetString(GL_VERSION), fbw, fbh);
    if (!Render::Init(fbw, fbh)) return 1;
    Render::presentHook = DisplayOptions::Present;   // Render::Update's buffer swap
    DisplayOptions::Load();
    if (!headless && (opt.fullscreen >= 0 ? opt.fullscreen != 0 : SystemFuncs::GetSetting_Bool("fullscreen", false, "port")))
        SDL_SetWindowFullscreen(win, true);
    Render::aspect = (float)fbw / (float)fbh;
    // The device screen decides the UI configuration; the port's "device screen" is the window's
    // framebuffer in pixels.
    GUI::SetScreenSize(fbw, fbh);
    Render::InitFonts();
    // PORT: the reminder left by the last quit is dropped (the player is back).
    SystemNotifications::Init();
    return 0;
}

// The loaders as LoadSystemConfiguration wants them.
bool LoadItems(const char* file) { return Items::Init(file); }
bool LoadItemPacks(const char* file) { return Items::InitPacks(file); }

}  // namespace

namespace Game {

// @0x18d850 Game::main_Loop_Init. 0: the game is loaded; 1: it could not start.
// The loading screen's letter and bar show as the start-up goes (ShowLoadProgress); the load
// steps go to LoadSampling.
int main_Loop_Init() {
    splashLoop();
    LoadSampling::Init();
    LoadSampling::LoadStep(1);
    SystemFuncs::Init();
    GameState::InitDefaultSettings();
    languageID = (unsigned)SystemFuncs::getCurrentLanguage();
    // PORT: what Android's Java side tells the game before this (SDLMain.mcSetLanguage and
    // mcSetAvailableLanguage).
    languageHasBeenSelected = SystemFuncs::HasSelectedLanguage();
    availableLanguageID = (unsigned)(opt.systemLang >= 0 ? opt.systemLang : SystemFuncs::GetSystemLanguage());
    // PORT: Korean and Japanese need Android's fallback font or a desktop stand-in (see GUI::Init
    // below); without one they are not offered. "--lang=" still forces them.
    if ((availableLanguageID == 7 || availableLanguageID == 8) &&
        FileManager::SystemFontPath("/system/fonts/DroidSansFallback.ttf").empty())
        availableLanguageID = 0;
    if (SDL_baseInit() != 0) return 1;
    // PORT: Java's ShowVideoSplash, the logo film the app opens with (not after a restart, which
    // Android does without a new activity).
    if (!headless && !std::getenv("RTK_RESTARTED") &&
        !IntroVideo::Play(win, IntroVideoPath(opt.apk)))
        return 1;
    if (opt.lang >= 0) languageID = (unsigned)opt.lang;   // (SDL_baseInit: "--lang=")
    // (CMyServer::Init / NET_Registration, Bonus::Init, PlayerProfileManager::Init, ToS::Init: online)
    LoadSampling::LoadStep(2);
    lastTicks = SDL_GetTicks();   // Game::startTime, Game::time
    Render::ForceBufferSwap();
    InitLoadProgress();
    LoadSampling::LoadStep(3);
    ShowLoadProgress("L", 0.0);
    // ("fbid" is cleared when not connected to Facebook: online)
    SetSaveNames();
    // (a "last_played_save" other than "offline": SocNets::ChangeLoginFriendsSocNet(0);
    // FileManager::GetServerUpdate: online)
    SoundsManager::Init("config/Sounds.xml", "ogg/", ".ogg");
    SoundsManager::SetVolume_Music(SettingsWindow::GetMusicMutedState() ? 0.f : 0.35f);
    SoundsManager::SetVolume_Sounds(SettingsWindow::GetSoundMutedState() ? 0.f : 1.f);
    SoundsManager::SetMusicFadeDelays(1000, 500);
    LoadSampling::LoadStep(4);
    gMusicName = "music_battle_map";
    turnMusic(true);
    canPlayMusic = true;
    LoadSampling::LoadStep(5);
    // (WallPost::Init("config/fbwall_mob.xml"): online. Render::CreateAtlas(0, 0x400, 0x800 or, on a
    // slow device, 0x400, 8): the port's renderer has no atlases.)
    if (!LoadLanguageTable("LocalizedStringsEN.xml", "L", 3.0 / 46.0)) return 1;
    // A new player gets the system's language without being asked.
    if (!FileManager::SaveExists(SaveManager::GetCurrentSaveName().c_str())) {
        FileManager::SetPreferredLanguade(availableLanguageID);
        languageID = availableLanguageID;
        languageHasBeenSelected = true;
    }
    // The language's own table goes on top of the English one; without it the game stays English.
    static const struct { const char* file; const char* code; } kLanguages[9] = {
        {nullptr, "EN"}, {"LocalizedStrings.xml", "RU"}, {"LocalizedStringsDE.xml", "DE"},
        {"LocalizedStringsLV.xml", "LV"}, {"LocalizedStringsES.xml", "ES"}, {"LocalizedStringsIT.xml", "IT"},
        {"LocalizedStringsFR.xml", "FR"}, {"LocalizedStringsKO.xml", "KO2"}, {"LocalizedStringsJP.xml", "JA"}};
    if (languageID >= 1 && languageID <= 8 && !LoadLanguageTable(kLanguages[languageID].file, "L", 3.0 / 46.0))
        languageID = 0;
    StringTable::SetLanguage(languageID <= 8 ? kLanguages[languageID].code : "EN", (int)languageID);
    // (the sandbox server's res_files_sandbox config is not used offline)
    GameState::LoadSettings("../resource/res_files/1Original/dynamic_config.xml");
    // (Tags::Init is empty; SocNets::Init, MyFacebook::Init, Neighbours::Init: online)
    // The settings set from the flag globals: their initial values in the binary (online,
    // CommonSetFlags lets the server change some of them first).
    GameState::SetSetting("exchange_enabled", 0.f);
    GameState::SetSetting("character_icon_opens_heal", 0.f);
    GameState::SetSetting("your_army_opens_tavern", 0.f);
    GameState::SetSetting("always_open_tavern", 0.f);
    GameState::SetSetting("skip_choose_troops_window", 1.f);
    GameState::SetSetting("train_only_for_crystals", 1.f);
    GameState::SetSetting("progress_as_time", 1.f);
    GameState::SetSetting("campaign_to_city_direct", 1.f);
    GameState::SetSetting("crystal_mult", 1.f);
    GameState::SetSetting("contract_coeff", 1.f);
    GameState::SetSetting("farm_coeff", 1.f);
    // PORT (user's bug report 2026-10-26): the server's switch set to 1, so the progress box over a
    // building at work shows only "tap to speed up" until it is tapped, and shrinks back on a tap
    // elsewhere (BuildProgressHoverWindow::Click); with 0 the big box and its hurry button never go.
    GameState::SetSetting("hide_speedup", 1.f);
    GameState::SetSetting("stack_size", 50.f);
    GameState::SetSetting("friend_fight_timeout", 3600.f);
    GameState::SetSetting("pvp_refresh_time", 14400.f);
    GameState::SetSetting("item_boosts", 0.f);
    GameState::SetSetting("spell_unlock", 0.f);
    GameState::SetSetting("starter_chest_b", 0.f);
    GameState::SetSetting("auto_fix_map_reset", 1.f);
    GameState::SetSetting("show_hp_value", 0.f);
    GameState::SetSetting("fb_sharing_enabled", 0.f);
    GameState::SetSetting("og_enabled", 0.f);
    GameState::SetSetting("og_enabled2", 1.f);
    GameState::SetSetting("enable_error_code", 0.f);
    GameState::SetSetting("flurry_enabled", 0.f);
    GameState::SetSetting("cinematic_camera", 1.f);
    GameState::SetSetting("bot_use_cb", 1.f);
    // PORT (freemium removal): the exchange's rates (ExchangeWindow), placeholders to tune.
    GameState::SetSetting("port_exchange_gold_per_crystal", 200.f);
    GameState::SetSetting("port_exchange_gold_per_bought_crystal", 400.f);
    // PORT (offline PvP, DECIDED 2026-10-18): the rivals in each list PvPServer makes.
    GameState::SetSetting("port_pvp_rival_count", 5.f);
    // PORT (DECIDED 2026-10-20): the PvP window's early refresh is free (the data file has 5 crystals).
    GameState::SetSetting("pvp_refresh_cost", 0.f);
    // PORT (user 2026-10-26): King of the Hill (arena event 2) has no cooldown (the data file has
    // 1800 s, skipped early for "arena_unlock" crystals); a run still lasts its 900 s.
    GameState::SetSetting("arena_waves_cooldown_time", 0.f);
    // PORT (docs/balance_plan.md): crystals from play. A quest pays its kind's base times the
    // level factor 1 + (level - 1) / div; a level-up pays level_up_cb + level / div.
    GameState::SetSetting("port_quest_crystals_main", 1.f);
    GameState::SetSetting("port_quest_crystals_side", 1.f);
    GameState::SetSetting("port_quest_crystals_campaign", 5.f);
    GameState::SetSetting("port_quest_crystals_daily", 1.f);
    GameState::SetSetting("port_level_factor_div", 14.f);
    GameState::SetSetting("port_levelup_crystals_div", 4.f);
    GameState::SetSetting("port_skip_price_mult", 5.f);   // skipping a quest step: 5x the original price
    GameState::SetSetting("port_daily_crystals", 3.f);    // the daily bonus: 3 + level / 5 crystals
    GameState::SetSetting("port_daily_crystals_div", 5.f);
    // PORT (balance, user 2026-10-20): every crystal reward x3 (GameState::CrystalReward).
    GameState::SetSetting("port_crystal_reward_mult", 3.f);
    SystemFuncs::SetSetting_Int("show_starting_win", 0, "MyCountry_flags");
    // (Building 99's first upgrade gets an exchange limit of 75 once the frame loop applies it.)
    Override::AddParameter(Override::kBuilding, 99, 1, "r_cb_limit", "75");
    Override::Apply(Override::kSetting);

    ShowLoadProgress("L", 3.0 / 46.0);
    LoadSampling::LoadStep(6);
    // The data files, in the original's order; the ones of later milestones are named in place.
    // (Resources::RegisterResources "files_in_packs_list.xml", 4/46: the port finds a file's pack
    // when it is asked for, see Resources.h)
    if (!LoadSystemConfiguration(Resources::Init, "AllAnimsFrames.xml", "L", "Resources::RegisterAnimations",
                                 5.0 / 46.0, 8, 8) ||
        !LoadSystemConfiguration(GameData::LoadDecorations, "decors.xml", "L", "Map::LoadDecorationList",
                                 6.0 / 46.0, 9, 9))
        return 1;
    // (Map::LoadPersonList "persons.xml", 7/46: the map's own person list is dead data in 5.11, no
    // Map::Person is ever created.)
    EntityFactory::Init();
    EntityManager::Init();
    if (!LoadSystemConfiguration(LoadEntityFactoryData, "persons.xml", "I", "GetEntityFactory::Init",
                                 8.0 / 46.0, 0x10, 0x10) ||
        !LoadSystemConfiguration(LoadItems, "items.xml", "L", "Items::Init", 9.0 / 46.0, 0xa, 0xa) ||
        !LoadSystemConfiguration(LoadItemPacks, "item_packs.xml", "L", "Items::InitPacks", 10.0 / 46.0, 0xb, 0xb))
        return 1;
    Items::InitSpecial();
    if (!LoadSystemConfiguration(Spells::Init, "spells.xml", "L", "Spells::Init", 11.0 / 46.0, 0x7dd, 0xc) ||
        !LoadSystemConfiguration(Locations::Init, "locations.xml", "L", "Locations::Init", 12.0 / 46.0, 0xd, 0xd))
        return 1;
    Locations::InitSpecial();
    if (!LoadSystemConfiguration(Sets::Init, "sets.xml", "L", "Sets::Init", 13.0 / 46.0, 0x7df, 0xe)) return 1;
    if (!LoadSystemConfiguration(Contracts::Init, "deliveries.xml", "L", "Contracts::Init", 14.0 / 46.0, 0x11,
                                 0x11))
        return 1;
    Contracts::InitSpecial();
    if (!LoadSystemConfiguration(GameData::LoadBuildings, "buildings.xml", "L", "Map::LoadBuildingList",
                                 15.0 / 46.0, 0x12, 0x12))
        return 1;
    if (!LoadSystemConfiguration(PresentsContainer::LoadPresentPacks, "shop_packs.xml", "L",
                                 "PresentsContainer::LoadPresentPacks", 16.0 / 46.0, 0x7e3, 0x13))
        return 1;
    if (!LoadSystemConfiguration(Collections::Init, "collections.xml", "L", "Collections::Init", 17.0 / 46.0, 0x7e4,
                                 0x14))
        return 1;
    if (!LoadSystemConfiguration(Tasks::Init, "tasks.xml", "L", "Tasks::Init", 18.0 / 46.0, 0x15, 0x15)) return 1;
    if (!LoadSystemConfiguration(Professions::Init, "professions.xml", "L", "Professions::Init", 19.0 / 46.0, 0x7e7,
                                 0))
        return 1;
    if (!LoadSystemConfiguration(Chests::Init, "chests.xml", "L", "Chests::Init", 20.0 / 46.0, 0x7e8, 0)) return 1;
    if (!LoadSystemConfiguration(ProjectileData::Init, "projectiles.xml", "L", "ProjectileData::Init", 21.0 / 46.0,
                                 0x18, 0x18))
        return 1;
    if (!LoadSystemConfiguration(Tavern::Init, "tavern.xml", "L", "Tavern::Init", 22.0 / 46.0, 0x16, 0x16)) return 1;
    Tavern::InitSpecial();
    Render::zoom = Render::GetDefaultZoom();
    // Korean and Japanese use Android's fallback font (FileManager::LoadFile reads it from the disk;
    // PORT: a desktop stand-in, FileManager::SystemFontPath; without one the game's own font, which
    // has no such glyphs).
    const char* font = "fonts/ARICYRB.ttf";
    if ((languageID == 7 || languageID == 8) && !FileManager::SystemFontPath("/system/fonts/DroidSansFallback.ttf").empty())
        font = "/system/fonts/DroidSansFallback.ttf";
    GUI::Init(font, false);
    // The game's windows: their static FunctionalWindow objects are constructed in the binary's
    // static-initialiser order (by source file), then WindowQueue::InitWindows runs their Init.
    ArenaDifficultyWindow::Queue();
    ArenaEventWindow::Queue();
    ArenaTurnWindow::Queue();
    BattleBarWindow::Queue();
    BeltBarWindow::Queue();
    BossIntroWindow::Queue();
    BossOutroWindow::Queue();
    BottomBattleWindow::Queue();
    BottomCityWindow::Queue();
    BottomFarmWindow::Queue();
    BottomScreenshotWindow::Queue();
    BuildingHovers::Queue();
    CampaignCompleteWindow::Queue();
    CampaignInfoWindow::Queue();
    CampaignPerfectedWindow::Queue();
    CastleTopWindow::Queue();
    ChapterWindow::Queue();
    CharacterInfoWindow::Queue();
    ChestOpenedWindow::Queue();
    ChestsWindow::Queue();
    ChoosePathWindow::Queue();
    CityRenameWindow::Queue();
    ConfirmPurchaseWindow::Queue();
    CraftItemWindow::Queue();
    CraftSuccessWindow::Queue();
    CraftingWindow::Queue();
    DailyBonusWindow::Queue();
    ExchangeWindow::Queue();
    FoundItemsWindow::Queue();
    GlobalMapWindow::Queue();
    HUDWindow::Queue();
    LandExpandedWindow::Queue();
    LandWindow::Queue();
    LevelInfoWindow::Queue();
    LevelUpWindow::Queue();
    LoadingScreenWindow::Queue();
    NeedItemWindow::Queue();
    HealSquadWindow::Queue();
    HireTroopsWindow::Queue();
    ItemShopWindow::Queue();
    NotEnoughWindow::Queue();
    PlayerBattleInOutWindow::Queue();
    PlayerCompetitionWindow::Queue();
    PlayerInfoWindow::Queue();
    PlayerTopWindow::Queue();
    PopupSelectionWindow::Queue();
    PopupWindow::Queue();
    RaidCompleteWindow::Queue();
    RaidInfoWindow::Queue();
    RepairWindow::Queue();
    SelectHQWindow::Queue();
    SetInfoWindow::Queue();
    SettingsWindow::Queue();
    ShopWindow::Queue();
    StartRaidingWindow::Queue();
    SultanArrivingWindow::Queue();
    TaskCompleteWindow::Queue();
    TaskHolderWindow::Queue();
    TaskInfoWindow::Queue();
    TaskListWindow::Queue();
    TopCityWindow::Queue();
    TutorialWindow::Queue();
    ShowLoadProgress("L", 0.5);
    TextStyleManager::Init();
    LoadSampling::LoadStep(0x17);
    // The desktop window last (the bottom of the queue), then every window's Init.
    WindowManager::g_desktopWindow = new WindowManager::DesktopWindow();
    WindowManager::InitWindows();
    ShowLoadProgress("I", 0.9);
    BuildingPlacement::Init();
    BuildingMovement::Init();
    SpellMovement::Init();
    MapMovement::Init();
    LoadSampling::LoadStep(0x18);
    ShowLoadProgress("L", 0.95);
    TutorialSender::SetTimeLimit(20.f);
    SaveManager::Init();
    LoadSampling::LoadStep(0x19);
    SoldierPool::Init();
    SoldierSlots::Init();
    PvP::Init();
    PvP::ClearPlayerInfo(0xff);
    PvP::CreatePvPTutorialPlayer();
    PvP::CreatePvPTutorialPlayersRandom();
    LoadSampling::LoadStep(0x19);
    TextInput::enabled = false;
    // (The waits for the server's first answers and for a save being fetched, Bonus::Enqueue: online.)
    // (Background::Init: the port loads the ground textures with the first land. FileManager::
    // SetGameState(1): a state Java reads.)
    ShowLoadProgress("T", 1.0);
    Render::SortRenderLayer(Render::kLayerGUI, 1);
    bool corrupt = false;
    SaveManager::Load(nullptr, &corrupt);
    LoadSampling::LoadStep(0x1a);
    // Walking costs: 50 while the required content packs are not installed (Expansions, online:
    // not enabled), 1.41 otherwise.
    AI::SetWeight(1.41f);
    // (GameState::ResetGameLock, Game::skipGameLoad, profileChosen: the online profile choice)
    ShowLoadProgress("T", 1.0);   // (the original only draws the bar again, the letter unchanged)
    if (!opt.mapGiven || SaveManager::GetGameStateData()) {
        LoadSavedGame(false);
    } else {
        // PORT test path ("--map N" without a save): instead of the new game's tutorial, map N
        // with the opening tutorial done. The hero, its squad and the first soldier are made as
        // LoadSavedGame's new game makes them.
        GameState::Reset();
        GameState::playerSeed = (uint32_t)opt.seed;
        GameState::tutorial = 0x80;   // (the opening tutorial done; 0x100 is secondTutorial's end)
        if (opt.secondTutorial >= 0) GameState::secondTutorial = opt.secondTutorial;
        Entity* hero = EntityManager::CreateEntity(0xc, false, true);
        hero->CreateSquad();
        hero->temporary = false;
        Entity* soldier = EntityManager::CreateEntity(0x1a2, false, true);
        SoldierSlots::AddSoldier(soldier);
        hero->GetSquad()->AddSoldier(soldier);
        soldier->temporary = false;
        GameState::SetCurrentMapID(opt.map);
        if (!Map::Load(SaveManager::GetMapData(opt.map), (uint32_t)Timer::GetGlobalTime())) return 1;
    }
    // (PlayerProfileManager: the online profile's start, sync and "no connection" paths; offline
    // only the HUD is shown.)
    HUDWindow::Show();
    Render::SortRenderLayer(Render::kLayerGUI, 1);
    // (LoadingInternetWindow::AllowToShow: online.) The queued start presents (none offline):
    PresentsContainer::UnqueueStartPresents();
    HideLoadProgress();
    // (The sandbox mode sets tutorial 0x80 / secondTutorial 0x100 here. EditorConsole::Init: the admin mode,
    // dead in 5.11 (port_plan 5w). FileManager::GameLoaded: a call to Java.)

    // PORT test aids: entities to spawn and a view other than Map::Load's (on the player, in the
    // city on tile (0x1e, 0xc)).
    Entity* lastSpawned = nullptr;
    for (const Options::Spawn& sp : opt.spawns)
        lastSpawned = EntityManager::SpawnEntityAt(sp.id, (unsigned)sp.x, (unsigned)sp.y, false, false);
    if (lastSpawned && opt.walkX >= 0) lastSpawned->GetAI()->WalkTo(opt.walkX, opt.walkY);
    if (opt.camera) {
        Render::CenterOn(opt.camX, opt.camY);
        Render::zoom = Render::GetDefaultZoom() * opt.zoom;
    }

    std::printf("main() Game loaded in %f\n", Timer::GetTime());
    LoadSampling::LoadStep(0x22);
    OnResume();
    pauseTime = 0.f;
    return 0;
}

// @0x18545c Game::main_Loop_BeforeInit: the events that piled up while loading are dropped.
void main_Loop_BeforeInit() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {}
}

// @0x188a40 Game::main_Loop_Func: one frame. False ends the loop.
// Left out: everything online at its start (scheduled bonuses, restart and server update, the
// heartbeat, the save sent and pinged to the server), the render
// suspend / restore events (Android loses its GL context in the background), the crystals
// bought or taken back by Java (mCallBackCB, mDecCB) and the billing results.
bool main_Loop_Func() {
    SDL_Delay((Uint32)GameState::GetServerSetting(0xae));   // "TSS_DELAY": 0
    Override::Apply(Override::kAll);
    // (Android's audio focus event: turnMusic(allowSoundEvent) once canPlayMusic)
    if (paused) {
        // In the background the loop only sleeps and counts the time (0.5 s a turn).
        SDL_Delay(500);
        pauseTime += 0.5f;
        lastTicks = SDL_GetTicks();
        // PORT: the foreground event comes through SDL's queue, so it is still read.
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_DID_ENTER_FOREGROUND) paused = false;
            if (e.type == SDL_EVENT_QUIT) done = true;
        }
        if (!paused) {
            // (the render restore event: after Render::Restart and the textures' reload)
            GameState::SetupPlayerRegenerationState();
            OnResume();
        }
        return true;
    }
    if (forceExitNoSave) return false;
    SDL_Event e;
    firstMove = true;
    auto toPixels = [](float wx, float wy, int& px, int& py) {
        int ww = 0, wh = 0;
        SDL_GetWindowSize(win, &ww, &wh);
        float k = ww > 0 ? (float)fbw / (float)ww : 1.f;
        px = (int)(wx * k);
        py = (int)(wy * k);
    };
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_EVENT_QUIT: done = true; break;
            case SDL_EVENT_WILL_ENTER_BACKGROUND:
                // The app's pause (Java sets createAutosave and paused; desktop systems do not send
                // this event, the exit saves): outside the city tutorial the drops are collected,
                // the edit modes closed, the game saved and the reminders handed to the system
                // (Notifications; the OS side: SystemNotifications, 7d).
                if (!forceExitNoSave && !GameState::IsCityTutorial()) {
                    TaskCompleteWindow::Hide();
                    BuildingHovers::CollectAll();
                    BuildingMovement::Decline();
                    BuildingPlacement::Decline();
                    SpellMovement::Decline();
                    Map::Save(0);
                    GetNotifications().PurgeNotifications();
                    GameState::UpdateNotifications();
                    Map::UpdateNotifications();
                    GetNotifications().FlushNotifications();
                }
                paused = true;   // SDLMain.mcPauseEvent(1)
                break;
            case SDL_EVENT_DID_ENTER_FOREGROUND:
                paused = false;   // SDLMain.mcPauseEvent(0)
                break;
            case SDL_EVENT_KEY_DOWN:
                // PORT: Esc is the desktop's Back key (Android's: the top window closes, else the exit
                // question); not while typing. F12 saves a screenshot (an aid).
                if (e.key.key == SDLK_ESCAPE) {
                    if (!TextInput::enabled) KeyDown(SDL_SCANCODE_AC_BACK);
                    break;
                }
                if (e.key.key == SDLK_F12) Render::SaveScreenshot((FileManager::GetPicturesPath() + "rtk_screenshot.png").c_str());
                // PORT: the display options' keys (DisplayOptions).
                if (!e.key.repeat) {
                    if (e.key.key == SDLK_F10) { DisplayOptions::CycleCap(); break; }
                    if (e.key.key == SDLK_F11 ||
                        (e.key.key == SDLK_F && (e.key.mod & SDL_KMOD_GUI) && (e.key.mod & SDL_KMOD_CTRL)) ||
                        (e.key.key == SDLK_RETURN && (e.key.mod & SDL_KMOD_ALT))) {
                        DisplayOptions::ToggleFullscreen();
                        break;
                    }
                }
                KeyDown(e.key.scancode);
                break;
            case SDL_EVENT_TEXT_INPUT:
                TextInputUtf8(e.text.text);
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP: {
                if (e.button.button != SDL_BUTTON_LEFT && e.button.button != SDL_BUTTON_RIGHT) break;
                int x, y;
                toPixels(e.button.x, e.button.y, x, y);
                if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) MouseDown(x, y, e.button.button == SDL_BUTTON_LEFT);
                else MouseUp(x, y, e.button.button == SDL_BUTTON_LEFT);
                break;
            }
            case SDL_EVENT_MOUSE_MOTION: {
                int x, y;
                toPixels(e.motion.x, e.motion.y, x, y);
                MouseMove(x, y);
                break;
            }
            case SDL_EVENT_MOUSE_WHEEL:
                // PORT: a trackpad scrolls in small fractions: they add up to whole steps (a mouse's
                // notches are whole: one step each, as before).
                if (e.wheel.y != std::floor(e.wheel.y)) {
                    wheelSum += e.wheel.y;
                    for (; wheelSum >= 1.f; wheelSum -= 1.f) Wheel(1);
                    for (; wheelSum <= -1.f; wheelSum += 1.f) Wheel(-1);
                } else {
                    Wheel(e.wheel.y > 0 ? 1 : (e.wheel.y < 0 ? -1 : 0));
                }
                break;
            case SDL_EVENT_PINCH_UPDATE:
                Pinch(e.pinch.scale);
                break;
            case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
            case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
                DisplayOptions::OnFullscreen(e.type == SDL_EVENT_WINDOW_ENTER_FULLSCREEN);
                break;
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                // SDL_WINDOWEVENT_RESIZED: new screen size, aspect, default zoom, Render::Resize, then the
                // windows' depths again from the queue's head. PORT: Render::Init takes the new size
                // (the GL viewport; on Android the surface keeps its size).
                SDL_GetWindowSizeInPixels(win, &fbw, &fbh);
                Render::Init(fbw, fbh);
                Render::aspect = (float)fbw / (float)fbh;
                Render::zoom = Render::GetDefaultZoom();
                Render::Resize(fbw, fbh);
                if (WindowManager::Head()) WindowManager::Head()->MoveWindowOnTop(true);
                break;
            default: break;
        }
    }
    // The frame's time, also kept for the frame-rate statistics.
    uint64_t now = SDL_GetTicks();
    double dt = (double)(now - lastTicks) / 1000.0;
    lastTicks = now;
    Timer::AddSample(dt);
    AfterEvents(dt);
    dt = Timer::ApplyDeltaTimeCompensation(dt);
    // (The online states that replace the game's Update: the Facebook question, a save being sent
    // or fetched, a required content update.)
    if (!restartGameAfterExit && !done) {
        SoundsManager::Update((float)dt);
        Tick((float)(dt * (double)HUDWindow::GetDeltaTimeMultiplier()));
        Render::Frame();
        DisplayOptions::Present();
    }
    // (FileManager::DebugUpdate)
    return true;
}

// @0x185200 Game::main_Exit_Func
int main_Exit_Func() {
    // (EditorConsole::Deinit: the admin mode, dead. ErrorReporter::Deinit: online.)
    gameLoaded = false;
    GameState::EndActiveCombat(true);
    RaidCompleteWindow::ForceHide();
    CampaignPerfectedWindow::ForceHide();
    TaskCompleteWindow::Hide();
    BuildingHovers::CollectAll();
    BuildingMovement::Decline();
    BuildingPlacement::Decline();
    SpellMovement::Decline();
    SpellMovement::Deinit();
    BuildingMovement::Deinit();
    BuildingPlacement::Deinit();
    MapMovement::Deinit();
    // (online: also not during a required content update or the Facebook sync window)
    if (!GameState::IsCityTutorial() && !forceExitNoSave) {
        Map::Save(0);
        // PORT (user 2026-10-26): a desktop sends no pause event, so the quit hands the reminders to
        // the system as the pause does (main_Loop_Func's WILL_ENTER_BACKGROUND).
        GetNotifications().PurgeNotifications();
        GameState::UpdateNotifications();
        Map::UpdateNotifications();
        GetNotifications().FlushNotifications();
    }
    Map::Free();
    EntityManager::Clean(true);
    // The windows' Deinit (WindowQueue::DeinitWindows), then the desktop window goes.
    delete WindowManager::g_desktopWindow;
    WindowManager::g_desktopWindow = nullptr;
    WindowManager::DestroyPendingWindows();
    GUI::Deinit();
    // The Deinit of every other system follows on the original (text styles, the data lists, the
    // online services, AI::FreeWaypoints, icons, resources, sounds, the renderer, files, timer,
    // strings, saves): the port's objects release what they hold when the program ends. The
    // game data lists are emptied here as there:
    GameData::BuildingDeinit();
    GameData::DecorDeinit();
    PresentsContainer::Clear(true);
    GameState::Clean();
    Render::Shutdown();
    SDL_GL_DestroyContext(gl);
    SDL_DestroyWindow(win);
    gl = nullptr;
    win = nullptr;
    // (IMG_Quit and) SDL_Quit, unless the game starts again
    if (!restartGameAfterExit) SDL_Quit();
    std::puts("main() Finished exit");
    return 0;
}

}  // namespace Game

// @0x241b98 RepeatedMain: one run of the game, from loading to exit.
int RepeatedMain() {
    int failed = Game::main_Loop_Init();
    if (failed != 0) return failed;
    gameLoaded = true;
    if (headless) {
        // PORT test aid: no window and no event loop; --quit also runs the exit (and its save).
        int result = RunHeadless();
        if (opt.quit) Game::main_Exit_Func();
        return result;
    }
    Game::main_Loop_BeforeInit();
    while (!done && Game::main_Loop_Func()) {}
    Game::main_Exit_Func();
    return 0;
}

// The program's entry (SDL_main 0x241bec). PORT: the files' sources and the save folder come
// first (on Android Java provides both), and a restart (FileManager::RestartGame, the language
// change) starts the program again where the original loops back into RepeatedMain.
int main(int argc, char** argv) {
    opt = Parse(argc, argv);
    headless = !opt.screenshot.empty();
    if (!headless) Game::loadScreenSeconds = 4.0;   // PORT: see Game.h
    traceInput = std::getenv("RTK_TRACE_INPUT") != nullptr;
    // PORT: the storage folder is the user's application support folder (Android: the app's files).
    if (opt.storage.empty()) {
        char* pref = SDL_GetPrefPath("", "Rule the Kingdom");
        if (pref) opt.storage = pref;
        SDL_free(pref);
    } else if (opt.storage.back() != '/') {
        opt.storage += '/';
    }
    FileManager::SetStoragePath(opt.storage);
    // PORT: pictures (the screenshot mode, F12) go to the user's Pictures folder, in its own folder;
    // headless runs keep them in the storage folder.
    if (!headless)
        if (const char* pictures = SDL_GetUserFolder(SDL_FOLDER_PICTURES))
            FileManager::SetPicturesPath(std::string(pictures) + "Rule the Kingdom/");
    opt.apk = FindGameApk(opt.root, opt.storage, !headless);
    if (opt.apk.empty() || !RegisterSources(opt.apk, FindGameData())) {
        if (!headless)
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Rule the Kingdom",
                                     opt.apk.empty() ? "The game needs the Rule the Kingdom 5.11 APK (the Android "
                                                       "game file) in the same folder as the game."
                                                     : "The game files could not be read.", nullptr);
        return 1;
    }

    int result = RepeatedMain();
    const bool restart = restartGameAfterExit && !headless;
    forceExitNoSave = false;
    done = false;
    GameState::ResetGamePauseState();
    if (restart) {
        setenv("RTK_RESTARTED", "1", 1);   // (no intro film again)
        SDL_Process* next = SDL_CreateProcess(argv, false);
        if (!next) std::fprintf(stderr, "restart failed: %s\n", SDL_GetError());
        SDL_Quit();
    }
    return result;
}
