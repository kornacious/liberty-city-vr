#pragma once
#include <math.h>

namespace QuestScopeZoom {
static constexpr float Default = 2.5f;
static constexpr float Minimum = 1.0f;
static constexpr float Maximum = 8.0f;

inline float Step(float zoom, float stickY, float seconds)
{
	// Raw OpenXR stick Y is positive upward. Rescale beyond the deadzone
	// so light pressure is slow and full pressure changes two powers/second.
	const float deadzone = 0.25f;
	if(!isfinite(stickY) || !isfinite(seconds) || seconds <= 0.0f)
		return zoom;
	if(stickY > 1.0f) stickY = 1.0f;
	if(stickY < -1.0f) stickY = -1.0f;
	if(fabsf(stickY) <= deadzone) return zoom;
	if(seconds > 0.1f) seconds = 0.1f; // no jump after a stalled frame
	const float axis = (fabsf(stickY)-deadzone)/(1.0f-deadzone);
	zoom += (stickY > 0.0f ? axis : -axis)*2.0f*seconds;
	return zoom < Minimum ? Minimum : (zoom > Maximum ? Maximum : zoom);
}
}
