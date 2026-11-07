#pragma once

#include <stdint.h>

// Positions are unmodified tracked grips in metres, in the same world frame
// as the HMD. World Z stays vertical when the player tilts their head.
namespace QuestSurrender {
template<class Vector>
bool HandsRaised(const Vector &left, const Vector &right, const Vector &head)
{
	// Positive comparisons also reject invalid/non-finite tracking samples.
	return left.z >= head.z - 0.10f && right.z >= head.z - 0.10f &&
		(left - head).MagnitudeSqr() <= 0.90f * 0.90f &&
		(right - head).MagnitudeSqr() <= 0.90f * 0.90f &&
		(left - right).MagnitudeSqr() <= 0.65f * 0.65f;
}

class Hold {
	uint32_t last = 0, elapsed = 0;
	bool active = false;
public:
	void Reset() { active = false; elapsed = 0; }
	bool Update(uint32_t now, bool eligible)
	{
		if(!eligible){ Reset(); return false; }
		const uint32_t delta = now - last;
		// A suspended game or a changed clock must not complete a gesture.
		if(!active || delta > 250){ elapsed = 0; active = true; }
		else elapsed += delta;
		last = now;
		if(elapsed < 3000) return false;
		Reset();
		return true;
	}
};
}
