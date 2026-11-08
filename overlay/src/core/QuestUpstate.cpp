#include "common.h"
#include "QuestUpstate.h"
#include "QuestUpstateInventory.h"
#include "QuestUpstatePackage.h"
#include "ModelInfo.h"
#include "PlayerPed.h"
#include "World.h"
#include "Zones.h"
#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include <cstdio>
#include <unistd.h>
#ifdef __ANDROID__
#include <android/log.h>
#endif

#ifdef LCVR_UPSTATE_ENGINE
static_assert(NUMBUILDINGS == 100000 && NUMOBJECTS == 10000 && NUMDUMMIES == 30000, "Upstate entity limits were not applied");
static_assert(NUMPHONES == 500 && NUM_GARAGES == 48 && NUMPICKUPS == 700, "Upstate save tables were not applied");
static_assert(NUMZONES == 100 && NUMMAPZONES == 50 && NUMAUDIOZONES == 50, "Upstate zone limits were not applied");
static_assert(NUM_PATHNODES == 32767 && NUM_PATHCONNECTIONS == 32767, "Upstate signed path limits were not applied");
#ifdef FIX_INCOMPATIBLE_SAVES
#error The legacy PC save converter must not process expanded Upstate saves
#endif
#endif

namespace QuestUpstate {
namespace {
std::string gameRoot, packageRoot;
std::map<std::string,std::vector<std::string>> traits;
std::map<std::string,std::vector<uint16_t>> text;
std::string lower(std::string s) { for(char &c:s) { if(c=='\\') c='/'; if(c>='A'&&c<='Z') c+=32; } return s; }
}
bool Initialize(const char *root) {
    if(!Active()) return true;
    if(!questpackage::CanActivate(root)) return false;
    gameRoot=root; while(!gameRoot.empty()&&gameRoot.back()=='/') gameRoot.pop_back();
    packageRoot=gameRoot+"/mods/upstate-liberty/";
    traits.clear(); text.clear();
    FILE *f=fopen((packageRoot+"data/modelTraits.dat").c_str(),"rt");
    char line[1024], section[24]="";
    if(f) {
        while(fgets(line,sizeof(line),f)) {
            char word[128]; if(sscanf(line,"%127s",word)!=1||word[0]=='#') continue;
            if(!strcmp(word,"door")||!strcmp(word,"tree")||!strcmp(word,"banner")||!strcmp(word,"glass")) strcpy(section,word);
            else if(!strcmp(word,"end")) section[0]=0;
            else if(section[0]) traits[section].push_back(lower(word));
        }
        fclose(f);
    }
    f=fopen((packageRoot+"CLEO/CLEO_TEXT/ul_text.fxt").c_str(),"rt");
    if(f) {
        while(fgets(line,sizeof(line),f)) {
            char *value=strpbrk(line," \t"); if(!value||line[0]=='#'||line[0]=='\0') continue;
            *value++=0; while(*value==' '||*value=='\t') ++value;
            value[strcspn(value,"\r\n")]=0;
            std::vector<uint16_t> wide; for(const unsigned char *p=(unsigned char*)value;*p;++p) wide.push_back(*p);
            wide.push_back(0); text[line]=std::move(wide);
        }
        fclose(f);
    }
#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_INFO,"LibertyVR","Upstate overlay ready: %zu pinned files, %zu FXT entries; expanded save profile",sizeof(questpackage::UpstateFiles)/sizeof(questpackage::UpstateFiles[0]),text.size());
#endif
    return true;
}
const char *Resolve(const char *path,char *out,size_t capacity) {
    if(!Active()||packageRoot.empty()||!path||!*path||!out||!capacity) return path;
    char cwd[2048]; if(!getcwd(cwd,sizeof(cwd))) return path;
    if(const char *relative=questpackage::RelativeFile(gameRoot,cwd,path)) {
        std::string resolved=packageRoot+relative;
        if(resolved.size()+1>capacity) return path;
        memcpy(out,resolved.c_str(),resolved.size()+1); return out;
    }
    return path;
}
bool IsAssetPath(const char *path) {
    return Active() && questpackage::IsPackageAssetPath(gameRoot,path);
}
bool HasTrait(int id,const char *trait) {
    if(!Active()||id<0||id>=MODELINFOSIZE) return false;
    CBaseModelInfo *info=CModelInfo::GetModelInfo(id); if(!info) return false;
    return HasNameTrait(info->GetModelName(),trait);
}
bool HasNameTrait(const char *modelName,const char *trait) {
    if(!Active()||!modelName) return false;
    const auto found=traits.find(trait); if(found==traits.end()) return false;
    auto name=lower(modelName);
    return std::find(found->second.begin(),found->second.end(),name)!=found->second.end();
}
uint16_t *Text(const char *key) { if(!Active()||!key) return nullptr; auto it=text.find(key); return it==text.end()?nullptr:it->second.data(); }
bool RuralRubbish() {
    if(!Active()) return false;
    CPlayerPed *player=FindPlayerPed(); if(!player) return false;
    const char *labels[]={"UL_ZON0","GT_ZON0","BAXTER","WARSAW","CARRIN"};
    bool inside=false;
    for(int i=0;i<5;i++) { int id=CTheZones::FindZoneByLabelAndReturnIndex(labels[i]); if(id<0) continue;
        CZone *zone=CTheZones::GetZone(id); bool hit=zone&&CTheZones::PointLiesWithinZone(&player->GetPosition(),zone);
        if(i==0) inside=hit; else if(hit) return false;
    }
    return inside;
}
}
