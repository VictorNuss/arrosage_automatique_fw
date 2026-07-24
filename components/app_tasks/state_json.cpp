#include "state_json.h"

#include "cJSON.h"

#include "sensor_manager.h"
#include "time_sync.h"
#include "valve_manager.h"

char* state_json_build(void)
{
    cJSON* root = cJSON_CreateObject();

    char ts[32];
    net_time_sync_now_iso8601(ts, sizeof(ts));
    cJSON_AddStringToObject(root, "ts", ts);

    sensor_manager_collect(root);

    for (size_t i = 0; i < VALVE_COUNT; i++) {
        cJSON_AddStringToObject(root, valve_manager_mqtt_key((int)i), valve_manager_is_open((int)i) ? "open" : "closed");
    }

    char* payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return payload;
}
