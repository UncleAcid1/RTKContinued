// Resources: image name resolution and texture cache.
// Port of Resources::GetImage @0x204304, GetDecoratedImageName @0x203c64, GetDecoration @0x204b24,
// GetDirectImage, GetUIImage.
#pragma once
#include <string>

// PORT (checked in the 4b.5 review): the original builds a registry when it starts. main_Loop_Init
// hands FileManager two callbacks for every file in the packs, RegisterResources @0x2064c8 and
// RegisterAnimations @0x205220. They fill hash tables keyed case-insensitively: an image's pack
// and file (@0x203f58 add, @0x20404c find, @0x20412c the pack list's push_back, GetPack @0x204270),
// and an animation's exploded frames (GetExplodedAnimationImagePath @0x205bdc; @0x205a5c the table
// growing). The port probes the pack folders on demand (GetDecoratedImageName) and reads the
// one-file animation registry (Init). PORT: the exploded-frame fallback (see Animation.cpp).
#include "engine/Image.h"

namespace Render { struct Texture; }

namespace Resources {

// Render::GetImageData(path) @0x1fc088: the image (and its "_.png" alpha mask) as premultiplied RGBA;
// bpp 32 with alpha, else 24. Not found or not decodable: an empty image.
ImageRGBA GetImageData(const char* path, unsigned& bpp);

bool Init(const char* file);  // loads the one-file animation registry (AllAnimsFrames.xml; stands in for RegisterAnimations)

// "<pack>/2Optimized/<name>.png|.jpg", then "1Original"; returns the resource path or "".
std::string GetDecoratedImageName(const char* pack, const char* name);
// Packs A2Static then A3MergedAnims; one-file animations resolve to "<name>_anim" in A3MergedAnims
// with Texture::frames set from the registry. Returns nullptr if not found. pixelMask (the
// original's 4th argument) gives a texture created by this call its hit mask (Render::HasPixelAt).
Render::Texture* GetImage(const char* name, bool pixelMask = false);
// @0x205b58: the frame count of a registered one-file animation, else 1.
int GetFrameCount(const char* name);
// "images/Decor/%s", "images/Buildings/%s", "images/%s", then the name itself.
Render::Texture* GetDecoration(const char* name, bool pixelMask = false);
// A plain resource path (e.g. "images/grass_01.png"), no pack decoration.
Render::Texture* GetDirectImage(const char* path);   // @0x205c94 (its atlas lookup: none in the data)
// @0x205e3c GetUIImage(name, quiet, async): the name as given, then
// "../resource/kingdom_ui/1Original/<name>", then "../resource/kingdom_ui/2Optimized/<file name>"
// with ".png" replaced by ".jpg". Cached by name (case-insensitive).
// PORT: the original first consults an atlas table (FUN_002040b8 @0x20452c) under the same two
// prefixes; nothing in the Android 5.11 data registers UI atlases, so the lookup is not ported.
// async (deferred loading) is not ported: textures load immediately.
Render::Texture* GetUIImage(const char* name, bool quiet, bool async);

}  // namespace Resources
