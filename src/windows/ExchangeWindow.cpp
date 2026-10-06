// ExchangeWindow: the crystal/gold exchange. UNVERIFIED stand-ins until it is ported (3e.4); the
// real-money packs it offered are removed (see STATUS.md, freemium removal).
#include <cstdio>
#include <string>

#include "windows/Windows.h"

namespace ExchangeWindow {

void Show() {}

void OnTab(unsigned tab, const char32_t* text) {
    std::string s;
    for (const char32_t* p = text; p && *p; ++p) s += *p < 0x80 ? (char)*p : '?';
    std::printf("ExchangeWindow (stand-in): tab %u: %s\n", tab, s.c_str());
}

}  // namespace ExchangeWindow
