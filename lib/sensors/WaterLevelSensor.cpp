#include "WaterLevelSensor.h"



WaterLevelSensor::WaterLevelSensor(
    String name,
    uint8_t trigPin,
    uint8_t echoPin,
    uint32_t interval
)
: Sensor(name)
{
    this->trigPin = trigPin;
    this->echoPin = echoPin;

    this->interval = interval;

    distance = 0;
    lastMeasure = 0;
}



void WaterLevelSensor::begin()
{

    pinMode(
        trigPin,
        OUTPUT
    );


    pinMode(
        echoPin,
        INPUT
    );


    digitalWrite(
        trigPin,
        LOW
    );

}



void WaterLevelSensor::update()
{

    digitalWrite(
        trigPin,
        LOW
    );

    delayMicroseconds(2);


    digitalWrite(
        trigPin,
        HIGH
    );

    delayMicroseconds(20);


    digitalWrite(
        trigPin,
        LOW
    );


    unsigned long duration =
        pulseIn(
            echoPin,
            HIGH,
            40000
        );


    Serial.print("Echo : ");
    Serial.println(duration);



    if(duration > 0)
    {
        distance =
            duration / 58.0;


        Serial.print(name);
        Serial.print(" : ");
        Serial.print(distance);
        Serial.println(" cm");
    }
    else
    {
        Serial.println("Pas d'echo");
    }


    delay(1000);
}

float WaterLevelSensor::getDistance()
{
    return distance;
}