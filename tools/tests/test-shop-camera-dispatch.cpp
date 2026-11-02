// Extract POINT_CAMERA_AT_PLAYER, SET_FIXED_CAMERA_POSITION, DO_FADE and
// GET_FADING_STATUS cases from
// assembled Script2.cpp into shop-camera-dispatch.inc; add its directory to /I.
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include "../../overlay/src/control/QuestAmbientCamera.h"
#define nil nullptr
enum { COMMAND_POINT_CAMERA_AT_PLAYER, COMMAND_SET_FIXED_CAMERA_POSITION,
    COMMAND_DO_FADE, COMMAND_GET_FADING_STATUS, CAMCONTROL_SCRIPT };
static int ScriptParams[6];
struct CVector { CVector(float, float, float) {} };
static struct {
    int takeovers=0, setups=0, fades=0, lastDirection=-1;
    float lastDuration=0;
    bool fading=false;
    void TakeControl(void *, int, int, int) { ++takeovers; }
    void SetCamPositionForFixedMode(CVector, CVector) { ++setups; }
    void Fade(float duration, int direction) {
        ++fades; lastDuration=duration; lastDirection=direction; fading=true;
    }
    bool GetFading() { return fading; }
} TheCamera;
struct Script {
    char m_abScriptName[8];
    bool m_bMissionFlag;
    int m_nIp=0;
    bool compare=false;
    void UpdateCompareFlag(bool result) { compare=result; }
    void CollectParameters(int *ip, int count) { *ip+=count; }
    int Dispatch(int command) {
        switch (command) {
#include "shop-camera-dispatch.inc"
        }
        return -1;
    }
};
static void Check(bool value) { if (!value) std::abort(); }
int main()
{
    unsigned checks=0;
    for (const char *name : {"i_ammu", "c_ammu", "camera", "love3"})
        for (bool mission : {false, true}) {
            Script script{};
            std::strncpy(script.m_abScriptName, name, 8);
            script.m_bMissionFlag=mission;
            TheCamera.takeovers=TheCamera.setups=0;
            Check(script.Dispatch(COMMAND_SET_FIXED_CAMERA_POSITION)==0);
            Check(script.m_nIp==6);
            Check(script.Dispatch(COMMAND_POINT_CAMERA_AT_PLAYER)==0);
            Check(script.m_nIp==9);
            bool suppress=false;
#ifdef __ANDROID__
            suppress=!mission && (std::strcmp(name,"i_ammu")==0 ||
                std::strcmp(name,"c_ammu")==0);
#endif
            Check(TheCamera.takeovers==(suppress ? 0 : 1));
            Check(TheCamera.setups==(suppress ? 0 : 1));
            TheCamera.fades=0;
            TheCamera.fading=false;
            for (int direction : {0, 1}) {
                ScriptParams[0]=500;
                ScriptParams[1]=direction;
                Check(script.Dispatch(COMMAND_DO_FADE)==0);
                Check(script.m_nIp==11+2*direction);
                Check(TheCamera.fades==(suppress ? 0 : direction+1));
                if (!suppress) {
                    Check(TheCamera.lastDuration==0.5f);
                    Check(TheCamera.lastDirection==direction);
                }
                Check(script.Dispatch(COMMAND_GET_FADING_STATUS)==0);
                Check(script.compare==!suppress);
            }
            // A real fade from another caller must still be reported, so
            // this bypass cannot accidentally clear a mission/loading fade.
            TheCamera.fading=true;
            Check(script.Dispatch(COMMAND_GET_FADING_STATUS)==0);
            Check(script.compare);
            ++checks;
        }
    std::printf("Shop camera dispatch: %u contexts passed.\n", checks);
}
