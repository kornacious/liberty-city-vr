#include <cstdio>
#include <cstdlib>
#include "ragdoll-test-vector.h"
#define GTA_VR_WEAPONS 1
#define RW_VULKAN 1
#define __ANDROID__ 1
#define nil nullptr
enum { PED_IDLE, PED_JUMP, PEDMOVE_STILL, PEDMOVE_SPRINT,
       ANIM_STD_DUCK_DOWN, ANIM_STD_DUCK_LOW, ASSOC_DELETEFADEDOUT=16 };
struct CAnimBlendAssociation { float blendDelta=4; int flags=0; };
static CAnimBlendAssociation testAnimations[2];
static CAnimBlendAssociation *RpAnimBlendClumpGetAssociation(void *, int id)
{ return &testAnimations[id==ANIM_STD_DUCK_LOW ? 1 : 0]; }
struct CMatrix {
    CVector r{1,0,0}, u{0,0,1}, f{0,1,0}, p;
    const CVector &GetRight()const{return r;}
    const CVector &GetUp()const{return u;}
    const CVector &GetForward()const{return f;}
    const CVector &GetPosition()const{return p;}
};
struct CPed {
    bool bIsDucking=false; CMatrix body;
    const CMatrix &GetMatrix()const{return body;}
};
struct CPlayerPed : CPed {
    bool bCrouchWhenShooting=false,bIsInTheAir=false,vehicle=false;
    int m_nPedState=PED_IDLE,m_nMoveState=PEDMOVE_STILL,m_duckTimer=0;
    void *GetClump(){return nullptr;}
    bool InVehicle(){return vehicle;}
    void SetDuck(int time){if(!bIsDucking && m_duckTimer==0){bIsDucking=true;m_duckTimer=time;}}
};
struct CPad {
    bool press=false,jump=false; int x=0,y=0;
    bool DuckJustDown(){bool result=press;press=false;return result;}
    bool JumpJustDown(){return jump;}
    int GetPedWalkLeftRight(){return x;}
    int GetPedWalkUpDown(){return y;}
};
struct RwMatrix { CVector pos,right,up,at; };
struct RpHAnimHierarchy { int numNodes; };
#include "../../overlay/src/peds/QuestCrouch.h"
#include "quest-crouch-production.inc"
static int checks;
static void Check(bool value,const char *message){
    ++checks;if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}
static void Near(CVector a,CVector b,const char *message){Check((a-b).Magnitude()<0.0001f,message);}
int main(){
    CPlayerPed player; CPad pad;
    auto enter=[&](){pad={};pad.press=true;ProcessQuestCrouch(&player,&pad);Check(player.bIsDucking,"crouch enters immediately");};
    auto standing=[&](){Check(!player.bIsDucking && player.m_duckTimer==0 && !player.bCrouchWhenShooting,"standing clears camera source and timer");
        for(auto &anim:testAnimations)Check(anim.blendDelta<0 && (anim.flags&ASSOC_DELETEFADEDOUT),"both duck animations fade and delete");};
    enter();ProcessQuestCrouch(&player,&pad);Check(player.bIsDucking,"duplicate controller query does not undo a press");
    pad.press=true;ProcessQuestCrouch(&player,&pad);standing();
    for(int direction=0;direction<4;direction++){
        enter();pad.x=direction<2 ? (direction==0?1:-1) : 0;pad.y=direction>=2 ? (direction==2?1:-1) : 0;
        ProcessQuestCrouch(&player,&pad);standing();
    }
    enter();pad.jump=true;ProcessQuestCrouch(&player,&pad);standing();
    enter();player.bIsInTheAir=true;ProcessQuestCrouch(&player,&pad);standing();player.bIsInTheAir=false;
    enter();player.m_nPedState=PED_JUMP;ProcessQuestCrouch(&player,&pad);standing();player.m_nPedState=PED_IDLE;
    pad={};pad.press=true;pad.jump=true;ProcessQuestCrouch(&player,&pad);Check(!player.bIsDucking,"jump wins simultaneous crouch press");
    pad={};pad.press=true;pad.x=1;ProcessQuestCrouch(&player,&pad);Check(!player.bIsDucking,"movement wins simultaneous crouch press");
    pad={};pad.press=true;player.vehicle=true;ProcessQuestCrouch(&player,&pad);Check(!player.bIsDucking,"no vehicle crouch");player.vehicle=false;
    CMatrix visual;visual.r={0,1,0};visual.f={-1,0,0};visual.p={3,4,5};
    RwMatrix bones[2]={{visual.p+CVector(1,0,-0.6f),{1,0,0},{0,1,0},{0,0,1}},
                       {visual.p+CVector(0,1,0.2f),{1,0,0},{0,0,1},{0,-1,0}}};
    RpHAnimHierarchy hierarchy{2};
    AlignCrouchedSkeleton(&player,&hierarchy,bones,visual);Near(bones[0].right,{1,0,0},"standing skeleton remains untouched");
    player.bIsDucking=true;AlignCrouchedSkeleton(&player,&hierarchy,bones,visual);
    Near(bones[0].pos,visual.p+CVector(0,1,-0.6f),"legs follow view yaw around translated root and retain crouch height");
    Near(bones[1].pos,visual.p+CVector(-1,0,0.2f),"torso follows same yaw as legs");
    Near(bones[0].right,visual.r,"leg basis follows visual body");Near(bones[1].up,{0,0,1},"authored crouch pitch is preserved");
    std::printf("Quest crouch: %d checks passed\n",checks);
}
