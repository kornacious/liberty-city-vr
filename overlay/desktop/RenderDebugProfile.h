#pragma once
#include "RenderDebugCore.h"
#include "MatchedProfile.h"
namespace RenderDebug {
inline double ProfileHorizontalFov(const MatchedProfile::Values &profile){
    double result=0;
    for(int eye=0;eye<2;++eye){auto prefix="eye"+std::to_string(eye)+"_";result=std::max(result,(profile.at(prefix+"right")-profile.at(prefix+"left"))*180./3.141592653589793);}
    return result;
}
inline void ApplyProfile(Scenario &scenario,const MatchedProfile::Values &profile){
    const auto value=[&](const char *key){return profile.at(key);};
    Check(!profile.count("setting_Sgsr2Mode")||value("setting_Sgsr2Mode")==0,"Profile requests unsupported temporal reconstruction");
    scenario.settings.width=int(value("width"));scenario.settings.height=int(value("height"));scenario.settings.views=2;
    for(const auto &setting:scenario.settings.graphics){auto found=profile.find("setting_"+setting.first);if(found!=profile.end()&&found->second!=setting.second)MatchedProfile::dirty=true;}
}
}
