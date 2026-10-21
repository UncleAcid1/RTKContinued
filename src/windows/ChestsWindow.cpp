// ChestsWindow: the chests window (Window_chests.xml): the four store chests (architect, warrior,
// hunter, collector) in selectors down the side, the selected chest's contents (a scrolled reward
// line, or the five-of-a-kind sets that win its ultimate prize), its luck circle, and the buttons to
// preview, buy / open, reroll and collect; the starter chest opens here too ("starter mode"). A
// chest's result (the items rolled) comes back as reward items and is shown in the reward line until
// collected.
// Port of ChestsWindow (libkingdom.so 5.11), 0x28979c..0x294f84. Statics at 0x61ad14..0x61c1d0 and
// 0x60f1b8..0x60f1c4.
//
// DECIDED (user 2026-10-19): the chests are bought for crystals (see Chests::Init); the real-money
// purchase (OnPaymentDataReceived, Billing) is not ported.
// PORT (online removed): the daily chest's server request (GameState::IsDailyChestRequestProcessing
// is false offline, LoadingInternetWindow and the failure popups "FB_ONLINE_NOTICE" are not
// ported) and Show's expansion-pack check (Expansions::AreEnabled is false offline).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "engine/SoundsManager.h"
#include "engine/IconManager.h"
#include "engine/Render.h"
#include "engine/Resources.h"
#include "engine/Timer.h"
#include "game/BuildingHovers.h"
#include "game/Chests.h"
#include "game/EntityData.h"
#include "game/GameData.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/Map.h"
#include "game/MetaData.h"
#include "game/MetaExpression.h"
#include "game/Rand48.h"
#include "game/Setting.h"
#include "game/StringTable.h"
#include "game/Tasks.h"
#include "gui/GUI.h"
#include "gui/WindowManager.h"
#include "hud/ContentScroller.h"
#include "hud/HUD.h"
#include "hud/ItemHoverWindow.h"
#include "hud/Shared.h"
#include "windows/Windows.h"

namespace ChestsWindow {
namespace {

using GUI::Textfield;
using GUI::Window;

// @0x28b060 ChestWindowHolder (0x28 bytes): a chest's picture, open (4 frames) or closed (5).
struct ChestWindowHolder {
    Window* root = nullptr;           // +0x00 "%s"
    Window* open[4] = {};             // +0x04 chest_01_open..04
    Window* closed[5] = {};           // +0x14 chest_01_closed..05

    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        for (int i = 0; i != 4; ++i) open[i] = GUI::GetWindowTypedF<Window>(w, "%s.chest_%02d_open", p, i + 1);
        for (int i = 0; i != 5; ++i) closed[i] = GUI::GetWindowTypedF<Window>(w, "%s.chest_%02d_closed", p, i + 1);
    }
};

// @0x28b0fc ChestExpireHolder (0x14 bytes): the time left of an offer.
struct ChestExpireHolder {
    Window* root = nullptr;           // +0x00 "%s"
    Window* timer = nullptr;          // +0x04 timer_60 (takes a depth slot)
    Textfield* timeLeft = nullptr;    // +0x08 text_time_left
    Textfield* time = nullptr;        // +0x0c text_time
    Textfield* expired = nullptr;     // +0x10 text_time_expired

    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        timer = GUI::GetWindowTypedF<Window>(w, "%s.timer_60", p);
        timer->takesZ = true;
        timeLeft = GUI::GetWindowTypedF<Textfield>(w, "%s.text_time_left", p);
        time = GUI::GetWindowTypedF<Textfield>(w, "%s.text_time", p);
        expired = GUI::GetWindowTypedF<Textfield>(w, "%s.text_time_expired", p);
    }
};

// @0x28b1a0 LuckHolder (0x38 bytes): the luck circle, its colour by the best luck item, and the four
// percents.
struct LuckHolder {
    enum BackgroundType { kPurple = 0, kOrange = 1, kBlue = 2, kRed = 3 };
    Window* root = nullptr;           // +0x00 "%s"
    Window* circleHolder = nullptr;   // +0x04 luck_circle_holder
    Window* circles[5] = {};          // +0x08 its luck_circle_purple, _orange, _blue, _red, _red again
    Textfield* luck = nullptr;        // +0x1c text_luck_01
    Textfield* percent[4] = {};       // +0x20 text_luck_percent_04, _03, _02, _01
    Textfield* percent1 = nullptr;    // +0x30 text_luck_percent_01 too
    Window* icon = nullptr;           // +0x34 icon_luck (takes a depth slot)

    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        circleHolder = GUI::GetWindowTypedF<Window>(w, "%s.luck_circle_holder", p);
        circles[0] = GUI::GetWindowTypedF<Window>(w, "%s.luck_circle_holder.luck_circle_purple", p);
        circles[1] = GUI::GetWindowTypedF<Window>(w, "%s.luck_circle_holder.luck_circle_orange", p);
        circles[2] = GUI::GetWindowTypedF<Window>(w, "%s.luck_circle_holder.luck_circle_blue", p);
        circles[3] = GUI::GetWindowTypedF<Window>(w, "%s.luck_circle_holder.luck_circle_red", p);
        circles[4] = GUI::GetWindowTypedF<Window>(w, "%s.luck_circle_holder.luck_circle_red", p);
        luck = GUI::GetWindowTypedF<Textfield>(w, "%s.text_luck_01", p);
        percent[0] = GUI::GetWindowTypedF<Textfield>(w, "%s.text_luck_percent_04", p);
        percent[1] = GUI::GetWindowTypedF<Textfield>(w, "%s.text_luck_percent_03", p);
        percent[2] = GUI::GetWindowTypedF<Textfield>(w, "%s.text_luck_percent_02", p);
        percent[3] = percent1 = GUI::GetWindowTypedF<Textfield>(w, "%s.text_luck_percent_01", p);
        icon = GUI::GetWindowTypedF<Window>(w, "%s.icon_luck", p);
        icon->takesZ = true;
    }
    // @0x28979c: the circle of that colour (UpdateContents asks for the fifth, a second red, when no
    // luck item is owned).
    void SetBackground(BackgroundType type) {
        for (int i = 0; i != 5; ++i) circles[i]->SetVisibility((int)type == i);
    }
};

// @0x28b328 ChestOpenPile (0x34 bytes): the opened chest with the days of its offer.
struct ChestOpenPile {
    Window* root = nullptr;           // +0x00 "%s"
    ChestWindowHolder chest;          // +0x04 chest_win_holder
    Textfield* day1 = nullptr;        // +0x2c text_day_01
    Textfield* day2 = nullptr;        // +0x30 text_day_02

    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        char sub[0x100];
        std::snprintf(sub, sizeof sub, "%s.chest_win_holder", p);
        chest.LoadFrom(w, sub);
        day1 = GUI::GetWindowTypedF<Textfield>(w, "%s.text_day_01", p);
        day2 = GUI::GetWindowTypedF<Textfield>(w, "%s.text_day_02", p);
    }
};

// @0x28b3ec ChestFiveWarriors (0x488 bytes): five soldiers that make the hunter chest's prize, with
// the gold "+" or tick of each.
struct ChestFiveWarriors {
    Window* root = nullptr;           // +0x00 "%s"
    Shared::WarriorSlotInfo slots[5]; // +0x004 slot_warrior_01..05
    uint32_t entities[5] = {};        // +0x450
    Window* add[4] = {};              // +0x464 icon_gold_add_01..04 (take a depth slot)
    Window* confirm[5] = {};          // +0x474 icon_gold_confirm_01..05 (take a depth slot)

    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        char sub[0x100];
        for (int i = 0; i != 5; ++i) {
            std::snprintf(sub, sizeof sub, "%s.slot_warrior_%02d", p, i + 1);
            slots[i].LoadFrom(w, sub);
        }
        for (uint32_t& e : entities) e = 0;
        for (int i = 0; i != 4; ++i) {
            add[i] = GUI::GetWindowTypedF<Window>(w, "%s.icon_gold_add_%02d", p, i + 1);
            add[i]->takesZ = true;
        }
        for (int i = 0; i != 5; ++i) {
            confirm[i] = GUI::GetWindowTypedF<Window>(w, "%s.icon_gold_confirm_%02d", p, i + 1);
            confirm[i]->takesZ = true;
        }
    }
};

// @0x28b558 ChestFiveItems (0x2e4 bytes): five items that make a chest's prize.
struct ChestFiveItems {
    Window* root = nullptr;           // +0x00 "%s"
    Shared::ItemHolder items[5];      // +0x004 slot_01..05.inventory_item_icon
    uint32_t ids[5] = {};             // +0x2ac
    Window* add[4] = {};              // +0x2c0 icon_gold_add_01..04 (take a depth slot)
    Window* confirm[5] = {};          // +0x2d0 icon_gold_confirm_01..05 (take a depth slot)

    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        char sub[0x100];
        for (int i = 0; i != 5; ++i) {
            std::snprintf(sub, sizeof sub, "%s.slot_%02d.inventory_item_icon", p, i + 1);
            items[i].LoadFrom(w, sub, false);
        }
        for (uint32_t& id : ids) id = 0;
        for (int i = 0; i != 4; ++i) {
            add[i] = GUI::GetWindowTypedF<Window>(w, "%s.icon_gold_add_%02d", p, i + 1);
            add[i]->takesZ = true;
        }
        for (int i = 0; i != 5; ++i) {
            confirm[i] = GUI::GetWindowTypedF<Window>(w, "%s.icon_gold_confirm_%02d", p, i + 1);
            confirm[i]->takesZ = true;
        }
    }
};

// @0x28af5c ChestFiveObjects (0x50 bytes): five buildings (statues) that make the architect
// chest's prize.
struct ChestFiveObjects {
    Window* root = nullptr;           // +0x00 "%s"
    Window* icons[5] = {};            // +0x04 icon_statue_01..05 (take a depth slot)
    uint32_t ids[5] = {};             // +0x18
    Window* add[4] = {};              // +0x2c icon_gold_add_01..04 (take a depth slot)
    Window* confirm[5] = {};          // +0x3c icon_gold_confirm_01..05 (take a depth slot)

    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        for (int i = 0; i != 5; ++i) {
            icons[i] = GUI::GetWindowTypedF<Window>(w, "%s.icon_statue_%02d", p, i + 1);
            icons[i]->takesZ = true;
        }
        for (uint32_t& id : ids) id = 0;
        for (int i = 0; i != 4; ++i) {
            add[i] = GUI::GetWindowTypedF<Window>(w, "%s.icon_gold_add_%02d", p, i + 1);
            add[i]->takesZ = true;
        }
        for (int i = 0; i != 5; ++i) {
            confirm[i] = GUI::GetWindowTypedF<Window>(w, "%s.icon_gold_confirm_%02d", p, i + 1);
            confirm[i]->takesZ = true;
        }
    }
};

