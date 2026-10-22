#pragma once
#include <stdint.h>

// All control calls run on the game thread. Scanning/metadata probing runs on
// one worker and publishes an immutable snapshot; no song data is kept in RAM.
namespace QuestMusic {
void Initialise();
void Terminate();
void Rescan();
void Service();
bool Available();
uint32_t Length();
bool Start(uint32_t position);
void Stop();
int Position();
bool Playing();
void Pause(bool paused); // game/lifecycle pause, independent of user pause
void Skip(int direction);
void TogglePause();
bool UserPaused();
const char *Title();
const char *Status();
}
