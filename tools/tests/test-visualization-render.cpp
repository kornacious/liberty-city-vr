// Executes production edge submission/queue replay and depth/blend policy.
#include "../../librw/src/vulkan/DebugVisualization.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <vector>
using uint32=uint32_t; using uint16=uint16_t; using uint64=uint64_t;
using VkPipeline=uint64; using VkBuffer=uint64; using VkDeviceMemory=uint64;
using VkDescriptorSet=uint64; using VkCommandBuffer=uint64; using VkDeviceSize=uint64;
namespace DebugVisualization=rw::vulkan::DebugVisualization;
#define nil nullptr
constexpr uint64 VK_NULL_HANDLE=0;
enum { VK_PRIMITIVE_TOPOLOGY_LINE_LIST=1,VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP=4,
    VK_BUFFER_USAGE_INDEX_BUFFER_BIT=1,VK_PIPELINE_BIND_POINT_GRAPHICS=0,
    VK_INDEX_TYPE_UINT16=0,VK_SHADER_STAGE_VERTEX_BIT=1,VK_SHADER_STAGE_FRAGMENT_BIT=2,
    ALPHAALWAYS=1,SHADER_WORLD=0,SHADER_SKIN=3,VK_FALSE=0,VK_TRUE=1,VK_BLEND_FACTOR_ONE=1 };
