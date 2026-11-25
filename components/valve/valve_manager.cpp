#include "valve_manager.h"

#include <cstdlib>

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
    Transitioning,  // ouverture ou fermeture en cours OU en attente (voir transitioning_to_open, queued)
};

struct valve_runtime_t {
    ValveRuntimeState state;
    bool transitioning_to_open;           // direction ; valide seulement si state == Transitioning
    bool queued;                          // transition en attente de son tour : le GPIO n'a pas encore bascule
    uint32_t pending_open_s;              // duree d'ouverture appliquee au demarrage effectif de l'ouverture
    esp_timer_handle_t auto_close_timer;  // fin de la duree demandee -> demande la fermeture
    esp_timer_handle_t transition_timer;  // fin de la transition physique (condensateur) -> confirme l'etat
};

// VALVE_CONFIG_COUNT est extern (definie dans valve_config.cpp) : sa valeur
// n'est pas une constante d'expression dans cette unite de compilation, donc
// le tableau runtime est alloue une fois au demarrage plutot que declare a
// taille fixe.
valve_runtime_t* s_runtime = nullptr;
SemaphoreHandle_t s_mutex = nullptr;

// Une seule vanne en mouvement a la fois : le demarrage du moteur (via son
// condensateur) provoque un pic de courant que l'alimentation ne supporte
// pas pour plusieurs vannes simultanement. s_active_idx est la vanne qui
// detient le "creneau de transition" (-1 si aucune) ; les autres transitions
// demandees attendent dans s_queue (FIFO) et demarrent l'une apres l'autre,
// chacune a la fin de la precedente. Plusieurs vannes peuvent en revanche
// rester ouvertes en meme temps : seul le mouvement est serialise.
// Invariant : s_queue non vide => s_active_idx != -1. Protege par s_mutex.
int s_active_idx = -1;
valve_queue_t s_queue = {};

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

void push_state(int idx, const char* state)
{
    if (!mqtt_event_push_valve_state(VALVE_CONFIGS[idx].mqtt_key, state)) {
        ESP_LOGW(TAG, "Queue MQTT pleine, evenement '%s' de %s perdu", state, VALVE_CONFIGS[idx].mqtt_key);
    }
}

// --- Fonctions "_locked" : a appeler avec s_mutex pris ----------------------

// Passe la vanne en "transitioning" vers to_open et lui attribue le creneau
// de transition s'il est libre (ou si elle le detient deja : inversion de
// sens d'un moteur deja en mouvement, aucun autre moteur ne demarre). Sinon
// la met en file d'attente. Retourne true si l'appelant doit lancer
// start_transition(idx) tout de suite (hors mutex).
bool request_transition_locked(int idx, bool to_open)
{
    valve_runtime_t& rt = s_runtime[idx];
    rt.state = ValveRuntimeState::Transitioning;
    rt.transitioning_to_open = to_open;

    if (s_active_idx == -1 || s_active_idx == idx) {
        s_active_idx = idx;
        rt.queued = false;
        return true;
    }
    if (!rt.queued) {
        valve_queue_push(&s_queue, idx);  // ne peut pas echouer : capacite >= VALVE_COUNT (verifie a l'init)
        rt.queued = true;
    }
    return false;
}

// Annule une transition encore en attente : le GPIO n'a jamais bascule, la
// vanne revient donc simplement a son etat physique d'origine.
void cancel_queued_locked(int idx)
{
    valve_runtime_t& rt = s_runtime[idx];
    valve_queue_remove(&s_queue, idx);
    rt.queued = false;
    rt.state = rt.transitioning_to_open ? ValveRuntimeState::Closed : ValveRuntimeState::Open;
}

// Libere le creneau de transition detenu par idx et l'attribue a la
// prochaine vanne en attente. Retourne cette vanne (a demarrer par
// l'appelant, hors mutex), ou -1.
int release_slot_locked(int idx)
{
    if (s_active_idx != idx) {
        return -1;
    }
    s_active_idx = -1;
    int next = -1;
    if (valve_queue_pop(&s_queue, &next)) {
        s_active_idx = next;
        s_runtime[next].queued = false;
    }
    return next;
}

// --- Fonctions a appeler SANS s_mutex ---------------------------------------

// Confirme l'etat cible d'une transition terminee et libere le creneau.
// Retourne la prochaine vanne a demarrer, ou -1.
int finish_transition(int idx)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool to_open = s_runtime[idx].transitioning_to_open;
    s_runtime[idx].state = to_open ? ValveRuntimeState::Open : ValveRuntimeState::Closed;
    int next = release_slot_locked(idx);
    xSemaphoreGive(s_mutex);

    push_state(idx, to_open ? "open" : "closed");
    ESP_LOGI(TAG, "%s (%s) transition terminee : %s", VALVE_CONFIGS[idx].name, VALVE_CONFIGS[idx].mqtt_key,
             to_open ? "ouverte" : "fermee");
    return next;
}

