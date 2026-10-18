// Game dialogs (the windows beyond the HUD). Each is a set of static functions behind a
// WindowManager::FunctionalWindow, as on the original.
#pragma once
#include <cstdint>
#include <functional>

namespace WindowManager { class FunctionalWindow; }
namespace Map { struct Building; struct Decor; }
namespace Render { struct Texture; }
namespace Locations { struct LocationInfo; }
class Entity;

namespace CityRenameWindow {
WindowManager::FunctionalWindow* Queue();   // static FunctionalWindow "CityRenameWindow" (_INIT_ 0x298ffc)
void Init();                                // @0x29949c
void Deinit();                              // @0x29945c
void Show();                                // @0x299248
void Hide();                                // @0x2992ec
}

// "Who are you?": the hero's name and gender (windows/SelectHQWindow.cpp).
namespace SelectHQWindow {
WindowManager::FunctionalWindow* Queue();   // static FunctionalWindow "SelectHQWindow"
void Init();                                // @0x33a3d0
void Deinit();                              // @0x339b40
void Show();                                // @0x33ab58
void Hide();                                // @0x339694
bool IsVisible();                           // @0x338c28
void SetMode(int mode);                     // @0x338c3c 0 new game, 1 name, 2 name and gender
void SetOnContinue(void (*fn)());           // @0x338c14
}

namespace PopupSelectionWindow {
WindowManager::FunctionalWindow* Queue();   // static FunctionalWindow "PopupSelectionWindow" (_INIT_ 0x32c688)
void Init();                                // @0x32cc84
void Deinit();                              // @0x32cc44
// @0x32c7ec: OK alone -> one centred button; with Cancel -> two (Cancel only works with an OK).
// onExit runs on the close button or Back (null: just Hide).
void Show(const char32_t* title, const char32_t* text, const char32_t* okText, Render::Texture* okIcon,
          std::function<void()> onOk, const char32_t* cancelText, Render::Texture* cancelIcon,
          std::function<void()> onCancel, void (*onExit)() = nullptr);
void Hide();                                // @0x32cb50
bool IsVisible();                           // @0x32c36c
void SetMentorIcon();                       // @0x32c47c
void SetGoblinIcon();                       // @0x32c508
void SetCustomIcon(Render::Texture* tex);   // @0x32c594
void HideClose();                           // @0x32c65c
void SetFirstGreen();                       // @0x32c7b0
void SetSecondGreen();                      // @0x32c774
}

namespace Tasks { struct TaskInfo; }

namespace PopupWindow {
using Callback = void (*)();
WindowManager::FunctionalWindow* Queue();   // static FunctionalWindow "PopupWindow" (_INIT_ 0x32d02c)
void Init();                                // @0x32d9b0
void Deinit();                              // @0x32d964
// @0x32d630: OK only -> one centred button; OK and/or Cancel -> two; onExit adds the close button.
// okText / cancelText are StringTable keys replacing "HUD_OK" / "CANCEL".
void Show(const char32_t* text, Callback onOk, Callback onCancel, Callback onExit, const char* okText = nullptr,
          const char* cancelText = nullptr);
void Hide();                                // @0x32d79c
bool IsVisible();                           // @0x32d000
void ForceOnTop();                          // @0x32d014
}

namespace SettingsWindow {
WindowManager::FunctionalWindow* Queue();   // static FunctionalWindow "SettingsWindow" (_INIT_ 0x33def8)
void Init();                                // @0x33fae0
void Deinit();                              // @0x33e4bc
void Show();                                // @0x33fa5c
void Hide();                                // @0x33e1dc
bool IsVisible();                           // @0x33de04
void UpdateContents();                      // @0x33ea94
void EnableSync(bool on);                   // @0x33f944
int GetSoundMutedState();                   // @0x33de18
void SetSoundMutedState(int muted);         // @0x33de28
int GetMusicMutedState();                   // @0x33de3c
void SetMusicMutedState(int muted);         // @0x33de50
int GetNotificationState();                 // @0x33de78
void SetNotificationState(int on);          // @0x33de64
}

