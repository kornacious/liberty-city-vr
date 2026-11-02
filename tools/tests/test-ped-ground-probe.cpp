#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include "../../overlay/src/core/QuestPedGroundProbe.h"

static int checks;
static void Require(bool value, const char *message)
{
	++checks;
	if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

int main()
{
	int legacyMisses = 0;
	for (float fps : {30.f, 60.f, 72.f, 90.f, 120.f}) {
		for (int substeps : {1, 2, 3, 4}) {
			const float step = 50.f / fps / substeps;
			const float reach = QuestPedGroundProbe::Drop(step);
			Require(std::fabs(reach-.25f) < 1.e-6f, "reference step-down reach survives render and collision substeps");
			for (float stairDrop : {.10f, .15f, .20f, .24f}) {
				Require(stairDrop <= reach, "normal stair tread remains grounded");
				if (stairDrop > .15f*step) ++legacyMisses;
			}
			Require(.30f > reach, "larger ledge remains a fall");
			// Slopes descend by horizontal velocity * dt * grade. Cover ramps
			// within the original 30 Hz step-down reach at sprint speed.
			for (float speed : {.08f, .14f, .22f}) {
				for (float grade : {.25f, .5f, .6f}) {
					const float drop = speed*step*grade;
					Require(drop <= reach, "walking, running and sprinting follow downhill ramps");
					if (drop > .15f*step) ++legacyMisses;
				}
			}
		}
	}
	for (float fps : {60.f, 72.f, 90.f, 120.f}) {
		const float step = 50.f/fps;
		const float sprintDrop = .22f*step*.75f;
		Require(sprintDrop > .15f*step, "legacy downhill sprint loses support on the same ramp");
		Require(sprintDrop <= QuestPedGroundProbe::Drop(step), "reference reach retains downhill sprint support");
		++legacyMisses;
	}
	for (float step : {2.f, 3.f, 4.f})
		Require(std::fabs(QuestPedGroundProbe::Drop(step)-.15f*step) < 1.e-6f, "low FPS sweep preserved");
	Require(legacyMisses > 0, "fixtures reproduce lost contact with legacy probe");
	std::printf("Ped ground probe: %d checks passed; %d legacy lost-contact cases reproduced\n", checks, legacyMisses);
}
