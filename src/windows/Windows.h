// Game dialogs (the windows beyond the HUD). Each is a set of static functions behind a
// WindowManager::FunctionalWindow, as on the original.
#pragma once
#include <cstdint>
#include <functional>

namespace WindowManager { class FunctionalWindow; }
namespace Map { struct Building; }
namespace Render { struct Texture; }

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
void SetItemToProduce(uint32_t item, uint32_t amount, float x, float y);   // @0x2f6e0c (milestone 4)
// @0x2f8c60: true when nothing is missing. UNVERIFIED (milestone 4): the profession requirement.
bool CheckRequirements();
void SetDescriptionText(const char32_t* text, const char32_t* title);   // @0x2f7134
void SetActionCallback(std::function<void()> cb, const char32_t* text, bool flag);   // @0x2f6e2c
void UpdateContents();                      // @0x2f8ecc
void Show();                                // @0x2fc7f8
void Hide();                                // @0x2f73f4
void RunLastHelpItem();                     // @0x2f8790
}
