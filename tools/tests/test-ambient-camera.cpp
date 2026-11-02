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
                expected=!flag && &name==&names[0];
#endif
                Check(QuestSkipAmbientCameraScript(name, mission, flag)==expected);
                ++checks;
            }
    // Retail saved camera threads have m_bIsMissionScript=true and
    // m_bMissionFlag=false. The former is also used by ambient launchers.
    // Keep this explicit regression independent of the combination matrix.
#ifdef __ANDROID__
    Check(QuestSkipAmbientCameraScript("camera", true, false));
    Check(!QuestSkipAmbientCameraScript("camera", true, true));
#else
    Check(!QuestSkipAmbientCameraScript("camera", true, false));
#endif
    // A newly named or restored mission-launched ambient thread must yield at
    // every dispatch, not just the first command following SCRIPT_NAME.
    char current[8]="noname";
    Check(!QuestSkipAmbientCameraScript(current, false, false));
    std::memcpy(current, "camera\0", 8);
    for (int frame=0; frame<100; ++frame) {
#ifdef __ANDROID__
        Check(QuestSkipAmbientCameraScript(current, true, false));
#else
        Check(!QuestSkipAmbientCameraScript(current, true, false));
#endif
        ++checks;
    }
    std::printf("Ambient camera: %u checks passed.\n", checks);
}
