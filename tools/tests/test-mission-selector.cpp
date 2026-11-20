#define _CRT_SECURE_NO_WARNINGS
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
using int32 = int32_t;
using uint8 = uint8_t;
#define nil nullptr
#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))
#define MISSION_SWITCHER
#define MISSION_REPLAY
constexpr int MAX_NUM_MISSION_SCRIPTS = 120;
constexpr int SIZE_MAIN_SCRIPT = 128;
constexpr int SIZE_MISSION_SCRIPT = 64;
enum {
    MISSION_RETRY_STAGE_NORMAL,
    MISSION_RETRY_STAGE_WAIT_FOR_SCRIPT_TO_TERMINATE,
    MISSION_RETRY_STAGE_START_PROCESSING,
    MISSION_RETRY_STAGE_SWITCHING_MISSION
};
static int AllowMissionReplay, missionRetryScriptIndex;
static bool doingMissionRetry;
static const char *nonMissionScripts[] = {"ambient"};
struct CStats { static inline char LastMissionPassedName[8] = {}; };

struct CRunningScript {
    CRunningScript *next = nil;
    bool m_bIsMissionScript = false, m_bMissionFlag = false;
    bool m_bDeatharrestEnabled = true, m_bDeatharrestExecuted = false;
    int m_nStackPointer = 0, m_nIp = 7, m_nWakeTime = 100;
    int m_anStack[8] = {42};
    int processed = 0;
    char m_abScriptName[8] = "mission";
    CRunningScript *GetNext() { return next; }
    bool CanAllowMissionReplay();
    bool ProcessOneCommand()
    {
        ++processed;
        // Simulate M_FAIL followed by TERMINATE_THIS_SCRIPT.
        if (CanAllowMissionReplay())
            AllowMissionReplay = MISSION_RETRY_STAGE_WAIT_FOR_SCRIPT_TO_TERMINATE;
        if (AllowMissionReplay == MISSION_RETRY_STAGE_WAIT_FOR_SCRIPT_TO_TERMINATE)
            AllowMissionReplay = MISSION_RETRY_STAGE_START_PROCESSING;
        assert(AllowMissionReplay == MISSION_RETRY_STAGE_SWITCHING_MISSION);
        return true;
    }
};
static CRunningScript launched;
struct CTheScripts {
    static inline bool bUsingAMultiScriptFile = true;
    static inline bool bAlreadyRunningAMissionScript = false;
    static inline int NumberOfMissionScripts = MAX_NUM_MISSION_SCRIPTS;
    static inline int MultiScriptArray[MAX_NUM_MISSION_SCRIPTS] = {};
    alignas(int32) static inline uint8 ScriptSpace[512] = {};
    static inline int OnAMissionFlag = 4;
    static inline CRunningScript *pActiveScripts = nil;
    static CRunningScript *StartNewScript(int ip) { launched = {}; launched.m_nIp = ip; return &launched; }
    static bool TrySwitchToMission(int32 mission);
    static void SwitchToMission(int32 mission);
};
struct CFileMgr {
    static inline bool missing = false, seekFailure = false, truncated = false;
    static inline int opened = 0, closed = 0;
    static void ChangeDir(const char *) {}
    static int OpenFile(const char *, const char *) { ++opened; return missing ? 0 : 1; }
    static bool Seek(int, int, int) { return seekFailure; }
    static size_t Read(int, char *buffer, size_t count) { std::memset(buffer, 0, count); return truncated ? 0 : count; }
    static void CloseFile(int) { ++closed; }
};
struct CTimer {
    static inline int suspensions;
    static void Suspend() { ++suspensions; }
    static void Resume() { assert(suspensions > 0); --suspensions; }
};
struct CMessages { static void ClearMessages() {} };
struct CPad { static inline bool bMissionCheatUsed = false; };
struct CVector {
    float x, y, z;
    CVector(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}
};
enum eLevelName { LEVEL_SOURCE, LEVEL_DESTINATION };
struct CGame { static inline eLevelName currLevel; };
struct CTheZones {
    static eLevelName GetLevelFromPosition(const CVector *) { return LEVEL_DESTINATION; }
};
struct CCollision {
    static void SortOutCollisionAfterLoad() { assert(CTimer::suspensions > 0); }
};
struct CStreaming {
    static inline int loads;
    static inline CVector last;
    static void LoadScene(const CVector &pos) { ++loads; last = pos; }
};
struct CWorld {
    static inline bool foundGround;
    static float FindGroundZFor3DCoord(float x, float y, float z, bool *found) {
        assert(x == CStreaming::last.x && y == CStreaming::last.y && z > CStreaming::last.z);
        *found = foundGround;
        return CStreaming::last.z - 1.0f;
    }
};
struct CPlayerPed {
    bool inVehicle = false;
    int teleports = 0;
    CVector position;
    bool InVehicle() { return inVehicle; }
    bool IsPedInControl() { return true; }
    CVector GetPosition() { return position; }
    float GetDistanceFromCentreOfMassToBaseOfModel() { return 0.75f; }
    void Teleport(const CVector &pos) {
        assert(launched.m_bMissionFlag && CTimer::suspensions > 0);
        ++teleports;
        position = pos;
    }
    void SetMoveSpeed(float x, float y, float z) { assert(x == 0 && y == 0 && z == 0); }
};
static CPlayerPed testPlayer;
static CPlayerPed *FindPlayerPed() { return &testPlayer; }
namespace QuestUpstate {
    static bool active;
    static bool Active() { return active; }
}
#include "mission-selector-production.inc"

