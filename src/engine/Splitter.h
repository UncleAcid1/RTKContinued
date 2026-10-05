// Splitter: splits a string at every occurrence of a delimiter string.
// Port of Splitter::parse @0x227200. Empty parts are kept, so "" gives one empty part.
#pragma once
#include <cstring>
#include <string>
#include <vector>

inline std::vector<std::string> SplitterParse(const char* s, const char* delim) {
    std::vector<std::string> parts;
    size_t n = std::strlen(delim);
    for (const char* hit; (hit = std::strstr(s, delim)) != nullptr; s = hit + n) parts.emplace_back(s, hit);
    parts.emplace_back(s);
    return parts;
}
