#include "game/Tasks.h"

#include <pugixml.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>
#include <memory>
#include <vector>

#include "engine/FileManager.h"
#include "engine/IconManager.h"
#include "engine/Resources.h"
#include "engine/Timer.h"
#include "game/GameState.h"
#include "game/Items.h"
#include "game/MetaData.h"
#include "game/StringTable.h"
#include "game/Tags.h"

namespace Tasks {
namespace {

std::map<uint32_t, TaskInfo*> g_byId;                 // HashMap<unsigned, TaskInfo*, 1024> (0x616960)
std::vector<std::unique_ptr<TaskInfo>> g_tasks;       // 0x616964 (count 0x616970), file order
std::vector<TaskInfo*> g_repeatTasks;                 // 0x616984 (count 0x616990)
std::vector<TaskInfo*> g_repeatChildren;              // 0x616994 (count 0x6169a0)
bool g_blocked = false;                               // 0x616974
float g_updateTimer = 0.f;                            // 0x616978 (EnqueueUpdate zeroes it)
uint32_t g_lastSubTaskHelp = 0;                       // SetLastSubTaskHelp
bool g_lastSubTaskHelpEnabled = false;                // 0x616980

MetaData* ParseList(const char* text) {
    const char* p = text;
    return ParseCustomStyleData(p, ",", "1");
}

// "id[=n];id[=n];..." into at most `max` entries ("items", "give_items"). Returns whether any was added.
bool ParseItemList(uint32_t task, const char* what, const char* v, TaskInfo::ItemCount* out, uint32_t& count,
                   uint32_t max) {
    bool added = false;
    if (std::strchr(v, ':'))
        std::printf("ERROR: Tasks::Init() Task %d has an incorrect %s data format: '%s'\n", task, what, v);
    if (!*v) return false;
    while (count < max) {
        out[count].id = (uint32_t)std::atoi(v);
        out[count].count = 1;
        // (the original looks for '=' in the rest of the whole text, not only this entry)
        if (const char* eq = std::strchr(v, '=')) out[count].count = (uint32_t)std::atoi(eq + 1);
        ++count;
        added = true;
        const char* semi = std::strchr(v, ';');
        if (!semi || !semi[1]) break;
        v = semi + 1;
    }
    return added;
}

int SubtaskTypeFromName(const char* n) {
    static const struct { const char* name; int type; } kTypes[] = {
        {"build_house", 1}, {"build_decor", 2}, {"upgrade_house", 3}, {"daily", 4}, {"increase_speed", 5},
        {"increase_land", 6}, {"get_level", 7}, {"ballance", 8}, {"friend_visit", 9}, {"friend_help", 10},
        {"upgrade_level", 0xb}, {"delivery_mission_start", 0xc}, {"delivery_mission_end", 0xd},
        {"delivery_buy_car", 0xe}, {"delivery_mission_incspeed", 0xf}, {"friend_bless", 0x10},
        {"build_any", 0x11}, {"upgrade_any", 0x12}, {"friend_add", 0x13}, {"join_community", 0x14},
        {"start_building_object", 0x16}, {"build_and_not_finish_object", 0x17}, {"give_job_task", 0x18},
        {"sell_on_market_task", 0x19}, {"buy_on_market_task", 0x1a}, {"train_worker", 0x1d},
        {"enter_map", 0x1e}, {"kill_mob", 0x1f}, {"get_item", 0x20}, {"interact_building", 0x22},
        {"interact_building_any", 0x23}, {"train_worker_in_building", 0x24}, {"talk_to_person", 0x25},
        {"equip", 0x26}, {"heal_hero", 0x27}, {"collect_set", 0x28}, {"mastery_level", 0x29},
        {"improve_item", 0x2a}, {"buy_farm_patch", 0x2b}, {"ballance_item", 0x2c},
        {"complete_compaign", 0x2d}, {"kill_all_on_map", 0x2e}, {"cast_spell", 0x2f},
        {"train_any_worker", 0x30}, {"make_screenshot", 0x31}, {"visit_fb_group", 0x32},
        {"visit_fb_faq", 0x33}, {"write_comment", 0x34}, {"dailybonus", 0x35}, {"watch_video", 0x36},
        {"invite_friend", 0x37}, {"like_game", 0x38}, {"buy_real_money", 0x39}, {"craft_item", 0x3a},
        {"increase_land_any", 0x3b}, {"repair_item", 0x3c}, {"kill_spawn", 0x3d}, {"get_reputation", 0x3e},
        {"get_profession", 0x3e}, {"kill_enemy_class", 0x3f}, {"balance_level", 0x40},
        {"deal_damage", 0x41}, {"win_in_row", 0x42}, {"tame_enemy_class", 0x43}, {"open_web", 0x44}};
    for (const auto& t : kTypes)
        if (!std::strcmp(n, t.name)) {
            if (t.type == 8) std::puts("Tasks::Init() ballance task is unimplemented");
            if (t.type == 0xb) std::puts("Tasks::Init() upgrade_level task is unimplemented");
            return t.type;
        }
    if (*n) std::printf("ERROR: Tasks::Init() Unknown subtask type %s", n);
    return 0;
}

void FreeLists(TaskInfo* t) {
    for (TaskSubtask& s : t->subtasks) {
        delete s.values;
        delete s.required;
        delete s.mobs;
    }
    delete t->lockTasks;
}

// One <XCondition type= vmin=> of a task.
void ReadCondition(TaskInfo* t, pugi::xml_node c) {
    const char* type = c.attribute("type").value();
    pugi::xml_attribute v = c.attribute("vmin");
    auto is = [&](const char* n) { return !std::strcmp(type, n); };
    if (is("level")) t->level = v.as_uint();
    else if (is("task")) t->lockTasks = ParseList(v.value());
    else if (is("show_in_battle")) t->showInBattle = v.as_bool();
    else if (is("tutorial_type")) t->tutorialType = v.as_bool();
    else if (is("is_farm")) t->isFarm = v.as_bool();
    else if (is("map_id")) {
        t->mapId = v.as_uint();
        if (const char* comma = std::strchr(v.value(), ',')) {
            t->mapX = (int16_t)std::atoi(comma + 1);
            if ((comma = std::strchr(comma + 1, ','))) t->mapY = (int16_t)std::atoi(comma + 1);
        }
    }
    else if (is("socnet")) {}   // (the social network's own tasks: ignored)
    else if (is("dialog_giver")) t->dialogGiver = v.as_uint();
    else if (is("is_hint")) t->isHint = v.as_bool();
    else if (is("is_city")) t->isCity = v.as_bool();
    else if (is("dn_assist")) t->dnAssist = v.as_bool();
    else if (is("dn_show")) t->dnShowDefault = t->dnShow = v.as_bool();
    else if (is("is_dublicate_talk")) t->isDublicateTalk = v.as_bool();
    else if (is("sidequest")) t->sidequest = v.as_bool();
    else if (is("items")) {
        if (ParseItemList(t->id, "items", v.value(), t->items, t->itemCount, 8)) t->hasReward = true;
    }
    else if (is("give_items")) ParseItemList(t->id, "give_items", v.value(), t->giveItems, t->giveItemCount, 4);
    else if (is("debugmap")) t->debugMap = v.as_uint();
    else if (is("dn_show_reward")) {}   // (ignored)
    else if (is("show_button")) t->showButton = v.as_bool();
    else if (is("any_subquest")) t->anySubquest = v.as_bool();
    else if (is("repeat_task")) t->repeatTask = v.as_bool();
    else if (is("do_not_show_reward")) t->doNotShowReward = v.as_bool();
    else if (is("chapter")) t->chapter = v.as_bool();
    else if (is("chapter_number")) t->chapterNumber = v.as_uint();
    else if (is("disable_task")) t->disableTask = v.as_uint();
    else if (is("repeat_timeout")) t->repeatTimeout = v.as_uint();
    else if (is("show_on_imperor")) t->showOnImperor = v.as_bool();
    else if (is("no_reward")) {}   // (ignored)
    else if (is("timeout")) t->timeout = v.as_uint();
    else if (is("quick_task")) t->quickTask = v.as_bool();
    else if (is("do_not_count")) t->doNotCount = v.as_bool();
    else if (is("show_everywhere")) t->showEverywhere = v.as_bool();
    else if (is("daily")) t->daily = v.as_bool();
    else if (is("featured")) t->featured = v.as_bool();
    else if (is("tags")) t->tag = Tags::GetTag(v.value());
    else if (*type) std::printf("ERROR: Tasks::Init() Unknown task flag %s", type);
}

void ReadSubtask(TaskSubtask& s, pugi::xml_node n) {
    s.id = n.attribute("id").as_uint();
    s.icon = n.attribute("icon").value();
    // "button": "<string key>[,<icon>]"
    std::string button = n.attribute("button").value();
    size_t comma = button.find(',');
    s.buttonText = StringTable::GetString(button.substr(0, comma).c_str());
    s.buttonIcon = comma == std::string::npos ? IconManager::GetIcon("gold_quest", false)
                                              : IconManager::GetIcon(button.c_str() + comma + 1, true);
    s.type = SubtaskTypeFromName(n.attribute("type").value());
    pugi::xml_attribute android = n.attribute("androidVal");
    if (android && *android.value()) {
        s.value = android.as_uint();
    } else {
        s.value = n.attribute("val").as_uint();
        if (std::strchr(n.attribute("val").value(), ',')) s.values = ParseList(n.attribute("val").value());
    }
    const char* amount = n.attribute("amount").value();
    s.amount = s.amountMin = s.amountMax = std::atoi(amount);
    if (const char* dash = std::strchr(amount, '-')) s.amountMax = std::atoi(dash + 1);
    s.assistX = n.attribute("assist_x").as_uint();
    s.assistY = n.attribute("assist_y").as_uint();
    s.assistMap = n.attribute("assist_map").as_uint();
    s.skipPrice = n.attribute("skip_price").as_uint();
    s.required = ParseList(n.attribute("req_stId").value());
    s.mobs = ParseList(n.attribute("mob_id").value());
}

}  // namespace

// @0x23aa70 Tasks::Init. A task read again (a content pack's copy of the file) replaces the first.
// Not ported: flagDemoVersion (the demo skips task 0x2da) and the three "can't be completed on
// any map except ..." error reports for buy / improve / equip / repair item steps.
bool Init(const char* file) {
    double t0 = Timer::GetTime();
    uint32_t size = 0;
    uint8_t* buf = FileManager::LoadFile(file, size);
    if (!buf) {
        std::printf("Tasks::Init() Failed to load task list (File not found: %s)\n", file);
        return false;
    }
    pugi::xml_document doc;
    pugi::xml_parse_result r = doc.load_buffer_inplace(buf, size, pugi::parse_default, pugi::encoding_auto);
    if (!r) {
        std::printf("Tasks::Init() Failed to parse task list (%s)\n", r.description());
        FileManager::FreeFile(buf);
        return false;
    }
    pugi::xml_node root = doc.child("XTasks");
    if (!root) {
        std::printf("Tasks::Init() Incorrect file format (%s)\n", file);
        FileManager::FreeFile(buf);
        return false;
    }
    for (pugi::xml_node x = root.child("XTask"); x; x = x.next_sibling("XTask")) {
        const uint32_t id = x.attribute("id").as_uint();
        TaskInfo* t = GetTask(id);
        if (!t) {
            g_tasks.push_back(std::make_unique<TaskInfo>());
            t = g_tasks.back().get();
            g_byId[id] = t;
        } else {
            FreeLists(t);
            *t = TaskInfo();
        }
        t->id = id;
        t->tab = x.attribute("tab").as_uint();
        t->character = x.attribute("char").value();
        const char* char2 = x.attribute("char2").value();
        t->character2 = *char2 ? Resources::GetImage((std::string("images/") + char2).c_str()) : nullptr;
        for (pugi::xml_node c = x.child("XCondition"); c; c = c.next_sibling("XCondition")) ReadCondition(t, c);
        unsigned n = 0;
        for (pugi::xml_node s = x.child("XSubtask"); s; s = s.next_sibling("XSubtask")) {
            // PORT: the original has no limit (no task in 5.11 has more than three steps).
            if (n == 3) break;
            ReadSubtask(t->subtasks[n++], s);
        }
        pugi::xml_node award = x.child("XAward");
        static const char* const kAward[11] = {"lumber", "rocks", "food", "planks", "stones", "meat",
                                               "sausage", "oil", "gold", "cb", "xp"};
        for (int i = 0; i < 11; ++i) {
            t->award[i] = award.attribute(kAward[i]).as_uint();
            if (t->award[i]) t->hasReward = true;
        }
        const char* pre = x.attribute("pre_quest").value();
        t->preQuest = *pre ? StringTable::GetString(pre) : nullptr;
        t->postQuest = x.attribute("post_quest").as_uint();
        t->showQuest = x.attribute("show_quest").as_bool();
        char key[64];
        std::snprintf(key, sizeof key, "TASK%d", id);
        // (with the HUD's debug state on, the name is prefixed with the task's id)
        t->name = StringTable::GetString(key);
        std::snprintf(key, sizeof key, "DESC%d", id);
        t->description = StringTable::GetString(key);
        if (t->repeatTimeout != 0) {
            bool listed = false;
            for (TaskInfo* o : g_repeatTasks) listed = listed || o->id == id;
            if (!listed) g_repeatTasks.push_back(t);
        }
    }
    FileManager::FreeFile(buf);
    double t1 = Timer::GetTime();
    std::printf("Tasks::Init() Loaded task list in %.3fms\n", (t1 - t0) * 1000.0);
    for (auto& t : g_tasks)
        if (TaskInfo* next = GetTask(t->postQuest)) next->previousTask = t->id;
    for (auto& t : g_tasks) {
        if (!GetRepeatableParentTask(t.get())) continue;
        bool listed = false;
        for (TaskInfo* o : g_repeatChildren) listed = listed || o->id == t->id;
        if (!listed) g_repeatChildren.push_back(t.get());
    }
    return true;
}

// @0x22e74c Tasks::GetTask
TaskInfo* GetTask(uint32_t id) {
    auto it = g_byId.find(id);
    return it == g_byId.end() ? nullptr : it->second;
}

// @0x22e7a8 Tasks::GetTaskWithSubtask
TaskInfo* GetTaskWithSubtask(uint32_t subtask) {
    if (subtask == 0) return nullptr;
    for (auto& t : g_tasks)
        for (const TaskSubtask& s : t->subtasks)
            if (s.id == subtask) return t.get();
    return nullptr;
}

// @0x22e844 Tasks::EnumTask
TaskInfo* EnumTask(unsigned i) { return i < g_tasks.size() ? g_tasks[i].get() : nullptr; }

TaskInfo* EnumRepeatTask(unsigned i) { return i < g_repeatTasks.size() ? g_repeatTasks[i] : nullptr; }
TaskInfo* EnumRepeatChildTask(unsigned i) { return i < g_repeatChildren.size() ? g_repeatChildren[i] : nullptr; }

// @0x22e868 Tasks::GetCampaignCompletionTask: the task whose first step is "complete campaign n".
TaskInfo* GetCampaignCompletionTask(uint32_t campaign) {
    for (auto& t : g_tasks)
        if (t->subtasks[0].type == kCompleteCompaign && t->subtasks[0].value == campaign) return t.get();
    return nullptr;
}

// @0x22e8c4 Tasks::GetSubtask
TaskSubtask* GetSubtask(uint32_t subtask) {
    if (subtask == 0) return nullptr;
    for (auto& t : g_tasks)
        for (TaskSubtask& s : t->subtasks)
            if (s.id == subtask) return &s;
    return nullptr;
}

// @0x22e984 Tasks::SetBlocked
void SetBlocked(bool blocked) { g_blocked = blocked; }

// @0x22e998 Tasks::EnqueueUpdate
void EnqueueUpdate() { g_updateTimer = 0.f; }

// @0x22e9b0 Tasks::Reset
void Reset() {
    for (auto& t : g_tasks) {
        t->f_1de = false;
        t->dnShow = t->dnShowDefault;
    }
}

// @0x22e9fc Tasks::HasTasksForLevel
bool HasTasksForLevel(uint32_t level) {
    for (auto& t : g_tasks)
        if (t->level == level) return true;
    return false;
}

namespace {
// The tasks a map's progress counts: not hints, tutorial-type, hidden, repeated or uncounted
// tasks, nor the "complete the campaign" task itself.
bool CountsForMap(const TaskInfo& t) {
    return !t.isHint && !t.tutorialType && !t.dnShow && !t.repeatTask && !t.doNotCount &&
           t.subtasks[0].type != kCompleteCompaign;
}
}  // namespace

// @0x22ea5c Tasks::GetMapTaskCount (mainOnly: without the side quests)
int GetMapTaskCount(uint32_t map, bool mainOnly) {
    int n = 0;
    for (auto& t : g_tasks)
        if (t->debugMap == map && CountsForMap(*t) && !(mainOnly && t->sidequest)) ++n;
    return n;
}

// @0x22eb84 Tasks::SetLastSubTaskHelp
void SetLastSubTaskHelp(uint32_t subtask) { g_lastSubTaskHelp = subtask; }

// @0x22eb98 Tasks::EnableLastSubTaskHelp
void EnableLastSubTaskHelp() { g_lastSubTaskHelpEnabled = true; }

// @0x22f1d0 Tasks::GetMapTasks
unsigned GetMapTasks(TaskInfo** out, unsigned max, uint32_t map, bool completed, bool sidequest, bool all) {
    unsigned n = 0;
    for (auto& t : g_tasks) {
        if (t->debugMap != map) continue;
        if (!all && (t->isHint || t->tutorialType || t->dnShow || t->repeatTask)) continue;
        if (GameState::TaskCompleted(t->id) != completed || t->sidequest != sidequest) continue;
        if (n < max) out[n++] = t.get();
    }
    return n;
}

// @0x22f35c Tasks::GetMapTaskCompleted
int GetMapTaskCompleted(uint32_t map, bool mainOnly) {
    int n = 0;
    for (auto& t : g_tasks)
        if (t->debugMap == map && CountsForMap(*t) && !(mainOnly && t->sidequest) &&
            GameState::TaskCompleted(t->id))
            ++n;
    return n;
}

// @0x22f4d0 Tasks::CompleteMapTasks
void CompleteMapTasks(uint32_t map) {
    for (auto& t : g_tasks)
        if (t->debugMap == map) GameState::CompleteTask(t->id, false);
}

// @0x22f690 Tasks::GetMapLatestUnfinishedTask: the unfinished, shown task of the map that was
// shown last (0: none).
uint32_t GetMapLatestUnfinishedTask(uint32_t map, bool mainOnly) {
    uint32_t latest = 0, id = 0;
    for (auto& t : g_tasks) {
        if (t->debugMap != map || GameState::TaskCompleted(t->id)) continue;
        if (!(latest < GameState::GetTaskShowTime(t->id))) continue;
        if (t->f_1df || t->dnShow || (mainOnly && t->sidequest)) continue;
        latest = GameState::GetTaskShowTime(t->id);
        id = t->id;
    }
    return id;
}

// @0x22f814 Tasks::ResetMap
void ResetMap(uint32_t map) {
    for (auto& t : g_tasks)
        if (t->debugMap == map) GameState::ResetTask(t->id);
}

// @0x22f87c Tasks::GetTaskSubProgress: the actions done over the actions asked, all steps together.
int GetTaskSubProgress(uint32_t task, unsigned* total) {
    TaskInfo* t = GetTask(task);
    if (!t) return 0;
    *total = 0;
    int done = 0;
    for (const TaskSubtask& s : t->subtasks) {
        if (s.type == 0) continue;
        unsigned d = (unsigned)GameState::GetSubTaskDone(s.id), need = (unsigned)s.amount;
        *total += need;
        done += (int)(need < d ? need : d);
    }
    return done;
}

// @0x22f8f0 Tasks::GetTaskProgress: the finished steps over the steps.
int GetTaskProgress(uint32_t task, unsigned* total) {
    TaskInfo* t = GetTask(task);
    if (!t) return 0;
    *total = 0;
    int done = 0;
    for (const TaskSubtask& s : t->subtasks) {
        if (s.type == 0) continue;
        ++*total;
        if (s.amount <= GameState::GetSubTaskDone(s.id)) ++done;
    }
    return done;
}

// @0x22f994 Tasks::GetRepeatableParentTask: up the chain of first locking tasks, the first one
// with a repeat timeout.
TaskInfo* GetRepeatableParentTask(TaskInfo* task) {
    TaskInfo* found = nullptr;
    while (task) {
        if (!task->lockTasks) return nullptr;
        // PORT: an empty list has no first entry (the original reads it unchecked)
        const MetaData* first = task->lockTasks->GetChild(0);
        if (!first) return nullptr;
        uint32_t parent = (uint32_t)first->GetInt();
        if (task->id == parent) return nullptr;
        task = GetTask(parent);
        if (!task) return nullptr;
        found = task;
        if (task->repeatTimeout != 0) break;
    }
    return found;
}

// @0x23096c Tasks::IsTaskActive: begun, not ended or completed, its event (tag) running and its
// time not up; with `started`, also started.
bool IsTaskActive(uint32_t task, bool started) {
    TaskInfo* t = GetTask(task);
    if (!t || (t->tag && !t->tag->IsActive())) return false;
    if (!GameState::GetTaskBeginTime(task) || GameState::GetTaskEndTime(task) || GameState::TaskCompleted(task))
        return false;
    if (t->HasTimeout() && t->GetRemainingTime() == 0) return false;
    return started ? GameState::IsTaskStarted(task) : true;
}

// ------------------------------------------------------------------------------------ TaskInfo

// @0x22e6f8 Tasks::TaskInfo::IsFeaturedTask
bool TaskInfo::IsFeaturedTask() const {
    if (id == 0x4d8 || id == 0x4da || id == 0x4db || id == 0x6b4 || id == 0x75b || id == 0x77b) return true;
    return featured;
}

// @0x22fae0 Tasks::TaskInfo::GetIcon
Render::Texture* TaskInfo::GetIcon() {
    if (iconMissing) return nullptr;
    if (icon) return icon;
    if (!character.empty()) {
        icon = Resources::GetImage(("images/" + character).c_str());
        // (the original also sets the texture's +0x48 flag)
        if (icon) return icon;
    }
    iconMissing = true;
    return nullptr;
}

// @0x22fbb4 Tasks::TaskSubtask::GetIcon
Render::Texture* TaskSubtask::GetIcon() {
    if (iconMissing) return nullptr;
    if (iconImage) return iconImage;
    if (!icon.empty()) {
        iconImage = Resources::GetImage(("images/" + icon).c_str());
        if (iconImage) return iconImage;
    }
    iconMissing = true;
    return nullptr;
}

// @0x230828 Tasks::TaskSubtask::HasValue
bool TaskSubtask::HasValue(uint32_t v) const {
    if (values)
        for (const MetaData* m = values->GetChild(0); m; m = m->GetNext())
            if ((uint32_t)m->GetInt() == v) return true;
    return value == v;
}

// @0x22f960 Tasks::TaskInfo::HasTimeout
bool TaskInfo::HasTimeout() const {
    if (timeout == 0 && endTime == 0) return false;
    return GameState::GetTaskBeginTime(id) != 0;
}

// @0x230890 Tasks::TaskInfo::GetRemainingTime: seconds left of the timeout (from the begin time)
// and of the end date, whichever is less; 0 when not begun or out of time.
int TaskInfo::GetRemainingTime() const {
    int begin = (int)GameState::GetTaskBeginTime(id);
    if (begin == 0) return 0;
    int byTimeout = timeout ? begin + (int)timeout - (int)Timer::GetGlobalTime() : 0;
    // (GameState::GetServerTimeOffset is 0 offline)
    int byDate = endTime ? (int)endTime - (int)std::time(nullptr)   /* Timer::GetRealGlobalTime */ : 0;
    if (timeout == 0) return endTime != 0 && byDate > 0 ? byDate : 0;
    if (endTime != 0) {
        int left = byTimeout <= byDate ? byTimeout : byDate;
        return left < 0 ? 0 : left;
    }
    return byTimeout < 0 ? 0 : byTimeout;
}

// @0x22f9f4 Tasks::TaskInfo::CanBeCompleted: false when a step asks for an item ("get_item",
// "ballance_item") of a profession level the player does not have.
bool TaskInfo::CanBeCompleted() const {
    for (const TaskSubtask& s : subtasks) {
        if (s.type != kBallanceItem && s.type != kGetItem) continue;
        const Items::ItemInfo* item = Items::GetItemInfo(s.value);
        if (!item || item->reputationId == 0) continue;
        // UNVERIFIED (milestone 5): Professions::GetPlayerProfessionLevel(item->reputationId) must
        // reach item->reputationLevel; without professions the player's level is 0.
        if (item->reputationLevel > 0) return false;
    }
    return true;
}

// @0x22fa9c Tasks::TaskInfo::RefreshCampaignInfo
void TaskInfo::RefreshCampaignInfo() {
    if (campaignInfoKnown) return;
    // UNVERIFIED (milestone 4f): Locations::GetCampaignForMap / GetMissionForMap of mapId (or, when
    // 0, of debugMap) into campaign and mission.
    campaignInfoKnown = true;
}

// @0x22fc88 Tasks::TaskInfo::LockByTasks
void TaskInfo::LockByTasks(const char* list) {
    delete lockTasks;
    lockTasks = ParseList(list);
}

// @0x22fce0 Tasks::TaskInfo::LockByTask
void TaskInfo::LockByTask(uint32_t task) {
    char text[32];
    std::snprintf(text, sizeof text, "%d", task);
    LockByTasks(text);
}

// @0x23067c Tasks::TaskInfo::IsLockedByTask
bool TaskInfo::IsLockedByTask() const {
    if (lockTasks)
        for (const MetaData* m = lockTasks->GetChild(0); m; m = m->GetNext())
            if (!GameState::TaskCompleted((unsigned)m->GetInt())) return true;
    return false;
}

// @0x2306d4 Tasks::TaskInfo::GetFirstLockedTaskInChain: down the chain of tasks that lock this
// one, the first that is not itself locked (at most 0x40 steps). A tutorial-type task goes to its
// previous task instead. With skipHidden the walk stops before a tutorial-type task, a hint or a
// lone "talk to" task.
uint32_t TaskInfo::GetFirstLockedTaskInChain(bool skipHidden) const {
    uint32_t cur = id;
    const TaskInfo* t = GetTask(cur);
    for (int n = 0;;) {
        if (!t) return cur;
        uint32_t next;
        if (t->tutorialType && t->previousTask != 0) {
            next = t->previousTask;
        } else {
            if (!t->IsLockedByTask()) return cur;
            // The first uncompleted locking task; a 0 entry is passed over when more follow.
            next = 0;
            for (const MetaData* m = t->lockTasks->GetChild(0); m;) {
                if (GameState::TaskCompleted((unsigned)m->GetInt())) {
                    next = 0;
                    m = m->GetNext();
                    continue;
                }
                next = (uint32_t)m->GetInt();
                m = m->GetNext();
                if (!(next == 0 && m)) break;
            }
            const TaskInfo* nt = GetTask(next);
            if (!nt) return cur;
            if (skipHidden) {
                if (nt->tutorialType || nt->isHint) return cur;
                if (nt->subtasks[0].type == kTalkToPerson && nt->subtasks[1].type == 0) return cur;
            }
        }
        if (++n == 0x40) return next;
        t = GetTask(next);
        cur = next;
    }
}

}  // namespace Tasks
