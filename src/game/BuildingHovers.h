// BuildingHovers: the city's world-space interaction layer. It keeps a hover record for every
// registered building, decoration and entity and gives each the hover window its state calls for
// (build bubble, taxes, cut tree, ...), animates the dropped resources (ItemDrop) until they are
// tapped or time out, flies collected ones to the HUD (ItemMove) and floats text popups (TextInfo).
// It is the FunctionalWindow "BuildingHovers"; its Click is the city tap.
// Port of BuildingHovers (libkingdom.so 5.11): Update @0x26e460, Click @0x27146c,
// UpdateHovers @0x268430, HoverInfo::SetHoverType @0x267e28, DropResource @0x2707ac.
//
// A tap that no hover takes opens the tapped object's info window (OnEntityClick, OnBuildingClick,
// OnDecorClick; the windows are in hud/InfoHoverWindows.cpp).
// Not ported yet (marked UNVERIFIED where they would run): the world dialog, the tutorial arrows
// and steps, item drops (DropItem, Items), farms (DropFarmFood, the farm hovers and windows: 3f),
// the entity hovers (talk, health bars, boss time, player names) and the text cache.
#pragma once
#include <cstdint>

#include "gui/GUI.h"

namespace Render { struct Sprite; struct Texture; }
namespace Map { struct Building; struct Decor; }
namespace WindowManager { class FunctionalWindow; }
class Entity;
class BuildingHoverWindow;

