// Windows platform entry point for the shared Vulkan renderer. No game data.
#include <csignal>
#include <rw.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include "XrMath.h"
#include "VulkanPolicy.h"
#include "LegacyColour.h"
#include "DesktopMirror.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <filesystem>

// librw's application-owned stall checkpoint hook. Keep diagnostics cheap;
// unlike Android, this initial desktop target has no background watchdog.
namespace platform {
std::atomic<const char*> lastCheckpoint{"startup"};
void setCheckpoint(const char *label) { lastCheckpoint.store(label, std::memory_order_relaxed); }
}

namespace {
std::atomic<bool> interrupted{false};
void consoleEvent(int) { interrupted=true; }
void xrCheck(XrResult result, const char *operation) {
    if(XR_FAILED(result)) throw std::runtime_error(std::string(operation) + " failed (OpenXR " + std::to_string(result) + ")");
}
void vkCheck(VkResult result, const char *operation) {
    if(result != VK_SUCCESS) throw std::runtime_error(std::string(operation) + " failed (Vulkan " + std::to_string(result) + ")");
}
template<class T> T xrFunction(XrInstance instance, const char *name) {
    PFN_xrVoidFunction function = nullptr;
    xrCheck(xrGetInstanceProcAddr(instance, name, &function), name);
    if(!function) throw std::runtime_error(std::string("Missing OpenXR function: ") + name);
    return reinterpret_cast<T>(function);
}

struct Application {
    XrInstance instance = XR_NULL_HANDLE;
    XrSystemId system = XR_NULL_SYSTEM_ID;
    XrSession session = XR_NULL_HANDLE;
    XrSpace space = XR_NULL_HANDLE;
    XrSwapchain swapchain = XR_NULL_HANDLE;
    XrActionSet actions = XR_NULL_HANDLE;
    XrAction grip = XR_NULL_HANDLE, select = XR_NULL_HANDLE;
    XrPath hands[2]{};
    XrSpace handSpaces[2]{};
    VkInstance vkInstance = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkImage probeImage = VK_NULL_HANDLE;
    VkDeviceMemory probeMemory = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    uint32_t queueFamily = UINT32_MAX, width = 0, height = 0;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkPhysicalDeviceFeatures enabled{};
    std::vector<XrSwapchainImageVulkanKHR> images;
    std::vector<VkImageView> imageViews;
    bool engineInitialized = false, engineOpened = false, engineStarted = false;
    bool running = false, focused = false, exit = false;
    XrEnvironmentBlendMode blend = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    uint32_t submitted = 0;
    uint32_t pacingSamples = 0;
    double pacingFrameMs = 0, pacingEndMs = 0, pacingPeriodMs = 0;

    ~Application() {
        if(device) vkDeviceWaitIdle(device);
        if(engineStarted) rw::Engine::stop();
        if(engineOpened) rw::Engine::close();
        if(engineInitialized) rw::Engine::term();
        for(auto view : imageViews) vkDestroyImageView(device, view, nullptr);
        if(probeImage) vkDestroyImage(device, probeImage, nullptr);
        if(probeMemory) vkFreeMemory(device, probeMemory, nullptr);
        if(swapchain) xrDestroySwapchain(swapchain);
        for(auto hand : handSpaces) if(hand) xrDestroySpace(hand);
        if(space) xrDestroySpace(space);
        if(session) xrDestroySession(session);
        if(actions) xrDestroyActionSet(actions);
        if(device) vkDestroyDevice(device, nullptr);
        if(vkInstance) vkDestroyInstance(vkInstance, nullptr);
        if(instance) xrDestroyInstance(instance);
    }

