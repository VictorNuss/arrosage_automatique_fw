#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/**
 * @file command_task.h
 * @ingroup app_tasks
 * @brief Task consommant les commandes et les appliquant au gestionnaire de vannes.
 */

/**
 * @ingroup app_tasks
 * @brief Cree la queue de commandes et demarre la task qui les applique au gestionnaire de vannes.
 *
 * Le handle retourne doit etre transmis a net_mqtt_init() et a
 * web_server_start() : c'est la seule queue du firmware, remplie par le
 * handler MQTT_EVENT_DATA et par le serveur web, consommee ici.
 *
 * @return Le handle de la queue a transmettre aux producteurs de commandes
 */
QueueHandle_t command_task_start(void);
