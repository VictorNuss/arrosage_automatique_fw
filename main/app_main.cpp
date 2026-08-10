#include "esp_log.h"
#include "nvs_flash.h"

#include "command_task.h"
#include "mqtt.h"
#include "net_events.h"
#include "ota.h"
#include "sensor_manager.h"
#include "sensor_task.h"
#include "time_sync.h"
#include "valve_manager.h"
#include "web_server.h"
#include "wifi.h"

#if CONFIG_ARROSAGE_ENABLE_DIAG_CONSOLE
#include "diag_console.h"
#endif

namespace {
const char* TAG = "app_main";
}

extern "C" void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    net_events_init();
    valve_manager_init();
    sensor_manager_init();

    net_wifi_init();
    net_time_sync_init();

    QueueHandle_t command_queue = command_task_start();
    net_mqtt_init(command_queue);

    sensor_task_start();

#if CONFIG_ARROSAGE_ENABLE_WEB_SERVER
    web_server_start(command_queue);
#endif

    // Confirme que cette image demarre correctement, annulant tout rollback
    // automatique en attente (voir CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE) -
    // necessaire pour TOUTE image flashee (USB ou OTA), pas seulement apres
    // une mise a jour OTA : sans cet appel, un redemarrage inattendu avant
    // confirmation reviendrait sur l'image precedente.
    ota_confirm_boot_ok();

#if CONFIG_ARROSAGE_ENABLE_DIAG_CONSOLE
    ESP_LOGW(TAG, "Mode diagnostic active (console REPL UART) - voir docs/bring_up_checklist.md");
    diag_console_start();
#else
    ESP_LOGI(TAG, "Firmware arrosage demarre (device_id=%s)", CONFIG_ARROSAGE_DEVICE_ID);
#endif
}
