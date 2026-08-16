#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/**
 * @file mqtt.h
 * @ingroup net
 * @brief Client MQTT (esp-mqtt) : publication d'etat et reception de commandes.
 */

/**
 * @ingroup net
 * @brief Initialise le client MQTT et demarre la connexion des que le WiFi obtient une IP.
 *
 * Souscrit a `arrosage/<device_id>/commande` des que la connexion MQTT est
 * etablie ; chaque commande recue et validee (voir command_parse()) est
 * deposee dans command_queue.
 *
 * @param command_queue Queue FreeRTOS dans laquelle deposer les commandes valides
 * @note A appeler une seule fois, apres net_events_init() et net_wifi_init().
 */
void net_mqtt_init(QueueHandle_t command_queue);

/**
 * @ingroup net
 * @brief Publie une lecture capteur sur `arrosage/<device_id>/etat/<key>` (QoS 1, retain=true).
 * @param key Cle du contrat MQTT (ex "water_level_cm")
 * @param value Valeur mesuree
 * @return false sans rien publier si le client n'est pas connecte
 */
bool net_mqtt_publish_sensor_reading(const char* key, float value);

/**
 * @ingroup net
 * @brief Publie l'etat d'une vanne sur `arrosage/<device_id>/etat/<key>` (QoS 1, retain=true).
 * @param key Cle du contrat MQTT (ex "vanne_1")
 * @param is_open Etat courant de la vanne
 * @return false sans rien publier si le client n'est pas connecte
 */
bool net_mqtt_publish_valve_state(const char* key, bool is_open);
