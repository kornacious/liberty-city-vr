#pragma once
#include <cmath>
#include <cstdint>

namespace rw { namespace vulkan {
namespace UniformAlphaPolicy {
enum Pass { Solid=1,Faint=2,Both=Solid|Faint };
struct BoundStats {
    uint32_t candidates=0,solidPassesSkipped=0;
    uint64_t indicesSkipped=0;
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
}
} }
