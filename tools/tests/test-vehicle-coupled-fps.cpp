#include "ragdoll-test-vector.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <vector>
#define __ANDROID__
#define FIX_BUGS
using int32=int;using int8=signed char;using uint16=unsigned short;
enum {STATUS_PHYSICS,STATUS_ABANDONED,STATUS_WRECKED};
static float Min(float a,float b){return std::min(a,b);}static float Max(float a,float b){return std::max(a,b);}
static float Abs(float a){return std::fabs(a);}static float Pow(float a,float b){return std::pow(a,b);}
static float Sqrt(float a){return std::sqrt(a);}static float sq(float a){return a*a;}
struct CTimer {static float step;static float GetTimeStep(){return step;}static float GetTimeStepInSeconds(){return step/50;}static float GetTimeStepFix(){return step/(50.0f/30);}};
float CTimer::step;
struct CMatrix {
    CVector right=CVector(1,0,0),forward=CVector(0,1,0),up=CVector(0,0,1),position;
    CVector &GetRight(){return right;}CVector &GetForward(){return forward;}CVector &GetUp(){return up;}
    void Translate(CVector v){position+=v;}void Reorthogonalise();
};
static CVector Multiply3x3(const CMatrix &m,const CVector &v){return m.right*v.x+m.forward*v.y+m.up*v.z;}
struct CColPoint {CVector point,normal;float depth=0;int surfaceA=0,surfaceB=0,pieceA=0,pieceB=0;};
struct CColLine {CVector p0,p1;};struct CColBox {CVector min,max;int surface=0,piece=0;};
struct CColSphere {CVector center;float radius;int surface=0,piece=0;};
struct CCollision {static bool ProcessLineBox(const CColLine &,const CColBox &,CColPoint &,float &);static bool ProcessSphereBox(const CColSphere &,const CColBox &,CColPoint &,float &);};
struct CEntity {bool IsBuilding(){return true;}};
struct CPhysical:CEntity {
    CMatrix matrix;CVector m_vecMoveSpeed,m_vecTurnSpeed,m_vecMoveFriction,m_vecTurnFriction,m_vecCentreOfMass;
    float m_fMass=1200,m_fTurnMass=2500,m_fAirResistance=0.01f,m_fElasticity=0.1f;
    bool bAffectedByGravity=true,bPedPhysics=false,bHasContacted=false,bHasHitWall=false,bInfiniteMass=false;
    CMatrix &GetMatrix(){return matrix;}CVector &GetPosition(){return matrix.position;}
    CVector &GetRight(){return matrix.right;}CVector &GetForward(){return matrix.forward;}CVector &GetUp(){return matrix.up;}
    bool IsObject(){return false;}bool IsVehicle(){return true;}
    int GetStatus(){return STATUS_PHYSICS;}
    CVector GetSpeed(const CVector &);float GetMass(const CVector &,const CVector &);
    void ApplyMoveSpeed();void ApplyTurnSpeed();void ApplyGravity();void ApplyAirResistance();
    void ApplyMoveForce(float,float,float);void ApplyMoveForce(const CVector &v){ApplyMoveForce(v.x,v.y,v.z);}
    void ApplyTurnForce(float,float,float,float,float,float);void ApplyTurnForce(const CVector &v,const CVector &p){ApplyTurnForce(v.x,v.y,v.z,p.x,p.y,p.z);}
    bool ApplySpringCollision(float,CVector &,CVector &,float,float);bool ApplySpringDampening(float,CVector &,CVector &,CVector &);
    bool ApplyCollisionAlt(CEntity *,CColPoint &,float &,CVector &,CVector &);
};
enum tWheelState {WHEEL_STATE_NORMAL,WHEEL_STATE_SPINNING,WHEEL_STATE_SKIDDING,WHEEL_STATE_FIXED};
enum {WHEEL_STATUS_OK,WHEEL_STATUS_BURST,WHEEL_STATUS_MISSING,MI_RCBANDIT};
static struct HandlingManager {float fWheelFriction=0.9f;} mod_HandlingManager;
struct CGeneral {static float GetRandomNumberInRange(float a,float b){return (a+b)*0.5f;}};
struct Handling {float fTractionLoss=0.8f,fMass=1200;};
struct CVehicle:CPhysical {
    Handling handling;Handling *pHandling=&handling;float m_fGasPedal=0;
    int GetModelIndex(){return -1;}
    bool IsBike(){return false;}
    void ProcessWheel(CVector &,CVector &,CVector &,CVector &,int32,float,float,float,int8,float *,tWheelState *,uint16);
};
#pragma warning(push)
#pragma warning(disable:4100) // Unused native wheel/response parameters.
#include "vehicle-coupled-fps-production.inc"
#pragma warning(pop)
static unsigned long long checks;
static void Check(bool ok,const char *why){++checks;if(!ok){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}}
static bool Finite(const CVector &v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&v.MagnitudeSqr()<1e8f;}
static constexpr float Pi=3.14159265358979323846f;
// Generated contact geometry; no copied retail car/road data.
static std::vector<CColBox> Scene(const char *scenario){
    std::vector<CColBox> boxes;boxes.push_back({CVector(-10000,-10000,-5),CVector(10000,10000,0)});
    if(std::strcmp(scenario,"curb")==0)boxes.push_back({CVector(-0.1f,8,0),CVector(3,8.3f,0.18f)});
    if(std::strcmp(scenario,"wall-impact")==0)boxes.push_back({CVector(-3,8,0),CVector(3,8.3f,1)});
    return boxes;
}
struct Metrics {
    double distance=0,stop=-1;float maxRoll=0,maxPitch=0,maxZSpeed=0,minHeight=1e9f,maxHeight=-1e9f;
    float maxLoadSplit=0,maxAxleSplit=0,maxSpring=0,lateMin=1e9f,lateMax=-1e9f,maxDepth=0,maxImpulse=0;
    unsigned contacts=0,airSteps=0,steps=0;bool inverted=false;
};
struct Rig {
    CVehicle car;Metrics m;std::vector<CColBox> boxes;
    tWheelState state[4]={};float timer[4]={},wheelSpeed[4]={};
    explicit Rig(const char *scenario):boxes(Scene(scenario)){
        // Ray length .70, spring travel .45, wheel-radius part .25.
        car.matrix.position.z=0.70f-0.45f/(4*1.5f);
        if(std::strcmp(scenario,"brake")==0)car.m_vecMoveSpeed.y=0.4f;
        if(std::strcmp(scenario,"corner")==0)car.m_vecMoveSpeed.y=0.3f;
        if(std::strcmp(scenario,"curb")==0||std::strcmp(scenario,"wall-impact")==0)car.m_vecMoveSpeed.y=0.16f;
        if(std::strstr(scenario,"landing")){
            car.matrix.position.z=1.5f;car.m_vecMoveSpeed.z=-0.08f;
            if(std::strcmp(scenario,"hard-landing")==0){car.matrix.position.z=2;car.m_vecMoveSpeed.z=-0.3f;}
            if(std::strcmp(scenario,"tilted-landing")==0){
                car.matrix.right=CVector(std::cos(0.12f),0,-std::sin(0.12f));
                car.matrix.up=CVector(std::sin(0.12f),0,std::cos(0.12f));
                car.matrix.forward=CVector(0,1,0);
            }
        }
    }
    void Step(const char *scenario,float dt,double time){
        CTimer::step=dt*50;car.ApplyGravity();car.ApplyAirResistance();
        CVector point[4],speed[4],dir[4];float ratio[4],compression[4];int grounded=0;
        // Engine index order FL, RL, FR, RR. Rays follow the rotating chassis.
        for(int i=0;i<4;++i){
            const CVector anchor(i>=2?0.75f:-0.75f,i%2?-1.2f:1.2f,0);
            CColLine line;line.p0=car.GetPosition()+Multiply3x3(car.matrix,anchor);
            line.p1=line.p0-car.GetUp()*0.70f;float nearest=1;CColPoint cp;
            for(const auto &box:boxes)CCollision::ProcessLineBox(line,box,cp,nearest);
            ratio[i]=nearest<1?(nearest-(1-0.45f/0.70f))/(0.45f/0.70f):1;
            point[i]=nearest<1?cp.point-car.GetPosition():line.p1-car.GetPosition();
            dir[i]=-car.GetUp();compression[i]=std::max(0.0f,1-ratio[i]);
            m.maxSpring=std::max(m.maxSpring,compression[i]);
            ContactTimer(ratio[i],timer[i]);grounded+=timer[i]>0;
        }
        const float total=compression[0]+compression[1]+compression[2]+compression[3];
        if(total>0)m.maxLoadSplit=std::max(m.maxLoadSplit,std::fabs(compression[0]+compression[1]-compression[2]-compression[3])/total);
        if(total>0)m.maxAxleSplit=std::max(m.maxAxleSplit,std::fabs(compression[0]+compression[2]-compression[1]-compression[3])/total);
        if(grounded==0)m.airSteps++;
        for(int i=0;i<4;++i)if(ratio[i]<1)car.ApplySpringCollision(1.5f,dir[i],point[i],ratio[i],0.5f);
        for(int i=0;i<4;++i)speed[i]=car.GetSpeed(point[i]);
        for(int i=0;i<4;++i)if(ratio[i]<1)car.ApplySpringDampening(0.15f,dir[i],point[i],speed[i]);
        for(int i=0;i<4;++i)speed[i]=car.GetSpeed(point[i]);
        const float steer=std::strcmp(scenario,"corner")==0&&time>=0.5?0.10f:0;
        for(int i:{0,2,1,3})if(timer[i]>0){
            const float a=(i==0||i==2)?steer:0;
            CVector fwd=Multiply3x3(car.matrix,CVector(-std::sin(a),std::cos(a),0));
            CVector right=Multiply3x3(car.matrix,CVector(std::cos(a),std::sin(a),0));
            car.ProcessWheel(fwd,right,speed[i],point[i],grounded,0,std::strcmp(scenario,"brake")==0?0.005f*CTimer::step:0,
                0.002f,static_cast<int8>(i),&wheelSpeed[i],&state[i],WHEEL_STATUS_OK);
        }
        // Free six-degree chassis integration, with native matrix repair.
        const CMatrix saved=car.matrix;
        car.ApplyMoveSpeed();car.ApplyTurnSpeed();car.matrix.Reorthogonalise();
        // Native narrow phase and static response on six chassis spheres.
        // Native ProcessCollisionSectorList averages contact velocity responses.
        CVector dv,dw,shift;unsigned responses=0;float deepest=0;
        CEntity road;
        for(int i=0;i<6;++i){
            const CVector local(i%2?0.55f:-0.55f,i/2==0?-1.1f:i/2==1?0:1.1f,0);
            CColSphere sphere{car.GetPosition()+Multiply3x3(car.matrix,local),0.28f};
            for(const auto &box:boxes){
                CColPoint cp;float nearest=sphere.radius*sphere.radius;
                if(CCollision::ProcessSphereBox(sphere,box,cp,nearest)){
                    m.contacts++;m.maxDepth=std::max(m.maxDepth,cp.depth);
                    float impulse=0;
                    if(car.ApplyCollisionAlt(&road,cp,impulse,dv,dw))responses++;
                    m.maxImpulse=std::max(m.maxImpulse,impulse*50); // engine velocity is m/20ms.
                    if(cp.depth>deepest){deepest=cp.depth;shift=cp.normal*cp.depth;}
                }
            }
        }
        if(responses){car.m_vecMoveSpeed+=dv/float(responses);car.m_vecTurnSpeed+=dw/float(responses);}
        // Explicit fixture depenetration, NOT the engine's sector retry/shift loop.
        car.matrix.position+=shift;
        const float speedMps=car.m_vecMoveSpeed.Magnitude()*50;
        m.distance+=(car.GetPosition()-saved.position).Magnitude();
        if(m.stop<0&&time>0.1&&speedMps<0.05)m.stop=time;
        m.minHeight=std::min(m.minHeight,car.GetPosition().z);m.maxHeight=std::max(m.maxHeight,car.GetPosition().z);
        m.maxZSpeed=std::max(m.maxZSpeed,std::fabs(car.m_vecMoveSpeed.z*50));
        m.maxRoll=std::max(m.maxRoll,std::fabs(std::atan2(car.GetRight().z,car.GetUp().z)*180/Pi));
        m.maxPitch=std::max(m.maxPitch,std::fabs(std::atan2(car.GetForward().z,car.GetUp().z)*180/Pi));
        m.inverted=m.inverted||car.GetUp().z<0;
        if(time>=5){m.lateMin=std::min(m.lateMin,car.GetPosition().z);m.lateMax=std::max(m.lateMax,car.GetPosition().z);}
        m.steps++;
        Check(Finite(car.GetPosition())&&Finite(car.m_vecMoveSpeed)&&Finite(car.m_vecTurnSpeed),"finite bounded coupled state");
        Check(std::fabs(DotProduct(car.GetRight(),car.GetUp()))<0.001f&&std::fabs(car.GetUp().Magnitude()-1)<0.001f,"native matrix repair retains orthonormal chassis");
    }
};
static void Smoke(){
    CColPoint cp;float t=1;CColBox box{CVector(-1,-1,-1),CVector(1,1,0)};
    Check(CCollision::ProcessLineBox({CVector(0,0,1),CVector(0,0,-1)},box,cp,t)&&std::fabs(t-.5f)<1e-6f&&cp.normal.z==1,"native suspension ray detects road top");
    float d=.25f;Check(CCollision::ProcessSphereBox({CVector(0,0,.2f),.5f},box,cp,d)&&std::fabs(cp.depth-.3f)<1e-6f,"native chassis sphere detects penetration");
    Rig rig("settle");for(int i=0;i<720;++i)rig.Step("settle",1.0f/120,(i+1)/120.0);
    Check(std::fabs(rig.car.GetPosition().z-.625f)<.01f,"four native springs support chassis near predicted equilibrium");
    Check(rig.m.maxRoll<.01f&&rig.m.maxPitch<.01f,"symmetric stationary chassis does not acquire pitch or roll");
    Rig tipped("tilted-landing");for(int i=0;i<720;++i)tipped.Step("tilted-landing",1.0f/120,(i+1)/120.0);
    Check(tipped.m.maxLoadSplit>.1f&&tipped.m.maxRoll>1,"asymmetric landing exercises free roll and unequal spring loads");
    CVehicle car;car.m_vecMoveSpeed=CVector(0,0,-.1f);CTimer::step=1;
    cp.point=CVector(0,0,-.3f);cp.normal=CVector(0,0,1);CVector dv,dw;float impulse=0;CEntity ground;
    Check(car.ApplyCollisionAlt(&ground,cp,impulse,dv,dw)&&dv.z>0&&impulse>0,"native static contact response opposes incoming velocity");
    std::printf("PASS: %llu coupled fixture checks\n",checks);
}
static void Run(FILE *out,const char *scenario,int fps,const char *pacing,int fixed){
    Rig rig(scenario);double wall=0,sim=0,debt=0;int frame=0;
    while(sim<6-1e-8){
        double dt=1.0/fps;
        if(std::strcmp(pacing,"jitter")==0)dt*=frame%2?1.15:0.85;
        if(std::strcmp(pacing,"stall")==0&&frame%fps==fps/2)dt=.12;
        const double advance=std::min(std::min(dt,.06),6-sim-debt);wall+=dt;debt+=advance;frame++;
        if(fixed){
            const double step=1.0/fixed;
            while(debt>=step-1e-9&&sim<6-1e-8){sim+=step;rig.Step(scenario,float(step),sim);debt-=step;}
        }else{sim+=debt;rig.Step(scenario,float(debt),sim);debt=0;}
        Check(frame<100000,"bounded outer pacing loop");
    }
    const auto &m=rig.m;const auto &c=rig.car;
    std::fprintf(out,"%s,%d,%s,%d,%.9f,%.9f,%u,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%u,%.9f,%.9f,%.9f,%.9f,%d,%.9f,%.9f\n",
        scenario,fps,pacing,fixed,wall,sim,m.steps,m.distance,m.stop,DotProduct(c.m_vecMoveSpeed,c.matrix.forward)*50,
        m.maxRoll,m.maxPitch,m.minHeight,m.maxHeight,m.maxZSpeed,m.maxLoadSplit,m.contacts,m.maxDepth,m.maxImpulse,
        m.lateMax>=m.lateMin?m.lateMax-m.lateMin:0,c.matrix.position.z,m.inverted?1:0,m.maxAxleSplit,m.maxSpring);
}
int main(int argc,char **argv){
    if(argc==1){Smoke();return 0;}
    Check(argc==3&&std::strcmp(argv[1],"--matrix")==0,"matrix output path required");
    FILE *out=nullptr;Check(fopen_s(&out,argv[2],"wb")==0,"open measurements output");
    std::fputs("scenario,fps,pacing,fixed_hz,wall_s,sim_s,steps,distance_m,stop_s,final_forward_mps,max_roll_deg,max_pitch_deg,min_height_m,max_height_m,peak_vertical_mps,max_left_right_spring_split,body_contacts,max_depth_m,max_impulse_Ns,late_vertical_span_m,final_height_m,inverted,max_front_rear_spring_split,max_spring_compression\n",out);
    unsigned rows=0;
    for(const char *scenario:{"settle","brake","corner","curb","landing","tilted-landing","hard-landing","wall-impact"})
        for(int fps:{15,18,20,30,36,60,72,80,90,120,144})for(const char *pacing:{"steady","jitter","stall"})for(int fixed:{0,30,60,120,240}){
            Run(out,scenario,fps,pacing,fixed);rows++;
        }
    std::fclose(out);std::printf("PASS: %u coupled scenarios; %llu numerical checks; CSV: %s\n",rows,checks,argv[2]);
}
