#pragma once

namespace QuestPedGroundProbe {
inline float Drop(float timeStep)
{
	// This is a step-down tolerance, not a falling velocity. At the original
	// 30 Hz it reaches 25 cm below the feet; shrinking it with render dt makes
	// a grounded ped lose stairs and downhill ramps at Quest refresh rates.
	// Keep the original reach at high FPS and the larger low-FPS sweep.
	const float referenceStep = 50.0f / 30.0f;
	return 0.15f * (timeStep < referenceStep ? referenceStep : timeStep);
}
}