static void Reset()
{
    CTheScripts::pActiveScripts = nil;
    CTheScripts::bAlreadyRunningAMissionScript = false;
    CTheScripts::bUsingAMultiScriptFile = true;
    CTheScripts::NumberOfMissionScripts = MAX_NUM_MISSION_SCRIPTS;
    for (int &offset : CTheScripts::MultiScriptArray)
        offset = 0;
    CTheScripts::MultiScriptArray[19] = 1024;
    CFileMgr::missing = CFileMgr::seekFailure = CFileMgr::truncated = false;
    CFileMgr::opened = CFileMgr::closed = 0;
    AllowMissionReplay = MISSION_RETRY_STAGE_NORMAL;
    doingMissionRetry = false;
    std::strcpy(CStats::LastMissionPassedName, "PASSED");
    CPad::bMissionCheatUsed = false;
    QuestUpstate::active = false;
    testPlayer = {};
    CGame::currLevel = LEVEL_SOURCE;
    CStreaming::loads = 0;
    CWorld::foundGround = true;
    assert(CTimer::suspensions == 0);
}

static void CheckMenu(bool upstate)
{
    Reset();
    QuestUpstate::active = upstate;
    const int offset = upstate ? 2 : 0;
    CTheScripts::NumberOfMissionScripts = 80 + offset;
    for (int i = 0; i < CTheScripts::NumberOfMissionScripts; ++i)
        CTheScripts::MultiScriptArray[i] = 1024 + 64 * i;

    bool seen[MAX_NUM_MISSION_SCRIPTS] = {};
    for (int category = 0; category < GetVrMissionCategoryCount(); ++category) {
        for (int item = 0; item < GetVrMissionCount(category); ++item) {
            const std::string label = GetVrMissionName(category, item);
            const int index = std::stoi(label);
            assert(index >= 15 + offset && index < 80 + offset && !seen[index]);
            seen[index] = true;
            const int previousTeleports = testPlayer.teleports;
            assert(ActivateVrMission(category, item));
            assert(missionRetryScriptIndex == index && CPad::bMissionCheatUsed);
            const bool hasMarker = index != 19 + offset;
            assert(testPlayer.teleports == previousTeleports + (hasMarker ? 1 : 0));
            if (hasMarker)
                assert(testPlayer.position.z == CStreaming::last.z - 0.25f);
            if (index == 75 + offset) {
                assert(label.substr(4) == "Toyminator");
                assert(testPlayer.position.x == -443.5f && testPlayer.position.y == -6.0625f);
            }
            if (index == 73 + offset)
                assert(label.substr(4) == "S.A.M.");
        }
    }
    for (int index = offset; index < 80 + offset; ++index)
        assert(seen[index] == (index >= 15 + offset));

    ++CTheScripts::NumberOfMissionScripts;
    const int previousLoads = CStreaming::loads;
    assert(!ActivateVrMission(VR_MISSION_CATEGORY_PORTLAND, 0));
    assert(CStreaming::loads == previousLoads);
}