// @0x28b6b8 ChestSelectorHolder (0x70 bytes): a chest in the side list, with four animated
// sparkles ("bless") at random frames and phases.
struct ChestSelectorHolder {
    Window* root = nullptr;           // +0x00 "%s"
    Window* bg = nullptr;             // +0x04 chest_holder_bg
    Window* activeBg = nullptr;       // +0x08 chest_holder_active_bg_mobile
    Textfield* name1 = nullptr;       // +0x0c text_chest_name_01
    Textfield* name2 = nullptr;       // +0x10 text_chest_name_02
    Window* typeIcon = nullptr;       // +0x14 icon_chest_type (takes a depth slot)
    ChestWindowHolder chest;          // +0x18 chest_win_holder
    Window* sparkles[4] = {};         // +0x40 sparkle_01..04
    int sparkleFrame[4] = {};         // +0x50
    float sparkleTime[4] = {};        // +0x60

    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        bg = GUI::GetWindowTypedF<Window>(w, "%s.chest_holder_bg", p);
        activeBg = GUI::GetWindowTypedF<Window>(w, "%s.chest_holder_active_bg_mobile", p);
        name1 = GUI::GetWindowTypedF<Textfield>(w, "%s.text_chest_name_01", p);
        name2 = GUI::GetWindowTypedF<Textfield>(w, "%s.text_chest_name_02", p);
        typeIcon = GUI::GetWindowTypedF<Window>(w, "%s.icon_chest_type", p);
        typeIcon->takesZ = true;
        char sub[0x100];
        std::snprintf(sub, sizeof sub, "%s.chest_win_holder", p);
        chest.LoadFrom(w, sub);
        for (int i = 0; i != 4; ++i) {
            sparkles[i] = GUI::GetWindowTypedF<Window>(w, "%s.sparkle_%02d", p, i + 1);
            sparkles[i]->SetTexture(Resources::GetImage("images/Buildings/mk/bless/bless"), true, 0, 0, false, 0);
        }
        for (int i = 0; i != 4; ++i)
            sparkleFrame[i] = (int)(Rand48::lrand48() % Resources::GetFrameCount("images/Buildings/mk/bless/bless"));
        for (int i = 0; i != 4; ++i) sparkleTime[i] = (float)(Rand48::lrand48() % 100) / 10.f;
    }
};

// @0x28ba00 ChestRewardLine (0x720 bytes): a scrolled line of eleven item cells and eleven object
// icons (what the chest holds or gave), with its arrows and scroll bar.
struct ChestRewardLine {
    Window* root = nullptr;           // +0x000 "%s"
    Shared::ItemHolder items[11];     // +0x004 slot_01..11.inventory_item_icon
    Window* objects[11] = {};         // +0x5dc icon_object_01..11 (take a depth slot)
    Window* arrowLeft = nullptr;      // +0x608 arrow_left
    Window* arrowRight = nullptr;     // +0x60c arrow_right
    int f610 = 0;                     // +0x610
    Shared::ContentScroller scroller; // +0x614 (its windows are the cells' slots and the objects)
    Window* scrollBarBg = nullptr;    // +0x68c scroll_bar_chests (the scroller's +0x78)

    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        char sub[0x100];
        for (int i = 0; i != 11; ++i) {
            std::snprintf(sub, sizeof sub, "%s.slot_%02d.inventory_item_icon", p, i + 1);
            items[i].LoadFrom(w, sub, false);
        }
        // The eleventh cell and object follow the tenth by the cells' spacing.
        items[10].root->MoveWindow(items[1].root->x - items[0].root->x, 0);
        for (int i = 0; i != 11; ++i) {
            objects[i] = GUI::GetWindowTypedF<Window>(w, "%s.icon_object_%02d", p, i + 1);
            objects[i]->takesZ = true;
        }
        objects[10]->MoveWindow(objects[1]->x - objects[0]->x, 0);
        arrowLeft = GUI::GetWindowTypedF<Window>(w, "%s.arrow_left", p);
        arrowRight = GUI::GetWindowTypedF<Window>(w, "%s.arrow_right", p);
        f610 = 0;
        scroller.Init();
        Window* mask = GUI::GetWindowTypedF<Window>(w, "%s.mask_universal", p);
        scroller.track = mask;
        scroller.viewport = {mask->x, mask->y, mask->x + mask->w, mask->y + mask->h};
        for (Shared::ItemHolder& it : items) {
            it.root->SetClipRect(&scroller.viewport);
            scroller.windows.push_back(it.root->parent);
        }
        for (Window* o : objects) {
            o->SetClipRect(&scroller.viewport);
            scroller.windows.push_back(o);
        }
        scrollBarBg = GUI::GetWindowTypedF<Window>(w, "%s.scroll_bar_chests", p);
        scroller.scrollbar = GUI::GetWindowTypedF<Window>(w, "%s.scroll_bar_chests.scroll_bar_tiny", p);
        scroller.scrollbar->SetTexture(Resources::GetDirectImage("images/scroll.png"), false, 0, 0, false, 0);
        scroller.perLine = 1;
        scroller.itemSize = items[1].root->x - items[0].root->x;
        scroller.onScroll = UpdateContents;
        scroller.unkA0 = 11;
        scroller.horizontal = true;
    }
};

// @0x28be40 ChestItemHolder (0x10a8 bytes): the selected chest's panel.
struct ChestItemHolder {
    Window* root = nullptr;           // +0x000 "%s"
    Window* arrowed = nullptr;        // +0x004 arrowed_itemiser
    Window* noArrowBg = nullptr;      // +0x008 sliced_crem_bg_no_arrow
    Window* bg = nullptr;             // +0x00c sliced_crem_bg
    Textfield* containLarge = nullptr;    // +0x010 text_contain_large
    Textfield* noChest = nullptr;     // +0x014 text_no_chest_selected
    Window* navLeft = nullptr;        // +0x018 button_navigate_left
    Window* navRight = nullptr;       // +0x01c button_navigate_right
    Window* ultiBg = nullptr;         // +0x020 bg_ulti_reward
    Textfield* ultiLong = nullptr;    // +0x024 text_ulti_long
    Textfield* name[5] = {};          // +0x028 item_class_holder_med.text_item_class_01..05
    Textfield* contain = nullptr;     // +0x03c text_contain
    Window* ultiBg2 = nullptr;        // +0x040 bg_ulti_reward (again)
    Window* statIcon[2] = {};         // +0x044 info_stat_1_line.icon, info_stat_2_line.icon (take a depth slot)
    Textfield* statText[2] = {};      // +0x04c info_stat_1_line.text_stat, info_stat_2_line.text_stat
    Textfield* needToGet = nullptr;   // +0x054 text_need_to_get
    ChestRewardLine line;             // +0x058 chest_reward_line
    ChestFiveItems fiveItems;         // +0x778 chest_need_5_items
    ChestFiveWarriors fiveWarriors;   // +0xa5c chest_need_5_troops
    ChestFiveObjects fiveObjects;     // +0xee4 chest_need_5_objects
    Shared::WarriorSlotInfo ultiWarrior;  // +0xf34 slot_ulti_warrior
    uint32_t ultiEntity = 0;          // +0x1010
    Shared::ItemHolder ultiItem;      // +0x1014 slot_ulti_item.inventory_item_icon
    uint32_t ultiItemId = 0;          // +0x109c
    Window* ultiStatue = nullptr;     // +0x10a0 icon_statue_ulti (takes a depth slot)
    uint32_t ultiStatueId = 0;        // +0x10a4

    void LoadFrom(Window* w, const char* p) {
        root = GUI::GetWindowTypedF<Window>(w, "%s", p);
        arrowed = GUI::GetWindowTypedF<Window>(w, "%s.arrowed_itemiser", p);
        Window* left = GUI::GetWindowTypedF<Window>(w, "%s.arrowed_itemiser.sliced_crem_bg_left", p);
        Window* right = GUI::GetWindowTypedF<Window>(w, "%s.arrowed_itemiser.sliced_crem_bg_right", p);
        left->SetSize((unsigned)left->w, (unsigned)(left->h + 1));
        right->SetSize((unsigned)right->w, (unsigned)(right->h + 1));
        noArrowBg = GUI::GetWindowTypedF<Window>(w, "%s.sliced_crem_bg_no_arrow", p);
        bg = GUI::GetWindowTypedF<Window>(w, "%s.sliced_crem_bg", p);
        containLarge = GUI::GetWindowTypedF<Textfield>(w, "%s.text_contain_large", p);
        noChest = GUI::GetWindowTypedF<Textfield>(w, "%s.text_no_chest_selected", p);
        navLeft = GUI::GetWindowTypedF<Window>(w, "%s.button_navigate_left", p);
        navRight = GUI::GetWindowTypedF<Window>(w, "%s.button_navigate_right", p);
        ultiBg = GUI::GetWindowTypedF<Window>(w, "%s.bg_ulti_reward", p);
        ultiLong = GUI::GetWindowTypedF<Textfield>(w, "%s.text_ulti_long", p);
        for (int i = 0; i != 5; ++i)
            name[i] = GUI::GetWindowTypedF<Textfield>(w, "%s.item_class_holder_med.text_item_class_%02d", p, i + 1);
        contain = GUI::GetWindowTypedF<Textfield>(w, "%s.text_contain", p);
        ultiBg2 = GUI::GetWindowTypedF<Window>(w, "%s.bg_ulti_reward", p);
        for (int i = 0; i != 2; ++i) {
            statIcon[i] = GUI::GetWindowTypedF<Window>(w, "%s.info_stat_%d_line.icon", p, i + 1);
            statIcon[i]->takesZ = true;
        }
        for (int i = 0; i != 2; ++i)
            statText[i] = GUI::GetWindowTypedF<Textfield>(w, "%s.info_stat_%d_line.text_stat", p, i + 1);
        needToGet = GUI::GetWindowTypedF<Textfield>(w, "%s.text_need_to_get", p);
        char sub[0x100];
        std::snprintf(sub, sizeof sub, "%s.chest_reward_line", p);
        line.LoadFrom(w, sub);
        std::snprintf(sub, sizeof sub, "%s.chest_need_5_items", p);
        fiveItems.LoadFrom(w, sub);
        std::snprintf(sub, sizeof sub, "%s.chest_need_5_troops", p);
        fiveWarriors.LoadFrom(w, sub);
        std::snprintf(sub, sizeof sub, "%s.chest_need_5_objects", p);
        fiveObjects.LoadFrom(w, sub);
        std::snprintf(sub, sizeof sub, "%s.slot_ulti_warrior", p);
        ultiWarrior.LoadFrom(w, sub);
        ultiEntity = 0;
        std::snprintf(sub, sizeof sub, "%s.slot_ulti_item.inventory_item_icon", p);
        ultiItem.LoadFrom(w, sub, false);
        ultiItemId = 0;
        ultiStatue = GUI::GetWindowTypedF<Window>(w, "%s.icon_statue_ulti", p);
        ultiStatueId = 0;
        ultiStatue->takesZ = true;
    }
};

// A cell of the reward line (0x20 bytes): what a chest gave (AddRewardItem) or holds (the preview,
// the starter chest): a resource, an item or an object.
struct RewardItemInfo {
    uint32_t type = 0xb;              // +0x00 GameState::ResourceType, 0xb an item or object
    uint32_t item = 0;                // +0x04 an item (its card)
    uint32_t object = 0;              // +0x08 an object that is no decoration
    uint32_t decoration = 0;          // +0x0c a decoration (its picture)
    int amount = 1;                   // +0x10 (0: no quantity shown)
    uint32_t amountMin = 0;           // +0x14 the preview's range and chance
    uint32_t amountMax = 0;           // +0x18
    int probability = 0;              // +0x1c
};

// The store's chests in the selectors' order (0x592b8c; the word before it is 0x676e) and the settings naming their prizes'
// sets (0x6016f0: five ids, then the prize "id|amount").
const uint32_t kChests[4] = {0x1b, 0x1c, 0x1d, 0x1e};
const char* const kPrizeSettings[4] = {"architect_chest", "warrior_chest", "hunter_chest", "collector_chest"};
const char* const kSparkle = "images/Buildings/mk/bless/bless";

