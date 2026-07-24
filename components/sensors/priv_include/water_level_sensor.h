#pragma once

#include "driver/gpio.h"
#include "sensor.h"

// Capteur ultrason JSN-SR04M-2 (trig/echo), repris du prototype existant.
// Mesure la distance capteur -> surface de l'eau, puis la convertit en
// niveau d'eau via la hauteur totale de la cuve (a calibrer, voir
// docs/mqtt_contract.md et Kconfig ARROSAGE_TANK_HEIGHT_CM).
class WaterLevelSensor : public Sensor {
   public:
    WaterLevelSensor(gpio_num_t trig_pin, gpio_num_t echo_pin, float tank_height_cm);

    esp_err_t init() override;
    esp_err_t read(float* out_value) override;
    const char* key() const override { return "water_level_cm"; }

   private:
    gpio_num_t trig_pin_;
    gpio_num_t echo_pin_;
    float tank_height_cm_;
};
