#include <cstdio>
#include <cstdlib>
#include <initializer_list>
enum { MISSION_RETRY_STAGE_NORMAL, MISSION_RETRY_STAGE_WAITING };
enum { SAVE_TYPE_QUICKSAVE = 2, SAVE_TYPE_QUICKSAVE_FOR_MISSION_REPLAY = 3 };
enum { GS_PLAYING_GAME, GS_MENU };
int AllowMissionReplay, WaitForSave, gGameState, IsQuickSave, MissionStartTime;
const int PAUSE_SAVE_SLOT = 8;
struct CTimer { static int GetTimeInMilliseconds() { return 100; } };
struct CTheScripts { static bool mission; static bool IsPlayerOnAMission() { return mission; } };
bool CTheScripts::mission;
struct CStats { static char LastMissionPassedName[8]; };
char CStats::LastMissionPassedName[8] = "MISSION";
struct SaveHelper {
    int calls = 0, result = 1, savedType = 0;
    bool savedMission = false;
    int SaveSlot(int slot) {
        if (slot != PAUSE_SAVE_SLOT) std::abort();
        ++calls; savedType = IsQuickSave; savedMission = CTheScripts::mission;
        return result;
    }
    void PopulateSlotInfo() {}
} PcSaveHelper;
void DisplaySaveResult(int, char*) {}
#include "mission-retry-production.inc"
static int checks;
static void require(bool value) {
    ++checks;
    if (!value) { std::fprintf(stderr, "FAIL mission retry check %d\n", checks); std::exit(1); }
}
static void reset() {
    AllowMissionReplay = MISSION_RETRY_STAGE_NORMAL; WaitForSave = 0;
    gGameState = GS_PLAYING_GAME; IsQuickSave = 0; MissionStartTime = 12;
    CTheScripts::mission = false; CStats::LastMissionPassedName[0] = 'M';
    PcSaveHelper = SaveHelper{};
}
int main() {
    for (bool mission : {false, true}) {
        reset(); CTheScripts::mission = mission;
        require(SaveGameForPause(SAVE_TYPE_QUICKSAVE_FOR_MISSION_REPLAY));
        require(PcSaveHelper.calls == 1 && PcSaveHelper.savedType == 3);
        require(PcSaveHelper.savedMission == mission && CTheScripts::mission == mission);
        require(IsQuickSave == 0);
    }
    reset(); CTheScripts::mission = true;
    require(!SaveGameForPause(SAVE_TYPE_QUICKSAVE) && PcSaveHelper.calls == 0);
    reset(); require(SaveGameForPause(SAVE_TYPE_QUICKSAVE));
    reset(); WaitForSave = 200;
    require(!SaveGameForPause(SAVE_TYPE_QUICKSAVE));
    require(SaveGameForPause(SAVE_TYPE_QUICKSAVE_FOR_MISSION_REPLAY));
    reset(); gGameState = GS_MENU;
    require(!SaveGameForPause(3) && PcSaveHelper.calls == 0);
    reset(); AllowMissionReplay = MISSION_RETRY_STAGE_WAITING;
    require(!SaveGameForPause(3) && PcSaveHelper.calls == 0);
    reset(); CStats::LastMissionPassedName[0] = 0;
    require(!SaveGameForPause(3) && PcSaveHelper.calls == 0);
    reset(); PcSaveHelper.result = 0;
    require(!SaveGameForPause(3) && IsQuickSave == 0);
    std::printf("PASS %d mission retry checks\n", checks);
}
