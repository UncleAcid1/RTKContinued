#include "game/LoadSampling.h"

#include "engine/FileManager.h"

namespace LoadSampling {
namespace {
unsigned g_step = 0x400;   // 0x60efec (0x400 until Init: no sampling)
unsigned g_last = 0;       // 0x613638
}  // namespace

void Init() { g_step = 0; }

void LoadStep(unsigned step) {
    if (g_step == 0x400) return;
    if (g_last == step) return;
    g_step = step;
    g_last = step;
    FileManager::LoadStep((int)step);
}

}  // namespace LoadSampling
