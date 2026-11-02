#include <cstdio>
#include <cstdlib>
#include "../../overlay/src/control/QuestAmbientCamera.h"

static void Check(bool value) { if (!value) std::abort(); }
int main()
{
    const char names[][8] = {"camera", "noname", "intro", "safehou", "gates",
        "love3", "cam", "camera1", {'c','a','m','e','r','a','x','x'}};
    unsigned checks=0;
    for (const auto &name : names)
        for (int mission=0; mission<2; ++mission)
            for (int flag=0; flag<2; ++flag) {
                bool expected=false;
#ifdef __ANDROID__
                expected=!mission && !flag && &name==&names[0];
#endif
                Check(QuestSkipAmbientCameraScript(name, mission, flag)==expected);
                ++checks;
            }
    // The first command assigns the normalized name. The next dispatch must
    // yield immediately, and a thread restored at any hallway state must yield.
    char current[8]="noname";
    Check(!QuestSkipAmbientCameraScript(current, false, false));
    std::memcpy(current, "camera\0", 8);
    for (int frame=0; frame<100; ++frame) {
#ifdef __ANDROID__
        Check(QuestSkipAmbientCameraScript(current, false, false));
#else
        Check(!QuestSkipAmbientCameraScript(current, false, false));
#endif
        ++checks;
    }
    std::printf("Ambient camera: %u checks passed.\n", checks);
}
