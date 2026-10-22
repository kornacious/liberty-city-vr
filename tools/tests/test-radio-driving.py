#!/usr/bin/env python3
"""Verify the production radio/driving/firing boundary without game assets."""
from pathlib import Path
import re
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
driving=(root/'overlay/src/vr/QuestDrivingVR.cpp').read_text()
weapons=(root/'overlay/src/vr/QuestWeaponVR.cpp').read_text()
def method(text,name):
    match=re.search(r'(?ms)^(?:static )?(?:void|uint32)\s+'+name+r'\(.*?^}',text)
    assert match,name
    return match.group()
code='\n'.join(method(driving,n) for n in ['MapButtonDriveBy','UpdateQuestDrivingInput','ApplyQuestVehicleButtonInput'])
code+='\n'+method(weapons,'UpdateWeaponTriggerEdges')
call=re.search(r'(?s)if\(inVehicle\)\s*UpdateWeaponTriggerEdges\([^;]+;',weapons).group()
code+='\nvoid ProductionFireEdgeGate(bool menu,bool gameplay,uint32 vehicleCapturedHands,bool inVehicle,const androidgame::PadInput &input){'+call+'}\n'
with tempfile.TemporaryDirectory(prefix='radio-driving-') as d:
    work=Path(d);(work/'production.inc').write_text(code)
    (work/'main.cpp').write_text(r'''
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <algorithm>
#define nil nullptr
using uint32=uint32_t;using int16=int16_t;
constexpr int VR_HAND_COUNT=2;
template<class T,class U,class V>T clamp(T v,U a,V b){return std::max(T(a),std::min(v,T(b)));}
struct CControllerState {int Square=0,Cross=0,Circle=0,LeftShoulder1=0,RightShoulder1=0,LeftShoulder2=0,RightShoulder2=0;void Clear(){*this={};}};
struct CVehicle {void*pDriver=nullptr;bool IsRealHeli(){return false;}bool IsRealPlane(){return false;}bool IsCar(){return true;}bool IsBike(){return false;}} vehicle;
int player;CVehicle*FindPlayerVehicle(){return &vehicle;}void*FindPlayerPed(){return &player;}
struct CPad {int GetMode(){return 0;}static CPad*GetPad(int){static CPad pad;return &pad;}};
namespace androidgame {struct PadInput{bool x=false,b=false,a=false;float leftTrigger=0,rightTrigger=0,leftGrip=0,rightGrip=0;}input;const PadInput&GetPadInput(){return input;}}
namespace QuestRadio {bool active=false;bool Update(bool blocked){if(blocked)active=false;return false;}bool Interacting(){return active;}void SuppressDriving(CControllerState*s){if(active)s->Clear();}}
namespace OculusVR {
CVehicle*gButtonDriveByVehicle=nullptr;int gButtonDriveByDirection=-1;
bool gVehicleForwardFireEnabled=true,gVrRadioChangeJustPressed=false;
bool physical=false;uint32 bikeBlocked=0,carBlocked=0;bool motionBlocked=false;
bool gPoseValid[2]={true,true},gTriggerPressed[2]={},gTriggerJustPressed[2]={},gTriggerJustReleased[2]={};
float gTrigger[2]={};int gHeldSlot[2]={-1,-1};
constexpr int VR_DRIVING_DEFAULT=0;int GetDrivingTypeForVehicle(CVehicle*){return physical?1:0;}
void LoadDrivingSettings(){}bool IsVrDrivingActive(){return physical;}bool IsImmersiveDrivingActive(){return physical;}bool IsTrackedWeaponHeld(int h){return gHeldSlot[h]>=0;}
bool IsQuestVehicleThirdPerson(){return false;}bool IsVrCarActive(){return physical;}bool IsVrBikeActive(){return false;}bool IsQuestDrivingHandUnavailable(int){return false;}
uint32 UpdateImmersiveBikeInput(const float*,const androidgame::PadInput&,uint32 blocked){bikeBlocked=blocked;return 0;}
uint32 UpdateImmersiveCarInput(const float*,uint32 blocked){carBlocked=blocked;return 0;}
void UpdateMotionInput(const androidgame::PadInput&,bool blocked){motionBlocked=blocked;}
void MapHornToPad(CControllerState*){}void MapHandbrakeToPad(CControllerState*s,bool on){if(on)s->LeftShoulder1=255;}
#include "production.inc"
}
int main(){
 using namespace OculusVR;vehicle.pDriver=&player;auto &input=androidgame::input;CControllerState state;
 input.rightTrigger=.8f;input.leftTrigger=.6f;input.b=true;
 assert(UpdateQuestDrivingInput(&state,false)==0);ApplyQuestVehicleButtonInput(&state,false);
 assert(state.Cross>0 && state.Square>0 && gButtonDriveByVehicle==&vehicle);
 QuestRadio::active=true;assert(UpdateQuestDrivingInput(&state,false)==3);
 assert(bikeBlocked==3 && carBlocked==3 && motionBlocked);
 ApplyQuestVehicleButtonInput(&state,false);
 assert(state.Cross==0 && state.Square==0 && state.Circle==0 && gButtonDriveByVehicle==nullptr);
 physical=true;gHeldSlot[1]=2;
 ProductionFireEdgeGate(false,true,3,true,input);assert(!gTriggerPressed[1] && !gTriggerJustPressed[1]);
 QuestRadio::active=false;UpdateQuestDrivingInput(&state,false);ApplyQuestVehicleButtonInput(&state,false);
 ProductionFireEdgeGate(false,true,3,true,input);assert(gTriggerPressed[1] && gTriggerJustPressed[1]);
 assert(state.Cross>0 && !motionBlocked);
 ProductionFireEdgeGate(true,true,0,true,input);assert(!gTriggerPressed[1]);
 puts("Production driving integration: radio captures throttle/brake, wheel/motion input and both native/physical firing; ordinary input restores correctly.");
}
''')
    exe=work/'test';subprocess.run(['c++','-std=c++17',str(work/'main.cpp'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
