#pragma once

#include <cstddef>
#include <cstdint>

#include "driver/gpio.h"

struct valve_config_t {
    const char* name;         // nom humain, pour les logs (ex "Pelouse")
    const char* mqtt_key;     // cle du contrat MQTT (ex "vanne_1"), utilisee
                              // pour le topic etat ET le champ "vanne" des
                              // commandes - distincte du nom humain.
    gpio_num_t pin;
    uint32_t max_duration_s;  // clamp de securite sur la duree d'ouverture
};

// Table de configuration statique des vannes. Pour ajouter une vanne :
// ajouter une ligne ici (jusqu'a 5 a terme), aucun autre fichier a modifier.
extern const valve_config_t VALVE_CONFIGS[];
extern const size_t VALVE_CONFIG_COUNT;
