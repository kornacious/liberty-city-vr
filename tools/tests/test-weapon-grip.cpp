#include <cmath>
#include <initializer_list>
#define nil nullptr
#include <cstdio>
#include <cstdlib>
#include "ragdoll-test-vector.h"
#include "weapon-grip-types.inc"
#include "../../overlay/src/vr/QuestWeaponGrip.h"
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#define DEGTORAD(a) ((a)*3.14159265358979323846f/180.0f)
namespace ModelSets {
enum eModelSet { MODEL_SET_CLASSIC, MODEL_SET_MODERN, MODEL_SET_XBOX };
enum { MODEL_CATEGORY_WEAPONS };
static eModelSet active=MODEL_SET_CLASSIC;
static eModelSet GetActiveForCategory(int){return active;}
}
struct CMatrix {
    CVector r{1,0,0},f{0,1,0},u{0,0,1},p{0,0,0};
    CVector &GetRight(){return r;} const CVector &GetRight() const {return r;}
    CVector &GetForward(){return f;} const CVector &GetForward() const {return f;}
    CVector &GetUp(){return u;} const CVector &GetUp() const {return u;}
    CVector &GetPosition(){return p;}
    void SetUnity(){*this=CMatrix();}
    void SetRotateX(float a){SetUnity();const float c=std::cos(a),s=std::sin(a);f={0,c,s};u={0,-s,c};}
    void SetRotateY(float a){SetUnity();const float c=std::cos(a),s=std::sin(a);r={c,0,-s};u={s,0,c};}
    void SetRotateZ(float a){SetUnity();const float c=std::cos(a),s=std::sin(a);r={c,s,0};f={-s,c,0};}
    CVector Vector(const CVector &v) const {return r*v.x+f*v.y+u*v.z;}
    CMatrix operator*(const CMatrix &b) const {
        CMatrix out;out.r=Vector(b.r);out.f=Vector(b.f);out.u=Vector(b.u);out.p=p+Vector(b.p);return out;
    }
    void SetRotate(float x,float y,float z){
        CMatrix rx,ry,rz;rx.SetRotateX(x);ry.SetRotateY(y);rz.SetRotateZ(z);*this=rz*rx*ry;
    }
};
static CMatrix tracked;
static bool BuildTrackedWeaponHandBasis(int, CVector *position, CVector *right, CVector *up, CVector *forward){
    *position=tracked.p;*right=tracked.r;*up=tracked.u;*forward=tracked.f;return true;
}
#include "weapon-grip-production.inc"
static int checks;
static void Check(bool ok, const char *message) {
    ++checks; if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}
