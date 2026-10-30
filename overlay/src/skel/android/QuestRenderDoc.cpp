#include "QuestRenderDoc.h"
#ifdef __ANDROID__
#include <dlfcn.h>
namespace androidgame { namespace {
questcapture::RenderDocFrame capture;
bool attached=false;
void Probe(){
    if(attached)return;
    // Passive lookup only: never load a capture layer into an ordinary run.
    const char *names[]={"libVkLayer_GLES_RenderDoc.so","libVkLayer_RenderDoc.so","librenderdoc.so"};
    for(const char *name:names){
        void *module=dlopen(name,RTLD_NOW|RTLD_NOLOAD);
        if(!module)continue;
        auto get=reinterpret_cast<pRENDERDOC_GetAPI>(dlsym(module,"RENDERDOC_GetAPI"));
        RENDERDOC_API_1_4_0 *api=nullptr;
        if(get && get(eRENDERDOC_API_Version_1_4_0,reinterpret_cast<void **>(&api))){
            capture.SetApi(api);
            if(capture.Available()){attached=true;return;} // Retain module reference.
        }
        capture.SetApi(nullptr);dlclose(module);
    }
}
}
bool QuestRenderDocAvailable(){Probe();return attached;}
questcapture::RenderDocFrame::Result QuestRenderDocBegin(void *vkInstance,const char *prefix){
    Probe();
    if(!attached)return questcapture::RenderDocFrame::Unavailable;
    // Vulkan requires the instance dispatch pointer, not VkDevice/VkInstance itself.
    void *device=vkInstance?RENDERDOC_DEVICEPOINTER_FROM_VKINSTANCE(vkInstance):nullptr;
    return capture.Begin(device,prefix);
}
questcapture::RenderDocFrame::Result QuestRenderDocEnd(bool keep){return capture.End(keep);}
bool QuestRenderDocActive(){return capture.Active();}
const char *QuestRenderDocCapturePath(){return capture.Path();}
}
#endif
