// LoadingScreenWindow: the "loading" card (Visit_friend_loading_screen.xml): the small game logo,
// the turning cogs and "GOTOBATTLEMAP". Map::Load shows it while a map switch that started from
// the open world map loads (drawn in its own frames between the load's stages) and hides it at the
// end. (Visiting a friend's city, online, also uses it.)
// Port of LoadingScreenWindow (libkingdom.so 5.11), 0x2f4044..0x2f45b4. Statics at 0x625748.
#include "engine/IconManager.h"
#include "engine/Render.h"
#include "game/StringTable.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/Shared.h"
#include "windows/Windows.h"

namespace LoadingScreenWindow {
namespace {

using GUI::Textfield;
using GUI::Window;

WindowManager::FunctionalWindow* g_queue = nullptr;   // LoadingScreenWindow::wnd 0x625770
Window* g_root = nullptr;              // 0x625748
Window* g_cogs = nullptr;              // 0x62574c loading_mobile (the cogs' animation)
Textfield* g_message = nullptr;        // 0x625750 message_text
Shared::SmallLogoWindow g_logo;        // 0x625754 logo_holder_small

// @0x2f4070: the taps stop here while it is up.
bool Click(int, int, bool) { return g_root->visibleSelf; }

// @0x2f4274
void SetZ(float z) {
    g_root->SetZ(z);
    g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::GetVerticalCenter(g_root->h));
}

// @0x2f4044
void Deinit() {
    delete g_root;
    g_root = nullptr;
}

}  // namespace

WindowManager::FunctionalWindow* Queue() {
    if (!g_queue) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.deinit = Deinit;
        f.click = Click;
        f.setZ = SetZ;
        f.hide = Hide;
        g_queue = new WindowManager::FunctionalWindow("LoadingScreenWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_root->visibleSelf; }   // @0x2f4084

// @0x2f4144: the logo goes to the layer above the GUI (Map::Load draws only from layer 0xe on).
void Show() {
    g_queue->shown = true;
    g_queue->MoveWindowOnTop(true);
    if (!g_root->visibleSelf) WindowManager::WindowShow(false);
    g_root->SetVisibility(true);
    if (g_logo.holder->sprite) Render::ChangeLayer(g_logo.holder->sprite, 0xf);
    if (g_logo.eng->sprite) Render::ChangeLayer(g_logo.eng->sprite, 0xf);
    if (g_logo.lv->sprite) Render::ChangeLayer(g_logo.lv->sprite, 0xf);
}

// @0x2f4210
void Hide() {
    if (!g_root->visibleSelf) return;
    WindowManager::WindowHide(false);
    g_queue->shown = false;
    g_queue->MoveWindowDown(true);
    g_root->SetVisibility(false);
}

// @0x2f42e4
void Init() {
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Visit_friend_loading_screen.xml",
                             "Visit_friend_loading_screen.png", -1.f, 0, 0, 0, 0, false, 1.f);
    g_queue->RegisterTopWindow(g_root);
    g_root->SetVisibility(false);
    g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::GetVerticalCenter(g_root->h));
    g_logo.LoadFrom(g_root, "logo_holder_small");
    g_logo.SetLanguage((unsigned)StringTable::GetLangID());
    g_cogs = GUI::GetWindowTyped<Window>(g_root, "loading_mobile");
    g_cogs->SetTexture(IconManager::GetLoadingCogsIcon(), true, 0, 0, false, 0);
    g_message = GUI::GetWindowTyped<Textfield>(g_root, "message_text");
    g_message->SetText(StringTable::GetString("GOTOBATTLEMAP"));
    g_queue->fn.zRange = WindowManager::ReturnSmallZRange;
}

}  // namespace LoadingScreenWindow
