#pragma once

#include <Arduino.h>
#include "Sensor.h"


class WaterLevelSensor : public Sensor
{

private:

    uint8_t trigPin;
    uint8_t echoPin;

    float distance;

    unsigned long lastMeasure;

    uint32_t interval;


public:

    WaterLevelSensor(
        String name,
        uint8_t trigPin,
        uint8_t echoPin,
        uint32_t interval = 1000
    );


    void begin() override;

    void update() override;


    float getDistance();

};