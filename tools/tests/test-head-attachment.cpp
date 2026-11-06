#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <initializer_list>
#include "ragdoll-test-vector.h"
#define nil nullptr
using std::isfinite;
struct CMatrix {
    CVector r{1,0,0}, f{0,1,0}, u{0,0,1}, p;
    CVector operator*(CVector v)const{return p+r*v.x+f*v.y+u*v.z;}
};
static CVector Multiply3x3(const CMatrix &m,CVector v){return m.r*v.x+m.f*v.y+m.u*v.z;}
static CMatrix Invert(const CMatrix &m){
    CMatrix out;out.r={m.r.x,m.f.x,m.u.x};out.f={m.r.y,m.f.y,m.u.y};out.u={m.r.z,m.f.z,m.u.z};
    out.p={-DotProduct(m.r,m.p),-DotProduct(m.f,m.p),-DotProduct(m.u,m.p)};return out;
}
// Actual Xbox bone axes: +X vertical, +Y facing, +Z left.
struct RwMatrix {CVector pos,right{0,0,1},up{0,1,0},at{-1,0,0};};
#include "../../overlay/src/peds/QuestHeadAttachment.h"
enum {BONE_head=9};
struct RpHAnimHierarchy {int numNodes=16;RwMatrix matrices[16];};
struct RpClump {RpHAnimHierarchy *hier;};
static RwMatrix nativeHead;
static unsigned rwUpdates,animUpdates,checks;
struct CPed {
    CMatrix root;RpClump *clump;bool player=true,skinned=true;
    bool IsPlayer(){return player;} RpClump *GetClump(){return clump;}
    CMatrix &GetMatrix(){return root;}
    void UpdateRwFrame(){++rwUpdates;}
    void UpdateRpHAnim(){++animUpdates;clump->hier->matrices[9]=nativeHead;}
};
static bool IsClumpSkinned(RpClump*){return true;}
static RpHAnimHierarchy *GetAnimHierarchyFromSkinClump(RpClump*c){return c->hier;}
static int RpHAnimIDGetIndex(RpHAnimHierarchy*,int id){return id;}
static RwMatrix *RpHAnimHierarchyGetMatrixArray(RpHAnimHierarchy*h){return h->matrices;}
namespace ModelSets {static bool modern=true;static bool IsModernActive(){return modern;}}
namespace VrRagdoll {
static bool owns;static RwMatrix head;
static bool Owns(CPed*){return owns;}
static void Apply(CPed*p){p->clump->hier->matrices[9]=head;}
}
struct CTimer {static unsigned frame;static unsigned GetFrameCounter(){return frame;}};unsigned CTimer::frame=0;
namespace VrBodyIK {
#include "head-attachment-native.inc"
}
static unsigned resets,anchorWrites;
static struct {void Reset(){++resets;}} gCrouchView,gKnockdownView;
static CVector submittedHead,submittedRight,submittedUp,submittedForward;
static float Atan2(float y,float x){return std::atan2(y,x);}
namespace rw {namespace vulkan {
static void setFirstPersonAnchorBasis(const float*h,const float*r,const float*u,const float*f,float,int,bool preserve){
    if(!preserve)std::exit(2);++anchorWrites;
    submittedHead={h[0],h[1],h[2]};submittedRight={r[0],r[1],r[2]};submittedUp={u[0],u[1],u[2]};submittedForward={f[0],f[1],f[2]};
}
}}
static void Require(bool v,const char*m){++checks;if(!v){std::fprintf(stderr,"FAIL %s\n",m);std::exit(1);}}
static void Near(CVector a,CVector b,const char*m){Require((a-b).Magnitude()<0.0001f,m);}
static bool Final(CPed *player,bool thirdPersonVehicle,CVector &head,CVector &forward){
    bool fullVehicleBasis=false;
    bool postPhysics=true;
#include "head-attachment-final.inc"
    return fullVehicleBasis;
}
int main(){
    RpHAnimHierarchy hierarchy;RpClump clump{&hierarchy};CPed player{{},&clump};
    {
        QuestHeadAttachment::Snapshot upright;
        RwMatrix xboxHead;xboxHead.pos={.25f,-.15f,1.6f};
        upright.Capture(&player,&clump,0,player.root,xboxHead);
        QuestHeadAttachment::Pose pose;
        Require(upright.Read(player.root,pose),"upright authored Xbox pose accepted");
        Near(pose.up,{0,0,1},"upright Xbox bone produces level camera up");
        Near(pose.right,{1,0,0},"upright Xbox bone produces camera right");
        Near(pose.forward,{0,1,0},"upright Xbox bone preserves forward heading");
        Near(pose.position,xboxHead.pos,"axis correction does not move the head anchor");
    }
    for(int rate:{30,36,60,72,80,90,120,144})for(unsigned step=0;step<180;step++){
        ++CTimer::frame;
        float t=float(step)/rate,yaw=t*.7f;
        player.root.r={std::cos(yaw),std::sin(yaw),0};player.root.f={-std::sin(yaw),std::cos(yaw),0};player.root.p={t*21,t*3,1.5f};
        // Includes the train's unusual root offset and right-side boarding.
        nativeHead.pos=player.root*CVector(-.27f,.13f,.71f+std::sin(t)*.2f);
        float pitch=std::sin(t)*.6f,roll=std::cos(t)*.5f;
        nativeHead.up=player.root.f*std::cos(pitch)+player.root.u*std::sin(pitch);
        CVector vertical=player.root.u*std::cos(pitch)-player.root.f*std::sin(pitch);
        const CVector expectedRight=player.root.r*std::cos(roll)+vertical*std::sin(roll);
        const CVector expectedUp=vertical*std::cos(roll)-player.root.r*std::sin(roll);
        nativeHead.right=expectedUp;
        nativeHead.at=-expectedRight;
        CVector p,r,u,f;
        unsigned before=animUpdates;
        Require(VrBodyIK::GetCameraHeadPose(&player,&p,&r,&u,&f,false),"first call refreshes native head immediately");
        Require(animUpdates==before+1,"one native refresh before PreRender");Near(p,nativeHead.pos,"exact native position without eye/seat offsets");
        Near(r,expectedRight,"authored bone axes decode to expected camera right");
        Near(u,expectedUp,"authored bone axes decode to expected camera up");
        hierarchy.matrices[9]=nativeHead;VrBodyIK::CaptureNativeCameraHead(&player);
        hierarchy.matrices[9].pos={900,-900,-900};hierarchy.matrices[9].right={};hierarchy.matrices[9].up={};hierarchy.matrices[9].at={};
        Require(VrBodyIK::GetCameraHeadPose(&player,&p,&r,&u,&f,true),"hidden/IK head cannot replace native snapshot");
        Require(animUpdates==before+1,"render lookup does not rebuild or overwrite the solved skeleton");Near(p,nativeHead.pos,"no feedback from presentation IK or head hiding");
        player.root.p+=CVector(.07f,-.03f,.01f);
        Require(VrBodyIK::GetCameraHeadPose(&player,&p,&r,&u,&f,true),"post-physics pose available");
        Near(p,nativeHead.pos+CVector(.07f,-.03f,.01f),"same-frame carrier motion has no delay");
        CVector legacyHead(50,-40,-3),legacyForward(1,0,0);
        Require(Final(&player,false,legacyHead,legacyForward),"native pose owns final camera submission");
        Near(submittedHead,p,"final submission erases legacy crouch/train/knockdown offsets");Near(submittedForward,f,"head orientation submitted");
        Near(submittedRight,r,"head roll submitted");Near(submittedUp,u,"head pitch submitted");
    }
    CVector p(1,2,3),f(0,1,0);unsigned writes=anchorWrites;
    Require(!Final(&player,true,p,f)&&anchorWrites==writes,"third person keeps its own camera");
    ModelSets::modern=false;Require(!Final(&player,false,p,f)&&anchorWrites==writes,"Classic bypasses bone attachment");
    Near(p,{1,2,3},"Classic position untouched");
    ModelSets::modern=true;CPed other=player;CVector r,u;
    Require(VrBodyIK::GetCameraHeadPose(&other,&p,&r,&u,&f,false),"new player identity refreshes the snapshot");
    ++CTimer::frame;
    Require(VrBodyIK::GetCameraHeadPose(&other,&p,&r,&u,&f,false),"hidden body can sample before simulation");
    nativeHead.pos+=CVector(.1f,.2f,.3f);
    Require(VrBodyIK::GetCameraHeadPose(&other,&p,&r,&u,&f,true),"without PreRender refresh native pose after simulation");
    Near(p,nativeHead.pos,"hidden body does not retain a pre-simulation animation sample");
    ++CTimer::frame;VrRagdoll::owns=true;VrRagdoll::head=nativeHead;
    VrRagdoll::head.pos={-3,4,.2f};
    Require(VrBodyIK::GetCameraHeadPose(&other,&p,&r,&u,&f,true),"hidden ragdoll can refresh before PreRender");
    Near(p,VrRagdoll::head.pos,"ragdoll head replaces native animation in the fallback path");
    VrRagdoll::owns=false;
    QuestHeadAttachment::Snapshot invalid;
    RwMatrix bad=nativeHead;bad.up={};
    invalid.Capture(&player,&clump,CTimer::frame,player.root,bad);
    QuestHeadAttachment::Pose out;
    Require(!invalid.Read(player.root,out),"collapsed head cannot become a camera pose");
    bad=nativeHead;bad.pos.x=NAN;
    invalid.Capture(&player,&clump,CTimer::frame,player.root,bad);
    Require(!invalid.Read(player.root,out),"non-finite native position rejected");
    Require(resets==anchorWrites*2,"legacy smoothing cleared on every exact submission");
    std::printf("PASS %u native head attachment and final camera checks\n",checks);
}
