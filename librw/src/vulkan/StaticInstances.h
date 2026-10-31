#pragma once
#include "OpaqueOrder.h"
#include <cmath>
#include <cstdint>

namespace StaticInstances {
constexpr unsigned Window=64,MaxInstances=16;
constexpr float Radius=32.0f,ScreenMargin=0.004f;
struct Bounds {
    OpaqueOrder::Interval depth{};
    float left[2]{},right[2]{},bottom[2]{},top[2]{};
    bool valid=false;
};
struct Stats {
    uint32_t queued=0,groups=0,instances=0,savedDraws=0;
    uint32_t rejectState=0,rejectBounds=0,rejectOrder=0,rejectSpatial=0;
    uint32_t pipelineMiss=0,allocationFailure=0;
    uint64_t sourceIndices=0,emittedIndices=0;
    double flushMs=0;
};
inline bool FiniteBounds(const Bounds &a){
    if(!a.valid)return false;
    for(unsigned e=0;e<2;++e)
        if(!std::isfinite(a.left[e])||!std::isfinite(a.right[e])||
           !std::isfinite(a.bottom[e])||!std::isfinite(a.top[e])||
           !std::isfinite(a.depth.nearDepth[e])||!std::isfinite(a.depth.farDepth[e])||
           a.left[e]>a.right[e]||a.bottom[e]>a.top[e]||
           a.depth.nearDepth[e]<0||a.depth.farDepth[e]>1||
           a.depth.nearDepth[e]>a.depth.farDepth[e])return false;
    return true;
}
// Opaque depth-writing draws commute if their conservative depth intervals
// are separated. Across a blending/alpha barrier, require pixel separation.
inline bool CanCross(const Bounds &moving,const Bounds &barrier,bool opaqueBarrier){
    if(!FiniteBounds(moving)||!FiniteBounds(barrier))return false;
    for(unsigned e=0;e<2;++e){
        const bool screen=moving.right[e]+ScreenMargin<barrier.left[e] ||
            barrier.right[e]+ScreenMargin<moving.left[e] ||
            moving.top[e]+ScreenMargin<barrier.bottom[e] ||
            barrier.top[e]+ScreenMargin<moving.bottom[e];
        const bool depth=opaqueBarrier &&
            (moving.depth.farDepth[e]+0.0001f<barrier.depth.nearDepth[e] ||
             barrier.depth.farDepth[e]+0.0001f<moving.depth.nearDepth[e]);
        if(!screen&&!depth)return false;
    }
    return true;
}
inline bool Nearby(const float a[3],const float b[3]){
    double squared=0;
    for(unsigned j=0;j<3;++j){
        if(!std::isfinite(a[j])||!std::isfinite(b[j]))return false;
        const double d=double(a[j])-b[j];squared+=d*d;
    }
    return squared<=double(Radius)*Radius;
}
// A planned group is emitted at its first draw. Every unconsumed draw crossed
// by each later member must be proven independent; selected members retain
// their original instance order. A failed proof never changes the source list.
template<class Item,class Match>
unsigned Plan(const Item *items,unsigned count,unsigned first,const bool *used,
              unsigned *selected,Match match,Stats &stats){
    unsigned n=1;selected[0]=first;
    if(!items[first].opaque)return n;
    bool member[Window]={};member[first]=true;
    for(unsigned j=first+1;j<count && n<MaxInstances;++j){
        if(used[j]||!items[j].opaque||!match(items[first],items[j]))continue;
        if(!Nearby(items[first].position,items[j].position)){++stats.rejectSpatial;continue;}
        bool safe=true;
        for(unsigned k=first+1;k<j;++k)
            if(!used[k]&&!member[k]&&!CanCross(items[j].bounds,items[k].bounds,items[k].opaque)){
                safe=false;break;
            }
        if(!safe){++stats.rejectOrder;continue;}
        member[j]=true;selected[n++]=j;
    }
    return n;
}
}
