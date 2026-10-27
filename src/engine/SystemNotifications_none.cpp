#include "engine/SystemNotifications.h"

// PORT: no notification backend on this system yet (the reminder is only logged).
namespace SystemNotifications {
void Init() {}
void Schedule(const std::string&, int) {}
void CancelAll() {}
}  // namespace SystemNotifications
