#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include "ragdoll-test-vector.h"
#include "../../overlay/src/skel/android/VrCrouchView.h"
static int checks;
static void Check(bool ok,const char *message){
	++checks;if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}
int main(){
	for(int hz : {72,90,120}){
		VrCrouchView view;
		const CVector target(0.25f,-0.18f,-0.45f);
		VrCrouchView faster;
		VrKnockdownView reference;
		reference.Update(true,2.0f/hz,target,0,0,0);
		CVector fastOffset=faster.Update(true,true,true,true,0,1.0f/hz,target);
		Check((fastOffset-reference.offset).Magnitude()<0.000001f,"crouch entry advances at twice the original speed");
		reference.Update(false,1.0f/hz,{},0,0,0);
		fastOffset=faster.Update(true,false,false,true,1,1.0f/hz,{});
		Check((fastOffset-reference.offset).Magnitude()<0.000001f,"standing recovery retains its existing speed");
		unsigned frame=0;
		CVector offset;
		for(int i=0;i<hz;i++){
			offset=view.Update(false,true,true,true,frame++,1.0f/hz,target);
			Check(offset.MagnitudeSqr()==0,"hands hidden preserves the stable anchor exactly");
		}
		for(int i=0;i<hz;i++){
			offset=view.Update(true,true,true,true,frame++,1.0f/hz,target);
			Check(offset.z>=target.z && offset.z<0,"crouch eases toward authored head without overshoot");
			CVector duplicate=view.Update(true,true,true,true,frame-1,1.0f/hz,target);
			Check((duplicate-offset).MagnitudeSqr()==0,"duplicate anchor publication does not advance easing");
			CVector before=view.Update(true,true,true,false,frame,1.0f/hz,target);
			Check((before-offset).MagnitudeSqr()==0,"pre-physics IK anchor read does not advance easing");
		}
		Check((offset-target).Magnitude()<0.001f,"camera converges to animated head position");
		for(int i=0;i<hz*2;i++)offset=view.Update(true,false,false,true,frame++,1.0f/hz,{});
		Check(offset.MagnitudeSqr()==0,"button movement or jump exit recovers standing anchor");
		view.Update(true,true,true,true,frame++,0.02f,target);
		offset=view.Update(false,true,true,true,frame++,0.02f,target);
		Check(offset.MagnitudeSqr()==0,"switching Hide VR Hands on restores exact stable camera");
		offset=view.Update(true,true,false,true,frame++,0.02f,target);
		Check(offset.MagnitudeSqr()==0,"missing head pose leaves stable anchor");
		view.Update(true,true,true,true,frame++,0.02f,target);
		view.Reset();offset=view.Update(true,false,false,true,frame++,0.02f,{});
		Check(offset.MagnitudeSqr()==0,"context reset cannot leak into vehicles cutscenes or respawn");
	}
	std::printf("Crouch camera: %d checks passed\n",checks);
}
