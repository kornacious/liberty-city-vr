#define _CRT_SECURE_NO_WARNINGS
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "../../overlay/src/skel/android/VrWeaponCalibrationMenu.h"
#include "../../overlay/src/vr/QuestRadio.h"
namespace QuestRadio { void CloseEditor() {} }
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))

template<class T> static T Min(T a, T b) { return a < b ? a : b; }
template<class T> static T Max(T a, T b) { return a > b ? a : b; }
static float Abs(float value) { return value < 0 ? -value : value; }
static int checks;
static void Check(bool condition, const char *what)
{
	++checks;
	if(!condition){ std::fprintf(stderr, "FAIL: %s\n", what); std::exit(1); }
}
namespace OculusVR {
enum { QUEST_VEHICLE_CAL_HAND = 0 };
static int GetQuestVehicleCalibrationItemCount() { return 2; }
static int GetQuestVehicleCalibrationItemForRow(int row) { return row; }
static int GetQuestHolsterPointCount() { return 3; }
static void SetQuestVehicleCalibrationPreview(bool) {}
static int GetQuestCalibrationWeaponType(int){return 0;}
static bool IsQuestDualWieldEligible(int){return false;}
static bool IsQuestDualWieldEnabled(int){return false;}
static int GetQuestCalibrationValue(int,int,int){return 360;}
static int GetQuestWeaponLaserOverride(int){return -1;}
}
static int nativeCount = 38;
static int activatedSource = -1, activationCount, cycledSource = -1, cycleDirection;
static bool godMode, unlimitedRun, neverWanted, carInvincibility;
namespace VrPlayerTraversal {
static bool flight,noclip;
static bool Flight(){return flight;}
static bool Noclip(){return noclip;}
static bool ToggleFlight(){flight=!flight; return true;}
static bool ToggleNoclip(){noclip=!noclip; return true;}
}
namespace TimeCheatFixture { static bool ActivateVrTimeCheat(int); bool GetVrCheatToggleState(int, bool*); }
static int GetVrCheatCount() { return nativeCount; }
static int GetVrMissionCategoryCount() { return 0; }
static int GetVrMissionCount(int) { return 0; }
static const char *GetVrCheatName(int source);
static bool GetVrCheatToggleState(int source, bool *enabled)
{
	if(source >= 35) return TimeCheatFixture::GetVrCheatToggleState(source, enabled);
	if(source != 2 && source != 3 && source != 4 && source != 5) return false;
	*enabled = source == 2 ? godMode : source == 3 ? unlimitedRun : source == 4 ? neverWanted : carInvincibility;
	return true;
}
static bool ActivateVrCheat(int source)
{
	Check(source >= 0 && source < nativeCount, "activation uses a native source index");
	activatedSource = source;
	++activationCount;
	if(source == 2) godMode = !godMode;
	if(source == 3) unlimitedRun = !unlimitedRun;
	if(source == 4) neverWanted = !neverWanted;
	if(source == 5) carInvincibility = !carInvincibility;
	if(source == 36) return VrPlayerTraversal::ToggleFlight();
	if(source == 37) return VrPlayerTraversal::ToggleNoclip();
	if(source >= 29) return TimeCheatFixture::ActivateVrTimeCheat(source-6);
	return source != 0 && source != 1; // Vehicle spawning can be unavailable without a player.
}
static bool CycleVrCheatSelection(int source, int direction)
{
	Check(source >= 0 && source < nativeCount, "model cycling uses a native source index");
	if(source < 0 || source > 1 || direction == 0) return false;
	cycledSource = source;
	cycleDirection = direction;
	return true;
}
static int spawnCount = 22, spawnedModel = -1;
static bool spawnedInside;
static int GetVrSpawnVehicleCount() { return spawnCount; }
static int GetVrSpawnVehicleModel(int index) { return index >= 0 && index < spawnCount ? 100+index : -1; }
static const char *GetVrVehicleModelName(int model) {
 static char name[32];
 std::snprintf(name, sizeof(name), "VEHICLE %03d", model);
 return name;
}
static bool SpawnVrVehicle(int model, bool inside) {
 spawnedModel = model; spawnedInside = inside; return model >= 100;
}
struct DrawnRow { std::string text; int y; bool selected, off, on; };
static std::vector<DrawnRow> rows;
static std::string title;
static const char *GetVrWeaponName(int){return "ROCKET LAUNCHER";}
static void CopyMenuText(char *out,int size,const char *in){std::snprintf(out,size,"%s",in);}
static void BeginFullVrMenuPage(const char *heading, const char * = nullptr)
{ rows.clear(); title = heading; }
static void DrawVrMenuText(const char *, int, int, int, int, int, int) {}
static void DrawFullVrMenuRow(const char *text, int y, int, bool selected,
	bool = true, bool off = false, bool on = false)
{ rows.push_back({text, y, selected, off, on}); }

