#include "ValveManager.h"



ValveManager::ValveManager(
    Valve* valves,
    uint8_t count
)
{
    this->valves = valves;
    this->count = count;
}




void ValveManager::begin()
{

    for(int i=0;i<count;i++)
    {
        valves[i].begin();
    }

}




void ValveManager::update()
{

    for(int i=0;i<count;i++)
    {
        valves[i].update();
    }
}


bool ValveManager::open(
    uint8_t id,
    uint32_t duration
)
{
    if(id >= count)
        return false;
    valves[id].open(duration);
    return true;
}





bool ValveManager::close(
    uint8_t id
)
{
    if(id >= count)
        return false;

    valves[id].close();
    return true;
}

void ValveManager::closeAll()
{
    for(int i=0;i<count;i++)
    {
        valves[i].close();
    }
}

void ValveManager::printStatus()
{

    Serial.println("---- Etat vannes ----");


    for(int i=0;i<count;i++)
    {

        Serial.print(
            valves[i].getName()
        );


        Serial.print(" : ");


        if(valves[i].isOpen())
        {
            Serial.print("OUVERTE ");

            Serial.print(
                valves[i].remainingTime()
            );

            Serial.println(" sec");
        }
        else
        {
            Serial.println("FERMEE");
        }

    }


    Serial.println("--------------------");

}