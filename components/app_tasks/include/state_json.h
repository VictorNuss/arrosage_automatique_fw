#pragma once

/**
 * @file state_json.h
 * @ingroup app_tasks
 * @brief Construction du JSON d'etat, partagee entre MQTT et le serveur web de test.
 */

/**
 * @ingroup app_tasks
 * @brief Construit le JSON d'etat (meme format que le contrat MQTT `arrosage/<device_id>/etat`).
 *
 * Assemble l'horodatage, les mesures des capteurs et l'etat des vannes.
 * Partagee entre sensor_task (publication MQTT) et web_server (page de
 * test locale), pour garantir que les deux affichent exactement la meme
 * chose.
 *
 * @return Une chaine allouee par cJSON (a liberer avec cJSON_free()), ou
 * nullptr en cas d'echec d'allocation
 */
char* state_json_build(void);