WindowManager::FunctionalWindow* g_queue = nullptr;   // ChestsWindow::wnd 0x61c1d4
Window* g_root = nullptr;             // 0x61ad14
GUI::TweenEffect* g_tween = nullptr;  // 0x61ad18
ChestItemHolder g_items;              // 0x61ad1c chests_items_holder
bool g_blockClicks = false;           // 0x61bdc4
LuckHolder g_luck;                    // 0x61bdc8 luck_holder
ItemHoverWindow* g_hover = nullptr;   // 0x61be00
std::vector<RewardItemInfo> g_rewards;    // 0x61be04 the result shown (sgl::vector, one inline)
bool g_collecting = false;            // 0x61be30 a result waits to be collected
bool g_starterMode = false;           // 0x61be31
bool g_starterFlag = false;           // 0x61be32 SetStarterChestMode's first argument
uint32_t g_starterChest = 0;          // 0x61be34
bool g_prizeFlight = false;           // 0x61be38 a set is complete: its five fly to the prize (blocks the clicks)
bool g_ultimatePending = false;       // 0x61be39 the ultimate prize shows 2 s after it is won
ChestSelectorHolder g_selectors[4];   // 0x61be3c chest_selector_holder_01..04
ChestOpenPile g_pile;                 // 0x61bffc chest_open_pile
Window* g_chains[8] = {};             // 0x61c030 chain_01..08
std::vector<RewardItemInfo> g_pending;    // 0x61c050 a result not shown yet (AddRewardItem)
Window* g_close = nullptr;            // 0x61c07c x_button
Textfield* g_title = nullptr;         // 0x61c080 text_title
Window* g_chainBorder = nullptr;      // 0x61c084 chain_border_holder_chests
Window* g_darkBg = nullptr;           // 0x61c088 sliced_dark_bg
Window* g_luxBg = nullptr;            // 0x61c08c sliced_lux_bg
Window* g_back = nullptr;             // 0x61c090 back_simple
Window* g_separator = nullptr;        // 0x61c094 sepparator_chests
ChestExpireHolder g_expire;           // 0x61c098 chest_expire_holder
Shared::ButtonRaidStart g_previewButton;  // 0x61c0ac button_raid_start_02
Shared::ButtonRaidStart g_openButton;     // 0x61c0f0 button_raid_start
Shared::ButtonRaidStart g_rerollButton;   // 0x61c134 button_raid_start_04
Shared::ButtonRaidStart g_collectButton;  // 0x61c178 button_raid_start_centre
bool g_preview = false;               // 0x61c1bc the contents preview
int g_background = 0;                 // 0x61c1c0 0..3 (OnLeft / OnRight; Show: random)
float g_ultimateTime = 0.f;           // 0x61c1c4
int g_lastSecond = 0;                 // 0x61c1c8 the contents follow the clock once a second
float g_pulse = 0.f;                  // 0x61c1cc the selectors' highlight, one after the other
float g_time = 0.f;                   // 0x61c1d0
uint32_t g_loadingChest = 0xffffffff; // 0x60f1b8
int g_selected = -1;                  // 0x60f1bc the selector chosen (-1: none)
bool g_tablet = false;                // 0x60f1c0
float g_rootScale = 1.f;              // 0x60f1c4

// The item card points at window w (below it) and comes in front.
void PointHoverAt(const Window* w) {
    const int x = g_root->x + w->x, y = g_root->y + w->y;
    g_hover->SetTargetRect(x, y, x + w->w, y + w->h, ItemHoverWindow::kS);
    g_hover->RecomputePosition();
    g_hover->SetZ(0.001f);
    Render::SortRenderLayer(0xe, 1);
}

// @0x289b7c: the luck circle's card: the best luck item owned, or the hint how luck is won.
void OnLuckHolder() {
    SoundsManager::PlaySound("ui_click", 1, false);
    g_hover->Show();
    unsigned luck = 0;
    if (const uint32_t best = GetBestLuckItem(luck)) {
        g_hover->SetItem(U"", GameState::GetFirstItemByID(best, false), Items::GetItemInfo(best), true, false, nullptr,
                         true, false, false, false);
    } else {
        g_hover->SetText(StringTable::GetString("BONUS_LUCK"), StringTable::GetString("CHESTS_LUCK_HINT"));
    }
    PointHoverAt(g_luck.circleHolder);
}

// @0x289d28: the card of what window w shows: an item, a soldier, a building, a decoration (its
// description with the time between its taxes) or a resource, with a drop chance (probability).
void ShowHoverWindow(const Window* w, Items::ItemInfo* item, EntityData* entity, GameData::BuildingData* building,
                     const GameData::DecorData* deco, uint32_t type, int probability) {
    SoundsManager::PlaySound("ui_click", 1, false);
    g_hover->Show();
    g_hover->SetProbability(probability);
    if (item)
        g_hover->SetItem(StringTable::GetString("BELT_ITEM"), nullptr, item, false, false, nullptr, true, false, false,
                         false);
    if (entity) g_hover->SetEntity(nullptr, entity, false, false);
    if (building)
        g_hover->SetText(StringTable::GetString(building->name.c_str()),
                         StringTable::GetStringF("%s_DESC", building->name.c_str()));
    if (deco) {
        const std::u32string time = StringTable::GetTimeString((int)deco->collectTime, true);
        const std::u32string text =
            SWPrintf(0x100, StringTable::GetStringF("%s_DESC", deco->name.c_str()), {time.c_str()});
        g_hover->SetText(StringTable::GetString(deco->name.c_str()), text.c_str());
    }
    if (type != 0xb) g_hover->SetText(GameState::GetResourceGameName((int)type), nullptr);
    PointHoverAt(w);
}

// The soldier an item gives (its meta 0x27), or nullptr.
EntityData* ItemEntity(Items::ItemInfo* info) {
    const MetaData* d = info && info->meta ? info->meta->FindChildData(0x27) : nullptr;
    return d ? EntityFactory::GetEntityByID(d->GetInt()) : nullptr;
}

// @0x28a1c8: the prize's card (item, soldier or statue, whichever is shown).
void OnUltimateItem() {
    if (g_items.ultiItem.root->visible)
        if (Items::ItemInfo* info = Items::GetItemInfo(g_items.ultiItemId))
            ShowHoverWindow(g_items.ultiItem.root, info, nullptr, nullptr, nullptr, 0xb, 0);
    if (g_items.ultiWarrior.root->visible)
        if (EntityData* e = EntityFactory::GetEntityByID(g_items.ultiEntity))
            ShowHoverWindow(g_items.ultiWarrior.root, nullptr, e, nullptr, nullptr, 0xb, 0);
    if (g_items.ultiStatue->visible)
        if (const GameData::DecorData* deco = GameData::GetDecoration(g_items.ultiStatueId))
            ShowHoverWindow(g_items.ultiStatue, nullptr, nullptr, nullptr, deco, 0xb, 0);
}

// @0x28a2ec: the card of one of the set's five.
void OnRequiredItem(unsigned i) {
    if (g_items.fiveItems.items[i].root->visible)
        if (Items::ItemInfo* info = Items::GetItemInfo(g_items.fiveItems.ids[i]))
            ShowHoverWindow(g_items.fiveItems.items[i].root, info, nullptr, nullptr, nullptr, 0xb, 0);
    if (g_items.fiveWarriors.slots[i].root->visible)
        if (EntityData* e = EntityFactory::GetEntityByID(g_items.fiveWarriors.entities[i]))
            ShowHoverWindow(g_items.fiveWarriors.slots[i].root, nullptr, e, nullptr, nullptr, 0xb, 0);
    if (g_items.fiveObjects.icons[i]->visible)
        if (const GameData::DecorData* deco = GameData::GetDecoration(g_items.fiveObjects.ids[i]))
            ShowHoverWindow(g_items.fiveObjects.icons[i], nullptr, nullptr, nullptr, deco, 0xb, 0);
}

// @0x28a424: the card of a cell of the reward line (an item, the soldier it gives, a resource or a
// decoration), with its chance in the preview.
void OnRewardItem(unsigned i) {
    const unsigned at = i + g_items.line.scroller.firstIndex;
    if (at >= g_rewards.size()) return;
    const RewardItemInfo& r = g_rewards[at];
    Window* cell = g_items.line.items[i].root;
    if (cell->visible) {
        if (Items::ItemInfo* info = Items::GetItemInfo(r.item)) {
            const MetaData* d = info->meta ? info->meta->FindChildData(0x27) : nullptr;
            if (!d)
                ShowHoverWindow(cell, info, nullptr, nullptr, nullptr, 0xb, r.probability);
            else if (EntityData* e = EntityFactory::GetEntityByID(d->GetInt()))
                ShowHoverWindow(cell, nullptr, e, nullptr, nullptr, 0xb, r.probability);
        } else if (r.type != 0xb) {
            ShowHoverWindow(cell, nullptr, nullptr, nullptr, nullptr, r.type, r.probability);
        }
    }
    if (g_items.line.objects[i]->visible)
        if (const GameData::DecorData* deco = GameData::GetDecoration(r.decoration))
            ShowHoverWindow(g_items.line.objects[i], nullptr, nullptr, nullptr, deco, 0xb, r.probability);
}

// Pays a crystal-sold chest's price (or opens NotEnoughWindow) and queues its purchase
// (GameState::AddItemPayment; UpdatePaymentItems hands it to OnChestBought). False: not paid.
// (Billing::LogCBPurchase(0x24, chest, price) and FileManager::OnItemBought(sku, chest, 1, price,
// "crystals"): purchase statistics, online. A chest not sold for crystals went to Billing::Process,
// the real-money store: not ported, every store chest is sold for crystals (Chests::Init).)
bool BuyChest(const Chests::ChestInfo* info) {
    if (!info->SoldForCrystals()) return true;
    NotEnoughWindow::ResetRequirements();
    NotEnoughWindow::AddRequirement(GameState::kCrystal, info->crystals);
    if (!NotEnoughWindow::CheckRequirements()) {
        NotEnoughWindow::Show();
        return false;
    }
    GameState::ChangeResourceAmount(GameState::kCrystal, -(int)info->crystals);
    GameState::AddItemPayment(info->sku ? info->sku : "", 0, info->id);
    return true;
}

// @0x294328: buys the chosen chest; its result comes back through OnChestBought.
void OnOpen() {
    if (g_selected == -1) return;
    if (Chests::ChestInfo* info = Chests::GetChestInfo(kChests[g_selected]))
        if (!BuyChest(info)) return;
    g_loadingChest = kChests[g_selected];
    UpdateContents();
}

}  // namespace

