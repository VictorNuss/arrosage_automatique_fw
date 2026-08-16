#include "mqtt.h"

#include <cstdio>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "mqtt_client.h"

#include "command.h"
#include "net_events.h"

namespace {

const char* TAG = "net_mqtt";

esp_mqtt_client_handle_t s_client = nullptr;
QueueHandle_t s_command_queue = nullptr;
bool s_started = false;
char s_topic_commande[64];

// Chaque evenement (capteur ou vanne) est publie sur son propre sous-topic
// `arrosage/<device_id>/etat/<key>` plutot que dans un JSON combine - voir
// docs/mqtt_contract.md. Construit a la volee (pas de cache par cle, le
// nombre de cles distinctes est faible et ces appels ne sont pas hot-path).
bool publish_event(const char* key, const char* json_payload)
{
    if (!(xEventGroupGetBits(net_events_group()) & NET_MQTT_CONNECTED_BIT)) {
        return false;
    }
    char topic[80];
    snprintf(topic, sizeof(topic), "arrosage/%s/etat/%s", CONFIG_ARROSAGE_DEVICE_ID, key);
    int msg_id = esp_mqtt_client_publish(s_client, topic, json_payload, 0, /*qos=*/1, /*retain=*/1);
    return msg_id >= 0;
}

void mqtt_event_handler(void* /*handler_args*/, esp_event_base_t /*base*/, int32_t event_id, void* event_data)
{
    auto* event = static_cast<esp_mqtt_event_handle_t>(event_data);

    switch (static_cast<esp_mqtt_event_id_t>(event_id)) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connecte, souscription a %s", s_topic_commande);
            esp_mqtt_client_subscribe(s_client, s_topic_commande, 1);
            xEventGroupSetBits(net_events_group(), NET_MQTT_CONNECTED_BIT);
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT deconnecte");
            xEventGroupClearBits(net_events_group(), NET_MQTT_CONNECTED_BIT);
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "Erreur MQTT (type=%d, errno_transport=%d, code_retour_connexion=%d)",
                     event->error_handle->error_type, event->error_handle->esp_transport_sock_errno,
                     event->error_handle->connect_return_code);
            break;

        case MQTT_EVENT_DATA: {
            Command cmd;
            if (command_parse(event->data, event->data_len, &cmd)) {
                if (xQueueSend(s_command_queue, &cmd, 0) != pdTRUE) {
                    ESP_LOGW(TAG, "File de commandes pleine, commande perdue");
                }
            } else {
                ESP_LOGW(TAG, "Commande MQTT invalide ignoree : %.*s", event->data_len, event->data);
            }
            break;
        }

        default:
            break;
    }
}

void ip_event_handler(void* /*arg*/, esp_event_base_t /*base*/, int32_t /*event_id*/, void* /*event_data*/)
{
    if (!s_started) {
        s_started = true;
        esp_mqtt_client_start(s_client);
    }
}

}  // namespace

void net_mqtt_init(QueueHandle_t command_queue)
{
    s_command_queue = command_queue;
    snprintf(s_topic_commande, sizeof(s_topic_commande), "arrosage/%s/commande", CONFIG_ARROSAGE_DEVICE_ID);

    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.uri = CONFIG_ARROSAGE_MQTT_BROKER_URI;
    mqtt_cfg.credentials.client_id = CONFIG_ARROSAGE_DEVICE_ID;

    s_client = esp_mqtt_client_init(&mqtt_cfg);
    if (s_client == nullptr) {
        ESP_LOGE(TAG, "Initialisation du client MQTT echouee, MQTT sera indisponible");
        return;
    }
    esp_mqtt_client_register_event(s_client, MQTT_EVENT_ANY, &mqtt_event_handler, nullptr);

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, nullptr));
}

bool net_mqtt_publish_sensor_reading(const char* key, float value)
{
    char payload[64];
    snprintf(payload, sizeof(payload), "{\"value\":%.3f}", (double)value);
    return publish_event(key, payload);
}

bool net_mqtt_publish_valve_state(const char* key, bool is_open)
{
    char payload[32];
    snprintf(payload, sizeof(payload), "{\"state\":\"%s\"}", is_open ? "open" : "closed");
    return publish_event(key, payload);
}
