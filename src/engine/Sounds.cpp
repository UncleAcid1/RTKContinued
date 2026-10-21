// Port of Sounds (libkingdom.so 5.11), see Sounds.h.
#include "engine/Sounds.h"

#include <SDL3/SDL.h>
#include <vorbis/vorbisfile.h>

#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "engine/FileManager.h"

namespace Sounds {

// ---------------------------------------------------------------------------------------------
// PORT: the Mix_* calls the original makes, over SDL3 audio streams. Each of the five channels is
// a stream bound to the device (SDL mixes them); a sound is decoded whole (Mix_LoadWAV_RW). The
// music is decoded as it plays (Mix_LoadMUS_RW keeps the file's bytes), looping, with Mix_FadeIn /
// Mix_FadeOutMusic's linear fades. Volumes are 0..128 as Mix_Volume / Mix_VolumeMusic.

struct Chunk {
    SDL_AudioSpec spec{};
    std::vector<uint8_t> pcm;   // signed 16-bit, interleaved
};

struct Music {
    OggVorbis_File vf{};
    SDL_AudioSpec spec{};
    const uint8_t* data = nullptr;
    size_t size = 0, pos = 0;   // the read position in data (vorbisfile's callbacks)
};

namespace {

constexpr int kChannels = 5;   // Mix_AllocateChannels(5)

SDL_AudioDeviceID g_device = 0;
SDL_AudioStream* g_channel[kChannels] = {};
SDL_AudioStream* g_musicStream = nullptr;

// The playing music (the audio thread reads these with the stream locked).
Music* g_playing = nullptr;
int g_fadeInMs = 0, g_fadeOutMs = 0;   // the fade's length (0: none)
double g_fadeDone = 0.0;               // ms of the current fade played so far
bool g_fadingOut = false;

size_t OvRead(void* ptr, size_t size, size_t n, void* src) {
    Music* m = static_cast<Music*>(src);
    size_t want = size * n, left = m->size - m->pos;
    if (want > left) want = left;
    std::memcpy(ptr, m->data + m->pos, want);
    m->pos += want;
    return size ? want / size : 0;
}

int OvSeek(void* src, ogg_int64_t off, int whence) {
    Music* m = static_cast<Music*>(src);
    ogg_int64_t p = whence == SEEK_SET ? off : whence == SEEK_CUR ? (ogg_int64_t)m->pos + off : (ogg_int64_t)m->size + off;
    if (p < 0 || p > (ogg_int64_t)m->size) return -1;
    m->pos = (size_t)p;
    return 0;
}

long OvTell(void* src) { return (long)static_cast<Music*>(src)->pos; }

const ov_callbacks kOvCallbacks = {OvRead, OvSeek, nullptr, OvTell};

// The music stream asks for more: decode (looping at the end), apply the fade.
void SDLCALL MusicCallback(void*, SDL_AudioStream* stream, int additional, int) {
    Music* m = g_playing;
    if (!m) return;
    std::vector<uint8_t> buf((size_t)additional);
    size_t got = 0;
    while (got < buf.size()) {
        int section = 0;
        long n = ov_read(&m->vf, (char*)buf.data() + got, (int)(buf.size() - got), 0, 2, 1, &section);
        if (n == 0) {   // the end: loops = -1, from the start again
            if (ov_raw_seek(&m->vf, 0) != 0) break;
            continue;
        }
        if (n < 0) break;
        got += (size_t)n;
    }
    int16_t* s = reinterpret_cast<int16_t*>(buf.data());
    const size_t frames = got / 2 / (size_t)m->spec.channels;
    const double msPerFrame = 1000.0 / m->spec.freq;
    const int fadeMs = g_fadingOut ? g_fadeOutMs : g_fadeInMs;
    for (size_t f = 0; f < frames && fadeMs > 0; ++f) {
        double t = g_fadeDone / fadeMs;
        if (t > 1.0) t = 1.0;
        const double gain = g_fadingOut ? 1.0 - t : t;
        for (int c = 0; c < m->spec.channels; ++c) {
            int16_t& v = s[f * (size_t)m->spec.channels + (size_t)c];
            v = (int16_t)(v * gain);
        }
        g_fadeDone += msPerFrame;
    }
    if (fadeMs > 0 && g_fadeDone >= fadeMs) {
        if (g_fadingOut) {   // the fade-out ends the music (Mix_HaltMusic)
            g_playing = nullptr;
            g_fadingOut = false;
        }
        g_fadeInMs = g_fadeOutMs = 0;
    }
    SDL_PutAudioStreamData(stream, buf.data(), (int)got);
}

void Mix_Volume(int ch, int v) { SDL_SetAudioStreamGain(g_channel[ch], (float)v / 128.f); }
void Mix_VolumeMusic(int v) { if (g_musicStream) SDL_SetAudioStreamGain(g_musicStream, (float)v / 128.f); }

bool ChannelBusy(int ch) {
    return g_channel[ch] && SDL_GetAudioStreamQueued(g_channel[ch]) + SDL_GetAudioStreamAvailable(g_channel[ch]) > 0;
}

void Mix_HaltChannel(int ch) { if (g_channel[ch]) SDL_ClearAudioStream(g_channel[ch]); }

// Mix_PlayChannelTimed(-1, chunk, 0, -1): the first free channel.
int Mix_PlayChannel(Chunk* c) {
    for (int ch = 0; ch < kChannels; ++ch) {
        if (!g_channel[ch] || ChannelBusy(ch)) continue;
        SDL_ClearAudioStream(g_channel[ch]);
        SDL_SetAudioStreamFormat(g_channel[ch], &c->spec, nullptr);
        SDL_PutAudioStreamData(g_channel[ch], c->pcm.data(), (int)c->pcm.size());
        SDL_FlushAudioStream(g_channel[ch]);
        return ch;
    }
    return -1;
}

bool Mix_PlayingMusic() {
    if (!g_musicStream) return false;
    SDL_LockAudioStream(g_musicStream);
    bool playing = g_playing != nullptr;
    SDL_UnlockAudioStream(g_musicStream);
    return playing;
}

void Mix_HaltMusic() {
    if (!g_musicStream) return;
    SDL_LockAudioStream(g_musicStream);
    g_playing = nullptr;
    g_fadingOut = false;
    g_fadeInMs = g_fadeOutMs = 0;
    SDL_UnlockAudioStream(g_musicStream);
    SDL_ClearAudioStream(g_musicStream);
}

void Mix_FadeOutMusic(int ms) {
    if (!g_musicStream) return;
    if (ms <= 0) { Mix_HaltMusic(); return; }
    SDL_LockAudioStream(g_musicStream);
    if (g_playing && !g_fadingOut) {
        g_fadingOut = true;
        g_fadeOutMs = ms;
        g_fadeInMs = 0;
        g_fadeDone = 0.0;
    }
    SDL_UnlockAudioStream(g_musicStream);
}

int Mix_FadeInMusic(Music* m, int ms) {
    if (!g_musicStream) return -1;
    Mix_HaltMusic();
    SDL_LockAudioStream(g_musicStream);
    ov_raw_seek(&m->vf, 0);
    SDL_SetAudioStreamFormat(g_musicStream, &m->spec, nullptr);
    g_playing = m;
    g_fadingOut = false;
    g_fadeInMs = ms;
    g_fadeOutMs = 0;
    g_fadeDone = 0.0;
    SDL_UnlockAudioStream(g_musicStream);
    return 0;
}

Chunk* Mix_LoadWAV(const uint8_t* data, uint32_t size) {
    Music src;
    src.data = data;
    src.size = size;
    OggVorbis_File vf;
    if (ov_open_callbacks(&src, &vf, nullptr, 0, kOvCallbacks) != 0) {
        SDL_SetError("Unrecognized audio format");
        return nullptr;
    }
    vorbis_info* vi = ov_info(&vf, -1);
    Chunk* c = new Chunk;
    c->spec = {SDL_AUDIO_S16LE, vi->channels, (int)vi->rate};
    char buf[4096];
    for (;;) {
        int section = 0;
        long n = ov_read(&vf, buf, sizeof buf, 0, 2, 1, &section);
        if (n <= 0) break;
        c->pcm.insert(c->pcm.end(), buf, buf + n);
    }
    ov_clear(&vf);
    return c;
}

Music* Mix_LoadMUS(const uint8_t* data, uint32_t size) {
    Music* m = new Music;
    m->data = data;
    m->size = size;
    if (ov_open_callbacks(m, &m->vf, nullptr, 0, kOvCallbacks) != 0) {
        delete m;
        SDL_SetError("Unrecognized audio format");
        return nullptr;
    }
    vorbis_info* vi = ov_info(&m->vf, -1);
    m->spec = {SDL_AUDIO_S16LE, vi->channels, (int)vi->rate};
    return m;
}

void Mix_FreeMusic(Music* m) {
    if (!m) return;
    if (g_musicStream) {
        SDL_LockAudioStream(g_musicStream);
        if (g_playing == m) g_playing = nullptr;
        SDL_UnlockAudioStream(g_musicStream);
    }
    ov_clear(&m->vf);
    delete m;
}

void Mix_CloseAudio() {
    for (SDL_AudioStream*& s : g_channel) {
        SDL_DestroyAudioStream(s);
        s = nullptr;
    }
    SDL_DestroyAudioStream(g_musicStream);
    g_musicStream = nullptr;
    g_playing = nullptr;
    if (g_device) SDL_CloseAudioDevice(g_device);
    g_device = 0;
}

// ---------------------------------------------------------------------------------------------

SoundInfo* g_channelSound[kChannels] = {};   // 0x61558c the sound each channel plays
float g_volumeMaster = 1.f;                  // 0x6155a4
float g_volumeSounds = 1.f;                  // 0x6155e4
float g_volumeMusic = 1.f;                   // 0x6155e8
MusicInfo* g_music = nullptr;                // 0x6155ec the music last started
std::map<std::string, SoundInfo*> g_sounds;  // 0x6155a8
std::map<std::string, MusicInfo*> g_musics;  // 0x6155c0
// @0x21f49c (a pool of 0x400-byte blocks on the original; RemoveAll resets it)
const char* DuplicateString(const char* s) {
    static std::vector<std::unique_ptr<std::string>> pool;
    pool.push_back(std::make_unique<std::string>(s));
    return pool.back()->c_str();
}

// PORT: SDL_mixer calls channelFinished from its thread when a channel ends; here the drained
// streams are noticed when the sounds are next looked at.
void PollChannels() {
    for (int ch = 0; ch < kChannels; ++ch)
        if (g_channelSound[ch] && !ChannelBusy(ch)) onChannelFinished(ch);
}

uint8_t* GetFileData(const char* name, uint32_t* size) {   // @0x21edc4
    uint32_t n = 0;
    uint8_t* d = FileManager::LoadFile(name, n);
    if (size) *size = n;
    return d;
}

}  // namespace

void onChannelFinished(int channel) {
    if ((unsigned)channel < 6) g_channelSound[channel] = nullptr;
}

bool IsSoundPlaying(SoundInfo* s) {
    PollChannels();
    for (SoundInfo* c : g_channelSound)
        if (c == s) return true;
    return false;
}

float GetVolume_Master() { return g_volumeMaster; }

void Update() { PollChannels(); }   // (empty on the original, see PollChannels)

// PORT: Java's isLowPerformanceDevice (the "low/" sound folder) is false on the Mac.
bool IsLowQualityMode() { return false; }

void SetVolume_Sounds(float v) {
    g_volumeSounds = v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
    for (int ch = 0; ch < kChannels; ++ch)
        if (g_channelSound[ch]) Mix_Volume(ch, (int)(g_volumeSounds * g_volumeMaster * 128.f));
}

void SetVolume_Music(float v) {
    g_volumeMusic = v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
    Mix_VolumeMusic((int)(g_volumeMusic * g_volumeMaster * 128.f));
}

// PORT: FileManager::SoundVolume @0x16c1d0 (Java: the device's media volume, stepped by the volume keys)
// gives the volume back unchanged.
void SetVolume_Master(float v, bool) {
    g_volumeMaster = v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
    SetVolume_Sounds(g_volumeSounds);
    SetVolume_Music(g_volumeMusic);
}

bool IsMusicPlaying(const char* name) {
    if (!Mix_PlayingMusic()) return false;
    if (!name) return true;
    return g_music && std::strcmp(g_music->name, name) == 0;
}

bool StopMusic(bool fade, int ms) {
    if (fade) Mix_FadeOutMusic(ms);
    else Mix_HaltMusic();
    g_music = nullptr;
    return true;
}

// Loops forever (-1); the volume is set again after the start.
int PlayMusic(MusicInfo* m, bool fade, int ms) {
    if (!m) return -1;
    int r = Mix_FadeInMusic(m->music, fade ? ms : 0);   // (Mix_PlayMusic: no fade)
    if (r == -1) return r;
    Mix_VolumeMusic((int)(g_volumeMusic * g_volumeMaster * 128.f));
    g_music = m;
    return r;
}

bool StopSound(int channel) {
    if (!g_channelSound[channel]) return false;
    Mix_HaltChannel(channel);
    g_channelSound[channel] = nullptr;
    return true;
}

bool StopAll(bool sounds, bool music) {
    if (music) {
        Mix_HaltMusic();
        g_music = nullptr;
    }
    if (sounds) {
        for (int ch = 0; ch < kChannels; ++ch) {
            if (g_channelSound[ch]) Mix_HaltChannel(ch);
            g_channelSound[ch] = nullptr;
        }
    }
    return true;
}

// Refused when all five slots count as busy: a slot counts when its channel plays or, for the
// first, when music was started (the original tests the five words from 0x6155ec, the current
// music and four that only Init clears, next to the channel table).
int PlaySound(SoundInfo* s) {
    PollChannels();
    int busy = 0;
    for (int i = 0; i < kChannels; ++i)
        if ((i == 0 && g_music) || g_channelSound[i]) ++busy;
    if (busy == kChannels) return -1;
    if (!s || !s->chunk) {
        std::puts("CSounds::PlaySound() Cannot play sound, handle is invalid");
        return -1;
    }
    int ch = Mix_PlayChannel(s->chunk);
    if (ch == -1) {
        std::printf("CSounds::PlaySound() Cannot play sound %s, no free channels -1\n", s->name);
        return -1;
    }
    Mix_Volume(ch, (int)(g_volumeSounds * g_volumeMaster * 128.f));
    g_channelSound[ch] = s;
    return ch;
}

void PauseAll(bool pause) {
    if (!g_device) return;
    if (pause) SDL_PauseAudioDevice(g_device);
    else SDL_ResumeAudioDevice(g_device);
}

// 44.1 kHz, 16-bit stereo, five channels (Mix_OpenAudio(0xac44, 0x8010, 2, 0x1000)).
bool InitHW() {
    if (!SDL_WasInit(SDL_INIT_AUDIO) && !SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        std::printf("CSounds::Init() %s\n", SDL_GetError());
        return false;
    }
    const SDL_AudioSpec spec = {SDL_AUDIO_S16LE, 2, 44100};
    g_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
    if (!g_device) {
        std::printf("CSounds::Init() %s\n", SDL_GetError());
        return false;
    }
    for (SDL_AudioStream*& s : g_channel) {
        s = SDL_CreateAudioStream(&spec, &spec);
        SDL_BindAudioStream(g_device, s);
    }
    g_musicStream = SDL_CreateAudioStream(&spec, &spec);
    SDL_SetAudioStreamGetCallback(g_musicStream, MusicCallback, nullptr);
    SDL_BindAudioStream(g_device, g_musicStream);
    return true;
}

void Init() {
    g_volumeMaster = g_volumeSounds = g_volumeMusic = 1.f;
    for (SoundInfo*& c : g_channelSound) c = nullptr;
    g_music = nullptr;
    InitHW();
}

bool RemoveAll() {
    for (auto& [name, s] : g_sounds) {
        delete s->chunk;
        delete s;
    }
    g_sounds.clear();
    for (auto& [name, m] : g_musics) {
        Mix_FreeMusic(m->music);
        delete[] m->data;
        delete m;
    }
    g_musics.clear();
    return true;
}

bool Destroy() {
    StopAll(true, true);
    RemoveAll();
    Mix_CloseAudio();
    return true;
}

MusicInfo* GetMusic(const char* name) {
    auto it = g_musics.find(name);
    return it != g_musics.end() ? it->second : nullptr;
}

SoundInfo* GetSound(const char* name) {
    auto it = g_sounds.find(name);
    return it != g_sounds.end() ? it->second : nullptr;
}

int PlayMusic(const char* name, bool fade, int ms) {
    if (MusicInfo* m = GetMusic(name)) return PlayMusic(m, fade, ms);
    std::printf("CSounds::PlayMusic() Cannot play music %s, GetMusic returned NULL\n", name);
    return -1;
}

int PlaySound(const char* name) {
    if (SoundInfo* s = GetSound(name)) return PlaySound(s);
    std::printf("CSounds::PlaySound() Cannot play sound %s, GetSound returned NULL\n", name);
    return 0;
}

bool RemoveSound(const char* name) {
    auto it = g_sounds.find(name);
    if (it == g_sounds.end()) return false;
    delete it->second->chunk;
    delete it->second;
    g_sounds.erase(it);
    return true;
}

bool RemoveMusic(const char* name) {
    auto it = g_musics.find(name);
    if (it == g_musics.end()) return false;
    Mix_FreeMusic(it->second->music);
    delete[] it->second->data;
    delete it->second;
    g_musics.erase(it);
    return true;
}

// The file's bytes stay with the music (it is decoded as it plays).
MusicInfo* LoadMusic(const char* name) {
    if (MusicInfo* m = GetMusic(name)) return m;
    uint32_t size = 0;
    uint8_t* data = GetFileData(name, &size);
    if (!data) {
        std::printf("CSounds::LoadMusic() GetFileData returned NULL, fn=%s\n", name);
        return nullptr;
    }
    Music* music = Mix_LoadMUS(data, size);
    if (!music) {
        std::printf("CSounds::LoadMusic() %s\n", SDL_GetError());
        delete[] data;   // (the original keeps it)
        return nullptr;
    }
    MusicInfo* m = new MusicInfo{DuplicateString(name), music, data};
    g_musics[name] = m;
    return m;
}

// A sound is decoded at once and the file's bytes freed. (A missing file is quiet: GetFileData
// loads quietly.)
SoundInfo* LoadSound(const char* name) {
    if (SoundInfo* s = GetSound(name)) return s;
    uint32_t size = 0;
    uint8_t* data = GetFileData(name, &size);
    if (!data) return nullptr;
    Chunk* c = Mix_LoadWAV(data, size);
    delete[] data;
    if (!c) {
        std::printf("CSounds::LoadSound() %s, fn=%s\n", SDL_GetError(), name);
        return nullptr;
    }
    std::printf("CSounds::LoadSound() SUCCEED, fn=%s\n", name);
    SoundInfo* s = new SoundInfo{DuplicateString(name), c};
    g_sounds[name] = s;
    return s;
}

}  // namespace Sounds
