// TextStyleManager: the popup text styles (damage numbers, collected gold/xp, item names), read from
// the textfields of Popup_damage_anim.xml. Port of TextStyleManager (libkingdom.so 5.11).
// (CreateGradient @0x36466c, the gradient surfaces for styled text, is not ported yet.)
#pragma once

namespace Render { struct GlowFilter; }

namespace TextStyleManager {

enum TextStyle {   // slots of 0x63060c, in Init's textfield order
    kHeal = 0, kBlocked, kBlockedNumber, kDamage, kCrit, kCritDmg, kGold, kXp,
    kFire, kFireNumber, kElec, kElecNumber, kIce, kIceNumber, kDodge,
    kItemClass2, kItemClass3, kItemClass4, kItemClass5
};

void Init();   // @0x364364
// @0x3642a8: the style's font size and colour; returns its glow filter chain.
Render::GlowFilter* GetTextStyle(TextStyle style, int& size, float& r, float& g, float& b);

}  // namespace TextStyleManager