namespace VehicleCatalogFixture {
static const int MI_FIRST_VEHICLE = 0, MI_LAST_VEHICLE = 5;
static bool IsVrCycleCarModel(int model) { return model == 0 || model == 2 || model == 4; }
struct CModelInfo { static bool IsBoatModel(int model) { return model == 1; } };
static const char *GetVrVehicleModelName(int model) {
 static const char *names[] = {"zebra", "Alpha", "Bravo", "excluded", "alpha", "excluded"};
 return names[model];
}
static int strcasecmp(const char *a, const char *b) { return _stricmp(a, b); }
}
static const int SimulationReplayMenuRows = 8;
namespace TimeCheatFixture {
using uint32 = unsigned int;
static const int NUMONSCREENTIMERENTRIES = 1, COUNTER_DISPLAY_NUMBER = 0;
struct CClock {
 static int hours, minutes, seconds, resets;
 static int GetHours() { return hours; }
 static int GetMinutes() { return minutes; }
 static void SetGameClock(int h, int m) { hours=h; minutes=m; seconds=0; ++resets; }
};
int CClock::hours, CClock::minutes, CClock::seconds, CClock::resets;
struct CReplay { static bool playing; static bool IsPlayingBack() { return playing; } };
bool CReplay::playing;
struct TimerEntry {
 int m_nTimerOffset, m_nCounterOffset, m_nType, ticks;
 bool m_bTimerProcessed, m_bCounterProcessed;
 char m_aTimerText[10], m_aCounterText[10];
 void Process() { ++ticks; }
};
struct COnscreenTimer {
 static bool bVrMissionTimerStopped;
 bool m_bDisabled;
 TimerEntry m_sEntries[1] = {};
 void Init();
 void Process();
};
bool COnscreenTimer::bVrMissionTimerStopped;
static bool gVrGodModeEnabled, gVrCarInvincibilityEnabled;
static bool IsVrRunWithoutLimitsEnabled() { return false; }
struct CWanted { static bool bNoWantedCheat; };
bool CWanted::bNoWantedCheat;
}
#include "cheat-menu-production.inc"

static const char *GetVrCheatName(int source)
{
	Check(source >= 0 && source < nativeCount, "draw uses a known native cheat");
	return nativeNames[source];
}

static void CheckDrawSelection(int selected)
{
	int found = 0;
	for(int i = 0; i < (int)rows.size(); ++i){
		Check(rows[i].y >= 150 && rows[i].y < 670, "cheat rows stay above footer");
		if(rows[i].selected){ ++found; Check(i == selected, "draw highlight matches input row"); }
	}
	Check(found == 1, "one selected row is visible");
}

