#include "game/Notifications.h"

#include <cstdio>

#include "game/GameState.h"
#include "game/StringTable.h"
#include "windows/Windows.h"

std::u32string g_lastSystemNotification;
int g_lastSystemNotificationTime = -1;

Notifications& GetNotifications() {
    static Notifications n;
    return n;
}

void SystemNotification(const std::u32string& text, int seconds) {
    g_lastSystemNotification = text;
    g_lastSystemNotificationTime = seconds;
    std::string utf8;   // (for the log only)
    for (char32_t c : text) {
        if (c < 0x80) utf8 += (char)c;
        else if (c < 0x800) { utf8 += (char)(0xc0 | (c >> 6)); utf8 += (char)(0x80 | (c & 0x3f)); }
        else if (c < 0x10000) {
            utf8 += (char)(0xe0 | (c >> 12));
            utf8 += (char)(0x80 | ((c >> 6) & 0x3f));
            utf8 += (char)(0x80 | (c & 0x3f));
        } else {
            utf8 += (char)(0xf0 | (c >> 18));
            utf8 += (char)(0x80 | ((c >> 12) & 0x3f));
            utf8 += (char)(0x80 | ((c >> 6) & 0x3f));
            utf8 += (char)(0x80 | (c & 0x3f));
        }
    }
    std::printf("notification in %d s: %s\n", seconds, utf8.c_str());
}

// @0x1d94ec: the kind's time, replacing one already there.
void Notifications::PushNotification(NotificationTypes type, uint32_t seconds) {
    mTimes[type] = seconds;
}

// @0x1d8ca4
void Notifications::PurgeNotifications() {
    mTimes.clear();
}

// @0x1d7e9c: with notifications on in the settings, the soonest kind (under 999999 s, the first on
// a tie; with none under it, the full HP one at 999999 s) becomes one system notification, its
// text "NOTIF_..." with the hero's name. Most come 600 s after their time; the hunting map and boss
// reminders at it. (The original builds the text in a 0x100-character buffer.)
void Notifications::FlushNotifications() {
    if (SettingsWindow::GetNotificationState() == 0) return;
    if (mTimes.empty()) return;
    uint32_t best = 999999;
    int type = kFullHP;
    for (const auto& [t, seconds] : mTimes) {
        if (seconds < best) {
            type = t;
            best = seconds;
        }
    }
    const char32_t* name = GameState::GetPlayerName();
    std::u32string text;
    int delay = 600;
    switch (type) {
        case kFullHP: text = SWPrintf(0x100, StringTable::GetString("NOTIF_FULL_HP"), {name}); break;
        case kContractsReady:
            text = SWPrintf(0x100, StringTable::GetString("NOTIF_CONTRACTS_READY"), {name});
            break;
        case kMoneyReady: text = SWPrintf(0x100, StringTable::GetString("NOTIF_MONEY_READY"), {name}); break;
        case kResourcesReady:
            text = SWPrintf(0x100, StringTable::GetString("NOTIF_RESOURCES_READY"), {name});
            break;
        case kFarmWillRot: text = SWPrintf(0x100, StringTable::GetString("NOTIF_FARM_WILL_ROT"), {name}); break;
        case kFarmMapExpiring:
            text = SWPrintf(0x100, StringTable::GetString("NOTIF_FARM_MAP_EXPIRING"), {name});
            delay = 0;
            break;
        case kBossExpiring:
            text = SWPrintf(0x100, StringTable::GetString("NOTIF_BOSS_EXPIRING"), {name});
            delay = 0;
            break;
        case kArenaEvent1:
        case kArenaEvent2:   // (sic: the hunting map's text)
            text = SWPrintf(0x100, StringTable::GetString("NOTIF_FARM_MAP_READY"));
            break;
        default:
            // PORT: kinds 1 and 3 (never pushed) hand over the original's unwritten buffer; here
            // an empty text.
            break;
    }
    SystemNotification(text, (int)(best + delay));
}
