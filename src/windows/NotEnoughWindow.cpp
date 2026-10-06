// NotEnoughWindow: what a purchase lacks, and the dialog offering it for crystals. Port of
// NotEnoughWindow (libkingdom.so 5.11). Only the resource requirements are ported so far (3e.2);
// the dialog comes with 3e.4.
#include "windows/Windows.h"

#include <cstdio>

#include "game/GameState.h"

namespace NotEnoughWindow {
namespace {

unsigned g_required[11] = {};   // 0x625924 per resource type
int g_missing = 0;              // 0x625d6c

}  // namespace

void ResetRequirements() {
    for (unsigned& r : g_required) r = 0;
    // UNVERIFIED (3e.4): also clears the level/profession/item/worker/building/upgrade requirements
    // and the dialog's callback.
}

void AddRequirement(int type, unsigned amount) { g_required[type] = amount; }

bool CheckRequirements() {
    g_missing = 0;
    for (int i = 0; i < 11; ++i) {
        int need = (int)g_required[i];
        if (need != 0 && (int)GameState::GetResourceAmount(i) < need) ++g_missing;
    }
    return g_missing == 0;
}

void Show() {
    for (int i = 0; i < 11; ++i) {
        if (g_required[i] != 0 && (int)GameState::GetResourceAmount(i) < (int)g_required[i])
            std::printf("NotEnoughWindow: %s %u needed, %u held\n", GameState::GetResourceName(i), g_required[i],
                        GameState::GetResourceAmount(i));
    }
}

}  // namespace NotEnoughWindow