// The building and decoration shop.
namespace ShopWindow {
WindowManager::FunctionalWindow* Queue();   // static FunctionalWindow "ShopWindow" (_INIT_ 0x346ff4)
void Init();                                // @0x34a358
void Deinit();                              // @0x348f30
bool IsVisible();                           // @0x346d14 the strip or the info panel is up
void Show();                                // @0x34d73c
void Hide();                                // @0x3479b8
// @0x346dd4: the left edge of the shop's info panel (its offset 0x62d6e8 + the screen width - the
// panel's width).
int GetInfoPanelX();
void OnTabSelect(unsigned i);               // @0x34d570 the tab at index i
void OnSelectItemID(unsigned id);           // @0x34d67c open the tab holding building `id`
void ShowArrowOnItem(unsigned id);          // @0x34bd24 the helper arrow on that building's cell
void ShowBestOfTab(unsigned type);          // @0x34d99c
}

// "Spend n crystals?" before a crystal purchase.
namespace ConfirmPurchaseWindow {
WindowManager::FunctionalWindow* Queue();   // static FunctionalWindow "ConfirmPurchaseWindow" (_INIT_ 0x299a00)
void Init();                                // @0x29a034
void Deinit();                              // @0x299fb8
// @0x2999ac: the purchase (run on OK, once the dialog is gone), its price; skip runs it without asking.
void SetParameters(std::function<void()> cb, unsigned price, bool skip);
void Show();                                // @0x299e4c
void Hide();                                // @0x299f20
bool IsVisible();                           // @0x299964
}

// The treasury's exchange: gold packs for crystals, crystal packs for gold (PORT: the original
// sold both for real money).
namespace ExchangeWindow {
WindowManager::FunctionalWindow* Queue();   // static FunctionalWindow "ExchangeWindow" (_INIT_ 0x2abf44)
void Init();                                // @0x2af8bc
void Deinit();                              // @0x2af1ec
void Show();                                // @0x2af0ec
void Hide();                                // @0x2af198
bool IsVisible();                           // @0x2abd94
// @0x2aed8c: tab 0 gold, 1 crystals; text replaces the header (null: the tab's name).
void OnTab(unsigned tab, const char32_t* text);
}

// "Items found": the loot strip that slides up from the bottom when a map, task, campaign, raid,
// boss or chest hands out loot. The owner collects the loot here, shows the window and gives the
// loot with GiveCollectedToPlayer.
namespace FoundItemsWindow {
WindowManager::FunctionalWindow* Queue();   // static FunctionalWindow "FoundItemsWindow" (FUN_002b915c)
void Init();                                // @0x2bb214
bool IsVisible();                           // @0x2b8dfc
void Show();                                // @0x2baba8
void Hide();                                // @0x2b9804
void OnClose();                             // @0x2b98e4 Hide, and the loot dialogs under it
void OnShop();                              // @0x2bbf5c
void OnShare();                             // @0x2bbfc0 Ok (the PvP arena: its outro's buttons)
void SetZ(float z);                         // @0x2b99c4
void SetHeader(const char32_t* text);       // @0x2b8e10
void SetSharing(bool share);                // @0x2b8e40
void HideOk();                              // @0x2b8e58 "tap to continue" instead of Ok
bool HasPostAction();                       // @0x2b8e70
void RunPostAction();                       // @0x2b9748
void AddCollectedItem(uint32_t id, int amount);     // @0x2b9df4
void AddCollectedResource(int type, int amount);    // @0x2bcb10
void ClearCollected();                      // @0x2bc440
void UpdateContents();                      // @0x2ba138
// @0x2bbc40: drop on the ground at `at` (null: the player), else add; crystals capped at 15
// unless noCrystalCap. The list is emptied.
void GiveCollectedToPlayer(bool drop, Entity* at, bool noCrystalCap);
void SetParameters(const char* setting, const char32_t* title);              // @0x2bcd50
void SetParameters(uint32_t map, uint32_t boss, const char32_t* title);     // @0x2bd11c
// @0x2bcffc: a task's reward: `crystals` (when not 0), then its crystals, XP, gold and resources,
// then its items.
void SetParameters(Tasks::TaskInfo* task, const char32_t* title, unsigned crystals);
}

// "Land expanded!" after a land (or farm patch) purchase.
namespace LandExpandedWindow {
WindowManager::FunctionalWindow* Queue();   // static FunctionalWindow "LandExpandedWindow" (_INIT_ 0x2e7400)
void Init();                                // @0x2e79f4
void Deinit();                              // @0x2e79b4
void SetAreaParameters(uint32_t areaId);    // @0x2e73c8
void SetPatchParameters(uint32_t farmId, int patch);   // @0x2e73e0 (3f)
void Show();                                // @0x2e7778
void Hide();                                // @0x2e77f8
bool IsVisible();                           // @0x2e73b4
}

