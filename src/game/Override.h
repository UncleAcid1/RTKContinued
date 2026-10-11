// Override: changes to the game data by name, queued and applied in groups. The server sends them
// (ServerFlags::OverrideParams, online: dormant) and the console can; the game itself queues one at
// start-up. Port of Override::AddParameter @0x1da7ac and Override::Apply @0x1daad4.
#pragma once
#include <cstdint>

namespace Override {

// What a parameter changes (Apply's switch).
enum Kind {
    kItem = 1, kTask = 2, kSubTask = 3, kSetting = 4, kMap = 5, kContract = 6, kEntity = 7,
    kCollection = 8, kChest = 9, kObject = 10, kBuilding = 0xb, kWarrior = 0xc, kOG = 0xd,
    kItemsOfType = 0xe, kItemsOfSlot = 0xf, kTag = 0x10, kLocation = 0x11, kAll = 0x12
};

// @0x1da7ac: queued; name and value shorter than 64 characters, a '^' in the value (after its
// first character) becomes '|'.
void AddParameter(uint32_t kind, uint32_t id, uint32_t sub, const char* name, const char* value);
// @0x1daad4: the queued parameters of that kind (kAll: all) are applied and leave the queue.
void Apply(int kind);

}  // namespace Override
