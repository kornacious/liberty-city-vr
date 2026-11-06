#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include "ragdoll-test-vector.h"
using std::isfinite;
#define nil nullptr
#define DEGTORAD(x) ((x)*0.01745329252f)
#include "QuestBodyPosture.h"
struct CMatrix {
 CVector r{1,0,0},u{0,0,1},f{0,1,0},p;
 CVector &GetRight(){return r;}const CVector &GetRight()const{return r;}
 CVector &GetUp(){return u;}const CVector &GetUp()const{return u;}
 CVector &GetForward(){return f;}const CVector &GetForward()const{return f;}
 CVector &GetPosition(){return p;}const CVector &GetPosition()const{return p;}
};
static CMatrix Yaw(float angle){CMatrix m;m.r={std::cos(angle),std::sin(angle),0};m.f={-std::sin(angle),std::cos(angle),0};return m;}
struct RwMatrix {CVector pos,right,up,at;int flags=0;};
enum {BONE_waist,BONE_upperlegr,BONE_lowerlegr,BONE_footr,BONE_upperlegl,BONE_lowerlegl,BONE_footl,BONE_mid,BONE_torso,BONE_head};
struct RpHAnimHierarchy {int numNodes=16;};
struct CPed {CMatrix root;bool modern=true,bIsDucking=false;int m_nPedState=0;CMatrix &GetMatrix(){return root;}CVector GetPosition(){return root.p;}};
enum {PED_JUMP=1};
static bool UsesNativeCameraHead(CPed *ped){return ped->modern;}
static int RpHAnimIDGetIndex(RpHAnimHierarchy*,int id){return id;}
static CMatrix view;
static CVector testEye,testAnchor;
static bool lookingDown=false;
static float correction=0;
namespace androidgame {
static float VrXboxBodyYawDegrees(){return correction;}
static bool VrGetViewBasis(float *p,float *r,float *u,float *f){
 CVector forward=lookingDown?CVector(0,0,-1):view.f;
 for(int i=0;i<3;++i){if(p)p[i]=(&testEye.x)[i];if(r)r[i]=(&view.r.x)[i];if(u)u[i]=(&view.u.x)[i];if(f)f[i]=(&forward.x)[i];}return true;
}
static bool VrGetOnFootBodyAnchor(float *p){p[0]=testAnchor.x;p[1]=testAnchor.y;p[2]=testAnchor.z;return true;}
}
#include "body-reference-types.inc"
static UpperBodyAnchor gUpperBody;
static ShoulderAnchor gShoulder[2];
#include "body-reference-production.inc"
static unsigned checks;
static void Require(bool ok,const char *text){++checks;if(!ok){std::fprintf(stderr,"FAIL %s\n",text);std::exit(1);}}
static void Near(CVector a,CVector b,const char *text){Require((a-b).Magnitude()<.0002f,text);}
static CVector World(const CMatrix &basis,CVector model){return basis.p+basis.r*model.x+basis.f*model.y+basis.u*model.z;}
static void Native(CPed &ped,RwMatrix *mats){
 for(int i=0;i<16;++i){mats[i].pos=World(ped.root,{0,0,.8f});mats[i].right=ped.root.u;mats[i].up=ped.root.f;mats[i].at=-ped.root.r;}
 mats[7].pos=World(ped.root,{.01f,.02f,1.05f});mats[8].pos=World(ped.root,{.015f,.03f,1.3f});mats[9].pos=World(ped.root,{.015f,.04f,1.58f});
 mats[13].pos=World(ped.root,{-.2f,.03f,1.3f});
}
int main(){
 CPed ped;RpHAnimHierarchy hier;RwMatrix mats[16];UpperBodyBones body{0,7,8,9};ArmBones arm{13,14,15};
 for(float nativeYaw:{0.0f,.7f,2.2f})for(float firstView:{0.0f,1.5707963f,3.14159265f,4.712389f})
 for(bool down:{false,true})for(float follow:{0.0f,.6f}){
  ped.root=Yaw(nativeYaw);ped.root.p={50,-20,2};gUpperBody={};gShoulder[0]={};view=Yaw(firstView);lookingDown=down;
  testAnchor=World(ped.root,{0,0,1.6f});testEye=testAnchor+CVector(follow,0,0);
  // First enable at arbitrary physical facing, then rotate and reset in place.
  for(int step=0;step<12;++step){
   const float facing=firstView+float(step)*.5235987756f;view=Yaw(facing);
   if(step==6){gUpperBody={};gShoulder[0]={};}
   Native(ped,mats);CMatrix visual;
   Require(BuildGroundedVisualBody(&ped,&hier,mats,visual),"grounded basis available");
   Near(visual.f,view.f,"steep downwards gaze retains physical body heading");
   float vertical=0;Require(StabilizeUpperBody(&ped,mats,body,visual,&vertical),"upper body stabilized");
   Near(mats[8].up,visual.f,"torso faces physical view independently of capture/reset heading");
   Near(mats[7].up,visual.f,"mid spine faces physical view independently of capture/reset heading");
   Near(mats[8].pos,World(visual,{.015f,.03f,1.3f})-visual.u*.025f,"cached torso position remains anatomical");
   StabilizeShoulderRoot(&ped,mats,arm,0,visual,vertical);
   Near(mats[13].pos,World(visual,{-.2f,.03f,1.3f}),"shoulder cache excludes roomscale follow at initialization");
  }
 }
 // Native axis capture must preserve the pre-existing path for Classic.
 ped.modern=false;ped.root=Yaw(.4f);view=Yaw(3.14f);lookingDown=false;gUpperBody={};Native(ped,mats);
 const CVector nativeFacing=mats[8].up;CMatrix visual;testAnchor=testEye=ped.root.p;
 BuildGroundedVisualBody(&ped,&hier,mats,visual);float vertical;
 StabilizeUpperBody(&ped,mats,body,visual,&vertical);Near(mats[8].up,nativeFacing,"Classic capture convention unchanged");
 std::printf("PASS %u production body reference/capture checks\n",checks);
}
