#include "Valve.h"



Valve::Valve(
    String name,
    uint8_t pin,
    uint32_t maxDuration
)
{
    this->name = name;
    this->pin = pin;
    this->maxDuration = maxDuration;

    state = false;
    stopTime = 0;
}


void Valve::begin()
{
    pinMode(pin, OUTPUT);

    close();
}



void Valve::open(
    uint32_t duration
)
{

    // Sécurité :
    // impossible d'ouvrir trop longtemps

    if(duration > maxDuration)
    {
        duration = maxDuration;
    }

    digitalWrite(
        pin,
        HIGH
    );

    state = true;


    stopTime =
        millis() + (duration * 1000);


    Serial.print("Ouverture ");
    Serial.print(name);

    Serial.print(" pendant ");
    Serial.print(duration);

    Serial.println(" secondes");

}



void Valve::close()
{

    digitalWrite(
        pin,
        LOW
    );

    state = false;
    stopTime = 0;
    Serial.print("Fermeture ");
    Serial.println(name);
}


void Valve::update()
{
    if(state)
    {
        if(millis() >= stopTime)
        {
            close();
        }
    }
}


bool Valve::isOpen()
{
    return state;
}


uint32_t Valve::remainingTime()
{
    if(!state)
        return 0;
    return 
        (stopTime - millis()) / 1000;
}

String Valve::getName()
{
    return name;
}