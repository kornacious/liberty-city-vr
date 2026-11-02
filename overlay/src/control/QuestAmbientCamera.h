#pragma once

#include <cstring>

// The retail ambient "camera" thread only stages Belleville Park's bathroom:
// fixed hallway views, fades, control freezes and entrance/exit repositioning.
// Yield before each opcode so both newly named and restored threads stay inert.
// The ambient thread is launched with m_bIsMissionScript set in retail saves.
// That flag describes its launch mechanism, not ownership by a story mission.
// Keep the thread/save layout and actual mission-flagged scripts unchanged.
inline bool QuestSkipAmbientCameraScript(const char *name, bool missionScript,
    bool missionFlag)
{
    (void)missionScript;
#ifdef __ANDROID__
    return !missionFlag && std::strncmp(name, "camera", 8) == 0;
#else
    return false;
#endif
}