// "Where to next?": the destinations of the world (windows/ChoosePathWindow.cpp).
namespace ChoosePathWindow {
WindowManager::FunctionalWindow* Queue();   // static FunctionalWindow "ChoosePathWindow" (_INIT_ 0x295378)
void Init();                                // @0x2962b8
void UpdateContents();                      // @0x296c14
void Show();                                // @0x298df0
void Hide();                                // @0x295718
bool IsVisible();                           // @0x2952e0
void EnqueueShowOnLoad();                   // @0x2952f4 it opens after the next map load
void RunShowOnLoad();                       // @0x298f74 (Map::Load)
}

// A campaign's brief from its world-map node: progress, the items to find, Begin / Resume
// (windows/CampaignInfoWindow.cpp).
namespace CampaignInfoWindow {
WindowManager::FunctionalWindow* Queue();   // static FunctionalWindow "CampaignInfoWindow" (_INIT_ 0x272cd4)
void Init();                                // @0x275450
// @0x275148: the campaign, the map Start travels to (its finds are listed); the bool is kept but
// never read.
void SetParameters(Locations::LocationInfo* info, uint32_t map, bool param3);
void SetAnimationOnce();                    // @0x272c68 Start travels with the world map's animation
void ShowItem(uint32_t item);               // @0x274598 the next opening points at that item's cell
void UpdateContents();                      // @0x2731a8
void Show();                                // @0x274630
void Hide();                                // @0x2746d0
bool IsVisible();                           // @0x272c54
}

// The chapter banner: a chapter task opens it, closing it completes the task.
namespace ChapterWindow {
WindowManager::FunctionalWindow* Queue();   // static FunctionalWindow "ChapterWindow"
void Init();                                // @0x27ab64
void SetParameters(uint32_t task);          // @0x27a068
void Show();                                // @0x27a8f4
void Hide();                                // @0x27a9f0
void ForceHide();                           // @0x27a890
bool IsVisible();                           // @0x27a054
}

// Buying a piece of land (tapping its for-sale sign).
namespace LandWindow {
WindowManager::FunctionalWindow* Queue();   // static FunctionalWindow "LandWindow" (_INIT_ 0x2e7da0)
void Init();                                // @0x2e8888
void Deinit();                              // @0x2e8848
void SetAreaParameters(uint32_t areaId);    // @0x2e8cc0
void Show();                                // @0x2e82f0
void Hide();                                // @0x2e8370
bool IsVisible();                           // @0x2e7d8c
// @0x2e7ef4: the visited farm's next soil patch: its gold and crystal prices and level from the
// farm's farm_patch lists (indexed past the default patches).
void SetPatchParameters();
}

// "New level!": the level's unlocks and its crystal reward.
namespace LevelUpWindow {
WindowManager::FunctionalWindow* Queue();   // static FunctionalWindow "LevelUpWindow" (_INIT_ 0x2f1568)
void Init();                                // @0x2f1b78
void Deinit();                              // @0x2f1b38
void Show();                                // @0x2f3740 (waits in WindowManager::EnqueueWindow if needed)
void Hide();                                // @0x2f1968
bool IsVisible();                           // @0x2f1508
void UpdateContents();                      // @0x2f2398
}

// The "not enough" dialog: the missing requirements, Find buttons and "Buy all" for crystals.
namespace NotEnoughWindow {
WindowManager::FunctionalWindow* Queue();   // static FunctionalWindow "NotEnoughWindow" (_INIT_ 0x2f6f1c)
void Init();                                // @0x2fd9a4
void Deinit();                              // @0x2f7810
bool IsVisible();                           // @0x2f6ce8
void ResetRequirements();                   // @0x2f6cfc
void AddRequirement(int type, unsigned amount);   // @0x2f6ea4 a resource amount needed
void AddLevelRequirement(unsigned level);         // @0x2f6ebc
void AddReputationRequirement(unsigned profession, unsigned level);   // @0x2f6ed0 (the profession check: milestone 5)
void AddPopulaionRequirement(unsigned people);    // @0x2f6ee8 free people needed (sic)
void AddItemRequirement(unsigned id, unsigned count);            // @0x2f87bc
void AddBuildingCountRequirement(unsigned id, unsigned count);   // @0x2f8948
void AddBuildingLevelRequirement(unsigned id, unsigned level);   // @0x2f8ad4
void SetExchangeLimit(unsigned limit);            // @0x2f6efc the most Buy all may cost
void SetLevelFailMessage(const char32_t* text);   // @0x2f6dd0 replaces "SHOP_UNLOCK"
void SetGoldFailMesage(const char32_t* text);     // @0x2f6de4 replaces "NO_GOLD" (sic)
void SetUpgradableBuilding(Map::Building* b);     // @0x2f6df8
// @0x2f6e0c: an order that makes `item` (drop subtask `subtask`, at world point x, y) can be bought
// instead when its level is not reached (OnBuyItem drops it there).
void SetItemToProduce(uint32_t item, uint32_t subtask, float x, float y);
// @0x2f8c60: true when nothing is missing. UNVERIFIED (milestone 5): the profession requirement.
bool CheckRequirements();
void SetDescriptionText(const char32_t* text, const char32_t* title);   // @0x2f7134
void SetActionCallback(std::function<void()> cb, const char32_t* text, bool flag);   // @0x2f6e2c
void UpdateContents();                      // @0x2f8ecc
void Show();                                // @0x2fc7f8
void Hide();                                // @0x2f73f4
void RunLastHelpItem();                     // @0x2f8790
// @0x2f7a0c: the arrow to where a resource comes from (its building, else its shop cell); line
// and fromLine: the dialog's line that asked.
void FindResourceHelp(int type, unsigned line, bool fromLine);
}