int main()
{
	using namespace VrCheatMenu;
 {
  using namespace TimeCheatFixture;
  const int presets[] = {6, 12, 18, 0};
  for(int i=0; i<4; ++i){
   CClock::minutes=47; CClock::seconds=33;
   Check(ActivateVrTimeCheat(23+i), "time preset activates");
   Check(CClock::hours==presets[i] && CClock::minutes==0 && CClock::seconds==0, "preset sets exact hour and resets clock tick");
  }
  for(int hour=0; hour<24; ++hour){
   CClock::hours=hour; CClock::minutes=37;
   Check(ActivateVrTimeCheat(27) && CClock::hours==(hour+1)%24 && CClock::minutes==37, "advance wraps all hours and preserves minutes");
   Check(ActivateVrTimeCheat(28) && CClock::hours==hour && CClock::minutes==37, "rewind reverses advance including midnight");
  }
  const int resets=CClock::resets;
  Check(!ActivateVrTimeCheat(-1) && !ActivateVrTimeCheat(30) && CClock::resets==resets, "unrelated cheats cannot change time");
  COnscreenTimer timer;
  timer.Init();
  Check(!COnscreenTimer::bVrMissionTimerStopped, "new game starts with mission timer running");
  timer.Process();
  Check(timer.m_sEntries[0].ticks==1, "normal timer processing continues");
  Check(ActivateVrTimeCheat(29) && COnscreenTimer::bVrMissionTimerStopped, "mission timer toggle turns on");
  bool enabled=false;
  Check(TimeCheatFixture::GetVrCheatToggleState(35, &enabled) && enabled, "backend exposes timer ON state");
  Check(!TimeCheatFixture::GetVrCheatToggleState(35, nullptr), "toggle state rejects null output");
  timer.Process();
  Check(timer.m_sEntries[0].ticks==1, "stopped mission timer does not advance");
  Check(ActivateVrTimeCheat(29) && !COnscreenTimer::bVrMissionTimerStopped, "mission timer toggle turns off");
  Check(TimeCheatFixture::GetVrCheatToggleState(35, &enabled) && !enabled, "backend exposes timer OFF state");
  timer.Process();
  Check(timer.m_sEntries[0].ticks==2, "mission timer resumes after toggle off");
  timer.m_bDisabled=true;
  ActivateVrTimeCheat(29); ActivateVrTimeCheat(29); timer.Process();
  Check(timer.m_sEntries[0].ticks==2 && timer.m_bDisabled, "cheat preserves script-controlled timer pause");
  timer.m_bDisabled=false; CReplay::playing=true; timer.Process();
  Check(timer.m_sEntries[0].ticks==2, "replay still prevents timer processing");
  CReplay::playing=false; ActivateVrTimeCheat(29); timer.Init();
  Check(!COnscreenTimer::bVrMissionTimerStopped, "timer initialization clears cheat for a new game");
 }

 Check(VehicleCatalogFixture::GetVrSpawnVehicleCount() == 4, "catalog contains eligible cars and boats only");
 const int sortedModels[] = {1, 4, 2, 0};
 for(int i = 0; i < 4; ++i)
  Check(VehicleCatalogFixture::GetVrSpawnVehicleModel(i) == sortedModels[i], "production catalog sorts case-insensitively and breaks ties by model ID");
 Check(VehicleCatalogFixture::GetVrSpawnVehicleModel(-1) == -1 && VehicleCatalogFixture::GetVrSpawnVehicleModel(4) == -1, "invalid catalog rows have no model");

	const int expectedCounts[] = {8, 3, 1, 14, 3, 5, 3};
	int seen[38] = {};
	Check(ActualBackendCheatCount() == nativeCount, "actual backend count includes exactly the 38 retained cheats");
	Check(VR_SPECIAL_SPAWN_NEXT_CAR == 0 && VR_SPECIAL_SPAWN_NEXT_BOAT == 1 &&
		VR_SPECIAL_GOD_MODE == 2 && VR_SPECIAL_RUN_WITHOUT_LIMITS == 3 &&
		VR_SPECIAL_NEVER_WANTED == 4 && VR_SPECIAL_CAR_INVINCIBILITY == 5 &&
		VR_SPECIAL_CHEAT_COUNT == 6,
		"actual special enum agrees with vehicle and toggle dispatch");
	Check(std::strcmp(nativeNames[SourceIndex(GENERAL, 0, nativeCount)], "GOD MODE") == 0,
		"General begins with God Mode");
	Check((int)sizeof(kSourceCategories) == GetVrCheatCount(), "all backend cheats are categorised");
	for(int category = 0; category < CATEGORY_COUNT; ++category){
		if(category == VEHICLE_SELECTOR){ seen[0] = seen[1] = seen[12] = 1; continue; }
		Check(Count(category, nativeCount) == expectedCounts[category], "expected category size");
		Check(Count(category, nativeCount) <= VR_CHEAT_ITEMS_PER_PAGE, "all cheat actions fit on the first page");
		OpenCheatMenu();
		Check(gVrCheatCategory == -1 && gVrCheatCategorySelection == 0, "opening starts at category root");
		Check(CurrentMenuSelection() == &gVrCheatCategorySelection, "root navigation selects categories");
		Check(CurrentMenuItemCount() == CATEGORY_COUNT+2, "root includes missions and Back");
		Check(!CurrentMenuValueRepeats(), "root trigger cannot repeat an action");
		const int before = activationCount;
		gVrCheatCategorySelection = category;
		DrawQuestCheatPage();
		CheckDrawSelection(category);
		Check((int)rows.size() == CATEGORY_COUNT+2, "draw has every root entry");
		Check(!CycleQuestCheatMenuSelection(1), "cycling root cannot activate a cheat");
		ActivateQuestCheatMenuSelection();
		if(category == WEAPONS){
			Check(rows[category].text == "WEAPONS", "root Weapons row is a direct action");
			Check(gVrCheatCategory == -1 && gVrCheatCategorySelection == WEAPONS, "Weapons remains at root");
			Check(activationCount == before+1 && activatedSource == 6, "root Weapons gives the weapon set exactly once");
			Check(gVrCheatStatusFrames > 0 && std::strcmp(gVrCheatStatus, "CHEAT ACTIVATED") == 0, "root action reports activation");
			++seen[6];
			DrawQuestCheatPage();
			CheckDrawSelection(WEAPONS);
			continue;
		}
		Check(gVrCheatCategory == category && gVrCheatSelection == 0, "A opens the chosen category at its first row");
		Check(activationCount == before, "opening a category never activates its first cheat");
		Check(CurrentMenuSelection() == &gVrCheatSelection, "submenu has independent row selection");
		Check(CurrentMenuItemCount() == expectedCounts[category]+1, "submenu input count includes Back");
		Check(!CurrentMenuValueRepeats(), "held trigger never repeats cheat activation");
		for(int item = 0; item < expectedCounts[category]; ++item){
			const int source = SourceIndex(category, item, nativeCount);
			Check(source >= 0 && source < nativeCount, "every submenu row resolves to source");
			++seen[source];
			gVrCheatSelection = item;
			DrawQuestCheatPage();
			CheckDrawSelection(item);
			Check(title == std::string("CHEATS / ")+CategoryName(category), "heading names active category");
			Check(rows[item].text == nativeNames[source], "display and action share the source mapping");
			Check(rows[item].text != "PASS CURRENT MISSION", "removed mission-pass entry cannot be rendered or activated");
			if(expectedCounts[category] < VR_CHEAT_ITEMS_PER_PAGE)
				Check(rows.back().text == "BACK TO CHEAT CATEGORIES", "Back is present on the last page");
			const bool isToggle = source == 2 || source == 3 || source == 4 || source == 5 || source >= 35;
			Check(rows[item].off == isToggle && !rows[item].on, "toggle state uses mapped source before activation");
			activatedSource = -1;
			ActivateQuestCheatMenuSelection();
			Check(activatedSource == source, "A dispatches exact displayed cheat");
			Check(gVrCheatCategory == category, "activation remains in current category");
			Check(std::strcmp(gVrCheatStatus, (source == 0 || source == 1) ? "UNAVAILABLE RIGHT NOW" : "CHEAT ACTIVATED") == 0,
				"activation feedback survives categorisation");
			DrawQuestCheatPage();
			Check(rows[item].on == isToggle && !rows[item].off, "toggle state updates after activation");
			for(int direction : {-1, 1}){
				cycledSource = -1;
				const bool changed = CycleQuestCheatMenuSelection(direction);
				const bool model = source >= 0 && source <= 1;
				Check(changed == model, "only vehicle selectors cycle");
				Check(!model || (cycledSource == source && cycleDirection == direction), "model direction and exact source preserved");
			}
		}
		gVrCheatSelection = expectedCounts[category];
		DrawQuestCheatPage();
		CheckDrawSelection(gVrCheatSelection%VR_CHEAT_ITEMS_PER_PAGE);
		Check(rows.back().text == "BACK TO CHEAT CATEGORIES", "Back appears on the final page");
		Check(!CycleQuestCheatMenuSelection(1), "Back row cannot cycle a model");
		const int beforeBack = activationCount;
		ActivateQuestCheatMenuSelection();
		Check(gVrCheatCategory == -1 && gVrCheatCategorySelection == category, "Back row remembers parent category");
		Check(activationCount == beforeBack, "Back row does not activate any cheat");
		ActivateQuestCheatMenuSelection();
		ReturnFromCurrentMenuPage();
		Check(gVrMenuPage == VR_MENU_PAGE_CHEATS && gVrCheatCategory == -1 &&
			gVrCheatCategorySelection == category, "B returns to exact parent category");
		ReturnFromCurrentMenuPage();
		Check(gVrMenuPage == VR_MENU_PAGE_SETTINGS && gVrMenuSelection == VR_MAIN_CHEATS, "second B returns to settings cheat row");
	}
	for(int source = 0; source < nativeCount; ++source) Check(seen[source] == (source == 14 ? 0 : 1), "retained cheats appear once; appearance is hidden");
	Check(activationCount == nativeCount-4, "visible native actions dispatch once; vehicle catalog owns spawning");
 // Every page repeats the toggle and blank row, with direct model dispatch.
 OpenCheatMenu();
 gVrCheatCategorySelection = VEHICLE_SELECTOR;
 ActivateQuestCheatMenuSelection();
 Check(SpawnVehiclePageCount() == 3, "22 vehicles span three pages");
 for(int page = 0; page < 3; ++page){
  Check(gVrSpawnVehiclePage == page, "next page selects expected page");
  const int count = page == 2 ? 4 : 9;
  Check(CurrentMenuItemCount() == count+4, "page input includes toggle, vehicles, navigation and Back");
  DrawQuestCheatPage();
  CheckDrawSelection(0);
  Check(rows[0].text == "SPAWN INSIDE VEHICLE", "toggle is first on every page");
  Check(rows[1].y-rows[0].y == 74, "blank row separates toggle and vehicles");
  const bool oldInside = gVrSpawnInsideVehicle;
  ActivateQuestCheatMenuSelection();
  Check(gVrSpawnInsideVehicle != oldInside, "toggle can change on every page");
  DrawQuestCheatPage();
  Check(rows[0].on == gVrSpawnInsideVehicle && rows[0].off != gVrSpawnInsideVehicle, "toggle renders its shared state");
  for(int row = 1; row <= count; ++row){
   gVrCheatSelection = row;
   DrawQuestCheatPage();
   CheckDrawSelection(row);
   Check(rows[row].text == GetVrVehicleModelName(100+page*9+row-1), "row shows exact page model");
   ActivateQuestCheatMenuSelection();
   Check(spawnedModel == 100+page*9+row-1 && spawnedInside == gVrSpawnInsideVehicle, "A spawns displayed vehicle with toggle state");
  }
  gVrCheatSelection = count+2;
  ActivateQuestCheatMenuSelection();
 }
 Check(gVrSpawnVehiclePage == 0, "next page wraps to first");
 gVrCheatSelection = SpawnVehiclePageSize()+1;
 ActivateQuestCheatMenuSelection();
 Check(gVrSpawnVehiclePage == 2 && gVrCheatSelection == 0, "previous page wraps and selects persistent toggle");
 gVrCheatSelection = SpawnVehiclePageSize()+3;
 ActivateQuestCheatMenuSelection();
 Check(gVrCheatCategory == -1, "vehicle Back returns to categories");
 spawnCount = 0;
 gVrCheatCategory = VEHICLE_SELECTOR;
 gVrSpawnVehiclePage = 0;
 Check(SpawnVehiclePageCount() == 1 && CurrentMenuItemCount() == 4, "empty catalog still has toggle and navigation");
 spawnCount = 22;
	OpenCheatMenu();
	gVrCheatCategorySelection = CATEGORY_COUNT;
	ActivateQuestCheatMenuSelection();
	Check(gVrMenuPage == VR_MENU_PAGE_MISSIONS && gVrMissionCategory == -1, "mission selector opens its own categories");
	gVrMissionCategory = 0;
	gVrMissionCategorySelection = 0;
	ReturnFromCurrentMenuPage();
	Check(gVrMenuPage == VR_MENU_PAGE_MISSIONS && gVrMissionCategory == -1 &&
		gVrMissionCategorySelection == 0, "mission Back preserves mission category choice");
	ReturnFromCurrentMenuPage();
	Check(gVrMenuPage == VR_MENU_PAGE_CHEATS && gVrCheatCategorySelection == CATEGORY_COUNT,
		"mission root Back highlights Mission Selector");
	gVrCheatCategorySelection = CATEGORY_COUNT+1;
	ActivateQuestCheatMenuSelection();
	Check(gVrMenuPage == VR_MENU_PAGE_SETTINGS, "root Back row returns to settings");
	for(int category : {-2, -1, (int)CATEGORY_COUNT, CATEGORY_COUNT+1}){
		Check(Count(category, nativeCount) == 0, "invalid category is empty");
		Check(SourceIndex(category, 0, nativeCount) == -1, "invalid category cannot dispatch");
	}
	Check(SourceIndex(GENERAL, -1, nativeCount) == -1, "negative row cannot dispatch");
	Check(SourceIndex(GENERAL, 8, nativeCount) == -1, "Back row has no native index");
	Check(SourceIndex(GENERAL, 8, nativeCount+1) == nativeCount, "future backend cheat remains reachable in General");
	Check(Count(GENERAL, 0) == 0, "empty backend still supports a Back-only menu");

	ResetMenuNavigationRepeat();
	Check(MenuNavigationPulse(-0.9f, 1000.0) == 1, "stick tap advances one row");
	Check(MenuNavigationPulse(-0.9f, 1429.0) == 0, "held stick waits for deliberate repeat");
	Check(MenuNavigationPulse(-0.9f, 1430.0) == 1, "held stick starts repeating");
	Check(MenuNavigationPulse(-0.9f, 1540.0) == 1, "held stick repeats at existing interval");
	Check(MenuNavigationPulse(0.9f, 1541.0) == -1, "reversing stick moves immediately");
	OpenCheatMenu();
	Check(gVrMenuNavigateDirection == 0 && gVrMenuNavigateRepeatAt == 0.0, "opening clears old navigation repeat");
	bool down = false;
	double repeatAt = 0.0, holdStart = 0.0;
	Check(MenuRepeatPulse(true, down, repeatAt, holdStart, 1000.0, false), "trigger presses once");
	Check(!MenuRepeatPulse(true, down, repeatAt, holdStart, 9000.0, false), "held trigger cannot repeatedly cycle models");
	Check(!MenuRepeatPulse(false, down, repeatAt, holdStart, 9010.0, false), "trigger release resets latch");
	Check(MenuRepeatPulse(true, down, repeatAt, holdStart, 9020.0, false), "fresh trigger tap works");
	gVrMenuPage=VR_MENU_PAGE_CALIBRATION;
	gVrCalibrationGroup=-1;
	Check(CurrentMenuItemCount()==VrWeaponCalibrationMenu::ROW_COUNT,"calibration root fits grouped controls and actions");
	for(int row=0;row<CurrentMenuItemCount();++row){
		gVrCalibrationSelection=row;
		Check(!CurrentMenuValueRepeats(),"root calibration actions never repeat while held");
		DrawQuestWeaponCalibrationPage();CheckDrawSelection(row);
		Check(rows.size()==VrWeaponCalibrationMenu::ROW_COUNT,"root renders exactly its selectable rows");
		Check(title.size()*6*4<968,"longest weapon heading fits panel");
		for(const auto &drawn:rows)Check(drawn.text.size()*6*2<850,"calibration row fits highlight width");
	}
	for(int group=0;group<6;++group){
		gVrCalibrationGroup=group;
		Check(CurrentMenuItemCount()==6,"each calibration submenu has hand, step, XYZ and Back");
		for(int row=0;row<6;++row){
			gVrCalibrationSelection=row;
			Check(CurrentMenuValueRepeats()==(row>=2 && row<=4),"repeat applies only to numeric axes");
			DrawQuestWeaponCalibrationPage();CheckDrawSelection(row);
			Check(rows.size()==6,"submenu renders exactly its selectable rows");
			Check(title.size()*6*4<968,"submenu heading fits panel");
			if(row>=2 && row<=4)Check(VrWeaponCalibrationMenu::ValueItem(group,row)==group*3+row-2,"axis maps to correct production field");
		}
		ReturnFromCurrentMenuPage();
		Check(gVrCalibrationGroup==-1 && gVrMenuPage==VR_MENU_PAGE_CALIBRATION &&
			gVrCalibrationSelection==group+2,"submenu Back highlights parent group");
	}
	ReturnFromCurrentMenuPage();
	Check(gVrMenuPage==VR_MENU_PAGE_WEAPONS,"calibration root Back returns to Weapons");
	std::printf("PASS: %d checks; cheat categories, alphabetical vehicle catalog, paginated spawning, persistent toggle, rendering, Back and navigation.\n", checks);
}
