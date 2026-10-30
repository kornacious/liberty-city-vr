#pragma once
#include "renderdoc/renderdoc_app.h"
#include <string>
#include <vector>

namespace questcapture {
// Own only captures started here. Never finish/discard a GUI-owned capture.
class RenderDocFrame {
    RENDERDOC_API_1_4_0 *api=nullptr;
    void *device=nullptr;
    bool owned=false;
    uint32_t firstCapture=0;
    std::string previousTemplate,path;
    void Restore(){api->SetCaptureFilePathTemplate(previousTemplate.c_str());}
public:
    enum Result {Unavailable,Busy,Failed,Started,Saved,Discarded};
    void SetApi(RENDERDOC_API_1_4_0 *value){if(!owned)api=value;}
    bool Available() const {return api && api->StartFrameCapture && api->EndFrameCapture &&
        api->IsFrameCapturing && api->DiscardFrameCapture && api->GetNumCaptures &&
        api->GetCapture && api->GetCaptureFilePathTemplate && api->SetCaptureFilePathTemplate;}
    bool Active() const {return owned;}
    const char *Path() const {return path.c_str();}
    Result Begin(void *owner,const char *prefix){
        if(owned || (Available() && api->IsFrameCapturing()))return Busy;
        if(!Available())return Unavailable;
        if(!owner || !prefix || !prefix[0])return Failed;
        path.clear();device=owner;firstCapture=api->GetNumCaptures();
        const char *old=api->GetCaptureFilePathTemplate();previousTemplate=old?old:"";
        api->SetCaptureFilePathTemplate(prefix);
        api->StartFrameCapture(device,nullptr);
        owned=api->IsFrameCapturing()!=0;
        if(!owned){Restore();return Failed;}
        return Started;
    }
    Result End(bool keep){
        if(!owned)return Failed;
        if(!api->IsFrameCapturing()){owned=false;Restore();device=nullptr;return Failed;}
        Result result=Discarded;
        if(keep){
            result=api->EndFrameCapture(device,nullptr)?Saved:Failed;
            if(result==Saved && api->GetNumCaptures()>firstCapture){
                uint32_t length=0;
                if(api->GetCapture(firstCapture,nullptr,&length,nullptr) && length && length<=4096){
                    std::vector<char> name(length+1,0);
                    if(api->GetCapture(firstCapture,name.data(),&length,nullptr))path=name.data();
                }
            }
        }
        if((!keep || result==Failed) && api->IsFrameCapturing())
            if(!api->DiscardFrameCapture(device,nullptr))result=Failed;
        owned=false;Restore();device=nullptr;return result;
    }
};
}