// PORT (offline): the original asked the server to open the chest (GameState::SendDailyChestRequest
// with the chest item's unique id, reroll and the best luck item's luck); its answer
// (CMyServer::NET_GetServerChest_Callback) gave each item, object and resource to
// GameState::AddDailyChestItem. Offline the chest is rolled here as the map chests are
// (Chests::ChestInfo::GenerateContents). PORT (user 2026-10-19): the server's rule for the luck is
// unknown; offline it raises every item's chance by that percent (+30: x1.3, at most always) for
// this roll.
void RollChest(uint32_t chest, unsigned luck) {
    GameState::SetProcessedChest(chest);
    if (Chests::ChestInfo* info = Chests::GetChestInfo(chest)) {
        const std::vector<Chests::ChestItem> items = info->items;
        for (Chests::ChestItem& it : info->items)
            it.probability = std::min(10000, (int)((long long)it.probability * (100 + luck) / 100));
        Chests::DropItem drops[0x20];
        const unsigned n = info->GenerateContents(drops, 0x20, false);
        info->items = items;
        for (unsigned i = 0; i != n; ++i)
            GameState::AddDailyChestItem(drops[i].item, drops[i].object, (GameState::ResourceType)drops[i].type,
                                         (uint32_t)drops[i].amount);
    }
    GameState::SetDailyChestFailure(false);
    GameState::FinishDailyChestItems();
}

namespace {

// @0x294438: pays the reroll (the chest's reroll_price, else the setting reroll_cost, at least 1)
// and rolls the chest again; the new result replaces the one shown. (Billing::LogCBPurchase(0x23,
// chest, price): online. LoadingInternetWindow with the chest's drop icon: online, not ported.)
void OnReroll() {
    // (!GameState::IsDailyChestRequestProcessing(): always so offline.)
    if (g_selected == -1) return;
    Chests::ChestInfo* info = Chests::GetChestInfo(kChests[g_selected]);
    if (!info) return;
    int price = info->rerollPrice ? (int)info->rerollPrice : Setting("reroll_cost").GetInt();
    if (price < 1) price = 1;
    NotEnoughWindow::ResetRequirements();
    NotEnoughWindow::AddRequirement(GameState::kCrystal, (unsigned)price);
    if (!NotEnoughWindow::CheckRequirements()) {
        NotEnoughWindow::Show();
        return;
    }
    GameState::ChangeResourceAmount(GameState::kCrystal, -price);
    unsigned luck = 0;
    GetBestLuckItem(luck);
    if (GameState::GetFirstItemByID(Chests::GetItemForChest(GameState::GetProcessedChest()), false))
        RollChest(GameState::GetProcessedChest(), luck);
    UpdateContents();
}

// @0x28a6c8: the starter chest: bought (its offer run out: the window closes); else the result is
// collected: the chest item is used up, the sets count what came, and ChestOpenedWindow shows the
// loot, returning to this chest after.
void OnCollect() {
    if (g_starterMode) {
        if (Items::ItemInfo* item = Items::GetItemInfo(0x2b6)) {
            const int now = Timer::GetGlobalTime();
            const int left = Setting("starter_chest_time").GetInt() - (now - (int)GameState::GetItemRecentTime(item->id));
            if (left <= 0 && !g_starterFlag) {
                Hide();
                return;
            }
        }
        if (Chests::ChestInfo* info = Chests::GetChestInfo(g_starterChest))
            if (!BuyChest(info)) return;
        g_loadingChest = g_starterChest;
        return;
    }
    Hide();
    Chests::ChestInfo* info = Chests::GetChestInfo(kChests[g_selected]);
    if (!info) return;
    GameState::RemoveItem(Chests::GetItemForChest(info->id), 1);
    FoundItemsWindow::ClearCollected();
    FoundItemsWindow::SetHeader(StringTable::GetString("CAMPAIGN_INFO_FOUND"));
    // (As the original, every cell also adds its item, 0 for a resource or object, which adds nothing.)
    for (const RewardItemInfo& r : g_rewards) {
        if (r.type != 0xb) {
            FoundItemsWindow::AddCollectedResource((int)r.type, r.amount);
        } else if (r.object) {
            GameState::SetChestBuildingReceivedCount(r.object, GameState::GetChestBuildingReceivedCount(r.object) + 1);
            FoundItemsWindow::AddCollectedItem(r.object + 0x4000, r.amount);
        } else if (r.decoration) {
            GameState::SetChestBuildingReceivedCount(r.decoration,
                                                     GameState::GetChestBuildingReceivedCount(r.decoration) + 1);
            FoundItemsWindow::AddCollectedItem(r.decoration + 0x8000, r.amount);
        } else if (r.item) {
            Items::ItemInfo* it = Items::GetItemInfo(r.item);
            if (it) {
                const MetaData* d = it->meta ? it->meta->FindChildData(0x27) : nullptr;
                EntityData* e = d ? EntityFactory::GetEntityByID(d->GetInt()) : nullptr;
                if (e)
                    GameState::SetChestEntityReceivedCount(e->id, GameState::GetChestEntityReceivedCount(e->id) + 1);
                else
                    GameState::SetChestItemReceivedCount(r.item, GameState::GetChestItemReceivedCount(r.item) + 1);
            }
        }
        FoundItemsWindow::AddCollectedItem(r.item, r.amount);
    }
    ChestOpenedWindow::SetName(info->name);
    ChestOpenedWindow::SetReturnToChestWindow((uint32_t)g_selected);
    ChestOpenedWindow::Show();
}

// @0x28a674: the chest being opened is shown in the inventory (the popup closed first). The OK of
// Update's popup for a failed daily chest request ("FB_ONLINE_NOTICE", online): no caller offline.
[[maybe_unused]] void ShowProcessingChestInInventory() {
    PopupWindow::Hide();
    if (!g_queue->shown) return;
    GameState::PlayerItem* p = GameState::GetFirstItemByID(Chests::GetItemForChest(GameState::GetProcessedChest()), false);
    if (!p) return;
    Hide();
    CharacterInfoWindow::Show();
    CharacterInfoWindow::ShowInventoryItem(p->id, 0, p);
}

// @0x289aa8
void OnTweenOut() {
    WindowManager::WindowHide(false);
    g_queue->shown = false;
    g_queue->MoveWindowDown(true);
    GameState::DropGamePauseState();
}

// @0x294208: the starter chest opens with a sound.
void OnTweenIn() {
    if (g_starterMode) SoundsManager::PlaySound("chest_opened", 1, false);
    UpdateContents();
}

// @0x28987c / @0x2898b0: the reward line scrolls by a cell.
void OnLeftReward() { g_items.line.scroller.bounce += (float)g_items.line.scroller.itemSize; }
void OnRightReward() { g_items.line.scroller.bounce -= (float)g_items.line.scroller.itemSize; }

// @0x294244: the contents preview on or off (the reward line back at its start).
void OnTogglePreview() {
    g_preview = !g_preview;
    g_items.line.scroller.HandleMove(-g_items.line.scroller.offset);
    g_items.line.scroller.firstIndex = 0;
    g_items.line.scroller.offset = 0;
    UpdateContents();
}

// @0x294294
void OnChest(unsigned index) {
    g_selected = (int)index;
    UpdateContents();
}

// @0x2942a8 / @0x2942cc: the background cycles through four.
void OnRight() {
    g_background = g_background == 3 ? 0 : g_background + 1;
    UpdateContents();
}
void OnLeft() {
    g_background = g_background == 0 ? 3 : g_background - 1;
    UpdateContents();
}

// @0x28a5f8: the item card closes first; a result waiting to be collected keeps the window.
bool OnBack() {
    if (!g_queue->shown) return false;
    if (g_blockClicks) return true;
    if (g_hover->shownHover) {
        g_hover->Hide();
        return true;
    }
    if (!g_collecting) Hide();
    return true;
}

// @0x2897f0
void SetZ(float z) {
    if (IsVisible()) g_root->SetZ(z);
    g_root->SetPosition(g_tween->GetCenteredX(), g_tween->GetCenteredY());
}

// @0x28add8
void Move(int x, int y) { g_items.line.scroller.Move(x, y); }

// @0x28adf8: the reward line's drag, then the item card, then the window.
bool Click(int x, int y, bool pressed) {
    if (g_blockClicks || g_prizeFlight || g_ultimatePending) return true;
    if (g_root->visibleSelf && g_items.line.scroller.Click(x, y, pressed, g_root)) return true;
    if (g_hover->Click(x, y, pressed)) return true;
    const bool visible = g_root->visibleSelf;
    g_root->Click(x, y, pressed, false);
    return visible;
}

// @0x2898e4
float ZRange() { return g_queue->shown ? 0.077f : 0.001f; }

// @0x28aef4
void Deinit() {
    GUI::RemoveTweenEffect(g_tween);
    g_tween = nullptr;
    delete g_root;
    g_root = nullptr;
    delete g_hover;
    g_hover = nullptr;
}

// @0x2946b8: the selectors' highlight runs down the list while none is chosen; the item card stays
// in front; a result (g_pending) moves into the reward line; the reward line scrolls and the
// sparkles turn; the ultimate prize is shown two seconds after it was won; once a second the
// contents are brought up to date.
void Update(float dt) {
    g_time += dt;
    if (g_queue->shown && g_selected == -1) {
        g_pulse += dt;
        for (int i = 0; i != 4; ++i) {
            Window* bg = g_selectors[i].activeBg;
            bg->SetVisibility(true);
            const int which = (int)g_pulse;
            const double c = std::cos((double)std::fmod(g_pulse, 1.f) * 3.141592653589793);
            if (g_pulse > 5.f) g_pulse = 0.f;
            float alpha = 0.f;
            if (which == i) alpha = 1.f - std::fabs((float)c);
            bg->SetAlpha(alpha, false);
        }
    }
    if (g_hover->shownHover && g_hover->GetZ() != 0.001f) {
        g_hover->SetZ(0.001f);
        Render::SortRenderLayer(0xe, 1);
    }
    // (!GameState::IsDailyChestRequestProcessing(): always so offline.)
    if (!g_pending.empty()) {
        if (!g_queue->shown) Show();
        int found = -1;
        for (int i = 0; i != 4; ++i)
            if (kChests[i] == GameState::GetProcessedChest()) found = i;
        if (found == -1) {
            std::fprintf(stderr, "ERROR: ChestsWindow::OnChestBought() Chest %d is not from the known list\n",
                         GameState::GetProcessedChest());   // ErrorReporter::Printf
            g_pending.clear();
            return;
        }
        // (LoadingInternetWindow::Hide when shown: online.)
        if (CharacterInfoWindow::IsVisible()) CharacterInfoWindow::Hide();
        g_rewards = g_pending;
        g_collecting = true;
        g_pending.clear();
        g_selected = found;
        g_preview = false;
        g_items.line.scroller.HandleMove(-g_items.line.scroller.offset);
        g_items.line.scroller.firstIndex = 0;
        g_items.line.scroller.offset = 0;
        g_items.line.scroller.velocity = 0.f;
        UpdateContents();
    }
    // (A failed daily chest request's popup, "FB_ONLINE_NOTICE": online.)
    if (g_queue->shown) {
        Shared::ContentScroller& sc = g_items.line.scroller;
        sc.Update(dt);
        g_items.line.arrowLeft->SetEnabled(sc.CanMoveLeft());
        g_items.line.arrowRight->SetEnabled(sc.CanMoveRight());
        for (ChestSelectorHolder& s : g_selectors) {
            for (int j = 0; j != 4; ++j) {
                const int frames = Resources::GetFrameCount(kSparkle);
                Render::Sprite* sprite = frames ? s.sparkles[j]->sprite : nullptr;
                if (!sprite) continue;
                s.sparkleTime[j] += dt;
                int frame = s.sparkleFrame[j];
                while (s.sparkleTime[j] > 0.2f) {
                    s.sparkleTime[j] -= 0.2f;
                    s.sparkleFrame[j] = ++frame;
                    if (frames <= frame) s.sparkleFrame[j] = frame = 0;
                }
                Render::Texture* tex = sprite->tex;
                Render::SetFrame(sprite, (float)(tex->h / frames), (float)tex->w, frame);
                Render::SetMagnifyFilter(sprite->tex, true);
                sprite->w *= g_rootScale;
                sprite->h *= g_rootScale;
            }
        }
    }
    g_ultimateTime += dt;
    if (g_ultimatePending && g_ultimateTime > 2.f) {
        g_ultimatePending = false;
        FoundItemsWindow::ClearCollected();
        FoundItemsWindow::SetHeader(StringTable::GetString("CAMPAIGN_INFO_FOUND"));
        // The set's five are counted again from zero; the prize ("id|amount") is collected.
        Setting set(kPrizeSettings[g_selected]);
        if (g_selected == 0) {
            for (unsigned i = 0; i != 5; ++i) GameState::SetChestBuildingReceivedCount((uint32_t)set.GetChild(i).GetInt(), 0);
            FoundItemsWindow::AddCollectedItem((uint32_t)set.GetChild(5).GetChild(0).GetInt() + 0x8000,
                                               set.GetChild(5).GetChild(1).GetInt());
        } else if (g_selected == 2) {
            for (unsigned i = 0; i != 5; ++i) {
                Items::ItemInfo* info = Items::GetItemInfo((uint32_t)set.GetChild(i).GetInt());
                const MetaData* d = info && info->meta ? info->meta->FindChildData(0x27) : nullptr;
                if (!d) continue;
                if (EntityData* e = EntityFactory::GetEntityByID(d->GetInt())) GameState::SetChestEntityReceivedCount(e->id, 0);
            }
            FoundItemsWindow::AddCollectedItem((uint32_t)set.GetChild(5).GetChild(0).GetInt(),
                                               set.GetChild(5).GetChild(1).GetInt());
        } else {
            for (unsigned i = 0; i != 5; ++i) GameState::SetChestItemReceivedCount((uint32_t)set.GetChild(i).GetInt(), 0);
            FoundItemsWindow::AddCollectedItem((uint32_t)set.GetChild(5).GetChild(0).GetInt(),
                                               set.GetChild(5).GetChild(1).GetInt());
        }
        ChestOpenedWindow::SetName(StringTable::GetString("CHESTS_ULTIMATE_PRIZE"));
        ChestOpenedWindow::Show();
        Hide();
    }
    if (Timer::GetGlobalTime() != g_lastSecond) {
        g_lastSecond = Timer::GetGlobalTime();
        if (g_queue->shown) UpdateContents();
    }
}

}  // namespace

