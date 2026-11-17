#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#define nil nullptr
using int16 = short;
enum { MI_PREDATOR, MI_RHINO, MI_FIRETRUCK, MI_NORMAL, VR_DRIVING_DEFAULT };
struct CControllerState {
    int16 Circle=0, LeftShoulder1=0, RightShoulder1=0;
    int16 LeftShoulder2=0, RightShoulder2=0;
};
struct CVehicle {
    void *pDriver=nullptr;
    int model=MI_NORMAL;
    int GetModelIndex(){return model;}
    bool IsRealHeli(){return false;}
    bool IsRealPlane(){return false;}
    bool IsCar(){return model!=MI_PREDATOR;}
    bool IsBike(){return false;}
} gVehicle;
static int player;
static CVehicle *FindPlayerVehicle(){return &gVehicle;}
static void *FindPlayerPed(){return &player;}
struct CPad {
    int mode=0;
    int GetMode(){return mode;}
    static CPad *GetPad(int){static CPad pad;return &pad;}
};
namespace androidgame {
struct PadInput {bool b=false;float leftGrip=0,rightGrip=0;};
}
static CVehicle *gButtonDriveByVehicle;
static int gButtonDriveByDirection;
static bool gVehicleForwardFireEnabled, physical, thirdPerson, weaponHeld;
static bool IsVrDrivingActive(){return physical;}
static bool IsTrackedWeaponHeld(int){return weaponHeld;}
static bool IsQuestVehicleThirdPerson(){return thirdPerson;}
static int GetDrivingTypeForVehicle(CVehicle*){return physical ? -1 : VR_DRIVING_DEFAULT;}
#include "vehicle-button-input-production.inc"
static void Check(bool value,const char *why){if(!value){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}}
int main()
{
    gVehicle.pDriver=&player;
    unsigned cases=0;
    for(int model:{MI_PREDATOR,MI_RHINO,MI_FIRETRUCK})
    for(int mode:{0,1,2,3})
    for(bool third:{false,true})
    for(bool immersive:{false,true})
    for(bool forward:{false,true})
    for(int grips:{0,1,2,3}){
        gVehicle.model=model;CPad::GetPad(0)->mode=mode;
        physical=immersive;thirdPerson=third;gVehicleForwardFireEnabled=forward;
        androidgame::PadInput in;in.b=true;
        in.leftGrip=(grips&1)?1.0f:0.0f;in.rightGrip=(grips&2)?1.0f:0.0f;
        CControllerState state;
        state.LeftShoulder2=17;state.RightShoulder2=23;
        MapButtonDriveBy(&state,in,false);
        Check((mode==3?state.RightShoulder1:state.Circle)==255,"B fires native vehicle weapon without a look modifier");
        Check(gButtonDriveByVehicle==nullptr&&gButtonDriveByDirection==-1,"native fire does not request a Uzi drive-by");
        Check(state.LeftShoulder2==17&&state.RightShoulder2==23,"native fire preserves steering/look input");
        state={};MapButtonDriveBy(&state,in,true);
        Check(state.Circle==0&&state.RightShoulder1==0,"blocked gameplay does not reconstruct fire");
        in.b=false;state={};MapButtonDriveBy(&state,in,false);
        Check(state.Circle==0&&state.RightShoulder1==0,"grips alone never fire");
        ++cases;
    }
    gVehicle.model=MI_NORMAL;physical=false;thirdPerson=true;
    CPad::GetPad(0)->mode=0;
    androidgame::PadInput in;in.b=true;CControllerState state;
    gVehicleForwardFireEnabled=false;MapButtonDriveBy(&state,in,false);
    Check(state.Circle==0&&gButtonDriveByDirection==-1,"ordinary forward Uzi fire remains opt-in");
    gVehicleForwardFireEnabled=true;MapButtonDriveBy(&state,in,false);
    Check(state.Circle==255&&gButtonDriveByDirection==0,"enabled forward Uzi fire is preserved");
    in.leftGrip=1;MapButtonDriveBy(&state,in,false);
    Check(gButtonDriveByDirection==1,"left drive-by is preserved");
    in.leftGrip=0;in.rightGrip=1;MapButtonDriveBy(&state,in,false);
    Check(gButtonDriveByDirection==2,"right drive-by is preserved");
    physical=true;weaponHeld=true;state={};MapButtonDriveBy(&state,in,false);
    Check(state.Circle==0&&gButtonDriveByDirection==-1,"tracked sidearm retains input ownership");
    std::printf("PASS: %u native vehicle input combinations and drive-by regressions\n",cases);
}
