#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <initializer_list>
#include "ragdoll-test-vector.h"
using std::isfinite;
using float32=float;using bool32=int;
#define nil nullptr
template<class T>T Min(T a,T b){return a<b?a:b;}template<class T>T Max(T a,T b){return a>b?a:b;}
static unsigned checks;
static void Require(bool ok,const char *text){++checks;if(!ok){std::fprintf(stderr,"FAIL %s\n",text);std::exit(1);}}
static void Near(CVector a,CVector b,const char *text){Require((a-b).Magnitude()<.0002f,text);}
namespace rw {namespace vulkan {
struct State {
 bool32 firstPersonActive=1,fpUseFullBasis=1;
 float fpPlayX[3]={1,0,0},fpPlayY[3]={0,0,1},fpPlayZ[3]={0,-1,0},fpAnchorYaw=0;
 float headPosition[3]={},fpLatchedHeadPos[3]={},headQuat[4]={0,0,0,1},headYaw=0,fpHeadWorld[3]={};
} gvk;
static bool32 gFirstPersonPositionLocked,gFirstPersonHeightLocked;
#include "roomscale-backend.inc"
}}
enum {PED_IDLE,PED_FALL,PED_GETUP,PED_STAGGER,PED_DIVE_AWAY,PED_ARRESTED,PED_HANDS_UP,WBSTATE_PLAYING,WBSTATE_BUSTED};
struct CMatrix {void UpdateRW(){}};
struct CPlayerPed {
 CVector position{0,0,1};CMatrix matrix;
 bool vehicle=false,dead=false,control=true,bIsInTheAir=false,bIsStanding=true,bUsesCollision=true;
 int state=PED_IDLE;struct {bool valid=true;}m_collPoly;
 bool InVehicle(){return vehicle;}bool DyingOrDead(){return dead;}bool IsPedInControl(){return control;}
 int GetPedState(){return state;}CVector GetPosition(){return position;}void SetPosition(CVector p){position=p;}
 CMatrix &GetMatrix(){return matrix;}void UpdateRwFrame(){}void UpdateRpHAnim(){}
};
struct CEntity {};
struct CColPoint {CVector point,normal;};
namespace ModelSets {static bool modern=true;static bool IsModernActive(){return modern;}}
namespace CWorld {
struct Info {int m_WBState=WBSTATE_PLAYING;};static Info Players[1];static int PlayerInFocus=0;
static bool ground=true,wall=false;static unsigned removes,adds;static float height=0;
static bool ProcessVerticalLine(CVector p,float,CColPoint &hit,CEntity*&,bool,bool,bool,bool,bool,bool,void*){hit.point={p.x,p.y,height};hit.normal={0,0,1};return ground;}
static CEntity *TestSphereAgainstWorld(CVector p,float,CPlayerPed*,bool,bool,bool,bool,bool,bool){static CEntity e;return wall&&p.x>.08f?&e:nullptr;}
static void Remove(CPlayerPed*){++removes;}static void Add(CPlayerPed*){++adds;}
}
struct CTimer {static unsigned frame;static bool paused;static unsigned GetFrameCounter(){return frame;}static bool GetIsPaused(){return paused;}};
unsigned CTimer::frame=0;bool CTimer::paused=false;
struct CPad {static bool disabled;static CPad *GetPad(int){static CPad p;return &p;}bool ArePlayerControlsDisabled(){return disabled;}};bool CPad::disabled=false;
static CPlayerPed *current;
static CPlayerPed *FindPlayerPed(){return current;}
static bool gVrFirstPersonActive=true;
static constexpr int GS_PLAYING_GAME=1;
static int gGameState=GS_PLAYING_GAME;
static struct {bool m_bGameNotLoaded=false,m_bMenuActive=false,m_bWantToRestart=false,m_bWantToLoad=false;}FrontEndMenuManager;
static struct {bool m_WideScreenOn=false;}TheCamera;
struct CGame {static bool playingIntro;};bool CGame::playingIntro=false;
namespace CCutsceneMgr {static bool running=false;static bool IsRunning(){return running;}static bool IsCutsceneProcessing(){return false;}}
namespace androidgame {
static bool menu=false;
static bool VrMenuConsumesInput(){return menu;}
static int VrCutsceneMode(){return 0;}
bool VrShouldUseTheaterMode();
static bool VrGetViewBasis(float *p,float*,float*,float*){
 if(!current)return false;
 auto &s=rw::vulkan::gvk;for(int i=0;i<3;++i)s.fpHeadWorld[i]=(&current->position.x)[i];
 s.fpHeadWorld[2]+=.6f;float r[3],u[3],f[3];return rw::vulkan::getFirstPersonViewFrame(r,u,f,p)!=0;
}
static bool VrGetOnFootBodyAnchor(float *p){std::memcpy(p,rw::vulkan::gvk.fpHeadWorld,sizeof(float)*3);return true;}
}
#include "roomscale-player.inc"
static CVector Eye(){CVector p;Require(androidgame::VrGetViewBasis(&p.x,nullptr,nullptr,nullptr),"view available");return p;}
static void SetOffset(CVector world){float play[3];rw::vulkan::firstPersonWorldVectorToPlay(&world.x,play);for(int i=0;i<3;++i)rw::vulkan::gvk.headPosition[i]=rw::vulkan::gvk.fpLatchedHeadPos[i]+play[i];}
int main(){
 using namespace rw::vulkan;
 CPlayerPed player;current=&player;
 for(float yaw:{0.0f,.4f,1.5707963f,3.0f}){
  ++CTimer::frame;player={};gvk=State();
  gvk.fpPlayX[0]=std::cos(yaw);gvk.fpPlayX[1]=std::sin(yaw);
  gvk.fpPlayZ[0]=std::sin(yaw);gvk.fpPlayZ[1]=-std::cos(yaw);
  SetOffset({.3f,0,0});const CVector before=Eye();
  Require(UpdateQuestRoomscale(),"physical motion transfers into player root");
  Near(player.position,{.2f,0,1},"AI and collision root moves to physical location with small lean allowance");
  Near(Eye(),before,"tracking origin consumption prevents double camera movement");
  const CVector once=player.position;Require(!UpdateQuestRoomscale(),"duplicate pre-simulation sample is ignored");Near(player.position,once,"one root transfer per frame");
 }
 for(unsigned mask=0;mask<128;++mask){
  ++CTimer::frame;player={};gvk=State();SetOffset({.3f,0,0});
  player.vehicle=(mask&1)!=0;player.control=(mask&2)==0;player.bIsInTheAir=(mask&4)!=0;
  CTimer::paused=(mask&8)!=0;CPad::disabled=(mask&16)!=0;androidgame::menu=(mask&32)!=0;ModelSets::modern=(mask&64)==0;
  const bool moved=UpdateQuestRoomscale();Require(moved==(mask==0),"only active controlled grounded Xbox gameplay transfers motion");
  if(mask)Near(player.position,{0,0,1},"excluded owner keeps native root");
 }
 CTimer::paused=CPad::disabled=androidgame::menu=false;ModelSets::modern=true;player={};gvk=State();
 CWorld::ground=false;++CTimer::frame;SetOffset({.3f,0,0});Require(!UpdateQuestRoomscale(),"no ground means no root teleport");CWorld::ground=true;
 CWorld::wall=true;++CTimer::frame;Require(UpdateQuestRoomscale(),"unblocked substeps are accepted before wall");Require(player.position.x<=.08f,"capsule probes stop root at obstacle");CWorld::wall=false;
 for(int state:{PED_FALL,PED_GETUP,PED_ARRESTED,PED_HANDS_UP}){player={};player.state=state;++CTimer::frame;Require(PlayerKnockedDown(&player),"falls/get-up/arrest share native camera ownership");Require(!UpdateQuestRoomscale(),"incapacitated pose cannot consume roomscale");}
 player={};CWorld::Players[0].m_WBState=WBSTATE_BUSTED;Require(PlayerKnockedDown(&player),"busted state remains attached through restart sequence");
 TheCamera.m_WideScreenOn=true;Require(!androidgame::VrShouldUseTheaterMode(),"busted letterbox does not force the director/theater camera");
 CCutsceneMgr::running=true;Require(androidgame::VrShouldUseTheaterMode(),"formal cutscene ownership remains intact");CCutsceneMgr::running=false;
 ModelSets::modern=false;Require(!PlayerKnockedDown(&player),"Classic arrest policy unchanged");ModelSets::modern=true;CWorld::Players[0].m_WBState=WBSTATE_PLAYING;
 TheCamera.m_WideScreenOn=false;
 gvk=State();player={};float quat[4]={0,0,0,1},pose[3]={0,1.7f,0};setHeadPose(pose,0,quat);std::memcpy(gvk.fpLatchedHeadPos,pose,sizeof(pose));
 setFirstPersonHeightLocked(true);const float level=Eye().z;
 for(int i=0;i<100;++i){pose[1]=.8f+float(i)*.01f;pose[0]=float(i)*.001f;setHeadPose(pose,0,quat);Require(std::fabs(Eye().z-level)<.0001f,"button crouch ignores physical height changes");}
 Require(Eye().x>.09f,"height lock retains horizontal head movement");setFirstPersonHeightLocked(false);Require(std::fabs(Eye().z-level)<.0001f,"crouch release does not replay physical height displacement");
 pose[1]+=.1f;setHeadPose(pose,0,quat);Require(Eye().z>level+.09f,"standing restores physical vertical tracking");
 std::printf("PASS %u production roomscale/root/crouch/arrest checks\n",checks);
}
