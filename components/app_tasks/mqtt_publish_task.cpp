#include "mqtt_publish_task.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "mqtt.h"
#include "mqtt_event.h"

namespace {

const char* TAG = "mqtt_publish_task";
constexpr uint32_t kTaskStackSize = 4096;
constexpr UBaseType_t kTaskPriority = 5;

void mqtt_publish_task_run(void* /*arg*/)
{
    QueueHandle_t queue = mqtt_event_queue();
    MqttMessage msg;

    for (;;) {
        if (xQueueReceive(queue, &msg, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        bool published;
        switch (msg.type) {
            case MqttMessageType::SensorReading:
                published = net_mqtt_publish_sensor_reading(msg.key, msg.numeric_value);
                break;
            case MqttMessageType::ValveState:
                published = net_mqtt_publish_valve_state(msg.key, msg.is_open);
                break;
            default:
                published = false;
                break;
        }

        if (!published) {
            ESP_LOGW(TAG, "Publication MQTT impossible pour '%s' (non connecte)", msg.key);
        }
    }
}

}  // namespace

void mqtt_publish_task_start(void)
{
    xTaskCreate(&mqtt_publish_task_run, "mqtt_publish_task", kTaskStackSize, nullptr, kTaskPriority, nullptr);
}
