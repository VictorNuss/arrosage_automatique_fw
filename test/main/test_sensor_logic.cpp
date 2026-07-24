#include "unity.h"

#include "sensor_logic.h"

TEST_CASE("jamais lu (last_read_us=0) est toujours du, quel que soit l'intervalle", "[sensor_logic]")
{
    // Cas du demarrage : now_us peut etre tres proche de 0 (juste apres le
    // boot, esp_timer_get_time() repart de zero) - un capteur avec un long
    // intervalle ne doit pas attendre son propre intervalle avant sa toute
    // premiere lecture.
    TEST_ASSERT_TRUE(sensor_logic_is_due(0, 0, 60000));
    TEST_ASSERT_TRUE(sensor_logic_is_due(1000, 0, 60000));
    TEST_ASSERT_TRUE(sensor_logic_is_due(5 * 1000000LL, 0, 60000));
}

TEST_CASE("intervalle nul est toujours du", "[sensor_logic]")
{
    TEST_ASSERT_TRUE(sensor_logic_is_due(10 * 1000000LL, 9 * 1000000LL, 0));
}

TEST_CASE("pas encore du : intervalle non ecoule", "[sensor_logic]")
{
    // Derniere lecture a t=0, intervalle 30s, on interroge a t=10s
    TEST_ASSERT_FALSE(sensor_logic_is_due(10 * 1000000LL, 0, 30000));
}

TEST_CASE("du : intervalle tout juste ecoule (limite incluse)", "[sensor_logic]")
{
    // Derniere lecture a t=0, intervalle 30s, on interroge exactement a t=30s
    TEST_ASSERT_TRUE(sensor_logic_is_due(30 * 1000000LL, 0, 30000));
}

TEST_CASE("du : intervalle largement depasse", "[sensor_logic]")
{
    TEST_ASSERT_TRUE(sensor_logic_is_due(120 * 1000000LL, 0, 30000));
}

// Reproduit la table de config de components/sensors/sensor_manager.cpp
// (ultrason/humidite sol a 30s, DS18B20 a 60s, batterie a cout nul) pour
// verifier que plusieurs capteurs a des frequences d'acquisition
// differentes evoluent bien independamment les uns des autres au fil du
// temps - sans aucun acces materiel, uniquement des entiers simules.
TEST_CASE("plusieurs capteurs a des frequences d'acquisition differentes sont dus independamment", "[sensor_logic]")
{
    struct sim_sensor_t {
        const char* name;
        uint32_t min_interval_ms;
        int64_t last_read_us;
    };

    sim_sensor_t battery = {"battery_v", 0, 0};        // cout nul : toujours du
    sim_sensor_t soil = {"humidity_pct", 30000, 0};     // 30s
    sim_sensor_t temperature = {"temperature_c", 60000, 0};  // 60s (DS18B20, couteux)

    // t=0 : premier cycle, tout le monde est du (jamais lu)
    int64_t t = 0;
    TEST_ASSERT_TRUE(sensor_logic_is_due(t, battery.last_read_us, battery.min_interval_ms));
    TEST_ASSERT_TRUE(sensor_logic_is_due(t, soil.last_read_us, soil.min_interval_ms));
    TEST_ASSERT_TRUE(sensor_logic_is_due(t, temperature.last_read_us, temperature.min_interval_ms));
    battery.last_read_us = soil.last_read_us = temperature.last_read_us = t;

    // t=10s : seul le capteur a cout nul (battery) est du
    t = 10 * 1000000LL;
    TEST_ASSERT_TRUE(sensor_logic_is_due(t, battery.last_read_us, battery.min_interval_ms));
    TEST_ASSERT_FALSE(sensor_logic_is_due(t, soil.last_read_us, soil.min_interval_ms));
    TEST_ASSERT_FALSE(sensor_logic_is_due(t, temperature.last_read_us, temperature.min_interval_ms));

    // t=35s : "soil" (30s depuis t=0) est maintenant du, "temperature" (60s) pas encore
    t = 35 * 1000000LL;
    TEST_ASSERT_TRUE(sensor_logic_is_due(t, battery.last_read_us, battery.min_interval_ms));
    TEST_ASSERT_TRUE(sensor_logic_is_due(t, soil.last_read_us, soil.min_interval_ms));
    TEST_ASSERT_FALSE(sensor_logic_is_due(t, temperature.last_read_us, temperature.min_interval_ms));
    soil.last_read_us = t;  // simule la relecture effective de "soil" a ce cycle

    // t=65s : "temperature" (60s depuis t=0) est maintenant du ; "soil" (30s depuis t=35s) pas encore
    t = 65 * 1000000LL;
    TEST_ASSERT_TRUE(sensor_logic_is_due(t, temperature.last_read_us, temperature.min_interval_ms));
    TEST_ASSERT_FALSE(sensor_logic_is_due(t, soil.last_read_us, soil.min_interval_ms));
    temperature.last_read_us = t;

    // t=66s : rafraichissement rapide (ex. page web toutes les 3s) juste
    // apres - aucun des deux capteurs a intervalle non nul ne doit etre du
    // de nouveau immediatement, seul "battery" (cout nul) l'est.
    t = 66 * 1000000LL;
    TEST_ASSERT_TRUE(sensor_logic_is_due(t, battery.last_read_us, battery.min_interval_ms));
    TEST_ASSERT_FALSE(sensor_logic_is_due(t, soil.last_read_us, soil.min_interval_ms));
    TEST_ASSERT_FALSE(sensor_logic_is_due(t, temperature.last_read_us, temperature.min_interval_ms));
}
