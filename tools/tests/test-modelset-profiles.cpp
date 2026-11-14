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
#include "../../overlay/src/core/AssetVariants.h"
#define nil nullptr
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define S_ISREG(m) (((m)&_S_IFMT)==_S_IFREG)
using int32=int32_t;
static std::map<std::string,int> settings;
static int writes,checks;
[[maybe_unused]] static uint32_t GetPrivateProfileIntA(const char*,const char *key,int fallback,const char*){
 auto it=settings.find(key);return uint32_t(it==settings.end()?fallback:it->second);
}
static bool WritePrivateProfileStringA(const char*,const char *key,const char *value,const char*){++writes;settings[key]=std::atoi(value);return true;}
static FILE *fcaseopen(const char *p,const char *m){return std::fopen(p,m);}
[[maybe_unused]] static char *casepath(const char*,bool){return nullptr;}
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
static void reset(const std::filesystem::path &root){
 std::filesystem::create_directories(root);
 std::ofstream(root/"vr_settings.ini")<<"[VR]\nModelSet="<<settings["ModelSet"]<<"\n";
 ModelSets::gInitialized=false;writes=0;ModelSets::InitializeStartup(root.string().c_str());
}
static void variant(const std::filesystem::path &root,int id){
 auto base=root/"variants"/(id?"xbox":"pc");
 for(const char *name:{"data/main.scm","data/default.dat","data/gta3.dat","data/handling.cfg","data/weapon.dat",
  "models/gta3.img","models/gta3.dir","models/frontend.txd","models/menu.txd","models/fonts.txd",
  "anim/ped.ifp","anim/cuts.img","anim/cuts.dir","text/american.gxt","audio/sfx.raw","audio/sfx.sdt","models/generic/player.bmp","data/particle.cfg"})touch(base/name);
 touch(base/"vegetation_models.txt","treepatch\n");
 std::ofstream audio(base/"audio/READY.txt");audio<<"ORIGINAL_XBOX_AUDIO_1\n";
 for(int i=0;i<195;i++){std::string name="audio/test"+std::to_string(i)+".flac";touch(base/name);audio<<name<<'\n';}
 audio<<"audio/sfx.raw\naudio/sfx.sdt\n";audio.close();
 std::ofstream marker(base/"VARIANT_INFO.txt");marker<<(id?"LCVR_VARIANT_XBOX_1\n":"LCVR_VARIANT_PC_1\n");
 for(const auto &entry:std::filesystem::recursive_directory_iterator(base))if(entry.is_regular_file()&&entry.path().filename()!="VARIANT_INFO.txt")
  marker<<entry.file_size()<<'\t'<<std::filesystem::relative(entry.path(),base).generic_string()<<'\n';
}
int main(){
 using namespace ModelSets;
 auto root=std::filesystem::temp_directory_path()/("lcvr-modelsets-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 settings={{"ModelSet",1}};reset(root);require(!ContentRoot()[0]&&!IsAvailable(MODEL_SET_CLASSIC),"no installation is unavailable rather than implicit Classic");
 variant(root,1);reset(root);require(IsModernActive(),"sole complete Xbox is selected");require(IsOriginalXboxAudioActive(),"Xbox audio is complete and active");
 require(GetCalibrationSetForCategory(MODEL_CATEGORY_PEDS)==MODEL_SET_XBOX,"Xbox calibration retained");
 require(!IsAvailable(MODEL_SET_CLASSIC)&&!IsAvailable(MODEL_SET_XBOX),"uninstalled PC and legacy third value unavailable");
 char out[1024];const char *path="data/main.scm";
 require(GetOriginalXboxAudioPath("audio/sfx.SDT",out,sizeof(out)),"mixed-case SDT resolves imported lowercase bank");
 require(GetOriginalXboxAudioPath("AUDIO\\SFX.RAW",out,sizeof(out)),"mixed-case RAW and separators resolve imported bank");
 require(GetOriginalXboxAudioPath("Audio/TEST0.WAV",out,sizeof(out)),"mixed-case speech resolves imported FLAC");
 require(!GetOriginalXboxAudioPath("audio/../sfx.RAW",out,sizeof(out)),"audio traversal remains rejected");
 require(std::string(ResolveAssetPath(path,out,sizeof(out))).find("variants/xbox/data/main.scm")!=std::string::npos,"script uses complete Xbox base");
 require(std::string(ResolveAssetPath("models/coll/vehicles.col",out,sizeof(out))).find("variants/xbox/")!=std::string::npos,"vehicle collision never borrows PC files");
 require(std::string(ResolveAssetPath("data/missing.dat",out,sizeof(out))).find("variants/xbox/")!=std::string::npos,"missing files remain scoped to selected base");
 require(!GetModernAssetPath("models/gta3.img",out,sizeof(out)),"variant archive is not registered twice");
 const char *save="userfiles/autosave.b";require(ResolveAssetPath(save,out,sizeof(out))==save,"personal state stays shared");
 SetRequested(MODEL_SET_CLASSIC);require(GetRequested()==MODEL_SET_MODERN&&writes==0,"cannot select unavailable PC");
 const std::string dir=(root/"variants/xbox/models/gta3.dir").generic_string();
 for(const char *entry:{"stinger.dff","building.dff","cop.dff","colt45.dff","treepatch.dff","sharedprop.dff","3d8ball.dff","building.txd"})
  require(QuestOverlayEntrySelected(dir.c_str(),entry),"all categories use selected complete archive");
 variant(root,0);reset(root);CycleRequested(1);require(GetRequested()==MODEL_SET_CLASSIC&&IsRestartRequired(),"installed PC can be requested without replacing live assets");
 require(IsModernActive(),"live objects retained until restart");reset(root);require(!IsModernActive()&&GetActive()==MODEL_SET_CLASSIC,"restart selects PC base");
 require(std::string(ResolveAssetPath(path,out,sizeof(out))).find("variants/pc/data/main.scm")!=std::string::npos,"PC script uses PC root");
 std::filesystem::remove(root/"variants/xbox/models/gta3.dir");settings["ModelSet"]=1;reset(root);
 require(GetActive()==MODEL_SET_CLASSIC&&!IsAvailable(MODEL_SET_MODERN),"incomplete Xbox unavailable, complete PC selected");
 std::filesystem::remove_all(root);printf("PASS %d complete variant routing/calibration/startup checks\n",checks);return 0;
}
