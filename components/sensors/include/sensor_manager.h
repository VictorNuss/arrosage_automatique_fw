#pragma once

#include "cJSON.h"

/**
 * @file sensor_manager.h
 * @ingroup sensors
 * @brief Table de config des capteurs et collecte groupee pour le JSON d'etat.
 *
 * Thread-safe : sensor_manager_collect() est protegee par un mutex interne
 * (appelee a la fois par sensor_task et par le serveur web de test).
 */

/**
 * @ingroup sensors
 * @brief Construit les instances de capteurs et appelle leur init().
 * @note A appeler une seule fois au demarrage.
 */
void sensor_manager_init(void);

/**
 * @ingroup sensors
 * @brief Relit les capteurs dus et, en option, ajoute chaque mesure a l'objet JSON fourni.
 *
 * Deux effets independants a chaque appel :
 * - Cote materiel : chaque capteur dont l'intervalle minimal de relecture
 *   (voir la table de config interne, sensor_manager.cpp) est ecoule est
 *   relu. Toute lecture reussie est aussitot poussee sur la queue MQTT
 *   evenementielle (voir mqtt_event.h) - jamais en cas d'echec, et jamais de
 *   valeur bidon avant la premiere lecture reussie (voir
 *   sensor_manager_publish_last_known() pour la republication a la demande).
 * - Cote `root` (optionnel, peut etre `nullptr` pour ne declencher que la
 *   relecture materielle ci-dessus sans construire de JSON) : ajoute
 *   systematiquement chaque cle du contrat (ex "water_level_cm": 34.5),
 *   y compris 0.0 par defaut avant la premiere lecture reussie - reserve a
 *   la page web de test (voir state_json.cpp), qui a besoin d'un snapshot
 *   complet immediat plutot que d'un flux d'evenements.
 *
 * Chaque capteur ayant son propre intervalle minimal, cette fonction peut
 * etre appelee bien plus souvent que le rythme d'acquisition materielle reel
 * d'un capteur donne (ex. par le serveur web de test, rafraichi toutes les
 * 3s) sans le solliciter inutilement.
 *
 * @param root Objet cJSON dans lequel ajouter les mesures, ou `nullptr`
 */
void sensor_manager_collect(cJSON* root);

/**
 * @ingroup sensors
 * @brief Republie la derniere valeur connue de chaque capteur deja lu avec succes au moins une fois.
 *
 * A appeler en reponse a une commande `get_status` (voir CommandType::GetStatus).
 * Ne declenche aucune relecture materielle (republication immediate depuis
 * le cache) et ne republie jamais un capteur jamais lu avec succes - la
 * cle correspondante est alors simplement absente de la reponse plutot que
 * de contenir une valeur bidon.
 */
void sensor_manager_publish_last_known(void);
