// IntroVideo: the publisher's logo film shown when the app starts, the port's stand-in for the Java
// side's ShowVideoSplash (SDLActivity.onCreate plays assets/LOGO.mp4 over the whole screen, a tap
// ends it, and only then does SDLMain call nativeInit, the game's start). macOS: AVFoundation over
// the game's window. Other systems: not yet (nothing is shown).
#pragma once
#include <string>

struct SDL_Window;

namespace IntroVideo {

// Plays the film at `path` in `window` until it ends, a click or a key; false when the window was
// closed meanwhile (the game then quits).
bool Play(SDL_Window* window, const std::string& path);

}  // namespace IntroVideo
