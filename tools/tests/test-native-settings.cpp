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
 rwRENDERSTATEDESTBLEND,rwFILTERLINEAR,rwBLENDSRCALPHA,rwBLENDINVSRCALPHA,MENUSPRITE_PLAYERSET,MENUSPRITE_MP3LOGO};
enum { FE_RADIO1,FE_RADIO2,FE_RADIO3,FE_RADIO4,FE_RADIO5,FE_RADIO6,FE_RADIO7,FE_RADIO8,FE_RADIO9 };
enum { SOUND_FRONTEND_MENU_BACK=1000,SOUND_FRONTEND_MENU_NEW_PAGE,SOUND_FRONTEND_MENU_SETTING_CHANGE,SOUND_FRONTEND_MENU_NAVIGATION };
constexpr float SCREEN_WIDTH=640,SCREEN_HEIGHT=448;
constexpr float BIGTEXT_X_SCALE=.75f,BIGTEXT_Y_SCALE=.9f,MEDIUMTEXT_X_SCALE=.55f,MEDIUMTEXT_Y_SCALE=.8f;
constexpr float SMALLESTTEXT_X_SCALE=.4f,SMALLESTTEXT_Y_SCALE=.6f,MENUHEADER_WIDTH=.84f,MENUHEADER_HEIGHT=1.6f;
#define FONT_LOCALE(x) (x)
struct CRGBA {int r,g,b,a;CRGBA(int R,int G,int B,int A):r(R),g(G),b(B),a(A){}};
struct CRect {float l,t,r,b;CRect(float L,float T,float R,float B):l(L),t(T),r(R),b(B){}};
static std::vector<CRect> highlights;
struct SpriteDraw {int id;CRect rect;CRGBA colour;};
static std::vector<SpriteDraw> spriteDraws;
struct Sprite {int id=0;void Draw(const CRect&r,const CRGBA&c){spriteDraws.push_back({id,r,c});}};
static std::vector<int> menuSounds;
static struct {void PlayFrontEndSound(int sound,int){menuSounds.push_back(sound);}} DMAudio;
static std::vector<float> sliders;
static struct {Sprite m_aMenuSprites[32],m_aFrontEndSprites[32];int m_nMenuFadeAlpha=0;int DisplaySlider(float,float,float,float,float,float progress){sliders.push_back(progress);return 0;}} FrontEndMenuManager;
struct Text {std::u16string value;float x,y,width;int font;float sx,sy;};
static std::vector<Text> texts;
namespace CSprite2d {void InitPerFrame(){} void DrawRect(const CRect&r,const CRGBA&){highlights.push_back(r);}}
namespace CFont {
static float xscale,yscale;static bool right;static int font;
void InitPerFrame(){} void SetAlphaFade(float){} void SetDropShadowPosition(int){}
void SetDropColor(const CRGBA&){} void SetFontStyle(int value){font=value;} void SetCentreOff(){}
void SetJustifyOff(){} void SetRightJustifyOff(){right=false;}void SetRightJustifyOn(){right=true;}
void SetBackgroundOff(){}void SetPropOn(){}void SetWrapx(float){}
void SetScale(float x,float y){xscale=x;yscale=y;}void SetColor(const CRGBA&){} void DrawFonts(){}
float GetStringWidth(const wchar*s,bool){return (float)std::char_traits<wchar>::length(s)*10*xscale;}
void PrintString(float x,float y,const wchar*s){float width=GetStringWidth(s,true);texts.push_back({s,right?x-width:x,y,width,font,xscale,yscale});}
}
static void AsciiToUnicode(const char *text,wchar*out){while(*text)*out++=(unsigned char)*text++;*out=0;}
static void RwRenderStateSet(int,void*){}
static QuestNativeSettings::Page suppliedPage;
namespace androidgame {bool VrCollectNativeSettings(QuestNativeSettings::Page &page){page=suppliedPage;return true;}}
static bool gVrMenuVisible=true;
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
   VR_MENU_PAGE_VEHICLE_CALIBRATION,VR_MENU_PAGE_HOLSTERS,VR_MENU_PAGE_CHEATS,VR_MENU_PAGE_MISSIONS,VR_MENU_PAGE_SIMULATION,VR_MENU_PAGE_ABOUT})
  for(int row=0;row<32;++row)Check(!IsFrontendMenuItemVisible(page,row),"gameplay-only pages cannot expose frontend rows");
 Check(!IsFrontendMenuItemVisible(VR_MENU_PAGE_SETTINGS,VR_MAIN_CHEATS)&&!IsFrontendMenuItemVisible(VR_MENU_PAGE_SETTINGS,VR_MAIN_HOLSTERS),"gameplay-only categories hidden");
 Check(!IsFrontendMenuItemVisible(VR_MENU_PAGE_SETTINGS,VR_MAIN_ABOUT),"About is absent from native settings");
 Check(!IsFrontendMenuItemVisible(VR_MENU_PAGE_WEAPONS,VR_WEAPONS_DUAL_WIELD),"Dual Wield is hidden outside live gameplay");
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
  Check(highlights[0].t==62+(selected%10)*20,"selected logical row tracks compact native pagination");
  Check(highlights[0].b<333,"rows remain above the original native title strip");
  Check(suppliedPage.First()==selected/10*10,"scroll window follows selected row");
  for(const auto&t:texts){
   Check(t.x>=29&&t.x+t.width<=611&&t.y>=0&&t.y<448,"native text stays inside menu margins");
   if(t.value.find(u"LONG SETTING")==0 || t.value==u"ON - RESTART REQUIRED")
    Check(t.font==FONT_HEADING&&t.sx==MEDIUMTEXT_X_SCALE&&t.sy==MEDIUMTEXT_Y_SCALE,"all setting labels and values retain original frontend font and size");
   if(t.value.find(u"LONG SETTING")==0)Check(t.x==110,"submenu labels use original inset left margin");
   if(t.value==u"ON - RESTART REQUIRED")Check(t.x+t.width==530,"submenu values use balanced inset right margin");
  }
 }
 suppliedPage=Page();suppliedPage.heading="SETTINGS";suppliedPage.categories=true;
 for(int i=0;i<11;++i)suppliedPage.rows.emplace_back("GRAPHICS  < OPEN >",i==0,true);
 texts.clear();highlights.clear();androidgame::VrDrawNativeSettings();
 int categories=0;
 for(const auto&t:texts){
  Check(t.value!=u"OPEN","root has no OPEN value column");
  Check(t.value.empty()||t.value[0]<u'0'||t.value[0]>u'9'||t.value.find(u" / ")==std::u16string::npos,"root has no item position counter");
  if(t.value==u"GRAPHICS"){
   ++categories;Check(t.x+t.width/2==320&&t.font==FONT_HEADING&&t.sx==BIGTEXT_X_SCALE&&t.sy==BIGTEXT_Y_SCALE,"root categories are centered with original category typography");
  }
 }
 Check(categories==10,"root contains only its centered category names");
 suppliedPage=Page();suppliedPage.heading="AUDIO AND RADIO";
 suppliedPage.rows.emplace_back("MUSIC VOLUME < 0% >",true,true);suppliedPage.rows.back().sliderProgress=0;
 suppliedPage.rows.emplace_back("SFX VOLUME < 100% >",false,true);suppliedPage.rows.back().sliderProgress=1;
 FrontEndMenuManager.m_nMenuFadeAlpha=0;
 sliders.clear();texts.clear();highlights.clear();androidgame::VrDrawNativeSettings();
 Check(FrontEndMenuManager.m_nMenuFadeAlpha==255,"legacy sliders receive visible alpha after bypassing old menu fades");
 Check(sliders==std::vector<float>({0,1}),"both volumes use the original native slider renderer with exact bounds");
 for(const auto&t:texts)Check(t.value!=u"0%"&&t.value!=u"100%","native volume sliders replace percentage text");
 for(int i=0;i<32;++i){FrontEndMenuManager.m_aFrontEndSprites[i].id=100+i;FrontEndMenuManager.m_aMenuSprites[i].id=200+i;}
 static const int radioOrder[]={FE_RADIO1,FE_RADIO2,FE_RADIO5,FE_RADIO7,FE_RADIO8,FE_RADIO3,FE_RADIO4,FE_RADIO6,FE_RADIO9};
 for(int station=0;station<12;++station)for(bool custom:{false,true}){
  suppliedPage=Page();suppliedPage.heading="AUDIO AND RADIO";suppliedPage.radioStation=station;suppliedPage.customRadioAvailable=custom;
  for(int row=0;row<6;++row)suppliedPage.rows.emplace_back("AUDIO CONTROL",row==0,true);
  spriteDraws.clear();texts.clear();highlights.clear();androidgame::VrDrawNativeSettings();
  Check(spriteDraws.size()==size_t(custom?11:10),"audio draws background and original nine station icons plus available custom tracks");
  int audioRow=0;
  for(const auto&t:texts)if(t.value==u"AUDIO CONTROL"){
   const float expectedY=64.0f+audioRow*20.0f+(audioRow>=3?65.0f:0.0f);
   Check(t.y==expectedY,"compact audio controls reserve space for the station logo strip");++audioRow;
  }
  Check(audioRow==6,"radio logos do not replace or duplicate audio controls");
  int bright=0;
  for(int i=0;i<(custom?10:9);++i){
   const auto&draw=spriteDraws[i+1];
   Check(draw.id==(i==9?200+MENUSPRITE_MP3LOGO:100+radioOrder[i]),"native station logos retain correct tuning-to-texture mapping");
   bool selected=draw.colour.r==255&&draw.colour.g==255&&draw.colour.b==255&&draw.colour.a==255;
   Check(selected==(station==i),"selected station is fully highlighted; every other icon is dimmed");
   if(selected)++bright;
   Check(draw.rect.l>=110&&draw.rect.r<=530&&draw.rect.t==130&&draw.rect.b<=170,"logo strip fits between station selection and remaining audio controls");
  }
  Check(bright==(station<(custom?10:9)?1:0),"Radio Off and unavailable stations do not highlight another logo");
 }
 menuSounds.clear();gVrMenuPage=VR_MENU_PAGE_GRAPHICS;gVrMenuVisible=true;
 PlaySharedSettingsFeedback(VR_MENU_PAGE_SETTINGS,0,13,true,false,false);
 Check(menuSounds==std::vector<int>({SOUND_FRONTEND_MENU_NEW_PAGE}),"entering Settings subpages uses native page sound");
 PlaySharedSettingsFeedback(VR_MENU_PAGE_GRAPHICS,1,19,false,false,true);
 Check(menuSounds.back()==SOUND_FRONTEND_MENU_NAVIGATION,"selection movement uses native navigation sound");
 PlaySharedSettingsFeedback(VR_MENU_PAGE_GRAPHICS,1,19,true,false,true);
 Check(menuSounds.back()==SOUND_FRONTEND_MENU_SETTING_CHANGE&&menuSounds.size()==3,"adjustment wins over simultaneous navigation without duplicate sounds");
 gVrMenuPage=VR_MENU_PAGE_SETTINGS;
 PlaySharedSettingsFeedback(VR_MENU_PAGE_GRAPHICS,18,19,true,false,false);
 Check(menuSounds.back()==SOUND_FRONTEND_MENU_BACK,"explicit Back row has the native back sound");
 gVrMenuVisible=false;PlaySharedSettingsFeedback(VR_MENU_PAGE_SETTINGS,0,13,false,true,false);
 Check(menuSounds.back()==SOUND_FRONTEND_MENU_BACK&&menuSounds.size()==5,"closing Settings with B has back feedback");
 gVrMenuVisible=true;
 for(int frame=0;frame<30;++frame)PlaySharedSettingsFeedback(VR_MENU_PAGE_SETTINGS,0,13,false,false,false);
 Check(menuSounds.size()==5,"idle and held inputs without pulses emit no repeated sounds");
 std::printf("Native settings: %u policy, column, pagination and renderer checks PASS.\n",checks);
}
