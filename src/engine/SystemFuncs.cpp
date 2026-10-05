#include "engine/SystemFuncs.h"

#include <map>

namespace SystemFuncs {
namespace {
std::map<std::string, int> g_ints;   // "<file>/<key>"; a null file is the default preferences file
std::string Key(const char* key, const char* file) { return std::string(file ? file : "") + "/" + key; }
}  // namespace

int GetSetting_Int(const char* key, int def, const char* file) {
    auto it = g_ints.find(Key(key, file));
    return it == g_ints.end() ? def : it->second;
}

void SetSetting_Int(const char* key, int value, const char* file) { g_ints[Key(key, file)] = value; }

std::string getGUID() { return std::string(); }

}  // namespace SystemFuncs
