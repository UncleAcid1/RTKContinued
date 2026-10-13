// PENDING (4g, HUD parts): the fight's HUD hooks, empty until their windows are ported in the next
// parts of this step. This file goes away with the last of them.
#include "hud/HUD.h"

namespace HUDWindow { void OnDamage(Entity*) {} }
namespace PlayerTopWindow { void OnDamage(Entity*) {} }
namespace BattleBarWindow {
void OnDamage(Entity*) {}
void LaunchFirstDeathHelper() {}
}
namespace BeltBarWindow { void UpdateContents() {} }
namespace BottomBattleWindow {
void EnableNet(uint32_t) {}
uint32_t GetNetTargetSpawnID() { return 0; }
}
namespace HealSquadWindow {
void Show() {}
bool IsVisible() { return false; }
void SetAttacker(Entity*) {}
}
