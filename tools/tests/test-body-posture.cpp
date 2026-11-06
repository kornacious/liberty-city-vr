#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include "ragdoll-test-vector.h"
using std::isfinite;
#define nil nullptr
#include "QuestBodyPosture.h"
static unsigned checks;
static void Require(bool ok,const char *message){++checks;if(!ok){std::fprintf(stderr,"FAIL %s\n",message);std::exit(1);}}
static void Near(CVector a,CVector b,const char *message,float tolerance=0.0003f){Require((a-b).Magnitude()<tolerance,message);}
static QuestBodyPosture::Input Standing(){
    QuestBodyPosture::Input in;
    in.spine[0]={0,0,.85f};in.spine[1]={0,0,1.07f};in.spine[2]={0,0,1.32f};in.spine[3]={0,0,1.57f};
    for(int i=0;i<2;++i){float x=i==0?-.12f:.12f;in.hip[i]={x,0,.82f};in.knee[i]={x,.08f,.42f};in.foot[i]={x,0,.02f};}
    in.up={0,0,1};in.forward={0,1,0};in.authoredDrop=0;return in;
}
struct CMatrix {
    CVector r{1,0,0},u{0,0,1},f{0,1,0},p;
    const CVector &GetRight()const{return r;}const CVector &GetUp()const{return u;}
    const CVector &GetForward()const{return f;}const CVector &GetPosition()const{return p;}
};
struct RwMatrix {CVector pos,right{0,0,1},up{0,1,0},at{-1,0,0};int flags=0;};
struct RpHAnimHierarchy {int numNodes=14;int parents[14]={-1,0,1,2,0,4,5,0,7,8,2,10,11,12};};
struct CPed {bool modern=true,vehicle=false,bIsStanding=false,bIsInTheAir=false,bIsDucking=false;int m_nPedState=0,m_nMoveState=0;CVector position;bool InVehicle(){return vehicle;}CVector GetPosition(){return position;}void *GetClump(){return this;}};
struct UpperBodyBones {int waist=0,mid=1,torso=2,head=3;};
enum {PED_JUMP=1,MAX_BODY_IK_NODES=96,BONE_upperlegl=4,BONE_lowerlegl=5,BONE_footl=6,BONE_upperlegr=7,BONE_lowerlegr=8,BONE_footr=9};
enum {PEDMOVE_STILL,PEDMOVE_WALK};
struct CEntity {};
struct CColPoint {CVector point,normal;};
static bool groundFound=false,headTargetAvailable=false;
static CVector skeletonHead;
static CVector viewRight(1,0,0),viewForward(0,1,0);
static bool fixedEyeFit=false;
namespace CWorld {
static bool ProcessVerticalLine(CVector from,float,CColPoint &hit,CEntity*&,bool,bool,bool,bool,bool,bool,void*){
    hit.point={from.x,from.y,0};hit.normal={0,0,1};return groundFound;
}
}
struct CTimer {static unsigned frame;static unsigned GetFrameCounter(){return frame;}static float GetTimeStep(){return 1.0f;}};unsigned CTimer::frame=0;
static QuestBodyPosture::Stance gTrackedStance;
static struct {QuestBodyPosture::Balance balance;}gUpperBody;
static bool gVehicleTransitionActive,tracking=true;
static CVector eye,anchor;
static bool UsesNativeCameraHead(CPed *ped){return ped->modern;}
static int RpHAnimIDGetIndex(RpHAnimHierarchy*,int id){return id;}
static void BuildParentTable(RpHAnimHierarchy *hier,int *parents){for(int i=0;i<hier->numNodes;++i)parents[i]=hier->parents[i];}
static bool IsDescendantOf(int node,int ancestor,const int *parents){for(int p=parents[node];p>=0;p=parents[p])if(p==ancestor)return true;return false;}
static CVector ToBasisLocal(CVector p,const CMatrix &m){return {DotProduct(p,m.r),DotProduct(p,m.u),DotProduct(p,m.f)};}
static CVector FromBasisLocal(CVector p,const CMatrix &m){return m.r*p.x+m.u*p.y+m.f*p.z;}
static float ClampF(float value,float lo,float hi){return QuestBodyPosture::Bounded(value,lo,hi);}
static CMatrix MatrixFromRw(const RwMatrix &m){CMatrix b;b.p=m.pos;b.r=m.right;b.u=m.up;b.f=m.at;return b;}
namespace androidgame {
static bool VrGetViewBasis(float *p,float *r,float*,float *f){if(!tracking)return false;p[0]=eye.x;p[1]=eye.y;p[2]=eye.z;if(r){r[0]=viewRight.x;r[1]=viewRight.y;r[2]=viewRight.z;}if(f){f[0]=viewForward.x;f[1]=viewForward.y;f[2]=viewForward.z;}return true;}
static bool VrGetOnFootBodyAnchor(float *p){p[0]=anchor.x;p[1]=anchor.y;p[2]=anchor.z;return true;}
static bool VrGetOnFootSkeletonHead(float *p){p[0]=skeletonHead.x;p[1]=skeletonHead.y;p[2]=skeletonHead.z;return headTargetAvailable;}
static void VrGetXboxHeadOffset(float *f,float *u,float *r){const CVector fit=fixedEyeFit?CVector(.02f,.06f,.08f):anchor-skeletonHead;*f=fit.y;*u=fit.z;*r=fit.x;}
}
#include "body-posture-runtime.inc"
int main(){
    using namespace QuestBodyPosture;
    const Input standing=Standing();Output neutral;
    Require(Solve(standing,neutral),"neutral anatomy supported");
    for(int i=0;i<4;++i)Near(neutral.spine[i],standing.spine[i],"neutral keeps native spine");
    for(int i=0;i<2;++i){Near(neutral.knee[i],standing.knee[i],"neutral keeps knees");Near(neutral.foot[i],standing.foot[i],"neutral keeps feet");}
    for(int rate:{30,36,60,72,80,90,120,144})for(int frame=0;frame<240;++frame){
        Input in=standing;float t=float(frame)/float(rate);
        in.delta={std::sin(t)*.24f,std::cos(t)*.23f,-.6f*(.5f+.5f*std::sin(t))};
        Output out;Require(Solve(in,out),"crouch and lean solve");
        for(int i=0;i<3;++i)Require(std::fabs((out.spine[i+1]-out.spine[i]).Magnitude()-(in.spine[i+1]-in.spine[i]).Magnitude())<.0002f,"spine length preserved");
        for(int i=0;i<2;++i){
            Require(std::fabs((out.knee[i]-out.hip[i]).Magnitude()-(in.knee[i]-in.hip[i]).Magnitude())<.0002f,"thigh length preserved");
            Require(std::fabs((out.foot[i]-out.knee[i]).Magnitude()-(in.foot[i]-in.knee[i]).Magnitude())<.0002f,"shin length preserved");
            const float reach=(in.knee[i]-in.hip[i]).Magnitude()+(in.foot[i]-in.knee[i]).Magnitude();
            if((in.foot[i]-out.hip[i]).Magnitude()<reach-.001f) Near(out.foot[i],in.foot[i],"reachable feet remain at native contact");
            Require(Finite(out.knee[i]),"finite knees at all rates");
        }
        // Frame duration must never change an identical physical pose.
        Output repeat;Require(Solve(in,repeat),"repeat solve");Near(repeat.spine[3],out.spine[3],"stateless pose is frame rate independent");
    }
    Input crouch=standing;crouch.delta={0,0,-.4f};crouch.authoredDrop=.5f;
    Output out;Require(Solve(crouch,out),"combined physical and button crouch supported");Near(out.spine[0],standing.spine[0],"authored lowering is not applied twice");
    Input deep=standing;deep.delta={10,10,-10};Require(Solve(deep,out),"extreme tracked displacement bounded");Require((out.spine[0]-standing.spine[0]).Magnitude()<.7f,"pelvis remains bounded");
    Input bad=standing;bad.delta.x=NAN;Require(!Solve(bad,out),"invalid tracking rejected");bad=standing;bad.spine[1]=bad.spine[0];Require(!Solve(bad,out),"collapsed spine rejected");
    Near(Swing({0,0,1},{0,0,1},{0,0,-1}),{0,0,-1},"antiparallel swing maps the reversed direction");
    CVector rotated=Swing({0,0,1},{0,0,1},{0,1,0});Near(rotated,{0,1,0},"swing maps segment direction");

    // Real production branch adapter: exercise torso/shoulder propagation,
    // planted foot orientation and ownership guards, rather than a model of it.
    RpHAnimHierarchy hierarchy;CPed ped;UpperBodyBones body;CMatrix visual;
    RwMatrix native[14];for(int i=0;i<4;++i)native[i].pos=standing.spine[i];
    for(int i=0;i<2;++i){native[4+i*3].pos=standing.hip[i];native[5+i*3].pos=standing.knee[i];native[6+i*3].pos=standing.foot[i];}
    native[10].pos={-.15f,0,1.32f};native[11].pos={-.25f,0,1.3f};native[12].pos={-.35f,.1f,1.1f};native[13].pos={-.35f,.2f,1};
    anchor={0,0,1.57f};eye=anchor+CVector(.2f,.15f,-.4f);
    for(int guard=0;guard<5;++guard){
        RwMatrix mats[14];for(int i=0;i<14;++i)mats[i]=native[i];
        ped.modern=guard!=0;ped.vehicle=guard==1;gVehicleTransitionActive=guard==2;ped.m_nPedState=guard==3?PED_JUMP:0;tracking=guard!=4;
        ApplyTrackedPosture(&ped,&hierarchy,mats,body,visual,0);
        for(int i=0;i<14;++i){Near(mats[i].pos,native[i].pos,"excluded owner leaves skeleton intact");Near(mats[i].right,native[i].right,"excluded owner leaves orientation intact");}
    }
    ped.modern=true;ped.vehicle=gVehicleTransitionActive=false;ped.m_nPedState=0;tracking=true;
    RwMatrix mats[14];for(int i=0;i<14;++i)mats[i]=native[i];
    const CVector originalEye=eye,originalAnchor=anchor;
    ApplyTrackedPosture(&ped,&hierarchy,mats,body,visual,0);
    Require(mats[0].pos.z<native[0].pos.z-.25f,"production adapter lowers pelvis");
    Require(mats[3].pos.x>native[3].pos.x+.12f,"production adapter leans spine toward headset");
    Require((mats[10].pos-native[10].pos).Magnitude()>.1f,"shoulders move before arm solve");
    Near(mats[6].pos,native[6].pos,"left contact retained");Near(mats[9].pos,native[9].pos,"right contact retained");
    Near(mats[6].right,native[6].right,"foot authored orientation retained");
    Near(eye,originalEye,"posture never writes tracked view");Near(anchor,originalAnchor,"posture never feeds back to camera anchor");Near(ped.position,{},"posture never changes gameplay root");
    for(int i=0;i<14;++i){Require(Finite(mats[i].pos),"finite transformed branches");Require(std::fabs(DotProduct(mats[i].right,mats[i].up))<.0002f,"bone frame stays orthogonal");}
    // Large roomscale movement already carried the skeleton. Only its residual
    // lean should reach the solver; translations and yaw must not change shape.
    const CVector carry(1.2f,-.7f,0);
    RwMatrix carried[14];for(int i=0;i<14;++i){carried[i]=native[i];carried[i].pos+=carry;}
    visual.p=carry;eye=originalEye+carry;
    ApplyTrackedPosture(&ped,&hierarchy,carried,body,visual,0);
    for(int i=0;i<14;++i)Near(carried[i].pos,mats[i].pos+carry,"roomscale follow is not applied twice");
    visual=CMatrix();ped.position={80,-50,4};visual.p=ped.position;
    auto yaw=[](CVector p){return CVector(-p.y,p.x,p.z);};
    for(int i=0;i<14;++i){carried[i]=native[i];carried[i].pos=ped.position+yaw(native[i].pos);carried[i].right=yaw(native[i].right);carried[i].up=yaw(native[i].up);carried[i].at=yaw(native[i].at);}
    anchor=ped.position+yaw(originalAnchor);eye=ped.position+yaw(originalEye);visual.r={0,1,0};visual.f={-1,0,0};
    ApplyTrackedPosture(&ped,&hierarchy,carried,body,visual,0);
    for(int i=0;i<14;++i)Near(carried[i].pos,ped.position+yaw(mats[i].pos),"yaw and world translation preserve posture",.001f);
    hierarchy.parents[body.head]=-1;
    for(int i=0;i<14;++i)carried[i]=native[i];
    ApplyTrackedPosture(&ped,&hierarchy,carried,body,visual,0);
    for(int i=0;i<14;++i)Near(carried[i].pos,native[i].pos,"unsupported hierarchy remains native");
    hierarchy.parents[body.head]=body.torso;visual=CMatrix();ped.position={};anchor=originalAnchor;eye=anchor+CVector(0,0,-.4f);
    for(int i=0;i<14;++i)carried[i]=native[i];
    ApplyTrackedPosture(&ped,&hierarchy,carried,body,visual,-1.0f);
    Near(carried[0].pos,native[0].pos,"starting IK crouched does not invent a standing calibration");
    // The former reach clamp shortened backwards head displacement. A tracked
    // endpoint must instead move the pelvis and retain the exact neck target.
    for(int rate:{30,72,90,120})for(int frame=0;frame<180;++frame){
        Input in=standing;const float t=float(frame)/float(rate);
        in.delta={.23f*std::sin(t),-.28f*std::cos(t),-.12f};
        in.hasHeadTarget=true;in.headTarget=in.spine[3]+in.delta+CVector(.025f,0,.025f);
        Output result;Require(Solve(in,result),"tracked backwards/sideways target solved");
        Near(result.spine[3],in.headTarget,"head endpoint remains at camera anatomical target");
        for(int i=0;i<3;++i)Require(std::fabs((result.spine[i+1]-result.spine[i]).Magnitude()-(in.spine[i+1]-in.spine[i]).Magnitude())<.0002f,"head authority does not stretch spine");
    }
    Input stale=standing;stale.hasHeadTarget=true;stale.headTarget={100,0,1};
    Require(!Solve(stale,out),"stale/teleported head target cannot tear the skeleton");
    Stance contacts;
    CVector firstContact;
    const CVector foot={-.12f,0,.06f},ground={-.12f,0,0},normal={0,0,1};
    for(unsigned frame=1;frame<100;++frame){
        contacts.Begin(&ped,&body,frame,{},CVector(0,1,0),true);
        const CVector jitter=foot+CVector(.02f*std::sin(float(frame)),0,.01f*std::cos(float(frame)));
        CVector planted=contacts.Contact(0,jitter,ground,normal,true);
        if(frame==1)firstContact=planted;
        Near(planted,firstContact,"standing contact does not follow native idle sway");
    }
    CVector fr={1,0,0},fu={0,0,1},fa={0,1,0};contacts.Orient(0,fr,fu,fa);
    fr={0,1,0};fa={-1,0,0};contacts.Orient(0,fr,fu,fa);Near(fr,{1,0,0},"planted orientation does not yaw with the torso");
    contacts.Begin(&ped,&body,100,{},CVector(0,1,0),false);
    Near(contacts.Contact(0,foot,ground,normal,true),foot,"locomotion releases contact");
    contacts.Begin(&ped,&body,101,{},CVector(0,1,0),true);
    Near(contacts.Contact(0,foot,ground,normal,false),foot,"missing ground does not retain stale contact");
    contacts.Contact(0,foot,ground,normal,true);contacts.Begin(&ped,&body,104,{},CVector(0,1,0),true);
    Near(contacts.Candidate(0,foot+CVector(.1f,0,0)),foot+CVector(.1f,0,0),"ownership gap releases planted stance");
    contacts.Contact(0,foot,ground,normal,true);contacts.Begin(&ped,&body,105,{5,0,0},CVector(0,1,0),true);
    Near(contacts.Candidate(0,{5,0,.06f}),{5,0,.06f},"teleport releases planted stance");

    // Real production adapter with the camera's independent target and ground
    // probes. Rebuild the native pose each frame, as PreRender does in game.
    ped.bIsStanding=true;groundFound=headTargetAvailable=true;gTrackedStance.Reset();
    skeletonHead=native[3].pos+CVector(.025f,0,.025f);anchor={0,0,1.65f};
    CVector leftPlant,rightPlant;
    for(unsigned frame=0;frame<180;++frame){
        ++CTimer::frame;eye=anchor+CVector(.1f*std::sin(float(frame)*.02f),-.15f*std::cos(float(frame)*.02f),-.12f);
        for(int i=0;i<14;++i)carried[i]=native[i];
        // Native ankle sway must not be confused with physical foot movement.
        carried[6].pos.x+=.01f*std::sin(float(frame));carried[9].pos.x+=.01f*std::cos(float(frame));
        const CVector trackedEye=eye;
        ApplyTrackedPosture(&ped,&hierarchy,carried,body,visual,0);
        Near(carried[3].pos,skeletonHead+eye-anchor,"production head matches current camera target during backward lean");
        if(frame==0){leftPlant=carried[6].pos;rightPlant=carried[9].pos;}
        Near(carried[6].pos,leftPlant,"production left foot retains ground contact");
        Near(carried[9].pos,rightPlant,"production right foot retains ground contact");
        Near(eye,trackedEye,"foot planting does not move the camera");
    }
    ped.m_nMoveState=PEDMOVE_WALK;++CTimer::frame;
    for(int i=0;i<14;++i)carried[i]=native[i];
    carried[6].pos.y+=.08f;
    ApplyTrackedPosture(&ped,&hierarchy,carried,body,visual,0);
    Require(carried[6].pos.y>.07f,"native walking foot releases the standing contact");
    // Physical root travel produces one swing at a time, alternating sides;
    // it must not translate both planted feet with the visual body.
    for(int rate:{30,72,90,120}){
        Stance gait;int lastSwing=-1,started=-1,steps=0;CVector prior[2];
        for(unsigned frame=1;frame<=unsigned(rate*4);++frame){
            const CVector rootTravel(0,.35f*float(frame)/float(rate),0);
            CVector feet[2]={{-.12f,rootTravel.y,.06f},{.12f,rootTravel.y,.06f}};
            gait.Begin(&ped,&body,frame,rootTravel,{0,1,0},true,true);
            gait.Advance(feet,1.0f/float(rate));
            const CVector once=gait.Candidate(0,feet[0]);gait.Advance(feet,1.0f/float(rate));
            Near(gait.Candidate(0,feet[0]),once,"repeat update cannot advance the physical step twice");
            Require(!(gait.IsSwinging(0)&&gait.IsSwinging(1)),"only one foot can swing");
            int swing=gait.IsSwinging(0)?0:(gait.IsSwinging(1)?1:-1);
            if(swing>=0&&swing!=lastSwing){Require(started<0||swing!=started,"physical steps alternate feet");started=swing;++steps;}
            for(int side=0;side<2;++side){
                const CVector target=gait.Contact(side,feet[side],{feet[side].x,feet[side].y,0},{0,0,1},true);
                if(frame>1&&swing>=0&&side!=swing&&lastSwing==swing) Near(target,prior[side],"support foot stays planted during opposite swing");
                Require(target.z>=.059f&&target.z<.13f,"physical step has bounded ground clearance");prior[side]=target;
            }
            lastSwing=swing;
        }
        Require(steps>=4,"sustained physical travel generates repeated footsteps");
    }
    ped={};ped.bIsStanding=true;visual=CMatrix();gTrackedStance.Reset();
    groundFound=headTargetAvailable=true;int visibleSwings[2]={};
    for(unsigned frame=1;frame<=240;++frame){
        ++CTimer::frame;ped.position={0,float(frame)*.005f,0};visual.p=ped.position;
        for(int i=0;i<14;++i){carried[i]=native[i];carried[i].pos+=ped.position;}
        skeletonHead=native[3].pos+ped.position;anchor=skeletonHead;eye=anchor;
        ApplyTrackedPosture(&ped,&hierarchy,carried,body,visual,0);
        Near(carried[3].pos,skeletonHead,"physical root stepping preserves exact head attachment");
        for(int side=0;side<2;++side)
            if(gTrackedStance.IsSwinging(side)&&carried[6+side*3].pos.z>.045f) ++visibleSwings[side];
        Require(carried[6].pos.z<.08f||carried[9].pos.z<.08f,"production roomscale stepping retains a low support foot");
    }
    Require(visibleSwings[0]>5&&visibleSwings[1]>5,"both production leg chains express alternating physical step lift");
    // Eye-socket fit is relative to physical facing, not the initial game yaw.
    // The torso may reset at any angle without moving the eye or putting the
    // neck in front of it. Include straight-down and past-vertical head poses.
    fixedEyeFit=true;headTargetAvailable=true;ped={};
    anchor={.02f,.06f,1.65f};eye=anchor;skeletonHead={0,0,1.57f};
    for(bool resetEachPose:{false,true})for(float pitch:{0.0f,-1.4f,-1.5707963f,-1.7f})
    for(int angle=0;angle<=72;++angle){
        const float physicalYaw=float(angle)*.0872664626f;
        const CVector forward(-std::sin(physicalYaw),std::cos(physicalYaw),0),right(std::cos(physicalYaw),std::sin(physicalYaw),0);
        viewRight=right;viewForward=forward*std::cos(pitch)+CVector(0,0,std::sin(pitch));
        visual=CMatrix();visual.r=right;visual.f=forward;
        for(int i=0;i<14;++i){
            carried[i]=native[i];carried[i].pos=right*native[i].pos.x+forward*native[i].pos.y+CVector(0,0,native[i].pos.z);
            carried[i].right=right*native[i].right.x+forward*native[i].right.y+CVector(0,0,native[i].right.z);
            carried[i].up=right*native[i].up.x+forward*native[i].up.y+CVector(0,0,native[i].up.z);
            carried[i].at=right*native[i].at.x+forward*native[i].at.y+CVector(0,0,native[i].at.z);
        }
        if(resetEachPose)gTrackedStance.Reset();
        ++CTimer::frame;const CVector eyeBefore=eye;
        ApplyTrackedPosture(&ped,&hierarchy,carried,body,visual,0);
        const CVector socket=eye-carried[3].pos;
        Require(std::fabs(DotProduct(socket,forward)-.06f)<.0002f,"eye remains ahead of neck at every physical yaw");
        Require(std::fabs(DotProduct(socket,right)-.02f)<.0002f,"side eye fit turns with the body");
        Require(std::fabs(socket.z-.08f)<.0002f,"physical turns preserve vertical eye fit");
        Near(eye,eyeBefore,"turning fit correction never moves the camera");
    }
    // Standing anatomy anchors the entire pelvis/stance, not only the head.
    // Recreate a native crouch with a translated, forward-bent upper body and
    // folded knees: those animation offsets must not choose the VR body origin.
    gUpperBody.balance.valid=true;
    gUpperBody.balance.headFromWaist={0,.72f,0};gUpperBody.balance.feetFromWaist={0,-.83f,0};
    for(int i=0;i<4;++i)gUpperBody.balance.spineFromWaist[i]={0,standing.spine[i].z-standing.spine[0].z,0};
    for(bool duck:{false,true})for(int angle=0;angle<24;++angle){
        const float facing=float(angle)*.2617993878f;
        viewRight={std::cos(facing),std::sin(facing),0};viewForward={-std::sin(facing),std::cos(facing),0};
        visual=CMatrix();visual.r=viewRight;visual.f=viewForward;ped={};ped.bIsDucking=duck;ped.bIsStanding=true;
        gTrackedStance.Reset();groundFound=headTargetAvailable=fixedEyeFit=true;
        anchor={.02f,.06f,1.65f-(duck?.45f:0)};eye=anchor;skeletonHead={0,0,1.57f-(duck?.45f:0)};
        for(int i=0;i<14;++i){
            carried[i]=native[i];CVector local=native[i].pos;
            if(duck){
                if(i<4){CVector offset=local-standing.spine[0];local=CVector(.24f,.30f,.4f)+CVector(offset.x,offset.z*.5f,offset.z*.8660254f);}
                else if(i>=4&&i<=9){
                    local.x+=.24f;local.y+=.30f;
                    if(i==4||i==7)local.z=.37f;
                    if(i==5||i==8){local.y=.30f+std::sqrt(.1664f-.175f*.175f);local.z=.195f;}
                }else {local.y+=.30f;local.x+=.24f;local.z-=.45f;}
            }
            carried[i].pos=viewRight*local.x+viewForward*local.y+CVector(0,0,local.z);
            carried[i].right=viewRight*native[i].right.x+viewForward*native[i].right.y+CVector(0,0,native[i].right.z);
            carried[i].up=viewRight*native[i].up.x+viewForward*native[i].up.y+CVector(0,0,native[i].up.z);
            carried[i].at=viewRight*native[i].at.x+viewForward*native[i].at.y+CVector(0,0,native[i].at.z);
        }
        ++CTimer::frame;ApplyTrackedPosture(&ped,&hierarchy,carried,body,visual,0);
        const CVector target=eye-viewForward*.06f-viewRight*.02f-CVector(0,0,.08f);
        Near(carried[3].pos,target,"balanced stance retains exact head attachment");
        CVector pelvisError=carried[0].pos-target;pelvisError.z=0;
        Require(pelvisError.Magnitude()<.003f,"pelvis stays beneath head through turns and button crouch");
        const CVector hips=(carried[4].pos+carried[7].pos)*.5f;
        CVector hipError=hips-target;hipError.z=0;
        Require(hipError.Magnitude()<.003f,"hips do not inherit authored forward/right crouch translation");
        {
            CVector footError=(carried[6].pos+carried[9].pos)*.5f-target;footError.z=0;
            Require(footError.Magnitude()<.003f,"standing and crouched stance midpoint remains under the body");
        }
        Require(std::fabs((carried[1].pos-carried[0].pos).Magnitude()-.22f)<.001f,"spine dimensions use standing anatomy rather than pose displacement");
    }
    // A relocated pelvis must not reuse a knee plane from the original pose.
    // Include near-straight and backwards native knees, full body yaw, and
    // contacts ahead/behind the hip (the former hock/chicken-leg cases).
    for(int angle=0;angle<72;++angle)for(float nativeBend:{-.08f,.00001f,.08f})
    for(float contactOffset:{-.12f,0.0f,.12f}){
        const float facing=float(angle)*.0872664626f;
        const CVector fore(-std::sin(facing),std::cos(facing),0),right(std::cos(facing),std::sin(facing),0);
        Input in=standing;
        auto turn=[&](CVector v){return right*v.x+fore*v.y+CVector(0,0,v.z);};
        for(int i=0;i<4;++i)in.spine[i]=turn(in.spine[i]);
        for(int side=0;side<2;++side){
            in.hip[side]=turn(in.hip[side]);in.knee[side].y=nativeBend;in.knee[side]=turn(in.knee[side]);
            in.foot[side]=turn(in.foot[side]);in.footTarget[side]=in.foot[side]+fore*contactOffset;
        }
        in.forward=fore;in.delta={0,0,-.25f};in.hasHeadTarget=in.hasFootTargets=true;
        in.headTarget=in.spine[3]+in.delta;Output solved;
        Require(Solve(in,solved),"tracked knee plane solves through full physical turns");
        for(int side=0;side<2;++side){
            CVector line=Unit(solved.foot[side]-solved.hip[side],{0,0,-1});
            CVector bend=solved.knee[side]-solved.hip[side];bend-=line*DotProduct(bend,line);
            Require(DotProduct(bend,fore)>.001f,"tracked knees bend anatomically forward rather than backwards or sideways");
            Require(std::fabs((solved.knee[side]-solved.hip[side]).Magnitude()-(in.knee[side]-in.hip[side]).Magnitude())<.0002f,"knee direction correction preserves thigh length");
            Require(std::fabs((solved.foot[side]-solved.knee[side]).Magnitude()-(in.foot[side]-in.knee[side]).Magnitude())<.0002f,"knee direction correction preserves shin length");
            Near(solved.foot[side],in.footTarget[side],"knee plane correction retains reachable contacts");
        }
        Near(solved.spine[3],in.headTarget,"knee correction preserves head/pelvis solve");
    }
    // Continuous physical half-turn, retaining contacts between frames. At
    // each replant they must use the eye/pelvis frame rather than entity yaw.
    ped={};ped.bIsStanding=true;gTrackedStance.Reset();anchor={.02f,.06f,1.65f};eye=anchor;skeletonHead={0,0,1.57f};
    groundFound=headTargetAvailable=fixedEyeFit=true;
    for(int angle=0;angle<=30;++angle){
        const float facing=float(angle)*.1047197551f;
        viewRight={std::cos(facing),std::sin(facing),0};viewForward={-std::sin(facing),std::cos(facing),0};
        visual=CMatrix();visual.r=viewRight;visual.f=viewForward;
        for(int i=0;i<14;++i){
            carried[i]=native[i];carried[i].pos=viewRight*native[i].pos.x+viewForward*native[i].pos.y+CVector(0,0,native[i].pos.z);
            carried[i].right=viewRight*native[i].right.x+viewForward*native[i].right.y+CVector(0,0,native[i].right.z);
            carried[i].up=viewRight*native[i].up.x+viewForward*native[i].up.y+CVector(0,0,native[i].up.z);
            carried[i].at=viewRight*native[i].at.x+viewForward*native[i].at.y+CVector(0,0,native[i].at.z);
        }
        ++CTimer::frame;ApplyTrackedPosture(&ped,&hierarchy,carried,body,visual,0);
        const CVector target=eye-viewForward*.06f-viewRight*.02f-CVector(0,0,.08f);
        CVector centerError=(carried[6].pos+carried[9].pos)*.5f-target;centerError.z=0;
        Require(centerError.Magnitude()<.04f,"retained turn contacts remain close to the anatomical stance origin");
        Near(carried[3].pos,target,"stance recentering preserves tracked head attachment");
    }
    std::printf("PASS %u body posture checks\n",checks);
}
