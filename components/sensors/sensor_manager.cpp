#include "sensor_manager.h"

#include "esp_log.h"
#include "esp_timer.h"
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

struct sensor_entry_t {
    Sensor* sensor;
    uint32_t min_interval_ms;  // ne pas relire ce capteur plus souvent que ca
};

// Table de config des capteurs : parcourue de facon generique par
// sensor_manager_collect(), aucun cas particulier par capteur (y compris
// battery_v, traite comme un capteur constant).
//
// min_interval_ms decouple le rythme d'acquisition materielle de chaque
// capteur du rythme d'appel de sensor_manager_collect() - celle-ci peut etre
// invoquee bien plus souvent que le cycle de publication MQTT (~60s) par le
// serveur web de test (GET /api/state rafraichi toutes les 3s cote page).
// Le DS18B20 (bit-bang + 750ms de conversion) et l'ultrason n'ont pas besoin
// d'etre relus a ce rythme ; tant que l'intervalle n'est pas ecoule, la
// derniere valeur connue est republiee sans toucher au materiel.
const sensor_entry_t s_sensors[] = {
    {&s_water_level, 30000},   // niveau d'eau : varie lentement (remplissage/vidange)
    {&s_temperature, 60000},   // DS18B20 : lecture couteuse (bit-bang + conversion 750ms), temperature lente
    {&s_soil_humidity, 30000}, // humidite sol : varie lentement
    {&s_battery, 0},           // constante, cout nul : toujours "du"
};
constexpr size_t kSensorCount = sizeof(s_sensors) / sizeof(s_sensors[0]);

float s_last_good[kSensorCount] = {0};
int64_t s_last_read_us[kSensorCount] = {0};

// Protege s_last_good[]/s_last_read_us[] ET l'acces materiel des capteurs
// (bit-bang GPIO ultrason/DS18B20) : sensor_manager_collect() est appelee a
// la fois par sensor_task (cycle periodique) et par le serveur web de test
// (a chaque GET /api/state, depuis la tache HTTP interne d'esp_http_server)
// - sans ce mutex, deux lectures concurrentes pourraient se marcher dessus
// sur les memes broches et corrompre une mesure.
SemaphoreHandle_t s_mutex = nullptr;

}  // namespace

void sensor_manager_init(void)
{
    s_mutex = xSemaphoreCreateMutex();

    for (size_t i = 0; i < kSensorCount; i++) {
        esp_err_t err = s_sensors[i].sensor->init();
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Init capteur '%s' echouee (%s)", s_sensors[i].sensor->key(), esp_err_to_name(err));
        }
    }
}

void sensor_manager_collect(cJSON* root)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    int64_t now_us = esp_timer_get_time();

    for (size_t i = 0; i < kSensorCount; i++) {
        Sensor* sensor = s_sensors[i].sensor;
        bool due = (now_us - s_last_read_us[i]) >= (int64_t)s_sensors[i].min_interval_ms * 1000;

        if (due) {
            float value = s_last_good[i];
            esp_err_t err = sensor->read(&value);
            if (err == ESP_OK) {
                s_last_good[i] = value;
                s_last_read_us[i] = now_us;
            } else {
                ESP_LOGW(TAG, "Lecture capteur '%s' echouee (%s), republication de la derniere valeur connue",
                         sensor->key(), esp_err_to_name(err));
            }
        }

        cJSON_AddNumberToObject(root, sensor->key(), s_last_good[i]);
    }

    xSemaphoreGive(s_mutex);
}
