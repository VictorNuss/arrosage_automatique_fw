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
 * @brief Publie le JSON d'etat sur `arrosage/<device_id>/etat` (QoS 1, retain=true).
 * @param json_payload Payload JSON deja serialise
 * @return false sans rien publier si le client n'est pas connecte
 */
bool net_mqtt_publish_state(const char* json_payload);
