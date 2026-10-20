#pragma once

#include "Timer.h"

// Legacy gameplay work runs at 30 Hz of scaled, clipped game time. Iterate
// crossed ticks rather than testing one modulo value on every rendered frame.
namespace QuestGameTime {
inline uint32 Count()
{
#if defined(__ANDROID__) && defined(FIX_BUGS)
	return CTimer::GetGameFramesPassed();
#else
	return 1;
#endif
}
inline uint64 Tick(uint32 index)
{
#if defined(__ANDROID__) && defined(FIX_BUGS)
	return CTimer::GetGameFrameCounter() - Count() + 1 + index;
#else
	(void)index;
	return CTimer::GetFrameCounter();
#endif
}
inline bool Due(uint32 period, uint32 phase, uint32 offset = 0)
{
	for(uint32 i = 0; i < Count(); i++)
		if((Tick(i) + offset) % period == phase)
			return true;
	return false;
}
}
