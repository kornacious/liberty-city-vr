#pragma once

#include "QuestAmbientCamera.h"
#include "ScriptCommands.h"

// Consume the staged entrance/exit actions without moving or stopping the VR
// player. Do not suspend these threads: they also own the clerk and shop state.
// A negative count dispatches the opcode normally, including shop logic.
inline int QuestShopTransitionParameterCount(const char *name, bool missionFlag,
    int command)
{
    if (!QuestKeepShopPlayerCamera(name, missionFlag))
        return -1;
    switch (command) {
    case COMMAND_SET_PLAYER_COORDINATES:
        return 4;
    case COMMAND_SET_PLAYER_HEADING:
    case COMMAND_SET_PLAYER_CONTROL:
        return 2;
    case COMMAND_CLEAR_AREA:
        return 5;
    case COMMAND_RESTORE_CAMERA_JUMPCUT:
    case COMMAND_SET_CAMERA_IN_FRONT_OF_PLAYER:
    case COMMAND_ENABLE_PLAYER_CONTROL_CAMERA:
        return 0;
    default:
        return -1;
    }
}
