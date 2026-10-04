// IconManager: item, spell and UI icons by short name. Port of IconManager::GetIcon @0x2dccc8.
#pragma once

namespace Render { struct Texture; }

namespace IconManager {

// "dollars" is looked up as "bugs" and "cb" as "country_bugs". Search order: Resources::GetImage,
// then 14 fixed path patterns (A2Static item/spell icons, kingdom_icons, res_files), then
// images/professions/tutorial/. Results (also misses) are cached by name.
Render::Texture* GetIcon(const char* name, bool async = false);

// Animated icons (FUN_002dc20c): numbered frames loaded once and linked into a loop.
// UNVERIFIED: Render does not play texture frame chains yet (Texture +0x0c delay, +0x14 next), so
// these return the first frame.
Render::Texture* GetSandClockIcon();     // @0x2dc324 "images/icon_60_timer (%d).png", 1 frame
Render::Texture* GetExclamaitionIcon();  // @0x2dc37c "images/exc_mark_%04d.png", 15 frames of 1/15 s

}  // namespace IconManager
