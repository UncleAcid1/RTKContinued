// IconManager: item, spell and UI icons by short name. Port of IconManager::GetIcon @0x2dccc8.
#pragma once

namespace Render { struct Texture; }

namespace IconManager {

// "dollars" is looked up as "bugs" and "cb" as "country_bugs". Search order: Resources::GetImage,
// then 14 fixed path patterns (A2Static item/spell icons, kingdom_icons, res_files), then
// images/professions/tutorial/. Results (also misses) are cached by name.
Render::Texture* GetIcon(const char* name, bool async = false);

}  // namespace IconManager
