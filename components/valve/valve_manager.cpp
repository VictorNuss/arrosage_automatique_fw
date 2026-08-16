#include "valve_manager.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "mqtt_event.h"
#include "valve_config.h"
#include "valve_logic.h"

namespace {

const char* TAG = "valve_manager";

struct valve_runtime_t {
    bool is_open;
    esp_timer_handle_t auto_close_timer;
};

// VALVE_CONFIG_COUNT est extern (definie dans valve_config.cpp) : sa valeur
// n'est pas une constante d'expression dans cette unite de compilation, donc
// le tableau runtime est alloue une fois au demarrage plutot que declare a
// taille fixe.
valve_runtime_t* s_runtime = nullptr;
SemaphoreHandle_t s_mutex = nullptr;

// Cles MQTT extraites de VALVE_CONFIGS, construites une fois a l'init - pour
// reutiliser valve_logic_find_by_key() (logique pure, testee sur l'hote)
// sans avoir a extraire les cles a chaque recherche.
const char** s_mqtt_keys = nullptr;

bool index_valid(int idx)
{
    return idx >= 0 && (size_t)idx < VALVE_CONFIG_COUNT;
}

void auto_close_timer_cb(void* arg)
{
    int idx = (int)(intptr_t)arg;
    if (!index_valid(idx)) {
        return;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    gpio_set_level(VALVE_CONFIGS[idx].pin, 0);
    s_runtime[idx].is_open = false;
    xSemaphoreGive(s_mutex);

    if (!mqtt_event_push_valve_state(VALVE_CONFIGS[idx].mqtt_key, false)) {
        ESP_LOGW(TAG, "Queue MQTT pleine, evenement de fermeture automatique de %s perdu", VALVE_CONFIGS[idx].mqtt_key);
    }

    ESP_LOGI(TAG, "%s (%s) fermee automatiquement (fin de duree)", VALVE_CONFIGS[idx].name,
             VALVE_CONFIGS[idx].mqtt_key);
}

}  // namespace

const size_t VALVE_COUNT = VALVE_CONFIG_COUNT;

void valve_manager_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    s_runtime = new valve_runtime_t[VALVE_CONFIG_COUNT]();
    s_mqtt_keys = new const char*[VALVE_CONFIG_COUNT];

    for (size_t i = 0; i < VALVE_CONFIG_COUNT; i++) {
        const valve_config_t& cfg = VALVE_CONFIGS[i];

        gpio_reset_pin(cfg.pin);
        gpio_set_direction(cfg.pin, GPIO_MODE_OUTPUT);
        gpio_set_level(cfg.pin, 0);  // etat sur par defaut : vanne fermee

        s_runtime[i].is_open = false;
        s_mqtt_keys[i] = cfg.mqtt_key;

        esp_timer_create_args_t timer_args = {};
        timer_args.callback = &auto_close_timer_cb;
        timer_args.arg = (void*)(intptr_t)i;
        timer_args.name = cfg.mqtt_key;
        esp_err_t err = esp_timer_create(&timer_args, &s_runtime[i].auto_close_timer);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Creation du timer d'auto-fermeture pour %s echouee (%s) - "
                          "cette vanne ne pourra pas s'ouvrir en toute securite",
                     cfg.mqtt_key, esp_err_to_name(err));
        }
    }

    ESP_LOGI(TAG, "%u vanne(s) initialisee(s)", (unsigned)VALVE_CONFIG_COUNT);
}

bool valve_manager_open(int idx, uint32_t duration_s)
{
    if (!index_valid(idx)) {
        return false;
    }

    const valve_config_t& cfg = VALVE_CONFIGS[idx];
    uint32_t clamped = valve_logic_clamp_duration(duration_s, cfg.max_duration_s);

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    gpio_set_level(cfg.pin, 1);
    s_runtime[idx].is_open = true;

    esp_timer_stop(s_runtime[idx].auto_close_timer);  // no-op si deja arrete
    esp_err_t err = esp_timer_start_once(s_runtime[idx].auto_close_timer, (uint64_t)clamped * 1000000ULL);

    if (err != ESP_OK) {
        // Sans timer arme, rien ne refermerait cette vanne : on refuse
        // l'ouverture plutot que de la laisser ouverte indefiniment.
        gpio_set_level(cfg.pin, 0);
        s_runtime[idx].is_open = false;
    }

    xSemaphoreGive(s_mutex);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s (%s) : armement du timer d'auto-fermeture echoue (%s), ouverture refusee",
                 cfg.name, cfg.mqtt_key, esp_err_to_name(err));
        return false;
    }

    if (!mqtt_event_push_valve_state(cfg.mqtt_key, true)) {
        ESP_LOGW(TAG, "Queue MQTT pleine, evenement d'ouverture de %s perdu", cfg.mqtt_key);
    }

    ESP_LOGI(TAG, "%s (%s) ouverte pour %u s", cfg.name, cfg.mqtt_key, (unsigned)clamped);
    return true;
}

bool valve_manager_close(int idx)
{
    if (!index_valid(idx)) {
        return false;
    }

    const valve_config_t& cfg = VALVE_CONFIGS[idx];

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    esp_timer_stop(s_runtime[idx].auto_close_timer);  // no-op si deja arrete
    gpio_set_level(cfg.pin, 0);
    s_runtime[idx].is_open = false;

    xSemaphoreGive(s_mutex);

    if (!mqtt_event_push_valve_state(cfg.mqtt_key, false)) {
        ESP_LOGW(TAG, "Queue MQTT pleine, evenement de fermeture de %s perdu", cfg.mqtt_key);
    }

    ESP_LOGI(TAG, "%s (%s) fermee", cfg.name, cfg.mqtt_key);
    return true;
}

void valve_manager_close_all(void)
{
    for (size_t i = 0; i < VALVE_CONFIG_COUNT; i++) {
        valve_manager_close((int)i);
    }
}

void valve_manager_publish_all(void)
{
    for (size_t i = 0; i < VALVE_CONFIG_COUNT; i++) {
        if (!mqtt_event_push_valve_state(VALVE_CONFIGS[i].mqtt_key, valve_manager_is_open((int)i))) {
            ESP_LOGW(TAG, "Queue MQTT pleine, republication de %s (get_status) perdue", VALVE_CONFIGS[i].mqtt_key);
        }
    }
}

int valve_manager_find_by_key(const char* mqtt_key)
{
    return valve_logic_find_by_key(s_mqtt_keys, VALVE_CONFIG_COUNT, mqtt_key);
}

bool valve_manager_is_open(int idx)
{
    if (!index_valid(idx)) {
        return false;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool open = s_runtime[idx].is_open;
    xSemaphoreGive(s_mutex);
    return open;
}

const char* valve_manager_mqtt_key(int idx)
{
    return index_valid(idx) ? VALVE_CONFIGS[idx].mqtt_key : nullptr;
}

const char* valve_manager_name(int idx)
{
    return index_valid(idx) ? VALVE_CONFIGS[idx].name : nullptr;
}
