#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

/**
 * @file net_events.h
 * @ingroup net
 * @brief Event group partage indiquant l'etat de connexion WiFi/MQTT/NTP.
 *
 * Un seul event group pour tout le firmware plutot qu'un par module
 * (wifi/mqtt/time_sync), consulte par les tasks applicatives qui ont
 * besoin de connaitre l'etat de connexion.
 */

/** @ingroup net @brief Bit positionne lorsque le WiFi a obtenu une IP. */
#define NET_WIFI_CONNECTED_BIT (1 << 0)
/** @ingroup net @brief Bit positionne lorsque le client MQTT est connecte. */
#define NET_MQTT_CONNECTED_BIT (1 << 1)
/** @ingroup net @brief Bit positionne apres la premiere synchronisation NTP reussie. */
#define NET_TIME_SYNCED_BIT (1 << 2)

/**
 * @ingroup net
 * @brief Cree l'event group.
 * @note A appeler une seule fois, avant net_wifi_init(), net_mqtt_init() et net_time_sync_init().
 */
void net_events_init(void);

/**
 * @ingroup net
 * @brief Accesseur vers l'event group partage (bits NET_WIFI_CONNECTED_BIT et similaires).
 */
EventGroupHandle_t net_events_group(void);
