#include "ragdoll-test-vector.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#define __ANDROID__
#define SQR(x) ((x)*(x))
constexpr float GRAVITY = 0.008f;
static float Abs(float x) { return std::fabs(x); }
static float Max(float a, float b) { return std::max(a,b); }
static float Pow(float a, float b) { return std::pow(a,b); }
struct CTimer {
    inline static float step = 50.0f/90;
    static float GetTimeStep() { return step; }
    static float GetTimeStepInSeconds() { return step/50; }
};
struct CPad {
    int accelerate=0, brake=0, lr=0, ud=0;
    static CPad *GetPad(int) { static CPad pad; return &pad; }
    int GetAccelerate() const { return accelerate; }
    int GetBrake() const { return brake; }
    int GetSteeringLeftRight() const { return lr; }
    int GetSteeringUpDown() const { return ud; }
    bool GetLookLeft() const { return false; }
    bool GetLookRight() const { return false; }
};
enum { MI_DODO, MI_CAR, STATUS_PLAYER, STATUS_PHYSICS,
       VEHICLE_TYPE_CAR, VEHICLE_TYPE_PLANE, FLIGHT_MODEL_RCPLANE, FLIGHT_MODEL_SEAPLANE };
struct Matrix {
    CVector right=CVector(1,0,0), forward=CVector(0,1,0), up=CVector(0,0,1);
};
static CVector Multiply3x3(const Matrix &m, const CVector &v) {
    return v.x*m.right+v.y*m.forward+v.z*m.up;
}
static CVector Multiply3x3(const CVector &v, const Matrix &m) {
    return CVector(DotProduct(v,m.right),DotProduct(v,m.forward),DotProduct(v,m.up));
}
struct CVehicle {
    int model=MI_DODO, status=STATUS_PLAYER, m_vehType=VEHICLE_TYPE_CAR;
    bool bEngineOn=true;
    float m_fHealth=1000, m_fMass=1000, m_fTurnMass=3000;
    CVector m_vecMoveSpeed, m_vecTurnSpeed, m_vecCentreOfMass, position, force, torque;
    Matrix matrix;
    struct ColModel { struct Box { CVector min=CVector(-2,-4,-1); } boundingBox; } col;
    int GetModelIndex() const { return model; }
    int GetStatus() const { return status; }
    CVector GetRight() const { return matrix.right; }
    CVector GetForward() const { return matrix.forward; }
    CVector GetUp() const { return matrix.up; }
    CVector GetMoveSpeed() const { return m_vecMoveSpeed; }
    CVector GetSpeed(const CVector &point) const { return m_vecMoveSpeed+CrossProduct(m_vecTurnSpeed,point); }
    CVector GetPosition() const { return position; }
    Matrix GetMatrix() const { return matrix; }
    ColModel *GetColModel() { return &col; }
    void ApplyMoveForce(const CVector &f) { force+=f; }
    void ApplyTurnForce(const CVector &f,const CVector &point) { torque+=CrossProduct(point,f); }
    void FixedWing(int flightModel);
    bool IsRealPlane();
};
#include "dodo-flight-production.inc"
static unsigned checks;
static void Check(bool ok,const char *message) {
    ++checks;
    if(!ok) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); }
}
static bool Near(float a,float b) { return std::fabs(a-b)<0.0001f*(1+std::fabs(b)); }
static bool Near(const CVector &a,const CVector &b) { return Near(a.x,b.x)&&Near(a.y,b.y)&&Near(a.z,b.z); }
static CVehicle Run(float speed=0.0f,int throttle=255,int lr=0,int ud=0) {
    *CPad::GetPad(0)={throttle,0,lr,ud};
    CVehicle v; v.m_vecMoveSpeed=CVector(0,speed,0); v.FixedWing(FLIGHT_MODEL_SEAPLANE); return v;
}
int main() {
    const CVehicle stationary=Run();
    Check(stationary.force.y>0,"propeller accelerates from rest");
    Check(stationary.force.z==0,"no hovering lift at rest");
    Check(std::isfinite(stationary.torque.x),"stationary flight has no division by zero");
    const CVehicle cruise=Run(0.85f);
    Check(cruise.force.z>GRAVITY*cruise.m_fMass*CTimer::step,"runway speed produces lift exceeding weight");
    Check(cruise.force.y>0,"engine can sustain lift-producing speed");
    Check(Run(-0.85f).force.z==0,"reverse taxiing does not create lift");
    Check(Run(0.85f,255,0,128).torque.x>0,"pull back pitches nose up");
    Check(Run(0.85f,255,0,-128).torque.x<0,"push forward pitches nose down");
    Check(Run(0.85f,255,128).torque.y>0,"right stick deflection banks right");
    Check(Run(0.85f,255,-128).torque.y<0,"left stick deflection banks left");
    *CPad::GetPad(0)={0,255,0,0};
    CVehicle braking; braking.m_vecMoveSpeed=CVector(0,0.85f,0); braking.FixedWing(FLIGHT_MODEL_SEAPLANE);
    Check(braking.force.y<cruise.force.y,"brake trigger reduces thrust for landing");
    for(int failure : {0,1,2}) {
        CVehicle a; a.m_vecMoveSpeed=CVector(0,0.85f,0);
        if(failure==0) a.status=STATUS_PHYSICS;
        if(failure==1) a.bEngineOn=false;
        if(failure==2) a.m_fHealth=0;
        CVehicle b=a;
        *CPad::GetPad(0)={255,0,128,128}; a.FixedWing(FLIGHT_MODEL_SEAPLANE);
        *CPad::GetPad(0)={0,255,-128,-128}; b.FixedWing(FLIGHT_MODEL_SEAPLANE);
        Check(Near(a.force,b.force)&&Near(a.torque,b.torque),"unpiloted or dead engine ignores pad input");
        Check(a.force.y<0&&a.force.z>0,"unpowered aircraft glides with drag and lift");
    }
    CVehicle high; high.position.z=201; high.m_vecMoveSpeed=CVector(0,0.85f,0); high.FixedWing(FLIGHT_MODEL_SEAPLANE);
    Check(high.force.z<GRAVITY*high.m_fMass*CTimer::step,"soft altitude ceiling limits climb");
    CVector baselineForce,baselineTorque;
    for(int fps : {30,72,80,90,120}) {
        CTimer::step=50.0f/static_cast<float>(fps);
        *CPad::GetPad(0)={255,0,0,0};
        CVehicle v; const float bank=0.4f;
        v.matrix.right=CVector(std::cos(bank),0,-std::sin(bank));
        v.matrix.up=CVector(std::sin(bank),0,std::cos(bank));
        v.m_vecMoveSpeed=CVector(0,0.85f,0); v.FixedWing(FLIGHT_MODEL_SEAPLANE);
        const CVector forcePerSecond=v.force*static_cast<float>(fps), torquePerSecond=v.torque*static_cast<float>(fps);
        if(fps==30) { baselineForce=forcePerSecond;baselineTorque=torquePerSecond; }
        Check(Near(forcePerSecond,baselineForce)&&Near(torquePerSecond,baselineTorque),"force and bank recovery have equal strength per second at Quest refresh rates");
    }
    CVehicle dodo,car,plane; car.model=MI_CAR;plane.model=MI_CAR;plane.m_vehType=VEHICLE_TYPE_PLANE;
    Check(dodo.IsRealPlane()&&!car.IsRealPlane()&&plane.IsRealPlane(),"Dodo avoids car interaction without reclassifying ordinary cars");
    // The alternate seaplane cheat retains its original behavior on other cars.
    CTimer::step=50.0f/30;
    *CPad::GetPad(0)={255,0,128,128};
    CVehicle cheat;cheat.model=MI_CAR;cheat.status=STATUS_PHYSICS;cheat.m_vecMoveSpeed=CVector(0,0.85f,0);
    cheat.FixedWing(FLIGHT_MODEL_SEAPLANE);
    Check(cheat.force.y>0&&cheat.torque.x>0,"other seaplane users retain controls");
    std::printf("Dodo flight: %u checks passed.\n",checks);
}
