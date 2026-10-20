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
 * @param key Cle du contrat MQTT (ex "water_distance_cm")
 * @param value Valeur mesuree
 * @return false sans rien publier si le client n'est pas connecte
 */
bool net_mqtt_publish_sensor_reading(const char* key, float value);

/**
 * @ingroup net
 * @brief Publie l'etat d'une vanne sur `arrosage/<device_id>/etat/<key>` (QoS 1, retain=true).
 * @param key Cle du contrat MQTT (ex "vanne_1")
 * @param state Etat courant : "open", "closed" ou "transitioning"
 * @return false sans rien publier si le client n'est pas connecte
 */
bool net_mqtt_publish_valve_state(const char* key, const char* state);

/**
 * @ingroup net
 * @brief Publie les informations d'identification du device (QoS 1, retain=true).
 *
 * Deux cles, chacune sur son sous-topic `arrosage/<device_id>/etat/<key>` :
 * - `ip` : IP fixe du device (voir CONFIG_ARROSAGE_WIFI_STATIC_IP, pas de
 *   DHCP). Permet a un backend gerant plusieurs devices de savoir a quelle
 *   adresse envoyer une mise a jour OTA (POST /api/ota, voir components/web)
 *   pour un device_id donne, sans mapping manuel a maintenir de son cote.
 * - `fw_version` : version du firmware en cours d'execution
 *   (esp_app_get_description()->version, soit PROJECT_VER, soit a defaut
 *   `git describe` au moment du build). Permet au backend d'afficher quelle
 *   version tourne avant/apres une mise a jour OTA.
 *
 * Appelee automatiquement a chaque connexion MQTT et en reponse a
 * `get_status` (voir CommandType::GetStatus) - jamais besoin de l'appeler
 * manuellement en dehors de ces deux points.
 *
 * @return false si au moins une des deux cles n'a pas pu etre publiee
 *         (client non connecte)
 */
bool net_mqtt_publish_device_info(void);
