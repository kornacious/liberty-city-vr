// Compile with assembled src/control on the include path. Extract the
// shopTransitionParams dispatch block from Script.cpp into shop-transition.inc.
#include <cstdio>
#include <cstdlib>
#include "../../overlay/src/control/QuestShopTransition.h"

struct Script {
    char m_abScriptName[8]{};
    bool m_bMissionFlag=false;
    int m_nIp=0, effects=0;
    void CollectParameters(int *ip, int count) { *ip+=count; }
    int Dispatch(int command) {
#include "shop-transition.inc"
        ++effects;
        return 0;
    }
};
static void Check(bool value) { if (!value) std::abort(); }
int main()
{
    const char names[][8]={"i_ammu", "c_ammu", "love3", "i_ammu1"};
    const struct { int command, parameters; } staged[]={
        {COMMAND_SET_PLAYER_COORDINATES,4},
        {COMMAND_SET_PLAYER_HEADING,2},
        {COMMAND_SET_PLAYER_CONTROL,2},
        {COMMAND_CLEAR_AREA,5},
        {COMMAND_RESTORE_CAMERA_JUMPCUT,0},
        {COMMAND_SET_CAMERA_IN_FRONT_OF_PLAYER,0},
        {COMMAND_ENABLE_PLAYER_CONTROL_CAMERA,0}
    };
    // These commands maintain the shop and must still reach their handlers.
    const int gameplay[]={COMMAND_CREATE_CHAR, COMMAND_DELETE_CHAR,
        COMMAND_SET_CHAR_HEADING, COMMAND_GIVE_WEAPON_TO_CHAR,
        COMMAND_HAS_SPECIAL_CHARACTER_LOADED, COMMAND_IS_PLAYER_IN_AREA_3D,
        COMMAND_IS_PLAYER_SHOOTING, COMMAND_SET_VAR_INT, COMMAND_GET_GAME_TIMER,
        COMMAND_WAIT, COMMAND_GOTO, COMMAND_SWITCH_STREAMING};
    unsigned checks=0;
    for (unsigned i=0; i<4; ++i)
        for (int mission=0; mission<2; ++mission) {
            Script script;
            std::memcpy(script.m_abScriptName,names[i],8);
            script.m_bMissionFlag=mission;
            bool suppress=false;
#ifdef __ANDROID__
            suppress=i<2 && !mission;
#endif
            // Repeat an entrance and exit, including freeze/unfreeze requests.
            for (int crossing=0; crossing<2; ++crossing)
                for (const auto &action : staged) {
                    int ip=script.m_nIp, effects=script.effects;
                    Check(QuestShopTransitionParameterCount(names[i],mission,
                        action.command)==(suppress ? action.parameters : -1));
                    Check(script.Dispatch(action.command)==0);
                    Check(script.m_nIp==ip+(suppress ? action.parameters : 0));
                    Check(script.effects==effects+(suppress ? 0 : 1));
                    ++checks;
                }
            for (int command : gameplay) {
                int effects=script.effects;
                Check(QuestShopTransitionParameterCount(names[i],mission,command)==-1);
                Check(script.Dispatch(command)==0);
                Check(script.effects==effects+1);
                ++checks;
            }
        }
    std::printf("Seamless shop dispatch: %u cases passed.\n",checks);
}
