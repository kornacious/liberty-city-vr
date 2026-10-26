#pragma once
namespace rw { namespace vulkan {
// Fully opaque pixels in a vehicle's alpha atomic are still bodywork, not
// glass. Claim their depth first; blend the remaining pixels without writes.
inline bool splitVehicleAlpha(bool vehiclePass, bool blended, bool depthWrite)
{
    return vehiclePass && blended && depthWrite;
}
inline float vehicleOpaqueReference() { return 254.0f/255.0f; }
} }
