// Tasks: the quests (tasks.xml). Each <XTask> has its conditions (<XCondition type= vmin=>), up to
// three steps (<XSubtask>) and a reward (<XAward>). Port of Tasks (libkingdom.so 5.11): Init
// @0x23aa70 and the lookups; the quests' progress itself is kept by GameState (completed and
// started tasks, the steps' counters, the show / begin / end times).
#pragma once
#include <cstdint>
#include <string>

class MetaData;
namespace Render { struct Texture; }
namespace Tags { struct Tag; }

namespace Tasks {

// TaskSubtask::type ("type" of <XSubtask>). 0x15, 0x1b, 0x1c, 0x21 have no name in 5.11.
enum SubtaskType {
    kNone = 0, kBuildHouse = 1, kBuildDecor = 2, kUpgradeHouse = 3, kDaily = 4, kIncreaseSpeed = 5,
    kIncreaseLand = 6, kGetLevel = 7, kBallance = 8, kFriendVisit = 9, kFriendHelp = 10,
    kUpgradeLevel = 0xb, kDeliveryMissionStart = 0xc, kDeliveryMissionEnd = 0xd, kDeliveryBuyCar = 0xe,
    kDeliveryMissionIncSpeed = 0xf, kFriendBless = 0x10, kBuildAny = 0x11, kUpgradeAny = 0x12,
    kFriendAdd = 0x13, kJoinCommunity = 0x14, kStartBuildingObject = 0x16,
    kBuildAndNotFinishObject = 0x17, kGiveJobTask = 0x18, kSellOnMarketTask = 0x19,
    kBuyOnMarketTask = 0x1a, kTrainWorker = 0x1d, kEnterMap = 0x1e, kKillMob = 0x1f, kGetItem = 0x20,
    kInteractBuilding = 0x22, kInteractBuildingAny = 0x23, kTrainWorkerInBuilding = 0x24,
    kTalkToPerson = 0x25, kEquip = 0x26, kHealHero = 0x27, kCollectSet = 0x28, kMasteryLevel = 0x29,
    kImproveItem = 0x2a, kBuyFarmPatch = 0x2b, kBallanceItem = 0x2c, kCompleteCompaign = 0x2d,
    kKillAllOnMap = 0x2e, kCastSpell = 0x2f, kTrainAnyWorker = 0x30, kMakeScreenshot = 0x31,
    kVisitFbGroup = 0x32, kVisitFbFaq = 0x33, kWriteComment = 0x34, kDailyBonus = 0x35,
    kWatchVideo = 0x36, kInviteFriend = 0x37, kLikeGame = 0x38, kBuyRealMoney = 0x39,
    kCraftItem = 0x3a, kIncreaseLandAny = 0x3b, kRepairItem = 0x3c, kKillSpawn = 0x3d,
    kGetReputation = 0x3e, kKillEnemyClass = 0x3f, kBalanceLevel = 0x40, kDealDamage = 0x41,
    kWinInRow = 0x42, kTameEnemyClass = 0x43, kOpenWeb = 0x44
};

struct TaskSubtask {             // 0x48 bytes, three per task (TaskInfo+0x18)
    uint32_t id = 0;             // +0x00 "id" (0: no step)
    int type = 0;                // +0x04 "type" (SubtaskType)
    uint32_t skipPrice = 0;      // +0x08 "skip_price"
    MetaData* required = nullptr;   // +0x0c "req_stId" a,b,... (steps to finish first)
    uint32_t value = 0;          // +0x10 "androidVal" when present and not empty, else "val"
    MetaData* values = nullptr;  // +0x14 "val" as a list when it has a comma
    int amount = 0;              // +0x18 "amount" n or n-m: how often
    int amountMin = 0;           // +0x1c n
    int amountMax = 0;           // +0x20 m (n without a range)
    uint32_t assistX = 0, assistY = 0, assistMap = 0;   // +0x24 +0x28 +0x2c "assist_x/_y/_map"
    std::string icon;            // +0x30 "icon" (a pooled name; images/<icon>)
    Render::Texture* iconImage = nullptr;   // +0x34 GetIcon's image
    bool iconMissing = false;    // +0x38
    const char32_t* buttonText = nullptr;   // +0x3c "button": the string key before the comma
    Render::Texture* buttonIcon = nullptr;  // +0x40 the icon after it (IconManager), else "gold_quest"
    MetaData* mobs = nullptr;    // +0x44 "mob_id" a,b,...
    Render::Texture* GetIcon();             // @0x22fbb4
    bool HasValue(uint32_t v) const;        // @0x230828 v is `value` or in `values`
    void SetAttribute(const char* name, const char* value);   // @0x22fd44
};

struct TaskInfo {                // 500 bytes
    uint32_t id = 0;             // +0x00 "id"
    uint32_t tab = 0;            // +0x04 "tab"
    std::string character;       // +0x08 "char" (a pooled name; images/<char>)
    Render::Texture* icon = nullptr;        // +0x0c GetIcon's image
    bool iconMissing = false;    // +0x10
    Render::Texture* character2 = nullptr;  // +0x14 "char2": images/<char2>
    TaskSubtask subtasks[3];     // +0x18
    // The conditions (<XCondition type= vmin=>):
    uint32_t level = 0;          // +0xf0 "level"
    MetaData* lockTasks = nullptr;   // +0xf4 "task" a,b,...: the tasks to complete first
    bool active = true;          // +0xf8 set by Init ("active" of SetAttribute)
    bool showInBattle = false;   // +0xf9 "show_in_battle"
    bool tutorialType = false;   // +0xfa "tutorial_type"
    bool isFarm = false;         // +0xfb "is_farm"
    bool isCity = false;         // +0xfc "is_city"
    bool isHint = false;         // +0xfd "is_hint"
    bool sidequest = false;      // +0xfe "sidequest"
    bool isDublicateTalk = false;   // +0xff "is_dublicate_talk"
    bool chapter = false;        // +0x100 "chapter"
    bool showOnImperor = false;  // +0x101 "show_on_imperor"
    bool quickTask = false;      // +0x102 "quick_task"
    bool dnAssist = false;       // +0x103 "dn_assist"
    bool dnShow = false;         // +0x104 "dn_show" (Tasks::Reset restores it from +0x105)
    bool dnShowDefault = false;  // +0x105
    bool showButton = false;     // +0x106 "show_button"
    bool doNotShowReward = false;   // +0x107 "do_not_show_reward"
    bool anySubquest = false;    // +0x108 "any_subquest"
    bool repeatTask = false;     // +0x109 "repeat_task"
    bool noComplete = false;     // +0x10a ("no_complete" of SetAttribute)
    bool doNotCount = false;     // +0x10b "do_not_count"
    bool showEverywhere = false; // +0x10c "show_everywhere"
    bool daily = false;          // +0x10d "daily"
    bool featured = false;       // +0x10e "featured"
    uint32_t mapId = 0;          // +0x110 "map_id" m[,x[,y]]
    uint32_t debugMap = 0;       // +0x114 "debugmap": the map the task belongs to
    int16_t mapX = 0, mapY = 0;  // +0x118 +0x11a
    uint32_t dialogGiver = 0;    // +0x11c "dialog_giver"
    uint32_t postQuest = 0;      // +0x120 "post_quest" (read as a number: the task after this one)
    uint32_t previousTask = 0;   // +0x124 the task whose postQuest this is
    uint32_t chapterNumber = 0;  // +0x128 "chapter_number"
    uint32_t disableTask = 0;    // +0x12c "disable_task"
    uint32_t repeatTimeout = 0;  // +0x130 "repeat_timeout"
    uint32_t timeout = 0;        // +0x134 "timeout": seconds from its begin time
    uint32_t endTime = 0;        // +0x138 an end date (set by the server only)
    // The reward (<XAward>): the resources 0..7, gold, crystals ("cb"), XP.
    uint32_t award[11] = {};     // +0x13c..+0x164
    struct ItemCount { uint32_t id = 0, count = 0; };
    ItemCount items[8];          // +0x168 "items" id[=n];...: reward items
    uint32_t itemCount = 0;      // +0x1a8
    ItemCount giveItems[4];      // +0x1ac "give_items" id[=n];...
    uint32_t giveItemCount = 0;  // +0x1cc
    const char32_t* name = nullptr;         // +0x1d0 string "TASK<id>"
    const char32_t* description = nullptr;  // +0x1d4 string "DESC<id>"
    const char32_t* preQuest = nullptr;     // +0x1d8 string of "pre_quest" (null when empty)
    bool hasReward = false;      // +0x1dc any award or item
    bool showQuest = false;      // +0x1dd "show_quest"
    bool f_1de = false;          // +0x1de (Tasks::Reset clears it)
    bool f_1df = false;          // +0x1df a hint / tutorial pop-up whose previous task is done: to be shown
    int priority = 0;            // +0x1e0 GetCurrentTasks' order key (2 main, 1 side, 10 featured, 0x32 campaign)
    bool f_1e4 = false;          // +0x1e4 came from the world map's campaign list
    bool campaignInfoKnown = false;   // +0x1e5 RefreshCampaignInfo ran
    uint32_t campaign = 0, mission = 0;   // +0x1e8 +0x1ec
    Tags::Tag* tag = nullptr;    // +0x1f0 "tags": the event the task belongs to

