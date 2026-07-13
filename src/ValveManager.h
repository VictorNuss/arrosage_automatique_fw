#pragma once

#include <Arduino.h>

#include "Valve.h"


class ValveManager
{

private:
    Valve* valves;
    uint8_t count;



public:
    ValveManager(
        Valve* valves,
        uint8_t count
    );

    void begin();

    void update();

    bool open(
        uint8_t id,
        uint32_t duration
    );

    bool close(
        uint8_t id
    );
    void closeAll();
    void printStatus();
};