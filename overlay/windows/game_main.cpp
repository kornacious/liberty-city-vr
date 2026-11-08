#include "RuntimeCompat.h"
#include "platform_windows.h"
#include "DesktopMirror.h"
#include <filesystem>
#include <iostream>
#include <string>
int RunWindowsGame(unsigned frameLimit);
int main(int argc,char **argv) {
    std::string data,user; unsigned frames=0;
    try {
        for(int i=1;i<argc;++i) {
            std::string argument=argv[i];
            if(argument=="--help") { std::cout<<"Liberty City VR Windows game (experimental)\n--game-data DIRECTORY --user-data DIRECTORY [--frames N]\nUse an isolated locally prepared PCVR game directory; original game assets are not supplied.\n"; return 0; }
            if(i+1>=argc) throw std::runtime_error("Missing option value.");
            if(argument=="--game-data") data=argv[++i];
            else if(argument=="--user-data") user=argv[++i];
            else if(argument=="--frames") { size_t used=0; auto value=std::stoul(argv[++i],&used); if(used!=strlen(argv[i]) || value>1000000) throw std::runtime_error("Invalid frame limit."); frames=unsigned(value); }
            else throw std::runtime_error("Unknown option.");
        }
        if(data.empty() || user.empty()) throw std::runtime_error("Explicit --game-data and --user-data directories are required.");
        if(!platform::configureWindows(data,user)) throw std::runtime_error("Local data or Windows media services unavailable.");
        if(!Desktop::Open(user)) throw std::runtime_error("Cannot create the desktop preview window.");
        int result=RunWindowsGame(frames); Desktop::Close(); platform::shutdownWindows(); return result;
    } catch(const std::exception &error) { Desktop::Close();std::cerr<<"Windows game startup: "<<error.what()<<'\n'; return 1; }
}
