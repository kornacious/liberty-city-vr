#include "../../librw/src/vulkan/PostFxPolicy.h"
#include <assert.h>
#include <stdio.h>

int main()
{
	using rw::vulkan::useNativePostFx;
	uint32_t mode[4] = {1, 1, 1512, 1584};
	uint32_t upscale[4] = {1512, 1584, 0, 100};
	for(uint32_t colour = 0; colour <= 2; colour++)
		for(uint32_t aa = 0; aa <= 1; aa++){
			mode[0] = colour; mode[1] = aa;
			assert(useNativePostFx(1512,1584,1512,1584,mode,upscale,0,true));
		}
	mode[0] = mode[1] = 1;
	assert(!useNativePostFx(1512,1584,1512,1584,mode,upscale,0,false));
	assert(!useNativePostFx(1512,1584,1890,1980,mode,upscale,0,true));
	assert(!useNativePostFx(0,1584,0,1584,mode,upscale,0,true));
	assert(!useNativePostFx(1512,0,1512,0,mode,upscale,0,true));
	for(uint32_t temporal = 1; temporal < 8; temporal++){
		upscale[2] = temporal;
		assert(!useNativePostFx(1512,1584,1512,1584,mode,upscale,0,true));
		upscale[2] = 0;
		assert(!useNativePostFx(1512,1584,1512,1584,mode,upscale,temporal,true));
	}
	// Mismatched draw constants and future/unknown modes must be rejected.
	for(int i = 0; i < 4; i++){
		const uint32_t saved = mode[i];
		mode[i] = 9999;
		assert(!useNativePostFx(1512,1584,1512,1584,mode,upscale,0,true));
		mode[i] = saved;
	}
	for(int i = 0; i < 2; i++){
		upscale[i]++;
		assert(!useNativePostFx(1512,1584,1512,1584,mode,upscale,0,true));
		upscale[i]--;
	}
	puts("PASS: native postprocess eligibility, AA/colour/mask modes, mismatched sizes/constants, missing pipeline and temporal/diagnostic fallback.");
}
