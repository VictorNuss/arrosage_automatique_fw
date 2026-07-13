#pragma once

#include <Arduino.h>


class Valve
{

private:
    String name;
    uint8_t pin;
    bool state;
    unsigned long stopTime;
    uint32_t maxDuration;

public:
    Valve(
        String name,
        uint8_t pin,
        uint32_t maxDuration = 1800
    );
    void begin();
    void open(
        uint32_t duration
    );
    void close();
    void update();
    bool isOpen();
    uint32_t remainingTime();
    String getName();
};