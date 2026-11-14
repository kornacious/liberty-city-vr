#pragma once
#include <stddef.h>
class CEntity;
class CAutomobile;
namespace androidgame {
#if defined(__ANDROID__) || (defined(LIBERTYVR_WINDOWS) || defined(LIBERTYVR_DESKTOP))
void SimulationReplayBeginFrame();
void SimulationReplayEndFrame();
void SimulationReplayInputs(CAutomobile *car);
void SimulationReplayAfterAppFrame();
// Unsmoothed app frame-start interval, attributed to the preceding replay frame.
void SimulationReplayObserveFrame(unsigned long long startNs,double intervalMs);
bool SimulationReplayProtected(CEntity *entity);
bool SimulationReplayIgnoreCollision(CEntity *a,CEntity *b);
bool SimulationReplayIgnoreDamage(CEntity *victim,CEntity *attacker);
// Rows: record, demo, play, stop, playback ghost, limit,
// recording ghost, delete, route assistance, benchmark forward view,
// RenderDoc auto capture, capture time, matched workload, playback count,
// manual current-frame capture, back.
constexpr int SimulationReplayMenuRows=16;
// Nonzero only during the playback preparation/playback belonging to this run.
unsigned long long SimulationReplayForwardViewRun();
bool SimulationReplayForwardViewNeutralPose(float position[3],float *yaw);
void SimulationReplayMenuRow(int row,char *text,size_t capacity);
bool SimulationReplayMenuAction(int row,int direction);
const char *SimulationReplayStatus();
bool SimulationReplayConsumeStartFailure();
bool SimulationReplayRecordingText(char *text,size_t capacity);
bool SimulationReplayPlaybackText(char *text,size_t capacity);
void SimulationReplayMinimumText(char *text,size_t capacity);
// Called only at complete OpenXR frame boundaries, outside GPU command recording.
void SimulationReplayBeginCaptureFrame(void *vkInstance);
void SimulationReplayEndCaptureFrame(bool rendered);
#else
inline void SimulationReplayBeginFrame(){}
inline void SimulationReplayEndFrame(){}
inline void SimulationReplayInputs(CAutomobile *){}
inline void SimulationReplayAfterAppFrame(){}
inline void SimulationReplayObserveFrame(unsigned long long,double){}
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
