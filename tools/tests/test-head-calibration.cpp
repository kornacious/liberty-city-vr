#define _CRT_SECURE_NO_WARNINGS
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <cstdint>
#include "ragdoll-test-vector.h"
#include "QuestHeadCalibration.h"
using int32=int32_t;
static unsigned checks,recenters;
static void Require(bool ok,const char *message){++checks;if(!ok){std::fprintf(stderr,"FAIL %s\n",message);std::exit(1);}}
static std::map<std::string,int> ini;
static unsigned GetPrivateProfileIntA(const char*,const char *key,int fallback,const char*){auto found=ini.find(key);return unsigned(found==ini.end()?fallback:found->second);}
static void SaveVrInteger(const char *key,int value){ini[key]=value;}
static QuestHeadCalibration::Settings gXboxHeadFit;
static bool gXboxHeadFitLoaded,gVrMenuVisible=true;
static int gVrHeadCalibrationSelection;
static char gXboxHeadFitNotice[96];
static bool returned;
static void ReturnFromCurrentMenuPage(){returned=true;}
namespace androidgame {static void VrRecenterView(){++recenters;}}
#include "head-calibration-persistence.inc"
namespace androidgame {
#include "head-calibration-api.inc"
}
static void Action(int selectedRow,bool decreasePulse=false,bool positivePulse=true){
    gVrHeadCalibrationSelection=selectedRow;
#include "head-calibration-action.inc"
}
int main(){
    using namespace QuestHeadCalibration;
    LoadXboxHeadFit();
    for(int i=0;i<VALUE_COUNT;++i){
        Require(gXboxHeadFit.value[i]==Factory(i),"missing keys use factory fit");
        for(int value=-1000;value<=1000;++value){int limited=Limit(i,value);Require(limited==Limit(i,limited),"persisted and live limits agree");}
    }
    float f,h,s;androidgame::VrGetXboxHeadOffset(&f,&h,&s);
    Require(std::fabs(f-.06f)<.00001f&&std::fabs(h-.08f)<.00001f&&s==0,"eye fit API converts centimetres to metres");
    Require(androidgame::VrXboxKnockdownLockEnabled()&&androidgame::VrXboxKnockdownBoneViewEnabled(),"factory knockdown remains at native head");
    for(int i=0;i<VALUE_COUNT;++i){for(int n=0;n<90;++n)Action(i,true,false);Require(gXboxHeadFit.value[i]==Limit(i,gXboxHeadFit.value[i]),"decrease action stays bounded");}
    gXboxHeadFit.value[FORWARD]=-7;gXboxHeadFit.value[HEIGHT]=4;gXboxHeadFit.value[SIDE]=2;gXboxHeadFit.value[BODY_YAW]=-4;
    Action(SAVE);Require(std::strstr(gXboxHeadFitNotice,"SAVED")!=nullptr,"save feedback visible");
    gXboxHeadFit=Settings();gXboxHeadFitLoaded=false;LoadXboxHeadFit();
    Require(gXboxHeadFit.value[FORWARD]==-7&&gXboxHeadFit.saved[FORWARD]==-7,"signed live and personal defaults survive restart");
    Require(gXboxHeadFit.value[BODY_YAW]==-4&&androidgame::VrXboxBodyYawDegrees()==-4,"signed body yaw survives restart");
    Action(FORWARD);Require(gXboxHeadFit.value[FORWARD]==-6&&ini[Key(FORWARD)]==-6,"live action applies and persists immediately");
    Action(RESTORE);Require(gXboxHeadFit.value[FORWARD]==-7,"restore uses personal baseline");
    Action(FACTORY);Require(gXboxHeadFit.value[FORWARD]==Factory(FORWARD)&&gXboxHeadFit.saved[FORWARD]==-7,"factory reset preserves personal default");
    Action(RESTORE);Require(gXboxHeadFit.value[HEIGHT]==4,"personal default remains available after factory reset");
    const bool wasLocked=androidgame::VrXboxKnockdownLockEnabled();
    Action(LOCK);Require(androidgame::VrXboxKnockdownLockEnabled()!=wasLocked,"movement lock responds immediately");
    Action(RECENTER);Require(recenters==1&&!gVrMenuVisible,"recenter closes overlay using existing reference reset");
    Action(BACK);Require(returned,"submenu back returns to locomotion");
    std::printf("PASS %u production head calibration checks\n",checks);
}
