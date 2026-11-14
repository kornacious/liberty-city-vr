
#include "platform_desktop.h"
#include "DesktopMirror.h"
#include "MatchedProfile.h"
#include "QuestUpstatePackage.h"
#include "QuestUpstate.h"
#include "AssetVariants.h"
#include "PcvrStartup.h"
#include <SDL3/SDL.h>
#include "common.h"
#include "CdStream.h"
#include <filesystem>
#include <cstdlib>
#include <iostream>
#include <string>
int RunDesktopGame(unsigned frameLimit);
// re3's global collision models free through RenderWare's allocator during CRT
// teardown, even if startup never initialized RenderWare. Flush CLI output and
// leave the process directly on help/startup failure; the OS reclaims its state.
[[noreturn]] static void FinishStartup(int code) {
    std::cout.flush(); std::cerr.flush(); std::_Exit(code);
}
int main(int argc,char **argv) {
    std::string data,user,questProfile; bool explicitData=false,explicitUser=false; unsigned frames=0; bool configured=false,checkUpstateArchive=false,checkAssets=false;
    try {
        pcvrstartup::Arguments utf8(argc,argv); argc=int(utf8.values.size()); argv=utf8.pointers.data();
        for(int i=1;i<argc;++i) {
            std::string argument=argv[i];
            if(argument=="--help") { std::cout<<"Liberty City VR Desktop game (experimental)\n--game-data DIRECTORY --user-data DIRECTORY [--frames N] [--quest-profile FILE]\nQuest profile matches captured rendering workload; PC timings are not Quest timings. Restart without the option for native PCVR.\nPaths default to pcvr-paths.ini beside the executable; CLI paths override it.\nUse an isolated locally prepared PCVR game directory; original game assets are not supplied.\n"; FinishStartup(0); }
            if(argument=="--check-upstate-archive") { checkUpstateArchive=true; continue; }
            if(argument=="--check-assets") { checkAssets=true; continue; }
            if(i+1>=argc) throw std::runtime_error("Missing option value.");
            if(argument=="--game-data") { data=argv[++i]; explicitData=true; }
            else if(argument=="--user-data") { user=argv[++i]; explicitUser=true; }
            else if(argument=="--quest-profile") questProfile=argv[++i];
            else if(argument=="--frames") { size_t used=0; auto value=std::stoul(argv[++i],&used); if(used!=strlen(argv[i]) || value>1000000) throw std::runtime_error("Invalid frame limit."); frames=unsigned(value); }
            else throw std::runtime_error("Unknown option.");
        }
        pcvrstartup::Defaults(pcvrstartup::ExecutableDirectory(),data,user,explicitData,explicitUser);
        if(data.empty() || user.empty()) throw std::runtime_error("Provide --game-data and --user-data or pcvr-paths.ini beside the executable.");
#ifdef LCVR_UPSTATE_ENGINE
        // Verify the pinned inventory before any engine state or save files open.
        if(!questpackage::CanActivate(data)) throw std::runtime_error("Upstate package is disabled or changed. Reinstall or enable it in the manager.");
        std::cout<<"Native engine: Upstate Liberty 1.1.2 (separate saves)\n";
        if(checkUpstateArchive) {
            if(!QuestUpstate::Initialize(data.c_str())) throw std::runtime_error("Cannot initialize Upstate overlay.");
            std::filesystem::current_path(std::filesystem::u8path(data));
            // Use DAT spelling/terminal whitespace and the Desktop archive opener,
            // not a resolved absolute path that could hide a missing redirect.
            if(!CdStreamAddImage("DATA\\MAPS\\UL\\ul.IMG ")) throw std::runtime_error("Cannot open the Upstate CDIMAGE through the streaming backend.");
            const bool valid=QuestUpstate::IsAssetPath(CdStreamGetImageName(0)) && GetGTA3ImgSize()>0;
            CdStreamRemoveImages();
            if(!valid) throw std::runtime_error("Upstate CDIMAGE was not registered from the mod overlay.");
            std::cout<<"Upstate CDIMAGE opened and registered from the verified mod overlay. No headset required.\n";
            FinishStartup(0);
        }
#else
        if(checkUpstateArchive) throw std::runtime_error("Use the Upstate executable for --check-upstate-archive.");
        if(questpackage::Enabled(data)) FinishStartup(pcvrstartup::DispatchUpstate(argc,argv,data,user));
#endif
        const int selected=assetvariants::Select(data);
        if(selected<0) {
            if(!checkAssets) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,"Liberty City VR","PC/Xbox base not installed or incomplete. Use Build and install in the Liberty City VR Manager to install a complete variant.",nullptr);
            throw std::runtime_error("PC/Xbox base not installed or incomplete.");
        }
        if(checkAssets) {
            if(!platform::configureDesktop(data,user))throw std::runtime_error("Desktop content root or media services unavailable.");
            platform::shutdownDesktop();
            std::cout<<(selected==0?"PC":"Original Xbox")<<" complete asset variant and Desktop platform ready.\n"; FinishStartup(0);
        }
        if(!questProfile.empty()) { MatchedProfile::Load(questProfile); std::cout<<"Quest 3 compatibility workload profile active. Timings measure this PC, not Quest hardware.\n"; }
        if(!platform::configureDesktop(data,user)) throw std::runtime_error("Local data or Desktop media services unavailable.");
        configured=true;
        if(!Desktop::Open(user)) throw std::runtime_error("Cannot create the desktop preview window.");
        int result=RunDesktopGame(frames); Desktop::Close(); platform::shutdownDesktop();
        if(result!=0) FinishStartup(result);
        return result;
    } catch(const std::exception &error) { Desktop::Close(); if(configured) platform::shutdownDesktop(); std::cerr<<"Desktop game startup: "<<error.what()<<'\n'; FinishStartup(1); }
}
