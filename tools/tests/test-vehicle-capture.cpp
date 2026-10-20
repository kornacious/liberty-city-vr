#define _CRT_SECURE_NO_WARNINGS
#include "ragdoll-test-vector.h"
#include "../../overlay/src/vehicles/VehicleCaptureCore.h"
#include <filesystem>
#include <chrono>
#include <cstring>
#include <ctime>
#include <string>
#define __ANDROID__
#define LIBERTYVR_VEHICLE_CAPTURE 1
#include "../../overlay/src/vehicles/VehicleCapture.h"
static unsigned long long fakeNs=1000000000ULL;
#define CLOCK_MONOTONIC 0
static int clock_gettime(int,timespec *t){t->tv_sec=static_cast<time_t>(fakeNs/1000000000ULL);t->tv_nsec=long(fakeNs%1000000000ULL);return 0;}
static std::string captureRoot;
#define LCVR_VEHICLE_CAPTURE_ROOT captureRoot.c_str()
using uint64=unsigned long long;
enum {STATUS_PLAYER,STATUS_ABANDONED};
struct tHandlingData {float fTractionMultiplier=1,fTractionLoss=.8f,fBrakeDeceleration=.01f,fBrakeBias=.5f,fSuspensionForceLevel=1.5f,fSuspensionDampingLevel=.15f,fSuspensionBias=.5f;};
struct CMatrix {void UpdateRW(){}};
struct CColPoint {CVector point,normal;int surfaceB=0;};
class CVehicle {public: virtual ~CVehicle()=default;int status=STATUS_PLAYER,model=100;bool IsCar(){return true;}int GetStatus(){return status;}int GetModelIndex(){return model;}};
class CPhysical:public CVehicle {};
class CAutomobile:public CPhysical {
public:
    tHandlingData handling;tHandlingData *pHandling=&handling;CMatrix matrix;CVector pos,right=CVector(1,0,0),forward=CVector(0,1,0),up=CVector(0,0,1);
    CVector m_vecMoveSpeed,m_vecTurnSpeed,m_vecMoveFriction,m_vecTurnFriction,m_vecMoveSpeedAvg,m_vecTurnSpeedAvg,m_vecCentreOfMass;
    float m_fMass=1200,m_fTurnMass=2500,m_fGasPedal=0,m_fBrakePedal=0,m_fSteerAngle=0,m_fHealth=1000,m_fDamageImpulse=0;
    bool bIsHandbrakeOn=false,bIsStuck=false;unsigned char m_nStaticFrames=0,m_nCurrentGear=1;
    float m_aSuspensionSpringRatio[4]={},m_aSuspensionSpringRatioPrev[4]={},m_aWheelTimer[4]={};
    int m_aWheelState[4]={};CColPoint m_aWheelColPoints[4];CPhysical *m_aGroundPhysical[4]={};
    CVector &GetPosition(){return pos;}CVector &GetRight(){return right;}CVector &GetForward(){return forward;}CVector &GetUp(){return up;}
    CMatrix &GetMatrix(){return matrix;}bool GetIsStatic(){return false;}void UpdateRwFrame(){}void Teleport(CVector p){pos=p;}
};
static CAutomobile car;static CVehicle *selected=&car;static int handle=5;
static CVehicle *FindPlayerVehicle(){return selected;}
struct CPools {static CVehicle *GetVehicle(int ref){return ref==handle?selected:nullptr;}static int GetVehicleRef(CVehicle *){return handle;}};
struct CWorld {static bool bProcessCutsceneOnly;static void Remove(CVehicle *){}static void Add(CVehicle *){}};
bool CWorld::bProcessCutsceneOnly;
struct CReplay {static bool playing;static bool IsPlayingBack(){return playing;}};bool CReplay::playing;
struct CTimer {static float step;static unsigned frame;static float GetTimeStepInSeconds(){return step;}static float GetTimeStepNonClippedInSeconds(){return step;}static unsigned GetFrameCounter(){return frame;}};
float CTimer::step=1.0f/72;unsigned CTimer::frame;
#include "vehicle-capture-runtime.inc"
static unsigned checks;
static void Check(bool ok,const char *why){++checks;if(!ok){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}}
static FILE *Memory(const char *text){FILE *f=nullptr;tmpfile_s(&f);Check(f!=nullptr,"create tape fixture");std::fputs(text,f);std::rewind(f);return f;}
static VehicleCaptureCore::Initial MakeInitial(){VehicleCaptureCore::Initial s;s.model=100;s.pose[3]=s.pose[7]=s.pose[11]=1;return s;}
static void Core(){
    using namespace VehicleCaptureCore;
    Check(Label("brake-72")&&!Label("../assets")&&!Label("")&&!Label("a/b")&&!Label("bad;name"),"bounded safe file labels");
    FILE *f=nullptr;tmpfile_s(&f);auto initial=MakeInitial();Check(WriteHeader(f,initial),"write canonical header");
    Check(Write(f,{0,.3f,0,.1f,0})&&Write(f,{33333,0,1,-.2f,1}),"write bounded inputs");
    std::fputs("END,66666\n",f);std::rewind(f);Tape tape;Check(tape.Load(f),"load completed tape");std::fclose(f);
    Input s;Check(tape.At(0,s)&&s.gas==.3f,"first input active at time zero");
    Check(tape.At(33332,s)&&s.gas==.3f,"input is held until recorded boundary");
    Check(tape.At(33333,s)&&s.brake==1&&s.handbrake==1,"boundary selects new input");
    Check(!tape.At(0,s)&&!tape.At(66666,s),"backward seek and end are rejected");
    for(const char *bad:{"LCVR_INPUT_V2,100,0\n","LCVR_INPUT_V1,1,0,nan\n"}){f=Memory(bad);Tape t;Check(!t.Load(f),"invalid tape rejected");std::fclose(f);}
    for(const char *row:{"0,nan,0,0,0\nEND,1\n","0,2,0,0,0\nEND,1\n","1,0,0,0,0\nEND,2\n","0,0,0,0,0\n0,1,0,0,0\nEND,1\n","0,0,0,0,0\n","0,0,0,0,0\nEND,120000001\n","0,0,0,0,0junk\nEND,1\n"}){
        tmpfile_s(&f);WriteHeader(f,initial);std::fputs(row,f);std::rewind(f);Tape t;Check(!t.Load(f),"malformed, nonmonotonic or incomplete tape rejected");std::fclose(f);
    }
    auto bad=initial;bad.pose[3]=-1;Check(!Valid(bad),"left-handed transform rejected");
}
static void Command(const char *text){
    FILE *f=nullptr;fopen_s(&f,(captureRoot+"/control.txt").c_str(),"w");Check(f!=nullptr,"create command");std::fputs(text,f);std::fclose(f);fakeNs+=600000000ULL;
}
static void Frame(){androidgame::VehicleCaptureBeginFrame();androidgame::VehicleCaptureInputs(&car);androidgame::VehicleCaptureInputs(&car);androidgame::VehicleCaptureCollision(&car,2);androidgame::VehicleCaptureRetry(&car);androidgame::VehicleCaptureSubsteps(&car,3);androidgame::VehicleCaptureEndFrame();CTimer::frame++;fakeNs+=14000000ULL;}
static void Runtime(){
    captureRoot=(std::filesystem::current_path()/".cache"/("capture-host-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))).string();
    std::filesystem::create_directories(captureRoot);
    Frame();Check(std::filesystem::is_empty(captureRoot),"disabled runtime creates no files");
    car.m_fGasPedal=.5f;Command("record brake 1\n");for(int i=0;i<80;++i)Frame();
    FILE *f=nullptr;fopen_s(&f,(captureRoot+"/brake.inputs.csv").c_str(),"r");VehicleCaptureCore::Tape tape;
    Check(f&&tape.Load(f),"runtime creates a complete replayable tape");if(f)std::fclose(f);
    Check(tape.samples.size()>=71&&tape.samples.size()<=73,"retry input calls do not duplicate capture timestamps");
    car.pos=CVector(50,50,50);car.m_fGasPedal=0;Command("replay brake\n");Frame();
    Check(car.pos.MagnitudeSqr()==0&&car.m_fGasPedal==.5f,"replay restores pose and overrides native controls");
    Check(androidgame::mode==2,"replay active only after explicit command");
    handle++;Frame();Check(androidgame::mode==0,"pool generation change aborts stale target");
    Command("record brake 1\n");Frame();Check(androidgame::mode==0,"existing recording cannot be overwritten");
    car.model=101;Command("replay brake\n");Frame();Check(androidgame::mode==0,"wrong vehicle model rejected");car.model=100;
    Command("record paused 1\n");CTimer::step=0;Frame();Check(androidgame::samples==0&&androidgame::elapsed==0,"pause neither records nor advances replay time");
    CTimer::step=1.0f/72;Frame();Command("stop\n");Frame();Check(androidgame::mode==0,"explicit stop releases session");
    Command("record maximum 120\n");CTimer::step=.06f;
    for(int i=0;i<2005;++i)Frame();
    fopen_s(&f,(captureRoot+"/maximum.inputs.csv").c_str(),"r");VehicleCaptureCore::Tape maximum;
    Check(f&&maximum.Load(f)&&maximum.endUs==VehicleCaptureCore::MaxTimeUs,"maximum duration remains replayable after clipped-frame overshoot");if(f)std::fclose(f);
    CTimer::step=1.0f/72;Command("replay brake\n");Frame();
    car.handling.fTractionLoss=.7f;Frame();Check(androidgame::mode==0,"mid-replay handling changes abort");
    car.handling.fTractionLoss=.8f;
    Command("record exited 1\n");Frame();selected=nullptr;Frame();Check(androidgame::mode==0,"exiting the player vehicle ends capture");selected=&car;
    std::printf("Runtime test artifacts: %s\n",captureRoot.c_str());
}
int main(){Core();Runtime();std::printf("PASS: %u vehicle capture/replay checks\n",checks);}
