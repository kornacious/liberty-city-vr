#pragma once
#include <algorithm>
#include <cstdint>
#include <vulkan/vulkan.h>

namespace XrVulkanPolicy {
// OpenXR uses XrVersion encoding, not VK_MAKE_VERSION encoding.
inline bool supportsVulkan11(std::uint64_t minimum, std::uint64_t maximum,
                            std::uint32_t deviceVersion) {
    constexpr std::uint64_t requested = (std::uint64_t(1) << 48) | (std::uint64_t(1) << 32);
    return minimum <= requested && maximum >= requested && deviceVersion >= VK_API_VERSION_1_1;
}
inline std::uint32_t graphicsQueue(const VkQueueFamilyProperties *families, std::uint32_t count) {
    for(std::uint32_t i = 0; i < count; ++i)
        if(families[i].queueCount && (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) return i;
    return UINT32_MAX;
}
inline VkPhysicalDeviceFeatures optionalFeatures(const VkPhysicalDeviceFeatures &available) {
    VkPhysicalDeviceFeatures result{};
    result.samplerAnisotropy = available.samplerAnisotropy;
    result.fragmentStoresAndAtomics = available.fragmentStoresAndAtomics;
    // Desktop GPUs need not support ASTC/ETC2. librw queries formats and handles
    // unsupported compressed uploads; never require Quest-only compression.
    result.textureCompressionBC = available.textureCompressionBC;
    result.textureCompressionASTC_LDR = available.textureCompressionASTC_LDR;
    result.textureCompressionETC2 = available.textureCompressionETC2;
    return result;
}
}
