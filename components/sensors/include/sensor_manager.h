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
 * @brief Lit tous les capteurs et ajoute chaque mesure a l'objet JSON fourni.
 *
 * Chaque mesure est ajoutee sous la cle du contrat MQTT (ex
 * "water_level_cm": 34.5). En cas d'echec de lecture d'un capteur, republie
 * sa derniere valeur connue (0.0 si aucune lecture n'a encore reussi)
 * plutot que d'omettre la cle.
 *
 * @param root Objet cJSON dans lequel ajouter les mesures
 */
void sensor_manager_collect(cJSON* root);
