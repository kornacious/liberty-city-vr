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
    using StaticArrayBatch::PageLayerTarget;
    assert(PageLayerTarget(0,64)==8);
    assert(PageLayerTarget(8,64)==16);
    assert(PageLayerTarget(16,64)==32);
    assert(PageLayerTarget(32,64)==64);
    assert(PageLayerTarget(64,64)==64);
    assert(PageLayerTarget(0,3)==3);
    assert(PageLayerTarget(8,12)==12);
    assert(PageLayerTarget(32,5)==5);
    assert(PageLayerTarget(2,64)==8);
    {
        using StaticArrayBatch::Result;
        using StaticArrayBatch::TexturePageBudget;
        constexpr uint32_t MiB=1024*1024;
        TexturePageBudget budget;
        assert(TexturePageBudget::IsSmall(64,64));
        assert(TexturePageBudget::IsSmall(32,64));
        assert(!TexturePageBudget::IsSmall(64,128));
        assert(budget.Remaining(true)==24*MiB && budget.Remaining(false)==10*MiB);
        // Larger images arrive first and consume their full share.
        budget.Add(false,4*MiB);
        budget.Add(false,4*MiB);
        budget.Add(false,2*MiB);
        assert(budget.Remaining(false)==0 && budget.Remaining(true)==14*MiB);
        assert(budget.MemoryFailure(1)==Result::SmallTextureReserve);
        budget.Add(true,14*MiB);
        assert(budget.Remaining(false)==0 && budget.Remaining(true)==0);
        assert(budget.MemoryFailure(1)==Result::PageMemoryLimit);
        // Many cheap pages still leave slots for small images.
        budget={};
        for(unsigned i=0;i<TexturePageBudget::OtherPages;++i) budget.Add(false,64*1024);
        assert(budget.CheckPageCount(false,TexturePageBudget::OtherPages)==Result::SmallTextureReserve);
        assert(budget.CheckPageCount(true,TexturePageBudget::OtherPages)==Result::Ready);
        assert(budget.CheckPageCount(true,StaticArrayBatch::MaxPages)==Result::PageCountLimit);
        // Small pages can use the shared pool when there is little other demand.
        budget={};
        budget.Add(true,20*MiB);
        assert(budget.Remaining(true)==4*MiB && budget.Remaining(false)==4*MiB);
    }
    assert((StaticArrayBatch::EncodeCrossLayer(63,7)&63)==63);
    assert(StaticArrayBatch::EncodeCrossLayer(63,7)>>6==7);
    const StaticArrayBatch::CrossGeometryKey ordered={1,2,1};
    const StaticArrayBatch::CrossGeometryKey reordered={1,1,2};
    const StaticArrayBatch::CrossGeometryKey replaced={1,3,1};
    assert(ordered!=reordered && ordered!=replaced);
    assert(StaticArrayBatch::UsesCrossGeometry(ordered,1));
    assert(StaticArrayBatch::UsesCrossGeometry(ordered,2));
    assert(!StaticArrayBatch::UsesCrossGeometry(replaced,2));
    assert(!StaticArrayBatch::UsesCrossGeometry(ordered,0));
    assert(StaticArrayBatch::FitsCrossBatch(7,4093,3));
    assert(!StaticArrayBatch::FitsCrossBatch(8,0,3));
    assert(!StaticArrayBatch::FitsCrossBatch(1,4094,3));
    assert(!StaticArrayBatch::FitsCrossBatch(1,UINT32_MAX,3));
    {
        using StaticArrayBatch::DemandPageSearch;
        unsigned a=0,b=0,c=0;
        struct Slot { const unsigned *owner; uint64_t lastReference; };
        Slot slots[]={{&a,100},{&b,110},{&c,120}};
        DemandPageSearch search;
        uint32_t scans=0,deferrals=0;
        assert(search.Find(slots,3,200,&a,scans,deferrals)==DemandPageSearch::NoLayer && scans==1);
        assert(search.Find(slots,3,340,&a,scans,deferrals)==DemandPageSearch::NoLayer && scans==1 && deferrals==1);
        assert(search.Find(slots,3,350,&a,scans,deferrals)==1 && scans==1);
        slots[1].lastReference=350;
        assert(search.Find(slots,3,350,&a,scans,deferrals)==DemandPageSearch::NoLayer && scans==2);
        assert(search.Find(slots,3,351,&a,scans,deferrals)==DemandPageSearch::NoLayer && scans==2);
        assert(search.Find(slots,3,360,&a,scans,deferrals)==2 && scans==2);
        slots[2].owner=nullptr;
        scans=0;
        assert(search.Find(slots,3,360,&a,scans,deferrals)==DemandPageSearch::NoLayer && scans==1);
        slots[2]={&c,360};
        search.Invalidate();
        assert(search.Find(slots,3,600,&a,scans,deferrals)==1 && scans==2);
        scans=StaticArrayBatch::DemandPageScanLimit;
        assert(search.Find(slots,3,600,&a,scans,deferrals)==1);
        slots[1].lastReference=600;
        assert(search.Find(slots,3,600,&a,scans,deferrals)==DemandPageSearch::NoLayer);

        StaticArrayBatch::DemandHistory history={};
        assert(!history.Observe(1) && !history.Observe(2) && history.Observe(3));
        history.Defer(3);
        assert(!history.Observe(3) && !history.Observe(10) && history.Observe(11));
    }
    {
        StaticArrayBatch::ResidencyStats sample;
        const unsigned uploaded=unsigned(StaticArrayBatch::Result::Ready);
        for(unsigned i=1;i<=StaticArrayBatch::MaxResidencyEvents;++i)
            sample.Observe(i,64,false,uploaded);
        sample.Observe(1,64,true,uploaded);
        sample.Observe(1,64,false,uploaded);
        assert(sample.count==StaticArrayBatch::MaxResidencyEvents && sample.overflow==1);
        assert(sample.events[0].requests==2);
    }
    {
        StaticArrayBatch::CrossObjectCursor cursor;
        constexpr uint32_t stride=128*96;
        constexpr uint32_t capacity=2*stride;
        uint32_t page=0,first=0;
        assert(!cursor.Allocate(0,stride,capacity,page,first));
        assert(!cursor.Allocate(9,stride,capacity,page,first));
        for(unsigned i=0;i<15;++i){
            assert(cursor.Allocate(8,stride,capacity,page,first));
            assert(page==0 && first==i*8);
        }
        assert(cursor.Allocate(5,stride,capacity,page,first) && page==0 && first==120);
        assert(cursor.Allocate(8,stride,capacity,page,first) && page==stride && first==0);
        for(unsigned i=1;i<16;++i){
            assert(cursor.Allocate(8,stride,capacity,page,first));
            assert(page==stride && first==i*8);
        }
        assert(!cursor.Allocate(2,stride,capacity,page,first));
        assert(!cursor.Allocate(2,stride,capacity,page,first));
        cursor={};
        assert(cursor.Allocate(2,stride,capacity,page,first) && page==0 && first==0);
    }
    {
        using Bindings=StaticArrayBatch::CrossArrayBindings<unsigned,unsigned>;
        Bindings bindings;
        constexpr unsigned all=Bindings::BindPipeline|Bindings::BindTexture|Bindings::BindTransforms;
        assert(bindings.Update(1,2,3,0)==all);
        assert(bindings.Update(1,2,3,0)==0);
        assert(bindings.Update(1,4,3,0)==Bindings::BindTexture);
        assert(bindings.Update(1,4,3,12288)==Bindings::BindTransforms);
        assert(bindings.Update(2,4,3,12288)==Bindings::BindPipeline);
        assert(bindings.Update(2,4,5,12288)==Bindings::BindTransforms);
        bindings.Invalidate();
        assert(bindings.Update(2,4,5,12288)==all);
        bindings.Invalidate();
        assert(bindings.UpdateDraw(1,2)==(Bindings::BindPipeline|Bindings::BindTexture));
        assert(bindings.UpdateDraw(1,2)==0);
        assert(bindings.Update(2,2,3,0)==(Bindings::BindPipeline|Bindings::BindTransforms));
        assert(bindings.UpdateDraw(1,2)==Bindings::BindPipeline);
        assert(bindings.UpdateDraw(1,4)==Bindings::BindTexture);
        assert(bindings.Update(2,4,3,0)==Bindings::BindPipeline);
        bindings.Invalidate();
        assert(bindings.UpdateDraw(1,4)==(Bindings::BindPipeline|Bindings::BindTexture));
        assert(bindings.Update(2,4,3,0)==(Bindings::BindPipeline|Bindings::BindTransforms));
    }
    uint32_t crossIndices[]={2,0,1,0,0,0};
    const uint32_t sourceIndices[]={1,0,2};
    StaticArrayBatch::CopyRebasedCrossIndices(reinterpret_cast<uint8_t *>(crossIndices+3),
        reinterpret_cast<const uint8_t *>(sourceIndices),3,3);
    assert((std::vector<uint32_t>(crossIndices,crossIndices+6)==std::vector<uint32_t>{2,0,1,4,3,5}));
    {
        using Arena=StaticArrayBatch::CrossGeometryArena<2>;
        Arena arena;
        assert(Arena::Blocks(2049)==2);
        const auto first=arena.Reserve(2);
        const auto middle=arena.Reserve(Arena::BlockCount-3);
        const auto tail=arena.Reserve(1);
        assert(first==0 && middle==2 && tail==Arena::BlockCount-1);
        assert(arena.Reserve(1)==Arena::Invalid);
        arena.Retire(0,first,2);
        arena.Retire(1,tail,1);
        assert(arena.HasRetired() && arena.Reserve(1)==Arena::Invalid);
        arena.Reclaim(1);
        assert(arena.Reserve(2)==Arena::Invalid);
        assert(arena.Reserve(1)==tail);
        arena.Reclaim(0);
        assert(!arena.HasRetired() && arena.Reserve(2)==first);
        arena.Release(middle,Arena::BlockCount-3);
        assert(arena.Reserve(3)==middle);
    }
    {
        using StaticArrayBatch::Result;
        std::vector<uint32_t> probes;
        uint32_t layers=9;
        const auto fit=StaticArrayBatch::FitPageLayers(layers,[&](uint32_t count){
            probes.push_back(count);
            return count>2?Result::PageMemoryLimit:Result::Ready;
        });
        assert(fit==Result::Ready && layers==2);
        assert((probes==std::vector<uint32_t>{9,4,2}));
        probes.clear();
        layers=5;
        const auto tooLarge=StaticArrayBatch::FitPageLayers(layers,[&](uint32_t count){
            probes.push_back(count);
            return Result::PageMemoryLimit;
        });
        assert(tooLarge==Result::PageMemoryLimit && layers==2);
        assert((probes==std::vector<uint32_t>{5,2}));
        layers=64;
        const auto failure=StaticArrayBatch::FitPageLayers(layers,[](uint32_t){return Result::PageAllocation;});
        assert(failure==Result::PageAllocation && layers==64);
    }
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
    {
        // Shared source vertices must still carry the second mesh's own layer.
        const uint16_t first[]={2,0,1,2,1,3};
        const uint16_t second[]={1,0,3};
        const uint16_t malformed[]={0,1,4};
        uint32_t remap[]={UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX};
        uint32_t output[9]={};
        std::vector<std::pair<uint16_t,uint32_t>> vertices;
        auto append=[&](uint32_t layer){
            return [&,layer](uint16_t original){
                const auto index=uint32_t(vertices.size());
                vertices.emplace_back(original,layer);
                return index;
            };
        };
        assert(StaticArrayBatch::RemapRange(first,6,remap,4,0,output,append(19)));
        assert(vertices.size()==4);
        assert(StaticArrayBatch::RemapRange(second,3,remap,4,4,output+6,append(31)));
        assert(vertices.size()==7);
        for(unsigned i=0;i<9;++i){
            assert(vertices[output[i]].first==(i<6?first[i]:second[i-6]));
            assert(vertices[output[i]].second==(i<6?19u:31u));
        }
        assert(!StaticArrayBatch::RemapRange(malformed,3,remap,4,7,output,append(47)));
        assert(vertices.size()==7 && output[0]==0);
    }
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
