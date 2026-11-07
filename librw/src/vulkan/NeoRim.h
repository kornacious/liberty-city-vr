#pragma once

namespace NeoRim {
// A ped may mix a skinned torso with separately submitted rigid head/hands.
// This state is independent of optional profiling and instancing attribution.
class Selection {
    bool pedestrian = false;
public:
    bool SetPedestrian(bool enabled) {
        const bool previous = pedestrian;
        pedestrian = enabled;
        return previous;
    }
    bool Eligible(bool skinned, bool lit, bool normals) const {
        return (skinned || pedestrian) && lit && normals;
    }
};

inline void CameraForward(const float quaternion[4], float direction[4])
{
    const float x=quaternion[0], y=quaternion[1], z=quaternion[2], w=quaternion[3];
    // OpenXR camera forward is local -Z. All ped parts use the same direction,
    // matching Neo's camera-at convention, rather than a reversed eye vector.
    direction[0]=-2*(x*z+y*w);
    direction[1]=-2*(y*z-x*w);
    direction[2]=-(1-2*(x*x+y*y));
    direction[3]=0;
}
}
