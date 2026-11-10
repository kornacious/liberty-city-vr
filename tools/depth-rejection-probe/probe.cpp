#include <jni.h>
#include <vulkan/vulkan.h>
#include <dlfcn.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "RenderDocFrameCapture.h"

namespace {
constexpr uint32_t Width=1024,Height=1024,Eyes=2,Layers=16,Grid=16;
constexpr unsigned Warmup=4,Samples=8;
const uint32_t Vert[]=
#include "control_vert.h"
;
const uint32_t Frag[]=
#include "control_frag.h"
;
const uint32_t TransitionNoKill[]=
#include "transition_0.h"
;
const uint32_t TransitionKill[]=
#include "transition_1.h"
;
template<class T>T Init(VkStructureType type){T out{};out.sType=type;return out;}
void Check(VkResult result,const char *operation){
    if(result!=VK_SUCCESS)throw std::runtime_error(std::string(operation)+" failed (VkResult "+std::to_string(result)+")");
}
struct Params {float shape[4],colour[4];};
static_assert(sizeof(Params)==32,"vertex push constant layout");
std::string Quote(const std::string &s){
    std::string out="\"";for(char c:s){if(c=='\\'||c=='\"')out+='\\';if(c=='\n')out+="\\n";else out+=c;}return out+'\"';
}
struct Image {VkImage image{};VkDeviceMemory memory{};VkImageView view{};};
class Probe {
public:
    VkInstance instance{};VkPhysicalDevice physical{};VkDevice device{};VkQueue queue{};uint32_t family=0,bits=0;
    VkPhysicalDeviceProperties properties{};
    VkCommandPool pool{};VkCommandBuffer command{};VkFence fence{};
    VkRenderPass pass{};VkFramebuffer framebuffer{};Image colour,depth;
    VkPipelineLayout layout{};VkPipeline pipelines[2]{},triggers[8]{};VkQueryPool queries{};
    VkBuffer readback{};VkDeviceMemory readMemory{};void *mapped=nullptr;
    PFN_vkCmdBeginDebugUtilsLabelEXT beginLabel=nullptr;PFN_vkCmdEndDebugUtilsLabelEXT endLabel=nullptr;
    questcapture::RenderDocFrame capture;
    ~Probe(){
        if(device){
            vkDeviceWaitIdle(device);
            if(mapped)vkUnmapMemory(device,readMemory);
            if(readback)vkDestroyBuffer(device,readback,nullptr);if(readMemory)vkFreeMemory(device,readMemory,nullptr);
            for(auto p:pipelines)if(p)vkDestroyPipeline(device,p,nullptr);
            for(auto p:triggers)if(p)vkDestroyPipeline(device,p,nullptr);
            if(layout)vkDestroyPipelineLayout(device,layout,nullptr);
            if(framebuffer)vkDestroyFramebuffer(device,framebuffer,nullptr);if(pass)vkDestroyRenderPass(device,pass,nullptr);
            for(auto i:{colour,depth}){if(i.view)vkDestroyImageView(device,i.view,nullptr);if(i.image)vkDestroyImage(device,i.image,nullptr);if(i.memory)vkFreeMemory(device,i.memory,nullptr);}
            if(queries)vkDestroyQueryPool(device,queries,nullptr);if(fence)vkDestroyFence(device,fence,nullptr);
            if(pool)vkDestroyCommandPool(device,pool,nullptr);vkDestroyDevice(device,nullptr);
        }
        if(instance)vkDestroyInstance(instance,nullptr);
    }
    uint32_t Memory(uint32_t mask,VkMemoryPropertyFlags required){
        VkPhysicalDeviceMemoryProperties p{};vkGetPhysicalDeviceMemoryProperties(physical,&p);
        for(uint32_t i=0;i<p.memoryTypeCount;++i)if((mask&(1u<<i))&&(p.memoryTypes[i].propertyFlags&required)==required)return i;
        throw std::runtime_error("Required memory type unavailable");
    }
    Image MakeImage(VkFormat format,VkImageUsageFlags usage,VkImageAspectFlags aspect){
        Image out;auto i=Init<VkImageCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
        i.imageType=VK_IMAGE_TYPE_2D;i.format=format;i.extent={Width,Height,1};i.mipLevels=1;i.arrayLayers=Eyes;
        i.samples=VK_SAMPLE_COUNT_1_BIT;i.tiling=VK_IMAGE_TILING_OPTIMAL;i.usage=usage;i.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        Check(vkCreateImage(device,&i,nullptr,&out.image),"create image");
        VkMemoryRequirements requirements{};vkGetImageMemoryRequirements(device,out.image,&requirements);
        auto a=Init<VkMemoryAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);a.allocationSize=requirements.size;a.memoryTypeIndex=Memory(requirements.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        Check(vkAllocateMemory(device,&a,nullptr,&out.memory),"image memory");Check(vkBindImageMemory(device,out.image,out.memory,0),"bind image");
        auto v=Init<VkImageViewCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);v.image=out.image;v.viewType=VK_IMAGE_VIEW_TYPE_2D_ARRAY;v.format=format;v.subresourceRange={aspect,0,1,0,Eyes};
        Check(vkCreateImageView(device,&v,nullptr,&out.view),"image view");return out;
    }
    void Setup(bool transition){
        auto app=Init<VkApplicationInfo>(VK_STRUCTURE_TYPE_APPLICATION_INFO);app.pApplicationName="Liberty VR synthetic depth control";app.apiVersion=VK_API_VERSION_1_1;
        uint32_t n=0;Check(vkEnumerateInstanceExtensionProperties(nullptr,&n,nullptr),"instance extension count");std::vector<VkExtensionProperties> exts(n);
        Check(vkEnumerateInstanceExtensionProperties(nullptr,&n,exts.data()),"instance extensions");
        const char *debug=VK_EXT_DEBUG_UTILS_EXTENSION_NAME;bool hasDebug=false;for(auto &e:exts)if(!std::strcmp(e.extensionName,debug))hasDebug=true;
        auto i=Init<VkInstanceCreateInfo>(VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO);i.pApplicationInfo=&app;i.enabledExtensionCount=hasDebug?1:0;i.ppEnabledExtensionNames=hasDebug?&debug:nullptr;
        Check(vkCreateInstance(&i,nullptr,&instance),"create instance");
        Check(vkEnumeratePhysicalDevices(instance,&n,nullptr),"device count");if(!n)throw std::runtime_error("No Vulkan device");std::vector<VkPhysicalDevice> devices(n);Check(vkEnumeratePhysicalDevices(instance,&n,devices.data()),"devices");physical=devices[0];
        vkGetPhysicalDeviceProperties(physical,&properties);
        vkGetPhysicalDeviceQueueFamilyProperties(physical,&n,nullptr);std::vector<VkQueueFamilyProperties> families(n);vkGetPhysicalDeviceQueueFamilyProperties(physical,&n,families.data());
        bool found=false;for(uint32_t f=0;f<n;++f)if((families[f].queueFlags&VK_QUEUE_GRAPHICS_BIT)&&families[f].timestampValidBits){family=f;bits=families[f].timestampValidBits;found=true;break;}
        if(!found)throw std::runtime_error("No graphics queue with timestamps");
        auto multiview=Init<VkPhysicalDeviceMultiviewFeatures>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES);
        auto features=Init<VkPhysicalDeviceFeatures2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);features.pNext=&multiview;vkGetPhysicalDeviceFeatures2(physical,&features);
        if(!multiview.multiview)throw std::runtime_error("Multiview unavailable");multiview.multiviewGeometryShader=multiview.multiviewTessellationShader=VK_FALSE;
        const float priority=1;auto q=Init<VkDeviceQueueCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO);q.queueFamilyIndex=family;q.queueCount=1;q.pQueuePriorities=&priority;
        auto d=Init<VkDeviceCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);d.pNext=&multiview;d.queueCreateInfoCount=1;d.pQueueCreateInfos=&q;Check(vkCreateDevice(physical,&d,nullptr,&device),"create device");vkGetDeviceQueue(device,family,0,&queue);
        beginLabel=reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(vkGetInstanceProcAddr(instance,"vkCmdBeginDebugUtilsLabelEXT"));endLabel=reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(vkGetInstanceProcAddr(instance,"vkCmdEndDebugUtilsLabelEXT"));
        auto cp=Init<VkCommandPoolCreateInfo>(VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO);cp.queueFamilyIndex=family;cp.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;Check(vkCreateCommandPool(device,&cp,nullptr,&pool),"command pool");
        auto ca=Init<VkCommandBufferAllocateInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);ca.commandPool=pool;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;Check(vkAllocateCommandBuffers(device,&ca,&command),"command buffer");
        auto fi=Init<VkFenceCreateInfo>(VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);Check(vkCreateFence(device,&fi,nullptr,&fence),"fence");
        auto qi=Init<VkQueryPoolCreateInfo>(VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO);qi.queryType=VK_QUERY_TYPE_TIMESTAMP;qi.queryCount=2;Check(vkCreateQueryPool(device,&qi,nullptr,&queries),"timestamp pool");
        colour=MakeImage(VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,VK_IMAGE_ASPECT_COLOR_BIT);
        const VkFormat depthFormat=transition?VK_FORMAT_D24_UNORM_S8_UINT:VK_FORMAT_D32_SFLOAT;
        depth=MakeImage(depthFormat,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,VK_IMAGE_ASPECT_DEPTH_BIT|(transition?VK_IMAGE_ASPECT_STENCIL_BIT:0));
        VkAttachmentDescription attachments[2]{};
        attachments[0].format=VK_FORMAT_R8G8B8A8_UNORM;attachments[1].format=depthFormat;
        for(auto &a:attachments){a.samples=VK_SAMPLE_COUNT_1_BIT;a.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;a.storeOp=VK_ATTACHMENT_STORE_OP_STORE;a.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;a.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;a.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;}
        if(transition)attachments[1].storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].finalLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;attachments[1].finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        VkAttachmentReference colourRef{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},depthRef{1,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        VkSubpassDescription sub{};sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;sub.colorAttachmentCount=1;sub.pColorAttachments=&colourRef;sub.pDepthStencilAttachment=&depthRef;
        const uint32_t mask=3;auto mv=Init<VkRenderPassMultiviewCreateInfo>(VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO);mv.subpassCount=1;mv.pViewMasks=&mask;mv.correlationMaskCount=1;mv.pCorrelationMasks=&mask;
        VkSubpassDependency dep[2]{};dep[0].srcSubpass=VK_SUBPASS_EXTERNAL;dep[0].dstSubpass=0;dep[0].srcStageMask=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;dep[0].dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;dep[0].dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dep[1].srcSubpass=0;dep[1].dstSubpass=VK_SUBPASS_EXTERNAL;dep[1].srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;dep[1].dstStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;dep[1].srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;dep[1].dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
        auto rp=Init<VkRenderPassCreateInfo>(VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO);rp.pNext=&mv;rp.attachmentCount=2;rp.pAttachments=attachments;rp.subpassCount=1;rp.pSubpasses=&sub;rp.dependencyCount=2;rp.pDependencies=dep;Check(vkCreateRenderPass(device,&rp,nullptr,&pass),"render pass");
        const VkImageView views[]={colour.view,depth.view};auto fb=Init<VkFramebufferCreateInfo>(VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO);fb.renderPass=pass;fb.attachmentCount=2;fb.pAttachments=views;fb.width=Width;fb.height=Height;fb.layers=1;Check(vkCreateFramebuffer(device,&fb,nullptr,&framebuffer),"framebuffer");
        auto buffer=Init<VkBufferCreateInfo>(VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);buffer.size=VkDeviceSize(Width)*Height*Eyes*4;buffer.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;Check(vkCreateBuffer(device,&buffer,nullptr,&readback),"readback buffer");VkMemoryRequirements req{};vkGetBufferMemoryRequirements(device,readback,&req);
        auto mem=Init<VkMemoryAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);mem.allocationSize=req.size;mem.memoryTypeIndex=Memory(req.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);Check(vkAllocateMemory(device,&mem,nullptr,&readMemory),"readback memory");Check(vkBindBufferMemory(device,readback,readMemory,0),"bind readback");Check(vkMapMemory(device,readMemory,0,VK_WHOLE_SIZE,0,&mapped),"map readback");
        VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT,0,sizeof(Params)};auto pl=Init<VkPipelineLayoutCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);pl.pushConstantRangeCount=1;pl.pPushConstantRanges=&push;Check(vkCreatePipelineLayout(device,&pl,nullptr,&layout),"pipeline layout");
        VkShaderModule modules[4]{};const uint32_t *code[]={Vert,Frag,TransitionNoKill,TransitionKill};const size_t sizes[]={sizeof(Vert),sizeof(Frag),sizeof(TransitionNoKill),sizeof(TransitionKill)};
        for(unsigned s=0;s<4;++s){auto sm=Init<VkShaderModuleCreateInfo>(VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);sm.codeSize=sizes[s];sm.pCode=code[s];Check(vkCreateShaderModule(device,&sm,nullptr,&modules[s]),"shader module");}
        for(unsigned kind=0;kind<(transition?10u:2u);++kind){
            const bool trigger=kind>=2;const unsigned bits=trigger?kind-2:0;
            const unsigned write=trigger?(bits&1):kind;const bool blended=trigger&&(bits&2),kill=trigger&&(bits&4);
            VkPipelineShaderStageCreateInfo stages[2]{};for(unsigned s=0;s<2;++s){stages[s]=Init<VkPipelineShaderStageCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);stages[s].stage=s?VK_SHADER_STAGE_FRAGMENT_BIT:VK_SHADER_STAGE_VERTEX_BIT;stages[s].module=modules[s];stages[s].pName="main";}
            if(trigger)stages[1].module=modules[kill?3:2];
            auto vi=Init<VkPipelineVertexInputStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO);
            auto ia=Init<VkPipelineInputAssemblyStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO);ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            VkViewport viewport{0,0,float(Width),float(Height),0,1};VkRect2D scissor{{0,0},{Width,Height}};auto vp=Init<VkPipelineViewportStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO);vp.viewportCount=vp.scissorCount=1;vp.pViewports=&viewport;vp.pScissors=&scissor;
            auto rs=Init<VkPipelineRasterizationStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO);rs.polygonMode=VK_POLYGON_MODE_FILL;rs.cullMode=VK_CULL_MODE_NONE;rs.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE;rs.lineWidth=1;
            auto ms=Init<VkPipelineMultisampleStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO);ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
            auto ds=Init<VkPipelineDepthStencilStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO);ds.depthTestEnable=VK_TRUE;ds.depthWriteEnable=write;ds.depthCompareOp=VK_COMPARE_OP_LESS_OR_EQUAL;
            VkPipelineColorBlendAttachmentState target{};target.colorWriteMask=15;target.blendEnable=blended;
            target.srcColorBlendFactor=target.srcAlphaBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;
            target.dstColorBlendFactor=target.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            target.colorBlendOp=target.alphaBlendOp=VK_BLEND_OP_ADD;
            auto blend=Init<VkPipelineColorBlendStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO);blend.attachmentCount=1;blend.pAttachments=&target;
            auto pi=Init<VkGraphicsPipelineCreateInfo>(VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO);pi.stageCount=2;pi.pStages=stages;pi.pVertexInputState=&vi;pi.pInputAssemblyState=&ia;pi.pViewportState=&vp;pi.pRasterizationState=&rs;pi.pMultisampleState=&ms;pi.pDepthStencilState=&ds;pi.pColorBlendState=&blend;pi.layout=layout;pi.renderPass=pass;
            Check(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&pi,nullptr,trigger?&triggers[bits]:&pipelines[write]),"pipeline");
        }
        for(auto m:modules)vkDestroyShaderModule(device,m,nullptr);
        for(const char *name:{"libVkLayer_GLES_RenderDoc.so","libVkLayer_RenderDoc.so","librenderdoc.so"}){
            void *module=dlopen(name,RTLD_NOW|RTLD_NOLOAD);if(!module)continue;
            auto get=reinterpret_cast<pRENDERDOC_GetAPI>(dlsym(module,"RENDERDOC_GetAPI"));RENDERDOC_API_1_4_0 *api=nullptr;
            if(get&&get(eRENDERDOC_API_Version_1_4_0,reinterpret_cast<void**>(&api))){capture.SetApi(api);if(capture.Available())break;}
            dlclose(module);
        }
    }
    void Label(const char *text){if(beginLabel&&endLabel){auto label=Init<VkDebugUtilsLabelEXT>(VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT);label.pLabelName=text;beginLabel(command,&label);}}
    void EndLabel(){if(beginLabel&&endLabel)endLabel(command);}
    double Frame(unsigned variant,bool transition){
        Check(vkResetCommandBuffer(command,0),"reset command");auto cb=Init<VkCommandBufferBeginInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);cb.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;Check(vkBeginCommandBuffer(command,&cb),"begin command");
        vkCmdResetQueryPool(command,queries,0,2);vkCmdWriteTimestamp(command,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,queries,0);
        VkClearValue clear[2]{};clear[0].color.float32[3]=1;clear[1].depthStencil.depth=1;
        auto rp=Init<VkRenderPassBeginInfo>(VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO);rp.renderPass=pass;rp.framebuffer=framebuffer;rp.renderArea.extent={Width,Height};rp.clearValueCount=2;rp.pClearValues=clear;
        vkCmdBeginRenderPass(command,&rp,VK_SUBPASS_CONTENTS_INLINE);
        Params p{};
        if(transition){
            Label("A: opaque seed before state transition");vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelines[1]);
            p=Params{{.95f,1,0,0},{.1f,.1f,.85f,1}};vkCmdPushConstants(command,layout,VK_SHADER_STAGE_VERTEX_BIT,0,sizeof(p),&p);vkCmdDraw(command,6,1,0,0);EndLabel();
            Label("B: trigger - blend/write/kill factorial state");vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,triggers[variant]);
            p=Params{{.9f,1,0,0},{.1f,.85f,.1f,1}};vkCmdPushConstants(command,layout,VK_SHADER_STAGE_VERTEX_BIT,0,sizeof(p),&p);vkCmdDraw(command,6,1,0,0);EndLabel();
            Label("C: opaque occluder after state transition");vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelines[1]);
            p=Params{{.1f,1,0,0},{.85f,.1f,.1f,1}};vkCmdPushConstants(command,layout,VK_SHADER_STAGE_VERTEX_BIT,0,sizeof(p),&p);vkCmdDraw(command,6,1,0,0);EndLabel();
        }else{
            Label("Foreground control: 2 triangles");vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelines[variant==2?0:1]);
            p=Params{{.1f,1,variant==0?3.f:0.f,0},{.85f,.1f,.1f,1}};vkCmdPushConstants(command,layout,VK_SHADER_STAGE_VERTEX_BIT,0,sizeof(p),&p);vkCmdDraw(command,6,1,0,0);EndLabel();
        }
        Label("Background: 16 identical 16x16 grids, far to near");vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelines[1]);
        for(unsigned layer=0;layer<Layers;++layer){
            p=Params{{.85f-float(layer)*.02f,float(Grid),0,0},{.1f,.1f,.85f,1}};
            vkCmdPushConstants(command,layout,VK_SHADER_STAGE_VERTEX_BIT,0,sizeof(p),&p);vkCmdDraw(command,Grid*Grid*6,1,0,0);
        }
        EndLabel();vkCmdEndRenderPass(command);vkCmdWriteTimestamp(command,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,queries,1);
        VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,Eyes};copy.imageExtent={Width,Height,1};vkCmdCopyImageToBuffer(command,colour.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,readback,1,&copy);
        auto host=Init<VkBufferMemoryBarrier>(VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER);host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;host.srcQueueFamilyIndex=host.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;host.buffer=readback;host.size=VK_WHOLE_SIZE;
        vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&host,0,nullptr);
        Check(vkEndCommandBuffer(command),"end command");Check(vkResetFences(device,1,&fence),"reset fence");auto submit=Init<VkSubmitInfo>(VK_STRUCTURE_TYPE_SUBMIT_INFO);submit.commandBufferCount=1;submit.pCommandBuffers=&command;Check(vkQueueSubmit(queue,1,&submit,fence),"submit");Check(vkWaitForFences(device,1,&fence,VK_TRUE,10000000000ULL),"frame timeout");
        uint64_t times[2]{};Check(vkGetQueryPoolResults(device,queries,0,2,sizeof(times),times,sizeof(uint64_t),VK_QUERY_RESULT_64_BIT),"timestamps");uint64_t delta=times[1]-times[0];if(bits<64)delta&=(uint64_t(1)<<bits)-1;
        return double(delta)*properties.limits.timestampPeriod/1000000;
    }
    bool Pixels(unsigned variant,bool transition){
        auto bytes=static_cast<const uint8_t*>(mapped);
        for(unsigned eye=0;eye<Eyes;++eye)for(unsigned y:{Height/4,Height/2,Height*3/4})for(unsigned x:{Width/4,Width/2,Width*3/4}){
            const auto *pixel=bytes+((size_t(eye)*Height+y)*Width+x)*4;
            if((transition||variant==1)?pixel[0]<pixel[2]*4:pixel[2]<pixel[0]*4)return false;
        }
        return true;
    }
};
std::string Run(const std::string &directory,bool transition){
    Probe p;p.Setup(transition);std::ostringstream report;report.precision(10);
    report<<"{\n\"schema\":2,\"synthetic\":true,\"suite\":"<<Quote(transition?"transition":"baseline")<<",\"width\":"<<Width<<",\"height\":"<<Height<<",\"views\":2,\"samples\":1,\"depth_format\":"<<Quote(transition?"D24_UNORM_S8_UINT":"D32_SFLOAT")<<",\"depth_clear\":1,\"compare\":\"LESS_OR_EQUAL\",\"background_layers\":"<<Layers<<",\"background_grid\":"<<Grid<<",\"draws_per_case\":"<<(transition?19:17)<<",\"triangles_per_case\":"<<(transition?8198:8194)<<",\"renderdoc_attached\":"<<(p.capture.Available()?"true":"false")<<",\"cases\":[\n";
    const char *names[]={"open","occluded","no_depth_write"};bool all=true;
    for(unsigned variant=0;variant<(transition?8u:3u);++variant){
        const std::string name=transition?"transition_b"+std::to_string((variant>>1)&1)+"_w"+std::to_string(variant&1)+"_k"+std::to_string((variant>>2)&1):names[variant];
        for(unsigned f=0;f<Warmup;++f)p.Frame(variant,transition);
        std::vector<double> times;for(unsigned f=0;f<Samples;++f)times.push_back(p.Frame(variant,transition));
        const bool pixels=p.Pixels(variant,transition);all=all&&pixels;
        std::string captureStatus="not_attached",capturePath;
        if(p.capture.Available()){
            const auto prefix=directory+"/depth-control-"+name;
            const auto began=p.capture.Begin(RENDERDOC_DEVICEPOINTER_FROM_VKINSTANCE(p.instance),prefix.c_str());
            if(began==questcapture::RenderDocFrame::Started){
                try{p.Frame(variant,transition);}catch(...){p.capture.End(false);throw;}
                const auto ended=p.capture.End(true);captureStatus=ended==questcapture::RenderDocFrame::Saved?"saved":"failed";capturePath=p.capture.Path();
            }else captureStatus="start_failed";
        }
        auto sorted=times;std::sort(sorted.begin(),sorted.end());
        if(variant)report<<",\n";
        report<<"{\"name\":"<<Quote(name)<<",\"pixels_pass\":"<<(pixels?"true":"false");
        if(transition)report<<",\"trigger_blend\":"<<((variant&2)?"true":"false")<<",\"trigger_depth_write\":"<<((variant&1)?"true":"false")<<",\"trigger_discard\":"<<((variant&4)?"true":"false");
        report<<",\"gpu_median_ms\":"<<(sorted[3]+sorted[4])*.5<<",\"gpu_samples_ms\":[";
        for(unsigned i=0;i<times.size();++i){if(i)report<<',';report<<times[i];}
        report<<"],\"capture\":"<<Quote(captureStatus)<<",\"capture_path\":"<<Quote(capturePath)<<'}';
    }
    report<<"\n],\"pixels_pass\":"<<(all?"true":"false")<<"}\n";
    std::ofstream file(directory+"/results.json");file<<report.str();file.close();if(!file)throw std::runtime_error("Cannot save results");
    return report.str();
}
}
extern "C" JNIEXPORT jstring JNICALL Java_com_libertyvr_depthprobe_DepthProbeActivity_runProbe(JNIEnv *env,jclass,jstring path,jboolean transition){
    const char *p=env->GetStringUTFChars(path,nullptr);std::string directory=p;env->ReleaseStringUTFChars(path,p);
    try{return env->NewStringUTF(Run(directory,transition!=JNI_FALSE).c_str());}
    catch(const std::exception &e){std::string error="Depth control failed: "+std::string(e.what());std::ofstream(directory+"/error.txt")<<error;return env->NewStringUTF(error.c_str());}
}