// @0x289adc: the owned item with the most luck (its meta 0x29), and that luck.
uint32_t GetBestLuckItem(unsigned& luck) {
    luck = 0;
    uint32_t best = 0;
    for (unsigned i = 0; Items::ItemInfo* info = Items::EnumItemInfo(i); ++i) {
        const MetaData* d = info->meta ? info->meta->FindChildData(0x29) : nullptr;
        if (!d || !GameState::GetItemAmount(info->id, false) || d->GetInt() <= (int)luck) continue;
        best = info->id;
        luck = (unsigned)d->GetInt();
    }
    return best;
}

namespace {

// UpdateContents (@0x28d204) is one function of 28 KB in the original; its parts follow.

Textfield* LuckPercent(int i) { return i < 4 ? g_luck.percent[i] : g_luck.percent1; }

// A copy of sprite flies from its window to the prize's window (target), in two seconds.
void FlyToPrize(Render::Sprite* sprite, const Window* target) {
    const int dh = (int)(sprite->h - (float)target->h);
    BuildingHovers::AddItemMovement(sprite, g_root->x + target->x, g_root->y + target->y - dh / 2, true, 2.f, -1, -1,
                                    false, false);
}

// Part of @0x28d204: a set's five are all collected (count 5): the next call flies them to the
// prize (g_lastSecond 0 makes Update call again at once); after the flight the prize is pending.
void CheckPrizeFlight(unsigned count) {
    if (g_prizeFlight && !g_ultimatePending) {
        g_prizeFlight = false;
        g_ultimatePending = true;
        g_ultimateTime = 0.f;
    }
    if (count == 5 && !g_prizeFlight && !g_ultimatePending && !g_tween->active) {
        g_lastSecond = 0;
        g_prizeFlight = true;
    }
}

// Part of @0x28d204: the reward line shows g_rewards (from firstIndex), centred when it has no
// scroll bar.
void UpdateRewardLine() {
    ChestRewardLine& line = g_items.line;
    Shared::ContentScroller& sc = line.scroller;
    sc.itemCount = (unsigned)g_rewards.size();
    sc.UpdateScroll();
    line.arrowLeft->SetVisibility(sc.CanMove());
    line.arrowRight->SetVisibility(sc.CanMove());
    for (unsigned i = 0; i != 11; ++i) {
        Shared::ItemHolder& h = line.items[i];
        line.objects[i]->MoveWindow(-line.f610, 0);
        h.root->parent->MoveWindow(-line.f610, 0);
        line.objects[i]->SetVisibility(false);
        h.root->SetVisibility(sc.firstIndex + i < g_rewards.size());
        if (!h.root->visibleSelf) continue;
        const RewardItemInfo& r = g_rewards[sc.firstIndex + i];
        if (Items::ItemInfo* info = Items::GetItemInfo(r.item)) {
            for (unsigned k = 0; k != 5; ++k) h.icon.itemClass[k]->SetVisibility(info->itemLevel == k);
            const MetaData* d = info->meta ? info->meta->FindChildData(0x27) : nullptr;
            EntityData* e = d ? EntityFactory::GetEntityByID(d->GetInt()) : nullptr;
            h.icon.icon->SetTexture(e ? e->side0 : info->GetIcon(), true);
        } else if (r.type != 0xb) {
            for (unsigned k = 0; k != 5; ++k) h.icon.itemClass[k]->SetVisibility(k == 0);
            h.icon.icon->SetTexture(IconManager::GetIcon(GameState::GetResourceMapIconName((int)r.type), false), true);
        } else if (const GameData::DecorData* deco = GameData::GetDecoration(r.decoration)) {
            h.root->SetVisibility(false);
            line.objects[i]->SetVisibility(true);
            GUI::FitImageIntoWindow(line.objects[i], GameData::DecorImage(deco), true, false);
        }
        h.quantity->SetVisibility(r.amount != 0);
        if (r.amount != 0) h.quantity->SetText(ToWideString(r.amount));
        h.icon.locked->SetVisibility(false);
        h.icon.discount[3].root->SetVisibility(false);
        h.icon.redX->SetVisibility(false);
        h.icon.owned->SetVisibility(false);
    }
    line.f610 = 0;
    if (!sc.scrollbar->visibleSelf) {
        line.f610 = (int)((float)(sc.itemSize * ((sc.unkA0 - 2) - (int)g_rewards.size())) * 0.5f);
        if (line.f610)
            for (unsigned i = 0; i != 11; ++i) {
                line.objects[i]->MoveWindow(line.f610, 0);
                line.items[i].root->parent->MoveWindow(line.f610, 0);
            }
    }
    line.root->UpdatePosition();
}

// Part of @0x28d204: the panel's texts; a mode shows and sets the ones it uses.
void HideTexts(bool noChest) {
    g_items.contain->SetVisibility(false);
    g_items.noChest->SetVisibility(noChest);
    g_items.needToGet->SetVisibility(false);
    g_items.ultiLong->SetVisibility(false);
}

void HideNames() {
    for (Textfield* t : g_items.name) t->SetVisibility(false);
}

void ShowStats(bool visible) {
    for (int i = 0; i != 2; ++i) {
        g_items.statIcon[i]->SetVisibility(visible);
        g_items.statText[i]->SetVisibility(visible);
    }
}

// Part of @0x28d204: the starter chest, with the time left of its offer and its fixed contents; when
// the offer has run out the chest item is used up (and its quest 0x75b starts over).
void ShowStarterChest() {
    Items::ItemInfo* item = Items::GetItemInfo(Chests::GetItemForChest(g_starterChest));
    Chests::ChestInfo* info = Chests::GetChestInfo(g_starterChest);
    if (!info || !item) return;
    const int now = Timer::GetGlobalTime();
    const int since = now - (int)GameState::GetItemRecentTime(item->id);
    const int limit = Setting("starter_chest_time").GetInt();
    const int left = g_starterFlag ? 1 : std::max(0, limit - since);
    g_title->SetText(SWPrintf(0x100, StringTable::GetString("CHEST_FOUND"), {item->title}).c_str());
    g_pile.chest.closed[4]->SetVisibility(true);
    g_expire.timer->SetTexture(IconManager::GetSandClockIcon(), true);
    g_expire.timeLeft->SetText(StringTable::GetString(left ? "CHEST_TIME_LEFT" : "CHEST_OFFER_EXPIRED"));
    g_expire.time->SetText(StringTable::GetNumericTimeString(left, true).c_str());
    HideTexts(false);
    g_items.containLarge->SetVisibility(true);
    g_items.containLarge->SetText(StringTable::GetString("CHEST_CONTAINS"));
    g_collectButton.activeGreen->SetVisibility(true);
    g_collectButton.textCenteredGreen->SetVisibility(true);
    if (!left) {
        g_collectButton.textCenteredGreen->SetText(StringTable::GetString("CLOSE"));
    } else {
        // PORT (no real money): the text "CHESTS_OPEN_FOR" with the price "$%d.%02d" of the chest's
        // cents is not set; every store chest is sold for crystals (Chests::Init), which hides it.
        const bool crystals = info->SoldForCrystals();
        if (crystals) g_collectButton.textCenteredGreen->SetVisibility(false);
        g_collectButton.goldText->SetVisibility(crystals);
        g_collectButton.price->SetVisibility(crystals);
        g_collectButton.crystalIcon->SetVisibility(crystals);
        if (crystals) {
            g_collectButton.goldText->SetText(StringTable::GetString("PERFORM_OPEN"));
            g_collectButton.price->SetText(ToWideString((int)info->crystals));
            g_collectButton.crystalIcon->SetTexture(IconManager::GetIcon("35_crystal", false), true);
        }
    }
    g_collectButton.active->SetVisibility(false);
    g_collectButton.icon->SetVisibility(false);
    g_collectButton.text->SetVisibility(false);
    g_rewards.clear();
    g_pile.day1->SetText(info->name);
    g_pile.day2->SetText(info->name);
    // Its resources (a chest sold for crystals does not show the crystals it gives), then its items.
    for (unsigned i = 0; info->resources && i < info->resources->GetChildrenCount(); ++i) {
        const MetaData* r = info->resources->GetChild(i);
        if (r->GetChild(0)->type == MetaData::kString) {
            const int type = GameState::StringToResourceType(r->GetChild(0)->GetString());
            if (info->SoldForCrystals() && type == GameState::kCrystal) continue;
            g_rewards.push_back(RewardItemInfo());
            g_rewards.back().type = (uint32_t)type;
            g_rewards.back().amount = r->GetChild(1)->GetInt();
            if (type == GameState::kCrystal)   // PORT (balance, user 2026-10-20), as Chests::GenerateContents
                g_rewards.back().amount = GameState::CrystalReward(g_rewards.back().amount);
        } else if (r->GetChild(0)->type == MetaData::kNumber) {
            int amount = r->GetChild(1)->GetInt();
            Items::ItemInfo* it = Items::GetItemInfo((uint32_t)r->GetChild(0)->GetInt());
            if (it && it->IsLimited() && (amount = it->ApplyLimitToAmount(amount)) == 0) continue;
            g_rewards.push_back(RewardItemInfo());
            g_rewards.back().item = (uint32_t)r->GetChild(0)->GetInt();
            g_rewards.back().amount = amount;
        }
    }
    for (const Chests::ChestItem& c : info->items) {
        if (!c.item) continue;
        g_rewards.push_back(RewardItemInfo());
        g_rewards.back().item = c.item;
        g_rewards.back().amount = (int)c.amountMin;
    }
    if (left || !GameState::GetItemAmount(item->id, false)) return;
    GameState::RemoveItem(item->id, 1);
    GameState::ResetTask(0x75b);
    if (Tasks::TaskInfo* t = Tasks::GetTask(0x75b)) t->dnShow = t->dnShowDefault;
    HUDWindow::UpdateTasks();
}

// Part of @0x28d204: no chest chosen: the description of the one in the background.
void ShowNoChest() {
    HideTexts(true);
    g_items.containLarge->SetVisibility(false);
    HideNames();
    g_title->SetText(StringTable::GetString("CHESTS_HEADER"));
    if (Chests::ChestInfo* info = Chests::GetChestInfo(kChests[g_background]))
        g_items.noChest->SetText(
            SWPrintf(0x100, info->desc, {ToWideString((int)info->countMin)}).c_str());
    g_previewButton.root->SetEnabled(false);
    g_previewButton.activeGreen->SetVisibility(false);
    g_previewButton.icon->SetTexture(IconManager::GetIcon("gold_find", false), true);
    g_previewButton.text->SetText(StringTable::GetString("CHESTS_PREVIEW"));
    g_openButton.root->SetEnabled(false);
    g_openButton.active->SetVisibility(false);
    g_openButton.textCenteredGreen->SetText(StringTable::GetString("PERFORM_OPEN"));
    ShowStats(false);
    // (The original then reads the chest kChests[g_selected], with g_selected -1 the word before the
    // list (0x676e, no chest), so its price below is never set.)
}

// Part of @0x28d204: the preview: everything the chest can give, in the reward line.
void ShowPreview() {
    HideTexts(false);
    HideNames();
    ShowStats(false);
    g_previewButton.root->SetEnabled(true);
    g_previewButton.activeGreen->SetVisibility(false);
    g_previewButton.icon->SetTexture(IconManager::GetIcon("gold_go_back", false), true);
    g_previewButton.text->SetText(StringTable::GetString("INVENTORY_BACK"));
    g_rewards.clear();
    Chests::ChestInfo* info = Chests::GetChestInfo(kChests[g_selected]);
    if (!info) return;
    g_pile.day1->SetText(info->name);
    g_pile.day2->SetText(info->name);
    g_items.containLarge->SetVisibility(true);
    g_items.containLarge->SetText(SWPrintf(0x100, StringTable::GetString("CHEST_PREVIEW_DESC"),
                                           {ToWideString((int)info->countMin)}).c_str());
    Chests::DropItem drops[0x40];
    const unsigned n = info->GenerateContents(drops, 0x40, true);
    for (unsigned i = 0; i != n; ++i) {
        const Chests::DropItem& d = drops[i];
        if (d.type == 0xb && !d.item && !d.object) {
            // As the original: only the chance is written, into the last cell.
            if (!g_rewards.empty()) g_rewards.back().probability = d.probability;   // PORT: the original has no check
            continue;
        }
        RewardItemInfo r;
        r.type = d.type;
        if (d.type == 0xb && d.item) {
            r.item = d.item;
        } else if (d.type == 0xb) {
            if (GameData::GetDecoration(d.object)) {
                r.decoration = d.object;
                if (!Items::GetItemInfo(r.decoration + 0x8000))
                    std::fprintf(stderr, "Decoration %d cannot be given in inventory\n", r.decoration);   // ErrorReporter::Printf
            } else {
                r.object = d.object;
            }
        }
        r.amount = 0;
        r.amountMin = d.amountMin;
        r.amountMax = d.amountMax;
        r.probability = d.probability;
        g_rewards.push_back(r);
    }
}

// Part of @0x28d204: a result to collect: the reward line holds it.
void ShowResult() {
    HideTexts(false);
    HideNames();
    ShowStats(false);
    if (Chests::ChestInfo* info = Chests::GetChestInfo(kChests[g_selected])) {
        g_pile.day1->SetText(info->name);
        g_pile.day2->SetText(info->name);
        g_rerollButton.active->SetVisibility(false);
        g_rerollButton.activeGreen->SetVisibility(false);
        g_rerollButton.goldText->SetText(StringTable::GetString("CHESTS_REROLL_CHEST"));
        const int price = info->rerollPrice ? (int)info->rerollPrice : Setting("reroll_cost").GetInt();
        g_rerollButton.price->SetText(ToWideString(price));
        g_rerollButton.crystalIcon->SetTexture(IconManager::GetIcon("35_crystal", false), true);
    }
    g_items.containLarge->SetVisibility(true);
    g_items.containLarge->SetText(StringTable::GetString("CHESTS_OPENED_DESC"));
    g_collectButton.active->SetVisibility(true);
    g_collectButton.text->SetVisibility(true);
    g_collectButton.text->SetText(StringTable::GetString("INVENTORY_COLLECT"));
    g_collectButton.icon->SetVisibility(true);
    g_collectButton.icon->SetTexture(IconManager::GetIcon("gold_confirm", false), true);
    g_collectButton.activeGreen->SetVisibility(false);
    g_collectButton.textCenteredGreen->SetVisibility(false);
    g_collectButton.goldText->SetVisibility(false);
    g_collectButton.price->SetVisibility(false);
    g_collectButton.crystalIcon->SetVisibility(false);
}

// An item's holder of the sets: its quality frame, the marks hidden.
void ClearItemHolder(Shared::ItemHolder& h, Items::ItemInfo* info) {
    h.emptyBg->SetVisibility(false);
    h.selected->SetVisibility(false);
    h.icon.redX->SetVisibility(false);
    h.icon.owned->SetVisibility(false);
    for (Shared::DiscountInfo& d : h.icon.discount) d.root->SetVisibility(false);
    h.icon.broken->SetVisibility(false);
    h.icon.lock->SetVisibility(false);
    for (unsigned k = 0; k != 5; ++k) h.icon.itemClass[k]->SetVisibility(info->itemLevel == k);
}

// Part of @0x28d204: the collector's and warrior's chests (selectors 3 and 1): five items.
void ShowFiveItems(const Setting& set) {
    g_items.needToGet->SetText(SWPrintf(0x100, StringTable::GetString("CHESTS_UNI_DESC"),
                                        {StringTable::GetString("CHESTS_UNI_DESC_ITEMS")}).c_str());
    unsigned count = 0;
    ChestFiveItems& five = g_items.fiveItems;
    for (unsigned i = 0; i != 5; ++i) {
        Items::ItemInfo* info = Items::GetItemInfo((uint32_t)set.GetChild(i).GetInt());
        if (!info) continue;
        Shared::ItemHolder& h = five.items[i];
        ClearItemHolder(h, info);
        info->PutImageInHolder(h.icon.icon);
        five.confirm[i]->SetVisibility(GameState::GetChestItemReceivedCount(info->id) != 0);
        five.ids[i] = info->id;
        if (GameState::GetChestItemReceivedCount(info->id)) ++count;
        if (g_prizeFlight && h.icon.icon->sprite) FlyToPrize(h.icon.icon->sprite, g_items.ultiItem.icon.icon);
    }
    CheckPrizeFlight(count);
    // The prize: a rare item of its set (the warrior's chest) or the ultimate prize (the collector's).
    Items::ItemInfo* info = Items::GetItemInfo((uint32_t)set.GetChild(5).GetChild(0).GetInt());
    if (!info) return;
    if (g_selected == 3) {
        g_items.ultiLong->SetVisibility(true);
        g_items.ultiLong->SetText(StringTable::GetString("CHESTS_ULTIMATE_PRIZE"));
        HideNames();
    } else {
        g_items.ultiLong->SetVisibility(false);
        for (unsigned k = 0; k != 5; ++k) g_items.name[k]->SetVisibility(info->itemLevel == k);
        g_items.name[info->itemLevel]->SetText(StringTable::GetString("CHESTS_RARE_SET_ITEM"));
    }
    g_items.contain->SetVisibility(true);
    g_items.contain->SetText(SWPrintf(0x100, U"%s:", {StringTable::GetString(info->name)}).c_str());
    if (info->meta) {
        g_items.statIcon[0]->SetTexture(IconManager::GetIcon(info->meta->GetBonusIcon(nullptr), false), true);
        std::u32string desc;
        info->meta->GenerateDescription(desc, 0x100, nullptr);
        g_items.statText[0]->SetText(desc.c_str());
    }
    g_items.statIcon[1]->SetTexture(IconManager::GetIcon("gold_add", false), false);
    g_items.statText[1]->SetText(StringTable::GetString("TAP_FOR_MORE_INFO"));
    Shared::ItemHolder& h = g_items.ultiItem;
    const int amount = set.GetChild(5).GetChild(1).GetInt();
    h.quantity->SetVisibility(amount > 1);
    h.quantity->SetText(ToWideString(amount));
    ClearItemHolder(h, info);
    h.icon.icon->SetTexture(info->GetIcon(), true);
    g_items.ultiItemId = info->id;
}

// A soldier's slot of the sets: its picture and health, the marks hidden.
void SetWarriorSlot(Shared::WarriorSlotInfo& s, EntityData* e) {
    s.soldierFine->SetTexture(e->side0, true);
    s.hpText->SetText(SWPrintf(0x100, U"%d/%d", {e->hp, e->hp}).c_str());
    for (Shared::DiscountInfo& d : s.discount) d.root->SetVisibility(false);
    s.lockIcon->SetVisibility(false);
}

// Part of @0x28d204: the hunter's chest (selector 2): five soldiers, a soldier as the prize.
void ShowFiveWarriors(const Setting& set) {
    g_items.needToGet->SetText(SWPrintf(0x100, StringTable::GetString("CHESTS_UNI_DESC"),
                                        {StringTable::GetString("CHESTS_UNI_DESC_WARRIORS")}).c_str());
    unsigned count = 0;
    ChestFiveWarriors& five = g_items.fiveWarriors;
    for (unsigned i = 0; i != 5; ++i) {
        EntityData* e = ItemEntity(Items::GetItemInfo((uint32_t)set.GetChild(i).GetInt()));
        if (!e) continue;
        Shared::WarriorSlotInfo& s = five.slots[i];
        s.bgDisabled->SetVisibility(false);
        SetWarriorSlot(s, e);
        five.entities[i] = e->id;
        five.confirm[i]->SetVisibility(GameState::GetChestEntityReceivedCount(e->id) != 0);
        if (GameState::GetChestEntityReceivedCount(e->id)) ++count;
        // (As the original: they fly to the item prize's place.)
        if (g_prizeFlight && s.soldierFine->sprite) FlyToPrize(s.soldierFine->sprite, g_items.ultiItem.icon.icon);
    }
    CheckPrizeFlight(count);
    Items::ItemInfo* info = Items::GetItemInfo((uint32_t)set.GetChild(5).GetChild(0).GetInt());
    if (!info) return;
    g_items.ultiLong->SetVisibility(true);
    g_items.ultiLong->SetText(StringTable::GetString("CHESTS_ULTIMATE_PRIZE"));
    HideNames();
    EntityData* e = ItemEntity(info);
    if (!e) return;
    g_items.contain->SetVisibility(true);
    g_items.contain->SetText(SWPrintf(0x100, U"%s:", {StringTable::GetString(e->name.c_str())}).c_str());
    g_items.statIcon[0]->SetTexture(IconManager::GetIcon("gold_16_health", false), true);
    g_items.statText[0]->SetText(
        SWPrintf(0x100, StringTable::GetString("char_hp"), {ToWideString(e->hp)}).c_str());
    g_items.statIcon[1]->SetTexture(IconManager::GetIcon("gold_16_attack", false), true);
    g_items.statText[1]->SetText(SWPrintf(0x100, StringTable::GetString("char_ranged_attack"),
                                          {ToWideString(e->attackMelee)}).c_str());
    SetWarriorSlot(g_items.ultiWarrior, e);
    g_items.ultiEntity = e->id;
}

// Part of @0x28d204: the architect's chest (selector 0): five statues, a statue as the prize.
void ShowFiveObjects(const Setting& set) {
    g_items.needToGet->SetText(SWPrintf(0x100, StringTable::GetString("CHESTS_UNI_DESC"),
                                        {StringTable::GetString("CHESTS_UNI_DESC_BUILDINGS")}).c_str());
    unsigned count = 0;
    ChestFiveObjects& five = g_items.fiveObjects;
    for (unsigned i = 0; i != 5; ++i) {
        const GameData::DecorData* deco = GameData::GetDecoration((uint32_t)set.GetChild(i).GetInt());
        if (!deco) continue;
        Window* icon = five.icons[i];
        GUI::FitImageIntoWindow(icon, GameData::DecorImage(deco), true, false);
        if (g_prizeFlight && icon->sprite) FlyToPrize(icon->sprite, g_items.ultiStatue);
        icon->SetEnabled(true);
        icon->SetAlpha(GameState::GetChestBuildingReceivedCount(deco->id) ? 1.f : 0.6f, false);
        five.confirm[i]->SetVisibility(GameState::GetChestBuildingReceivedCount(deco->id) != 0);
        five.ids[i] = deco->id;
        if (GameState::GetChestBuildingReceivedCount(deco->id)) ++count;
    }
    CheckPrizeFlight(count);
    const GameData::DecorData* deco = GameData::GetDecoration((uint32_t)set.GetChild(5).GetChild(0).GetInt());
    if (!deco) return;
    g_items.ultiLong->SetVisibility(true);
    g_items.ultiLong->SetText(StringTable::GetString("CHESTS_ULTIMATE_PRIZE"));
    HideNames();
    g_items.contain->SetVisibility(true);
    g_items.contain->SetText(SWPrintf(0x100, U"%s:", {StringTable::GetString(deco->name.c_str())}).c_str());
    g_items.statIcon[0]->SetTexture(IconManager::GetIcon("16_crystal", false), true);
    g_items.statText[0]->SetText(SWPrintf(0x100, StringTable::GetString("CHESTS_STATUE_STAT_01"),
                                          {ToWideString(GameState::CrystalReward(Setting("ult_statue_crystals").GetInt()))})
                                     .c_str());
    g_items.statIcon[1]->SetVisibility(false);
    g_items.statText[1]->SetVisibility(false);
    g_items.ultiStatueId = deco->id;
    GUI::FitImageIntoWindow(g_items.ultiStatue, GameData::DecorImage(deco), true, false);
}

// Part of @0x28d204: a chest chosen: the set of five that wins its prize, and its price.
void ShowSets() {
    const int sel = g_selected;
    g_items.noChest->SetVisibility(false);
    const Setting set(kPrizeSettings[sel]);
    g_items.needToGet->SetVisibility(true);
    g_items.fiveWarriors.root->SetVisibility(sel == 2);
    g_items.fiveItems.root->SetVisibility(sel == 1 || sel == 3);
    g_items.fiveObjects.root->SetVisibility(sel == 0);
    g_items.ultiWarrior.root->SetVisibility(sel == 2);
    g_items.ultiItem.root->SetVisibility(sel == 1 || sel == 3);
    g_items.ultiStatue->SetVisibility(sel == 0);
    g_items.containLarge->SetVisibility(false);
    for (Window* w : g_items.fiveWarriors.add) w->SetTexture(IconManager::GetIcon("gold_add", false), true);
    for (Window* w : g_items.fiveWarriors.confirm) w->SetTexture(IconManager::GetIcon("gold_confirm", false), true);
    for (Window* w : g_items.fiveItems.add) w->SetTexture(IconManager::GetIcon("gold_add", false), true);
    for (Window* w : g_items.fiveItems.confirm) w->SetTexture(IconManager::GetIcon("gold_confirm", false), true);
    for (Window* w : g_items.fiveObjects.add) w->SetTexture(IconManager::GetIcon("gold_add", false), true);
    for (Window* w : g_items.fiveObjects.confirm) w->SetTexture(IconManager::GetIcon("gold_confirm", false), true);
    ShowStats(true);
    if (sel == 0) ShowFiveObjects(set);
    else if (sel == 2) ShowFiveWarriors(set);
    else ShowFiveItems(set);
    g_previewButton.root->SetEnabled(true);
    g_previewButton.activeGreen->SetVisibility(false);
    g_previewButton.icon->SetTexture(IconManager::GetIcon("gold_find", false), true);
    g_previewButton.text->SetText(StringTable::GetString("CHESTS_PREVIEW"));
    g_openButton.root->SetEnabled(true);
    g_openButton.active->SetVisibility(false);
    Chests::ChestInfo* info = Chests::GetChestInfo(kChests[sel]);
    if (!info) return;
    // PORT (no real money): the price "$%d.%02d" of the chest's cents in "CHESTS_OPEN_FOR" is not
    // made; every store chest is sold for crystals (Chests::Init), which hides that text.
    if (!info->SoldForCrystals()) return;
    g_openButton.textCenteredGreen->SetVisibility(false);
    g_openButton.goldText->SetVisibility(true);
    g_openButton.price->SetVisibility(true);
    g_openButton.crystalIcon->SetVisibility(true);
    g_openButton.goldText->SetText(StringTable::GetString("PERFORM_OPEN"));
    g_openButton.price->SetText(ToWideString((int)info->crystals));
    g_openButton.crystalIcon->SetTexture(IconManager::GetIcon("35_crystal", false), true);
}

}  // namespace

