#include "sensor_manager.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "battery_placeholder_sensor.h"
#include "ds18b20_temperature_sensor.h"
#include "sensor.h"
#include "soil_humidity_sensor.h"
#include "water_level_sensor.h"

namespace {

const char* TAG = "sensor_manager";

// GPIO reserves aux capteurs (distincts des pins vannes 25/26/27, voir
// components/valve/valve_config.cpp) :
//  - ultrason JSN-SR04M-2 (repris du prototype) : trig=32, echo=33
//  - DS18B20 (1-Wire)                            : GPIO 4
//  - capacitif sol (ADC1)                         : GPIO 34 / ADC_CHANNEL_6
WaterLevelSensor s_water_level(GPIO_NUM_32, GPIO_NUM_33, (float)CONFIG_ARROSAGE_TANK_HEIGHT_CM);
Ds18b20TemperatureSensor s_temperature(GPIO_NUM_4);
SoilHumiditySensor s_soil_humidity(ADC_CHANNEL_6);
BatteryPlaceholderSensor s_battery;

// Table de config des capteurs : parcourue de facon generique par
// sensor_manager_collect(), aucun cas particulier par capteur (y compris
// battery_v, traite comme un capteur constant).
Sensor* const s_sensors[] = {
    &s_water_level,
    &s_temperature,
    &s_soil_humidity,
    &s_battery,
};
constexpr size_t kSensorCount = sizeof(s_sensors) / sizeof(s_sensors[0]);

float s_last_good[kSensorCount] = {0};

// Protege s_last_good[] ET l'acces materiel des capteurs (bit-bang GPIO
// ultrason/DS18B20) : sensor_manager_collect() est appelee a la fois par
// sensor_task (cycle periodique) et par le serveur web de test (a chaque
// GET /api/state, depuis la tache HTTP interne d'esp_http_server) - sans ce
// mutex, deux lectures concurrentes pourraient se marcher dessus sur les
// memes broches et corrompre une mesure.
SemaphoreHandle_t s_mutex = nullptr;

}  // namespace

void sensor_manager_init(void)
{
    s_mutex = xSemaphoreCreateMutex();

    for (size_t i = 0; i < kSensorCount; i++) {
        esp_err_t err = s_sensors[i]->init();
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Init capteur '%s' echouee (%s)", s_sensors[i]->key(), esp_err_to_name(err));
        }
    }
}

void sensor_manager_collect(cJSON* root)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    for (size_t i = 0; i < kSensorCount; i++) {
        float value = s_last_good[i];
        esp_err_t err = s_sensors[i]->read(&value);
        if (err == ESP_OK) {
            s_last_good[i] = value;
        } else {
            ESP_LOGW(TAG, "Lecture capteur '%s' echouee (%s), republication de la derniere valeur connue",
                     s_sensors[i]->key(), esp_err_to_name(err));
        }
        cJSON_AddNumberToObject(root, s_sensors[i]->key(), s_last_good[i]);
    }

    xSemaphoreGive(s_mutex);
}
