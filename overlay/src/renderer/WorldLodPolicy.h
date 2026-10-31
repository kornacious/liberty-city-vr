#pragma once
#include <cmath>
#include <cstdint>

namespace WorldLodPolicy {
enum Mode { Off=0,On=1,Performance=2,Aggressive=3,Count=4 };
struct Stats {
    uint32_t highSelections=0,lowerSelections=0,lodSelections=0;
    uint32_t suppressedLod=0,missingSelections=0,pairedModels=0,unpairedLodModels=0;
};
inline int ClampMode(int mode){return mode>=0&&mode<Count?mode:On;}
// Only choose within the intact OR damaged family. A damaged atomic is not
// a low-detail substitute, and big-building related-model pointers are not
// atomic slots. Caller supplies the safe end of the atomic array.
inline int Select(int mode,float distance,float limit,const float *thresholds,
                  const bool *loaded,int first,int end){
    if(!std::isfinite(distance)||!std::isfinite(limit)||distance>=limit||first<0||end>3||first>=end)return -1;
    if(mode==Off)return loaded[first]?first:-1;
    for(int i=first;i<end-1;++i)
        if(distance<thresholds[i])return loaded[i]?i:-1;
    return loaded[end-1]?end-1:-1;
}
}
