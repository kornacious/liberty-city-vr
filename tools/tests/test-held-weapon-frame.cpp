#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include "ragdoll-test-vector.h"
#define __ANDROID__ 1
#define GTA_VR_WEAPONS 1
#define nil nullptr
struct CMatrix {
    CVector r{1,0,0},u{0,0,1},f{0,1,0},p;
    CVector &GetRight(){return r;} const CVector &GetRight()const{return r;}
    CVector &GetUp(){return u;} const CVector &GetUp()const{return u;}
    CVector &GetForward(){return f;} const CVector &GetForward()const{return f;}
    CVector &GetPosition(){return p;} const CVector &GetPosition()const{return p;}
};
struct RwFrame {};
enum { PED_IDLE, PED_FALL, PED_GETUP, PED_STAGGER, PED_DIVE_AWAY,
    PED_DIE, PED_DEAD, PED_HANDS_UP, PED_ARRESTED, PED_DRAG_FROM_CAR,
    PED_JUMP, PED_DRIVING, PED_ENTER_CAR, PED_EXIT_CAR };
enum { WBSTATE_PLAYING, WBSTATE_WASTED, WBSTATE_BUSTED, WBSTATE_FAILED_CRITICAL_MISSION };
struct CPed {
    float m_fHealth=100.0f;
    int m_nPedState=PED_IDLE;
    bool skinned=true,vehicle=false;
    void *m_pMyVehicle=nullptr;
    void *GetClump(){return this;} bool IsPlayer(){return true;}
    bool InVehicle(){return vehicle;} CVector GetPosition(){return {};}
};
using CPlayerPed = CPed;
static CPlayerPed *testPlayer;
static CPlayerPed *FindPlayerPed(){return testPlayer;}
namespace CWorld {static int PlayerInFocus=0;struct Info {int m_WBState=WBSTATE_PLAYING;};static Info Players[1];}
static bool IsClumpSkinned(void *p){return static_cast<CPed*>(p)->skinned;}
static RwFrame *RpClumpGetFrame(void *){return nullptr;}
struct RwMatrix {CVector pos;};
static RwMatrix *RwFrameGetMatrix(RwFrame *){static RwMatrix m;return &m;}
static void RwFrameUpdateObjects(RwFrame *){}
static bool gVrFirstPersonActive=true,ikEnabled=true;
static bool gameplay=true,ragdoll=false,twoHands=false,tracking[2]={true,true};
static CMatrix camera,currentAnchor;
static int checks,anchorUpdates,poseSolves;
static void Require(bool ok,const char *message){
    ++checks;if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}
