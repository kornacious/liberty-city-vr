#include "../../librw/src/vulkan/PostFxPolicy.h"
#include <assert.h>
#include <stdio.h>
#include <limits>
#include <initializer_list>

int main()
{
	using rw::vulkan::useNativePostFx;
	using rw::vulkan::selectNativePostFxVariant;
	using rw::vulkan::NativePostFxVariant;
	assert(selectNativePostFxVariant(false,true,true)==NativePostFxVariant::Filtered);
	assert(selectNativePostFxVariant(true,true,true)==NativePostFxVariant::Integer);
	assert(selectNativePostFxVariant(true,false,true)==NativePostFxVariant::Filtered);
	assert(selectNativePostFxVariant(true,false,false)==NativePostFxVariant::General);
	assert(selectNativePostFxVariant(false,true,false)==NativePostFxVariant::General);
	assert(selectNativePostFxVariant(false,false,false)==NativePostFxVariant::General);
	assert(selectNativePostFxVariant(true,true,false)==NativePostFxVariant::Integer);
	assert(selectNativePostFxVariant(false,false,true)==NativePostFxVariant::Filtered);
	float colourValues[4] = {.8f,.7f,.6f,.4f};
	uint32_t mode[4] = {1, 1, 1512, 1584};
	uint32_t upscale[4] = {1512, 1584, 0, 100};
	for(uint32_t colour = 0; colour <= 2; colour++)
		for(uint32_t aa = 0; aa <= 1; aa++){
			mode[0] = colour; mode[1] = aa;
			assert(useNativePostFx(1512,1584,1512,1584,mode,upscale,0,true,colourValues));
		}
	mode[0] = mode[1] = 1;
	assert(!useNativePostFx(1512,1584,1512,1584,mode,upscale,0,false,colourValues));
	assert(!useNativePostFx(1512,1584,1890,1980,mode,upscale,0,true,colourValues));
	assert(!useNativePostFx(0,1584,0,1584,mode,upscale,0,true,colourValues));
	assert(!useNativePostFx(1512,0,1512,0,mode,upscale,0,true,colourValues));
	for(uint32_t temporal = 1; temporal < 8; temporal++){
		upscale[2] = temporal;
		assert(!useNativePostFx(1512,1584,1512,1584,mode,upscale,0,true,colourValues));
		upscale[2] = 0;
		assert(!useNativePostFx(1512,1584,1512,1584,mode,upscale,temporal,true,colourValues));
	}
	// Mismatched draw constants and future/unknown modes must be rejected.
	for(int i = 0; i < 4; i++){
		const uint32_t saved = mode[i];
		mode[i] = 9999;
		assert(!useNativePostFx(1512,1584,1512,1584,mode,upscale,0,true,colourValues));
		mode[i] = saved;
	}
	for(int i = 0; i < 2; i++){
		upscale[i]++;
		assert(!useNativePostFx(1512,1584,1512,1584,mode,upscale,0,true,colourValues));
		upscale[i]--;
	}
	// A failed coefficient proof must select the unchanged general shader.
	for(int component=0;component<4;++component){
		float saved=colourValues[component];
		for(float invalid:{-.01f,1.001f,std::numeric_limits<float>::infinity(),
		                  std::numeric_limits<float>::quiet_NaN()}){
			colourValues[component]=invalid;
			mode[0]=1;
			assert(!useNativePostFx(1512,1584,1512,1584,mode,upscale,0,true,colourValues));
			// Copy/mask modes never evaluate the colour filter.
			for(uint32_t bypass:{0u,2u}){
				mode[0]=bypass;
				assert(useNativePostFx(1512,1584,1512,1584,mode,upscale,0,true,colourValues));
			}
		}
		colourValues[component]=saved;
	}
	puts("PASS: native postprocess eligibility, AA/colour/mask modes, mismatched sizes/constants, missing pipeline and temporal/diagnostic fallback.");
}
