#include "wifi.h"

#include <cstring>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#include "net_events.h"

namespace {

const char* TAG = "net_wifi";

void wifi_event_handler(void* arg, esp_event_base_t base, int32_t event_id, void* event_data)
{
    if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(net_events_group(), NET_WIFI_CONNECTED_BIT);
        ESP_LOGW(TAG, "WiFi deconnecte, nouvelle tentative de connexion");
        esp_wifi_connect();
    } else if (base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        auto* event = static_cast<ip_event_got_ip_t*>(event_data);
        ESP_LOGI(TAG, "IP obtenue : " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(net_events_group(), NET_WIFI_CONNECTED_BIT);
    }
}

// IP fixe plutot que DHCP : le backend/dashboard (notamment pour l'OTA, voir
// components/ota) a besoin de joindre le device a une adresse previsible.
// Le client DHCP est arrete avant d'etre demarre (le netif vient d'etre
// cree, jamais mis en route) - l'erreur "deja arrete" est benigne et
// ignoree plutot que de faire planter le boot avec ESP_ERROR_CHECK dessus.
void configure_static_ip(esp_netif_t* netif)
{
    esp_netif_dhcpc_stop(netif);

    esp_netif_ip_info_t ip_info = {};
    ESP_ERROR_CHECK(esp_netif_str_to_ip4(CONFIG_ARROSAGE_WIFI_STATIC_IP, &ip_info.ip));
    ESP_ERROR_CHECK(esp_netif_str_to_ip4(CONFIG_ARROSAGE_WIFI_STATIC_NETMASK, &ip_info.netmask));
    ESP_ERROR_CHECK(esp_netif_str_to_ip4(CONFIG_ARROSAGE_WIFI_STATIC_GATEWAY, &ip_info.gw));
    ESP_ERROR_CHECK(esp_netif_set_ip_info(netif, &ip_info));

    esp_netif_dns_info_t dns_info = {};
    dns_info.ip.type = ESP_IPADDR_TYPE_V4;
    ESP_ERROR_CHECK(esp_netif_str_to_ip4(CONFIG_ARROSAGE_WIFI_STATIC_DNS, &dns_info.ip.u_addr.ip4));
    ESP_ERROR_CHECK(esp_netif_set_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns_info));

    ESP_LOGI(TAG, "IP fixe : %s (masque %s, passerelle %s, DNS %s)", CONFIG_ARROSAGE_WIFI_STATIC_IP,
             CONFIG_ARROSAGE_WIFI_STATIC_NETMASK, CONFIG_ARROSAGE_WIFI_STATIC_GATEWAY,
             CONFIG_ARROSAGE_WIFI_STATIC_DNS);
}

}  // namespace

void net_wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t* netif = esp_netif_create_default_wifi_sta();

    configure_static_ip(netif);

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, nullptr));

    wifi_config_t wifi_config = {};
    std::strncpy(reinterpret_cast<char*>(wifi_config.sta.ssid), CONFIG_ARROSAGE_WIFI_SSID,
                 sizeof(wifi_config.sta.ssid) - 1);
    std::strncpy(reinterpret_cast<char*>(wifi_config.sta.password), CONFIG_ARROSAGE_WIFI_PASSWORD,
                 sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connexion WiFi a '%s' en cours...", CONFIG_ARROSAGE_WIFI_SSID);
}
