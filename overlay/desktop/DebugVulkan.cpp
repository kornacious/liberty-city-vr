#include "DebugVulkan.h"
#include "DesktopMirror.h"
#include "MatchedProfile.h"
#include "VulkanPolicy.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
namespace RenderDebug {
static void VkCheck(VkResult result,const char *what){if(result!=VK_SUCCESS)throw std::runtime_error(std::string(what)+" (Vulkan "+std::to_string(result)+")");}
void Vulkan::Open(const Settings &s){
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.pApplicationName="Liberty City renderer debug";app.apiVersion=VK_API_VERSION_1_1;
    unsigned count=0;auto extensions=Desktop::VulkanExtensions(count);
    VkInstanceCreateInfo instance{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};instance.pApplicationInfo=&app;instance.enabledExtensionCount=count;instance.ppEnabledExtensionNames=extensions;
    VkCheck(vkCreateInstance(&instance,nullptr,&context.instance),"Create desktop Vulkan instance");
    VkSurfaceKHR surface{};
    Check(SDL_Vulkan_CreateSurface(static_cast<SDL_Window*>(Desktop::WindowHandle()),context.instance,nullptr,&surface),"Create desktop Vulkan surface");
    try {
        VkCheck(vkEnumeratePhysicalDevices(context.instance,&count,nullptr),"Enumerate GPUs");std::vector<VkPhysicalDevice> devices(count);VkCheck(vkEnumeratePhysicalDevices(context.instance,&count,devices.data()),"Enumerate GPUs");
        int best=-1;
        for(auto device:devices){VkPhysicalDeviceProperties props;vkGetPhysicalDeviceProperties(device,&props);if(props.apiVersion<VK_API_VERSION_1_1)continue;
            uint32_t n=0;vkGetPhysicalDeviceQueueFamilyProperties(device,&n,nullptr);std::vector<VkQueueFamilyProperties> families(n);vkGetPhysicalDeviceQueueFamilyProperties(device,&n,families.data());
            VkPhysicalDeviceMultiviewFeatures multiview{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES};VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};features.pNext=&multiview;vkGetPhysicalDeviceFeatures2(device,&features);
            if(s.views==2&&!multiview.multiview)continue;
            for(uint32_t i=0;i<n;++i){VkBool32 present=false;vkGetPhysicalDeviceSurfaceSupportKHR(device,i,surface,&present);int score=props.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU?2:1;
                if(present&&(families[i].queueFlags&VK_QUEUE_GRAPHICS_BIT)&&score>best){best=score;context.physicalDevice=device;context.queueFamilyIndex=i;context.enabledFeatures=XrVulkanPolicy::optionalFeatures(features.features);context.maxSamplerAnisotropy=props.limits.maxSamplerAnisotropy;deviceName=props.deviceName;}}
        }
        Check(context.physicalDevice!=VK_NULL_HANDLE,"No Vulkan 1.1 GPU supports the requested display/views");
        if(MatchedProfile::active){
            auto get=[](const char *key){return MatchedProfile::requested.at(key);};
            Check(!get("anisotropy")||context.enabledFeatures.samplerAnisotropy,"Profile requires unsupported anisotropy");Check(!get("atomics")||context.enabledFeatures.fragmentStoresAndAtomics,"Profile requires unsupported fragment atomics");
            context.enabledFeatures.samplerAnisotropy=get("anisotropy")!=0;context.enabledFeatures.fragmentStoresAndAtomics=get("atomics")!=0;
            Check(get("max_anisotropy")<=context.maxSamplerAnisotropy,"Profile anisotropy exceeds GPU limit");
            context.maxSamplerAnisotropy=float(get("max_anisotropy"));
            context.enabledFeatures.textureCompressionBC &= get("bc")!=0;
            context.enabledFeatures.textureCompressionASTC_LDR &= get("astc")!=0;
            context.enabledFeatures.textureCompressionETC2 &= get("etc2")!=0;
        }
        VkPhysicalDeviceProperties props;vkGetPhysicalDeviceProperties(context.physicalDevice,&props);Check(unsigned(s.width)<=props.limits.maxImageDimension2D&&unsigned(s.height)<=props.limits.maxImageDimension2D,"Render extent exceeds GPU limit");
        float priority=1;VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};queue.queueFamilyIndex=context.queueFamilyIndex;queue.queueCount=1;queue.pQueuePriorities=&priority;
        VkPhysicalDeviceMultiviewFeatures mv{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES};mv.multiview=s.views==2;
        const char *swapchain=VK_KHR_SWAPCHAIN_EXTENSION_NAME;VkDeviceCreateInfo device{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};device.pNext=&mv;device.queueCreateInfoCount=1;device.pQueueCreateInfos=&queue;device.pEnabledFeatures=&context.enabledFeatures;device.enabledExtensionCount=1;device.ppEnabledExtensionNames=&swapchain;
        VkCheck(vkCreateDevice(context.physicalDevice,&device,nullptr,&context.device),"Create desktop Vulkan device");vkGetDeviceQueue(context.device,context.queueFamilyIndex,0,&context.queue);
        context.width=s.width;context.height=s.height;context.viewCount=s.views;context.renderScaleEffectivePercent=100;context.fragmentStoresAndAtomicsEnabled=context.enabledFeatures.fragmentStoresAndAtomics;context.colourFormat=VK_FORMAT_R8G8B8A8_UNORM;
        VkImageCreateInfo target{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};target.imageType=VK_IMAGE_TYPE_2D;target.format=context.colourFormat;target.extent={unsigned(s.width),unsigned(s.height),1};target.mipLevels=1;target.arrayLayers=s.views;target.samples=VK_SAMPLE_COUNT_1_BIT;target.tiling=VK_IMAGE_TILING_OPTIMAL;target.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        VkCheck(vkCreateImage(context.device,&target,nullptr,&image),"Create viewer render target");VkMemoryRequirements requirements;vkGetImageMemoryRequirements(context.device,image,&requirements);VkPhysicalDeviceMemoryProperties memoryProps;vkGetPhysicalDeviceMemoryProperties(context.physicalDevice,&memoryProps);
        unsigned type=UINT32_MAX;for(unsigned i=0;i<memoryProps.memoryTypeCount;++i)if((requirements.memoryTypeBits&(1u<<i))&&(memoryProps.memoryTypes[i].propertyFlags&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)){type=i;break;}Check(type!=UINT32_MAX,"No device-local render target memory");
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};allocation.allocationSize=requirements.size;allocation.memoryTypeIndex=type;VkCheck(vkAllocateMemory(context.device,&allocation,nullptr,&memory),"Allocate viewer render target");VkCheck(vkBindImageMemory(context.device,image,memory,0),"Bind viewer render target");
        VkImageViewCreateInfo v{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};v.image=image;v.viewType=VK_IMAGE_VIEW_TYPE_2D_ARRAY;v.format=context.colourFormat;v.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,unsigned(s.views)};VkCheck(vkCreateImageView(context.device,&v,nullptr,&view),"Create viewer target view");
    }catch(...){vkDestroySurfaceKHR(context.instance,surface,nullptr);throw;}
    vkDestroySurfaceKHR(context.instance,surface,nullptr);
    Check(Desktop::InitializeGpu(context.instance,context.physicalDevice,context.device,context.queue,context.queueFamilyIndex),"Initialize desktop presentation");
}
void Vulkan::Close(){
    if(context.device){vkDeviceWaitIdle(context.device);Desktop::ShutdownGpu();if(view)vkDestroyImageView(context.device,view,nullptr);if(image)vkDestroyImage(context.device,image,nullptr);if(memory)vkFreeMemory(context.device,memory,nullptr);vkDestroyDevice(context.device,nullptr);}
    if(context.instance)vkDestroyInstance(context.instance,nullptr);context={};view={};image={};memory={};
}
}
