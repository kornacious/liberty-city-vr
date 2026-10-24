#pragma once

#include <math.h>
#include <string.h>

namespace rw { namespace vulkan {

// Retain the common head-centred plane's pose and distance. In immersive
// mode give it the same physical size as the compositor's theater quad.
// In theater mode its orthographic-to-quad mapping already supplies that size.
inline void
buildLoadingScreenPlane(float out[16], const float source[16],
                        float screenWidth, float screenHeight, bool theater)
{
	memcpy(out, source, 16*sizeof(float));
	if(theater || screenWidth <= 0.0f || screenHeight <= 0.0f)
		return;
	const float xLength = sqrtf(source[0]*source[0] + source[1]*source[1] + source[2]*source[2]);
	const float yLength = sqrtf(source[4]*source[4] + source[5]*source[5] + source[6]*source[6]);
	if(xLength <= 0.0f || yLength <= 0.0f)
		return;
	const float xScale = 2.56f/(screenWidth*xLength);
	const float yScale = 1.44f/(screenHeight*yLength);
	for(int i = 0; i < 3; i++){
		out[i] = source[i]*xScale;
		out[4+i] = source[4+i]*yScale;
		out[12+i] = source[12+i] +
			(source[i]-out[i])*screenWidth*0.5f +
			(source[4+i]-out[4+i])*screenHeight*0.5f;
	}
}

} }
