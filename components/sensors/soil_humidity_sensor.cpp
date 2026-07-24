#include "soil_humidity_sensor.h"

SoilHumiditySensor::SoilHumiditySensor(adc_channel_t channel) : channel_(channel) {}

esp_err_t SoilHumiditySensor::init()
{
    adc_oneshot_unit_init_cfg_t unit_cfg = {};
    unit_cfg.unit_id = ADC_UNIT_1;
    esp_err_t err = adc_oneshot_new_unit(&unit_cfg, &adc_handle_);
    if (err != ESP_OK) {
        return err;
    }

    adc_oneshot_chan_cfg_t chan_cfg = {};
    chan_cfg.atten = ADC_ATTEN_DB_12;
    chan_cfg.bitwidth = ADC_BITWIDTH_DEFAULT;
    return adc_oneshot_config_channel(adc_handle_, channel_, &chan_cfg);
}

esp_err_t SoilHumiditySensor::read(float* out_value)
{
    int raw = 0;
    esp_err_t err = adc_oneshot_read(adc_handle_, channel_, &raw);
    if (err != ESP_OK) {
        return err;
    }

    // raw eleve = sol sec, raw faible = sol sature (capteur capacitif type courant)
    float pct = 100.0f * (float)(kRawDry - raw) / (float)(kRawDry - kRawWet);
    if (pct < 0) {
        pct = 0;
    } else if (pct > 100) {
        pct = 100;
    }

    *out_value = pct;
    return ESP_OK;
}
