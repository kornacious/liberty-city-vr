#include <cstdio>
#include <cstdlib>
#include "../../overlay/src/control/QuestAmbientCamera.h"

static void Check(bool value) { if (!value) std::abort(); }
int main()
{
    const char names[][8] = {"camera", "noname", "intro", "safehou", "gates",
        "love3", "cam", "camera1", {'c','a','m','e','r','a','x','x'}};
    unsigned checks=0;
    const char shops[][8]={"i_ammu", "c_ammu", "camera", "love3", "noname",
        "i_ammu1", "c_ammu1", {'i','_','a','m','m','u','x','x'}};
    for (unsigned i=0; i<sizeof(shops)/sizeof(shops[0]); ++i)
        for (int flag=0; flag<2; ++flag) {
            bool expected=false;
#ifdef __ANDROID__
            expected=i<2 && !flag;
#endif
            Check(QuestKeepShopPlayerCamera(shops[i], flag)==expected);
            // The shop scripts must continue running to maintain the clerk
            // and purchases, even though their fixed camera is suppressed.
            Check(!QuestSkipAmbientCameraScript(shops[i], true, flag) || i==2);
            ++checks;
        }
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
