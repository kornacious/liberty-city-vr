#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <initializer_list>
#include "gta3-recovery-animations.inc"
using uint8=uint8_t;
using uint32=uint32_t;
#define nil nullptr
enum {PED_IDLE,PED_FALL,PED_GETUP,PED_DIVE_AWAY,PED_STEP_AWAY,PED_ARRESTED,PED_FLEE_POS,PED_FLEE_ENTITY,
	PEDMOVE_STILL,PEDMOVE_RUN,ASSOCGRP_STD,ASSOC_PARTIAL=0x100,ASSOC_FADEOUTWHENDONE=0x200,ASSOC_DELETEFADEDOUT=0x400};
struct CPed;
struct Hierarchy { float totalLength=1; } hierarchy;
struct CAnimBlendAssociation {
	int animId=0,flags=0;float blendAmount=1,blendDelta=0,currentTime=0;
	bool running=true;Hierarchy *hierarchy=&::hierarchy;
	bool IsRunning(){return running;}
	void SetCurrentTime(float t){currentTime=t;}
	void SetRun(){running=true;}
};
static CAnimBlendAssociation animations[ANIM_STD_NUM];
static bool present[ANIM_STD_NUM];
struct CTimer {
	static uint32 now;
	static uint32 GetTimeInMilliseconds(){return now;}
	static uint32 GetFrameCounter(){return 0;}
};
uint32 CTimer::now=10000;
struct CPed {
	int m_nPedState=PED_IDLE,m_randomSeed=0,lookClears=0,restores=0,getups=0,moves=0;
	uint32 m_getUpTimer=0;
	bool bIsDucking=false,bFallenDown=false,bUpdateAnimHeading=false,bGetUpAnimStarted=false,protectedReplay=false;
	bool IsPedInControl(){return m_nPedState==PED_IDLE || m_nPedState==PED_STEP_AWAY;}
	bool IsPlayer(){return false;}
	bool DyingOrDead(){return false;}
	void *GetClump(){return this;}
	void ClearLookFlag(){++lookClears;}
	void ClearAimFlag(){}
	void SetStoredState(){}
	void SetPedState(int state){m_nPedState=state;}
	void RestorePreviousState(){m_nPedState=PED_IDLE;++restores;}
	void SetMoveState(int){++moves;}
	void SetMoveAnim(){}
	void SetGetUp(){m_nPedState=PED_GETUP;bGetUpAnimStarted=true;++getups;}
	void SetFall(int,AnimationId,uint8);
	void Fall();void ClearFall();
	static void PedEvadeCB(CAnimBlendAssociation*,void*);
	static void PedGetupCB(CAnimBlendAssociation*,void*);
};
namespace androidgame { static bool SimulationReplayProtected(CPed *p){return p->protectedReplay;} }
static CAnimBlendAssociation *RpAnimBlendClumpGetAssociation(void*,int id){return id>=0&&id<ANIM_STD_NUM&&present[id]?&animations[id]:nullptr;}
namespace CAnimManager {
static CAnimBlendAssociation *BlendAnimation(void*,int,int id,float delta){
	if(id<0 || id>=ANIM_STD_NUM)return nullptr;
	present[id]=true;animations[id].animId=id;animations[id].blendDelta=delta;return &animations[id];
}
}
static void RpAnimBlendClumpSetBlendDeltas(void*,int,float){}
namespace VrRagdoll {
static bool enabled=true;static unsigned takes;
static bool timerReady;
static bool BeginFall(CPed *p){if(!enabled)return false;++takes;timerReady=p->bFallenDown&&p->m_getUpTimer>CTimer::now;return true;}
}
#pragma warning(push)
#pragma warning(disable:4244) // Native millisecond deadlines truncate animation seconds.
#include "ragdoll-recovery-production.inc"
#pragma warning(pop)
static unsigned checks;
static void Check(bool ok,const char*why){++checks;if(!ok){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}}
static void Reset(){
	for(int i=0;i<ANIM_STD_NUM;i++){present[i]=false;animations[i]=CAnimBlendAssociation();}
	VrRagdoll::takes=0;VrRagdoll::timerReady=false;VrRagdoll::enabled=true;CTimer::now=10000;
}
static void Recover(CPed &ped){
	Check(ped.m_getUpTimer!=UINT32_MAX,"ordinary native fall has finite recovery deadline");
	CTimer::now=ped.m_getUpTimer+1;ped.Fall();
	Check(ped.getups==1&&ped.m_nPedState==PED_GETUP,"fall reaches native getup");
	CAnimBlendAssociation getup;CPed::PedGetupCB(&getup,&ped);
	Check(ped.m_nPedState==PED_IDLE&&!ped.bFallenDown&&!ped.bGetUpAnimStarted,"getup callback restores normal AI and clears fallen flags");
}
int main(){
	for(int state:{PED_DIVE_AWAY,PED_STEP_AWAY,PED_GETUP}){
		Reset();CPed ped;ped.m_nPedState=state;ped.SetFall(500,ANIM_STD_HIGHIMPACT_BACK,true);
		Check(VrRagdoll::takes==0,"intentional evasion/recovery is not hijacked by a collision SetFall");Recover(ped);
	}
	for(int id:{ANIM_STD_EVADE_DIVE,ANIM_STD_HANDSUP,ANIM_STD_DUCK_DOWN,ANIM_STD_DUCK_LOW}) for(bool running:{false,true}){
		Reset();CPed ped;present[id]=true;animations[id].running=running;animations[id].blendAmount=1;
		ped.SetFall(200,ANIM_STD_HIGHIMPACT_BACK,false);
		Check(VrRagdoll::takes==0,"active or completed-but-blended evasion keeps native recovery chain");Recover(ped);
	}
	Reset();CPed duck;duck.bIsDucking=true;duck.SetFall(200,ANIM_STD_KO_BACK,false);Check(VrRagdoll::takes==0,"cover posture remains animated");Recover(duck);
	Reset();CPed hit;hit.SetFall(500,ANIM_STD_HIGHIMPACT_FRONT,false);Check(VrRagdoll::takes==1&&VrRagdoll::timerReady,"genuine knockdown admitted after animation and timer initialization");Recover(hit);
	Reset();CPed fading;present[ANIM_STD_EVADE_DIVE]=true;animations[ANIM_STD_EVADE_DIVE].blendDelta=-4;
	fading.SetFall(500,ANIM_STD_HIGHIMPACT_FRONT,false);Check(VrRagdoll::takes==1,"fading old dive cannot suppress a later genuine impact");
	Reset();CPed scripted;scripted.SetFall(-1,ANIM_STD_KO_FRONT,false);Check(scripted.m_getUpTimer==UINT32_MAX&&VrRagdoll::takes==0,"script-controlled indefinite fall remains native");
	Reset();CPed dive;dive.m_nPedState=PED_DIVE_AWAY;CAnimBlendAssociation evasive; evasive.animId=ANIM_STD_EVADE_DIVE;
	CPed::PedEvadeCB(&evasive,&dive);Check(dive.m_nPedState==PED_FALL&&dive.m_getUpTimer==CTimer::now+1,"normal dive completion schedules immediate native getup");Recover(dive);
	Reset();CPed off;VrRagdoll::enabled=false;off.SetFall(500,ANIM_STD_HIGHIMPACT_BACK,false);Check(VrRagdoll::takes==0,"disabled ragdoll retains native fall recovery");Recover(off);
	std::printf("PASS: native evasion/fall/getup recovery, %u checks.\n",checks);
}
