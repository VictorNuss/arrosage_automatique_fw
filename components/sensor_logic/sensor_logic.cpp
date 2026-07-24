#include "sensor_logic.h"

bool sensor_logic_is_due(int64_t now_us, int64_t last_read_us, uint32_t min_interval_ms)
{
    if (last_read_us == 0) {
        // Jamais lu (ex. au demarrage) : toujours du, quel que soit
        // l'intervalle configure - sinon un capteur avec un intervalle long
        // (ex. DS18B20, 60s) pourrait publier sa valeur par defaut (0.0) au
        // premier cycle si moins de min_interval_ms se sont ecoules depuis
        // le boot (t=0 de esp_timer_get_time()).
        return true;
    }
    return (now_us - last_read_us) >= (int64_t)min_interval_ms * 1000;
}
