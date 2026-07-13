#pragma once

#include <Arduino.h>
#include <PubSubClient.h>

#include "ValveManager.h"


class CommandManager
{
private:
    PubSubClient* mqtt;
    ValveManager* valveManager;

public:
    CommandManager(
        PubSubClient* mqtt,
        ValveManager* valveManager
    );

    void begin();
    void update();
    
    void callback(
        char* topic,
        byte* payload,
        unsigned int length
    );
};