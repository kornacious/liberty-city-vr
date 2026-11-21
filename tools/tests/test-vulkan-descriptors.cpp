// Executes the production cache, raster destruction and fence retirement code.
// Vulkan mocks reject descriptor rewrites and frees while a submission uses it.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <unordered_map>
#include <vector>
#include "../../librw/src/vulkan/OpaqueMeshBatch.h"

using uint8 = uint8_t;
using uint32 = uint32_t;
using int32 = int32_t;
using bool32 = int;
using VkImage = uint64_t;
using VkImageView = uint64_t;
using VkBuffer = uint64_t;
using VkDeviceMemory = uint64_t;
using VkDeviceSize = uint64_t;
using VkDescriptorSet = uint64_t;
using VkCommandBuffer = uint64_t;
using VkFormat = uint32;
using VkImageLayout = uint32;
enum {
    NUM_FRAME_CONTEXTS = 2, MAX_TEXTURE_DESCRIPTORS = 8192,
    VK_NULL_HANDLE = 0, VK_SUCCESS = 0,
    VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO = 1,
    VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET = 2,
    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER = 3,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL = 4
};
#define nil nullptr
#define nelem(a) (sizeof(a)/sizeof((a)[0]))
#define VKERR(...) ((void)0)
#include "vulkan-descriptors-types.inc"

struct Raster { VulkanRaster native = {}; int width = 64, height = 64; };
// Offset is deliberately consumed to keep the production plugin ABI exercised.
#define PLUGINOFFSET(type, object, offset) \
    ((void)(offset), &static_cast<Raster*>(object)->native)