// Demarre physiquement la transition de idx, qui detient le creneau : bascule
// le GPIO, et ne confirmera l'etat cible ("open"/"closed") qu'apres
// CONFIG_ARROSAGE_VALVE_TRANSITION_DELAY_S - le temps que le condensateur de
// demarrage de la vanne motorisee permette au moteur d'actionner reellement
// le passage d'eau, dans un sens comme dans l'autre. Si ce demarrage echoue,
// le creneau passe a la vanne suivante (boucle plutot que recursion).
// Retourne false si la transition de idx elle-meme n'a pas pu demarrer.
bool start_transition(int idx)
{
    bool first = true;
    bool first_ok = true;

    while (idx >= 0) {
        const valve_config_t& cfg = VALVE_CONFIGS[idx];
        valve_runtime_t& rt = s_runtime[idx];

        xSemaphoreTake(s_mutex, portMAX_DELAY);
        bool to_open = rt.transitioning_to_open;
        uint32_t open_s = rt.pending_open_s;
        xSemaphoreGive(s_mutex);

        int next = -1;
        bool ok = true;

        if (to_open) {
            // Arme le timer de duree AVANT de toucher au GPIO : sans lui, rien
            // ne refermerait cette vanne - on refuse l'ouverture plutot que de
            // la laisser ouverte sans filet. La duree court a partir de
            // l'ouverture effective : l'attente en file n'est pas decomptee.
            esp_timer_stop(rt.auto_close_timer);  // no-op si deja arrete
            esp_err_t err = esp_timer_start_once(rt.auto_close_timer, (uint64_t)open_s * 1000000ULL);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "%s (%s) : armement du timer d'auto-fermeture echoue (%s), ouverture refusee",
                         cfg.name, cfg.mqtt_key, esp_err_to_name(err));
                xSemaphoreTake(s_mutex, portMAX_DELAY);
                rt.state = ValveRuntimeState::Closed;
                next = release_slot_locked(idx);
                xSemaphoreGive(s_mutex);
                push_state(idx, "closed");
                ok = false;
            }
        }

        if (ok) {
            esp_timer_stop(rt.transition_timer);  // no-op si deja arrete ; annule une transition en cours

            xSemaphoreTake(s_mutex, portMAX_DELAY);
            gpio_set_level(cfg.pin, rt.transitioning_to_open ? 1 : 0);
            xSemaphoreGive(s_mutex);

            esp_err_t err = esp_timer_start_once(rt.transition_timer,
                                                  (uint64_t)CONFIG_ARROSAGE_VALVE_TRANSITION_DELAY_S * 1000000ULL);
            if (err != ESP_OK) {
                // Sans timer de transition, mieux vaut confirmer immediatement
                // que de rester bloque en "transitioning" (et de bloquer la file).
                ESP_LOGW(TAG, "%s (%s) : armement du timer de transition echoue (%s), confirmation immediate",
                         cfg.name, cfg.mqtt_key, esp_err_to_name(err));
                next = finish_transition(idx);
            } else if (to_open) {
                ESP_LOGI(TAG, "%s (%s) : ouverture lancee pour %u s", cfg.name, cfg.mqtt_key, (unsigned)open_s);
            } else {
                ESP_LOGI(TAG, "%s (%s) : fermeture lancee", cfg.name, cfg.mqtt_key);
            }
        }

        if (first) {
            first_ok = ok;
            first = false;
        }
        idx = next;
    }

    return first_ok;
}

void transition_timer_cb(void* arg)
{
    int idx = (int)(intptr_t)arg;
    if (!index_valid(idx)) {
        return;
    }
    start_transition(finish_transition(idx));
}

// Temps restant (arrondi a la seconde superieure) avant la fermeture
// automatique, 0 si le timer n'est pas arme. esp_timer_get_expiry_time()
// renvoie la derniere echeance meme pour un timer arrete, d'ou le test
// esp_timer_is_active() prealable.
uint32_t auto_close_remaining_s(int idx)
{
    esp_timer_handle_t timer = s_runtime[idx].auto_close_timer;
    uint64_t expiry_us = 0;
    if (!esp_timer_is_active(timer) || esp_timer_get_expiry_time(timer, &expiry_us) != ESP_OK) {
        return 0;
    }
    int64_t now_us = esp_timer_get_time();
    if ((int64_t)expiry_us <= now_us) {
        return 0;
    }
    return (uint32_t)(((int64_t)expiry_us - now_us + 999999) / 1000000);
}

void auto_close_timer_cb(void* arg)
{
    int idx = (int)(intptr_t)arg;
    if (!index_valid(idx)) {
        return;
    }

    ESP_LOGI(TAG, "%s (%s) : fin de duree, demande de fermeture", VALVE_CONFIGS[idx].name,
             VALVE_CONFIGS[idx].mqtt_key);
    valve_manager_close(idx);
}

}  // namespace

const size_t VALVE_COUNT = VALVE_CONFIG_COUNT;

