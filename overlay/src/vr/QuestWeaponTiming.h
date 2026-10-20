#pragma once

#include <stdint.h>
#include <cmath>

namespace QuestWeaponTiming {

// The attack animation, not WeaponInfo::m_nFiringRate, controls normal re3
// repeat shots. Keep the fractional milliseconds of its 30 Hz authored loop.
inline double AnimationIntervalMs(float start, float end, uint32_t fallback)
{
	const double interval = (double(end)-double(start))*1000.0;
	return std::isfinite(interval) && interval >= 1.0 ? interval :
		double(fallback > 0 ? fallback : 1);
}

struct Cadence {

	double remainingMs = 0.0;
	uint32_t previousTime = 0;
	bool valid = false;
	bool continuous = false;

	void Update(uint32_t now, bool active)
	{
		const uint32_t elapsed = now-previousTime;
		if(valid)
			remainingMs -= double(elapsed);
		// No catch-up for time spent released, reloading, without tracking,
		// holstered, or stalled. Preserve a positive cooldown across trigger taps.
		if(!active || !continuous || !valid || elapsed > 250U)
			if(remainingMs < 0.0)
				remainingMs = 0.0;
		previousTime = now;
		valid = true;
		continuous = active;
	}

	bool Due() const { return remainingMs <= 0.0001; }
	void Fired(double intervalMs) { remainingMs += intervalMs; }
	void Block()
	{
		if(remainingMs < 0.0) remainingMs = 0.0;
		continuous = false;
	}
	void LimitCatchUp(double intervalMs)
	{
		if(Due()) remainingMs = intervalMs;
	}
};

// Inventory owns the cooldown. Hand changes and entering a car must not create
// a second firing clock, and CWeapon::m_nTimer remains owned by native reloads.
template<int Count> inline Cadence &ForWeapon(int type)
{
	static Cadence clocks[Count];
	return clocks[type]; // Caller has validated the inventory weapon type.
}

enum { MAX_SHOTS_PER_STEP = 4 };

} // namespace QuestWeaponTiming
