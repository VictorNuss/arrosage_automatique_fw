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

enum class ValveRuntimeState {
    Closed,
    Open,
    Transitioning,  // ouverture ou fermeture en cours (voir transitioning_to_open)
};

struct valve_runtime_t {
    ValveRuntimeState state;
    bool transitioning_to_open;          // direction en cours ; valide seulement si state == Transitioning
    esp_timer_handle_t auto_close_timer;  // fin de la duree demandee -> lance la fermeture
    esp_timer_handle_t transition_timer;  // fin de la transition physique (condensateur) -> confirme l'etat
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

const char* state_string(ValveRuntimeState state)
{
    switch (state) {
        case ValveRuntimeState::Open:
            return "open";
        case ValveRuntimeState::Transitioning:
            return "transitioning";
        case ValveRuntimeState::Closed:
        default:
            return "closed";
    }
}

// Lance une transition (ouverture si to_open, fermeture sinon) : bascule le
// GPIO immediatement, mais ne confirme l'etat cible ("open"/"closed") sur
// MQTT qu'apres CONFIG_ARROSAGE_VALVE_TRANSITION_DELAY_S - le temps que le
// condensateur de demarrage de la vanne motorisee permette au moteur
// d'actionner reellement le passage d'eau, dans un sens comme dans l'autre.
// Generique : utilisee aussi bien par valve_manager_open()/close() que par
// auto_close_timer_cb() (fin de duree demandee), pas de logique dupliquee.
void begin_transition(int idx, bool to_open)
{
    const valve_config_t& cfg = VALVE_CONFIGS[idx];

    esp_timer_stop(s_runtime[idx].transition_timer);  // no-op si deja arrete ; annule une transition en cours

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    gpio_set_level(cfg.pin, to_open ? 1 : 0);
    s_runtime[idx].state = ValveRuntimeState::Transitioning;
    s_runtime[idx].transitioning_to_open = to_open;
    xSemaphoreGive(s_mutex);

    esp_err_t err = esp_timer_start_once(s_runtime[idx].transition_timer,
                                          (uint64_t)CONFIG_ARROSAGE_VALVE_TRANSITION_DELAY_S * 1000000ULL);
    if (err != ESP_OK) {
        // Sans timer de transition, mieux vaut confirmer immediatement que
        // de rester bloque en "transitioning" indefiniment.
        ESP_LOGW(TAG, "%s (%s) : armement du timer de transition echoue (%s), confirmation immediate",
                 cfg.name, cfg.mqtt_key, esp_err_to_name(err));
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        s_runtime[idx].state = to_open ? ValveRuntimeState::Open : ValveRuntimeState::Closed;
        xSemaphoreGive(s_mutex);
        if (!mqtt_event_push_valve_state(cfg.mqtt_key, to_open ? "open" : "closed")) {
            ESP_LOGW(TAG, "Queue MQTT pleine, evenement de %s perdu", cfg.mqtt_key);
        }
        return;
    }

    if (!mqtt_event_push_valve_state(cfg.mqtt_key, "transitioning")) {
        ESP_LOGW(TAG, "Queue MQTT pleine, evenement de transition de %s perdu", cfg.mqtt_key);
    }
}

void transition_timer_cb(void* arg)
{
    int idx = (int)(intptr_t)arg;
    if (!index_valid(idx)) {
        return;
    }

    bool to_open;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    to_open = s_runtime[idx].transitioning_to_open;
    s_runtime[idx].state = to_open ? ValveRuntimeState::Open : ValveRuntimeState::Closed;
    xSemaphoreGive(s_mutex);

    if (!mqtt_event_push_valve_state(VALVE_CONFIGS[idx].mqtt_key, to_open ? "open" : "closed")) {
        ESP_LOGW(TAG, "Queue MQTT pleine, confirmation d'etat de %s perdue", VALVE_CONFIGS[idx].mqtt_key);
    }

    ESP_LOGI(TAG, "%s (%s) transition terminee : %s", VALVE_CONFIGS[idx].name, VALVE_CONFIGS[idx].mqtt_key,
             to_open ? "ouverte" : "fermee");
}

void auto_close_timer_cb(void* arg)
{
    int idx = (int)(intptr_t)arg;
    if (!index_valid(idx)) {
        return;
    }

    ESP_LOGI(TAG, "%s (%s) : fin de duree, lancement de la fermeture", VALVE_CONFIGS[idx].name,
             VALVE_CONFIGS[idx].mqtt_key);
    begin_transition(idx, false);
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

        s_runtime[i].state = ValveRuntimeState::Closed;
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

        esp_timer_create_args_t transition_timer_args = {};
        transition_timer_args.callback = &transition_timer_cb;
        transition_timer_args.arg = (void*)(intptr_t)i;
        transition_timer_args.name = cfg.mqtt_key;
        err = esp_timer_create(&transition_timer_args, &s_runtime[i].transition_timer);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Creation du timer de transition pour %s echouee (%s) - "
                          "les changements d'etat seront confirmes immediatement",
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

    // Arme le timer de duree AVANT de toucher au GPIO : sans lui, rien ne
    // refermerait cette vanne - on refuse l'ouverture plutot que de la
    // laisser ouverte sans filet.
    esp_timer_stop(s_runtime[idx].auto_close_timer);  // no-op si deja arrete
    esp_err_t err = esp_timer_start_once(s_runtime[idx].auto_close_timer, (uint64_t)clamped * 1000000ULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s (%s) : armement du timer d'auto-fermeture echoue (%s), ouverture refusee",
                 cfg.name, cfg.mqtt_key, esp_err_to_name(err));
        return false;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool already_open = (s_runtime[idx].state == ValveRuntimeState::Open);
    xSemaphoreGive(s_mutex);

    if (already_open) {
        // Deja pleinement ouverte : le GPIO ne change pas, rien ne bouge
        // physiquement - seule la duree (deja prolongee ci-dessus) change.
        // Ne pas rejouer la transition, sinon on republierait "transitioning"
        // pendant 15s pour une vanne qui n'a en realite jamais bouge.
        ESP_LOGI(TAG, "%s (%s) : deja ouverte, duree prolongee a %u s", cfg.name, cfg.mqtt_key, (unsigned)clamped);
    } else {
        begin_transition(idx, true);
        ESP_LOGI(TAG, "%s (%s) : ouverture lancee pour %u s", cfg.name, cfg.mqtt_key, (unsigned)clamped);
    }

    return true;
}

bool valve_manager_close(int idx)
{
    if (!index_valid(idx)) {
        return false;
    }

    const valve_config_t& cfg = VALVE_CONFIGS[idx];

    esp_timer_stop(s_runtime[idx].auto_close_timer);  // no-op si deja arrete ; fermeture explicite, plus besoin

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool already_closed = (s_runtime[idx].state == ValveRuntimeState::Closed);
    xSemaphoreGive(s_mutex);

    if (already_closed) {
        // Meme logique qu'a l'ouverture : rien a rejouer physiquement.
        ESP_LOGI(TAG, "%s (%s) : deja fermee", cfg.name, cfg.mqtt_key);
    } else {
        begin_transition(idx, false);
        ESP_LOGI(TAG, "%s (%s) : fermeture lancee", cfg.name, cfg.mqtt_key);
    }

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
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        const char* state = state_string(s_runtime[i].state);
        xSemaphoreGive(s_mutex);

        if (!mqtt_event_push_valve_state(VALVE_CONFIGS[i].mqtt_key, state)) {
            ESP_LOGW(TAG, "Queue MQTT pleine, republication de %s (get_status) perdue", VALVE_CONFIGS[i].mqtt_key);
        }
    }
}

int valve_manager_find_by_key(const char* mqtt_key)
{
    return valve_logic_find_by_key(s_mqtt_keys, VALVE_CONFIG_COUNT, mqtt_key);
}

const char* valve_manager_state_string(int idx)
{
    if (!index_valid(idx)) {
        return nullptr;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    const char* state = state_string(s_runtime[idx].state);
    xSemaphoreGive(s_mutex);
    return state;
}

const char* valve_manager_mqtt_key(int idx)
{
    return index_valid(idx) ? VALVE_CONFIGS[idx].mqtt_key : nullptr;
}

const char* valve_manager_name(int idx)
{
    return index_valid(idx) ? VALVE_CONFIGS[idx].name : nullptr;
}
