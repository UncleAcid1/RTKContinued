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

}  // namespace Resources
