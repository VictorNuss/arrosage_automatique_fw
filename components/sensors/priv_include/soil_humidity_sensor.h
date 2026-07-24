#pragma once

#include "esp_adc/adc_oneshot.h"
#include "sensor.h"

// Capteur d'humidite du sol capacitif, lu sur une entree ADC. Choix materiel
// par defaut (pas encore fige par l'utilisateur) : facilement remplacable
// sans toucher au reste de l'application grace a l'interface Sensor.
class SoilHumiditySensor : public Sensor {
   public:
    explicit SoilHumiditySensor(adc_channel_t channel);

    esp_err_t init() override;
    esp_err_t read(float* out_value) override;
    const char* key() const override { return "humidity_pct"; }

   private:
    adc_channel_t channel_;
    adc_oneshot_unit_handle_t adc_handle_ = nullptr;

    // Calibration lineaire dry/wet a ajuster sur le terrain via le mode
    // diagnostic (commande `sensor read`) : raw_dry = lecture a l'air libre
    // (sol sec), raw_wet = lecture capteur plonge dans l'eau (sol sature).
    static constexpr int kRawDry = 3000;
    static constexpr int kRawWet = 1200;
};
