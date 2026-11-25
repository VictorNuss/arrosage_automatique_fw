#include "valve_logic.h"

#include <cstring>

uint32_t valve_logic_clamp_duration(uint32_t requested_s, uint32_t max_duration_s)
{
    if (requested_s == 0 || requested_s > max_duration_s) {
        return max_duration_s;
    }
    return requested_s;
}

uint32_t valve_logic_extend_duration(uint32_t remaining_s, uint32_t requested_s, uint32_t max_duration_s)
{
    uint64_t total = (uint64_t)remaining_s + valve_logic_clamp_duration(requested_s, max_duration_s);
    return total > max_duration_s ? max_duration_s : (uint32_t)total;
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

bool valve_queue_push(valve_queue_t* q, int idx)
{
    for (size_t i = 0; i < q->count; i++) {
        if (q->items[i] == idx) {
            return true;
        }
    }
    if (q->count >= VALVE_LOGIC_QUEUE_CAPACITY) {
        return false;
    }
    q->items[q->count++] = idx;
    return true;
}

bool valve_queue_pop(valve_queue_t* q, int* out)
{
    if (q->count == 0) {
        return false;
    }
    *out = q->items[0];
    for (size_t i = 1; i < q->count; i++) {
        q->items[i - 1] = q->items[i];
    }
    q->count--;
    return true;
}

bool valve_queue_remove(valve_queue_t* q, int idx)
{
    for (size_t i = 0; i < q->count; i++) {
        if (q->items[i] == idx) {
            for (size_t j = i + 1; j < q->count; j++) {
                q->items[j - 1] = q->items[j];
            }
            q->count--;
            return true;
        }
    }
    return false;
}
