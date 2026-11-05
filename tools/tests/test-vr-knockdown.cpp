#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
namespace OculusVR { bool IsQuestTrainPassenger() { return false; } bool IsQuestTrainCinematic() { return false; } }
struct CVector {
	float x,y,z;
	CVector(float a=0,float b=0,float c=0):x(a),y(b),z(c){}
	CVector operator-(const CVector &b) const {return {x-b.x,y-b.y,z-b.z};}
	CVector operator*(float n) const {return {x*n,y*n,z*n};}
	CVector &operator+=(const CVector &b) {x+=b.x;y+=b.y;z+=b.z;return *this;}
	float MagnitudeSqr() const {return x*x+y*y+z*z;}
};
#include "../../overlay/src/skel/android/VrKnockdownView.h"
using float32=float; using bool32=int; using int32=int;
#define nil nullptr
struct {
	int firstPersonActive=1, temporalHistoryValid=1, fpFollowHeading=1, fpUseFullBasis=0;
	float fpLatchedHeadYaw=0.3f, headYaw=0.8f, fpAnchorYaw=0;
	float fpLatchedHeadPos[3]={1,2,3}, headPosition[3]={4,5,6};
	float fpHeadWorld[3]={},fpPlayX[3]={},fpPlayY[3]={},fpPlayZ[3]={};
} gvk;
static void resetAtomicMotionHistory() {}
enum {PED_IDLE,PED_FALL,PED_GETUP,PED_STAGGER,PED_DIVE_AWAY,PED_DIE,PED_DEAD,PED_DRIVING,PED_HANDS_UP,PED_ARRESTED,PED_DRAG_FROM_CAR};
struct CPlayerPed {
	int state=PED_IDLE, m_nPedState=PED_IDLE; bool vehicle=false, dead=false;
	float m_fHealth=100;
	bool InVehicle() {return vehicle;} bool DyingOrDead() {return dead;}
	int GetPedState() {return state;}
};
using CPed=CPlayerPed;
static CPlayerPed gatePlayer;
static CPed *FindPlayerPed() {return &gatePlayer;}
static bool gVrFirstPersonActive=true;
static VrKnockdownView gKnockdownView;
struct {bool m_bMenuActive=false;} FrontEndMenuManager;
struct {bool m_WideScreenOn=false;} TheCamera;
struct CCutsceneMgr {static bool IsRunning(){return false;} static bool IsCutsceneProcessing(){return false;}};
namespace androidgame {
	bool VrKnockdownPresentationActive();
	static void *VrCutsceneCameraActor(){return nullptr;}
}
enum {WBSTATE_PLAYING};
struct CWorld {
	struct Player {int m_WBState=WBSTATE_PLAYING;};
	static Player Players[1]; static int PlayerInFocus;
};
CWorld::Player CWorld::Players[1]; int CWorld::PlayerInFocus;
namespace VrRagdoll {static bool Owns(CPed *){return false;}}
enum { VR_HAND_COUNT=2 };
int gHeldSlot[2],gHolsterSelection[2],gWeaponRenderSlot[2],gWeaponContactSlot[2],gSupportHand[2];
bool gThrowablePreview[2],gManualReloadGripDown[2],gTrackedDetonatorWasActive[2];
bool gTrackedDetonatorWaitForRelease[2],gTrackedDetonatorJustPressed[2],gGripLatched[2];
bool gTriggerPressed[2],gTriggerJustPressed[2],gTriggerJustReleased[2];
float gGrip[2]; unsigned gPreviousPoseTime[2]; CVector gHandVelocity[2];
struct DroppedWeapon {int slot=-1;} gDropped[2];
struct MeleeMotion {int sample=0;} gMeleeMotion[2];
struct MeleeStrike {bool hit=false;} gMeleeStrike[2];
struct ManualReloadState {bool active=false;} gManualReloadState[2];
int gScopeHand,gScopeWeaponType,gScopeCandidateHand,gScopeCandidateType;
unsigned gScopeCandidateSince,gScopeReleaseSince;
bool gActiveFire,gActiveThrowable,gBodyForwardValid;
int gActiveFireHand,gActiveFireWeaponType;
static void ClearAttachedMissionWeaponState() {}
static void ClearSupportForHand(int hand) {gSupportHand[hand]=-1;}
static void ResetManualReloadState(ManualReloadState &state) {state={};}
// Exact production camera API and state predicate, extracted by the runner.
#include "vr-knockdown-production.inc"
static unsigned checks;
static void Check(bool condition,const char *message) {
	++checks;if(!condition) {std::fprintf(stderr,"FAIL at check %u: %s\n",checks,message);throw std::runtime_error(message);}
}
int main() {
	CPlayerPed player;
	for(int state : {PED_FALL,PED_GETUP,PED_STAGGER,PED_DIVE_AWAY}) {
		player.state=state; Check(PlayerKnockedDown(&player),"fall/get-up states must own presentation");
	}
	player.vehicle=true; Check(!PlayerKnockedDown(&player),"vehicle view must retain ownership");
	player.vehicle=false;player.dead=true;Check(!PlayerKnockedDown(&player),"death camera must retain ownership");
	player.dead=false;player.state=PED_IDLE;Check(!PlayerKnockedDown(&player),"ordinary gameplay must remain unchanged");
	Check(!PlayerKnockedDown(nullptr),"missing player must be safe");
	float referenceZ=0;
	for(int hz : {30,72,90,120}) {
		VrKnockdownView view;
		const CVector target(0.25f,0.1f,-0.65f);
		for(int i=0;i<hz;i++) {
			const float oldYaw=view.yaw, oldPitch=view.pitch, oldRoll=view.roll;
			view.Update(true,1.0f/hz,target,1.0f,0.4f,0.3f);
			Check(std::fabs(view.yaw-oldYaw)<=0.785399f/hz+0.000001f,"imposed yaw rate exceeded comfort cap");
			Check(std::fabs(view.pitch-oldPitch)<=0.523600f/hz+0.000001f,"pitch rate exceeded cap");
			Check(std::fabs(view.roll-oldRoll)<=0.523600f/hz+0.000001f,"roll rate exceeded cap");
		}
		Check(view.active && std::fabs(view.offset.z-target.z)<0.001f,"head must settle at the authored height");
		if(referenceZ) Check(std::fabs(view.offset.z-referenceZ)<0.00001f,"head smoothing must agree across frame rates");
		referenceZ=view.offset.z;
		const CVector paused=view.offset;const float yaw=view.yaw;
		view.Update(true,0,target,-1.0f,0,0);
		Check((view.offset-paused).MagnitudeSqr()==0 && yaw==view.yaw,"pause must freeze imposed camera motion");
		for(int i=0;i<hz*5 && view.active;i++)
			view.Update(false,1.0f/hz,{},0.9f,0,0);
		Check(!view.active && view.offset.MagnitudeSqr()==0,"recovery must finish without a residual offset");
		view.Update(true,1.0f/hz,target,0,0,0);
		Check(view.active && view.offset.z<0,"a second knockdown must activate cleanly");
		view.Reset(); Check(!view.active && view.yaw==0,"reset must clear state for respawn/context changes");
	}
	VrKnockdownView wrap;wrap.active=true;wrap.yaw=3.13f;
	wrap.Update(true,0.02f,{},-3.13f,0,0);
	Check(VrKnockdownView::Wrap(wrap.yaw-3.13f)>0,"yaw must take the short route across pi");
	VrKnockdownView stall;stall.Update(true,10.0f,{0,0,-1},1,1,1);
	Check(stall.yaw<=0.03928f && stall.offset.z>-0.4f,"a stalled frame must not teleport/rotate the camera");
	for(int hz : {30,72,90,120}) {
		VrKnockdownView turning;
		turning.active=true;turning.yaw=0.8f;turning.offset={0,0,-0.6f};
		float normalYaw=2.7f;
		// Begin heading handoff during get-up while still following the head.
		for(int i=0;i<hz;i++) {
			normalYaw=VrKnockdownView::Wrap(normalYaw+0.6f/hz);
			turning.Update(true,1.0f/hz,{0,0,-0.1f},normalYaw,0,0,true);
		}
		const float errorBefore=std::fabs(VrKnockdownView::Wrap(turning.yaw-normalYaw));
		Check(errorBefore<1.6f,"get-up must begin returning heading before animation ends");
		int frames=0;
		int hiddenFrames=0;
		for(;frames<hz*6 && turning.active;frames++) {
			const float oldYaw=turning.yaw;
			const float bodyTurn=0.6f/hz;
			normalYaw=VrKnockdownView::Wrap(normalYaw+bodyTurn);
			turning.Update(false,1.0f/hz,{},normalYaw,0,0);
			if(turning.OwnsBody()) ++hiddenFrames;
			if(turning.active)
				Check(std::fabs(VrKnockdownView::Wrap(turning.yaw-oldYaw-bodyTurn))<=0.785399f/hz+0.000001f,
				      "recovery must follow body turns while bounding only imposed yaw");
		}
		Check(!turning.active && frames<hz*4,"continuous turning must not keep hands hidden indefinitely");
		Check(hiddenFrames<hz,"hand/IK recovery must finish within a second independently of yaw");
	}
	VrKnockdownView yawOnly;yawOnly.active=true;yawOnly.yaw=2.0f;
	Check(!yawOnly.OwnsBody(),"a remaining yaw correction must never hide hands or lock body IK");
	gatePlayer.state=gatePlayer.m_nPedState=PED_GETUP;
	Check(androidgame::VrKnockdownPresentationActive() && !BodyIKGameplayContext(),"get-up must hide tracked hands and leave animation ownership intact");
	gatePlayer.state=gatePlayer.m_nPedState=PED_IDLE;
	gKnockdownView=yawOnly;gKnockdownView.offset={0,0,-0.1f};
	Check(androidgame::VrKnockdownPresentationActive() && !BodyIKGameplayContext(),"head-height recovery must keep hand visibility and IK ownership synchronized");
	gKnockdownView=yawOnly;
	Check(!androidgame::VrKnockdownPresentationActive() && BodyIKGameplayContext(),"yaw-only recovery must restore hands and body IK together");
	for(int hand=0;hand<2;hand++) {
		gHeldSlot[hand]=hand+2;gHolsterSelection[hand]=hand+2;
		gDropped[hand].slot=hand+2;gSupportHand[hand]=1-hand;
		gManualReloadState[hand].active=true;gThrowablePreview[hand]=true;
		gGrip[hand]=hand==0 ? 0.9f : 0.1f;gHandVelocity[hand]={5,0,0};
		gTriggerPressed[hand]=gTriggerJustPressed[hand]=true;
	}
	gActiveFire=gActiveThrowable=true;gBodyForwardValid=true;
	ResetKnockdownInteraction();
	for(int hand=0;hand<2;hand++) {
		Check(gHeldSlot[hand]==-1 && gHolsterSelection[hand]==-1,"both weapons must require a new holster draw");
		Check(gDropped[hand].slot==-1 && gSupportHand[hand]==-1,"knockdown must clear catchable props and support ownership");
		Check(!gManualReloadState[hand].active && !gThrowablePreview[hand],"reload and throw interactions must be cleared");
		Check(gHandVelocity[hand].MagnitudeSqr()==0 && !gTriggerJustPressed[hand],"recovery must not inherit a swing or shot");
	}
	Check(gGripLatched[0] && !gGripLatched[1],"held grip must be released before a new holster grab");
	Check(!gActiveFire && !gActiveThrowable && !gBodyForwardValid,"offensive and body socket caches must reset");
	const float head[3]={0,0,0},side[3]={1,0,0},up[3]={0,0,1},forward[3]={0,1,0};
	setFirstPersonAnchorBasis(head,side,up,forward,0,1,1);
	Check(gvk.fpLatchedHeadYaw==0.3f && gvk.fpLatchedHeadPos[0]==1,"knockdown tilt must preserve physical head reference");
	setFirstPersonAnchor(head,0,1,1,1);
	Check(gvk.fpLatchedHeadYaw==0.3f && gvk.fpLatchedHeadPos[0]==1,"recovery must preserve physical head reference");
	setFirstPersonAnchorBasis(head,side,up,forward,0,1,0);
	Check(gvk.fpLatchedHeadYaw==0.8f && gvk.fpLatchedHeadPos[0]==4,"ordinary vehicle entry must still recenter");
	gvk.firstPersonActive=0;gvk.fpLatchedHeadYaw=0;
	setFirstPersonAnchor(head,0,1,1,1);
	Check(gvk.fpLatchedHeadYaw==0.8f,"initial activation must latch even when preserving tracking");
	std::printf("Knockdown camera: %u checks passed.\n",checks);
}
