// Sounds: the low-level audio engine (libkingdom.so 5.11 @0x21ec34..0x2215fc): five sound channels
// and one music track, the loaded sounds and music by file name, the master / sound / music volumes.
// The original drives SDL_mixer (Mix_*) with libvorbis; PORT: the port has no SDL_mixer for SDL3, so
// a small mixer in Sounds.cpp stands in for the Mix_* calls (SDL3 audio streams, vorbisfile).
// SoundsManager (the sound events) is its only user.
#pragma once

namespace Sounds {

struct Chunk;
struct Music;

struct SoundInfo {   // 8 bytes
    const char* name = nullptr;   // +0x00 the file name (DuplicateString)
    Chunk* chunk = nullptr;       // +0x04 Mix_Chunk
};

struct MusicInfo {   // 0xc bytes
    const char* name = nullptr;   // +0x00
    Music* music = nullptr;       // +0x04 Mix_Music
    unsigned char* data = nullptr;  // +0x08 the file's bytes (Mix_LoadMUS_RW reads from them)
};

void Init();                                   // @0x21f44c
bool InitHW();                                 // @0x21f314
bool Destroy();                                // @0x21f934
bool RemoveAll();                              // @0x21f740
SoundInfo* LoadSound(const char* name);        // @0x2215fc
MusicInfo* LoadMusic(const char* name);        // @0x220fdc
SoundInfo* GetSound(const char* name);         // @0x21fbe4
MusicInfo* GetMusic(const char* name);         // @0x21f9f0
bool RemoveSound(const char* name);            // @0x21fdc8
bool RemoveMusic(const char* name);            // @0x220000
int PlaySound(SoundInfo* s);                   // @0x21f164: the channel, or -1
int PlaySound(const char* name);               // @0x21fd90
bool StopSound(int channel);                   // @0x21f0b8
bool IsSoundPlaying(SoundInfo* s);             // @0x21ec6c
int PlayMusic(MusicInfo* m, bool fade, int ms);   // @0x21f030
int PlayMusic(const char* name, bool fade, int ms);   // @0x21fb9c
bool StopMusic(bool fade, int ms);             // @0x21eff8
bool IsMusicPlaying(const char* name);         // @0x21efa0 (null: any)
bool StopAll(bool sounds, bool music);         // @0x21f0f0
void PauseAll(bool pause);                     // @0x21f258
float GetVolume_Master();                      // @0x21ecc4
void SetVolume_Master(float v, bool flag);     // @0x21ef28
void SetVolume_Sounds(float v);                // @0x21ee04
void SetVolume_Music(float v);                 // @0x21eeb8
bool IsLowQualityMode();                       // @0x21ee00
void onChannelFinished(int channel);           // @0x21ec34
void Update();                                 // @0x21ecd8 (empty; PORT: see Sounds.cpp)

}  // namespace Sounds
