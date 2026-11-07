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
struct CVehicle {
    CMatrix matrix;
    CVector GetForward(){return matrix.f;} CVector GetUp(){return matrix.u;}
    CVector GetRight(){return matrix.r;} CVector GetPosition(){return matrix.p;}
    bool IsBike(){return false;}
};
struct CPed {
    CMatrix root;RpClump *clump;bool player=true,skinned=true;
    bool vehicle=false,bIsDucking=false;CVehicle *m_pMyVehicle=nullptr;
    bool InVehicle(){return vehicle;}CVector GetForward(){return root.f;}
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
static CVector gVrOnFootSkeletonHead;
static float Atan2(float y,float x){return std::atan2(y,x);}
enum {PED_IDLE,PED_DRIVING,PED_JUMP,PED_OPEN_DOOR,PED_CARJACK,PED_ENTER_CAR,PED_STEAL_CAR,PED_ENTER_TRAIN,PED_EXIT_TRAIN};
static bool vehicleTransition,knockedDown,gVrInVehicle,trainPassenger,headModeOnFoot;
static int playerVehicleState=PED_IDLE;
static bool PlayerKnockedDown(CPed*){return knockedDown;}
namespace androidgame {
static bool bobbing=true,boneView=false;
static CVector eyeOffset;
static bool VrHeadBobbingEnabled(){return bobbing;}
static bool VrXboxKnockdownBoneViewEnabled(){return boneView;}
static void VrGetXboxHeadOffset(float *forward,float *height,float *side){*forward=eyeOffset.y;*height=eyeOffset.z;*side=eyeOffset.x;}
}
namespace OculusVR {static bool IsQuestBikeHorizonLocked(){return false;}}
static struct {struct {CVector Front{0,1,0};} Cams[1];int ActiveCam=0;} TheCamera;
namespace rw {namespace vulkan {
static float physicalYaw=0;
static bool getFirstPersonLocalHeadYaw(float *yaw){*yaw=physicalYaw;return true;}
static void setFirstPersonAnchorBasis(const float*h,const float*r,const float*u,const float*f,float,int,bool preserve){
    if(!preserve)std::exit(2);++anchorWrites;
    submittedHead={h[0],h[1],h[2]};submittedRight={r[0],r[1],r[2]};submittedUp={u[0],u[1],u[2]};submittedForward={f[0],f[1],f[2]};
}
}}
static void Require(bool v,const char*m){++checks;if(!v){std::fprintf(stderr,"FAIL %s\n",m);std::exit(1);}}
static void Near(CVector a,CVector b,const char*m){Require((a-b).Magnitude()<0.0001f,m);}
static bool Final(CPed *player,bool thirdPersonVehicle,CVector &head,CVector &forward,bool postPhysics=true){
    bool fullVehicleBasis=false;
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
        Near(submittedHead,p,"bobbing on follows exact head position");Near(submittedForward,player.root.f,"animation head turns do not steer the camera");
        Near(submittedRight,player.root.r,"animation roll does not steer the camera");Near(submittedUp,{0,0,1},"level locomotion up ignores head pitch and rest tilt");
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
    // Execute the real final-camera block against the live toggle, not only
    // the latch helper. Root motion remains immediate while bone gait is removed.
    androidgame::bobbing=false;player.root=CMatrix();
    ++CTimer::frame;nativeHead=RwMatrix();nativeHead.pos={.1f,.2f,1.6f};
    Require(Final(&player,false,p,f),"bobbing off captures standing anatomy");
    for(int i=0;i<200;i++){
        ++CTimer::frame;float yaw=i*.02f;
        player.root.r={std::cos(yaw),std::sin(yaw),0};player.root.f={-std::sin(yaw),std::cos(yaw),0};player.root.p={i*.2f,i*.1f,1};
        nativeHead.pos=player.root*CVector(.1f+std::sin(float(i))*.1f,.2f+std::cos(float(i))*.1f,1.6f+std::sin(float(i))*.2f);
        Require(Final(&player,false,p,f),"idle/walk/run pose available with bobbing off");
        Near(submittedHead,player.root*CVector(.1f,.2f,1.6f),"off removes gait in all axes but follows root without delay");
        Near(submittedUp,{0,0,1},"off retains level horizon");
    }
    for(int posture=0;posture<4;posture++){
        ++CTimer::frame;vehicleTransition=posture==0;knockedDown=posture==1;player.bIsDucking=posture==2;playerVehicleState=posture==3?PED_JUMP:PED_IDLE;
        nativeHead.pos=player.root*CVector(-.3f,.3f,.4f);
        Final(&player,false,p,f);
        Near(submittedHead,posture==2 ? player.root*CVector(.1f,.2f,1.6f)-CVector(0,0,.45f) :
            posture==3 ? player.root*CVector(.1f,.2f,.4f) : nativeHead.pos,
            "crouch and jump retain horizontal neck alignment; jump retains live height");
    }
    vehicleTransition=knockedDown=player.bIsDucking=false;playerVehicleState=PED_IDLE;
    ++CTimer::frame;nativeHead.pos=player.root*CVector(.3f,.3f,1.1f);
    Final(&player,false,p,f);Near(submittedHead,player.root*CVector(.1f,.2f,1.6f),"get-up cannot relatch a low recovery sample");
    androidgame::bobbing=true;++CTimer::frame;Final(&player,false,p,f);Near(submittedHead,nativeHead.pos,"on restores live positional bob immediately");
    androidgame::bobbing=false;
    CVehicle train;train.matrix.p={20,30,2};player.root=train.matrix;
    player.vehicle=gVrInVehicle=trainPassenger=true;player.m_pMyVehicle=&train;playerVehicleState=PED_DRIVING;
    f={-1,0,0};
    ++CTimer::frame;nativeHead.pos=player.root*CVector(-.25f,.1f,1.0f);
    Final(&player,false,p,f,false);Near(submittedHead,nativeHead.pos,"early train occupancy does not commit standing reference");
    nativeHead.pos=player.root*CVector(-.25f,.1f,2.1f);
    Final(&player,false,p,f,true);Near(submittedHead,nativeHead.pos,"train commits actual post-lineup head position");
    ++CTimer::frame;player.root.p+=CVector(5,2,0);nativeHead.pos=player.root*CVector(-.2f,.15f,1.9f);
    Final(&player,false,p,f);Near(submittedHead,nativeHead.pos,"standing train follows live native head rather than retaining the first passenger pose");
    Near(submittedForward,{-1,0,0},"Xbox override preserves the boarding-side window heading instead of entity forward");
    androidgame::eyeOffset={.02f,.06f,.08f};
    for(int step=0;step<90;++step){
        const float angle=float(step)*.01f;train.matrix.r={std::cos(angle),std::sin(angle),0};
        train.matrix.f={-std::sin(angle),std::cos(angle),0};train.matrix.p+=CVector(.2f,.04f,0);
        player.root=train.matrix;f=-train.matrix.r;
        ++CTimer::frame;nativeHead.pos=player.root*CVector(-.25f,.1f,2.1f+std::sin(float(step))*.02f);
        Final(&player,false,p,f);
        const CVector right=CrossProduct(-train.matrix.r,train.matrix.u);
        Near(submittedHead,nativeHead.pos-train.matrix.r*.06f+train.matrix.u*.08f+right*.02f,
            "moving train keeps calibrated eye ahead of live body along the outward heading");
        Near(submittedForward,-train.matrix.r,"passenger view follows carriage-relative windows while moving");
    }
    androidgame::eyeOffset={};
    trainPassenger=false;train.matrix.f={1,0,0};train.matrix.r={0,-.8f,.6f};train.matrix.u={0,.6f,.8f};
    ++CTimer::frame;Final(&player,false,p,f);
    Near(submittedForward,train.matrix.f,"driving follows carrier heading rather than head animation");
    Near(submittedUp,train.matrix.u,"driving retains carrier tilt");
    player.vehicle=gVrInVehicle=false;player.m_pMyVehicle=nullptr;headModeOnFoot=true;
    TheCamera.Cams[0].Front={3,0,4};
    ++CTimer::frame;Final(&player,false,p,f);
    Near(submittedForward,{1,0,0},"head-relative locomotion keeps gameplay heading without animation pitch");
    Near(submittedUp,{0,0,1},"returning on foot levels the horizon");
    headModeOnFoot=false;player.root=CMatrix();knockedDown=true;androidgame::boneView=true;
    androidgame::eyeOffset={.02f,.06f,.08f};
    ++CTimer::frame;nativeHead=RwMatrix();nativeHead.pos={1,2,.3f};
    nativeHead.right={0,1,0};nativeHead.up={0,0,-1};nativeHead.at={-1,0,0};
    Final(&player,false,p,f);
    Near(submittedUp,{0,1,0},"knockdown may follow native head orientation through get-up");
    Near(submittedHead,CVector(1,2,.3f)+CVector(.02f,.08f,-.06f),"eye fit follows the downed bone axes");
    knockedDown=false;++CTimer::frame;Final(&player,false,p,f);
    Near(submittedUp,{0,0,1},"standing recovers level horizon independent of bone view option");
    Require(resets==anchorWrites*2,"legacy smoothing cleared on every exact submission");
    QuestHeadAttachment::ComfortAnchor button;
    CMatrix root;const CVector standingHead(.1f,.2f,1.6f);
    button.Position(&player,&clump,nullptr,root,standingHead,false,true,true);
    for(int frame=0;frame<100;++frame){
        const CVector animated(-.4f+.01f*frame,.5f,.7f);
        Near(button.Position(&player,&clump,nullptr,root,animated,false,false,true,true),
            standingHead-CVector(0,0,.45f),"button crouch lowers vertically without native horizontal drift");
    }
    Near(button.Position(&player,&clump,nullptr,root,standingHead,false,true,true),standingHead,"button stand restores original position");
    vehicleTransition=true;trainPassenger=false;knockedDown=false;
    androidgame::boneView=false;
    for(int entry:{PED_OPEN_DOOR,PED_CARJACK,PED_ENTER_CAR,PED_STEAL_CAR}){
        playerVehicleState=entry;++CTimer::frame;
        Final(&player,false,p,f);
        Near(submittedForward,CVector(nativeHead.up),"car entry follows native animation facing rather than idle heading");
        Near(submittedUp,CVector(nativeHead.right),"car entry retains authored head basis");
    }
    CVehicle car;player.vehicle=gVrInVehicle=true;player.m_pMyVehicle=&car;
    vehicleTransition=false;playerVehicleState=PED_DRIVING;++CTimer::frame;
    Final(&player,false,p,f);
    Near(submittedForward,car.GetForward(),"completed entry hands view forward to driving carrier");
    Near(submittedUp,car.GetUp(),"completed entry restores driving up vector");
    player.vehicle=gVrInVehicle=false;player.m_pMyVehicle=nullptr;playerVehicleState=PED_IDLE;
    ++CTimer::frame;Final(&player,false,p,f);
    Near(submittedUp,{0,0,1},"cancelled/finished entry does not leak animation tilt into on-foot comfort view");
    train.matrix=CMatrix();player.m_pMyVehicle=&train;player.vehicle=gVrInVehicle=false;
    trainPassenger=vehicleTransition=headModeOnFoot=true;playerVehicleState=PED_EXIT_TRAIN;
    androidgame::eyeOffset={.02f,.06f,.08f};
    for(float side:{-1.0f,1.0f})for(int step=0;step<36;++step){
        const float angle=float(step)*.1745329252f;train.matrix.r={std::cos(angle),std::sin(angle),0};train.matrix.f={-std::sin(angle),std::cos(angle),0};
        player.root=train.matrix;nativeHead.pos=player.root*CVector(side*.7f,.1f,1.8f);
        f=-train.matrix.r*side;TheCamera.Cams[0].Front=f;
        ++CTimer::frame;Final(&player,false,p,f);
        const CVector outward=train.matrix.r*side,right=CrossProduct(outward,train.matrix.u);
        Near(submittedForward,outward,"train exit stays outward on either carriage side despite inward gameplay heading");
        Near(submittedHead,nativeHead.pos+outward*.06f+train.matrix.u*.08f+right*.02f,"train exit eye fit remains attached in the outward basis");
    }
    playerVehicleState=PED_ENTER_TRAIN;
    for(int step=0;step<=36;++step){
        const float yaw=float(step)*.0872664626f;
        nativeHead.up={std::cos(yaw),std::sin(yaw),0};nativeHead.right={0,0,1};nativeHead.at=-CrossProduct(nativeHead.up,nativeHead.right);
        ++CTimer::frame;Final(&player,false,p,f);
        Near(submittedForward,nativeHead.up,"boarding camera follows the authored turn despite an unrelated gameplay heading");
        Near(submittedUp,{0,0,1},"boarding maintains a level horizon");
    }
    trainPassenger=vehicleTransition=false;playerVehicleState=PED_IDLE;player.m_pMyVehicle=nullptr;
    TheCamera.Cams[0].Front={0,1,0};++CTimer::frame;Final(&player,false,p,f);
    Near(submittedForward,{0,1,0},"completed train exit restores on-foot heading policy");
    androidgame::bobbing=true;androidgame::eyeOffset={.02f,.06f,.08f};
    for(float rootYaw:{0.0f,.7f,3.14159265f})
    for(float cameraYaw:{0.0f,1.5707963f,3.14159265f})
    for(float physicalYaw:{0.0f,1.5707963f,3.14159265f,-1.5707963f})
    for(int state:{PED_IDLE,PED_JUMP}){
        player.root.r={std::cos(rootYaw),std::sin(rootYaw),0};
        player.root.f={-std::sin(rootYaw),std::cos(rootYaw),0};player.root.p={12,7,3};
        TheCamera.Cams[0].Front={-std::sin(cameraYaw),std::cos(cameraYaw),0};
        rw::vulkan::physicalYaw=physicalYaw;playerVehicleState=state;
        const CVector local(.03f,.04f,state==PED_JUMP?1.3f:1.6f);
        nativeHead.pos=player.root*(local+CVector(0,state==PED_JUMP?.31f:0,0));
        const CVector originalNativeHead=nativeHead.pos;
        ++CTimer::frame;Final(&player,false,p,f);
        const float yaw=cameraYaw+physicalYaw;
        const CVector facing(-std::sin(yaw),std::cos(yaw),0),right(std::cos(yaw),std::sin(yaw),0);
        Near(submittedHead,player.root.p+right*(local.x+.02f)+facing*(local.y+.06f)+CVector(0,0,local.z+.08f),
            "idle/jump head and eye fit follow physical facing independently of native heading");
        Near(gVrOnFootSkeletonHead,player.root.p+right*local.x+facing*local.y+CVector(0,0,local.z),
            "posture target shares the physical head frame before eye fit");
        Near(nativeHead.pos,originalNativeHead,"facing correction never mutates native animation");
        Near(submittedForward,TheCamera.Cams[0].Front,"physical yaw is not applied twice to anchor orientation");
    }
    std::printf("PASS %u native head attachment and final camera checks\n",checks);
}
