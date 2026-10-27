// Port of SoundsManager (libkingdom.so 5.11), see SoundsManager.h.
#include "engine/SoundsManager.h"

#include "engine/ErrorReporter.h"

#include <pugixml.hpp>

#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "engine/FileManager.h"
#include "engine/Sounds.h"
#include "game/Rand48.h"

namespace SoundsManager {

namespace {

struct SoundEventInfo {   // 8 bytes
    std::string name;     // +0x00
    float delay = 0.f;    // +0x04 seconds (the XML's ms / 1000)
};

// CSoundEvent (0x18 bytes): the files per variant id (-1: the default ones).
struct CSoundEvent {
    std::map<int, std::vector<SoundEventInfo>> names;

    // @0x221e5c
    std::vector<SoundEventInfo>* GetNames(int id) {
        auto it = names.find(id);
        return it != names.end() ? &it->second : nullptr;
    }

    // @0x2231b4
    void AddFileName(int id, const char* name, float delay) { names[id].push_back({name, delay}); }

    // @0x222870: one of the variant's files at random (lrand48), else the default ones; none: "".
    SoundEventInfo GetInfoForEvent(int id) {
        std::vector<SoundEventInfo>* v = GetNames(id);
        if (!v) return id != -1 ? GetInfoForEvent(-1) : SoundEventInfo{};
        if (v->empty()) return {};
        return (*v)[(unsigned long)Rand48::lrand48() % v->size()];
    }

    // @0x22335c: <File name= delay=> (the default ones), <Spec id= delay=><File name=/></Spec>.
    void LoadFromXML(pugi::xml_node n) {
        for (pugi::xml_node f = n.child("File"); f; f = f.next_sibling("File"))
            AddFileName(-1, f.attribute("name").value(), f.attribute("delay").as_float() / 1000.f);
        for (pugi::xml_node s = n.child("Spec"); s; s = s.next_sibling("Spec")) {
            int id = s.attribute("id").as_int();
            for (pugi::xml_node f = s.child("File"); f; f = f.next_sibling("File"))
                AddFileName(id, f.attribute("name").value(), s.attribute("delay").as_float() / 1000.f);
        }
    }

