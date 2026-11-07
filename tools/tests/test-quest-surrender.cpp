#include <cstdio>
#include <cstdlib>
#include <limits>
#include "ragdoll-test-vector.h"
#include "QuestSurrender.h"
using uint32 = uint32_t;
#define nil nullptr
enum { PEDTYPE_COP, PEDTYPE_CIV, PED_IDLE, PED_ATTACK, PED_FIGHT, WBSTATE_PLAYING, WBSTATE_BUSTED };
struct Wanted {
	int level = 1; bool ignored = false, m_bIgnoredByEveryone = false;
	int GetWantedLevel() { return level; }
	bool IsIgnoredByCops() { return ignored; }
} wanted;
struct CPed {
	int m_nPedType = PEDTYPE_COP, state = PED_ATTACK;
	bool control = true, vehicle = false;
	CPed *m_pedInObjective = nullptr;
	CVector pos;
	bool IsPedInControl() { return control; }
	bool InVehicle() { return vehicle; }
	int GetPedState() { return state; }
	CVector GetPosition() { return pos; }
};
struct CPlayerPed : CPed { Wanted *m_pWanted = &wanted; } testPlayer;
int arrests = 0, resets = 0;
struct CCopPed : CPed {
	bool m_bIsDisabledCop = false, m_bZoneDisabled = false;
	void SetArrestPlayer(CPed *) { ++arrests; testPlayer.control = false; }
} testCop;
struct CPedPool {
	CPed *slot = &testCop;
	int GetSize() { return 1; }
	CPed *GetSlot(int) { return slot; }
} testPool;
struct CPools { static CPedPool *GetPedPool() { return &testPool; } };
struct CPad {
	bool DisablePlayerControls = false;
	static CPad *GetPad(int) { static CPad pad; return &pad; }
};
struct PlayerInfo { int m_WBState = WBSTATE_PLAYING; };
struct CWorld {
	inline static PlayerInfo Players[1]; inline static int PlayerInFocus = 0;
	inline static bool visible = true;
	static bool GetIsLineOfSightClear(CVector, CVector, bool, bool, bool, bool, bool, bool) { return visible; }
};
uint32 now = 0;
struct CTimer { static uint32 GetTimeInMilliseconds() { return now; } };
bool hands = true, vehicle = false, view = true, gPoseValid[2] = {true, true};
struct Matrix { CVector pos; CVector GetPosition() { return pos; } } gGripMatrix[2];
bool AreTrackedHandsEnabled() { return hands; }
CPlayerPed *FindPlayerPed() { return &testPlayer; }
bool FindPlayerVehicle() { return vehicle; }
CVector VectorFromArray(float *p) { return {p[0], p[1], p[2]}; }
void ResetInteraction() { ++resets; }
namespace rw { namespace vulkan {
bool getFirstPersonViewFrame(float *, float *, float *, float *p) { p[0]=0; p[1]=0; p[2]=1.7f; return view; }
} }
// Extract UpdateSurrender from QuestWeaponVR.cpp, without altering its body.
#include "quest-surrender-production.inc"
int checks = 0;
void Check(bool value, const char *label) { ++checks; if(!value) { std::fprintf(stderr,"FAIL: %s\n",label); std::exit(1); } }
void Tick(int count = 1, bool blocked = false) { for(int i=0;i<count;++i) { now+=100; UpdateSurrender(blocked); } }
void Fresh() {
	UpdateSurrender(true); now+=100; testPlayer.control=true; wanted={}; testCop={}; testCop.m_pedInObjective=&testPlayer;
	testPool.slot=&testCop; hands=true; vehicle=false; view=true; gPoseValid[0]=gPoseValid[1]=true;
	CPad::GetPad(0)->DisablePlayerControls=false; CWorld::Players[0].m_WBState=WBSTATE_PLAYING;
	CWorld::visible=true; gGripMatrix[0].pos={-.2f,0,1.9f}; gGripMatrix[1].pos={.2f,0,1.9f}; arrests=resets=0;
}
int main() {
	Fresh(); Tick(30); Check(arrests==0,"full three seconds required"); Tick(); Check(arrests==1 && resets==1,"native arrest and interaction reset"); Tick(50); Check(arrests==1,"no repeated arrest");
	for(int melee=0;melee<2;++melee) { Fresh(); testCop.state=melee?PED_FIGHT:PED_ATTACK; Tick(31); Check(arrests==1,"shooting and punching officers qualify"); }
	for(int gate=0;gate<17;++gate) {
		Fresh();
		switch(gate) {
		case 0: wanted.level=0; break; case 1: wanted.ignored=true; break;
		case 2: wanted.m_bIgnoredByEveryone=true; break; case 3: testCop.m_nPedType=PEDTYPE_CIV; break;
		case 4: testCop.m_pedInObjective=nullptr; break; case 5: testCop.state=PED_IDLE; break;
		case 6: testCop.pos={16,0,0}; break; case 7: testCop.control=false; break;
		case 8: testCop.vehicle=true; break; case 9: testCop.m_bIsDisabledCop=true; break;
		case 10: testCop.m_bZoneDisabled=true; break; case 11: CWorld::visible=false; break;
		case 12: vehicle=true; break; case 13: CPad::GetPad(0)->DisablePlayerControls=true; break;
		case 14: CWorld::Players[0].m_WBState=WBSTATE_BUSTED; break;
		case 15: hands=false; break; case 16: view=false; break;
		}
		Tick(50); Check(arrests==0,"ineligible gameplay or officer cannot arrest");
	}
	for(int interruption=0;interruption<6;++interruption) {
		Fresh(); Tick(25);
		switch(interruption) {
		case 0: gPoseValid[0]=false; Tick(); gPoseValid[0]=true; break;
		case 1: gGripMatrix[1].pos.z=1.0f; Tick(); gGripMatrix[1].pos.z=1.9f; break;
		case 2: gGripMatrix[1].pos.x=1.0f; Tick(); gGripMatrix[1].pos.x=.2f; break;
		case 3: Tick(1,true); break; case 4: testPool.slot=nullptr; Tick(); testPool.slot=&testCop; break;
		case 5: now+=1000; Tick(); break;
		}
		Tick(25); Check(arrests==0,"interrupted hold must restart"); Tick(6); Check(arrests==1,"fresh hold succeeds after interruption");
	}
	QuestSurrender::Hold hold;
	uint32 wrap=std::numeric_limits<uint32>::max()-1000;
	Check(!hold.Update(wrap,true),"start near timer rollover");
	for(int i=1;i<=30;++i) Check(hold.Update(wrap+i*100,true)==(i==30),"timer rollover retains duration");
	const CVector head(0,0,1.7f);
	Check(!QuestSurrender::HandsRaised(CVector(NAN,0,2),CVector(0,0,2),head),"invalid position rejected");
	Check(!QuestSurrender::HandsRaised(CVector(0,0,3),CVector(0,0,3),head),"implausibly distant hands rejected");
	std::printf("Quest surrender: %d checks passed\n",checks);
}
