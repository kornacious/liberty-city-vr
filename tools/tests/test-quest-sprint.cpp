#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <cmath>
#include "QuestMovementDirection.h"
#include "quest-sprint-weapon-types.inc"
#include <cstdint>
using uint8=uint8_t;
#define GTA_VR_WEAPONS
#define RW_VULKAN
#define __ANDROID__
#define nil nullptr
#define PC_PLAYER_CONTROLS
#define DEGTORAD(a) ((a)*0.017453292519943295f)
static constexpr float PI=3.14159265358979323846f;
#include "quest-sprint-anims.inc"
static int checks;
static void Require(bool v,const char *label){++checks;if(!v){std::fprintf(stderr,"FAIL: %s\n",label);std::exit(1);}}
enum { WEAPONFLAG_HEAVY=1,WEAPONFLAG_CANAIM_WITHARM=2,PED_IDLE=0,PED_ATTACK=1,PED_JUMP=2,PEDMOVE_STILL=0 };
enum AnimationId { ANIM_STD_WALK,ANIM_STD_RUN,ANIM_STD_RUNFAST,ANIM_STD_STARTWALK,ANIM_STD_RUNSTOP1,ANIM_STD_RUNSTOP2,ANIM_STD_IDLE };
enum { ASSOC_RUNNING=1,ASSOC_DELETEFADEDOUT=2 };
struct CAnimBlendAssociation {
    int flags=ASSOC_RUNNING;float blendDelta=0;
    bool IsRunning()const{return (flags&ASSOC_RUNNING)!=0;}void SetRun(){flags|=ASSOC_RUNNING;}
};
static CAnimBlendAssociation animations[7];
static CAnimBlendAssociation *RpAnimBlendClumpGetAssociation(void *,AnimationId id){return &animations[id];}
static int idleBlends;
static AssocGroupId idleGroup;
namespace CAnimManager {
static void BlendAnimation(void *,AssocGroupId group,AnimationId id,float rate){
    Require(id==ANIM_STD_IDLE && rate==8,"VR release crossfades to idle");++idleBlends;idleGroup=group;
}
}
struct CWeaponInfo {
    int flags=0;
    bool IsFlagSet(int f)const{return (flags&f)!=0;}
    static CWeaponInfo *GetWeaponInfo(eWeaponType);
};
static CWeaponInfo infos[WEAPONTYPE_TOTALWEAPONS];
CWeaponInfo *CWeaponInfo::GetWeaponInfo(eWeaponType t){return &infos[t];}
struct CWeapon {
    eWeaponType m_eWeaponType=WEAPONTYPE_UNARMED;
    bool twoHanded=false;
    bool IsType2Handed()const{return twoHanded;}
};
struct CPad {
    int horizontal=0,vertical=0;bool sprint=false;
    short GetPedWalkLeftRight()const{return short(horizontal);}
    short GetPedWalkUpDown()const{return short(vertical);}
    bool GetSprint()const{return sprint;}
    static CPad *GetPad(int);
};
static CPad velocityPad;
CPad *CPad::GetPad(int){return &velocityPad;}
static bool directionalEnabled=false;
static float questAnimationAngle;
namespace androidgame {
static bool VrGetOnFootAnimationAngle(float *out){if(!directionalEnabled)return false;if(out)*out=questAnimationAngle;return true;}
}
static bool cheat=false,physical=true;
static int held[2]={-1,-1}, fixtureWeaponType=WEAPONTYPE_UNARMED;
namespace OculusVR {
static bool IsRunWithoutLimitsEnabled(){return cheat;}
static bool IsPhysicalWeaponInteractionActive(){return physical;}
static int GetHeldWeaponSlot(int h){return held[h];}
static int GetVrWeaponTypeForSlot(int){return fixtureWeaponType;}
}
struct CVector2D {
    float x,y;
    CVector2D(float a=0,float b=0):x(a),y(b){}
    float Magnitude()const{return std::sqrt(x*x+y*y);}
    CVector2D operator*(float s)const{return {x*s,y*s};}
};
static float Sin(float x){return std::sin(x);}static float Cos(float x){return std::cos(x);}
static float Atan2(float x,float y){return std::atan2(x,y);}
struct FrontAxis {float heading=0;float Heading()const{return heading;}};
namespace CGeneral {
static float LimitRadianAngle(float x){while(x>PI)x-=2*PI;while(x<-PI)x+=2*PI;return x;}
#include "quest-sprint-angle.inc"
}
struct CameraMode{bool mouse=true;FrontAxis Front;float m_fPlayerVelocity=0;bool Using3rdPersonMouseCam()const{return mouse;}};
static struct Camera{CameraMode Cams[1];int ActiveCam=0;} TheCamera;
struct CPlayerPed {
    CWeapon weapons[WEAPONTYPE_TOTALWEAPONS];int current=WEAPONSLOT_MELEE,rebuilt=0;
    int m_nPedState=PED_IDLE;void *m_pPointGunAt=nullptr;bool bIsDucking=false;
    float m_fWalkAngle=0;bool canStrafe=true;AssocGroupId m_animGroup=ASSOCGRP_PLAYER;
    CWeapon *GetWeapon(){return &weapons[current];}
    CWeapon &GetWeapon(int type){return weapons[type];}
    bool HasWeapon(uint8 type)const{return type<WEAPONTYPE_TOTALWEAPONS;}
    void ReApplyMoveAnims(){++rebuilt;}
    bool CanStrafeOrMouseControl()const{return canStrafe;}
    bool vehicle=false,bIsInTheAir=false;
    bool InVehicle()const{return vehicle;}bool IsPlayer()const{return true;}
    float m_fMoveSpeed=0;
    int m_nMoveState=PEDMOVE_STILL,headingRestores=0;
    void RestoreHeadingRate(){++headingRestores;}
    CVector2D m_moved;
    float WorkOutHeadingForMovingFirstPerson(float);
    void *GetClump(){return nullptr;}
    int GetFireAnimGround(CWeaponInfo*,bool){return 0;}
    int GetMeleeStartAnim(CWeaponInfo*){return 0;}
    void ProcessAnimGroups();
};
using CPed=CPlayerPed;
static CPed *velocityPlayer;
static CPed *FindPlayerPed(){return velocityPlayer;}
#include "quest-sprint-production.inc"
static const char *SprintName(AssocGroupId group){
    switch(group){
    case ASSOCGRP_PLAYER:return aPlayerAnimations[2];
    case ASSOCGRP_PLAYERROCKET:return aPlayerWithRocketAnimations[2];
    case ASSOCGRP_PLAYER1ARMED:return aPlayer1ArmedAnimations[2];
    case ASSOCGRP_PLAYER2ARMED:return aPlayer2ArmedAnimations[2];
    case ASSOCGRP_PLAYERBBBAT:return aPlayerBBBatAnimations[2];
    case ASSOCGRP_PLAYERBACK:return aPlayerStrafeBackAnimations[2];
    case ASSOCGRP_PLAYERLEFT:return aPlayerStrafeLeftAnimations[2];
    case ASSOCGRP_PLAYERRIGHT:return aPlayerStrafeRightAnimations[2];
    default:return "weapon strafe";
    }
}
int main(){
    CPlayerPed player;CPad pad;pad.vertical=128;
    for(eWeaponType weapon:{WEAPONTYPE_SHOTGUN,WEAPONTYPE_ROCKETLAUNCHER,
        WEAPONTYPE_FLAMETHROWER,WEAPONTYPE_BASEBALLBAT,WEAPONTYPE_M16,WEAPONTYPE_COLT45}){
        const bool restricted=weapon==WEAPONTYPE_ROCKETLAUNCHER||weapon==WEAPONTYPE_FLAMETHROWER;
        infos[weapon].flags=restricted?WEAPONFLAG_HEAVY:0;
        player.current=int(weapon); fixtureWeaponType=weapon;
        player.GetWeapon()->m_eWeaponType=weapon;
        player.GetWeapon()->twoHanded=weapon>=WEAPONTYPE_SHOTGUN && weapon<=WEAPONTYPE_FLAMETHROWER && weapon!=WEAPONTYPE_ROCKETLAUNCHER;
        for(bool tracked:{false,true})
        for(int hand=0;hand<2;++hand){
            physical=tracked;
            held[0]=held[1]=-1;held[hand]=WEAPONSLOT_MELEE;
            for(int mode=0;mode<3;++mode)
            for(float angle:{0.0f,DEGTORAD(90.0f),DEGTORAD(-90.0f),DEGTORAD(180.0f)}){
                TheCamera.Cams[0].mouse=mode!=1;player.canStrafe=mode!=2;
                player.m_fWalkAngle=angle;cheat=true;player.m_animGroup=ASSOCGRP_PLAYERROCKET;
                player.m_nPedState=PED_ATTACK;player.m_pPointGunAt=&player;
                player.ProcessAnimGroups();
                const AssocGroupId expected=mode!=0||angle==0?ASSOCGRP_PLAYER:
                    angle==DEGTORAD(90.0f)?ASSOCGRP_PLAYERLEFT:
                    angle==DEGTORAD(-90.0f)?ASSOCGRP_PLAYERRIGHT:ASSOCGRP_PLAYERBACK;
                Require(player.m_animGroup==expected,"cheat preserves unarmed forward/left/right/back movement group");
                const char *motion=expected==ASSOCGRP_PLAYER?"SPRINT_civi":
                    expected==ASSOCGRP_PLAYERLEFT?"run_left":
                    expected==ASSOCGRP_PLAYERRIGHT?"run_right":"run_player_back";
                Require(std::strcmp(SprintName(player.m_animGroup),motion)==0,"actual root-motion mapping retains requested direction");
                Require(ShouldPlayerSprint(&player,&pad),"cheat requests sprint on movement without button");
                Require(!IsHeavyWeaponMobilityRestricted(&player),"cheat bypasses held heavy flag");
                Require(!MovementDisabledBecauseOfTargeting(&player),"cheat bypasses weapon aim movement clamp");
            }
            TheCamera.Cams[0].mouse=true;player.canStrafe=true;
            cheat=false;player.m_fWalkAngle=0;player.ProcessAnimGroups();
            Require(IsHeavyWeaponMobilityRestricted(&player)==restricted,"OFF restores held heavy restriction");
            pad.sprint=false;Require(!ShouldPlayerSprint(&player,&pad),"OFF requires sprint input");
            pad.sprint=true;Require(ShouldPlayerSprint(&player,&pad)==!restricted,"OFF preserves normal sprint gate");pad.sprint=false;
            Require(MovementDisabledBecauseOfTargeting(&player),"OFF restores two-arm targeting stop");
            if(weapon==WEAPONTYPE_SHOTGUN||weapon==WEAPONTYPE_ROCKETLAUNCHER||weapon==WEAPONTYPE_FLAMETHROWER||
               weapon==WEAPONTYPE_BASEBALLBAT)
                Require(std::strcmp(SprintName(player.m_animGroup),"SPRINT_civi")!=0,"OFF restores weapon animation mapping");
        }
    }
    cheat=true;pad.vertical=0;Require(!ShouldPlayerSprint(&player,&pad),"stationary cheat does not request movement");
    Require(!ShouldPlayerSprint(&player,nullptr),"missing input is safe");
    cheat=false;physical=true;held[0]=held[1]=-1;player.m_fWalkAngle=0;player.ProcessAnimGroups();
    Require(player.m_animGroup==ASSOCGRP_PLAYER,"holstered inventory does not slow movement");
    directionalEnabled=true;velocityPlayer=&player;player.m_nPedState=PED_IDLE;player.canStrafe=true;
    TheCamera.Cams[0].mouse=false;
    for(int yaw=0;yaw<8;yaw++)for(int stick=0;stick<8;stick++)for(float speed:{0.03f,0.07f,0.12f}){
        const float head=yaw*PI/4,angle=stick*PI/4;
        const float rawRight=-std::sin(angle),rawForward=std::cos(angle);
        questAnimationAngle=QuestMovementDirection::UpdateAngle(0,rawRight,rawForward,0.3f);
        player.m_fWalkAngle=0; // The native controller's forward reset must not select the clip.
        player.ProcessAnimGroups();
        const AssocGroupId expected=stick==0||stick==1||stick==7?ASSOCGRP_PLAYER:
            stick==2?ASSOCGRP_PLAYERLEFT:stick==6?ASSOCGRP_PLAYERRIGHT:ASSOCGRP_PLAYERBACK;
        Require(player.m_animGroup==expected,"head-relative animation uses unrotated forward/left/right/back stick direction");
        const float x=rawRight*std::cos(head)-rawForward*std::sin(head);
        const float y=rawForward*std::cos(head)+rawRight*std::sin(head);
        velocityPad.horizontal=int(std::round(x*128));velocityPad.vertical=int(std::round(-y*128));
        for(float base:{-2.0f,0.0f,1.8f}){
            TheCamera.Cams[0].Front.heading=base;
            // Native root motion may have either sign or a lateral component.
            player.m_moved={speed*rawRight,speed*rawForward};
            Require(ApplyQuestMovementVelocity(&player),"VR velocity handles signed directional root motion");
            const float expectedHeading=base+head+angle;
            Require(std::fabs(player.m_moved.x+std::sin(expectedHeading)*speed)<0.0002f &&
                std::fabs(player.m_moved.y-std::cos(expectedHeading)*speed)<0.0002f,
                "directional clips do not reverse or double-rotate actual movement");
            Require(std::fabs(player.m_moved.Magnitude()-speed)<0.00001f,"directional steering preserves root-motion speed");
        }
        // Release retains the last direction even with below-deadzone noise.
        questAnimationAngle=QuestMovementDirection::UpdateAngle(questAnimationAngle,0.1f,-0.1f,0.3f);
        player.m_fWalkAngle=0;player.ProcessAnimGroups();
        Require(player.m_animGroup==expected,"release does not snap the animation group forwards");
        player.m_fMoveSpeed=0;
        for(auto &anim:animations)anim={ASSOC_RUNNING,0};
        Require(BlendQuestLocomotionToIdle(&player),"VR stop bypasses forward-only run-stop clips");
        Require(idleGroup==expected,"idle blend retains the last directional group");
        for(int i=0;i<6;i++)Require(!(animations[i].flags&ASSOC_RUNNING) &&
            (animations[i].flags&ASSOC_DELETEFADEDOUT) && animations[i].blendDelta==-8,
            "all moving and braking clips freeze and fade on release");
        velocityPad.horizontal=velocityPad.vertical=0;
        player.m_moved={0.04f,0.02f};
        Require(ApplyQuestMovementVelocity(&player) && player.m_moved.Magnitude()==0 &&
            TheCamera.Cams[0].m_fPlayerVelocity==0,"release discards residual root motion in every direction");
        player.m_fMoveSpeed=1;
        Require(!BlendQuestLocomotionToIdle(&player),"new stick input immediately releases the idle-stop path");
        for(int i=0;i<4;i++)Require(animations[i].IsRunning() &&
            !(animations[i].flags&ASSOC_DELETEFADEDOUT) && animations[i].blendDelta==0,
            "new input resumes the gait without a stale fade or delete flag");
    }
    for(int guard=0;guard<6;guard++){
        directionalEnabled=guard!=0;player.vehicle=guard==1;player.bIsInTheAir=guard==2;
        player.m_nPedState=guard==3?PED_JUMP:PED_IDLE;player.canStrafe=guard!=4;
        CPed other;velocityPlayer=guard==5?&other:&player;
        player.m_fMoveSpeed=0;player.m_moved={0.04f,0.02f};
        Require(!ApplyQuestMovementVelocity(&player),"non-VR, vehicle, airborne, jump, native reaction and NPC velocity stay native");
        if(guard!=5)Require(!BlendQuestLocomotionToIdle(&player),"native animation contexts bypass the VR stop blend");
    }
    std::printf("Quest production unlimited sprint: %d checks passed\n",checks);
}
