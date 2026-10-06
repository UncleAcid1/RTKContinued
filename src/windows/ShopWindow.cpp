// ShopWindow: the building and decoration shop. Port of ShopWindow (libkingdom.so 5.11).
// UNVERIFIED (3e.3): not ported yet; these stand in for a shop that is never open.
#include "windows/Windows.h"

#include "gui/GUI.h"

namespace ShopWindow {

bool IsVisible() { return false; }
void Show() {}
void Hide() {}
// The closed shop's info panel lies off screen; PlaceBuildingHoverWindow then centres itself.
int GetInfoPanelX() { return GUI::ScreenWidth(); }

}  // namespace ShopWindow
