#define _CRT_SECURE_NO_WARNINGS
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "../../overlay/src/core/QuestGameSettings.h"
#include "../../overlay/src/skel/android/QuestSettingsMenu.h"
using int32 = int;
#define DEFAULT_SKIN_NAME "DEFAULT_SKIN"
struct CMenuManager {
    int m_PrefsMusicVolume=102, m_PrefsSfxVolume=102, m_PrefsRadioStation=0, m_PrefsLanguage=0;
    float m_PrefsLOD=1.2f;
    bool m_PrefsShowSubtitles=true, m_PrefsCutsceneBorders=true, m_bGameNotLoaded=true;
    bool m_bFrontEnd_ReloadObrTxtGxt=false, m_bMenuActive=false;
    int m_nCurrScreen=0, m_nPrevScreen=0, m_nCurrOption=0, saves=0, reloads=0;
    char m_PrefsSkinFile[256]=DEFAULT_SKIN_NAME;
    void SaveSettings(){++saves;}
    void InitialiseChangedLanguageSettings(){++reloads; m_bFrontEnd_ReloadObrTxtGxt=false;}
} FrontEndMenuManager;
struct Audio {
    int music=-1,effects=-1,station=0; bool custom=false;
    void SetMusicMasterVolume(int v){music=v;}
    void SetEffectsMasterVolume(int v){effects=v;}
    int GetRadioInCar(){return station;}
    void SetRadioInCar(int v){station=v;}
    bool IsMP3RadioChannelAvailable(){return custom;}
} DMAudio;
struct {bool m_bSetNextStation=false;} MusicManager;
int gNumRetunePresses=3, gRetuneCounter=9;
namespace CRenderer {float ms_lodDistScale=1.2f;}
static std::vector<std::string> installed;
namespace CFileMgr {
int OpenFile(const char *name,const char*) {return std::find(installed.begin(),installed.end(),name)!=installed.end()?1:0;}
int CloseFile(int){return 0;}
}
struct Player {int changes=0; std::string skin; void SetPlayerSkin(const char*v){++changes;skin=v;}};
namespace CWorld {Player Players[1];}
using HANDLE=int;
static constexpr int INVALID_HANDLE_VALUE=0;
struct WIN32_FIND_DATA {char cFileName[260];};
static std::vector<std::string> skinFiles;
static size_t skinIndex;
bool FindNextFile(HANDLE,WIN32_FIND_DATA *data){if(skinIndex==skinFiles.size())return false;std::strcpy(data->cFileName,skinFiles[skinIndex++].c_str());return true;}
HANDLE FindFirstFile(const char*,WIN32_FIND_DATA *data){skinIndex=0;return FindNextFile(1,data)?1:0;}
void FindClose(HANDLE){}

