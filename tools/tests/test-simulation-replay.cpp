#define _CRT_SECURE_NO_WARNINGS
#include "ragdoll-test-vector.h"
#include "../../overlay/src/vehicles/SimulationReplayCore.h"
#include <filesystem>
#include <chrono>
#include <cstring>
#include <ctime>
#include <memory>
#include <string>
#include <sys/stat.h>
#define __ANDROID__
#include "../../overlay/src/vehicles/SimulationReplay.h"
static unsigned long long fakeNs=1000000000ULL;
#define CLOCK_MONOTONIC 0
static int clock_gettime(int,timespec *t){t->tv_sec=static_cast<time_t>(fakeNs/1000000000ULL);t->tv_nsec=long(fakeNs%1000000000ULL);return 0;}
static tm *gmtime_r(const time_t *t,tm *out){return gmtime_s(out,t)==0?out:nullptr;}
static std::string captureRoot;
#define LCVR_SIMULATION_REPLAY_ROOT captureRoot.c_str()
static int mkdir(const char *path,int){std::error_code ec;std::filesystem::create_directories(path,ec);return ec? -1:0;}
static int lstat(const char *p,struct stat *s){return stat(p,s);}
#define S_ISREG(mode) (((mode)&S_IFMT)==S_IFREG)
struct dirent {char d_name[256]={};};
struct DIR {std::vector<std::string> files;size_t index=0;dirent entry;};
static DIR *opendir(const char *path){
    if(!std::filesystem::is_directory(path)) return nullptr;
    DIR *d=new DIR;for(const auto &e:std::filesystem::directory_iterator(path)) d->files.push_back(e.path().filename().string());return d;
}
static dirent *readdir(DIR *d){if(d->index>=d->files.size())return nullptr;std::snprintf(d->entry.d_name,sizeof(d->entry.d_name),"%s",d->files[d->index++].c_str());return &d->entry;}
static int closedir(DIR *d){delete d;return 0;}
using uint8=unsigned char;
enum {STATUS_PLAYER,STATUS_ABANDONED};
struct tHandlingData {
    float fTractionMultiplier=1,fTractionLoss=.8f,fBrakeDeceleration=.01f,fBrakeBias=.5f,fSuspensionForceLevel=1.5f,fSuspensionDampingLevel=.15f,fSuspensionBias=.5f,fSteeringLock=30;
    float fTractionBias=.5f,fSuspensionUpperLimit=.2f,fSuspensionLowerLimit=-.2f,fBuoyancy=1,fCollisionDamageMultiplier=1;
    int bABS=0;unsigned Flags=0;
    struct {
        float fEngineAcceleration=.02f,fMaxVelocity=1.0f,fMaxCruiseVelocity=1,fMaxReverseVelocity=-.2f;
        int nNumberOfGears=5,nDriveType='R',nEngineType='P',Flags=0;
        struct {float fMaxVelocity=1,fShiftUpVelocity=.9f,fShiftDownVelocity=.5f;} Gears[6];
    } Transmission;
};
struct CMatrix {void UpdateRW(){}};
class CEntity {public: virtual ~CEntity()=default;int kind=0,model=0;int GetModelIndex(){return model;}bool IsPed(){return kind==1;}bool IsVehicle(){return kind==2;}bool IsObject(){return kind==3;}bool IsDummy(){return kind==4;}};
class CVehicle:public CEntity {public:CVehicle(){kind=2;model=100;}int status=STATUS_PLAYER;bool IsCar(){return true;}int GetStatus(){return status;}};
class CPed:public CEntity {public:CPed(){kind=1;}CVehicle *m_pMyVehicle=nullptr;};
class CAutomobile:public CVehicle {
public:
    tHandlingData handling;tHandlingData *pHandling=&handling;CMatrix matrix;CVector pos,right=CVector(1,0,0),forward=CVector(0,1,0),up=CVector(0,0,1);
    CVector m_vecMoveSpeed,m_vecTurnSpeed,m_vecMoveFriction,m_vecTurnFriction,m_vecMoveSpeedAvg,m_vecTurnSpeedAvg,m_vecCentreOfMass;
    float m_fMass=1200,m_fTurnMass=2500,m_fGasPedal=0,m_fBrakePedal=0,m_fSteerAngle=0,m_fHealth=1000,m_fChangeGearTime=0,m_fFireBlowUpTimer=0,m_fDamageImpulse=0;
    float m_aSuspensionSpringRatioPrev[4]={},m_aWheelSpeed[4]={};CAutomobile *m_aGroundPhysical[4]={};CVector m_aGroundOffset[4];
    bool bIsHandbrakeOn=false,bIsStuck=false,bHasContacted=false,bHasHitWall=false;unsigned char m_nStaticFrames=0,m_nCurrentGear=1;unsigned repairs=0;
    const CVector &GetPosition(){return pos;}void SetPosition(CVector p){pos=p;}CVector &GetRight(){return right;}CVector &GetForward(){return forward;}CVector &GetUp(){return up;}
    CMatrix &GetMatrix(){return matrix;}void UpdateRwFrame(){}void Teleport(CVector p){pos=p;}void Fix(){++repairs;}void ExtinguishCarFire(){}void ResetSuspension(){}void RemoveAndAdd(){}
};
static CAutomobile car;static CVehicle *selectedCar=&car;static int poolHandle=5;static CPed player;
static CVehicle *FindPlayerVehicle(){return selectedCar;}
static CPed *FindPlayerPed(){return &player;}
struct CPools {static CVehicle *GetVehicle(int ref){return ref==poolHandle?&car:nullptr;}static int GetVehicleRef(CVehicle *){return poolHandle;}};
struct CWorld {static bool bProcessCutsceneOnly;static void Remove(CVehicle *){}static void Add(CVehicle *){}};
bool CWorld::bProcessCutsceneOnly;
struct CReplay {static bool playing;static bool IsPlayingBack(){return playing;}};bool CReplay::playing;
struct CTimer {static float step;static float GetTimeStepInSeconds(){return step;}static void Stop(){}static void Update(){}};float CTimer::step=.01f;
struct CGame {static int currLevel;};int CGame::currLevel=0;
struct CTheZones {static int GetLevelFromPosition(const CVector *){return 2;}};
struct CCollision {static int loads;static void SortOutCollisionAfterLoad(){++loads;}};int CCollision::loads=0;
struct CStreaming {static int loads;static CVector loaded;static void LoadScene(const CVector &p){++loads;loaded=p;}};int CStreaming::loads=0;CVector CStreaming::loaded;
static bool IsStreetLight(int id){return id==2001;}
static bool IsTreeModel(int id){return id==2002;}
static bool IsFence(int id){return id==2003;}
struct CCarCtrl {static bool ThisRoadObjectCouldMove(int id){return id==2004 || id==2005;}};
namespace ModelSets {enum eModelCategory {World,Vegetation,Vehicles,Peds,Weapons};static int asset=0;static int GetActiveForCategory(eModelCategory){return asset;}}
namespace androidgame {
static int profilerMode=0;static unsigned profileStarts=0,profileStops=0;static std::string profileContext;
static bool captureAvailable=true;
static void QuestProfilerSetDisplayMode(int m){if(m && !profilerMode)++profileStarts;if(!m && profilerMode)++profileStops;profilerMode=m;}
static void QuestProfilerSetCaptureContext(const char *s){profileContext=s;}
static const char *QuestProfilerGetCaptureStem(){return captureAvailable?"test-profile":"";}
static bool QuestProfilerIsEnabled(){return profilerMode!=0;}
}
#include "simulation-replay-runtime.inc"
static unsigned checks=0;
static void Check(bool ok,const char *why){++checks;if(!ok){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}}
namespace androidgame {
enum {VR_MENU_WIDTH=1024,VR_MENU_HEIGHT=768};
enum {VR_DEBUG_WIDTH=512};
static uint8 gDebugPixels[512*64*4]={};
#define nil nullptr
struct Rect {int left,top,right,bottom;};
static std::vector<Rect> textRects,highlightRects;
static int menuSelection=0;
static int *CurrentMenuSelection(){return &menuSelection;}
static void FillVrMenuRect(int l,int t,int r,int b,uint8,uint8,uint8,uint8){if(l==85)highlightRects.push_back({l,t,r,b});}
static void DrawVrMenuText(const char *s,int x,int y,int scale,uint8,uint8,uint8){const int w=int(std::strlen(s))*6*scale;textRects.push_back({x-w/2,y,x+w/2,y+7*scale});}
static void DrawDebugText(const char *s,int x,int y,int scale,uint8 r,uint8 g,uint8 b){DrawVrMenuText(s,x,y,scale,r,g,b);}
#include "simulation-replay-menu.inc"
#include "simulation-replay-hud.inc"
static bool Overlap(Rect a,Rect b){return a.left<b.right&&b.left<a.right&&a.top<b.bottom&&b.top<a.bottom;}
static void LayoutTests(){
    for(int row=0;row<SimulationReplayMenuRows;++row){
        textRects.clear();highlightRects.clear();menuSelection=row;DrawQuestSimulationPage();
        Check(highlightRects.size()==1,"one selected replay row");
        for(size_t i=0;i<textRects.size();++i){
            const Rect a=textRects[i];Check(a.left>=0&&a.right<=1024&&a.top>=0&&a.bottom<=768,"replay menu text fits surface");
            for(size_t j=i+1;j<textRects.size();++j) Check(!Overlap(a,textRects[j]),"replay headings, rows and footers do not overlap");
            if(i<3 || i>=3+SimulationReplayMenuRows) Check(!Overlap(a,highlightRects[0]),"selected row does not cover heading or footer");
        }
    }
}
}
static FILE *Temporary(){FILE *f=nullptr;tmpfile_s(&f);Check(f!=nullptr,"temporary file");return f;}
static SimulationReplayCore::Initial MakeInitial(){SimulationReplayCore::Initial s;s.vehicle.model=100;s.vehicle.pose[3]=s.vehicle.pose[7]=s.vehicle.pose[11]=1;return s;}
static SimulationReplayCore::Sample MakeSample(uint64_t us){SimulationReplayCore::Sample s;s.input.us=us;s.forward[1]=1;return s;}
static void CoreTests(){
    namespace C=SimulationReplayCore;
    auto initial=MakeInitial();auto s=MakeSample(0);s.input.gas=.5f;
    FILE *f=Temporary();Check(C::Header(f,initial)&&C::Write(f,s),"valid tape header and first sample");
    s.input.us=10000;s.input.gas=1;s.position[0]=10;s.speed=50;
    Check(C::Write(f,s),"second sample");std::fputs("END,20000\n",f);std::rewind(f);C::Tape t;
    Check(t.Load(f),"completed tape loads");std::fclose(f);
    C::Sample out;Check(t.At(5000,out)&&out.input.gas==.5f&&out.position[0]==5&&out.speed==25,"hold inputs while interpolating route at different replay frame rate");
    C::RouteState route;Check(!t.Route(out,route),"implausible path velocity rejected by assistance");
    Check(t.At(10000,out)&&out.input.gas==1,"input boundary");Check(!t.At(0,out)&&!t.At(20000,out),"backward time and exact end rejected");
    f=Temporary();C::Header(f,initial);std::rewind(f);char header[1024];Check(std::fgets(header,sizeof(header),f)!=nullptr,"V2 header fixture");std::fclose(f);
    std::string legacy=header;legacy.replace(0,11,"LCVR_SIM_V1");legacy.erase(legacy.rfind(','));legacy+='\n';
    f=Temporary();std::fputs(legacy.c_str(),f);C::Write(f,MakeSample(0));std::fputs("END,10000\n",f);std::rewind(f);C::Tape old;
    Check(old.Load(f)&&old.initial.recordingGhost==-1,"legacy V1 demos remain readable with unknown recording ghost state");std::fclose(f);
    for(const char *row:{"0,nan,0,0,0,0,0,0,0,1,0,0\nEND,1\n","0,2,0,0,0,0,0,0,0,1,0,0\nEND,1\n","1,0,0,0,0,0,0,0,0,1,0,0\nEND,2\n","0,0,0,0,0,0,0,0,0,1,0,0\n0,0,0,0,0,0,0,0,0,1,0,0\nEND,2\n","0,0,0,0,0,0,0,0,0,1,0,0\n","0,0,0,0,0,0,0,0,0,1,0,0\nEND,120000001\n","0,0,0,0,0,0,0,0,0,1,0,0junk\nEND,1\n","0,0,0,0,0,0,0,0,0,1,0,0\nEND,1\ntrailing"}){
        f=Temporary();C::Header(f,initial);std::fputs(row,f);std::rewind(f);C::Tape bad;Check(!bad.Load(f),"malformed or unfinished tape rejected");std::fclose(f);
    }
    initial.gear=6;Check(!C::Valid(initial),"out-of-bounds transmission gear rejected");initial.gear=1;
    initial.vehicle.pose[3]=-1;Check(!C::Valid(initial),"reflected transform rejected");
    s=MakeSample(0);float p[]={3,4,0},forward[]={0,1,0};C::Drift drift;
    Check(drift.Add(s,p,forward,10)&&drift.Rms()==5&&drift.maxSpeed==10,"drift uses meters and kmh");p[0]=51;
    Check(!drift.Add(s,p,forward,0),"large drift abort threshold");
    p[0]=std::numeric_limits<float>::quiet_NaN();Check(!drift.Add(s,p,forward,0)&&drift.invalid,"nonfinite drift state is distinguished from ordinary divergence");
    C::Tape path;auto first=MakeSample(0),second=MakeSample(1000000);second.position[0]=-10;path.samples={first,second};path.endUs=2000000;
    Check(path.At(500000,out)&&path.Route(out,route)&&route.position[0]==-5&&std::fabs(route.move[0]+.2f)<.00001f,"route assistance derives signed world velocity from recorded positions");
    Check(std::fabs(route.right[0]-1)<.00001f&&std::fabs(route.up[2]-1)<.00001f,"route basis stays orthonormal");
    for(int a=0;a<2;++a)for(int b=0;b<2;++b)for(int da=0;da<2;++da)for(int db=0;db<2;++db){
        Check(C::IgnorePair(true,a!=0,b!=0,da!=0,db!=0)==C::IgnorePair(true,b!=0,a!=0,db!=0,da!=0),"ghost collision policy symmetric");
        Check(!C::IgnorePair(false,a!=0,b!=0,da!=0,db!=0),"ghost off always retains collision");
    }
}
static void Frame(bool after=true){androidgame::SimulationReplayBeginFrame();androidgame::SimulationReplayInputs(&car);androidgame::SimulationReplayInputs(&car);androidgame::SimulationReplayEndFrame();fakeNs+=10000000ULL;if(after)androidgame::SimulationReplayAfterAppFrame();}
static void Stop(){androidgame::SimulationReplayMenuAction(3,1);androidgame::SimulationReplayAfterAppFrame();}
static void Prepare(){for(int i=0;i<220 && androidgame::mode!=androidgame::Playing;++i)Frame();Check(androidgame::mode==androidgame::Playing,"preparation transitions to live physics playback");}
static void RuntimeTests(){
    using namespace androidgame;
    captureRoot=(std::filesystem::current_path()/".cache"/("simulation-host-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))).string();
    Frame();Check(!std::filesystem::exists(captureRoot),"idle runtime does not write files");
    Check(!SimulationReplayMenuAction(2,1)&&std::strstr(SimulationReplayStatus(),"NO RECORDED"),"missing playback keeps menu open with a useful message");
    Check(!SimulationReplayConsumeStartFailure(),"synchronous menu failure does not request a reopen");
    selectedCar=nullptr;Check(!SimulationReplayMenuAction(0,1)&&std::strstr(SimulationReplayStatus(),"ENTER AN AUTOMOBILE"),"on-foot start stays in menu with explanation");selectedCar=&car;
    SimulationReplayMenuAction(0,1);fakeNs+=4000000000ULL;SimulationReplayAfterAppFrame();Check(SimulationReplayConsumeStartFailure()&&std::strstr(SimulationReplayStatus(),"RESUME"),"paused gameplay cannot silently strand a queued start");
    Check(SimulationReplayMenuAction(0,1),"record queued from overlay");car.m_fGasPedal=.5f;
    SimulationReplayBeginFrame();char hud[128];Check(SimulationReplayRecordingText(hud,sizeof(hud))&&std::strcmp(hud,"0.0S RECORDED / 120.0S")==0,"recording HUD starts at zero with selected limit");
    CVehicle recordingTraffic;CPed recordingPed;CEntity scenery;
    Check(SimulationReplayIgnoreCollision(&car,&recordingTraffic)&&SimulationReplayIgnoreCollision(&recordingPed,&car)&&!SimulationReplayIgnoreCollision(&car,&scenery),"default ghost recording ignores traffic both ways and retains scenery");
    CEntity prop;prop.kind=3;
    Check(SimulationReplayIgnoreCollision(&car,&prop)&&SimulationReplayIgnoreCollision(&prop,&car)&&!SimulationReplayIgnoreCollision(&recordingTraffic,&prop),"recording ignores physical props both ways without altering ambient interactions");
    prop.kind=4;Check(SimulationReplayIgnoreCollision(&prop,&car),"dummy props are ghost obstacles too");
    prop.kind=0;for(int id:{2001,2002,2003}){prop.model=id;Check(SimulationReplayIgnoreCollision(&car,&prop),"static street lights, trees and fences are ignored");}
    prop.kind=3;for(int id:{2004,2005}){prop.model=id;Check(!SimulationReplayIgnoreCollision(&car,&prop),"moving road/bridge objects retain contact");}
    textRects.clear();Check(VrSimulationRecordingPixels()!=nullptr,"recording panel works with profiling off");
    Check(textRects.size()==2&&textRects[0].left>=0&&textRects[1].right<=512&&textRects[1].bottom<=64,"timer glyphs fit compact panel");
    for(int i=0;i<130;++i)Frame();Check(sampleCount==130,"one sample per simulation frame despite repeated input hook");
    Check(SimulationReplayRecordingText(hud,sizeof(hud))&&std::strstr(hud,"1.3S"),"recording HUD reports elapsed simulation time");
    CTimer::step=0;Frame();Check(sampleCount==130,"paused simulation does not record");CTimer::step=.01f;Stop();
    Check(mode==Idle&&!demos.empty(),"stopping records and discovers demo");
    Check(!SimulationReplayRecordingText(hud,sizeof(hud))&&!VrSimulationRecordingPixels(),"recording timer disappears at completion");
    FILE *f=std::fopen(Path(label+".sim.csv").c_str(),"r");SimulationReplayCore::Tape saved;
    Check(f&&saved.Load(f)&&saved.samples.size()==130&&saved.initial.recordingGhost==1,"saved V2 tape preserves recording ghost state");if(f)std::fclose(f);
    CVehicle ambient;CPed ped;CEntity building;player.m_pMyVehicle=&car;
    Check(!SimulationReplayIgnoreCollision(&car,&ambient),"ghost does not affect ordinary gameplay");
    SimulationReplayMenuAction(2,1);Frame();Check(mode==Preparing&&!profilerMode,"preparation excluded from automatic capture");
    Check(SimulationReplayIgnoreCollision(&car,&ambient)&&SimulationReplayIgnoreCollision(&ambient,&car)&&SimulationReplayIgnoreCollision(&ped,&car),"car and ped contacts ignored in both directions");
    prop.model=0;Check(SimulationReplayIgnoreCollision(&car,&prop)&&SimulationReplayIgnoreCollision(&prop,&car),"playback ghosts props symmetrically too");
    Check(!SimulationReplayIgnoreCollision(&car,&building)&&!SimulationReplayIgnoreCollision(&ambient,&ped),"terrain and ambient mutual collisions retained");
    Check(SimulationReplayIgnoreDamage(&car,&ped)&&SimulationReplayIgnoreDamage(&ped,&player)&&!SimulationReplayIgnoreDamage(&ped,&ambient),"damage isolated around protected replay group");
    Prepare();Check(profilerMode==2&&profileStarts==1&&profileContext.find("ghost=1")!=std::string::npos,"playback starts minimal capture with linked context");
    car.m_fGasPedal=0;Frame();Check(car.m_fGasPedal==.5f,"playback overrides live driving controls");
    while(mode==Playing)Frame(false);
    Check(mode==Finishing&&profilerMode==2,"final playback frame remains measured before app frame completion");
    SimulationReplayAfterAppFrame();Check(mode==Idle&&profilerMode==0&&profileStops==1,"after final app frame capture closes");
    Check(std::filesystem::exists(Path(runId+".run.csv")),"run report persisted");
    Check(!SimulationReplayProtected(&car)&&!SimulationReplayProtected(&player),"ghost fully clears after playback");
    SimulationReplayMenuAction(2,1);Frame();Prepare();QuestProfilerSetDisplayMode(0);Frame();
    Check(mode==Idle&&result.find("INTERRUPTED")!=std::string::npos,"manual profiler interruption marks playback aborted");
    SimulationReplayMenuAction(2,1);Frame();captureAvailable=false;
    for(int i=0;i<220 && mode!=Idle;++i)Frame();
    Check(mode==Idle&&result.find("COULD NOT START")!=std::string::npos,"capture creation failure aborts benchmark");captureAvailable=true;
    SimulationReplayMenuAction(9,1);SimulationReplayMenuAction(2,1);Frame();Prepare();car.pos.x=51;Frame();Check(mode==Idle&&result.find("DRIFT")!=std::string::npos,"input-only mode retains drift abort without corrections");car.pos.x=0;SimulationReplayMenuAction(9,1);
    SimulationReplayMenuAction(4,1);SimulationReplayMenuAction(5,1);SimulationReplayMenuAction(2,1);Frame();
    Check(!SimulationReplayIgnoreCollision(&car,&ambient),"ghost toggle respected");Prepare();Check(!profilerMode,"automatic capture can be disabled");Stop();
    car.model=101;SimulationReplayMenuAction(2,1);Frame();Check(mode==Idle&&SimulationReplayConsumeStartFailure()&&std::strstr(SimulationReplayStatus(),"CAR MODEL"),"wrong car model produces useful visible failure");car.model=100;
    ModelSets::asset=1;SimulationReplayMenuAction(2,1);Frame();Check(mode==Idle,"changed assets rejected");ModelSets::asset=0;
    car.handling.fTractionLoss=.7f;SimulationReplayMenuAction(2,1);Frame();Check(mode==Idle,"changed handling rejected");car.handling.fTractionLoss=.8f;
    car.pos.x=2000;SimulationReplayMenuAction(2,1);Frame();Check(mode==Preparing&&car.pos.x==0&&CStreaming::loads==1&&CCollision::loads==1&&CGame::currLevel==2,"distant playback loads target island and scene before teleporting");Stop();
    SimulationReplayMenuAction(2,1);Frame();++poolHandle;Frame();Check(mode==Idle,"pool generation change releases stale replay binding");
    SimulationReplayMenuAction(0,1);Frame();selectedCar=nullptr;Frame();Check(mode==Idle,"player exit stops recording");selectedCar=&car;
    SimulationReplayMenuAction(7,1);SimulationReplayMenuAction(0,1);Frame();Check(!SimulationReplayIgnoreCollision(&car,&recordingTraffic),"recording ghost can be disabled independently");
    const std::string activePath=Path(label+".sim.csv");SimulationReplayMenuAction(8,1);Check(std::filesystem::exists(activePath)&&mode==Recording,"deletion cannot disrupt active capture");Stop();
    SimulationReplayMenuAction(7,1);
    duration=120;CTimer::step=.2f;SimulationReplayMenuAction(0,1);for(int i=0;i<605;++i)Frame();
    f=std::fopen(Path(label+".sim.csv").c_str(),"r");SimulationReplayCore::Tape maximum;
    Check(f&&maximum.Load(f)&&maximum.endUs==SimulationReplayCore::MaxTimeUs,"maximum-duration recording remains valid after frame overshoot");if(f)std::fclose(f);
    const std::string maximumPath=Path(label+".sim.csv");const std::string reportPath=Path("keep.run.csv");f=std::fopen(reportPath.c_str(),"w");std::fputs("kept",f);std::fclose(f);
    SimulationReplayMenuAction(8,1);Check(std::filesystem::exists(maximumPath),"delete requires an explicit second press");
    SimulationReplayMenuAction(8,1);Check(!std::filesystem::exists(maximumPath)&&std::filesystem::exists(reportPath),"delete removes only selected tape and retains reports");
    std::printf("Runtime artifacts: %s\n",captureRoot.c_str());
}
static void LongRoute(){
    using namespace androidgame;
    namespace C=SimulationReplayCore;
    selectedCar=&car;car.m_vecMoveSpeed=CVector(0,0,0);car.m_vecTurnSpeed=CVector(0,0,0);
    auto start=Initial(&car);start.vehicle.pose[0]=100;start.vehicle.pose[1]=100;start.vehicle.pose[2]=20;
    const std::string name=Unique();FILE *f=std::fopen(Path(name+".sim.csv").c_str(),"wx");Check(f!=nullptr,"create 120-second route");C::Header(f,start);
    bool wrote=true;
    for(unsigned i=0;i<12000;++i){
        const float t=float(i)*.01f;auto s=MakeSample(uint64_t(i)*10000);
        s.position[0]=100+10*t;s.position[1]=100+20*std::sin(t*.25f);s.position[2]=20;
        s.forward[0]=10;s.forward[1]=5*std::cos(t*.25f);C::Normalize(s.forward);
        s.input.gas=.7f;s.input.steer=.2f*std::sin(t*.25f);s.speed=36;
        wrote=wrote&&C::Write(f,s);
    }
    Check(wrote,"write all route frames");
    std::fputs("END,120000000\n",f);std::fclose(f);Refresh();selectedDemo=int(std::find(demos.begin(),demos.end(),name)-demos.begin());
    ghost=routeAssistance=autoProfile=true;CTimer::step=.01f;SimulationReplayMenuAction(2,1);Frame();Prepare();
    const unsigned repairsAtStart=car.repairs;unsigned frames=0;
    while(mode==Playing && frames<30000){
        // Deliberately different step lengths, delayed acceleration/turning,
        // and large disturbances must not end assisted benchmark playback.
        const float steps[]={.007f,.013f,.020f,.008f};CTimer::step=steps[frames%4];
        car.pos.x+=frames%100==0?80.0f:.3f;car.forward=CVector(-1,0,0);car.m_vecMoveSpeed=CVector(0,0,0);
        Frame();++frames;
    }
    Check(mode==Idle&&result=="COMPLETE"&&elapsed>=120000000,"assisted playback completes the full 120 seconds under mixed frame times and large drift");
    Check(corrections>5000&&correctionMax>=79.0f&&drift.maxPosition>=79.0f,"correction counts and pre-correction divergence remain visible");
    Check(car.repairs==repairsAtStart&&!profilerMode,"assistance does not repeatedly repair the car and closes profiling normally");
    f=std::fopen(Path(runId+".run.csv").c_str(),"r");char report[4096]={};Check(f!=nullptr,"assisted report persisted");std::fread(report,1,sizeof(report)-1,f);std::fclose(f);
    Check(std::strstr(report,"tracking,route_assisted")&&std::strstr(report,"route_corrections,")&&std::strstr(report,"estimated_path_velocity"),"assisted captures cannot be mistaken for unmodified simulation replay");
}
static void ExternalTape(const char *path){
    FILE *f=std::fopen(path,"r");SimulationReplayCore::Tape input;
    Check(f&&input.Load(f),"external replay tape loads");if(f)std::fclose(f);
    unsigned frames=0;bool valid=true;
    for(uint64_t time=0;time<input.endUs;){
        SimulationReplayCore::Sample s;SimulationReplayCore::RouteState r;
        if(!input.At(time,s)||!input.Route(s,r)){valid=false;break;}
        const unsigned steps[]={5000,11000,17000,7000};time+=steps[frames%4];++frames;
    }
    Check(valid,"external tape supports route assistance over its entire duration at mixed frame rates");
    std::printf("External tape: %.3f seconds, %zu source samples, %u route frames validated.\n",double(input.endUs)/1000000,input.samples.size(),frames);
}
int main(int argc,char **argv){CoreTests();RuntimeTests();LongRoute();androidgame::LayoutTests();for(int i=1;i<argc;++i)ExternalTape(argv[i]);std::printf("PASS: %u simulation replay checks\n",checks);}