// "Quest complete!": the quest's reward; closing it completes the quest.
namespace TaskCompleteWindow {
WindowManager::FunctionalWindow* Queue();   // static FunctionalWindow "TaskCompleteWindow"
void Init();                                // @0x353c20
void Deinit();                              // @0x353224
bool IsVisible();                           // @0x352730
void Complete(uint32_t task);               // @0x35327c fills the window for the task; Show opens it
void UpdateContents();                      // @0x353bf4
void Show();                                // @0x352d4c
void Hide();                                // @0x352f54 completes the task
}

// The full quest list (tabs All / Battle / City / Completed), opened from the bottom bar.
namespace TaskListWindow {
WindowManager::FunctionalWindow* Queue();   // static FunctionalWindow "TaskListWindow" (_INIT_ 0x35e7e4)
void Init();                                // @0x35f4ac
void UpdateContents();                      // @0x360908
void Show();                                // @0x361fcc
void Hide();                                // @0x35eb20
void RunLastHelpItem();                     // @0x35ecb0
void FindGoblin();                          // @0x361df4
void BuyGoblin();                           // @0x35eaf0
}

// "Item needed": craft the item in its building or buy it; also the quest/map-object item questions.
namespace TaskInfoWindow {
WindowManager::FunctionalWindow* Queue();   // static FunctionalWindow "TaskInfoWindow"
void Init();                                // @0x35d514
void Deinit();                              // @0x35af38
void SetZ(float z);                         // @0x359dc8
bool Click(int x, int y, bool pressed);     // @0x35ae38
void Update(float dt);                      // @0x35cee4
bool IsVisible();                           // @0x359e54
// @0x35afb4: fills the window for the task (boss: the boss form); Show opens it.
void SetTask(uint32_t task, bool boss);
void UpdateContents();                      // @0x35cecc
void Show();                                // @0x35a9cc (a quick task: its text line and action instead)
void Hide();                                // @0x35a490
void ForceHide();                           // @0x35a9a0 at once
void TaskTimeout(uint32_t task);            // @0x35ae00 closes when it shows that task
void BlockArrowLimitChangeOnce();           // @0x359e68
void SetFightBossCallback(Entity* boss);    // @0x359e80
void SetOnContinue(void (*cb)());           // @0x359e9c run once when the window is closed by its X
}

namespace NeedItemWindow {
WindowManager::FunctionalWindow* Queue();   // static FunctionalWindow "NeedItemWindow" (_INIT_ 0x2f474c)
void Init();                                // @0x2f6800
void Deinit();                              // @0x2f6754
bool IsVisible();                           // @0x2f46b4
void Show();                                // @0x2f65b4
void Hide();                                // @0x2f5e08
uint32_t GetBuildingThatProducesItem(uint32_t item, unsigned& slot);   // @0x2f4840
void SetParameters(uint32_t item, Map::Decor* decor);                  // @0x2f578c
void SetItemsToBeRemoved(unsigned task, std::function<void()>* cb);    // @0x2f48ec (cb owned)
void SetFindParameters(uint32_t item, std::function<void()>* cb);      // @0x2f4cf0 (cb owned)
void SetSearchParameters(uint32_t item, int resource, unsigned amount, std::function<void()>* onOk,
                         std::function<void()>* onCancel);             // @0x2f50c8 (owned)
void OnBuy();                               // @0x2f5f00
void OnProduce(uint32_t item);              // @0x2f619c (0: the dialog's item)
void ExecuteFirstHelp();                    // @0x2f6114
void SetLastHelpItem(unsigned id);          // @0x2f46c8
void EnableLastHelpItem();                  // @0x2f46dc
void RunLastHelpItem();                     // @0x2f6568
}

