#define _CRT_SECURE_NO_WARNINGS
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <map>
#include <vector>
#define nil nullptr
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
static int checks;
static void Require(bool b,const char *m){++checks;if(!b){std::fprintf(stderr,"FAIL: %s\n",m);std::exit(1);}}
static std::map<std::string,int> saved;
static int writes;
static unsigned GetPrivateProfileIntA(const char*,const char *key,int fallback,const char*){
    const auto found=saved.find(key);return found==saved.end()?fallback:found->second;
}
static void WritePrivateProfileStringA(const char*,const char *key,const char *value,const char*){
    saved[key]=std::atoi(value);++writes;
}
namespace OculusVR {
static bool loaded=false,gHandsEnabled=true,gWeaponLaser=false,gHolsterHighlights=false,gManualReload=false,gGripLock=false,gPistolHoldToRepeat=false;
enum {SCOPE_AIM_OFF=0,VR_HAND_COUNT=2};
static int gScopeAimMode=0,gManualReloadState[2];
static const char *kSettingsPath="test.ini";
static void LoadSettings(){if(loaded)return;
#include "pistol-fire-load.inc"
    loaded=true;
}
static void CycleQuestScopeAimMode(int){gScopeAimMode=(gScopeAimMode+1)%3;}
static void ResetManualReloadState(int &state){state=0;}
static const char *GetQuestScopeAimModeName(){return "OFF";}
#include "pistol-fire-settings.inc"
}
#include "pistol-fire-menu-enum.inc"
#include "pistol-fire-menu-map.inc"
enum {VR_MENU_WIDTH=1024,VR_MENU_HEIGHT=768};
static int gVrWeaponsSelection;
struct Row {std::string text;int y,scale;bool selected;};
static std::vector<Row> capturedRows;
static void BeginFullVrMenuPage(const char*,const char*){capturedRows.clear();}
static void DrawFullVrMenuRow(const char *text,int y,int scale,bool selected){capturedRows.push_back({text,y,scale,selected});}
#include "pistol-fire-menu-draw.inc"
int main(){
    Require(!OculusVR::IsQuestPistolHoldToRepeatEnabled(),"missing setting defaults to individual trigger pulls");
    const int setting=QuestWeaponSettingForWeaponItem(VR_WEAPONS_PISTOL_FIRE);
    Require(setting>=0&&setting<OculusVR::GetQuestWeaponSettingCount(),"new menu row maps to an exposed setting");
    Require(std::strcmp(OculusVR::GetQuestWeaponSettingName(setting),"PISTOL FIRE")==0,"pistol option has explicit label");
    for(int pass=0;pass<2;pass++){
        gVrWeaponsSelection=VR_WEAPONS_PISTOL_FIRE;DrawQuestWeaponsPage();
        Require(capturedRows.size()==VR_WEAPONS_ITEM_COUNT,"every menu item renders");
        const auto &pistol=capturedRows[VR_WEAPONS_PISTOL_FIRE];
        Require(pistol.text.find(pass==0?"SINGLE SHOT":"HOLD TO REPEAT")!=std::string::npos,"menu names the actual trigger behavior");
        Require(pistol.selected,"new option participates in menu selection");
        for(size_t i=0;i<capturedRows.size();i++){
            Require(capturedRows[i].y>=150&&capturedRows[i].y+8*capturedRows[i].scale<VR_MENU_HEIGHT,"menu row stays inside 768px panel");
            Require(capturedRows[i].text.size()*6*capturedRows[i].scale<VR_MENU_WIDTH,"option text fits panel width");
            if(i)Require(capturedRows[i].y-capturedRows[i-1].y>=8*capturedRows[i].scale,"menu capturedRows do not overlap");
        }
        OculusVR::ToggleQuestWeaponSetting(setting);
        Require(OculusVR::IsQuestPistolHoldToRepeatEnabled()==(pass==0),"toggle changes runtime mode");
        Require(saved["PistolHoldToRepeat"]==(pass==0?1:0),"mode is saved under its own key");
        OculusVR::loaded=false;OculusVR::gPistolHoldToRepeat=false;
        Require(OculusVR::IsQuestPistolHoldToRepeatEnabled()==(pass==0),"saved mode survives settings reload");
    }
    Require(writes==2&&saved.size()==1,"pistol toggle cannot overwrite other settings");
    Require(QuestWeaponSettingForWeaponItem(VR_WEAPONS_SCOPE_AIM)==4&&QuestWeaponSettingForWeaponItem(VR_WEAPONS_GRIP_LOCK)==5,"existing setting identities remain stable");
    Require(QuestWeaponSettingForWeaponItem(VR_WEAPONS_CALIBRATION)==-1&&QuestWeaponSettingForWeaponItem(VR_WEAPONS_BACK)==-1,"navigation capturedRows stay outside settings range");
    std::printf("Pistol setting and menu: %d checks passed.\n",checks);
}
