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
    CVector r{1,0,0},f{0,1,0},u{0,0,1};
    const CVector &GetRight() const {return r;}
    const CVector &GetForward() const {return f;}
    const CVector &GetUp() const {return u;}
};
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
            Check(a.rotationX==-180 && a.rotationZ==0,"III native Z-up conversion");
            // Independently transform the measured mesh grip through the resulting
            // native-to-controller basis; translation quantizes to half cm.
            const float angle=DEGTORAD(a.rotationY/2.0f);
            const float gx=-profile.y+a.offsetX/200.0f;
            const float gy=std::cos(angle)*profile.x+std::sin(angle)*profile.z+a.offsetY/200.0f;
            const float gz=-std::sin(angle)*profile.x+std::cos(angle)*profile.z+a.offsetZ/200.0f;
            Check(std::fabs(gx)<0.0026f && std::fabs(gy-.055f)<.0026f && std::fabs(gz+.045f)<.0026f,
                "grip centre reaches palm within half-centimetre quantization");
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
