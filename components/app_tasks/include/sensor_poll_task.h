#pragma once

/**
 * @file sensor_poll_task.h
 * @ingroup app_tasks
 * @brief Task periodique de relecture materielle des capteurs.
 */

/**
 * @ingroup app_tasks
 * @brief Demarre la task periodique de relecture des capteurs.
 *
 * Appelle sensor_manager_collect(nullptr) en boucle a un rythme rapide
 * (voir kPollIntervalMs, sensor_poll_task.cpp) : chaque capteur a son propre
 * intervalle minimal de relecture materielle (voir sensor_manager.cpp), donc
 * cette task ne fait que declencher les relectures dues suffisamment souvent
 * pour que le flux MQTT evenementiel (voir mqtt_event.h) reste reactif. Ne
 * construit aucun JSON, ne publie rien elle-meme - sensor_manager_collect()
 * pousse directement chaque lecture reussie sur la queue MQTT.
 *
 * @note A appeler apres sensor_manager_init() et mqtt_event_queue_init().
 */
void sensor_poll_task_start(void);
