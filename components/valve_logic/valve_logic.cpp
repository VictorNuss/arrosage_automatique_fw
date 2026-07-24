#include "valve_logic.h"

#include <cstring>

uint32_t valve_logic_clamp_duration(uint32_t requested_s, uint32_t max_duration_s)
{
    if (requested_s == 0 || requested_s > max_duration_s) {
        return max_duration_s;
    }
    return requested_s;
}

int valve_logic_find_by_key(const char* const* mqtt_keys, size_t count, const char* mqtt_key)
{
    if (mqtt_key == nullptr) {
        return -1;
    }
    for (size_t i = 0; i < count; i++) {
        if (std::strcmp(mqtt_keys[i], mqtt_key) == 0) {
            return (int)i;
        }
    }
    return -1;
}
