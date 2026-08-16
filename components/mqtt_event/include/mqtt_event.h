#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/**
 * @file mqtt_event.h
 * @ingroup mqtt_event
 * @brief Queue partagee des evenements a publier sur MQTT (capteurs/vannes).
 *
 * Remplace le modele "snapshot periodique complet" (une valeur bidon par
 * cle en cas d'echec de lecture) par un modele evenementiel : un capteur ou
 * une vanne pousse un MqttMessage des qu'il a une donnee reelle et fraiche a
 * communiquer, jamais avant. Un thread consommateur (voir
 * app_tasks/mqtt_publish_task) vide cette queue et publie chaque message
 * individuellement (voir mqtt.h cote net).
 *
 * Composant volontairement sans dependance reseau : c'est le seul point de
 * couplage entre les composants materiels (producteurs : valve, sensors) et
 * la couche transport (consommateur : app_tasks/net), qui ne se connaissent
 * sinon pas entre eux.
 */

enum class MqttMessageType {
    SensorReading, /**< Lecture capteur reussie */
    ValveState,    /**< Changement (ou rappel explicite) d'etat d'une vanne */
};

struct MqttMessage {
    MqttMessageType type;
    char key[24]; /**< Cle du contrat MQTT (ex "water_level_cm", "vanne_1") */
    union {
        float numeric_value; /**< Valide si type == SensorReading */
        bool is_open;         /**< Valide si type == ValveState */
    };
};

/**
 * @ingroup mqtt_event
 * @brief Cree la queue partagee.
 * @note A appeler une seule fois, avant valve_manager_init(), sensor_manager_init()
 * et avant de demarrer le thread consommateur (mqtt_publish_task_start()).
 */
void mqtt_event_queue_init(void);

/**
 * @ingroup mqtt_event
 * @brief Accesseur vers la queue partagee.
 */
QueueHandle_t mqtt_event_queue(void);

/**
 * @ingroup mqtt_event
 * @brief Pousse un evenement de lecture capteur (non bloquant).
 * @return false si la queue est pleine (evenement perdu ; a loguer par l'appelant)
 */
bool mqtt_event_push_sensor_reading(const char* key, float value);

/**
 * @ingroup mqtt_event
 * @brief Pousse un evenement de changement d'etat vanne (non bloquant).
 * @return false si la queue est pleine (evenement perdu ; a loguer par l'appelant)
 */
bool mqtt_event_push_valve_state(const char* key, bool is_open);