// CharacterInfoWindow: the hero's equipment and inventory window. Only the stat part of
// UpdateContents is ported yet (milestone 5: the window itself).
namespace CharacterInfoWindow {
// @0x28047c, the stat part: the player's stats again from its level (ResetStats, the HP bar too),
// its soldiers' (not HP), then the effects of the unbroken equipment (bindings 0..9) and
// customisations (1..9), the complete sets and the running buffs. Map::Load runs it, as do the
// item windows.
void UpdateContents();
inline bool IsVisible() { return false; }   // (0x27aeb4) the window itself: milestone 5
}

// The world map (windows/GlobalMapWindow.cpp).
class MetaData;
namespace GlobalMapWindow {
struct CoreLocation;
extern bool unlockAll;                      // GlobalMapWindow::unlockAll (debug; false)
WindowManager::FunctionalWindow* Queue();   // static FunctionalWindow "GlobalMapWindow" (FUN_002bdf48)
void Init();                                // @0x2c4f00
void Deinit();                              // @0x2bf438
void Show();                                // @0x2c448c
void Hide();                                // @0x2c0f70
bool IsVisible();                           // @0x2bdd28
void SetZ(float z);                         // @0x2c37b8
bool Click(int x, int y, bool pressed);     // @0x2c23f4
void Move(int x, int y);                    // @0x2c227c
void Wheel(bool up);                        // @0x2bdb7c
void Update(float dt);                      // @0x2c2728
void OnReturn();                            // @0x2c1440
void SwitchMap(uint32_t map, bool keep, bool animated);                             // @0x2c14b4
void SwitchMapAnimated(uint32_t map, bool keep, float pixelsPerSecond, bool ambush);   // @0x2c1734
void ShowTutorialAnimation(float unused, bool ambush, float t);                    // @0x2be6b8
void ShowPvPAnimation();                    // @0x2c4038
void ShowSultanMovement();                  // @0x2bdb80
void CenterOnMap(uint32_t map);             // @0x2c3ed0
void CenterOnSultan();                      // @0x2c3dc8
void CenterOnArrows();                      // @0x2c2324
void HandleMove(int dx, int dy, bool moveMarker);   // @0x2c1e08
void ArrowAtMap(uint32_t map, bool keep, bool center);   // @0x2c414c
void ArrowAt(int x, int y, bool keep);      // @0x2c6234
void HideArrow();                           // @0x2c0eb0
bool ExecuteArrowCallback();                // @0x2c0f14
unsigned GetCurrentArrowTarget();           // @0x2bdd54
void PreventArrowHide();                    // @0x2bdd10
void SetArrowVisibleWindowLimit(unsigned n);   // @0x2bdb98
void ExecuteFirstHelp();                    // @0x2be2f8
void AnimateSepiaIn();                      // @0x2be4f8
void AnimateSepiaOut();                     // @0x2be3c8
void GetMarkerLocationForMap(uint32_t map, int& x, int& y);   // @0x2bda40
uint32_t GetNearestMapToMap(uint32_t map);  // @0x2bdbac
bool ShowCampaignWindowForMap(uint32_t map);   // @0x2bdc4c
uint32_t GetMapForCraftItem(uint32_t item, uint32_t current);     // @0x2be7c0
uint32_t GetMissionMapForItem(uint32_t item, uint32_t current);   // @0x2be9ec
void FinishCampaign(uint32_t map);          // @0x2bedd0
void ResetCampaign(uint32_t map);           // @0x2c4944
unsigned GetCampaignTasks(Tasks::TaskInfo** out, unsigned max);   // @0x2bef14
bool LocationHasBeenCompletedByTask(const MetaData* endTasks, uint32_t task);   // @0x2befa4
void OnTaskCompleted(uint32_t task);        // @0x2bf168
void EnqueueArenaEventWindow(unsigned event);   // @0x2bdd78
CoreLocation* GetCoreLocation(unsigned i);  // @0x2bda0c
CoreLocation* GetSideLocation(unsigned i);  // @0x2bda24
}

