#define VK_USE_PLATFORM_WIN32_KHR
#define NOMINMAX
#include "DesktopMirror.h"
#include "imgui.h"
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <vector>
#include <windows.h>
namespace Desktop {
namespace {
const uint32_t mirrorVert[] =
#include "desktop_mirror_vert.h"
    ;
const uint32_t mirrorFrag[] =
#include "desktop_mirror_frag.h"
    ;
const uint32_t uiVert[] =
#include "desktop_ui_vert.h"
    ;
const uint32_t uiFrag[] =
#include "desktop_ui_frag.h"
    ;
const uint32_t overlayFrag[] =
#include "desktop_overlay_frag.h"
    ;
void Check(VkResult result, const char *what) {
  if (result != VK_SUCCESS)
    throw std::runtime_error(std::string("Desktop ") + what +
                             " failed: " + std::to_string(result));
}
struct Buffer {
  VkBuffer buffer{};
  VkDeviceMemory memory{};
  void *mapped = nullptr;
  VkDeviceSize size = 0;
};
struct Frame {
  VkCommandBuffer command{};
  VkFence fence{};
  VkSemaphore acquired{};
  VkDescriptorSet eye{}, overlaySet{};
  Buffer vertices, indices, overlayStaging;
  VkImage overlayImage{};
  VkImageView overlayView{};
  VkDeviceMemory overlayMemory{};
  unsigned overlayW = 0, overlayH = 0;
  bool overlayInitialized = false;
};
struct Image {
  VkImage image{};
  VkImageView view{};
  VkFramebuffer framebuffer{};
  VkSemaphore ready{};
};
struct State {
  VkInstance instance{};
  VkPhysicalDevice physical{};
  VkDevice device{};
  VkQueue queue{};
  unsigned family = 0;
  VkSurfaceKHR surface{};
  VkSwapchainKHR swapchain{};
  VkRenderPass pass{};
  VkPipeline mirror{}, ui{}, overlay{};
  VkDescriptorSetLayout descriptors{};
  VkDescriptorPool descriptorPool{};
  VkPipelineLayout layout{};
  VkCommandPool pool{};
  VkSampler sampler{};
  VkImage font{};
  VkImageView fontView{};
  VkDeviceMemory fontMemory{};
  VkDescriptorSet fontSet{};
  std::array<Frame, 2> frames{};
  std::vector<Image> images;
  unsigned next = 0, width = 0, height = 0;
  VkFormat format{};
  bool srgb = false, snapshotSupported = false;
  Picture picture;
  unsigned presented = 0, skipped = 0;
  std::string snapshot;
  std::vector<unsigned char> overlayPixels;
  unsigned overlayW = 0, overlayH = 0;
  bool menu = false;
} g;
unsigned Memory(unsigned bits, VkMemoryPropertyFlags flags) {
  VkPhysicalDeviceMemoryProperties properties;
  vkGetPhysicalDeviceMemoryProperties(g.physical, &properties);
  for (unsigned i = 0; i < properties.memoryTypeCount; ++i)
    if ((bits & (1u << i)) &&
        (properties.memoryTypes[i].propertyFlags & flags) == flags)
      return i;
  throw std::runtime_error("Desktop GPU memory type unavailable.");
}
void Free(Buffer &buffer) {
  if (buffer.mapped)
    vkUnmapMemory(g.device, buffer.memory);
  if (buffer.buffer)
    vkDestroyBuffer(g.device, buffer.buffer, nullptr);
  if (buffer.memory)
    vkFreeMemory(g.device, buffer.memory, nullptr);
  buffer = {};
}
void Allocate(Buffer &buffer, VkDeviceSize size, VkBufferUsageFlags usage) {
  if (buffer.size >= size)
    return;
  Free(buffer);
  buffer.size = std::max<VkDeviceSize>(size, 4096);
  VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  info.size = buffer.size;
  info.usage = usage;
  Check(vkCreateBuffer(g.device, &info, nullptr, &buffer.buffer), "buffer");
  VkMemoryRequirements requirements;
  vkGetBufferMemoryRequirements(g.device, buffer.buffer, &requirements);
  VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  alloc.allocationSize = requirements.size;
  alloc.memoryTypeIndex = Memory(requirements.memoryTypeBits,
                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  Check(vkAllocateMemory(g.device, &alloc, nullptr, &buffer.memory),
        "buffer memory");
  Check(vkBindBufferMemory(g.device, buffer.buffer, buffer.memory, 0),
        "buffer binding");
  Check(vkMapMemory(g.device, buffer.memory, 0, buffer.size, 0, &buffer.mapped),
        "buffer mapping");
}
VkImageView View(VkImage image, VkFormat format) {
  VkImageViewCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  info.image = image;
  info.format = format;
  info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  info.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  VkImageView view;
  Check(vkCreateImageView(g.device, &info, nullptr, &view), "image view");
  return view;
}
void Update(VkDescriptorSet set, VkImageView view,
            VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
  VkDescriptorImageInfo image{g.sampler, view, layout};
  VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
  write.dstSet = set;
  write.dstBinding = 0;
  write.descriptorCount = 1;
  write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  write.pImageInfo = &image;
  vkUpdateDescriptorSets(g.device, 1, &write, 0, nullptr);
}
VkShaderModule Shader(const uint32_t *data, size_t size) {
  VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
  info.codeSize = size;
  info.pCode = data;
  VkShaderModule module;
  Check(vkCreateShaderModule(g.device, &info, nullptr, &module), "shader");
  return module;
}
VkPipeline Pipeline(int kind) {
  const bool ui = kind == 1;
  VkShaderModule vertex = ui ? Shader(uiVert, sizeof(uiVert))
                             : Shader(mirrorVert, sizeof(mirrorVert));
  VkShaderModule fragment = ui ? Shader(uiFrag, sizeof(uiFrag))
                            : kind == 2
                                ? Shader(overlayFrag, sizeof(overlayFrag))
                                : Shader(mirrorFrag, sizeof(mirrorFrag));
  VkPipelineShaderStageCreateInfo stages[2]{};
  for (auto &s : stages) {
    s.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    s.pName = "main";
  }
  stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  stages[0].module = vertex;
  stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[1].module = fragment;
  VkVertexInputBindingDescription binding{0, sizeof(ImDrawVert),
                                          VK_VERTEX_INPUT_RATE_VERTEX};
  VkVertexInputAttributeDescription attributes[] = {
      {0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, pos)},
      {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, uv)},
      {2, 0, VK_FORMAT_R8G8B8A8_UNORM, offsetof(ImDrawVert, col)}};
  VkPipelineVertexInputStateCreateInfo input{
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
  if (ui) {
    input.vertexBindingDescriptionCount = 1;
    input.pVertexBindingDescriptions = &binding;
    input.vertexAttributeDescriptionCount = 3;
    input.pVertexAttributeDescriptions = attributes;
  }
  VkPipelineInputAssemblyStateCreateInfo assembly{
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
  assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  VkPipelineViewportStateCreateInfo viewport{
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
  viewport.viewportCount = viewport.scissorCount = 1;
  VkPipelineRasterizationStateCreateInfo raster{
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
  raster.polygonMode = VK_POLYGON_MODE_FILL;
  raster.lineWidth = 1;
  raster.cullMode = VK_CULL_MODE_NONE;
  VkPipelineMultisampleStateCreateInfo multisample{
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
  multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  VkPipelineColorBlendAttachmentState blend{};
  blend.colorWriteMask = 15;
  blend.blendEnable = kind != 0;
  blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  blend.colorBlendOp = VK_BLEND_OP_ADD;
  blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  blend.alphaBlendOp = VK_BLEND_OP_ADD;
  VkPipelineColorBlendStateCreateInfo color{
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
  color.attachmentCount = 1;
  color.pAttachments = &blend;
  VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                    VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic{
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
  dynamic.dynamicStateCount = 2;
  dynamic.pDynamicStates = dynamicStates;
  VkGraphicsPipelineCreateInfo info{
      VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
  info.stageCount = 2;
  info.pStages = stages;
  info.pVertexInputState = &input;
  info.pInputAssemblyState = &assembly;
  info.pViewportState = &viewport;
  info.pRasterizationState = &raster;
  info.pMultisampleState = &multisample;
  info.pColorBlendState = &color;
  info.pDynamicState = &dynamic;
  info.layout = g.layout;
  info.renderPass = g.pass;
  VkPipeline result{};
  auto status = vkCreateGraphicsPipelines(g.device, VK_NULL_HANDLE, 1, &info,
                                          nullptr, &result);
  vkDestroyShaderModule(g.device, vertex, nullptr);
  vkDestroyShaderModule(g.device, fragment, nullptr);
  Check(status, "pipeline");
  return result;
}
void DestroySwap() {
  for (auto &image : g.images) {
    if (image.framebuffer)
      vkDestroyFramebuffer(g.device, image.framebuffer, nullptr);
    if (image.view)
      vkDestroyImageView(g.device, image.view, nullptr);
    if (image.ready)
      vkDestroySemaphore(g.device, image.ready, nullptr);
  }
  g.images.clear();
  if (g.mirror)
    vkDestroyPipeline(g.device, g.mirror, nullptr);
  if (g.ui)
    vkDestroyPipeline(g.device, g.ui, nullptr);
  if (g.overlay)
    vkDestroyPipeline(g.device, g.overlay, nullptr);
  if (g.pass)
    vkDestroyRenderPass(g.device, g.pass, nullptr);
  if (g.swapchain)
    vkDestroySwapchainKHR(g.device, g.swapchain, nullptr);
  g.mirror = {};
  g.ui = {};
  g.overlay = {};
  g.pass = {};
  g.swapchain = {};
}
bool Recreate() {
  unsigned width, height;
  ClientSize(width, height);
  if (!width || !height)
    return false;
  Check(vkDeviceWaitIdle(g.device), "resize completion");
  DestroySwap();
  VkSurfaceCapabilitiesKHR caps;
  Check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g.physical, g.surface, &caps),
        "surface capabilities");
  unsigned count = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(g.physical, g.surface, &count, nullptr);
  std::vector<VkSurfaceFormatKHR> formats(count);
  vkGetPhysicalDeviceSurfaceFormatsKHR(g.physical, g.surface, &count,
                                       formats.data());
  if (formats.empty())
    throw std::runtime_error("No desktop surface formats.");
  auto selected = formats[0];
  for (auto desired : {VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM,
                       VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_R8G8B8A8_SRGB}) {
    bool found = false;
    for (auto f : formats)
      if (f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR &&
          (f.format == desired || f.format == VK_FORMAT_UNDEFINED)) {
        selected = {desired, f.colorSpace};
        found = true;
        break;
      }
    if (found)
      break;
  }
  g.format = selected.format;
  g.srgb = g.format == VK_FORMAT_B8G8R8A8_SRGB ||
           g.format == VK_FORMAT_R8G8B8A8_SRGB;
  VkExtent2D extent = caps.currentExtent;
  if (extent.width == UINT32_MAX)
    extent = {
        std::clamp(width, caps.minImageExtent.width, caps.maxImageExtent.width),
        std::clamp(height, caps.minImageExtent.height,
                   caps.maxImageExtent.height)};
  g.width = extent.width;
  g.height = extent.height;
  vkGetPhysicalDeviceSurfacePresentModesKHR(g.physical, g.surface, &count,
                                            nullptr);
  std::vector<VkPresentModeKHR> modes(count);
  vkGetPhysicalDeviceSurfacePresentModesKHR(g.physical, g.surface, &count,
                                            modes.data());
  auto mode = VK_PRESENT_MODE_FIFO_KHR;
  for (auto want : {VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_MAILBOX_KHR})
    if (std::find(modes.begin(), modes.end(), want) != modes.end()) {
      mode = want;
      break;
    }
  VkSwapchainCreateInfoKHR swap{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
  swap.surface = g.surface;
  swap.minImageCount = std::max(3u, caps.minImageCount);
  if (caps.maxImageCount)
    swap.minImageCount = std::min(swap.minImageCount, caps.maxImageCount);
  swap.imageFormat = g.format;
  swap.imageColorSpace = selected.colorSpace;
  swap.imageExtent = extent;
  swap.imageArrayLayers = 1;
  swap.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  swap.preTransform = caps.currentTransform;
  swap.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  g.snapshotSupported =
      (caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0;
  if (g.snapshotSupported)
    swap.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  if (!(caps.supportedCompositeAlpha & swap.compositeAlpha))
    for (auto alpha : {VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
                       VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
                       VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR})
      if (caps.supportedCompositeAlpha & alpha) {
        swap.compositeAlpha = alpha;
        break;
      }
  swap.presentMode = mode;
  swap.clipped = VK_TRUE;
  Check(vkCreateSwapchainKHR(g.device, &swap, nullptr, &g.swapchain),
        "swapchain");
  VkAttachmentDescription attachment{};
  attachment.format = g.format;
  attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  VkAttachmentReference reference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &reference;
  VkSubpassDependency dependency{VK_SUBPASS_EXTERNAL,
                                 0,
                                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 0,
                                 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                 0};
  VkRenderPassCreateInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
  pass.attachmentCount = 1;
  pass.pAttachments = &attachment;
  pass.subpassCount = 1;
  pass.pSubpasses = &subpass;
  pass.dependencyCount = 1;
  pass.pDependencies = &dependency;
  Check(vkCreateRenderPass(g.device, &pass, nullptr, &g.pass), "render pass");
  g.mirror = Pipeline(0);
  g.ui = Pipeline(1);
  g.overlay = Pipeline(2);
  vkGetSwapchainImagesKHR(g.device, g.swapchain, &count, nullptr);
  std::vector<VkImage> images(count);
  vkGetSwapchainImagesKHR(g.device, g.swapchain, &count, images.data());
  g.images.resize(count);
  for (unsigned i = 0; i < count; ++i) {
    auto &image = g.images[i];
    image.image = images[i];
    image.view = View(image.image, g.format);
    VkFramebufferCreateInfo framebuffer{
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    framebuffer.renderPass = g.pass;
    framebuffer.attachmentCount = 1;
    framebuffer.pAttachments = &image.view;
    framebuffer.width = g.width;
    framebuffer.height = g.height;
    framebuffer.layers = 1;
    Check(vkCreateFramebuffer(g.device, &framebuffer, nullptr,
                              &image.framebuffer),
          "framebuffer");
    VkSemaphoreCreateInfo semaphore{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    Check(vkCreateSemaphore(g.device, &semaphore, nullptr, &image.ready),
          "presentation semaphore");
  }
  return true;
}
void Font() {
  unsigned char *pixels;
  int width, height;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  Buffer staging;
  Allocate(staging, VkDeviceSize(width) * height * 4,
           VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
  memcpy(staging.mapped, pixels, size_t(width) * height * 4);
  VkImageCreateInfo image{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  image.imageType = VK_IMAGE_TYPE_2D;
  image.format = VK_FORMAT_R8G8B8A8_UNORM;
  image.extent = {unsigned(width), unsigned(height), 1};
  image.mipLevels = image.arrayLayers = 1;
  image.samples = VK_SAMPLE_COUNT_1_BIT;
  image.tiling = VK_IMAGE_TILING_OPTIMAL;
  image.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  Check(vkCreateImage(g.device, &image, nullptr, &g.font), "font image");
  VkMemoryRequirements requirements;
  vkGetImageMemoryRequirements(g.device, g.font, &requirements);
  VkMemoryAllocateInfo memory{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  memory.allocationSize = requirements.size;
  memory.memoryTypeIndex =
      Memory(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  Check(vkAllocateMemory(g.device, &memory, nullptr, &g.fontMemory),
        "font memory");
  Check(vkBindImageMemory(g.device, g.font, g.fontMemory, 0), "font binding");
  g.fontView = View(g.font, image.format);
  VkCommandBufferAllocateInfo alloc{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  alloc.commandPool = g.pool;
  alloc.commandBufferCount = 1;
  alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  VkCommandBuffer cmd;
  Check(vkAllocateCommandBuffers(g.device, &alloc, &cmd), "font commands");
  VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  Check(vkBeginCommandBuffer(cmd, &begin), "font begin");
  VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  barrier.image = g.font;
  barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex =
      VK_QUEUE_FAMILY_IGNORED;
  barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);
  VkBufferImageCopy copy{};
  copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  copy.imageExtent = image.extent;
  vkCmdCopyBufferToImage(cmd, staging.buffer, g.font,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
  barrier.oldLayout = barrier.newLayout;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);
  Check(vkEndCommandBuffer(cmd), "font end");
  VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submit.commandBufferCount = 1;
  submit.pCommandBuffers = &cmd;
  Check(vkQueueSubmit(g.queue, 1, &submit, VK_NULL_HANDLE), "font submit");
  Check(vkQueueWaitIdle(g.queue), "font completion");
  Free(staging);
  vkFreeCommandBuffers(g.device, g.pool, 1, &cmd);
  Update(g.fontSet, g.fontView);
  ImGui::GetIO().Fonts->TexID = reinterpret_cast<void *>(1);
}
} // namespace
Picture CurrentPicture() { return g.picture; }
void SetGameOverlay(const unsigned char *pixels, int width, int height,
                    bool menu) {
  g.overlayPixels.clear();
  g.overlayW = g.overlayH = 0;
  g.menu = menu;
  if (!pixels || width <= 0 || height <= 0 || width > 2048 || height > 2048)
    return;
  g.overlayW = unsigned(width);
  g.overlayH = unsigned(height);
  g.overlayPixels.assign(pixels, pixels + size_t(width) * height * 4);
}
void RequestSnapshot(const std::string &path) { g.snapshot = path; }
unsigned PresentedFrames() { return g.presented; }
bool InitializeGpu(VkInstance instance, VkPhysicalDevice physical,
                   VkDevice device, VkQueue queue, unsigned family) {
  g.instance = instance;
  g.physical = physical;
  g.device = device;
  g.queue = queue;
  g.family = family;
  VkWin32SurfaceCreateInfoKHR surface{
      VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
  surface.hinstance = GetModuleHandleW(nullptr);
  surface.hwnd = HWND(WindowHandle());
  Check(vkCreateWin32SurfaceKHR(instance, &surface, nullptr, &g.surface),
        "Win32 surface");
  VkBool32 supported;
  Check(vkGetPhysicalDeviceSurfaceSupportKHR(physical, family, g.surface,
                                             &supported),
        "presentation support");
  if (!supported)
    throw std::runtime_error(
        "OpenXR GPU queue cannot present on this monitor.");
  VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  pool.queueFamilyIndex = family;
  pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  Check(vkCreateCommandPool(device, &pool, nullptr, &g.pool), "command pool");
  VkDescriptorSetLayoutBinding binding{
      0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
      VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
  VkDescriptorSetLayoutCreateInfo descriptors{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  descriptors.bindingCount = 1;
  descriptors.pBindings = &binding;
  Check(vkCreateDescriptorSetLayout(device, &descriptors, nullptr,
                                    &g.descriptors),
        "descriptor layout");
  VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5};
  VkDescriptorPoolCreateInfo dp{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  dp.maxSets = 5;
  dp.poolSizeCount = 1;
  dp.pPoolSizes = &size;
  Check(vkCreateDescriptorPool(device, &dp, nullptr, &g.descriptorPool),
        "descriptor pool");
  VkDescriptorSetLayout layouts[] = {g.descriptors, g.descriptors,
                                     g.descriptors, g.descriptors,
                                     g.descriptors};
  VkDescriptorSet sets[5];
  VkDescriptorSetAllocateInfo da{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
  da.descriptorPool = g.descriptorPool;
  da.descriptorSetCount = 5;
  da.pSetLayouts = layouts;
  Check(vkAllocateDescriptorSets(device, &da, sets), "descriptor sets");
  g.fontSet = sets[0];
  VkPushConstantRange push{
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 32};
  VkPipelineLayoutCreateInfo layout{
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  layout.setLayoutCount = 1;
  layout.pSetLayouts = &g.descriptors;
  layout.pushConstantRangeCount = 1;
  layout.pPushConstantRanges = &push;
  Check(vkCreatePipelineLayout(device, &layout, nullptr, &g.layout),
        "pipeline layout");
  VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  sampler.magFilter = sampler.minFilter = VK_FILTER_LINEAR;
  sampler.addressModeU = sampler.addressModeV = sampler.addressModeW =
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler.maxLod = 0;
  Check(vkCreateSampler(device, &sampler, nullptr, &g.sampler), "sampler");
  for (unsigned i = 0; i < g.frames.size(); ++i) {
    auto &f = g.frames[i];
    f.eye = sets[i + 1];
    f.overlaySet = sets[i + 3];
    VkCommandBufferAllocateInfo a{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    a.commandPool = g.pool;
    a.commandBufferCount = 1;
    a.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    Check(vkAllocateCommandBuffers(device, &a, &f.command), "frame commands");
    VkFenceCreateInfo fence{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    Check(vkCreateFence(device, &fence, nullptr, &f.fence), "frame fence");
    VkSemaphoreCreateInfo semaphore{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    Check(vkCreateSemaphore(device, &semaphore, nullptr, &f.acquired),
          "acquire semaphore");
  }
  Font();
  ConsumeResize();
  return Recreate();
}
void Present(VkImage source, VkImageView view, unsigned sourceW,
             unsigned sourceH, float theaterAspect) {
  EndUi();
  if (!g.device || !source || !view)
    return;
  unsigned w, h;
  ClientSize(w, h);
  if (!w || !h) {
    ++g.skipped;
    return;
  }
  if (ConsumeResize() || !g.swapchain) {
    if (!Recreate())
      return;
  }
  auto &frame = g.frames[g.next];
  if (vkGetFenceStatus(g.device, frame.fence) != VK_SUCCESS) {
    ++g.skipped;
    return;
  }
  unsigned index = 0;
  auto acquired = vkAcquireNextImageKHR(g.device, g.swapchain, 0,
                                        frame.acquired, VK_NULL_HANDLE, &index);
  if (acquired == VK_ERROR_OUT_OF_DATE_KHR) {
    RequestResize();
    return;
  }
  if (acquired == VK_NOT_READY || acquired == VK_TIMEOUT) {
    ++g.skipped;
    return;
  }
  if (acquired != VK_SUCCESS && acquired != VK_SUBOPTIMAL_KHR)
    Check(acquired, "acquire");
  if (acquired == VK_SUBOPTIMAL_KHR)
    RequestResize();
  auto &image = g.images[index];
  Update(frame.eye, view);
  if (!g.overlayPixels.empty()) {
    if (frame.overlayW != g.overlayW || frame.overlayH != g.overlayH) {
      if (frame.overlayView)
        vkDestroyImageView(g.device, frame.overlayView, nullptr);
      if (frame.overlayImage)
        vkDestroyImage(g.device, frame.overlayImage, nullptr);
      if (frame.overlayMemory)
        vkFreeMemory(g.device, frame.overlayMemory, nullptr);
      frame.overlayView = {};
      frame.overlayImage = {};
      frame.overlayMemory = {};
      frame.overlayInitialized = false;
      VkImageCreateInfo texture{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
      texture.imageType = VK_IMAGE_TYPE_2D;
      texture.format = VK_FORMAT_R8G8B8A8_UNORM;
      texture.extent = {g.overlayW, g.overlayH, 1};
      texture.mipLevels = texture.arrayLayers = 1;
      texture.samples = VK_SAMPLE_COUNT_1_BIT;
      texture.tiling = VK_IMAGE_TILING_OPTIMAL;
      texture.usage =
          VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
      Check(vkCreateImage(g.device, &texture, nullptr, &frame.overlayImage),
            "game overlay image");
      VkMemoryRequirements requirements;
      vkGetImageMemoryRequirements(g.device, frame.overlayImage, &requirements);
      VkMemoryAllocateInfo memory{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
      memory.allocationSize = requirements.size;
      memory.memoryTypeIndex = Memory(requirements.memoryTypeBits,
                                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
      Check(vkAllocateMemory(g.device, &memory, nullptr, &frame.overlayMemory),
            "game overlay memory");
      Check(vkBindImageMemory(g.device, frame.overlayImage, frame.overlayMemory,
                              0),
            "game overlay binding");
      frame.overlayView = View(frame.overlayImage, texture.format);
      frame.overlayW = g.overlayW;
      frame.overlayH = g.overlayH;
      Update(frame.overlaySet, frame.overlayView);
    }
    Allocate(frame.overlayStaging, g.overlayPixels.size(),
             VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    memcpy(frame.overlayStaging.mapped, g.overlayPixels.data(),
           g.overlayPixels.size());
  }
  auto draw = ImGui::GetDrawData();
  if (draw && draw->TotalVtxCount) {
    Allocate(frame.vertices,
             VkDeviceSize(draw->TotalVtxCount) * sizeof(ImDrawVert),
             VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    Allocate(frame.indices,
             VkDeviceSize(draw->TotalIdxCount) * sizeof(ImDrawIdx),
             VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    auto vertices = static_cast<ImDrawVert *>(frame.vertices.mapped);
    auto indices = static_cast<ImDrawIdx *>(frame.indices.mapped);
    for (int i = 0; i < draw->CmdListsCount; ++i) {
      auto list = draw->CmdLists[i];
      memcpy(vertices, list->VtxBuffer.Data,
             list->VtxBuffer.Size * sizeof(ImDrawVert));
      memcpy(indices, list->IdxBuffer.Data,
             list->IdxBuffer.Size * sizeof(ImDrawIdx));
      vertices += list->VtxBuffer.Size;
      indices += list->IdxBuffer.Size;
    }
  }
  Check(vkResetCommandBuffer(frame.command, 0), "reset commands");
  VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  Check(vkBeginCommandBuffer(frame.command, &begin), "begin commands");
  if (!g.overlayPixels.empty()) {
    VkImageMemoryBarrier upload{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    upload.image = frame.overlayImage;
    upload.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    upload.srcQueueFamilyIndex = upload.dstQueueFamilyIndex =
        VK_QUEUE_FAMILY_IGNORED;
    upload.oldLayout = frame.overlayInitialized
                           ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                           : VK_IMAGE_LAYOUT_UNDEFINED;
    upload.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    upload.srcAccessMask =
        frame.overlayInitialized ? VK_ACCESS_SHADER_READ_BIT : 0;
    upload.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(
        frame.command,
        frame.overlayInitialized ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                                 : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &upload);
    VkBufferImageCopy copy{};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = {g.overlayW, g.overlayH, 1};
    vkCmdCopyBufferToImage(frame.command, frame.overlayStaging.buffer,
                           frame.overlayImage,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    upload.oldLayout = upload.newLayout;
    upload.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    upload.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    upload.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(frame.command, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &upload);
    frame.overlayInitialized = true;
  }
  VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  barrier.image = source;
  barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 2};
  barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex =
      VK_QUEUE_FAMILY_IGNORED;
  barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  vkCmdPipelineBarrier(frame.command,
                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);
  VkClearValue black{};
  black.color.float32[3] = 1;
  VkRenderPassBeginInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
  pass.renderPass = g.pass;
  pass.framebuffer = image.framebuffer;
  pass.renderArea.extent = {g.width, g.height};
  pass.clearValueCount = 1;
  pass.pClearValues = &black;
  vkCmdBeginRenderPass(frame.command, &pass, VK_SUBPASS_CONTENTS_INLINE);
  g.picture =
      theaterAspect > 0
          ? FitTheater(float(sourceW), float(sourceH), float(g.width),
                       float(g.height), theaterAspect, Settings().framing)
          : FitPicture(float(sourceW), float(sourceH), float(g.width),
                       float(g.height), Settings().framing);
  auto &p = g.picture;
  VkViewport viewport{p.x, p.y, p.width, p.height, 0, 1};
  VkRect2D scissor{{0, 0}, {g.width, g.height}};
  vkCmdSetViewport(frame.command, 0, 1, &viewport);
  vkCmdSetScissor(frame.command, 0, 1, &scissor);
  vkCmdBindPipeline(frame.command, VK_PIPELINE_BIND_POINT_GRAPHICS, g.mirror);
  vkCmdBindDescriptorSets(frame.command, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          g.layout, 0, 1, &frame.eye, 0, nullptr);
  float push[8] = {
      p.u, p.v, p.du, p.dv, float(Settings().eye), g.srgb ? 1.f : 0.f, 0, 0};
  vkCmdPushConstants(frame.command, g.layout,
                     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                     0, sizeof(push), push);
  vkCmdDraw(frame.command, 3, 1, 0, 0);
  if (!g.overlayPixels.empty()) {
    const float scale = std::min(1.f, std::min(float(g.width) / g.overlayW,
                                               float(g.height) / g.overlayH));
    const float width = g.overlayW * scale, height = g.overlayH * scale;
    viewport = {(g.width - width) * .5f,
                g.menu ? (g.height - height) * .5f : g.height - height,
                width,
                height,
                0,
                1};
    vkCmdSetViewport(frame.command, 0, 1, &viewport);
    vkCmdBindPipeline(frame.command, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      g.overlay);
    vkCmdBindDescriptorSets(frame.command, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            g.layout, 0, 1, &frame.overlaySet, 0, nullptr);
    push[0] = push[1] = 0;
    push[2] = push[3] = 1;
    vkCmdPushConstants(frame.command, g.layout,
                       VK_SHADER_STAGE_VERTEX_BIT |
                           VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(push), push);
    vkCmdDraw(frame.command, 3, 1, 0, 0);
  }
  if (draw && draw->TotalVtxCount) {
    viewport = {0, 0, float(g.width), float(g.height), 0, 1};
    vkCmdSetViewport(frame.command, 0, 1, &viewport);
    vkCmdBindPipeline(frame.command, VK_PIPELINE_BIND_POINT_GRAPHICS, g.ui);
    vkCmdBindDescriptorSets(frame.command, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            g.layout, 0, 1, &g.fontSet, 0, nullptr);
    const VkDeviceSize zero = 0;
    vkCmdBindVertexBuffers(frame.command, 0, 1, &frame.vertices.buffer, &zero);
    vkCmdBindIndexBuffer(frame.command, frame.indices.buffer, 0,
                         sizeof(ImDrawIdx) == 2 ? VK_INDEX_TYPE_UINT16
                                                : VK_INDEX_TYPE_UINT32);
    push[0] = 2.f / g.width;
    push[1] = 2.f / g.height;
    push[2] = push[3] = -1;
    vkCmdPushConstants(frame.command, g.layout,
                       VK_SHADER_STAGE_VERTEX_BIT |
                           VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(push), push);
    unsigned firstIndex = 0;
    int firstVertex = 0;
    for (int i = 0; i < draw->CmdListsCount; ++i) {
      auto list = draw->CmdLists[i];
      for (int j = 0; j < list->CmdBuffer.Size; ++j) {
        auto &cmd = list->CmdBuffer[j];
        if (cmd.UserCallback)
          cmd.UserCallback(list, &cmd);
        else {
          int x = std::clamp(int(cmd.ClipRect.x), 0, int(g.width)),
              y = std::clamp(int(cmd.ClipRect.y), 0, int(g.height));
          int right = std::clamp(int(cmd.ClipRect.z), x, int(g.width)),
              bottom = std::clamp(int(cmd.ClipRect.w), y, int(g.height));
          if (right > x && bottom > y) {
            scissor = {{x, y}, {unsigned(right - x), unsigned(bottom - y)}};
            vkCmdSetScissor(frame.command, 0, 1, &scissor);
            vkCmdDrawIndexed(frame.command, cmd.ElemCount, 1, firstIndex,
                             firstVertex, 0);
          }
        }
        firstIndex += cmd.ElemCount;
      }
      firstVertex += list->VtxBuffer.Size;
    }
  }
  vkCmdEndRenderPass(frame.command);
  std::swap(barrier.oldLayout, barrier.newLayout);
  barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  vkCmdPipelineBarrier(frame.command, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0,
                       nullptr, 0, nullptr, 1, &barrier);
  Buffer snapshot;
  if (!g.snapshot.empty() && g.snapshotSupported) {
    Allocate(snapshot, VkDeviceSize(g.width) * g.height * 4,
             VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    VkImageMemoryBarrier read{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    read.image = image.image;
    read.srcQueueFamilyIndex = read.dstQueueFamilyIndex =
        VK_QUEUE_FAMILY_IGNORED;
    read.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    read.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    read.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    read.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    read.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(
        frame.command, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &read);
    VkBufferImageCopy copy{};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = {g.width, g.height, 1};
    vkCmdCopyImageToBuffer(frame.command, image.image,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           snapshot.buffer, 1, &copy);
    std::swap(read.oldLayout, read.newLayout);
    read.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    read.dstAccessMask = 0;
    vkCmdPipelineBarrier(frame.command, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &read);
    VkBufferMemoryBarrier host{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    host.buffer = snapshot.buffer;
    host.size = VK_WHOLE_SIZE;
    host.srcQueueFamilyIndex = host.dstQueueFamilyIndex =
        VK_QUEUE_FAMILY_IGNORED;
    host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    vkCmdPipelineBarrier(frame.command, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1, &host, 0,
                         nullptr);
  }
  Check(vkEndCommandBuffer(frame.command), "end commands");
  VkPipelineStageFlags wait = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submit.waitSemaphoreCount = 1;
  submit.pWaitSemaphores = &frame.acquired;
  submit.pWaitDstStageMask = &wait;
  submit.commandBufferCount = 1;
  submit.pCommandBuffers = &frame.command;
  submit.signalSemaphoreCount = 1;
  submit.pSignalSemaphores = &image.ready;
  Check(vkResetFences(g.device, 1, &frame.fence), "reset fence");
  Check(vkQueueSubmit(g.queue, 1, &submit, frame.fence), "submit");
  if (snapshot.buffer) {
    Check(vkWaitForFences(g.device, 1, &frame.fence, VK_TRUE, UINT64_MAX),
          "snapshot completion");
    auto bytes = static_cast<unsigned char *>(snapshot.mapped);
    if (g.format == VK_FORMAT_R8G8B8A8_UNORM ||
        g.format == VK_FORMAT_R8G8B8A8_SRGB)
      for (size_t pixel = 0; pixel < size_t(g.width) * g.height; ++pixel)
        std::swap(bytes[pixel * 4], bytes[pixel * 4 + 2]);
    BITMAPFILEHEADER file{};
    file.bfType = 0x4D42;
    file.bfOffBits = sizeof(file) + sizeof(BITMAPINFOHEADER);
    file.bfSize = file.bfOffBits + g.width * g.height * 4;
    BITMAPINFOHEADER info{};
    info.biSize = sizeof(info);
    info.biWidth = LONG(g.width);
    info.biHeight = -LONG(g.height);
    info.biPlanes = 1;
    info.biBitCount = 32;
    info.biCompression = BI_RGB;
    auto output = _wfopen(std::filesystem::u8path(g.snapshot).c_str(), L"wb");
    if (!output)
      throw std::runtime_error("Cannot write desktop snapshot.");
    fwrite(&file, sizeof(file), 1, output);
    fwrite(&info, sizeof(info), 1, output);
    fwrite(bytes, size_t(g.width) * g.height * 4, 1, output);
    fclose(output);
    Free(snapshot);
    g.snapshot.clear();
  }
  VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
  present.waitSemaphoreCount = 1;
  present.pWaitSemaphores = &image.ready;
  present.swapchainCount = 1;
  present.pSwapchains = &g.swapchain;
  present.pImageIndices = &index;
  auto result = vkQueuePresentKHR(g.queue, &present);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    RequestResize();
  else
    Check(result, "present");
  g.next = (g.next + 1) % g.frames.size();
  ++g.presented;
}
void ShutdownGpu() {
  if (!g.device)
    return;
  vkDeviceWaitIdle(g.device);
  DestroySwap();
  for (auto &frame : g.frames) {
    Free(frame.vertices);
    Free(frame.indices);
    Free(frame.overlayStaging);
    if (frame.overlayView)
      vkDestroyImageView(g.device, frame.overlayView, nullptr);
    if (frame.overlayImage)
      vkDestroyImage(g.device, frame.overlayImage, nullptr);
    if (frame.overlayMemory)
      vkFreeMemory(g.device, frame.overlayMemory, nullptr);
    if (frame.fence)
      vkDestroyFence(g.device, frame.fence, nullptr);
    if (frame.acquired)
      vkDestroySemaphore(g.device, frame.acquired, nullptr);
  }
  if (g.fontView)
    vkDestroyImageView(g.device, g.fontView, nullptr);
  if (g.font)
    vkDestroyImage(g.device, g.font, nullptr);
  if (g.fontMemory)
    vkFreeMemory(g.device, g.fontMemory, nullptr);
  if (g.sampler)
    vkDestroySampler(g.device, g.sampler, nullptr);
  if (g.descriptorPool)
    vkDestroyDescriptorPool(g.device, g.descriptorPool, nullptr);
  if (g.layout)
    vkDestroyPipelineLayout(g.device, g.layout, nullptr);
  if (g.descriptors)
    vkDestroyDescriptorSetLayout(g.device, g.descriptors, nullptr);
  if (g.pool)
    vkDestroyCommandPool(g.device, g.pool, nullptr);
  if (g.surface)
    vkDestroySurfaceKHR(g.instance, g.surface, nullptr);
  std::fprintf(stderr,
               "Desktop mirror: %u presented, %u skipped (nonblocking).\n",
               g.presented, g.skipped);
  g = {};
}
} // namespace Desktop
