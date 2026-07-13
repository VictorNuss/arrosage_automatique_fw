#pragma once

#include <Arduino.h>

#include "Sensor.h"



class SensorManager
{

private:

    Sensor** sensors;

    uint8_t count;



public:

    SensorManager(
        Sensor** sensors,
        uint8_t count
    );


    void begin();


    void update();

};