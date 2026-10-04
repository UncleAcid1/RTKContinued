// Resources: image name resolution and texture cache.
// Port of Resources::GetImage @0x204304, GetDecoratedImageName @0x203c64, GetDecoration @0x204b24,
// GetDirectImage, GetUIImage.
#pragma once
#include <string>

namespace Render { struct Texture; }

namespace Resources {

bool Init();  // loads the one-file animation registry (res_files/1Original/AllAnimsFrames.xml)

// "<pack>/2Optimized/<name>.png|.jpg", then "1Original"; returns the resource path or "".
std::string GetDecoratedImageName(const char* pack, const char* name);
// Packs A2Static then A3MergedAnims; one-file animations resolve to "<name>_anim" in A3MergedAnims
// with Texture::frames set from the registry. Returns nullptr if not found.
Render::Texture* GetImage(const char* name);
// "images/Decor/%s", "images/Buildings/%s", "images/%s", then the name itself.
Render::Texture* GetDecoration(const char* name);
// A plain resource path (e.g. "images/grass_01.png"), no pack decoration.
Render::Texture* GetDirectImage(const char* path);
// @0x205e3c GetUIImage(name, quiet, async): the name as given, then
// "../resource/kingdom_ui/1Original/<name>", then "../resource/kingdom_ui/2Optimized/<file name>"
// with ".png" replaced by ".jpg". Cached by name (case-insensitive).
// UNVERIFIED: the original first consults an atlas table (FUN_002040b8 @0x20452c) under the same two
// prefixes; nothing in the Android 5.11 data registers UI atlases, so the lookup is not ported.
// async (deferred loading) is not ported: textures load immediately.
Render::Texture* GetUIImage(const char* name, bool quiet, bool async);

}  // namespace Resources
