#pragma once
#include <stddef.h>
class CEntity;
class CAutomobile;
namespace androidgame {
#ifdef __ANDROID__
void SimulationReplayBeginFrame();
void SimulationReplayEndFrame();
void SimulationReplayInputs(CAutomobile *car);
void SimulationReplayAfterAppFrame();
bool SimulationReplayProtected(CEntity *entity);
bool SimulationReplayIgnoreCollision(CEntity *a,CEntity *b);
bool SimulationReplayIgnoreDamage(CEntity *victim,CEntity *attacker);
// Rows: record, demo, play, stop, playback ghost, auto profile, limit,
// recording ghost, delete, route assistance, benchmark forward view, back.
constexpr int SimulationReplayMenuRows=12;
// Nonzero only during the playback preparation/playback belonging to this run.
unsigned long long SimulationReplayForwardViewRun();
bool SimulationReplayForwardViewNeutralPose(float position[3],float *yaw);
void SimulationReplayMenuRow(int row,char *text,size_t capacity);
bool SimulationReplayMenuAction(int row,int direction);
const char *SimulationReplayStatus();
bool SimulationReplayConsumeStartFailure();
bool SimulationReplayRecordingText(char *text,size_t capacity);
#else
inline void SimulationReplayBeginFrame(){}
inline void SimulationReplayEndFrame(){}
inline void SimulationReplayInputs(CAutomobile *){}
inline void SimulationReplayAfterAppFrame(){}
inline unsigned long long SimulationReplayForwardViewRun(){return 0;}
inline bool SimulationReplayForwardViewNeutralPose(float[3],float *){return false;}
inline bool SimulationReplayProtected(CEntity *){return false;}
inline bool SimulationReplayIgnoreCollision(CEntity *,CEntity *){return false;}
inline bool SimulationReplayIgnoreDamage(CEntity *,CEntity *){return false;}
#endif
struct SimulationReplayFrame {
    SimulationReplayFrame(){SimulationReplayBeginFrame();}
    ~SimulationReplayFrame(){SimulationReplayEndFrame();}
};
}
