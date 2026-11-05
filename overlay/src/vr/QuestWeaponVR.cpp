#include "common.h"

#if defined(GTA_VR_WEAPONS) && defined(__ANDROID__)

#include "OculusVR.h"
#include "QuestRadio.h"
#include "VrBodyIK.h"

#include "android.h"
#include "Camera.h"
#include "CutsceneMgr.h"
#include "Frontend.h"
#include "Pad.h"
#include "PlayerInfo.h"
#include "PlayerPed.h"
#include "platform_android.h"
#include "ProjectileInfo.h"
#include "Timer.h"
#include "Vehicle.h"
#include "VRHandModel.h"
#include "WeaponInfo.h"
#include "WeaponType.h"
#include "QuestWeaponGrip.h"
#include "QuestDualWield.h"
#include "QuestScopeZoom.h"
#include "QuestWeaponFactoryDefaults.h"
#include "VrWeaponCalibrationMenu.h"
#include "World.h"
#include "ModelSets.h"
#include "Sprite.h"
#include "vulkan/rwvk.h"

#include <android/log.h>
#include <math.h>
#include <string.h>

extern bool gVrFirstPersonActive;
extern const char *GetVrWeaponName(int weaponType);

template<typename T, typename U, typename V>
static inline T clamp(T val, U low, V high) {
	return val < (T)low ? (T)low : (val > (T)high ? (T)high : val);
}

namespace OculusVR {
namespace {

enum {
	VR_HAND_COUNT = 2,
	WEAPON_VALUE_SCALE = 2,
	SUPPORT_GRIP_LEGACY_VERSION = 3,
	SUPPORT_GRIP_MODEL_BOUND_V4 = 4,
	SUPPORT_GRIP_MODEL_BOUND_V5 = 5,
	SUPPORT_GRIP_POSE_VERSION = 6,
	WEAPON_AIM_ALIGNMENT_SCALE = 1000000
};

enum SupportGripType {
	SUPPORT_GRIP_MAGAZINE = 0,
	SUPPORT_GRIP_FROM_BELOW,
	SUPPORT_GRIP_TYPE_COUNT
};

enum { SUPPORT_GRIP_FROM_BELOW_ROLL_HALF_DEG = -292 };

enum {
	HOLSTER_WAIST_LEFT = 0,
	HOLSTER_WAIST_RIGHT,
	HOLSTER_CHEST_LEFT,
	HOLSTER_CHEST_RIGHT,
	HOLSTER_CHEST_CENTRE,
	HOLSTER_BACK_LEFT,
	HOLSTER_BACK_RIGHT,
	HOLSTER_POINT_COUNT
};

struct WeaponCalibration
{
	int offsetX, offsetY, offsetZ;
	int aimOffsetX, aimOffsetY, aimOffsetZ;
	int aimRotationX, aimRotationY, aimRotationZ;
	int rotationX, rotationY, rotationZ;
	int supportX, supportY, supportZ;
	int supportRotationX, supportRotationY, supportRotationZ;
	int supportGripType, supportPoseVersion;
	bool valid;

	WeaponCalibration() :
		offsetX(0), offsetY(2), offsetZ(-10),
		aimOffsetX(0), aimOffsetY(8), aimOffsetZ(0),
		aimRotationX(0), aimRotationY(0), aimRotationZ(0),
		rotationX(0), rotationY(18), rotationZ(14),
		supportX(0), supportY(60), supportZ(-10),
		supportRotationX(0), supportRotationY(360), supportRotationZ(0),
		supportGripType(SUPPORT_GRIP_MAGAZINE),
		supportPoseVersion(SUPPORT_GRIP_POSE_VERSION), valid(false)
	{}
};

struct BuiltInHandCalibration
{
	int main[12];
	int support[3];
};

struct BuiltInWeaponDefaults
{
	int weaponType;
	BuiltInHandCalibration hand[VR_HAND_COUNT];
};

struct BuiltInWeaponSetDefaults
{
	int weaponType;
	ModelSets::eModelSet modelSet;
	BuiltInHandCalibration hand[VR_HAND_COUNT];
};

struct BuiltInSupportGripDefaults
{
	int weaponType;
	ModelSets::eModelSet modelSet;
	int offset[3];
	int rotation[3];
	int gripType;
};

// Stable v0.3.1 release baselines captured from the desktop project's
// calibrated vr_settings.ini. Values are absolute half-centimetres /
// half-degrees; a configured user value replaces its matching default and is
// never added to it. Hand order is LEFT, RIGHT.
static const BuiltInWeaponDefaults gBuiltInWeaponDefaults[] = {
	{ WEAPONTYPE_UNARMED, {
		{ { -12,0,-8, 0,8,0, 0,0,0, 0,8,12 }, { 0,60,-10 } },
		{ { -12,0,-8, 0,8,0, 0,0,0, 0,8,12 }, { 0,60,-10 } } } },
	{ WEAPONTYPE_BASEBALLBAT, {
		{ { 0,-5,-10, 0,0,0, 0,0,0, 0,0,0 }, { 0,60,-10 } },
		{ { 0,-5,-11, 2,0,0, 0,0,0, 0,0,0 }, { 0,60,-10 } } } },
	{ WEAPONTYPE_GRENADE, {
		{ { 0,0,-16, 0,0,0, 0,0,0, 0,0,0 }, { 0,60,-10 } },
		{ { 0,0,-16, 0,0,0, 0,0,0, 0,0,0 }, { 0,60,-10 } } } },
	{ WEAPONTYPE_MOLOTOV, {
		{ { 24,0,-16, 0,0,0, 0,0,0, 0,0,0 }, { 0,60,-10 } },
		{ { 24,0,-16, 0,0,0, 0,0,0, 0,0,0 }, { 0,60,-10 } } } },
	{ WEAPONTYPE_COLT45, {
		{ { 0,-2,-6, 1,9,-8, 0,0,0, -4,16,-8 }, { 0,60,-10 } },
		{ { 2,-3,-7, -2,10,-8, 0,0,0, -4,16,-8 }, { 0,60,-10 } } } },
	{ WEAPONTYPE_SHOTGUN, {
		{ { -2,2,-10, 0,7,0, 0,0,0, 0,18,14 }, { 0,60,-10 } },
		{ { -2,2,-10, 0,7,0, 0,0,0, 0,18,14 }, { 0,60,-10 } } } },
	{ WEAPONTYPE_UZI, {
		{ { 0,-3,-10, 2,8,0, -10,2,0, 0,18,14 }, { 0,60,-10 } },
		{ { 0,-3,-10, -2,8,0, -10,-2,0, 0,18,14 }, { 0,60,-10 } } } },
	{ WEAPONTYPE_AK47, {
		{ { 1,0,-11, 0,20,0, 0,0,0, 0,8,11 }, { 0,51,-7 } },
		{ { -1,0,-11, 0,20,0, 0,0,0, 0,8,11 }, { 0,50,-6 } } } },
	{ WEAPONTYPE_M16, {
		{ { 5,0,-12, 6,25,65, 0,0,0, 0,0,11 }, { -1,41,-20 } },
		{ { 5,0,-12, -6,25,65, 0,0,0, 0,0,11 }, { -1,41,-20 } } } },
	{ WEAPONTYPE_SNIPERRIFLE, {
		{ { -3,0,-6, 0,10,100, 0,0,0, 0,10,11 }, { 0,60,-10 } },
		{ { -3,0,-6, 0,10,100, 0,0,0, 0,10,11 }, { 0,60,-10 } } } },
	{ WEAPONTYPE_ROCKETLAUNCHER, {
		{ { 5,0,-16, -8,21,71, 0,0,0, 0,0,0 }, { 0,60,-10 } },
		{ { 5,30,-16, -8,21,71, 0,0,0, 0,0,0 }, { 0,45,-10 } } } },
	{ WEAPONTYPE_FLAMETHROWER, {
		{ { 21,1,-10, 0,-7,100, 0,0,0, -3,56,8 }, { 0,60,-10 } },
		{ { 21,1,-10, 0,-7,100, 0,0,0, -3,56,8 }, { 0,60,-10 } } } },
	{ WEAPONTYPE_DETONATOR, {
		{ { 5,0,-16, 0,1,0, 0,0,0, 0,0,0 }, { 0,60,-10 } },
		{ { 5,0,-16, 0,1,0, 0,0,0, 0,0,0 }, { 0,60,-10 } } } },
};

// Desktop v0.5.0 calibration corrections which are independent of the
// graphics backend. Keeping them as an override table makes the Quest update
// explicit while preserving its original fallback rows for untouched weapons.
static const BuiltInWeaponDefaults gBuiltInWeaponV050Overrides[] = {
	{ WEAPONTYPE_COLT45, {
		{ { 0,-3,-7, 2,10,-8, 0,0,0, -4,16,-8 }, { 0,60,-10 } },
		{ { 0,-3,-7, -2,10,-8, 0,0,0, -4,16,-8 }, { 0,60,-10 } } } },
	{ WEAPONTYPE_SHOTGUN, {
		{ { 3,1,-10, 4,12,0, 0,0,0, 2,19,11 }, { 0,60,-10 } },
		{ { 3,1,-10, -4,12,0, 0,0,0, 2,19,11 }, { 0,60,-10 } } } },
	{ WEAPONTYPE_UZI, {
		{ { 0,-3,-10, 2,8,0, -10,2,0, 0,18,14 }, { 0,60,-10 } },
		{ { 0,-3,-10, -2,8,0, -10,-2,0, 0,18,14 }, { 0,60,-10 } } } },
	{ WEAPONTYPE_AK47, {
		{ { -1,0,-11, 0,20,0, 0,0,0, 0,8,11 }, { 0,51,-7 } },
		{ { -1,0,-11, 0,20,0, 0,0,0, 0,8,11 }, { 0,50,-6 } } } },
	{ WEAPONTYPE_SNIPERRIFLE, {
		{ { -2,-5,-9, 0,10,100, 0,0,0, 0,10,0 }, { 0,60,-10 } },
		{ { -2,-5,-9, 0,10,100, 0,0,0, 0,10,0 }, { 0,60,-10 } } } },
	{ WEAPONTYPE_ROCKETLAUNCHER, {
		{ { -2,26,-11, 8,21,71, 0,0,0, 0,0,0 }, { 0,60,-10 } },
		{ { -2,26,-11, -8,21,71, 0,0,0, 0,0,0 }, { 0,45,-10 } } } },
};

// Model-specific v0.5.0 baselines. Missing rows fall back to the corrected
// Classic table above; user INI data still has higher priority.
static const BuiltInWeaponSetDefaults gBuiltInWeaponSetDefaults[] = {
	{ WEAPONTYPE_COLT45, ModelSets::MODEL_SET_MODERN, {
		{ { 0,-3,-7, 2,10,-8, 0,0,0, -4,16,-8 }, { 0,60,-10 } },
		{ { 0,-3,-7, -2,10,-8, 0,0,0, -4,16,-8 }, { 0,60,-10 } } } },
	{ WEAPONTYPE_SHOTGUN, ModelSets::MODEL_SET_MODERN, {
		{ { 3,1,-10, 0,7,0, 0,0,0, 2,19,11 }, { 0,60,-10 } },
		{ { 3,1,-10, 0,7,0, 0,0,0, 2,19,11 }, { 0,60,-10 } } } },
	{ WEAPONTYPE_UZI, ModelSets::MODEL_SET_MODERN, {
		{ { 0,-3,-10, 2,9,0, -10,-1,0, 0,18,14 }, { 0,60,-10 } },
		{ { 0,-3,-10, -2,9,0, -10,1,0, 0,18,14 }, { 0,60,-10 } } } },
	{ WEAPONTYPE_M16, ModelSets::MODEL_SET_MODERN, {
		{ { 5,0,-12, 2,16,48, 2,0,0, 0,0,11 }, { -1,41,-20 } },
		{ { 1,-5,-8, -2,16,48, 2,0,0, 0,0,11 }, { -1,41,-20 } } } },
	{ WEAPONTYPE_AK47, ModelSets::MODEL_SET_MODERN, {
		{ { -1,0,-11, 2,8,87, 1,-3,0, 0,8,11 }, { 0,51,-7 } },
		{ { 1,-6,-12, -2,8,87, 1,3,0, 0,8,11 }, { 0,50,-6 } } } },
	{ WEAPONTYPE_ROCKETLAUNCHER, ModelSets::MODEL_SET_MODERN, {
		{ { -2,26,-11, 4,15,71, 0,0,0, 0,0,0 }, { 0,60,-10 } },
		{ { -2,26,-11, -4,15,71, 0,0,0, 0,0,0 }, { 0,45,-10 } } } },
};

// PC v0.5.0 support-hand profiles. The socket is RIGHT-canonical and bound to
// the visible weapon model: X=lateral, Y=barrel, Z=top. LEFT-primary gameplay
// derives the mirrored pose at runtime, so both hands and both model sets share
// one authoritative calibration.
static const BuiltInSupportGripDefaults gBuiltInSupportGripDefaults[] = {
	{ WEAPONTYPE_SHOTGUN, ModelSets::MODEL_SET_CLASSIC,
		{ -13,80,-6 }, { -32,320,-360 }, SUPPORT_GRIP_FROM_BELOW },
	{ WEAPONTYPE_M16, ModelSets::MODEL_SET_CLASSIC,
		{ -15,75,-6 }, { -66,359,-292 }, SUPPORT_GRIP_FROM_BELOW },
	{ WEAPONTYPE_AK47, ModelSets::MODEL_SET_CLASSIC,
		{ -14,92,-8 }, { -54,360,-295 }, SUPPORT_GRIP_FROM_BELOW },
	{ WEAPONTYPE_SNIPERRIFLE, ModelSets::MODEL_SET_CLASSIC,
		{ -15,71,-4 }, { -47,300,-332 }, SUPPORT_GRIP_FROM_BELOW },
	{ WEAPONTYPE_ROCKETLAUNCHER, ModelSets::MODEL_SET_CLASSIC,
		{ -7,12,-1 }, { -75,-360,0 }, SUPPORT_GRIP_MAGAZINE },
	{ WEAPONTYPE_FLAMETHROWER, ModelSets::MODEL_SET_CLASSIC,
		{ -30,48,0 }, { 0,234,0 }, SUPPORT_GRIP_FROM_BELOW },
	{ WEAPONTYPE_AK47, ModelSets::MODEL_SET_MODERN,
		{ -11,98,-4 }, { -29,359,-360 }, SUPPORT_GRIP_FROM_BELOW },
	{ WEAPONTYPE_M16, ModelSets::MODEL_SET_MODERN,
		{ -12,70,-6 }, { -74,360,-332 }, SUPPORT_GRIP_FROM_BELOW },
};

struct DroppedWeapon
{
	int slot;
	int copy = 1;
	CMatrix matrix;
	CVector velocity;
	CVector angularAxis;
	float angularSpeed;
	uint32 startTime;

	DroppedWeapon() : slot(-1), velocity(0.0f, 0.0f, 0.0f),
		angularAxis(0.0f, 0.0f, 1.0f), angularSpeed(0.0f),
		startTime(0)
	{}
};

struct MeleeStrike
{
	bool pending;
	int slot, weaponType;
	CVector tipStart, tipEnd;
	CVector rootStart, rootEnd;
	float speed, impactScale;
	uint32 frame;

	MeleeStrike() : pending(false), slot(-1), weaponType(-1),
		speed(0.0f), impactScale(0.0f), frame(0)
	{}
};

struct MeleeMotion
{
	bool valid, armed;
	int slot, weaponType;
	CVector previousTip, previousRoot;
	CVector previousTrackingTip, previousTrackingRoot;
	uint32 previousFrame, lastStrikeTime, calmSince;
	bool strikeInProgress;
	float swingTravel;
	uint32 strikeContinueUntil;

	MeleeMotion() : valid(false), armed(false), slot(-1), weaponType(-1),
		previousFrame(0), lastStrikeTime(0), calmSince(0),
		strikeInProgress(false), swingTravel(0.0f),
		strikeContinueUntil(0)
	{}
};

struct ManualReloadState
{
	bool active;
	bool requested;
	bool movedAwayFromSpot;
	int weaponHand;
	int magazineHand;
	int slot;
	int weaponType;
	uint32 grabbedAt;

