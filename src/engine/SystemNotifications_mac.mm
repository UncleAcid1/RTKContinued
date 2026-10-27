#include "engine/SystemNotifications.h"

#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

namespace SystemNotifications {
namespace {
NSString* const kId = @"rtk_reminder";   // one reminder at a time, as Android's

// UNUserNotificationCenter needs the app bundle (it throws without a bundle id).
bool Available() { return [[NSBundle mainBundle] bundleIdentifier] != nil; }
}  // namespace

void Init() {
    if (!Available()) return;
    UNUserNotificationCenter* center = [UNUserNotificationCenter currentNotificationCenter];
    [center requestAuthorizationWithOptions:(UNAuthorizationOptionAlert | UNAuthorizationOptionSound)
                          completionHandler:^(BOOL, NSError*) {}];
    [center removePendingNotificationRequestsWithIdentifiers:@[ kId ]];
}

void Schedule(const std::string& text, int seconds) {
    if (!Available()) return;
    UNMutableNotificationContent* content = [[UNMutableNotificationContent alloc] init];
    content.title = @"Rule the Kingdom";
    content.body = [NSString stringWithUTF8String:text.c_str()];
    content.sound = [UNNotificationSound defaultSound];
    UNTimeIntervalNotificationTrigger* trigger =
        [UNTimeIntervalNotificationTrigger triggerWithTimeInterval:(seconds < 1 ? 1 : seconds) repeats:NO];
    UNNotificationRequest* request = [UNNotificationRequest requestWithIdentifier:kId content:content trigger:trigger];
    // The game is quitting: wait (up to 2 s) until the system has the request.
    dispatch_semaphore_t done = dispatch_semaphore_create(0);
    [[UNUserNotificationCenter currentNotificationCenter] addNotificationRequest:request
                                                           withCompletionHandler:^(NSError*) {
                                                               dispatch_semaphore_signal(done);
                                                           }];
    dispatch_semaphore_wait(done, dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC));
}

void CancelAll() {
    if (!Available()) return;
    [[UNUserNotificationCenter currentNotificationCenter] removePendingNotificationRequestsWithIdentifiers:@[ kId ]];
}

}  // namespace SystemNotifications
