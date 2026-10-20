#include "ragdoll-test-vector.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#define __ANDROID__
#define FIX_BUGS
using int32=int; using int8=signed char; using uint16=unsigned short;
static float Min(float a,float b){return std::min(a,b);}
static float Max(float a,float b){return std::max(a,b);}
static float Abs(float v){return std::fabs(v);}
static float Pow(float a,float b){return std::pow(a,b);}
static float Sqrt(float v){return std::sqrt(v);}
static float sq(float v){return v*v;}
struct CTimer {
    static float step;
    static float GetTimeStep(){return step;}
    static float GetTimeStepInSeconds(){return step/50.0f;}
    static float GetTimeStepFix(){return step/(50.0f/30.0f);}
};
float CTimer::step;
struct CMatrix {
    CVector right=CVector(1,0,0),forward=CVector(0,1,0),up=CVector(0,0,1),position;
    void Translate(const CVector &v){position+=v;}
    void Yaw(float angle){right=CVector(std::cos(angle),std::sin(angle),0);forward=CVector(-std::sin(angle),std::cos(angle),0);}
};
static CVector Multiply3x3(const CMatrix &m,const CVector &v){return m.right*v.x+m.forward*v.y+m.up*v.z;}
struct CPhysical {
    CMatrix matrix;
    CVector m_vecMoveSpeed,m_vecTurnSpeed,m_vecMoveFriction,m_vecTurnFriction,m_vecCentreOfMass;
    float m_fMass=1000,m_fTurnMass=2000,m_fAirResistance=0.01f;
    bool bAffectedByGravity=true;
    CMatrix &GetMatrix(){return matrix;}
    CVector GetSpeed(const CVector &);
    float GetMass(const CVector &,const CVector &);
    void ApplyMoveSpeed();
    void ApplyMoveForce(float,float,float);
    void ApplyMoveForce(const CVector &v){ApplyMoveForce(v.x,v.y,v.z);}
    void ApplyTurnForce(float,float,float,float,float,float);
    void ApplyTurnForce(const CVector &v,const CVector &p){ApplyTurnForce(v.x,v.y,v.z,p.x,p.y,p.z);}
    bool ApplySpringCollision(float,CVector &,CVector &,float,float);
    bool ApplySpringDampening(float,CVector &,CVector &,CVector &);
    void ApplyGravity(); void ApplyAirResistance();
};
enum tWheelState {WHEEL_STATE_NORMAL,WHEEL_STATE_SPINNING,WHEEL_STATE_SKIDDING,WHEEL_STATE_FIXED};
enum {WHEEL_STATUS_OK,WHEEL_STATUS_BURST,WHEEL_STATUS_MISSING,MI_RCBANDIT};
static struct HandlingManager {float fWheelFriction=0.9f;} mod_HandlingManager;
struct CGeneral {static float GetRandomNumberInRange(float a,float b){return (a+b)*0.5f;}};
struct Handling {float fTractionLoss=0.8f,fMass=1000;};
struct CVehicle : CPhysical {
    Handling handling; Handling *pHandling=&handling;
    float m_fGasPedal=0;
    int GetModelIndex(){return -1;}
    void ProcessWheel(CVector &,CVector &,CVector &,CVector &,int32,float,float,float,int8,float *,tWheelState *,uint16);
};
#pragma warning(push)
#pragma warning(disable: 4100) // Native wheelId/wheelSpeed parameters are unused.
#include "wheel-suspension-fps-production.inc"
#pragma warning(pop)
static unsigned checks;
static void Check(bool value,const char *message){++checks;if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
static bool Finite(const CVector &v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&v.MagnitudeSqr()<1.0e8f;}
static float WheelCorrection(CVehicle &car,tWheelState initial,int wheelId=0,float brake=0,float thrust=0){
    car.m_vecMoveSpeed=CVector();car.m_vecTurnSpeed=CVector();
    // Between clean and already-skidding adhesion limits at every timestep.
    CVector fwd(0,1,0),right(1,0,0),contact(0.9f*0.002f*CTimer::step*4,0,0),point;
    float wheelSpeed=0;
    car.ProcessWheel(fwd,right,contact,point,4,thrust,brake,0.002f,
        static_cast<int8>(wheelId),&wheelSpeed,&initial,WHEEL_STATUS_OK);
    return -car.m_vecMoveSpeed.x;
}
static void SkidHistoryRegression(){
    for(int fps : {15,18,20,30,36,60,72,80,90,120,144}){
        CTimer::step=50.0f/float(fps);
        const float normal=0.9f*0.002f*CTimer::step;
        for(tWheelState state : {WHEEL_STATE_SPINNING,WHEEL_STATE_SKIDDING,WHEEL_STATE_FIXED}){
            CVehicle car,other;
            const float reduced=0.002f*CTimer::step*car.handling.fTractionLoss;
            Check(std::fabs(WheelCorrection(car,state)-reduced)<1.0e-7f,
                "incoming non-normal wheel retains its own traction loss");
            Check(std::fabs(WheelCorrection(other,WHEEL_STATE_NORMAL)-normal)<1.0e-7f,
                "another vehicle does not inherit skid history");
            for(int wheel=0;wheel<4;++wheel){
                WheelCorrection(car,state,(wheel+3)%4);
                Check(std::fabs(WheelCorrection(car,WHEEL_STATE_NORMAL,wheel)-normal)<1.0e-7f,
                    "another wheel does not inherit skid history");
                WheelCorrection(car,state,wheel);
                Check(std::fabs(WheelCorrection(car,WHEEL_STATE_NORMAL,wheel)-normal)<1.0e-7f,
                    "a recovered wheel does not retain stale skid history");
            }
            // Braking/driving flags must also reflect only the current call.
            WheelCorrection(other,state,0,0.001f);
            WheelCorrection(other,state,1,0,0.001f);
            Check(std::fabs(WheelCorrection(car,WHEEL_STATE_NORMAL)-normal)<1.0e-7f,
                "interleaved braking and driving do not contaminate normal grip");
        }
    }
}
static void Smoke(){
    CPhysical p; CVector dir(0,0,-1),point,velocity;
    CTimer::step=1;
    p.ApplySpringDampening(0.15f,dir,point,velocity);
    Check(Finite(p.m_vecMoveSpeed)&&p.m_vecMoveSpeed.MagnitudeSqr()==0,"stationary damper handles zero speed");
    p.ApplySpringCollision(1.5f,dir,point,1,0.5f);
    Check(p.m_vecMoveSpeed.MagnitudeSqr()==0,"fully extended spring adds no impulse");
    CTimer::step=0; p.ApplySpringCollision(1.5f,dir,point,0.5f,0.5f);
    Check(p.m_vecMoveSpeed.MagnitudeSqr()==0,"zero timestep spring adds no impulse");
    SkidHistoryRegression();
    std::printf("PASS: %u production suspension and skid-history checks\n",checks);
}
int main(int argc,char **argv){
    if(argc==1){Smoke();return 0;}
    Check(argc==4,"scenario, FPS and pacing arguments required");
    const char *scenario=argv[1],*pacing=argv[3];const int fps=std::atoi(argv[2]);
    Check(fps>=15&&fps<=144,"supported controlled frame rate");
    CVehicle car;
    const bool brake=std::strcmp(scenario,"brake")==0,coast=std::strcmp(scenario,"coast")==0;
    const bool lateral=std::strcmp(scenario,"lateral")==0,steer=std::strcmp(scenario,"steer")==0;
    const bool drop=std::strcmp(scenario,"drop")==0,bump=std::strcmp(scenario,"bump")==0;
    const bool grace=std::strcmp(scenario,"grace")==0;
    const bool history=std::strncmp(scenario,"history-",8)==0;
    Check(brake||coast||lateral||steer||drop||bump||grace||history,"known scenario");
    car.matrix.position.z=0.5f*(1.0f-1.0f/(4.0f*1.5f));
    if(brake||coast||steer)car.m_vecMoveSpeed.y=0.6f;
    if(lateral)car.m_vecMoveSpeed.x=0.08f;
    if(drop){car.matrix.position.z=0.65f;car.m_vecMoveSpeed.z=-0.06f;}
    tWheelState states[4]={};
    float wheelSpeed=0,contactTimer=4,contactGrace=0;
    double wall=0,sim=0,distance=0,stop=-1;float yaw=0;
    float minHeight=car.matrix.position.z,maxHeight=minHeight,peakVertical=0,lateMin=1.0e30f,lateMax=-1.0e30f;
    int skids=0,locks=0,wheelSamples=0,reversals=0;float previousForward=car.m_vecMoveSpeed.y;
    if(history){
        CTimer::step=50.0f/float(fps);
        CVector fwd(0,1,0),right(1,0,0),contact,point;
        if(std::strcmp(scenario,"history-primed")==0){
            CVehicle other; tWheelState primed=WHEEL_STATE_SKIDDING;
            other.ProcessWheel(fwd,right,contact,point,4,0,0,0.002f,0,&wheelSpeed,&primed,WHEEL_STATUS_OK);
        }
        contact.x=0.005f;tWheelState clean=WHEEL_STATE_NORMAL;
        car.ProcessWheel(fwd,right,contact,point,4,0,0,0.002f,0,&wheelSpeed,&clean,WHEEL_STATUS_OK);
        std::printf("%s,%d,%s,0,0,-1,0,0,%.9f,0,0,0,0,0,0,0,0,0,0\n",scenario,fps,pacing,car.m_vecMoveSpeed.x*50.0f);
        return 0;
    }
    const double duration=(brake||coast)?12.0:4.0;
    for(int frame=0;wall<duration-1.0e-9;++frame){
        double dt=1.0/fps;
        if(std::strcmp(pacing,"jitter")==0)dt*=frame%2?1.15:0.85;
        if(std::strcmp(pacing,"stall")==0&&frame%fps==fps/2)dt=0.12;
        dt=std::min(dt,duration-wall);
        CTimer::step=float(std::min(dt,0.06)*50.0);wall+=dt;sim+=CTimer::GetTimeStepInSeconds();
        if(grace){
            ProductionContactTimer(1.0f,contactTimer);
            if(contactTimer==0&&contactGrace==0)contactGrace=float(sim*1000.0);
        }else if(drop||bump){
            // A constrained vertical rig and analytic horizontal road replace
            // model collision. Native gravity, impulses, point speeds, damping
            // order and translation are used; pitch/roll are constrained.
            car.ApplyGravity();car.ApplyAirResistance();
            const float road=(bump&&sim>=1&&sim<1.2)?0.08f:0;
            const float ratio=std::max(0.0f,std::min(1.0f,(car.matrix.position.z-road)/0.5f));
            CVector dir(0,0,-1),points[4],speeds[4];
            for(int i=0;i<4;++i){
                // Engine wheel indices: front-left, rear-left, front-right, rear-right.
                points[i]=CVector(i>=2?0.8f:-0.8f,i%2?-1.2f:1.2f,-car.matrix.position.z+road);
                car.ApplySpringCollision(1.5f,dir,points[i],ratio,0.5f);
            }
            for(int i=0;i<4;++i)speeds[i]=car.GetSpeed(points[i]);
            for(int i=0;i<4;++i)if(ratio<1)car.ApplySpringDampening(0.15f,dir,points[i],speeds[i]);
            car.m_vecTurnSpeed=CVector();
            car.ApplyMoveSpeed();
            minHeight=std::min(minHeight,car.matrix.position.z);maxHeight=std::max(maxHeight,car.matrix.position.z);
            peakVertical=std::max(peakVertical,std::fabs(car.m_vecMoveSpeed.z*50));
            if(sim>=2.5){lateMin=std::min(lateMin,car.matrix.position.z);lateMax=std::max(lateMax,car.matrix.position.z);}
        }else{
            // Planar four-contact rig: native point velocities are captured
            // before the four wheel calls, matching Automobile.cpp. A fixed
            // road constrains heave/pitch/roll; steering may produce yaw.
            CVector points[4],speeds[4];
            for(int i=0;i<4;++i){
                points[i]=Multiply3x3(car.matrix,CVector(i>=2?0.8f:-0.8f,i%2?-1.2f:1.2f,-0.5f));
                speeds[i]=car.GetSpeed(points[i]);
            }
            for(int i : {0,2,1,3}){
                const float angle=steer&&(i==0||i==2)?0.15f:0;
                CVector fwd=Multiply3x3(car.matrix,CVector(-std::sin(angle),std::cos(angle),0));
                CVector right=Multiply3x3(car.matrix,CVector(std::cos(angle),std::sin(angle),0));
                car.ProcessWheel(fwd,right,speeds[i],points[i],4,0,brake?0.005f*CTimer::step:0,0.002f,static_cast<int8>(i),&wheelSpeed,&states[i],WHEEL_STATUS_OK);
                skids+=states[i]==WHEEL_STATE_SKIDDING;locks+=states[i]==WHEEL_STATE_FIXED;++wheelSamples;
            }
            car.m_vecMoveSpeed.z=0;car.m_vecTurnSpeed.x=car.m_vecTurnSpeed.y=0;
            car.ApplyMoveSpeed();yaw+=car.m_vecTurnSpeed.z*CTimer::step;car.matrix.Yaw(yaw);
            distance+=car.m_vecMoveSpeed.Magnitude()*CTimer::step;
            const float fwd=DotProduct(car.m_vecMoveSpeed,car.matrix.forward);
            if(fwd*previousForward<0&&std::fabs(fwd)>1.0e-6f)++reversals;
            previousForward=fwd;
            if(stop<0&&car.m_vecMoveSpeed.Magnitude()*50<0.05f)stop=sim;
        }
        Check(Finite(car.m_vecMoveSpeed)&&Finite(car.m_vecTurnSpeed)&&Finite(car.matrix.position),"finite bounded state throughout controlled run");
    }
    std::printf("%s,%d,%s,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%d,%.9f,%.9f,%.9f\n",scenario,fps,pacing,wall,sim,stop,distance,
        DotProduct(car.m_vecMoveSpeed,car.matrix.forward)*50,DotProduct(car.m_vecMoveSpeed,car.matrix.right)*50,yaw*180.0/3.141592653589793,
        minHeight,maxHeight,peakVertical,wheelSamples?double(skids)/wheelSamples:0,wheelSamples?double(locks)/wheelSamples:0,reversals,contactGrace,
        car.matrix.position.z,lateMax>=lateMin?lateMax-lateMin:0);
}
