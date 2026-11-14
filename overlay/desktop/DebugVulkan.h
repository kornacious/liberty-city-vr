#pragma once
#include "android.h"
#include "RenderDebugCore.h"
namespace RenderDebug {
class Vulkan {
public:
    androidgame::VulkanContext context{};
    VkImage image{};VkImageView view{};VkDeviceMemory memory{};
    std::string deviceName;
    void Open(const Settings &settings);
    void Close();
    ~Vulkan(){Close();}
};
}
