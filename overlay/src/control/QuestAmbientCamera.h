#pragma once

#include <cstring>

// The retail ambient "camera" thread only stages Belleville Park's bathroom:
// fixed hallway views, fades, control freezes and entrance/exit repositioning.
// Yield before each opcode so both newly named and restored threads stay inert.
// Keep the thread/save layout and mission-owned cameras unchanged.
inline bool QuestSkipAmbientCameraScript(const char *name, bool missionScript,
    bool missionFlag)
{
#ifdef __ANDROID__
    return !missionScript && !missionFlag && std::strncmp(name, "camera", 8) == 0;
#else
    return false;
#endif
}
