#pragma once
#include <cstdint>
#include <array>

namespace StaticArrayBatch {
constexpr unsigned MaxPages=32;
constexpr unsigned OccupancySampleInterval=8;
constexpr uint32_t SharedPageSamplerKey=512;
constexpr unsigned CrossObjectLimit=8;
constexpr unsigned CrossObjectPageSize=128;
constexpr unsigned DemandMinFrames=3;
constexpr uint64_t DemandMaxGap=24;
constexpr uint64_t DemandColdFrames=240;
struct DemandHistory {
    uint64_t lastFrame;
    uint32_t frames;
    bool Observe(uint64_t frame){
        if(lastFrame==frame) return false;
        if(!lastFrame || frame-lastFrame>DemandMaxGap) frames=0;
        lastFrame=frame;
        if(frames<DemandMinFrames) ++frames;
        return frames==DemandMinFrames;
    }
};
inline bool CanReplaceLayer(uint64_t lastReference,uint64_t frame){
    return frame>lastReference && frame-lastReference>=DemandColdFrames;
}
struct CrossObjectCursor {
    uint32_t pageOffset=0, nextObject=0;
    bool Allocate(unsigned count,uint32_t stride,uint32_t capacity,
                  uint32_t &offset,uint32_t &first){
        if(!count || count>CrossObjectLimit) return false;
        if(count>CrossObjectPageSize-nextObject){
            if(capacity-pageOffset<2*stride) return false;
            pageOffset+=stride;
            nextObject=0;
        }
        if(capacity-pageOffset<stride) return false;
        offset=pageOffset;
        first=nextObject;
        nextObject+=count;
        return true;
    }
};
template<class Pipeline,class Descriptor>
struct CrossArrayBindings {
    enum : unsigned { BindPipeline=1, BindTexture=2, BindTransforms=4 };
    Pipeline pipeline{};
    Descriptor texture{},transforms{};
    uint32_t offset=0;
    bool known=false;

    unsigned Update(Pipeline nextPipeline,Descriptor nextTexture,
                    Descriptor nextTransforms,uint32_t nextOffset){
        unsigned changes=0;
        if(!known || pipeline!=nextPipeline) changes|=BindPipeline;
        if(!known || texture!=nextTexture) changes|=BindTexture;
        if(!known || transforms!=nextTransforms || offset!=nextOffset) changes|=BindTransforms;
        pipeline=nextPipeline;
        texture=nextTexture;
        transforms=nextTransforms;
        offset=nextOffset;
        known=true;
        return changes;
    }
    void Invalidate(){known=false;}
};
using CrossGeometryKey=std::array<uint64_t,CrossObjectLimit>;
inline bool UsesCrossGeometry(const CrossGeometryKey &key,uint64_t generation){
    if(!generation) return false;
    for(auto id:key) if(id==generation) return true;
    return false;
}
constexpr uint32_t EncodeCrossLayer(uint32_t layer,uint32_t object){
    return layer|(object<<6);
}
inline void OffsetCrossIndices(uint32_t *indices,uint32_t count,uint32_t vertexBase){
    for(uint32_t i=0;i<count;++i) indices[i]+=vertexBase;
}
constexpr uint32_t InitialPageLayers=8;
inline uint32_t PageLayerTarget(uint32_t largestPage,uint32_t limit){
    // A group only grows when none of its existing pages has a reusable layer.
    if(largestPage>=limit) return limit;
    const uint32_t next=largestPage>limit/2?limit:largestPage*2;
    const uint32_t target=next<InitialPageLayers?InitialPageLayers:next;
    return target<limit?target:limit;
}

struct PageOccupancy {
    uint32_t width=0,height=0,samplerKey=0;
    uint32_t layers=0,live=0,held=0,bytes=0;
    uint32_t duplicateLayers=0;
};
struct Occupancy {
    bool sampled=false;
    unsigned count=0;
    PageOccupancy pages[MaxPages]={};
};

enum class Result : unsigned {
    PipelineUnavailable, PreferredPageFull, PageCountLimit, PageMemoryLimit,
    PageCreationBudget, LayerDeviceLimit, TextureUploadBudget, PageAllocation,
    TextureUpload, GeometryBuildBudget, GeometryMemoryLimit, GeometryInput,
    GeometryUpload, HostAllocation, SmallTextureReserve, Ready
};
constexpr unsigned FallbackCount=unsigned(Result::Ready);
inline const char *FallbackName(unsigned index){
    constexpr const char *names[]={
        "pipeline_unavailable","preferred_page_full","page_count_limit","page_memory_limit",
        "page_creation_budget","layer_device_limit","texture_upload_budget","page_allocation",
        "texture_upload","geometry_build_budget","geometry_memory_limit","geometry_input",
        "geometry_upload","host_allocation","small_texture_reserve"
    };
    static_assert(sizeof(names)/sizeof(names[0])==FallbackCount,"fallback schema");
    return names[index];
}

constexpr unsigned MaxResidencyEvents=48;
constexpr unsigned ResidentOnlyMiss=unsigned(Result::Ready)+1;
inline const char *ResidencyOutcomeName(unsigned outcome){
    if(outcome==unsigned(Result::Ready)) return "uploaded";
    if(outcome==ResidentOnlyMiss) return "resident_only";
    return FallbackName(outcome);
}
struct ResidencyEvent {
    uint16_t width=0,height=0;
    uint8_t outcome=0;
    bool elsewhere=false;
    uint32_t requests=0;
};
struct ResidencyStats {
    bool sampled=false;
    unsigned count=0;
    uint32_t overflow=0;
    ResidencyEvent events[MaxResidencyEvents]={};

