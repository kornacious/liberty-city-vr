#include "../../librw/src/vulkan/GpuPassProfile.h"
#include <cassert>
#include <cstdio>
#include <cstring>
using uint64=uint64_t;using uint32=uint32_t;using bool32=int;using float32=float;
#define nil nullptr
enum {VK_NULL_HANDLE=0,VK_SUCCESS=0,VK_NOT_READY=1,VK_QUERY_RESULT_64_BIT=1,VK_QUERY_RESULT_WITH_AVAILABILITY_BIT=2,VK_QUERY_RESULT_WAIT_BIT=4,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT=8};
using VkResult=int;
struct FrameContext {
    int timestampQueryPool=1;bool32 timestampPending=0,gpuTimingActive=0;
    uint64 gpuEpoch=0,gpuSourceId=0,gpuSourceStartNs=0;
    GpuPassProfile::Recording gpuProfile;
};
struct Globals {
    int device=1,activeFrame=0,frameCommands=1;
    bool32 frameTimestampEnabled=0,frameTimestampAvailable=0;
    uint32 frameTimestampValidBits=64;float frameGpuMilliseconds=0;
    struct {struct {double timestampPeriod=1000;}limits;}deviceProperties;
    FrameContext frames[2];
}gvk;
static uint64 gGpuEpoch=0,gGpuSourceId=0,gGpuSourceStartNs=0;
static bool32 gGpuRecordingActive=0;
static GpuPassProfile::Result gGpuPassResult;
static uint64 values[GpuPassProfile::QueryCount][2]={},tick=200;
static unsigned writes=0,readCount=0;
static void vkCmdWriteTimestamp(int,int stage,int,unsigned query){
    assert(stage==VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT&&query>=2&&query%2==0);
    assert(query+2<=GpuPassProfile::QueryCount); // Safe for both active views.
    values[query][0]=tick;values[query][1]=1;tick+=100;++writes;
}
static VkResult vkGetQueryPoolResults(int,int,unsigned first,unsigned count,size_t,void *out,size_t stride,unsigned flags){
    assert(first==0&&count<=GpuPassProfile::QueryCount&&stride==16);
    assert((flags&VK_QUERY_RESULT_WITH_AVAILABILITY_BIT)&&!(flags&VK_QUERY_RESULT_WAIT_BIT));
    std::memcpy(out,values,count*sizeof(values[0]));readCount=count;
    return VK_NOT_READY; // Spare mono query lanes are intentionally unwritten.
}
#include "gpu-pass-backend.inc"
static void Start(uint64 id=8,unsigned slot=0){
    gvk.activeFrame=int(slot);setGpuProfileFrame(id,id*1000);gGpuRecordingActive=1;
    auto &f=gvk.frames[slot];f.gpuEpoch=gGpuEpoch;f.gpuSourceId=gGpuSourceId;f.gpuSourceStartNs=gGpuSourceStartNs;
    f.gpuTimingActive=1;f.timestampPending=1;f.gpuProfile={};f.gpuProfile.sampled=true;
    std::memset(values,0,sizeof(values));values[0][0]=100;values[0][1]=1;values[1][0]=1100;values[1][1]=1;tick=200;writes=0;
}
static void Decode(){readCompletedFrameTimestamps(gvk.frames[gvk.activeFrame]);}
int main(){
    using namespace GpuPassProfile;
    assert(QueryCount==130&&QueryStride==2&&SampleInterval==8);
    unsigned phases=0;
    assert(!ShouldSample(0));
    for(unsigned block=0;block<8;++block){
        unsigned sampled=0;
        for(unsigned i=1;i<=8;++i){const unsigned id=block*8+i;if(ShouldSample(id)){++sampled;phases|=1u<<(id%8);}}
        assert(sampled==1);
    }
    assert(phases==255); // Do not alias every sample to one periodic frame phase.
    setGpuFrameTimingEnabled(1);Start();
    beginGpuProfilePass(World);countGpuProfileDraw();beginGpuProfilePass(Fading);countGpuProfileDraw();countGpuProfileDraw();endGpuProfilePass(Fading);endGpuProfilePass(World);
    Decode();Result result;assert(getGpuPassProfile(&result));
    assert(result.status==Complete&&result.frameId==8&&result.startNs==8000&&result.mask==((1u<<World)|(1u<<Fading)|(1u<<Other)));
    assert(std::fabs(result.ms[World]-.2f)<1e-6f&&std::fabs(result.ms[Fading]-.1f)<1e-6f&&std::fabs(result.ms[Other]-.7f)<1e-6f);
    assert(result.frameMs==1&&result.draws[World]==1&&result.draws[Fading]==2&&result.ms[Sky]==-1&&readCount==10);
    float frame=0;assert(getGpuFrameTimeMs(&frame)&&frame==1);
    Start(16,1);beginGpuProfilePass(Sky);endGpuProfilePass(Sky);Decode();assert(getGpuPassProfile(&result)&&result.frameId==16);
    Start();beginGpuProfilePass(World);endGpuProfilePass(World);values[2][1]=0;Decode();
    assert(getGpuPassProfile(&result)&&result.status==Unavailable&&result.mask==0&&result.ms[World]==-1);
    Start();beginGpuProfilePass(World);endGpuProfilePass(Fading);Decode();assert(getGpuPassProfile(&result)&&result.status==Unbalanced);
    Start();for(unsigned i=0;i<MaxMarkers/2+1;++i){beginGpuProfilePass(Sky);endGpuProfilePass(Sky);}Decode();
    assert(writes==MaxMarkers&&getGpuPassProfile(&result)&&result.status==Overflow);
    Start();beginGpuProfilePass(World);const unsigned before=writes;setGpuFrameTimingEnabled(0);endGpuProfilePass(World);countGpuProfileDraw();
    assert(writes==before&&!getGpuPassProfile(&result));
    setGpuFrameTimingEnabled(1);Decode();assert(!getGpuPassProfile(&result)); // Old session slot cannot leak.
    Start();gvk.frames[0].gpuProfile.sampled=false;beginGpuProfilePass(World);countGpuProfileDraw();endGpuProfilePass(World);Decode();
    assert(writes==0&&readCount==2&&!getGpuPassProfile(&result));
    Start();beginGpuProfilePass(World);endGpuProfilePass(World);values[0][0]=250;values[2][0]=252;values[4][0]=5;values[1][0]=20;gvk.frameTimestampValidBits=8;Decode();
    assert(getGpuPassProfile(&result)&&result.status==Complete&&std::fabs(result.frameMs-.026f)<1e-6f&&std::fabs(result.ms[World]-.009f)<1e-6f);
    assert(Milliseconds(0,0,64,1)==0&&Milliseconds(0,1,0,1)==-1&&Milliseconds(0,1,65,1)==-1);
    Start();beginGpuProfilePass(World);beginGpuProfilePass(World);countGpuProfileDraw();endGpuProfilePass(World);endGpuProfilePass(World);Decode();
    assert(getGpuPassProfile(&result)&&result.status==Complete&&result.draws[World]==1&&writes==2);
    Start();for(unsigned i=0;i<StackSize+1;++i)beginGpuProfilePass(World);Decode();assert(getGpuPassProfile(&result)&&result.status==Overflow);
    std::puts("PASS: GPU pass production hooks/readback, stereo query bounds, nested/repeated passes, draw attribution, availability, wrapping, overflow, disabled/sampled paths and session isolation.");
}
