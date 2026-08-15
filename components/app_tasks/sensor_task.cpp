#include "sensor_task.h"

#include "cJSON.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "mqtt.h"
#include "net_events.h"
#include "state_json.h"

namespace {

const char* TAG = "sensor_task";
constexpr uint32_t kTaskStackSize = 4096;
constexpr UBaseType_t kTaskPriority = 5;

void publish_state(void)
{
    char* payload = state_json_build();
    if (payload != nullptr) {
        if (!net_mqtt_publish_state(payload)) {
            ESP_LOGW(TAG, "Publication d'etat impossible (MQTT non connecte)");
        }
        cJSON_free(payload);
    }
}

void sensor_task_run(void* /*arg*/)
{
    EventGroupHandle_t events = net_events_group();

    // Attente initiale : inutile de construire un etat sans moyen de le
    // publier. Les cycles suivants ne re-attendent pas indefiniment - voir
    // net_mqtt_publish_state() qui gere les deconnexions transitoires.
    xEventGroupWaitBits(events, NET_WIFI_CONNECTED_BIT | NET_MQTT_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);

    for (;;) {
        if (!(xEventGroupGetBits(events) & NET_TIME_SYNCED_BIT)) {
            // Attente bornee uniquement tant que l'heure n'a jamais ete
            // synchronisee (le bit reste positionne une fois acquis).
            xEventGroupWaitBits(events, NET_TIME_SYNCED_BIT, pdFALSE, pdTRUE, pdMS_TO_TICKS(2000));
        }

        publish_state();

        vTaskDelay(pdMS_TO_TICKS(CONFIG_ARROSAGE_STATE_PUBLISH_INTERVAL_S * 1000));
    }
}

}  // namespace

void sensor_task_start(void)
{
    xTaskCreate(&sensor_task_run, "sensor_task", kTaskStackSize, nullptr, kTaskPriority, nullptr);
}

void sensor_task_publish_now(void)
{
    publish_state();
}
