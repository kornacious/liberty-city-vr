#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace rw { namespace vulkan {
namespace UniformAlphaPolicy {
enum Pass { Solid=1,Faint=2,Both=Solid|Faint };
struct BoundStats {
    uint32_t candidates=0,solidPassesSkipped=0;
    uint64_t indicesSkipped=0;
    uint32_t uploadedSolidSkipped=0,uploadedFaintSkipped=0;
    uint32_t foliageFaintSkipped=0;
};
// Keep a conservative envelope across writes. An overwritten level can widen
// it, and a level with unknown contents prevents a proof until it is uploaded.
struct UploadedRange {
    uint32_t levels;
    uint8_t minimum,maximum;

    void Observe(const uint8_t *pixels,std::size_t texels,unsigned level,unsigned count){
        if(level>=31 || count==0 || count>31-level)return;
        const uint32_t mask=((uint32_t(1)<<count)-1)<<level;
        if(!pixels || texels==0){levels&=~mask;return;}
        if(levels==0){minimum=255;maximum=0;}
        for(std::size_t i=0;i<texels && (minimum!=0 || maximum!=255);++i){
            const uint8_t alpha=pixels[i*4+3];
            if(alpha<minimum)minimum=alpha;
            if(alpha>maximum)maximum=alpha;
        }
        levels|=mask;
    }

    bool Complete(unsigned count) const {
        return count>0 && count<=31 && levels==((uint32_t(1)<<count)-1);
    }
};
// Much wider than float roundoff, but smaller than one authored 8-bit step.
constexpr float Margin=1.0f/1024.0f;
inline unsigned Select(float alpha,float reference){
    // Zero has no negative-reference encoding; endpoint/invalid references
    // and near-threshold values retain the original two-pass behavior.
    if(!std::isfinite(alpha)||!std::isfinite(reference)||alpha<0||alpha>1||
       reference<=Margin||reference>=1-Margin)return Both;
    if(alpha-reference>Margin)return Solid;
    if(reference-alpha>Margin)return Faint;
    return Both;
}
// An upper bound can prove the solid half empty, never the faint half empty.
inline unsigned FromUpperBound(float maximum,float reference){
    return Select(maximum,reference)==Faint ? Faint : Both;
}
inline unsigned FromUploadedRange(const UploadedRange &range,unsigned levels,
                                  float modulation,float reference){
    if(!range.Complete(levels))return Both;
    if(Select(modulation*(range.minimum/255.f),reference)==Solid)return Solid;
    return FromUpperBound(modulation*(range.maximum/255.f),reference);
}
}
} }
