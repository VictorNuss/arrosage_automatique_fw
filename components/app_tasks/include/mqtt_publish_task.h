#pragma once

/**
 * @file mqtt_publish_task.h
 * @ingroup app_tasks
 * @brief Task consommatrice de la queue MQTT evenementielle.
 */

/**
 * @ingroup app_tasks
 * @brief Demarre la task qui vide la queue MQTT evenementielle en continu.
 *
 * Bloque sur la queue (voir mqtt_event.h) et publie chaque message des son
 * arrivee (esp_mqtt_client_publish() est non bloquant, la latence perçue est
 * donc negligeable) - pas de cycle periodique, purement reactif. Si le
 * client MQTT n'est pas connecte au moment de la publication, le message est
 * perdu (logue en warning) plutot que reessaye : les producteurs (vannes,
 * capteurs) republient de toute facon a leur prochain evenement, et
 * CommandType::GetStatus permet de forcer une republication complete.
 *
 * @note A appeler apres mqtt_event_queue_init() et net_mqtt_init().
 */
void mqtt_publish_task_start(void);
