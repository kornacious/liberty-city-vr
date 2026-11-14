#include "common.h"
#include "RenderDebugClient.h"
#include "RenderDebugProfile.h"
#include "DesktopMirror.h"
#include "platform_desktop.h"
#include "WorldSession.h"
#include "MatchedProfile.h"
#include "PcvrStartup.h"
#include "AssetVariants.h"
#include "QuestUpstatePackage.h"
#include <iostream>
#include <cstdlib>
[[noreturn]] static void Finish(int result){std::cout.flush();std::cerr.flush();std::_Exit(result);}
int main(int argc,char **argv){
    bool configured=false;int failure=2;
    try {
        pcvrstartup::Arguments utf8(argc,argv);argc=int(utf8.values.size());argv=utf8.pointers.data();
        std::string data,user,profile,scenarioFile,output;bool explicitData=false,explicitUser=false;unsigned frames=0;
        for(int i=1;i<argc;++i){std::string arg=argv[i];if(arg=="--help"){
            std::cout<<"Liberty City renderer debug viewer (Windows/Linux, no OpenXR required)\n"
                "--game-data DIRECTORY --user-data DIRECTORY [--quest-profile FILE]\n"
                "[--scenario FILE --output DIRECTORY] [--frames N]\n"
                "F1: settings; F2: statistics; hold right mouse in the viewport to fly/look with WASD/QE; Shift/Ctrl: fast/slow.\n"
                "Scenario version 1: settings, camera or path, timestep, duration, warmup_frames, repetitions, screenshots.\n"
                "Exit codes: 0 completed, 1 runtime/output error or interrupted batch, 2 invalid/unsupported input.\n"
                "Uses locally owned prepared game assets. Timings measure this PC, never Quest hardware.\n";Finish(0);}
            if(i+1>=argc)throw std::runtime_error("Missing option value");
            if(arg=="--game-data"){data=argv[++i];explicitData=true;}else if(arg=="--user-data"){user=argv[++i];explicitUser=true;}else if(arg=="--quest-profile")profile=argv[++i];else if(arg=="--scenario")scenarioFile=argv[++i];else if(arg=="--output")output=argv[++i];else if(arg=="--frames"){std::string n=argv[++i];size_t used=0;auto value=std::stoul(n,&used);RenderDebug::Check(used==n.size()&&value<=1000000&&n[0]!='-',"Invalid frame limit");frames=unsigned(value);}else throw std::runtime_error("Unknown option: "+arg);
        }
        auto absolute=[&](std::string &p){if(!p.empty())p=std::filesystem::absolute(std::filesystem::u8path(p)).u8string();};absolute(scenarioFile);absolute(profile);absolute(output);
        pcvrstartup::Defaults(pcvrstartup::ExecutableDirectory(),data,user,explicitData,explicitUser);
        RenderDebug::Check(!data.empty(),"Provide --game-data or pcvr-paths.ini");absolute(data);
        // Even an explicitly selected gameplay user root receives a viewer-only
        // child. This prevents accidental preference/save writes on direct launch.
        if(user.empty())user=(std::filesystem::u8path(data).parent_path()/"render-debug-user").u8string();
        user=(std::filesystem::u8path(user)/"render-debug").u8string();absolute(user);
        auto contentPath=std::filesystem::u8path(data),userPath=std::filesystem::u8path(user);
        RenderDebug::Check(!RenderDebug::Within(contentPath,userPath)&&!RenderDebug::Within(userPath,contentPath),"Keep viewer user/data directories separate");
        if(!output.empty()){auto outputPath=std::filesystem::u8path(output);RenderDebug::Check(!RenderDebug::Within(contentPath,outputPath)&&!RenderDebug::Within(outputPath,contentPath),"Keep run outputs separate from game content");}
        RenderDebug::Scenario scenario;
        if(!scenarioFile.empty()){
            scenario=RenderDebug::DecodeScenario(DebugJson::Load(std::filesystem::u8path(scenarioFile)));
            if(profile.empty()&&!scenario.questProfile.empty())profile=(std::filesystem::u8path(scenarioFile).parent_path()/std::filesystem::u8path(scenario.questProfile)).lexically_normal().u8string();
        }else if(std::filesystem::exists(std::filesystem::u8path(user)/"viewer.json"))scenario=RenderDebug::DecodeScenario(DebugJson::Load(std::filesystem::u8path(user)/"viewer.json"));
        if(profile.empty()&&!scenario.questProfile.empty())profile=scenario.questProfile;
        if(!profile.empty()){if(scenarioFile.empty())scenario.settings.graphics.clear();MatchedProfile::Load(profile);scenario.questProfile=profile;RenderDebug::ApplyProfile(scenario,MatchedProfile::requested);}
        const bool upstate=questpackage::Enabled(data);
        if(scenario.content=="upstate")RenderDebug::Check(upstate,"Enable the prepared Upstate package before selecting its scenario");
        const bool useUpstate=scenario.content=="upstate" || (scenario.content=="selected" && upstate);
#ifdef LCVR_UPSTATE_ENGINE
        RenderDebug::Check(questpackage::CanActivate(data),"Upstate package is disabled or changed");
#else
        if(useUpstate)Finish(pcvrstartup::DispatchUpstate(argc,argv,data,std::filesystem::u8path(user).parent_path().u8string()));
#endif
        if(scenario.content=="classic")assetvariants::sessionSelection=0;
        if(scenario.content=="xbox")assetvariants::sessionSelection=1;
        if(MatchedProfile::active && assetvariants::sessionSelection<0 && MatchedProfile::requested.count("setting_ModelSet"))assetvariants::sessionSelection=MatchedProfile::requested.at("setting_ModelSet")>=1?1:0;
        const int variant=assetvariants::Select(data);RenderDebug::Check(variant>=0,"Prepared Classic/Original Xbox data is missing or incomplete");
        if(scenario.content=="classic")RenderDebug::Check(variant==0,"Scenario requires the Classic variant");if(scenario.content=="xbox")RenderDebug::Check(variant==1,"Scenario requires the Original Xbox variant");
        WorldSession::viewer=true;WorldSession::settings=scenario.settings.graphics;
        failure=1;RenderDebug::Check(platform::configureDesktop(data,user),"Cannot configure local viewer data/services");configured=true;
        RenderDebug::Check(Desktop::Open(user),"Cannot create SDL viewer window");
        int result=RenderDebug::Run(scenario,user,output,!scenarioFile.empty(),frames);
        Desktop::Close();platform::shutdownDesktop();Finish(result);
    }catch(const std::exception &e){Desktop::Close();if(configured)platform::shutdownDesktop();std::cerr<<"Renderer debug: "<<e.what()<<'\n';Finish(dynamic_cast<const std::invalid_argument*>(&e)?2:failure);}
}