static const int32 nativeRasterOffset = 0;
static size_t gTextureMemoryUsed;
static struct {
    uint64_t device = 1, commandPool = 1;
    bool32 initialised = 1, inFrame = 1, hasSubmittedFrame = 0;
    uint32 activeFrame = 0, lastSubmittedFrame = 0;
    Raster *envRaster = nil;
} gvk;
static struct {
    VkDescriptorSet whiteDescriptor = 999999;
    uint64_t descriptorPool = 1, textureLayout = 1;
} gs;
static struct {
    uint32 textureFilter = 2, textureAddressU = 1, textureAddressV = 1, mipLodBias = 0;
} gstate;
struct VkDescriptorSetAllocateInfo {
    uint32 sType;
    uint64_t descriptorPool;
    uint32 descriptorSetCount;
    const uint64_t *pSetLayouts;
};
struct VkDescriptorImageInfo { uint64_t sampler; VkImageView imageView; VkImageLayout imageLayout; };
struct VkWriteDescriptorSet {
    uint32 sType;
    VkDescriptorSet dstSet;
    uint32 dstBinding, descriptorCount, descriptorType;
    const VkDescriptorImageInfo *pImageInfo;
};
struct MockDescriptor {
    uint32 writes = 0, pendingMask = 0;
    bool freed = false;
    VkImageView view = 0;
    uint64_t sampler = 0;
};
static std::unordered_map<VkDescriptorSet, MockDescriptor> mockDescriptors;
static VkDescriptorSet nextSet = 1;
static bool allocationFails;
static unsigned checks;
static void Check(bool value, const char *message) {
    ++checks;
    if(!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
static int vkAllocateDescriptorSets(uint64_t, const VkDescriptorSetAllocateInfo *info, VkDescriptorSet *set) {
    Check(info->descriptorSetCount == 1, "one immutable texture set per allocation");
    if(allocationFails) return -1;
    *set = nextSet++;
    mockDescriptors.emplace(*set, MockDescriptor{});
    return VK_SUCCESS;
}
static void vkUpdateDescriptorSets(uint64_t, uint32 count, const VkWriteDescriptorSet *writes, uint32, const void *) {
    for(uint32 i = 0; i < count; ++i) {
        MockDescriptor &d = mockDescriptors.at(writes[i].dstSet);
        Check(d.writes == 0 && !d.freed, "never rewrite a recorded or cached descriptor");
        ++d.writes;
        d.view = writes[i].pImageInfo->imageView;
        d.sampler = writes[i].pImageInfo->sampler;
    }
}
static int vkFreeDescriptorSets(uint64_t, uint64_t, uint32 count, const VkDescriptorSet *sets) {
    for(uint32 i = 0; i < count; ++i) {
        MockDescriptor &d = mockDescriptors.at(sets[i]);
        Check(d.pendingMask == 0 && !d.freed, "free only after every using submission completes");
        d.freed = true;
    }
    return VK_SUCCESS;
}
static uint64_t getSampler(uint32 key, uint32, uint32, uint32) { return key+1; }
static void vkFreeCommandBuffers(uint64_t, uint64_t, uint32, const VkCommandBuffer *) {}
static void vkDestroyBuffer(uint64_t, VkBuffer, const void *) {}
static void vkFreeMemory(uint64_t, VkDeviceMemory, const void *) {}
static void vkDestroyImageView(uint64_t, VkImageView view, const void *) {
    for(const auto &entry : mockDescriptors)
        Check(entry.second.view != view || entry.second.pendingMask == 0,
            "view survives every pending descriptor user");
}
static void vkDestroyImage(uint64_t, VkImage, const void *) {}
static void vkUnmapMemory(uint64_t, VkDeviceMemory) {}
static void cancelGeneratedMips(VulkanRaster *) {}
static void flushStaticInstances() {}
static void rwFree(void *pointer) { std::free(pointer); }
static unsigned arrayInvalidations;
static void invalidateStaticArrayRaster(VulkanRaster *native) {
    if(native->staticArrayCopySource) ++arrayInvalidations;
}
struct StaticArrayTexture {
    VulkanRaster *native;
    uint32 width,height,samplerKey;
    uint64_t sampler;
};
struct StaticArrayLayer {
    VkDescriptorSet page=0;
    uint32 layer=0;
    StaticArrayBatch::Result result=StaticArrayBatch::Result::Ready;
    uint32 pageIndex=StaticArrayBatch::MaxPages;
    bool duplicate=false;
};
struct StaticTexturePage {
    VkImageView view=0;
    uint32 width=64,height=64,layers=2;
    struct Slot { VulkanRaster *owner=nullptr; uint64_t releasedFrame=0,lastReference=0; } slots[2];
    struct Descriptor { uint32 key; VkDescriptorSet set; };
    std::vector<Descriptor> descriptors;
};
struct LayerLocation { uint32 page,layer; };
static std::vector<StaticTexturePage> staticArrayPages;
static std::unordered_map<VulkanRaster*,std::vector<LayerLocation>> staticArrayLocations;
static uint64_t staticArrayFrame=1;
static constexpr size_t StaticArrayUploadLimit=512*1024;
static unsigned staticArrayUploads=0;
static size_t staticArrayUploadBytes=0;
static StaticArrayBatch::ResidencyStats staticArrayResidency;
static uint32 staticArrayDemandProbes=0,staticArrayDemandReplacements=0,staticArrayDemandUploadBytes=0;
static bool arrayUploadFails=false;
static StaticArrayBatch::Result createStaticArrayPage(const StaticArrayTexture &) {
    return StaticArrayBatch::Result::PageCountLimit;
}
static bool uploadStaticArrayLayer(const StaticArrayTexture &,StaticTexturePage &,uint32) {
    return !arrayUploadFails;
}
StaticArrayLayer getStaticArrayLayer(const StaticArrayTexture &,uint32,bool residentOnly=false,
                                     const StaticArrayTexture *companion=nullptr);
#include "vulkan-descriptors-production.inc"

static void Use(VkDescriptorSet set, uint32 slot) {
    Check(mockDescriptors.at(set).writes == 1 && !mockDescriptors.at(set).freed, "bind a live initialized set");
    mockDescriptors.at(set).pendingMask |= 1u << slot;
}
static void Complete(uint32 slot) {
    for(auto &entry : mockDescriptors) entry.second.pendingMask &= ~(1u << slot);
    flushRetired(slot);
}
int main() {
    Raster raster;
    raster.native.view = 10;
    const VkDescriptorSet linear = getTextureDescriptor(&raster);
    Use(linear, 0);
    for(int i = 0; i < 500; ++i) {
        gstate.textureFilter = 6;
        gstate.textureAddressU = 3;
        gstate.mipLodBias = 3;
        const VkDescriptorSet mask = getTextureDescriptor(&raster);
        Check(mask != linear, "a different sampler needs a different descriptor");
        Use(mask, 0);
        gstate.textureFilter = 2;
        gstate.textureAddressU = 1;
        gstate.mipLodBias = 0;
        Check(getTextureDescriptor(&raster) == linear, "A/B/A draws reuse the immutable A set");
    }
    Check(mockDescriptors.size() == 2, "repeated sampler switches do not grow the pool");
    const VkDescriptorSet edge = nextSet;
    gstate.textureAddressV = 3;
    Check(getTextureDescriptor(&raster) == edge, "V address mode has its own descriptor");
    gstate.textureAddressV = 1;
    gvk.activeFrame = 1;
    Use(getTextureDescriptor(&raster), 1);
    Check(mockDescriptors.size() == 3, "frame slots share immutable descriptor variants");

    // Production mip completion retires the old cache before changing views.
    retireRasterTextureDescriptors(&raster.native);
    retireImage(raster.native.view, 0, 0);
    raster.native.view = 11;
    const VkDescriptorSet replacement = getTextureDescriptor(&raster);
    Check(replacement != linear && mockDescriptors.at(replacement).view == 11,
        "replacement view gets a new immutable descriptor");
    Check(!mockDescriptors.at(linear).freed, "old view descriptors survive invalidation");
    Complete(0);
    Check(!mockDescriptors.at(linear).freed, "the other slot can still use the retired set");
    Complete(1);
    Check(mockDescriptors.at(linear).freed, "last ordered submission frees retired variants");
    Use(replacement, 1);
    gvk.inFrame = 0;
    gvk.hasSubmittedFrame = 1;
    gvk.lastSubmittedFrame = 1;
    raster.native.staticArrayCopySource=1;
    destroyNativeRaster(&raster, 0, 0);
    Check(arrayInvalidations==1,"raster destruction invalidates resident array layers");
    Check(textureDescriptors.empty(), "raster destruction erases CPU cache entries");
    Check(!mockDescriptors.at(replacement).freed, "outside-frame eviction waits for last submission");
    Complete(0);
    Check(!mockDescriptors.at(replacement).freed, "unrelated fence cannot free an evicted set");
    Complete(1);
    Check(mockDescriptors.at(replacement).freed, "raster eviction returns all sets to pool");

    // Recycle the same raster address repeatedly; no stale hits or pool leaks.
    gvk.inFrame = 1;
    for(int i = 0; i < 2000; ++i) {
        gvk.activeFrame = uint32(i % NUM_FRAME_CONTEXTS);
        raster.native.view = 12;
        const VkDescriptorSet set = getTextureDescriptor(&raster);
        Use(set, gvk.activeFrame);
        destroyNativeRaster(&raster, 0, 0);
        Complete(gvk.activeFrame);
        Check(mockDescriptors.at(set).freed && textureDescriptors.empty(), "streaming eviction has no cache/set leak");
    }
    raster.native.view = 13;
    allocationFails = true;
    Check(getTextureDescriptor(&raster) == gs.whiteDescriptor, "pool failure uses safe fallback");
    allocationFails = false;
    const VkDescriptorSet recovered = getTextureDescriptor(&raster);
    Check(recovered != gs.whiteDescriptor && mockDescriptors.at(recovered).writes == 1,
        "failed allocation is not cached and can recover");
    gvk.inFrame = 0;
    gvk.hasSubmittedFrame = 0;
    destroyNativeRaster(&raster, 0, 0);
    Check(mockDescriptors.at(recovered).freed, "startup eviction frees an unused set immediately");
    for(const auto &entry : mockDescriptors) Check(entry.second.freed, "all retired texture sets returned");
    Check(getTextureDescriptor(nil) == gs.whiteDescriptor, "untextured draw uses white");
    Check(getTextureDescriptor(&raster) == gs.whiteDescriptor, "raster without a view uses white");
    raster.native.view = 14;
    (void)getTextureDescriptor(&raster);
    clearTextureDescriptorCache();
    Check(textureDescriptors.empty(), "device shutdown releases CPU descriptor cache");
    {
        auto &page=staticArrayPages.emplace_back();
        page.view=20;
        StaticArrayTexture linearTexture{&raster.native,64,64,16,17};
        Check(!getStaticArrayLayer(linearTexture,StaticArrayBatch::MaxPages,true).page && staticArrayUploads==0,
              "resident-only lookup never enrolls a texture");
        const auto first=getStaticArrayLayer(linearTexture,StaticArrayBatch::MaxPages);
        auto alternate=linearTexture;
        alternate.samplerKey=80;
        alternate.sampler=81;
        Use(first.page,0);
        allocationFails=true;
        Check(getStaticArrayLayer(alternate,StaticArrayBatch::MaxPages).result==StaticArrayBatch::Result::PageAllocation,
              "failed sampler descriptor leaves the resident layer intact");
        allocationFails=false;
        const auto second=getStaticArrayLayer(alternate,StaticArrayBatch::MaxPages);
        Check(getStaticArrayLayer(alternate,StaticArrayBatch::MaxPages,true).layer==first.layer &&
              staticArrayUploads==1,"resident-only lookup returns the existing layer");
        Use(second.page,1);
        Check(first.page!=second.page && first.pageIndex==second.pageIndex && first.layer==second.layer &&
              staticArrayUploads==1 && staticArrayLocations[&raster.native].size()==1,
              "sampler variants share a physical page and a single uploaded layer");
        Check(mockDescriptors.at(first.page).view==page.view && mockDescriptors.at(second.page).view==page.view &&
              mockDescriptors.at(first.page).sampler==17 && mockDescriptors.at(second.page).sampler==81,
              "each immutable descriptor keeps its effective sampler");
        Check(getStaticArrayLayer(linearTexture,second.pageIndex).page==first.page,
              "preferred physical page can resolve either sampler descriptor");
        alternate.width=128;
        Check(getStaticArrayLayer(alternate,second.pageIndex).result==StaticArrayBatch::Result::PreferredPageFull,
              "an incompatible texture shape cannot reuse a page");
        staticArrayResidency.sampled=true;
        auto &otherPage=staticArrayPages.emplace_back();
        otherPage.view=21;
        const auto other=getStaticArrayLayer(linearTexture,1);
        Check(other.page && other.duplicate && staticArrayUploads==2,
              "a second physical copy is tagged when selected");
        Check(staticArrayResidency.count==1 && staticArrayResidency.events[0].elsewhere &&
              staticArrayResidency.events[0].outcome==unsigned(StaticArrayBatch::Result::Ready),
              "a miss resolved by copying an already-resident raster is classified as other-page upload");
        Check(getStaticArrayLayer(linearTexture,0).page && staticArrayResidency.count==1,
              "cache hits do not record residency misses");
        VulkanRaster missing{};
        auto missingTexture=linearTexture;
        missingTexture.native=&missing;
        Check(!getStaticArrayLayer(missingTexture,StaticArrayBatch::MaxPages,true).page &&
              staticArrayResidency.count==2 && !staticArrayResidency.events[1].elsewhere &&
              staticArrayResidency.events[1].outcome==StaticArrayBatch::ResidentOnlyMiss,
              "a missing resident-only texture is classified without an upload");
        VulkanRaster cold{};
        auto coldTexture=linearTexture;
        coldTexture.native=&cold;
        const auto coldLayer=getStaticArrayLayer(coldTexture,1);
        Check(coldLayer.page && coldLayer.layer!=other.layer,"populate the last free layer");
        staticArrayFrame+=StaticArrayBatch::DemandColdFrames;
        for(unsigned i=0;i<2;++i,++staticArrayFrame){
            Check(getStaticArrayLayer(linearTexture,1).duplicate,"refresh the in-use duplicate");
            Check(!getStaticArrayLayer(missingTexture,1).page &&
                  !getStaticArrayLayer(missingTexture,1).page,
                  "multiple misses in one frame count as one observation");
        }
        Check(getStaticArrayLayer(linearTexture,1).duplicate,"refresh the in-use duplicate");
        arrayUploadFails=true;
        Check(getStaticArrayLayer(missingTexture,1).result==StaticArrayBatch::Result::TextureUpload &&
              staticArrayLocations.count(&cold)==1 && staticArrayDemandReplacements==0,
              "a recording failure preserves the victim mapping");
        arrayUploadFails=false;
        ++staticArrayFrame;
        const auto promoted=getStaticArrayLayer(missingTexture,1);
        Check(promoted.page && promoted.layer==coldLayer.layer &&
              staticArrayLocations.count(&cold)==0 && getStaticArrayLayer(linearTexture,1).duplicate &&
              staticArrayDemandProbes==2 && staticArrayDemandReplacements==1 &&
              staticArrayDemandUploadBytes==64*64*4,
              "a repeated miss replaces only the cold layer and preserves the live duplicate");
        VulkanRaster unrelated{};
        auto unrelatedTexture=linearTexture;
        unrelatedTexture.native=&unrelated;
        Check(getStaticArrayLayer(unrelatedTexture,0).page!=0,"fill the unrelated page");
        VulkanRaster next{};
        auto nextTexture=linearTexture;
        nextTexture.native=&next;
        staticArrayFrame+=StaticArrayBatch::DemandColdFrames;
        for(unsigned i=0;i<2;++i,++staticArrayFrame)
            Check(!getStaticArrayLayer(nextTexture,StaticArrayBatch::MaxPages,false,&missingTexture).page,
                  "a new base texture must establish demand");
        const auto grouped=getStaticArrayLayer(nextTexture,StaticArrayBatch::MaxPages,false,&missingTexture);
        Check(grouped.pageIndex==1 && grouped.layer==other.layer &&
              getStaticArrayLayer(missingTexture,1).layer==promoted.layer &&
              staticArrayLocations.at(linearTexture.native).size()==1,
              "replacement selects the companion page and keeps the companion resident");
        Complete(0);
        Complete(1);
        for(auto &arrayPage:staticArrayPages)
            for(auto &descriptor:arrayPage.descriptors) freeTextureDescriptorSets(&descriptor.set,1);
    }
    // Pool destruction owns all remaining Vulkan descriptor allocations.
    mockDescriptors.clear();
    std::printf("PASS: immutable Vulkan descriptors, sampler variants, view replacement, two-slot fences, eviction and failure recovery: %u checks\n", checks);
}
