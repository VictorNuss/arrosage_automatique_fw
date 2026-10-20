#pragma once

#include "driver/gpio.h"
#include "sensor.h"

// Capteur ultrason JSN-SR04M-2 (trig/echo), repris du prototype existant.
// Publie la distance brute capteur -> surface de l'eau : le firmware ne sait
// rien de la cuve, c'est le serveur qui convertit cette distance en niveau
// d'eau (voir docs/mqtt_contract.md).
class WaterLevelSensor : public Sensor {
   public:
    WaterLevelSensor(gpio_num_t trig_pin, gpio_num_t echo_pin);

    esp_err_t init() override;
    esp_err_t read(float* out_value) override;
    const char* key() const override { return "water_distance_cm"; }

   private:
    gpio_num_t trig_pin_;
    gpio_num_t echo_pin_;
};
