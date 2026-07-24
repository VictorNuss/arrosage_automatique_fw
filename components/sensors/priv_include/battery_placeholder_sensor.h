#pragma once

#include "sensor.h"

// Le device est alimente secteur (pas de vraie batterie). Le contrat MQTT
// exige quand meme la cle "battery_v" a chaque publication : ce "capteur"
// constant permet a sensor_manager de traiter battery_v exactement comme les
// autres mesures, sans cas particulier dans la boucle de collecte.
class BatteryPlaceholderSensor : public Sensor {
   public:
    esp_err_t init() override { return ESP_OK; }

    esp_err_t read(float* out_value) override
    {
        *out_value = 0.0f;
        return ESP_OK;
    }

    const char* key() const override { return "battery_v"; }
};
