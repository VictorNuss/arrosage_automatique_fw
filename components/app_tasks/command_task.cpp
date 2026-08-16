#include "command_task.h"

#include "esp_log.h"

#include "command.h"
#include "sensor_manager.h"
#include "valve_manager.h"

namespace {

const char* TAG = "command_task";
constexpr int kCommandQueueLen = 10;
constexpr uint32_t kTaskStackSize = 4096;
constexpr UBaseType_t kTaskPriority = 10;  // priorite haute : securite (fermeture de vanne reactive)

void command_task_run(void* arg)
{
    auto queue = static_cast<QueueHandle_t>(arg);
    Command cmd;

    for (;;) {
        if (xQueueReceive(queue, &cmd, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        switch (cmd.type) {
            case CommandType::OpenValve: {
                int idx = valve_manager_find_by_key(cmd.valve_key);
                if (idx >= 0) {
                    valve_manager_open(idx, cmd.duration_s);
                } else {
                    ESP_LOGW(TAG, "Commande open: vanne inconnue '%s'", cmd.valve_key);
                }
                break;
            }
            case CommandType::CloseValve: {
                int idx = valve_manager_find_by_key(cmd.valve_key);
                if (idx >= 0) {
                    valve_manager_close(idx);
                } else {
                    ESP_LOGW(TAG, "Commande close: vanne inconnue '%s'", cmd.valve_key);
                }
                break;
            }
            case CommandType::StopAll:
                ESP_LOGW(TAG, "Arret d'urgence : fermeture de toutes les vannes");
                valve_manager_close_all();
                break;
            case CommandType::GetStatus:
                valve_manager_publish_all();
                sensor_manager_publish_last_known();
                break;
            case CommandType::Invalid:
            default:
                break;
        }
    }
}

}  // namespace

QueueHandle_t command_task_start(void)
{
    QueueHandle_t queue = xQueueCreate(kCommandQueueLen, sizeof(Command));
    xTaskCreate(&command_task_run, "command_task", kTaskStackSize, queue, kTaskPriority, nullptr);
    return queue;
}
