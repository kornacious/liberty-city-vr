#include "../../overlay/src/core/QuestSirenInput.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

static void Check(bool value, const char *message)
{
	if(!value){ std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

struct Input {
	QuestSirenInput input;
	int car = 0, otherCar = 0;
	std::uint32_t now = 0, frame = 0;
	bool Step(bool down, std::uint32_t elapsed = 10, bool enabled = true)
	{ now += elapsed; return input.Update(&car, down, enabled, now, ++frame); }
};

int main()
{
	// Sample actual press/release edges at common headset and desktop rates.
	for(unsigned fps : {30, 60, 72, 80, 90, 120}){
		Input x;
		const unsigned dt = 1000/fps;
		int toggles = 0;
		for(unsigned t = 0; t < 2000; t += dt){
			const bool down = (t >= 100 && t < 200) || (t >= 550 && t < 650) ||
			                  (t >= 1200 && t < 1300) || (t >= 1600 && t < 1700);
			toggles += x.Step(down, dt);
		}
		Check(toggles == 2, "two double taps toggle on and off at every frame rate");
	}
	for(unsigned gap : {1, 100, 350, 500, 501, 900}){
		Input x;
		Check(!x.Step(true), "first press only honks");
		Check(!x.Step(false, 100), "first release does not toggle");
		Check(x.Step(true, gap) == (gap <= 500), "inclusive 500 ms release-to-press window");
		Check(!x.Step(true, 100), "held second press does not repeat");
		Check(!x.Step(false), "second release does not toggle");
		if(gap <= 500) Check(!x.Step(true), "third press begins a new pair");
	}
	Input hold;
	hold.Step(true); hold.Step(true, 1000); hold.Step(false);
	Check(!hold.Step(true), "long horn hold cannot arm a double tap");
	Input disabled;
	disabled.Step(true); disabled.Step(false);
	Check(!disabled.Step(false, 10, false) && !disabled.Step(true), "disabled controls discard tap history");
	Input switched;
	switched.Step(true); switched.Step(false);
	Check(!switched.input.Update(&switched.otherCar, true, true, switched.now+10, ++switched.frame), "vehicle change discards tap history");
	Input gap;
	gap.Step(true); gap.Step(false); ++gap.frame;
	Check(!gap.Step(true), "leaving player vehicle processing discards tap history");
	Input wrap;
	wrap.now = UINT32_MAX-100;
	wrap.frame = UINT32_MAX-2;
	wrap.Step(true); wrap.Step(false, 80);
	Check(wrap.Step(true, 350), "clock and frame wrap preserve valid tap timing");
	std::puts("PASS: siren double tap timing, holds, reset boundaries and clock wrap");
}
