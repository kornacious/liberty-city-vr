#include "vulkan/AtomicSubmission.h"
#include <cassert>
#include <cstdio>
#include <vector>
#include <array>
using VkPipeline=unsigned;
using VkDescriptorSet=unsigned;
using VkPipelineLayout=unsigned;
constexpr unsigned VK_NULL_HANDLE=0,VK_PIPELINE_BIND_POINT_GRAPHICS=1;
#define nil nullptr
struct Raster {unsigned descriptor;};
static unsigned boundPipeline,boundTexture,boundLayout,pipelineCalls,textureCalls,lookupCalls;
static void vkCmdBindPipeline(unsigned,unsigned,VkPipeline p){boundPipeline=p;++pipelineCalls;}
static void vkCmdBindDescriptorSets(unsigned,unsigned,VkPipelineLayout l,unsigned first,unsigned count,
    const VkDescriptorSet *set,unsigned dynamic,const void*){
    assert(first==1&&count==1&&dynamic==0);boundLayout=l;boundTexture=*set;++textureCalls;
}
static VkDescriptorSet getTextureDescriptor(Raster *r){++lookupCalls;return r?r->descriptor:22;}
struct Mesh {unsigned first,second,texture,passes;};
using Draw=std::array<unsigned,4>;
struct Result {std::vector<Draw> draws;AtomicSubmission::Stats stats;};
static Result Submit(const std::vector<Mesh> &meshes,bool cacheBindings,bool measure=true){
    // Simulate unrelated UI/scope commands before every atomic: the first
    // successful draw must always replace both bindings, even for equal keys.
    boundPipeline=999;boundTexture=998;boundLayout=997;
    pipelineCalls=textureCalls=lookupCalls=0;
    AtomicSubmission::Cache<VkPipeline,VkDescriptorSet,VkPipelineLayout> bindings(cacheBindings,measure);
    const VkPipelineLayout layout=77;const unsigned commandBuffer=1;
    Result result;
    for(unsigned m=0;m<meshes.size();++m){
        const auto &mesh=meshes[m];Raster data{mesh.texture};Raster *raster=mesh.texture?&data:nil;
        VkDescriptorSet textureSet=VK_NULL_HANDLE;bool textureResolved=false;
        for(unsigned pass=0;pass<mesh.passes;++pass){
            const VkPipeline pipeline=pass?mesh.second:mesh.first;
            if(pipeline==VK_NULL_HANDLE)continue;
#include "atomic-submission-production.inc"
            assert(boundLayout==77);
            result.draws.push_back({m,pass,boundPipeline,boundTexture});
        }
    }
    result.stats=bindings.stats;
    if(measure){
        assert(result.stats.pipelineBinds==pipelineCalls);
        assert(result.stats.textureBinds==textureCalls);
        assert(result.stats.textureLookups==lookupCalls);
        assert(result.stats.pipelineRequests==result.draws.size());
        assert(result.stats.textureRequests==result.draws.size());
    }else assert(result.stats.pipelineRequests==0&&result.stats.textureBinds==0);
    if(!cacheBindings){assert(pipelineCalls==result.draws.size()&&textureCalls==result.draws.size());}
    return result;
}
static std::vector<Draw> Reference(const std::vector<Mesh> &meshes){
    std::vector<Draw> expected;
    for(unsigned i=0;i<meshes.size();++i)for(unsigned p=0;p<meshes[i].passes;++p){
        const auto &m=meshes[i];const auto pipeline=p?m.second:m.first;
        if(pipeline)expected.push_back({i,p,pipeline,m.texture?m.texture:22});
    }
    return expected;
}
int main(){
    // Opaque, alpha halves, sampler/texture changes, failed pipeline halves,
    // untextured white fallback and empty geometry. Preserve draw state/order.
    const Mesh cases[]={{10,0,20,1},{10,0,21,1},{11,12,20,2},{0,12,20,2},
        {0,0,20,2},{10,10,0,2},{10,0,20,0}};
    unsigned checks=0;
    for(unsigned code=0;code<2401;++code){
        unsigned n=code;std::vector<Mesh> meshes;
        for(unsigned i=0;i<4;++i){meshes.push_back(cases[n%7]);n/=7;}
        const auto expected=Reference(meshes);
        const auto fast=Submit(meshes,true),slow=Submit(meshes,false);
        assert(fast.draws==expected&&slow.draws==expected);
        assert(fast.stats.pipelineBinds<=slow.stats.pipelineBinds);
        assert(fast.stats.textureBinds<=slow.stats.textureBinds);
        assert(fast.stats.textureLookups<=slow.stats.textureLookups);
        assert(Submit(meshes,true,false).draws==expected);
        ++checks;
    }
    const std::vector<Mesh> repeated={{10,0,20,1},{10,0,20,1},{11,12,20,2}};
    const auto a=Submit(repeated,true),b=Submit(repeated,true);
    assert(a.stats.atomics==1&&a.stats.pipelineRequests==4&&a.stats.pipelineBinds==3);
    assert(a.stats.textureRequests==4&&a.stats.textureBinds==1&&a.stats.textureLookups==3);
    assert(a.stats.pipelineBinds==b.stats.pipelineBinds&&a.stats.textureBinds==b.stats.textureBinds);
    AtomicSubmission::Stats sum;sum.Add(a.stats);sum.Add(b.stats);assert(sum.atomics==2&&sum.textureBinds==2);
    AtomicSubmission::Cache<unsigned,unsigned,unsigned> layouts(true,true);
    assert(layouts.TextureBind(1,20)&&!layouts.TextureBind(1,20));
    assert(layouts.TextureBind(2,20)&&!layouts.TextureBind(2,20));
    assert(layouts.TextureBind(2,21)&&layouts.TextureBind(1,20));
    std::printf("PASS: %u production binding-block traces; identical draw order/state, split descriptor reuse, failed halves, material/sampler/layout changes, independent object boundaries, fallback and profiling-off behavior.\n",checks);
}
