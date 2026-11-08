#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
#include <cstddef>
#include <initializer_list>

// re3 shades and blends display-encoded values. Keep those bytes unchanged
// through an UNORM attachment, but identify them as sRGB to the compositor.
// The OpenXR image must be requested with MUTABLE_FORMAT usage for this view.
namespace LegacyColour {
inline VkFormat attachmentFormat(VkFormat swapchain) {
    if(swapchain == VK_FORMAT_R8G8B8A8_SRGB) return VK_FORMAT_R8G8B8A8_UNORM;
    if(swapchain == VK_FORMAT_B8G8R8A8_SRGB) return VK_FORMAT_B8G8R8A8_UNORM;
    return swapchain;
}
inline VkFormat selectSrgb(const int64_t *formats, size_t count) {
    for(auto wanted : {VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_B8G8R8A8_SRGB})
        for(size_t i=0; i<count; ++i)
            if(formats[i] == wanted) return wanted;
    return VK_FORMAT_UNDEFINED;
}
}
