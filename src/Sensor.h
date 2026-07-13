#pragma once

#include <Arduino.h>


class Sensor
{

protected:

    String name;


public:

    Sensor(String name)
    {
        this->name = name;
    }


    virtual void begin() = 0;


    virtual void update() = 0;


    String getName()
    {
        return name;
    }

};