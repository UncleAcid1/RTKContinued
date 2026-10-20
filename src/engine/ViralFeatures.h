// ViralFeatures: the screenshot framed for sharing (the screenshot mode's OK). Port of the part of
// ViralFeatures (libkingdom.so 5.11) that makes the picture, 0x23ef20..0x23f5d4.
#pragma once
#include <cstdint>

namespace ViralFeatures {
// @0x23f2b8: the screen (w x h, 0xAARRGGBB words, top row first) scaled into an 814x480 picture
// under the cover frame and the logo, then saved (FileManager::SaveImage).
void PostScreenshot(const uint32_t* pixels, int w, int h);
}
