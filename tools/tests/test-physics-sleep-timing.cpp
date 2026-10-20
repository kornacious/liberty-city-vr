#define CVector SleepBaseVector
#include "ragdoll-test-vector.h"
#undef CVector
struct CVector:SleepBaseVector {
    using SleepBaseVector::SleepBaseVector;
    CVector(const SleepBaseVector &v):SleepBaseVector(v){}
    bool IsZero() const{return x==0&&y==0&&z==0;}
};
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#define __ANDROID__
#define FIX_BUGS
#define nil nullptr
using uint8=unsigned char;using uint32=unsigned;using uint64=unsigned long long;
inline float Pow(float a,float b){return std::pow(a,b);}
template<class T> T Min(T a,T b){return std::min(a,b);}
static float sq(float a){return a*a;}
struct CTimer {
    static float step;static uint32 ticks;static uint64 counter;
    static float GetTimeStep(){return step;}static float GetTimeStepInSeconds(){return step/50;}
    static uint32 GetGameFramesPassed(){return ticks;}static uint64 GetGameFrameCounter(){return counter;}
};
float CTimer::step;uint32 CTimer::ticks;uint64 CTimer::counter;
enum {STATUS_PHYSICS,STATUS_SIMPLE,STATUS_ABANDONED,STATUS_WRECKED};
struct CPhysical {
    CVector m_vecMoveSpeed,m_vecTurnSpeed,m_vecMoveSpeedAvg,m_vecTurnSpeedAvg,m_vecMoveFriction,m_vecTurnFriction,pos;
    bool bIsInWater=false,bHasContacted=false,bIsInSafePosition=false,bWasPostponed=false,bHasHitWall=false;
    bool bHasCollided=false,bIsStuck=false,bPedPhysics=false,asleep=false,object=true,ped=false;
    int status=STATUS_PHYSICS,m_nCollisionRecords=0,m_nDamagePieceType=0,gravityCalls=0;
    float m_fDamageImpulse=0,m_fDistanceTravelled=0,m_fSleepDistance=0;
    void *m_pDamageEntity=nullptr;uint8 m_nStaticFrames=0;
    bool IsPed(){return ped;}bool IsObject(){return object;}int GetStatus(){return status;}
    void SetIsStatic(bool v){asleep=v;}CVector &GetPosition(){return pos;}
    void ApplyGravity(){gravityCalls++;}void ApplyFriction(){}void ApplyAirResistance(){}
    void ProcessControl();
};
struct CAutomobile:CPhysical {
    bool bVehicleColProcessed=true;float m_aSuspensionSpringRatioPrev[4]={1,1,1,1};
    bool CheckSleep();
};
struct CCarCtrl {static bool movingMap;static bool MapCouldMoveInThisArea(float,float){return movingMap;}};
bool CCarCtrl::movingMap;
struct CWorld {static bool bForceProcessControl;};
bool CWorld::bForceProcessControl;
#include "physics-sleep-timing-production.inc"
static unsigned checks;
__declspec(noinline) static void Check(bool ok,const char *message){++checks;if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
static bool Near(float a,float b){return std::fabs(a-b)<0.000002f;}
struct Clock {
    double fraction=0;unsigned remainder=0;
    void Step(double seconds){
        CTimer::step=float(seconds*50);fraction+=seconds*1000;
        const unsigned ms=unsigned(fraction+1e-8);fraction-=ms;
        remainder+=ms*30;CTimer::ticks=remainder/1000;remainder%=1000;CTimer::counter+=CTimer::ticks;
    }
};
static void Reference30(){
    CTimer::step=50.0f/30;CTimer::ticks=1;
    CPhysical p;p.m_vecMoveSpeed=CVector(0.004f,0,0);
    float avg=0;
    for(int i=1;i<=11;++i){
        avg=(avg+0.004f)/2;p.ProcessControl();
        Check(Near(p.m_vecMoveSpeedAvg.x,avg),"30 FPS velocity average preserved");
        Check(p.asleep==(i==11),"30 FPS object sleeps on eleventh qualifying update");
    }
    for(int status:{STATUS_ABANDONED,STATUS_WRECKED}){
        CAutomobile car;car.status=status;
        for(int i=1;i<=11;++i)Check(car.CheckSleep()==(i==11),"30 FPS vehicle quiet delay preserved");
    }
}
static void Rates(int fps,float scale,bool jitter){
    Clock clock;CPhysical p;CAutomobile abandoned,wrecked;
    abandoned.status=STATUS_ABANDONED;wrecked.status=STATUS_WRECKED;
    unsigned ticks=0;double elapsed=0;bool stopped=false;
    for(int frame=0;elapsed<0.7;++frame){
        const double dt=std::min(0.06,scale/fps*(jitter?(frame%2?1.15:0.85):1.0));
        clock.Step(dt);elapsed+=dt;ticks+=CTimer::ticks;
        p.ProcessControl();const bool a=abandoned.CheckSleep(),w=wrecked.CheckSleep();
        Check(p.asleep==(ticks>=11),"object sleep follows game-time quiet ticks");
        Check(a==(ticks>=11)&&w==(ticks>=11),"vehicles remain asleep between ticks after becoming ready");
        if(ticks>=11&&!stopped){
            Check(elapsed>=0.366-0.001&&elapsed<0.367+dt+0.001,"quiet sleep occurs near 367 game milliseconds");stopped=true;
        }
    }
    // Above the legacy 30 FPS threshold must stay awake at every rate.
    CPhysical moving;moving.m_vecMoveSpeedAvg=moving.m_vecMoveSpeed=CVector(0.0052f,0,0);
    for(int frame=0;frame<fps;++frame){clock.Step(std::min(0.06,1.0/fps));moving.ProcessControl();}
    Check(!moving.asleep&&moving.m_nStaticFrames==0,"speed threshold does not increase with low FPS");
    CPhysical gentle;gentle.m_vecMoveSpeedAvg=gentle.m_vecMoveSpeed=CVector(0.004f,0,0);
    CAutomobile slowCar;slowCar.status=STATUS_ABANDONED;
    slowCar.m_vecMoveSpeedAvg=slowCar.m_vecMoveSpeed=CVector(0.004f,0,0);
    slowCar.m_vecTurnSpeedAvg=slowCar.m_vecTurnSpeed=CVector(0,0.001f,0);
    CAutomobile spinning;spinning.status=STATUS_WRECKED;
    spinning.m_vecTurnSpeedAvg=spinning.m_vecTurnSpeed=CVector(0,0,0.0026f);
    bool slowAsleep=false,spinAsleep=false;
    for(int frame=0;frame<fps;++frame){
        clock.Step(std::min(0.06,1.0/fps));gentle.ProcessControl();
        slowAsleep=slowCar.CheckSleep();spinAsleep=spinning.CheckSleep();
    }
    Check(gentle.asleep&&slowAsleep,"gentle linear/angular motion below reference limits settles at high FPS");
    Check(!spinAsleep&&spinning.m_nStaticFrames==0,"wreck angular threshold remains independent of FPS");
    // Reference-space displacement is captured with the producing timestep.
    for(float previousStep:{50.0f/18,50.0f/72,50.0f/144}){
        CAutomobile car;car.status=STATUS_ABANDONED;car.m_nStaticFrames=11;
        CTimer::step=previousStep;car.m_fDistanceTravelled=0.006f*previousStep/(50.0f/30);
        car.m_fSleepDistance=QuestPhysicsSleep::ReferenceDistance(car.m_fDistanceTravelled);
        clock.Step(1.0/fps);
        Check(!car.CheckSleep()&&car.m_nStaticFrames==0,"previous displacement above limit prevents sleep despite changed dt");
        CTimer::step=previousStep;car.m_fDistanceTravelled=0.004f*previousStep/(50.0f/30);
        car.m_fSleepDistance=QuestPhysicsSleep::ReferenceDistance(car.m_fDistanceTravelled);
        car.m_nStaticFrames=11;clock.Step(1.0/fps);
        Check(car.CheckSleep(),"previous displacement below limit permits sleep despite changed dt");
    }
    // Analytic smoothing response while a large mean keeps the object awake.
    CPhysical smooth;smooth.m_vecMoveSpeedAvg=CVector(1,0,0);double time=0;
    for(int frame=0;frame<fps/4;++frame){
        const float dt=scale/fps*(jitter?(frame%2?1.15f:0.85f):1.0f);
        clock.Step(dt);time+=CTimer::GetTimeStepInSeconds();smooth.ProcessControl();
        Check(Near(smooth.m_vecMoveSpeedAvg.x,float(std::pow(0.5,time*30))),"velocity averaging follows elapsed time");
    }
}
static void GatesAndWake(){
    CTimer::step=50.0f/30;CTimer::ticks=1;
    CPhysical p;p.m_nStaticFrames=10;p.m_vecMoveSpeedAvg=CVector(0.001f,0,0);
    CTimer::step=0;CTimer::ticks=0;p.ProcessControl();
    Check(!p.asleep&&p.m_nStaticFrames==10&&p.m_vecMoveSpeedAvg.x==0.001f,"pause freezes averaging and quiet count");
    CTimer::step=3;CTimer::ticks=2;p.ProcessControl();Check(p.asleep,"stall catches crossed sleep ticks");
    p.asleep=false;p.m_nStaticFrames=0;CTimer::ticks=0;CTimer::step=50.0f/144;p.ProcessControl();
    Check(!p.asleep&&p.m_nStaticFrames==0,"external wake reset discards previous ready state");
    CTimer::ticks=1;CTimer::step=50.0f/30;
    for(int kind=0;kind<3;++kind){
        CPhysical q;q.m_nStaticFrames=10;
        if(kind==0)q.bIsStuck=true;
        if(kind==1)q.status=STATUS_SIMPLE;
        if(kind==2){q.object=false;q.ped=true;q.bPedPhysics=true;}
        q.ProcessControl();Check(!q.asleep,"stuck/simple/ped-physics eligibility gates preserved");
    }
    CAutomobile map;map.status=STATUS_WRECKED;CCarCtrl::movingMap=true;
    for(int i=0;i<300;++i)Check(!map.CheckSleep(),"moving map prevents physics skipping");
    Check(map.m_nStaticFrames==11,"quiet counter saturates instead of wrapping on moving map");
    CCarCtrl::movingMap=false;CTimer::ticks=0;Check(map.CheckSleep(),"settled vehicle skips physics immediately after map gate clears");
    map.m_nStaticFrames=0;map.bVehicleColProcessed=false;map.m_aSuspensionSpringRatioPrev[3]=0.5f;
    Check(map.CheckSleep(),"existing stationary-contact fast path preserved between ticks");
    map.bVehicleColProcessed=true;map.m_nStaticFrames=11;map.m_vecMoveSpeed=CVector(0.05f,0,0);CTimer::ticks=1;
    Check(!map.CheckSleep()&&map.m_nStaticFrames==0,"movement wakes a settled vehicle and clears its quiet history");
    CAutomobile retried;retried.status=STATUS_ABANDONED;
    retried.m_vecMoveSpeed=CVector(0.001f,0,0);retried.CheckSleep();
    const float firstAverage=retried.m_vecMoveSpeedAvg.x;const uint8 firstCount=retried.m_nStaticFrames;
    CWorld::bForceProcessControl=true;retried.CheckSleep();
    Check(retried.m_nStaticFrames==firstCount&&retried.m_vecMoveSpeedAvg.x==firstAverage,"postponed control retry does not count time or averaging twice");
    CWorld::bForceProcessControl=false;
}
int main(){
    Reference30();for(int fps:{15,18,20,30,36,60,72,80,90,120,144})for(float scale:{0.5f,1.0f})for(bool jitter:{false,true})Rates(fps,scale,jitter);
    GatesAndWake();std::printf("PASS: %u production physics sleep timing checks\n",checks);
}
