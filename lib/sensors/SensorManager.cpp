#include "SensorManager.h"



SensorManager::SensorManager(
    Sensor** sensors,
    uint8_t count
)
{
    this->sensors = sensors;
    this->count = count;
}




void SensorManager::begin()
{

    for(
        int i = 0;
        i < count;
        i++
    )
    {
        sensors[i]->begin();
    }

}




void SensorManager::update()
{

    for(
        int i = 0;
        i < count;
        i++
    )
    {
        sensors[i]->update();
    }

}