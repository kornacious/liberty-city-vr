#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <vector>
#include <limits>
#include "ragdoll-test-vector.h"
#include "weapon-cadence-types.inc"
#include "weapon-cadence-clock.inc"
using uint32=uint32_t;using int32=int32_t;using uint8=uint8_t;
#define AEHANDLE_IS_OK(a) ((a)>=0)
#define debug(...) ((void)0)
template<class T>T Max(T a,T b){return std::max(a,b);}
static int checks;
static void Require(bool b,const char *m){++checks;if(!b){std::fprintf(stderr,"FAIL: %s\n",m);std::exit(1);}}
static uint32 testNow=1000,previous=1000;
struct CTimer {static uint32 GetTimeInMilliseconds(){return testNow;}
    static uint32 GetPreviousTimeInMilliseconds(){return previous;}};
struct CWeaponInfo {
    float m_fAnimLoopStart=0,m_fAnimLoopEnd=0.1f;
    uint32 m_nFiringRate=250,m_nReload=800;
    int m_nAmountofAmmunition=10000;
    eWeaponFire m_eWeaponFire=WEAPON_FIRE_INSTANT_HIT;
    static CWeaponInfo *GetWeaponInfo(eWeaponType);
};
static CWeaponInfo infos[WEAPONTYPE_TOTALWEAPONS];
CWeaponInfo *CWeaponInfo::GetWeaponInfo(eWeaponType type){return &infos[type];}
struct CEntity {};
struct CPhysical:CEntity {int m_audioEntityId=0;};
struct CPlayerPed;
static CPlayerPed *FindPlayerPed();
struct CWorld {struct Player {bool m_bFastReload=false;};static Player Players[1];static int PlayerInFocus;};
CWorld::Player CWorld::Players[1];int CWorld::PlayerInFocus=0;
struct CStats {static int RoundsFiredByPlayer;};int CStats::RoundsFiredByPlayer=0;
enum {SOUND_WEAPON_RELOAD,SOUND_WEAPON_FLAMETHROWER_FIRE};
struct Audio {void PlayOneShot(int,int,float){}};static Audio DMAudio;
static uint32 gReloadSampleTime[WEAPONTYPE_TOTALWEAPONS]={};
static bool fireFailure=false;
static std::vector<uint32> shots[WEAPONTYPE_TOTALWEAPONS];
struct CWeapon {
    eWeaponType m_eWeaponType=WEAPONTYPE_UZI;
    eWeaponState m_eWeaponState=WEAPONSTATE_READY;
    int m_nAmmoInClip=10000,m_nAmmoTotal=20000;uint32 m_nTimer=0;
    CWeaponInfo *GetInfo(){return &infos[m_eWeaponType];}
    void Update(int32);void Reload();bool Fire(CEntity *,CVector *);
};
struct CPlayerPed:CPhysical {
    CWeapon weapons[WEAPONTYPE_TOTALWEAPONS];
    bool HasWeapon(uint8){return true;} CWeapon &GetWeapon(uint8 type){return weapons[type];}
    void OnFoot();void InCar();
};
static CPlayerPed fixturePlayer;
static CPlayerPed *FindPlayerPed(){return &fixturePlayer;}
bool CWeapon::Fire(CEntity *shooter,CVector *){
    if(fireFailure || (m_eWeaponState!=WEAPONSTATE_READY&&m_eWeaponState!=WEAPONSTATE_FIRING)||m_nAmmoInClip<=0)return false;
    shots[m_eWeaponType].push_back(testNow);const bool isPlayer=true;
    // Actual re3 successful-shot ammo transaction and reload timer ownership.
#include "weapon-cadence-ammo.inc"
    return true;
}
#include "weapon-cadence-update.inc"
static bool pressed[2],edge[2],aimValid[2],manual=false,reloadRequest=false,manualAvailable=false;
static int held[2];
static bool vrWaitForTriggerRelease[2];