	ManualReloadState() : active(false), requested(false),
		movedAwayFromSpot(false), weaponHand(-1), magazineHand(-1),
		slot(-1), weaponType(-1), grabbedAt(0)
	{}
};

static bool gPoseValid[VR_HAND_COUNT];
static CMatrix gGripMatrix[VR_HAND_COUNT];
static CMatrix gAimMatrix[VR_HAND_COUNT];
static CVector gHandVelocity[VR_HAND_COUNT];
static CVector gPreviousHandPosition[VR_HAND_COUNT];
static uint32 gPreviousPoseTime[VR_HAND_COUNT];
static float gGrip[VR_HAND_COUNT];
static float gTrigger[VR_HAND_COUNT];
static bool gTriggerPressed[VR_HAND_COUNT];
static bool gTriggerJustPressed[VR_HAND_COUNT];
static bool gTriggerJustReleased[VR_HAND_COUNT];
static bool gGripLatched[VR_HAND_COUNT];

static uint32 gWeaponHolsterMask;
static int gHolsterPointSlot[HOLSTER_POINT_COUNT] = {
	WEAPONSLOT_SUBMACHINEGUN, WEAPONSLOT_HANDGUN, WEAPONSLOT_SHOTGUN,
	WEAPONSLOT_MELEE, WEAPONSLOT_PROJECTILE, WEAPONSLOT_HEAVY,
	WEAPONSLOT_RIFLE
};
static int gHeldSlot[VR_HAND_COUNT] = { -1, -1 };
static int gHeldCopy[VR_HAND_COUNT] = { 0, 1 };
static int gHolsterSelection[VR_HAND_COUNT] = { -1, -1 };
// Scripted mounted-gun sequences attach Tommy to another entity instead of
// putting him in a normal vehicle seat. Mirror the script-owned gun into the
// tracked right hand only for the lifetime of that attachment.
static bool gAttachedMissionWeaponForced;
static int gAttachedMissionWeaponSlot = -1;
static int gSupportHand[VR_HAND_COUNT] = { -1, -1 };
// Set while the weapon calibration page is open. The free hand takes the
// foregrip on its own there and keeps it, so the sockets being edited can
// be judged against a hand that is actually on them.
static bool gWeaponCalibrationPreview;
static CMatrix gWeaponRenderMatrix[VR_HAND_COUNT];
static int gWeaponRenderSlot[VR_HAND_COUNT] = { -1, -1 };
static CMatrix gWeaponContactMatrix[VR_HAND_COUNT];
static int gWeaponContactSlot[VR_HAND_COUNT] = { -1, -1 };
static int gWeaponContactType[VR_HAND_COUNT] = { -1, -1 };
static uint32 gWeaponContactFrame[VR_HAND_COUNT];
static DroppedWeapon gDropped[VR_HAND_COUNT];

static WeaponCalibration
	gCalibration[VR_HAND_COUNT][WEAPONTYPE_TOTALWEAPONS];
static bool gSettingsLoaded;
static bool gHandsEnabled = true;
static bool gHolsterHighlights = true;
// Metres from front holster anchors. Back/shoulder points keep the full reach.
static const float kHolsterGrabRadii[] = { 0.27f, 0.20f, 0.14f };
static int gHolsterGrabDistance = 0;
static bool gWeaponLaser;
struct WeaponLaserOverride {
	bool loaded;
	int value;
};
static WeaponLaserOverride gWeaponLaserOverrides[2][WEAPONTYPE_TOTALWEAPONS];
static bool gManualReload;
static bool gPistolHoldToRepeat;
static bool gDualWieldPistol, gDualWieldSmg;
enum eScopeAimMode {
	SCOPE_AIM_OFF = 0,
	SCOPE_AIM_2D_OVERLAY = 1,
	SCOPE_AIM_3D_PHYSICAL = 2,
	SCOPE_AIM_MODE_COUNT = 3
};
static int gScopeAimMode = SCOPE_AIM_2D_OVERLAY;
static bool gGripLock;
static bool gRunWithoutLimits;
static CVector gBodyForward(0.0f, 1.0f, 0.0f);
static bool gBodyForwardValid;

// Baked LEFT-hand weapon position from the LAST completed calibration
// explicitly selected by the player. This is position-only and is not
// interactive at runtime.
// exactLeftCorrection=(0.0008,0.0344,0.0328)m
static const CVector kBakedLeftWeaponDeltaLocal(
	0.0008f, 0.0344f, 0.0328f);

static bool gActiveFire;
static int gActiveFireHand = -1;
static int gActiveFireWeaponType = -1;
static CVector gActiveFireSource;
static CVector gActiveFireDirection;
static bool gThrowablePreview[VR_HAND_COUNT];
static CVector gActiveThrowableSource;
static CVector gActiveThrowableVelocity;
static bool gActiveThrowable;

static MeleeStrike gMeleeStrike[VR_HAND_COUNT];
static MeleeMotion gMeleeMotion[VR_HAND_COUNT];

static int gScopeHand = -1;
static float gPhysicalScopeZoom = QuestScopeZoom::Default;
static int gScopeWeaponType = -1;
static int gScopeCandidateHand = -1;
static int gScopeCandidateType = -1;
static uint32 gScopeCandidateSince;
static uint32 gScopeReleaseSince;

static ManualReloadState gManualReloadState[VR_HAND_COUNT];
static bool gManualReloadGripDown[VR_HAND_COUNT];
static int gTrackedDetonatorHand = -1;
static bool gTrackedDetonatorWasActive[VR_HAND_COUNT];
static bool gTrackedDetonatorWaitForRelease[VR_HAND_COUNT];
static bool gTrackedDetonatorJustPressed[VR_HAND_COUNT];

static const char *kSettingsPath = ".\\vr_settings.ini";

static bool IsTrackedDetonatorHandReserved(int hand);

#define QUEST_WEAPON_LOG(...) \
	__android_log_print(ANDROID_LOG_INFO, "LibertyVR", __VA_ARGS__)

static CVector
VectorFromArray(const float value[3])
{
	return CVector(value[0], value[1], value[2]);
}

static CVector
RotateAroundAxis(const CVector &value, CVector axis, float angle)
{
	if(axis.MagnitudeSqr() < 0.000001f || angle == 0.0f)
		return value;
	axis.Normalise();
	const float cosine = Cos(angle);
	const float sine = Sin(angle);
	return value*cosine + CrossProduct(axis, value)*sine +
		axis*(DotProduct(axis, value)*(1.0f-cosine));
}

static bool
PoseToMatrix(const androidgame::PadInput::Pose &pose, CMatrix *matrix)
{
	if(!matrix || !pose.valid)
		return false;
	float right[3], up[3], forward[3], position[3];
	if(!rw::vulkan::playPoseToFirstPersonWorld(
	   pose.position, pose.orientation, right, up, forward, position))
		return false;
	matrix->SetUnity();
	matrix->GetRight() = VectorFromArray(right);
	matrix->GetUp() = VectorFromArray(up);
	matrix->GetForward() = VectorFromArray(forward);
	matrix->GetPosition() = VectorFromArray(position);
	return true;
}

static bool
IsGameplayAvailable()
{
	CPlayerPed *player = FindPlayerPed();
	return gVrFirstPersonActive && player && !player->DyingOrDead() &&
		!androidgame::VrKnockdownPresentationActive() &&
		!FrontEndMenuManager.m_bMenuActive &&
		!CCutsceneMgr::IsRunning() && !CCutsceneMgr::IsCutsceneProcessing() &&
		!TheCamera.m_WideScreenOn && !CTimer::GetIsPaused();
}

static void
NormalizeDualWieldHolsters()
{
	static const char *keys[HOLSTER_POINT_COUNT] = {
		"HolsterWaistLeftSlot", "HolsterWaistRightSlot", "HolsterChestLeftSlot",
		"HolsterChestRightSlot", "HolsterChestCentreSlot", "HolsterBackLeftSlot", "HolsterBackRightSlot"
	};
	for(int point = 0; point < HOLSTER_POINT_COUNT; point++){
		const int slot = gHolsterPointSlot[point];
		if(slot != WEAPONSLOT_HANDGUN && slot != WEAPONSLOT_SUBMACHINEGUN) continue;
		const bool dual = slot == WEAPONSLOT_HANDGUN ? gDualWieldPistol : gDualWieldSmg;
		const bool left = point == HOLSTER_WAIST_LEFT || point == HOLSTER_CHEST_LEFT || point == HOLSTER_BACK_LEFT;
		for(int other = 0; other < point; other++){
			const bool otherLeft = other == HOLSTER_WAIST_LEFT || other == HOLSTER_CHEST_LEFT || other == HOLSTER_BACK_LEFT;
			if(gHolsterPointSlot[other] == slot && (!dual || left == otherLeft)){
				gHolsterPointSlot[point] = -1;
				WritePrivateProfileStringA("VR", keys[point], "-1", kSettingsPath);
				break;
			}
		}
	}
}

static void
LoadSettings()
{
	if(gSettingsLoaded)
		return;
	InvalidateQuestWeaponLaserOverrides();
	gHandsEnabled = GetPrivateProfileIntA("VR", "VrHands", 1,
		kSettingsPath) != 0;
	gWeaponLaser = GetPrivateProfileIntA("VR", "WeaponLaser", 0,
		kSettingsPath) != 0;
	gHolsterHighlights = GetPrivateProfileIntA("VR", "HolsterHighlights", 0,
		kSettingsPath) != 0;
	gHolsterGrabDistance = clamp((int)(int32)GetPrivateProfileIntA(
		"VR", "HolsterGrabDistance", 0, kSettingsPath), 0,
		(int)ARRAY_SIZE(kHolsterGrabRadii)-1);
	gManualReload = GetPrivateProfileIntA("VR", "ManualReloading", 0,
		kSettingsPath) != 0;
	gPistolHoldToRepeat = GetPrivateProfileIntA("VR", "PistolHoldToRepeat", 0,
		kSettingsPath) != 0;
	gDualWieldPistol = GetPrivateProfileIntA("VR", "DualWieldPistol", 0, kSettingsPath) != 0;
	gDualWieldSmg = GetPrivateProfileIntA("VR", "DualWieldSmg", 0, kSettingsPath) != 0;
	const int storedScope = (int)(int32)GetPrivateProfileIntA("VR", "PhysicalScopeAim", 1,
		kSettingsPath);
	if(storedScope >= 0 && storedScope < SCOPE_AIM_MODE_COUNT)
		gScopeAimMode = storedScope;
	else
		gScopeAimMode = SCOPE_AIM_2D_OVERLAY;
	gGripLock = GetPrivateProfileIntA("VR", "WeaponGripLock", 0,
		kSettingsPath) != 0;
	gRunWithoutLimits = GetPrivateProfileIntA("VR", "RunWithoutLimits", 0,
		kSettingsPath) != 0;
	static const char *keys[HOLSTER_POINT_COUNT] = {
		"HolsterWaistLeftSlot", "HolsterWaistRightSlot",
		"HolsterChestLeftSlot", "HolsterChestRightSlot",
		"HolsterChestCentreSlot", "HolsterBackLeftSlot", "HolsterBackRightSlot"
	};
	for(int point = 0; point < HOLSTER_POINT_COUNT; point++){
		if(!keys[point])
			continue;
		const int stored = (int)(int32)GetPrivateProfileIntA("VR", keys[point],
			gHolsterPointSlot[point], kSettingsPath);
		if(stored >= -1 && stored < TOTAL_WEAPON_SLOTS)
			gHolsterPointSlot[point] = stored;
	}
	NormalizeDualWieldHolsters();
	// Keep the centre dedicated to throwables, including a saved empty point.
	if(gHolsterPointSlot[HOLSTER_CHEST_CENTRE] != -1 &&
	   gHolsterPointSlot[HOLSTER_CHEST_CENTRE] != WEAPONSLOT_GRENADE)
		gHolsterPointSlot[HOLSTER_CHEST_CENTRE] = WEAPONSLOT_PROJECTILE;
	gSettingsLoaded = true;
}

static const char *
CalibrationKey(int hand, const char *name, char *storage)
{
	if(hand == 0){
		sprintf(storage, "Left%s", name);
		return storage;
	}
	return name;
}

static int
ReadCalibrationValue(const char *section, int hand, const char *name,
	int fallback)
{
	char key[64];
	return (int)(int32)GetPrivateProfileIntA(section,
		CalibrationKey(hand, name, key), fallback, kSettingsPath);
}

// The laser override lives beside the weapon's calibration, in the same
// section, so it travels with everything else authored for that weapon.
// -1 follows the global switch, 0 is off, 1 is on.
static void
WeaponLaserSection(int weaponType, bool modern, char *section, int size)
{
	snprintf(section, size, modern ? "VRWeaponModern_%02d_%s" :
		"VRWeapon_%02d_%s", weaponType, GetVrWeaponName(weaponType));
}

static int
ReadWeaponLaserOverride(int weaponType)
{
	if(weaponType < 0 || weaponType >= WEAPONTYPE_TOTALWEAPONS)
		return -1;
	const bool modern = ModelSets::GetCalibrationSetForCategory(
		ModelSets::MODEL_CATEGORY_WEAPONS) == ModelSets::MODEL_SET_MODERN;
	WeaponLaserOverride &cached = gWeaponLaserOverrides[modern ? 1 : 0][weaponType];
	if(cached.loaded)
		return cached.value;
	char section[96];
	WeaponLaserSection(weaponType, modern, section, sizeof(section));
	const int stored = (int)(int32)GetPrivateProfileIntA(section,
		"Laser", -1, kSettingsPath);
	cached.value = stored < 0 ? -1 : (stored != 0 ? 1 : 0);
	cached.loaded = true;
	return cached.value;
}

static void
WriteWeaponLaserOverride(int weaponType, int value)
{
	if(weaponType < 0 || weaponType >= WEAPONTYPE_TOTALWEAPONS)
		return;
	const bool modern = ModelSets::GetCalibrationSetForCategory(
		ModelSets::MODEL_CATEGORY_WEAPONS) == ModelSets::MODEL_SET_MODERN;
	WeaponLaserOverride &cached = gWeaponLaserOverrides[modern ? 1 : 0][weaponType];
	char section[96], text[16];
	WeaponLaserSection(weaponType, modern, section, sizeof(section));
	snprintf(text, sizeof(text), "%d", value);
	cached.loaded = WritePrivateProfileStringA(section, "Laser", text, kSettingsPath) != 0;
	if(cached.loaded)
		cached.value = value < 0 ? -1 : (value != 0 ? 1 : 0);
}

static bool
CalibrationConfigured(const char *section, int hand)
{
	return ReadCalibrationValue(section, hand, "Configured", 0) != 0;
}

static bool
SupportCalibrationConfigured(const char *section, int hand)
{
	return ReadCalibrationValue(section, hand,
		"SupportGripConfigured", 0) != 0;
}

static bool
SectionHasCalibration(const char *section)
{
	return CalibrationConfigured(section, 0) ||
		CalibrationConfigured(section, 1) ||
		SupportCalibrationConfigured(section, 0) ||
		SupportCalibrationConfigured(section, 1);
}

static void
ApplyBuiltInHandCalibration(const BuiltInHandCalibration &value,
	WeaponCalibration *calibration)
{
	calibration->offsetX = value.main[0];
	calibration->offsetY = value.main[1];
	calibration->offsetZ = value.main[2];
	calibration->aimOffsetX = value.main[3];
	calibration->aimOffsetY = value.main[4];
	calibration->aimOffsetZ = value.main[5];
	calibration->aimRotationX = value.main[6];
	calibration->aimRotationY = value.main[7];
	calibration->aimRotationZ = value.main[8];
	calibration->rotationX = value.main[9];
	calibration->rotationY = value.main[10];
	calibration->rotationZ = value.main[11];
	calibration->supportX = value.support[0];
	calibration->supportY = value.support[1];
	calibration->supportZ = value.support[2];
}

static const BuiltInSupportGripDefaults *
FindBuiltInSupportGripDefaults(int weaponType, ModelSets::eModelSet modelSet)
{
	for(int index = 0; index < (int)ARRAY_SIZE(gBuiltInSupportGripDefaults);
	    index++){
		const BuiltInSupportGripDefaults &value =
			gBuiltInSupportGripDefaults[index];
		if(value.weaponType == weaponType && value.modelSet == modelSet)
			return &value;
	}
	return nil;
}

static void
ApplyMirroredSupportGrip(const WeaponCalibration &source,
	WeaponCalibration *mirrored)
{
	if(!mirrored)
		return;
	// V5 socket reflection is lateral only. Wrist Euler values are axial and
	// use the proven PC v0.5.0 local-basis conjugation (-X,+Y,-Z).
	mirrored->supportX = -source.supportX;
	mirrored->supportY = source.supportY;
	mirrored->supportZ = source.supportZ;
	mirrored->supportRotationX = -source.supportRotationX;
	mirrored->supportRotationY = source.supportRotationY;
	mirrored->supportRotationZ = -source.supportRotationZ;
	mirrored->supportGripType = source.supportGripType;
	mirrored->supportPoseVersion = source.supportPoseVersion;
}

static void
ApplyDefaultSupportGrip(int weaponType, int hand,
	ModelSets::eModelSet modelSet, WeaponCalibration *calibration)
{
	if(!calibration)
		return;
	const BuiltInSupportGripDefaults *value =
		FindBuiltInSupportGripDefaults(weaponType, modelSet);
	if(!value && modelSet == ModelSets::MODEL_SET_MODERN)
		value = FindBuiltInSupportGripDefaults(weaponType,
			ModelSets::MODEL_SET_CLASSIC);
	WeaponCalibration canonical = *calibration;
	if(value){
		canonical.supportX = value->offset[0];
		canonical.supportY = value->offset[1];
		canonical.supportZ = value->offset[2];
		canonical.supportRotationX = value->rotation[0];
		canonical.supportRotationY = value->rotation[1];
		canonical.supportRotationZ = value->rotation[2];
		canonical.supportGripType = value->gripType;
	}else{
		// Untuned weapons keep their established RIGHT support coordinates, but
		// enter the deterministic model-bound v5 contract.
		canonical.supportRotationX = 0;
		canonical.supportRotationY = 360;
		canonical.supportRotationZ = 0;
		canonical.supportGripType = SUPPORT_GRIP_MAGAZINE;
	}
	canonical.supportPoseVersion = SUPPORT_GRIP_MODEL_BOUND_V5;
	if(hand == 0)
		ApplyMirroredSupportGrip(canonical, calibration);
	else{
		calibration->supportX = canonical.supportX;
		calibration->supportY = canonical.supportY;
		calibration->supportZ = canonical.supportZ;
		calibration->supportRotationX = canonical.supportRotationX;
		calibration->supportRotationY = canonical.supportRotationY;
		calibration->supportRotationZ = canonical.supportRotationZ;
		calibration->supportGripType = canonical.supportGripType;
		calibration->supportPoseVersion = canonical.supportPoseVersion;
	}
}

static void
GetFallbackCalibrationForSet(int weaponType, int hand,
	ModelSets::eModelSet modelSet,
	WeaponCalibration *calibration)
{
	if(!calibration)
		return;
	*calibration = WeaponCalibration();
	if(hand < 0 || hand >= VR_HAND_COUNT){
		calibration->valid = true;
		return;
	}
	for(int index = 0;
	    index < (int)ARRAY_SIZE(gBuiltInWeaponDefaults); index++){
		const BuiltInWeaponDefaults &defaults =
			gBuiltInWeaponDefaults[index];
		if(defaults.weaponType != weaponType)
			continue;
		ApplyBuiltInHandCalibration(defaults.hand[hand], calibration);
		break;
	}
	for(int index = 0;
	    index < (int)ARRAY_SIZE(gBuiltInWeaponV050Overrides); index++){
		const BuiltInWeaponDefaults &defaults =
			gBuiltInWeaponV050Overrides[index];
		if(defaults.weaponType == weaponType){
			ApplyBuiltInHandCalibration(defaults.hand[hand], calibration);
			break;
		}
	}
	if(modelSet == ModelSets::MODEL_SET_MODERN){
		for(int index = 0;
		    index < (int)ARRAY_SIZE(gBuiltInWeaponSetDefaults); index++){
			const BuiltInWeaponSetDefaults &defaults =
				gBuiltInWeaponSetDefaults[index];
			if(defaults.weaponType == weaponType &&
			   defaults.modelSet == modelSet){
				ApplyBuiltInHandCalibration(defaults.hand[hand], calibration);
				break;
			}
		}
	}
	ApplyDefaultSupportGrip(weaponType, hand, modelSet, calibration);
	if(modelSet != ModelSets::MODEL_SET_MODERN){
		const QuestWeaponGrip::Profile *profile = QuestWeaponGrip::Find(weaponType);
		if(profile){
			// Keep the established controller-to-hand roll. Mesh Z-up does not
			// mean that the calibrated hand basis itself is world Z-up.
			calibration->rotationX = 0;
			calibration->rotationY = (int)roundf(profile->pitch*WEAPON_VALUE_SCALE);
			calibration->rotationZ = 0;
			const float a = DEGTORAD(profile->pitch);
			const float x = cosf(a)*profile->x+sinf(a)*profile->z;
			const float z = -sinf(a)*profile->x+cosf(a)*profile->z;
			// Put the physical grip in the closed palm, 5.5 cm ahead of the
			// controller origin and 4.5 cm below it (the baked wrist offset).
			calibration->offsetX = (int)roundf(-z*200.0f);
			calibration->offsetY = (int)roundf((0.055f-x)*200.0f);
			calibration->offsetZ = (int)roundf((-0.045f-profile->y)*200.0f);
		}
		// Wrist sockets beside/below III's actual foregrips, in lateral,
		// barrel, top coordinates. Mirror once for left-primary gameplay.
		static const BuiltInSupportGripDefaults grips[] = {
			{ WEAPONTYPE_SHOTGUN, ModelSets::MODEL_SET_CLASSIC, { -10,90,8 }, { 0,360,-292 }, SUPPORT_GRIP_FROM_BELOW },
			{ WEAPONTYPE_AK47, ModelSets::MODEL_SET_CLASSIC, { -10,68,6 }, { 0,360,-292 }, SUPPORT_GRIP_FROM_BELOW },
			{ WEAPONTYPE_M16, ModelSets::MODEL_SET_CLASSIC, { -10,76,12 }, { 0,360,-292 }, SUPPORT_GRIP_FROM_BELOW },
			{ WEAPONTYPE_SNIPERRIFLE, ModelSets::MODEL_SET_CLASSIC, { -10,60,4 }, { 0,360,-292 }, SUPPORT_GRIP_FROM_BELOW },
			{ WEAPONTYPE_ROCKETLAUNCHER, ModelSets::MODEL_SET_CLASSIC, { -10,34,-28 }, { 0,360,-292 }, SUPPORT_GRIP_FROM_BELOW },
			{ WEAPONTYPE_FLAMETHROWER, ModelSets::MODEL_SET_CLASSIC, { -14,36,10 }, { 0,360,-292 }, SUPPORT_GRIP_FROM_BELOW },
		};
		for(unsigned i = 0; i < ARRAY_SIZE(grips); ++i){
			if(grips[i].weaponType != weaponType) continue;
			WeaponCalibration canonical = *calibration;
			canonical.supportX = grips[i].offset[0];
			canonical.supportY = grips[i].offset[1];
			canonical.supportZ = grips[i].offset[2];
			canonical.supportRotationX = grips[i].rotation[0];
			canonical.supportRotationY = grips[i].rotation[1];
			canonical.supportRotationZ = grips[i].rotation[2];
			canonical.supportGripType = grips[i].gripType;
			canonical.supportPoseVersion = SUPPORT_GRIP_POSE_VERSION;
			if(hand == 0) ApplyMirroredSupportGrip(canonical, &canonical);
			calibration->supportX = canonical.supportX;
			calibration->supportY = canonical.supportY;
			calibration->supportZ = canonical.supportZ;
			calibration->supportRotationX = canonical.supportRotationX;
			calibration->supportRotationY = canonical.supportRotationY;
			calibration->supportRotationZ = canonical.supportRotationZ;
			calibration->supportGripType = canonical.supportGripType;
			calibration->supportPoseVersion = canonical.supportPoseVersion;
		}
	}
	calibration->valid = true;
}

static void
GetDefaultCalibrationForSet(int weaponType, int hand,
	ModelSets::eModelSet modelSet, WeaponCalibration *calibration)
{
	GetFallbackCalibrationForSet(weaponType, hand, modelSet, calibration);
	if(!calibration || modelSet == ModelSets::MODEL_SET_MODERN)
		return;
	const QuestWeaponFactoryDefaults::Profile *profile =
		QuestWeaponFactoryDefaults::Find(weaponType, hand);
	if(!profile)
		return;
	// Apply authored values last, without mirroring or coordinate conversion.
	BuiltInHandCalibration primary;
	memcpy(primary.main, profile->main, sizeof(primary.main));
	memcpy(primary.support, profile->support, sizeof(primary.support));
	ApplyBuiltInHandCalibration(primary, calibration);
	calibration->supportRotationX = profile->support[3];
	calibration->supportRotationY = profile->support[4];
	calibration->supportRotationZ = profile->support[5];
	calibration->supportGripType = profile->support[6];
	calibration->supportPoseVersion = profile->supportPoseVersion;
}

static void
GetDefaultCalibration(int weaponType, int hand,
	WeaponCalibration *calibration)
{
	GetDefaultCalibrationForSet(weaponType, hand,
		ModelSets::GetCalibrationSetForCategory(ModelSets::MODEL_CATEGORY_WEAPONS),
		calibration);
}

static WeaponCalibration *
GetCalibration(int hand, int weaponType)
{
	if(hand < 0 || hand >= VR_HAND_COUNT || weaponType < 0 ||
	   weaponType >= WEAPONTYPE_TOTALWEAPONS)
		return nil;
	WeaponCalibration &calibration = gCalibration[hand][weaponType];
	if(calibration.valid)
		return &calibration;
	char section[96];
	const bool modern = ModelSets::GetCalibrationSetForCategory(
		ModelSets::MODEL_CATEGORY_WEAPONS) == ModelSets::MODEL_SET_MODERN;
	ModelSets::eModelSet calibrationSet = modern ?
		ModelSets::MODEL_SET_MODERN : ModelSets::MODEL_SET_CLASSIC;
	sprintf(section, modern ? "VRWeaponModern_%02d_%s" :
		"VRWeapon_%02d_%s", weaponType, GetVrWeaponName(weaponType));
	if(modern && !SectionHasCalibration(section)){
		char classicSection[96];
		sprintf(classicSection, "VRWeapon_%02d_%s", weaponType,
			GetVrWeaponName(weaponType));
		if(SectionHasCalibration(classicSection)){
			strcpy(section, classicSection);
			calibrationSet = ModelSets::MODEL_SET_CLASSIC;
		}
	}
	GetDefaultCalibrationForSet(weaponType, hand, calibrationSet,
		&calibration);

	int readHand = hand;
	bool configured = CalibrationConfigured(section, hand);
	if(!configured && hand == 0 && CalibrationConfigured(section, 1)){
		readHand = 1;
		configured = true;
	}
	if(configured){
		WeaponCalibration defaults;
		GetDefaultCalibrationForSet(weaponType, readHand, calibrationSet,
			&defaults);
		const int storedScale =
			ReadCalibrationValue(section, readHand, "ValueScale", 1);
		const int conversion =
			storedScale == WEAPON_VALUE_SCALE ? 1 : WEAPON_VALUE_SCALE;
		const int fallbackDivisor = conversion;
		calibration.offsetX = clamp(ReadCalibrationValue(section, readHand,
			"OffsetX", defaults.offsetX/fallbackDivisor)*conversion,
			-100, 100);
		calibration.offsetY = clamp(ReadCalibrationValue(section, readHand,
			"OffsetY", defaults.offsetY/fallbackDivisor)*conversion,
			-100, 100);
		calibration.offsetZ = clamp(ReadCalibrationValue(section, readHand,
			"OffsetZ", defaults.offsetZ/fallbackDivisor)*conversion,
			-100, 100);
		calibration.aimOffsetX = clamp(ReadCalibrationValue(section, readHand,
			"AimOffsetX", defaults.aimOffsetX/fallbackDivisor)*conversion,
			-100, 100);
		calibration.aimOffsetY = clamp(ReadCalibrationValue(section, readHand,
			"AimOffsetY", defaults.aimOffsetY/fallbackDivisor)*conversion,
			-100, 100);
		calibration.aimOffsetZ = clamp(ReadCalibrationValue(section, readHand,
			"AimOffsetZ", defaults.aimOffsetZ/fallbackDivisor)*conversion,
			-100, 100);
		calibration.aimRotationX = clamp(ReadCalibrationValue(section,
			readHand, "AimRotationX",
			defaults.aimRotationX/fallbackDivisor)*conversion, -360, 360);
		calibration.aimRotationY = clamp(ReadCalibrationValue(section,
			readHand, "AimRotationY",
			defaults.aimRotationY/fallbackDivisor)*conversion, -360, 360);
		calibration.aimRotationZ = clamp(ReadCalibrationValue(section,
			readHand, "AimRotationZ",
			defaults.aimRotationZ/fallbackDivisor)*conversion, -360, 360);
		calibration.rotationX = clamp(ReadCalibrationValue(section, readHand,
			"RotationX", defaults.rotationX/fallbackDivisor)*conversion,
			-360, 360);
		calibration.rotationY = clamp(ReadCalibrationValue(section, readHand,
			"RotationY", defaults.rotationY/fallbackDivisor)*conversion,
			-360, 360);
		calibration.rotationZ = clamp(ReadCalibrationValue(section, readHand,
			"RotationZ", defaults.rotationZ/fallbackDivisor)*conversion,
			-360, 360);
	}

	int supportHand = hand;
	bool mirrorSupportFromRight = false;
	bool supportConfigured = SupportCalibrationConfigured(section, hand);
	if(!supportConfigured && hand == 0 &&
	   SupportCalibrationConfigured(section, 1)){
		supportHand = 1;
		supportConfigured = true;
		mirrorSupportFromRight = true;
	}
	if(supportConfigured){
		WeaponCalibration defaults;
		GetDefaultCalibrationForSet(weaponType, supportHand, calibrationSet,
			&defaults);
		const int storedScale = ReadCalibrationValue(section, supportHand,
			"SupportGripValueScale", 1);
		const int conversion =
			storedScale == WEAPON_VALUE_SCALE ? 1 : WEAPON_VALUE_SCALE;
		const int fallbackDivisor = conversion;
		calibration.supportX = clamp(ReadCalibrationValue(section, supportHand,
			"SupportGripOffsetX",
			defaults.supportX/fallbackDivisor)*conversion, -200, 200);
		calibration.supportY = clamp(ReadCalibrationValue(section, supportHand,
			"SupportGripOffsetY",
			defaults.supportY/fallbackDivisor)*conversion, -200, 200);
		calibration.supportZ = clamp(ReadCalibrationValue(section, supportHand,
			"SupportGripOffsetZ",
			defaults.supportZ/fallbackDivisor)*conversion, -200, 200);
		calibration.supportRotationX = clamp(ReadCalibrationValue(section,
			supportHand, "SupportGripRotationX",
			defaults.supportRotationX/fallbackDivisor)*conversion, -360, 360);
		calibration.supportRotationY = clamp(ReadCalibrationValue(section,
			supportHand, "SupportGripRotationY",
			defaults.supportRotationY/fallbackDivisor)*conversion, -360, 360);
		calibration.supportRotationZ = clamp(ReadCalibrationValue(section,
			supportHand, "SupportGripRotationZ",
			defaults.supportRotationZ/fallbackDivisor)*conversion, -360, 360);
		calibration.supportPoseVersion = clamp(ReadCalibrationValue(section,
			supportHand, "SupportGripPoseVersion",
			SUPPORT_GRIP_LEGACY_VERSION), 1, SUPPORT_GRIP_POSE_VERSION);
		calibration.supportGripType = clamp(ReadCalibrationValue(section, supportHand,
			"SupportGripStyle", (int)(int32)GetPrivateProfileIntA(section,
				"SupportGripStyle", defaults.supportGripType, kSettingsPath)),
			0, SUPPORT_GRIP_TYPE_COUNT-1);
		if(calibration.supportPoseVersion == SUPPORT_GRIP_MODEL_BOUND_V4){
			const int oldX = calibration.supportX;
			const int oldY = calibration.supportY;
			const int oldZ = calibration.supportZ;
			calibration.supportX = -oldZ;
			calibration.supportY = oldX;
			calibration.supportZ = -oldY;
			calibration.supportPoseVersion = SUPPORT_GRIP_MODEL_BOUND_V5;
		}
		if(mirrorSupportFromRight){
			WeaponCalibration canonical = calibration;
			ApplyMirroredSupportGrip(canonical, &calibration);
		}
	}
	calibration.valid = true;
	return &calibration;
}

static bool
IsPhysicalGun(int weaponType)
{
	switch(weaponType){
	case WEAPONTYPE_COLT45:
	case WEAPONTYPE_UZI:
	case WEAPONTYPE_SHOTGUN:
	case WEAPONTYPE_AK47:
	case WEAPONTYPE_M16:
	case WEAPONTYPE_SNIPERRIFLE:
	case WEAPONTYPE_ROCKETLAUNCHER:
	case WEAPONTYPE_FLAMETHROWER:
		return true;
	default:
		return false;
	}
}

static bool
IsPhysicalMelee(int weaponType)
{
	return weaponType == WEAPONTYPE_UNARMED ||
		weaponType == WEAPONTYPE_BASEBALLBAT;
}

static bool
IsPhysicalThrowable(int weaponType)
{
	return weaponType == WEAPONTYPE_GRENADE ||
		weaponType == WEAPONTYPE_MOLOTOV;
}

static bool
IsTwoHanded(int weaponType)
{
	switch(weaponType){
	case WEAPONTYPE_SHOTGUN:
	case WEAPONTYPE_AK47:
	case WEAPONTYPE_M16:
	case WEAPONTYPE_SNIPERRIFLE:
	case WEAPONTYPE_ROCKETLAUNCHER:
	case WEAPONTYPE_FLAMETHROWER:
		return true;
	default:
		return false;
	}
}

static float
GetOneHandAimSwayDegrees(int weaponType)
{
	switch(weaponType){
	case WEAPONTYPE_SHOTGUN: return 1.50f;
	case WEAPONTYPE_AK47:
	case WEAPONTYPE_M16: return 1.75f;
	case WEAPONTYPE_SNIPERRIFLE: return 2.25f;
	case WEAPONTYPE_ROCKETLAUNCHER: return 3.00f;
	case WEAPONTYPE_FLAMETHROWER: return 2.25f;
	default: return 0.0f;
	}
}

static bool
IsScopeWeapon(int weaponType)
{
	return weaponType == WEAPONTYPE_SNIPERRIFLE;
}

static bool
IsSupportHand(int hand)
{
	for(int primary = 0; primary < VR_HAND_COUNT; primary++)
		if(gSupportHand[primary] == hand)
			return true;
	return false;
}

static void
ClearSupportForHand(int hand)
{
	if(hand < 0 || hand >= VR_HAND_COUNT)
		return;
	gSupportHand[hand] = -1;
	for(int primary = 0; primary < VR_HAND_COUNT; primary++)
		if(gSupportHand[primary] == hand)
			gSupportHand[primary] = -1;
}

static bool
GetBodyFrame(CVector *origin, CVector *right, CVector *up,
	CVector *forward)
{
	CPlayerPed *player = FindPlayerPed();
	if(!player || !origin || !right || !up || !forward)
		return false;
	float vrRight[3], vrUp[3], vrAt[3], vrPosition[3];
	if(rw::vulkan::getFirstPersonViewFrame(
	   vrRight, vrUp, vrAt, vrPosition)){
		*origin = VectorFromArray(vrPosition);
		// Body sockets follow the HMD's yaw continuously and stay anchored to
		// the real room-space head position.  Ignore pitch/roll so looking up
		// cannot spin the belt or move it in front of the player.
		CVector headForward = VectorFromArray(vrAt);
		headForward.z = 0.0f;
		if(headForward.MagnitudeSqr() >= 0.0001f){
			headForward.Normalise();
			gBodyForward = headForward;
			gBodyForwardValid = true;
		}
		*forward = gBodyForwardValid ? gBodyForward :
			CVector(0.0f, 1.0f, 0.0f);
		*up = CVector(0.0f, 0.0f, 1.0f);
		*right = CrossProduct(*forward, *up);
		right->Normalise();
	}else{
		*right = player->GetRight();
		*up = player->GetUp();
		*forward = player->GetForward();
		if(right->MagnitudeSqr() < 0.0001f ||
		   up->MagnitudeSqr() < 0.0001f ||
		   forward->MagnitudeSqr() < 0.0001f)
			return false;
		right->Normalise();
		up->Normalise();
		forward->Normalise();
		*origin = player->GetPosition()+*up*1.55f;
	}
	return true;
}

static int
HolsterCopy(int point)
{
	return point == HOLSTER_WAIST_LEFT || point == HOLSTER_CHEST_LEFT ||
		point == HOLSTER_BACK_LEFT ? 0 : 1;
}

static bool
DualWieldSlot(int slot)
{
	return (slot == WEAPONSLOT_HANDGUN && gDualWieldPistol) ||
		(slot == WEAPONSLOT_SUBMACHINEGUN && gDualWieldSmg);
}

static bool
CopyInUse(int slot, int copy)
{
	for(int hand = 0; hand < VR_HAND_COUNT; hand++)
		if((gHeldSlot[hand] == slot && (!DualWieldSlot(slot) || gHeldCopy[hand] == copy)) ||
		   (gDropped[hand].slot == slot && (!DualWieldSlot(slot) || gDropped[hand].copy == copy)))
			return true;
	return false;
}

static int
FindHolsterPoint(int slot, int copy = -1)
{
	for(int point = 0; point < HOLSTER_POINT_COUNT; point++)
		if(gHolsterPointSlot[point] == slot &&
		   (copy < 0 || !DualWieldSlot(slot) || HolsterCopy(point) == copy))
			return point;
	if(slot == WEAPONSLOT_SNIPER){
		if(!OculusVR::IsVrWeaponSlotOwned(WEAPONSLOT_RIFLE))
			return HOLSTER_BACK_LEFT;
		if(!OculusVR::IsVrWeaponSlotOwned(WEAPONSLOT_HEAVY))
			return HOLSTER_BACK_RIGHT;
		return HOLSTER_BACK_LEFT;
	}
	return -1;
}

static CVector
WeaponMatrixToLocalVector(const CVector &world, const CMatrix &basis)
{
	return CVector(
		DotProduct(world, basis.GetRight()),
		DotProduct(world, basis.GetForward()),
		DotProduct(world, basis.GetUp()));
}

static CVector
WeaponMatrixFromLocalVector(const CVector &local, const CMatrix &basis)
{
	return basis.GetRight()*local.x +
		basis.GetForward()*local.y +
		basis.GetUp()*local.z;
}

static bool
BuildLegacyTrackedWeaponHandBasis(int hand, CVector *position, CVector *right,
	CVector *up, CVector *forward)
{
	if(hand < 0 || hand >= VR_HAND_COUNT || !position || !right || !up ||
	   !forward || !gPoseValid[hand])
		return false;

	*position = gGripMatrix[hand].GetPosition();
	*forward = gAimMatrix[hand].GetForward();
	if(forward->MagnitudeSqr() < 0.0001f)
		return false;
	forward->Normalise();

	const int modelHand = 1-hand;
	*up = gGripMatrix[hand].GetRight()*(modelHand == 0 ? 1.0f : -1.0f);
	*up -= *forward*DotProduct(*up, *forward);
	if(up->MagnitudeSqr() < 0.0001f)
		return false;
	up->Normalise();

	*right = CrossProduct(*up, *forward);
	if(right->MagnitudeSqr() < 0.0001f)
		return false;
	right->Normalise();

	if(DotProduct(*right, gGripMatrix[hand].GetForward()) < 0.0f)
		*right *= -1.0f;
	return true;
}

static bool
BuildLegacyWeaponModelMatrix(int hand, const WeaponCalibration &calibration,
	CMatrix *matrix)
{
	if(!matrix)
		return false;

	CVector position, right, up, forward;
	if(!BuildLegacyTrackedWeaponHandBasis(
	   hand, &position, &right, &up, &forward))
		return false;

	matrix->SetUnity();
	matrix->GetRight() = right*-1.0f;
	matrix->GetForward() = forward*-1.0f;
	matrix->GetUp() = up;
	matrix->GetPosition() = position+
		right*((float)calibration.offsetX/200.0f)+
		forward*((float)calibration.offsetY/200.0f)+
		up*((float)calibration.offsetZ/200.0f);

	CMatrix base, rotationX, rotationY, rotationZ;
	base.SetRotate(DEGTORAD(90.0f), 0.0f, DEGTORAD(-90.0f));
	rotationX.SetRotateX(DEGTORAD((float)calibration.rotationX/
		WEAPON_VALUE_SCALE));
	rotationY.SetRotateY(DEGTORAD((float)calibration.rotationY/
		WEAPON_VALUE_SCALE));
	rotationZ.SetRotateZ(DEGTORAD((float)calibration.rotationZ/
		WEAPON_VALUE_SCALE));

	*matrix = *matrix*base*rotationX*rotationY*rotationZ;
	return true;
}

static bool
BuildLegacyAimNoTwoHand(int hand, const WeaponCalibration &calibration,
	CVector *source, CVector *direction)
{
	if(!source || !direction || hand < 0 || hand >= VR_HAND_COUNT ||
	   !gPoseValid[hand])
		return false;

	CVector right = gAimMatrix[hand].GetRight();
	CVector up = gAimMatrix[hand].GetUp();
	CVector forward = gAimMatrix[hand].GetForward();
	if(right.MagnitudeSqr() < 0.0001f ||
	   up.MagnitudeSqr() < 0.0001f ||
	   forward.MagnitudeSqr() < 0.0001f)
		return false;

	forward.Normalise();
	up -= forward*DotProduct(up, forward);
	if(up.MagnitudeSqr() < 0.0001f)
		return false;
	up.Normalise();

	// Preserve the exact original non-IK lateral axis behavior.
	right -= forward*DotProduct(right, forward);
	right -= up*DotProduct(right, up);
	if(right.MagnitudeSqr() < 0.0001f)
		return false;
	right.Normalise();

	const float pitch =
		DEGTORAD((float)calibration.aimRotationX/WEAPON_VALUE_SCALE);
	const float yaw =
		DEGTORAD((float)calibration.aimRotationY/WEAPON_VALUE_SCALE);
	const float roll =
		DEGTORAD((float)calibration.aimRotationZ/WEAPON_VALUE_SCALE);

	if(pitch != 0.0f){
		forward = RotateAroundAxis(forward, right, pitch);
		up = RotateAroundAxis(up, right, pitch);
	}
	if(yaw != 0.0f){
		forward = RotateAroundAxis(forward, up, yaw);
		right = RotateAroundAxis(right, up, yaw);
	}
	if(roll != 0.0f){
		right = RotateAroundAxis(right, forward, roll);
		up = RotateAroundAxis(up, forward, roll);
	}

	forward.Normalise();
	*direction = forward;
	*source = gAimMatrix[hand].GetPosition() +
		right*((float)calibration.aimOffsetX/200.0f) +
		up*((float)calibration.aimOffsetY/200.0f) +
		forward*(0.18f+(float)calibration.aimOffsetZ/200.0f);
	return true;
}

static bool BuildWeaponModelMatrixBase(
	int hand, const WeaponCalibration &calibration, CMatrix *matrix);
static void ApplyBakedLeftWeaponPosition(int hand, CMatrix *matrix);

static bool
BuildTrackedWeaponHandBasis(int hand, CVector *position, CVector *right,
	CVector *up, CVector *forward)
{
	if(hand < 0 || hand >= VR_HAND_COUNT || !position || !right || !up ||
	   !forward || !gPoseValid[hand])
		return false;
	if(VrBodyIK::WeaponOwnershipActive() &&
	   VrBodyIK::GetRigidWeaponHandBasis(
		   hand, position, right, up, forward))
		return true;
	*position = gGripMatrix[hand].GetPosition();
	*forward = gAimMatrix[hand].GetForward();
	forward->Normalise();
	const int modelHand = 1-hand;
	*up = gGripMatrix[hand].GetRight()*(modelHand == 0 ? 1.0f : -1.0f);
	*up -= *forward*DotProduct(*up, *forward);
	if(up->MagnitudeSqr() < 0.0001f)
		return false;
	up->Normalise();
	*right = CrossProduct(*up, *forward);
	if(right->MagnitudeSqr() < 0.0001f)
		return false;
	right->Normalise();
	if(DotProduct(*right, gGripMatrix[hand].GetForward()) < 0.0f)
		*right *= -1.0f;
	return true;
}

static bool
BuildWeaponModelMatrixBase(int hand, const WeaponCalibration &calibration,
	CMatrix *matrix)
{
	if(!matrix)
		return false;
	CVector position, right, up, forward;
	if(!BuildTrackedWeaponHandBasis(hand, &position, &right, &up, &forward))
		return false;
	matrix->SetUnity();
	matrix->GetRight() = right*-1.0f;
	matrix->GetForward() = forward*-1.0f;
	matrix->GetUp() = up;
	matrix->GetPosition() = position+
		right*((float)calibration.offsetX/200.0f)+
		forward*((float)calibration.offsetY/200.0f)+
		up*((float)calibration.offsetZ/200.0f);
	CMatrix base, rotationX, rotationY, rotationZ;
	base.SetRotate(DEGTORAD(90.0f), 0.0f, DEGTORAD(-90.0f));
	rotationX.SetRotateX(DEGTORAD((float)calibration.rotationX/
		WEAPON_VALUE_SCALE));
	rotationY.SetRotateY(DEGTORAD((float)calibration.rotationY/
		WEAPON_VALUE_SCALE));
	rotationZ.SetRotateZ(DEGTORAD((float)calibration.rotationZ/
		WEAPON_VALUE_SCALE));
	*matrix = *matrix*base*rotationX*rotationY*rotationZ;
	return true;
}

static bool
BuildWeaponModelMatrix(int hand, const WeaponCalibration &calibration,
	CMatrix *matrix)
{
	if(!BuildWeaponModelMatrixBase(hand, calibration, matrix))
		return false;

	// Position-only baked LEFT-hand correction selected from the player's
	// final completed calibration. No runtime calibration mode remains.
	ApplyBakedLeftWeaponPosition(hand, matrix);
	return true;
}

static CVector
WeaponHandLocalToWorld(const CVector &local, const CVector &right,
	const CVector &up, const CVector &forward)
{
	return right*local.x + forward*local.y + up*local.z;
}

static void
ApplyBakedLeftWeaponPosition(int hand, CMatrix *matrix)
{
	if(!matrix || hand != 0 ||
	   ModelSets::GetCalibrationSetForCategory(ModelSets::MODEL_CATEGORY_WEAPONS) !=
	   ModelSets::MODEL_SET_MODERN)
		return;

	CVector handPosition, right, up, forward;
	if(!BuildTrackedWeaponHandBasis(
	   hand, &handPosition, &right, &up, &forward))
		return;

	matrix->GetPosition() += WeaponHandLocalToWorld(
		kBakedLeftWeaponDeltaLocal, right, up, forward);
}

static bool
BuildCanonicalSupportFrame(const CVector &forwardCandidate,
	const CVector &upCandidate, CVector *right, CVector *up,
	CVector *forward)
{
	if(!right || !up || !forward ||
	   !isfinite(forwardCandidate.x) || !isfinite(forwardCandidate.y) ||
	   !isfinite(forwardCandidate.z) || !isfinite(upCandidate.x) ||
	   !isfinite(upCandidate.y) || !isfinite(upCandidate.z))
		return false;
	*forward = forwardCandidate;
	if(forward->MagnitudeSqr() < 0.0001f)
		return false;
	forward->Normalise();
	*up = upCandidate-*forward*DotProduct(upCandidate, *forward);
	if(up->MagnitudeSqr() < 0.0001f)
		return false;
	up->Normalise();
	*right = CrossProduct(*forward, *up);
	if(right->MagnitudeSqr() < 0.0001f)
		return false;
	right->Normalise();
	*up = CrossProduct(*right, *forward);
	up->Normalise();
	return true;
}

static bool
IsHolsterWithinGrabDistance(int point, float distance)
{
	const bool behind = point == HOLSTER_BACK_LEFT || point == HOLSTER_BACK_RIGHT;
	return point >= 0 && point < HOLSTER_POINT_COUNT &&
		distance <= kHolsterGrabRadii[behind ? 0 : gHolsterGrabDistance];
}

static bool
BuildHolsterMatrix(int slot, CMatrix *matrix, int copy = -1)
{
	if(!matrix || slot <= WEAPONSLOT_UNARMED ||
	   slot >= TOTAL_WEAPON_SLOTS ||
	   (FindPlayerVehicle() &&
	    (!IsImmersiveDrivingActive() ||
	     !IsImmersiveVehicleSidearm(GetVrWeaponTypeForSlot(slot)))))
		return false;
	const int point = FindHolsterPoint(slot, copy);
	if(point < 0)
		return false;
	static const float lateral[HOLSTER_POINT_COUNT] = {
		-0.27f, 0.27f, -0.21f, 0.21f, 0.0f, 0.24f, -0.24f
	};
	static const float vertical[HOLSTER_POINT_COUNT] = {
		-0.58f, -0.58f, -0.30f, -0.30f, -0.36f, -0.14f, -0.14f
	};
	static const float depth[HOLSTER_POINT_COUNT] = {
		0.07f, 0.07f, 0.12f, 0.12f, 0.14f, -0.23f, -0.23f
	};
	CVector origin, right, up, forward;
	if(!GetBodyFrame(&origin, &right, &up, &forward))
		return false;
	const bool behind =
		point == HOLSTER_BACK_LEFT || point == HOLSTER_BACK_RIGHT;
	matrix->SetUnity();
	matrix->GetRight() = forward*(behind ? 1.0f : -1.0f);
	matrix->GetForward() = up;
	matrix->GetUp() = right*(behind ? 1.0f : -1.0f);
	matrix->GetPosition() = origin + right*lateral[point] +
		up*vertical[point] + forward*depth[point];
	return true;
}

static void
GetSupportModelAxes(int weaponType, const WeaponCalibration &calibration,
	const CMatrix &model, CVector *barrel, CVector *top)
{
	*barrel = model.GetRight();
	*top = calibration.supportPoseVersion >= SUPPORT_GRIP_POSE_VERSION ?
		model.GetUp() : model.GetForward();
	if(calibration.supportPoseVersion >= SUPPORT_GRIP_POSE_VERSION &&
	   weaponType == WEAPONTYPE_ROCKETLAUNCHER){
		// III's launcher tube rises diagonally along native Z, unlike its rifles.
		*barrel = model.GetRight()*-0.300706f+model.GetUp()*0.953717f;
		*top = model.GetRight()*-0.953717f-model.GetUp()*0.300706f;
	}
}

static bool
BuildSupportVector(int primary, int weaponType, CVector *pivot,
	CVector *expected, CVector *frameRight = nil, CVector *frameUp = nil,
	CVector *frameForward = nil)
{
	if(primary < 0 || primary >= VR_HAND_COUNT || !pivot || !expected ||
	   !gPoseValid[primary] || !IsTwoHanded(weaponType) ||
	   ((frameRight || frameUp || frameForward) &&
	    (!frameRight || !frameUp || !frameForward)))
		return false;
	WeaponCalibration *calibration =
		GetCalibration(primary, weaponType);
	if(!calibration)
		return false;
	CVector rawRight, rawUp, rawForward;
	if(!BuildTrackedWeaponHandBasis(primary, pivot, &rawRight, &rawUp,
	   &rawForward))
		return false;
	CVector socketRight = rawRight;
	CVector socketUp = rawUp;
	CVector socketForward = rawForward;
	CVector visualRight = rawRight;
	CVector visualUp = rawUp;
	CVector visualForward = rawForward;
	CVector socketOrigin = *pivot;
	if(calibration->supportPoseVersion >= SUPPORT_GRIP_MODEL_BOUND_V5){
		CMatrix weaponModel;
		if(!BuildWeaponModelMatrix(primary, *calibration, &weaponModel))
			return false;
		CVector barrel, top;
		GetSupportModelAxes(weaponType, *calibration, weaponModel, &barrel, &top);
		CVector lateral;
		if(!BuildCanonicalSupportFrame(barrel, top,
		   &lateral, &top, &barrel))
			return false;
		socketRight = lateral;
		socketForward = barrel;
		socketUp = top;
		visualRight = barrel;
		visualUp = lateral*-1.0f;
		visualForward = top*-1.0f;
		socketOrigin = weaponModel.GetPosition();
	}
	const CVector socket = socketOrigin+
		socketRight*((float)calibration->supportX/200.0f)+
		socketForward*((float)calibration->supportY/200.0f)+
		socketUp*((float)calibration->supportZ/200.0f);
	*expected = socket-*pivot;
	if(frameRight){
		if(calibration->supportPoseVersion < SUPPORT_GRIP_MODEL_BOUND_V5 &&
		   !BuildCanonicalSupportFrame(visualForward, visualUp,
		   &visualRight, &visualUp, &visualForward))
			return false;
		*frameRight = visualRight;
		*frameUp = visualUp;
		*frameForward = visualForward;
	}
	return expected->MagnitudeSqr() > 0.0001f;
}

static bool
BuildTwoHandRotation(int primary, int weaponType, CVector *pivot,
	CVector *axis, float *angle)
{
	if(!pivot || !axis || !angle || primary < 0 ||
	   primary >= VR_HAND_COUNT)
		return false;
	const int support = gSupportHand[primary];
	if(support < 0 || support >= VR_HAND_COUNT || !gPoseValid[support])
		return false;
	CVector expected;
	if(!BuildSupportVector(primary, weaponType, pivot, &expected))
		return false;
	CVector actual = gGripMatrix[support].GetPosition()-*pivot;
	if(actual.MagnitudeSqr() < 0.0001f)
		return false;
	expected.Normalise();
	actual.Normalise();
	const float dot = clamp(DotProduct(expected, actual), -1.0f, 1.0f);
	*axis = CrossProduct(expected, actual);
	if(axis->MagnitudeSqr() < 0.000001f){
		*axis = CVector(0.0f, 0.0f, 1.0f);
		*angle = 0.0f;
		return true;
	}
	const float sine = axis->Magnitude();
	*axis *= 1.0f/sine;
	*angle = Min(Atan2(sine, dot), DEGTORAD(90.0f));
	return true;
}

static void
GiveWeaponToHand(int hand, int slot, int copy = -1)
{
	ClearSupportForHand(hand);
	gHeldSlot[hand] = slot;
	gHeldCopy[hand] = copy < 0 ? hand : copy;
	gHolsterSelection[hand] = slot;
	gWeaponRenderSlot[hand] = -1;
	gWeaponContactSlot[hand] = -1;
	gMeleeMotion[hand] = MeleeMotion();
	QUEST_WEAPON_LOG("[weapons] hand=%d grabbed slot=%d type=%d",
		hand, slot, GetVrWeaponTypeForSlot(slot));
}

static bool
BuildDroppedMatrix(int hand, CMatrix *matrix)
{
	if(!matrix || hand < 0 || hand >= VR_HAND_COUNT ||
	   gDropped[hand].slot < 0)
		return false;
	const uint32 now = CTimer::GetTimeInMillisecondsNonClipped();
	const float elapsed =
		(float)(now-gDropped[hand].startTime)*0.001f;
	if(elapsed > 1.8f)
		return false;
	*matrix = gDropped[hand].matrix;
	if(gDropped[hand].angularSpeed > 0.001f){
		const float angle = gDropped[hand].angularSpeed*elapsed;
		matrix->GetRight() = RotateAroundAxis(matrix->GetRight(),
			gDropped[hand].angularAxis, angle);
		matrix->GetUp() = RotateAroundAxis(matrix->GetUp(),
			gDropped[hand].angularAxis, angle);
		matrix->GetForward() = RotateAroundAxis(matrix->GetForward(),
			gDropped[hand].angularAxis, angle);
	}
	matrix->GetPosition() = gDropped[hand].matrix.GetPosition() +
		gDropped[hand].velocity*elapsed -
		CVector(0.0f, 0.0f, 1.0f)*(2.1f*elapsed*elapsed);
	return true;
}

static void
StartDroppedWeapon(int hand, int slot)
{
	if(hand < 0 || hand >= VR_HAND_COUNT || slot < 0)
		return;
	DroppedWeapon dropped;
	dropped.slot = slot;
	dropped.copy = gHeldCopy[hand];
	dropped.startTime = CTimer::GetTimeInMillisecondsNonClipped();
	if(gWeaponRenderSlot[hand] == slot)
		dropped.matrix = gWeaponRenderMatrix[hand];
	else
		dropped.matrix = gGripMatrix[hand];
	dropped.velocity = gHandVelocity[hand];
	const float speed = dropped.velocity.Magnitude();
	if(speed > 6.0f)
		dropped.velocity *= 6.0f/speed;
	dropped.angularAxis =
		CrossProduct(gGripMatrix[hand].GetForward(),
			gAimMatrix[hand].GetForward());
	if(dropped.angularAxis.MagnitudeSqr() < 0.0001f)
		dropped.angularAxis = gGripMatrix[hand].GetRight();
	dropped.angularAxis.Normalise();
	dropped.angularSpeed = 4.5f+Min(speed, 4.0f);
	gDropped[hand] = dropped;
	gWeaponRenderSlot[hand] = -1;
	gWeaponContactSlot[hand] = -1;
}

static void
ExpireDroppedWeapons()
{
	const uint32 now = CTimer::GetTimeInMillisecondsNonClipped();
	for(int hand = 0; hand < VR_HAND_COUNT; hand++)
		if(gDropped[hand].slot >= 0 &&
		   now-gDropped[hand].startTime > 1800U)
			gDropped[hand] = DroppedWeapon();
}

static bool
ManualReloadAvailable()
{
	LoadSettings();
	return gManualReload && gHandsEnabled && IsGameplayAvailable() &&
		FindPlayerVehicle() == nil;
}

static bool
IsMagazineHandInUse(int hand)
{
	for(int weaponHand = 0; weaponHand < VR_HAND_COUNT; weaponHand++)
		if(gManualReloadState[weaponHand].active &&
		   gManualReloadState[weaponHand].magazineHand == hand)
			return true;
	return false;
}

static bool
BuildManualReloadSpotMatrix(int slot, CMatrix *matrix, int weaponHand)
{
	if(!matrix)
		return false;
	CMatrix holster;
	const int copy = gHeldCopy[weaponHand];
	const bool assigned = BuildHolsterMatrix(slot, &holster, copy);
	CVector origin, right, up, forward;
	if(!GetBodyFrame(&origin, &right, &up, &forward))
		return false;
	matrix->SetUnity();
	matrix->GetRight() = right;
	matrix->GetUp() = up;
	matrix->GetForward() = forward;
	matrix->GetPosition() = assigned ? holster.GetPosition() :
		origin-up*0.34f+forward*0.14f;
	return true;
}

static bool
BuildManualReloadHeldMatrix(int hand, CMatrix *matrix)
{
	if(!matrix || hand < 0 || hand >= VR_HAND_COUNT ||
	   !gPoseValid[hand])
		return false;
	const CMatrix &pose = gGripMatrix[hand];
	matrix->SetUnity();
	matrix->GetRight() = pose.GetRight();
	matrix->GetUp() = pose.GetForward();
	matrix->GetForward() = pose.GetUp();
	matrix->GetPosition() = pose.GetPosition()+
		pose.GetForward()*0.035f-pose.GetUp()*0.015f;
	return true;
}

static bool
BuildManualReloadSocketPosition(int weaponHand, CVector *position)
{
	if(!position || weaponHand < 0 || weaponHand >= VR_HAND_COUNT ||
	   !gPoseValid[weaponHand])
		return false;
	const CMatrix &pose = gAimMatrix[weaponHand];
	CVector up = pose.GetUp();
	CVector forward = pose.GetForward();
	up.Normalise();
	forward.Normalise();
	*position = pose.GetPosition()-up*0.060f+forward*0.020f;
	return true;
}

static void
ResetManualReloadState(ManualReloadState &state)
{
	state = ManualReloadState();
}

static void
UpdateManualReloadInput(bool blocked)
{
	const uint32 now = CTimer::GetTimeInMillisecondsNonClipped();
	if(!ManualReloadAvailable()){
		for(int weaponHand = 0; weaponHand < VR_HAND_COUNT; weaponHand++)
			ResetManualReloadState(gManualReloadState[weaponHand]);
		for(int hand = 0; hand < VR_HAND_COUNT; hand++)
			gManualReloadGripDown[hand] = gGrip[hand] >= 0.45f;
		return;
	}

	for(int weaponHand = 0; weaponHand < VR_HAND_COUNT; weaponHand++){
		ManualReloadState &reload = gManualReloadState[weaponHand];
		if(reload.active &&
		   (gHeldSlot[weaponHand] != reload.slot ||
		    !gPoseValid[weaponHand]))
			ResetManualReloadState(reload);
	}

	// A held magazine must first leave its body socket, then enter the
	// magwell. This prevents an instant reload when the gun is close to the
	// same chest/waist holster.
	for(int weaponHand = 0; weaponHand < VR_HAND_COUNT; weaponHand++){
		ManualReloadState &reload = gManualReloadState[weaponHand];
		const int magazineHand = reload.magazineHand;
		if(!reload.active || reload.requested || magazineHand < 0 ||
		   blocked)
			continue;
		if(gGrip[magazineHand] <= 0.30f){
			reload.magazineHand = -1;
			reload.movedAwayFromSpot = false;
			reload.grabbedAt = 0;
			continue;
		}
		CMatrix heldMatrix, spotMatrix;
		CVector socketPosition;
		if(!BuildManualReloadHeldMatrix(magazineHand, &heldMatrix) ||
		   !BuildManualReloadSpotMatrix(reload.slot, &spotMatrix, weaponHand) ||
		   !BuildManualReloadSocketPosition(weaponHand, &socketPosition))
			continue;
		if((heldMatrix.GetPosition()-spotMatrix.GetPosition()).Magnitude() >=
		   0.08f)
			reload.movedAwayFromSpot = true;
		if(reload.movedAwayFromSpot && now-reload.grabbedAt >= 150U &&
		   (heldMatrix.GetPosition()-socketPosition).Magnitude() <= 0.115f){
			reload.requested = true;
			QUEST_WEAPON_LOG("[weapons] reload inserted hand=%d slot=%d",
				weaponHand, reload.slot);
		}
	}

	// A free hand grips the nearest available magazine.
	if(!blocked){
		for(int hand = 0; hand < VR_HAND_COUNT; hand++){
			if(IsMagazineHandInUse(hand) || IsSupportHand(hand) ||
			   IsTrackedDetonatorHandReserved(hand) ||
			   gHeldSlot[hand] >= 0 || !gPoseValid[hand] ||
			   gGrip[hand] < 0.65f || gManualReloadGripDown[hand])
				continue;
			float closestDistance = 0.20f;
			int closestWeaponHand = -1;
			for(int weaponHand = 0;
			    weaponHand < VR_HAND_COUNT; weaponHand++){
				ManualReloadState &reload =
					gManualReloadState[weaponHand];
				if(!reload.active || reload.requested ||
				   reload.magazineHand >= 0 || hand == weaponHand)
					continue;
				CMatrix spotMatrix;
				if(!BuildManualReloadSpotMatrix(reload.slot,
				   &spotMatrix, weaponHand))
					continue;
				const float distance =
					(gGripMatrix[hand].GetPosition()-
					 spotMatrix.GetPosition()).Magnitude();
				if(distance < closestDistance){
					closestDistance = distance;
					closestWeaponHand = weaponHand;
				}
			}
			if(closestWeaponHand >= 0){
				ManualReloadState &reload =
					gManualReloadState[closestWeaponHand];
				reload.magazineHand = hand;
				reload.movedAwayFromSpot = false;
				reload.grabbedAt = now;
				gGripLatched[hand] = true;
				QUEST_WEAPON_LOG(
					"[weapons] reload magazine grabbed hand=%d slot=%d",
					hand, reload.slot);
			}
		}
	}

	for(int hand = 0; hand < VR_HAND_COUNT; hand++){
		if(gGrip[hand] <= 0.30f)
			gManualReloadGripDown[hand] = false;
		else if(gGrip[hand] >= 0.65f)
			gManualReloadGripDown[hand] = true;
	}
}

static int
GetTrackedRemoteGrenadeHand()
{
	return -1;
}

static bool
HasTrackedRemoteCharges()
{
	return false;
}

static bool
IsTrackedDetonatorHandReserved(int hand)
{
	if(hand < 0 || hand >= VR_HAND_COUNT || !gHandsEnabled ||
	   !IsGameplayAvailable() || FindPlayerVehicle() ||
	   !HasTrackedRemoteCharges() && GetTrackedRemoteGrenadeHand() < 0)
		return false;
	const int grenadeHand = GetTrackedRemoteGrenadeHand();
	const int desiredHand = grenadeHand >= 0 ? 1-grenadeHand :
		gTrackedDetonatorHand;
	return desiredHand == hand && gHeldSlot[hand] < 0 &&
		!IsSupportHand(hand) && !IsMagazineHandInUse(hand);
}

static void
UpdateTrackedDetonatorInput(bool blocked)
{
	for(int hand = 0; hand < VR_HAND_COUNT; hand++){
		const bool active = IsTrackedDetonatorHandReserved(hand) &&
			gPoseValid[hand];
		gTrackedDetonatorJustPressed[hand] = false;
		if(!active){
			gTrackedDetonatorWasActive[hand] = false;
			gTrackedDetonatorWaitForRelease[hand] = false;
			continue;
		}
		if(!gTrackedDetonatorWasActive[hand])
			gTrackedDetonatorWaitForRelease[hand] =
				gTrigger[hand] > 0.30f;
		gTrackedDetonatorWasActive[hand] = true;
		if(blocked){
			if(gTrigger[hand] > 0.30f)
				gTrackedDetonatorWaitForRelease[hand] = true;
			continue;
		}
		if(gTrackedDetonatorWaitForRelease[hand]){
			if(gTrigger[hand] <= 0.30f)
				gTrackedDetonatorWaitForRelease[hand] = false;
			continue;
		}
		gTrackedDetonatorJustPressed[hand] =
			gTriggerJustPressed[hand];
	}
}

static void
ClearAttachedMissionWeaponState()
{
	if(!gAttachedMissionWeaponForced)
		return;
	for(int hand = 0; hand < VR_HAND_COUNT; hand++){
		if(gHeldSlot[hand] == gAttachedMissionWeaponSlot)
			gHeldSlot[hand] = -1;
		ClearSupportForHand(hand);
		gHolsterSelection[hand] = -1;
		gWeaponRenderSlot[hand] = -1;
		gWeaponContactSlot[hand] = -1;
		gDropped[hand] = DroppedWeapon();
		ResetManualReloadState(gManualReloadState[hand]);
		gManualReloadGripDown[hand] = gGrip[hand] >= 0.45f;
		gGripLatched[hand] = gGrip[hand] >= 0.45f;
		gMeleeMotion[hand] = MeleeMotion();
		gMeleeStrike[hand] = MeleeStrike();
		gThrowablePreview[hand] = false;
	}
	gAttachedMissionWeaponForced = false;
	gAttachedMissionWeaponSlot = -1;
}

static bool
UpdateAttachedMissionWeaponInput()
{
	ClearAttachedMissionWeaponState();
	return false;
}

static bool
IsAuxiliaryHandReserved(int hand)
{
	return IsMagazineHandInUse(hand) ||
		IsTrackedDetonatorHandReserved(hand);
}

static void
UpdatePoses(const androidgame::PadInput &input)
{
	const uint32 now = CTimer::GetTimeInMillisecondsNonClipped();
	for(int hand = 0; hand < VR_HAND_COUNT; hand++){
		CMatrix gripMatrix, aimMatrix;
		const bool gripValid = PoseToMatrix(input.gripPose[hand], &gripMatrix);
		const bool aimValid = PoseToMatrix(input.aimPose[hand], &aimMatrix);
		gPoseValid[hand] = gripValid;
		if(!gripValid){
			gPreviousPoseTime[hand] = 0;
			gHandVelocity[hand] = CVector(0.0f, 0.0f, 0.0f);
			continue;
		}
		gGripMatrix[hand] = gripMatrix;
		gAimMatrix[hand] = aimValid ? aimMatrix : gripMatrix;
		if(gPreviousPoseTime[hand] != 0 && now > gPreviousPoseTime[hand]){
			const float dt =
				(float)(now-gPreviousPoseTime[hand])*0.001f;
			if(dt > 0.001f && dt < 0.12f)
				gHandVelocity[hand] =
					(gripMatrix.GetPosition()-
					 gPreviousHandPosition[hand])/dt;
			else
				gHandVelocity[hand] = CVector(0.0f, 0.0f, 0.0f);
		}
		gPreviousHandPosition[hand] = gripMatrix.GetPosition();
		gPreviousPoseTime[hand] = now;
	}
	gGrip[0] = input.leftGrip;
	gGrip[1] = input.rightGrip;
	gTrigger[0] = input.leftTrigger;
	gTrigger[1] = input.rightTrigger;
}

static void
UpdateWeaponTriggerEdges(bool blocked, uint32 blockedHands,
	const androidgame::PadInput &input)
{
	const bool inVehicle = FindPlayerVehicle() != nil;
	const bool vehicleWeaponButtonAvailable =
		inVehicle && IsImmersiveDrivingActive();
	for(int hand = 0; hand < VR_HAND_COUNT; hand++){
		const bool useVehicleWeaponButton =
			vehicleWeaponButtonAvailable && gHeldSlot[hand] >= 0;
		const bool pressed = !blocked &&
			(useVehicleWeaponButton ||
			 (blockedHands & (1u << hand)) == 0) &&
			gPoseValid[hand] &&
			(useVehicleWeaponButton ? input.b : !inVehicle &&
			 (gTriggerPressed[hand] ? gTrigger[hand] >= 0.45f :
			  gTrigger[hand] >= 0.55f));
		gTriggerJustPressed[hand] =
			pressed && !gTriggerPressed[hand];
		gTriggerJustReleased[hand] =
			!pressed && gTriggerPressed[hand] && gPoseValid[hand];
		gTriggerPressed[hand] = pressed;
	}
}

// The holster update below runs only during gameplay with no menu up, and
// the calibration page is a menu. The one thing that still has to happen
// there is the free hand taking the foregrip: that socket is exactly what
// the page edits, and it cannot be judged against a hand beside it.
static void
UpdateWeaponCalibrationSupportPreview(void)
{
	for(int primary = 0; primary < VR_HAND_COUNT; primary++){
		const int support = 1-primary;
		const int slot = gHeldSlot[primary];
		const int weaponType =
			slot >= 0 ? GetVrWeaponTypeForSlot(slot) : -1;
		if(slot < 0 || gHeldSlot[support] >= 0 ||
		   !IsTwoHanded(weaponType) || !gPoseValid[support]){
			if(gSupportHand[primary] == support)
				gSupportHand[primary] = -1;
			continue;
		}
		gSupportHand[primary] = support;
	}
}

static void
UpdateHolsterInput(uint32 blockedHands)
{
	ExpireDroppedWeapons();
	if(UpdateAttachedMissionWeaponInput())
		return;

	// Keep/release established support grips first. Releasing the primary while
	// the support hand remains closed naturally hands the weapon over.
	for(int primary = 0; primary < VR_HAND_COUNT; primary++){
		const int support = gSupportHand[primary];
		if(support < 0)
			continue;
		if((blockedHands & (1u << primary)) != 0 ||
		   (blockedHands & (1u << support)) != 0 ||
		   gHeldSlot[primary] < 0 || !gPoseValid[support] ||
		   IsAuxiliaryHandReserved(support)){
			gSupportHand[primary] = -1;
			continue;
		}
		if(gGrip[support] <= 0.30f && !gWeaponCalibrationPreview){
			gSupportHand[primary] = -1;
			continue;
		}
		if(gGrip[primary] <= 0.30f && !gGripLock){
			const int slot = gHeldSlot[primary];
			gHeldSlot[primary] = -1;
			gSupportHand[primary] = -1;
			GiveWeaponToHand(support, slot, gHeldCopy[primary]);
		}
	}

	// A fresh grip near the saved foregrip becomes the support hand.
	for(int primary = 0; primary < VR_HAND_COUNT; primary++){
		const int support = 1-primary;
		const int slot = gHeldSlot[primary];
		const int weaponType =
			slot >= 0 ? GetVrWeaponTypeForSlot(slot) : -1;
		if((blockedHands & (1u << support)) != 0 ||
		   slot < 0 || gSupportHand[primary] >= 0 ||
		   gHeldSlot[support] >= 0 || IsSupportHand(support) ||
		   IsAuxiliaryHandReserved(support) ||
		   !IsTwoHanded(weaponType) || !gPoseValid[support])
			continue;
		if(gGrip[support] < 0.65f || gGripLatched[support])
			continue;
		CVector pivot, expected;
		if(BuildSupportVector(primary, weaponType, &pivot, &expected) &&
		   (gGripMatrix[support].GetPosition()-
		    (pivot+expected)).Magnitude() <= 0.18f){
			gSupportHand[primary] = support;
			gGripLatched[support] = true;
		}
	}

	// Explicit hand-to-hand transfer at the gun body.
	for(int receiver = 0; receiver < VR_HAND_COUNT; receiver++){
		const int source = 1-receiver;
		if((blockedHands & (1u << receiver)) != 0 ||
		   gHeldSlot[receiver] >= 0 || gHeldSlot[source] < 0 ||
		   IsSupportHand(receiver) || !gPoseValid[receiver] ||
		   IsAuxiliaryHandReserved(receiver) ||
		   gGrip[receiver] < 0.65f || gGripLatched[receiver])
			continue;
		const CVector gunPosition =
			gWeaponRenderSlot[source] == gHeldSlot[source] ?
			gWeaponRenderMatrix[source].GetPosition() :
			gGripMatrix[source].GetPosition();
		if((gGripMatrix[receiver].GetPosition()-gunPosition).Magnitude() >
		   0.20f)
			continue;
		const int slot = gHeldSlot[source];
		ClearSupportForHand(source);
		gHeldSlot[source] = -1;
		GiveWeaponToHand(receiver, slot, gHeldCopy[source]);
		gGripLatched[receiver] = true;
	}

	// A dropped gun can be caught by either free closed hand.
	for(int receiver = 0; receiver < VR_HAND_COUNT; receiver++){
		if((blockedHands & (1u << receiver)) != 0 ||
		   gHeldSlot[receiver] >= 0 || IsSupportHand(receiver) ||
		   IsAuxiliaryHandReserved(receiver) ||
		   !gPoseValid[receiver] || gGrip[receiver] < 0.55f)
			continue;
		for(int source = 0; source < VR_HAND_COUNT; source++){
			CMatrix dropped;
			if(!BuildDroppedMatrix(source, &dropped) ||
			   (gGripMatrix[receiver].GetPosition()-
			    dropped.GetPosition()).Magnitude() > 0.22f)
				continue;
			const int slot = gDropped[source].slot;
			const int copy = gDropped[source].copy;
			gDropped[source] = DroppedWeapon();
			GiveWeaponToHand(receiver, slot, copy);
			gWeaponRenderMatrix[receiver] = dropped;
			gWeaponRenderSlot[receiver] = slot;
			gGripLatched[receiver] = true;
			break;
		}
	}

	for(int hand = 0; hand < VR_HAND_COUNT; hand++){
		if((blockedHands & (1u << hand)) != 0){
			gGripLatched[hand] = gGrip[hand] >= 0.45f;
			continue;
		}
		if(IsAuxiliaryHandReserved(hand)){
			if(gGrip[hand] <= 0.30f)
				gGripLatched[hand] = false;
			continue;
		}
		if(IsSupportHand(hand)){
			if(gGrip[hand] <= 0.30f)
				gGripLatched[hand] = false;
			continue;
		}
		// Neutral input after focus/tracking loss is not a physical release.
		if(gPoseValid[hand] && gHeldSlot[hand] >= 0 && gGrip[hand] <= 0.30f){
			const int slot = gHeldSlot[hand];
			CMatrix holster;
			const bool returnToHolster = gPoseValid[hand] &&
				BuildHolsterMatrix(slot, &holster, gHeldCopy[hand]) &&
				(gGripMatrix[hand].GetPosition()-holster.GetPosition()).Magnitude()
					<= 0.24f;
			if(gGripLock && !returnToHolster)
				continue;
			ClearSupportForHand(hand);
			gHeldSlot[hand] = -1;
			gHolsterSelection[hand] = -1;
			if(!returnToHolster)
				StartDroppedWeapon(hand, slot);
		}
		if(gHeldSlot[hand] < 0 && gPoseValid[hand] &&
		   gGrip[hand] >= 0.65f && !gGripLatched[hand]){
			float closestDistance = 1000.0f;
			int closestSlot = -1;
			int closestCopy = hand;
			for(int slot = WEAPONSLOT_MELEE; slot < TOTAL_WEAPON_SLOTS; slot++){
				if((gWeaponHolsterMask & (1u << slot)) == 0) continue;
				for(int index = 0; index < (DualWieldSlot(slot) ? 2 : 1); index++){
					const int copy = DualWieldSlot(slot) ? index : 1;
					if(CopyInUse(slot, copy)) continue;
					CMatrix holster;
					if(!BuildHolsterMatrix(slot, &holster, copy)) continue;
					const float distance = (gGripMatrix[hand].GetPosition()-holster.GetPosition()).Magnitude();
					if(!IsHolsterWithinGrabDistance(FindHolsterPoint(slot, copy), distance)) continue;
					if(distance < closestDistance){
						closestDistance = distance;
						closestSlot = slot;
						closestCopy = copy;
					}
				}
			}
			if(closestSlot >= 0){
				GiveWeaponToHand(hand, closestSlot, closestCopy);
				gGripLatched[hand] = true;
			}
		}
		if(gGrip[hand] <= 0.30f)
			gGripLatched[hand] = false;
		else if(gGrip[hand] >= 0.65f)
			gGripLatched[hand] = true;
	}
}

static CVector
GetWeaponLocalMuzzle(int weaponType)
{
	switch(weaponType){
	case WEAPONTYPE_COLT45:
		return CVector(0.275f, 0.0004f, 0.0812f);
	case WEAPONTYPE_UZI:
		return CVector(0.241f, -0.0015f, 0.0817f);
	case WEAPONTYPE_SHOTGUN:
		return CVector(0.812f, -0.086f, 0.103f);
	case WEAPONTYPE_AK47:
		return CVector(0.638f, -0.047f, 0.083f);
	case WEAPONTYPE_M16:
		return CVector(0.741f, -0.066f, 0.137f);
	case WEAPONTYPE_SNIPERRIFLE:
		return CVector(0.770f, -0.0902f, 0.0902f);
	case WEAPONTYPE_ROCKETLAUNCHER:
		if(ModelSets::GetCalibrationSetForCategory(ModelSets::MODEL_CATEGORY_WEAPONS) !=
		   ModelSets::MODEL_SET_MODERN)
			// Centre of the front opening, not an approximate point beside it.
			return CVector(-0.070460401f, 0.114642583f, 0.783905983f);
		return CVector(0.553f, 0.000f, 0.000f);
	case WEAPONTYPE_FLAMETHROWER:
		return CVector(0.780f, -0.058f, 0.095f);
	default:
		return CVector(0.250f, 0.0f, 0.080f);
	}
}

// Full native strength requires both speed at contact and a deliberate stroke.
// A fast tracking twitch or a slow follow-through cannot deliver full damage.
static float
MeleeImpactScale(float speed, float travel, bool fist)
{
	const float minimumSpeed = fist ? 0.35f : 0.30f;
	const float fullSpeed = fist ? 3.0f : 4.0f;
	const float fullTravel = fist ? 0.30f : 0.55f;
	const float velocity = Min(1.0f, Max(0.0f,
		(speed-minimumSpeed)/(fullSpeed-minimumSpeed)));
	const float distance = Min(1.0f, Max(0.0f, travel/fullTravel));
	return velocity*velocity*distance;
}

static CVector
MeleeModelTip(int weaponType)
{
	switch(weaponType){
	case WEAPONTYPE_BASEBALLBAT: return CVector(0.068f, -0.019f, 0.680f);
	default: return IsPhysicalGun(weaponType) ?
		GetWeaponLocalMuzzle(weaponType) : CVector(0.0f, 0.0f, 0.11f);
	}
}

static CVector
MeleeModelRoot(int weaponType)
{
	switch(weaponType){
	case WEAPONTYPE_BASEBALLBAT: return CVector(0.066f, 0.035f, 0.0f);
	default: return CVector(0.0f, 0.0f, 0.0f);
	}
}

// Express a world point relative to the first-person camera frame. Gesture
// speeds must come from this space: Tommy's locomotion, snap turns and
// anchor refreshes move the world-space blade at tens of metres per second,
// which kept the swing detector permanently above its calm threshold. The
// desktop implementation measures in the equivalent camera-relative space.
static CVector
ToMeleeTrackingSpace(const CVector &worldPoint)
{
	float right[3], up[3], at[3], position[3];
	if(!rw::vulkan::getFirstPersonViewFrame(right, up, at, position))
		return worldPoint;
	const CVector r = VectorFromArray(right);
	const CVector u = VectorFromArray(up);
	const CVector f = VectorFromArray(at);
	const CVector relative = worldPoint-VectorFromArray(position);
	return CVector(DotProduct(relative, r), DotProduct(relative, f),
		DotProduct(relative, u));
}

static void
UpdateMelee()
{
	const uint32 frame = CTimer::GetFrameCounter();
	const uint32 now = CTimer::GetTimeInMillisecondsNonClipped();
	const float dt = Max(0.001f, CTimer::GetTimeStepNonClippedInSeconds());
	for(int hand = 0; hand < VR_HAND_COUNT; hand++){
		if(gMeleeStrike[hand].pending &&
		   frame-gMeleeStrike[hand].frame > 1U)
			gMeleeStrike[hand].pending = false;
		if(!IsGameplayAvailable() || FindPlayerVehicle() ||
		   !gPoseValid[hand] || IsSupportHand(hand)){
			gMeleeStrike[hand].pending = false;
			gMeleeMotion[hand] = MeleeMotion();
			continue;
		}
		int slot = gHeldSlot[hand];
		int weaponType = -1;
		bool enabled = false;
		CVector root, tip;
		if(slot >= 0){
			weaponType = GetVrWeaponTypeForSlot(slot);
			enabled = IsPhysicalMelee(weaponType) || IsPhysicalGun(weaponType);
			if(!enabled){
				gMeleeStrike[hand].pending = false;
				gMeleeMotion[hand] = MeleeMotion();
				continue;
			}
			// Build the calibrated visible weapon from this input sample. The
			// render cache belongs to the previous frame and camera anchor; using
			// it here both delays a hit and measures old world points against the
			// new camera. Use the same full reach for collision as for rendering.
			CMatrix model;
			WeaponCalibration *calibration = GetCalibration(hand, weaponType);
			if(!calibration || !BuildWeaponModelMatrix(hand, *calibration, &model)){
				gMeleeStrike[hand].pending = false;
				gMeleeMotion[hand] = MeleeMotion();
				continue;
			}
			root = model*MeleeModelRoot(weaponType);
			tip = model*MeleeModelTip(weaponType);
		}else{
			slot = WEAPONSLOT_UNARMED;
			weaponType = GetVrWeaponTypeForSlot(slot);
			enabled = IsPhysicalMelee(weaponType) &&
				gGrip[hand] >= 0.65f && gTrigger[hand] >= 0.45f;
			root = gGripMatrix[hand].GetPosition();
			tip = root+gAimMatrix[hand].GetForward()*0.11f;
		}
		if(!enabled){
			gMeleeStrike[hand].pending = false;
			gMeleeMotion[hand] = MeleeMotion();
			continue;
		}
		const CVector trackingRoot = ToMeleeTrackingSpace(root);
		const CVector trackingTip = ToMeleeTrackingSpace(tip);
		MeleeMotion &motion = gMeleeMotion[hand];
		if(!motion.valid || motion.slot != slot ||
		   motion.weaponType != weaponType ||
		   frame-motion.previousFrame > 2U || dt > 0.10f){
			gMeleeStrike[hand].pending = false;
			motion = MeleeMotion();
			motion.valid = true;
			// The first sample establishes a baseline and never hits. Permit a
			// deliberate swing on the next sample: requiring a perfectly calm
			// long blade before its first hit could leave it disarmed forever
			// when the player grabbed it directly into a series of swings.
			motion.armed = true;
			motion.slot = slot;
			motion.weaponType = weaponType;
			motion.previousRoot = root;
			motion.previousTip = tip;
			motion.previousTrackingRoot = trackingRoot;
			motion.previousTrackingTip = trackingTip;
			motion.previousFrame = frame;
			motion.calmSince = now;
			continue;
		}
		const float trackingTravel =
			(trackingTip-motion.previousTrackingTip).Magnitude();
		const float tipSpeed = trackingTravel/dt;
		const float rootSpeed =
			(trackingRoot-motion.previousTrackingRoot).Magnitude()/dt;
		const float threshold =
			slot == WEAPONSLOT_UNARMED ? 0.35f : 0.30f;
		const bool calm = slot == WEAPONSLOT_UNARMED ?
			tipSpeed <= 0.22f :
			tipSpeed <= 0.20f && rootSpeed <= 0.20f;
		if(calm){
			if(!motion.strikeInProgress) motion.swingTravel = 0.0f;
			if(motion.calmSince == 0)
				motion.calmSince = now;
			if(slot == WEAPONSLOT_UNARMED ||
			   now-motion.calmSince >= 70U)
				motion.armed = true;
		}else
			motion.calmSince = 0;
		if(!motion.armed && motion.lastStrikeTime &&
		   now-motion.lastStrikeTime >= 450U)
			motion.armed = true;
		// Large discontinuities are tracking loss or recenter events, never
		// superhuman attacks. Keep the new sample but require calm to rearm.
		if(trackingTravel > 0.65f){
			gMeleeStrike[hand].pending = false;
			motion.armed = false;
			motion.strikeInProgress = false;
			motion.swingTravel = 0.0f;
			motion.previousRoot = root;
			motion.previousTip = tip;
			motion.previousTrackingRoot = trackingRoot;
			motion.previousTrackingTip = trackingTip;
			motion.previousFrame = frame;
			continue;
		}
		const bool deliberate = slot == WEAPONSLOT_UNARMED ?
			rootSpeed >= 0.30f : rootSpeed >= 0.20f ||
			(trackingTip-trackingRoot-motion.previousTrackingTip+
			 motion.previousTrackingRoot).Magnitude()/dt >= 0.50f;
		if(motion.strikeInProgress && now > motion.strikeContinueUntil){
			motion.strikeInProgress = false;
			motion.swingTravel = 0.0f;
		}
		if(enabled && motion.armed && !calm)
			motion.swingTravel += trackingTravel;
		const bool fastStrikeSample = enabled && motion.armed &&
			tipSpeed >= threshold && deliberate &&
			now-motion.lastStrikeTime >= 220U;
		if(fastStrikeSample){
			motion.strikeInProgress = true;
			motion.strikeContinueUntil = now+180U;
		}
		// Once a genuine fast swing has started, keep publishing its slower
		// tail. The trigger frame's short sweep segment usually hasn't reached
		// the target yet; the weapon must keep dealing contact while it
		// travels. Without this a swing only landed when the player's own
		// movement carried the one-frame segment onto the target -- running
		// into someone worked, standing still and swinging a bat did not.
		// Contact ends the window immediately, so this cannot repeat-hit.
		const bool continuationSample =
			motion.strikeInProgress &&
			now <= motion.strikeContinueUntil && tipSpeed >= 0.08f;
		if(enabled && motion.armed &&
		   (fastStrikeSample || continuationSample) &&
		   !gMeleeStrike[hand].pending){
			MeleeStrike &strike = gMeleeStrike[hand];
			strike.pending = true;
			strike.slot = slot;
			strike.weaponType = weaponType;
			strike.tipStart = motion.previousTip;
			strike.tipEnd = tip;
			strike.rootStart = motion.previousRoot;
			strike.rootEnd = root;
			strike.speed = tipSpeed;
			strike.impactScale = MeleeImpactScale(tipSpeed, motion.swingTravel,
				slot == WEAPONSLOT_UNARMED);
			strike.frame = frame;
		}
		motion.previousRoot = root;
		motion.previousTip = tip;
		motion.previousTrackingRoot = trackingRoot;
		motion.previousTrackingTip = trackingTip;
		motion.previousFrame = frame;
	}
}

static CVector
GetWeaponLocalBarrelDirection(int weaponType)
{
	switch(weaponType){
	case WEAPONTYPE_COLT45:
		return CVector(0.9973f, -0.0734f, 0.0f);
	case WEAPONTYPE_UZI:
		return CVector(0.9974f, -0.0726f, 0.0f);
	case WEAPONTYPE_SHOTGUN:
		return CVector(0.9921f, -0.1255f, 0.0f);
	case WEAPONTYPE_AK47:
		return CVector(0.9934f, -0.1147f, 0.0f);
	case WEAPONTYPE_M16:
		return CVector(0.9925f, -0.1218f, 0.0068f);
	case WEAPONTYPE_SNIPERRIFLE:
		return CVector(0.9891f, -0.1477f, 0.0f);
	case WEAPONTYPE_ROCKETLAUNCHER:
		if(ModelSets::GetCalibrationSetForCategory(ModelSets::MODEL_CATEGORY_WEAPONS) !=
		   ModelSets::MODEL_SET_MODERN)
			// Direction between the III mesh's rear and front tube centres.
			return CVector(-0.295286557f, 0.132528077f, 0.946172372f);
		return CVector(1.0f, 0.0f, 0.0f);
	case WEAPONTYPE_FLAMETHROWER:
		return CVector(1.0f, 0.0f, 0.0f);
	default:
		return CVector(1.0f, 0.0f, 0.0f);
	}
}

static bool
BuildWeaponMuzzleRay(const CMatrix &weaponMatrix, int weaponType,
	CVector *source, CVector *direction)
{
	const CVector localDir = GetWeaponLocalBarrelDirection(weaponType);
	*direction = weaponMatrix.GetRight() * localDir.x +
		weaponMatrix.GetForward() * localDir.y +
		weaponMatrix.GetUp() * localDir.z;
	if(direction->MagnitudeSqr() <= 0.0001f)
		return false;
	direction->Normalise();

	const CVector localMuzzle = GetWeaponLocalMuzzle(weaponType);
	*source = weaponMatrix.GetPosition() +
		weaponMatrix.GetRight()*localMuzzle.x +
		weaponMatrix.GetForward()*localMuzzle.y +
		weaponMatrix.GetUp()*localMuzzle.z;
	return true;
}

static bool
BuildAim(int hand, int weaponType, CVector *source, CVector *direction)
{
	if(!source || !direction || hand < 0 || hand >= VR_HAND_COUNT ||
	   !gPoseValid[hand] || gHeldSlot[hand] < 0)
		return false;

	if(IsPhysicalThrowable(weaponType)){
		*direction = gAimMatrix[hand].GetForward();
		if(direction->MagnitudeSqr() <= 0.0001f)
			return false;
		direction->Normalise();
		*source = gAimMatrix[hand].GetPosition() + (*direction)*0.15f;
		return true;
	}

	CMatrix weaponMatrix;
	bool haveMatrix = false;
	const uint32 frame = CTimer::GetFrameCounter();
	if(gWeaponContactFrame[hand] >= frame - 1U &&
	   gWeaponContactSlot[hand] == gHeldSlot[hand] &&
	   gWeaponContactType[hand] == weaponType){
		weaponMatrix = gWeaponContactMatrix[hand];
		haveMatrix = true;
	}else{
		WeaponCalibration *calibration = GetCalibration(hand, weaponType);
		if(calibration && BuildWeaponModelMatrix(hand, *calibration, &weaponMatrix)){
			if(!VrBodyIK::WeaponOwnershipActive()){
				CVector pivot, axis;
				float angle;
				if(BuildTwoHandRotation(hand, weaponType, &pivot, &axis, &angle)){
					weaponMatrix.GetRight() = RotateAroundAxis(weaponMatrix.GetRight(), axis, angle);
					weaponMatrix.GetUp() = RotateAroundAxis(weaponMatrix.GetUp(), axis, angle);
					weaponMatrix.GetForward() = RotateAroundAxis(weaponMatrix.GetForward(), axis, angle);
					weaponMatrix.GetPosition() = pivot + RotateAroundAxis(weaponMatrix.GetPosition() - pivot, axis, angle);
				}
			}
			haveMatrix = true;
		}
	}

	if(!haveMatrix)
		return false;

	return BuildWeaponMuzzleRay(weaponMatrix, weaponType, source, direction);
}

static bool
BuildScopeCentreTarget(CVector *target)
{
	if(!target || !IsGameplayAvailable() || FindPlayerVehicle())
		return false;
	float viewRight[3], viewUp[3], viewAt[3], viewPosition[3];
	if(!rw::vulkan::getFirstPersonViewFrame(
	   viewRight, viewUp, viewAt, viewPosition))
		return false;

	const CVector origin = VectorFromArray(viewPosition);
	CVector forward = VectorFromArray(viewAt);
	if(forward.MagnitudeSqr() < 0.0001f)
		return false;
	forward.Normalise();

	// The scope/camera overlay is centred on the HMD view, so its centre ray is
	// authoritative while the optic is active. Converge the tracked muzzle on
	// the first world point under that ray, matching the desktop implementation.
	*target = origin+forward*150.0f;
	CColPoint point;
	CEntity *hitEntity = nil;
	CEntity *savedIgnoreEntity = CWorld::pIgnoreEntity;
	CWorld::pIgnoreEntity = FindPlayerPed();
	if(CWorld::ProcessLineOfSight(origin, *target, point, hitEntity,
	   true, true, true, true, true, false, false))
		*target = point.point;
	CWorld::pIgnoreEntity = savedIgnoreEntity;
	return true;
}

static void
UpdateScope()
{
	if(!IsGameplayAvailable() || FindPlayerVehicle()){
		gScopeHand = -1;
		gScopeWeaponType = -1;
		gScopeCandidateHand = -1;
		gScopeCandidateType = -1;
		gScopeCandidateSince = gScopeReleaseSince = 0;
		return;
	}
	float viewRight[3], viewUp[3], viewAt[3], viewPosition[3];
	if(!rw::vulkan::getFirstPersonViewFrame(
	   viewRight, viewUp, viewAt, viewPosition)){
		gScopeHand = -1;
		gScopeWeaponType = -1;
		return;
	}
	const CVector head = VectorFromArray(viewPosition);
	CVector headForward = VectorFromArray(viewAt);
	CVector headUp = VectorFromArray(viewUp);
	CVector headRight = VectorFromArray(viewRight);
	headForward.Normalise();
	headUp.Normalise();
	headRight.Normalise();
	const uint32 now = CTimer::GetTimeInMillisecondsNonClipped();
	int candidateHand = -1;
	int candidateType = -1;
	float candidateScore = 1000.0f;
	for(int hand = 0; hand < VR_HAND_COUNT; hand++){
		const int slot = gHeldSlot[hand];
		const int weaponType =
			slot >= 0 ? GetVrWeaponTypeForSlot(slot) : -1;
		if(!IsScopeWeapon(weaponType) || !gPoseValid[hand] || gScopeAimMode == SCOPE_AIM_OFF)
			continue;
		if(IsTwoHanded(weaponType) && gSupportHand[hand] < 0)
			continue;
		const CVector relative =
			gAimMatrix[hand].GetPosition()-head;
		const float distance = relative.Magnitude();
		const float forward = DotProduct(relative, headForward);
		const float height = DotProduct(relative, headUp);
		const float lateral = DotProduct(relative, headRight);
		CVector aimSource, aim;
		if(!BuildAim(hand, weaponType, &aimSource, &aim))
			continue;
		aim.Normalise();
		const float alignment = DotProduct(headForward, aim);
		if(distance < 0.04f || distance > 0.65f ||
		   forward < -0.03f || forward > 0.58f ||
		   height < -0.30f || height > 0.14f ||
		   Abs(lateral) > 0.30f || alignment < 0.70f)
			continue;
		const float score = distance+Abs(lateral)*1.5f+
			(1.0f-alignment)*0.8f;
		if(score < candidateScore){
			candidateScore = score;
			candidateHand = hand;
			candidateType = weaponType;
		}
	}
	if(candidateHand >= 0){
		if(gScopeHand == candidateHand &&
		   gScopeWeaponType == candidateType){
			gScopeReleaseSince = 0;
			gScopeCandidateHand = candidateHand;
			gScopeCandidateType = candidateType;
			gScopeCandidateSince = 0;
			return;
		}
		if(gScopeCandidateHand != candidateHand ||
		   gScopeCandidateType != candidateType){
			gScopeCandidateHand = candidateHand;
			gScopeCandidateType = candidateType;
			gScopeCandidateSince = now;
		}
		if(gScopeCandidateSince == 0)
			gScopeCandidateSince = now;
		if(now-gScopeCandidateSince >= 120U){
			gScopeHand = candidateHand;
			gScopeWeaponType = candidateType;
			gScopeReleaseSince = 0;
		}
	}else{
		gScopeCandidateHand = -1;
		gScopeCandidateType = -1;
		gScopeCandidateSince = 0;
		if(gScopeHand >= 0){
			if(gScopeReleaseSince == 0)
				gScopeReleaseSince = now;
			if(now-gScopeReleaseSince >= 180U){
				gScopeHand = -1;
				gScopeWeaponType = -1;
				gScopeReleaseSince = 0;
			}
		}
	}
}

static void
UpdatePhysicalScopeZoom(CControllerState *state, const androidgame::PadInput &input,
    bool blocked)
{
	if(blocked || gScopeHand < 0 || gScopeAimMode != SCOPE_AIM_3D_PHYSICAL ||
	   gScopeWeaponType != WEAPONTYPE_SNIPERRIFLE)
		return;
	gPhysicalScopeZoom = QuestScopeZoom::Step(gPhysicalScopeZoom, input.rightStickY,
		CTimer::GetTimeStepNonClippedInSeconds());
	// The vertical axis belongs to the optic while aiming, even at a zoom
	// limit. It must not also move the legacy camera or native sniper zoom.
	state->RightStickY = 0;
}

static void
ResetInteraction()
{
	ClearAttachedMissionWeaponState();
	for(int hand = 0; hand < VR_HAND_COUNT; hand++){
		ClearSupportForHand(hand);
		gHeldSlot[hand] = -1;
		gHolsterSelection[hand] = -1;
		gWeaponRenderSlot[hand] = -1;
		gWeaponContactSlot[hand] = -1;
		gDropped[hand] = DroppedWeapon();
		gMeleeMotion[hand] = MeleeMotion();
		gMeleeStrike[hand] = MeleeStrike();
		gThrowablePreview[hand] = false;
		ResetManualReloadState(gManualReloadState[hand]);
		gManualReloadGripDown[hand] = gGrip[hand] >= 0.45f;
		gTrackedDetonatorWasActive[hand] = false;
		gTrackedDetonatorWaitForRelease[hand] = false;
		gTrackedDetonatorJustPressed[hand] = false;
	}
	gScopeHand = -1;
	gScopeWeaponType = -1;
	gScopeCandidateHand = -1;
	gScopeCandidateType = -1;
	gScopeCandidateSince = gScopeReleaseSince = 0;
}

static void
ResetKnockdownInteraction()
{
	// Return both hands to inventory holsters, not catchable thrown props.
	// Inventory/ammunition and loadout assignments are intentionally untouched.
	ResetInteraction();
	for(int hand = 0; hand < VR_HAND_COUNT; hand++){
		// Holding grip through the fall must not count as a fresh holster grab.
		gGripLatched[hand] = gGrip[hand] >= 0.45f;
		gPreviousPoseTime[hand] = 0;
		gHandVelocity[hand] = CVector(0,0,0);
		gTriggerPressed[hand] = false;
		gTriggerJustPressed[hand] = false;
		gTriggerJustReleased[hand] = false;
	}
	gActiveFire = false;
	gActiveFireHand = -1;
	gActiveFireWeaponType = -1;
	gActiveThrowable = false;
	gBodyForwardValid = false;
}

} // namespace

bool
ApplyTouchInput(CControllerState *state)
{
	if(!state)
		return false;
	LoadSettings();
	const androidgame::PadInput &input = androidgame::GetPadInput();
	UpdatePoses(input);
	const bool menu = androidgame::VrMenuConsumesInput();
	const bool gameplay = IsGameplayAvailable();
	// Passenger hands stay free even if a weapon was held when boarding.
	if(IsQuestTrainPassenger())
		ResetInteraction();
	const bool knockedDown = androidgame::VrKnockdownPresentationActive();
	if(knockedDown)
		ResetKnockdownInteraction();
	const bool shortcutModifier =
		input.leftGrip >= 0.75f && input.rightGrip >= 0.75f &&
		gHeldSlot[0] < 0 && gHeldSlot[1] < 0 &&
		!IsTrackedDetonatorHandReserved(0) &&
		!IsTrackedDetonatorHandReserved(1);
	const bool weaponStickHorizontal =
		fabsf(input.rightStickX) >= 0.65f &&
		fabsf(input.rightStickX) > fabsf(input.rightStickY);
	// Desktop fallback for players who disable physical VR hands: both grips
	// turn the right stick into one-shot previous/next weapon input instead of
	// camera rotation. CPad owns edge detection through its normal old/new
	// Shoulder2 states, so holding the stick cannot race through every slot.
	const bool weaponCycleLeft = gameplay && !menu && !gHandsEnabled &&
		!FindPlayerVehicle() && shortcutModifier &&
		weaponStickHorizontal && input.rightStickX < 0.0f;
	const bool weaponCycleRight = gameplay && !menu && !gHandsEnabled &&
		!FindPlayerVehicle() && shortcutModifier &&
		weaponStickHorizontal && input.rightStickX > 0.0f;
	const uint32 vehicleCapturedHands =
		UpdateQuestDrivingInput(state, menu || !gameplay);
	const bool inVehicle = FindPlayerVehicle() != nil;
	if(!inVehicle)
		UpdateWeaponTriggerEdges(menu || !gameplay, vehicleCapturedHands, input);
	UpdateTrackedDetonatorInput(menu || !gameplay || IsQuestTrainPassenger() || QuestRadio::Interacting());
	// Quest has no in-vehicle magazine insertion path. Entering any vehicle
	// makes ManualReloadAvailable false, which also clears an in-progress
	// reload instead of leaving a magazine hand reserved behind the wheel.
	UpdateManualReloadInput(menu || !gameplay || FindPlayerVehicle());
	if(gameplay && !menu && !IsQuestTrainPassenger() &&
	   (!FindPlayerVehicle() || IsImmersiveDrivingActive()))
		UpdateHolsterInput(vehicleCapturedHands);
	else if(gWeaponCalibrationPreview && gameplay)
		UpdateWeaponCalibrationSupportPreview();
	else if(!gameplay && !knockedDown)
		ResetInteraction();
	// A draw or holster can change which firing path owns B during this frame.
	if(inVehicle)
		UpdateWeaponTriggerEdges(menu || !gameplay || QuestRadio::Interacting(), vehicleCapturedHands, input);
	ApplyQuestVehicleButtonInput(state, menu || !gameplay);
	const bool vehicleWeaponButtonConsumed =
		gameplay && !menu && inVehicle && IsImmersiveDrivingActive() &&
		(gHeldSlot[0] >= 0 || gHeldSlot[1] >= 0);
	UpdateMelee();
	UpdateScope();
	UpdatePhysicalScopeZoom(state, input, !gameplay || menu || knockedDown);

	// CapturePad has to map triggers to classic acceleration/brake for vehicles.
	// On foot those same states must not become a second animation-driven shot:
	// retain the face buttons, but remove only the trigger contribution.
	// In Immersive/Motion driving B is consumed by the physically aimed weapon;
	// do not also pass it into the legacy vehicle action.
	if(vehicleWeaponButtonConsumed)
		state->Circle = 0;
	if(androidgame::VrKnockdownPresentationActive() && !menu){
		// Native attack/weapon cycling must not leak through while the
		// tracked path is suspended, especially during the recovery blend.
		androidgame::PadInput mappedInput = input;
		androidgame::VrApplyPadBindings(state, mappedInput, false);
		const int mode = CPad::GetPad(0)->GetMode();
		if(mode == 0 || mode == 1) state->Circle = 0;
		if(mode == 2) state->Cross = 0;
		if(mode == 3) state->RightShoulder1 = 0;
		state->LeftShoulder2 = state->RightShoulder2 = 0;
	}
	if(gameplay && !menu && !FindPlayerVehicle()){
		const int padMode = CPad::GetPad(0)->GetMode();
		// Rebuild the mapped buttons from the CONTROLS assignment without the
		// triggers. The attack button of the current pad mode then belongs to
		// the physically aimed weapon, whichever input reaches it.
		androidgame::PadInput mappedInput = input;
		androidgame::VrApplyPadBindings(state, mappedInput, false);
		if(padMode == 2)
			state->Cross = 0;
		if(padMode == 0 || padMode == 1)
			state->Circle = 0;
		if(gHeldSlot[0] >= 0 || IsSupportHand(0) ||
		   IsAuxiliaryHandReserved(0))
			state->LeftShoulder1 = 0;
		if(gHeldSlot[1] >= 0 || IsSupportHand(1) ||
		   IsAuxiliaryHandReserved(1) || padMode == 3)
			state->RightShoulder1 = 0;
		if(gAttachedMissionWeaponForced){
			// Both grips belong to the mounted gun and its optional support
			// grip. Never leak either into Vice City's L1/radio bindings.
			state->LeftShoulder1 = 0;
			state->RightShoulder1 = 0;
		}
	}
	if(weaponCycleLeft || weaponCycleRight){
		state->RightStickX = 0;
		state->LeftShoulder1 = 0;
		state->RightShoulder1 = 0;
		if(weaponCycleLeft)
			state->LeftShoulder2 = Max(
				state->LeftShoulder2, (int16)255);
		if(weaponCycleRight)
			state->RightShoulder2 = Max(
				state->RightShoulder2, (int16)255);
	}
	return true;
}

bool AreTrackedHandsEnabled() { LoadSettings(); return gHandsEnabled && gVrFirstPersonActive; }
bool ShouldUseTrackedHands()
{
	return AreTrackedHandsEnabled() &&
		(IsQuestTrainPassenger() || !FindPlayerVehicle() || IsImmersiveDrivingActive());
}
bool IsTrackedHandReady(int hand)
{
	return hand >= 0 && hand < VR_HAND_COUNT &&
		gPoseValid[hand] && ShouldUseTrackedHands();
}
// The calibration page has to show what is being calibrated, whatever the
// player has done with the highlights during ordinary play.
void
SetQuestWeaponCalibrationPreview(bool visible)
{
	gWeaponCalibrationPreview = visible;
}

bool AreWeaponHolsterHighlightsEnabled()
{
	LoadSettings();
	return gHolsterHighlights || gWeaponCalibrationPreview;
}
bool IsTrackedWeaponLaserEnabled() { LoadSettings(); return gWeaponLaser; }

void
InvalidateQuestWeaponLaserOverrides()
{
	memset(gWeaponLaserOverrides, 0, sizeof(gWeaponLaserOverrides));
}

// The Weapons page is the master switch. Per-weapon choices can suppress a
// laser while it is on, but cannot leave a laser visible after it is disabled.
bool
IsTrackedWeaponLaserEnabledForType(int weaponType)
{
	LoadSettings();
	const int stored = ReadWeaponLaserOverride(weaponType);
	return gWeaponLaser && stored != 0;
}

int
GetQuestWeaponLaserOverride(int weaponType)
{
	LoadSettings();
	return ReadWeaponLaserOverride(weaponType);
}

void
CycleQuestWeaponLaserOverride(int weaponType, int direction)
{
	LoadSettings();
	// Three states in a ring: follow the master switch, off, on when enabled.
	int stored = ReadWeaponLaserOverride(weaponType)+1;
	stored = (stored+3+(direction < 0 ? -1 : 1)) % 3;
	WriteWeaponLaserOverride(weaponType, stored-1);
}
bool IsTrackedScopeActive() { return gScopeHand >= 0 && gScopeAimMode != SCOPE_AIM_OFF; }
bool IsTrackedScopeActiveForHand(int hand) { return gScopeHand == hand && gScopeAimMode != SCOPE_AIM_OFF; }
bool IsTrackedScopeOverlayActive() { return IsTrackedScopeActive() && gScopeAimMode == SCOPE_AIM_2D_OVERLAY; }
bool IsTrackedScopeOverlayActiveForHand(int hand) { return IsTrackedScopeActiveForHand(hand) && gScopeAimMode == SCOPE_AIM_2D_OVERLAY; }
float GetTrackedPhysicalScopeZoom() { return gPhysicalScopeZoom; }
int GetTrackedScopeWeaponType() { return gScopeWeaponType; }

int GetQuestScopeAimMode() { LoadSettings(); return gScopeAimMode; }

const char *
GetQuestScopeAimModeName()
{
	LoadSettings();
	switch(gScopeAimMode){
	case SCOPE_AIM_2D_OVERLAY: return "2D OVERLAY";
	case SCOPE_AIM_3D_PHYSICAL: return "3D PHYSICAL";
	case SCOPE_AIM_OFF: return "OFF";
	default: return "UNKNOWN";
	}
}

void
CycleQuestScopeAimMode(int direction)
{
	LoadSettings();
	gScopeAimMode = (gScopeAimMode + (direction < 0 ? (SCOPE_AIM_MODE_COUNT - 1) : 1)) % SCOPE_AIM_MODE_COUNT;
	char buf[16];
	snprintf(buf, sizeof(buf), "%d", gScopeAimMode);
	WritePrivateProfileStringA("VR", "PhysicalScopeAim", buf, kSettingsPath);
	if(gScopeAimMode == SCOPE_AIM_OFF){
		gScopeHand = -1;
		gScopeWeaponType = -1;
	}
}

bool
GetTrackedScopeReticlePos(float *screenX, float *screenY)
{
	if(!screenX || !screenY)
		return false;
	*screenX = (float)RsGlobal.width * 0.5f;
	*screenY = (float)RsGlobal.height * 0.5f;
	if(gScopeHand < 0 || gScopeWeaponType < 0)
		return false;
	CVector source, direction;
	if(!BuildAim(gScopeHand, gScopeWeaponType, &source, &direction))
		return false;
	direction.Normalise();
	const CVector targetPoint = source + direction * 100.0f;
	RwV3d targetRw = { targetPoint.x, targetPoint.y, targetPoint.z };
	RwV3d screenPos;
	float szx, szy;
	if(CSprite::CalcScreenCoors(targetRw, &screenPos, &szx, &szy, false) && screenPos.z > 0.0f){
		const float maxOffsetX = (float)RsGlobal.width * 0.28f;
		const float maxOffsetY = (float)RsGlobal.height * 0.28f;
		const float midX = (float)RsGlobal.width * 0.5f;
		const float midY = (float)RsGlobal.height * 0.5f;
		*screenX = Clamp(screenPos.x, midX - maxOffsetX, midX + maxOffsetX);
		*screenY = Clamp(screenPos.y, midY - maxOffsetY, midY + maxOffsetY);
		return true;
	}
	return false;
}

bool
ShouldUseTrackedWeapon(int hand)
{
	return !IsQuestTrainPassenger() && hand >= 0 && hand < VR_HAND_COUNT &&
		gHeldSlot[hand] >= 0 && gPoseValid[hand] &&
		IsGameplayAvailable();
}

bool IsTrackedWeaponTriggerPressed(int hand)
{
	return hand >= 0 && hand < VR_HAND_COUNT &&
		gTriggerPressed[hand] &&
		(!FindPlayerVehicle() || IsImmersiveDrivingActive());
}
bool IsTrackedWeaponTriggerJustPressed(int hand)
{
	return IsTrackedWeaponTriggerPressed(hand) &&
		gTriggerJustPressed[hand];
}

static int
GetTrackedWeaponFireTriggerHand(int hand, int weaponType)
{
	if(hand < 0 || hand >= VR_HAND_COUNT || gHeldSlot[hand] < 0 ||
	   !gPoseValid[hand] || !IsGameplayAvailable())
		return -1;
	if(weaponType != WEAPONTYPE_ROCKETLAUNCHER)
		return hand;
	const int opposite = 1-hand;
	// Rockets require a live two-hand grip, including when grip-lock keeps
	// the launcher in the primary hand or the player is in a vehicle.
	if(gSupportHand[hand] != opposite || !gPoseValid[opposite] ||
	   gGrip[hand] <= 0.30f || gGrip[opposite] <= 0.30f)
		return -1;
	if(FindPlayerVehicle())
		return hand;
	// The RPG model is shouldered from the support side: right-held uses L2,
	// left-held uses R2. Never steal a trigger from another gun/detonator.
	if(gHeldSlot[opposite] >= 0 || IsTrackedDetonatorActive(opposite))
		return -1;
	return opposite;
}

bool IsTrackedWeaponFireTriggerPressed(int hand, int weaponType)
{
	const int triggerHand = GetTrackedWeaponFireTriggerHand(hand, weaponType);
	return triggerHand >= 0 && gTriggerPressed[triggerHand];
}

bool IsTrackedWeaponFireTriggerJustPressed(int hand, int weaponType)
{
	const int triggerHand = GetTrackedWeaponFireTriggerHand(hand, weaponType);
	return triggerHand >= 0 && gTriggerPressed[triggerHand] &&
		gTriggerJustPressed[triggerHand];
}
bool IsTrackedWeaponTriggerJustReleased(int hand)
{
	return hand >= 0 && hand < VR_HAND_COUNT &&
		gTriggerJustReleased[hand] &&
		(!FindPlayerVehicle() || IsImmersiveDrivingActive());
}
bool IsTrackedDetonatorActive(int hand) { return !IsQuestTrainPassenger() && IsTrackedDetonatorHandReserved(hand) && gPoseValid[hand]; }
bool IsTrackedDetonatorTriggerJustPressed(int hand)
{
	return hand >= 0 && hand < VR_HAND_COUNT &&
		HasTrackedRemoteCharges() && IsTrackedDetonatorActive(hand) &&
		gTrackedDetonatorJustPressed[hand];
}
bool IsTrackedRemoteGrenadeFireActive()
{
	return false;
}
bool ShouldKeepTrackedRemoteCharges(CEntity *source)
{
	return false;
}
void NotifyTrackedRemoteGrenadeThrown(int hand)
{
	if(hand < 0 || hand >= VR_HAND_COUNT)
		return;
	gTrackedDetonatorHand = 1-hand;
	gTrackedDetonatorWasActive[gTrackedDetonatorHand] = false;
	gTrackedDetonatorWaitForRelease[gTrackedDetonatorHand] =
		gTrigger[gTrackedDetonatorHand] > 0.30f;
}
void NotifyTrackedDetonatorActivated(int hand)
{
	if(hand < 0 || hand >= VR_HAND_COUNT)
		return;
	gTrackedDetonatorJustPressed[hand] = false;
	gTrackedDetonatorWaitForRelease[hand] = true;
}

bool
IsManualReloadWeaponType(int weaponType)
{
	return weaponType == WEAPONTYPE_COLT45 ||
		weaponType == WEAPONTYPE_UZI;
}

void
SetManualReloadWeaponState(int weaponHand, int slot, int weaponType,
	bool available)
{
	if(weaponHand < 0 || weaponHand >= VR_HAND_COUNT)
		return;
	ManualReloadState &reload = gManualReloadState[weaponHand];
	available = available && ManualReloadAvailable() &&
		IsManualReloadWeaponType(weaponType) &&
		slot > WEAPONSLOT_UNARMED && slot < TOTAL_WEAPON_SLOTS &&
		gHeldSlot[weaponHand] == slot;
	if(!available){
		ResetManualReloadState(reload);
		return;
	}
	if(gSupportHand[weaponHand] >= 0)
		gSupportHand[weaponHand] = -1;
	if(!reload.active || reload.slot != slot ||
	   reload.weaponType != weaponType){
		ResetManualReloadState(reload);
		reload.active = true;
		reload.weaponHand = weaponHand;
		reload.slot = slot;
		reload.weaponType = weaponType;
	}
}
bool ShouldUseManualReload() { return ManualReloadAvailable(); }
bool
ConsumeManualReloadRequest(int weaponHand, int slot, int weaponType)
{
	if(weaponHand < 0 || weaponHand >= VR_HAND_COUNT)
		return false;
	ManualReloadState &reload = gManualReloadState[weaponHand];
	const bool requested = reload.active && reload.requested &&
		reload.weaponHand == weaponHand && reload.slot == slot &&
		reload.weaponType == weaponType &&
		gHeldSlot[weaponHand] == slot;
	if(requested)
		ResetManualReloadState(reload);
	return requested;
}
bool
GetManualReloadMagazineMatrix(int weaponHand, CMatrix *matrix,
	int *weaponType, bool *held)
{
	if(!matrix || weaponHand < 0 || weaponHand >= VR_HAND_COUNT)
		return false;
	ManualReloadState &reload = gManualReloadState[weaponHand];
	if(!reload.active || reload.requested || !ManualReloadAvailable())
		return false;
	const bool magazineHeld = reload.magazineHand >= 0;
	const bool ready = magazineHeld ?
		BuildManualReloadHeldMatrix(reload.magazineHand, matrix) :
		BuildManualReloadSpotMatrix(reload.slot, matrix, weaponHand);
	if(!ready)
		return false;
	if(weaponType)
		*weaponType = reload.weaponType;
	if(held)
		*held = magazineHeld;
	return true;
}

void
SetWeaponHolsterMask(uint32 mask)
{
	gWeaponHolsterMask = mask;
	for(int hand = 0; hand < VR_HAND_COUNT; hand++){
		if(gHeldSlot[hand] >= 0 &&
		   (mask & (1u << gHeldSlot[hand])) == 0){
			ClearSupportForHand(hand);
			gHeldSlot[hand] = -1;
			gWeaponRenderSlot[hand] = -1;
		}
		if(gDropped[hand].slot >= 0 &&
		   (mask & (1u << gDropped[hand].slot)) == 0)
			gDropped[hand] = DroppedWeapon();
	}
}

bool
ConsumeWeaponHolsterSelection(int hand, int *slot)
{
	if(!slot || hand < 0 || hand >= VR_HAND_COUNT)
		return false;
	*slot = gHolsterSelection[hand];
	gHolsterSelection[hand] = -1;
	return *slot >= 0;
}

bool
GetWeaponHolsterMatrix(int slot, CMatrix *matrix, int copy)
{
	if(IsQuestTrainPassenger() || !AreTrackedHandsEnabled() ||
	   (FindPlayerVehicle() && !IsImmersiveDrivingActive()) ||
	   (gWeaponHolsterMask & (1u << slot)) == 0)
		return false;
	if(copy < 0) copy = 1;
	if(CopyInUse(slot, copy)) return false;
	return BuildHolsterMatrix(slot, matrix, copy);
}

bool IsPhysicalGunType(int weaponType) { return IsPhysicalGun(weaponType); }
bool IsPhysicalMeleeType(int weaponType) { return IsPhysicalMelee(weaponType); }
bool IsPhysicalThrowableType(int weaponType) { return IsPhysicalThrowable(weaponType); }
bool IsPhysicalWeaponType(int weaponType) { return IsPhysicalGun(weaponType) || IsPhysicalMelee(weaponType) || IsPhysicalThrowable(weaponType) || weaponType == WEAPONTYPE_DETONATOR; }
bool IsPhysicalWeaponInteractionActive() { return AreTrackedHandsEnabled() && IsGameplayAvailable() && !FindPlayerVehicle(); }
bool IsQuestFirstPersonOnFoot() { return gVrFirstPersonActive && !FindPlayerVehicle(); }
bool IsTrackedWeaponHeld(int hand)
{
	return hand >= 0 && hand < VR_HAND_COUNT &&
		gHeldSlot[hand] >= 0 &&
		(!FindPlayerVehicle() || IsImmersiveDrivingActive());
}
int GetHeldWeaponCopy(int hand) { return hand >= 0 && hand < VR_HAND_COUNT ? gHeldCopy[hand] : 1; }
int GetHeldWeaponSlot(int hand) { return hand >= 0 && hand < VR_HAND_COUNT ? gHeldSlot[hand] : -1; }

void
ApplyBakedLeftWeaponPositionToMatrix(int hand, CMatrix *matrix)
{
	LoadSettings();
	ApplyBakedLeftWeaponPosition(hand, matrix);
}

bool IsRunWithoutLimitsEnabled()
{
	LoadSettings();
	return gRunWithoutLimits;
}

void SetRunWithoutLimitsEnabled(bool enabled)
{
	LoadSettings();
	gRunWithoutLimits = enabled;
	WritePrivateProfileStringA("VR", "RunWithoutLimits",
		enabled ? "1" : "0", kSettingsPath);
}

bool
IsQuestDrivingHandUnavailable(int hand)
{
	return hand < 0 || hand >= VR_HAND_COUNT ||
		gHeldSlot[hand] >= 0 || IsSupportHand(hand) ||
		IsAuxiliaryHandReserved(hand);
}

void
RestrictQuestVehicleWeaponsToSidearms()
{
	CPlayerPed *player = FindPlayerPed();
	if(!player)
		return;
	for(int hand = 0; hand < VR_HAND_COUNT; hand++){
		const int slot = gHeldSlot[hand];
		const int weaponType =
			slot >= 0 ? GetVrWeaponTypeForSlot(slot) : -1;
		if(slot >= 0 && !IsImmersiveVehicleSidearm(weaponType)){
			ClearSupportForHand(hand);
			gHeldSlot[hand] = -1;
			gHolsterSelection[hand] = -1;
			gWeaponRenderSlot[hand] = -1;
			gWeaponContactSlot[hand] = -1;
			gDropped[hand] = DroppedWeapon();
		}
	}
	uint32 mask = 0;
	for(int slot = WEAPONSLOT_HANDGUN;
	    slot <= WEAPONSLOT_SUBMACHINEGUN; slot++){
		if(player->HasWeaponSlot(slot) &&
		   IsImmersiveVehicleSidearm(GetVrWeaponTypeForSlot(slot)))
			mask |= 1u << slot;
	}
	for(int hand = 0; hand < VR_HAND_COUNT; hand++){
		if(gHeldSlot[hand] >= 0 && !DualWieldSlot(gHeldSlot[hand]))
			mask &= ~(1u << gHeldSlot[hand]);
		if(gDropped[hand].slot >= 0 && !DualWieldSlot(gDropped[hand].slot))
			mask &= ~(1u << gDropped[hand].slot);
	}
	gWeaponHolsterMask = mask;
}

void
ReleaseTrackedWeaponAfterUse(int hand, int slot)
{
	if(hand < 0 || hand >= VR_HAND_COUNT || gHeldSlot[hand] != slot)
		return;
	ClearSupportForHand(hand);
	gHeldSlot[hand] = -1;
	gHolsterSelection[hand] = -1;
	gWeaponRenderSlot[hand] = -1;
	gWeaponContactSlot[hand] = -1;
	gGripLatched[hand] = true;
}

void
SetTrackedWeaponRenderMatrix(int hand, int slot, int weaponType,
	const CMatrix *matrix, const CMatrix *contactMatrix)
{
	if(hand < 0 || hand >= VR_HAND_COUNT || !matrix)
		return;
	gWeaponRenderMatrix[hand] = *matrix;
	gWeaponRenderSlot[hand] = slot;
	if(contactMatrix){
		gWeaponContactMatrix[hand] = *contactMatrix;
		gWeaponContactSlot[hand] = slot;
		gWeaponContactType[hand] = weaponType;
		gWeaponContactFrame[hand] = CTimer::GetFrameCounter();
	}
}

int GetDroppedWeaponSlot(int hand) { return hand >= 0 && hand < VR_HAND_COUNT ? gDropped[hand].slot : -1; }

bool
GetDroppedWeaponMatrix(int slot, CMatrix *matrix, int copy)
{
	for(int hand = 0; hand < VR_HAND_COUNT; hand++)
		if(gDropped[hand].slot == slot &&
		   (copy < 0 || !DualWieldSlot(slot) || gDropped[hand].copy == copy)){
			if(BuildDroppedMatrix(hand, matrix))
				return true;
			gDropped[hand] = DroppedWeapon();
		}
	return false;
}

void GetTrackedWeaponOffset(float *x, float *y, float *z) { GetTrackedWeaponOffsetForType(1, GetVrWeaponTypeForSlot(Max(gHeldSlot[1], 0)), x, y, z); }
void GetTrackedWeaponRotation(float *x, float *y, float *z) { GetTrackedWeaponRotationForType(1, GetVrWeaponTypeForSlot(Max(gHeldSlot[1], 0)), x, y, z); }

void
GetTrackedWeaponOffsetForType(int hand, int weaponType,
	float *x, float *y, float *z)
{
	WeaponCalibration *calibration = GetCalibration(hand, weaponType);
	if(x) *x = calibration ? (float)calibration->offsetX/200.0f : 0.0f;
	if(y) *y = calibration ? (float)calibration->offsetY/200.0f : 0.0f;
	if(z) *z = calibration ? (float)calibration->offsetZ/200.0f : 0.0f;
}

void
GetTrackedWeaponRotationForType(int hand, int weaponType,
	float *x, float *y, float *z)
{
	WeaponCalibration *calibration = GetCalibration(hand, weaponType);
	if(x) *x = calibration ? (float)calibration->rotationX/WEAPON_VALUE_SCALE : 0.0f;
	if(y) *y = calibration ? (float)calibration->rotationY/WEAPON_VALUE_SCALE : 0.0f;
	if(z) *z = calibration ? (float)calibration->rotationZ/WEAPON_VALUE_SCALE : 0.0f;
}

bool
ApplyTrackedWeaponTwoHandTransform(int hand, int weaponType,
	CMatrix *matrix)
{
	if(VrBodyIK::WeaponOwnershipActive())
		return false;
	if(!matrix)
		return false;
	CVector pivot, axis;
	float angle;
	if(!BuildTwoHandRotation(hand, weaponType, &pivot, &axis, &angle))
		return false;
	matrix->GetRight() =
		RotateAroundAxis(matrix->GetRight(), axis, angle);
	matrix->GetUp() =
		RotateAroundAxis(matrix->GetUp(), axis, angle);
	matrix->GetForward() =
		RotateAroundAxis(matrix->GetForward(), axis, angle);
	matrix->GetPosition() = pivot+
		RotateAroundAxis(matrix->GetPosition()-pivot, axis, angle);
	return true;
}

bool
GetTrackedWeaponSupportAnchor(int hand, int weaponType,
	CVector *position, bool *engaged)
{
	if(!position || hand < 0 || hand >= VR_HAND_COUNT ||
	   gHeldSlot[hand] < 0)
		return false;
	CVector pivot, expected;
	if(!BuildSupportVector(hand, weaponType, &pivot, &expected))
		return false;
	const int support = gSupportHand[hand] >= 0 ?
		gSupportHand[hand] : 1-hand;
	if(gSupportHand[hand] < 0){
		if(!gPoseValid[support] || gHeldSlot[support] >= 0 ||
		   (gGripMatrix[support].GetPosition()-
		    (pivot+expected)).Magnitude() > 0.45f)
			return false;
	}else{
		CVector axis;
		float angle;
		if(BuildTwoHandRotation(hand, weaponType, &pivot, &axis, &angle))
			expected = RotateAroundAxis(expected, axis, angle);
	}
	*position = pivot+expected;
	if(engaged)
		*engaged = gSupportHand[hand] == support;
	return true;
}

bool
GetTrackedWeaponAim(int hand, int weaponType, CVector *source,
	CVector *direction)
{
	if(!BuildAim(hand, weaponType, source, direction))
		return false;
	if(IsTwoHanded(weaponType) && gSupportHand[hand] < 0){
		const float amplitude =
			GetOneHandAimSwayDegrees(weaponType)*1.5f;
		if(amplitude > 0.0f){
			CVector aimUp = gAimMatrix[hand].GetUp();
			aimUp -= *direction*DotProduct(aimUp, *direction);
			if(aimUp.MagnitudeSqr() < 0.0001f)
				aimUp = CVector(0.0f, 0.0f, 1.0f);
			aimUp.Normalise();
			CVector aimRight = CrossProduct(aimUp, *direction);
			aimRight.Normalise();
			const float time = (float)(
				CTimer::GetTimeInMillisecondsNonClipped() %
				120000U)*0.001f;
			const float phase = weaponType*0.731f+hand*2.173f;
			const float yaw = DEGTORAD(amplitude*(
				0.68f*sinf(3.35f*time+phase)+
				0.32f*sinf(8.91f*time+1.73f*phase+0.4f)));
			const float pitch = DEGTORAD(0.78f*amplitude*(
				0.62f*sinf(2.87f*time+1.31f*phase+1.2f)+
				0.38f*sinf(7.43f*time+0.77f*phase)));
			CVector swayedRight =
				RotateAroundAxis(aimRight, aimUp, yaw);
			*direction =
				RotateAroundAxis(*direction, aimUp, yaw);
			*direction =
				RotateAroundAxis(*direction, swayedRight, pitch);
			direction->Normalise();
		}
	}
	if(gScopeHand != hand || gScopeWeaponType != weaponType)
		return true;

	// In both 2D dynamic-reticle overlay mode and 3D physical scope mode, shots
	// travel authoritatively along the rifle barrel bore direction (*direction)
	// rather than forcibly snapping to the center gaze of the HMD.
	return true;
}

bool
GetTrackedThrowableLaunch(int hand, int weaponType, CVector *source,
	CVector *velocity)
{
	CVector direction;
	if(!IsPhysicalThrowable(weaponType) ||
	   !BuildAim(hand, weaponType, source, &direction))
		return false;
	float viewRight[3], viewUp[3], viewAt[3], viewPosition[3];
	if(rw::vulkan::getFirstPersonViewFrame(
	   viewRight, viewUp, viewAt, viewPosition)){
		const CVector anchor = VectorFromArray(viewPosition);
		CVector reach = *source-anchor;
		const float requestedDistance = reach.Magnitude();
		if(requestedDistance > 0.001f){
			reach *= 1.0f/requestedDistance;
			CColPoint point;
			CEntity *hitEntity = nil;
			CEntity *savedIgnoreEntity = CWorld::pIgnoreEntity;
			CWorld::pIgnoreEntity = FindPlayerPed();
			const bool hit = CWorld::ProcessLineOfSight(anchor, *source,
				point, hitEntity, true, true, true, true, true, false,
				false);
			CWorld::pIgnoreEntity = savedIgnoreEntity;
			if(hit){
				const float safeDistance = Max(0.0f,
					(point.point-anchor).Magnitude()-0.08f);
				*source = anchor+reach*safeDistance;
			}
		}
	}
	direction.Normalise();
	*velocity = direction*0.46f;
	return true;
}

void SetTrackedThrowablePreviewActive(int hand, bool active) { if(hand >= 0 && hand < VR_HAND_COUNT) gThrowablePreview[hand] = active; }
bool IsTrackedThrowablePreviewActive(int hand)
{
	return hand >= 0 && hand < VR_HAND_COUNT &&
		gThrowablePreview[hand] && gTriggerPressed[hand];
}

void
BeginTrackedWeaponFire(int hand, int weaponType, const CVector &source,
	const CVector &direction)
{
	gActiveFire = true;
	gActiveFireHand = hand;
	// 0.4.1 PC parity: controller kick on every shot.
	androidgame::TriggerWeaponHaptic(hand, 0.8f);
	gActiveFireWeaponType = weaponType;
	gActiveFireSource = source;
	gActiveFireDirection = direction;
	if(IsPhysicalThrowable(weaponType)){
		gActiveThrowable = GetTrackedThrowableLaunch(hand, weaponType,
			&gActiveThrowableSource, &gActiveThrowableVelocity);
	}else
		gActiveThrowable = false;
}

void
EndTrackedWeaponFire()
{
	gActiveFire = false;
	gActiveFireHand = -1;
	gActiveFireWeaponType = -1;
	gActiveThrowable = false;
}

bool
GetActiveTrackedWeaponAim(CVector *source, CVector *direction)
{
	if(!gActiveFire || !source || !direction)
		return false;
	*source = gActiveFireSource;
	*direction = gActiveFireDirection;
	return true;
}

// The aim ray of whichever tracked weapon the player is holding, in world
// space, or false when nothing is in hand. Worked out once a frame and kept:
// the threat scan asks for it from every ped that might be looking down the
// barrel, and none of them should pay for it twice.
bool
GetHeldTrackedWeaponAim(CVector *source, CVector *direction)
{
	static uint32 computedFrame = ~0u;
	static bool held;
	static CVector heldSource, heldDirection;

	const uint32 frame = CTimer::GetFrameCounter();
	if(frame != computedFrame){
		computedFrame = frame;
		held = false;
		CPlayerPed *player = FindPlayerPed();
		for(int hand = 0; player != nil && hand < 2 && !held; hand++){
			if(!IsTrackedWeaponHeld(hand))
				continue;
			const int slot = GetHeldWeaponSlot(hand);
			if(slot < 0 || slot >= TOTAL_WEAPON_SLOTS)
				continue;
			const int weaponType = GetVrWeaponTypeForSlot(slot);
			if(weaponType < 0)
				continue;
			CWeapon *weapon = &player->GetWeapon(weaponType);
			// The same list the laser draws for: something with a barrel,
			// pointed where the hand points.
			if(weapon == nil ||
			   !IsPhysicalGunType(weapon->m_eWeaponType))
				continue;
			if(!GetTrackedWeaponAim(hand, weapon->m_eWeaponType,
			   &heldSource, &heldDirection))
				continue;
			heldDirection.Normalise();
			held = true;
		}
	}
	if(!held || source == nil || direction == nil)
		return false;
	*source = heldSource;
	*direction = heldDirection;
	return true;
}

bool
GetActiveTrackedThrowableLaunch(CVector *source, CVector *velocity)
{
	if(!gActiveFire || !gActiveThrowable || !source || !velocity)
		return false;
	*source = gActiveThrowableSource;
	*velocity = gActiveThrowableVelocity;
	return true;
}

bool
GetQuestRawTrackedHandMatrix(int hand, CMatrix *matrix,
	float *grip, float *trigger)
{
	if(!matrix || hand < 0 || hand >= VR_HAND_COUNT || !gPoseValid[hand])
		return false;
	*matrix = gGripMatrix[hand];
	if(grip) *grip = gGrip[hand];
	if(trigger) *trigger = gTrigger[hand];
	return true;
}

void
RefreshQuestTrackedHandWorldPosesForRender()
{
	if(!gVrFirstPersonActive)
		return;

	const androidgame::PadInput &input = androidgame::GetPadInput();
	for(int hand = 0; hand < VR_HAND_COUNT; hand++){
		// ApplyTouchInput sampled this pose before CWorld::Process. The player
		// or vehicle can move substantially during that process, while the
		// first-person anchor is intentionally refreshed afterwards. Convert
		// the same OpenXR sample against that final anchor so the rendered
		// hands and weapons share the camera's simulation frame.
		CMatrix gripMatrix;
		if(!gPoseValid[hand] ||
		   !PoseToMatrix(input.gripPose[hand], &gripMatrix))
			continue;

		CMatrix aimMatrix;
		const bool aimValid =
			PoseToMatrix(input.aimPose[hand], &aimMatrix);
		gGripMatrix[hand] = gripMatrix;
		gAimMatrix[hand] = aimValid ? aimMatrix : gripMatrix;
	}
}

bool
GetQuestRawTrackedHandAimMatrix(int hand, CMatrix *matrix)
{
	if(!matrix || hand < 0 || hand >= VR_HAND_COUNT || !gPoseValid[hand])
		return false;
	*matrix = gAimMatrix[hand];
	return true;
}

static bool
ApplyTwoHandVisualHandLock(int hand, CMatrix *handMatrix,
	CVector *aimOrigin, CVector *aimDirection, float *grip, float *trigger,
	bool useCurrentWeaponSocket = false)
{
	if(hand < 0 || hand >= VR_HAND_COUNT)
		return false;
	int primary = -1;
	int support = -1;
	int primarySlot = -1;
	int weaponType = -1;
	for(int candidate = 0; candidate < VR_HAND_COUNT; candidate++){
		const int slot = gHeldSlot[candidate];
		const int candidateSupport = gSupportHand[candidate];
		if(slot < 0 || candidateSupport < 0 ||
		   candidateSupport >= VR_HAND_COUNT)
			continue;
		const int candidateType = GetVrWeaponTypeForSlot(slot);
		if(!IsTwoHanded(candidateType) ||
		   (hand != candidate && hand != candidateSupport))
			continue;
		primary = candidate;
		support = candidateSupport;
		primarySlot = slot;
		weaponType = candidateType;
		break;
	}
	if(primary < 0)
		return false;

	CVector pivot, axis;
	float angle;
	if(!BuildTwoHandRotation(primary, weaponType, &pivot, &axis, &angle))
		return false;
	if(hand == primary){
		if(handMatrix && angle != 0.0f){
			handMatrix->GetRight() = RotateAroundAxis(
				handMatrix->GetRight(), axis, angle);
			handMatrix->GetUp() = RotateAroundAxis(
				handMatrix->GetUp(), axis, angle);
			handMatrix->GetForward() = RotateAroundAxis(
				handMatrix->GetForward(), axis, angle);
			handMatrix->GetPosition() = pivot+RotateAroundAxis(
				handMatrix->GetPosition()-pivot, axis, angle);
		}
		if(aimOrigin && angle != 0.0f)
			*aimOrigin = pivot+RotateAroundAxis(*aimOrigin-pivot, axis, angle);
		if(aimDirection && angle != 0.0f){
			*aimDirection = RotateAroundAxis(*aimDirection, axis, angle);
			aimDirection->Normalise();
		}
		return true;
	}

	CVector rawPivot, weaponRight, weaponUp, weaponForward, expected;
	if(hand != support || !BuildSupportVector(primary, weaponType,
	   &rawPivot, &expected, &weaponRight, &weaponUp, &weaponForward))
		return false;
	// On the normal floating-hand path, rotate the support target by the
	// same delta used for the primary weapon hand. Body IK has a second pass
	// after solving the primary wrist; in that pass BuildSupportVector already
	// describes the current rendered gun, so rotating this socket again would
	// move the skinned hand away from the actual gun grip.
	if(angle != 0.0f && !useCurrentWeaponSocket){
		expected = RotateAroundAxis(expected, axis, angle);
		weaponRight = RotateAroundAxis(weaponRight, axis, angle);
		weaponUp = RotateAroundAxis(weaponUp, axis, angle);
		weaponForward = RotateAroundAxis(weaponForward, axis, angle);
	}
	const CVector lockedSocket = pivot+expected;
	WeaponCalibration *calibration = GetCalibration(primary, weaponType);
	if(calibration &&
	   calibration->supportPoseVersion >= SUPPORT_GRIP_MODEL_BOUND_V5 &&
	   gWeaponContactSlot[primary] == primarySlot &&
	   gWeaponContactType[primary] == weaponType &&
	   gWeaponContactFrame[primary] == CTimer::GetFrameCounter()){
		CVector barrel, top;
		GetSupportModelAxes(weaponType, *calibration,
			gWeaponContactMatrix[primary], &barrel, &top);
		CVector lateral;
		if(BuildCanonicalSupportFrame(barrel, top,
		   &lateral, &top, &barrel)){
			weaponRight = barrel;
			weaponUp = lateral*-1.0f;
			weaponForward = top*-1.0f;
		}
	}
	if(!BuildCanonicalSupportFrame(weaponForward, weaponUp,
	   &weaponRight, &weaponUp, &weaponForward))
		return false;
	const int gripType = calibration ? calibration->supportGripType :
		SUPPORT_GRIP_MAGAZINE;
	const float palmSign = support == 0 ? -1.0f : 1.0f;
	CVector encodedPalmAxis = gripType == SUPPORT_GRIP_FROM_BELOW ?
		weaponUp*(-palmSign) : weaponRight;
	encodedPalmAxis.Normalise();
	CVector handUp = encodedPalmAxis*palmSign;
	CVector handForward = weaponForward;
	handForward.Normalise();
	CVector handRight = CrossProduct(handUp, handForward);
	if(handRight.MagnitudeSqr() < 0.0001f)
		return false;
	handRight.Normalise();
	const float rotationX = DEGTORAD((float)(calibration ?
		calibration->supportRotationX : 0)/WEAPON_VALUE_SCALE);
	const float rotationY = DEGTORAD((float)(calibration ?
		calibration->supportRotationY : 0)/WEAPON_VALUE_SCALE);
	const float rotationZ = DEGTORAD((float)(calibration ?
		calibration->supportRotationZ : 0)/WEAPON_VALUE_SCALE);
	if(rotationX != 0.0f){
		handUp = RotateAroundAxis(handUp, handRight, rotationX);
		handForward = RotateAroundAxis(handForward, handRight, rotationX);
	}
	if(rotationY != 0.0f){
		handRight = RotateAroundAxis(handRight, handUp, rotationY);
		handForward = RotateAroundAxis(handForward, handUp, rotationY);
	}
	if(rotationZ != 0.0f){
		handRight = RotateAroundAxis(handRight, handForward, rotationZ);
		handUp = RotateAroundAxis(handUp, handForward, rotationZ);
	}
	handForward.Normalise();
	handUp -= handForward*DotProduct(handUp, handForward);
	if(handUp.MagnitudeSqr() < 0.0001f)
		return false;
	handUp.Normalise();
	handRight = CrossProduct(handUp, handForward);
	handRight.Normalise();
	// The UltimateXR LEFT mesh was canonicalised during conversion and needs
	// this one visual roll; RIGHT is already in the authored support basis.
	if(support == 0){
		handUp *= -1.0f;
		handRight *= -1.0f;
	}
	encodedPalmAxis = handUp*palmSign;
	if(handMatrix){
		handMatrix->GetRight() = encodedPalmAxis;
		handMatrix->GetUp() = handForward;
		handMatrix->GetForward() = handRight*(support == 0 ? -1.0f : 1.0f);
		handMatrix->GetPosition() = lockedSocket;
	}
	if(aimOrigin)
		*aimOrigin = lockedSocket;
	if(aimDirection)
		*aimDirection = handForward;
	if(grip)
		*grip = gripType == SUPPORT_GRIP_FROM_BELOW ? 0.55f : 1.0f;
	if(trigger)
		*trigger = gripType == SUPPORT_GRIP_FROM_BELOW ? 0.65f : 1.0f;
	return true;
}

bool
ApplyTrackedWeaponSupportHandLock(int hand, CMatrix *handMatrix,
	bool useCurrentWeaponSocket)
{
	if(!handMatrix || hand < 0 || hand >= VR_HAND_COUNT)
		return false;

	// A two-handed weapon is one rigid visual assembly around the primary
	// grip pivot. Body IK must rotate BOTH wrists from the same constraint:
	// locking only the support wrist lets it sweep around the socket while the
	// weapon stays aligned to the unrotated primary wrist. The primary branch
	// rotates its basis about the same pivot, which carries the attached gun's
	// front toward the support controller; the support branch places the offhand
	// on the correspondingly rotated grip socket.
	bool weaponHandActive = false;
	for(int candidate = 0; candidate < VR_HAND_COUNT; candidate++){
		const int slot = gHeldSlot[candidate];
		const int support = gSupportHand[candidate];
		if(slot < 0 || (candidate != hand && support != hand) ||
		   support < 0 || support >= VR_HAND_COUNT || support == candidate)
			continue;
		const int weaponType = GetVrWeaponTypeForSlot(slot);
		if(IsTwoHanded(weaponType)){
			weaponHandActive = true;
			break;
		}
	}
	if(!weaponHandActive)
		return false;

	return ApplyTwoHandVisualHandLock(
		hand, handMatrix, nil, nil, nil, nil,
		useCurrentWeaponSocket);
}

bool
GetCarWheelHandVisualBasis(int hand, CVector *right, CVector *up,
	CVector *forward)
{
	if(hand != 0 || !right || !up || !forward || !IsImmersiveCarDrivingActive() ||
	   !IsImmersiveSteeringHandleGrabbed(hand))
		return false;
	// The rim is a position/steering constraint, not a controller quaternion.
	// Use the same authored mesh conversion as the free tracked hand. The
	// converted assets rely on its handedness signs; imposing an orthonormal
	// wheel basis here turns the left mesh into the wrong anatomical hand.
	CVector unusedPosition, trackedRight, trackedUp, trackedForward;
	if(!BuildLegacyTrackedWeaponHandBasis(
	   hand, &unusedPosition, &trackedRight, &trackedUp, &trackedForward))
		return false;
	*right = trackedRight;
	*up = trackedUp;
	*forward = trackedForward;
	return true;
}

bool
GetTrackedHandMatrix(int hand, CMatrix *matrix, float *grip, float *trigger)
{
	if(!matrix || hand < 0 || hand >= VR_HAND_COUNT || !gPoseValid[hand])
		return false;
	if(IsImmersiveCarDrivingActive() &&
	   IsImmersiveSteeringHandleGrabbed(hand) &&
	   GetImmersiveSteeringHandleMatrix(hand, matrix)){
		// The interaction anchor remains exactly on the authoritative rim. Move
		// only the rendered palm a little outward, then close every finger around
		// the rim; the marker, radius and steering maths are deliberately unchanged.
		const float side = hand == 0 ? -1.0f : 1.0f;
		matrix->GetPosition() += matrix->GetRight()*(side*0.025f);
		const float wheelGrip = Max(gGrip[hand], 0.90f);
		if(grip) *grip = wheelGrip;
		if(trigger) *trigger = Max(gTrigger[hand], wheelGrip);
		return true;
	}
	if(IsImmersiveSteeringHandleGrabbed(hand) &&
	   GetImmersiveSteeringHandleMatrix(hand, matrix)){
		if(grip) *grip = gGrip[hand];
		if(trigger) *trigger = gTrigger[hand];
		return true;
	}
	return GetQuestRawTrackedHandMatrix(hand, matrix, grip, trigger);
}

static void
ApplyBikeVisualGripAlignment(int hand, CMatrix *matrix)
{
	if(!matrix || hand < 0 || hand >= VR_HAND_COUNT)
		return;
	// The calibrated socket axis runs along the cylindrical handlebar grip, but
	// the rendered fingers must wrap across it. Convert only the visual wrist;
	// interaction sockets, steering and saved calibration remain authoritative.
	const float angle = DEGTORAD(-90.0f);
	matrix->GetForward() = RotateAroundAxis(matrix->GetForward(),
		matrix->GetRight(), angle);
	matrix->GetUp() = RotateAroundAxis(matrix->GetUp(),
		matrix->GetRight(), angle);
	matrix->GetForward().Normalise();
	matrix->GetUp().Normalise();
	// A round grip has no intrinsic forward hemisphere. Resolve that ambiguity
	// deterministically against the bike so tiny pose noise cannot flip the hand
	// by 180 degrees from one frame to the next.
	CVehicle *vehicle = FindPlayerVehicle();
	if(vehicle && vehicle->IsBike() &&
	   DotProduct(matrix->GetForward(), vehicle->GetForward()) < 0.0f){
		matrix->GetForward() *= -1.0f;
		matrix->GetUp() *= -1.0f;
	}
}

bool
GetTrackedVisualHandMatrix(int hand, CMatrix *matrix, float *grip,
	float *trigger)
{
	if(!GetTrackedHandMatrix(hand, matrix, grip, trigger))
		return false;
	if(IsImmersiveBikeHandleGrabbed(hand))
		ApplyBikeVisualGripAlignment(hand, matrix);
	ApplyTwoHandVisualHandLock(hand, matrix, nil, nil, grip, trigger);
	// Last, over whatever produced the pose: the rendered hand only, never
	// the anchor the wheel maths and the grab test run against.
	ApplyQuestWheelHandPullBack(hand, matrix);
	return true;
}

bool
GetTrackedHandAimRay(int hand, CVector *origin, CVector *direction)
{
	if(!origin || !direction || hand < 0 ||
	   hand >= VR_HAND_COUNT || !gPoseValid[hand])
		return false;
	CMatrix handle;
	if(IsImmersiveSteeringHandleGrabbed(hand) &&
	   GetImmersiveSteeringHandleMatrix(hand, &handle)){
		*origin = handle.GetPosition();
		*direction = handle.GetForward();
		direction->Normalise();
		return true;
	}
	*origin = gAimMatrix[hand].GetPosition();
	*direction = gAimMatrix[hand].GetForward();
	direction->Normalise();
	return true;
}

bool
GetTrackedIndexTip(int hand, CVector *tip)
{
    if(!tip || hand<0 || hand>=VR_HAND_COUNT || !AreTrackedHandsEnabled() || !gPoseValid[hand] ||
       IsTrackedWeaponHeld(hand) || IsTrackedDetonatorActive(hand) ||
       IsImmersiveSteeringHandleGrabbed(hand) || IsImmersiveBikeHandleGrabbed(hand)) return false;
    CMatrix matrix; float grip=0,trigger=0;
    if(!GetTrackedVisualHandMatrix(hand,&matrix,&grip,&trigger)) return false;
    // Match RenderVrTrackedHand's basis and optional body-IK wrist exactly.
    CVector up=matrix.GetRight()*(hand==1?1.f:-1.f),origin,forward;
    if(!GetTrackedVisualHandAimRay(hand,&origin,&forward)) forward=matrix.GetUp();
    if(forward.MagnitudeSqr()<.0001f) return false;
    forward.Normalise(); up-=forward*DotProduct(up,forward);
    if(up.MagnitudeSqr()<.0001f) up=matrix.GetRight()*(hand==1?1.f:-1.f);
    up.Normalise(); CVector right=CrossProduct(up,forward); right.Normalise();
    if(DotProduct(right,matrix.GetForward())<0) right*= -1.f;
    CVector position=matrix.GetPosition();
    if(VrBodyIK::WeaponOwnershipActive() &&
       VrBodyIK::GetRigidWeaponHandBasis(hand,&position,&right,&up,&forward) && hand==0) up*= -1.f;
    return VRHandModel::IndexTip(hand,position,right,up,forward,grip,trigger,tip);
}

bool
GetTrackedVisualHandAimRay(int hand, CVector *origin, CVector *direction)
{
	if(!origin || !direction)
		return false;
	if(IsImmersiveBikeHandleGrabbed(hand)){
		CMatrix visualHandle;
		if(GetImmersiveSteeringHandleMatrix(hand, &visualHandle)){
			ApplyBikeVisualGripAlignment(hand, &visualHandle);
			*origin = visualHandle.GetPosition();
			*direction = visualHandle.GetForward();
			direction->Normalise();
			return true;
		}
	}
	if(!GetTrackedHandAimRay(hand, origin, direction))
		return false;
	ApplyTwoHandVisualHandLock(hand, nil, origin, direction, nil, nil);
	return true;
}

bool ConsumePhysicalMeleeStrike(int hand, int *slot, int *weaponType,
	CVector *sweepStart, CVector *sweepEnd, float *speed,
	CVector *rootStart, CVector *rootEnd, float *impactScale)
{
	if(hand < 0 || hand >= VR_HAND_COUNT || !slot || !weaponType ||
	   !sweepStart || !sweepEnd || !gMeleeStrike[hand].pending)
		return false;
	MeleeStrike &strike = gMeleeStrike[hand];
	if(CTimer::GetFrameCounter()-strike.frame > 1U){
		strike.pending = false;
		return false;
	}
	*slot = strike.slot;
	*weaponType = strike.weaponType;
	*sweepStart = strike.tipStart;
	*sweepEnd = strike.tipEnd;
	if(rootStart) *rootStart = strike.rootStart;
	if(rootEnd) *rootEnd = strike.rootEnd;
	if(speed) *speed = strike.speed;
	if(impactScale) *impactScale = strike.impactScale;
	strike.pending = false;
	return true;
}

void
ResolvePhysicalMeleeStrike(int hand, bool contact)
{
	if(!contact || hand < 0 || hand >= VR_HAND_COUNT)
		return;
	// A miss leaves the swing live so its continuation segments can still
	// reach the target; a real contact latches until the hand slows down.
	gMeleeMotion[hand].armed = false;
	gMeleeMotion[hand].strikeInProgress = false;
	gMeleeMotion[hand].swingTravel = 0.0f;
	gMeleeMotion[hand].strikeContinueUntil = 0;
	gMeleeMotion[hand].lastStrikeTime =
		CTimer::GetTimeInMillisecondsNonClipped();
	gMeleeMotion[hand].calmSince = 0;
}

int
GetQuestWeaponSettingCount()
{
	return 7;
}

const char *
GetQuestWeaponSettingName(int setting)
{
	static const char *names[] = {
		"VR HANDS", "WEAPON LASER", "HOLSTER HIGHLIGHTS",
		"MANUAL RELOADING", "SCOPE AIM",
		"WEAPON GRIP LOCK", "PISTOL FIRE"
	};
	return setting >= 0 && setting < (int)ARRAY_SIZE(names) ?
		names[setting] : "UNKNOWN";
}

bool
GetQuestWeaponSetting(int setting)
{
	LoadSettings();
	switch(setting){
	case 0: return gHandsEnabled;
	case 1: return gWeaponLaser;
	case 2: return gHolsterHighlights;
	case 3: return gManualReload;
	case 4: return gScopeAimMode != SCOPE_AIM_OFF;
	case 5: return gGripLock;
	case 6: return gPistolHoldToRepeat;
	default: return false;
	}
}

void
ToggleQuestWeaponSetting(int setting)
{
	LoadSettings();
	if(setting == 4){
		CycleQuestScopeAimMode(1);
		return;
	}
	bool *value = nil;
	const char *key = nil;
	switch(setting){
	case 0: value = &gHandsEnabled; key = "VrHands"; break;
	case 1: value = &gWeaponLaser; key = "WeaponLaser"; break;
	case 2: value = &gHolsterHighlights; key = "HolsterHighlights"; break;
	case 3: value = &gManualReload; key = "ManualReloading"; break;
	case 5: value = &gGripLock; key = "WeaponGripLock"; break;
	case 6: value = &gPistolHoldToRepeat; key = "PistolHoldToRepeat"; break;
	default: return;
	}
	*value = !*value;
	WritePrivateProfileStringA("VR", key, *value ? "1" : "0",
		kSettingsPath);
	if(setting == 3 && !gManualReload)
		for(int hand = 0; hand < VR_HAND_COUNT; hand++)
			ResetManualReloadState(gManualReloadState[hand]);
}

bool
IsQuestPistolHoldToRepeatEnabled()
{
	LoadSettings();
	return gPistolHoldToRepeat;
}

int
GetQuestCalibrationWeaponType(int hand)
{
	if(hand >= 0 && hand < VR_HAND_COUNT && gHeldSlot[hand] >= 0)
		return GetVrWeaponTypeForSlot(gHeldSlot[hand]);
	for(int other = 0; other < VR_HAND_COUNT; other++)
		if(gHeldSlot[other] >= 0)
			return GetVrWeaponTypeForSlot(gHeldSlot[other]);
	CPlayerPed *player = FindPlayerPed();
	return player ? player->GetWeapon()->m_eWeaponType :
		WEAPONTYPE_UNARMED;
}

bool IsQuestDualWieldEligible(int type)
{
	return type == WEAPONTYPE_COLT45 || type == WEAPONTYPE_UZI;
}

bool IsQuestDualWieldEnabled(int type)
{
	LoadSettings();
	return type == WEAPONTYPE_COLT45 ? gDualWieldPistol :
		type == WEAPONTYPE_UZI ? gDualWieldSmg : false;
}

void ToggleQuestDualWield(int type)
{
	LoadSettings();
	if(!IsQuestDualWieldEligible(type)) return;
	bool &enabled = type == WEAPONTYPE_COLT45 ? gDualWieldPistol : gDualWieldSmg;
	enabled = !enabled;
	QuestDualWield::Reset(type);
	WritePrivateProfileStringA("VR", type == WEAPONTYPE_COLT45 ? "DualWieldPistol" : "DualWieldSmg",
		enabled ? "1" : "0", kSettingsPath);
	if(enabled) return;
	const int slot = type == WEAPONTYPE_COLT45 ? WEAPONSLOT_HANDGUN : WEAPONSLOT_SUBMACHINEGUN;
	NormalizeDualWieldHolsters();
	// Keep the native inventory copy; release any second held/dropped copy.
	for(int hand = 0; hand < VR_HAND_COUNT; hand++){
		if(gHeldSlot[hand] == slot && gHeldCopy[hand] == 0){
			ClearSupportForHand(hand);
			gHeldSlot[hand] = gHolsterSelection[hand] = -1;
			gWeaponRenderSlot[hand] = gWeaponContactSlot[hand] = -1;
			gManualReloadState[hand] = ManualReloadState();
		}
		if(gDropped[hand].slot == slot && gDropped[hand].copy == 0)
			gDropped[hand] = DroppedWeapon();
	}
}

static int *
QuestCalibrationValue(WeaponCalibration *calibration, int item,
	int *minimum, int *maximum)
{
	if(!calibration || !minimum || !maximum)
		return nil;
	*minimum = -100;
	*maximum = 100;
	switch(item){
	case 0: return &calibration->aimOffsetX;
	case 1: return &calibration->aimOffsetY;
	case 2: return &calibration->aimOffsetZ;
	case 3:
		*minimum = -360; *maximum = 360;
		return &calibration->aimRotationX;
	case 4:
		*minimum = -360; *maximum = 360;
		return &calibration->aimRotationY;
	case 5:
		*minimum = -360; *maximum = 360;
		return &calibration->aimRotationZ;
	case 6: return &calibration->offsetX;
	case 7: return &calibration->offsetY;
	case 8: return &calibration->offsetZ;
	case 9:
		*minimum = -360; *maximum = 360;
		return &calibration->rotationX;
	case 10:
		*minimum = -360; *maximum = 360;
		return &calibration->rotationY;
	case 11:
		*minimum = -360; *maximum = 360;
		return &calibration->rotationZ;
	case 12:
		*minimum = -200; *maximum = 200;
		return &calibration->supportX;
	case 13:
		*minimum = -200; *maximum = 200;
		return &calibration->supportY;
	case 14:
		*minimum = -200; *maximum = 200;
		return &calibration->supportZ;
	case 15:
		*minimum = -360; *maximum = 360;
		return &calibration->supportRotationX;
	case 16:
		*minimum = -360; *maximum = 360;
		return &calibration->supportRotationY;
	case 17:
		*minimum = -360; *maximum = 360;
		return &calibration->supportRotationZ;
	case 18:
		*minimum = 0; *maximum = SUPPORT_GRIP_TYPE_COUNT-1;
		return &calibration->supportGripType;
	default: return nil;
	}
}

static void
WriteCalibrationInt(const char *section, int hand, const char *name,
	int value)
{
	char key[64], text[24];
	sprintf(text, "%d", value);
	WritePrivateProfileStringA(section,
		CalibrationKey(hand, name, key), text, kSettingsPath);
}

static void
SaveQuestCalibrationToSection(const char *section, int hand,
	const WeaponCalibration &calibration)
{
	WriteCalibrationInt(section, hand, "Configured", 1);
	WriteCalibrationInt(section, hand, "ValueScale", WEAPON_VALUE_SCALE);
	WriteCalibrationInt(section, hand, "OffsetX", calibration.offsetX);
	WriteCalibrationInt(section, hand, "OffsetY", calibration.offsetY);
	WriteCalibrationInt(section, hand, "OffsetZ", calibration.offsetZ);
	WriteCalibrationInt(section, hand, "AimOffsetX",
		calibration.aimOffsetX);
	WriteCalibrationInt(section, hand, "AimOffsetY",
		calibration.aimOffsetY);
	WriteCalibrationInt(section, hand, "AimOffsetZ",
		calibration.aimOffsetZ);
	WriteCalibrationInt(section, hand, "AimRotationX",
		calibration.aimRotationX);
	WriteCalibrationInt(section, hand, "AimRotationY",
		calibration.aimRotationY);
	WriteCalibrationInt(section, hand, "AimRotationZ",
		calibration.aimRotationZ);
	WriteCalibrationInt(section, hand, "RotationX",
		calibration.rotationX);
	WriteCalibrationInt(section, hand, "RotationY",
		calibration.rotationY);
	WriteCalibrationInt(section, hand, "RotationZ",
		calibration.rotationZ);
	WriteCalibrationInt(section, hand, "SupportGripConfigured", 1);
	WriteCalibrationInt(section, hand, "SupportGripValueScale",
		WEAPON_VALUE_SCALE);
	WriteCalibrationInt(section, hand, "SupportGripOffsetX",
		calibration.supportX);
	WriteCalibrationInt(section, hand, "SupportGripOffsetY",
		calibration.supportY);
	WriteCalibrationInt(section, hand, "SupportGripOffsetZ",
		calibration.supportZ);
	WriteCalibrationInt(section, hand, "SupportGripRotationX",
		calibration.supportRotationX);
	WriteCalibrationInt(section, hand, "SupportGripRotationY",
		calibration.supportRotationY);
	WriteCalibrationInt(section, hand, "SupportGripRotationZ",
		calibration.supportRotationZ);
	WriteCalibrationInt(section, hand, "SupportGripPoseVersion",
		calibration.supportPoseVersion);
	WriteCalibrationInt(section, hand, "SupportGripStyle", calibration.supportGripType);
}

static void
QuestCalibrationSection(int weaponType, bool defaults, char *section)
{
	sprintf(section, ModelSets::GetCalibrationSetForCategory(
		ModelSets::MODEL_CATEGORY_WEAPONS) == ModelSets::MODEL_SET_MODERN ?
		"VRWeaponModern%s_%02d_%s" : "VRWeapon%s_%02d_%s",
		defaults ? "Default" : "", weaponType, GetVrWeaponName(weaponType));
}

static void
SaveQuestCalibration(int hand, int weaponType, const WeaponCalibration &calibration)
{
	char section[96];
	QuestCalibrationSection(weaponType, false, section);
	SaveQuestCalibrationToSection(section, hand, calibration);
}

void
SaveQuestCalibrationDefault(int hand, int weaponType)
{
	WeaponCalibration *calibration = GetCalibration(hand, weaponType);
	if(!calibration) return;
	char section[96];
	QuestCalibrationSection(weaponType, true, section);
	SaveQuestCalibrationToSection(section, hand, *calibration);
}

void
RestoreQuestCalibrationDefault(int hand, int weaponType, bool factory)
{
	WeaponCalibration *calibration = GetCalibration(hand, weaponType);
	if(!calibration) return;
	GetDefaultCalibrationForSet(weaponType, hand,
		ModelSets::GetCalibrationSetForCategory(ModelSets::MODEL_CATEGORY_WEAPONS), calibration);
	char section[96];
	QuestCalibrationSection(weaponType, true, section);
	if(!factory && ReadCalibrationValue(section, hand, "Configured", 0)){
		static const char *keys[] = {
			"AimOffsetX", "AimOffsetY", "AimOffsetZ",
			"AimRotationX", "AimRotationY", "AimRotationZ",
			"OffsetX", "OffsetY", "OffsetZ", "RotationX", "RotationY", "RotationZ",
			"SupportGripOffsetX", "SupportGripOffsetY", "SupportGripOffsetZ",
			"SupportGripRotationX", "SupportGripRotationY", "SupportGripRotationZ",
			"SupportGripStyle"
		};
		for(int item = 0; item < (int)ARRAY_SIZE(keys); ++item){
			int minimum, maximum;
			int *value = QuestCalibrationValue(calibration, item, &minimum, &maximum);
			*value = clamp(ReadCalibrationValue(section, hand, keys[item], *value), minimum, maximum);
		}
		calibration->supportPoseVersion = clamp(ReadCalibrationValue(section, hand,
			"SupportGripPoseVersion", calibration->supportPoseVersion), 1, SUPPORT_GRIP_POSE_VERSION);
	}
	SaveQuestCalibration(hand, weaponType, *calibration);
}

void
CopyQuestCalibrationToOtherHand(int hand, int weaponType)
{
	WeaponCalibration *source = GetCalibration(hand, weaponType);
	WeaponCalibration *destination = GetCalibration(1-hand, weaponType);
	if(!source || !destination) return;
	*destination = *source;
	SaveQuestCalibration(1-hand, weaponType, *destination);
}

int
GetQuestCalibrationValue(int hand, int weaponType, int item)
{
	int minimum, maximum;
	int *value = QuestCalibrationValue(
		GetCalibration(hand, weaponType), item, &minimum, &maximum);
	return value ? *value : 0;
}

void
AdjustQuestCalibrationValue(int hand, int weaponType, int item,
	int direction)
{
	if(direction == 0)
		return;
	WeaponCalibration *calibration = GetCalibration(hand, weaponType);
	int minimum, maximum;
	int *value = QuestCalibrationValue(calibration, item,
		&minimum, &maximum);
	if(!value)
		return;
	*value = VrWeaponCalibrationMenu::Adjusted(*value, item,
		direction, minimum, maximum);
	// The underhand grip starts palm down, which points the fingers at the
	// floor instead of around the grip. Seed the roll the first time the
	// style is picked; a wrist that has already been rolled keeps its own.
	if(item == 18 && *value == SUPPORT_GRIP_FROM_BELOW &&
	   calibration->supportRotationZ == 0)
		calibration->supportRotationZ =
			SUPPORT_GRIP_FROM_BELOW_ROLL_HALF_DEG;
	if(item >= 12 && calibration->supportPoseVersion < SUPPORT_GRIP_MODEL_BOUND_V5)
		calibration->supportPoseVersion = SUPPORT_GRIP_MODEL_BOUND_V5;
	SaveQuestCalibration(hand, weaponType, *calibration);
}

const char *
GetQuestHolsterGrabDistanceName()
{
	LoadSettings();
	static const char *names[] = { "FAR", "MEDIUM", "CLOSE" };
	return names[gHolsterGrabDistance];
}

void
CycleQuestHolsterGrabDistance(int direction)
{
	LoadSettings();
	const int count = (int)ARRAY_SIZE(kHolsterGrabRadii);
	gHolsterGrabDistance = (gHolsterGrabDistance + (direction < 0 ? count-1 : 1)) % count;
	char value[16];
	sprintf(value, "%d", gHolsterGrabDistance);
	WritePrivateProfileStringA("VR", "HolsterGrabDistance", value, kSettingsPath);
}

int
GetQuestHolsterPointCount()
{
	return HOLSTER_POINT_COUNT;
}

const char *
GetQuestHolsterPointName(int point)
{
	static const char *names[HOLSTER_POINT_COUNT] = {
		"WAIST LEFT", "WAIST RIGHT", "CHEST LEFT", "CHEST RIGHT",
		"CHEST CENTER THROWABLE", "BACK LEFT", "BACK RIGHT"
	};
	return point >= 0 && point < HOLSTER_POINT_COUNT ?
		names[point] : "UNKNOWN";
}

int
GetQuestHolsterPointSlot(int point)
{
	LoadSettings();
	return point >= 0 && point < HOLSTER_POINT_COUNT ?
		gHolsterPointSlot[point] : -1;
}

void
CycleQuestHolsterPointSlot(int point, int direction)
{
	LoadSettings();
	if(point < 0 || point >= HOLSTER_POINT_COUNT || direction == 0)
		return;
	static const int choices[] = {
		-1, WEAPONSLOT_MELEE, WEAPONSLOT_HANDGUN,
		WEAPONSLOT_SHOTGUN, WEAPONSLOT_SUBMACHINEGUN,
		WEAPONSLOT_RIFLE, WEAPONSLOT_HEAVY, WEAPONSLOT_SNIPER,
		WEAPONSLOT_OTHER, WEAPONSLOT_AK47, WEAPONSLOT_ROCKET,
		WEAPONSLOT_PROJECTILE, WEAPONSLOT_GRENADE
	};
	int choice = 0;
	for(int index = 0; index < (int)ARRAY_SIZE(choices); index++)
		if(choices[index] == gHolsterPointSlot[point])
			choice = index;
	for(int attempts = 0; attempts < (int)ARRAY_SIZE(choices);
	    attempts++){
		choice = (choice+(direction < 0 ? -1 : 1)+
			(int)ARRAY_SIZE(choices))%(int)ARRAY_SIZE(choices);
		const int candidate = choices[choice];
		if(point == HOLSTER_CHEST_CENTRE && candidate != -1 && candidate != WEAPONSLOT_PROJECTILE &&
		   candidate != WEAPONSLOT_GRENADE)
			continue;
		bool used = false;
		if(candidate >= 0)
			for(int other = 0; other < HOLSTER_POINT_COUNT; other++)
				if(other != point &&
				   gHolsterPointSlot[other] == candidate &&
				   (!DualWieldSlot(candidate) || HolsterCopy(other) == HolsterCopy(point)))
					used = true;
		if(!used){
			gHolsterPointSlot[point] = candidate;
			break;
		}
	}
	static const char *keys[HOLSTER_POINT_COUNT] = {
		"HolsterWaistLeftSlot", "HolsterWaistRightSlot",
		"HolsterChestLeftSlot", "HolsterChestRightSlot", "HolsterChestCentreSlot",
		"HolsterBackLeftSlot", "HolsterBackRightSlot"
	};
	if(keys[point]){
		char value[16];
		sprintf(value, "%d", gHolsterPointSlot[point]);
		WritePrivateProfileStringA("VR", keys[point], value,
			kSettingsPath);
	}
}

bool CanSkipDesktopGameplayRender() { return IsPhysicalWeaponInteractionActive(); }
void ToggleCheatMenu() {}

void
PrepareForGameShutdown()
{
	ReleaseAllVrHolsterWeapons();
	InvalidateQuestWeaponLaserOverrides();
	QuestDualWield::Reset(WEAPONTYPE_COLT45);
	QuestDualWield::Reset(WEAPONTYPE_UZI);
	for(int hand = 0; hand < VR_HAND_COUNT; hand++){
		gPoseValid[hand] = false;
		gGrip[hand] = 0.0f;
		gTrigger[hand] = 0.0f;
		gTriggerPressed[hand] = false;
		gTriggerJustPressed[hand] = false;
		gTriggerJustReleased[hand] = false;
		gGripLatched[hand] = false;
		gHeldSlot[hand] = -1;
		gHolsterSelection[hand] = -1;
		gSupportHand[hand] = -1;
		gWeaponRenderSlot[hand] = -1;
		gWeaponContactSlot[hand] = -1;
		gWeaponContactType[hand] = -1;
		gWeaponContactFrame[hand] = 0;
		gDropped[hand] = DroppedWeapon();
		ResetManualReloadState(gManualReloadState[hand]);
		gManualReloadGripDown[hand] = false;
		gMeleeMotion[hand] = MeleeMotion();
		gMeleeStrike[hand] = MeleeStrike();
		gThrowablePreview[hand] = false;
		gTrackedDetonatorWasActive[hand] = false;
		gTrackedDetonatorWaitForRelease[hand] = false;
		gTrackedDetonatorJustPressed[hand] = false;
	}
	gWeaponHolsterMask = 0;
	gAttachedMissionWeaponForced = false;
	gAttachedMissionWeaponSlot = -1;
	gTrackedDetonatorHand = -1;
	gActiveFire = false;
	gActiveFireHand = -1;
	gActiveFireWeaponType = -1;
	gActiveThrowable = false;
	gScopeHand = -1;
	gScopeWeaponType = -1;
	gScopeCandidateHand = -1;
	gScopeCandidateType = -1;
	gBodyForward = CVector(0.0f, 1.0f, 0.0f);
	gBodyForwardValid = false;
	VRHandModel::Shutdown();
}

} // namespace OculusVR

#endif