// @0x28d204: the window's state: the selectors, the chosen chest's pile and luck circle, the
// buttons, then the panel of the mode (the starter chest, no chest chosen, the preview, a result to
// collect, or the chosen chest's set of five) and last the reward line.
void UpdateContents() {
    g_close->SetVisibility(!g_collecting);
    g_chainBorder->SetVisibility(!g_starterMode);
    g_darkBg->SetVisibility(!g_starterMode);
    g_luxBg->SetVisibility(!g_starterMode);
    g_back->SetVisibility(!g_starterMode);
    g_expire.root->SetVisibility(g_starterMode && !g_starterFlag);
    // The buttons, each with its two chains.
    g_previewButton.root->SetVisibility(!g_collecting && !g_starterMode);
    g_chains[0]->SetVisibility(g_previewButton.root->visibleSelf);
    g_chains[1]->SetVisibility(g_previewButton.root->visibleSelf);
    g_openButton.root->SetVisibility(!g_collecting && !g_starterMode);
    g_chains[2]->SetVisibility(g_openButton.root->visibleSelf);
    g_chains[3]->SetVisibility(g_openButton.root->visibleSelf);
    g_rerollButton.root->SetVisibility(g_collecting);
    g_chains[6]->SetVisibility(g_rerollButton.root->visibleSelf);
    g_chains[7]->SetVisibility(g_rerollButton.root->visibleSelf);
    g_collectButton.root->SetVisibility(g_collecting || g_starterMode);
    g_chains[4]->SetVisibility(g_collectButton.root->visibleSelf);
    g_chains[5]->SetVisibility(g_collectButton.root->visibleSelf);
    g_separator->SetVisibility(!g_preview && !g_collecting);
    // The selectors: the chosen one (or, none chosen, all) highlighted.
    for (int i = 0; i != 4; ++i) {
        ChestSelectorHolder& s = g_selectors[i];
        s.root->SetVisibility(!g_preview && !g_collecting);
        if (Chests::ChestInfo* info = Chests::GetChestInfo(kChests[i])) {
            s.name1->SetText(info->name);
            s.name2->SetText(info->name);
        }
        for (Window* w : s.chest.open) w->SetVisibility(false);
        for (int j = 0; j != 5; ++j) s.chest.closed[j]->SetVisibility(j == i);
        s.bg->SetVisibility(g_selected != i);
        s.activeBg->SetVisibility(g_selected == i || g_selected == -1);
        if (g_selected == i) s.activeBg->SetAlpha(1.f, false);
    }
    g_selectors[0].typeIcon->SetTexture(IconManager::GetIcon("b_masters", false), true);
    g_selectors[1].typeIcon->SetTexture(IconManager::GetIcon("army_warrior_40", false), true);
    g_selectors[2].typeIcon->SetTexture(IconManager::GetIcon("tab_shop_tamer", false), true);
    g_selectors[3].typeIcon->SetTexture(IconManager::GetIcon("tab_shop_orbs", false), true);
    // The chosen chest's pile, open while its result waits.
    g_pile.root->SetVisibility(g_preview || g_collecting);
    for (int j = 0; j != 4; ++j) g_pile.chest.open[j]->SetVisibility(g_collecting && g_selected == j);
    for (int j = 0; j != 5; ++j) g_pile.chest.closed[j]->SetVisibility(!g_collecting && g_selected == j);
    // The luck circle: its colour by the best luck item owned (more than 30 purple, 21..30 orange,
    // else blue; none: the second red).
    g_luck.root->SetVisibility(!g_starterMode);
    g_luck.luck->SetText(StringTable::GetString("BONUS_LUCK"));
    unsigned luck = 0;
    const uint32_t best = GetBestLuckItem(luck);
    int circle = luck <= 20 ? LuckHolder::kBlue : luck <= 30 ? LuckHolder::kOrange : LuckHolder::kPurple;
    if (!best) circle = 4;
    g_luck.SetBackground((LuckHolder::BackgroundType)circle);
    for (int i = 0; i != 5; ++i) LuckPercent(i)->SetVisibility(circle == i);
    LuckPercent(circle)->SetText(SWPrintf(0x100, U"+%d%%", {(int)luck}).c_str());
    if (Items::ItemInfo* info = Items::GetItemInfo(best))
        g_luck.icon->SetTexture(info->GetIcon(), true);
    else
        g_luck.icon->SetTexture(IconManager::GetIcon("gold_question", false), true);
    // The panel's frames.
    const bool sets = !g_preview && !g_collecting && g_selected != -1;
    g_items.arrowed->SetVisibility(sets);
    g_items.noArrowBg->SetVisibility(g_selected == -1 || g_preview || g_collecting);
    g_items.bg->SetVisibility(false);
    g_items.navLeft->SetVisibility(g_selected == -1 && !g_starterMode);
    g_items.navRight->SetVisibility(g_selected == -1 && !g_starterMode);
    g_items.line.root->SetVisibility(g_preview || g_collecting);
    g_items.fiveItems.root->SetVisibility(sets);
    g_items.fiveWarriors.root->SetVisibility(sets);
    g_items.fiveObjects.root->SetVisibility(sets);
    g_items.ultiWarrior.root->SetVisibility(sets);
    g_items.ultiItem.root->SetVisibility(sets);
    g_items.ultiStatue->SetVisibility(sets);
    g_items.ultiBg->SetVisibility(sets);
    if (g_starterMode) ShowStarterChest();
    else if (g_selected == -1) ShowNoChest();
    else if (g_preview) ShowPreview();
    else if (g_collecting) ShowResult();
    else ShowSets();
    UpdateRewardLine();
}

