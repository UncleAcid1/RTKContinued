// The city HUD: HUDWindow (the driver) and its panels. Each panel is a set of static functions behind
// a WindowManager::FunctionalWindow, as on the original.
#pragma once
#include <functional>
#include <cstdint>

namespace WindowManager { class FunctionalWindow; }

namespace Render { struct Texture; struct Sprite; }
class Entity;

namespace HUDWindow {
void Init();                  // @0x2c9a7c
void Show();                  // @0x2c909c
void SetZ(float z);           // @0x2c9158
float GetZ();                 // @0x2c6548
void UpdateFrameBorder();     // @0x2c76e0
void Update(float dt);        // @0x2c81c8
void UpdateTasks();            // @0x2c7ba8 (a jump to TaskHolderWindow::UpdateTasks)
void RemoveTaskArrows();       // @0x2c7bac
int UpdateNumberToTarget(int current, int target);   // @0x2c6560 (animated counters)
inline int GetDeltaTimeMultiplier() { return 1; }    // @0x2c6664
inline bool GetDebugState() { return false; }       // @0x2c665c (always 0 in 5.11)
WindowManager::FunctionalWindow* Queue();
// @0x2c6948: a line of text under the top bar (the second map-name popup), or none.
void SetInfoText(const char32_t* text);
// @0x2c8d64: which bottom bar shows. 0: the city's (when no shop or world map is open), 3: the
// farm's, anything else: none.
void SetBottomType(int type);
void EnterFarm();             // @0x2c9040 the farm view's HUD
// @0x2c7588: the big map-name popup in the screen's centre zooms in, then fades (HUDWindow::Update).
void ShowMapName(const char32_t* name);
void ExitFarm();              // @0x2c8ddc back to the city's
// The edge locators: the hero's (away from home, out of view) and the quest target's.
void HideLocators();                                  // @0x2c66bc
void UpdateScreenDarkening(float dt);                 // @0x2c6a74
void HideBlack();                                     // @0x2c67cc the dark cover goes at once
bool HideMapName();                                   // @0x2c6680
void HardMapReset(uint32_t map);                      // @0x2c96d4
void MapReset();                                      // @0x2c9714
void SoftMapReset();                                  // @0x2c97dc
void SetQuickLocatorForce(bool force);                // @0x2c670c
void SetTaskLocaterIcon(Render::Texture* icon, bool battle, bool side, bool quick);   // @0x2c680c
void ShowTaskLocaterTo(int x, int y, int tileX, int tileY);   // @0x2c814c
}

// Panels: Init/Show/Hide/SetZ/Update/IsVisible as on the original.
#define RTK_HUD_PANEL(Name)                                       \
    namespace Name {                                              \
    void Init(); void Show(); void Hide(); void SetZ(float z);    \
    void Update(float dt); bool IsVisible();                      \
    WindowManager::FunctionalWindow* Queue();                     \
    }
RTK_HUD_PANEL(TopCityWindow)
RTK_HUD_PANEL(PlayerTopWindow)
RTK_HUD_PANEL(CastleTopWindow)
RTK_HUD_PANEL(BeltBarWindow)
RTK_HUD_PANEL(BattleBarWindow)
RTK_HUD_PANEL(BottomCityWindow)
RTK_HUD_PANEL(TaskHolderWindow)
#undef RTK_HUD_PANEL

// The fight's HUD hooks (4g). Each is called by the fight code whenever an entity loses HP.
namespace HUDWindow {
void OnDamage(Entity* e);            // @0x2c69f8
inline void ShowMainUI(bool) {}      // @0x2c6658 (empty in 5.11)
}
namespace PlayerTopWindow {
void OnDamage(Entity* e);            // @0x327814 the hurt portrait
void RefreshHP();                    // @0x327564
// @0x3276c4: an item's icon flies into the portrait and the item takes effect on arrival.
void RaiseAndMoveIntoPlayer(float z, Render::Sprite* sprite, uint32_t item);
// The hero's own speech bubble (PlayerHintHoverWindow).
void SetParameters(const char32_t* text, uint32_t task, bool unused, bool noNext);   // @0x3278e0 noNext: no Next button
bool IsPlayerDialogVisible();        // @0x327520
void HideDialog();                   // @0x327890
void ShowArrowAndLock();             // @0x3278cc
}
namespace BattleBarWindow {
void OnDamage(Entity* e);            // @0x24b114
void LaunchFirstDeathHelper();       // @0x249dd4 the helper arrow after the first soldier died
bool DeathTutorialActive();          // @0x249c10
void HideHoverWindow();              // @0x249bd4 the soldier's card
void ShowSoldier(uint32_t id, bool help);   // @0x24a6a0
void UpdateContents();               // @0x24a750
}
namespace BeltBarWindow {
void UpdateContents();               // @0x24dc20
void UpdateBeltItems();              // @0x24d790 which items ride on the belt
void OnUseSlot(unsigned slot);       // @0x24f05c the slot's potion is drunk
void OnActivateSlot(unsigned slot);  // @0x24e944 the slot's orb goes on or off
void HideHoverWindow();              // @0x24c63c
}
// The campaign maps' bottom bar (hud/BottomBattleWindow.cpp; its net and stance parts so far,
// the spells with milestone 5).
namespace BottomBattleWindow {
WindowManager::FunctionalWindow* Queue();
void Init();                        // @0x25d4a8
void Deinit();                      // @0x259708
void Show();                        // @0x25cfd8
void Hide();                        // @0x259338
void SetZ(float z);                 // @0x2593e4
bool IsVisible();                   // @0x256e98
void SetOffset(float y);            // @0x256e7c
void SetSelectedButton(unsigned b); // @0x256eac the stance shown: 0 attack, 1 block, 3 auto, -1 none
bool IsInAttackStance();            // @0x2570b0
bool IsInBlockStance();             // @0x2570cc
void SetEnabled(bool on, bool opponentTurn, bool autoCombat);   // @0x256ec0 (only `on` is used)
void ShowAttackButton(unsigned task, bool lock, bool arrow);    // @0x257490
void ShowBlockButton(unsigned task, bool lock, bool arrow);     // @0x257388
void ShowMagicButton(unsigned task, bool lock, bool arrow);     // @0x25d3bc
void EnableNet(uint32_t spawn);     // @0x258f30 a dying monster may be tamed
void TameNext();                    // @0x25710c
uint32_t GetNetTargetSpawnID();     // @0x257090
void OnCenter();                    // @0x2588b4 the centre button (the net)
}
// The "Heal Hero" window (windows/HealSquadWindow.cpp).
namespace HealSquadWindow {
WindowManager::FunctionalWindow* Queue();
void Init();                         // @0x2cd3c0
void Show();                         // @0x2cd1d8 (not in the arenas)
void Hide();                         // @0x2cafa0
void UpdateContents();               // @0x2cb3ec
bool IsVisible();                    // @0x2c9f10
void SetAttacker(Entity* e);         // @0x2c9efc who felled the hero
void SetHealPostAction(std::function<void()> cb);   // @0x2c9f28 run when it closes with him alive
void ResetRecentDeaths();            // @0x2c9f6c
void UseItem(uint32_t id);            // @0x2cc24c one of the item flies to the hero's figure and is drunk
}

