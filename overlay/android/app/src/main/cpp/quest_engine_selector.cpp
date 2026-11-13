#include <android/native_activity.h>
#include <android/log.h>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include "core/QuestUpstatePackage.h"
#include "core/AssetVariants.h"

// Only one engine library is loaded. Their fixed tables and serialized save layouts
// are intentionally isolated. Neither engine loads Windows plugins.
extern "C" __attribute__((visibility("default")))
void ANativeActivity_onCreate(ANativeActivity *activity,void *state,size_t size)
{
    std::string root=std::string(activity->externalDataPath?activity->externalDataPath:activity->internalDataPath)+"/gamedata";
    if(assetvariants::Select(root)<0) {
        JNIEnv *env=activity->env;
        // NativeActivity itself belongs to the framework loader. Resolve the
        // notice through the APK context loader so a clean install can show it.
        jclass activityClass=env->GetObjectClass(activity->clazz);
        jmethodID getLoader=env->GetMethodID(activityClass,"getClassLoader","()Ljava/lang/ClassLoader;");
        jobject loader=getLoader?env->CallObjectMethod(activity->clazz,getLoader):nullptr;
        jclass notice=nullptr;
        if(loader && !env->ExceptionCheck()) {
            jclass loaderClass=env->GetObjectClass(loader);
            jmethodID load=env->GetMethodID(loaderClass,"loadClass","(Ljava/lang/String;)Ljava/lang/Class;");
            jstring name=env->NewStringUTF("com.libertyvr.quest.InstallationStatus");
            if(load)notice=static_cast<jclass>(env->CallObjectMethod(loader,load,name));
            env->DeleteLocalRef(name);env->DeleteLocalRef(loaderClass);env->DeleteLocalRef(loader);
        }
        env->DeleteLocalRef(activityClass);
        if(notice) {
            jmethodID show=env->GetStaticMethodID(notice,"show","(Landroid/app/Activity;)V");
            if(show) env->CallStaticVoidMethod(notice,show,activity->clazz);
            env->DeleteLocalRef(notice);
        }
        if(env->ExceptionCheck()) { env->ExceptionClear(); ANativeActivity_finish(activity); }
        else if(!notice) ANativeActivity_finish(activity);
        return;
    }
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