enum {MENUPAGE_OPTIONS=1,MENUPAGE_START_MENU=2,MENUPAGE_PAUSE_MENU=3,NUM_MENUROWS=18};
struct CMenuScreen {struct Entry {int m_TargetMenu=0;} m_aEntries[NUM_MENUROWS];};
static CMenuScreen aScreens[4];
struct PadInput {bool a=false,b=false,leftStickClick=false,rightStickClick=false;float rightTrigger=0,leftTrigger=0;};
static bool gSettingsFromFrontend=false,gVrMenuVisible=false,gSettingsReleaseGate=false;
static bool gVrMenuSelectDown,gVrMenuBackDown,gVrMenuIncreaseDown,gVrMenuDecreaseDown;
static int gVrMenuPage,gVrMenuSelection;
static double gVrMenuIncreaseRepeatAt,gVrMenuDecreaseRepeatAt;
static void ResetMenuNavigationRepeat(){}
static bool AnyVrAboutButtonDown(const PadInput &in){return in.a||in.b||in.leftStickClick||in.leftTrigger>=.55f||in.rightTrigger>=.55f;}
#include "shared-settings-production.inc"
static unsigned checks;
static void Check(bool result,const char *message){++checks;if(!result){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}

int main()
{
    using namespace QuestGameSettings;
    char text[112];
    for(int i=0;i<40;++i) AudioAction(Music,1);
    Check(FrontEndMenuManager.m_PrefsMusicVolume==127&&DMAudio.music==127,"music clamps at backend maximum");
    SetMusicVolume(19); AudioText(Music,text,sizeof(text));
    Check(std::strstr(text,"14%")!=nullptr,"menu observes radio-board changes without a settings mirror");
    AudioAction(Music,-1); Check(DMAudio.music==11,"menu changes same volume as board");
    for(int i=0;i<40;++i) AudioAction(Effects,-1);
    Check(DMAudio.effects==0&&FrontEndMenuManager.m_PrefsSfxVolume==0,"SFX clamps and reaches audio backend");
    const float lod=FrontEndMenuManager.m_PrefsLOD;
    AudioAction(AudioDefaults,1);
    Check(DMAudio.music==102&&DMAudio.effects==102&&FrontEndMenuManager.m_PrefsLOD==lod,"audio reset preserves graphics and VR policies");
    DMAudio.station=8; CycleRadioStation(1);
    Check(DMAudio.station==11,"station skips missing custom tracks and police radio");
    Check(MusicManager.m_bSetNextStation&&gNumRetunePresses==0&&gRetuneCounter==0,"station change requests audible retune");
    CycleRadioStation(1); Check(DMAudio.station==0,"radio off wraps to first station");
    DMAudio.custom=true; CycleRadioStation(-1);CycleRadioStation(-1);
    Check(DMAudio.station==9,"custom tracks remain reachable when available");
    for(int i=0;i<100;++i) AdjustDrawDistance(-1);
    Check(std::fabs(CRenderer::ms_lodDistScale-.8f)<.001f,"draw distance updates renderer and clamps");
    for(int i=0;i<100;++i) AdjustDrawDistance(1);
    Check(std::fabs(FrontEndMenuManager.m_PrefsLOD-1.8f)<.001f,"draw distance upper bound");
    ToggleCinemaBorders(); Check(!FrontEndMenuManager.m_PrefsCutsceneBorders,"cinema borders use engine preference");
    installed={"TEXT/AMERICAN.GXT","TEXT/GERMAN.GXT"};
    LanguageAction(Language,1);Check(FrontEndMenuManager.m_PrefsLanguage==2&&FrontEndMenuManager.reloads==1,"language skips absent text resources and reloads");
    LanguageAction(Language,1);Check(FrontEndMenuManager.m_PrefsLanguage==0,"language wraps among installed resources");
    installed.clear(); LanguageAction(Language,1);Check(FrontEndMenuManager.m_PrefsLanguage==0,"missing resources preserve working language");
    LanguageAction(Subtitles,1);Check(!FrontEndMenuManager.m_PrefsShowSubtitles,"subtitles update saved engine preference");
    CycleSkin(1);Check(std::strcmp(SkinName(),"DEFAULT")==0,"empty skins folder keeps default");
    skinFiles={"z.bmp","a.bmp"};CycleSkin(1);
    Check(std::strcmp(SkinName(),"a")==0&&CWorld::Players[0].changes==0,"skin selection is sorted and safe before game init");
    FrontEndMenuManager.m_bGameNotLoaded=false; CycleSkin(-1);
    Check(CWorld::Players[0].changes==1&&CWorld::Players[0].skin==DEFAULT_SKIN_NAME,"default skin applies to live player");
    Check(FrontEndMenuManager.saves>10,"changes invoke engine persistence");
    // Both frontends select destinations from this same catalog, with distinct
    // pages and enough space between every row and the help footer.
    for(int i=0;i<VR_MAIN_ITEM_COUNT;++i){
        Check(152+i*36+14<718,"category rows fit the menu");
        Check(QuestSettingsCategories[i].page!=VR_MENU_PAGE_SIMULATION,"Replay is not a top-level settings category");
        for(int j=0;j<i;++j) Check(QuestSettingsCategories[i].page!=QuestSettingsCategories[j].page,"categories have unique destinations");
    }
    for(bool startup:{false,true}) {
        FrontEndMenuManager.m_bMenuActive=true; FrontEndMenuManager.m_bGameNotLoaded=startup;
        FrontEndMenuManager.m_nCurrScreen=MENUPAGE_OPTIONS;
        aScreens[2].m_aEntries[1].m_TargetMenu=MENUPAGE_OPTIONS;
        aScreens[3].m_aEntries[5].m_TargetMenu=MENUPAGE_OPTIONS; // map insertion shifts Settings
        PadInput held;held.a=true;held.rightTrigger=1;
        Check(ServiceFrontendSettings(held)&&gVrMenuVisible&&gSettingsFromFrontend,"standard Settings opens shared pages");
        Check(gVrMenuPage==VR_MENU_PAGE_SETTINGS&&gVrMenuSelectDown&&gVrMenuIncreaseDown,"opening input is latched without editing first setting");
        Check(!ServiceFrontendSettings(PadInput{}),"open menu continues through shared input dispatcher");
        gVrMenuVisible=false;held.a=false;held.b=true;
        Check(ServiceFrontendSettings(held)&&gSettingsReleaseGate,"closing consumes Back until release");
        Check(FrontEndMenuManager.m_nCurrScreen==(startup?2:3)&&FrontEndMenuManager.m_nCurrOption==(startup?1:5),"return preserves parent and handles inserted map row");
        Check(ServiceFrontendSettings(PadInput{})&&!gSettingsReleaseGate,"release frame is consumed");
        Check(!ServiceFrontendSettings(PadInput{})&&!gVrMenuVisible,"close does not reopen Settings");
    }
    FrontEndMenuManager.m_bMenuActive=false;gVrMenuVisible=true;
    Check(!ServiceFrontendSettings(PadInput{})&&gVrMenuVisible&&!gSettingsFromFrontend,"shortcut overlay retains its own exit ownership");
    std::printf("Shared settings: %u checks PASS.\n",checks);
}
