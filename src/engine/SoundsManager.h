// SoundsManager: the game's sound events (libkingdom.so 5.11 @0x221df0..0x224ad0). config/Sounds.xml
// names each event's files (<Sound event=...><File name= delay=/> and <Spec id=...> variants); a
// play picks one of them at random and plays "<prefix><name>.wav", else "<prefix><name><postfix>"
// (prefix "ogg/", postfix ".ogg" from main). The music is "<prefix><name><postfix>", cross-faded.
#pragma once

namespace SoundsManager {

void Init(const char* xml, const char* prefix, const char* postfix);   // @0x224a54
void Reload(const char* xml);                                          // @0x224930
void Destroy();                                                        // @0x222684
void Update(float dt);                                                 // @0x221ec8 delayed sounds / music
// @0x223584: the event's sound (variant id, else the default ones); with onlyIfNotPlaying it is not
// started again while it plays. A delayed variant waits in the queue.
bool PlaySound(const char* event, int id = -1, bool onlyIfNotPlaying = false);
bool PlaySoundDelayed(const char* event, int id, float delay);         // @0x224ad0
void PlayMusic(const char* name, bool fade);                           // @0x223e2c
bool IsMusicPlaying(const char* name);                                 // @0x223d94
bool LoadMusic(const char* name);                                      // @0x223cf0
void StopMusic();                                                      // @0x22205c
void StopSounds();                                                     // @0x222068
void PauseAudio(bool pause);                                           // @0x222074
bool LoadAllEventsSounds();                                            // @0x2222f8
void SetPrefixPostfix(const char* prefix, const char* postfix);       // @0x22316c
void SetMusicFadeDelays(int fadeIn, int fadeOut);                      // @0x221df0 (ms)
void ShowInfoMessages();                                               // @0x221e04
float GetVolume_Master();                                              // @0x22204c
void SetVolume_Master(float v, bool flag);                             // @0x222050
void SetVolume_Sounds(float v);                                        // @0x222054
void SetVolume_Music(float v);                                         // @0x222058

}  // namespace SoundsManager
