#pragma once    
#include <PubSubClient.h>
#include "CommandManager.h"
#include <WiFi.h>


class MqttManager
{
public:

    MqttManager(CommandManager& commandManager);

    void begin();

    void update();

    bool publish(
        const char* topic,
        const char* payload
    );

private:
    void connect();
    void reconnect();
    void onMessage(
        char* topic,
        byte* payload,
        unsigned int length
    );

    Command parseCommand(
        const char* payload
    );
    void send_command_to_queue(
        const Command& command
    );

private:

    WiFiClient wifiClient;
    PubSubClient mqtt;
    CommandManager& commandManager;
};