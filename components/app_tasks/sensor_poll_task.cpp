#include "sensor_poll_task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "sensor_manager.h"

namespace {

constexpr uint32_t kTaskStackSize = 4096;
constexpr UBaseType_t kTaskPriority = 5;
// Rythme d'appel de sensor_manager_collect(), pas rythme de lecture
// materielle reel (throttle par capteur, voir sensor_manager.cpp - le plus
// rapide est a 30s). Suffisamment court pour que chaque capteur soit relu
// (et donc publie) des que son propre intervalle est ecoule, sans attendre.
constexpr uint32_t kPollIntervalMs = 5000;

void sensor_poll_task_run(void* /*arg*/)
{
    for (;;) {
        sensor_manager_collect(nullptr);
        vTaskDelay(pdMS_TO_TICKS(kPollIntervalMs));
    }
}

}  // namespace

void sensor_poll_task_start(void)
{
    xTaskCreate(&sensor_poll_task_run, "sensor_poll_task", kTaskStackSize, nullptr, kTaskPriority, nullptr);
}
