#pragma once

#include <cstdint>

// Player input only; keep transient tap history out of vehicle/save layouts.
class QuestSirenInput
{
	const void *vehicle = nullptr;
	std::uint32_t lastFrame = 0, pressedAt = 0, releasedAt = 0;
	bool down = false, awaitingSecond = false;
public:
	bool Update(const void *owner, bool pressed, bool enabled,
	            std::uint32_t now, std::uint32_t frame)
	{
		if(!enabled || owner != vehicle || frame-lastFrame != 1){
			vehicle = owner;
			down = false;
			awaitingSecond = false;
		}
		lastFrame = frame;
		if(!enabled)
			return false;
		if(awaitingSecond && now-releasedAt > 500)
			awaitingSecond = false;
		bool toggle = false;
		if(pressed && !down){
			pressedAt = now;
			toggle = awaitingSecond;
			awaitingSecond = false;
		}else if(!pressed && down){
			// A held horn is not the first half of a double tap. Consumed
			// second presses must not start another pair on release.
			awaitingSecond = !consumed && now-pressedAt <= 500;
			releasedAt = now;
		}
		if(pressed && !down)
			consumed = toggle;
		down = pressed;
		return toggle;
	}
private:
	bool consumed = false;
};
