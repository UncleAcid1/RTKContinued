#include "engine/Resources.h"

#include <pugixml.hpp>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <unordered_map>

#include "engine/FileManager.h"
#include "engine/Image.h"
#include "engine/Render.h"

namespace Resources {
namespace {

std::unordered_map<std::string, int> g_anims;               // lower-cased name -> frames (onefile)
std::unordered_map<std::string, Render::Texture*> g_cache;  // lower-cased request -> texture (or null)

std::string Lower(std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// Render::CreateTexture: load the file; for non-alpha images the alpha mask is the same path with
// the extension replaced by "_.png" (memcpy of "_.png" over the '.'). Premultiplied (GetImageData).
Render::Texture* LoadTexture(const std::string& path) {
    uint32_t n = 0;
    uint8_t* data = FileManager::LoadFile(path.c_str(), n);
    if (!data) return nullptr;
    DecodedImage color;
    bool ok = DecodeImage(data, n, color);
    FileManager::FreeFile(data);
    if (!ok) {
        std::fprintf(stderr, "Resources: cannot decode %s\n", path.c_str());
        return nullptr;
    }
    DecodedImage alpha;
    bool hasMask = false;
    if (color.channels != 4) {
        std::string mask = path.substr(0, path.rfind('.')) + "_.png";
        uint8_t* m = FileManager::LoadFile(mask.c_str(), n);
        if (m) {
            hasMask = DecodeImage(m, n, alpha);
            FileManager::FreeFile(m);
        }
    }
    return Render::CreateTexture(MakePremultiplied(color, hasMask ? &alpha : nullptr), path);
}

}  // namespace

bool Init() {
    uint32_t n = 0;
    uint8_t* xml = FileManager::LoadFile("../resource/res_files/1Original/AllAnimsFrames.xml", n);
    if (!xml) return false;
    pugi::xml_document doc;
    bool ok = doc.load_buffer(xml, n);
    FileManager::FreeFile(xml);
    if (!ok) return false;
    for (auto a : doc.child("animations").children("anim")) {
        if (a.attribute("onefile").as_bool()) g_anims[Lower(a.attribute("name").as_string())] = a.attribute("frames").as_int();
    }
    return true;
}

std::string GetDecoratedImageName(const char* pack, const char* name) {
    static const char* fmts[] = {"../resource/%s/2Optimized/%s.png", "../resource/%s/2Optimized/%s.jpg",
                                 "../resource/%s/1Original/%s.png", "../resource/%s/1Original/%s.jpg"};
    char buf[1024];
    for (const char* f : fmts) {
        std::snprintf(buf, sizeof buf, f, pack, name);
        if (FileManager::FileExists(buf)) return buf;
    }
    if (FileManager::FileExists(name)) return name;
    return "";
}

Render::Texture* GetImage(const char* name) {
    if (!name || !*name) return nullptr;
    std::string key = Lower(name);
    auto it = g_cache.find(key);
    if (it != g_cache.end()) return it->second;
    Render::Texture* t = nullptr;
    std::string path = GetDecoratedImageName("A2Static", name);
    if (path.empty()) path = GetDecoratedImageName("A3MergedAnims", name);
    if (!path.empty()) {
        t = LoadTexture(path);
    } else if (auto a = g_anims.find(key); a != g_anims.end()) {
        path = GetDecoratedImageName("A3MergedAnims", (std::string(name) + "_anim").c_str());
        if (!path.empty() && (t = LoadTexture(path))) {
            t->frames = a->second;
            // GetImage: "fir5_stump"/"tree5_stump" textures are forced to one frame
            if (std::strstr(name, "fir5_stump") || std::strstr(name, "tree5_stump")) t->frames = 1;
        }
    }
    // UNVERIFIED: multi-file animations ("%s_00", "%s_%02d" frames) are not wired up yet.
    g_cache[key] = t;
    return t;
}

int GetFrameCount(const char* name) {
    auto a = g_anims.find(Lower(name));
    return a != g_anims.end() ? a->second : 1;
}

Render::Texture* GetDecoration(const char* name) {
    static const char* fmts[] = {"images/Decor/%s", "images/Buildings/%s", "images/%s"};
    char buf[1024];
    for (const char* f : fmts) {
        std::snprintf(buf, sizeof buf, f, name);
        if (Render::Texture* t = GetImage(buf)) return t;
    }
    return GetImage(name);
}

Render::Texture* GetDirectImage(const char* path) {
    std::string key = "$direct:" + Lower(path);
    auto it = g_cache.find(key);
    if (it != g_cache.end()) return it->second;
    Render::Texture* t = LoadTexture(path);
    g_cache[key] = t;
    return t;
}

// @0x205e3c (non-async path)
Render::Texture* GetUIImage(const char* name, bool quiet, bool async) {
    (void)async;
    if (!name || !*name) return nullptr;
    std::string key = "$ui:" + Lower(name);
    auto it = g_cache.find(key);
    if (it != g_cache.end()) return it->second;
    Render::Texture* t = LoadTexture(name);
    if (!t) t = LoadTexture(std::string("../resource/kingdom_ui/1Original/") + name);
    if (!t) {
        const char* base = std::strrchr(name, '/');
        std::string opt = std::string("../resource/kingdom_ui/2Optimized/") + (base ? base + 1 : name);
        size_t png = opt.find(".png");
        if (png != std::string::npos) opt.replace(png, 4, ".jpg");
        t = LoadTexture(opt);
        if (!t && !quiet) std::printf("Resources::GetUIImage() Failed to load %s\n", opt.c_str());
    }
    if (t) g_cache[key] = t;   // only successful loads are cached
    return t;
}

}  // namespace Resources
