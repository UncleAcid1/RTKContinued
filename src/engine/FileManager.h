// FileManager: the game's virtual file system.
// Port of FileManager (libkingdom.so 5.11). All game files are looked up by their resource name
// (e.g. "maps/map_0.bin", "../resource/A2Static/2Optimized/images/....png") in one case-insensitive table.
// Sources, in registration order (later registrations win, as in the original's hash chains):
//   1. Android base data: assets/data/res_desc.bin + data/<chunk>.jet
//   2. Expansion blobs (.kbf): FileManager::RegisterBlobFile @0x16e7b8
//   3. PORT: Win8 res_desc.bin/res_data.bin, registered only for names still missing (fallback).
#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace FileManager {

struct Entry {
    std::string name;
    uint32_t hash = 0, width = 0, height = 0, flags = 0, bpp = 0, offset = 0, size = 0, chunk = 0;
    int source = -1;          // index into the source list
};

// Registration
bool RegisterAndroidData(const std::string& dataDir);          // res_desc.bin + <chunk>.jet
bool RegisterBlobFile(const std::string& kbfPath);              // @0x16e7b8
bool RegisterWin8Fallback(const std::string& assetsDir);        // PORT: fallback source

// Queries (original API)
bool FileExists(const char* name);                                              // @0x16d0ec
bool GetFileInfo(const char* name, int& w, int& h, unsigned& bpp, bool quiet);  // @0x16b0d8
// Returns a malloc'd, NUL-terminated buffer (size excludes the NUL) or nullptr.
// Payloads flagged as gzip (flags & 1) are inflated, as in LoadFile @0x172ff0.
uint8_t* LoadFile(const char* name, uint32_t& size);
void FreeFile(uint8_t* p);

size_t EntryCount();

// Text input for the edited Textfield. The callback gets the whole text after every change, with
// final = true when editing ends.
using TextInputCallback = void (*)(const char32_t* text, bool final);
// @0x16cb18: on Android a Java text dialog (initial text cut to 250 characters). PORT: the Mac port
// starts the binary's keyboard path instead: TextInput::Enable(text) plus SDL text events.
void BeginTextInput(TextInputCallback cb, const char32_t* text);
void EndTextInput(const char32_t* text, bool final);   // @0x16a934
// @0x16a958: empty on Android (the Java dialog is modal). PORT: stops the keyboard path.
void AbortTextInput();

}  // namespace FileManager
