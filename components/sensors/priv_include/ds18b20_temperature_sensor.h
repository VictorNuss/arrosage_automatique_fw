#pragma once

#include "driver/gpio.h"
#include "sensor.h"

// Sonde etanche DS18B20 (1-Wire), choix materiel par defaut pour
// temperature_c (pas encore fige par l'utilisateur). Driver 1-Wire minimal
// en bit-bang (pas de dependance externe) : un seul capteur sur le bus,
// adressage "Skip ROM".
class Ds18b20TemperatureSensor : public Sensor {
   public:
    explicit Ds18b20TemperatureSensor(gpio_num_t pin);

    esp_err_t init() override;
    esp_err_t read(float* out_value) override;
    const char* key() const override { return "temperature_c"; }

   private:
    gpio_num_t pin_;

    bool onewire_reset();
    void onewire_write_bit(int bit);
    int onewire_read_bit();
    void onewire_write_byte(uint8_t byte);
    uint8_t onewire_read_byte();
};
