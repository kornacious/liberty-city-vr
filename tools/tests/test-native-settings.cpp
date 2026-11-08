#define _CRT_SECURE_NO_WARNINGS
#define __ANDROID__ 1
#define RW_VULKAN 1
#define LIBERTYVR_DEV_TOOLS 1
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "../../overlay/src/core/QuestNativeSettings.h"
#include "../../overlay/src/skel/android/QuestSettingsMenu.h"
#include "../../overlay/src/vr/QuestRadio.h"
#include "../../overlay/src/vr/QuestSpeedometer.h"
using wchar=char16_t;
enum {FALSE=0,TRUE=1,FONT_HEADING=0,FONT_BANK=1,rwRENDERSTATEZTESTENABLE,
 rwRENDERSTATEZWRITEENABLE,rwRENDERSTATETEXTUREFILTER,rwRENDERSTATESRCBLEND,
 rwRENDERSTATEDESTBLEND,rwFILTERLINEAR,rwBLENDSRCALPHA,rwBLENDINVSRCALPHA,MENUSPRITE_PLAYERSET};
constexpr float SCREEN_WIDTH=640,SCREEN_HEIGHT=448;
struct CRGBA {int r,g,b,a;CRGBA(int R,int G,int B,int A):r(R),g(G),b(B),a(A){}};
struct CRect {float l,t,r,b;CRect(float L,float T,float R,float B):l(L),t(T),r(R),b(B){}};
static std::vector<CRect> highlights;
struct Sprite {void Draw(const CRect&,const CRGBA&){}};
static struct {Sprite m_aMenuSprites[32];} FrontEndMenuManager;
struct Text {std::u16string value;float x,y,width;};
static std::vector<Text> texts;
namespace CSprite2d {void InitPerFrame(){} void DrawRect(const CRect&r,const CRGBA&){highlights.push_back(r);}}
namespace CFont {
static float xscale;static bool right;
void InitPerFrame(){} void SetAlphaFade(float){} void SetDropShadowPosition(int){}
void SetDropColor(const CRGBA&){} void SetFontStyle(int){} void SetCentreOff(){}
void SetJustifyOff(){} void SetRightJustifyOff(){right=false;}void SetRightJustifyOn(){right=true;}
void SetBackgroundOff(){}void SetPropOn(){}void SetWrapx(float){}
void SetScale(float x,float){xscale=x;}void SetColor(const CRGBA&){} void DrawFonts(){}
float GetStringWidth(const wchar*s,bool){return (float)std::char_traits<wchar>::length(s)*10*xscale;}
void PrintString(float x,float y,const wchar*s){float width=GetStringWidth(s,true);texts.push_back({s,right?x-width:x,y,width});}
}
static void AsciiToUnicode(const char *text,wchar*out){while(*text)*out++=(unsigned char)*text++;*out=0;}
static void RwRenderStateSet(int,void*){}
static QuestNativeSettings::Page suppliedPage;
namespace androidgame {bool VrCollectNativeSettings(QuestNativeSettings::Page &page){page=suppliedPage;return true;}}
static int gVrMenuPage=VR_MENU_PAGE_SETTINGS,gVrMenuSelection=0,menuCount=32;
static int *CurrentMenuSelection(){return &gVrMenuSelection;}
static int CurrentMenuItemCount(){return menuCount;}
static bool IsMenuItemVisible(int page,int item);
#include "native-settings-production.inc"
static bool IsMenuItemVisible(int page,int item){return IsFrontendMenuItemVisible(page,item);}
static unsigned checks;
static void Check(bool ok,const char*message){++checks;if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
int main(){
 using namespace QuestNativeSettings;
 Row restart("* OUTPUT REQUEST  < 125% >  RESTART",true,true);
 Check(restart.label=="OUTPUT REQUEST"&&restart.value=="125%   RESTART","native columns preserve restart requirement and remove overlay decoration");
 Row action("BACK TO SETTINGS",true,true);Check(action.value.empty(),"actions have no false value column");
 for(int page:{VR_MENU_PAGE_CALIBRATION,VR_MENU_PAGE_HEAD_CALIBRATION,VR_MENU_PAGE_WRIST_RADAR,
   VR_MENU_PAGE_VEHICLE_CALIBRATION,VR_MENU_PAGE_HOLSTERS,VR_MENU_PAGE_CHEATS,VR_MENU_PAGE_MISSIONS,VR_MENU_PAGE_SIMULATION})
  for(int row=0;row<32;++row)Check(!IsFrontendMenuItemVisible(page,row),"gameplay-only pages cannot expose frontend rows");
 Check(!IsFrontendMenuItemVisible(VR_MENU_PAGE_SETTINGS,VR_MAIN_CHEATS)&&!IsFrontendMenuItemVisible(VR_MENU_PAGE_SETTINGS,VR_MAIN_HOLSTERS),"gameplay-only categories hidden");
 for(int row:{VR_GRAPHICS_PROFILER,VR_GRAPHICS_DIAGNOSTICS,VR_GRAPHICS_CULL_VIZ,VR_GRAPHICS_QUICK_START})
  Check(!IsFrontendMenuItemVisible(VR_MENU_PAGE_GRAPHICS,row),"profiler and renderer diagnostics hidden");
 Check(!IsFrontendMenuItemVisible(VR_MENU_PAGE_OPTIMIZATIONS,VR_OPT_SIMULATION),"replay link hidden");
 Check(!IsFrontendMenuItemVisible(VR_MENU_PAGE_WEAPONS,VR_WEAPONS_CALIBRATION),"weapon and hand calibration hidden");
 Check(!IsFrontendMenuItemVisible(VR_MENU_PAGE_LOCOMOTION,VR_LOCOMOTION_HEAD_CALIBRATION)&&!IsFrontendMenuItemVisible(VR_MENU_PAGE_LOCOMOTION,VR_LOCOMOTION_RECENTER),"head fitting and recenter hidden");
 for(int row=VR_HUD_HORIZONTAL_SCALE;row<=VR_HUD_OFFSET_Y;++row) Check(!IsFrontendMenuItemVisible(VR_MENU_PAGE_HUD,row),"HUD placement hidden");
 for(int row:{VR_HUD_PANEL_HAND,VR_HUD_PANEL_SIDE,VR_HUD_PANEL_CALIBRATE})Check(!IsFrontendMenuItemVisible(VR_MENU_PAGE_HUD,row),"wrist calibration links hidden");
 for(int row=VR_VEHICLE_DEFAULT_SEAT_HEIGHT;row<=VR_VEHICLE_MODEL_SEAT_FORWARD;++row)Check(!IsFrontendMenuItemVisible(VR_MENU_PAGE_VEHICLE,row),"all seat placement contexts hidden");
 Check(!IsFrontendMenuItemVisible(VR_MENU_PAGE_VEHICLE,VR_VEHICLE_CALIBRATION)&&!IsFrontendMenuItemVisible(VR_MENU_PAGE_VEHICLE,VR_VEHICLE_WHEEL_HAND_PULL_BACK),"wheel calibration hidden");
 for(int row=2;row<QuestRadio::BackRow-1;++row)Check(!IsFrontendMenuItemVisible(VR_MENU_PAGE_RADIO,row),"radio placement editing and export hidden");
 for(int row=QuestSpeedometer::FirstPlacementRow;row<QuestSpeedometer::BackRow;++row)Check(!IsFrontendMenuItemVisible(VR_MENU_PAGE_SPEEDOMETER,row),"speedometer placement and reset hidden");
 Check(IsFrontendMenuItemVisible(VR_MENU_PAGE_HUD,VR_HUD_ENABLED)&&IsFrontendMenuItemVisible(VR_MENU_PAGE_SPEEDOMETER,0)&&IsFrontendMenuItemVisible(VR_MENU_PAGE_WEAPONS,VR_WEAPONS_MANUAL_RELOAD),"persistent gameplay preferences remain editable");
 gVrMenuPage=VR_MENU_PAGE_GRAPHICS;gVrMenuSelection=VR_GRAPHICS_PROFILER;menuCount=VR_GRAPHICS_ITEM_COUNT;
 NormalizeMenuSelection();Check(IsFrontendMenuItemVisible(gVrMenuPage,gVrMenuSelection),"stale hidden selection is replaced before action dispatch");
 gVrMenuPage=VR_MENU_PAGE_CALIBRATION;gVrMenuSelection=0;menuCount=10;
 NormalizeMenuSelection();Check(gVrMenuPage==VR_MENU_PAGE_SETTINGS&&gVrMenuSelection==0,"context-only page cannot remain active in frontend");
 for(int selected=0;selected<27;++selected){
  suppliedPage=Page();suppliedPage.heading="GRAPHICS";suppliedPage.subtitle="SHARED SETTINGS";
  for(int i=0;i<27;++i)suppliedPage.rows.emplace_back("LONG SETTING DESCRIPTION FOR FITTING  < ON - RESTART REQUIRED >",i==selected,true);
  texts.clear();highlights.clear();androidgame::VrDrawNativeSettings();
  Check(highlights.size()==1,"native page has one actual selection bar");
  Check(highlights[0].t==60+(selected%10)*26,"selected logical row tracks native pagination");
  Check(highlights[0].b<333,"rows remain above the original native title strip");
  Check(suppliedPage.First()==selected/10*10,"scroll window follows selected row");
  for(const auto&t:texts)Check(t.x>=29&&t.x+t.width<=611&&t.y>=0&&t.y<448,"native text fitting stays inside menu margins");
 }
 std::printf("Native settings: %u policy, column, pagination and renderer checks PASS.\n",checks);
}
