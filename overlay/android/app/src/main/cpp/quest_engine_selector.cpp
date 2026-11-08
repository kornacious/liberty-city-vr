#include <android/native_activity.h>
#include <android/log.h>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include "core/QuestUpstatePackage.h"

// Only one engine library is loaded. Their fixed tables and serialized save layouts
// are intentionally isolated. Neither engine loads Windows plugins.
extern "C" __attribute__((visibility("default")))
void ANativeActivity_onCreate(ANativeActivity *activity,void *state,size_t size)
{
    std::string root=std::string(activity->externalDataPath?activity->externalDataPath:activity->internalDataPath)+"/gamedata";
    std::error_code error;
    auto mods=std::filesystem::path(root)/"mods";
    std::filesystem::create_directories(mods,error);
    if(!error && std::filesystem::is_directory(std::filesystem::symlink_status(mods,error))) {
        auto temporary=mods/".runtime-capabilities.new";
        if(!std::filesystem::is_symlink(std::filesystem::symlink_status(temporary,error))) {
            std::ofstream capabilities(temporary,std::ios::trunc);
            // Advertise only completed adapters. This token is enabled after engine validation.
#ifdef LCVR_UPSTATE_ADAPTER_READY
            capabilities<<"upstate-liberty/1\n";
#endif
            capabilities.close();
            if(capabilities) std::filesystem::rename(temporary,mods/".runtime-capabilities",error);
        }
    }
    bool upstate=false;
#ifdef LCVR_UPSTATE_ADAPTER_READY
    upstate=questpackage::CanActivate(root);
#endif
    const char *library=upstate?"liblibertyvr_upstate.so":"liblibertyvr_classic.so";
    void *engine=dlopen(library,RTLD_NOW|RTLD_LOCAL);
    auto entry=engine?reinterpret_cast<void(*)(ANativeActivity*,void*,size_t)>(dlsym(engine,"ANativeActivity_onCreate")):nullptr;
    if(!entry) {
        __android_log_print(ANDROID_LOG_ERROR,"LibertyVR","Cannot load selected native engine: %s",library);
        ANativeActivity_finish(activity); return;
    }
    __android_log_print(ANDROID_LOG_INFO,"LibertyVR","Native engine: %s",upstate?"Upstate Liberty 1.1.2":"Classic / Xbox");
    entry(activity,state,size);
}