static void Near(CVector a,CVector b,const char *message){Require((a-b).Magnitude()<0.00015f,message);}
void VrUpdateFirstPersonAnchor(bool postPhysics){
    Require(!postPhysics,"IK preparation must not repeat post-physics seat recenter/camera sync");
    ++anchorUpdates;camera=currentAnchor;
}
namespace VrRagdoll {static bool Owns(CPed *){return ragdoll;}}
namespace OculusVR {
static bool IsQuestFirstPersonOnFoot(){return testPlayer && !testPlayer->vehicle;}
static CVector local[2]={{-0.2f,0.6f,-0.2f},{0.2f,0.3f,-0.2f}};
static bool GetQuestRawTrackedHandMatrix(int hand,CMatrix *out){
    if(!tracking[hand])return false;
    *out=camera;
    out->p=camera.p+camera.r*local[hand].x+camera.f*local[hand].y+camera.u*local[hand].z;
    // Controller grip right is the palm up axis in production.
    out->r=camera.u*(hand==1?1.0f:-1.0f);out->u=camera.r;return true;
}
static bool GetQuestRawTrackedHandAimMatrix(int hand,CMatrix *out){
    if(!tracking[hand])return false;*out=camera;return true;
}
static bool IsImmersiveSteeringHandleGrabbed(int){return false;}
}
namespace CCutsceneMgr {
static bool IsRunning(){return !gameplay;}
static bool IsCutsceneProcessing(){return false;}
}
static struct {bool m_WideScreenOn=false;} TheCamera;
namespace androidgame {
static void *VrCutsceneCameraActor(){return nullptr;}
static bool VrKnockdownPresentationActive(){return false;}
}
namespace VrBodyIK {
struct RigidWeaponHandLink {bool valid=false;CVector positionLocal,rightLocal,upLocal,forwardLocal;};
static RigidWeaponHandLink gRigidWeaponLink[2];
static CMatrix gSolvedHandWorld[2];
static bool gSolvedHandValid[2];
static bool gVehicleTransitionActive=false;
static CPed *gHandCarrierPed=nullptr,*gLastPlayer=nullptr;
static bool gHandCarrierValid=false,gHandCarrierWasVehicle=false;
static CMatrix gHandCarrierPrev;
static bool Enabled(){return ikEnabled;}
static bool VehicleEnabled(){return true;}
#include "held-weapon-context.inc"
static void OrthonormaliseBasis(CMatrix &m){m.r.Normalise();m.u.Normalise();m.f.Normalise();}
#include "held-weapon-frame-helpers.inc"
static void EnsureBakedCalibration(){}
static void Reset(){
    gHandCarrierValid=false;gHandCarrierPed=nullptr;gLastPlayer=nullptr;
    for(int i=0;i<2;i++){gSolvedHandValid[i]=false;gRigidWeaponLink[i]=RigidWeaponHandLink();}
}
// Adapter boundary for skeleton storage: production pose conversion, carrier
// adjustment, rigid weapon reconstruction and Apply sequencing run unchanged.
static void Solve(CPed *ped,bool vehicleIK,bool skinned){
    ++poseSolves;
    Near(camera.p,currentAnchor.p,"IK must sample the current locomotion anchor");
    CMatrix desiredWorld[2];bool desiredValid[2];
    bool weaponSupportLocked[2]={twoHands,twoHands};
    CMatrix visualBody=currentAnchor;
    for(int hand=0;hand<2;hand++){
        CMatrix raw;desiredValid[hand]=GetFloatingHandPresentationPose(hand,raw);
        if(!desiredValid[hand])continue;
        desiredWorld[hand]=raw;
        // A nonzero wrist calibration must stay rigid with the gun.
        desiredWorld[hand].p+=raw.u*0.04f;
        CaptureRigidWeaponLink(hand,desiredWorld[hand],raw);
    }
    if(skinned){
#include "held-weapon-carrier-0.inc"
    }else{
#include "held-weapon-carrier-1.inc"
    }
    for(int hand=0;hand<2;hand++){
        if(!desiredValid[hand]){Require(!gSolvedHandValid[hand],"tracking loss cannot expose a previous solve");continue;}
        gSolvedHandWorld[hand]=desiredWorld[hand];gSolvedHandValid[hand]=true;
        CVector p,r,u,f;Require(RebuildRigidWeaponBasis(hand,&p,&r,&u,&f),"rigid weapon basis rebuilt");
        const CVector local=OculusVR::local[hand];
        Near(p,currentAnchor.p+currentAnchor.r*local.x+currentAnchor.f*local.y+currentAnchor.u*local.z,
             "held gun follows locomotion exactly once in both grip modes");
        Near(f,currentAnchor.f,"held gun orientation stays in current frame during turning");
    }
}
static void ApplySkinned(CPed *ped,bool,bool vehicleIK){Solve(ped,vehicleIK,true);}
static void ApplyNonSkinned(CPed *ped,bool,bool vehicleIK){Solve(ped,vehicleIK,false);}
#include "held-weapon-frame-apply.inc"
}
int main(){
    CPed player;testPlayer=&player;
    for(bool skin:{false,true})for(bool two:{false,true})for(float hz:{15.f,30.f,45.f,60.f,72.f,80.f,90.f,120.f,144.f}){
        player.skinned=skin;twoHands=two;camera={};currentAnchor={};VrBodyIK::Reset();
        for(int i=0;i<180;i++){
            // Alternating frame durations, acceleration, running, stop, turning,
            // a teleport and one-hand tracking loss. Input has the old anchor;
            // production Apply must publish the current anchor before solving.
            const float dt=(i%2?1.5f:0.5f)/hz;
            const float speed=i<30?6.f*i/30.f:(i<120?6.f:0.f);
            currentAnchor.p+=CVector(0,speed*dt,0);
            if(i==75)currentAnchor.p+=CVector(20,10,2);
            const float yaw=i*0.02f;
            currentAnchor.r={cosf(yaw),sinf(yaw),0};
            currentAnchor.f={-sinf(yaw),cosf(yaw),0};
            tracking[0]=i!=95;
            VrBodyIK::Apply(&player);
        }
    }
    for(bool skin:{false,true})for(bool vehicle:{false,true}){
        player.skinned=skin;player.vehicle=vehicle;player.m_pMyVehicle=vehicle?&player:nullptr;
        for(int state:{PED_FALL,PED_GETUP,PED_STAGGER,PED_DIVE_AWAY,PED_DIE,PED_DEAD,
                      PED_HANDS_UP,PED_ARRESTED,PED_DRAG_FROM_CAR}){
            VrBodyIK::gSolvedHandValid[0]=true;VrBodyIK::gRigidWeaponLink[0].valid=true;
            VrBodyIK::gHandCarrierValid=true;player.m_nPedState=state;
            const int count=anchorUpdates;
            const int solves=poseSolves;
            Require(!VrBodyIK::WeaponOwnershipActive(),"native full-body animation releases weapon pose ownership");
            VrBodyIK::Apply(&player);
            Require(anchorUpdates==count,"native full-body animation cannot be pinned to HMD");
            Require(poseSolves==solves,"native animation bypasses both skeleton solvers");
            Require(!VrBodyIK::gSolvedHandValid[0]&&!VrBodyIK::gRigidWeaponLink[0].valid&&
                    !VrBodyIK::gHandCarrierValid,"suspension clears stale tracked poses");
            player.m_nPedState=PED_IDLE;VrBodyIK::Apply(&player);
            Require(poseSolves==solves+1,"recovery resumes on-foot or vehicle IK");
            Require(anchorUpdates==count+(vehicle?0:1),"recovery preserves anchor update ownership");
        }
    }
    player.vehicle=false;player.m_pMyVehicle=nullptr;
    for(int wb:{WBSTATE_WASTED,WBSTATE_BUSTED,WBSTATE_FAILED_CRITICAL_MISSION}){
        CWorld::Players[0].m_WBState=wb;
        Require(!VrBodyIK::WeaponOwnershipActive(),"restart sequence releases IK even with idle ped state");
        const int count=anchorUpdates;VrBodyIK::Apply(&player);
        Require(anchorUpdates==count,"restart sequence keeps native pose");
    }
    CWorld::Players[0].m_WBState=WBSTATE_PLAYING;
    player.m_fHealth=0;Require(!VrBodyIK::WeaponOwnershipActive(),"zero health releases IK before death state");
    const int healthSolves=poseSolves;VrBodyIK::Apply(&player);
    Require(poseSolves==healthSolves,"zero health bypasses skeleton solve before death state");
    player.m_fHealth=100;
    for(int state:{PED_IDLE,PED_JUMP,PED_DRIVING,PED_ENTER_CAR,PED_EXIT_CAR}){
        player.m_nPedState=state;Require(VrBodyIK::WeaponOwnershipActive(),"locomotion and vehicle states retain existing ownership");
    }
    player.m_nPedState=PED_IDLE;
    const int before=anchorUpdates;
    gameplay=false;VrBodyIK::Apply(&player);gameplay=true;
    ragdoll=true;VrBodyIK::Apply(&player);ragdoll=false;
    ikEnabled=false;VrBodyIK::Apply(&player);ikEnabled=true;
    VrBodyIK::Apply(nullptr);
    Require(anchorUpdates==before,"inactive IK, cutscenes and ragdolls cannot change the anchor");
    std::printf("Held weapon frame: %d checks passed across 9 rates, both skeleton paths and grip modes.\n",checks);
}