    void createInstance() {
        uint32_t count = 0;
        xrCheck(xrEnumerateInstanceExtensionProperties(nullptr, 0, &count, nullptr), "enumerate extensions");
        std::vector<XrExtensionProperties> extensions(count, {XR_TYPE_EXTENSION_PROPERTIES});
        xrCheck(xrEnumerateInstanceExtensionProperties(nullptr, count, &count, extensions.data()), "enumerate extensions");
        if(std::none_of(extensions.begin(), extensions.end(), [](const auto &e) {
            return std::strcmp(e.extensionName, XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME) == 0;
        })) throw std::runtime_error("The active OpenXR runtime does not support XR_KHR_vulkan_enable2.");
        const char *extension = XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME;
        XrInstanceCreateInfo info{XR_TYPE_INSTANCE_CREATE_INFO};
        std::strcpy(info.applicationInfo.applicationName, "Liberty City VR Desktop bring-up");
        std::strcpy(info.applicationInfo.engineName, "librw Vulkan");
        info.applicationInfo.applicationVersion = 900;
        // Vulkan enable2 works with OpenXR 1.0; do not require a 1.1 runtime.
        info.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 0);
        info.enabledExtensionCount = 1; info.enabledExtensionNames = &extension;
        xrCheck(xrCreateInstance(&info, &instance), "xrCreateInstance");
        XrInstanceProperties properties{XR_TYPE_INSTANCE_PROPERTIES};
        xrCheck(xrGetInstanceProperties(instance, &properties), "xrGetInstanceProperties");
        std::cout << "OpenXR runtime: " << properties.runtimeName << '\n';
        XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO};
        systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        xrCheck(xrGetSystem(instance, &systemInfo, &system), "xrGetSystem (connect a PCVR headset)");
    }

    void createVulkan() {
        auto requirementsFn = xrFunction<PFN_xrGetVulkanGraphicsRequirements2KHR>(instance, "xrGetVulkanGraphicsRequirements2KHR");
        XrGraphicsRequirementsVulkan2KHR requirements{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR};
        xrCheck(requirementsFn(instance, system, &requirements), "Vulkan requirements");
        if(!XrVulkanPolicy::supportsVulkan11(requirements.minApiVersionSupported,
                requirements.maxApiVersionSupported, VK_API_VERSION_1_1))
            throw std::runtime_error("This renderer requires Vulkan 1.1 within the runtime's supported range.");
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        app.pApplicationName = "Liberty City VR"; app.applicationVersion = VK_MAKE_VERSION(0,9,0);
        app.apiVersion = VK_API_VERSION_1_1;
        VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; info.pApplicationInfo = &app;
        XrVulkanInstanceCreateInfoKHR xrInfo{XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR};
        xrInfo.systemId = system; xrInfo.pfnGetInstanceProcAddr = vkGetInstanceProcAddr; xrInfo.vulkanCreateInfo = &info;
        VkResult result = VK_SUCCESS;
        xrCheck(xrFunction<PFN_xrCreateVulkanInstanceKHR>(instance, "xrCreateVulkanInstanceKHR")(
            instance, &xrInfo, &vkInstance, &result), "xrCreateVulkanInstanceKHR");
        vkCheck(result, "create Vulkan instance");
        XrVulkanGraphicsDeviceGetInfoKHR getDevice{XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR};
        getDevice.systemId = system; getDevice.vulkanInstance = vkInstance;
        xrCheck(xrFunction<PFN_xrGetVulkanGraphicsDevice2KHR>(instance, "xrGetVulkanGraphicsDevice2KHR")(
            instance, &getDevice, &physical), "runtime-selected Vulkan GPU");
        VkPhysicalDeviceProperties properties{}; vkGetPhysicalDeviceProperties(physical, &properties);
        if(!XrVulkanPolicy::supportsVulkan11(requirements.minApiVersionSupported,
                requirements.maxApiVersionSupported, properties.apiVersion))
            throw std::runtime_error("The runtime-selected GPU lacks Vulkan 1.1.");
        std::cout << "Runtime-selected GPU: " << properties.deviceName << '\n';
        VkPhysicalDeviceMultiviewFeatures multiview{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES};
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2}; features.pNext = &multiview;
        vkGetPhysicalDeviceFeatures2(physical, &features);
        VkPhysicalDeviceMultiviewProperties multiProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES};
        VkPhysicalDeviceProperties2 properties2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2}; properties2.pNext = &multiProperties;
        vkGetPhysicalDeviceProperties2(physical, &properties2);
        if(!multiview.multiview || multiProperties.maxMultiviewViewCount < 2)
            throw std::runtime_error("Stereo multiview is required; no multipass fallback is implemented.");
        uint32_t count = 0; vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, nullptr);
        std::vector<VkQueueFamilyProperties> families(count); vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, families.data());
        queueFamily = XrVulkanPolicy::graphicsQueue(families.data(), count);
        if(queueFamily == UINT32_MAX) throw std::runtime_error("No Vulkan graphics queue.");
        enabled = XrVulkanPolicy::optionalFeatures(features.features);
        const float priority = 1;
        VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queueInfo.queueFamilyIndex = queueFamily; queueInfo.queueCount = 1; queueInfo.pQueuePriorities = &priority;
        // Only enable the multiview feature we use, not geometry/tessellation support.
        multiview = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES}; multiview.multiview = VK_TRUE;
        VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        deviceInfo.pNext = &multiview; deviceInfo.queueCreateInfoCount = 1;
        deviceInfo.pQueueCreateInfos = &queueInfo; deviceInfo.pEnabledFeatures = &enabled;
        XrVulkanDeviceCreateInfoKHR xrDevice{XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR};
        xrDevice.systemId = system; xrDevice.pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
        xrDevice.vulkanPhysicalDevice = physical; xrDevice.vulkanCreateInfo = &deviceInfo;
        xrCheck(xrFunction<PFN_xrCreateVulkanDeviceKHR>(instance, "xrCreateVulkanDeviceKHR")(
            instance, &xrDevice, &device, &result), "xrCreateVulkanDeviceKHR");
        vkCheck(result, "create Vulkan device"); vkGetDeviceQueue(device, queueFamily, 0, &queue);
    }

    XrPath path(const char *text) {
        XrPath value = XR_NULL_PATH; xrCheck(xrStringToPath(instance, text, &value), "xrStringToPath"); return value;
    }
    void createSession() {
        XrGraphicsBindingVulkan2KHR binding{XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR};
        binding.instance = vkInstance; binding.physicalDevice = physical; binding.device = device;
        binding.queueFamilyIndex = queueFamily;
        XrSessionCreateInfo info{XR_TYPE_SESSION_CREATE_INFO}; info.next = &binding; info.systemId = system;
        xrCheck(xrCreateSession(instance, &info, &session), "xrCreateSession");
        // LOCAL avoids assuming a calibrated stage/floor or a specific guardian setup.
        XrReferenceSpaceCreateInfo reference{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
        reference.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL; reference.poseInReferenceSpace.orientation.w = 1;
        xrCheck(xrCreateReferenceSpace(session, &reference, &space), "local reference space");
        XrActionSetCreateInfo set{XR_TYPE_ACTION_SET_CREATE_INFO};
        std::strcpy(set.actionSetName, "bringup"); std::strcpy(set.localizedActionSetName, "Bring-up");
        xrCheck(xrCreateActionSet(instance, &set, &actions), "xrCreateActionSet");
        hands[0] = path("/user/hand/left"); hands[1] = path("/user/hand/right");
        auto action = [&](const char *name, XrActionType type, XrAction &output) {
            XrActionCreateInfo create{XR_TYPE_ACTION_CREATE_INFO};
            std::strcpy(create.actionName, name); std::strcpy(create.localizedActionName, name);
            create.actionType = type; create.countSubactionPaths = 2; create.subactionPaths = hands;
            xrCheck(xrCreateAction(actions, &create, &output), "xrCreateAction");
        };
        action("grip_pose", XR_ACTION_TYPE_POSE_INPUT, grip);
        action("select", XR_ACTION_TYPE_BOOLEAN_INPUT, select);
        for(const char *profile : {"/interaction_profiles/khr/simple_controller", "/interaction_profiles/oculus/touch_controller",
                                  "/interaction_profiles/valve/index_controller"}) {
            const bool simple = std::strstr(profile, "simple_controller") != nullptr;
            std::vector<XrActionSuggestedBinding> bindings;
            for(const char *hand : {"/user/hand/left", "/user/hand/right"}) {
                bindings.push_back({grip, path((std::string(hand) + "/input/grip/pose").c_str())});
                bindings.push_back({select, path((std::string(hand) + (simple ? "/input/select/click" : "/input/trigger/value")).c_str())});
            }
            XrInteractionProfileSuggestedBinding suggested{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
            suggested.interactionProfile = path(profile); suggested.countSuggestedBindings = uint32_t(bindings.size());
            suggested.suggestedBindings = bindings.data();
            auto result = xrSuggestInteractionProfileBindings(instance, &suggested);
            if(result != XR_ERROR_PATH_UNSUPPORTED) xrCheck(result, "controller bindings");
        }
        XrSessionActionSetsAttachInfo attach{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
        attach.countActionSets = 1; attach.actionSets = &actions;
        xrCheck(xrAttachSessionActionSets(session, &attach), "attach actions");
        for(int i = 0; i < 2; ++i) {
            XrActionSpaceCreateInfo hand{XR_TYPE_ACTION_SPACE_CREATE_INFO};
            hand.action = grip; hand.subactionPath = hands[i]; hand.poseInActionSpace.orientation.w = 1;
            xrCheck(xrCreateActionSpace(session, &hand, &handSpaces[i]), "controller space");
        }
    }

    void createSwapchain() {
        uint32_t count = 0;
        xrCheck(xrEnumerateViewConfigurationViews(instance, system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
            0, &count, nullptr), "stereo views");
        if(count != 2) throw std::runtime_error("Exactly two primary stereo views are required.");
        std::array<XrViewConfigurationView,2> views{{{XR_TYPE_VIEW_CONFIGURATION_VIEW},{XR_TYPE_VIEW_CONFIGURATION_VIEW}}};
        xrCheck(xrEnumerateViewConfigurationViews(instance, system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
            2, &count, views.data()), "stereo view dimensions");
        width = std::max(views[0].recommendedImageRectWidth, views[1].recommendedImageRectWidth);
        height = std::max(views[0].recommendedImageRectHeight, views[1].recommendedImageRectHeight);
        for(const auto &v : views) if(width > v.maxImageRectWidth || height > v.maxImageRectHeight)
            throw std::runtime_error("Runtime view extents cannot share an array swapchain.");
        xrCheck(xrEnumerateEnvironmentBlendModes(instance, system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
            0, &count, nullptr), "blend modes");
        std::vector<XrEnvironmentBlendMode> modes(count);
        xrCheck(xrEnumerateEnvironmentBlendModes(instance, system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
            count, &count, modes.data()), "blend modes");
        if(std::find(modes.begin(), modes.end(), blend) == modes.end()) throw std::runtime_error("Opaque VR blend mode required.");
        xrCheck(xrEnumerateSwapchainFormats(session, 0, &count, nullptr), "swapchain formats");
        std::vector<int64_t> formats(count); xrCheck(xrEnumerateSwapchainFormats(session, count, &count, formats.data()), "swapchain formats");
        for(auto candidate : {VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_B8G8R8A8_SRGB,
                               VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM}) {
            if(std::find(formats.begin(), formats.end(), int64_t(candidate)) != formats.end()) { format = candidate; break; }
        }
        if(format == VK_FORMAT_UNDEFINED) throw std::runtime_error("No supported RGBA/BGRA OpenXR colour format.");
        XrSwapchainCreateInfo create{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        create.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT; create.format = format;
        create.sampleCount = 1; create.width = width; create.height = height;
        create.faceCount = 1; create.arraySize = 2; create.mipCount = 1;
        xrCheck(xrCreateSwapchain(session, &create, &swapchain), "create stereo array swapchain");
        xrCheck(xrEnumerateSwapchainImages(swapchain, 0, &count, nullptr), "swapchain images");
        images.resize(count, {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR});
        xrCheck(xrEnumerateSwapchainImages(swapchain, count, &count,
            reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data())), "swapchain images");
        for(const auto &image : images) {
            VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            view.image = image.image; view.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY; view.format = format;
            view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 2};
            VkImageView output = VK_NULL_HANDLE; vkCheck(vkCreateImageView(device, &view, nullptr, &output), "swapchain image view");
            imageViews.push_back(output);
        }
        openRenderer();
    }

    void openRenderer() {
        engineInitialized = rw::Engine::init() != 0;
        if(!engineInitialized) throw std::runtime_error("librw initialization failed.");
        rw::EngineOpenParams params{};
        params.instance = vkInstance; params.physicalDevice = physical; params.device = device;
        params.fragmentStoresAndAtomicsEnabled = enabled.fragmentStoresAndAtomics;
        params.queueFamilyIndex = queueFamily; params.queue = queue;
        params.width = width; params.height = height; params.viewCount = 2;
        params.colourFormat = format; params.renderScaleEffectivePercent = 100; params.sceneSampleCount = 1;
        engineOpened = rw::Engine::open(&params) != 0;
        // Engine::open historically does not propagate DEVICEOPEN's result.
        if(!engineOpened || !rw::vulkan::isInitialized()) throw std::runtime_error("librw Vulkan device open failed.");
        engineStarted = rw::Engine::start() != 0;
        if(!engineStarted) throw std::runtime_error("librw Vulkan start failed.");
        // The game normally establishes worldToPlay through Camera::beginUpdate.
        // Diagnostic vertices already use OpenXR play space. Initialize the
        // backend's supported no-camera identity path before submitting draws.
        rw::vulkan::renderdevice.beginUpdate(nullptr);
        std::cout << "Shared librw stereo renderer: " << width << 'x' << height << " per eye, 1x MSAA.\n";
    }

    void events() {
        XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
        XrResult result;
        while((result = xrPollEvent(instance, &event)) == XR_SUCCESS) {
            if(event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
                const auto &state = *reinterpret_cast<XrEventDataSessionStateChanged*>(&event);
                focused = state.state == XR_SESSION_STATE_FOCUSED;
                if(state.state == XR_SESSION_STATE_READY) {
                    XrSessionBeginInfo begin{XR_TYPE_SESSION_BEGIN_INFO}; begin.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                    xrCheck(xrBeginSession(session, &begin), "xrBeginSession"); running = true;
                } else if(state.state == XR_SESSION_STATE_STOPPING) {
                    xrCheck(xrEndSession(session), "xrEndSession"); running = false;
                } else if(state.state == XR_SESSION_STATE_EXITING || state.state == XR_SESSION_STATE_LOSS_PENDING) exit = true;
            } else if(event.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) exit = true;
            event = {XR_TYPE_EVENT_DATA_BUFFER};
        }
        if(result != XR_EVENT_UNAVAILABLE) xrCheck(result, "xrPollEvent");
    }

    void triangle(const XrPosef &pose, float size, bool selected, bool textured = false) {
        auto model = XrMath::worldFromPose(pose);
        rw::vulkan::Im3DVertex vertices[3]{};
        const float points[3][3] = {{-size,-size,0},{size,-size,0},{0,size,0}};
        for(int i = 0; i < 3; ++i) {
            auto &p = vertices[i].position;
            p.x = model.m[0]*points[i][0] + model.m[4]*points[i][1] + model.m[12];
            p.y = model.m[1]*points[i][0] + model.m[5]*points[i][1] + model.m[13];
            p.z = model.m[2]*points[i][0] + model.m[6]*points[i][1] + model.m[14];
            vertices[i].setColor(selected ? 255 : 60, 200, selected ? 60 : 255, 255);
            if(textured) {
                vertices[i].setColor(255,255,255,255);
                vertices[i].u=(points[i][0]/size+1)*8;
                vertices[i].v=(points[i][1]/size+1)*8;
            }
        }
        rw::vulkan::im3DTransform(vertices, 3, nullptr, 0);
        rw::vulkan::im3DRenderPrimitive(rw::PRIMTYPETRILIST); rw::vulkan::im3DEnd();
    }

    void frame() {
        const auto frameStart = std::chrono::steady_clock::now();
        XrFrameWaitInfo wait{XR_TYPE_FRAME_WAIT_INFO}; XrFrameState state{XR_TYPE_FRAME_STATE};
        xrCheck(xrWaitFrame(session, &wait, &state), "xrWaitFrame");
        XrFrameBeginInfo begin{XR_TYPE_FRAME_BEGIN_INFO}; xrCheck(xrBeginFrame(session, &begin), "xrBeginFrame");
        // Every begun frame ends, including shouldRender=false and invalid views.
        XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO}; end.displayTime = state.predictedDisplayTime; end.environmentBlendMode = blend;
        std::array<XrCompositionLayerProjectionView,2> projection{{{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}}};
        XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION}; layer.space = space;
        const XrCompositionLayerBaseHeader *base = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&layer);
        uint32_t imageIndex = 0; bool acquired = false, waited = false;
        try {
            std::array<XrView,2> views{{{XR_TYPE_VIEW},{XR_TYPE_VIEW}}};
            XrViewLocateInfo locate{XR_TYPE_VIEW_LOCATE_INFO}; locate.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
            locate.displayTime = state.predictedDisplayTime; locate.space = space;
            XrViewState viewState{XR_TYPE_VIEW_STATE}; uint32_t count = 0;
            xrCheck(xrLocateViews(session, &locate, &viewState, 2, &count, views.data()), "xrLocateViews");
            const auto valid = XR_VIEW_STATE_POSITION_VALID_BIT | XR_VIEW_STATE_ORIENTATION_VALID_BIT;
            if(state.shouldRender && count == 2 && (viewState.viewStateFlags & valid) == valid) {
                XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
                xrCheck(xrAcquireSwapchainImage(swapchain, &acquire, &imageIndex), "acquire swapchain image"); acquired = true;
                XrSwapchainImageWaitInfo imageWait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO}; imageWait.timeout = 1000000000;
                // XR_TIMEOUT_EXPIRED is a success-class result, but does not grant image access.
                auto waitResult = xrWaitSwapchainImage(swapchain, &imageWait);
                if(waitResult == XR_TIMEOUT_EXPIRED) throw std::runtime_error("Swapchain image wait timed out.");
                xrCheck(waitResult, "wait swapchain image"); waited = true;
                XrMath::Mat4 vp[2];
                for(int i = 0; i < 2; ++i) {
                    vp[i] = XrMath::multiply(XrMath::projectionFromFov(views[i].fov, 0.05f, 100.0f), XrMath::viewFromPose(views[i].pose));
                    projection[i].pose = views[i].pose; projection[i].fov = views[i].fov;
                    projection[i].subImage.swapchain = swapchain; projection[i].subImage.imageArrayIndex = i;
                    projection[i].subImage.imageRect.extent = {int32_t(width), int32_t(height)};
                }
                rw::vulkan::setStereoViewProjection(vp[0].m, vp[1].m);
                rw::vulkan::setClearColour(18, 24, 38);
                if(!rw::vulkan::beginFrame(images.at(imageIndex).image, imageViews.at(imageIndex)))
                    throw std::runtime_error("librw could not begin frame.");
                rw::vulkan::renderdevice.setRenderState(rw::CULLMODE, reinterpret_cast<void*>(rw::CULLNONE));
                XrPosef center{}; center.orientation.w = 1; center.position = {0,0,-2}; triangle(center, 0.35f, false);
                if(focused) {
                    XrActiveActionSet active{actions, XR_NULL_PATH}; XrActionsSyncInfo sync{XR_TYPE_ACTIONS_SYNC_INFO};
                    sync.countActiveActionSets = 1; sync.activeActionSets = &active;
                    auto synced = xrSyncActions(session, &sync);
                    if(synced == XR_SUCCESS) for(int i = 0; i < 2; ++i) {
                        XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO}; get.action = grip; get.subactionPath = hands[i];
                        XrActionStatePose poseState{XR_TYPE_ACTION_STATE_POSE}; xrCheck(xrGetActionStatePose(session, &get, &poseState), "grip activity");
                        if(!poseState.isActive) continue;
                        XrSpaceLocation hand{XR_TYPE_SPACE_LOCATION}; xrCheck(xrLocateSpace(handSpaces[i], space, state.predictedDisplayTime, &hand), "grip location");
                        auto flags = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
                        if((hand.locationFlags & flags) != flags) continue;
                        get.action = select; XrActionStateBoolean selected{XR_TYPE_ACTION_STATE_BOOLEAN};
                        xrCheck(xrGetActionStateBoolean(session, &get, &selected), "select state");
                        triangle(hand.pose, 0.07f, selected.isActive && selected.currentState);
                    } else if(synced != XR_SESSION_NOT_FOCUSED) xrCheck(synced, "sync actions");
                }
                rw::vulkan::endFrame();
                // Deliberately serialize the initial diagnostic path before release.
                // This is conservative bring-up synchronization, not a performance baseline.
                vkCheck(vkQueueWaitIdle(queue), "complete submitted stereo work");
                XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
                xrCheck(xrReleaseSwapchainImage(swapchain, &release), "release swapchain image"); acquired = false;
                layer.viewCount = 2; layer.views = projection.data(); end.layerCount = 1; end.layers = &base;
            }
        } catch(...) {
            if(device) vkDeviceWaitIdle(device);
            if(acquired && waited) { XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO}; xrReleaseSwapchainImage(swapchain, &release); }
            xrEndFrame(session, &end); throw;
        }
        const auto endStart = std::chrono::steady_clock::now();
        xrCheck(xrEndFrame(session, &end), "xrEndFrame");
        const auto frameEnd = std::chrono::steady_clock::now();
        if(end.layerCount) {
            ++submitted;
            pacingFrameMs += std::chrono::duration<double,std::milli>(frameEnd-frameStart).count();
            pacingEndMs += std::chrono::duration<double,std::milli>(frameEnd-endStart).count();
            pacingPeriodMs += double(state.predictedDisplayPeriod)/1e6;
            if(++pacingSamples==600) {
                std::cout << "Diagnostic rendered pacing: frame=" << pacingFrameMs/600
                    << " end=" << pacingEndMs/600 << " predicted period=" << pacingPeriodMs/600
                    << " ms (serialized diagnostic, not a game benchmark)\n";
                pacingSamples=0; pacingFrameMs=pacingEndMs=pacingPeriodMs=0;
            }
        }
    }
};

