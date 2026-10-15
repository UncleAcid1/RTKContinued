// Shared::PlayerHolder: the hero's dressed figure (SelectHQWindow, HealSquadWindow,
// CharacterInfoWindow, PlayerBattleInOutWindow): the body with a picture per worn piece and look
// on it. Port of 0x342470 (LoadFrom), 0x34572c (PlacePlayerEquipment), 0x345058
// (PlaceItemInHolder), 0x344d78 (PlaceObjectInHolder).
#include "hud/Shared.h"

#include <cstdio>
#include <cstring>

#include "engine/IconManager.h"
#include "engine/Render.h"
#include "game/Items.h"
#include "game/MetaExpression.h"
#include "game/PlayerEquipmentInfo.h"
#include "gui/GUI.h"

namespace Shared {

namespace {
// The layers' window names (tables 0x601818 for the man, 0x601868 for the woman): equipment
// bindings 0..9, then customisation bindings 0..9. A binding without a name has no picture.
const char* const kMaleLayers[20] = {
    "item_head_holder", "item_weapon_holder", "item_torso_holder", "item_legs_holder", nullptr, nullptr,
    "item_shield_holder", "item_arms_holder", "item_belt_holder", nullptr,
    nullptr, "item_head_holder", "item_custom_hair_holder", "item_custom_eyes_holder", "item_custom_nose_holder",
    "item_custom_mouth_holder", "item_custom_facial_hair_holder", "item_custom_back_holder", nullptr,
    "item_shoulder_holder"};
const char* const kFemaleLayers[20] = {
    "item_head_holder", "item_weapon_holder_f", "item_torso_holder", "item_legs_holder", nullptr, nullptr,
    "item_shield_holder_f", "item_arms_holder", "item_belt_holder", nullptr,
    nullptr, "item_head_holder", "item_custom_hair_holder", "item_custom_eyes_holder", "item_custom_nose_holder",
    "item_custom_mouth_holder", "item_custom_facial_hair_holder", "item_custom_back_holder", nullptr,
    "item_shoulder_holder"};
}  // namespace

void PlayerHolder::LoadFrom(GUI::Window* r, const char* name) {
    root = GUI::GetWindowTypedF<GUI::Window>(r, "%s", name);
    root->takesZ = true;
    body = GUI::GetWindowTypedF<GUI::Window>(r, "%s.inventory_character_male", name);
    bodyX = body->x - root->x;
    bodyY = body->y - root->y;
    body->takesZ = true;
    bodyFemale = GUI::GetWindowTypedF<GUI::Window>(r, "%s.inventory_character_female", name);
    bodyFemale->takesZ = true;
    bodyFemaleX = bodyFemale->x - root->x;
    bodyFemaleY = bodyFemale->y - root->y;
    for (int i = 0; i < 20; ++i) {
        Layer& l = layers[i];
        l.isDefault = false;
        l.female = false;
        l.item = 0;
        l.male = kMaleLayers[i] ? GUI::GetWindowTypedF<GUI::Window>(r, "%s.%s", name, kMaleLayers[i]) : GUI::DummyWindow();
        l.male->takesZ = true;
        l.f10 = 0.f;
        l.femaleWindow =
            kFemaleLayers[i] ? GUI::GetWindowTypedF<GUI::Window>(r, "%s.%s", name, kFemaleLayers[i]) : GUI::DummyWindow();
        l.femaleWindow->takesZ = true;
    }
}

// @0x34572c: the ten pieces (hidden ones: no picture), then the looks 1..9 (layers 11..19). A
// woman's hair goes under a helmet without a free-hair mark (meta 0x34) unless a head look is
// worn. Each picture is placed at (x, y) in the holder's frame, scaled, moved by (dx, dy) and,
// with z, drawn at z over its window's depth; flag: the windows only (no sprite placement).
void PlayerHolder::PlacePlayerEquipment(PvP::PlayerEquipmentInfo& info, float x, float y, GUI::Window* itemHolder,
                                        bool male, float scale, float dx, float dy, float z, bool flag) {
    for (int i = 0; i < 10; ++i) {
        Layer& l = layers[i];
        GUI::Window* w = male ? l.male : l.femaleWindow;
        Items::ItemInfo* item = info.GetItemAt(i);
        Items::ItemInfo* hair = info.GetCustomizationAt(2);
        const char* variant = hair ? hair->name : nullptr;
        if (!info.IsItemVisibleAt(i)) {
            l.male->SetVisibility(false);
            l.femaleWindow->SetVisibility(false);
            continue;
        }
        PlaceItemInHolder(item, (unsigned)i, x, y, itemHolder, male, scale, dx, dy, z != 0.f ? z + w->z : 0.f, flag,
                          variant);
    }
    for (int s = 1; s <= 9; ++s) {
        Layer& l = layers[10 + s];
        GUI::Window* w = male ? l.male : l.femaleWindow;
        Items::ItemInfo* look = info.GetCustomizationAt(s);
        if (s == 2 && !male && !info.GetCustomizationAt(1)) {
            Items::ItemInfo* head = info.GetItemAt(0);
            if (head && (!head->meta || !head->meta->FindChildData(0x34))) {
                layers[12].male->SetVisibility(false);
                layers[12].femaleWindow->SetVisibility(false);
                continue;
            }
        }
        if (!info.IsCustomizationVisibleAt(s)) {
            l.male->SetVisibility(false);
            l.femaleWindow->SetVisibility(false);
        } else {
            PlaceItemInHolder(look, (unsigned)s | 0x100, x, y, itemHolder, male, scale, dx, dy, z != 0.f ? z + w->z : 0.f,
                              flag, nullptr);
        }
    }
}

// @0x345058: one layer's picture. idx: the piece (0..9) or, with 0x100, the look (layer 10 +
// it; look 1 shares the head's windows and is not cleared). Without an item the default sword,
// boots and shield, or the gender's default hair, eyes, nose, mouth (a man's beard) are drawn;
// pieces 4, 5, 9 have no picture. Each picture sits at its offset in the holder (a woman's from
// her "_f" variant matching the hair's "_0.." when there is one), nudged by a pixel for some.
void PlayerHolder::PlaceItemInHolder(Items::ItemInfo* item, unsigned idx, float x, float y, GUI::Window* itemHolder,
                                     bool male, float scale, float dx, float dy, float z, bool flag,
                                     const char* variant) {
    const bool look = (idx & 0x100) != 0;
    const unsigned s = idx & ~0x100u;
    const unsigned L = look ? s + 10 : idx;
    Layer& l = layers[L];
    if (!look || s != 1) {
        l.male->SetVisibility(false);
        l.femaleWindow->SetVisibility(false);
    }
    GUI::Window* w = male ? l.male : l.femaleWindow;
    if (item) {
        l.isDefault = false;
        if (w->root) w->SetVisibility(true);
        l.female = !male;
        l.item = item->id;
        if (L == 9 || L == 4 || L == 5) return;
        int ox = 0, oy = 0;
        if (male) {
            Items::GetItemOffsetInHolder(ox, oy, true, item->id, "");
        } else {
            bool found = false;
            if (variant) {
                if (const char* v = std::strstr(variant, "_0")) {
                    char suffix[32];
                    std::snprintf(suffix, sizeof suffix, "_f%s", v);
                    found = Items::GetItemOffsetInHolder(ox, oy, male, item->id, suffix);
                }
            }
            if (!found && !Items::GetItemOffsetInHolder(ox, oy, false, item->id, "_f_01_02") &&
                !Items::GetItemOffsetInHolder(ox, oy, false, item->id, "_f"))
                Items::GetItemOffsetInHolder(ox, oy, false, item->id, "");
        }
        if ((L == 3 || L == 0) && male) --ox;
        if (s - 3 <= 1) --oy;
        if (w->root)
            PlaceObjectInHolder(w, x, y, itemHolder, item->name, ox, oy, male, scale, dx, dy, z, flag, variant);
        return;
    }
    const char* def = nullptr;
    if (!look) {
        if (L == 3) def = male ? "Armor_000_boots" : "Armor_000_boots_f";
        else if (L == 1) def = male ? "Sword_000_basic_sword" : "Sword_000_basic_sword_f";
        else if (L == 6) def = male ? "Shield_000_basic_shield" : "Shield_000_basic_shield_f";
    } else if (s == 2) {
        def = male ? "Male_hair_default" : "Female_hair_default";
    } else if (s == 3) {
        def = male ? "Male_eyes_default" : "Female_eyes_default";
    } else if (s == 4) {
        def = male ? "Male_nose_default" : "Female_nose_default";
    } else if (s == 5) {
        def = male ? "Male_mouth_default" : "Female_mouth_default";
    } else if (s == 6 && male) {
        def = "Male_beard_default";
    }
    if (!def) {
        l.item = 0;
        l.isDefault = false;
        return;
    }
    if (w->root) w->SetVisibility(true);
    l.female = !male;
    l.isDefault = true;
    int ox = 0, oy = 0;
    Items::GetItemOffsetInHolder(ox, oy, male, def);
    if (L == 3) {
        if (male) --ox;
    } else if (L == 1 && !male) {
        ++ox;
    }
    if (s == 2) {
        if (!male) ++oy;
    } else if ((s == 3 || s == 4) && male) {
        --oy;
    }
    if (w->root) PlaceObjectInHolder(w, x, y, itemHolder, def, ox, oy, male, scale, dx, dy, z, flag, nullptr);
    l.item = 0;
}

// @0x344d78: the picture `name` (a woman's "<name>_f", or "<name>_f<the hair's _0..>" /
// "_f_01_02", when one exists) laid into w at its offset in the item holder, at the root's
// scale times `scale`; it magnifies sharp at scale 1. Without flag its sprite is placed (and
// drawn at z) at once.
void PlayerHolder::PlaceObjectInHolder(GUI::Window* w, float x, float y, GUI::Window* itemHolder, const char* name,
                                       int ox, int oy, bool male, float scale, float dx, float dy, float z, bool flag,
                                       const char* variant) {
    char buf[256];
    const char* pic = name;
    if (!male) {
        std::snprintf(buf, sizeof buf, "%s_f", name);
        if (IconManager::GetIcon(buf, false)) {
            pic = buf;
        } else {
            const char* v = variant ? std::strstr(variant, "_0") : nullptr;
            std::snprintf(buf, sizeof buf, "%s_f%s", name, v ? v : "_01_02");
            if (IconManager::GetIcon(buf, false)) pic = buf;
        }
    }
    if (Render::Texture* tex = IconManager::GetIcon(pic, false)) {
        const float rs = w->root->scale;
        const float fx = ((x + rs * (float)ox * scale) - rs * (float)itemHolder->baseX * scale) + dx;
        const float fy = ((y + rs * (float)oy * scale) - rs * (float)itemHolder->baseY * scale) + dy;
        const float tw = (float)tex->w * rs * scale, th = (float)tex->h * rs * scale;
        w->w = w->origW = (int16_t)(int)(tw + 0.5f);
        w->h = w->origH = (int16_t)(int)(th + 0.5f);
        w->x = w->origX = (int16_t)(int)(fx + 0.5f);
        w->y = w->origY = (int16_t)(int)(fy + 0.75f);
        w->SetTexture(tex, false, 0, 0, true, 0);
        Render::SetMagnifyFilter(tex, scale * w->root->scale == 1.f);
        if (!flag) {
            Render::Sprite* sp = w->sprite;
            sp->h = th;
            sp->w = tw;
            if (z != 0.f) sp->z = z;
            sp->y = (float)w->root->y + fy + th + 0.5f;
            sp->x = (float)w->root->x + fx + 0.5f;
        }
    }
    Render::SortRenderLayer(Render::kLayerGUI, 1);
}

}  // namespace Shared