WindowManager::FunctionalWindow* Queue() {
    if (!g_queue) {
        WindowManager::FunctionalWindow::Functions f;
        f.init = Init;
        f.deinit = Deinit;
        f.click = Click;
        f.move = Move;
        f.setZ = SetZ;
        f.hide = Hide;
        g_queue = new WindowManager::FunctionalWindow("ChestsWindow", std::move(f));
    }
    return g_queue;
}

bool IsVisible() { return g_queue->shown; }              // @0x2897dc
void BlockClicks(bool block) { g_blockClicks = block; }  // @0x289914

// @0x28d044: a cell of a chest's result, kept until Update shows it (object: a decoration, else
// an object that is none).
void AddRewardItem(uint32_t item, uint32_t object, uint32_t type, int amount) {
    g_pending.push_back(RewardItemInfo());
    RewardItemInfo& r = g_pending.back();
    r.item = item;
    if (GameData::GetDecoration(object)) r.decoration = object;
    else r.object = object;
    r.type = type;
    r.amount = amount;
}

// @0x28aa68: a bought chest (UpdatePaymentItems, until it returns true). The starter and special
// chests (0x35, 0x3f, 0x51..0x53, 0x57..0x59) open at once in ChestOpenedWindow, in the city with
// no other window up; a store chest becomes its chest item and is opened (RollChest), or, in a
// fight, the item is shown in the inventory.
bool OnChestBought(const char* sku, uint32_t chest) {
    g_blockClicks = false;
    if (!GameState::IsPlayerCity()) return false;
    const bool special = chest == 0x35 || chest == 0x3f || chest == 0x51 || chest == 0x52 || chest == 0x53 ||
                         chest == 0x57 || chest == 0x58 || chest == 0x59;
    if (special) {
        if (PlayerCompetitionWindow::IsVisible()) {
            PlayerCompetitionWindow::Hide();
            if (GlobalMapWindow::IsVisible()) GlobalMapWindow::Hide();
        }
        if (g_queue->shown) Hide();
        // (LoadingInternetWindow::Hide when shown: online.)
        if (WindowManager::GetShownWindowCount() != 0) return false;
        if (GameState::IsCombatActive() || GameState::IsCityTutorial()) return false;
        Chests::ChestInfo* info = Chests::GetChestInfo(chest);
        if (!info) return false;
        Chests::DropItem drops[0x20];
        const unsigned n = info->GenerateContents(drops, 0x20, false);
        FoundItemsWindow::ClearCollected();
        FoundItemsWindow::SetHeader(StringTable::GetString("CAMPAIGN_INFO_FOUND"));
        // A chest sold for crystals does not give back its crystals.
        for (unsigned i = 0; i != n; ++i) {
            const Chests::DropItem& d = drops[i];
            if (d.type == 0xb) FoundItemsWindow::AddCollectedItem(d.item, d.amount);
            else if (!info->SoldForCrystals() || d.type != GameState::kCrystal)
                FoundItemsWindow::AddCollectedResource((int)d.type, d.amount);
        }
        ChestOpenedWindow::SetName(info->name);
        ChestOpenedWindow::SetChestTransactionMode(info->id);
        ChestOpenedWindow::Show();
        return true;
    }
    if (!g_queue->shown) {
        if (PlayerCompetitionWindow::IsVisible()) PlayerCompetitionWindow::Hide();
        if (WindowManager::GetShownWindowCount() != 0 || GameState::IsCombatActive() || GameState::IsCityTutorial())
            return false;
    }
    GameState::PlayerItem* p = nullptr;
    if (const uint32_t item = Chests::GetItemForChest(chest))
        p = GameState::AddItem(item, 1, false, false, false);
    else
        std::fprintf(stderr, "ERROR: ChestsWindow::OnChestBought() Chest %d doesn't have an item ID\n", chest);   // ErrorReporter::Printf
    // (SaveManager::QueueSendSave, and FileManager::BillingReceived for a chest not sold for
    // crystals: online.)
    Map::SafeSave();
    if (!p) return true;
    // (GameState::IsDailyChestRequestProcessing() shows the inventory too: never so offline.)
    if (GameState::IsCombatActive()) {
        if (g_queue->shown) Hide();
        CharacterInfoWindow::Show();
        CharacterInfoWindow::ShowInventoryItem(p->id, 0, p);
        return true;
    }
    unsigned luck = 0;
    GetBestLuckItem(luck);
    RollChest(Chests::GetChestForItem(p->id), luck);
    // (LoadingInternetWindow with the chest's drop icon and "WEBVIEW_LOADING": online.)
    return true;
}

