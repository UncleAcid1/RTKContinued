// HiddenObjects: decorations faded out while something is placed or moved over them, restored
// afterwards. Port of HiddenObjects (libkingdom.so 5.11, 0x3a4280..0x3a5160): a std::set of
// UpdatedObject ordered by object pointer. Its users (BuildingPlacement, BuildingMovement,
// AIPlayer) only hide decorations. The masked variant HideObject(obj, x, y, r) @0x3a4e44 (a round
// colour mask instead of a fade) has no caller and is not ported.
#pragma once
#include <map>

namespace Map { struct Decor; }

class HiddenObjects {
public:
    void HideObject(Map::Decor* d, float alpha);   // @0x3a4ff8 fade towards alpha
    void ShowAll();                                // @0x3a470c fade every object back in
    void Clear(bool restore);                      // @0x3a478c forget all (restore: fully opaque now)
    // @0x3a4868: each sprite's alpha moves 4 per second towards its target; restored objects are
    // dropped once opaque.
    void Update(float dt);

private:
    struct UpdatedObject {   // the set node from +0x10: object, +0x14 masked, +0x18 alpha, +0x28 restore
        float alpha = 0.f;
        bool restore = false;
    };
    std::map<Map::Decor*, UpdatedObject> objects_;
};