void verifyProbePixels(Application &app, int grey = -1, bool textured = false) {
    struct Readback {
        VkDevice device;
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkCommandPool pool = VK_NULL_HANDLE;
        bool mapped = false;
        ~Readback() {
            if(mapped) vkUnmapMemory(device, memory);
            if(pool) vkDestroyCommandPool(device, pool, nullptr);
            if(buffer) vkDestroyBuffer(device, buffer, nullptr);
            if(memory) vkFreeMemory(device, memory, nullptr);
        }
    } readback{app.device};
    const VkDeviceSize bytes = VkDeviceSize(app.width)*app.height*4*2;
    VkBufferCreateInfo buffer{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer.size = bytes; buffer.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    vkCheck(vkCreateBuffer(app.device, &buffer, nullptr, &readback.buffer), "readback buffer");
    VkMemoryRequirements requirements{}; vkGetBufferMemoryRequirements(app.device, readback.buffer, &requirements);
    VkPhysicalDeviceMemoryProperties memory{}; vkGetPhysicalDeviceMemoryProperties(app.physical, &memory);
    uint32_t type = UINT32_MAX;
    const auto flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    for(uint32_t i = 0; i < memory.memoryTypeCount; ++i)
        if((requirements.memoryTypeBits & (1u << i)) && (memory.memoryTypes[i].propertyFlags & flags) == flags) { type = i; break; }
    if(type == UINT32_MAX) throw std::runtime_error("No coherent host memory for pixel verification.");
    VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocate.allocationSize = requirements.size; allocate.memoryTypeIndex = type;
    vkCheck(vkAllocateMemory(app.device, &allocate, nullptr, &readback.memory), "readback memory");
    vkCheck(vkBindBufferMemory(app.device, readback.buffer, readback.memory, 0), "bind readback buffer");
    VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; pool.queueFamilyIndex = app.queueFamily;
    vkCheck(vkCreateCommandPool(app.device, &pool, nullptr, &readback.pool), "readback command pool");
    VkCommandBufferAllocateInfo commands{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    commands.commandPool = readback.pool; commands.commandBufferCount = 1; commands.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    VkCommandBuffer command = VK_NULL_HANDLE; vkCheck(vkAllocateCommandBuffers(app.device, &commands, &command), "readback commands");
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkCheck(vkBeginCommandBuffer(command, &begin), "begin readback");
    VkImageMemoryBarrier image{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    image.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; image.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    image.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; image.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    image.srcQueueFamilyIndex = image.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image.image = app.probeImage; image.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,2};
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&image);
    VkBufferImageCopy copy{}; copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT,0,0,2}; copy.imageExtent = {app.width,app.height,1};
    vkCmdCopyImageToBuffer(command,app.probeImage,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,readback.buffer,1,&copy);
    VkBufferMemoryBarrier host{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    host.srcQueueFamilyIndex = host.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; host.buffer = readback.buffer; host.size = VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&host,0,nullptr);
    image.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
    image.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    image.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    image.newLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,0,0,nullptr,0,nullptr,1,&image);
    vkCheck(vkEndCommandBuffer(command),"end readback");
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO}; submit.commandBufferCount = 1; submit.pCommandBuffers = &command;
    vkCheck(vkQueueSubmit(app.queue,1,&submit,VK_NULL_HANDLE),"submit readback");
    vkCheck(vkQueueWaitIdle(app.queue),"complete readback");
    void *mapped = nullptr; vkCheck(vkMapMemory(app.device,readback.memory,0,bytes,0,&mapped),"map pixels"); readback.mapped = true;
    auto pixels = static_cast<const uint8_t*>(mapped);
    if(textured) {
        // Every 2x2 source block has one transparent red and three opaque cyan
        // texels. Alpha-weighted generated mips must stay cyan, without red bleed.
        for(unsigned eye=0; eye<2; ++eye) {
            const auto *center=pixels+((size_t(eye)*app.height+app.height/2)*app.width+app.width/2)*4;
            const int expected[]={0,200,255};
            for(unsigned channel=0; channel<3; ++channel)
                if(std::abs(int(center[channel])-expected[channel])>2)
                    throw std::runtime_error("Generated mip texture failed GPU sampling/readback.");
        }
    }
    if(grey >= 0) {
        for(unsigned eye=0; eye<2; ++eye) {
            const auto *corner=pixels+size_t(eye)*app.width*app.height*4;
            for(unsigned channel=0; channel<3; ++channel)
                if(std::abs(int(corner[channel])-grey)>1)
                    throw std::runtime_error("Encoded colour changed at the sRGB/UNORM attachment boundary.");
        }
    }
    uint64_t count[2]{}, sumX[2]{};
    for(uint32_t eye = 0; eye < 2; ++eye)
        for(uint32_t y = 0; y < app.height; ++y)
            for(uint32_t x = 0; x < app.width; ++x) {
                const auto *pixel = pixels + ((size_t(eye)*app.height+y)*app.width+x)*4;
                if(pixel[0] < 120 && pixel[1] > 130 && pixel[2] > 150) { ++count[eye]; sumX[eye] += x; }
            }
    std::cout << "Stereo geometry pixels: left=" << count[0] << ", right=" << count[1] << '\n';
    if(count[0] < 500 || count[1] < 500) throw std::runtime_error("Missing triangle pixels in one or both eye layers.");
    if(double(sumX[0])/count[0] <= double(sumX[1])/count[1] + 1.0)
        throw std::runtime_error("Expected stereo disparity is missing or reversed.");
}

