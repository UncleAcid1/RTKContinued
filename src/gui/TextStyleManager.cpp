#include "gui/TextStyleManager.h"

#include "engine/Text.h"
#include "gui/GUI.h"

namespace TextStyleManager {

namespace {
GUI::Window* g_root = nullptr;     // 0x630658.. the layout
GUI::Textfield* g_styles[19] = {}; // 0x63060c
}

void Init() {
    float scale = GUI::GetHoverScaleFactor(1.f, 1.f);
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Popup_damage_anim.xml", "Popup_damage_anim.png",
                             scale, 0, 0, 0, 0, false, 1.f);
    g_root->SetVisibility(false);
    static const struct { TextStyle slot; const char* name; } kFields[] = {
        {kHeal, "popup_damage_heal"}, {kBlocked, "popup_damage_blocked"},
        {kBlockedNumber, "popup_damage_blocked_number"}, {kDamage, "popup_damage"},
        {kDodge, "popup_damage_dodge"}, {kCrit, "popup_damage_crit"}, {kCritDmg, "popup_damage_crit_dmg"},
        {kGold, "popup_gold"}, {kXp, "popup_xp"}, {kFire, "popup_magic_fire_03"},
        {kFireNumber, "popup_magic_fire_03_number"}, {kElec, "popup_magic_elec_02"},
        {kElecNumber, "popup_magic_elec_02_number"}, {kIce, "popup_magic_ice_01"},
        {kIceNumber, "popup_magic_ice_01_number"}, {kItemClass2, "popup_text_item_class_02"},
        {kItemClass3, "popup_text_item_class_03"}, {kItemClass4, "popup_text_item_class_04"},
        {kItemClass5, "popup_text_item_class_05"}};
    for (const auto& f : kFields) g_styles[f.slot] = GUI::GetWindowTyped<GUI::Textfield>(g_root, f.name);
}

Render::GlowFilter* GetTextStyle(TextStyle style, int& size, float& r, float& g, float& b) {
    GUI::Textfield* t = g_styles[style];
    r = t->color[0];
    g = t->color[1];
    b = t->color[2];
    size = (int)t->font->size;
    return t->glow;
}

}  // namespace TextStyleManager
