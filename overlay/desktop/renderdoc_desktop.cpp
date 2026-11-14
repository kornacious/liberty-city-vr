#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include <vulkan/vulkan.h>
#include "QuestRenderDoc.h"
#include "renderdoc/renderdoc_app.h"
namespace androidgame {
namespace {
questcapture::RenderDocFrame capture;
bool Probe() {
    if(capture.Available()) return true;
    // Use an explicitly injected capture tool; never load or install one implicitly.
#ifdef _WIN32
    auto module=GetModuleHandleW(L"renderdoc.dll"); if(!module) return false;
    auto get=reinterpret_cast<pRENDERDOC_GetAPI>(GetProcAddress(module,"RENDERDOC_GetAPI"));
#else
    auto module=dlopen("librenderdoc.so",RTLD_NOW|RTLD_NOLOAD);if(!module)return false;
    auto get=reinterpret_cast<pRENDERDOC_GetAPI>(dlsym(module,"RENDERDOC_GetAPI"));
#endif
    RENDERDOC_API_1_4_0 *api=nullptr;
    if(get && get(eRENDERDOC_API_Version_1_4_0,reinterpret_cast<void**>(&api))) capture.SetApi(api);
    return capture.Available();
}
}
bool QuestRenderDocAvailable() { return Probe(); }
questcapture::RenderDocFrame::Result QuestRenderDocBegin(void *instance,const char *prefix) {
    if(!Probe()) return questcapture::RenderDocFrame::Unavailable;
    return capture.Begin(instance ? RENDERDOC_DEVICEPOINTER_FROM_VKINSTANCE(instance) : nullptr,prefix);
}
questcapture::RenderDocFrame::Result QuestRenderDocEnd(bool keep) { return capture.End(keep); }
bool QuestRenderDocActive() { return capture.Active(); }
const char *QuestRenderDocCapturePath() { return capture.Path(); }
}
