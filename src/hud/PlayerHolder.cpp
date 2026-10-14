// Shared::PlayerHolder: the hero's dressed figure (SelectHQWindow, HealSquadWindow,
// CharacterInfoWindow). Port of 0x342470 (LoadFrom); PlacePlayerEquipment @0x34572c with
// PlaceItemInHolder @0x345058 and PlaceObjectInHolder @0x344d78 follow in 4h part 2.
#include "hud/Shared.h"

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

// UNVERIFIED (4h part 2): the pieces of `info` drawn over the body. Until then the bare body of
// the asked gender shows.
void PlayerHolder::PlacePlayerEquipment(PvP::PlayerEquipmentInfo&, float, float, GUI::Window*, bool male, float, float,
                                        float, bool) {
    body->SetVisibility(male);
    bodyFemale->SetVisibility(!male);
}

}  // namespace Shared