    void Observe(uint32_t width,uint32_t height,bool elsewhere,unsigned outcome){
        for(unsigned i=0;i<count;++i){
            auto &event=events[i];
            if(event.width==width && event.height==height &&
               event.elsewhere==elsewhere && event.outcome==outcome){
                ++event.requests;
                return;
            }
        }
        if(count==MaxResidencyEvents){
            ++overflow;
            return;
        }
        events[count++]={uint16_t(width),uint16_t(height),uint8_t(outcome),elsewhere,1};
    }
};

struct TexturePageBudget {
    static constexpr uint32_t PoolBytes=24*1024*1024;
    static constexpr uint32_t OtherBytes=10*1024*1024;
    static constexpr unsigned OtherPages=MaxPages-17;
    uint32_t bytes=0,otherBytes=0;
    unsigned otherPages=0;

    static bool IsSmall(uint32_t width,uint32_t height){
        return width<=64 && height<=64;
    }
    Result CheckPageCount(bool small,unsigned pageCount) const {
        if(pageCount>=MaxPages) return Result::PageCountLimit;
        if(!small && otherPages>=OtherPages) return Result::SmallTextureReserve;
        return Result::Ready;
    }
    uint32_t Remaining(bool small) const {
        const uint32_t global=PoolBytes-bytes;
        const uint32_t other=OtherBytes-otherBytes;
        return small || global<other?global:other;
    }
    Result MemoryFailure(uint64_t required) const {
        return required>PoolBytes-bytes?Result::PageMemoryLimit:Result::SmallTextureReserve;
    }
    void Add(bool small,uint32_t allocation){
        bytes+=allocation;
        if(!small){
            otherBytes+=allocation;
            ++otherPages;
        }
    }
};
struct Stats {
    uint32_t batches=0,savedDraws=0,indices=0,cacheBuilds=0,fallbacks=0;
    uint32_t pageBytes=0,pageUploadBytes=0,geometryBytes=0,geometryUploadBytes=0;
    uint32_t shortenedBatches=0;
    uint32_t pageSizeRetries=0,pageMemoryCacheHits=0;
    uint32_t crossBatches=0,crossSavedDraws=0,crossUploadBytes=0,crossFallbacks=0;
    uint32_t crossCacheHits=0,crossCacheMisses=0,crossCacheBuilds=0;
    uint32_t crossCacheBytes=0,crossCacheUploadBytes=0;
    uint32_t crossPipelineBinds=0,crossDescriptorBinds=0,crossTransformBinds=0;
    uint32_t duplicateMergedLayers=0;
    uint32_t demandProbes=0,demandReplacements=0,demandUploadBytes=0;
    double crossFlushMs=0;
    uint32_t fallbackReasons[FallbackCount]={};
    unsigned Fallback(Result reason){
        ++fallbacks;
        ++fallbackReasons[unsigned(reason)];
        return 0;
    }
};

// Retry only a measured budget mismatch. Preserve driver failures and always
// test the minimum useful page before ruling out this image shape.
template<class Probe>
Result FitPageLayers(uint32_t &layers,Probe probe){
    for(;;){
        const auto result=probe(layers);
        if(result!=Result::PageMemoryLimit || layers<=2) return result;
        layers=layers>4?layers/2:2;
    }
}

// Each material range owns its vertex attributes. Reuse source vertices
// inside that range and append its indices in the exact source order.
template<class AddVertex>
bool RemapRange(const uint16_t *source,uint32_t count,uint32_t *remap,
                uint32_t sourceVertices,uint32_t firstVertex,uint32_t *output,AddVertex addVertex)
{
    for(uint32_t i=0;i<count;++i)
        if(source[i]>=sourceVertices) return false;
    for(uint32_t i=0;i<count;++i){
        const uint16_t original=source[i];
        auto &mapped=remap[original];
        if(mapped==UINT32_MAX || mapped<firstVertex) mapped=addVertex(original);
        output[i]=mapped;
    }
    return true;
}
}
#include <limits>

namespace OpaqueMeshBatch {
// Concatenate triangle-list ranges only. Strip/fan boundaries would create
// extra primitives. The bound limits work without changing visibility units.
constexpr uint32_t MaxMeshes=32;
struct Stats {
    uint32_t eligible=0,checks=0,merged=0,savedDraws=0;
    uint32_t rejectedState=0,rejectedMaterial=0,rejectedRange=0;
};
inline bool CanAppend(uint64_t firstByte,uint32_t count,uint64_t nextByte,
                      uint32_t nextCount,uint32_t meshes){
    return meshes<MaxMeshes && count && nextCount && !(firstByte%2) &&
        !(nextByte%2) && !(count%3) && !(nextCount%3) &&
        firstByte/2<=std::numeric_limits<uint32_t>::max() &&
        nextByte/2<=std::numeric_limits<uint32_t>::max() &&
        firstByte<=std::numeric_limits<uint64_t>::max()-uint64_t(count)*2 &&
        firstByte+uint64_t(count)*2==nextByte &&
        nextCount<=std::numeric_limits<uint32_t>::max()-count;
}
}
