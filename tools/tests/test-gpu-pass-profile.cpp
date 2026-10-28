#include "../../librw/src/vulkan/GpuPassProfile.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <limits>
using uint64=uint64_t;using uint32=uint32_t;using int32=int;using bool32=int;using float32=float;
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
static bool renderPassOpen=false;
static void vkCmdWriteTimestamp(int,int stage,int,unsigned query){
    assert(!renderPassOpen); // Timestamp hooks must stay outside native render passes.
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
void countGpuProfileDraw(uint32 count=0,uint32 topology=~0u,uint32 flags=0);
#include "gpu-pass-backend.inc"
static void Start(uint64 id=8,unsigned slot=0){
    gvk.frameTimestampValidBits=64;gvk.activeFrame=int(slot);setGpuProfileFrame(id,id*1000);gGpuRecordingActive=1;
    auto &f=gvk.frames[slot];f.gpuEpoch=gGpuEpoch;f.gpuSourceId=gGpuSourceId;f.gpuSourceStartNs=gGpuSourceStartNs;
    f.gpuTimingActive=1;f.timestampPending=1;f.gpuProfile.Reset(true);
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
    beginGpuProfilePass(Scene);renderPassOpen=true;
    beginGpuProfilePass(World);countGpuProfileDraw();beginGpuProfilePass(Fading);countGpuProfileDraw();countGpuProfileDraw();endGpuProfilePass(Fading);endGpuProfilePass(World);
    // Logical brackets are draw-only, so tiled scene execution stays in one
    // interval spanning the native render pass, including resolve/store.
    assert(writes==1);renderPassOpen=false;endGpuProfilePass(Scene);
    values[4][0]=1000; // Simulate scene work deferred until render-pass end.
    Decode();Result result;assert(getGpuPassProfile(&result));
    assert(result.status==Complete&&result.frameId==8&&result.startNs==8000&&result.mask==((1u<<Scene)|(1u<<Other)));
    assert(std::fabs(result.ms[Scene]-.8f)<1e-6f&&std::fabs(result.ms[Other]-.2f)<1e-6f);
    assert(result.ms[World]==-1&&result.ms[Fading]==-1);
    assert(result.frameMs==1&&result.draws[World]==1&&result.draws[Fading]==2&&result.ms[Sky]==-1&&readCount==6);
    float frame=0;assert(getGpuFrameTimeMs(&frame)&&frame==1);
    Start(16,1);beginGpuProfilePass(Sky);endGpuProfilePass(Sky);Decode();assert(getGpuPassProfile(&result)&&result.frameId==16);
    Start();beginGpuProfilePass(Scene);endGpuProfilePass(Scene);values[2][1]=0;Decode();
    assert(getGpuPassProfile(&result)&&result.status==Unavailable&&result.mask==0&&result.ms[World]==-1);
    Start();beginGpuProfilePass(World);endGpuProfilePass(Fading);Decode();assert(getGpuPassProfile(&result)&&result.status==Unbalanced);
    Start();for(unsigned i=0;i<MaxMarkers/2+1;++i){beginGpuProfilePass(Wrist);endGpuProfilePass(Wrist);}Decode();
    assert(writes==MaxMarkers&&getGpuPassProfile(&result)&&result.status==Overflow);
    Start();beginGpuProfilePass(World);const unsigned before=writes;setGpuFrameTimingEnabled(0);endGpuProfilePass(World);countGpuProfileDraw();
    assert(writes==before&&!getGpuPassProfile(&result));
    setGpuFrameTimingEnabled(1);Decode();assert(!getGpuPassProfile(&result)); // Old session slot cannot leak.
    Start();gvk.frames[0].gpuProfile.sampled=false;beginGpuProfilePass(World);countGpuProfileDraw();endGpuProfilePass(World);Decode();
    assert(writes==0&&readCount==2&&!getGpuPassProfile(&result));
    Start();beginGpuProfilePass(Scene);endGpuProfilePass(Scene);values[0][0]=250;values[2][0]=252;values[4][0]=5;values[1][0]=20;gvk.frameTimestampValidBits=8;Decode();
    assert(getGpuPassProfile(&result)&&result.status==Complete&&std::fabs(result.frameMs-.026f)<1e-6f&&std::fabs(result.ms[Scene]-.009f)<1e-6f);
    assert(Milliseconds(0,0,64,1)==0&&Milliseconds(0,1,0,1)==-1&&Milliseconds(0,1,65,1)==-1);
    Start();beginGpuProfilePass(World);beginGpuProfilePass(World);countGpuProfileDraw();endGpuProfilePass(World);endGpuProfilePass(World);Decode();
    assert(getGpuPassProfile(&result)&&result.status==Complete&&result.draws[World]==1&&writes==0&&result.ms[World]==-1);
    Start();for(unsigned i=0;i<StackSize+1;++i)beginGpuProfilePass(World);Decode();assert(getGpuPassProfile(&result)&&result.status==Overflow);
    Start();beginGpuProfilePass(Postprocess);beginGpuProfilePass(Scope);countGpuProfileDraw();endGpuProfilePass(Scope);endGpuProfilePass(Postprocess);Decode();
    assert(getGpuPassProfile(&result)&&result.status==Complete&&writes==4);
    assert(std::fabs(result.ms[Postprocess]-.2f)<1e-6f&&std::fabs(result.ms[Scope]-.1f)<1e-6f);
    float sum=0;for(unsigned i=0;i<Count;++i)if(result.mask&(1u<<i))sum+=result.ms[i];
    assert(std::fabs(sum-result.frameMs)<1e-6f&&result.draws[Scope]==1);
    Start();beginGpuProfilePass(Scene);beginGpuProfilePass(Fading);
    assert(gpuProfileSampling());
    assert(beginGpuProfileEntity(10,0,"road,\"name",DrawProfile::Distance));
    countGpuProfileDraw(12,3,DrawProfile::Mesh|DrawProfile::HasAlpha|DrawProfile::Ps2);
    countGpuProfileDraw(12,3,DrawProfile::Extra|DrawProfile::HasAlpha|DrawProfile::Ps2);
    beginGpuProfileEntity(20,1,"car",0);countGpuProfileDraw(8,4,DrawProfile::Mesh|DrawProfile::Skin);endGpuProfileEntity();
    countGpuProfileDraw(6,3,DrawProfile::Mesh);endGpuProfileEntity();
    endGpuProfilePass(Fading);endGpuProfilePass(Scene);Decode();
    assert(getGpuPassProfile(&result)&&result.status==Complete);
    const auto savedGeometry=result.geometry;
    const auto &details=*savedGeometry;
    assert(details.modelCount==2&&details.overflowDraws==0);
    assert(details.passes[Fading][DrawProfile::Draws]==4&&result.draws[Fading]==4);
    assert(details.passes[Fading][DrawProfile::Triangles]==16);
    assert(details.passes[Fading][DrawProfile::MeshPasses]==3&&details.passes[Fading][DrawProfile::ExtraSplit]==1);
    assert(details.models[0].counters[DrawProfile::DistanceFade]==3&&details.models[1].counters[DrawProfile::DistanceFade]==0);
    assert(std::strcmp(details.models[0].entity.name,"road__name")==0);
    // Census exceeds the old 512 cap and forces multiple transactional
    // rehashes, with the same owner appearing in three logical phases.
    DrawProfile::Recording bounded;bounded.Reset();
    const unsigned ownerCount=DrawProfile::InitialModels+257;
    for(unsigned i=0;i<ownerCount;++i){
        for(unsigned phase:{unsigned(World),unsigned(Fading),unsigned(Effects)}){
            bounded.Begin(DrawProfile::Entity(int(i),int(i%5),"model",0));
            bounded.Draw(phase,3,3,DrawProfile::Mesh);bounded.End();
        }
    }
    assert(bounded.stats.modelCount==ownerCount*3&&bounded.stats.overflowDraws==0);
    assert(bounded.stats.models.size()==bounded.stats.modelCount&&bounded.stats.tableGrowths>=3);
    assert(bounded.stats.passes[World][DrawProfile::Draws]==ownerCount);
    for(const auto &model:bounded.stats.models){
        assert(model.counters[DrawProfile::Draws]==1&&model.counters[DrawProfile::Triangles]==1);
    }
    bounded.Begin(DrawProfile::Entity(0,0,"model",0));bounded.Draw(World,6,3,DrawProfile::Extra);bounded.End();
    assert(bounded.stats.models[0].counters[DrawProfile::Draws]==2&&bounded.stats.models[0].counters[DrawProfile::Triangles]==3);
    const auto retained=bounded.stats.models.data();const unsigned capacity=bounded.stats.modelCapacity;
    bounded.Reset();assert(bounded.stats.models.data()==retained&&bounded.stats.modelCapacity==capacity);
    assert(bounded.stats.modelCount==0&&bounded.stats.tableGrowths==0&&bounded.stats.overflowDraws==0);
    bounded.Begin(DrawProfile::Entity(0,0,"model",0));bounded.Draw(World,3,3,0);bounded.End();
    assert(bounded.stats.models[0].counters[DrawProfile::Draws]==1); // no previous-frame counters
    // Exhaustion keeps existing owners and complete phase totals intact.
    DrawProfile::Recording failed;failed.Reset();
    for(unsigned i=0;i<DrawProfile::InitialModels;++i){
        failed.Begin(DrawProfile::Entity(int(i),0,"model",0));failed.Draw(World,3,3,0);failed.End();
    }
    assert(!failed.Reserve(std::numeric_limits<size_t>::max()));
    failed.Begin(DrawProfile::Entity(0,0,"model",0));failed.Draw(World,3,3,0);failed.End();
    failed.Begin(DrawProfile::Entity(int(DrawProfile::InitialModels),0,"new",0));failed.Draw(World,3,3,0);failed.End();
    assert(failed.stats.allocationFailures==1&&failed.stats.overflowDraws==1);
    assert(failed.stats.modelCount==DrawProfile::InitialModels&&failed.stats.models[0].counters[DrawProfile::Draws]==2);
    assert(failed.stats.passes[World][DrawProfile::Draws]==DrawProfile::InitialModels+2);
    assert(savedGeometry->models[0].counters[DrawProfile::Draws]==3); // immutable result survives recorder growth/reset
    for(unsigned i=0;i<DrawProfile::ContextDepth+2;++i)bounded.Begin(DrawProfile::Entity(int(i),0,"nested",0));
    assert(bounded.entity.model==-1&&bounded.stats.contextOverflows==2);
    bounded.End();bounded.End();assert(bounded.entity.model==int(DrawProfile::ContextDepth-1));
    for(unsigned i=0;i<DrawProfile::ContextDepth;++i)bounded.End();
    assert(bounded.depth==0&&bounded.entity.model==-1);
    Start();gvk.frames[0].gpuProfile.sampled=false;
    assert(!gpuProfileSampling()&&!beginGpuProfileEntity(1,0,"disabled",0));
    countGpuProfileDraw(900,3,DrawProfile::Mesh);endGpuProfileEntity();
    assert(gvk.frames[0].gpuProfile.geometry.stats.modelCount==0&&writes==0);
    std::puts("PASS: GPU pass production hooks/readback, stereo query bounds, nested/repeated passes, draw attribution, availability, wrapping, overflow, disabled/sampled paths and session isolation.");
}
