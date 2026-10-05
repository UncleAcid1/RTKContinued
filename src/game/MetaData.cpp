#include "game/MetaData.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

const char* TypeName(int t) {   // 0x601674
    static const char* names[] = {"nothing", "number", "string", "resource", "list"};
    return t < 5 ? names[t] : "complex";
}

const MetaData g_empty;          // 0x613950

// bionic's ctype: _S (space) and _U|_L|_N (alphanumeric), ASCII only.
bool IsSpace(unsigned char c) { return c < 0x80 && std::isspace(c); }
bool IsAlnum(unsigned char c) { return c < 0x80 && std::isalnum(c); }

// @0x1d2414
MetaData* ParseString(const char*& p) {
    while (*p && IsSpace((unsigned char)*p)) ++p;
    if (!IsAlnum((unsigned char)*p) && *p != '_') return nullptr;
    MetaData* m = new MetaData(MetaData::kString);
    const char* start = p;
    while (*p && (IsAlnum((unsigned char)*p) || *p == '_')) ++p;
    m->string = new char[p - start + 1];
    std::memcpy(m->string, start, p - start);
    m->string[p - start] = 0;
    return m;
}

// @0x1d2710
MetaData* ParseTerminal(const char*& p) {
    if (MetaData* m = ParseInteger(p)) return m;
    return ParseString(p);
}

}  // namespace

// (A '-' not followed by a digit is consumed anyway.)
MetaData* ParseInteger(const char*& p) {
    if (!*p) return nullptr;
    while (IsSpace((unsigned char)*p)) {
        ++p;
        if (!*p) return nullptr;
    }
    bool negative = *p == '-';
    if (negative) ++p;
    const char* start = p;
    if ((unsigned)(*p - '0') >= 10) return nullptr;
    MetaData* m = new MetaData(MetaData::kNumber);
    int sign = negative ? -1 : 1;
    m->intValue = std::atoi(p) * sign;
    m->floatValue = (float)m->intValue;
    while ((unsigned)(*p - '0') < 10) ++p;
    if (*p == '.') {
        float f = (float)std::strtod(start, nullptr) * (negative ? -1.f : 1.f);
        ++p;
        m->floatValue = f;
        m->intValue = (int)f;
    }
    while ((unsigned)(*p - '0') < 10) ++p;
    return m;
}

MetaData::~MetaData() {
    for (MetaData* c = first; c;) {
        MetaData* n = c->next;
        delete c;
        c = n;
    }
    delete[] string;
}

void MetaData::AppendChild(MetaData* child) {
    if (!child) std::fprintf(stderr, "ERROR: MetaData::AppendChild() Child is invalid\n");
    if (!first) first = last = child;
    else {
        last->next = child;
        last = child;
    }
}

unsigned MetaData::GetChildrenCount() const {
    if ((unsigned)(type - 1) < 2)
        std::fprintf(stderr, "ERROR: MetaData::GetChildrenCount() Expected list, but got type %s\n", TypeName(type));
    unsigned n = 0;
    for (const MetaData* c = first; c; c = c->next) ++n;
    return n;
}

const MetaData* MetaData::GetChild(unsigned i) const {
    if ((unsigned)(type - 1) < 2)
        std::fprintf(stderr, "ERROR: MetaData::GetChild() Expected list, but got type %s\n", TypeName(type));
    if (i >= GetChildrenCount())
        std::fprintf(stderr, "ERROR: MetaData::GetChildrenCount() Expected list with at least %d elements, but got %d\n",
                     i + 1, GetChildrenCount());
    const MetaData* c = first;
    while (i != 0 && c) {
        --i;
        c = c->next;
    }
    return c ? c : &g_empty;
}

const char* MetaData::GetString() const {
    if (type != kString)
        std::fprintf(stderr, "ERROR: MetaData::GetString() Expected string, but got type %s\n", TypeName(type));
    return string ? string : "";
}

float MetaData::GetFloat() const {
    if (type != kNumber)   // (prints the type number with %d on the original)
        std::fprintf(stderr, "ERROR: MetaData::GetFloat() Expected number, but got type %d\n", type);
    return floatValue;
}

int MetaData::GetInt() const {
    if (type != kNumber)
        std::fprintf(stderr, "ERROR: MetaData::GetInt() Expected number, but got %s\n", TypeName(type));
    return intValue;
}

MetaData* ParseCustomStyleData(const char*& p, const char* delims, const char* wrap) {
    if (!*p) return new MetaData(MetaData::kNothing);
    const char* subWrap = *wrap ? wrap + 1 : wrap;
    bool leaf = !delims[0] || !delims[1];
    MetaData* item = leaf ? ParseTerminal(p) : ParseCustomStyleData(p, delims + 1, subWrap);
    MetaData* node = item;
    if (*p == delims[0] || *wrap == '1') {
        node = new MetaData(MetaData::kList);
        if (item) node->AppendChild(item);
        else if (*wrap == '1') node->AppendChild(new MetaData);
    }
    while (*p == delims[0]) {
        ++p;
        MetaData* m = leaf ? ParseTerminal(p) : ParseCustomStyleData(p, delims + 1, subWrap);
        if (m) node->AppendChild(m);
    }
    return node;
}
