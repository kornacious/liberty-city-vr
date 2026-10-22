#include <cstdio>
#include <cstdlib>
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
static bool GetTrackedVisualHandMatrix(int, CMatrix *out, float *, float *){*out=anchor;return true;}
static bool GetTrackedVisualHandAimRay(int, CVector *p, CVector *f){*p=anchor.p;*f=anchor.f;return true;}
bool IsImmersiveCarDrivingActive(){return car;}
static bool IsImmersiveSteeringHandleGrabbed(int){return grabbed;}
}
namespace VrBodyIK {
static bool WeaponOwnershipActive(){return ik;}
static bool GetRigidWeaponHandBasis(int, CVector *p, CVector *r, CVector *u, CVector *f){
    *p=anchor.p;*r=anchor.u;*u=anchor.r;*f=anchor.f;return true;
}
}
#include "wheel-hand-basis-production.inc"
static int checks;
static void Require(bool ok, const char *message){
    ++checks;if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}
static void Near(CVector a,CVector b,const char *message){Require((a-b).Magnitude()<0.0001f,message);}
int main(){
    // Tilted wheel with multiple steering rotations, including past 180 degrees.
    CVector normal(0,0.8f,-0.6f), right(1,0,0);
    const CVector up=CrossProduct(right,normal);
    for(int mode=0;mode<2;++mode){
        ik=mode!=0;
        for(int degrees=-270;degrees<=270;degrees+=15){
            const float angle=degrees*3.14159265f/180.0f;
            anchor.r=right*std::cos(angle)+up*std::sin(angle);
            anchor.f=normal;
            anchor.u=CrossProduct(anchor.r,normal);
            anchor.p=CVector(1,2,3)-anchor.r*0.2f;
            car=true;grabbed=true;
            RenderVrTrackedHand(0);
            Near(rendered.p,anchor.p,"left wrist stays on its calibrated anchor");
            Near(rendered.f,-normal,"left fingers face the corrected wheel hemisphere");
            Near(rendered.u,anchor.r,"left palm roll keeps the thumb upright");
            Near(rendered.r,-anchor.u,"left lateral axis follows the wheel without reflection");
            Require(DotProduct(CrossProduct(rendered.u,rendered.f),rendered.r)>0.999f,
                "left mesh basis must not be mirrored");
            const Basis left=rendered;
            RenderVrTrackedHand(1);
            Near(rendered.f,normal,"right finger direction is unchanged");
            Near(rendered.u,anchor.r,"right palm direction is unchanged");
            Near(rendered.r,anchor.u,"right lateral direction is unchanged");
            Near(left.u,rendered.u,"both palms share the same radial roll convention");
            Near(left.r,-rendered.r,"opposing finger directions retain an unreflected lateral pair");
            Near(anchor.f,normal,"visual correction never changes the steering anchor");
            car=false;
            RenderVrTrackedHand(0);
            Near(rendered.f,normal,"bike finger direction is unchanged");
            Near(rendered.r,anchor.u*(ik?-1.0f:1.0f),"bike lateral direction is unchanged");
            car=true;grabbed=false;
            RenderVrTrackedHand(0);
            Near(rendered.f,normal,"released hand resumes its tracked direction");
        }
    }
    std::printf("Wheel hand basis: %d checks passed (floating and IK).\n",checks);
}