// @0x294204: the store's answer (real money, dormant: no caller offline).
void OnPaymentDataReceived() { UpdateContents(); }

// @0x2942f0
void SetChest(unsigned index) {
    g_selected = (int)index;
    UpdateContents();
}

// @0x294304: the starter chest (chest), with its preview on.
void SetStarterChestMode(bool flag, uint32_t chest) {
    g_preview = true;
    g_starterChest = chest;
    g_starterFlag = flag;
    g_starterMode = true;
    UpdateContents();
}

// @0x289a30: the waiting picture of a chest being fetched. PORT (online): LoadingInternetWindow
// (its SetParameters with the chest item's drop icon and "WEBVIEW_LOADING", Show) is not ported.
void StartLoading() {
    Items::ItemInfo* info = Items::GetItemInfo(Chests::GetItemForChest(g_loadingChest));
    if (!info) return;
    g_loadingChest = 0xffffffff;
}

// @0x28a5d4
void Hide() {
    if (!g_root->visibleSelf) return;
    g_tween->AnimateOut();
}

// @0x29459c: no chest chosen, the background at random.
void Show() {
    g_queue->shown = true;
    g_queue->MoveWindowOnTop(true);
    if (!g_root->visibleSelf) {
        WindowManager::WindowShow(false);
        g_tween->AnimateIn();
        GameState::RaiseGamePauseState();
        g_pulse = 0.f;
    }
    g_root->SetVisibility(true);
    g_starterMode = false;
    g_selected = -1;
    g_blockClicks = false;
    g_preview = false;
    g_collecting = false;
    g_background = (int)(Rand48::lrand48() % 4);
    UpdateContents();
    g_root->PreloadTextures();   // GUI::PreloadWindow
}

// @0x28c4fc
void Init() {
    g_tablet = GUI::IsTabletVersion();
    g_root = GUI::RegisterUI("../resource/kingdom_ui/1Original/Window_chests.xml", "Window_chests.png", -1.f, 0, 0, 0, 0,
                             false, 1.f);
    g_queue->RegisterTopWindow(g_root);
    g_rootScale = g_root->scale;
    g_root->SetVisibility(false);
    g_tween = GUI::CreateTweenEffect(g_root, nullptr, OnTweenIn, OnTweenOut);
    g_close = GUI::GetWindowTyped<Window>(g_root, "x_button");
    g_close->SetOnClick(Hide);
    g_close->SetActionSound("ui_close", 1);
    g_title = GUI::GetWindowTyped<Textfield>(g_root, "text_title");
    g_chainBorder = GUI::GetWindowTyped<Window>(g_root, "chain_border_holder_chests");
    g_chainBorder->unk5f = true;   // +0x5f (Update9Slices skips the centre slice)
    g_darkBg = GUI::GetWindowTyped<Window>(g_root, "sliced_dark_bg");
    g_darkBg->unk5f = true;
    g_luxBg = GUI::GetWindowTyped<Window>(g_root, "sliced_lux_bg");
    g_back = GUI::GetWindowTyped<Window>(g_root, "back_simple");
    g_separator = GUI::GetWindowTyped<Window>(g_root, "sepparator_chests");
    for (int i = 0; i != 4; ++i) {
        char name[0x40];
        std::snprintf(name, sizeof name, "chest_selector_holder_%02d", i + 1);
        g_selectors[i].LoadFrom(g_root, name);
        g_selectors[i].root->SetOnClick([i] { OnChest((unsigned)i); });
        g_selectors[i].typeIcon->MoveWindow(1, 0);
    }
    // (As the original: the last selector's type icon is moved once more.)
    g_selectors[3].typeIcon->MoveWindow(1, 0);
    g_pile.LoadFrom(g_root, "chest_open_pile");
    g_luck.LoadFrom(g_root, "luck_holder");
    g_luck.root->SetOnClick(OnLuckHolder);
    g_items.LoadFrom(g_root, "chests_items_holder");
    g_items.navLeft->SetOnClick(OnLeft);
    g_items.navRight->SetOnClick(OnRight);
    for (int i = 0; i != 11; ++i) g_items.line.objects[i]->SetOnClick([i] { OnRewardItem((unsigned)i); });
    for (int i = 0; i != 11; ++i) g_items.line.items[i].root->SetOnClick([i] { OnRewardItem((unsigned)i); });
    for (int i = 0; i != 5; ++i) g_items.fiveItems.items[i].root->SetOnClick([i] { OnRequiredItem((unsigned)i); });
    for (int i = 0; i != 5; ++i) g_items.fiveObjects.icons[i]->SetOnClick([i] { OnRequiredItem((unsigned)i); });
    for (int i = 0; i != 5; ++i) g_items.fiveWarriors.slots[i].root->SetOnClick([i] { OnRequiredItem((unsigned)i); });
    g_items.ultiItem.root->SetOnClick(OnUltimateItem);
    g_items.ultiStatue->SetOnClick(OnUltimateItem);
    g_items.ultiWarrior.root->SetOnClick(OnUltimateItem);
    g_items.ultiBg2->SetOnClick(OnUltimateItem);
    g_items.line.arrowLeft->SetOnClick(OnLeftReward);
    g_items.line.arrowRight->SetOnClick(OnRightReward);
    g_expire.LoadFrom(g_root, "chest_expire_holder");
    for (int i = 0; i != 8; ++i) g_chains[i] = GUI::GetWindowTypedF<Window>(g_root, "chain_%02d", i + 1);
    g_previewButton.LoadFrom(g_root, "button_raid_start_02");
    g_previewButton.root->SetOnClick(OnTogglePreview);
    g_openButton.LoadFrom(g_root, "button_raid_start");
    g_openButton.root->SetOnClick(OnOpen);
    g_rerollButton.LoadFrom(g_root, "button_raid_start_04");
    g_rerollButton.root->SetOnClick(OnReroll);
    g_collectButton.LoadFrom(g_root, "button_raid_start_centre");
    g_collectButton.root->SetOnClick(OnCollect);
    g_queue->hideOnOuterClick = true;
    g_queue->fn.zRange = ZRange;
    g_queue->fn.update = Update;
    g_queue->fn.back = OnBack;
    g_queue->fn.show = Show;
    g_hover = new ItemHoverWindow();
}

}  // namespace ChestsWindow
