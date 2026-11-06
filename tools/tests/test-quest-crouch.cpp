#include <cstdio>
#include <cstdlib>
#include "ragdoll-test-vector.h"
#define GTA_VR_WEAPONS 1
#define RW_VULKAN 1
#define __ANDROID__ 1
#define nil nullptr
namespace androidgame {static bool fixed=false;static bool VrUsesFixedButtonCrouch(){return fixed;}}
struct CTimer {static unsigned GetTimeInMilliseconds(){return 1000;}};
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
    CVector &GetRight(){return r;}
    CVector &GetUp(){return u;}
    CVector &GetForward(){return f;}
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
enum { BONE_upperlegl, BONE_upperlegr, BONE_waist, BONE_lowerlegl, BONE_footl, BONE_lowerlegr, BONE_footr };
struct RpHAnimHierarchy { int numNodes; };
int RpHAnimIDGetIndex(RpHAnimHierarchy *,int tag){return tag;}
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
    androidgame::fixed=true;pad={};pad.press=true;pad.x=1;
    ProcessQuestCrouch(&player,&pad);Check(player.bIsDucking,"Xbox button crouch can enter while moving");
    pad.jump=true;ProcessQuestCrouch(&player,&pad);
    Check(player.bIsDucking&&player.m_duckTimer==61000,"Xbox crouch remains toggled and refreshes native timer despite other input");
    pad.press=true;ProcessQuestCrouch(&player,&pad);standing();androidgame::fixed=false;
    CMatrix visual;visual.r={0,1,0};visual.f={-1,0,0};visual.p={3,4,5};
    RwMatrix bones[2]={{visual.p+CVector(-0.1f,0,-0.6f),{1,0,0},{0,1,0},{0,0,1}},
                       {visual.p+CVector(0.1f,0,-0.6f),{1,0,0},{0,0,1},{0,-1,0}}};
    RpHAnimHierarchy hierarchy{2};

    player.bIsDucking=true;AlignGroundedSkeleton(&player,&hierarchy,bones,visual);
    Near(bones[0].pos,visual.p+CVector(0,-0.1f,-0.6f),"legs follow view yaw around translated root and retain crouch height");
    Near(bones[1].pos,visual.p+CVector(0,0.1f,-0.6f),"both hips follow the same heading");
    Near(bones[0].right,visual.r,"leg basis follows visual body");Near(bones[1].up,{0,0,1},"authored crouch pitch is preserved");
    // Exercise an authored pelvis yaw independent of the entity matrix.
    // The original entity-to-view transform fails every quarter-turn case.
    for(int view=0;view<8;view++)for(int pose=0;pose<8;pose++){
        const float v=view*0.785398163f,a=pose*0.785398163f;
        visual.r={std::cos(v),std::sin(v),0};visual.f={-std::sin(v),std::cos(v),0};
        const CVector hipRight{std::cos(a),std::sin(a),0};
        const CVector hipForward{-std::sin(a),std::cos(a),0};
        bones[0]={visual.p-hipRight*0.15f+CVector(0,0,-0.3f),hipRight,hipForward,{0,0,1}};
        bones[1]={visual.p+hipRight*0.15f+CVector(0,0,-0.3f),hipRight,hipForward,{0,0,1}};
        CMatrix measured;
        Check(BuildCrouchPoseBasis(bones[0].pos,bones[1].pos,visual,measured),"hip heading resolves");
        RwMatrix clumpRoot{{},player.body.r,player.body.f,player.body.u};
        AlignCrouchAxes(clumpRoot,measured,visual);
        // Non-skinned animation keeps its authored pose below the root.
        Near(clumpRoot.right*hipRight.x+clumpRoot.up*hipRight.y,visual.r,"non-skinned authored hip span aligns with torso");
        AlignGroundedSkeleton(&player,&hierarchy,bones,visual);
        Near(bones[1].pos-bones[0].pos,visual.r*0.3f,"skinned hip span aligns with torso despite authored root yaw");
        Near(bones[0].up,visual.f,"authored forward follows torso heading");
        Near(bones[0].at,visual.u,"crouch vertical axis is preserved");
    }
    CMatrix invalid;
    Check(!BuildCrouchPoseBasis({0,0,0},{0,0,1},visual,invalid),"degenerate hip span is rejected");
    player.bIsDucking=false;
    // Physical turning changes the visual heading without changing the entity.
    // Include an animated foot lift/pitch and a translated roomscale root.
    for(int view=0;view<8;view++)for(int native=0;native<8;native++){
        const float v=view*0.785398163f,n=native*0.785398163f;
        visual.r={std::cos(v),std::sin(v),0};visual.f={-std::sin(v),std::cos(v),0};
        player.body.r={std::cos(n),std::sin(n),0};player.body.f={-std::sin(n),std::cos(n),0};
        RwMatrix standingBones[8];
        const CVector offset{0.2f,-0.7f,0.25f};
        const CVector pitchedForward{0,0.6f,0.8f};
        for(auto &bone:standingBones)
            bone={visual.p+FromBasisLocal(offset,player.body),player.body.r,
                FromBasisLocal(pitchedForward,player.body),player.body.u};
        RpHAnimHierarchy standingHierarchy{8};
        const RwMatrix upper=standingBones[7];
        AlignGroundedSkeleton(&player,&standingHierarchy,standingBones,visual);
        for(int i=0;i<7;i++){
            Near(standingBones[i].pos,visual.p+FromBasisLocal(offset,visual),"standing pelvis and legs follow headset yaw with gait and roomscale offset");
            Near(standingBones[i].right,visual.r,"standing lower-body axes follow headset yaw");
            Near(standingBones[i].up,FromBasisLocal(pitchedForward,visual),"animated foot pitch is preserved");
        }
        Near(standingBones[7].pos,upper.pos,"upper-body solve position remains untouched");
        Near(standingBones[7].right,upper.right,"upper-body solve orientation remains untouched");
        RwMatrix root{{},player.body.r,player.body.f,player.body.u};
        AlignCrouchAxes(root,player.body,visual);
        Near(root.right,visual.r,"standing non-skinned root follows headset yaw");
        Near(root.up,visual.f,"standing non-skinned forward follows headset yaw");
    }
    std::printf("Quest crouch: %d checks passed\n",checks);
}
