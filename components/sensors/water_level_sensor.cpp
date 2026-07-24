#include "water_level_sensor.h"

#include "esp_rom_sys.h"
#include "esp_timer.h"

namespace {
constexpr int64_t kEchoTimeoutUs = 40000;  // ~40ms -> ~6.8m, large marge pour une cuve domestique
}

WaterLevelSensor::WaterLevelSensor(gpio_num_t trig_pin, gpio_num_t echo_pin, float tank_height_cm)
    : trig_pin_(trig_pin), echo_pin_(echo_pin), tank_height_cm_(tank_height_cm) {}

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
    gpio_set_level(trig_pin_, 0);
    esp_rom_delay_us(2);
    gpio_set_level(trig_pin_, 1);
    esp_rom_delay_us(20);
    gpio_set_level(trig_pin_, 0);

    int64_t wait_start = esp_timer_get_time();
    while (gpio_get_level(echo_pin_) == 0) {
        if (esp_timer_get_time() - wait_start > kEchoTimeoutUs) {
            return ESP_ERR_TIMEOUT;
        }
    }

    int64_t echo_start = esp_timer_get_time();
    while (gpio_get_level(echo_pin_) == 1) {
        if (esp_timer_get_time() - echo_start > kEchoTimeoutUs) {
            return ESP_ERR_TIMEOUT;
        }
    }
    int64_t echo_end = esp_timer_get_time();

    // Formule reprise du prototype (JSN-SR04M-2) : distance_cm = duree_us / 58
    float distance_cm = (float)(echo_end - echo_start) / 58.0f;

    float water_level_cm = tank_height_cm_ - distance_cm;
    if (water_level_cm < 0) {
        water_level_cm = 0;
    } else if (water_level_cm > tank_height_cm_) {
        water_level_cm = tank_height_cm_;
    }

    *out_value = water_level_cm;
    return ESP_OK;
}
