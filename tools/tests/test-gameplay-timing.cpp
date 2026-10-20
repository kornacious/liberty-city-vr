#include "ragdoll-test-vector.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <vector>
#undef _WIN32
#define __ANDROID__
#define FIX_BUGS
using uint32=uint32_t;using uint64=uint64_t;using int32=int32_t;using int8=int8_t;
template<class T>T Min(T a,T b){return a<b?a:b;}
static float Abs(float v){return std::fabs(v);}
static float Clamp(float v,float a,float b){return v<a?a:v>b?b:v;}
static double clockMs;
static double RsTimer(){return clockMs;}
#define debug(...) ((void)0)
static void OutputDebugString(const char *){}
static struct Audio{void ResetTimers(uint32){}} DMAudio;
struct CRecordDataForGame{static bool IsPlayingBack(){return false;}};
struct CRecordDataForChase{static bool IsRecording(){return false;}};
#pragma warning(push)
#pragma warning(disable:4244 4245 4267 4100 4127 4996)
#include "gameplay-timing-timer-header.inc"
#include "gameplay-timing-timer.inc"
#include "gameplay-timing-helper.inc"
struct CMatrix{CVector operator*(const CVector &v)const{return v;}};
static CVector Multiply3x3(const CVector &v,const CMatrix &){return v;}
class CVehicle{public:CVector speed;CMatrix matrix;CVector GetSpeed(const CVector &){return speed;}CMatrix &GetMatrix(){return matrix;}};
#include "gameplay-timing-door-header.inc"
#include "gameplay-timing-door.inc"
#define nil nullptr
enum {MI_AIRTRAIN,MI_DEADDODO,EXPLOSION_HELI,PARTICLE_HELI_DEBRIS,CESNA_STATUS_DESTROYED=7,FADE_OUT,FADE_IN};
struct CRGBA{int r,g,b,a;CRGBA(int r_=0,int g_=0,int b_=0,int a_=0):r(r_),g(g_),b(b_),a(a_){}};
struct CGeneral{static int GetRandomNumber(){return 1;}static int GetRandomNumberInRange(int a,int){return a;}static float GetRandomNumberInRange(float a,float){return a;}};
static std::vector<uint32> explosions;
static int debris,fades;
struct CExplosion{static void AddExplosion(void*,void*,int,CVector,int){explosions.push_back(CTimer::GetTimeInMilliseconds());}};
struct CParticle{template<class... T>static void AddParticle(T...){++debris;}};
static void *FindPlayerPed(){return nil;}
struct Camera{void SetFadeColour(int,int,int){}void Fade(float,int){++fades;}void ProcessFade(){}} TheCamera;
struct CPlane;
static CPlane *pDrugRunCesna,*pDropOffCesna;
static int CesnaMissionStatus,DropOffCesnaMissionStatus;
struct CPlane{
    uint32 m_nFrameWhenHit=0,m_nDestructionFrame=0;
    bool m_bHasBeenHit=true,m_bIsDrugRunCesna=false,m_bIsDropOffCesna=false,bRenderScorched=false,destroyed=false;
    int model=MI_DEADDODO;CMatrix matrix;
    int GetModelIndex(){return model;}CMatrix &GetMatrix(){return matrix;}
    void FlagToDestroyWhenNextProcessed(){destroyed=true;}
    void ProcessDestruction();
};
#include "gameplay-timing-plane.inc"
#pragma warning(pop)
static unsigned checks;
static void Check(bool okay,const char *message){++checks;if(!okay){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
static void Tick(double ms){clockMs+=ms;CTimer::Update();}
static void Reset(float scale=1){CTimer::Initialise();CTimer::SetTimeScale(scale);}
static void Schedules(int fps,float scale,bool jitter){
    Reset(scale);uint32 calls=0,phases[8]={},visits[320]={};
    for(int f=0;f<fps*10;++f){
        Tick(1000.0/fps*(jitter?(f%2?1.15:0.85):1.0));
        Check(QuestGameTime::Count()<=2,"clipped frames bound gameplay catch-up");
        for(unsigned phase=0;phase<8;++phase){if(QuestGameTime::Due(8,phase))++phases[phase];}
        for(uint32 i=0;i<QuestGameTime::Count();++i){
            ++calls;unsigned phase=unsigned(QuestGameTime::Tick(i)%6);
            for(unsigned slot=320*phase/6;slot<320*(phase+1)/6;++slot)++visits[slot];
        }
    }
    Check(calls==CTimer::GetGameFrameCounter(),"each crossed gameplay tick visited exactly once");
    Check(calls==uint64(CTimer::GetTimeInMilliseconds())*30/1000,"gameplay ticks follow scaled clipped time");
    unsigned phaseTotal=0;for(unsigned n:phases)phaseTotal+=n;
    Check(phaseTotal==calls,"periodic phases are neither repeated nor skipped");
    for(unsigned n:visits)Check(n>=calls/6&&n<=calls/6+1,"partitioned pickup scans cover every slot fairly");
    std::printf("PASS: schedule %d FPS scale=%.1f jitter=%d ticks=%u\n",fps,scale,jitter,calls);
}
static void Doors(int fps,bool accelerating,int axis,int direction){
    Reset();CDoor door;door.Init(-100,100,int8(direction),int8(axis));door.m_fAngVel=accelerating?0:0.02f;CVehicle car;
    const float sign=(axis==2?direction==0:direction==1)?1.0f:-1.0f;
    for(int f=0;f<fps;++f){Tick(1000.0/fps);if(accelerating)car.speed.y+=sign*0.3f*CTimer::GetTimeStepInSeconds();door.Process(&car);}
    float v=accelerating?0:0.02f,angle=0;
    for(int i=0;i<(fps==15?27:30);++i){if(accelerating)v+=0.01f;v*=0.945f;angle+=v;}
    Check(std::fabs(door.m_fAngVel-v)<0.00003f,"door velocity matches original 30 FPS recurrence");
    Check(std::fabs(door.m_fAngle-angle)<0.0004f,"door angle matches original 30 FPS recurrence");
    CTimer::StartUserPause();Tick(1000);float saved=door.m_fAngle;door.Process(&car);
    Check(door.m_fAngle==saved,"paused door does not integrate");CTimer::EndUserPause();
    door.Init(-0.4f,0.4f,0,2);door.m_fAngle=0;door.m_fAngVel=0.3f;
    for(int i=0;i<fps*3;++i){Tick(1000.0/fps);door.Process(&car);Check(std::isfinite(door.m_fAngle)&&door.m_fAngle>=-0.4f&&door.m_fAngle<=0.4f,"door stop collisions stay bounded");}
    std::printf("PASS: door %d FPS acceleration=%d referenceAngle=%.6f\n",fps,accelerating,angle);
}
static void Destruction(int fps,bool airtrain,bool deferred){
    Reset();Tick(17);CPlane plane;plane.model=airtrain?MI_AIRTRAIN:MI_DEADDODO;
    plane.m_bIsDrugRunCesna=!airtrain;plane.m_bIsDropOffCesna=!airtrain;
    plane.m_nFrameWhenHit=CTimer::GetTimeInMilliseconds();
    explosions.clear();debris=fades=0;CesnaMissionStatus=DropOffCesnaMissionStatus=0;pDrugRunCesna=pDropOffCesna=&plane;
    const int end=airtrain?82:62;
    for(int f=0;f<fps*4;++f){
        Tick(1000.0/fps);
        if(!deferred){
            plane.ProcessDestruction();
            Check(plane.destroyed==(uint64(CTimer::GetTimeInMilliseconds()-plane.m_nFrameWhenHit)*30/1000>=uint64(end)),"destruction completion follows elapsed game time since impact");
        }
        if(f==fps/2){
            const uint64 before=CTimer::GetGameFrameCounter();CTimer::StartUserPause();
            for(int pause=0;pause<fps;++pause){Tick(1000.0/fps);if(!deferred)plane.ProcessDestruction();}
            Check(CTimer::GetGameFrameCounter()==before,"pause freezes destruction clock");CTimer::EndUserPause();
        }
    }
    if(deferred)plane.ProcessDestruction();
    Check(plane.destroyed&&plane.bRenderScorched,"deferred destruction preserves scorch and removal events");
    Check(explosions.size()==size_t(airtrain?21:11)&&debris==40&&fades==2,"all destruction events fire exactly once");
    const size_t count=explosions.size();plane.ProcessDestruction();
    Check(explosions.size()==count&&fades==2,"completed destruction never repeats events");
    if(!airtrain)Check(CesnaMissionStatus==CESNA_STATUS_DESTROYED&&DropOffCesnaMissionStatus==CESNA_STATUS_DESTROYED&&!pDrugRunCesna&&!pDropOffCesna,"Cesna mission callbacks are preserved");
    if(!deferred)Check(explosions.front()-plane.m_nFrameWhenHit>=666&&explosions.front()-plane.m_nFrameWhenHit<=727,"first explosion keeps its 20/30-second delay");
    std::printf("PASS: destruction %d FPS airtrain=%d deferred=%d explosions=%zu\n",fps,airtrain,deferred,count);
}
int main(){
    for(int fps:{15,18,20,30,36,60,72,80,90,120,144}){
        Schedules(fps,1,false);Schedules(fps,0.5f,false);Schedules(fps,1,true);
        for(int axis:{0,2})for(int direction:{0,1}){Doors(fps,false,axis,direction);Doors(fps,true,axis,direction);}
        Destruction(fps,false,false);Destruction(fps,true,false);Destruction(fps,false,true);Destruction(fps,true,true);
    }
    Reset();Tick(1000);Check(CTimer::GetGameFramesPassed()==1,"long stall keeps only clipped gameplay time");
    Reset();Check(CTimer::GetGameFrameCounter()==0&&CTimer::GetGameFramesPassed()==0,"new game resets gameplay schedule state");
    std::printf("PASS: %u door/destruction/schedule checks\n",checks);
}