int vulkanProbe(bool render = false, bool colour = false, bool textures = false, bool burst = false, bool mirror = false) {
    Application app;
    struct DesktopScope { bool active; ~DesktopScope(){if(active)Desktop::Close();} } desktop{mirror};
    if(mirror && !Desktop::Open("")) throw std::runtime_error("Cannot create mirror probe window.");
    VkApplicationInfo info{VK_STRUCTURE_TYPE_APPLICATION_INFO}; info.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo create{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; create.pApplicationInfo = &info;
    const char *windowExtensions[]={VK_KHR_SURFACE_EXTENSION_NAME,"VK_KHR_win32_surface"};
    if(mirror){create.enabledExtensionCount=2;create.ppEnabledExtensionNames=windowExtensions;}
    vkCheck(vkCreateInstance(&create, nullptr, &app.vkInstance), "Vulkan loader probe");
    uint32_t count = 0; vkCheck(vkEnumeratePhysicalDevices(app.vkInstance, &count, nullptr), "Vulkan GPU enumeration");
    std::vector<VkPhysicalDevice> devices(count); vkCheck(vkEnumeratePhysicalDevices(app.vkInstance, &count, devices.data()), "Vulkan GPU enumeration");
    bool supported = false;
    for(auto device : devices) {
        VkPhysicalDeviceProperties properties{}; vkGetPhysicalDeviceProperties(device, &properties);
        if(properties.apiVersion < VK_API_VERSION_1_1) continue;
        VkPhysicalDeviceMultiviewFeatures multiview{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES};
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2}; features.pNext = &multiview;
        vkGetPhysicalDeviceFeatures2(device, &features);
        std::cout << "Vulkan GPU: " << properties.deviceName << ", multiview=" << (multiview.multiview ? "yes" : "no") << '\n';
        supported |= multiview.multiview != 0;
        if(multiview.multiview && !app.physical) app.physical = device;
    }
    std::cout << "This probe does not validate OpenXR runtime GPU selection or headset rendering.\n";
    if(render && supported) {
        uint32_t count = 0; vkGetPhysicalDeviceQueueFamilyProperties(app.physical, &count, nullptr);
        std::vector<VkQueueFamilyProperties> families(count); vkGetPhysicalDeviceQueueFamilyProperties(app.physical, &count, families.data());
        app.queueFamily = XrVulkanPolicy::graphicsQueue(families.data(), count);
        if(app.queueFamily == UINT32_MAX) throw std::runtime_error("No graphics queue for renderer probe.");
        const float priority = 1;
        VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queueInfo.queueFamilyIndex = app.queueFamily; queueInfo.queueCount = 1; queueInfo.pQueuePriorities = &priority;
        VkPhysicalDeviceFeatures available{}; vkGetPhysicalDeviceFeatures(app.physical, &available);
        app.enabled = XrVulkanPolicy::optionalFeatures(available);
        VkPhysicalDeviceMultiviewFeatures multiview{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES}; multiview.multiview = VK_TRUE;
        VkDeviceCreateInfo createDevice{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO}; createDevice.pNext = &multiview;
        createDevice.pEnabledFeatures = &app.enabled; createDevice.queueCreateInfoCount = 1; createDevice.pQueueCreateInfos = &queueInfo;
        const char *presentExtensions[]={VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        if(mirror){createDevice.enabledExtensionCount=1;createDevice.ppEnabledExtensionNames=presentExtensions;}
        vkCheck(vkCreateDevice(app.physical, &createDevice, nullptr, &app.device), "probe device");
        vkGetDeviceQueue(app.device, app.queueFamily, 0, &app.queue);
        app.width = 640; app.height = 480; app.format = VK_FORMAT_R8G8B8A8_UNORM;
        VkImageCreateInfo image{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        image.imageType = VK_IMAGE_TYPE_2D; image.format = app.format; image.extent = {app.width,app.height,1};
        if(colour) {
            const int64_t formats[]={VK_FORMAT_R8G8B8A8_SRGB};
            image.format=LegacyColour::selectSrgb(formats,1);
            image.flags=VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
            app.format=LegacyColour::attachmentFormat(image.format);
        }
        image.mipLevels = 1; image.arrayLayers = 2; image.samples = VK_SAMPLE_COUNT_1_BIT;
        image.tiling = VK_IMAGE_TILING_OPTIMAL; image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        vkCheck(vkCreateImage(app.device, &image, nullptr, &app.probeImage), "probe image");
        VkMemoryRequirements requirements{}; vkGetImageMemoryRequirements(app.device, app.probeImage, &requirements);
        VkPhysicalDeviceMemoryProperties memory{}; vkGetPhysicalDeviceMemoryProperties(app.physical, &memory);
        uint32_t type = UINT32_MAX;
        for(uint32_t i = 0; i < memory.memoryTypeCount; ++i)
            if(requirements.memoryTypeBits & (1u << i)) { type = i; break; }
        if(type == UINT32_MAX) throw std::runtime_error("No probe image memory type.");
        VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocate.allocationSize = requirements.size; allocate.memoryTypeIndex = type;
        vkCheck(vkAllocateMemory(app.device, &allocate, nullptr, &app.probeMemory), "probe memory");
        vkCheck(vkBindImageMemory(app.device, app.probeImage, app.probeMemory, 0), "bind probe image");
        VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view.image = app.probeImage; view.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY; view.format = app.format;
        view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,2}; VkImageView output = VK_NULL_HANDLE;
        vkCheck(vkCreateImageView(app.device, &view, nullptr, &output), "probe image view"); app.imageViews.push_back(output);
        app.openRenderer();
        if(mirror) Desktop::InitializeGpu(app.vkInstance,app.physical,app.device,app.queue,app.queueFamily);
        if(textures) rw::vulkan::setGenerateMipmaps(true);
        double textureMs=0;
        XrFovf fov{-0.7f,0.7f,0.7f,-0.7f};
        XrPosef left{{0,0,0,1},{-0.032f,0,0}}, right{{0,0,0,1},{0.032f,0,0}};
        auto projection = XrMath::projectionFromFov(fov,0.05f,100);
        auto leftVP = XrMath::multiply(projection,XrMath::viewFromPose(left));
        auto rightVP = XrMath::multiply(projection,XrMath::viewFromPose(right));
        const int greys[]={0,1,2,8,18,64,128,192,255};
        static bool diagnosticVisible = false;
        if(mirror) Desktop::SetRenderingDetailsProvider([](bool visible) -> const char * {
            diagnosticVisible = visible;
            return "SYNTHETIC DIAGNOSTICS TEST\nNo game performance measurements";
        });
        if(mirror) Desktop::SetOcclusionMapProvider([](int eye,int *size) -> const unsigned char * {
            static unsigned char preview[48*48];*size=48;
            for(int y=0;y<48;++y)for(int x=0;x<48;++x)
                preview[y*48+x]=(x>8+eye*4&&x<40&&y>8&&y<40)?(unsigned char)(64+y*3):0;
            return preview;
        });
        for(int i = 0; i < (mirror ? 60 : burst ? 1 : colour ? 9 : 5); ++i) {
            if(mirror){
                Desktop::Pump();
                auto key=[](unsigned code){Desktop::InjectKey(code,true);Desktop::InjectKey(code,false);};
                if(i==1)key(Desktop::Key_F1);
                if(i==5){Desktop::InjectKey('W',true);Desktop::InjectFocus(false);if(Desktop::Input().keys['W'])throw std::runtime_error("Desktop focus loss left a held key.");Desktop::InjectFocus(true);}
                if(i==10){key(Desktop::Key_F2);if(Desktop::Settings().eye!=1)throw std::runtime_error("Desktop eye shortcut failed.");}
                if(i==20)key(Desktop::Key_F3);
                if(i==25)key(Desktop::Key_F5);
                if(i==30)key(Desktop::Key_F6);
                if(i==35)key(Desktop::Key_F4);
                if(i==40){Desktop::Settings().capture=true;key(Desktop::Key_F12);}
                if(i==50)key(Desktop::Key_F12);
                if(i==45){std::filesystem::create_directories(".cache");Desktop::RequestSnapshot(".cache/desktop-preview-probe.bmp");}
                Desktop::BeginUi();
                if(i==40 && (!diagnosticVisible || Desktop::Settings().capture || Desktop::Settings().eye!=1))
                    throw std::runtime_error("F12 did not open diagnostics, release capture and preserve preview options.");
                if(i==50 && diagnosticVisible) throw std::runtime_error("F12 did not close diagnostics.");
            }
            rw::Raster *sampleTexture=nullptr;
            rw::vulkan::setStereoViewProjection(leftVP.m,rightVP.m); rw::vulkan::setClearColour(18,24,38);
            if(colour) rw::vulkan::setClearColour(greys[i],greys[i],greys[i]);
            if(!rw::vulkan::beginFrame(app.probeImage,output)) throw std::runtime_error("Probe renderer frame failed.");
            if(textures) {
                const int dimension=burst ? 32 : 256;
                auto image=rw::Image::create(dimension,dimension,32);
                if(!image) throw std::runtime_error("Texture fixture allocation failed.");
                image->allocate();
                if(!image->pixels) throw std::runtime_error("Texture fixture pixels unavailable.");
                for(int y=0; y<dimension; ++y) for(int x=0; x<dimension; ++x) {
                    auto pixel=image->pixels+y*image->stride+x*4;
                    const bool clear=(x%2==0 && y%2==0);
                    pixel[0]=clear ? 255 : 0; pixel[1]=clear ? 0 : 200;
                    pixel[2]=clear ? 0 : 255; pixel[3]=clear ? 0 : 255;
                }
                const auto before=std::chrono::steady_clock::now();
                for(int texture=0; texture<(burst ? 4096 : 16); ++texture) {
                    auto raster=burst && sampleTexture ? sampleTexture : rw::Raster::create(dimension,dimension,32,rw::Raster::TEXTURE|rw::Raster::C8888);
                    if(!raster || !raster->setFromImage(image)) throw std::runtime_error("Texture upload failed.");
                    if(rw::vulkan::rasterNumLevels(raster)!=(burst ? 6 : 9)) throw std::runtime_error("Generated texture mip chain missing.");
                    if(burst || texture==15) sampleTexture=raster;
                    else raster->destroy();
                }
                textureMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-before).count();
                image->destroy();
            }
            rw::vulkan::renderdevice.setRenderState(rw::CULLMODE,reinterpret_cast<void*>(rw::CULLNONE));
            if(textures) {
                rw::vulkan::renderdevice.setRenderState(rw::TEXTURERASTER,sampleTexture);
                rw::vulkan::renderdevice.setRenderState(rw::TEXTUREFILTER,reinterpret_cast<void*>(rw::Texture::LINEARMIPLINEAR));
                rw::vulkan::renderdevice.setRenderState(rw::VERTEXALPHA,reinterpret_cast<void*>(0));
            }
            XrPosef triangle{{0,0,0,1},{0,0,-2}}; app.triangle(triangle,0.35f,false,textures);
            rw::vulkan::endFrame(); vkCheck(vkQueueWaitIdle(app.queue),"probe GPU completion");
            if(mirror){Desktop::Present(app.probeImage,output,app.width,app.height);Desktop::EndUi();}
            if(colour) verifyProbePixels(app,greys[i]);
            if(textures) {
                verifyProbePixels(app,-1,true);
                rw::vulkan::renderdevice.setRenderState(rw::TEXTURERASTER,nullptr);
                sampleTexture->destroy();
            }
        }
        if(!colour && !textures) verifyProbePixels(app);
        if(mirror && Desktop::PresentedFrames()==0)throw std::runtime_error("Desktop mirror presented no frames.");
        if(textures) std::cout << (burst ? "Upload burst probe: 4096 ordered 32x32 updates in one frame: " : "Texture probe: 80 RGBA 256x256 uploads with alpha-weighted mips: ") << textureMs << " ms CPU.\n";
        if(colour) std::cout << "Colour probe passed: sRGB images with UNORM attachments preserve nine encoded greys in both eyes.\n";
        if(!colour) std::cout << "Renderer probe passed: " << (burst ? "one upload-burst frame" : "five stereo frames") << " completed; triangle pixels and stereo disparity verified.\n";
    }
    return supported ? 0 : 1;
}
}

