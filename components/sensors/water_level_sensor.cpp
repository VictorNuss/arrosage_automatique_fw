#include "water_level_sensor.h"

#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"

namespace {
const char* TAG = "water_level";
constexpr int64_t kEchoTimeoutUs = 40000;  // ~40ms -> ~6.8m, large marge pour une cuve domestique
}

WaterLevelSensor::WaterLevelSensor(gpio_num_t trig_pin, gpio_num_t echo_pin) : trig_pin_(trig_pin), echo_pin_(echo_pin) {}

esp_err_t WaterLevelSensor::init()
{
    gpio_reset_pin(trig_pin_);
    gpio_set_direction(trig_pin_, GPIO_MODE_OUTPUT);
    gpio_set_level(trig_pin_, 0);

    gpio_reset_pin(echo_pin_);
    gpio_set_direction(echo_pin_, GPIO_MODE_INPUT);

    return ESP_OK;
}

esp_err_t WaterLevelSensor::read(float* out_value)
{
    // Les logs d'echec distinguent les trois cas, de causes differentes :
    // echo deja haut avant le declenchement (mesure precedente pas finie,
    // broche flottante), jamais monte (declenchement pas vu par le capteur :
    // niveau du trig, cablage) ou reste haut (pas de retour d'onde : montage,
    // angle, parois - ou niveau echo trop bas apres pont diviseur).
    if (gpio_get_level(echo_pin_) == 1) {
        ESP_LOGW(TAG, "echo deja haut avant le declenchement");
        return ESP_ERR_INVALID_STATE;
    }

    gpio_set_level(trig_pin_, 0);
    esp_rom_delay_us(2);
    gpio_set_level(trig_pin_, 1);
    esp_rom_delay_us(20);
    gpio_set_level(trig_pin_, 0);

    int64_t wait_start = esp_timer_get_time();
    while (gpio_get_level(echo_pin_) == 0) {
        if (esp_timer_get_time() - wait_start > kEchoTimeoutUs) {
            ESP_LOGW(TAG, "echo jamais monte apres %lld us (declenchement non vu ?)", (long long)kEchoTimeoutUs);
            return ESP_ERR_TIMEOUT;
        }
    }

    int64_t echo_start = esp_timer_get_time();
    while (gpio_get_level(echo_pin_) == 1) {
        if (esp_timer_get_time() - echo_start > kEchoTimeoutUs) {
            ESP_LOGW(TAG, "echo monte apres %lld us mais reste haut > %lld us (pas de retour d'onde ?)",
                     (long long)(echo_start - wait_start), (long long)kEchoTimeoutUs);
            return ESP_ERR_TIMEOUT;
        }
    }
    int64_t echo_end = esp_timer_get_time();

    // Formule reprise du prototype (JSN-SR04M-2) : distance_cm = duree_us / 58
    float distance_cm = (float)(echo_end - echo_start) / 58.0f;
    ESP_LOGI(TAG, "echo %lld us -> distance %.1f cm", (long long)(echo_end - echo_start), distance_cm);

    *out_value = distance_cm;
    return ESP_OK;
}
