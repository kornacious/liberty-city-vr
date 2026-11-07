#define _CRT_SECURE_NO_WARNINGS
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cstddef>
#include <sys/stat.h>
#include <filesystem>
#include <chrono>
#include <map>
#include <string>
#include "../../overlay/src/audio/QuestFlacDuration.h"
#define nil nullptr
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define S_ISREG(m) (((m)&_S_IFMT)==_S_IFREG)
using int32=int32_t;
static std::map<std::string,int> settings;
static int writes,checks;
static uint32_t GetPrivateProfileIntA(const char*,const char *key,int fallback,const char*){
 auto it=settings.find(key);return uint32_t(it==settings.end()?fallback:it->second);
}
static bool WritePrivateProfileStringA(const char*,const char *key,const char *value,const char*){++writes;settings[key]=std::atoi(value);return true;}
static FILE *fcaseopen(const char *p,const char *m){return std::fopen(p,m);}
static char *casepath(const char*,bool){return nullptr;}
static void debug(const char*,...){}
enum{MITYPE_SIMPLE,MITYPE_VEHICLE,MITYPE_PED};
enum{MI_GRENADE=3,MI_BOMB=3,MODELINFOSIZE=6};
struct CBaseModelInfo{
 const char *name;int type,txd;
 int GetModelType()const{return type;}int GetTxdSlot()const{return txd;}
 const char *GetModelName()const{return name;}
};
static CBaseModelInfo catalog[6]={{"stinger",MITYPE_VEHICLE,0},{"building",MITYPE_SIMPLE,1},{"cop",MITYPE_PED,2},{"colt45",MITYPE_SIMPLE,3},{"treepatch",MITYPE_SIMPLE,4},{"sharedprop",MITYPE_SIMPLE,0}};
struct CModelInfo{
 static CBaseModelInfo *GetModelInfo(int id){return id>=0&&id<6?&catalog[id]:nullptr;}
 static CBaseModelInfo *GetModelInfo(const char *name,int *id){for(int i=0;i<6;i++)if(!_stricmp(catalog[i].name,name)){*id=i;return &catalog[i];}return nullptr;}
};
struct CTxdStore{static int FindTxdSlot(const char *name){for(int i=0;i<6;i++)if(!_stricmp(catalog[i].name,name))return catalog[i].txd;return -1;}};
#include "modelsets-production.inc"
static void require(bool b,const char *msg){++checks;if(!b){fprintf(stderr,"FAIL %s\n",msg);exit(1);}}
static void touch(const std::filesystem::path &p,const char *data="x"){
 std::filesystem::create_directories(p.parent_path());FILE*f=fopen(p.string().c_str(),"wb");require(f!=nullptr,"create fixture");fputs(data,f);fclose(f);
}
static void reset(const std::filesystem::path &root){ModelSets::gInitialized=false;writes=0;ModelSets::InitializeStartup(root.string().c_str());}
int main(){
 using namespace ModelSets;
 auto root=std::filesystem::temp_directory_path()/("lcvr-modelsets-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 settings={{"ModelSet",1}};reset(root);require(GetActive()==MODEL_SET_CLASSIC,"missing profiles fall back");require(writes==0,"startup does not rewrite settings");
 const char *identity="Source=ORIGINAL_XBOX_US_1_0\n";
 for(const char *set:{"xbox","modern"}){
  for(const char *name:{"models/gta3.img","models/gta3.dir","models/generic/wheels.dff","models/generic/player.bmp","models/generic.txd","models/particle.txd","models/coll/vehicles.col","models/coll/generic.col","data/default.ide","data/maps/generic.ide","data/maps/comse/comse.ipl","data/maps/comse/comse.col","txd/splash1.txd"})touch(root/"modelsets"/set/name);
  touch(root/"modelsets"/set/"vegetation_models.txt","treepatch\n");touch(root/"modelsets"/set/"BUILD_INFO.txt",identity);
 }
 settings={{"ModelSet",2}};reset(root);require(IsModernActive()&&!IsXboxActive(),"legacy Xbox migrates to Modern");
 require(settings["ModelSet"]==1,"legacy preset persists canonical value");
 require(!IsAvailable(MODEL_SET_XBOX),"third profile is not selectable");
 std::string md=(root/"modelsets/modern/models/gta3.dir").generic_string();
 for(int category=0;category<MODEL_CATEGORY_COUNT;category++)require(GetActiveForCategory((eModelCategory)category)==MODEL_SET_MODERN,"one complete profile for every category");
 for(const char *entry:{"stinger.dff","building.dff","cop.dff","colt45.dff","treepatch.dff","stinger.txd","building.txd","cop.txd","colt45.txd","radar00.txd","3d8ball.dff"})require(QuestOverlayEntrySelected(md.c_str(),entry),"complete Modern conversion selected");
 require(QuestOverlayEntrySelected(md.c_str(),"stinger.txd"),"shared dictionary stays paired with complete profile");
 require(!QuestOverlayEntrySelected(md.c_str(),"bogus.col"),"unsupported archive entry rejected");
 require(!GetXboxAssetPath("models/gta3.img",gGameRoot,0),"duplicate Xbox archive never registered");
 char out[1024];
 require(!IsOriginalXboxAudioActive(),"old visual-only import keeps PC audio");
 require(!GetOriginalXboxAudioPath("audio/head.wav",out,sizeof(out)),"old import cannot redirect radio");
 std::string inventory="ORIGINAL_XBOX_AUDIO_1\naudio/sfx.raw\naudio/sfx.sdt\naudio/head.flac\naudio/a1_sso.flac\n";
 for(const char *name:{"audio/sfx.raw","audio/sfx.sdt","audio/head.flac","audio/a1_sso.flac"})touch(root/"modelsets/modern"/name);
 for(int i=0;i<193;++i){auto name="audio/fixture"+std::to_string(i)+".flac";touch(root/"modelsets/modern"/name);inventory+=name+"\n";}
 touch(root/"modelsets/modern/audio/READY.txt",inventory.c_str());reset(root);
 require(IsOriginalXboxAudioActive(),"complete collection enables Xbox audio");
 require(GetOriginalXboxAudioPath("AUDIO/HEAD.WAV",out,sizeof(out))&&std::string(out).find("modelsets/modern")!=std::string::npos,"Xbox radio bypasses PC compressed sibling");
 require(GetOriginalXboxAudioPath("audio\\a1_sso.mp3",out,sizeof(out)),"PC MP3 identifier resolves Xbox FLAC");
 require(GetOriginalXboxAudioPath("audio/sfx.RAW",out,sizeof(out)),"Xbox bank resolves");
 for(const char *name:{"audio/../head.wav","/audio/head.wav","audio/nested/head.wav","mp3/custom.mp3","data/main.scm"})require(!GetOriginalXboxAudioPath(name,out,sizeof(out)),"audio override rejects unrelated and unsafe paths");
 SetRequested(MODEL_SET_CLASSIC);require(IsOriginalXboxAudioActive(),"requested source does not hot-swap live audio");reset(root);
 require(!IsOriginalXboxAudioActive()&&!GetOriginalXboxAudioPath("audio/head.wav",out,sizeof(out)),"Classic restart restores PC audio");
 SetRequested(MODEL_SET_MODERN);std::filesystem::remove(root/"modelsets/modern/audio/fixture0.flac");reset(root);
 require(!IsOriginalXboxAudioActive(),"one missing stream keeps entire collection on PC");
 touch(root/"modelsets/modern/audio/fixture0.flac");reset(root);
 // Test exact FLAC STREAMINFO duration, including non-whole-second tracks.
 auto flac=root/"duration.flac";unsigned char header[42]={};memcpy(header,"fLaC",4);header[7]=34;
 uint64_t packed=(uint64_t(44100)<<44)|44101;
 for(int i=25;i>=18;--i){header[i]=uint8_t(packed);packed>>=8;}
 FILE *duration=fopen(flac.string().c_str(),"wb");fwrite(header,1,42,duration);fclose(duration);
 require(QuestFlacDuration(flac.string().c_str())==1000,"FLAC authored sample count determines duration");
 touch(flac,"fLaC");require(QuestFlacDuration(flac.string().c_str())==0,"truncated FLAC rejected");
 for(const char *name:{"data/default.ide","data/maps/generic.ide","data/maps/comse/comse.ipl","data/maps/comse/comse.col","models/coll/generic.col","models/generic.txd","models/particle.txd","txd/splash1.txd","models/generic/wheels.dff","models/generic/player.bmp"})require(IsModernAssetPath(ResolveAssetPath(name,out,sizeof(out))),"complete Modern loose assets routed");
 for(const char *name:{"models/gta3.img","models/gta3.dir","models/coll/vehicles.col","skins/custom.bmp","data/main.scm","userfiles/GTA3sf1.b","../data/maps/generic.ide","C:/models/particle.txd"})require(ResolveAssetPath(name,out,sizeof(out))==name,"retail/private paths preserved");
 require(GetCalibrationSetForCategory(MODEL_CATEGORY_VEHICLES)==MODEL_SET_XBOX,"Xbox calibration identity retained internally");
 CycleRequested(1);require(GetRequested()==MODEL_SET_CLASSIC&&IsRestartRequired(),"cycle only offers Classic and Modern");
 require(IsModernActive(),"active objects remain until restart");reset(root);require(!IsModernActive()&&!IsXboxActive(),"Classic restart uses only base");
 const char *skin="models/generic/player.bmp"; require(ResolveAssetPath(skin,out,sizeof(out))==skin,"Classic retains original Claude bitmap");
 CycleRequested(1);require(GetRequested()==MODEL_SET_MODERN,"Classic cycles back to Modern");
 settings={{"ModelSet",1},{"ModelSetVehicles",0},{"ModelSetWorld",2},{"ModelSetVegetation",0}};reset(root);
 for(int c=0;c<MODEL_CATEGORY_COUNT;c++)require(GetActiveForCategory((eModelCategory)c)==MODEL_SET_MODERN,"old per-category mix cannot split the HD pack");
 require(writes==0,"canonical startup preserves settings");
 settings={{"ModelSet",999}};reset(root);require(GetActive()==MODEL_SET_CLASSIC,"invalid preset falls back");
 std::filesystem::remove_all(root);
 printf("PASS %d production modelset routing/calibration/startup checks\n",checks);
}
