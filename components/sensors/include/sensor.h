#pragma once

#include "esp_err.h"

/**
 * @file sensor.h
 * @ingroup sensors
 * @brief Interface commune a tous les capteurs.
 */

/**
 * @ingroup sensors
 * @brief Interface commune a tous les capteurs.
 *
 * Isole le driver materiel (ultrason, ADC, 1-Wire...) du reste de
 * l'application : remplacer un capteur ne touche que son implementation,
 * pas sensor_manager ni sensor_task.
 */
class Sensor {
   public:
    virtual ~Sensor() = default;

    /**
     * @brief Initialise le capteur (GPIO, ADC, bus...).
     * @return ESP_OK en cas de succes
     */
    virtual esp_err_t init() = 0;

    /**
     * @brief Effectue une nouvelle mesure.
     *
     * En cas d'erreur, *out_value n'est pas modifie - l'appelant
     * (sensor_manager) doit republier la derniere valeur connue, car le
     * contrat MQTT exige que la cle soit toujours presente dans le JSON
     * d'etat.
     *
     * @param[out] out_value Valeur mesuree, ecrite uniquement si ESP_OK est retourne
     * @return ESP_OK en cas de succes
     */
    virtual esp_err_t read(float* out_value) = 0;

    /**
     * @brief Cle JSON du contrat d'etat (ex "water_level_cm").
     */
    virtual const char* key() const = 0;
};
