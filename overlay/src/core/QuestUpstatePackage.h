#pragma once
#include "QuestPackageHash.h"
#include "QuestUpstateInventory.h"
#include <filesystem>
#include <fstream>

namespace questpackage {
inline std::string Normalized(std::string value) {
    for(char &c:value) { if(c=='\\') c='/'; if(c>='A'&&c<='Z') c+=32; }
    return value;
}
inline bool IsPackageAssetPath(const std::string &root, const char *path) {
    if(root.empty() || !path || !*path) return false;
    std::string base=Normalized(root), candidate=Normalized(path);
    while(!base.empty() && base.back()=='/') base.pop_back();
    base+="/mods/upstate-liberty/";
    return candidate.find("..") == std::string::npos && candidate.compare(0,base.size(),base)==0;
}
inline const char *RelativeFile(const std::string &root,const std::string &cwd,const char *input) {
    if(!input||!*input) return nullptr;
    std::string relative=Normalized(input),base=Normalized(root);
    // LoadLine turns its terminal LF/CR into spaces. Match fcaseopen's trim
    // semantics before looking up a manifest entry, including nested DAT paths.
    const auto first=relative.find_first_not_of(" \t\r\n");
    if(first==std::string::npos) return nullptr;
    relative=relative.substr(first,relative.find_last_not_of(" \t\r\n")-first+1);
    while(!base.empty()&&base.back()=='/') base.pop_back(); base+='/';
    if(relative.find("..")!=std::string::npos) return nullptr;
    if(relative[0]!='/') {
        std::string dir=Normalized(cwd); while(!dir.empty()&&dir.back()=='/') dir.pop_back(); dir+='/';
        if(dir.compare(0,base.size(),base)!=0) return nullptr;
        relative=dir.substr(base.size())+relative;
    } else {
        if(relative.compare(0,base.size(),base)!=0) return nullptr;
        relative.erase(0,base.size());
    }
    while(relative.compare(0,2,"./")==0) relative.erase(0,2);
    for(const auto &file:UpstateFiles) if(Normalized(file.path)==relative) return file.path;
    return nullptr;
}
inline bool Regular(const std::filesystem::path &path) {
    std::error_code error;
    return std::filesystem::is_regular_file(std::filesystem::symlink_status(path,error)) && !error;
}
inline bool PackageFilesValid(const std::string &root, const UpstateFile *files, size_t count) {
    namespace fs=std::filesystem;
    fs::path base=fs::path(root).lexically_normal();
    while(base!=base.root_path()&&base.filename().empty()) base=base.parent_path();
    fs::path package=base/"mods/upstate-liberty";
    std::error_code error;
    if(fs::is_symlink(fs::symlink_status(base/"mods",error)) || error) return false;
    for(size_t i=0;i<count;i++) {
        const auto &file=files[i];
        fs::path path=package/file.path;
        for(auto parent=path.parent_path();parent!=base;parent=parent.parent_path()) {
            if(parent.empty()||parent==parent.parent_path()) return false;
            if(!fs::is_directory(fs::symlink_status(parent,error))||error) return false;
        }
        if(!Regular(path)||fs::file_size(path,error)!=file.bytes||error||FileHash(path.string().c_str())!=file.sha256) return false;
    }
    return Regular(package/".lcvr-package.json");
}
inline bool PackageFilesValid(const std::string &root) { return PackageFilesValid(root,UpstateFiles,sizeof(UpstateFiles)/sizeof(UpstateFiles[0])); }
inline bool Enabled(const std::string &root) {
    auto marker=std::filesystem::path(root)/"mods/upstate-liberty.enabled";
    if(!Regular(marker)) return false;
    std::ifstream input(marker,std::ios::binary);
    std::string line;
    if(!std::getline(input,line)||line!=UpstateMarker) return false;
    return input.peek()==std::char_traits<char>::eof();
}
inline bool CanActivate(const std::string &root) { return Enabled(root)&&PackageFilesValid(root); }
}
