#include "CommandManager.h"


void CommandManager::execute(const Command& command)
{
    switch(command.type)
    {
        case CommandType::OpenValve:
            valveManager.open(
                command.valveId,
                command.duration
            );
            break;

        case CommandType::CloseValve:
            valveManager.close(
                command.valveId
            );
            break;

        case CommandType::StopAll:
            valveManager.closeAll();
            break;

        default:
            break;
    }
}