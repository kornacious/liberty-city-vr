#pragma once
#include <cstdint>

// Bounded CPU-side accounting of submitted commands, not rasterized pixels.
namespace DrawProfile {
constexpr unsigned PassCount=14,CounterCount=16,MaxModels=512,HashSize=1024,ContextDepth=8;
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
    Model models[MaxModels];
    unsigned modelCount=0,overflowDraws=0,contextOverflows=0;
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
    uint16_t slots[HashSize]={};
    Context entity,parents[ContextDepth],overflowParent;
    unsigned depth=0;
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
        unsigned hash=(uint32_t(entity.model)*2654435761u+unsigned(entity.entityClass)*17u+pass*31u)%HashSize;
        for(unsigned probe=0;probe<HashSize;++probe,hash=(hash+1)%HashSize){
            unsigned index=slots[hash];
            if(!index){
                if(stats.modelCount==MaxModels){++stats.overflowDraws;return;}
                index=++stats.modelCount;slots[hash]=uint16_t(index);
                stats.models[index-1].entity=entity;stats.models[index-1].pass=pass;
            }
            Model &model=stats.models[index-1];
            if(model.entity.model==entity.model&&model.entity.entityClass==entity.entityClass&&model.pass==pass){
                AddCounters(model.counters,count,topology,flags);return;
            }
        }
        ++stats.overflowDraws;
    }
};
}
