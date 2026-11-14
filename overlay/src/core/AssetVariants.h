#pragma once
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <algorithm>
#include <map>
#include <mutex>
#include <cstdint>

// Shared by the engine and its asset-independent launchers. The content root is
// selected once per process; personal state and mods stay at the installation root.
namespace assetvariants {
// Process-local tools may explicitly choose content without rewriting the
// installation's gameplay preference. Set before any asset roots are cached.
inline int sessionSelection = -1;
inline bool Safe(const std::string &p) {
    return !p.empty() && p[0]!='/' && p.find('\\')==std::string::npos &&
        p.find(':')==std::string::npos && p.find("..")==std::string::npos;
}
inline bool Regular(const std::filesystem::path &p) {
    std::error_code e;return std::filesystem::is_regular_file(std::filesystem::symlink_status(p,e))&&!e;
}
inline bool Complete(const std::string &root,int variant) {
    namespace fs=std::filesystem;
    fs::path base=fs::u8path(root)/"variants"/(variant==0?"pc":"xbox");
    std::error_code baseError;
    for(const auto &directory:{fs::u8path(root),fs::u8path(root)/"variants",base})
        if(!fs::is_directory(fs::symlink_status(directory,baseError))||baseError)return false;
    if(!Regular(base/"VARIANT_INFO.txt"))return false;
    std::ifstream input(base/"VARIANT_INFO.txt");std::string line;
    if(!std::getline(input,line))return false;
    if(!line.empty()&&line.back()=='\r')line.pop_back();
    if(line!=(variant==0?"LCVR_VARIANT_PC_1":"LCVR_VARIANT_XBOX_1"))return false;
    std::set<std::string> files;size_t count=0;
    while(std::getline(input,line)) {
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        const auto tab=line.find('\t');if(tab==std::string::npos||!Safe(line.substr(tab+1)))return false;
        uint64_t bytes=0;try{size_t used=0;bytes=std::stoull(line.substr(0,tab),&used);if(used!=tab)return false;}catch(...){return false;}
        std::string relative=line.substr(tab+1);if(!files.insert(relative).second||++count>20000)return false;
        fs::path file=base/fs::u8path(relative);std::error_code e;
        for(auto parent=file.parent_path();parent!=base;parent=parent.parent_path())
            if(parent.empty()||parent==parent.parent_path()||!fs::is_directory(fs::symlink_status(parent,e))||e)return false;
        if(!Regular(file)||fs::file_size(file,e)!=bytes||e)return false;
    }
    for(const char *p:{"data/main.scm","data/default.dat","data/gta3.dat","data/handling.cfg","data/weapon.dat",
            "models/gta3.img","models/gta3.dir","models/frontend.txd","models/menu.txd","models/fonts.txd",
            "anim/ped.ifp","anim/cuts.img","anim/cuts.dir","text/american.gxt","audio/sfx.raw","audio/sfx.sdt"})
        if(!files.count(p))return false;
    return input.eof();
}
inline bool LegacyPc(const std::string &root) {
    // A legacy PC installation remains a separate source until reimported. It
    // never supplies fallback files to a selected managed variant.
    for(const char *p:{"data/main.scm","data/default.dat","data/gta3.dat","data/handling.cfg","data/weapon.dat",
            "models/gta3.img","models/gta3.dir","models/frontend.txd","models/menu.txd","models/fonts.txd",
            "anim/ped.ifp","anim/cuts.img","anim/cuts.dir","text/american.gxt","audio/sfx.raw","audio/sfx.sdt"}) {
        auto current=std::filesystem::u8path(root);std::string part,relative=p;std::error_code e;
        size_t start=0;
        while(start<relative.size()) {
            auto slash=relative.find('/',start);part=relative.substr(start,slash==std::string::npos?slash:slash-start);
            auto candidate=current/part;
            if(!std::filesystem::exists(candidate,e)) {
                bool found=false;
                for(std::filesystem::directory_iterator it(current,e),end;!e&&it!=end;it.increment(e)) {
                    auto name=it->path().filename().u8string();for(char &c:name)if(c>='A'&&c<='Z')c+=32;
                    if(name==part){candidate=it->path();found=true;break;}
                }
                if(!found)return false;
            }
            current=candidate;if(slash==std::string::npos)break;start=slash+1;
        }
        if(!Regular(current))return false;
    }
    return true;
}
inline int Requested(const std::string &root) {
    std::ifstream input(std::filesystem::u8path(root)/"vr_settings.ini");std::string line;bool vr=false;
    auto trim=[](std::string value) {const auto first=value.find_first_not_of(" \t\r\n");return first==std::string::npos?std::string():value.substr(first,value.find_last_not_of(" \t\r\n")-first+1);};
    while(std::getline(input,line)) {
        if(line.compare(0,3,"\xEF\xBB\xBF")==0)line.erase(0,3);
        line=trim(line);for(char &c:line)if(c>='A'&&c<='Z')c+=32;
        if(!line.empty()&&line.front()=='[')vr=line=="[vr]";
        const auto equal=line.find('=');
        if(vr&&equal!=std::string::npos&&trim(line.substr(0,equal))=="modelset") {
            const auto value=trim(line.substr(equal+1).substr(0,line.substr(equal+1).find_first_of(";#")));
            return value=="1"||value=="2"?1:0;
        }
    }
    return 0;
}
inline int Select(const std::string &root) {
    const bool pc=Complete(root,0)||LegacyPc(root),xbox=Complete(root,1);
    if(sessionSelection>=0)return sessionSelection==0?(pc?0:-1):(xbox?1:-1);
    if(pc&&xbox)return Requested(root);return pc?0:xbox?1:-1;
}
inline std::string ContentRoot(const std::string &root,int selected) {
    if(selected==0&&!Complete(root,0)&&LegacyPc(root))return root;
    return (std::filesystem::u8path(root)/"variants"/(selected==0?"pc":"xbox")).generic_u8string();
}
inline std::string Resolve(const std::string &root,std::string relative) {
    for(char &c:relative) if(c=='\\')c='/';
    std::string lower=relative;for(char &c:lower)if(c>='A'&&c<='Z')c+=32;
    bool content=false;for(const char *prefix:{"data/","models/","anim/","text/","audio/","txd/","neo/","movies/"})
        if(lower.rfind(prefix,0)==0)content=true;
    if(!content)return (std::filesystem::u8path(root)/std::filesystem::u8path(relative)).lexically_normal().generic_u8string();
    static std::map<std::string,std::string> roots;
    static std::mutex gate;std::lock_guard<std::mutex> lock(gate);
    auto found=roots.find(root);
    if(found==roots.end()) {int selected=Select(root);found=roots.emplace(root,selected<0?root+"/variants/unavailable":ContentRoot(root,selected)).first;}
    return (std::filesystem::u8path(found->second)/std::filesystem::u8path(relative)).lexically_normal().generic_u8string();
}
}