namespace BattleBarWindow {
// The "all quests" button above the army bar: shown from tutorial step 0x66 on, outside the
// farm, the shop and a boss fight.
void ShowAllTasks();   // @0x24b660
void HideAllTasks();   // @0x249b90
}

namespace BeltBarWindow {
// @0x24c738: the screen point of belt slot `slot`'s holder.
void GetBeltItemLocation(int& x, int& y, int slot);
// @0x24c6b8: where a w x h image centred on the inventory button goes (half the belt further left
// while the belt is hidden).
void GetAdjustedSideButtonLocation(int& x, int& y, int w, int h);
// @0x24dafc: an unlocked orb (type 3) the player has, not on the belt yet, goes to the first empty
// slot (or one whose item ran out; not slot 0 at second-tutorial step 0x94); an activatable one
// starts its activation time (not at step 0x85).
void AutoBind(uint32_t id);
}

// The farm view's bottom bar (Farm_buttons_container.xml): Back to the city, the main action
// (plant / harvest / clear / clean up, whatever the patches need), Upgrade and the powder Speed
// Up, plus the sign posts to the previous and next farm. Port of BottomFarmWindow, 0x25fb60..
// 0x262490; statics at 0x6186b0.
namespace BottomFarmWindow {
WindowManager::FunctionalWindow* Queue();   // static FunctionalWindow "BottomFarmWindow"
void Init();                  // @0x261fdc
void Deinit();                // @0x261d00
void Show();                  // @0x2618e0
void Hide();                  // @0x2617f4
bool IsVisible();             // @0x25fb60
void HelpWithUpgrade();       // @0x25fd30 the helper arrow at the Upgrade button
void* GetPlantButton();       // @0x25fb74 the main button's click area
void ShowPatchSpeedUp(unsigned patch);   // @0x26018c the patch's progress box in speed-up mode
void OnFarmBack();            // @0x260cc0
}

namespace TopCityWindow {
bool Click(int x, int y, bool pressed);   // @0x365908 (NotEnoughWindow passes its clicks here first)
}

namespace TaskHolderWindow {
void SetTaskVisibility(bool on);   // @0x359648 (hidden while buildings are edited)
void UpdateTasks();                // @0x356048 the list is made from the open tasks
void UpdateContents();             // @0x358c70 icons and time left
bool Click(int x, int y, bool pressed);   // @0x354d8c
void ResetTasks();                 // @0x355eb8 every button freed (a new game, another save)
void RemoveTaskArrows();           // @0x354ad8 the banners go, the first is reset
// @0x355098 / @0x354674: the line of text that folds out at the bottom for `seconds`.
void ShowQuickTaskDescription(const char32_t* text, float seconds);
void HideQuickTaskDescription();
}

namespace CastleTopWindow {
inline void HideTools() {}         // @0x2769cc (empty in 5.11)
void OnFindPlayer();               // @0x2772e8
}

namespace BottomCityWindow {
void ShowMainButton();        // @0x25e95c
void HideMainButton();        // @0x25e860
void ShowInstruments();       // @0x25e68c
void HideInstruments();       // @0x25e564
void UpdateContents();        // @0x25df84
// Called by BuildingMovement while a building is being edited.
void EnableUndoButton(bool on);     // @0x25e514
void EnableRotateButton(bool on);   // @0x25e528
void EnableCancelButton(bool on);   // @0x25e53c
void EnableOkButton(bool on);       // @0x25e550
}

// The tutorial's script (hud/TutorialWindow.cpp).
namespace TutorialWindow {
WindowManager::FunctionalWindow* Queue();
void ResetState();                   // @0x366230
void WaitUntil(uint32_t time);       // @0x366268
}

// The tutorial steps reported to the server (online: only the bookkeeping is kept).
namespace TutorialSender {
void SetTimeLimit(float seconds);    // @0x23ee5c
void AddTutorialStep(int step);      // @0x23ee70
void Update(float dt);               // @0x23ee90
}
