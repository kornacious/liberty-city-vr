#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
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
enum {PED_IDLE,PED_FALL,PED_GETUP,PED_STAGGER,PED_DIVE_AWAY,PED_DIE,PED_DEAD,PED_DRIVING};
struct CPlayerPed {
	int state=PED_IDLE; bool vehicle=false, dead=false;
	bool InVehicle() {return vehicle;} bool DyingOrDead() {return dead;}
	int GetPedState() {return state;}
};
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
