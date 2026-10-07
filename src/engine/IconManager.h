// IconManager: item, spell and UI icons by short name. Port of IconManager::GetIcon @0x2dccc8.
#pragma once

namespace Render { struct Texture; }

namespace IconManager {

// "dollars" is looked up as "bugs" and "cb" as "country_bugs". Search order: Resources::GetImage,
// then 14 fixed path patterns (A2Static item/spell icons, kingdom_icons, res_files), then
// images/professions/tutorial/. Results (also misses) are cached by name.
Render::Texture* GetIcon(const char* name, bool async = false);

// Animated icons (FUN_002dc20c): numbered frames loaded once and linked into a loop that
// Render::UpdateAnimatedSprites plays.
Render::Texture* GetSandClockIcon();       // @0x2dc324 "images/icon_60_timer (%d).png", 1 frame
Render::Texture* GetExclamaitionIcon();    // @0x2dc37c "images/exc_mark_%04d.png", 15 frames of 1/15 s
Render::Texture* GetPlayerMarkerAttack();  // @0x2dc3c4 the attack marker, 15 frames of 1/30 s
Render::Texture* GetPlayerMarkerBlocked(); // @0x2dc3f0 the hero cannot go there, 13 frames
Render::Texture* GetPlayerMarkerGoto();    // @0x2dc41c the hero's walk target, 15 frames

}  // namespace IconManager
