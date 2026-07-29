#include "valve_config.h"

// Mapping GPIO (configurable via `idf.py menuconfig` -> Broches) / duree max
// (fixe en dur, pas un "port") repris du prototype (src/main.cpp d'origine) :
// Pelouse=1800s, Potager=900s, Serre=600s. Le mqtt_key est nouveau (impose
// par le contrat MQTT figé), le nom humain et les durees sont conservees.
const valve_config_t VALVE_CONFIGS[] = {
    {"Pelouse", "vanne_1", (gpio_num_t)CONFIG_ARROSAGE_VALVE1_GPIO, 1800},
    {"Potager", "vanne_2", (gpio_num_t)CONFIG_ARROSAGE_VALVE2_GPIO, 900},
    {"Serre", "vanne_3", (gpio_num_t)CONFIG_ARROSAGE_VALVE3_GPIO, 600},
    // Ajouter une 4e/5e vanne : une ligne ici (ajouter aussi son GPIO au
    // Kconfig si on veut le rendre configurable), ex.
    // {"Verger", "vanne_4", GPIO_NUM_14, 1200},
};

const size_t VALVE_CONFIG_COUNT = sizeof(VALVE_CONFIGS) / sizeof(VALVE_CONFIGS[0]);