int main() {
    for(const auto &profile : QuestWeaponGrip::profiles){
        for(int hand=0; hand<2; ++hand){
            WeaponCalibration a,b;
            GetDefaultCalibrationForSet(profile.type,hand,ModelSets::MODEL_SET_CLASSIC,&a);
            GetDefaultCalibrationForSet(profile.type,hand,ModelSets::MODEL_SET_XBOX,&b);
            Check(a.valid && b.valid,"profile loaded");
            Check(a.offsetX==b.offsetX && a.offsetY==b.offsetY && a.offsetZ==b.offsetZ &&
                a.rotationX==b.rotationX && a.rotationY==b.rotationY,"Xbox shares the generic III weapon geometry");
            // Exercise the production model builder, including base and calibration
            // rotations, against independent hand-contact/orientation expectations.
            for(int pose=0;pose<3;++pose){
                tracked.SetRotate(.27f*pose,-.19f*pose,.63f*pose);
                tracked.p={2.f,-3.f,1.f};
                CMatrix rendered;
                Check(BuildWeaponModelMatrixBase(hand,a,&rendered),"production weapon matrix builds");
                const CVector contact=rendered.p+rendered.Vector({profile.x,profile.y,profile.z});
                const CVector palm=tracked.p+tracked.f*.055f-tracked.u*.045f;
                Check((contact-palm).Magnitude()<.0044f,"grip remains in palm after roll correction");
                const float pitch=DEGTORAD(profile.pitch);
                // After launcher pitch, (-sin(pitch),0,cos(pitch)) is mesh top.
                const CVector top=rendered.Vector({-std::sin(pitch),0,std::cos(pitch)});
                Check(DotProduct(top,tracked.r)>.9999f,"mesh top follows calibrated hand across poses");
                const CVector forward=rendered.Vector({std::cos(pitch),0,std::sin(pitch)});
                Check(DotProduct(forward,tracked.f)>.9999f,"roll leaves barrel along aim axis");
            }
        }
    }
    for(int type : {WEAPONTYPE_AK47,WEAPONTYPE_M16,WEAPONTYPE_SHOTGUN,WEAPONTYPE_SNIPERRIFLE,WEAPONTYPE_ROCKETLAUNCHER,WEAPONTYPE_FLAMETHROWER}){
        WeaponCalibration l,r,m;
        GetDefaultCalibrationForSet(type,0,ModelSets::MODEL_SET_CLASSIC,&l);
        GetDefaultCalibrationForSet(type,1,ModelSets::MODEL_SET_CLASSIC,&r);
        GetDefaultCalibrationForSet(type,1,ModelSets::MODEL_SET_MODERN,&m);
        Check(l.supportPoseVersion==6 && r.supportPoseVersion==6,"III sockets use native Z-up version");
        Check(m.supportPoseVersion==5,"Modern retains Y-up v5 sockets");
        Check(l.supportX==-r.supportX && l.supportY==r.supportY && l.supportZ==r.supportZ,
            "left-primary sockets reflect lateral coordinate only");
        Check(l.supportRotationX==-r.supportRotationX && l.supportRotationZ==-r.supportRotationZ,
            "left-primary wrist orientation mirrors");
        if(type==WEAPONTYPE_AK47) Check(r.supportY>=60 && r.supportY<=76,"AK wrist stays on wood foregrip, behind front sight");
    }
    CMatrix model;
    WeaponCalibration frame;
    CVector barrel,top;
    frame.supportPoseVersion=6;
    GetSupportModelAxes(WEAPONTYPE_AK47,frame,model,&barrel,&top);
    Check((top-CVector(0,0,1)).Magnitude()<.0001f,"III support frame uses mesh Z");
    frame.supportPoseVersion=5;
    GetSupportModelAxes(WEAPONTYPE_AK47,frame,model,&barrel,&top);
    Check((top-CVector(0,1,0)).Magnitude()<.0001f,"saved v5 support frame retains mesh Y");
    frame.supportPoseVersion=6;
    GetSupportModelAxes(WEAPONTYPE_ROCKETLAUNCHER,frame,model,&barrel,&top);
    const CVector rocket=GetWeaponLocalBarrelDirection(WEAPONTYPE_ROCKETLAUNCHER);
    Check((barrel-rocket).Magnitude()<.0001f && std::fabs(DotProduct(barrel,top))<.0001f,
        "launcher support frame follows actual tube and orthogonal top");
    const float pitch=DEGTORAD(107.5f);
    Check(std::fabs(-std::sin(pitch)*rocket.x+std::cos(pitch)*rocket.z)<.0001f,
        "launcher ray follows controller forward after model alignment");
    Check(GetWeaponLocalMuzzle(WEAPONTYPE_ROCKETLAUNCHER).z>.7f,"launcher ray starts at tube mouth");
    ModelSets::active=ModelSets::MODEL_SET_MODERN;
    Check(GetWeaponLocalBarrelDirection(WEAPONTYPE_ROCKETLAUNCHER).x==1.f &&
        GetWeaponLocalMuzzle(WEAPONTYPE_ROCKETLAUNCHER).x==.553f,"Modern launcher aim unchanged");
    WeaponCalibration modern;
    GetDefaultCalibrationForSet(WEAPONTYPE_COLT45,1,ModelSets::MODEL_SET_MODERN,&modern);
    Check(modern.rotationX==-4 && modern.offsetZ==-7,"Modern primary calibration unchanged");
    Check(QuestWeaponGrip::Find(WEAPONTYPE_UNARMED)==nullptr,"unarmed is not repositioned");
    std::printf("Weapon grip regression: %d checks passed.\n",checks);
}
