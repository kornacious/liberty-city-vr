#pragma once
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <vector>

// CPU-side accounting of submitted commands, not rasterized pixels.
namespace DrawProfile {
constexpr unsigned PassCount=14,CounterCount=16,InitialModels=2048,ContextDepth=8;
enum Counter { Draws,Elements,Triangles,GeometryKnown,MeshPasses,ExtraSplit,
    Alpha,TextureAlpha,VertexAlpha,MaterialAlpha,AlphaTest,DepthWrite,
    Ps2Split,VehicleSplit,DistanceFade,Skinned };
enum Flag { Mesh=1,Extra=2,HasAlpha=4,HasTextureAlpha=8,HasVertexAlpha=16,
    HasMaterialAlpha=32,HasAlphaTest=64,WritesDepth=128,Ps2=256,Vehicle=512,
    Distance=1024,Skin=2048 };
inline const char *Name(unsigned i){
    static const char *names[]={"draws","elements","triangles","geometry_known_draws",
        "mesh_passes","extra_split_draws","alpha_requested_draws","texture_alpha_draws",
        "vertex_alpha_draws","material_alpha_draws","alpha_test_draws","depth_write_draws",
        "ps2_split_draws","vehicle_split_draws","distance_fade_draws","skinned_draws"};
    return i<CounterCount?names[i]:"invalid";
}
struct Context {
    int model=-1,entityClass=4; // building, vehicle, ped, object, unknown
    unsigned flags=0;
    char name[24]="unattributed";
};
inline Context Entity(int model,int entityClass,const char *name,unsigned flags){
    Context out;out.model=model;out.entityClass=entityClass;out.flags=flags;
    if(name){
        unsigned i=0;
        for(;i+1<sizeof(out.name)&&name[i];++i){
            const char c=name[i];
            out.name[i]=((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
                (c>='0'&&c<='9')||c=='_'||c=='-'||c=='.')?c:'_';
        }
        out.name[i]=0;
    }
    return out;
}
struct Model { Context entity;unsigned pass=0;uint32_t counters[CounterCount]={}; };
struct Stats {
    uint32_t passes[PassCount][CounterCount]={};
    std::vector<Model> models;
    unsigned modelCount=0,overflowDraws=0,contextOverflows=0;
    unsigned modelCapacity=0,tableGrowths=0,allocationFailures=0;
};
inline void AddCounters(uint32_t *out,unsigned count,unsigned topology,unsigned flags){
    ++out[Draws];out[Elements]+=count;
    if(topology<=5){ // Vulkan point/line/triangle lists, strips and fans.
        ++out[GeometryKnown];
        out[Triangles]+=topology==3?count/3:(topology==4||topology==5)&&count>=3?count-2:0;
    }
    for(unsigned i=0;i<12;++i)if(flags&(1u<<i))++out[MeshPasses+i];
}
struct Recording {
    Stats stats;
    std::vector<uint32_t> slots;
    Context entity,parents[ContextDepth],overflowParent;
    unsigned depth=0;
    bool allocationFailed=false;
    static uint32_t Hash(const Context &owner,unsigned pass){
        return uint32_t(owner.model)*2654435761u+unsigned(owner.entityClass)*17u+pass*31u;
    }
    // Grow only when needed; rehash transactionally so a failed allocation
    // preserves existing owners and leaves phase counters fully usable.
    bool Reserve(size_t requested){
        if(requested<=stats.modelCapacity)return true;
        size_t capacity=stats.modelCapacity?stats.modelCapacity:InitialModels;
        const size_t limit=std::numeric_limits<uint32_t>::max()/2;
        if(requested>limit){allocationFailed=true;++stats.allocationFailures;return false;}
        while(capacity<requested)capacity*=2;
        try {
            std::vector<uint32_t> next(capacity*2,0);
            for(unsigned i=0;i<stats.modelCount;++i){
                const auto &model=stats.models[i];
                size_t hash=Hash(model.entity,model.pass)&(next.size()-1);
                while(next[hash])hash=(hash+1)&(next.size()-1);
                next[hash]=i+1;
            }
            stats.models.reserve(capacity);
            slots.swap(next);stats.modelCapacity=unsigned(capacity);++stats.tableGrowths;
            return true;
        } catch(const std::bad_alloc &) {
            allocationFailed=true;++stats.allocationFailures;return false;
        }
    }
    void Reset(){
        std::memset(stats.passes,0,sizeof(stats.passes));
        stats.models.clear();stats.modelCount=stats.overflowDraws=stats.contextOverflows=0;
        stats.tableGrowths=stats.allocationFailures=0;allocationFailed=false;
        depth=0;entity=Context{};
        for(auto &slot:slots)slot=0;
        if(slots.empty())Reserve(InitialModels);
    }
    void Begin(const Context &next){
        if(depth<ContextDepth)parents[depth]=entity;
        else {if(depth==ContextDepth)overflowParent=entity;++stats.contextOverflows;}
        entity=depth<ContextDepth?next:Context{};++depth;
    }
    void End(){
        if(!depth)return;
        --depth;
        if(depth<ContextDepth)entity=parents[depth];
        else if(depth==ContextDepth)entity=overflowParent;
    }
    void Draw(unsigned pass,unsigned count,unsigned topology,unsigned flags){
        if(pass>=PassCount)return;
        flags|=entity.flags;
        AddCounters(stats.passes[pass],count,topology,flags);
        if(slots.empty()&&(allocationFailed||!Reserve(InitialModels))){++stats.overflowDraws;return;}
        size_t hash=Hash(entity,pass)&(slots.size()-1);
        for(;;hash=(hash+1)&(slots.size()-1)){
            unsigned index=slots[hash];
            if(!index){
                if(stats.modelCount==stats.modelCapacity){
                    if(allocationFailed||!Reserve(size_t(stats.modelCapacity)*2)){++stats.overflowDraws;return;}
                    hash=Hash(entity,pass)&(slots.size()-1);
                    while(slots[hash])hash=(hash+1)&(slots.size()-1);
                }
                index=stats.modelCount+1;
                // Reserve above guarantees this append cannot allocate.
                stats.models.emplace_back();++stats.modelCount;slots[hash]=index;
                stats.models.back().entity=entity;stats.models.back().pass=pass;
            }
            Model &model=stats.models[index-1];
            if(model.entity.model==entity.model&&model.entity.entityClass==entity.entityClass&&model.pass==pass){
                AddCounters(model.counters,count,topology,flags);return;
            }
        }
    }
};
}