int main(int argc, char **argv) {
    try {
        bool probe = false; uint32_t frames = 300;
        for(int i = 1; i < argc; ++i) {
            const std::string argument = argv[i];
            if(argument == "--help") {
                std::cout << "Liberty City VR 0.9.0 Desktop OpenXR/Vulkan bring-up (not the game)\n"
                    "--probe          Negotiate OpenXR GPU/device/session, then exit\n"
                    "--vulkan-probe   Check desktop Vulkan without a headset/runtime\n"
                    "--renderer-probe Render five offscreen stereo frames without OpenXR\n"
                    "--colour-probe   Verify legacy colour bytes through mutable sRGB images\n"
                    "--texture-probe  Measure generated-mip texture streaming without game data\n"
                    "--upload-burst-probe Verify a large load-time upload batch in one frame\n"
                    "--mirror-probe   Exercise the Win32 desktop mirror without a headset\n"
                    "--frames N       Submit N stereo frames (default 300, maximum 10000)\n"
                    "Session readiness/rendering timeout: 60 seconds. Ctrl+C stops.\n"; return 0;
            } else if(argument == "--vulkan-probe" && argc == 2) return vulkanProbe();
            else if(argument == "--renderer-probe" && argc == 2) return vulkanProbe(true);
            else if(argument == "--colour-probe" && argc == 2) return vulkanProbe(true,true);
            else if(argument == "--texture-probe" && argc == 2) return vulkanProbe(true,false,true);
            else if(argument == "--upload-burst-probe" && argc == 2) return vulkanProbe(true,false,true,true);
            else if(argument == "--mirror-probe" && argc == 2) return vulkanProbe(true,false,false,false,true);
            else if(argument == "--probe") probe = true;
            else if(argument == "--frames" && i+1 < argc) {
                std::string value = argv[++i]; size_t end = 0; auto number = std::stoul(value, &end);
                if(end != value.size() || number == 0 || number > 10000) throw std::runtime_error("Invalid frame count.");
                frames = uint32_t(number);
            } else throw std::runtime_error("Unknown/incomplete option; use --help.");
        }
        std::signal(SIGINT,consoleEvent); std::signal(SIGTERM,consoleEvent);
        Application app; app.createInstance(); app.createVulkan(); app.createSession();
        if(probe) { std::cout << "OpenXR/Vulkan device and session negotiation passed; no frames tested.\n"; return 0; }
        app.createSwapchain();
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
        while(!app.exit && !interrupted && app.submitted < frames && std::chrono::steady_clock::now() < deadline) {
            app.events();
            if(app.running && !app.exit) app.frame(); else std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        std::cout << "Submitted stereo frames: " << app.submitted << '/' << frames << '\n';
        return app.submitted >= frames ? 0 : 2;
    } catch(const std::exception &error) { std::cerr << "PCVR bring-up: " << error.what() << '\n'; return 1; }
}
