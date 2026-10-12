// Game dialogs (the windows beyond the HUD). Each is a set of static functions behind a
// WindowManager::FunctionalWindow, as on the original.
#pragma once
#include <cstdint>
#include <functional>

namespace WindowManager { class FunctionalWindow; }
namespace Map { struct Building; struct Decor; }
namespace Render { struct Texture; }
class Entity;

namespace CityRenameWindow {
WindowManager::FunctionalWindow* Queue();   // static FunctionalWindow "CityRenameWindow" (_INIT_ 0x298ffc)
void Init();                                // @0x29949c
void Deinit();                              // @0x29945c
void Show();                                // @0x299248
void Hide();                                // @0x2992ec
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
void AddReputationRequirement(unsigned profession, unsigned level);   // @0x2f6ed0 (milestone 4)
void AddPopulaionRequirement(unsigned people);    // @0x2f6ee8 free people needed (sic)
void AddItemRequirement(unsigned id, unsigned count);            // @0x2f87bc (milestone 4)
void AddBuildingCountRequirement(unsigned id, unsigned count);   // @0x2f8948
void AddBuildingLevelRequirement(unsigned id, unsigned level);   // @0x2f8ad4
void SetExchangeLimit(unsigned limit);            // @0x2f6efc the most Buy all may cost
void SetLevelFailMessage(const char32_t* text);   // @0x2f6dd0 replaces "SHOP_UNLOCK"
void SetGoldFailMesage(const char32_t* text);     // @0x2f6de4 replaces "NO_GOLD" (sic)
void SetUpgradableBuilding(Map::Building* b);     // @0x2f6df8
// @0x2f6e0c: an order that makes `item` (drop subtask `subtask`, at world point x, y) can be bought
// instead when its level is not reached (OnBuyItem drops it there).
void SetItemToProduce(uint32_t item, uint32_t subtask, float x, float y);
// @0x2f8c60: true when nothing is missing. UNVERIFIED (milestone 4): the profession requirement.
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
}
