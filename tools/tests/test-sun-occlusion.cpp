#ifdef NDEBUG
#undef NDEBUG
#endif
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
using uint8 = unsigned char;
using uint32 = unsigned;
using std::min;
using std::max;
template<class T> T Min(T a,T b){return min(a,b);}
template<class T> T Max(T a,T b){return max(a,b);}
template<class T> T sq(T a){return a*a;}
float Abs(float a){return std::fabs(a);}
#define RW_VULKAN
#define nil nullptr
#define FALSE 0
#define TRUE 1
#define PUSH_RENDERGROUP(x) ((void)0)
#define POP_RENDERGROUP() ((void)0)
#define WORLD_MIN_X (-2000.0f)
#define WORLD_MIN_Y (-2000.0f)
#define WORLD_MAX_X 2000.0f
#define WORLD_MAX_Y 2000.0f
constexpr float PI=3.14159265f;
struct CVector {
    float x=0,y=0,z=0;
    CVector operator+(CVector b)const{return {x+b.x,y+b.y,z+b.z};}
    CVector operator*(float s)const{return {x*s,y*s,z*s};}
};
using RwV3d=CVector;
float DotProduct(CVector a,CVector b){return a.x*b.x+a.y*b.y+a.z*b.z;}
struct Camera {
    CVector position,forward={1,0,0};
    struct {CVector Source;} Cams[1];
    int ActiveCam=0;
    CVector GetPosition()const{return position;}
    CVector GetForward()const{return forward;}
} TheCamera;
struct CTimeCycle {static CVector direction; static CVector GetSunDirection(){return direction;}};
CVector CTimeCycle::direction={1,0,0};
struct {void *camera=nullptr;} Scene;
float farClip=1000;
float RwCameraGetFarClipPlane(void*){return farClip;}
float RwCameraGetNearClipPlane(void*){return .1f;}
void *RwCameraGetRaster(void*){return nullptr;}
int RwRasterGetWidth(void*){return 1000;}
int RwRasterGetHeight(void*){return 1000;}
void *RwTextureGetRaster(void *p){return p;}
enum {rwRENDERSTATEZWRITEENABLE,rwRENDERSTATEVERTEXALPHAENABLE,rwRENDERSTATESRCBLEND,
      rwRENDERSTATEDESTBLEND,rwRENDERSTATEZTESTENABLE,rwRENDERSTATETEXTURERASTER,rwBLENDONE};
bool depthTest=false;
void RwRenderStateSet(int state,void *value){if(state==rwRENDERSTATEZTESTENABLE)depthTest=value!=nullptr;}
struct CWorld {
    static float wall;
    static unsigned calls;
    static CVector end;
    static bool GetIsLineOfSightClear(CVector a,CVector b,bool buildings,bool,bool,bool,bool,bool,bool=false){
        assert(buildings); ++calls; end=b;
        assert(b.x>=WORLD_MIN_X&&b.x<WORLD_MAX_X&&b.y>=WORLD_MIN_Y&&b.y<WORLD_MAX_Y);
        return !(wall>min(a.x,b.x)&&wall<max(a.x,b.x));
    }
};
float CWorld::wall=500;
unsigned CWorld::calls=0;
CVector CWorld::end;
struct CTimer {static unsigned GetTimeInMilliseconds(){return 100;} static float GetTimeStep(){return 1;}};
struct CWeather {static constexpr float Foggyness=0;};
struct Draw {float depth,reciprocal;bool tested;};
std::vector<Draw> draws;
struct CSprite {
    static bool CalcScreenCoors(CVector,CVector *p,float *w,float *h,bool){*p={500,500,150};*w=*h=1;return true;}
    static void RenderOneXLUSprite(float,float,float z,float,float,int,int,int,int,float rz,int){draws.push_back({z,rz,depthTest});}
    static void RenderOneXLUSprite_Rotate_Aspect(float,float,float z,float,float,int,int,int,int,float rz,float,int){draws.push_back({z,rz,depthTest});}
};
struct Corona {
    unsigned id=0,lastLOScheck=0;
    int fadeAlpha=255,alpha=255,red=255,green=255,blue=255;
    bool offScreen=false,sightClear=true,LOScheck=false;
    bool hasValue[6]={};
    float prevX[6]={},prevY[6]={};
    int prevRed[6]={},prevGreen[6]={},prevBlue[6]={};
    CVector coors;
    float drawDist=999999,size=10,someAngle=0;
    void *texture=reinterpret_cast<void*>(1);
    int flareType=0;
};
constexpr int NUMCORONAS=3;
struct CCoronas {
    enum {SUN_CORE=1,SUN_CORONA=2,FLARE_NONE=0,FLARE_SUN=1,FLARE_HEADLIGHTS=2};
    static Corona aCoronas[NUMCORONAS];
    static void Render();
};
Corona CCoronas::aCoronas[NUMCORONAS];
constexpr int FLARE_NONE=CCoronas::FLARE_NONE,FLARE_SUN=CCoronas::FLARE_SUN,FLARE_HEADLIGHTS=CCoronas::FLARE_HEADLIGHTS;
struct FlareDef {int texture;float position,size;int red,green,blue,alpha;};
FlareDef SunFlareDef[]={{1,1,1,255,255,255,255},{0,0,0,0,0,0,0}};
FlareDef HeadLightsFlareDef[]={{0,0,0,0,0,0,0}};
void *gpCoronaTexture[9]={};
// Upstream narrows fade/colour values and asserts on unknown flare types.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4244 4701 4703)
#endif
#include "sun-occlusion-production.inc"
#ifdef _MSC_VER
#pragma warning(pop)
#endif
int main(){
    // A wall beyond the historical 150 m endpoint blocks both sun sprites
    // and the lens ghost, immediately, even with fully visible fade state.
    auto &core=CCoronas::aCoronas[0], &halo=CCoronas::aCoronas[1];
    core.id=CCoronas::SUN_CORE;halo.id=CCoronas::SUN_CORONA;
    halo.LOScheck=true;halo.flareType=FLARE_SUN;
    CCoronas::Render();assert(draws.empty());assert(CWorld::calls==1);
    CWorld::wall=10000;CWorld::calls=0;
    CCoronas::Render();assert(draws.size()==3);assert(CWorld::calls==1);
    assert(draws[0].tested&&draws[1].tested&&!draws[2].tested);
    assert(1/draws[0].reciprocal>999&&1/draws[1].reciprocal>999);
    // Ordinary lights retain their original camera depth and LOS policy.
    draws.clear();core.id=3;halo.id=0;CCoronas::Render();
    assert(draws.size()==1&&draws[0].tested&&std::fabs(1/draws[0].reciprocal-150)<.01f);
    // Angled views trace to the far plane, rather than a sphere in front of it.
    CTimeCycle::direction={.6f,.8f,0};
    assert(QuestSunSightClear());assert(std::fabs(CWorld::end.x-1000)<.01f);
    // All horizontal directions near map boundaries remain valid sector queries.
    for(float x : {-1999.0f,1999.0f})for(float y : {-1999.0f,1999.0f}){
        TheCamera.position={x,y,0};
        for(float dx : {-1.0f,0.0f,1.0f})for(float dy : {-1.0f,0.0f,1.0f}){
            CTimeCycle::direction={dx,dy,1};QuestSunSightClear();
        }
    }
    CWorld::calls=0;TheCamera.position={2500,0,0};
    assert(QuestSunSightClear()&&CWorld::calls==0);
    std::puts("PASS: production sun rendering and LOS; distant blockers, immediate hiding, background depth, flare state, ordinary lights, angled rays and map bounds.");
}
