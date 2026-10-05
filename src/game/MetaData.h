// MetaData: a parsed value tree for game-data attributes such as buildings.xml's
// requirestoupgrade="1:99=1,2:99=2". Port of MetaData (@0x1c7770..0x1c86bc) and ParseCustomStyleData
// @0x1d2730 / ParseTerminal @0x1d2710 / ParseInteger @0x1d2564 / ParseString @0x1d2414.
//
// ParseCustomStyleData(p, delims, wrap): split by delims[0], each part parsed with delims+1, the last
// level parsed as a terminal (integer, else identifier [A-Za-z0-9_]+). A level with more than one part,
// or whose wrap flag (wrap[i] == '1') is set, becomes a list node. An empty input gives a "nothing" node.
#pragma once

class MetaData {
public:
    enum Type { kNothing = 0, kNumber = 1, kString = 2, kResource = 3, kList = 4 };

    MetaData() = default;                       // @0x1c7770
    explicit MetaData(int type) : type(type) {} // @0x1c77f0
    virtual ~MetaData();                        // @0x1c8004 (frees the children and the string)

    MetaData* GetNext() const { return next; } // @0x1c7880
    void AppendChild(MetaData* child);          // @0x1c83ec
    unsigned GetChildrenCount() const;          // @0x1c8430
    // @0x1c84bc: past the end, a shared empty node (0x613950).
    const MetaData* GetChild(unsigned i) const;
    const char* GetString() const;              // @0x1c8600
    float GetFloat() const;                     // @0x1c8668
    int GetInt() const;                         // @0x1c86bc

    MetaData* first = nullptr;                  // +0x04
    MetaData* last = nullptr;                   // +0x08
    MetaData* next = nullptr;                   // +0x0c
    int type = kNothing;                        // +0x10
    char* string = nullptr;                     // +0x14
    int intValue = 0;                           // +0x18
    float floatValue = 0.f;                     // +0x1c
};

MetaData* ParseCustomStyleData(const char*& p, const char* delims, const char* wrap);