namespace BuildingHovers {

enum HoverWindowType {
    kNone = 0, kAssignBuilder = 1, kBuildProgress = 2, kTaxes = 3, kContractFinished = 4,
    kCutResource = 5, kResourceRestore = 6, kHireTroops = 7, kFarmWater = 8, kFarmReady = 9,
    kFarmSleeping = 10, kDelivery = 0xb, kTalk = 0xc, kHealthbar = 0xd, kHealthbarTiny = 0xe,
    kUseItem = 0xf, kBossTime = 0x10, kPlayerName = 0x11, kTraining = 0x12, kFriendInfo = 0x13
};

struct HoverInfo {                    // 0x18 bytes
    int id = 0;                       // +0x00
    Map::Building* building = nullptr;  // +0x04
    Entity* entity = nullptr;         // +0x08
    Map::Decor* decor = nullptr;      // +0x0c
    int type = kNone;                 // +0x10
    BuildingHoverWindow* window = nullptr;  // +0x14
    void SetHoverType(int t);         // @0x267e28
};

WindowManager::FunctionalWindow* Queue();   // the static FunctionalWindow (FUN_00263378)

void Init();                                // @0x265de4
void Deinit();                              // @0x265050
bool Click(int x, int y, bool pressed);     // @0x27146c
inline void SetZ(float) {}                  // @0x262fe4
void Show();                                // @0x26fa90
void Hide();                                // @0x264354
// @0x26e460: the drops, text popups and flying items; when not paused the hover positions and,
// once per second of global time (or when forced), the hover types.
void Update(double dt, bool force);
void UpdateHovers();                        // @0x268430
void UpdateHoverPositions();                // @0x267920

void RegisterBuilding(Map::Building* b);    // @0x267b60
void RegisterDecoration(Map::Decor* d);     // @0x267c4c
void SafeRegisterEntity(Entity* e);         // @0x26446c (once per entity)
void UnregisterBuilding(Map::Building* b);  // @0x265608
void UnregisterDecoration(Map::Decor* d);   // @0x2657b4
void UnregisterEntity(Entity* e);           // @0x264c54

bool BuildingHasActiveHover(Map::Building* b);   // @0x262f14
void ActivateBuildingHover(Map::Building* b);    // @0x263018
inline bool IsWorldDialogVisible() { return false; }   // @0x2632c4 UNVERIFIED: no world dialog yet
bool IsHoverVisible();                      // @0x263320
void ScheduleUpdate();                      // @0x2630a4
void AddDeliveryContractToFinishOnLastItem(unsigned id);   // @0x263164
void SetHoverVisiblity(bool visible, bool arg);  // @0x2630bc
bool HasDroppedItems();                     // @0x2631b0
void CollectAll();                          // @0x26e0e8 every drop collected at once (before a save)
bool IsItemMoving();                        // @0x26333c

// @0x2707ac: drop `amount` of a resource at a world point as bouncing pickups (gold as piles of up
// to 100). collectNow collects each at once (flying to the HUD); bonus marks the bonus text.
void DropResource(float x, float y, int type, unsigned amount, bool collectNow, bool bonus);
// @0x26976c: a harvested crop (resource `type`, `amount`) drops at a world point with its farm drop
// image; its subtasks are farm drop id `dropId`. On the farm, drops below y 3000 move up to 2995.
void DropFarmFood(float x, float y, int type, unsigned amount, Render::Texture* tex, unsigned dropId);
// @0x26c534: a collected pickup's sprite flies to the HUD (resources: the top bar or, XP, the
// level badge), optionally with a glow.
// @0x26c124: a copy of `sprite` flies (over `duration` seconds) to the screen point (x, y + its
// height), resizing to w x h (-1: keep). !screenSpace: the sprite is in the world. topLayer draws it
// on layer 0xd (else 0xf) and plays "building_position" on arrival; fadeOut fades the last 0.25 s.
void AddItemMovement(Render::Sprite* sprite, int x, int y, bool screenSpace, float duration, int w, int h,
                     bool topLayer, bool fadeOut);
void OnCollect(unsigned item, Render::Sprite* sprite, bool screenSpace, bool glow, int type, bool noBelt);
// @0x269f4c: a rising, fading text popup in the GUI font.
void ShowTextHover(float x, float y, const char32_t* text, float r, float g, float b, float glowR,
                   float glowG, float glowB, int fontSize, int glowBlur, float glowStrength,
                   bool nearest, float fade, float rise);
// @0x26b6b8: a text popup in a TextStyleManager style; returns the text height without its glow.
int ShowTextHoverWithStyle(float x, float y, const char32_t* text, int style, float fade, float rise,
                           bool topLayer, bool screenSpace);
// The helper arrow. ArrowAt @0x266cb8 points it at (x, y): hide hides it instead; left points it
// left (else down); mirror/flip mirror its sprite; screenSpace draws it in GUI space at the hover
// scale; newArrow adds another arrow instead of moving the last.
void ArrowAt(float x, float y, bool hide, bool left, bool mirror, bool flip, bool screenSpace, bool tablet,
             bool newArrow);
void HideArrow();                           // @0x2663d8 back to the one (hidden) arrow
void UpdateArrow();                         // @0x264df8 the bobbing
bool ArrowVisible();                        // @0x264a54
bool CanAutoHideArrow();                    // @0x26324c
int GetArrowClickCallbackID();              // @0x2631e8
void SetArrowVisibleWindowLimit(unsigned limit);   // @0x26330c the arrow hides above this many windows
// @0x264270: the last arrow calls `cb` (owned) when tapped; returns the new callback id.
int SetArrowClickCallback(GUI::Callback* cb);
// @0x266088: a tap on a visible arrow runs its callback (on the release) and hides the arrows.
// UNVERIFIED (tutorial): the +0x24..+0x2c fields it clears are not ported.
bool ClickOnArrow(int x, int y, bool pressed);
bool OnBuildingClick(Map::Building* b, int x, int y);   // @0x26726c the building's info window
int OnEntityClick(Entity* e, int x, int y);             // @0x26aafc 0 not taken, 1 taken, 2 go on
bool OnDecorClick(Map::Decor* d, int x, int y);         // @0x26a3b0
void OnBuildingAssignBuilder(Map::Building* b);   // @0x26b4a4
void OnResourceAssign(Map::Building* b);          // @0x266a24
void OnFreeWorkerAssign(Map::Building* b);        // @0x263b60
void OnFreeWorkerBuild();                         // @0x263ae4
void OnBuildingFinishedClick(Map::Building* b);   // @0x271208
void CreateDecorationDrop(Map::Decor* d);         // @0x270e08

}  // namespace BuildingHovers
