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
#include "game/Locations.h"
#include "game/Professions.h"
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
uint32_t g_lastUpdate = 0;                            // 0x616978 the second of the last Update (0: run now)
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
void EnqueueUpdate() { g_lastUpdate = 0; }

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
        if (Professions::GetPlayerProfessionLevel(item->reputationId) < item->reputationLevel) return false;
    }
    return true;
}

// @0x22fa9c Tasks::TaskInfo::RefreshCampaignInfo
void TaskInfo::RefreshCampaignInfo() {
    if (campaignInfoKnown) return;
    uint32_t map = mapId ? mapId : debugMap;
    campaign = Locations::GetCampaignForMap(map);
    mission = Locations::GetMissionForMap(map);
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

// =========================================================================== the quest engine
#include "game/BuildingHovers.h"
#include "game/BuildingMovement.h"
#include "game/BuildingPlacement.h"
#include "game/Entity.h"
#include "game/EntityManager.h"
#include "game/Map.h"
#include "game/Rand48.h"
#include "game/Setting.h"
#include "game/SoldierSlots.h"
#include "gui/WindowManager.h"
#include "hud/HUD.h"
#include "windows/Windows.h"

namespace Tasks {
namespace {

// Quest givers and "talk to" persons are entities of spawn points.
Entity* EntityBySpawnPoint(uint32_t id) { return EntityManager::GetEntityBySpawnPoint(id); }

// Every non-zero numeric entry of a step's "req_stId" list must be a completed step of the task.
bool RequiredStepsDone(const TaskInfo* t, const TaskSubtask& s) {
    if (!s.required) return true;
    for (unsigned i = 0; i < s.required->GetChildrenCount(); ++i) {
        const MetaData* m = s.required->GetChild(i);
        if (m->type == MetaData::kNumber && m->GetInt() != 0 &&
            !GameState::SubTaskCompleted(t->id, (unsigned)m->GetInt()))
            return false;
    }
    return true;
}

}  // namespace

// @0x230dec Tasks::GetCurrentTasks
unsigned GetCurrentTasks(TaskInfo** out, unsigned max, bool forHud, bool all) {
    unsigned count = 0;
    const int location = GameState::GetCurrentLocation();
    // For the HUD outside the campaign maps and the farm, the world map's campaign tasks come
    // first (priority 5; timed-out ones are left out). f_1e4: a counted campaign's task, else a
    // mission's that is not a side quest.
    if (forHud && !all && location != 2 && location != 1) {
        TaskInfo* campaignTasks[0x20] = {};
        const unsigned n = GlobalMapWindow::GetCampaignTasks(campaignTasks, 0x20);
        for (unsigned i = 0; i < n; ++i) {
            TaskInfo* t = campaignTasks[i];
            t->RefreshCampaignInfo();
            if (t->HasTimeout() && t->GetRemainingTime() == 0) continue;
            if (count >= max) continue;
            t->priority = 5;
            const bool flag = (t->campaign != 0 && !t->doNotCount) || (t->mission != 0 && !t->sidequest);
            out[count++] = t;
            t->f_1e4 = flag;
        }
    }
    const uint32_t curMap = GameState::GetCurrentMapID();
    const uint32_t campaign = Locations::GetCampaignForMap(curMap), mission = Locations::GetMissionForMap(curMap);
    const uint32_t level = (uint32_t)GameState::GetLevel();
    for (auto& up : g_tasks) {
        TaskInfo* t = up.get();
        if (!(t->level <= level) || GameState::TaskCompleted(t->id)) continue;
        bool ok = !(t->id == 0x3d3 && !GameState::IsTaskStarted(0x3d3));
        if (t->IsLockedByTask()) ok = false;
        bool special = false;
        if (!all) {
            if (!forHud) {
                // a task of nothing but "mastery level" steps (or none) is open everywhere
                for (const TaskSubtask& s : t->subtasks) {
                    special = s.type == kMasteryLevel || s.type == 0;
                    if (!special) break;
                }
            }
            if (t->showEverywhere && location != 1) special = true;
            uint32_t map = curMap;
            bool placeOk = ok;
            bool keepSpecial = false;
            if (ok && !special) {
                placeOk = false;
                if (!t->isFarm && !t->isCity) {
                    placeOk = !t->showInBattle;
                } else {
                    if (t->isFarm && (location == 1 || location == 0)) placeOk = true;
                    if (t->isCity && (location == 0 || (!forHud && location == 1 && t->chapterNumber == 0)))
                        placeOk = true;
                }
                if (t->showInBattle && (location == 2 || location == 3)) placeOk = true;
                // (the emperor's map shows such a task whatever its place)
                keepSpecial = t->showOnImperor && curMap == 0xb;
            }
            if (keepSpecial) {
                special = true;
            } else {
                ok = placeOk;
                if (curMap == 0) map = 0x2f;
                special = false;
            }
            if (t->mapId != 0 && map != t->mapId && (t->mapX == 0 || map != (uint16_t)t->mapX)) {
                if (t->mapY == 0 || map != (uint16_t)t->mapY) ok = false;
            }
        }
        // A tutorial pop-up or a hint shows once its previous task is done (never in the lists).
        if (t->tutorialType && t->subtasks[0].type == 0) {
            if (forHud || all || t->previousTask == 0 || !GameState::TaskCompleted(t->previousTask)) ok = false;
            else t->f_1df = true;
        }
        bool filters = true;
        if (t->isHint) {
            if (forHud || all || t->previousTask == 0 || !GameState::TaskCompleted(t->previousTask)) {
                ok = false;
            } else {
                t->f_1df = true;
                filters = false;
            }
        }
        if (filters) {
            if (forHud && t->dnShow) ok = false;
            if (forHud || all) {
                if (t->chapter) ok = false;
                if (forHud && !t->isDublicateTalk)
                    for (const TaskSubtask& s : t->subtasks)
                        if (s.type == kTalkToPerson) ok = false;
            }
        }
        // Already in the list? (the original does not compare with the last entry)
        bool take = false, evaluate = true;
        if (count != 0) {
            if (!ok) evaluate = false;
            for (unsigned k = 0; ok && k + 1 < count; ++k)
                if (out[k]->id == t->id) { evaluate = false; break; }
        }
        if (evaluate && ok) {
            if (!t->campaignInfoKnown) t->RefreshCampaignInfo();
            bool checkTime = true;
            if (forHud && !all && location == 2 && !t->showEverywhere) {
                bool here = ok;
                if (campaign != 0 && campaign != t->campaign) here = false;
                if (mission != 0 && mission != t->mission) here = false;
                if (mission == 0 && campaign == 0 && (t->campaign != 0 || t->mission != 0) && !special) here = false;
                if (1 < t->id - 0x4d7u) ok = here;
                // (the discount tasks 0x4d9..0x4db are taken even when not here)
                if (1 < t->id - 0x4d9u && t->id != 0x4db && !ok) checkTime = false;
            }
            if (checkTime) {
                if ((!t->daily || !t->dnShow) && t->HasTimeout() && t->GetRemainingTime() == 0) take = false;
                else take = !forHud || all || !t->IsFeaturedTask() || !GameState::IsFeaturedTaskClicked(t->id);
            }
        }
        if (t->tag && !t->tag->IsActive()) take = false;
        if (!t->active || !take || max <= count) continue;
        t->priority = t->sidequest ? 1 : 2;
        if (t->id == 0x3d3) t->priority = 10;
        if (t->id == 0x2f9) t->priority = 9;
        if (t->id == 0x4d7 || t->id == 0x4d9) t->priority = 10;
        if (t->IsFeaturedTask()) t->priority = 10;
        out[count++] = t;
        t->f_1e4 = false;
    }
    if (forHud) {
        if (location == 0 && count != 0) {
            // In the city the newest campaign (else mission) battle task leads; the other battle
            // tasks fall behind.
            unsigned bestCampaign = ~0u, bestMission = ~0u;
            for (unsigned i = 0; i < count; ++i) {
                TaskInfo* t = out[i];
                if (!t->showInBattle || t->sidequest) continue;
                if (t->campaign != 0 && (bestCampaign == ~0u || GameState::GetTaskBeginTime(t->id) >
                                                                    GameState::GetTaskBeginTime(out[bestCampaign]->id)))
                    bestCampaign = i;
                if (t->mission != 0 && (bestMission == ~0u || GameState::GetTaskBeginTime(t->id) >
                                                                  GameState::GetTaskBeginTime(out[bestMission]->id)))
                    bestMission = i;
            }
            unsigned lead = bestCampaign != ~0u ? bestCampaign : bestMission;
            if (lead != ~0u) out[lead]->priority = 0x32;
            for (unsigned i = 0; i < count; ++i) {
                TaskInfo* t = out[i];
                if (i == lead || !t->showInBattle) continue;
                if (!t->isCity || t->sidequest) t->priority = t->campaign == 0 ? 1 : 0;
                else t->priority += 1;
            }
        }
        // In a finished campaign its "complete the campaign" task comes last.
        if (campaign != 0 && !all) {
            Locations::LocationInfo* loc = Locations::GetCampaignLocation(campaign);
            if (loc && GameState::GetCampaignCompletionStatus(loc->id)) {
                if (TaskInfo* t = GetCampaignCompletionTask(loc->id)) {
                    t->RefreshCampaignInfo();
                    t->priority = 0x32;
                    out[count++] = t;
                }
            }
        }
    }
    return count;
}

// @0x2318a0 Tasks::CompleteSubtask
void CompleteSubtask(int type, uint32_t value, int n) {
    TaskInfo* current[0x80] = {};
    unsigned count = GetCurrentTasks(current, 0x80, false, false);
    bool changed = false;
    for (unsigned i = 0; i < count; ++i) {
        TaskInfo* t = current[i];
        for (TaskSubtask& s : t->subtasks) {
            if (s.type != type || !s.HasValue(value)) continue;
            if (t->dnShow) {
                if (t->daily) continue;
                if (!RequiredStepsDone(t, s)) continue;
            }
            if (s.type == kKillEnemyClass && s.assistMap == 0x74) {
                // UNVERIFIED (milestone 5): only during a PvP fight (GameState::IsPvPCombatActive).
                continue;
            }
            if (GameState::GetSubTaskDone(s.id) < s.amount) {
                GameState::AddSubTask(s.id, n);
                if (s.amount <= GameState::GetSubTaskDone(s.id)) {
                    changed = true;
                    EntityManager::OnSubTaskCompleted(s.id);
                    Map::OnSubTaskCompleted(s.id);
                    continue;
                }
            }
            changed = true;
        }
    }
    if (changed) {
        EnqueueUpdate();
        HUDWindow::UpdateTasks();
    }
}

// @0x2317c0 Tasks::ResetSubtask
void ResetSubtask(int type, uint32_t value) {
    TaskInfo* current[0x80] = {};
    unsigned count = GetCurrentTasks(current, 0x80, false, false);
    bool changed = false;
    for (unsigned i = 0; i < count; ++i)
        for (TaskSubtask& s : current[i]->subtasks)
            if (s.type == type && s.HasValue(value)) {
                changed = true;
                GameState::SetSubTaskDone(s.id, 0);
            }
    if (changed) {
        EnqueueUpdate();
        HUDWindow::UpdateTasks();
    }
}

// @0x22fa5c Tasks::CompleteSubtaskDirect
void CompleteSubtaskDirect(uint32_t subtask, int n) {
    GameState::AddSubTask(subtask, n);
    EntityManager::OnSubTaskCompleted(subtask);
    Map::OnSubTaskCompleted(subtask);
    EnqueueUpdate();
    HUDWindow::UpdateTasks();
}

int PortQuestCrystals(const TaskInfo* t) {
    if (t->isHint || t->tutorialType) return 0;
    const char* kind;
    if (t->subtasks[0].type == kCompleteCompaign) kind = "port_quest_crystals_campaign";
    else if (t->daily) kind = "port_quest_crystals_daily";
    else if (!t->hasReward) return 0;
    else kind = t->sidequest ? "port_quest_crystals_side" : "port_quest_crystals_main";
    float base = GameState::GetSetting(kind);
    float div = GameState::GetSetting("port_level_factor_div");
    float factor = div > 0.f ? 1.f + (float)(GameState::GetLevel() - 1) / div : 1.f;
    int n = (int)(base * factor + 0.5f);
    return base > 0.f && n < 1 ? 1 : n;
}

// @0x236350 Tasks::CompleteTask. Not ported (online): the OG / OG2 requests and the "invite
// friends" wall post of the fb_invite_task.
void CompleteTask(uint32_t id, bool drop, bool takeItems, bool setBegin) {
    TaskInfo* t = GetTask(id);
    if (!t) return;
    std::printf("Done task %d. Is battle: %d Is city: %d Is farm: %d Is hint: %d Dialog giver: %d Next task: %d\n",
                t->id, t->showInBattle, t->isCity, t->isFarm, t->isHint, t->dialogGiver, t->postQuest);
    // The next task's opening words, said by its giver (or the hero's own panel).
    if (t->postQuest != 0) {
        if (TaskInfo* next = GetTask(t->postQuest)) {
            if (GameState::TaskCompleted(next->id)) {
                std::printf("Tasks::CompleteTask() Cannot show next dialog %d - it was already completed\n", t->postQuest);
            } else {
                if (next->dialogGiver == 0) {
                    if (EntityManager::GetPlayer())
                        PlayerTopWindow::SetParameters(next->preQuest, next->id, !next->showButton, next->showButton);
                } else {
                    Entity* giver = EntityManager::GetEntityBySpawnPoint(next->dialogGiver);
                    if (giver && giver->IsActive())
                        BuildingHovers::ShowWorldDialog(giver, next->preQuest, next->id, false, !next->showButton, false,
                                                        t->repeatTask && t->repeatTimeout == 0 ? t->id : 0, false,
                                                        false);
                }
            }
        }
    }
    GameState::CompleteTask(id, setBegin);
    Map::OnTaskCompleted(id);
    EntityManager::OnTaskCompleted(id);
    GlobalMapWindow::OnTaskCompleted(id);
    BuildingHovers::HideWorldDialogByTask(id);
    if ((int)id == Setting("squad_slot_02_quest_unlock").GetInt() ||
        (int)id == Setting("squad_slot_03_quest_unlock").GetInt()) {
        // UNVERIFIED (milestone 5): WindowManager::EnqueueWindow(Tasks::ShowWarriorSlotUnlock, 0.2).
    }
    // (task 0x402: CraftSuccessWindow for unit 0x195, milestone 5)

    // Where the reward drops: at the first "talk to" person of the task, else its giver, else
    // (once the city tutorial is over) the hero.
    Entity* at = nullptr;
    if (drop) {
        for (const TaskSubtask& s : t->subtasks)
            if (!at && s.type == kTalkToPerson) at = EntityBySpawnPoint(s.value);
        if (!at && t->dialogGiver != 0) at = EntityBySpawnPoint(t->dialogGiver);
        if (!at && GameState::secondTutorial == 0x100) at = EntityManager::GetPlayer();
    }
    for (int type = 0; type < 11; ++type) {
        uint32_t amount = t->award[type];
        if (amount == 0) continue;
        if (type != GameState::kCrystal) {
            if (at) BuildingHovers::DropResource(at->worldX, at->worldY, type, amount, false, false);
            else GameState::ChangeResourceAmount(type, (int)amount);
        } else if ((int)amount <= 5) {
            // (the original never pays more than 5 crystals for a task)
            GameState::AddCrystals((int)amount);
        }
    }
    // PORT (docs/balance_plan.md): the offline crystals of a quest.
    if (int crystals = PortQuestCrystals(t)) {
        if (at) BuildingHovers::DropResource(at->worldX, at->worldY, GameState::kCrystal, (unsigned)crystals, false, false);
        else GameState::AddCrystals(crystals);
    }
    for (uint32_t i = 0; i < t->itemCount; ++i) {
        const TaskInfo::ItemCount& it = t->items[i];
        if (!at) {
            // (a profession's point item gives its points instead)
            if (Professions::Profession* p = Professions::GetProfessionForPointItem(it.id))
                Professions::AddProfessionPoint(p->id, it.count);
            else
                GameState::AddItem(it.id, (int)it.count, false, false, false);
            BeltBarWindow::AutoBind(it.id);
        } else {
            for (uint32_t k = 0; k < it.count; ++k)
                BuildingHovers::DropItem(at->worldX, at->worldY, const_cast<Items::ItemInfo*>(Items::GetItemInfo(it.id)), 0, 1);
        }
    }
    if (t->giveItemCount != 0 && takeItems)
        for (uint32_t i = 0; i < t->giveItemCount; ++i) GameState::RemoveItem(t->giveItems[i].id, (int)t->giveItems[i].count);
    SoldierSlots::SetFreeSlotsCount(GameState::GetSoldierSlotCount());
    HUDWindow::UpdateTasks();
    BuildingHovers::Update(0.f, true);
    EnqueueUpdate();
    if (id == 0x5b2) {
        TaskInfoWindow::SetTask(0x5b3, false);
        TaskInfoWindow::Show();
    }
}

namespace {

// CheckOverlimit's monster lists ("mob_id" spawn points): true when all `amount` of them are dead.
// kind: "kill_mob" or "get_item" (the original's messages).
bool AllMobsDead(const TaskInfo* t, const TaskSubtask& s, const char* kind) {
    unsigned found = 0, dead = 0;
    for (unsigned i = 0; i < (unsigned)s.amount; ++i) {
        const MetaData* m = s.mobs->GetChild(i);
        if (m->type != MetaData::kNumber) {
            std::fprintf(stderr, "ERROR: Tasks::CheckOverLimit() Task %d: %s subtask %d mob ID list is invalid\n",
                         t->id, kind, s.id);
            continue;
        }
        Entity* e = EntityBySpawnPoint((uint32_t)m->GetInt());
        if (!e) continue;
        ++found;
        if (e->IsDead()) ++dead;
    }
    if (found != (unsigned)s.amount)
        std::fprintf(stderr,
                     "ERROR: Tasks::CheckOverLimit() Task %d: %s subtask %d failed to find all spawn points on the map\n",
                     t->id, kind, s.id);
    return dead == (unsigned)s.amount;
}

}  // namespace

// @0x236f28 Tasks::CheckOverlimit
void CheckOverlimit(TaskInfo** tasks, unsigned count, bool withMobs) {
    for (unsigned n = 0; n < count; ++n) {
        TaskInfo* t = tasks[n];
        if (t->tutorialType) continue;
        bool any = false;
        for (TaskSubtask& s : t->subtasks) {
            int done = GameState::GetSubTaskDone(s.id);
            if (s.amount <= done) continue;
            bool met = false;      // counted as done, announced to the world
            bool direct = false;   // completed by CompleteSubtaskDirect
            switch (s.type) {
            case kBuildHouse:
                met = s.id == 0xd9 && s.value == 0x3ef && Map::GetBuildingCount(0x3ef, true) != 0;
                break;
            case kIncreaseLand: {
                Map::Patch* p = Map::GetAreaByID(s.value);
                met = p && GameState::GetCurrentMapID() == 0 && p->owned;
                break;
            }
            case kKillMob:
                if (!withMobs || !s.mobs || s.mobs->type != MetaData::kList) break;
                if ((int)s.mobs->GetChildrenCount() != s.amount) {
                    std::fprintf(stderr,
                                 "ERROR: Tasks::CheckOverLimit() Task %d: kill_mob subtask %d tells player to kill %d "
                                 "enemies, but mob ID list contains only %d\n",
                                 t->id, s.id, s.amount, s.mobs->GetChildrenCount());
                    break;
                }
                met = AllMobsDead(t, s, "kill_mob");
                break;
            case kGetItem: {
                if (s.id == 0x4b0 && s.value == 0x15c && GameState::GetItemAmount(0x15c, false) != 0) {
                    direct = true;
                    break;
                }
                if (any) break;
                if (Items::ItemInfo* item = Items::GetItemInfo(s.value)) {
                    if (item->type == 8 && s.amount == 1 && SetInfoWindow::GetPlayerItemOrBetter(s.value)) direct = true;
                    else if (item->type == 1 && (unsigned)s.amount <= (unsigned)GameState::GetItemAmount(s.value, false))
                        direct = true;
                    if (direct) break;
                }
                // Not owned: the monsters that drop it are all dead and none of it lies around.
                if (!withMobs || !s.mobs || s.mobs->type != MetaData::kList) break;
                if (s.assistMap != 0 && GameState::GetCurrentMapID() != s.assistMap) break;
                if ((int)s.mobs->GetChildrenCount() != s.amount) {
                    std::fprintf(stderr,
                                 "ERROR: Tasks::CheckOverLimit() Task %d: get_item subtask %d tells player to get %d "
                                 "items, but mob ID list contains only %d monsters\n",
                                 t->id, s.id, s.amount, s.mobs->GetChildrenCount());
                    break;
                }
                float x = 0.f, y = 0.f;
                if (!BuildingHovers::HasDroppedItem(s.value, x, y)) direct = AllMobsDead(t, s, "get_item");
                break;
            }
            case kInteractBuildingAny:
                // Step 0x515 (value 0x7315c) in the city: no decoration is left to click.
                if (s.id == 0x515 && s.value == 0x7315c && GameState::GetCurrentMapID() == 0)
                    direct = Map::GetDecorationToClick((uint32_t)s.value) == nullptr;
                break;
            case kTrainWorkerInBuilding:
                if (s.value == 0x12 && GameState::GetCurrentMapID() == 0)
                    direct = Setting("goblin_cost").GetChildrenCount() + 1 <= (unsigned)GameState::GetGoblinCount();
                break;
            case kEquip:
                // What the hero wears is reported as equipped.
                for (unsigned i = 0; i < 10; ++i)
                    if (GameState::PlayerItem* it = GameState::GetItemAt(i)) CharacterInfoWindow::CompleteEquipTaskFor(it->id);
                break;
            case kMasteryLevel:
                // value: the mission's map * 10 + the stars it needs
                if (s.value % 10 <= (uint32_t)Locations::GetMissionStarCount(s.value / 10, 0)) direct = true;
                break;
            case kImproveItem:
                // UNVERIFIED (milestone 5): met when the player has an item (type 8, with an item
                // level) improved from `value` (Collections::GetCollectionWithResult, 0x12d7d8).
                break;
            case kIncreaseLandAny:
                if (GameState::GetCurrentMapID() == 0) {
                    GameState::AreaInfo info;
                    met = !GameState::GetAreaInfo(Map::GetNextAreaID(), info);   // nothing left to buy
                }
                break;
            default:
                break;
            }
            if (met) {
                GameState::AddSubTask(s.id, s.amount - done);
                EntityManager::OnSubTaskCompleted(s.id);
                Map::OnSubTaskCompleted(s.id);
                any = true;
            } else if (direct) {
                CompleteSubtaskDirect(s.id, s.amount - done);
                any = true;
            }
        }
        if (!any) continue;
        if (!t->anySubquest) {
            bool all = true;
            for (const TaskSubtask& s : t->subtasks)
                if (GameState::GetSubTaskDone(s.id) < s.amount) all = false;
            if (!all) continue;
        }
        CompleteTask(t->id, true, true, false);
    }
}

// @0x22fd44 Tasks::TaskSubtask::SetAttribute
void TaskSubtask::SetAttribute(const char* name, const char* text) {
    if (!std::strcmp(name, "type")) type = std::atoi(text);
    else if (!std::strcmp(name, "value")) {
        delete values;
        values = nullptr;   // PORT: the original leaves the freed list's pointer when there is no comma
        value = (uint32_t)std::atoi(text);
        if (std::strchr(text, ',')) values = ParseList(text);
    } else if (!std::strcmp(name, "amount")) amount = std::atoi(text);
    else if (!std::strcmp(name, "costCB")) skipPrice = (uint32_t)std::atoi(text);
    else if (!std::strcmp(name, "reqST")) {
        delete required;
        required = ParseList(text);
    } else if (!std::strcmp(name, "x")) assistX = (uint32_t)std::atoi(text);
    else if (!std::strcmp(name, "y")) assistY = (uint32_t)std::atoi(text);
    else if (!std::strcmp(name, "map")) assistMap = (uint32_t)std::atoi(text);
}

// @0x22ffbc Tasks::TaskInfo::SetAttribute
void TaskInfo::SetAttribute(const char* name, const char* text) {
    auto is = [name](const char* n) { return std::strcmp(name, n) == 0; };
    int v = std::atoi(text);
    if (is("level")) level = (uint32_t)v;
    else if (is("task")) LockByTasks(text);
    else if (is("battle")) showInBattle = v != 0;
    else if (is("dialog")) tutorialType = v != 0;
    else if (is("farm")) isFarm = v != 0;
    else if (is("city")) isCity = v != 0;
    else if (is("hint")) isHint = v != 0;
    else if (is("sidequest")) sidequest = v != 0;
    else if (is("duplicate_talk")) isDublicateTalk = v != 0;
    else if (is("chapter")) chapter = v != 0;
    else if (is("emperor")) showOnImperor = v != 0;
    else if (is("no_assist")) dnAssist = v != 0;
    else if (is("no_show")) dnShow = v != 0;
    else if (is("no_button")) showButton = v != 0;
    else if (is("no_reward")) doNotShowReward = v != 0;
    else if (is("complete_any")) anySubquest = v != 0;
    else if (is("repeat_dialog")) repeatTask = v != 0;
    else if (is("no_complete")) noComplete = v != 0;
    else if (is("map")) mapId = (uint32_t)v;
    else if (is("reset_map")) debugMap = (uint32_t)v;
    else if (is("dialog_giver")) dialogGiver = (uint32_t)v;
    else if (is("next")) postQuest = (uint32_t)v;
    else if (is("disabled_by")) disableTask = (uint32_t)v;
    else if (is("repeat_time")) repeatTimeout = (uint32_t)v;
    else if (is("timeout")) timeout = (uint32_t)v;
    else if (is("end_ts")) endTime = (uint32_t)v;
    else if (is("tags")) tag = Tags::GetTag(text);
    else if (is("active")) active = v != 0;
}

// @0x22ff7c Tasks::ChangeSubTaskAttribute
void ChangeSubTaskAttribute(uint32_t task, unsigned step, const char* name, const char* value) {
    TaskInfo* t = GetTask(task);
    if (!t || step > 2) return;
    t->subtasks[step].SetAttribute(name, value);
}

// @0x230524 Tasks::ChangeTaskAttribute
void ChangeTaskAttribute(uint32_t task, const char* name, const char* value) {
    if (TaskInfo* t = GetTask(task)) t->SetAttribute(name, value);
}

// @0x2381b8 Tasks::TaskTimeout
void TaskTimeout(TaskInfo* task) {
    TaskInfoWindow::TaskTimeout(task->id);
    Update(true);
    Map::Save(0);
}

// @0x230a44 Tasks::LaunchDailyTaskFromGroup
bool LaunchDailyTaskFromGroup(const char* const* groups, unsigned groupCount, TaskInfo** current,
                              unsigned currentCount) {
    std::vector<uint32_t> ids;   // 0x6169d0
    for (unsigned g = 0; g < groupCount; ++g) {
        Setting list(groups[g]);
        for (unsigned i = 0; i < list.GetChildrenCount(); ++i) ids.push_back((uint32_t)list.GetChild(i).GetInt());
    }
    if (ids.empty()) return false;
    // A finished or stuck one goes back to "not shown"; while one is running nothing starts.
    for (uint32_t id : ids) {
        TaskInfo* t = GetTask(id);
        const bool started = GameState::IsTaskStarted(id);
        if (t && !(!GameState::TaskCompleted(id) && (!started || t->CanBeCompleted()) &&
                   (!started || IsTaskActive(id, true)))) {
            GameState::ResetTask(id);
            t->dnShow = true;
            if (t->postQuest != 0) GameState::ResetTask(t->postQuest);
        }
        if (IsTaskActive(id, true)) return false;
    }
    TaskInfo* candidates[0x40];
    unsigned n = 0;
    for (uint32_t id : ids) {
        if (n >= 0x40) continue;
        for (unsigned i = 0; i < currentCount; ++i)
            if (current[i]->id == id && current[i]->CanBeCompleted()) {
                candidates[n++] = current[i];
                break;
            }
    }
    if (n == 0) return false;
    TaskInfo* t = candidates[(unsigned long)Rand48::lrand48() % n];
    GameState::StartTask(t->id);
    GameState::SetTaskBeginTime(t->id, (uint32_t)Timer::GetGlobalTime());
    for (TaskSubtask& s : t->subtasks) {
        if (s.amountMin == s.amountMax) continue;
        s.amount = (int)((unsigned long)Rand48::lrand48() % (unsigned)(s.amountMax - s.amountMin)) + s.amountMin;
        GameState::SetCustomSubTaskRequrement(s.id, (uint32_t)s.amount);
    }
    return true;
}

// @0x2377b0 Tasks::Update
void Update(bool force) {
    if (!GameState::IsPlayerCity()) return;
    uint32_t now = (uint32_t)Timer::GetGlobalTime();
    if (now == g_lastUpdate && !force) return;
    g_lastUpdate = now;
    if (!(3 < (unsigned)(GameState::tutorial - 0x3c))) return;   // not during tutorial steps 0x3c..0x3f
    // (event 1 running: task 0x5b1's first step asks for 5 on difficulty 1, else 150)
    if (GameState::GetEventStartTime(1) != 0)
        if (TaskInfo* e = GetTask(0x5b1)) {
            if (GameState::GetEventDifficulty(1) == 1) e->subtasks[0].amount = 5;
            else if (e->subtasks[0].amount == 5) e->subtasks[0].amount = 0x96;
        }
    if (!((!g_blocked && WindowManager::GetShownWindowCount() == 0 && !Map::IsCameraMoving() &&
           !GameState::IsCombatActive()) || force))
        return;
    TaskInfo* cur[0x80] = {};
    unsigned count = GetCurrentTasks(cur, 0x80, false, false);
    bool changed = false;
    for (unsigned i = 0; i < count; ++i) {
        TaskInfo* t = cur[i];
        if (GameState::GetTaskBeginTime(t->id) == 0) GameState::SetTaskBeginTime(t->id, now);
        if (GameState::IsTaskStarted(t->id) && t->dnShow) {
            t->dnShow = false;
            changed = true;
        }
    }
    // A daily task's random amount, kept in the save.
    for (unsigned i = 0; i < count; ++i)
        for (TaskSubtask& s : cur[i]->subtasks)
            if (s.amountMin != s.amountMax && s.amountMin == s.amount)
                if (uint32_t custom = GameState::GetCustomSubTaskRequrement(s.id)) s.amount = (int)custom;
    // Each shown task's open "talk to" step (its required steps done) gives its person the talk task;
    // a post-quest that was already completed starts over.
    for (unsigned i = 0; i < count; ++i) {
        TaskInfo* t = cur[i];
        if (t->dnShow) continue;
        for (const TaskSubtask& s : t->subtasks) {
            if (s.type != kTalkToPerson) continue;
            bool blocked = false;
            if (s.required)
                for (unsigned k = 0; k < s.required->GetChildrenCount() && !blocked; ++k) {
                    const MetaData* r = s.required->GetChild(k);
                    if (r->type == MetaData::kNumber && r->GetInt() != 0)
                        blocked = !GameState::SubTaskCompleted(t->id, (unsigned)r->GetInt());
                }
            if (blocked) continue;
            Entity* e = EntityManager::GetEntityBySpawnPoint(s.value);
            if (e && e->IsActive() && GameState::GetSubTaskDone(s.id) < s.amount) {
                e->AssignTalkTask(t->id);
                if (GameState::TaskCompleted(t->postQuest)) GameState::ResetTask(t->postQuest);
            }
        }
    }
    for (unsigned i = 0; i < count; ++i)
        for (TaskSubtask& s : cur[i]->subtasks) {
            if (GameState::GetSubTaskDone(s.id) == 0x2000) continue;
            int before = GameState::GetSubTaskDone(s.id);
            if (s.type == kBallance) {
                // value 0: resource 0; 1: crystals; n: resource n - 2
                int res = s.value == 0 ? 0 : s.value == 1 ? GameState::kCrystal : (int)s.value - 2;
                GameState::SetSubTaskDone(s.id, (int)GameState::GetResourceAmount(res));
            } else if (s.type == kBallanceItem) {
                GameState::SetSubTaskDone(s.id, GameState::GetItemAmount(s.value, false));
            } else if (s.type == kCompleteCompaign) {
                // the location's completed tasks in percent
                if (Locations::LocationInfo* loc = Locations::GetLocationInfo(s.value)) {
                    float done = (float)(unsigned)Locations::GetLocationTaskCompleted(loc, false);
                    float all = (float)(unsigned)Locations::GetLocationTaskCount(loc, false);
                    // (no tasks: 0 / 0, which the original's float-to-int conversion turns into 0)
                    GameState::SetSubTaskDone(s.id, all == 0.f ? 0 : (int)(0.5f + done / all * 100.f));
                }
            } else if (s.type == kGetReputation) {
                if ((uint32_t)s.amount <= Professions::GetPlayerProfessionLevel(s.value))
                    GameState::SetSubTaskDone(s.id, s.amount);
            } else if (s.type == kBalanceLevel) {
                if ((uint32_t)s.amount <= (uint32_t)GameState::GetLevel()) GameState::SetSubTaskDone(s.id, s.amount);
            }
            if (before != GameState::GetSubTaskDone(s.id)) changed = true;
        }
    if (changed) HUDWindow::UpdateTasks();
    for (unsigned i = 0; i < count; ++i) {
        TaskInfo* t = cur[i];
        bool done;
        if (!t->anySubquest) {
            done = true;
            for (const TaskSubtask& s : t->subtasks)
                if (GameState::GetSubTaskDone(s.id) < s.amount) done = false;
        } else {
            done = false;
            for (const TaskSubtask& s : t->subtasks)
                if (s.amount != 0 && s.amount <= GameState::GetSubTaskDone(s.id)) done = true;
        }
        if (done && !t->tutorialType && !t->isHint) {
            if (t->chapter) {
                // The chapter banner, unless the world map or an edit mode is up.
                if (GlobalMapWindow::IsVisible()) return;
                if (BuildingPlacement::Activated() || BuildingMovement::Activated()) return;
                ChapterWindow::SetParameters(t->id);
                ChapterWindow::Show();
                // (FileManager::SetChapterNow tells Android which content pack to fetch: online.)
                return;
            }
            if (!t->hasReward || t->doNotShowReward) {
                if (!t->noComplete) {
                    CompleteTask(t->id, true, true, false);
                    return;
                }
            } else if (WindowManager::GetEnqueuedWindowCount() == 0 && !FoundItemsWindow::HasPostAction() &&
                       !Map::IsMapLoadQueued()) {
                // UNVERIFIED: not during a PvP fight (GameState::IsPvPCombatActive, milestone 5).
                TaskCompleteWindow::Complete(t->id);
                TaskCompleteWindow::Show();
                return;
            }
            continue;
        }
        if (t->disableTask != 0 && GameState::TaskCompleted(t->disableTask)) {
            if (t->postQuest != 0) CompleteTask(t->postQuest, false, true, false);
            CompleteTask(t->id, false, true, false);
            return;
        }
        // A pop-up to show: said by its giver, else in the hero's own panel.
        if (t->f_1df) {
            if (t->dialogGiver == 0) {
                if (EntityManager::GetPlayer())
                    PlayerTopWindow::SetParameters(t->preQuest, t->id, !t->showButton, t->showButton);
            } else if (Entity* giver = EntityManager::GetEntityBySpawnPoint(t->dialogGiver)) {
                BuildingHovers::ShowWorldDialog(giver, t->preQuest, t->id, false, !t->showButton, false, 0, false, false);
            }
        }
    }
    CheckOverlimit(cur, count, false);
    // A completed repeating task comes back once its timeout has passed, with the tasks under it.
    for (TaskInfo* t : g_repeatTasks) {
        uint32_t end = GameState::GetTaskEndTime(t->id);
        if (end == 0 || !(end + t->repeatTimeout < now) || !GameState::TaskCompleted(t->id)) continue;
        if (t->disableTask != 0 && GameState::TaskCompleted(t->disableTask)) continue;
        GameState::ResetTask(t->id);
        if (t->postQuest != 0) GameState::ResetTask(t->postQuest);
        for (TaskInfo* c : g_repeatChildren)
            if (GetRepeatableParentTask(c) == t) {
                GameState::ResetTask(c->id);
                c->dnShowDefault = c->dnShow;
            }
        HUDWindow::UpdateTasks();
    }
    // Once a day a daily task is launched from each group.
    if (0x15180 < now - GameState::GetEventStartTime(3000)) {
        static const char* const kDaily[3] = {"daily_tasks", "daily_herb", "daily_min"};   // 0x60f074
        static const char* const kDailyPvp[1] = {"daily_pvp"};                              // 0x60f080
        GameState::SetEventStartTime(3000, now);
        bool a = LaunchDailyTaskFromGroup(kDaily, 3, cur, count);
        bool b = LaunchDailyTaskFromGroup(kDailyPvp, 1, cur, count);
        changed = a || b || changed;
    }
    if (changed) HUDWindow::UpdateTasks();
}

// @0x23a968 Tasks::RunLastSubTaskHelp: after travelling for a step, its help runs again on arrival.
void RunLastSubTaskHelp() {
    if (g_lastSubTaskHelpEnabled && g_lastSubTaskHelp != 0) {
        TaskInfo* cur[0x80] = {};
        unsigned n = GetCurrentTasks(cur, 0x80, false, false);
        for (unsigned i = 0; i < n; ++i) {
            int step = -1;
            for (int k = 0; k < 3; ++k)
                if (cur[i]->subtasks[k].id == g_lastSubTaskHelp) step = k;
            if (step < 0) continue;
            BuildingHovers::SetArrowVisibleWindowLimit(2);
            cur[i]->subtasks[step].OnHelp(true);
            break;
        }
    }
    g_lastSubTaskHelp = 0;
    g_lastSubTaskHelpEnabled = false;
}

}  // namespace Tasks