    // @0x2221bc
    void LoadSounds(const char* prefix, const char* postfix) {
        char path[256];
        for (auto& [id, v] : names) {
            for (SoundEventInfo& e : v) {
                std::snprintf(path, sizeof path, "%s%s%s", prefix, e.name.c_str(), postfix);
                Sounds::LoadSound(path);
            }
        }
    }
};

std::map<std::string, CSoundEvent> g_events;   // 0x61562c
std::string g_prefix, g_postfix;               // 0x615644 / 0x61565c
std::string g_latestMusicPlayed;               // SoundsManager::mLatestMusicPlayed
int g_fadeIn = 1000, g_fadeOut = 1000;         // 0x60f064 / 0x60f068 (ms)
int g_musicChannel = -1;                       // 0x60f06c what the last music start returned
bool g_showInfo = false;                       // 0x615610 (the ErrorReporter lines below)
bool g_inFallback = false;                     // 0x615674

struct DelayedSound { Sounds::SoundInfo* sound; float delay; };   // 0x615614
std::vector<DelayedSound> g_delayedSounds;
struct DelayedMusic { Sounds::MusicInfo* music; float delay; float fadeIn; };   // 0x615620
std::vector<DelayedMusic> g_delayedMusic;

// @0x223a60: "<prefix>[low/]<name><postfix>".
std::string GetMusicFileName(const char* name) {
    std::string low = Sounds::IsLowQualityMode() ? "low/" : "";
    return g_prefix + low + name + g_postfix;
}

// PlaySound / PlaySoundDelayed: the event's file, "<prefix>[low/]<name>.wav" before the postfix.
Sounds::SoundInfo* FindSound(const char* event, int id, float* delay) {
    auto it = g_events.find(event);
    if (it == g_events.end()) {
        std::printf("SoundsManager::PlaySound() Failed to find a sound event for %s[%d]\n", event, id);
        return nullptr;
    }
    SoundEventInfo info = it->second.GetInfoForEvent(id);
    if (delay) *delay = info.delay;
    const char* low = Sounds::IsLowQualityMode() ? "low/" : "";
    char path[256];
    std::snprintf(path, sizeof path, "%s%s%s%s", g_prefix.c_str(), low, info.name.c_str(), ".wav");
    Sounds::SoundInfo* s = Sounds::LoadSound(path);
    if (!s) {
        std::snprintf(path, sizeof path, "%s%s%s%s", g_prefix.c_str(), low, info.name.c_str(), g_postfix.c_str());
        s = Sounds::LoadSound(path);
    }
    if (!s) std::printf("SoundsManager::PlaySound() Failed to find a sound file for event %s[%d]\n", event, id);
    return s;
}

// @0x2244c0: <Sound><Sound event=...>...</Sound>...</Sound>; an event already known keeps its first
// definition.
bool LoadFromXML(const char* xml) {
    uint32_t size = 0;
    uint8_t* data = FileManager::LoadFile(xml, size);
    if (!data) return false;
    pugi::xml_document doc;
    doc.load_buffer_inplace(data, size, 0x74);
    for (pugi::xml_node n = doc.child("Sound").child("Sound"); n; n = n.next_sibling("Sound")) {
        CSoundEvent e;
        e.LoadFromXML(n);
        g_events.emplace(n.attribute("event").value(), std::move(e));
    }
    delete[] data;
    return true;
}

}  // namespace

void SetMusicFadeDelays(int fadeIn, int fadeOut) {
    g_fadeIn = fadeIn;
    g_fadeOut = fadeOut;
}

void ShowInfoMessages() { g_showInfo = true; }

// Queued sounds and music count down and start (each removed by moving the last one into its place).
void Update(float dt) {
    for (size_t i = 0; i < g_delayedSounds.size();) {
        g_delayedSounds[i].delay -= dt;
        if (g_delayedSounds[i].delay >= 0.f) { ++i; continue; }
        Sounds::PlaySound(g_delayedSounds[i].sound);
        g_delayedSounds[i] = g_delayedSounds.back();
        g_delayedSounds.pop_back();
    }
    for (size_t i = 0; i < g_delayedMusic.size();) {
        g_delayedMusic[i].delay -= dt;
        if (g_delayedMusic[i].delay >= 0.f) { ++i; continue; }
        std::printf("SoundsManager::Update() Playing delayed music with %fms fade in\n", (double)g_delayedMusic[i].fadeIn);
        g_musicChannel = Sounds::PlayMusic(g_delayedMusic[i].music, true, (int)g_delayedMusic[i].fadeIn);
        g_delayedMusic[i] = g_delayedMusic.back();
        g_delayedMusic.pop_back();
    }
    Sounds::Update();   // PORT: notices the channels that ended (Sounds.cpp)
}

float GetVolume_Master() { return Sounds::GetVolume_Master(); }
void SetVolume_Master(float v, bool flag) { Sounds::SetVolume_Master(v, flag); }
void SetVolume_Sounds(float v) { Sounds::SetVolume_Sounds(v); }
void SetVolume_Music(float v) { Sounds::SetVolume_Music(v); }
void StopMusic() { Sounds::StopAll(false, true); }
void StopSounds() { Sounds::StopAll(true, false); }

void PauseAudio(bool pause) {
    if (pause) Sounds::StopAll(true, true);
    Sounds::PauseAll(pause);
}

bool LoadAllEventsSounds() {
    for (auto& [name, e] : g_events) e.LoadSounds(g_prefix.c_str(), g_postfix.c_str());
    return true;
}

void SetPrefixPostfix(const char* prefix, const char* postfix) {
    g_prefix = prefix;
    g_postfix = postfix;
}

bool PlaySound(const char* event, int id, bool onlyIfNotPlaying) {
    float delay = 0.f;
    Sounds::SoundInfo* s = FindSound(event, id, &delay);
    if (!s) return false;
    if (delay > 0.f) {
        g_delayedSounds.push_back({s, delay});
        if (g_showInfo) ErrorReporter::Printf(1.f, 1.f, 1.f, "SoundsManager::PlaySound(%s, %d) Playing with delay %f\n", event, id, (double)delay);
    } else if (!onlyIfNotPlaying || !Sounds::IsSoundPlaying(s)) {
        Sounds::PlaySound(s);
    }
    return true;
}

bool PlaySoundDelayed(const char* event, int id, float delay) {
    Sounds::SoundInfo* s = FindSound(event, id, nullptr);
    if (!s) return false;
    if (delay <= 0.f) Sounds::PlaySound(s);
    else g_delayedSounds.push_back({s, delay});
    if (g_showInfo) ErrorReporter::Printf(1.f, 1.f, 1.f, "SoundsManager::PlaySound(%s, %d) Playing with delay %f\n", event, id, (double)delay);
    return true;
}

bool LoadMusic(const char* name) { return Sounds::LoadMusic(GetMusicFileName(name).c_str()) != nullptr; }

bool IsMusicPlaying(const char* name) { return Sounds::IsMusicPlaying(GetMusicFileName(name).c_str()); }

// A missing file falls back to the city music (once, not while it plays). With fade, while music
// plays, the old one fades out and the new one waits (fade-out / 1000 + 0.125 s) to fade in.
void PlayMusic(const char* name, bool fade) {
    std::string file = GetMusicFileName(name);
    g_latestMusicPlayed = file;
    Sounds::MusicInfo* m = Sounds::LoadMusic(file.c_str());
    if (!m) {
        if (!IsMusicPlaying("music_city")) {
            if (!g_inFallback) {
                g_inFallback = true;
                PlayMusic("music_city", false);
            }
            g_inFallback = false;
        }
        return;
    }
    if (g_musicChannel != -1) Sounds::StopMusic(fade, g_fadeOut);
    if (fade && g_musicChannel != -1) {
        g_delayedMusic.push_back({m, (float)g_fadeOut / 1000.f + 0.125f, (float)g_fadeIn});
    } else {
        g_musicChannel = Sounds::PlayMusic(file.c_str(), fade, g_fadeIn);
    }
    if (g_showInfo) ErrorReporter::Printf(1.f, 1.f, 1.f, "SoundsManager::PlayMusic(%s, %d)\n", file.c_str(), g_fadeIn);
}

void Init(const char* xml, const char* prefix, const char* postfix) {
    g_prefix = prefix;
    g_postfix = postfix;
    g_fadeOut = 1000;
    g_musicChannel = -1;
    g_fadeIn = 1000;
    Sounds::Init();
    LoadFromXML(xml);
}

void Reload(const char* xml) {
    g_events.clear();
    Sounds::Destroy();
    Sounds::Init();
    LoadFromXML(xml);
}

// @0x222684: the queues and the events (the sounds stay loaded).
void Destroy() {
    g_delayedMusic.clear();
    g_delayedSounds.clear();
    g_events.clear();
}

}  // namespace SoundsManager
