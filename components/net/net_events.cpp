#include "net_events.h"

namespace {
EventGroupHandle_t s_events = nullptr;
}

void net_events_init(void)
{
    s_events = xEventGroupCreate();
}

EventGroupHandle_t net_events_group(void)
{
    return s_events;
}
