#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "../../overlay/src/vr/RadioBoardLayout.h"
struct CVector {
    float x=0,y=0,z=0;
    CVector(float a=0,float b=0,float c=0):x(a),y(b),z(c){}
    CVector operator-(CVector b) const { return {x-b.x,y-b.y,z-b.z}; }
    CVector operator+(CVector b) const { return {x+b.x,y+b.y,z+b.z}; }
    CVector operator*(float s) const { return {x*s,y*s,z*s}; }
};
float DotProduct(CVector a,CVector b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
struct CControllerState { int acceleration=255; void Clear(){acceleration=0;} };
namespace androidgame {
struct PadInput { struct Pose { float position[3]={},orientation[4]={}; bool valid=false; } aimPose[2];
    bool x=false; float leftTrigger=0,rightTrigger=0; } input;
const PadInput &GetPadInput() { return input; }
}
namespace OculusVR {
bool weapon[2]={},valid[2]={}; CVector tips[2];
bool steering[2]={};
bool IsImmersiveSteeringHandleGrabbed(int h) { return steering[h]; }
bool IsTrackedWeaponHeld(int h) { return weapon[h]; }
bool GetTrackedIndexTip(int h,CVector *tip) { *tip=tips[h]; return valid[h] && !weapon[h]; }
}
namespace rw { namespace vulkan {
bool playPoseToFirstPersonWorld(const float *p,const float*,float*,float*,float *f,float *o) {
    memcpy(o,p,12); f[0]=f[1]=0;f[2]=-1;return true;
}
} }
bool writeOK=true; int savedMode=-1;
bool WritePrivateProfileStringA(const char*,const char *key,const char *value,const char*) {
    if(writeOK && !strcmp(key,"InteractionMode")) savedMode=atoi(value);
    return writeOK;
}
const char *GetVrVehicleModelName(int) { return "CAR"; }
namespace platform { const char *gameDataRoot(){return ".";} }
namespace QuestMusic { int rescans=0; void Rescan(){++rescans;} }
namespace QuestRadio {
using RadioBoard::Placement;
struct Plane { CVector origin,right={1,0,0},up={0,1,0},normal={0,0,1}; float width=.4f; };
bool touchMode=false,interacting=false,xDown=false,triggerDown[2]={true,true};
unsigned interactingHands=0;
bool boardAvailable=true,vehicle=true,visible=true,edit=false,modelTarget=false;
RadioBoard::Touch touch[2];
int hover=-1,modelId=1,activations=0,last=-1;
double clockNow=0,xSince=0;
char notice[96]={}; std::string personal="unused";
const char *labels[]={"RIGHT","FORWARD","UP","PITCH","YAW","ROLL","SIZE"};
Placement global,model,bundledGlobal,bundledModel,draft;
void Load() {} bool BuildPlane(Plane &) { return boardAvailable; } bool Vehicle() { return vehicle; }
double Now(){return clockNow;}
void Activate(int b){++activations;last=b;}
Placement TargetPlacement(){return draft;} Placement Resolved(){return draft;}
void CloseEditor(){edit=false;} void BeginEdit(){edit=true;} void SavePlacement(bool=true){}
bool Write(const std::string&,int,const Placement&){return false;}
}
namespace VRHandModel {
constexpr int HAND_COUNT=2,POSE_COUNT=4;
struct Vertex { CVector position[4]; };
struct HandMesh { size_t indexTip=0; std::vector<Vertex> vertices; } gHands[2];
bool meshAvailable=true;
bool LoadHand(int) { return meshAvailable; } bool LoadAlbedo() { return meshAvailable; }
}
#include "radio-touch-production.inc"
void Finger(int hand,int button,float depth,float width=.4f) {
    const auto &b=RadioBoard::Buttons[button];
    OculusVR::tips[hand]=CVector(((b.left+b.right)/2-256)*width/512,
        (128-(b.top+b.bottom)/2)*width/512,depth);
}
int main() {
    using namespace QuestRadio;
    char text[120]; MenuRow(1,text,sizeof(text)); assert(strstr(text,"LASER POINTER"));
    MenuAction(1,1); assert(touchMode && savedMode==1);
    MenuRow(1,text,sizeof(text)); assert(strstr(text,"TOUCH"));
    MenuRow(3,text,sizeof(text)); assert(strstr(text,"RIGHT"));
    MenuRow(15,text,sizeof(text)); assert(!strcmp(text,"BACK TO SETTINGS"));
    OculusVR::valid[0]=true;
    Finger(0,1,.005f); Update(false); assert(activations==0); // tracked inside: unarmed
    Finger(0,1,.04f); Update(false); assert(interacting && hover==1);
    Finger(0,1,.005f); Update(false); assert(activations==1 && last==1);
    for(int i=0;i<60;++i) Update(false);
    assert(activations==1); // holding contact never repeats
    Finger(0,0,.005f); Update(false); assert(activations==1); // sliding never clicks
    Finger(0,0,.04f); Update(false); Finger(0,0,.005f); Update(false);
    assert(activations==2 && last==0);
    CControllerState pad; SuppressDriving(&pad); assert(pad.acceleration==0);
    Finger(0,0,.2f); Update(false); assert(!interacting);
    pad.acceleration=255;SuppressDriving(&pad);assert(pad.acceleration==255);
    // Menu/hidden/tracking/weapon transitions must rearm from in front.
    Update(true); Finger(0,0,.005f); Update(false); assert(activations==2);
    Finger(0,0,.04f);Update(false);OculusVR::valid[0]=false;Update(false);
    OculusVR::valid[0]=true;Finger(0,0,.005f);Update(false);assert(activations==2);
    Finger(0,0,.04f);Update(false);OculusVR::weapon[0]=true;Update(false);
    OculusVR::weapon[0]=false;Finger(0,0,.005f);Update(false);assert(activations==2);
    boardAvailable=false;Update(false);boardAvailable=true;Update(false);assert(activations==2);
    Finger(0,0,-.1f);Update(false);Finger(0,0,.005f);Update(false);assert(activations==2);
    // Both hands are independently armed, with at most one activation per frame.
    OculusVR::valid[1]=true;Finger(0,1,.04f);Finger(1,1,.04f);Update(false);
    Finger(0,1,.005f);Finger(1,1,.005f);Update(false);assert(activations==3);
    Update(false);assert(activations==3);
    MenuAction(1,1);assert(!touchMode && savedMode==0);
    // Ordinary laser short-X tap and hold-to-trigger still work.
    auto &in=androidgame::input;
    in.x=true;clockNow=1;assert(!Update(false));in.x=false;clockNow=1.1;assert(Update(false));
    in.x=true;clockNow=2;Update(false);clockNow=2.4;Update(false);assert(interacting);
    in.aimPose[0].valid=true;
    Finger(0,6,1);memcpy(in.aimPose[0].position,&OculusVR::tips[0].x,12);
    in.leftTrigger=1;Update(false);assert(activations==4 && last==6);
    Update(false);assert(activations==4);
    writeOK=false;MenuAction(1,1);assert(!touchMode && savedMode==0);
    // A held X belongs only to free hands, for either steering hand.
    in.aimPose[1]=in.aimPose[0];
    for(int steeringHand=0;steeringHand<2;++steeringHand) {
        OculusVR::steering[steeringHand]=true;
        in.leftTrigger=in.rightTrigger=0;Update(false);
        assert(interacting && interactingHands==(1u<<(1-steeringHand)));
        int before=activations;
        if(steeringHand==0) in.leftTrigger=1; else in.rightTrigger=1;
        Update(false);assert(activations==before);
        in.leftTrigger=in.rightTrigger=1;Update(false);assert(activations==before+1);
        OculusVR::steering[1-steeringHand]=true;Update(false);
        assert(!interacting && interactingHands==0);
        OculusVR::steering[0]=OculusVR::steering[1]=false;
        Update(false);assert(activations==before+1); // held trigger cannot click on release
    }
    // Direct contact must also leave the steering hand out of UI ownership.
    touchMode=true;in.x=false;
    for(int steeringHand=0;steeringHand<2;++steeringHand) {
        for(auto &t:touch) t.Reset();
        OculusVR::steering[steeringHand]=true;
        Finger(0,1,.04f);Finger(1,1,.04f);Update(false);
        assert(interactingHands==(1u<<(1-steeringHand)));
        int before=activations;
        Finger(0,1,.005f);Finger(1,1,.005f);Update(false);
        assert(activations==before+1 && interactingHands==(1u<<(1-steeringHand)));
        OculusVR::steering[1-steeringHand]=true;Update(false);
        assert(!interacting && interactingHands==0);
        OculusVR::steering[0]=OculusVR::steering[1]=false;
        Update(false);assert(activations==before+1);
    }
    RadioBoard::Touch t;int near=-1;
    for(float width:{.16f,.4f,1.f}) {
        const auto &b=RadioBoard::Buttons[7];float x=((b.left+b.right)/2-256)*width/512;
        float y=(128-(b.top+b.bottom)/2)*width/512;
        t.Update(x,y,.04f,width,near);assert(t.Update(x,y,.005f,width,near)==7);
        assert(t.Update(x,y,.005f,width,near)==-1);
    }
    t.Update(0,0,NAN,.4f,near); assert(!t.armed);
    // Production fingertip interpolation uses the exact vertex and basis used
    // by the animated mesh, rather than a constant controller-space offset.
    for(int hand=0;hand<2;++hand) {
        auto &mesh=VRHandModel::gHands[hand];mesh.vertices.resize(2);mesh.indexTip=1;
        for(int pose=0;pose<4;++pose) mesh.vertices[1].position[pose]=CVector(.1f+.02f*pose,.01f*pose,.03f);
        CVector tip;
        assert(VRHandModel::IndexTip(hand,{1,2,3},{1,0,0},{0,1,0},{0,0,1},.5f,.5f,&tip));
        assert(fabsf(tip.x-1.03f)<.00001f && fabsf(tip.y-1.985f)<.00001f && fabsf(tip.z-3.075f)<.00001f);
        VRHandModel::IndexTip(hand,{0,0,0},{1,0,0},{0,1,0},{0,0,1},0,1,&tip);
        assert(fabsf(tip.z-.085f)<.00001f);
    }
    VRHandModel::meshAvailable=false;CVector tip;
    assert(VRHandModel::IndexTip(0,{0,0,0},{1,0,0},{0,1,0},{0,0,1},0,0,&tip));
    assert(fabsf(tip.x-.03f)<.00001f && tip.z>.16f);
    assert(!VRHandModel::IndexTip(-1,{},{},{},{},0,0,&tip));
    puts("Radio interaction: touch/laser steering ownership, saved mode, contact hysteresis, capture, tracking, weapon gates and laser fallback passed.");
}
