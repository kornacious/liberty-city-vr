#ifndef RW_VULKAN_POSTFX_POLICY_H
#define RW_VULKAN_POSTFX_POLICY_H

#include <stdint.h>

namespace rw { namespace vulkan {

// The small shader implements only native-size copy/FXAA/colour/fade modes.
// Check the actual draw constants as well as the configured mode so stale or
// diagnostic constants always retain the general shader.
inline bool
useNativePostFx(uint32_t width, uint32_t height,
               uint32_t sceneWidth, uint32_t sceneHeight,
               const uint32_t mode[4], const uint32_t upscale[4],
               uint32_t sgsrMode, bool pipelineAvailable)
{
	return pipelineAvailable && width != 0 && height != 0 &&
		width == sceneWidth && height == sceneHeight &&
		mode[0] <= 2 && mode[1] <= 1 &&
		mode[2] == sceneWidth && mode[3] == sceneHeight &&
		upscale[0] == width && upscale[1] == height &&
		upscale[2] == 0 && sgsrMode == 0;
}

} }
#endif
