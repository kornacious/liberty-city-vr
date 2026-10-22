#!/usr/bin/env python3
"""Exercise production radio input with deterministic poses and clock."""
from pathlib import Path
import re
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
source=(root/'overlay/src/vr/QuestRadio.cpp').read_text()
production=[]
for name in ['Update','SuppressDriving']:
    match=re.search(r'(?ms)^bool Update\(.*?^}',source) if name=='Update' else re.search(r'(?m)^void SuppressDriving[^\n]+',source)
    assert match, name
    production.append(match.group())
with tempfile.TemporaryDirectory(prefix='radio-input-') as d:
    work=Path(d)
    (work/'production.inc').write_text('\n'.join(production))
    (work/'main.cpp').write_text(r'''
#include "RadioBoardLayout.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdint>
struct CVector { float x,y,z; CVector(float a=0,float b=0,float c=0):x(a),y(b),z(c){} CVector operator-(CVector b) const{return {x-b.x,y-b.y,z-b.z};} };
float DotProduct(CVector a,CVector b){return a.x*b.x+a.y*b.y+a.z*b.z;}
namespace androidgame {
struct PadInput { struct Pose {float position[3],orientation[4];bool valid=false;}; Pose aimPose[2]; bool x=false; float leftTrigger=0,rightTrigger=0; } input;
const PadInput &GetPadInput(){return input;}
}
namespace OculusVR { bool weapon=false; bool IsTrackedWeaponHeld(int){return weapon;} bool GetTrackedIndexTip(int,CVector*){return false;} }
namespace rw { namespace vulkan {
bool playPoseToFirstPersonWorld(const float *pos,const float*,float*,float*,float *forward,float *origin){memcpy(origin,pos,12);forward[0]=forward[1]=0;forward[2]=-1;return true;}
} }
struct CControllerState { int acceleration=255; void Clear(){acceleration=0;} };
namespace QuestRadio {
struct Plane{CVector origin,right={1,0,0},up={0,1,0},normal={0,0,1};float width=.4f;};
bool available=true,vehicle=true,interacting=false,xDown=false,triggerDown[2]={true,true};
bool touchMode=false; RadioBoard::Touch touch[2];
int hover=-1,activations=0,last=-1;double now=0,xSince=0;
void Load(){} bool BuildPlane(Plane &){return available;} bool Vehicle(){return vehicle;} double Now(){return now;}
void Activate(int hit){++activations;last=hit;}
#include "production.inc"
}
int main(){
 using namespace QuestRadio; auto &in=androidgame::input;
 assert(!Update(false)); in.x=true; now=.01; assert(!Update(false));
 in.x=false; now=.1; assert(Update(false)); // ordinary short station tap
 in.x=true; now=1; assert(!Update(false)); now=1.31; assert(!Update(false) && interacting);
 // Aim at NEXT, but a trigger already held for driving must not click.
 in.aimPose[0].valid=true;in.aimPose[0].position[0]=(422-256)*.4f/512;in.aimPose[0].position[1]=(128-159)*.4f/512;in.aimPose[0].position[2]=1;
 in.leftTrigger=1; Update(true); now=2; Update(false); now=2.31; Update(false); assert(activations==0);
 in.leftTrigger=0; Update(false);in.leftTrigger=1;Update(false);assert(activations==1 && last==6);
 Update(false);assert(activations==1); // no repeat while held
 CControllerState pad;SuppressDriving(&pad);assert(pad.acceleration==0);
 in.x=false;now=2.5;assert(!Update(false));assert(!interacting);
 pad.acceleration=255;SuppressDriving(&pad);assert(pad.acceleration==255);
 // Invalid tracking, weapon ownership and blocked/menu frames never activate.
 in.x=true;now=3;Update(false);now=3.4;Update(false);
 in.leftTrigger=0;Update(false);in.aimPose[0].valid=false;in.leftTrigger=1;Update(false);assert(activations==1);
 in.aimPose[0].valid=true;in.leftTrigger=0;Update(false);OculusVR::weapon=true;in.leftTrigger=1;Update(false);assert(activations==1);
 Update(true);assert(!interacting);
 puts("Production radio input: tap/hold, pointer click, held-trigger gating, tracking, weapon exclusion and driving capture passed.");
}
''')
    exe=work/'test'
    subprocess.run(['c++','-std=c++17','-I'+str(root/'overlay/src/vr'),str(work/'main.cpp'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
