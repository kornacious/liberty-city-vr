#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
static int gGameState=1;
enum { GS_PLAYING_GAME=1 };
static struct {bool m_bGameNotLoaded=false,m_bMenuActive=false;} FrontEndMenuManager;
struct CGame {static bool playingIntro;};bool CGame::playingIntro=false;
struct CCutsceneMgr {static bool running,processing;static bool IsRunning(){return running;}static bool IsCutsceneProcessing(){return processing;}};
bool CCutsceneMgr::running=false,CCutsceneMgr::processing=false;
static bool vehicle=false;
static bool FindPlayerVehicle(){return vehicle;}
static struct {bool m_WideScreenOn=false;} TheCamera;
static bool headRelative=true,tracking=true;
static float yaw=0;
namespace androidgame {static bool VrUsesHeadRelativeMovement(){return headRelative;}}
namespace rw {namespace vulkan {static bool getFirstPersonLocalHeadYaw(float *out){*out=yaw;return tracking;}}}
template<class T>T clamp(T x,T lo,T hi){return x<lo?lo:x>hi?hi:x;}
static unsigned checks;
static void Require(bool ok,const char *why){++checks;if(!ok){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
int main(){
 for(bool trackedScopeActive:{false,true})for(float angle:{0.0f,1.5707963f,3.14159265f,-1.5707963f})
 for(bool relative:{false,true})for(int gate=0;gate<11;++gate){
  (void)trackedScopeActive; // Available to the extracted gate to catch a scope exclusion.
  bool remoteMode=gate==1;
  vehicle=gate==2;FrontEndMenuManager.m_bMenuActive=gate==3;FrontEndMenuManager.m_bGameNotLoaded=gate==4;
  CGame::playingIntro=gate==5;CCutsceneMgr::running=gate==6;CCutsceneMgr::processing=gate==7;
  TheCamera.m_WideScreenOn=gate==8;gGameState=gate==9?0:GS_PLAYING_GAME;tracking=gate!=10;
  headRelative=relative;yaw=angle;
  struct {float leftStickX=.25f,leftStickY=.9f;} in;
#include "physical-turn-gate.inc"
#include "physical-turn-stick.inc"
  const bool transformed=gate==0 && relative;
  const float expectedX=transformed?in.leftStickX*std::cos(angle)-in.leftStickY*std::sin(angle):in.leftStickX;
  const float expectedY=transformed?in.leftStickY*std::cos(angle)+in.leftStickX*std::sin(angle):in.leftStickY;
  Require(std::fabs(moveStickX-expectedX)<.0001f && std::fabs(moveStickY-expectedY)<.0001f,
   "physical movement follows headset with and without tracked sniper; gameplay/tracking gates retain raw input");
  Require(onFootGameplay==(gate==0 || gate==10),"scope never disables directional animation availability");
 }
 std::printf("PASS %u production physical-turn input checks\n",checks);
}