struct State { uint32 vertexAlphaEnabled=1,alphaTestFunction=2,zWriteEnabled=1,zTestEnabled=1; } gstate;
struct {VkCommandBuffer frameCommands=42;} gvk;
struct PushConstants {float model[16]={},materialColour[4]={},surfaceProps[4]={},ambientLight[4]={},lightDirColour[4]={};};
struct InstanceData {uint32 numIndex=0,offset=0,wireOffset=0,wireCount=0;};
struct InstanceDataHeader {uint32 numMeshes=0,primType=3;uint16 *indexBuffer=nullptr;InstanceData *inst=nullptr;VkBuffer vbo=10,ibo=20,wireIbo=0;VkDeviceMemory wireMemory=0;};
static bool cold=false,finishedDepth=false,requireDepth=false;
static unsigned draws=0,profiled=0,sceneBinds=0,boneBinds=0,neoBinds=0;
static uint32 boundBone=0; static VkBuffer boundVertices=0,boundIndices=0;
static PushConstants lastPush;static std::vector<uint16> uploaded;
static void flushStaticInstances() {}
static uint64 getPipelineLayout(){return 100;}
static VkDescriptorSet getSceneDescriptor(){return 11;}
static VkDescriptorSet getBoneDescriptor(){return 12;}
static VkPipeline getPipeline(uint32,unsigned){assert(!gstate.zWriteEnabled);return cold?0:88;}
static bool uploadStaticBuffer(const void *p,VkDeviceSize size,unsigned,VkBuffer *buffer,VkDeviceMemory *memory){
    uploaded.assign((const uint16*)p,(const uint16*)p+size/2);*buffer=33;*memory=44;return true;
}
static void vkCmdBindDescriptorSets(VkCommandBuffer,unsigned,uint64,unsigned first,unsigned count,const VkDescriptorSet *sets,unsigned dynamic,const uint32 *offset){
    if(first==0){++sceneBinds;assert(*sets==11);}
    if(first==2){++boneBinds;assert(dynamic==1);boundBone=*offset;}
    if(first==4){++neoBinds;assert(count==2&&sets[0]==61&&sets[1]==62);}
}
static void vkCmdBindPipeline(VkCommandBuffer,unsigned,VkPipeline pipeline){assert(pipeline==88);}
static void vkCmdBindVertexBuffers(VkCommandBuffer,unsigned,unsigned,const VkBuffer *buffer,const VkDeviceSize*){boundVertices=*buffer;}
static void vkCmdBindIndexBuffer(VkCommandBuffer,VkBuffer buffer,VkDeviceSize,unsigned){boundIndices=buffer;}
static void vkCmdPushConstants(VkCommandBuffer,uint64,unsigned,unsigned,unsigned size,const void *p){assert(size==sizeof(lastPush));std::memcpy(&lastPush,p,size);}
static void countGpuProfileDraw(uint32 count,unsigned topology,uint32){assert(topology==VK_PRIMITIVE_TOPOLOGY_LINE_LIST&&count==6);++profiled;}
static void vkCmdDrawIndexed(VkCommandBuffer,uint32 count,uint32 instances,uint32 first,int,int){
    assert(!requireDepth||finishedDepth);assert(count==6&&instances==1&&first==0);assert(boundIndices==33);++draws;
}
#include "visualization-render-production.inc"
static void Policy(uint64 key,bool &test,bool &write,unsigned &color,unsigned &motion){
    struct {bool depthTestEnable,depthWriteEnable;} depthStencil{test,write};
    struct Blend {bool blendEnable=false;unsigned srcColorBlendFactor=0,dstColorBlendFactor=0,colorWriteMask=15;} blendAttachments[2];
    auto &blendAttachment=blendAttachments[0];
#include "visualization-policy-production.inc"
    test=depthStencil.depthTestEnable;write=depthStencil.depthWriteEnable;
    color=blendAttachment.colorWriteMask;motion=blendAttachments[1].colorWriteMask;
    if(key&(uint64(1)<<25)) assert(blendAttachment.blendEnable&&blendAttachment.srcColorBlendFactor==1&&blendAttachment.dstColorBlendFactor==1);
}
int main(){
    InstanceData mesh;mesh.numIndex=3;uint16 indices[]={0,1,2};
    InstanceDataHeader header;header.numMeshes=1;header.indexBuffer=indices;header.inst=&mesh;
    PushConstants push;push.model[0]=7;VkDescriptorSet neo[]={61,62};uint32 bone=256;
    DebugVisualization::Get().mesh=DebugVisualization::HiddenWire;
    const auto saved=gstate;
    requireDepth=true;
    DrawDebugEdges(42,&header,&mesh,SHADER_SKIN,push,51,&bone,neo,0);
    assert(draws==0&&profiled==0&&debugWireDraws.size()==1);
    assert(std::memcmp(&saved,&gstate,sizeof(saved))==0);
    assert((uploaded==std::vector<uint16>{0,1,1,2,2,0}));
    bone=512;push.model[0]=9; // queue must own captured values
    finishedDepth=true;flushDebugWires();
    assert(draws==1&&profiled==1&&sceneBinds==1&&boneBinds==1&&neoBinds==1);
    assert(boundVertices==10&&boundBone==256&&lastPush.model[0]==7&&lastPush.surfaceProps[0]==-999);
    assert(debugWireDraws.empty());flushDebugWires();assert(draws==1);
    DrawDebugEdges(42,&header,&mesh,SHADER_WORLD,push,51,nullptr,neo,0);
    assert(debugWireDraws.size()==1);discardDebugWires();assert(debugWireDraws.empty());
    cold=true;DrawDebugEdges(42,&header,&mesh,SHADER_WORLD,push,51,nullptr,neo,0);
    assert(debugWireDraws.empty()&&draws==1);cold=false;
    DebugVisualization::Get().mesh=DebugVisualization::Wire;requireDepth=false;
    DrawDebugEdges(42,&header,&mesh,SHADER_WORLD,push,51,nullptr,neo,0);
    assert(draws==2&&profiled==2&&debugWireDraws.empty()&&boundIndices==20);
    for(bool originalWrite:{false,true}){
        bool test=true,write=originalWrite;unsigned color=0,motion=0;
        Policy(uint64(1)<<25,test,write,color,motion);assert(!test&&!write&&color==15&&motion==0);
        test=true;write=originalWrite;
        Policy((uint64(1)<<25)|(uint64(1)<<27),test,write,color,motion);assert(test&&write==originalWrite&&color==15&&motion==0);
        test=true;write=originalWrite;
        Policy(uint64(1)<<28,test,write,color,motion);assert(test&&write==originalWrite&&color==0&&motion==0);
    }
    puts("PASS: hidden-wire replay order, captured skin/material state, cold/aborted frames, xray wires and depth/blend policies");
}
