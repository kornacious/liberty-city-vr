#include "ragdoll-test-vector.h"
#include "gta3-recovery-animations.inc"
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <vector>
#define nil nullptr
#define __ANDROID__
#define GTA_VR_WEAPONS
#define FEET_OFFSET 1.04f
using int16=int16_t;using uint8=uint8_t;
enum {PED_IDLE,PED_FALL,PED_GETUP,PED_DIE,PED_DEAD,PED_ARRESTED,PED_FLEE_POS,PED_FLEE_ENTITY,
	PEDMOVE_STILL,PEDMOVE_RUN,RANDOM_CHAR=0,MISSION_CHAR=1,VEHICLE_TYPE_CAR=0,VEHICLE_TYPE_BIKE=1,
	ASSOCGRP_STD=0,ASSOC_PARTIAL=1,ASSOC_FRONTAL=2,ASSOC_DELETEFADEDOUT=4,
	WEAPONTYPE_RUNOVERBYCAR,PEDPIECE_TORSO,MAX_COLLISION_POINTS=64};
static constexpr float HALFPI=1.570796327f;
namespace CGeneral { static float LimitRadianAngle(float x){return x;} }
struct CMatrix {
	CVector pos,right{1,0,0},forward{0,1,0},up{0,0,1};
	void SetTranslate(CVector p){pos=p;}
	CVector Point(CVector p)const{return pos+right*p.x+forward*p.y+up*p.z;}
	CVector Local(CVector p)const{p-=pos;return {DotProduct(p,right),DotProduct(p,forward),DotProduct(p,up)};}
};
struct CColSphere {CVector center;float radius=0;void Set(float r,CVector p){radius=r;center=p;}};
struct CColBox {CVector min,max;void Set(CVector a,CVector b){min=a;max=b;}};
struct CColPoint {};
struct CColModel {
	CColSphere boundingSphere;CColBox boundingBox;
	bool ownsCollisionVolumes=true;
	int numSpheres=0,numLines=0,numBoxes=0,numTriangles=0;
	CColSphere *spheres=nullptr;
};
struct CEntity {
	bool vehicle=false,bUsesCollision=true,bRemoveFromWorld=false;
	CMatrix matrix;CColModel *model=nullptr;
	bool IsVehicle()const{return vehicle;}
	CVector &GetPosition(){return matrix.pos;}
	CMatrix &GetMatrix(){return matrix;}
	CColModel *GetColModel(){return model;}
};
struct CVehicle:CEntity {
	int m_vehType=VEHICLE_TYPE_CAR;CColModel collision;
	CVehicle(){vehicle=true;model=&collision;collision.numBoxes=1;collision.boundingBox.Set({-.8f,-2,0},{.8f,2,1.4f});}
};
struct CPed:CEntity {
	int m_nPedState=PED_FALL,CharCreatedBy=RANDOM_CHAR,m_randomSeed=0,m_animGroup=0,getups=0,damage=0;
	bool player=false,bInVehicle=false,bGetUpAnimStarted=false,bUpdateAnimHeading=false,bFallenDown=true,bKnockedUpIntoAir=true;
	float m_fHealth=100,m_fRotationCur=0;CEntity *m_pCollidingEntity=nullptr;
	CColModel collision;CColSphere nativeSphere;
	CPed(){matrix.pos.z=FEET_OFFSET;model=&collision;nativeSphere.Set(.35f,{});collision.numSpheres=1;collision.spheres=&nativeSphere;}
	bool IsPlayer(){return player;}
	bool CanSetPedState(){return m_nPedState!=PED_DIE&&m_nPedState!=PED_DEAD&&m_nPedState!=PED_ARRESTED;}
	bool IsPedHeadAbovePos(float){return false;}
	void SetStoredState(){}
	void SetPedState(int s){m_nPedState=s;if(s==PED_GETUP)++getups;}
	void RestorePreviousState(){m_nPedState=PED_IDLE;}
	void SetMoveState(int){}void SetMoveAnim(){}
	void *GetClump(){return this;}
	void InflictDamage(CEntity*,int,float,int,int){++damage;}
	void SetDie(AnimationId,float,float){m_nPedState=PED_DIE;}
	void SetGetUp();static void PedGetupCB(struct CAnimBlendAssociation*,void*);
};
static unsigned queries,collisionTests;
static std::vector<CEntity*> sceneEntities;
namespace CWorld {
static void FindObjectsKindaColliding(const CVector&p,float radius,bool ignoreZ,int16 *count,int16 limit,CEntity **out,
	bool buildings,bool vehicles,bool peds,bool objects,bool dummies){
	++queries;if(!ignoreZ||buildings||!vehicles||peds||objects||dummies)std::abort();*count=0;
	for(auto *e:sceneEntities){CVector d=e->GetPosition()-p;d.z=0;if(d.Magnitude()>radius+3)continue;if(*count<limit)out[(*count)++]=e;}
}
}
namespace CCollision {
static int ProcessColModels(const CMatrix&a,CColModel&ma,const CMatrix&b,CColModel&mb,CColPoint*,void*,void*){
	++collisionTests;
	for(int i=0;i<ma.numSpheres;i++){
		const auto&s=ma.spheres[i];CVector p=b.Local(a.Point(s.center));
		if(mb.numBoxes>0){
			auto clamp=[](float v,float low,float high){return v<low?low:v>high?high:v;};
			CVector q{clamp(p.x,mb.boundingBox.min.x,mb.boundingBox.max.x),clamp(p.y,mb.boundingBox.min.y,mb.boundingBox.max.y),clamp(p.z,mb.boundingBox.min.z,mb.boundingBox.max.z)};
			if((p-q).MagnitudeSqr()<s.radius*s.radius)return 1;
		}
	}
	return 0;
}
}
#include "QuestPedGetup.h"
namespace VrRagdoll {static bool enabled=true;[[maybe_unused]] static bool IsEnabled(){return enabled;}}
static CEntity *legacyBlocker=nullptr;
namespace CPedPlacement {static CEntity *IsPositionClearOfCars(const CVector*){return legacyBlocker;}}
struct CTimer {static unsigned frame;static unsigned GetFrameCounter(){return frame;}static float GetTimeStep(){return 1;}};
unsigned CTimer::frame=0;
struct CPad {static bool disabled;static CPad *GetPad(int){static CPad p;return &p;}bool ArePlayerControlsDisabled(){return disabled;}};
bool CPad::disabled=false;
struct CAnimBlendAssociation {
	int flags=0;float blendDelta=0;
	void (*callback)(CAnimBlendAssociation*,void*)=nullptr;void *arg=nullptr;
	void SetFinishCallback(void(*cb)(CAnimBlendAssociation*,void*),void*p){callback=cb;arg=p;}
};
static CAnimBlendAssociation getup;
static CAnimBlendAssociation *RpAnimBlendClumpGetAssociation(void*,int){return nullptr;}
static CAnimBlendAssociation *RpAnimBlendClumpGetFirstAssociation(void*,int){return nullptr;}
static void RpAnimBlendClumpSetBlendDeltas(void*,int,float){}
namespace CAnimManager {static CAnimBlendAssociation *BlendAnimation(void*,int,int,float){return &getup;}}
static CColPoint aTempPedColPts[MAX_COLLISION_POINTS];
#include "ragdoll-getup-production.inc"
static unsigned checks;
static void Check(bool b,const char*why){++checks;if(!b){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}}
static void Reset(){sceneEntities.clear();queries=collisionTests=0;legacyBlocker=nullptr;getup={};VrRagdoll::enabled=true;CTimer::frame=0;CPad::disabled=false;}
static void Recovered(CPed &ped){
	ped.SetGetUp();Check(ped.bGetUpAnimStarted&&ped.m_nPedState==PED_GETUP,"clear upright volume starts native getup");
	Check(!ped.m_pCollidingEntity&&!ped.bKnockedUpIntoAir&&!ped.damage,"successful recovery clears stale vehicle record without damage");
	Check(getup.callback!=nullptr,"native finish callback installed");getup.callback(&getup,&ped);
	Check(ped.m_nPedState==PED_IDLE&&!ped.bFallenDown&&!ped.bGetUpAnimStarted,"actual getup callback returns ped to normal AI");
}
int main(){
	// Every native modulo phase must permit standing beside a car, despite an
	// old arm/leg contact and the old point query reporting that same car.
	for(unsigned phase=0;phase<8;phase++){
		Reset();CVehicle car;CPed ped;ped.GetPosition().x=1.1f;ped.m_pCollidingEntity=&car;
		sceneEntities={&car};legacyBlocker=&car;CTimer::frame=phase;Recovered(ped);Check(queries==1,"one bounded fresh sceneEntities query");
	}
	Reset();CVehicle old;old.GetPosition().x=20;CPed stale;stale.m_pCollidingEntity=&old;sceneEntities={&old};Recovered(stale);
	Reset();CVehicle obstructed;CPed under;under.m_pCollidingEntity=&obstructed;sceneEntities={&obstructed};
	under.SetGetUp();Check(!under.bGetUpAnimStarted&&!getup.callback&&!under.damage,"actual torso/leg obstruction remains blocked without killing ambient NPC");
	obstructed.GetPosition().x=5;Recovered(under);Check(queries==2,"moving vehicle away permits retry immediately");
	Reset();CVehicle moved;CPed entering;entering.GetPosition().x=1.1f;sceneEntities={&moved};moved.GetPosition().x=.4f;
	entering.SetGetUp();Check(!entering.bGetUpAnimStarted,"current car pose can newly obstruct recovery");
	Reset();CVehicle rotated;rotated.matrix.right={0,1,0};rotated.matrix.forward={-1,0,0};CPed beside;
	beside.GetPosition().y=1.1f;beside.m_pCollidingEntity=&rotated;sceneEntities={&rotated};Recovered(beside);
	Reset();CVehicle low;low.collision.boundingBox.max.z=.5f;CPed legs;sceneEntities={&low};
	legs.SetGetUp();Check(!legs.bGetUpAnimStarted,"low bonnet or chassis below hip height still blocks standing legs");
	Reset();CVehicle roof;roof.collision.boundingBox.min.z=1.6f;roof.collision.boundingBox.max.z=2;CPed head;sceneEntities={&roof};
	head.SetGetUp();Check(!head.bGetUpAnimStarted,"head-height obstruction is checked above native root sphere");
	for(int policy=0;policy<4;policy++){
		Reset();CVehicle car;CPed ped;ped.GetPosition().x=1.1f;ped.m_pCollidingEntity=&car;sceneEntities={&car};legacyBlocker=&car;
		if(policy==0)ped.player=true;if(policy==1)ped.CharCreatedBy=MISSION_CHAR;if(policy==2)ped.bInVehicle=true;if(policy==3)VrRagdoll::enabled=false;
		ped.SetGetUp();Check(queries==0&&!ped.bGetUpAnimStarted,"player, mission, occupant and disabled paths retain native gate");
	}
	Reset();CVehicle pile;CPed crowded;sceneEntities.assign(16,&pile);crowded.SetGetUp();Check(!crowded.bGetUpAnimStarted,"overflow is conservatively blocked");
	Reset();CVehicle disabled;disabled.bUsesCollision=false;CPed clear;sceneEntities={&disabled};Recovered(clear);
	Reset();CVehicle removed;removed.bRemoveFromWorld=true;CPed removedContact;sceneEntities={&removed};Recovered(removedContact);
	Reset();CVehicle unsupported;unsupported.collision.numBoxes=0;CPed unknown;sceneEntities={&unsupported};unknown.SetGetUp();Check(!unknown.bGetUpAnimStarted,"missing collision volumes cannot falsely declare clearance");
	Reset();CEntity wall;CPed solid;legacyBlocker=&wall;solid.SetGetUp();Check(!solid.bGetUpAnimStarted&&queries==0,"native static-obstacle gate remains intact");
	std::printf("PASS: actual NPC getup gate and fresh vehicle standing clearance, %u checks.\n",checks);
}
