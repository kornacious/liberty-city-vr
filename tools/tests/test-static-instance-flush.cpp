#include "../../librw/src/vulkan/StaticInstances.h"
#include <cassert>
#include <chrono>
#include <cstring>
#include <vector>
using uint32=uint32_t;using uint64=uint64_t;
using VkPipeline=uint64_t;using VkDescriptorSet=uint64_t;using VkBuffer=uint64_t;using VkDeviceSize=uint64_t;
constexpr uint64_t VK_NULL_HANDLE=0;
#define nil nullptr
namespace DrawProfile {struct Context {int model=0;unsigned flags=0;};}
struct PushConstants {float model[16]{},materialColour[4]{},surfaceProps[4]{},ambientLight[4]{},lightDirColour[4]{};};
#include "static-instance-draw-production.inc"
static StaticInstanceDraw gInstanceDraws[StaticInstances::Window];
static unsigned gInstanceCount;
static StaticInstances::Stats gStaticInstanceStats;
static struct {bool inFrame=true;uint32 sceneWidth=64,sceneHeight=64;} gvk;
struct InstanceDataHeader {bool orderBoundsValid=true;float orderMin[3]={-.2f,-.2f,.2f},orderMax[3]={.2f,.2f,.4f};};
static struct Scene {float viewProj[2][16]{};} scene;
static Scene *getSceneData(){return &scene;}
static void multiplyMatrix(float *out,const float *a,const float *b){
    for(unsigned c=0;c<4;++c)for(unsigned r=0;r<4;++r){out[c*4+r]=0;
        for(unsigned k=0;k<4;++k)out[c*4+r]+=a[k*4+r]*b[c*4+k];}
}
using std::isfinite;
#include "static-instance-bounds-production.inc"
static float matrices[StaticInstances::MaxInstances*16];
static bool allocationFails=false;
static std::vector<unsigned> emitted;
static bool allocateStaticInstanceTransforms(unsigned,VkBuffer *buffer,VkDeviceSize *offset,void **mapped){
    if(allocationFails)return false;
    *buffer=1;*offset=0;*mapped=matrices;return true;
}
static void emitStaticInstanceDraw(const StaticInstanceDraw &d,VkPipeline pipeline,VkBuffer buffer,VkDeviceSize,unsigned instances){
    if(instances>1){
        assert(buffer==1 && pipeline==d.instanced);
        for(unsigned i=0;i<instances;++i){
            assert(matrices[i*16+3]==0 && matrices[i*16+7]==0 && matrices[i*16+11]==0 && matrices[i*16+15]==1);
            emitted.push_back(unsigned(matrices[i*16+12]));
        }
    }else{assert(pipeline==d.pipeline);emitted.push_back(unsigned(d.push.model[12]));}
    gStaticInstanceStats.emittedIndices+=uint64(d.count)*instances;
}
#include "static-instance-flush-production.inc"
static void Fill(unsigned count){
    emitted.clear();gInstanceCount=count;gStaticInstanceStats={};
    for(unsigned i=0;i<count;++i){
        auto &d=gInstanceDraws[i];d={};d.pipeline=1;d.instanced=2;d.texture=3;
        d.vertices=4;d.indices=5;d.count=12;d.object=i+1;d.opaque=true;d.bounds.valid=true;
        d.push.model[12]=float(i); // tag transforms, root-motion row starts zero
        for(unsigned e=0;e<2;++e){
            d.bounds.left[e]=d.bounds.bottom[e]=-.5f;d.bounds.right[e]=d.bounds.top[e]=.5f;
            d.bounds.depth.nearDepth[e]=d.bounds.depth.farDepth[e]=.5f;
        }
        gStaticInstanceStats.sourceIndices+=d.count;
    }
}
int main(){
    for(unsigned e=0;e<2;++e)for(unsigned i=0;i<4;++i)scene.viewProj[e][i*5]=1;
    scene.viewProj[1][12]=.1f;
    PushConstants push;push.model[0]=push.model[5]=push.model[10]=1;
    InstanceDataHeader header;
    auto bounds=staticInstanceBounds(&header,push);
    assert(bounds.valid && std::abs(bounds.left[0]-(-.2f-2.f/64))<1e-6f);
    assert(std::abs(bounds.left[1]-bounds.left[0]-.1f)<1e-6f);
    assert(std::abs(bounds.depth.nearDepth[0]-.2f)<1e-6f);
    scene.viewProj[1][15]=-1;assert(!staticInstanceBounds(&header,push).valid);
    scene.viewProj[1][15]=1;header.orderMin[2]=-.1f;assert(!staticInstanceBounds(&header,push).valid);
    header.orderMin[2]=.2f;header.orderBoundsValid=false;assert(!staticInstanceBounds(&header,push).valid);
    Fill(3);gInstanceDraws[1].texture=99;
    for(unsigned e=0;e<2;++e)gInstanceDraws[1].bounds.depth.nearDepth[e]=gInstanceDraws[1].bounds.depth.farDepth[e]=.8f;
    flushStaticInstances();
    assert((emitted==std::vector<unsigned>{0,2,1}));
    assert(gStaticInstanceStats.groups==1 && gStaticInstanceStats.instances==2 && gStaticInstanceStats.savedDraws==1);
    assert(gStaticInstanceStats.sourceIndices==gStaticInstanceStats.emittedIndices);
    assert(gInstanceCount==0);
    Fill(3);gInstanceDraws[1].texture=99;
    for(unsigned e=0;e<2;++e)gInstanceDraws[1].bounds.depth.nearDepth[e]=gInstanceDraws[1].bounds.depth.farDepth[e]=.8f;
    allocationFails=true;flushStaticInstances();allocationFails=false;
    assert((emitted==std::vector<unsigned>{0,1,2})); // allocation failure restores original order
    assert(gStaticInstanceStats.savedDraws==0 && gStaticInstanceStats.allocationFailure==1);
    assert(gStaticInstanceStats.sourceIndices==gStaticInstanceStats.emittedIndices);
    Fill(StaticInstances::Window);flushStaticInstances();
    assert(emitted.size()==StaticInstances::Window && gStaticInstanceStats.groups==4);
    for(unsigned i=0;i<emitted.size();++i)assert(emitted[i]==i);
    assert(gStaticInstanceStats.savedDraws==60);
    assert(gStaticInstanceStats.sourceIndices==gStaticInstanceStats.emittedIndices);
    Fill(3);for(auto &d:gInstanceDraws)d.instanced=0;
    flushStaticInstances();assert((emitted==std::vector<unsigned>{0,1,2}));
    assert(gStaticInstanceStats.savedDraws==0);
}
