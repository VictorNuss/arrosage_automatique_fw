#pragma once

/**
 * @file state_json.h
 * @ingroup app_tasks
 * @brief Construction du snapshot JSON complet pour le serveur web de test.
 */

/**
 * @ingroup app_tasks
 * @brief Construit un snapshot JSON complet (horodatage, capteurs, vannes).
 *
 * Utilise uniquement par web_server (`GET /api/state`, page de test locale)
 * pour afficher un etat complet immediat. Ne correspond plus au format du
 * contrat MQTT `arrosage/<device_id>/etat/<key>` (evenementiel, un message
 * par cle - voir docs/mqtt_contract.md) : conserve 0.0 par defaut avant la
 * premiere lecture reussie d'un capteur, la ou le flux MQTT ne publie
 * jamais de valeur bidon.
 *
 * @return Une chaine allouee par cJSON (a liberer avec cJSON_free()), ou
 * nullptr en cas d'echec d'allocation
 */
char* state_json_build(void);
