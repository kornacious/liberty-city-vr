#pragma once

#include <math.h>

namespace QuestMovementDirection {

// Sample before the HMD yaw rotates the virtual gamepad. Animation direction
// belongs to the headset-facing visual body, not the recenter/gameplay frame.
inline float UpdateAngle(float previous, float right, float forward, float deadzone)
{
	if(right*right + forward*forward <= deadzone*deadzone)
		return previous;
	return atan2f(-right, forward);
}

}
