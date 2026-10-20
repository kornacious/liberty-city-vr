#include "ragdoll-test-vector.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#define __ANDROID__
#define FIX_BUGS
#define nil nullptr
using uint32=unsigned;using uint64=unsigned long long;
inline float Pow(float a,float b){return std::pow(a,b);}
struct CVector2D {float x=0,y=0;};
struct CTimer {
    static float step;static uint32 ticks,frame,previous;static uint64 counter;
    static float GetTimeStep(){return step;}static float GetTimeStepInSeconds(){return step/50;}
    static uint32 GetGameFramesPassed(){return ticks;}static uint64 GetGameFrameCounter(){return counter;}
    static uint32 GetFrameCounter(){return frame;}static uint32 GetPreviousTimeInMilliseconds(){return previous;}
};
float CTimer::step;uint32 CTimer::ticks,CTimer::frame,CTimer::previous;uint64 CTimer::counter;
enum {HELI_STATUS_HOVER,HELI_STATUS_CHASE_PLAYER,HELI_STATUS_FLY_AWAY,HELI_STATUS_SHOT_DOWN,HELI_STATUS_HOVER2};
struct CWorld {
    static bool rightBlocked,leftBlocked,frontBlocked;static unsigned queries;
    static void *TestSphereAgainstWorld(const CVector &p,float,void *,bool,bool,bool,bool,bool,bool){
        queries++;const bool blocked=std::fabs(p.y)>1?frontBlocked:(p.x>0?rightBlocked:leftBlocked);
        return blocked?&queries:nullptr;
    }
};
bool CWorld::rightBlocked,CWorld::leftBlocked,CWorld::frontBlocked;unsigned CWorld::queries;
struct Matrix {CVector pos;CVector &GetPosition(){return pos;}};
struct CHeli {
    int m_heliStatus=HELI_STATUS_HOVER2,m_pathState=6;uint32 m_randomSeed=0,m_nExplosionTimer=99999;
    float m_fTargetOffset=17,m_fAvoidanceSideSpeed=0;bool m_bTestRight=true;Matrix matrix;
    CVector &GetPosition(){return matrix.pos;}Matrix &GetMatrix(){return matrix;}
    CVector GetRight(){return CVector(1,0,0);}CVector GetForward(){return CVector(0,1,0);}
    void ProcessAvoidance(CVector2D &,const CVector2D &);void Landing(CVector);
};
#include "heli-control-timing-production.inc"
static unsigned checks;
__declspec(noinline) static void Check(bool ok,const char *message){++checks;if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
static bool Near(float a,float b){return std::fabs(a-b)<0.0002f;}
// Independent 30 FPS behavior, with supplied scene answers and tick phase.
static unsigned Reference(CHeli &h,uint64 tick){
    if(h.m_heliStatus!=HELI_STATUS_HOVER2&&h.m_heliStatus!=HELI_STATUS_SHOT_DOWN){
        h.m_fAvoidanceSideSpeed=0;if(h.m_fTargetOffset>=2)h.m_fTargetOffset-=2;return 0;
    }
    const bool force=((tick+h.m_randomSeed)&8)!=0;unsigned queries=force?0:1;
    const bool blocked=h.m_bTestRight?CWorld::rightBlocked:CWorld::leftBlocked;
    if(force||!blocked){
        h.m_fAvoidanceSideSpeed=(h.m_bTestRight?1:-1)*(h.m_heliStatus==HELI_STATUS_SHOT_DOWN?0.15f:0.05f);
        if(h.m_heliStatus==HELI_STATUS_SHOT_DOWN)h.m_fTargetOffset-=50.0f/30*0.05f;
    }else{
        h.m_bTestRight=!h.m_bTestRight;h.m_fAvoidanceSideSpeed=0;
        h.m_fTargetOffset+=h.m_heliStatus==HELI_STATUS_HOVER2?5:-5;
    }
    h.m_fTargetOffset=std::min(30.0f,h.m_fTargetOffset);
    if(h.m_heliStatus==HELI_STATUS_SHOT_DOWN&&force){
        queries+=CWorld::frontBlocked?1:2;
        if(CWorld::frontBlocked)h.m_nExplosionTimer=CTimer::previous;
    }
    return queries;
}
struct Clock {
    double fraction=0,time=0;unsigned remainder=0;
    void Step(double dt){
        CTimer::previous=uint32(time*1000);time+=dt;CTimer::step=float(dt*50);CTimer::frame++;
        fraction+=dt*1000;const unsigned ms=unsigned(fraction+1e-8);fraction-=ms;
        remainder+=ms*30;CTimer::ticks=remainder/1000;remainder%=1000;CTimer::counter+=CTimer::ticks;
    }
};
static void Run(int fps,float scale,bool jitter,int status,unsigned seed,int scene){
    CHeli h,reference;h.m_heliStatus=reference.m_heliStatus=status;h.m_randomSeed=reference.m_randomSeed=seed;
    Clock clock;CTimer::counter=0;CTimer::frame=0;
    CWorld::rightBlocked=scene!=0;CWorld::leftBlocked=scene==2;CWorld::frontBlocked=scene==2;
    for(int frame=0;frame<fps*2;++frame){
        clock.Step(std::min(0.06,scale/fps*(jitter?(frame%2?1.15:0.85):1.0)));
        unsigned expectedQueries=0;
        for(unsigned i=0;i<CTimer::ticks;++i)expectedQueries+=Reference(reference,CTimer::counter-CTimer::ticks+1+i);
        CWorld::queries=0;CVector2D target,dir;dir.x=0.6f;dir.y=-0.8f;
        h.ProcessAvoidance(target,dir);
        Check(Near(h.m_fTargetOffset,reference.m_fTargetOffset),"offset follows 30 Hz recovery and obstacle responses");
        Check(h.m_bTestRight==reference.m_bTestRight,"avoidance side changes only on reference decisions");
        Check(Near(target.x,reference.m_fAvoidanceSideSpeed*dir.x)&&Near(target.y,reference.m_fAvoidanceSideSpeed*dir.y),"latest lateral decision is held between game ticks");
        Check(h.m_nExplosionTimer==reference.m_nExplosionTimer,"shot-down contact preserves explosion-timer semantics");
        Check(CWorld::queries==expectedQueries,"obstacle and forward checks follow game-clock windows, not render FPS");
    }
}
static void Landing(int fps,float scale,bool jitter){
    CHeli h;h.matrix.pos=CVector(100,-200,42);const CVector target(10,-20,0);double time=0;
    for(int i=0;i<fps*2;++i){
        const float dt=float(std::min(0.06,scale/fps*(jitter?(i%2?1.15:0.85):1.0)));
        CTimer::step=dt*50;time+=CTimer::GetTimeStepInSeconds();h.Landing(target);
        const float decay=float(std::pow(0.99,30*time));
        Check(std::fabs(h.GetPosition().x-(10+90*decay))<0.001f&&std::fabs(h.GetPosition().y-(-20-180*decay))<0.002f,"landing blend follows equal game time");
        Check(h.GetPosition().z==42,"landing blend leaves scripted altitude handling intact");
    }
}
static void PauseAndTransitions(){
    CHeli h;CWorld::rightBlocked=CWorld::leftBlocked=CWorld::frontBlocked=false;
    CTimer::counter=8;CTimer::ticks=1;CTimer::step=50.0f/30;CVector2D dir,target;dir.x=1;
    h.ProcessAvoidance(target,dir);const float offset=h.m_fTargetOffset;
    CTimer::ticks=0;CTimer::step=0;CWorld::queries=0;
    for(int i=0;i<100;++i){target=CVector2D();h.ProcessAvoidance(target,dir);}
    Check(h.m_fTargetOffset==offset&&CWorld::queries==0,"pause does not repeat obstacle responses or queries");
    h.m_heliStatus=HELI_STATUS_HOVER;target=CVector2D();h.ProcessAvoidance(target,dir);
    Check(target.x==0&&h.m_fAvoidanceSideSpeed==0&&h.m_fTargetOffset==offset,"leaving avoidance clears cached steering without advancing paused recovery");
    h.m_fTargetOffset=5;CTimer::ticks=2;CTimer::step=3;h.ProcessAvoidance(target,dir);
    Check(h.m_fTargetOffset==1,"catch-up preserves the original odd recovery remainder");
    CHeli first,second;
    CWorld::rightBlocked=CWorld::leftBlocked=true;CWorld::frontBlocked=false;
    CTimer::counter=8;CTimer::ticks=2;CTimer::step=3;CWorld::queries=0;
    CVector2D a,b;first.ProcessAvoidance(a,dir);second.ProcessAvoidance(b,dir);
    Check(first.m_fTargetOffset==22&&second.m_fTargetOffset==22&&first.m_bTestRight==false,"catch-up processes the blocked tick before the forced window for each helicopter");
    Check(Near(a.x,-0.05f)&&Near(b.x,-0.05f)&&CWorld::queries==2,"helicopters retain separate decisions without consuming shared tick counts");
    CHeli down;down.m_heliStatus=HELI_STATUS_SHOT_DOWN;
    CWorld::frontBlocked=true;CTimer::previous=123;CVector2D crash;
    down.ProcessAvoidance(crash,dir);
    Check(down.m_nExplosionTimer==123,"forward contact during crossed forced window schedules immediate destruction");
    CHeli landing;landing.matrix.pos=CVector(10,20,30);CTimer::step=0;landing.Landing(CVector());
    Check(landing.GetPosition().x==10&&landing.GetPosition().y==20,"paused landing blend does not move the helicopter");
}
int main(){
    for(int fps:{15,18,20,30,36,60,72,80,90,120,144})for(float scale:{0.5f,1.0f})for(bool jitter:{false,true}){
        Landing(fps,scale,jitter);
        for(int status:{HELI_STATUS_HOVER,HELI_STATUS_HOVER2,HELI_STATUS_SHOT_DOWN})for(unsigned seed:{0u,7u,8u,15u})for(int scene:{0,1,2})Run(fps,scale,jitter,status,seed,scene);
    }
    PauseAndTransitions();std::printf("PASS: %u production helicopter timing checks\n",checks);
}
