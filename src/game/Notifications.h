// Notifications: the reminders the game leaves for later when the app is paused ("your HP has
// recovered", "your resources are ready", ...). Port of Notifications (libkingdom.so 5.11,
// @0x1d7e0c..0x1d9540; an App member, App::GetNotifications). On the pause, Game::main_Loop_Func
// purges the list, GameState::UpdateNotifications and Map::UpdateNotifications push one time per
// kind, and FlushNotifications hands the soonest one to the system (FileManager::PushNotification).
//
// DECIDED (user 2026-10-19): these become real desktop notifications; the system call is
// milestone 7 (macOS / Linux / Windows backends), until then SystemNotification below is a stub.
#pragma once
#include <cstdint>
#include <map>
#include <string>

class Notifications {
public:
    enum NotificationTypes {
        kFullHP = 0,             // GameState: the hero's HP is full again
        kContractsReady = 2,     // (nothing in 5.11 pushes 1, 2 or 3)
        kMoneyReady = 4,         // Map: every tax building can be collected
        kResourcesReady = 5,     // Map: every gathering building is full
        kFarmWillRot = 6,        // Map: the first farm patch that rots
        kFarmMapExpiring = 7,    // GameState: the nearest unfinished hunting map
        kBossExpiring = 8,       // GameState: the nearest boss fight under way
        kArenaEvent1 = 9,        // GameState: the arena events' cooldowns
        kArenaEvent2 = 10,
    };

    void PushNotification(NotificationTypes type, uint32_t seconds);   // @0x1d94ec
    void PurgeNotifications();                                         // @0x1d8ca4
    void FlushNotifications();                                         // @0x1d7e9c

private:
    std::map<NotificationTypes, uint32_t> mTimes;   // +0x00 seconds from now, per kind
};

// PORT: App::GetNotifications (the App's member; the port keeps one instance).
Notifications& GetNotifications();

// PORT stub (milestone 7): FileManager::PushNotification(text, seconds) asked Java to show `text`
// as a system notification `seconds` from now. Until the OS backends exist this only logs it and
// keeps it for the test hooks.
void SystemNotification(const std::u32string& text, int seconds);
extern std::u32string g_lastSystemNotification;   // PORT (tests): the last text handed over
extern int g_lastSystemNotificationTime;          // PORT (tests): its delay, -1 for none
