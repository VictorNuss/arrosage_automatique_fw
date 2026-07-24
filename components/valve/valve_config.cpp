#include "valve_config.h"

// Mapping GPIO / duree max repris du prototype (src/main.cpp d'origine) :
// Pelouse=25/1800s, Potager=26/900s, Serre=27/600s. Le mqtt_key est nouveau
// (impose par le contrat MQTT figé), le nom humain et le pin sont conserves.
const valve_config_t VALVE_CONFIGS[] = {
    {"Pelouse", "vanne_1", GPIO_NUM_25, 1800},
    {"Potager", "vanne_2", GPIO_NUM_26, 900},
    {"Serre", "vanne_3", GPIO_NUM_27, 600},
    // Ajouter une 4e/5e vanne : une ligne ici, ex.
    // {"Verger", "vanne_4", GPIO_NUM_14, 1200},
};

const size_t VALVE_CONFIG_COUNT = sizeof(VALVE_CONFIGS) / sizeof(VALVE_CONFIGS[0]);