void valve_manager_init(void)
{
    if (VALVE_CONFIG_COUNT > VALVE_LOGIC_QUEUE_CAPACITY) {
        ESP_LOGE(TAG, "%u vannes configurees mais file de transitions limitee a %d : augmenter "
                      "VALVE_LOGIC_QUEUE_CAPACITY (valve_logic.h)",
                 (unsigned)VALVE_CONFIG_COUNT, VALVE_LOGIC_QUEUE_CAPACITY);
        abort();
    }

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
    valve_runtime_t& rt = s_runtime[idx];
    bool extend = false;
    bool start_now = false;
    bool publish_transitioning = false;
    bool publish_open = false;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool transitioning = rt.state == ValveRuntimeState::Transitioning;

    if (rt.state == ValveRuntimeState::Open || (transitioning && rt.transitioning_to_open && !rt.queued)) {
        // Deja ouverte, ou moteur en train d'ouvrir : rien ne doit bouger
        // physiquement - la commande ajoute seulement du temps ("extra-time")
        // au lieu de rejouer la transition.
        extend = true;
    } else if (transitioning && rt.transitioning_to_open) {
        // Ouverture deja en attente : la duree s'ajoute a celle prevue.
        rt.pending_open_s = valve_logic_extend_duration(rt.pending_open_s, duration_s, cfg.max_duration_s);
        ESP_LOGI(TAG, "%s (%s) : ouverture en attente, duree portee a %u s", cfg.name, cfg.mqtt_key,
                 (unsigned)rt.pending_open_s);
    } else if (transitioning && rt.queued) {
        // Fermeture en attente : la vanne n'a pas encore bouge, elle est
        // toujours ouverte - on annule la fermeture et on re-arme la duree.
        cancel_queued_locked(idx);
        extend = true;
        publish_open = true;
    } else {
        // Fermee, ou moteur en train de fermer : (re)ouverture.
        rt.pending_open_s = valve_logic_clamp_duration(duration_s, cfg.max_duration_s);
        start_now = request_transition_locked(idx, true);
        publish_transitioning = true;
        if (!start_now) {
            ESP_LOGI(TAG, "%s (%s) : ouverture (%u s) en attente - une autre vanne est en mouvement", cfg.name,
                     cfg.mqtt_key, (unsigned)rt.pending_open_s);
        }
    }
    xSemaphoreGive(s_mutex);

    if (publish_open) {
        push_state(idx, "open");
    }
    if (publish_transitioning) {
        push_state(idx, "transitioning");
    }
    if (start_now) {
        return start_transition(idx);
    }
    if (!extend) {
        return true;
    }

    uint32_t effective_s = valve_logic_extend_duration(auto_close_remaining_s(idx), duration_s, cfg.max_duration_s);
    esp_timer_stop(rt.auto_close_timer);  // no-op si deja arrete
    esp_err_t err = esp_timer_start_once(rt.auto_close_timer, (uint64_t)effective_s * 1000000ULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s (%s) : re-armement du timer d'auto-fermeture echoue (%s), fermeture", cfg.name,
                 cfg.mqtt_key, esp_err_to_name(err));
        // Le timer precedent vient d'etre arrete : ne pas laisser une vanne
        // ouverte sans fermeture programmee.
        valve_manager_close(idx);
        return false;
    }

    ESP_LOGI(TAG, "%s (%s) : deja ouverte, prolongee - fermeture dans %u s", cfg.name, cfg.mqtt_key,
             (unsigned)effective_s);
    return true;
}

bool valve_manager_close(int idx)
{
    if (!index_valid(idx)) {
        return false;
    }

    const valve_config_t& cfg = VALVE_CONFIGS[idx];
    valve_runtime_t& rt = s_runtime[idx];

    esp_timer_stop(rt.auto_close_timer);  // no-op si deja arrete ; fermeture explicite, plus besoin

    bool start_now = false;
    const char* publish = nullptr;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool transitioning = rt.state == ValveRuntimeState::Transitioning;

    if (rt.state == ValveRuntimeState::Closed || (transitioning && !rt.transitioning_to_open)) {
        // Deja fermee, ou fermeture deja lancee/en attente : rien a rejouer.
        ESP_LOGI(TAG, "%s (%s) : deja fermee ou en cours de fermeture", cfg.name, cfg.mqtt_key);
    } else if (transitioning && rt.queued) {
        // Ouverture encore en attente : la vanne n'a jamais bouge, on annule.
        cancel_queued_locked(idx);
        publish = "closed";
        ESP_LOGI(TAG, "%s (%s) : ouverture en attente annulee", cfg.name, cfg.mqtt_key);
    } else {
        // Ouverte, ou moteur en train d'ouvrir.
        start_now = request_transition_locked(idx, false);
        publish = "transitioning";
        if (!start_now) {
            ESP_LOGI(TAG, "%s (%s) : fermeture en attente - une autre vanne est en mouvement", cfg.name,
                     cfg.mqtt_key);
        }
    }
    xSemaphoreGive(s_mutex);

    if (publish != nullptr) {
        push_state(idx, publish);
    }
    if (start_now) {
        start_transition(idx);
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
