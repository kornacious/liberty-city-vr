#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
using int32 = int32_t;
using uint8 = uint8_t;
#define nil nullptr
constexpr int MAX_NUM_MISSION_SCRIPTS = 80;
constexpr int SIZE_MAIN_SCRIPT = 128;
constexpr int SIZE_MISSION_SCRIPT = 64;

struct CRunningScript {
    CRunningScript *next = nil;
    bool m_bIsMissionScript = false, m_bMissionFlag = false;
    bool m_bDeatharrestEnabled = true, m_bDeatharrestExecuted = false;
    int m_nStackPointer = 0, m_nIp = 7, m_nWakeTime = 100;
    int m_anStack[8] = {42};
    int processed = 0;
    CRunningScript *GetNext() { return next; }
    bool ProcessOneCommand() { ++processed; return true; }
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
struct CTimer { static void Suspend() {} static void Resume() {} };
struct CMessages { static void ClearMessages() {} };
#include "mission-selector-production.inc"

static void Reset()
{
    CTheScripts::pActiveScripts = nil;
    CTheScripts::bAlreadyRunningAMissionScript = false;
    CTheScripts::bUsingAMultiScriptFile = true;
    CTheScripts::NumberOfMissionScripts = MAX_NUM_MISSION_SCRIPTS;
    CTheScripts::MultiScriptArray[19] = 1024;
    CFileMgr::missing = CFileMgr::seekFailure = CFileMgr::truncated = false;
    CFileMgr::opened = CFileMgr::closed = 0;
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
    assert(CTheScripts::TrySwitchToMission(19));
    assert(mission.processed == 1 && mission.m_bDeatharrestExecuted);
    assert(mission.m_nIp == 42 && mission.m_nStackPointer == 0);
    assert(ambient.processed == 0 && ambient.m_nStackPointer == 2);

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
    std::puts("Mission selector: free roam, background preservation, mission safety and file checks passed.");
}