namespace OculusVR {
static int GetHeldWeaponSlot(int h){return held[h];}
static int GetVrWeaponTypeForSlot(int s){return s;}
static bool IsImmersiveVehicleSidearm(int t){return t==WEAPONTYPE_UZI||t==WEAPONTYPE_COLT45;}
static bool IsTrackedWeaponTriggerPressed(int h){return pressed[h];}
static bool IsTrackedWeaponTriggerJustPressed(int h){return edge[h];}
static bool IsTrackedWeaponFireTriggerPressed(int h,int){return pressed[h];}
static bool IsTrackedWeaponFireTriggerJustPressed(int h,int){return edge[h];}
static bool GetTrackedWeaponAim(int h,int,CVector*,CVector*){return aimValid[h];}
static bool ShouldUseManualReload(){return manual;}
static bool IsManualReloadWeaponType(int t){return t!=WEAPONTYPE_FLAMETHROWER;}
static void SetManualReloadWeaponState(int,int,int,bool active){manualAvailable=active;}
static bool ConsumeManualReloadRequest(int,int,int){bool r=reloadRequest;reloadRequest=false;return r;}
static void BeginTrackedWeaponFire(int,int,CVector,CVector){}
static void EndTrackedWeaponFire(){}
}
void CPlayerPed::OnFoot(){
    const uint32 vrNow=testNow;
    for(int hand=0;hand<2;hand++){
        const int heldSlot=held[hand];if(heldSlot<1)continue;
        CWeapon &vrHeldWeapon=weapons[heldSlot];const eWeaponType vrWeaponType=vrHeldWeapon.m_eWeaponType;
#include "weapon-cadence-player.inc"
}
void CPlayerPed::InCar(){
    CPlayerPed *player=this;
#include "weapon-cadence-car.inc"
}
static void Reset(eWeaponType type,int span=3,int capacity=10000){
    testNow=previous=1000;manual=reloadRequest=manualAvailable=fireFailure=false;
    for(int i=0;i<WEAPONTYPE_TOTALWEAPONS;i++){
        QuestWeaponTiming::ForWeapon<WEAPONTYPE_TOTALWEAPONS>(i)={};infos[i]={};shots[i].clear();
        fixturePlayer.weapons[i]={};fixturePlayer.weapons[i].m_eWeaponType=static_cast<eWeaponType>(i);
    }
    infos[type].m_fAnimLoopStart=8.f/30.f;
    infos[type].m_fAnimLoopEnd=(8.f+span)/30.f;
    infos[type].m_nAmountofAmmunition=capacity;
    fixturePlayer.weapons[type].m_nAmmoInClip=capacity;
    for(int h=0;h<2;h++){held[h]=-1;pressed[h]=edge[h]=false;aimValid[h]=true;vrWaitForTriggerRelease[h]=false;}
    held[1]=type;pressed[1]=edge[1]=true;
}
static void Tick(uint32 t,bool car=false){
    previous=testNow;testNow=t;
    for(int i=0;i<WEAPONTYPE_TOTALWEAPONS;i++)
        if(!(manual&&fixturePlayer.weapons[i].m_nAmmoInClip==0))fixturePlayer.weapons[i].Update(-1);
    if(car)fixturePlayer.InCar();else fixturePlayer.OnFoot();
    edge[0]=edge[1]=false;
}
int main(){
    // Synthetic loop widths exercise each firearm through the production
    // integration without distributing any original weapon.dat records.
    const eWeaponType automatic[]={WEAPONTYPE_UZI,WEAPONTYPE_AK47,WEAPONTYPE_M16,WEAPONTYPE_FLAMETHROWER,WEAPONTYPE_HELICANNON};
    for(auto type:automatic)for(int span:{1,2,3,7,36})for(double hz:{15.,30.,45.,60.,72.,80.,90.,120.,144.,180.})for(bool jitter:{false,true}){
        Reset(type,span);const double interval=QuestWeaponTiming::AnimationIntervalMs(infos[type].m_fAnimLoopStart,infos[type].m_fAnimLoopEnd,1);
        Tick(1000);double time=0;
        while(time<10000){time=std::min(10000.,time+(1000./hz)*(jitter?(static_cast<int>(time)%2?1.4:0.6):1.));Tick(1000+static_cast<uint32>(time+0.5));}
        const int expected=static_cast<int>(10000./interval)+1;
        Require(std::abs(static_cast<int>(shots[type].size())-expected)<=1,"automatic shot count follows loop time across FPS");
        Require(fixturePlayer.weapons[type].m_nAmmoInClip==10000-static_cast<int>(shots[type].size()),"one ammo consumed per shot");
        for(uint32 stamp:shots[type])Require(stamp>=1000,"valid shot timestamp");
    }
    for(auto type:{WEAPONTYPE_COLT45,WEAPONTYPE_SHOTGUN,WEAPONTYPE_SNIPERRIFLE,WEAPONTYPE_ROCKETLAUNCHER}){
        Reset(type,7);Tick(1000);for(uint32 t=1005;t<3000;t+=5)Tick(t);
        Require(shots[type].size()==1,"holding a semi-auto trigger cannot repeat");
        Reset(type,7);Tick(1000);pressed[1]=false;Tick(1005);pressed[1]=edge[1]=true;Tick(1010);
        Require(shots[type].size()==1,"trigger taps cannot bypass cooldown");
        pressed[1]=false;Tick(3000);pressed[1]=edge[1]=true;Tick(3010);
        Require(shots[type].size()==2,"new semi-auto press works after cooldown");
    }
    for(auto type:{WEAPONTYPE_UZI,WEAPONTYPE_AK47,WEAPONTYPE_M16})for(double hz:{15.,30.,72.,90.,180.}){
        const int span=type==WEAPONTYPE_UZI?3:(type==WEAPONTYPE_AK47?2:1);
        Reset(type,span,60);
        for(int frame=0;shots[type].size()<60&&frame<3000;frame++)Tick(1000+static_cast<uint32>(frame*1000./hz+0.5));
        Require(shots[type].size()==60,"controlled magazine depletion reaches exactly 60 rounds");
        const double expected=59.*QuestWeaponTiming::AnimationIntervalMs(infos[type].m_fAnimLoopStart,infos[type].m_fAnimLoopEnd,1);
        Require(std::abs(double(shots[type].back()-shots[type].front())-expected)<1000./hz+1.,"magazine depletion duration stays within one frame of loop cadence");
    }
    Reset(WEAPONTYPE_UZI,3);held[0]=WEAPONTYPE_AK47;pressed[0]=edge[0]=true;
    for(uint32 t=1000;t<=2000;t+=10)Tick(t);
    Require(shots[WEAPONTYPE_UZI].size()==11&&shots[WEAPONTYPE_AK47].size()==11,"two held guns have independent inventory clocks");
    Reset(WEAPONTYPE_M16,1);Tick(1000);Tick(1050);const auto count=shots[WEAPONTYPE_M16].size();
    Tick(5000);Require(shots[WEAPONTYPE_M16].size()==count+1,"long stall cannot create a catch-up burst");
    aimValid[1]=false;Tick(5100);aimValid[1]=true;Tick(5200);
    Require(shots[WEAPONTYPE_M16].size()==count+2,"tracking recovery cannot accumulate missed shots");
    held[0]=held[1];held[1]=-1;pressed[0]=edge[0]=true;Tick(5201);
    Require(shots[WEAPONTYPE_M16].size()==count+2,"passing a gun between hands preserves its cooldown");
    Reset(WEAPONTYPE_UZI,3,2);Tick(1000);Tick(1100);
    Require(fixturePlayer.weapons[WEAPONTYPE_UZI].m_eWeaponState==WEAPONSTATE_RELOADING,"last round starts native reload");
    const uint32 reloadDue=fixturePlayer.weapons[WEAPONTYPE_UZI].m_nTimer;Tick(reloadDue-1);
    Require(shots[WEAPONTYPE_UZI].size()==2,"auto reload blocks shots until native deadline");
    Tick(reloadDue+1);Require(shots[WEAPONTYPE_UZI].size()==3,"reload resumes with one immediate shot");
    Reset(WEAPONTYPE_UZI,3,2);manual=true;Tick(1000);Tick(1100);Tick(3000);
    Require(shots[WEAPONTYPE_UZI].size()==2&&manualAvailable,"empty manual magazine stays blocked");
    reloadRequest=true;Tick(3010);Require(shots[WEAPONTYPE_UZI].size()==2,"manual insertion cannot fire while trigger held");
    pressed[1]=false;Tick(3020);pressed[1]=edge[1]=true;Tick(3030);
    Require(shots[WEAPONTYPE_UZI].size()==3,"release and repress resumes after manual reload");
    for(double hz:{15.,30.,72.,90.,180.}){
        Reset(WEAPONTYPE_UZI,3);for(double t=0;t<=7000;t+=1000./hz)Tick(1000+static_cast<uint32>(t+0.5),true);
        Require(std::abs(static_cast<int>(shots[WEAPONTYPE_UZI].size())-100)<=1,"vehicle Uzi retains native 70 ms cadence");
        Reset(WEAPONTYPE_UZI,3,2);Tick(1000,true);Tick(1070,true);
        Require(fixturePlayer.weapons[WEAPONTYPE_UZI].m_nTimer==1870,"vehicle firing cannot replace native reload deadline");
        Tick(1800,true);Require(shots[WEAPONTYPE_UZI].size()==2,"vehicle reload blocks early shots");
        Tick(1871,true);Require(shots[WEAPONTYPE_UZI].size()==3,"vehicle reload completes normally");
    }
    QuestWeaponTiming::Cadence wrap;wrap.Update(UINT32_MAX-10,true);wrap.Fired(100);wrap.Update(9,true);
    Require(!wrap.Due()&&std::abs(wrap.remainingMs-80)<0.001,"timer wrap preserves cooldown");
    Require(QuestWeaponTiming::AnimationIntervalMs(1,1,250)==250,"invalid loop uses explicit fallback");
    Require(QuestWeaponTiming::AnimationIntervalMs(1,std::numeric_limits<float>::quiet_NaN(),0)==1,"NaN data cannot spin firing loop");
    Reset(WEAPONTYPE_M16,1);fireFailure=true;Tick(1000);Tick(1100);
    Require(shots[WEAPONTYPE_M16].empty(),"failed shots do not consume ammo");
    std::printf("Weapon cadence: %d checks passed; 10 rates, variable pacing, all gun types, native ammo/reload integration.\n",checks);
}
