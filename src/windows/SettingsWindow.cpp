// SettingsWindow: the settings dialog opened from the navigation panel (Window_settings.xml): ten
// buttons (Google Play achievements, news, music, sound, social connect, FAQ, notifications, coupon,
// sync, profiles), the privacy policy / terms buttons and the player's id. Port of SettingsWindow
// (libkingdom.so 5.11), 0x33de04..0x340230. Statics at 0x62d088 (buttons at +0xc, 0x5c bytes each)
// and 0x60f390.
#include <string>

#include "engine/SystemFuncs.h"
#include "engine/Timer.h"
#include "game/GameState.h"
#include "game/StringTable.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "windows/Windows.h"

namespace SettingsWindow {
namespace {

using GUI::Button;
using GUI::Textfield;
using GUI::Window;

// SettingsWindow::ButtonInfo (0x5c bytes): one button_%02d and every icon it can show.
struct ButtonInfo {
    Window* holder;            // +0x00 "%s"
    Window* news;              // +0x04 icon_settings_news
    Window* inviteFriends;     // +0x08
    Window* sound;             // +0x0c
    Window* moreGames;         // +0x10
    Window* screenshot;        // +0x14
    Window* music;             // +0x18
    Window* musicOff;          // +0x1c
    Window* soundOff;          // +0x20
    Window* restart;           // +0x24
    Window* faq;               // +0x28
    Window* notifications;     // +0x2c
    Window* notificationsOff;  // +0x30
    Window* coupon;            // +0x34
    Window* saveGame;          // +0x38
    Window* switchProfile;     // +0x3c
    Window* gpgsAchievements;  // +0x40
    Window* gpgsLogin;         // +0x44 button_login_google_play_short
    Textfield* text;           // +0x48 text_setting
    Textfield* lastSync;       // +0x4c text_last_sync
    Textfield* newGame;        // +0x50 icon_settings_restart.text_new_game
    Textfield* costName;       // +0x54
    Textfield* priceCrystal;   // +0x58
    void LoadFrom(Window* root, const char* name);   // @0x33e500
};

void ButtonInfo::LoadFrom(Window* root, const char* name) {
    holder = GUI::GetWindowTypedF<Window>(root, "%s", name);
    news = GUI::GetWindowTypedF<Window>(root, "%s.icon_settings_news", name);
    inviteFriends = GUI::GetWindowTypedF<Window>(root, "%s.icon_settings_invite_friends", name);
    sound = GUI::GetWindowTypedF<Window>(root, "%s.icon_settings_sound", name);
    moreGames = GUI::GetWindowTypedF<Window>(root, "%s.icon_settings_more_games", name);
    screenshot = GUI::GetWindowTypedF<Window>(root, "%s.icon_settings_screenshot", name);
    music = GUI::GetWindowTypedF<Window>(root, "%s.icon_settings_music", name);
    musicOff = GUI::GetWindowTypedF<Window>(root, "%s.icon_settings_music_off", name);
    soundOff = GUI::GetWindowTypedF<Window>(root, "%s.icon_settings_sound_off", name);
    restart = GUI::GetWindowTypedF<Window>(root, "%s.icon_settings_restart", name);
    faq = GUI::GetWindowTypedF<Window>(root, "%s.icon_settings_faq", name);
    notifications = GUI::GetWindowTypedF<Window>(root, "%s.icon_settings_notifications", name);
    notificationsOff = GUI::GetWindowTypedF<Window>(root, "%s.icon_settings_notifications_off", name);
    coupon = GUI::GetWindowTypedF<Window>(root, "%s.icon_settings_coupon", name);
    saveGame = GUI::GetWindowTypedF<Window>(root, "%s.icon_settings_save_game", name);
    switchProfile = GUI::GetWindowTypedF<Window>(root, "%s.icon_settings_switch_profile", name);
    gpgsAchievements = GUI::GetWindowTypedF<Window>(root, "%s.gpgs_achievements", name);
    gpgsLogin = GUI::GetWindowTypedF<Window>(root, "%s.button_login_google_play_short", name);
    text = GUI::GetWindowTypedF<Textfield>(root, "%s.text_setting", name);
    lastSync = GUI::GetWindowTypedF<Textfield>(root, "%s.text_last_sync", name);
    newGame = GUI::GetWindowTypedF<Textfield>(root, "%s.icon_settings_restart.text_new_game", name);
    costName = GUI::GetWindowTypedF<Textfield>(root, "%s.icon_settings_restart.text_cost_name", name);
    priceCrystal = GUI::GetWindowTypedF<Textfield>(root, "%s.icon_settings_restart.text_price_crystal", name);
}

WindowManager::FunctionalWindow* g_queue = nullptr;
int g_soundMuted = 0;              // 0x62d088
int g_musicMuted = 0;              // 0x62d08c
ButtonInfo g_buttons[10];          // 0x62d094
Window* g_root = nullptr;          // 0x62d430
GUI::TweenEffect* g_tween = nullptr;   // 0x62d434
Textfield* g_title = nullptr;      // 0x62d43c text_title
Textfield* g_id = nullptr;         // 0x62d440 text_id
Window* g_terms1 = nullptr;        // 0x62d448 button_settings_terms_01 (privacy policy)
Textfield* g_terms1Text = nullptr;
Window* g_terms2 = nullptr;        // 0x62d450 button_settings_terms_02 (terms of service)
Textfield* g_terms2Text = nullptr;
Button* g_close = nullptr;         // 0x62d458 x_button.clickArea
int g_lastUpdate = 0;              // the global time of the last UpdateContents from Update
int g_notifications = 1;           // 0x60f390
bool g_syncEnabled = true;         // 0x60f39c
bool g_tablet = false;             // 0x60f39d
float g_scale = 1.f;               // 0x60f3a0 the layout's scale

ButtonInfo& Btn(int n) { return g_buttons[n - 1]; }   // button_%02d, 1-based
int Flip(int v) { return v > 1 ? 0 : 1 - v; }          // the original's "1 - v, 0 above 1"

void CenterOnScreen() {
    g_root->SetPosition((GUI::ScreenWidth() - g_root->w) / 2, GUI::GetVerticalCenter(g_root->h));
}

// @0x33dfcc
void OnTweenOut() {
    GameState::DropGamePauseState();
    WindowManager::WindowHide(false);
    g_queue->shown = false;
    g_queue->MoveWindowDown(true);
}

// @0x33e000: only points the tutorial arrow at the screenshot button after HelpWithScreenShot.
// UNVERIFIED (milestone 3): BuildingHovers arrows are not ported; the help flag is never set.
void OnTweenIn() {}

// @0x33e424. UNVERIFIED (milestone 3): a click first goes to the tutorial arrow when it belongs to
// this window (BuildingHovers::ClickOnArrow).
bool Click(int x, int y, bool pressed) {
    bool visible = g_root->visibleSelf;
    g_root->Click(x, y, pressed, false);
    return visible;
}

// @0x33e3b4
void SetZ(float z) {
    g_root->SetZ(z);
    CenterOnScreen();
}

// @0x33e200
bool OnBack() {
    if (!g_queue->shown) return false;
    Hide();
    return true;
}

// @0x33f958: refreshed once per second while shown (the last-sync time).
void Update(float) {
    if (!IsVisible()) return;
    if (Timer::GetGlobalTime() == g_lastUpdate) return;
    g_lastUpdate = Timer::GetGlobalTime();
    UpdateContents();
}

// @0x33f9c4. UNVERIFIED (milestone 5): SoundsManager::SetVolume_Music(muted ? 0 : 0.35) is not ported.
void OnMusic() {
    g_musicMuted = Flip(g_musicMuted);
    // FileManager::OnMusicMute @0x16af80
    SystemFuncs::SetSetting_Int("MC_music_mute", 1 - SystemFuncs::GetSetting_Int("MC_music_mute", 0, "MyCountry_mute"),
                                "MyCountry_mute");
    UpdateContents();
}

// @0x33fa10. UNVERIFIED (milestone 5): SoundsManager::SetVolume_Sounds(muted ? 0 : 1) is not ported.
void OnSound() {
    g_soundMuted = Flip(g_soundMuted);
    // FileManager::OnSoundMute @0x16afc0
    SystemFuncs::SetSetting_Int("MC_sound_mute", 1 - SystemFuncs::GetSetting_Int("MC_sound_mute", 0, "MyCountry_mute"),
                                "MyCountry_mute");
    UpdateContents();
}

// @0x33f994: FileManager::OnNotifications (Java) and MyNeighbours::OnLoaded are online/platform
// calls with nothing to do offline.
void OnNotifications() {
    g_notifications = Flip(g_notifications);
    UpdateContents();
}

// The remaining buttons open online features or windows that are not ported. Each keeps the
// original's Hide() so the dialog closes as it did. UNVERIFIED (later milestones / online):
void OnFAQ() { Hide(); }               // @0x33e238 then SystemFuncs::OpenFAQ (web page)
void OnNews() { Hide(); }              // @0x33e35c then HelpWindow::Show(0)
void OnSocialConnect() { Hide(); }     // @0x33e338 then SocialConnectWindow::Show
void OnCoupon() { Hide(); }            // @0x33e228 CouponEntryWindow::Show, then Hide
void OnSpecial() { Hide(); }           // @0x33e3a4 then HUDWindow::ShowCheatWindow
void OnGPGS() {}                       // @0x33e17c Google Play Games sign-in / achievements
void OnPP() {}                         // @0x33e990 ToS::Show(1, ...) (privacy policy web view)
void OnTOS() {}                        // @0x33e88c ToS::Show(0, ...) (terms web view)
void OnSync() {}                       // @0x33e248 server save (needs the player's city and the server)
void OnProfiles() {}                   // @0x33e794 SwitchProfilesWindow / a popup

}  // namespace

WindowManager::FunctionalWindow* Queue() {
    if (!g_queue) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.deinit = Deinit;
        f.click = Click;
        f.setZ = SetZ;
        f.hide = Hide;
        g_queue = new WindowManager::FunctionalWindow("SettingsWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_queue && g_queue->shown; }
int GetSoundMutedState() { return g_soundMuted; }
void SetSoundMutedState(int muted) { g_soundMuted = muted; }
int GetMusicMutedState() { return g_musicMuted; }
void SetMusicMutedState(int muted) { g_musicMuted = muted; }
int GetNotificationState() { return g_notifications; }
void SetNotificationState(int on) { g_notifications = on; }

// @0x33fae0
void Init() {
    g_tablet = GUI::IsTabletVersion();
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Window_settings.xml", "Window_settings.png", -1.f,
                             0, 0, 0, 0, false, 1.f);
    g_queue->RegisterTopWindow(g_root);
    g_scale = g_root->scale;
    g_root->SetVisibility(false);
    CenterOnScreen();

    g_terms1 = GUI::GetWindowTyped<Window>(g_root, "button_settings_terms_01");
    g_terms1->SetOnClick(OnPP);
    g_terms1Text = GUI::GetWindowTyped<Textfield>(g_root, "button_settings_terms_01.text_blue");
    g_terms1Text->SetText(StringTable::GetString("BUTTON_PP"));
    g_terms2 = GUI::GetWindowTyped<Window>(g_root, "button_settings_terms_02");
    g_terms2->SetOnClick(OnTOS);
    g_terms2Text = GUI::GetWindowTyped<Textfield>(g_root, "button_settings_terms_02.text_blue");
    g_terms2Text->SetText(StringTable::GetString("BUTTON_TOS"));

    g_close = GUI::GetWindowTyped<Button>(g_root, "x_button.clickArea");
    g_close->SetOnClick(Hide);
    g_tween = GUI::CreateTweenEffect(g_root, nullptr, OnTweenIn, OnTweenOut);
    GUI::GetWindowTyped<Window>(g_root, "header")->SetOnClick(OnSpecial);
    g_title = GUI::GetWindowTyped<Textfield>(g_root, "text_title");
    char name[64];
    for (int i = 1; i <= 10; ++i) {
        std::snprintf(name, sizeof name, "button_%02d", i);
        Btn(i).LoadFrom(g_root, name);
    }
    g_id = GUI::GetWindowTyped<Textfield>(g_root, "text_id");

    Btn(1).holder->SetOnClick(OnGPGS);
    Btn(1).screenshot->SetVisibility(false);
    Btn(2).holder->SetOnClick(OnNews);
    Btn(3).holder->SetOnClick(OnMusic);
    Btn(4).holder->SetOnClick(OnSound);
    Btn(5).holder->SetOnClick(OnSocialConnect);
    Btn(6).holder->SetOnClick(OnFAQ);
    Btn(7).holder->SetOnClick(OnNotifications);
    Btn(9).holder->SetOnClick(OnSync);
    Btn(10).holder->SetOnClick(OnProfiles);
    Btn(8).holder->SetOnClick(OnCoupon);

    g_queue->hideOnOuterClick = true;
    g_queue->fn.zRange = WindowManager::ReturnSmallZRange;
    g_queue->fn.back = OnBack;
    g_queue->fn.show = Show;
    g_queue->fn.update = Update;
}

// @0x33e4bc
void Deinit() {
    GUI::RemoveTweenEffect(g_tween);
    g_tween = nullptr;
    delete g_root;
    g_root = nullptr;
}

// @0x33fa5c
void Show() {
    g_queue->shown = true;
    g_queue->MoveWindowOnTop(true);
    if (!g_root->visibleSelf) {
        WindowManager::WindowShow(false);
        g_tween->AnimateIn();
    }
    g_root->SetVisibility(true);
    UpdateContents();
    g_root->PreloadTextures();   // GUI::PreloadWindow @0x178828 (its load-time compensation is moot)
    GameState::RaiseGamePauseState();
}

// @0x33e1dc
void Hide() {
    if (!g_root->visibleSelf) return;
    g_tween->AnimateOut();
}

void EnableSync(bool on) {   // @0x33f944
    g_syncEnabled = on;
    UpdateContents();
}

// @0x33ea94
void UpdateContents() {
    g_title->SetText(StringTable::GetString("SETTINGS_HEADER"));
    if (SystemFuncs::signedInToGPGS()) {
        Btn(1).text->SetText(U"Achievements");
        Btn(1).gpgsLogin->SetVisibility(false);
        Btn(1).gpgsAchievements->SetVisibility(true);
    } else {
        Btn(1).text->SetText(U"Sign In");
        Btn(1).gpgsLogin->SetVisibility(true);
        Btn(1).gpgsAchievements->SetVisibility(false);
    }
    Btn(2).text->SetText(StringTable::GetString("SETTINGS_NEWS"));
    Btn(3).text->SetText(StringTable::GetString("SETTINGS_MUSIC"));
    Btn(4).text->SetText(StringTable::GetString("SETTINGS_SOUND"));
    Btn(5).text->SetText(StringTable::GetString("SOCIAL_CONNECT_HEADER"));
    Btn(6).text->SetText(StringTable::GetString("SETTINGS_FAQ"));
    Btn(7).text->SetText(StringTable::GetString("SETTINGS_NOTIFICATION"));
    Btn(9).text->SetText(StringTable::GetString("SETTINGS_SYNC"));
    Btn(10).text->SetText(StringTable::GetString("SETTINGS_PROFILES"));
    Btn(3).music->SetVisibility(Flip(g_musicMuted) != 0);
    Btn(3).musicOff->SetVisibility(g_musicMuted == 1);
    Btn(4).sound->SetVisibility(Flip(g_soundMuted) != 0);
    Btn(4).soundOff->SetVisibility(g_soundMuted == 1);
    Btn(5).holder->SetEnabled(!GameState::IsCityTutorial());
    Btn(7).notifications->SetVisibility(g_notifications == 1);
    Btn(7).notificationsOff->SetVisibility(Flip(g_notifications) != 0);
    Btn(8).text->SetText(StringTable::GetString("WEBVIEW_COUPONS"));
    Btn(8).holder->SetEnabled(true);

    // The player's id: L"%s %S" of WEBVIEW_BLOG_MOBILE_YOURID and the device GUID. UNVERIFIED
    // (online): when signed in to a social network with a "fb_displayname" setting, that name is
    // shown instead; SocNets is not ported, so the port always takes the GUID path.
    std::u32string id = StringTable::GetString("WEBVIEW_BLOG_MOBILE_YOURID");
    id += U' ';
    for (char c : SystemFuncs::getGUID()) id += (char32_t)(unsigned char)c;
    g_id->SetText(id.c_str());

    Btn(9).holder->SetEnabled(g_syncEnabled && GameState::SecondTutorialStep() == 0x100 && GameState::IsPlayerCity());
    // "last_sync_time" is only written by a server save. UNVERIFIED (online): the "SETTINGS_SYNC_LAST"
    // text with StringTable::GetTimeString(now - last) is not ported.
    if (SystemFuncs::GetSetting_Int("last_sync_time", 0) == 0)
        Btn(9).lastSync->SetText(StringTable::GetString("SETTINGS_SYNC_NEVER"));
    // NeighboursServer::IsRegistered / PushEnabledToggle (online) are skipped.
    // PlayerProfileManager::FBEnabled is false offline: the sync and profile buttons show as the
    // server's settings say (both default to hidden).
    int sync = SystemFuncs::GetSetting_Int("syncbutton_enabled", 0);
    Btn(9).text->SetVisibility(sync != 0);
    Btn(9).holder->SetVisibility(sync != 0);
    int profile = SystemFuncs::GetSetting_Int("profilebutton_enabled", 0);
    Btn(10).text->SetVisibility(profile != 0);
    Btn(10).holder->SetVisibility(profile != 0);
}

}  // namespace SettingsWindow
