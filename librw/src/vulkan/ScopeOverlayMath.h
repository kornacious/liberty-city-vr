#pragma once
#include <cmath>
#include <cstring>

namespace ScopeOverlayMath {
// Keep the HUD's angular size, but converge its reticle at the world target.
inline bool targetPlane(float out[16], const float plane[16], const float eye[3],
                        const float target[3], float x, float y)
{
    const float normal[3] = {
        plane[1]*plane[6]-plane[2]*plane[5],
        plane[2]*plane[4]-plane[0]*plane[6],
        plane[0]*plane[5]-plane[1]*plane[4]};
    float from = 0.0f, to = 0.0f;
    for(int i = 0; i < 3; ++i){
        from += (plane[12+i]-eye[i])*normal[i];
        to += (target[i]-eye[i])*normal[i];
    }
    const float scale = to/from;
    if(!std::isfinite(scale) || scale <= 0.0f) return false;
    std::memcpy(out, plane, 16*sizeof(float));
    for(int i = 0; i < 3; ++i){
        out[i] *= scale;
        out[4+i] *= scale;
        out[12+i] = target[i]-out[i]*x-out[4+i]*y;
    }
    return true;
}
}
