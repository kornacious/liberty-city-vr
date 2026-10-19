// Executes the production cache, raster destruction and fence retirement code.
// Vulkan mocks reject descriptor rewrites and frees while a submission uses it.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <vector>

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
static std::unordered_map<VkDescriptorSet, MockDescriptor> descriptors;
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
    descriptors.emplace(*set, MockDescriptor{});
    return VK_SUCCESS;
}
static void vkUpdateDescriptorSets(uint64_t, uint32 count, const VkWriteDescriptorSet *writes, uint32, const void *) {
    for(uint32 i = 0; i < count; ++i) {
        MockDescriptor &d = descriptors.at(writes[i].dstSet);
        Check(d.writes == 0 && !d.freed, "never rewrite a recorded or cached descriptor");
        ++d.writes;
        d.view = writes[i].pImageInfo->imageView;
        d.sampler = writes[i].pImageInfo->sampler;
    }
}
static int vkFreeDescriptorSets(uint64_t, uint64_t, uint32 count, const VkDescriptorSet *sets) {
    for(uint32 i = 0; i < count; ++i) {
        MockDescriptor &d = descriptors.at(sets[i]);
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
    for(const auto &entry : descriptors)
        Check(entry.second.view != view || entry.second.pendingMask == 0,
            "view survives every pending descriptor user");
}
static void vkDestroyImage(uint64_t, VkImage, const void *) {}
static void vkUnmapMemory(uint64_t, VkDeviceMemory) {}
static void cancelGeneratedMips(VulkanRaster *) {}
#include "vulkan-descriptors-production.inc"

static void Use(VkDescriptorSet set, uint32 slot) {
    Check(descriptors.at(set).writes == 1 && !descriptors.at(set).freed, "bind a live initialized set");
    descriptors.at(set).pendingMask |= 1u << slot;
}
static void Complete(uint32 slot) {
    for(auto &entry : descriptors) entry.second.pendingMask &= ~(1u << slot);
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
    Check(descriptors.size() == 2, "repeated sampler switches do not grow the pool");
    const VkDescriptorSet edge = nextSet;
    gstate.textureAddressV = 3;
    Check(getTextureDescriptor(&raster) == edge, "V address mode has its own descriptor");
    gstate.textureAddressV = 1;
    gvk.activeFrame = 1;
    Use(getTextureDescriptor(&raster), 1);
    Check(descriptors.size() == 3, "frame slots share immutable descriptor variants");

    // Production mip completion retires the old cache before changing views.
    retireRasterTextureDescriptors(&raster.native);
    retireImage(raster.native.view, 0, 0);
    raster.native.view = 11;
    const VkDescriptorSet replacement = getTextureDescriptor(&raster);
    Check(replacement != linear && descriptors.at(replacement).view == 11,
        "replacement view gets a new immutable descriptor");
    Check(!descriptors.at(linear).freed, "old view descriptors survive invalidation");
    Complete(0);
    Check(!descriptors.at(linear).freed, "the other slot can still use the retired set");
    Complete(1);
    Check(descriptors.at(linear).freed, "last ordered submission frees retired variants");
    Use(replacement, 1);
    gvk.inFrame = 0;
    gvk.hasSubmittedFrame = 1;
    gvk.lastSubmittedFrame = 1;
    destroyNativeRaster(&raster, 0, 0);
    Check(textureDescriptors.empty(), "raster destruction erases CPU cache entries");
    Check(!descriptors.at(replacement).freed, "outside-frame eviction waits for last submission");
    Complete(0);
    Check(!descriptors.at(replacement).freed, "unrelated fence cannot free an evicted set");
    Complete(1);
    Check(descriptors.at(replacement).freed, "raster eviction returns all sets to pool");

    // Recycle the same raster address repeatedly; no stale hits or pool leaks.
    gvk.inFrame = 1;
    for(int i = 0; i < 2000; ++i) {
        gvk.activeFrame = uint32(i % NUM_FRAME_CONTEXTS);
        raster.native.view = 12;
        const VkDescriptorSet set = getTextureDescriptor(&raster);
        Use(set, gvk.activeFrame);
        destroyNativeRaster(&raster, 0, 0);
        Complete(gvk.activeFrame);
        Check(descriptors.at(set).freed && textureDescriptors.empty(), "streaming eviction has no cache/set leak");
    }
    raster.native.view = 13;
    allocationFails = true;
    Check(getTextureDescriptor(&raster) == gs.whiteDescriptor, "pool failure uses safe fallback");
    allocationFails = false;
    const VkDescriptorSet recovered = getTextureDescriptor(&raster);
    Check(recovered != gs.whiteDescriptor && descriptors.at(recovered).writes == 1,
        "failed allocation is not cached and can recover");
    gvk.inFrame = 0;
    gvk.hasSubmittedFrame = 0;
    destroyNativeRaster(&raster, 0, 0);
    Check(descriptors.at(recovered).freed, "startup eviction frees an unused set immediately");
    for(const auto &entry : descriptors) Check(entry.second.freed, "all retired texture sets returned");
    Check(getTextureDescriptor(nil) == gs.whiteDescriptor, "untextured draw uses white");
    Check(getTextureDescriptor(&raster) == gs.whiteDescriptor, "raster without a view uses white");
    raster.native.view = 14;
    (void)getTextureDescriptor(&raster);
    clearTextureDescriptorCache();
    Check(textureDescriptors.empty(), "device shutdown releases CPU descriptor cache");
    // Pool destruction owns all remaining Vulkan descriptor allocations.
    descriptors.clear();
    std::printf("PASS: immutable Vulkan descriptors, sampler variants, view replacement, two-slot fences, eviction and failure recovery: %u checks\n", checks);
}
