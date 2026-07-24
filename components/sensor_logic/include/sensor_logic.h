#pragma once

#include <cstdint>

/**
 * @file sensor_logic.h
 * @ingroup sensors
 * @brief Logique pure de throttle d'acquisition des capteurs, sans aucun
 * acces materiel (pas de GPIO, pas d'ADC) : isolee ici pour rester testable
 * sur l'hote (target idf.py "linux", voir test/), independamment du
 * materiel ESP32 - meme principe que components/valve_logic.
 */

/**
 * @ingroup sensors
 * @brief Determine si un capteur doit etre relu.
 *
 * Decouple le rythme d'acquisition materielle d'un capteur du rythme
 * d'appel de sensor_manager_collect() : chaque capteur a son propre
 * intervalle minimal de relecture (voir components/sensors/sensor_manager.cpp),
 * independant des autres et du rythme auquel collect() est invoquee (cycle
 * MQTT periodique, ou requetes plus frequentes du serveur web de test).
 *
 * @param now_us Horodatage courant, en microsecondes (ex esp_timer_get_time())
 * @param last_read_us Horodatage de la derniere lecture reussie ; 0 si jamais lu
 * @param min_interval_ms Intervalle minimal entre deux lectures, en millisecondes ; 0 = toujours du
 * @return true si le capteur doit etre relu maintenant
 */
bool sensor_logic_is_due(int64_t now_us, int64_t last_read_us, uint32_t min_interval_ms);
