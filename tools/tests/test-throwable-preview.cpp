#include <cassert>
#include <cmath>
#include <cstdio>
#include <initializer_list>
using uint8 = unsigned char;
#define nil nullptr
#define TRUE 1
#define FALSE 0
struct CVector {
    float x, y, z;
    CVector(float x=0, float y=0, float z=0):x(x),y(y),z(z) {}
    CVector operator+(CVector b) const { return {x+b.x,y+b.y,z+b.z}; }
    CVector operator-(CVector b) const { return {x-b.x,y-b.y,z-b.z}; }
    CVector operator*(float b) const { return {x*b,y*b,z*b}; }
    CVector operator/(float b) const { return *this*(1/b); }
    float MagnitudeSqr() const { return x*x+y*y+z*z; }
    float Magnitude() const { return std::sqrt(MagnitudeSqr()); }
    void Normalise() { *this=*this/Magnitude(); }
};
CVector CrossProduct(CVector a,CVector b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
struct CEntity {};
struct CWeapon { int m_eWeaponType; };
struct CPed : CEntity {
    bool owned=true;
    CWeapon weapon={0};
    bool HasWeapon(uint8) { return owned; }
    CWeapon &GetWeapon(uint8) { return weapon; }
};
struct CColPoint { CVector point; };
static bool collision;
static int segments, draws, launchCalls, lastHand;
struct CWorld {
    static CEntity *pIgnoreEntity;
    static bool ProcessLineOfSight(CVector, CVector next, CColPoint &point, CEntity *&, bool,bool,bool,bool,bool,bool,bool) {
        ++segments; point.point=next; return collision;
    }
};
CEntity *CWorld::pIgnoreEntity;
constexpr float GRAVITY=0.008f;
struct RwIm3DVertex {};
using RwImVertexIndex = unsigned short;
enum { rwRENDERSTATETEXTURERASTER, rwRENDERSTATEZTESTENABLE, rwRENDERSTATEZWRITEENABLE,
    rwRENDERSTATEVERTEXALPHAENABLE, rwRENDERSTATEFOGENABLE, rwRENDERSTATECULLMODE,
    rwCULLMODECULLNONE, rwIM3D_VERTEXXYZ=16, rwIM3D_VERTEXRGBA=32, rwPRIMTYPETRILIST };
void RwRenderStateSet(int,void *) {}
void AppendVrHandBox(RwIm3DVertex *,int &v,RwImVertexIndex *,int &i,CVector,CVector,CVector,CVector,float,float,float,int,int,int) { v+=8;i+=36; }
namespace GameRender {
enum { IMMEDIATE3D_DRAW_VR_LASER };
bool BeginImmediate3D(RwIm3DVertex *,int,void *,int,int) { ++draws;return true; }
void SubmitImmediate3DIndexed(int,RwImVertexIndex *,int) {}
void EndImmediate3D() {}
}
namespace OculusVR {
static bool preview=true, launch=true;
static int slot, type;
bool IsTrackedThrowablePreviewActive(int) { return preview; }
int GetHeldWeaponSlot(int) { return slot; }
int GetVrWeaponTypeForSlot(int) { return type; }
bool IsPhysicalThrowableType(int);
bool GetTrackedThrowableLaunch(int hand,int,CVector *source,CVector *velocity) {
    ++launchCalls;lastHand=hand;*source={0,0,1};*velocity={0,0.46f,0.2f};return launch;
}
}
#include "throwable-preview-production.inc"
bool OculusVR::IsPhysicalThrowableType(int t) { return t==WEAPONTYPE_MOLOTOV || t==WEAPONTYPE_GRENADE; }
int main() {
    CPed player;
    CEntity ignored;
    CWorld::pIgnoreEntity=&ignored;
    for(int hand=0;hand<2;++hand) {
        for(int type : {WEAPONTYPE_MOLOTOV,WEAPONTYPE_GRENADE}) {
            OculusVR::type=player.weapon.m_eWeaponType=type;
            OculusVR::slot=type==WEAPONTYPE_GRENADE ? WEAPONSLOT_GRENADE : WEAPONSLOT_PROJECTILE;
            for(bool hit : {false,true}) {
                collision=hit;segments=draws=0;
                RenderVrThrowableTrajectory(&player,hand);
                assert(draws==1 && segments==(hit ? 1 : 30) && lastHand==hand);
                assert(CWorld::pIgnoreEntity==&ignored);
            }
        }
    }
    const int before=launchCalls;
    OculusVR::preview=false;RenderVrThrowableTrajectory(&player,0);
    OculusVR::preview=true;RenderVrThrowableTrajectory(nullptr,0);
    player.owned=false;RenderVrThrowableTrajectory(&player,0);player.owned=true;
    for(int slot : {-1,(int)TOTAL_WEAPON_SLOTS}) { OculusVR::slot=slot;RenderVrThrowableTrajectory(&player,0); }
    OculusVR::slot=WEAPONSLOT_HANDGUN;
    OculusVR::type=player.weapon.m_eWeaponType=WEAPONTYPE_COLT45;
    RenderVrThrowableTrajectory(&player,0);
    OculusVR::type=-1;RenderVrThrowableTrajectory(&player,0);
    assert(launchCalls==before);
    OculusVR::type=player.weapon.m_eWeaponType=WEAPONTYPE_GRENADE;
    OculusVR::slot=WEAPONSLOT_GRENADE;OculusVR::launch=false;draws=0;
    RenderVrThrowableTrajectory(&player,0);assert(draws==0);
    std::puts("Throwable preview: both hands, grenade/Molotov arcs, collision, ownership and visibility gates passed.");
}
