#pragma once

/**
 * @file sensor_task.h
 * @ingroup app_tasks
 * @brief Task periodique de publication de l'etat.
 */

/**
 * @ingroup app_tasks
 * @brief Demarre la task periodique de publication d'etat.
 *
 * Cycle configurable (~60s par defaut, voir Kconfig
 * `ARROSAGE_STATE_PUBLISH_INTERVAL_S`) : lit tous les capteurs et l'etat
 * des vannes, construit le JSON du contrat `arrosage/<device_id>/etat`
 * (voir state_json_build()), publie via net_mqtt_publish_state().
 *
 * @note A appeler apres net_wifi_init(), net_mqtt_init() et net_time_sync_init().
 */
void sensor_task_start(void);
