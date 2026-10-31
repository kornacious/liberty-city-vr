#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

// Opportunity accounting only: no command reordering or GPU instancing.
namespace StaticReuseAudit {
struct Key {
    uint64_t geometry=0,pipeline=0,texture=0;
    uint32_t first=0,count=0,boundary=0;
    int32_t model=-1;
    uint32_t state[16]={}; // Exact non-transform push data, including lighting.
    bool operator==(const Key &b)const{
        return geometry==b.geometry && pipeline==b.pipeline && texture==b.texture &&
            first==b.first && count==b.count && boundary==b.boundary && model==b.model &&
            std::memcmp(state,b.state,sizeof(state))==0;
    }
};
struct Stats {
    uint64_t sourceFrame=0;
    uint32_t sampled=0,candidates=0,groups=0,geometryRepeats=0;
    uint32_t compatibleInstances=0,nearbyInstances=0,adjacentInstances=0;
    uint32_t rejectedState=0,rejectedOwner=0,rejectedTransform=0;
    uint32_t tableOverflow=0,ownerOverflow=0,maxInstances=0;
    int32_t topModel=-1;
    double cpuMs=0;
};
template<unsigned Capacity=2048,unsigned Owners=32> class Recording {
    static_assert(Capacity && !(Capacity&(Capacity-1)),"Power-of-two table");
    struct Group {
        Key key;
        uint64_t owners[Owners]={};
        float position[3]={};
        uint32_t generation=0,used=0;
    };
    struct Geometry {
        uint64_t geometry=0,owner=0;
        uint32_t first=0,count=0,boundary=0,generation=0;
    };
    std::array<Group,Capacity> groups{};
    std::array<Geometry,Capacity> geometries{};
    uint32_t generation=0,boundary=0;
    Key last;
    uint64_t lastOwner=0;
    bool lastValid=false,pending=false;
    static uint64_t Mix(uint64_t h,uint64_t v){return (h^v)*1099511628211ULL;}
public:
    Stats stats;
    void Reset(bool sample,uint64_t source){
        stats={};stats.sampled=sample?1:0;stats.sourceFrame=sample?source:0;
        boundary=0;lastValid=pending=false;
        if(sample && ++generation==0){
            for(auto &g:groups)g.generation=0;
            for(auto &g:geometries)g.generation=0;
            generation=1;
        }
    }
    void Boundary(){++boundary;lastValid=pending=false;}
    // Called for EVERY profiled draw, including Im2D/Im3D and rejected draws.
    // Thus "adjacent" cannot bridge an intervening unsupported command.
    void FinishDraw(){if(!pending)lastValid=false;pending=false;}
    void Observe(Key key,uint64_t owner,const float position[3]){
        if(!stats.sampled)return;
        if(!owner){++stats.rejectedOwner;return;}
        for(int i=0;i<3;++i)if(!std::isfinite(position[i])){++stats.rejectedTransform;return;}
        key.boundary=boundary;++stats.candidates;
        if(lastValid && lastOwner!=owner && last==key)++stats.adjacentInstances;
        last=key;lastOwner=owner;lastValid=pending=true;
        const uint64_t gh=Mix(Mix(Mix(key.geometry,key.first),key.count),boundary);
        bool found=false;
        for(unsigned n=0;n<Capacity && n<64;++n){
            auto &g=geometries[(gh+n)&(Capacity-1)];
            if(g.generation!=generation){g={key.geometry,owner,key.first,key.count,boundary,generation};found=true;break;}
            if(g.geometry==key.geometry && g.first==key.first && g.count==key.count && g.boundary==boundary){
                if(g.owner!=owner)++stats.geometryRepeats;
                found=true;break;
            }
        }
        if(!found)++stats.tableOverflow;
        uint64_t h=Mix(Mix(Mix(gh,key.pipeline),key.texture),uint32_t(key.model));
        for(auto v:key.state)h=Mix(h,v);
        for(unsigned n=0;n<Capacity && n<64;++n){
            auto &g=groups[(h+n)&(Capacity-1)];
            if(g.generation!=generation){
                g.key=key;g.generation=generation;g.used=1;g.owners[0]=owner;
                std::memcpy(g.position,position,sizeof(g.position));++stats.groups;
                if(stats.maxInstances<1){stats.maxInstances=1;stats.topModel=key.model;}
                return;
            }
            if(!(g.key==key))continue;
            for(unsigned j=0;j<g.used;++j)if(g.owners[j]==owner)return;
            if(g.used==Owners){++stats.ownerOverflow;return;}
            g.owners[g.used++]=owner;++stats.compatibleInstances;
            double distance=0;
            for(int j=0;j<3;++j){const double d=double(position[j])-g.position[j];distance+=d*d;}
            if(distance<=32.0*32.0)++stats.nearbyInstances;
            if(g.used>stats.maxInstances){stats.maxInstances=g.used;stats.topModel=key.model;}
            return;
        }
        ++stats.tableOverflow;
    }
};
}
