#include "mqtt_event.h"

#include <cstring>

namespace {

// Profondeur modeste : les producteurs (vannes, capteurs) sont rate-limites
// par nature (changement d'etat vanne = evenement rare, capteur = throttle
// par sensor_manager) - pas besoin d'un tampon large. mqtt_publish_task vide
// la queue en continu (xQueueReceive bloquant), donc elle ne devrait quasiment
// jamais s'approcher de cette limite hors coupure MQTT prolongee.
constexpr int kQueueLen = 32;

QueueHandle_t s_queue = nullptr;

}  // namespace

void mqtt_event_queue_init(void)
{
    s_queue = xQueueCreate(kQueueLen, sizeof(MqttMessage));
}

QueueHandle_t mqtt_event_queue(void)
{
    return s_queue;
}

bool mqtt_event_push_sensor_reading(const char* key, float value)
{
    MqttMessage msg{};
    msg.type = MqttMessageType::SensorReading;
    std::strncpy(msg.key, key, sizeof(msg.key) - 1);
    msg.numeric_value = value;
    return xQueueSend(s_queue, &msg, 0) == pdTRUE;
}

bool mqtt_event_push_valve_state(const char* key, const char* state)
{
    MqttMessage msg{};
    msg.type = MqttMessageType::ValveState;
    std::strncpy(msg.key, key, sizeof(msg.key) - 1);
    msg.valve_state = state;
    return xQueueSend(s_queue, &msg, 0) == pdTRUE;
}
