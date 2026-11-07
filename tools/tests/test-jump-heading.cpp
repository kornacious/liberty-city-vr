#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#define nil nullptr
static constexpr float pi=3.14159265358979323846f;
#define PI pi
struct CVector {
 float x=0,y=0,z=0;
 CVector()=default;CVector(float a,float b,float c):x(a),y(b),z(c){}
 float MagnitudeSqr()const{return x*x+y*y+z*z;}
 float Heading()const{return std::atan2(-x,y);}
};
static float Sin(float f){return std::sin(f);}static float Cos(float f){return std::cos(f);}
struct CPad {
 float x=0,y=0;static CPad pad;
 static CPad *GetPad(int){return &pad;}
 float GetPedWalkLeftRight(){return x;}float GetPedWalkUpDown(){return y;}
};CPad CPad::pad;
struct CGeneral {
 static float LimitRadianAngle(float x){return std::remainder(x,2*pi);}
 static float GetRadianAngleBetweenPoints(float,float,float x,float y){return std::atan2(x,-y);}
};
static struct {struct {CVector Front{0,1,0};bool mouse=true;bool Using3rdPersonMouseCam(){return mouse;}} Cams[1];int ActiveCam=0;} TheCamera;
struct CPed {
 bool vehicle=false,player=true;float m_fRotationCur=0,m_fRotationDest=0,m_fWalkAngle=0;
 CVector m_vecMoveSpeed,forward{0,1,0};int walkCalls=0;
 bool InVehicle(){return vehicle;}bool IsPlayer(){return player;}
 void SetHeading(float f){forward={-Sin(f),Cos(f),0};}
 float WorkOutHeadingForMovingFirstPerson(float);
};
using CPlayerPed=CPed;
static CPed *current;
static CPed *FindPlayerPed(){return current;}
namespace androidgame {
static bool active=true,tracked=true,vertical=false,invalid=false;
static float physicalHeading=0;
static bool VrGetOnFootAnimationAngle(float*){return active;}
static bool VrGetViewBasis(float*,float *right,float*,float *forward){
 if(!tracked)return false;
 right[0]=invalid?0:Cos(physicalHeading);right[1]=invalid?0:Sin(physicalHeading);right[2]=0;
 forward[0]=vertical?0:-Sin(physicalHeading);forward[1]=vertical?0:Cos(physicalHeading);forward[2]=vertical?-1.0f:0;
 return true;
}
}
#include "jump-walk.inc"
#include "jump-heading.inc"
static unsigned checks;
static void Require(bool ok,const char *why){++checks;if(!ok){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
int main(){
 CPed player,npc;current=&player;npc.player=false;
 for(float camera:{0.0f,.7f,pi/2,pi,-pi/2})for(float physical:{0.0f,pi/2,pi,-pi/2})
 for(float old:{0.0f,pi/2,pi})for(int direction=0;direction<5;++direction)
 for(bool down:{false,true})for(bool mouse:{false,true}){
  TheCamera.Cams[0].Front={-Sin(camera),Cos(camera),0};TheCamera.Cams[0].mouse=mouse;
  androidgame::physicalHeading=camera+physical;androidgame::vertical=down;
  const float inputAngle=direction==0?0:float(direction-1)*pi/2;
  const float rawX=direction==0?0:-Sin(inputAngle),rawY=direction==0?0:Cos(inputAngle);
  CPad::pad.x=(rawX*Cos(physical)-rawY*Sin(physical))*128;
  CPad::pad.y=-(rawY*Cos(physical)+rawX*Sin(physical))*128;
  player.m_fRotationCur=player.m_fRotationDest=old;player.m_fWalkAngle=1.2f;
  Require(UpdateQuestJumpHeading(&player),"Quest launch heading available");
  const float expected=camera+physical+inputAngle;
  Require(std::fabs(player.forward.x+Sin(expected))<.0002f && std::fabs(player.forward.y-Cos(expected))<.0002f,
   "jump collision facing follows physical view or transformed locomotion, never stale heading/walk angle");
  CPed *ped=&player;const float velocityFromAnim=.1f;const bool questJump=true;
#include "jump-impulse.inc"
  Require(std::fabs(ped->m_vecMoveSpeed.x+.1f*Sin(expected))<.0002f && std::fabs(ped->m_vecMoveSpeed.y-.1f*Cos(expected))<.0002f,
   "actual launch impulse applies physical/stick direction exactly once in both camera paths");
 }
 for(int gate=0;gate<5;++gate){
  CPed *ped=gate==0?&npc:&player;ped->vehicle=gate==1;
  androidgame::active=gate!=2;androidgame::tracked=gate!=3;
  androidgame::invalid=gate==4;androidgame::vertical=true;
  ped->m_fRotationCur=.43f;ped->m_fRotationDest=.65f;
  Require(!UpdateQuestJumpHeading(ped),"NPC/vehicle/body movement/unavailable view retain legacy behavior");
  Require(ped->m_fRotationCur==.43f && ped->m_fRotationDest==.65f,"failed gates leave heading unchanged");
 }
 std::printf("PASS %u production physical jump heading/impulse checks\n",checks);
}
