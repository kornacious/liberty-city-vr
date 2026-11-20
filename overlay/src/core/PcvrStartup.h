#pragma once
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#else
#include <unistd.h>
#endif

namespace pcvrstartup {
inline constexpr char GameHelp[] =
#ifdef _WIN32
    "Liberty City VR Windows game\n"
#else
    "Liberty City VR Linux game (experimental)\n"
#endif
    "--game-data DIRECTORY --user-data DIRECTORY [--frames N] [--quest-profile FILE]\n"
    "Quest profile matches captured rendering workload; PC timings are not Quest timings. Restart without the option for native PCVR.\n"
    "Paths default to pcvr-paths.ini beside the executable; CLI paths override it.\n"
    "Use an isolated locally prepared PCVR game directory; original game assets are not supplied.\n";

struct Arguments {
    std::vector<std::string> values;
    std::vector<char*> pointers;
    Arguments(int argc,char **argv) {
#ifdef _WIN32
        int count=0; auto wide=CommandLineToArgvW(GetCommandLineW(),&count);
        if(!wide) throw std::runtime_error("Cannot read PCVR command line.");
        for(int i=0;i<count;++i) values.push_back(std::filesystem::path(wide[i]).u8string());
        LocalFree(wide);
#else
        for(int i=0;i<argc;++i) values.emplace_back(argv[i]);
#endif
        for(auto &value:values) pointers.push_back(value.data());
        pointers.push_back(nullptr);
    }
};
inline std::filesystem::path ExecutableDirectory() {
#ifdef _WIN32
    std::vector<wchar_t> path(32768);
    const auto size=GetModuleFileNameW(nullptr,path.data(),DWORD(path.size()));
    if(!size || size>=path.size()) throw std::runtime_error("Cannot locate PCVR executable.");
    return std::filesystem::path(std::wstring(path.data(),size)).parent_path();
#else
    return std::filesystem::read_symlink("/proc/self/exe").parent_path();
#endif
}

inline void Defaults(const std::filesystem::path &directory,std::string &data,std::string &user,bool explicitData=false,bool explicitUser=false) {
    if(explicitData && explicitUser) return;
    auto file=directory/"pcvr-paths.ini";
    if(!std::filesystem::exists(file)) return;
    if(std::filesystem::file_size(file)>32768) throw std::runtime_error("PCVR path configuration is too large.");
    std::ifstream input(file,std::ios::binary);
    if(!input) throw std::runtime_error("Cannot read pcvr-paths.ini.");
    std::map<std::string,std::string> values;
    std::string line;
    while(std::getline(input,line)) {
        if(!line.empty() && line.back()=='\r') line.pop_back();
        if(line.empty() || line[0]=='#') continue;
        auto equal=line.find('=');
        if(equal==std::string::npos || equal==0 || !values.emplace(line.substr(0,equal),line.substr(equal+1)).second)
            throw std::runtime_error("Invalid pcvr-paths.ini.");
    }
    if(values["version"]!="1") throw std::runtime_error("Unsupported pcvr-paths.ini version.");
    for(const auto &entry:values) if(entry.first!="version" && entry.first!="data" && entry.first!="user") throw std::runtime_error("Unknown pcvr-paths.ini key.");
    auto resolve=[&](const char *key) {
        auto value=values[key];
        if(value.empty() || value.find('\0')!=std::string::npos) throw std::runtime_error("Missing PCVR data/user path in pcvr-paths.ini.");
        return (directory/std::filesystem::u8path(value)).lexically_normal().u8string();
    };
    if(!explicitData) data=resolve("data");
    if(!explicitUser) user=resolve("user");
}

// A sibling process retains the resolved paths and every original CLI option.
// The Upstate build does not dispatch again and validates its own package.
inline int DispatchSibling(const std::filesystem::path &executable,int argc,char **argv,const std::string &data,const std::string &user) {
    if(!std::filesystem::exists(executable)) throw std::runtime_error("Enabled Upstate engine is missing. Rebuild or disable the mod in the manager.");
    std::vector<std::string> args{executable.u8string()};
    for(int i=1;i<argc;++i) args.emplace_back(argv[i]);
    args.insert(args.end(),{"--game-data",data,"--user-data",user});
#ifdef _WIN32
    auto quote=[](const std::string &utf8) {
        auto value=std::filesystem::u8path(utf8).wstring();
        std::wstring result=L"\""; size_t slashes=0;
        for(wchar_t c:value) {
            if(c==L'\\') {++slashes;continue;}
            result.append(c==L'"'?slashes*2+1:slashes,L'\\'); slashes=0; result+=c;
        }
        result.append(slashes*2,L'\\'); return result+L'"';
    };
    std::wstring command;
    for(const auto &arg:args) { if(!command.empty())command+=L' '; command+=quote(arg); }
    STARTUPINFOW startup{}; startup.cb=sizeof(startup); PROCESS_INFORMATION process{};
    if(!CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&startup,&process)) throw std::runtime_error("Cannot start Upstate engine.");
    CloseHandle(process.hThread);
    WaitForSingleObject(process.hProcess,INFINITE);
    DWORD code=1; GetExitCodeProcess(process.hProcess,&code); CloseHandle(process.hProcess);
    return int(code);
#else
    std::vector<char*> pointers;
    for(auto &arg:args) pointers.push_back(arg.data());
    pointers.push_back(nullptr);
    execv(executable.c_str(),pointers.data());
    throw std::runtime_error("Cannot start Upstate engine.");
#endif
}
inline int DispatchUpstate(int argc,char **argv,const std::string &data,const std::string &user) {
    return DispatchSibling(ExecutableDirectory()/
#ifdef _WIN32
        #ifdef LIBERTYVR_RENDER_DEBUG
        "libertyvr-render-debug-upstate.exe",
        #else
        "libertyvr-upstate.exe",
        #endif
#else
        #ifdef LIBERTYVR_RENDER_DEBUG
        "libertyvr-render-debug-upstate",
        #else
        "libertyvr-upstate",
        #endif
#endif
        argc,argv,data,user);
}
}
