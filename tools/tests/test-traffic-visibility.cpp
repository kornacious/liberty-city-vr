#include <cassert>
#include <cmath>
#include <initializer_list>
#include "../../overlay/src/control/QuestTrafficVisibility.h"

int main()
{
	using namespace QuestTrafficVisibility;
	for(float multiplier : {0.5f, 1.0f, 1.5f}){
		const float scale = multiplier > 1.0f ? multiplier : 1.0f;
		assert(SpawnDistance(multiplier) == 120.0f * scale);
		assert(!CanSpawn(40.0f, 40.0f, multiplier));
		assert(!CanSpawn(109.0f * scale, 120.0f * scale, multiplier));
		assert(!CanSpawn(131.0f * scale, 120.0f * scale, multiplier));
		assert(!CanSpawn(120.0f * scale, 89.0f * scale, multiplier));
		assert(CanSpawn(110.0f * scale, 90.0f * scale, multiplier));
		assert(CanSpawn(130.0f * scale, 90.0f * scale, multiplier));
		// Turning around the player must not open the old near spawn ring.
		for(int degrees = 0; degrees < 360; ++degrees){
			const float angle = degrees * 0.01745329252f;
			const float x = SpawnDistance(multiplier) * std::cos(angle);
			const float y = SpawnDistance(multiplier) * std::sin(angle);
			const float distance = std::sqrt(x*x + y*y);
			assert(CanSpawn(distance, distance, multiplier));
			assert(distance < RetentionDistance(multiplier));
		}
		// A stopped car at 26/51/100 m survives mono offscreen cleanup.
		assert(RetentionDistance(multiplier) >= 130.0f);
	}
}
