#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include "vehicle-seat-recenter-helper.inc"
#define nil nullptr
#define GTA_VR_WEAPONS 1
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#define ALOG(...) ((void)0)
namespace VrBodyIK {static bool enabled=true; static bool Enabled(){return enabled;} static void SetVehicleTransition(bool){} static void ResetVehicleTransition(){}}
using float32 = float;
using bool32 = int;
using int32 = int32_t;
using uint32 = uint32_t;
using PedState = int;
static unsigned checks, recenters;
static void Check(bool ok, const char *message) { ++checks; if(!ok) throw std::runtime_error(message); }
static bool Near(float a,float b) { return std::fabs(a-b)<0.00002f; }

namespace rw { namespace vulkan {
struct State {
 bool32 firstPersonActive=0, temporalHistoryValid=0, fpFollowHeading=0, fpUseFullBasis=0;
 float headPosition[3]={}, headQuat[4]={0,0,0,1}, headYaw=0, fpLatchedHeadYaw=0, fpAnchorYaw=0;
 float fpLatchedHeadPos[3]={}, fpHeadWorld[3]={}, fpPlayX[3]={},fpPlayY[3]={},fpPlayZ[3]={};
} gvk;
static void resetAtomicMotionHistory() {}
void setFirstPersonAnchor(const float32[3], float32, bool32, bool32, bool32 = false);
void setFirstPersonAnchorBasis(const float32[3], const float32[3], const float32[3], const float32[3], float32, bool32, bool32 = false);
} }
#include "vehicle-seat-recenter-backend.inc"
struct CVector {
 float x=0,y=0,z=0;
 CVector()=default; CVector(float a,float b,float c):x(a),y(b),z(c){}
 CVector operator+(CVector b)const{return {x+b.x,y+b.y,z+b.z};}
 CVector operator-(CVector b)const{return {x-b.x,y-b.y,z-b.z};}
 CVector operator*(float f)const{return {x*f,y*f,z*f};}
 CVector &operator+=(CVector b){x+=b.x;y+=b.y;z+=b.z;return *this;}
 void Normalise(){float n=std::sqrt(x*x+y*y+z*z);x/=n;y/=n;z/=n;}
};
static float Dot(CVector a,CVector b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static CVector CrossProduct(CVector a,CVector b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
struct CMatrix {
 CVector position,right={1,0,0},forward={0,1,0},up={0,0,1};
 CVector &GetPosition(){return position;}
 CVector operator*(CVector v)const{return position+right*v.x+forward*v.y+up*v.z;}
};
static CVector Multiply3x3(const CMatrix &m,CVector v){return m.right*v.x+m.forward*v.y+m.up*v.z;}
static CMatrix Invert(const CMatrix &m){
 CMatrix r;r.right={m.right.x,m.forward.x,m.up.x};r.forward={m.right.y,m.forward.y,m.up.y};r.up={m.right.z,m.forward.z,m.up.z};
 r.position={-Dot(m.right,m.position),-Dot(m.forward,m.position),-Dot(m.up,m.position)};return r;
}
enum { TRAIN_POS_LEFT_ENTRY, TRAIN_POS_MID_ENTRY, TRAIN_POS_RIGHT_ENTRY };
enum { WRIST_CONTEXT_DEFAULT, WRIST_CONTEXT_BIKE, WRIST_CONTEXT_CAR };
// Synthetic interior markers deliberately use the negative boarding side.
struct CVehicleModelInfo { CVector m_positions[3]={{-.8f,-2,1.5f},{-.8f,0,1.5f},{-.8f,2,1.5f}}; } trainModel;
struct CModelInfo { static void *GetModelInfo(int){return &trainModel;} };
struct CTimer {
 static unsigned frame; static bool GetIsPaused(){return false;}
 static float GetTimeStep(){return 1;} static unsigned GetFrameCounter(){return frame;}
}; unsigned CTimer::frame=0;
struct VrKnockdownView {static float Wrap(float a){return std::remainder(a,6.283185307f);}};
static float Atan2(float y,float x){return std::atan2(y,x);}
static float Cos(float x){return std::cos(x);} static float Sin(float x){return std::sin(x);}
template<typename T> static T clamp(T x,T a,T b){return x<a?a:(x>b?b:x);}
struct CEntity {};
struct CPlayerPed;
struct CVehicle {
 CPlayerPed *pDriver=nullptr,*pPassengers[8]={}; int model=175;
 bool train=false; CMatrix matrix;
 bool IsTrain(){return train;} CMatrix &GetMatrix(){return matrix;}
 bool IsBike(){return false;}
 CVector GetUp(){return matrix.up;}
 int GetModelIndex(){return model;} CVector GetForward(){return matrix.forward;}
};
struct CPlayerPed:CEntity {
 bool inVehicle=false,dead=false,bIsDucking=false; int state=0,m_objective=0;
 CVehicle *m_pMyVehicle=nullptr; void *m_rwObject=this,*m_pFrames[64]={};
 CVector position,forward={0,1,0};
 CVector GetPosition(){return position;} CVector GetForward(){return forward;}
 bool InVehicle(){return inVehicle;} int GetPedState(){return state;} bool DyingOrDead(){return dead;}
 void *GetClump(){return m_rwObject;}
};
namespace ModelSets {static bool modern=false;static bool IsModernActive(){return modern;}}
static bool IsClumpSkinned(void*){return true;}
static bool testKnockdown;
static bool PlayerKnockedDown(CPlayerPed*){return testKnockdown;}
static CPlayerPed *currentPlayer;
static bool trainNeckReady=false; static CVector trainNeck;
static bool GetAnimatedPedHeadPose(CEntity*,CVector *head,CVector *forward){if(!trainNeckReady)return false;*head=trainNeck;*forward={0,1,0};return true;}
static CPlayerPed *FindPlayerPed(){return currentPlayer;}
static CVehicle *FindPlayerVehicle(){return currentPlayer?currentPlayer->m_pMyVehicle:nullptr;}
struct PlayerInfo { bool remote=false; bool IsPlayerInRemoteMode(){return remote;} };
struct CWorld { static PlayerInfo Players[1]; static int PlayerInFocus; };
PlayerInfo CWorld::Players[1]; int CWorld::PlayerInFocus;
struct { bool m_bMenuActive=false,m_bGameNotLoaded=false,m_bWantToRestart=false,m_bWantToLoad=false; } FrontEndMenuManager;
struct CGame { static bool playingIntro; }; bool CGame::playingIntro;
struct CCutsceneMgr { static bool running,processing; static bool IsRunning(){return running;} static bool IsCutsceneProcessing(){return processing;} };
bool CCutsceneMgr::running, CCutsceneMgr::processing;
struct { bool m_WideScreenOn=false; } TheCamera;
static constexpr int GS_PLAYING_GAME=1;
static int gGameState=GS_PLAYING_GAME;
static CEntity *gCutsceneCameraActor,*gVrPlayerEntity;
static bool gVrFirstPersonActive,gVrInVehicle;
static VrVehicleSeatRecenter gVehicleSeatRecenter;
namespace androidgame {
static bool knockdownLock=true;
static bool VrXboxKnockdownLockEnabled(){return knockdownLock;}
static bool vehicleBody=true;
bool VrVehicleBodyIKEnabled(){return vehicleBody;}
static bool vehiclePanels=false;
bool VrWristPanelsInVehicle(){return vehiclePanels;}
static bool menu=false; static int cutsceneMode=0;
bool VrMenuConsumesInput(){return menu;} int VrCutsceneMode(){return cutsceneMode;}
void VrRecenterView();
}
namespace OculusVR {
bool IsQuestTrainPassenger();
bool IsQuestTrainCinematic();
static bool hands=true,immersive=false,gVehicleThirdPerson[1]={};
bool AreTrackedHandsEnabled(){return hands;}
bool IsImmersiveDrivingActive(){return immersive;}
static bool thirdPerson=false;
bool IsQuestVehicleThirdPerson(){return thirdPerson;}
void ResetQuestDrivingInteraction(){++recenters;}
struct DefaultVehicleViewOffset { int seatDistanceCm=22,seatHeightCm=31; };
struct VehicleViewCalibration { int defaultSeatHeightCm=7,defaultSeatDistanceCm=-3,seatHeightCm=11,seatDistanceCm=-8; };
struct VehicleCategoryCalibration { int seatHeightCm=25,seatDistanceCm=19; };
static DefaultVehicleViewOffset gDefaultVehicleViewOffset[1];
static VehicleViewCalibration modelCalibration;
static VehicleCategoryCalibration categoryCalibration;
static int defaultView=0;
static void LoadDrivingSettings(){}
static float PlacementValue(int, const char *, float fallback){return fallback;}
static int GetVehicleCategory(CVehicle*){return 0;}
static int VehicleKindFor(CVehicle*){return 0;}
static int GetDefaultVehicleViewTypeForCurrentVehicle(){return defaultView;}
static VehicleViewCalibration *GetViewCalibration(int){return &modelCalibration;}
static VehicleCategoryCalibration *GetCategoryCalibration(CVehicle*){return &categoryCalibration;}
}
namespace xrvk {
using XrViewStateFlags=unsigned;
static constexpr unsigned XR_VIEW_STATE_POSITION_VALID_BIT=2,XR_VIEW_STATE_ORIENTATION_VALID_BIT=1;
static constexpr unsigned XR_VIEW_STATE_POSITION_TRACKED_BIT=8,XR_VIEW_STATE_ORIENTATION_TRACKED_BIT=4;
static constexpr int XR_SESSION_STATE_FOCUSED=5;
struct ViewState { unsigned viewStateFlags=15; };
struct { bool headPoseTrackedForFrame=false,theaterMode=false; int sessionState=XR_SESSION_STATE_FOCUSED; } g;
}
#define XR_SUCCEEDED(result) ((result)>=0)
// The native offset function has a pre-existing unused categoryIndex local.
#pragma warning(push)
#pragma warning(disable:4189)
#include "vehicle-seat-recenter-production.inc"
#pragma warning(pop)

static void Head(float x,float y,float z,float yaw=0) {
 const float pos[3]={x,y,z},quat[4]={0,std::sin(yaw*.5f),0,std::cos(yaw*.5f)};
 rw::vulkan::setHeadPose(pos,yaw,quat);
}
static void Anchor() {
 CMatrix m;m.position={10,20,1.4f};OculusVR::ApplyQuestVehicleViewOffset(&m);
 const float right[3]={1,0,0},up[3]={0,0,1},forward[3]={0,1,0};
 if(gVrInVehicle)rw::vulkan::setFirstPersonAnchorBasis(&m.position.x,right,up,forward,1.5707963268f,1);
 else rw::vulkan::setFirstPersonAnchor(&m.position.x,1.5707963268f,1,1);
}
static CVector Eye() {
 float r[3],u[3],f[3];CVector p;
 Check(rw::vulkan::getFirstPersonViewFrame(r,u,f,&p.x)!=0,"native view exists");return p;
}
static void Reset(CPlayerPed &p,CVehicle &car) {
 p={};car={};currentPlayer=&p;p.state=PED_DRIVING;p.m_objective=OBJECTIVE_NONE;p.m_pFrames[PED_HEAD]=&p;
 p.m_pMyVehicle=&car;p.inVehicle=true;car.pDriver=&p;
 gVehicleSeatRecenter.Reset();recenters=0;rw::vulkan::gvk={};gVrInVehicle=false;
 gGameState=GS_PLAYING_GAME;FrontEndMenuManager={};CGame::playingIntro=false;
 CCutsceneMgr::running=CCutsceneMgr::processing=false;TheCamera.m_WideScreenOn=false;
 CWorld::Players[0].remote=false;androidgame::menu=false;OculusVR::thirdPerson=false;
 xrvk::g={};xrvk::UpdateTracking(0,2,{});Head(0,1.7f,0);Anchor();
}
static void Tick(bool post=true) { EvaluateSeat(post);Anchor(); }

static void EntryAndMotion() {
 CPlayerPed p;CVehicle car;Reset(p,car);
 p.inVehicle=false;car.pDriver=nullptr;p.state=PED_ENTER_CAR;p.m_objective=OBJECTIVE_ENTER_CAR_AS_DRIVER;
 for(int i=0;i<16;i++){Head(0,1.7f,0);Tick(false);Tick();}
 Check(recenters==0,"no new recenter during native entry animation");
 // Native PedSetInCarCB enables occupancy at completion. The XR renderer
 // still accepts orientation-only frames, so its automatic basis latch can
 // capture a stale physical position at exactly that completed-seat frame.
 p.state=PED_DRIVING;p.m_objective=OBJECTIVE_NONE;p.inVehicle=true;car.pDriver=&p;
 xrvk::UpdateTracking(0,2,{xrvk::XR_VIEW_STATE_ORIENTATION_VALID_BIT});Tick();
 Check(recenters==0,"completed seat with stale XR position stays pending");
 xrvk::UpdateTracking(0,2,{});
 Head(.35f,1.42f,-.18f,.31f);Anchor();
 const CVector old=Eye();
 Check(std::fabs(old.x-10)>.3f,"actual old backend retains stale-entry lateral offset after tracking recovers");
 Tick(false);Check(recenters==0,"pre-physics does not latch seat");
 Tick();Check(recenters==1,"first completed post-physics seat recenters once");
 CVector eye=Eye();Check(Near(eye.x,10)&&Near(eye.y,20.19f)&&Near(eye.z,1.78f),"all physical entry displacement removed, custom default offsets preserved");
 Check(Near(rw::vulkan::gvk.fpLatchedHeadYaw,.31f),"actual current head facing is neutral forward");
 for(int i=0;i<240;i++){
  Head(.35f+.001f*i,1.42f,-.18f,.31f+.001f*i);Tick(false);Tick();
  Check(recenters==1,"steady driving never repeats recenter or releases wheel grabs");
 }
 eye=Eye();Check(std::fabs(eye.x-10)>.2f,"physical leaning still displaces native rendered view");
 androidgame::menu=true;Tick();androidgame::menu=false;Tick();
 xrvk::g.headPoseTrackedForFrame=false;Tick();xrvk::UpdateTracking(0,2,{});Tick();
 Check(recenters==1,"menus and tracking recovery do not rearm an already centered seat");
 androidgame::VrRecenterView();Anchor();Tick();
 Check(recenters==2,"manual recenter is respected without a second automatic reset");
 Check(OculusVR::gDefaultVehicleViewOffset[0].seatHeightCm==31&&OculusVR::modelCalibration.defaultSeatDistanceCm==-3,"saved offset values never overwritten");
 OculusVR::defaultView=-1;Anchor();eye=Eye();
 Check(Near(eye.y,20.11f)&&Near(eye.z,1.76f),"immersive category and model offsets remain additive");
 OculusVR::defaultView=0;
}

static void SuppressionAndLifecycle() {
 CPlayerPed p;CVehicle car;
 for(int guard=0;guard<16;guard++){
  Reset(p,car);
  if(guard==0)FrontEndMenuManager.m_bMenuActive=true;
  if(guard==1)FrontEndMenuManager.m_bGameNotLoaded=true;
  if(guard==2)FrontEndMenuManager.m_bWantToRestart=true;
  if(guard==3)FrontEndMenuManager.m_bWantToLoad=true;
  if(guard==4)CGame::playingIntro=true;
  if(guard==5)CCutsceneMgr::running=true;
  if(guard==6)CCutsceneMgr::processing=true;
  if(guard==7)TheCamera.m_WideScreenOn=true;
  if(guard==8)CWorld::Players[0].remote=true;
  if(guard==9)OculusVR::thirdPerson=true;
  if(guard==10)androidgame::menu=true;
  if(guard==11)p.m_rwObject=nullptr;
  if(guard==12)p.m_pFrames[PED_HEAD]=nullptr;
  if(guard==13)p.dead=true;
  if(guard==14)gGameState=0;
  if(guard==15)xrvk::g.theaterMode=true;
  for(int i=0;i<4;i++){Tick(false);Tick();}
  Check(recenters==0,"unsafe/non-seat frame never consumes pending center");
  FrontEndMenuManager={};CGame::playingIntro=false;CCutsceneMgr::running=CCutsceneMgr::processing=false;
  TheCamera.m_WideScreenOn=false;CWorld::Players[0].remote=false;OculusVR::thirdPerson=false;
  androidgame::menu=false;p.m_rwObject=&p;p.m_pFrames[PED_HEAD]=&p;p.dead=false;gGameState=GS_PLAYING_GAME;xrvk::g.theaterMode=false;
  Tick();Tick();Check(recenters==1,"postponed entry captures exactly once when safe");
 }
 for(int state:{PED_OPEN_DOOR,PED_CARJACK,PED_ENTER_CAR,PED_STEAL_CAR,PED_EXIT_CAR,PED_DRAG_FROM_CAR,PED_ENTER_TRAIN,PED_EXIT_TRAIN}){
  Reset(p,car);p.state=state;Tick();Check(recenters==0,"all native transition states defer recenter");
 }
 Reset(p,car);car.pDriver=nullptr;Tick();Check(recenters==0,"InVehicle without registered seat is not enough");
 car.pPassengers[7]=&p;Tick();Check(recenters==1&&gVehicleSeatRecenter.seat==8,"passenger seat is recognized");
 car.pPassengers[7]=nullptr;car.pDriver=&p;Tick();Check(recenters==2&&gVehicleSeatRecenter.seat==0,"seat change in same car recenters");
 p.inVehicle=false;p.state=PED_IDLE;Tick();p.inVehicle=true;p.state=PED_DRIVING;Tick();Check(recenters==3,"leave and reenter same car rearms");
 CVehicle other;other.pDriver=&p;p.m_pMyVehicle=&other;Tick();Check(recenters==4,"different vehicle rearms");
 CPlayerPed replacement=p;replacement.m_pMyVehicle=&other;other.pDriver=&replacement;currentPlayer=&replacement;Tick();Check(recenters==5,"new player identity rearms");
 currentPlayer=nullptr;EvaluateSeat(true);Check(!gVehicleSeatRecenter.centered,"missing player clears pending state without dereference");
 currentPlayer=&replacement;Tick();Check(recenters==6,"player returning already seated centers once");
 gVehicleSeatRecenter.Reset();Tick();Check(recenters==7,"restart reset supports address reuse on load/new game");
}

static void TrackingTruthTable() {
 CPlayerPed p;CVehicle car;
 for(unsigned mask=0;mask<16;mask++)for(unsigned views=0;views<4;views++)for(int focused=0;focused<2;focused++)for(int success=0;success<2;success++){
  Reset(p,car);xrvk::g.sessionState=focused?xrvk::XR_SESSION_STATE_FOCUSED:0;
  xrvk::UpdateTracking(success?0:-1,views,{mask});
  const bool expected=mask==15&&views==2&&focused&&success;
  Check(xrvk::hasTrackedGameplayHeadPose()==expected,"exact native flags/view/focus/result predicate");
  Tick();Check((recenters==1)==expected,"invalid or stale XR position cannot be latched into seat");
  xrvk::g.sessionState=xrvk::XR_SESSION_STATE_FOCUSED;xrvk::UpdateTracking(0,2,{});Tick();
  Check(recenters==1,"first fully tracked frame consumes pending entry once");
 }
}

static void TrainPassengerMotion() {
 CPlayerPed p;CVehicle train;Reset(p,train);train.train=true;
 CVector head,forward;
 p.inVehicle=false;p.state=PED_ENTER_TRAIN;
 Check(OculusVR::IsQuestTrainPassenger(),"boarding is already a passenger context");
 OculusVR::gVehicleThirdPerson[0]=true;
 Check(!OculusVR::ProductionVehicleThirdPerson(),"train ignores third-person driving preference");
 Check(OculusVR::ShouldUseTrackedHands(),"boarding hands work in default driving mode");
 EvaluateSeat(true);EvaluateTrain(true,head,forward);
 Head(.35f,1.7f,-.2f,.3f);
 // Boarding uses the yaw anchor. Finishing boarding changes to a carriage
 // basis without discarding real movement made during the animation.
 const float boardingEye[3]={2,-1,1};
 rw::vulkan::setFirstPersonAnchor(boardingEye,1.5707963268f,1,1,true);
 const float latchedX=rw::vulkan::gvk.fpLatchedHeadPos[0];
 p.inVehicle=true;p.state=PED_DRIVING;train.pDriver=nullptr;train.pPassengers[0]=&p;
 p.position={20,-1,0};p.forward={-1,0,0};
 auto publish=[&](bool post=true){
  EvaluateSeat(post);EvaluateTrain(post,head,forward);
  CVector right={forward.y,-forward.x,0};
  rw::vulkan::setFirstPersonAnchorBasis(&head.x,&right.x,&train.matrix.up.x,&forward.x,
   std::atan2(forward.y,forward.x),1,true);
  rw::vulkan::setFirstPersonPositionLocked(true);
 };
 // A pre-physics sample must not commit the still-unsettled occupant pose.
 publish(false);p.position={3,-2,0};publish();
 Check(Near(head.x,-.87f)&&Near(head.y,-1.98f)&&Near(head.z,2.42f),"fallback eye includes forward/right/up neck clearance");
 Check(VrBodyIK::VehicleEnabled(),"interior train enables normal body IK with tracked hands");
#ifdef TEST_TRAIN_HUD
 Check(VrWristPanelsRideAlong(),"interior train permits immersive wrist panels without driving mode");
 Check(ActiveWristContext()==WRIST_CONTEXT_DEFAULT,"interior train uses normal wrist calibration rather than dashboard calibration");
#endif
 VrBodyIK::enabled=false;Check(!VrBodyIK::VehicleEnabled(),"train respects normal body IK setting");VrBodyIK::enabled=true;
 Check(recenters==0,"train entry bypasses automatic car-seat recenter");
 Check(OculusVR::ShouldUseTrackedHands(),"riding hands work in default driving mode");
 OculusVR::hands=false;Check(!OculusVR::ShouldUseTrackedHands()&&!VrBodyIK::VehicleEnabled(),"passenger respects disabled hands setting");OculusVR::hands=true;
 Check(!Near(rw::vulkan::gvk.fpLatchedHeadPos[0],latchedX),"interior position lock absorbs earlier physical displacement");
 CVector baseEye=Eye();
 auto palm=[](float x){
  const float pos[3]={x,1.3f,-.4f},quat[4]={0,0,0,1};float r[3],u[3],f[3];CVector out;
  Check(rw::vulkan::playPoseToFirstPersonWorld(pos,quat,r,u,f,&out.x)!=0,"controller world pose exists");return out;
 };
 CVector basePalm=palm(.6f);
 const CVector localEye=baseEye,localPalm=basePalm;
 for(int i=0;i<240;i++){
  ++CTimer::frame;
  float angle=.003f*i;
  train.matrix.right={std::cos(angle),std::sin(angle),0};
  train.matrix.forward={-std::sin(angle),std::cos(angle),0};
  train.matrix.position={.1f*i,-.03f*i,.005f*i};
  // Neither native body placement nor train speed/camera changes may drag
  // the cached VR passenger around inside the cabin.
  p.position={50,40,30};p.forward={1,0,0};publish(false);publish();
  CVector expectedEye=train.matrix*localEye,expectedPalm=train.matrix*localPalm;
  CVector eye=Eye(),hand=palm(.6f);
  Check(Near(eye.x,expectedEye.x)&&Near(eye.y,expectedEye.y)&&Near(eye.z,expectedEye.z),"eyes ride carriage translation and turns");
  Check(Near(hand.x,expectedPalm.x)&&Near(hand.y,expectedPalm.y)&&Near(hand.z,expectedPalm.z),"hands share carriage frame with eyes");
 }
 CVector before=palm(.6f),after=palm(1.2f);
 Check(Near(std::sqrt(Dot(after-before,after-before)),.6f),"controller motion remains one to one without arm reach limits");
 CVector beforeLean=Eye();Head(.55f,1.8f,-.2f,.5f);publish();
 CVector lean=Eye()-beforeLean;
 Check(Near(Dot(lean,lean),0),"physical translation cannot move the interior eyes");
 Check(Near(rw::vulkan::gvk.headYaw,.5f),"headset rotation remains live with position locked");
 OculusVR::UpdateQuestTrainCamera(false,false);
 OculusVR::UpdateQuestTrainCamera(true,false);
 Check(OculusVR::IsQuestTrainCinematic(),"R3 selects cinematic view");
 Check(!VrBodyIK::VehicleEnabled()&&!OculusVR::IsQuestTrainInterior(),"cinematic view restores native body and head ownership");
#ifdef TEST_TRAIN_HUD
 Check(!VrWristPanelsRideAlong(),"cinematic train retains normal flat HUD fallback");
#endif
 OculusVR::UpdateQuestTrainCamera(true,false);
 Check(OculusVR::IsQuestTrainCinematic(),"holding R3 does not toggle repeatedly");
 OculusVR::UpdateQuestTrainCamera(false,false);OculusVR::UpdateQuestTrainCamera(true,false);
 Check(!OculusVR::IsQuestTrainCinematic(),"second R3 press returns to interior");
 OculusVR::UpdateQuestTrainCamera(false,false);OculusVR::UpdateQuestTrainCamera(true,true);
 Check(!OculusVR::IsQuestTrainCinematic(),"menu consumes R3 without toggling camera");
 p.state=PED_EXIT_TRAIN;EvaluateTrain(true,head,forward);
 Check(OculusVR::IsQuestTrainPassenger(),"alighting retains passenger hands");
 p.inVehicle=false;p.state=PED_IDLE;EvaluateTrain(true,head,forward);
 Check(!OculusVR::IsQuestTrainPassenger(),"stale vehicle pointer after exit does not retain passenger mode");
 p.inVehicle=true;p.state=PED_DRIVING;p.position={10,20,1};publish();
 CVector expected=train.matrix*CVector(-.8f,-2,2.38f)+train.matrix.up*.04f+forward*.07f+CrossProduct(forward,train.matrix.up)*.02f;
 Check(Near(head.x,expected.x)&&Near(head.y,expected.y)&&Near(head.z,expected.z),"reboarding restores authored interior slot");
 // Start facing along the carriage and verify a slow one-time outward turn.
 p.inVehicle=false;p.state=PED_IDLE;EvaluateTrain(true,head,forward);
 train.matrix={};p.inVehicle=true;p.state=PED_DRIVING;p.forward={0,1,0};
 ++CTimer::frame;publish();float first=Atan2(forward.y,forward.x);
 Check(first>1.5708f&&first<1.6f,"boarding turn starts gradually toward negative-X windows");
 publish();Check(Near(Atan2(forward.y,forward.x),first),"multiple camera updates cannot double turn rate");
 for(int i=0;i<13;i++){++CTimer::frame;publish();}
 Check(forward.y>.001f,"boarding turn is still progressing before 300 ms");
 ++CTimer::frame;publish();
 Check(Near(forward.x,-1)&&Near(forward.y,0),"boarding turn finishes facing outside at 300 ms");
 for(int slot=1;slot<=2;slot++){
  p.inVehicle=false;p.state=PED_IDLE;EvaluateTrain(true,head,forward);
  train.pPassengers[slot-1]=nullptr;train.pPassengers[slot]=&p;
  p.inVehicle=true;p.state=PED_DRIVING;++CTimer::frame;publish();
  CVector slotEye=train.matrix*trainModel.m_positions[slot]+train.matrix.up*.92f+forward*.07f+CrossProduct(forward,train.matrix.up)*.02f;
  Check(Near(head.x,slotEye.x)&&Near(head.y,slotEye.y)&&Near(head.z,slotEye.z),"each door retains its interior fallback and eye clearance");
 }
 trainNeckReady=true;trainNeck={-.4f,1.3f,2.1f};publish();
 expected=trainNeck+train.matrix.up*.04f+forward*.07f+CrossProduct(forward,train.matrix.up)*.02f;
 Check(Near(head.x,expected.x)&&Near(head.y,expected.y)&&Near(head.z,expected.z),"posed neck replaces marker and gets forward/right/up eye clearance");
 trainNeckReady=false;
 rw::vulkan::setFirstPersonPositionLocked(false);
 CVector beforeFree=Eye();Head(.65f,1.8f,-.2f,.5f);
 CVector freeDelta=Eye()-beforeFree;
 Check(Dot(freeDelta,freeDelta)>.009f,"leaving position lock restores ordinary physical translation");
 train.train=false;Check(!OculusVR::IsQuestTrainPassenger(),"ordinary cars retain existing policy");
 Check(!OculusVR::ShouldUseTrackedHands(),"default driving hands policy unchanged for cars");
 Check(OculusVR::ProductionVehicleThirdPerson(),"cars retain third-person preference");
 currentPlayer=nullptr;Check(!OculusVR::IsQuestTrainPassenger(),"missing player is safe");
}

static void KnockdownAttachment() {
 CPlayerPed p;CVehicle car;Reset(p,car);p.inVehicle=false;p.state=1001; // Synthetic fall state; ownership predicate is supplied by the adapter.
 ModelSets::modern=true;testKnockdown=true;
 for(int i=0;i<180;++i){
  // Includes physical crouching, turning and sideways roomscale motion.
  Head(.4f*std::sin(float(i)*.1f),1.2f+.2f*std::cos(float(i)*.1f),i*.003f,i*.01f);
  EvaluateSeat(true);const float head[3]={1,2,.3f},right[3]={1,0,0},up[3]={0,1,0},forward[3]={0,0,-1};
  rw::vulkan::setFirstPersonAnchorBasis(head,right,up,forward,0,1,true);
  CVector actual=Eye();Check(Near(actual.x,1)&&Near(actual.y,2)&&Near(actual.z,.3f),"knockdown and get-up stay at head despite physical movement");
  if(i==90)p.state=1002; // Synthetic get-up state.
 }
 testKnockdown=false;p.state=PED_IDLE;EvaluateSeat(true);
 CVector neutral=Eye();Head(.7f,1.3f,.4f);CVector after=Eye()-neutral;
 Check(Dot(after,after)>.01f,"getting up restores roomscale tracking");
 // Classic must continue tracking normally even during the same state.
 ModelSets::modern=false;testKnockdown=true;EvaluateSeat(true);neutral=Eye();Head(.9f,1.5f,.6f);
 after=Eye()-neutral;Check(Dot(after,after)>.01f,"Classic knockdown tracking policy is unchanged");
 ModelSets::modern=true;androidgame::knockdownLock=false;EvaluateSeat(true);neutral=Eye();Head(1,1.7f,.8f);
 after=Eye()-neutral;Check(Dot(after,after)>.01f,"calibration can opt out of knockdown translation lock");
 ModelSets::modern=false;testKnockdown=false;androidgame::knockdownLock=true;
}

static void XboxEntryAttachment(){
 CPlayerPed p;CVehicle car;Reset(p,car);ModelSets::modern=true;p.inVehicle=false;p.state=PED_OPEN_DOOR;
 Head(.3f,1.5f,-.2f,3.14159265f);Tick();
 Check(Near(rw::vulkan::gvk.fpLatchedHeadYaw,3.14159265f),"entry latches current physical facing even when turned 180 degrees");
 for(int state:{PED_OPEN_DOOR,PED_CARJACK,PED_ENTER_CAR,PED_STEAL_CAR}){
  p.state=state;const float entryYaw=rw::vulkan::gvk.fpLatchedHeadYaw;
  Head(.8f,1.1f,.4f,2.7f);Tick(false);Tick();const CVector before=Eye();
  Head(1,1.8f,.8f,2.3f);Tick();
  const CVector after=Eye();Check(Near(before.x,after.x)&&Near(before.y,after.y)&&Near(before.z,after.z),"entry remains head-position locked through every native phase");
  Check(Near(rw::vulkan::gvk.fpLatchedHeadYaw,entryYaw),"entry does not recenter heading repeatedly while physically looking around");
 }
 p.inVehicle=true;p.state=PED_DRIVING;Tick();
 Check(!rw::vulkan::gFirstPersonPositionLocked,"driving resumes ordinary physical translation");
 p.inVehicle=false;p.state=PED_IDLE;Tick();p.state=PED_ENTER_CAR;
 Head(.2f,1.6f,0,.6f);Tick();Check(Near(rw::vulkan::gvk.fpLatchedHeadYaw,.6f),"a later entry captures a fresh heading once");
 p.state=PED_IDLE;Tick();Check(!rw::vulkan::gFirstPersonPositionLocked,"cancelled entry releases the positional lock");
 ModelSets::modern=false;
}

static void XboxTrainWindowFacing(){
 CPlayerPed p;CVehicle train;Reset(p,train);ModelSets::modern=true;train.train=true;
 p.inVehicle=false;p.state=PED_ENTER_TRAIN;train.pDriver=nullptr;train.pPassengers[0]=&p;
 Head(.3f,1.6f,-.2f,1.1f);CVector head,forward;
 EvaluateSeat(false);EvaluateTrain(false,head,forward);
 const float beforeBoard=rw::vulkan::gvk.fpLatchedHeadYaw;
 p.inVehicle=true;p.state=PED_DRIVING;p.position={3,-2,0};p.forward={0,1,0};
 trainNeckReady=true;trainNeck={-.4f,1.3f,2.1f};
 auto publish=[&](bool post){
  EvaluateSeat(post);EvaluateTrain(post,head,forward);
  CVector right=CrossProduct(forward,train.matrix.up);
  rw::vulkan::setFirstPersonAnchorBasis(&head.x,&right.x,&train.matrix.up.x,&forward.x,Atan2(forward.y,forward.x),1,true);
 };
 publish(false);Check(Near(rw::vulkan::gvk.fpLatchedHeadYaw,beforeBoard),"Xbox train does not capture heading before boarding lineup finishes");
 publish(true);Check(Near(rw::vulkan::gvk.fpLatchedHeadYaw,1.1f),"Xbox train completion captures current physical facing once");
 for(int i=0;i<16;++i){++CTimer::frame;publish(true);}
 float r[3],u[3],f[3],eye[3];rw::vulkan::getFirstPersonViewFrame(r,u,f,eye);
 Check(Near(f[0],-1)&&Near(f[1],0),"completed window turn faces outside without a physical headset turn");
 for(int i=0;i<60;++i){
  ++CTimer::frame;const float angle=.01f*i;
  train.matrix.right={std::cos(angle),std::sin(angle),0};train.matrix.forward={-std::sin(angle),std::cos(angle),0};train.matrix.position={float(i)*.3f,2,1};
  p.position=train.matrix*CVector(0,0,-1.5f);p.forward=train.matrix.forward;trainNeck=train.matrix*CVector(-.4f,.1f,2.1f);
  Head(.5f+.001f*i,1.5f,-.3f,1.1f);publish(true);
  rw::vulkan::getFirstPersonViewFrame(r,u,f,eye);
  Check(Near(f[0],-train.matrix.right.x)&&Near(f[1],-train.matrix.right.y),"outside view remains carriage-relative while riding");
  Check(Near(rw::vulkan::gvk.fpLatchedHeadYaw,1.1f),"riding does not repeatedly reset physical looking");
 }
 Head(.7f,1.4f,-.2f,1.5f);++CTimer::frame;publish(true);
 Check(Near(rw::vulkan::gvk.fpLatchedHeadYaw,1.1f),"passenger can look away from window without forced recenter every frame");
 p.inVehicle=false;p.state=PED_EXIT_TRAIN;EvaluateSeat(true);
 Check(rw::vulkan::gFirstPersonPositionLocked,"Xbox exit keeps head position locked after vehicle occupancy clears");
 const CVector exitEye=Eye();Head(1.2f,.8f,-.9f,1.1f);EvaluateSeat(true);
 const CVector movedExitEye=Eye();
 Check(Near(exitEye.x,movedExitEye.x)&&Near(exitEye.y,movedExitEye.y)&&Near(exitEye.z,movedExitEye.z),"physical translation cannot detach camera during Xbox train exit");
 Check(Near(rw::vulkan::gvk.fpLatchedHeadYaw,1.1f),"Xbox exit preserves passenger heading calibration");
 p.inVehicle=false;p.state=PED_IDLE;EvaluateSeat(true);EvaluateTrain(true,head,forward);
 Check(!rw::vulkan::gFirstPersonPositionLocked,"leaving passenger context restores ordinary tracking policy");
 ModelSets::modern=false;p.state=PED_EXIT_TRAIN;EvaluateSeat(true);
 Check(!rw::vulkan::gFirstPersonPositionLocked,"Classic late train exit retains its existing tracking policy");
 trainNeckReady=false;ModelSets::modern=false;
}

int main(){try{EntryAndMotion();SuppressionAndLifecycle();TrackingTruthTable();TrainPassengerMotion();KnockdownAttachment();XboxEntryAttachment();XboxTrainWindowFacing();
 std::printf("PASS: %u production seat/train/XR/backend checks; carriage motion, free controllers and tracking continuity verified.\n",checks);return 0;
 }catch(const std::exception&e){std::fprintf(stderr,"FAIL after %u checks: %s\n",checks,e.what());return 1;}}
