#ifndef RW_NEO_RIM_GLSL
#define RW_NEO_RIM_GLSL
#ifndef RW_NEO_INLINE
#define RW_NEO_INLINE
#endif

// Shared by the rigid and skinned shaders and exercised by the CPU regression.
// normalDot uses camera FORWARD, not the opposite vector toward the eye.
RW_NEO_INLINE float RwNeoRimChannel(float endColour, float startColour,
    float offset, float scale, float strength, float normalDot)
{
    float factor = offset - scale * normalDot;
    float value = (endColour + (startColour - endColour) * factor) * strength;
    return value < 0.0f ? 0.0f : value > 1.0f ? 1.0f : value;
}
#endif
