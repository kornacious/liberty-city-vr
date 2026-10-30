#ifndef RW_VULKAN_POSTFX_POLICY_H
#define RW_VULKAN_POSTFX_POLICY_H

#include <stdint.h>
#include <cmath>

namespace rw { namespace vulkan {

// With normalized source RGB and coefficients, the five colour-filter steps
// stay in [0,1]. Their clamps are redundant and a uniform gain can be factored
// out of the per-pixel work. Unbounded intensity/unknown inputs keep the general
// shader and its original recurrence/clamps.
inline bool canFactorPostFxColour(const float colour[4])
{
	for(unsigned i=0;i<4;++i)
		if(!std::isfinite(colour[i]) || colour[i]<0.0f || colour[i]>1.0f)
			return false;
	return true;
}

// The small shader implements only native-size copy/FXAA/colour/fade modes.
// Check the actual draw constants as well as the configured mode so stale or
// diagnostic constants always retain the general shader.
inline bool
useNativePostFx(uint32_t width, uint32_t height,
               uint32_t sceneWidth, uint32_t sceneHeight,
               const uint32_t mode[4], const uint32_t upscale[4],
               uint32_t sgsrMode, bool pipelineAvailable, const float colour[4])
{
	return pipelineAvailable && width != 0 && height != 0 &&
		width == sceneWidth && height == sceneHeight &&
		mode[0] <= 2 && mode[1] <= 1 &&
		mode[2] == sceneWidth && mode[3] == sceneHeight &&
		upscale[0] == width && upscale[1] == height &&
		upscale[2] == 0 && sgsrMode == 0 &&
		(mode[0] != 1 || canFactorPostFxColour(colour));
}

} }
#endif
