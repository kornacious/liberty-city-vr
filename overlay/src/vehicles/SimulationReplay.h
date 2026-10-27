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
// Rows: record, selected demo, play, stop, ghost, auto profile, duration, back.
constexpr int SimulationReplayMenuRows=8;
void SimulationReplayMenuRow(int row,char *text,size_t capacity);
bool SimulationReplayMenuAction(int row,int direction);
const char *SimulationReplayStatus();
#else
inline void SimulationReplayBeginFrame(){}
inline void SimulationReplayEndFrame(){}
inline void SimulationReplayInputs(CAutomobile *){}
inline void SimulationReplayAfterAppFrame(){}
inline bool SimulationReplayProtected(CEntity *){return false;}
inline bool SimulationReplayIgnoreCollision(CEntity *,CEntity *){return false;}
inline bool SimulationReplayIgnoreDamage(CEntity *,CEntity *){return false;}
#endif
struct SimulationReplayFrame {
    SimulationReplayFrame(){SimulationReplayBeginFrame();}
    ~SimulationReplayFrame(){SimulationReplayEndFrame();}
};
}
