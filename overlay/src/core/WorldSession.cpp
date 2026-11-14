#include "common.h"
#include "WorldSession.h"
#include "Wanted.h"
#include "PlayerPed.h"
#include "World.h"
#include "Timer.h"
CWanted *WorldSession::Wanted(){
    if(auto player=FindPlayerPed())return player->m_pWanted;
    static CWanted neutral;static bool initialized=false;
    if(!initialized){neutral.Initialise();initialized=true;}
    return &neutral;
}
#ifdef LIBERTYVR_DESKTOP
void CTimer::UpdateObservationTime(uint64 timeNs,uint64 stepNs){
    m_snPreviousTimeInMilliseconds=m_snTimeInMilliseconds;
    m_snTimeInMilliseconds=m_snTimeInMillisecondsNonClipped=uint32(timeNs/1000000);
    m_snTimeInMillisecondsPauseMode=m_snTimeInMilliseconds;
    ms_fTimeStep=ms_fTimeStepNonClipped=float(double(stepNs)*50./1e9);
    ++m_FrameCounter;
#ifdef FIX_BUGS
    const uint64 ticks=timeNs/1000000000*30+(timeNs%1000000000)*30/1000000000;
    m_GameFramesPassed=ticks>=m_GameFrameCounter?uint32(ticks-m_GameFrameCounter):0;
    m_GameFrameCounter=ticks;m_GameFrameRemainder=uint32((timeNs/1000000*30)%1000);
    m_LogicalFramesPassed=m_GameFramesPassed;m_LogicalFrameCounter=uint32(ticks);
#endif
}
#endif
