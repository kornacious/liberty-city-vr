#pragma once
#include <cstdint>

namespace ParticleFrame {
// Xbox configuration frame numbers address console animation sequences, not
// the re3 raster arrays. Preserve progress through the available sequence.
// The final clamp also protects Classic and explicit AddParticle frame values.
inline unsigned Resolve(unsigned current, unsigned start, unsigned end,
                        unsigned count, bool xbox)
{
    if(count <= 1) return 0;
    if(xbox && end >= count && end > start){
        if(current <= start) return 0;
        if(current >= end) return count-1;
        return unsigned(std::uint64_t(current-start)*(count-1)/(end-start));
    }
    return current < count ? current : count-1;
}
}
