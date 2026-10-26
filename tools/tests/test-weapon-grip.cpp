#define _CRT_SECURE_NO_WARNINGS
#include <cmath>
#include <initializer_list>
#define nil nullptr
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <cstring>
#include "../../overlay/src/skel/android/VrWeaponCalibrationMenu.h"
#include "ragdoll-test-vector.h"
#include "weapon-grip-types.inc"
#include "../../overlay/src/vr/QuestWeaponGrip.h"
#include "../../overlay/src/vr/QuestWeaponFactoryDefaults.h"
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#define DEGTORAD(a) ((a)*3.14159265358979323846f/180.0f)
namespace ModelSets {
enum eModelSet { MODEL_SET_CLASSIC, MODEL_SET_MODERN, MODEL_SET_XBOX };
enum { MODEL_CATEGORY_WEAPONS };
static eModelSet active=MODEL_SET_CLASSIC;
static eModelSet GetCalibrationSetForCategory(int){return active;}
}
struct CMatrix {
    CVector r{1,0,0},f{0,1,0},u{0,0,1},p{0,0,0};
    CVector &GetRight(){return r;} const CVector &GetRight() const {return r;}
    CVector &GetForward(){return f;} const CVector &GetForward() const {return f;}
    CVector &GetUp(){return u;} const CVector &GetUp() const {return u;}
    CVector &GetPosition(){return p;} const CVector &GetPosition() const {return p;}
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
template<class T,class U,class V> static T clamp(T value, U low, V high){return value<low?(T)low:value>high?(T)high:value;}
static std::map<std::string,std::string> ini;
static const char *kSettingsPath="test.ini";
static const char *GetVrWeaponName(int type){return type==WEAPONTYPE_AK47?"AK47":"COLT45";}
static const char *CalibrationKey(int hand,const char *name,char *key){
    if(hand==0){std::sprintf(key,"Left%s",name);return key;} return name;
}
static bool WritePrivateProfileStringA(const char *section,const char *key,const char *value,const char *){
    ini[std::string(section)+"/"+key]=value;return true;
}
static int ReadCalibrationValue(const char *section,int hand,const char *name,int fallback){
    char key[64];auto it=ini.find(std::string(section)+"/"+CalibrationKey(hand,name,key));
    return it==ini.end()?fallback:std::atoi(it->second.c_str());
}
static WeaponCalibration working[2][WEAPONTYPE_TOTALWEAPONS];
static WeaponCalibration *GetCalibration(int hand,int type){
    if(hand<0 || hand>1 || type<0 || type>=WEAPONTYPE_TOTALWEAPONS)return nullptr;
    WeaponCalibration &c=working[hand][type];
    if(!c.valid)GetDefaultCalibrationForSet(type,hand,ModelSets::active,&c);
    return &c;
}
#include "weapon-calibration-production.inc"
static int checks;
static void Check(bool ok, const char *message) {
    ++checks; if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}
int main() {
    for(const auto &profile : QuestWeaponGrip::profiles){
        for(int hand=0; hand<2; ++hand){
            WeaponCalibration a,b;
            GetFallbackCalibrationForSet(profile.type,hand,ModelSets::MODEL_SET_CLASSIC,&a);
            GetFallbackCalibrationForSet(profile.type,hand,ModelSets::MODEL_SET_XBOX,&b);
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
        GetFallbackCalibrationForSet(type,0,ModelSets::MODEL_SET_CLASSIC,&l);
        GetFallbackCalibrationForSet(type,1,ModelSets::MODEL_SET_CLASSIC,&r);
        GetFallbackCalibrationForSet(type,1,ModelSets::MODEL_SET_MODERN,&m);
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
    Check(std::fabs(DotProduct(barrel,top))<.0001f,
        "authored launcher support frame retains orthogonal axes");
    // Independent reference centres measured from the III weapon mesh.
    const CVector opening(-0.070460401f,0.114642583f,0.783905983f);
    const CVector rear(0.251384735f,-0.029805297f,-0.247366756f);
    for(auto set:{ModelSets::MODEL_SET_CLASSIC,ModelSets::MODEL_SET_XBOX}){
        ModelSets::active=set;
        for(int hand=0;hand<2;++hand){
            WeaponCalibration calibration;
            GetDefaultCalibrationForSet(WEAPONTYPE_ROCKETLAUNCHER,hand,set,&calibration);
            for(int pose=0;pose<6;++pose){
                // The model matrix includes the saved grip plus arbitrary rigid
                // rotations, as on the normal and two-hand contact-frame paths.
                tracked.SetRotate(.31f*pose,-.43f*pose,.71f*pose);
                tracked.p={3.4f,-1.2f,2.7f};
                CMatrix rendered;
                Check(BuildWeaponModelMatrixBase(hand,calibration,&rendered),"launcher model matrix builds");
                CVector source,direction;
                Check(BuildWeaponMuzzleRay(rendered,WEAPONTYPE_ROCKETLAUNCHER,&source,&direction),"launcher muzzle ray builds");
                Check((source-rendered.p-rendered.Vector(opening)).Magnitude()<.000002f,
                    "launcher laser and launch ray start at opening centre");
                CVector expected=rendered.Vector(opening-rear);expected.Normalise();
                Check(DotProduct(direction,expected)>.999999f,
                    "launcher direction follows measured tube centreline");
                const CVector back=rendered.p+rendered.Vector(rear);
                Check(CrossProduct(source-back,direction).Magnitude()<.000002f,
                    "muzzle ray lies on tube axis for both hands and all rigid poses");
            }
        }
    }
    ModelSets::active=ModelSets::MODEL_SET_MODERN;
    Check(GetWeaponLocalBarrelDirection(WEAPONTYPE_ROCKETLAUNCHER).x==1.f &&
        GetWeaponLocalMuzzle(WEAPONTYPE_ROCKETLAUNCHER).x==.553f,"Modern launcher aim unchanged");
    WeaponCalibration modern;
    GetDefaultCalibrationForSet(WEAPONTYPE_COLT45,1,ModelSets::MODEL_SET_MODERN,&modern);
    Check(modern.rotationX==-4 && modern.offsetZ==-7,"Modern primary calibration unchanged");
    Check(QuestWeaponGrip::Find(WEAPONTYPE_UNARMED)==nullptr,"unarmed is not repositioned");
    Check(ARRAY_SIZE(QuestWeaponFactoryDefaults::profiles)==22,"all saved headset hand profiles are baked in");
    for(const auto &profile:QuestWeaponFactoryDefaults::profiles){
        for(auto set:{ModelSets::MODEL_SET_CLASSIC,ModelSets::MODEL_SET_XBOX}){
            ModelSets::active=set;
            WeaponCalibration builtin;
            GetDefaultCalibrationForSet(profile.weaponType,profile.hand,set,&builtin);
            working[profile.hand][profile.weaponType].aimOffsetX=99;
            RestoreQuestCalibrationDefault(profile.hand,profile.weaponType,true);
            const int mainIndex[]={3,4,5,6,7,8,0,1,2,9,10,11};
            for(int item=0;item<19;++item){
                const int expected=item<12?profile.main[mainIndex[item]]:profile.support[item-12];
                int minimum,maximum;
                Check(*QuestCalibrationValue(&builtin,item,&minimum,&maximum)==expected,
                    "fresh factory profile matches headset checkpoint field exactly");
                Check(GetQuestCalibrationValue(profile.hand,profile.weaponType,item)==expected,
                    "Restore Factory Values loads authored hand profile exactly");
            }
            Check(builtin.supportPoseVersion==profile.supportPoseVersion &&
                GetCalibration(profile.hand,profile.weaponType)->supportPoseVersion==profile.supportPoseVersion,
                "fresh and restored factory profiles retain saved support frame version");
        }
    }
    for(int type:{WEAPONTYPE_GRENADE,WEAPONTYPE_DETONATOR}){
        Check(QuestWeaponFactoryDefaults::Find(type,1)==nullptr,"unsaved right-hand profiles stay unmodified");
        WeaponCalibration original,factory;
        GetFallbackCalibrationForSet(type,1,ModelSets::MODEL_SET_CLASSIC,&original);
        GetDefaultCalibrationForSet(type,1,ModelSets::MODEL_SET_CLASSIC,&factory);
        for(int item=0;item<19;++item){
            int low,high;
            Check(*QuestCalibrationValue(&original,item,&low,&high)==*QuestCalibrationValue(&factory,item,&low,&high),
                "right-hand grenade/detonator preserve existing factory fields");
        }
    }
    using namespace VrWeaponCalibrationMenu;
    ModelSets::active=ModelSets::MODEL_SET_CLASSIC;
    // All operations below call the extracted production API, backed by an INI store.
    for(int item=0;item<19;++item){
        int minimum,maximum;int *v=QuestCalibrationValue(GetCalibration(0,WEAPONTYPE_AK47),item,&minimum,&maximum);
        *v=maximum;
        AdjustQuestCalibrationValue(0,WEAPONTYPE_AK47,item,StepSize(false));
        Check(*v==(IsRotation(item)?-359:maximum),"fine step wraps angles and clamps distances/styles");
        *v=minimum;
        AdjustQuestCalibrationValue(0,WEAPONTYPE_AK47,item,-StepSize(false));
        Check(*v==(IsRotation(item)?359:minimum),"negative fine step crosses rotation seam only");
        *v=maximum;
        AdjustQuestCalibrationValue(0,WEAPONTYPE_AK47,item,StepSize(true));
        Check(*v==(IsRotation(item)?-340:maximum),"turbo preserves ten-degree seam overshoot");
        *v=0;
        AdjustQuestCalibrationValue(0,WEAPONTYPE_AK47,item,StepSize(true));
        Check(*v==(item==18?1:20),"turbo moves ten physical units, not one half-unit");
    }
    const int type=WEAPONTYPE_AK47;
    WeaponCalibration &left=*GetCalibration(0,type), &right=*GetCalibration(1,type);
    left.aimOffsetX=37;left.rotationZ=-57;left.supportGripType=0;left.supportPoseVersion=5;
    right.aimOffsetX=-13;right.supportGripType=1;
    SaveQuestCalibrationDefault(0,type);SaveQuestCalibrationDefault(1,type);
    left.aimOffsetX=99;right.aimOffsetX=88;
    RestoreQuestCalibrationDefault(0,type,false);
    Check(left.aimOffsetX==37 && left.rotationZ==-57 && left.supportPoseVersion==5,"saved default restores all fields and frame version");
    Check(right.aimOffsetX==88,"restore affects only selected hand");
    RestoreQuestCalibrationDefault(1,type,false);
    Check(right.aimOffsetX==-13 && right.supportGripType==1,"opposite hand has its own checkpoint and grip style");
    CopyQuestCalibrationToOtherHand(0,type);
    Check(right.aimOffsetX==37 && right.rotationZ==-57 && right.supportGripType==0,"copy transfers exact values to opposite hand");
    Check(ReadCalibrationValue("VRWeapon_05_AK47",0,"SupportGripStyle",-1)==0 &&
          ReadCalibrationValue("VRWeapon_05_AK47",1,"SupportGripStyle",-1)==0,"working grip style saved separately for both hands");
    RestoreQuestCalibrationDefault(1,type,false);
    Check(right.aimOffsetX==-13 && right.supportGripType==1,"copy leaves destination default checkpoint intact");
    working[0][type]=WeaponCalibration();
    RestoreQuestCalibrationDefault(0,type,false);
    Check(left.aimOffsetX==37,"saved checkpoint survives runtime cache reload");
    RestoreQuestCalibrationDefault(0,type,true);
    Check(left.aimOffsetX!=37 && left.supportPoseVersion==6,"factory restore uses built-in profile");
    RestoreQuestCalibrationDefault(0,type,false);
    Check(left.aimOffsetX==37,"factory restore preserves personal checkpoint");
    ModelSets::active=ModelSets::MODEL_SET_MODERN;
    left.aimOffsetX=21;SaveQuestCalibrationDefault(0,type);
    ModelSets::active=ModelSets::MODEL_SET_CLASSIC;RestoreQuestCalibrationDefault(0,type,false);
    Check(left.aimOffsetX==37,"Modern and Classic checkpoints are isolated");
    ModelSets::active=ModelSets::MODEL_SET_MODERN;RestoreQuestCalibrationDefault(0,type,false);
    Check(left.aimOffsetX==21,"Modern checkpoint restores its own values");
    RestoreQuestCalibrationDefault(0,WEAPONTYPE_COLT45,false);
    WeaponCalibration factory;GetDefaultCalibrationForSet(WEAPONTYPE_COLT45,0,ModelSets::active,&factory);
    Check(GetQuestCalibrationValue(0,WEAPONTYPE_COLT45,0)==factory.aimOffsetX,"missing personal checkpoint falls back to factory");
    std::printf("Weapon grip regression: %d checks passed.\n",checks);
}