// The banner before a boss, an arena wave or the tutorial's ambush (windows/BossIntroWindow.cpp).
namespace BossIntroWindow {
WindowManager::FunctionalWindow* Queue();   // static FunctionalWindow "BossIntroWindow" (FUN_00251830)
void Init();                                // @0x253720
void Deinit();                              // @0x2536bc
void Show();                                // @0x252c3c
void Hide();                                // @0x2532e0
void SetZ(float z);                         // @0x253474
bool Click(int x, int y, bool pressed);     // @0x25360c
void Update(float dt);                      // @0x252028
bool IsVisible();                           // @0x251770
bool IsBlackVisible();                      // @0x251784
void SetBossName(const char32_t* name);     // @0x2517ac
void SetWave(int wave, int total);          // @0x2517ec
int GetTotalWaves();                        // @0x251804
void SetAmbush();                           // @0x251818
}

// The arena's turn banner at the top (windows/ArenaTurnWindow.cpp).
namespace ArenaTurnWindow {
WindowManager::FunctionalWindow* Queue();   // static FunctionalWindow "ArenaTurnWindow"
void Init();                                // @0x249958
void Deinit();                              // @0x249918
void Show();                                // @0x249760
void Hide();                                // @0x2496f4
void SetZ(float z);                         // @0x249884
bool Click(int x, int y, bool pressed);     // @0x248d04
bool IsVisible();                           // @0x248d48
void UpdateContents();                      // @0x248d70
void SetIcon(int background, Render::Texture* icon);              // @0x248d5c
void SetNumber(int background, const char32_t* text, bool hurry);   // @0x2492f8
void SetHeader(const char32_t* text);       // @0x249508
void SetDescription(const char32_t* text);  // @0x24910c
int GetHorizontalPosition();                // @0x249070
}

// The PvP arena fight's intro and outro (windows/PlayerBattleInOutWindow.cpp).
namespace PlayerBattleInOutWindow {
WindowManager::FunctionalWindow* Queue();   // static FunctionalWindow "PlayerBattleInOutWindow" (FUN_002fe640)
void Show();                                // @0x2ffdfc
void Hide();                                // @0x2fe52c
void SetZ(float z);                         // @0x2ff148
bool Click(int x, int y, bool pressed);     // @0x3012f8
bool IsVisible();                           // @0x2fe558
float GetZ();                               // @0x2fe514
void UpdateContents();                      // @0x2ff2bc
void SetModeIntro();                        // @0x2fe56c
void SetModeOutro();                        // @0x2fe584
void SetParameters(uint32_t gid, bool won); // @0x2fe59c the opponent; won: it was beaten
bool EnemyDefeated();                       // @0x2fe5b4
void ScheduleRestart();                     // @0x2fe5c8
void ScheduleReturn();                      // @0x2ffd88
void OnContinue();                          // @0x2ffd5c
}

namespace GameState { struct PlayerItem; }
namespace SetInfoWindow {
GameState::PlayerItem* GetPlayerItemOrBetter(uint32_t id);   // @0x33b3d4
}

// The tavern and barracks: train and hire warriors, move soldiers between the squad and the reserve
// (windows/HireTroopsWindow.cpp).
namespace HireTroopsWindow {
WindowManager::FunctionalWindow* Queue();   // static FunctionalWindow "HireTroopsWindow"
void Init();                                // @0x2dae58
void Show();                                // @0x2d9650 (not in the opening tutorial nor an arena fight)
void Hide();                                // @0x2ceeac
bool IsVisible();                           // @0x2cde20
void ShowArrow();                           // @0x2cde34
// @0x2d99d8: opens on the tab of building id (0x8f, 0x8e, 0x69, 0x93, 0x609, 0x12); arrowOnBuild
// picks the first warrior the building does not train yet.
void ShowForBuilding(uint32_t id, bool arrowOnBuild);
void OnTab(unsigned tab, bool arrowOnBuild);   // @0x2d8f04
void UpdateContents();                      // @0x2d05e4
void ShowSlot(unsigned n);                  // @0x2cee94 the strip scrolls to card slot n
void ShowEntity(uint32_t entity);           // @0x2d913c the strip scrolls to the offer of that unit
void OnHintMove();                          // @0x2d8788 the tapped soldier changes sides (ItemHoverWindow's Move)
void OnHintDismiss();                       // @0x2cea28 asks before dismissing the tapped reserve soldier
bool ExecuteFirstHelp();                    // @0x2da9b4 the bot's first help (Train, else a refresh)
}
