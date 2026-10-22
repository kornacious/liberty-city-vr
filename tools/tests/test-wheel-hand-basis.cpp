#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include "ragdoll-test-vector.h"
struct CMatrix {
    CVector p, r{1,0,0}, u{0,0,1}, f{0,1,0};
    CVector &GetPosition(){return p;}
    CVector &GetRight(){return r;}
    CVector &GetUp(){return u;}
    CVector &GetForward(){return f;}
};
struct Basis {CVector p,r,u,f;};
static Basis rendered;
static CMatrix anchor;
static bool car, grabbed, ik;
namespace OculusVR {
enum {VR_HAND_COUNT=2};
static CMatrix gGripMatrix[2],gAimMatrix[2];
static bool gPoseValid[2]={true,true};
static bool IsImmersiveCarDrivingActive(){return car;}
static bool IsImmersiveSteeringHandleGrabbed(int){return grabbed;}
#include "wheel-hand-raw-production.inc"
static bool GetTrackedVisualHandMatrix(int hand, CMatrix *out, float *, float *){
    *out=grabbed?anchor:gGripMatrix[hand];return true;
}
static bool GetTrackedVisualHandAimRay(int hand, CVector *p, CVector *f){
    *p=anchor.p;*f=grabbed?anchor.f:gAimMatrix[hand].f;return true;
}
}
namespace VrBodyIK {
static bool WeaponOwnershipActive(){return ik;}
static bool GetRigidWeaponHandBasis(int, CVector *p, CVector *r, CVector *u, CVector *f){
    // Deliberately unlike the controller pose: conversion must run after IK
    // without moving the body's wheel-locked wrist.
    *p=anchor.p;*r=anchor.u;*u=anchor.r;*f=anchor.f;return true;
}
}
#include "wheel-hand-basis-production.inc"
static int checks;
static void Require(bool ok,const char *message){
    ++checks;if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}
static void Near(CVector a,CVector b,const char *message){Require((a-b).Magnitude()<0.0001f,message);}
static void SameOrientation(Basis a,Basis b){
    Near(a.r,b.r,"grab preserves free-hand thumb/lateral direction");
    Near(a.u,b.u,"grab preserves free-hand palm direction");
    Near(a.f,b.f,"grab preserves free-hand finger direction");
}
static CVector MeshVector(CVector local,Basis b){return b.f*local.x-b.u*local.y+b.r*local.z;}
static CVector Rotate(CVector v,CVector axis,float angle){
    return v*std::cos(angle)+CrossProduct(axis,v)*std::sin(angle)+axis*DotProduct(axis,v)*(1-std::cos(angle));
}
int main(){
    using namespace OculusVR;
    const CVector normal(0,0.8f,-0.6f),wheelRight(1,0,0);
    const CVector wheelUp=CrossProduct(wheelRight,normal);
    // Vary wheel angle independently from controller wrist orientation. The
    // reference is the actual free-hand render path, not a guessed wheel basis
    // or determinant: the converted meshes share local geometry.
    for(int roll=-180;roll<=180;roll+=30){
        for(int pitch=-60;pitch<=60;pitch+=30){
            const float rad=3.14159265f/180.0f;
            CVector finger=Rotate(CVector(0,1,0),CVector(1,0,0),pitch*rad);
            CVector gripRight=Rotate(CVector(-1,0,0),finger,roll*rad);
            gGripMatrix[0].r=gripRight;gGripMatrix[0].f=finger;
            gAimMatrix[0].f=finger;
            car=true;grabbed=false;ik=false;
            RenderVrTrackedHand(0);
            const Basis free=rendered;
            for(int angle=-270;angle<=270;angle+=30){
                anchor.r=wheelRight*std::cos(angle*rad)+wheelUp*std::sin(angle*rad);
                anchor.f=normal;anchor.u=CrossProduct(anchor.r,normal);
                anchor.p=CVector(1,2,3)-anchor.r*0.2f;
                for(int mode=0;mode<2;++mode){
                    grabbed=true;ik=mode!=0;
                    RenderVrTrackedHand(0);
                    SameOrientation(rendered,free);
                    Near(rendered.p,anchor.p,"conversion must leave the wheel/IK wrist fixed");
                    // Same local-to-world mapping as VRHandModel, including
                    // asymmetric palm/thumb points and finger length.
                    for(CVector local : {CVector(.13f,.04f,-.02f),CVector(.06f,.02f,.06f),CVector(.08f,-.02f,-.04f)})
                        Near(MeshVector(local,rendered),MeshVector(local,free),"mesh landmarks retain their tracked orientation");
                    car=false;
                    RenderVrTrackedHand(0);
                    Near(rendered.f,normal,"bike grip direction must be unchanged");
                    car=true;
                    RenderVrTrackedHand(1);
                    Near(rendered.f,normal,"right grip direction must be unchanged");
                }
            }
        }
    }
    CVector r(1,2,3),u(4,5,6),f(7,8,9);
    car=true;grabbed=true;
    Require(!GetCarWheelHandVisualBasis(1,&r,&u,&f),"right hand is outside correction scope");
    gPoseValid[0]=false;
    Require(!GetCarWheelHandVisualBasis(0,&r,&u,&f),"invalid tracking rejects conversion");
    Near(r,CVector(1,2,3),"failed conversion preserves existing basis");
    gPoseValid[0]=true;gAimMatrix[0].f=CVector();
    Require(!GetCarWheelHandVisualBasis(0,&r,&u,&f),"degenerate aim rejects conversion");
    Near(f,CVector(7,8,9),"degenerate conversion preserves existing basis");
    Require(!GetCarWheelHandVisualBasis(0,nullptr,&u,&f),"null output rejects conversion");
    std::printf("Wheel hand continuity: %d checks passed (floating and IK).\n",checks);
}
