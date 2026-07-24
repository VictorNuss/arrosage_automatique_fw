#include "time_sync.h"

#include <ctime>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"

#include "net_events.h"

namespace {

const char* TAG = "net_time_sync";

void on_time_synced(struct timeval* /*tv*/)
{
    ESP_LOGI(TAG, "Heure synchronisee via NTP (%s)", CONFIG_ARROSAGE_NTP_SERVER);
    xEventGroupSetBits(net_events_group(), NET_TIME_SYNCED_BIT);
}

void ip_event_handler(void* /*arg*/, esp_event_base_t /*base*/, int32_t /*event_id*/, void* /*event_data*/)
{
    esp_netif_sntp_start();
}

}  // namespace

void net_time_sync_init(void)
{
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(CONFIG_ARROSAGE_NTP_SERVER);
    config.start = false;  // demarre uniquement une fois l'IP obtenue, voir ip_event_handler
    config.sync_cb = &on_time_synced;
    ESP_ERROR_CHECK(esp_netif_sntp_init(&config));

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, nullptr));
}

void net_time_sync_now_iso8601(char* buf, size_t len)
{
    time_t now = time(nullptr);
    struct tm tm_utc;
    gmtime_r(&now, &tm_utc);
    strftime(buf, len, "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
}