    bool IsFeaturedTask() const;            // @0x22e6f8
    void SetAttribute(const char* name, const char* value);   // @0x22ffbc
    Render::Texture* GetIcon();             // @0x22fae0
    bool HasTimeout() const;                // @0x22f960
    int GetRemainingTime() const;           // @0x230890
    bool CanBeCompleted() const;            // @0x22f9f4
    void RefreshCampaignInfo();             // @0x22fa9c
    void LockByTasks(const char* list);     // @0x22fc88
    void LockByTask(uint32_t task);         // @0x22fce0
    bool IsLockedByTask() const;            // @0x23067c
    uint32_t GetFirstLockedTaskInChain(bool skipHidden) const;   // @0x2306d4
};

bool Init(const char* file);                         // @0x23aa70
TaskInfo* GetTask(uint32_t id);                      // @0x22e74c
TaskInfo* GetTaskWithSubtask(uint32_t subtask);      // @0x22e7a8
TaskInfo* EnumTask(unsigned i);                      // @0x22e844 (file order)
TaskInfo* GetCampaignCompletionTask(uint32_t campaign);   // @0x22e868
TaskSubtask* GetSubtask(uint32_t subtask);           // @0x22e8c4
void SetBlocked(bool blocked);                       // @0x22e984
void EnqueueUpdate();                                // @0x22e998 the next Tasks::Update runs at once
void Reset();                                        // @0x22e9b0
bool HasTasksForLevel(uint32_t level);               // @0x22e9fc
int GetMapTaskCount(uint32_t map, bool mainOnly);    // @0x22ea5c
void SetLastSubTaskHelp(uint32_t subtask);           // @0x22eb84
void EnableLastSubTaskHelp();                        // @0x22eb98
inline uint32_t NormalizeMapID(uint32_t map) { return map == 0x2f ? 0 : map; }   // @0x22ebb0
// @0x22f1d0: the map's tasks that are (or are not) completed and are (or are not) side quests;
// unless `all`, hints, tutorial-type, hidden and repeated tasks are left out. Returns how many
// were written (at most max).
unsigned GetMapTasks(TaskInfo** out, unsigned max, uint32_t map, bool completed, bool sidequest, bool all);
int GetMapTaskCompleted(uint32_t map, bool mainOnly);          // @0x22f35c
void CompleteMapTasks(uint32_t map);                           // @0x22f4d0
uint32_t GetMapLatestUnfinishedTask(uint32_t map, bool mainOnly);   // @0x22f690
void ResetMap(uint32_t map);                                   // @0x22f814
int GetTaskSubProgress(uint32_t task, unsigned* total);        // @0x22f87c counted actions
int GetTaskProgress(uint32_t task, unsigned* total);           // @0x22f8f0 finished steps
TaskInfo* GetRepeatableParentTask(TaskInfo* task);             // @0x22f994
bool IsTaskActive(uint32_t task, bool started);                // @0x23096c

// ---- the quest engine
// @0x230dec: the tasks open right now (level reached, not completed, not locked, right place).
// forHud: the list the HUD shows (hidden tasks and plain "talk to" steps left out); all: no place
// or visibility rules at all. Returns the count (at most max).
unsigned GetCurrentTasks(TaskInfo** out, unsigned max, bool forHud, bool all);
// @0x2318a0: the player did something: every open step of that type and value counts n more;
// a step that reaches its amount is announced to the entities and the map.
void CompleteSubtask(int type, uint32_t value, int n);
void ResetSubtask(int type, uint32_t value);                   // @0x2317c0 such steps back to 0
void CompleteSubtaskDirect(uint32_t subtask, int n);           // @0x22fa5c by step id
// @0x236350: the task is completed: the next task's dialog, the world's reactions, the reward
// (dropped at the quest giver, the "talk to" person or the hero when `drop`, else added), the
// items it takes (takeItems).
void CompleteTask(uint32_t task, bool drop, bool takeItems, bool setBegin);
// @0x2377b0, once a second in the player's city (or at once when forced): begin times, the
// balance / item / level steps' counters, finished tasks, repeats and the daily tasks.
void Update(bool force);
void TaskTimeout(TaskInfo* task);                              // @0x2381b8
// @0x236f28: steps of these tasks whose goal is already met (an area owned, an item in the bag, no
// area left to buy ...) are completed, and then the task when its steps are done. withMobs: also
// the "kill" and "get item" steps whose listed monsters are all dead.
void CheckOverlimit(TaskInfo** tasks, unsigned count, bool withMobs);
// @0x22ff7c / @0x230524: a task's or a step's field changed by name (Override::Apply).
void ChangeSubTaskAttribute(uint32_t task, unsigned step, const char* name, const char* value);
void ChangeTaskAttribute(uint32_t task, const char* name, const char* value);
// @0x230a44: unless one of the groups' tasks is running, one of them that is open is started at
// random (the groups are settings listing task ids). True when one was started.
bool LaunchDailyTaskFromGroup(const char* const* groups, unsigned count, TaskInfo** current, unsigned currentCount);
// PORT (docs/balance_plan.md): the crystals a completed task pays offline, by its kind and the
// player's level; 0 for hints and tutorial pop-ups.
int PortQuestCrystals(const TaskInfo* task);

// The lists Init builds: the tasks with a repeat timeout (0x616984) and the tasks under one
// (0x616994).
TaskInfo* EnumRepeatTask(unsigned i);
TaskInfo* EnumRepeatChildTask(unsigned i);

}  // namespace Tasks
