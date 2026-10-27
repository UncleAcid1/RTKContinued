// SystemNotifications: the operating system's scheduled notifications, the port's stand-in for the
// Java side of FileManager::PushNotification / OnNotifications (Android's alarm notifications).
// DECIDED (user 2026-10-26): the desktop schedules the game's reminder when the game quits, and the
// next start cancels it. macOS: Apple's UserNotifications framework, which only works inside the
// .app bundle (a plain binary has no bundle id: nothing is scheduled). Other systems: not yet.
#pragma once
#include <string>

namespace SystemNotifications {

// Asks for the permission once (the system remembers the answer) and drops a reminder still pending.
void Init();
// `text` (UTF-8) shown `seconds` from now, replacing a pending one, even if the game has quit.
void Schedule(const std::string& text, int seconds);
// Drops the pending reminder.
void CancelAll();

}  // namespace SystemNotifications
