#include "../../librw/src/vulkan/OpaqueMeshBatch.h"
#include <cassert>
#include <vector>
#include <random>
#include <limits>
struct Material {
    const void *texture=nullptr;
    struct {unsigned red=255,green=255,blue=255,alpha=255;} color;
    struct {float ambient=1,diffuse=1,specular=0;} surfaceProps;
    bool effect=false;
};
struct MatFX {static const void *get(const Material *m){return m->effect?m:nullptr;}};
#include "opaque-mesh-batch-production.inc"
int main(){
    using OpaqueMeshBatch::CanAppend;
    Material a,b;
    assert(sameOpaqueBatchMaterial(&a,&b));
    assert(!sameOpaqueBatchMaterial(nullptr,&b));
    b.effect=true;assert(!sameOpaqueBatchMaterial(&a,&b));b.effect=false;
    b.texture=&b;assert(!sameOpaqueBatchMaterial(&a,&b));b.texture=nullptr;
    b.color.red=254;assert(!sameOpaqueBatchMaterial(&a,&b));b.color.red=255;
    b.color.alpha=254;assert(!sameOpaqueBatchMaterial(&a,&b));b.color.alpha=255;
    b.surfaceProps.diffuse=.5f;assert(!sameOpaqueBatchMaterial(&a,&b));b.surfaceProps.diffuse=1;
    b.surfaceProps.ambient=std::numeric_limits<float>::quiet_NaN();
    assert(!sameOpaqueBatchMaterial(&a,&b));
    assert(CanAppend(12,6,24,3,1));
    assert(!CanAppend(12,6,26,3,1)); // hole
    assert(!CanAppend(12,6,22,3,1)); // overlap
    assert(!CanAppend(13,6,25,3,1)); // misaligned
    assert(!CanAppend(0,4,8,3,1)); // incomplete primitive
    assert(!CanAppend(0,3,6,0,1));
    assert(!CanAppend(0,3,6,3,32));
    assert(!CanAppend(0,0xfffffffc,0x1fffffff8ULL,6,1)); // count overflow
    assert(!CanAppend(0x200000000ULL,3,0x200000006ULL,3,1)); // firstIndex overflow
    // Replay real index sequences through random legal/illegal boundaries.
    // Grouping must preserve every triangle, including duplicate vertices.
    std::mt19937 rng(0x7148);
    for(int trial=0;trial<1000;++trial){
        std::vector<uint16_t> indices(3000);
        for(auto &v:indices)v=uint16_t(rng()%200);
        uint32_t start=0,count=0,meshes=0;
        std::vector<uint16_t> original,batched;
        auto flush=[&]{batched.insert(batched.end(),indices.begin()+start,indices.begin()+start+count);};
        for(uint32_t pos=0;pos+60<indices.size();){
            const uint32_t n=(rng()%20+1)*3;
            original.insert(original.end(),indices.begin()+pos,indices.begin()+pos+n);
            if(meshes && CanAppend(uint64_t(start)*2,count,uint64_t(pos)*2,n,meshes)){count+=n;++meshes;}
            else{if(meshes)flush();start=pos;count=n;meshes=1;}
            pos+=n+(rng()%3==0?3:0);
        }
        if(meshes)flush();
        assert(original==batched);
    }
}