static void CheckTeleportFallbacks()
{
    Reset();
    CTheScripts::NumberOfMissionScripts = 80;
    CTheScripts::MultiScriptArray[15] = 1024;
    testPlayer.inVehicle = true;
    assert(ActivateVrMission(VR_MISSION_CATEGORY_ACTIVITIES, 0));
    assert(testPlayer.teleports == 0 && CStreaming::loads == 0);

    testPlayer.inVehicle = false;
    testPlayer.position = CVector(1.0f, 2.0f, 3.0f);
    CWorld::foundGround = false;
    assert(ActivateVrMission(VR_MISSION_CATEGORY_ACTIVITIES, 0));
    assert(testPlayer.teleports == 0 && CStreaming::loads == 2);
    assert(CGame::currLevel == LEVEL_SOURCE && CStreaming::last.z == 3.0f);
}

int main()
{
    Reset();
    assert(CTheScripts::TrySwitchToMission(19));
    assert(launched.m_bIsMissionScript && launched.m_bMissionFlag);
    assert(launched.m_nIp == SIZE_MAIN_SCRIPT);

    // LAUNCH_MISSION main-script threads can persist outside a loaded mission.
    Reset();
    CRunningScript ambient;
    ambient.m_bIsMissionScript = true;
    CTheScripts::pActiveScripts = &ambient;
    assert(CTheScripts::TrySwitchToMission(19));
    assert(ambient.processed == 0 && ambient.m_nIp == 7);
    assert(!ambient.m_bDeatharrestExecuted && ambient.m_nWakeTime == 100);

    // A background thread with a return stack must also remain untouched.
    Reset();
    ambient.m_nStackPointer = 2;
    CTheScripts::pActiveScripts = &ambient;
    assert(CTheScripts::TrySwitchToMission(19));
    assert(ambient.m_nStackPointer == 2 && ambient.processed == 0);

    Reset();
    CRunningScript mission;
    mission.m_bIsMissionScript = mission.m_bMissionFlag = true;
    mission.next = &ambient;
    CTheScripts::pActiveScripts = &mission;
    assert(!CTheScripts::TrySwitchToMission(19)); // no death/arrest return frame
    assert(CFileMgr::opened == 0 && mission.processed == 0);
    mission.m_nStackPointer = 1;
    mission.m_bDeatharrestEnabled = false;
    assert(!CTheScripts::TrySwitchToMission(19));
    mission.m_bDeatharrestEnabled = true;
    assert(mission.CanAllowMissionReplay());
    AllowMissionReplay = MISSION_RETRY_STAGE_START_PROCESSING;
    doingMissionRetry = true;
    CFileMgr::missing = true;
    assert(!CTheScripts::TrySwitchToMission(19));
    assert(AllowMissionReplay == MISSION_RETRY_STAGE_START_PROCESSING && doingMissionRetry);
    CFileMgr::missing = false;
    CTheScripts::MultiScriptArray[20] = 1088;
    assert(CTheScripts::TrySwitchToMission(20));
    assert(mission.processed == 1 && mission.m_bDeatharrestExecuted);
    assert(mission.m_nIp == 42 && mission.m_nStackPointer == 0);
    assert(ambient.processed == 0 && ambient.m_nStackPointer == 2);
    assert(AllowMissionReplay == MISSION_RETRY_STAGE_NORMAL && !doingMissionRetry);
    assert(launched.CanAllowMissionReplay());

    Reset();
    assert(!CTheScripts::TrySwitchToMission(-1));
    assert(!CTheScripts::TrySwitchToMission(MAX_NUM_MISSION_SCRIPTS));
    assert(!CTheScripts::TrySwitchToMission(0)); // absent offset
    CTheScripts::NumberOfMissionScripts = 19;
    assert(!CTheScripts::TrySwitchToMission(19));
    Reset();
    CTheScripts::bUsingAMultiScriptFile = false;
    assert(!CTheScripts::TrySwitchToMission(19));
    Reset();
    CFileMgr::missing = true;
    assert(!CTheScripts::TrySwitchToMission(19));
    Reset();
    CFileMgr::seekFailure = true;
    assert(!CTheScripts::TrySwitchToMission(19));
    assert(CFileMgr::closed == 1 && !CTheScripts::bAlreadyRunningAMissionScript);
    Reset();
    CFileMgr::truncated = true;
    assert(!CTheScripts::TrySwitchToMission(19));
    assert(CFileMgr::closed == 1 && !CTheScripts::bAlreadyRunningAMissionScript);
    CheckMenu(false);
    CheckMenu(true);
    CheckTeleportFallbacks();
    std::puts("Mission selector: menu filtering, mission markers, retry suppression and file checks passed.");
}
