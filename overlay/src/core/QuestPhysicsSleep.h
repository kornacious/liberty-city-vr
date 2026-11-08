#pragma once

#include "QuestGameTime.h"
#include "World.h"

namespace QuestPhysicsSleep {
inline float Blend()
{
#if (defined(__ANDROID__) || defined(LIBERTYVR_WINDOWS)) && defined(FIX_BUGS)
	if(CWorld::bForceProcessControl)
		return 0.0f; // Postponed control is retried within the same frame.
	return 1.0f - Pow(0.5f, CTimer::GetTimeStepInSeconds()*30.0f);
#else
	return 0.5f;
#endif
}
inline float SpeedLimit(float legacyLimit)
{
#if (defined(__ANDROID__) || defined(LIBERTYVR_WINDOWS)) && defined(FIX_BUGS)
	return legacyLimit * (50.0f/30.0f);
#else
	return legacyLimit * CTimer::GetTimeStep();
#endif
}
inline void Advance(uint8 &frames)
{
#if (defined(__ANDROID__) || defined(LIBERTYVR_WINDOWS)) && defined(FIX_BUGS)
	if(CWorld::bForceProcessControl)
		return;
	// Eleven quiet reference ticks; retain the ready state between ticks.
	frames = uint8(Min(uint32(11), uint32(frames) + QuestGameTime::Count()));
#else
	frames++;
#endif
}
inline uint8 RestingCount()
{
#if (defined(__ANDROID__) || defined(LIBERTYVR_WINDOWS)) && defined(FIX_BUGS)
	return 11;
#else
	return 10;
#endif
}
inline float ReferenceDistance(float distance)
{
	// Call when displacement is produced, before the next frame changes dt.
	const float step = CTimer::GetTimeStep();
	return step > 0.0f ? distance * (50.0f/30.0f) / step : 0.0f;
}
}
