#include "../../overlay/src/skel/android/RenderDocFrameCapture.h"
#include <cassert>
#include <cstdio>
#include <cstring>
static bool active=false,failStart=false,failEnd=false;
static unsigned begins=0,ends=0,discards=0,count=0;
static void *owner=nullptr;
static std::string prefix="external/template";
static const char *GetPrefix(){return prefix.c_str();}
static void SetPrefix(const char *p){prefix=p;}
static uint32_t Count(){return count;}
static uint32_t IsActive(){return active?1:0;}
static void Start(void *p,void *window){assert(!window&&!active);owner=p;++begins;active=!failStart;}
static uint32_t End(void *p,void *window){assert(!window&&active&&p==owner);++ends;
    if(failEnd)return 0;active=false;++count;return 1;}
static uint32_t Discard(void *p,void *window){assert(!window&&active&&p==owner);++discards;active=false;return 1;}
static uint32_t GetCapture(uint32_t i,char *name,uint32_t *length,uint64_t*){
    if(i>=count)return 0;const char *path="/local/replay_frame.rdc";
    if(name)std::memcpy(name,path,std::strlen(path)+1);if(length)*length=uint32_t(std::strlen(path));return 1;
}
int main(){
    using Frame=questcapture::RenderDocFrame;Frame f;int device=0;void *root=&device;
    assert(!f.Available()&&f.Begin(root,"local/run")==Frame::Unavailable);
    assert(f.End(true)==Frame::Failed&&ends==0);
    RENDERDOC_API_1_4_0 api={};f.SetApi(&api);assert(!f.Available());
    api.StartFrameCapture=Start;api.EndFrameCapture=End;api.DiscardFrameCapture=Discard;
    api.IsFrameCapturing=IsActive;api.GetNumCaptures=Count;api.GetCapture=GetCapture;
    api.GetCaptureFilePathTemplate=GetPrefix;api.SetCaptureFilePathTemplate=SetPrefix;
    assert(f.Available());
    active=true;assert(f.Begin(root,"local/run")==Frame::Busy);
    assert(prefix=="external/template"&&begins==0&&ends==0&&discards==0);
    assert(f.End(false)==Frame::Failed&&active);active=false;
    assert(f.Begin(nullptr,"local/run")==Frame::Failed);
    failStart=true;assert(f.Begin(root,"local/run")==Frame::Failed);
    assert(!f.Active()&&prefix=="external/template");failStart=false;
    assert(f.Begin(root,"local/run")==Frame::Started&&f.Active()&&prefix=="local/run");
    const auto before=begins;assert(f.Begin(root,"local/other")==Frame::Busy&&begins==before);
    assert(f.End(true)==Frame::Saved&&count==1&&!f.Active());
    assert(std::strcmp(f.Path(),"/local/replay_frame.rdc")==0&&prefix=="external/template");
    assert(f.End(true)==Frame::Failed&&ends==1);
    assert(f.Begin(root,"local/next")==Frame::Started);
    assert(f.End(false)==Frame::Discarded&&discards==1&&count==1);
    assert(prefix=="external/template");
    assert(f.Begin(root,"local/failure")==Frame::Started);failEnd=true;
    assert(f.End(true)==Frame::Failed&&discards==2&&!active);failEnd=false;
    assert(prefix=="external/template");
    assert(f.Begin(root,"local/interrupted")==Frame::Started);active=false;
    const auto oldEnds=ends;assert(f.End(true)==Frame::Failed&&ends==oldEnds);
    assert(prefix=="external/template");
    std::puts("PASS: actual RenderDoc API table; unavailable/incomplete APIs, foreign capture ownership, matching Vulkan dispatch owner, successful path, template restoration, discard, start/end failure and interrupted capture.");
}
