#pragma once

#include <cstddef>
#include <cstdint>

/**
 * @file valve_logic.h
 * @ingroup valve_logic
 * @brief Logique pure du gestionnaire de vannes, sans aucun acces materiel
 * (pas de GPIO, pas de timer) : isolee ici pour rester testable sur l'hote
 * (target idf.py "linux", voir test/), independamment du materiel ESP32.
 */

/**
 * @ingroup valve_logic
 * @brief Clamp de securite sur la duree d'ouverture d'une vanne.
 *
 * Une duree superieure a max_duration_s est ramenee a max_duration_s
 * (impossible d'ouvrir une vanne trop longtemps). Une duree nulle est
 * traitee comme "non specifiee" et ramenee au max egalement (impossible
 * d'ouvrir une vanne indefiniment).
 *
 * @param requested_s Duree demandee, en secondes
 * @param max_duration_s Duree max configuree pour la vanne concernee
 * @return La duree effective a utiliser, toujours dans [1, max_duration_s]
 */
uint32_t valve_logic_clamp_duration(uint32_t requested_s, uint32_t max_duration_s);

/**
 * @ingroup valve_logic
 * @brief Recherche l'index d'une cle MQTT (ex "vanne_1") dans un tableau de cles.
 * @param mqtt_keys Tableau de cles a parcourir
 * @param count Nombre d'entrees dans mqtt_keys
 * @param mqtt_key Cle recherchee
 * @return L'index correspondant, ou -1 si la cle est absente ou si mqtt_key est nullptr
 */
int valve_logic_find_by_key(const char* const* mqtt_keys, size_t count, const char* mqtt_key);
