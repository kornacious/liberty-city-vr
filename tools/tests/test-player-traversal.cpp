#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <limits>
#include <initializer_list>
#include "../../overlay/src/peds/VrTraversalRules.h"
#define nil nullptr
static const float FEET_OFFSET=1.0f;
enum { PED_IDLE, PEDMOVE_STILL, PEDMOVE_WALK };
template<class T>T Min(T a,T b){return a<b?a:b;}
template<class T>T Max(T a,T b){return a>b?a:b;}
float Abs(float f){return std::fabs(f);}
struct CVector {
 float x,y,z;
 CVector(float a=0,float b=0,float c=0):x(a),y(b),z(c){}
 CVector operator+(const CVector &b)const{return {x+b.x,y+b.y,z+b.z};}
 CVector operator*(float f)const{return {x*f,y*f,z*f};}
 CVector operator/(float f)const{return *this*(1/f);}
 float MagnitudeSqr()const{return x*x+y*y+z*z;}
 float Magnitude()const{return std::sqrt(MagnitudeSqr());}
 void Normalise(){*this=*this/Magnitude();}
};
struct CEntity{};
struct CColPoint{CVector point;};
struct Wanted{int ticks=0;void Update(){++ticks;}};
struct Matrix{void UpdateRW(){}};
struct CPlayerPed: CEntity {
 bool bUsesCollision=true,bInVehicle=false,bIsInTheAir=false,bIsLanding=false,bIsStanding=true,inControl=true;
 float m_fHealth=100,m_fRotationCur=0,m_fRotationDest=0;
 int m_nMoveState=0,state=0;
 CVector position{0,0,1},m_vecMoveSpeed,m_vecTurnSpeed;
 struct Poly{bool valid=true;}m_collPoly;
 CEntity *m_pCurrentPhysSurface=nil;
 Wanted wanted;Wanted *m_pWanted=&wanted;Matrix matrix;
 bool IsPedInControl(){return inControl;}
 CVector &GetPosition(){return position;}
 void SetPosition(CVector p){position=p;}
 void SetHeading(float){}
 void UpdateRwFrame(){}
 Matrix &GetMatrix(){return matrix;}
 void SetPedState(int s){state=s;}
 void SetMoveAnim(){}
};
static CPlayerPed *current;
CPlayerPed *FindPlayerPed(){return current;}
struct CPad {
 static CPad pad;bool disabled=false;float x=0,y=0;
 static CPad *GetPad(int){return &pad;}
 bool ArePlayerControlsDisabled(){return disabled;}
 float GetPedWalkLeftRight(){return x;}
 float GetPedWalkUpDown(){return y;}
};CPad CPad::pad;
struct CTimer{static float step;static float GetTimeStep(){return step;}};float CTimer::step=1;
struct Camera{struct Cam{CVector Front{0,1,0};}Cams[1];int ActiveCam=0,resets=0;bool m_WideScreenOn=false;void RestoreWithJumpCut(){++resets;}}TheCamera;
struct CCutsceneMgr{static bool active;static bool IsRunning(){return active;}static bool IsCutsceneProcessing(){return active;}};bool CCutsceneMgr::active;
struct Frontend{bool m_bMenuActive=false;}FrontEndMenuManager;
struct CGame{static bool playingIntro;};bool CGame::playingIntro;
namespace androidgame{
struct Input{bool a=false,b=false,leftStickClick=false;}input;
static bool menu=false;
bool VrMenuConsumesInput(){return menu;}
const Input &GetPadInput(){return input;}
}
struct CStreaming{static int loads;static void LoadScene(CVector){++loads;}};int CStreaming::loads;
struct CWorld{
 struct Player{bool remote=false;bool IsPlayerInRemoteMode(){return remote;}};
 static Player Players[1];static int PlayerInFocus,removes,adds;
 static bool obstacle,terrainWall;static float floor;
 static void Remove(CPlayerPed *){++removes;}
 static void Add(CPlayerPed *){++adds;}
 static bool ProcessVerticalLine(CVector,float,CColPoint &,CEntity *&,bool,bool,bool,bool,bool,bool,void *);
 static CEntity *TestSphereAgainstWorld(CVector,float,CEntity *,bool,bool,bool,bool,bool,bool);
};
CWorld::Player CWorld::Players[1];int CWorld::PlayerInFocus,CWorld::removes,CWorld::adds;
bool CWorld::obstacle,CWorld::terrainWall;float CWorld::floor=0;
namespace VrPlayerTraversal {
class TerrainModelScope { bool previous; public: explicit TerrainModelScope(const char *); ~TerrainModelScope(); };
}
#include "player-traversal-production.inc"
bool CWorld::ProcessVerticalLine(CVector p,float bottom,CColPoint &hit,CEntity *&,bool,bool,bool,bool,bool,bool,void *){
 if(!VrPlayerTraversal::TerrainQuery())std::abort();
 if(floor>p.z||floor<bottom)return false;
 hit.point={p.x,p.y,floor};return true;
}
CEntity *CWorld::TestSphereAgainstWorld(CVector p,float,CEntity *,bool,bool,bool,bool,bool,bool){
 static CEntity wall;
 if(VrPlayerTraversal::TerrainQuery())return terrainWall&&p.x>0.4f?&wall:nil;
 return obstacle&&p.x>0.4f?&wall:nil;
}
static int checks;
void Check(bool b,const char *s){++checks;if(!b){std::fprintf(stderr,"FAIL: %s\n",s);std::exit(1);}}
void Clear(CPlayerPed &p){
 VrPlayerTraversal::Reset(&p);p=CPlayerPed();p.m_pWanted=&p.wanted;current=&p;
 CPad::pad=CPad();androidgame::input={};androidgame::menu=false;
 CWorld::obstacle=CWorld::terrainWall=false;CWorld::floor=0;
 CCutsceneMgr::active=false;TheCamera.m_WideScreenOn=false;
 CTimer::step=1;
}
int main(){
 using namespace VrPlayerTraversal;
 for(int surface=0;surface<35;++surface)
  Check(VrTraversal::GroundSurface(surface)==((surface>=1&&surface<=5)||surface==18||surface==26),"terrain mask excludes buildings, props, cars and peds");
 for(int hz:{30,72,90,120}){
  auto m=VrTraversal::Step(1,1,1,false,1.0f/(float)hz,true);
  Check(std::fabs(std::sqrt(m.right*m.right+m.forward*m.forward+m.up*m.up)*hz-6)<0.001f,"flight speed is frame independent and diagonal normalized");
 }
 Check(VrTraversal::Step(0,0,1,true,0.1f,false).up==0,"noclip alone cannot fly");
 Check(VrTraversal::Step(0,1,0,true,0.1f,true).forward==3,"boost is 30 units per second");
 Check(VrTraversal::Step(0,1,0,true,10,true).forward==3,"stalls cannot cause giant jumps");
 Check(VrTraversal::OutOfBounds(1950,0,0)&&VrTraversal::OutOfBounds(0,0,-81)&&VrTraversal::OutOfBounds(0,0,1001),"world boundaries recover");
 Check(VrTraversal::OutOfBounds(std::numeric_limits<float>::quiet_NaN(),0,0),"invalid position recovers");
 Check(VrTraversal::TerrainModelName("twnhl_lndnite")&&VrTraversal::TerrainModelName("COM_LAND050")&&VrTraversal::TerrainModelName("rd_Road1C50kb"),"terrain model names include generic ground and road tiles");
 Check(!VrTraversal::TerrainModelName("townhall_nite")&&!VrTraversal::TerrainModelName("roadplanterkb1")&&!VrTraversal::TerrainModelName("road_sign"),"buildings and roadside props remain permeable");
 Check(VrTraversal::GroundSurface(0,true)&&!VrTraversal::GroundSurface(0,false),"default material is solid only on terrain models");
 terrainQuery=true;
 {TerrainModelScope outer("COM_LAND050");Check(TerrainModel(),"ground context enabled");{TerrainModelScope inner("townhall_nite");Check(!TerrainModel(),"nested model context changes");}Check(TerrainModel(),"model context restores after nested query");}
 Check(!TerrainModel(),"model context does not leak into gameplay collision");
 terrainQuery=false;
 CPlayerPed p;Clear(p);
 Check(ToggleFlight()&&Flight(),"flight toggles on");
 androidgame::input.a=true;Process(&p);
 Check(p.position.z>1&&!p.bUsesCollision,"A ascends and native physics is suspended");
 androidgame::input.a=false;androidgame::input.b=true;Process(&p);
 Check(std::fabs(p.position.z-1)<0.001f,"B descends");
 androidgame::input.b=false;float z=p.position.z;Process(&p);
 Check(p.position.z==z,"flight hovers without input");
 androidgame::menu=true;androidgame::input.a=true;Process(&p);
 Check(p.position.z==z,"menu input cannot move player");
 androidgame::menu=false;androidgame::input.a=false;CPad::pad.x=128;CWorld::obstacle=true;
 for(int i=0;i<30;++i)Process(&p);
 Check(p.position.x<=0.4f,"flight alone stops at obstacles with substeps");
 Check(ToggleNoclip(),"noclip combines with flight");
 for(int i=0;i<30;++i)Process(&p);
 Check(p.position.x>1,"flight and noclip bypass obstacles");
 androidgame::input.b=true;for(int i=0;i<30;++i)Process(&p);
 Check(p.position.z<0,"flight and noclip pass below terrain");
 Clear(p);ToggleNoclip();CPad::pad.x=128;CWorld::obstacle=true;
 for(int i=0;i<30;++i)Process(&p);
 Check(p.position.x>1&&p.position.z==1,"walking noclip crosses obstacles but preserves terrain floor");
 CWorld::terrainWall=true;p.position.x=0;
 for(int i=0;i<30;++i)Process(&p);
 Check(p.position.x<=0.4f,"walking noclip stops at terrain wall");
 Clear(p);ToggleFlight();Process(&p);CCutsceneMgr::active=true;
 Check(!Process(&p)&&p.bUsesCollision,"cutscene restores normal physics");
 CCutsceneMgr::active=false;Process(&p);p.bInVehicle=true;
 Check(!Process(&p)&&p.bUsesCollision,"vehicles retain normal physics");
 p.bInVehicle=false;Process(&p);p.m_fHealth=0;
 Check(!Process(&p)&&p.bUsesCollision,"death restores collision");
 Clear(p);p.bUsesCollision=false;ToggleFlight();Process(&p);ToggleFlight();Process(&p);
 Check(!p.bUsesCollision,"previous collision state is preserved");
 Clear(p);ToggleFlight();Process(&p);p.position={2001,0,1};Process(&p);
 Check(!Flight()&&!Noclip()&&p.bUsesCollision,"out of bounds clears traversal and restores collision");
 Check(std::fabs(p.position.x-96.8f)<0.01f&&p.position.y==-1001&&p.position.z==1,"recovery loads Town Hall destination and resolves ground height");
 Check(CStreaming::loads>0&&TheCamera.resets>0,"recovery loads scene and resets camera");
 Clear(p);p.bInVehicle=true;
 Check(!ToggleFlight()&&!ToggleNoclip(),"traversal cannot be enabled in a vehicle");
 Clear(p);p.inControl=false;
 Check(!ToggleFlight()&&!ToggleNoclip(),"traversal cannot be enabled during a scripted control sequence");
 Clear(p);Check(!Process(&p)&&p.bUsesCollision&&p.position.z==1,"disabled traversal leaves normal player processing intact");
 ToggleFlight();ToggleNoclip();Process(&p);p.position.x=1;CWorld::obstacle=true;
 Check(ToggleNoclip()&&!Flight()&&!Noclip()&&p.bUsesCollision,"disabling noclip inside a wall recovers before solid flight resumes");
 Clear(p);ToggleFlight();ToggleNoclip();Process(&p);p.position.x=1;CWorld::obstacle=true;
 ToggleFlight();ToggleNoclip();Process(&p);
 Check(p.position.x==96.8f&&p.bUsesCollision,"turning both modes off inside an obstacle recovers safely");
 Clear(p);ToggleFlight();Process(&p);p.position={1949.99f,0,1};CPad::pad.x=128;
 Process(&p);Check(!Flight()&&p.position.x==96.8f,"movement crossing boundary recovers before world-sector insertion");
 Clear(p);ToggleFlight();Process(&p);p.position.x=std::numeric_limits<float>::quiet_NaN();
 ToggleFlight();Process(&p);Check(p.position.x==96.8f,"invalid position recovers even when mode has just been disabled");
 Clear(p);p.position={0,0,-81};p.inControl=false;
 Check(Process(&p)&&p.position.x==96.8f,"falling out of bounds recovers even with both cheats disabled");
 Check(CWorld::adds==CWorld::removes,"all movement keeps world sector membership balanced");
 std::printf("PASS: %d traversal checks.\n",checks);
}
