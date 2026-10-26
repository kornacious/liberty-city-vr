#include "common.h"

#include "VrBodyIK.h"
#include "VrRagdoll.h"
#include "Bones.h"
#include "OculusVR.h"
#include "Ped.h"
#include "PlayerInfo.h"
#include "World.h"
#include "PlayerPed.h"
#include "Vehicle.h"
#include "RwHelper.h"
#include "android.h"
#include "main.h"
#include "Timer.h"
#include "Camera.h"
#include "CutsceneMgr.h"

extern bool gVrFirstPersonActive;
extern void VrUpdateFirstPersonAnchor(bool postPhysics);

#include <android/log.h>
#include <math.h>

// First stage of the body solver: controller-driven arms on the real player
// skeleton. The weapon system remains authoritative and completely untouched.
// We only READ its final visual hand poses so Claude's wrists follow the same
// tracked hands that are already being rendered.
namespace VrBodyIK
{

static bool gVehicleTransitionActive;

bool
Enabled(void)
{
	return androidgame::VrBodyIKEnabled();
}

bool
VehicleEnabled(void)
{
	return androidgame::VrVehicleBodyIKEnabled();
}

void
SetVehicleTransition(bool active)
{
	gVehicleTransitionActive = active;
}

static bool
BodyIKGameplayContext(void)
{
	CPed *player = FindPlayerPed();
	if(player == nil || player->m_fHealth <= 0.0f ||
	   CWorld::Players[CWorld::PlayerInFocus].m_WBState != WBSTATE_PLAYING ||
	   VrRagdoll::Owns(player) || androidgame::VrKnockdownPresentationActive())
		return false;

	// These animations own the entire skeleton. Pinning the torso to the HMD
	// while the hips follow a fall/death/arrest tears the body apart. Ped state
	// changes can precede PlayerInfo's Wasted/Busted restart sequence.
	switch(player->m_nPedState){
	case PED_FALL:
	case PED_GETUP:
	case PED_STAGGER:
	case PED_DIVE_AWAY:
	case PED_DIE:
	case PED_DEAD:
	case PED_HANDS_UP:
	case PED_ARRESTED:
	case PED_DRAG_FROM_CAR:
		return false;
	default:
		break;
	}

	// Stereo cutscenes deliberately run through the first-person rendering path,
	// so gVrFirstPersonActive by itself is NOT proof that gameplay IK should run.
	// Native cutscene/scripted animation must own the player while those sequences
	// play.
	return !CCutsceneMgr::IsRunning() &&
		!CCutsceneMgr::IsCutsceneProcessing() &&
		!TheCamera.m_WideScreenOn &&
		androidgame::VrCutsceneCameraActor() == nil;
}

bool
WeaponOwnershipActive(void)
{
	if(!BodyIKGameplayContext())
		return false;

	if(Enabled() && (OculusVR::IsQuestFirstPersonOnFoot() ||
	   (::gVrFirstPersonActive && gVehicleTransitionActive)))
		return true;

	CPlayerPed *player = FindPlayerPed();
	return VehicleEnabled() && ::gVrFirstPersonActive &&
		player != nil && player->InVehicle() &&
		player->m_pMyVehicle != nil;
}

namespace
{
#define IKCALLOG(...) __android_log_print(ANDROID_LOG_INFO, "LibertyVR", __VA_ARGS__)

struct WristCalibration
{
	bool valid;
	bool frozen;
	bool clickDown;
	bool lastPoseValid;
	CVector positionLocal;
	CVector rightLocal;
	CVector upLocal;
	CVector atLocal;
	CMatrix frozenPose;
	CMatrix lastPose;
};

struct ArmBones
{
	int upper;
	int forearm;
	int hand;
};

struct ShoulderAnchor
{
	bool valid;
	CVector upperPosLocal;
};

struct UpperBodyBones
{
	int waist;
	int mid;
	int torso;
	int head;
};

struct UpperBodyAnchor
{
	bool valid;
	CVector midPosLocal, torsoPosLocal;
	CVector nativeTorsoPosLocal;

	CVector midRightLocal, midUpLocal, midAtLocal;
	CVector torsoRightLocal, torsoUpLocal, torsoAtLocal;

	CVector headFromTorsoLocal;
	CVector headRightLocal, headUpLocal, headAtLocal;
};

struct ArmSolveAnchor
{
	bool valid;
	float upperLen;
	float lowerLen;
	CVector upperDirLocal;
	CVector lowerDirLocal;
	CVector elbowOffsetLocal;
	CVector upperRightLocal, upperUpLocal, upperAtLocal;
	CVector foreRightLocal, foreUpLocal, foreAtLocal;
};

struct RigidWeaponHandLink
{
	bool valid;
	CVector positionLocal;
	CVector rightLocal;
	CVector upLocal;
	CVector forwardLocal;
};

static WristCalibration gWrist[2];
static ShoulderAnchor gShoulder[2];
static UpperBodyAnchor gUpperBody;
static ArmSolveAnchor gArmSolve[2];
static RigidWeaponHandLink gRigidWeaponLink[2];
static CMatrix gSolvedHandWorld[2];
static bool gSolvedHandValid[2];

static bool gVehicleNeckValid;
static CVehicle *gVehicleNeckVehicle;
static CVector gVehicleNeckWorld;

static CVehicle *gBikeBodyVehicle;
static bool gBikeBodyRootValid;
static CVector gBikeBodyRootLocal;

static bool gBikeBodyLateralValid;
static float gBikeBodyLateral;
static float gBikeBodyLateralLast;
static float gBikeBodyLateralAccum;
static int gBikeBodyLateralStableFrames;

enum { MAX_BODY_IK_NODES = 96 };

struct HandDescendantPose
{
	bool valid;
	bool nodeValid[MAX_BODY_IK_NODES];
	CVector posLocal[MAX_BODY_IK_NODES];
	CVector rightLocal[MAX_BODY_IK_NODES];
	CVector upLocal[MAX_BODY_IK_NODES];
	CVector atLocal[MAX_BODY_IK_NODES];
};

static HandDescendantPose gHandPose[2];

static const float gHandPoseOpenDeg[2] = { -40.0f, -40.0f };
static const float gHandPoseHoldDeg[2] = { -25.0f, -20.0f };

static float
ResolveHandPoseDeg(int handIndex, float grip)
{
	return gHandPoseOpenDeg[handIndex] +
		(gHandPoseHoldDeg[handIndex]-gHandPoseOpenDeg[handIndex])*grip;
}

static CPed *gLastPlayer;

static float ClampF(float v, float lo, float hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}

static CVector Unit(const CVector &v, const CVector &fallback)
{
	const float len2 = v.MagnitudeSqr();
	if(len2 <= 1.0e-8f)
		return fallback;
	return v * (1.0f / sqrtf(len2));
}

static CVector RotateAroundAxis(const CVector &v, const CVector &axis, float angle)
{
	const float c = cosf(angle);
	const float s = sinf(angle);
	return v*c + CrossProduct(axis, v)*s + axis*(DotProduct(axis, v)*(1.0f-c));
}

static CVector SwingVector(const CVector &fromIn, const CVector &toIn,
	const CVector &value, const CVector &fallbackAxis)
{
	const CVector from = Unit(fromIn, CVector(0.0f, 1.0f, 0.0f));
	const CVector to = Unit(toIn, from);
	const float dot = ClampF(DotProduct(from, to), -1.0f, 1.0f);
	if(dot > 0.99999f)
		return value;
	CVector axis = CrossProduct(from, to);
	if(axis.MagnitudeSqr() <= 1.0e-8f){
		axis = CrossProduct(from, fallbackAxis);
		if(axis.MagnitudeSqr() <= 1.0e-8f)
			axis = CrossProduct(from, CVector(0.0f, 0.0f, 1.0f));
	}
	axis = Unit(axis, CVector(1.0f, 0.0f, 0.0f));
	return RotateAroundAxis(value, axis, acosf(dot));
}

static void Orthonormalise(RwMatrix &m)
{
	CVector at = Unit(m.at, CVector(0.0f, 1.0f, 0.0f));
	CVector right = m.right - at*DotProduct(m.right, at);
	right = Unit(right, CVector(1.0f, 0.0f, 0.0f));
	CVector up = CrossProduct(at, right);
	up = Unit(up, CVector(0.0f, 0.0f, 1.0f));
	right = Unit(CrossProduct(up, at), right);
	m.right = right;
	m.up = up;
	m.at = at;
	m.flags = 0;
}

static CVector ToBasisLocal(const CVector &world, const CMatrix &basis)
{
	return CVector(DotProduct(world, basis.GetRight()),
		DotProduct(world, basis.GetUp()),
		DotProduct(world, basis.GetForward()));
}

static CVector FromBasisLocal(const CVector &local, const CMatrix &basis)
{
	return basis.GetRight()*local.x + basis.GetUp()*local.y +
		basis.GetForward()*local.z;
}

static bool BuildBikeCarrierBasis(CVehicle *bike, const CMatrix &nativeBody,
	CMatrix &out)
{
	if(bike == nil || !bike->IsBike())
		return false;

	out = nativeBody;
	CVector forward = bike->GetForward();
	CVector up = bike->GetUp();
	if(forward.MagnitudeSqr() <= 1.0e-8f ||
	   up.MagnitudeSqr() <= 1.0e-8f)
		return false;

	forward.Normalise();
	up.Normalise();
	CVector right = CrossProduct(forward, up);
	if(right.MagnitudeSqr() <= 1.0e-8f)
		return false;
	right.Normalise();
	up = CrossProduct(right, forward);
	if(up.MagnitudeSqr() <= 1.0e-8f)
		return false;
	up.Normalise();

	out.GetRight() = right;
	out.GetUp() = up;
	out.GetForward() = forward;
	return true;
}

static void CaptureRigidWeaponLink(int hand, const CMatrix &solvedHand,
	const CMatrix &weaponHandBasis)
{
	if(hand < 0 || hand > 1)
		return;

	RigidWeaponHandLink &link = gRigidWeaponLink[hand];
	link.positionLocal = ToBasisLocal(
		weaponHandBasis.GetPosition()-solvedHand.GetPosition(), solvedHand);
	link.rightLocal = ToBasisLocal(weaponHandBasis.GetRight(), solvedHand);
	link.upLocal = ToBasisLocal(weaponHandBasis.GetUp(), solvedHand);
	link.forwardLocal = ToBasisLocal(weaponHandBasis.GetForward(), solvedHand);
	link.valid = true;
}

static bool RebuildRigidWeaponBasis(int hand, CVector *position,
	CVector *right, CVector *up, CVector *forward)
{
	if(hand < 0 || hand > 1 || !position || !right || !up || !forward ||
	   !WeaponOwnershipActive() ||
	   !gSolvedHandValid[hand] || !gRigidWeaponLink[hand].valid)
		return false;

	const CMatrix &wrist = gSolvedHandWorld[hand];
	const RigidWeaponHandLink &link = gRigidWeaponLink[hand];

	*right = FromBasisLocal(link.rightLocal, wrist);
	*up = FromBasisLocal(link.upLocal, wrist);
	*forward = FromBasisLocal(link.forwardLocal, wrist);

	if(right->MagnitudeSqr() <= 1.0e-8f ||
	   up->MagnitudeSqr() <= 1.0e-8f ||
	   forward->MagnitudeSqr() <= 1.0e-8f)
		return false;

	right->Normalise();
	up->Normalise();
	forward->Normalise();

	*position = wrist.GetPosition() +
		FromBasisLocal(link.positionLocal, wrist);

	return true;
}

static void OrthonormaliseBasis(CMatrix &m)
{
	CVector at = Unit(m.GetForward(), CVector(0.0f, 1.0f, 0.0f));
	CVector right = m.GetRight() - at*DotProduct(m.GetRight(), at);
	right = Unit(right, CVector(1.0f, 0.0f, 0.0f));
	CVector up = Unit(CrossProduct(at, right), CVector(0.0f, 0.0f, 1.0f));
	right = Unit(CrossProduct(up, at), right);
	m.GetRight() = right;
	m.GetUp() = up;
	m.GetForward() = at;
}

static CMatrix ApplyCalibration(const CMatrix &raw, const WristCalibration &cal)
{
	CMatrix out;
	out.GetPosition() = raw.GetPosition() + FromBasisLocal(cal.positionLocal, raw);
	out.GetRight() = FromBasisLocal(cal.rightLocal, raw);
	out.GetUp() = FromBasisLocal(cal.upLocal, raw);
	out.GetForward() = FromBasisLocal(cal.atLocal, raw);
	OrthonormaliseBasis(out);
	return out;
}

// The authored pose can turn the pelvis relative to the entity matrix.
// Measure anatomical right from the hips instead of assuming entity right
// is also the skeleton's right (that assumption preserves a 90-degree twist).
static bool BuildCrouchPoseBasis(const CVector &leftHip,
	const CVector &rightHip, const CMatrix &visualBody, CMatrix &poseBody)
{
	CVector up = visualBody.GetUp();
	if(up.MagnitudeSqr() <= 1.0e-8f)
		return false;
	up.Normalise();
	CVector right = rightHip-leftHip;
	right -= up*DotProduct(right, up);
	if(right.MagnitudeSqr() <= 1.0e-8f)
		return false;
	right.Normalise();
	poseBody = visualBody;
	poseBody.GetRight() = right;
	poseBody.GetUp() = up;
	poseBody.GetForward() = CrossProduct(up, right);
	return true;
}

static void AlignCrouchAxes(RwMatrix &frame,
	const CMatrix &poseBody, const CMatrix &visualBody)
{
	frame.right = FromBasisLocal(ToBasisLocal(frame.right, poseBody), visualBody);
	frame.up = FromBasisLocal(ToBasisLocal(frame.up, poseBody), visualBody);
	frame.at = FromBasisLocal(ToBasisLocal(frame.at, poseBody), visualBody);
}

static void AlignCrouchedSkeleton(CPed *ped, RpHAnimHierarchy *hier,
	RwMatrix *mats, const CMatrix &visualBody)
{
	if(!ped->bIsDucking)
		return;
	const int leftHip = RpHAnimIDGetIndex(hier, BONE_upperlegl);
	const int rightHip = RpHAnimIDGetIndex(hier, BONE_upperlegr);
	if(leftHip < 0 || rightHip < 0 ||
	   leftHip >= hier->numNodes || rightHip >= hier->numNodes)
		return;
	CMatrix poseBody;
	if(!BuildCrouchPoseBasis(mats[leftHip].pos, mats[rightHip].pos,
	   visualBody, poseBody))
		return;
	const CVector root = visualBody.GetPosition();
	for(int i = 0; i < hier->numNodes; i++){
		RwMatrix &bone = mats[i];
		bone.pos = root + FromBasisLocal(
			ToBasisLocal(CVector(bone.pos)-root, poseBody), visualBody);
		AlignCrouchAxes(bone, poseBody, visualBody);
	}
}

static bool BuildGroundedVisualBody(CPed *ped,
	RpHAnimHierarchy *hier, RwMatrix *mats, CMatrix &visualBody)
{
	visualBody = ped->GetMatrix();
	const CVector nativeRoot = ped->GetPosition();
	visualBody.GetPosition() = nativeRoot;

	float eyePosRaw[3];
	float rightRaw[3], upRaw[3], forwardRaw[3];
	if(androidgame::VrGetViewBasis(
	       eyePosRaw, rightRaw, upRaw, forwardRaw)){
		CVector up = visualBody.GetUp();
		up = Unit(up, CVector(0.0f, 0.0f, 1.0f));

		CVector forward(forwardRaw[0], forwardRaw[1], forwardRaw[2]);
		forward -= up*DotProduct(forward, up);
		if(forward.MagnitudeSqr() > 1.0e-8f){
			forward.Normalise();

			CVector right = CrossProduct(forward, up);
			if(right.MagnitudeSqr() > 1.0e-8f){
				right.Normalise();

				const float yawCorrection = DEGTORAD(-6.0f);
				const float cy = cosf(yawCorrection);
				const float sy = sinf(yawCorrection);

				CVector correctedForward =
					forward*cy + right*sy;
				correctedForward -=
					up*DotProduct(correctedForward, up);
				correctedForward =
					Unit(correctedForward, forward);

				CVector correctedRight =
					CrossProduct(correctedForward, up);
				correctedRight =
					Unit(correctedRight, right);
				up = CrossProduct(
					correctedRight, correctedForward);
				up.Normalise();

				visualBody.GetRight() = correctedRight;
				visualBody.GetUp() = up;
				visualBody.GetForward() = correctedForward;
			}
		}

		float anchorRaw[3];
		if(androidgame::VrGetOnFootBodyAnchor(anchorRaw)){
			const CVector eye(
				eyePosRaw[0], eyePosRaw[1], eyePosRaw[2]);
			const CVector anchor(
				anchorRaw[0], anchorRaw[1], anchorRaw[2]);

			CVector delta = eye-anchor;
			delta.z = 0.0f;

			const float horizontal =
				sqrtf(delta.x*delta.x + delta.y*delta.y);

			if(horizontal > 0.0001f){
				const float maxFollow = 2.0f;
				const float scale = (horizontal > maxFollow) ? (maxFollow / horizontal) : 1.0f;
				const CVector follow(delta.x * scale, delta.y * scale, 0.0f);

				visualBody.GetPosition() = nativeRoot+follow;

				for(int i = 0; i < hier->numNodes; i++){
					mats[i].pos.x += follow.x;
					mats[i].pos.y += follow.y;
				}
			}
		}
	}

	AlignCrouchedSkeleton(ped, hier, mats, visualBody);
	return true;
}

static bool ResolveUpperBody(RpHAnimHierarchy *hier, UpperBodyBones &out)
{
	out.waist = RpHAnimIDGetIndex(hier, BONE_waist);
	out.mid = RpHAnimIDGetIndex(hier, BONE_mid);
	out.torso = RpHAnimIDGetIndex(hier, BONE_torso);
	out.head = RpHAnimIDGetIndex(hier, BONE_head);
	if(out.waist < 0)
		out.waist = 0;
	if(out.mid < 0 && out.torso >= 0)
		out.mid = out.torso;
	else if(out.mid < 0)
		out.mid = out.waist;
	if(out.torso < 0 && out.mid >= 0)
		out.torso = out.mid;
	else if(out.torso < 0)
		out.torso = out.waist;
	if(out.head < 0)
		out.head = RpHAnimIDGetIndex(hier, ConvertPedNode2BoneTag(PED_HEAD));
	return out.head >= 0 && out.head < hier->numNodes &&
		out.torso >= 0 && out.torso < hier->numNodes &&
		out.mid >= 0 && out.mid < hier->numNodes;
}

static bool StabilizeUpperBody(CPed *ped, RwMatrix *mats,
	const UpperBodyBones &b, const CMatrix &visualBody,
	float *authoredVerticalOffset)
{
	const CMatrix &nativeBody = ped->GetMatrix();
	const CVector nativeRoot = ped->GetPosition();
	const CVector visualRoot = visualBody.GetPosition();
	RwMatrix &mid = mats[b.mid];
	RwMatrix &torso = mats[b.torso];
	RwMatrix &head = mats[b.head];

	if(!gUpperBody.valid){
		gUpperBody.midPosLocal =
			ToBasisLocal(CVector(mid.pos)-visualRoot, visualBody);
		gUpperBody.torsoPosLocal =
			ToBasisLocal(CVector(torso.pos)-visualRoot, visualBody);
		gUpperBody.nativeTorsoPosLocal =
			ToBasisLocal(CVector(torso.pos)-nativeRoot, nativeBody);

		gUpperBody.midRightLocal = ToBasisLocal(mid.right, visualBody);
		gUpperBody.midUpLocal = ToBasisLocal(mid.up, visualBody);
		gUpperBody.midAtLocal = ToBasisLocal(mid.at, visualBody);

		gUpperBody.torsoRightLocal = ToBasisLocal(torso.right, visualBody);
		gUpperBody.torsoUpLocal = ToBasisLocal(torso.up, visualBody);
		gUpperBody.torsoAtLocal = ToBasisLocal(torso.at, visualBody);

		gUpperBody.headFromTorsoLocal =
			ToBasisLocal(CVector(head.pos)-CVector(torso.pos), nativeBody);
		gUpperBody.headRightLocal = ToBasisLocal(head.right, nativeBody);
		gUpperBody.headUpLocal = ToBasisLocal(head.up, nativeBody);
		gUpperBody.headAtLocal = ToBasisLocal(head.at, nativeBody);

		gUpperBody.valid = true;
	}

	const CVector currentNativeTorsoLocal =
		ToBasisLocal(CVector(torso.pos)-nativeRoot, nativeBody);
	const float nativeVerticalDelta =
		currentNativeTorsoLocal.y-gUpperBody.nativeTorsoPosLocal.y;
	const float crouchOffset =
		nativeVerticalDelta < -0.020f ?
			ClampF(nativeVerticalDelta+0.020f, -0.55f, 0.0f) : 0.0f;
	if(authoredVerticalOffset)
		*authoredVerticalOffset = crouchOffset;

	CVector midLocal = gUpperBody.midPosLocal;
	CVector torsoLocal = gUpperBody.torsoPosLocal;
	midLocal.y += crouchOffset;
	torsoLocal.y += crouchOffset;

	if(b.mid != b.torso){
		mid.pos = visualRoot + FromBasisLocal(midLocal, visualBody);
		mid.right = FromBasisLocal(gUpperBody.midRightLocal, visualBody);
		mid.up = FromBasisLocal(gUpperBody.midUpLocal, visualBody);
		mid.at = FromBasisLocal(gUpperBody.midAtLocal, visualBody);
		Orthonormalise(mid);
	}

	torso.pos = visualRoot +
		FromBasisLocal(torsoLocal, visualBody);

	CVector torsoRetractUp = visualBody.GetUp();
	torsoRetractUp = Unit(torsoRetractUp, CVector(0.0f, 0.0f, 1.0f));
	torso.pos = CVector(torso.pos) - torsoRetractUp*0.025f;

	torso.right = FromBasisLocal(gUpperBody.torsoRightLocal, visualBody);
	torso.up = FromBasisLocal(gUpperBody.torsoUpLocal, visualBody);
	torso.at = FromBasisLocal(gUpperBody.torsoAtLocal, visualBody);
	Orthonormalise(torso);

	head.pos = CVector(torso.pos) +
		FromBasisLocal(gUpperBody.headFromTorsoLocal, visualBody);
	head.right = FromBasisLocal(gUpperBody.headRightLocal, visualBody);
	head.up = FromBasisLocal(gUpperBody.headUpLocal, visualBody);
	head.at = FromBasisLocal(gUpperBody.headAtLocal, visualBody);
	Orthonormalise(head);

	return true;
}

static bool ResolveArm(RpHAnimHierarchy *hier, bool left, ArmBones &out)
{
	const int upper = left ? BONE_upperarml : BONE_upperarmr;
	const int forearm = left ? BONE_lowerarml : BONE_lowerarmr;
	const int hand = left ? BONE_Lhand : BONE_Rhand;
	out.upper = RpHAnimIDGetIndex(hier, upper);
	out.forearm = RpHAnimIDGetIndex(hier, forearm);
	out.hand = RpHAnimIDGetIndex(hier, hand);
	return out.upper >= 0 && out.forearm >= 0 && out.hand >= 0 &&
		out.upper < hier->numNodes && out.forearm < hier->numNodes &&
		out.hand < hier->numNodes;
}

static void StabilizeShoulderRoot(CPed *ped, RwMatrix *mats,
	const ArmBones &b, int handIndex, const CMatrix &visualBody,
	float authoredVerticalOffset)
{
	ShoulderAnchor &anchor = gShoulder[handIndex];
	const CMatrix &nativeBody = ped->GetMatrix();
	const CVector nativeRoot = ped->GetPosition();
	RwMatrix &upper = mats[b.upper];

	if(!anchor.valid){
		anchor.upperPosLocal =
			ToBasisLocal(CVector(upper.pos)-nativeRoot, nativeBody);
		anchor.valid = true;
	}

	const CVector root = visualBody.GetPosition();
	CVector upperLocal = anchor.upperPosLocal;
	upperLocal.y += authoredVerticalOffset;

	upper.pos = root +
		FromBasisLocal(upperLocal, visualBody);
}

static void BuildParentTable(RpHAnimHierarchy *hier, int *parentOf)
{
	int stack[96];
	int sp = 0;
	int parent = -1;
	stack[sp++] = parent;
	for(int i = 0; i < hier->numNodes; i++){
		parentOf[i] = parent;
		const int flags = hier->nodeInfo[i].flags;
		if((flags & rw::HAnimHierarchy::PUSH) && sp < 96)
			stack[sp++] = parent;
		parent = i;
		if((flags & rw::HAnimHierarchy::POP) && sp > 0)
			parent = stack[--sp];
	}
}

static bool IsDescendantOf(int node, int ancestor, const int *parentOf)
{
	for(int p = parentOf[node]; p >= 0; p = parentOf[p])
		if(p == ancestor)
			return true;
	return false;
}

static void RotateHandBasis(RwMatrix &m, const CVector &axis, float radians);

static CMatrix
MatrixFromRw(const RwMatrix &m)
{
	CMatrix out;
	out.GetPosition() = CVector(m.pos);
	out.GetRight() = CVector(m.right);
	out.GetUp() = CVector(m.up);
	out.GetForward() = CVector(m.at);
	OrthonormaliseBasis(out);
	return out;
}

static bool
CaptureStaticHandPose(RpHAnimHierarchy *hier, RwMatrix *mats,
	int handNode, int handIndex, const int *parentOf)
{
	if(handIndex < 0 || handIndex > 1 || hier->numNodes <= 0 ||
	   hier->numNodes > MAX_BODY_IK_NODES)
		return false;

	HandDescendantPose &pose = gHandPose[handIndex];
	if(pose.valid)
		return true;

	for(int i = 0; i < MAX_BODY_IK_NODES; i++)
		pose.nodeValid[i] = false;

	for(int i = 0; i < hier->numNodes; i++){
		if(i == handNode || !IsDescendantOf(i, handNode, parentOf))
			continue;

		const int parent = parentOf[i];
		if(parent < 0 || parent >= hier->numNodes)
			continue;

		const CMatrix parentBasis = MatrixFromRw(mats[parent]);
		pose.posLocal[i] = ToBasisLocal(
			CVector(mats[i].pos)-CVector(mats[parent].pos), parentBasis);
		pose.rightLocal[i] =
			ToBasisLocal(CVector(mats[i].right), parentBasis);
		pose.upLocal[i] =
			ToBasisLocal(CVector(mats[i].up), parentBasis);
		pose.atLocal[i] =
			ToBasisLocal(CVector(mats[i].at), parentBasis);
		pose.nodeValid[i] = true;
	}

	pose.valid = true;
	return true;
}

static void
ApplyStaticGripHandPose(RpHAnimHierarchy *hier, RwMatrix *mats,
	int handNode, int handIndex, const int *parentOf,
	const CVector &physicalCurlAxis, float gripAmount)
{
	if(handIndex < 0 || handIndex > 1 ||
	   !gHandPose[handIndex].valid ||
	   hier->numNodes <= 0 || hier->numNodes > MAX_BODY_IK_NODES)
		return;

	HandDescendantPose &pose = gHandPose[handIndex];

	const float grip = ClampF((gripAmount-0.12f)/0.58f, 0.0f, 1.0f);
	const CVector curlAxis = Unit(
		physicalCurlAxis, CVector(1.0f, 0.0f, 0.0f));

	for(int i = 0; i < hier->numNodes; i++){
		if(!pose.nodeValid[i] ||
		   i == handNode || !IsDescendantOf(i, handNode, parentOf))
			continue;

		const int parent = parentOf[i];
		if(parent < 0 || parent >= hier->numNodes)
			continue;

		const CMatrix parentBasis = MatrixFromRw(mats[parent]);

		mats[i].pos = CVector(mats[parent].pos) +
			FromBasisLocal(pose.posLocal[i], parentBasis);
		mats[i].right =
			FromBasisLocal(pose.rightLocal[i], parentBasis);
		mats[i].up =
			FromBasisLocal(pose.upLocal[i], parentBasis);
		mats[i].at =
			FromBasisLocal(pose.atLocal[i], parentBasis);
		mats[i].flags = 0;
		Orthonormalise(mats[i]);

		const float basePoseDeg =
			ResolveHandPoseDeg(handIndex, grip);

		const float handMirror =
			(handIndex == 0) ? -1.0f : 1.0f;
		const float poseDeg =
			basePoseDeg*handMirror;

		if(fabsf(poseDeg) > 0.001f)
			RotateHandBasis(
				mats[i], curlAxis,
				DEGTORAD(poseDeg));
	}
}

static bool GetFloatingHandPresentationPose(int handIndex, CMatrix &pose)
{
	CMatrix grip, aim;
	if(!OculusVR::GetQuestRawTrackedHandMatrix(handIndex, &grip) ||
	   !OculusVR::GetQuestRawTrackedHandAimMatrix(handIndex, &aim))
		return false;

	CVector forward = aim.GetForward();
	if(forward.MagnitudeSqr() < 1.0e-8f)
		forward = grip.GetForward();
	if(forward.MagnitudeSqr() < 1.0e-8f)
		return false;
	forward.Normalise();

	const int modelHand = 1-handIndex;
	CVector up =
		grip.GetRight()*(modelHand == 0 ? 1.0f : -1.0f);
	up -= forward*DotProduct(up, forward);
	if(up.MagnitudeSqr() < 1.0e-8f)
		up = grip.GetUp();
	up.Normalise();

	CVector right = CrossProduct(up, forward);
	if(right.MagnitudeSqr() < 1.0e-8f)
		return false;
	right.Normalise();
	if(DotProduct(right, grip.GetForward()) < 0.0f)
		right *= -1.0f;

	pose.GetPosition() = grip.GetPosition();
	pose.GetRight() = right;
	pose.GetUp() = up;
	pose.GetForward() = forward;
	OrthonormaliseBasis(pose);
	return true;
}

static bool GetVehicleHandPresentationPose(int handIndex, CMatrix &pose)
{
	if(!OculusVR::IsImmersiveSteeringHandleGrabbed(handIndex))
		return GetFloatingHandPresentationPose(handIndex, pose);

	CMatrix vehicleTarget;
	float grip = 0.0f, trigger = 0.0f;
	if(!OculusVR::GetTrackedVisualHandMatrix(
	   handIndex, &vehicleTarget, &grip, &trigger))
		return false;

	CMatrix onFootPose;
	if(!GetFloatingHandPresentationPose(handIndex, onFootPose))
		return false;

	pose = onFootPose;

	CVector steerAxis;
	float steerAngle = 0.0f;
	if(OculusVR::GetQuestVehicleHandSteerRotation(
	   handIndex, &steerAxis, &steerAngle) &&
	   steerAxis.MagnitudeSqr() > 1.0e-8f &&
	   fabsf(steerAngle) > 1.0e-6f){
		steerAxis.Normalise();
		pose.GetRight() = RotateAroundAxis(
			pose.GetRight(), steerAxis, steerAngle);
		pose.GetUp() = RotateAroundAxis(
			pose.GetUp(), steerAxis, steerAngle);
		pose.GetForward() = RotateAroundAxis(
			pose.GetForward(), steerAxis, steerAngle);
	}

	pose.GetPosition() = vehicleTarget.GetPosition();
	OrthonormaliseBasis(pose);
	return true;
}

static void RotateHandBasis(RwMatrix &hand, const CVector &axis, float angle)
{
	CVector n = Unit(axis, CVector(1.0f, 0.0f, 0.0f));
	hand.right = RotateAroundAxis(hand.right, n, angle);
	hand.up = RotateAroundAxis(hand.up, n, angle);
	hand.at = RotateAroundAxis(hand.at, n, angle);
	Orthonormalise(hand);
}

static CMatrix BuildDefaultHandPose(int handIndex, const CMatrix &tracked)
{
	CMatrix out;
	out.GetPosition() = tracked.GetPosition();

	CVector finger = Unit(tracked.GetForward(), CVector(0.0f, 1.0f, 0.0f));
	CVector palm = Unit(tracked.GetUp(), CVector(0.0f, 0.0f, 1.0f));
	if(handIndex == 1)
		palm *= -1.0f;
	palm -= finger*DotProduct(palm, finger);
	palm = Unit(palm, CVector(handIndex == 0 ? 1.0f : -1.0f, 0.0f, 0.0f));
	CVector across = Unit(CrossProduct(palm, finger), tracked.GetRight());

	out.GetRight() = across;
	out.GetUp() = palm;
	out.GetForward() = finger;
	OrthonormaliseBasis(out);

	if(handIndex == 0){
		RwMatrix tmp;
		tmp.right = out.GetRight();
		tmp.up = out.GetUp();
		tmp.at = out.GetForward();
		tmp.pos = out.GetPosition();
		RotateHandBasis(tmp, tracked.GetUp(), DEGTORAD(180.0f));
		out.GetRight() = tmp.right;
		out.GetUp() = tmp.up;
		out.GetForward() = tmp.at;
	}
	return out;
}

static bool CaptureArmSolveAnchorExplicit(const CMatrix &nativeBody,
	const RwMatrix &upperMat, const RwMatrix &forearmMat, const RwMatrix &handMat,
	int handIndex)
{
	ArmSolveAnchor &cal = gArmSolve[handIndex];
	if(cal.valid)
		return true;

	const CVector shoulder = upperMat.pos;
	const CVector elbow = forearmMat.pos;
	const CVector wrist = handMat.pos;
	const CVector upperDir = elbow-shoulder;
	const CVector lowerDir = wrist-elbow;

	cal.upperLen = upperDir.Magnitude();
	cal.lowerLen = lowerDir.Magnitude();
	if(cal.upperLen < 0.03f || cal.lowerLen < 0.03f){
		cal.upperLen = 0.281f;
		cal.lowerLen = 0.280f;
	}

	cal.upperDirLocal = ToBasisLocal(upperDir, nativeBody);
	cal.lowerDirLocal = ToBasisLocal(lowerDir, nativeBody);
	cal.elbowOffsetLocal = ToBasisLocal(elbow-shoulder, nativeBody);

	cal.upperRightLocal =
		ToBasisLocal(upperMat.right, nativeBody);
	cal.upperUpLocal =
		ToBasisLocal(upperMat.up, nativeBody);
	cal.upperAtLocal =
		ToBasisLocal(upperMat.at, nativeBody);

	cal.foreRightLocal =
		ToBasisLocal(forearmMat.right, nativeBody);
	cal.foreUpLocal =
		ToBasisLocal(forearmMat.up, nativeBody);
	cal.foreAtLocal =
		ToBasisLocal(forearmMat.at, nativeBody);

	cal.valid = true;
	return true;
}

static bool CaptureArmSolveAnchor(CPed *ped, RwMatrix *mats,
	const ArmBones &b, int handIndex)
{
	return CaptureArmSolveAnchorExplicit(ped->GetMatrix(),
		mats[b.upper], mats[b.forearm], mats[b.hand], handIndex);
}

static void
StretchBoneAlongWorldDirection(RwMatrix &bone, const CVector &worldDirection,
	float stretch)
{
	if(stretch <= 1.0001f || worldDirection.MagnitudeSqr() <= 1.0e-8f)
		return;

	CVector axis = worldDirection;
	axis.Normalise();
	const float extra = stretch-1.0f;

	CVector r = bone.right;
	CVector u = bone.up;
	CVector a = bone.at;

	r += axis * (DotProduct(r,axis)*extra);
	u += axis * (DotProduct(u,axis)*extra);
	a += axis * (DotProduct(a,axis)*extra);

	bone.right = r;
	bone.up = u;
	bone.at = a;
}

static bool SolveArmMatrices(
	RwMatrix &upper, RwMatrix &forearm, RwMatrix &hand,
	int handIndex,
	const CMatrix &tracked, const CMatrix &body,
	const CVector &physicalPitchAxis, bool pitchWeaponHand,
	bool preserveWeaponContact)
{
	ArmSolveAnchor &cal = gArmSolve[handIndex];
	const CVector shoulder = upper.pos;
	if(!cal.valid)
		return false;
	const CVector target = tracked.GetPosition();
	CVector delta = target-shoulder;
	float targetDistance = delta.Magnitude();
	if(targetDistance < 0.02f) return false;
	const CVector aim = delta*(1.0f/targetDistance);

	const float naturalReach = cal.upperLen+cal.lowerLen;
	const float maxForearmStretch =
		preserveWeaponContact ? 2.25f : 1.35f;
	const float maxExtraForearm =
		cal.lowerLen*(maxForearmStretch-1.0f);
	const float requestedExtra =
		Max(0.0f,targetDistance-naturalReach);
	const float extraForearm =
		Min(requestedExtra,maxExtraForearm);
	const float solvedLowerLen =
		cal.lowerLen+extraForearm;
	const float forearmStretch =
		cal.lowerLen > 1.0e-5f ?
			solvedLowerLen/cal.lowerLen : 1.0f;

	const float maxReach = cal.upperLen+solvedLowerLen;
	const float minReach =
		Max(0.02f,Abs(cal.upperLen-solvedLowerLen)+0.005f);
	const float d = ClampF(targetDistance,minReach,maxReach);
	const CVector wrist = shoulder+aim*d;
	CVector hint = FromBasisLocal(cal.elbowOffsetLocal, body);
	hint -= aim*DotProduct(hint,aim);
	if(hint.MagnitudeSqr() < 1.0e-7f){
		const CVector side = handIndex == 0 ? CVector(-1.0f,0.0f,0.0f) : CVector(1.0f,0.0f,0.0f);
		hint = FromBasisLocal(side,body);
		hint -= aim*DotProduct(hint,aim);
	}
	hint = Unit(hint,CVector(0.0f,0.0f,1.0f));
	const float aa=cal.upperLen, cc=solvedLowerLen;
	const float along=ClampF((aa*aa-cc*cc+d*d)/(2.0f*d),0.0f,aa);
	const CVector elbow=shoulder+aim*along+hint*sqrtf(Max(0.0f,aa*aa-along*along));
	const CVector refUpper=FromBasisLocal(cal.upperDirLocal,body);
	const CVector refLower=FromBasisLocal(cal.lowerDirLocal,body);
	const CVector newUpper=elbow-shoulder;
	const CVector newLower=wrist-elbow;
	const CVector uR=FromBasisLocal(cal.upperRightLocal,body), uU=FromBasisLocal(cal.upperUpLocal,body), uA=FromBasisLocal(cal.upperAtLocal,body);
	upper.right=SwingVector(refUpper,newUpper,uR,uU);
	upper.up=SwingVector(refUpper,newUpper,uU,uR);
	upper.at=SwingVector(refUpper,newUpper,uA,uR);
	upper.pos=shoulder;
	Orthonormalise(upper);
	const CVector fR=FromBasisLocal(cal.foreRightLocal,body), fU=FromBasisLocal(cal.foreUpLocal,body), fA=FromBasisLocal(cal.foreAtLocal,body);
	forearm.right=SwingVector(refLower,newLower,fR,fU);
	forearm.up=SwingVector(refLower,newLower,fU,fR);
	forearm.at=SwingVector(refLower,newLower,fA,fR);
	forearm.pos=elbow;
	Orthonormalise(forearm);

	StretchBoneAlongWorldDirection(
		forearm,newLower,forearmStretch);

	hand.right=tracked.GetRight();
	hand.up=tracked.GetUp();
	hand.at=tracked.GetForward();

	hand.pos =
		(preserveWeaponContact ||
		 targetDistance <= maxReach+0.0001f) ?
			tracked.GetPosition() : wrist;
	Orthonormalise(hand);
	(void)pitchWeaponHand;
	(void)physicalPitchAxis;
	return true;
}

static bool SolveArm(RpHAnimHierarchy *hier, RwMatrix *mats,
	const int *parentOf, const ArmBones &b, int handIndex,
	const CMatrix &tracked, const CMatrix &body,
	const CVector &physicalPitchAxis, bool pitchWeaponHand,
	float gripAmount, bool preserveWeaponContact)
{
	CaptureStaticHandPose(hier, mats, b.hand, handIndex, parentOf);

	if(!SolveArmMatrices(mats[b.upper], mats[b.forearm], mats[b.hand],
		handIndex, tracked, body, physicalPitchAxis, pitchWeaponHand,
		preserveWeaponContact))
		return false;

	ApplyStaticGripHandPose(
		hier, mats, b.hand, handIndex, parentOf,
		physicalPitchAxis, gripAmount);
	return true;
}
}

void Reset(void)
{
	gWrist[0] = WristCalibration();
	gWrist[1] = WristCalibration();
	gShoulder[0] = ShoulderAnchor();
	gShoulder[1] = ShoulderAnchor();
	gUpperBody = UpperBodyAnchor();
	gArmSolve[0] = ArmSolveAnchor();
	gArmSolve[1] = ArmSolveAnchor();
	gRigidWeaponLink[0] = RigidWeaponHandLink();
	gRigidWeaponLink[1] = RigidWeaponHandLink();
	gSolvedHandValid[0] = false;
	gSolvedHandValid[1] = false;
	gVehicleNeckValid = false;
	gVehicleNeckVehicle = nil;
	gBikeBodyVehicle = nil;
	gBikeBodyRootValid = false;
	gBikeBodyLateralValid = false;
	gBikeBodyLateral = 0.0f;
	gBikeBodyLateralLast = 0.0f;
	gBikeBodyLateralAccum = 0.0f;
	gBikeBodyLateralStableFrames = 0;
	gHandPose[0] = HandDescendantPose();
	gHandPose[1] = HandDescendantPose();
	gLastPlayer = nil;
}

void ResetVehicleTransition(void)
{
	for(int i = 0; i < 2; i++){
		gWrist[i].frozen = false;
		gWrist[i].clickDown = false;
		gWrist[i].lastPoseValid = false;
		gSolvedHandValid[i] = false;
		gRigidWeaponLink[i] = RigidWeaponHandLink();
	}

	gVehicleNeckValid = false;
	gVehicleNeckVehicle = nil;

	gBikeBodyVehicle = nil;
	gBikeBodyRootValid = false;
	gBikeBodyLateralValid = false;
	gBikeBodyLateral = 0.0f;
	gBikeBodyLateralLast = 0.0f;
	gBikeBodyLateralAccum = 0.0f;
	gBikeBodyLateralStableFrames = 0;

	IKCALLOG("[IK] vehicle transition: transient ownership cleared; anatomy preserved");
}

static void EnsureBakedCalibration(void)
{
	if(!gWrist[0].valid){
		gWrist[0].valid = true;
		gWrist[0].positionLocal = CVector(0.0f, 0.0f, -0.045f);
		gWrist[0].rightLocal = CVector(0.255107f, -0.086611f, 0.963026f);
		gWrist[0].upLocal = CVector(-0.047421f, -0.995902f, -0.077006f);
		gWrist[0].atLocal = CVector(-0.965749f, 0.026023f, 0.258169f);
	}
	if(!gWrist[1].valid){
		gWrist[1].valid = true;
		gWrist[1].positionLocal = CVector(0.0f, 0.0f, -0.045f);
		gWrist[1].rightLocal = CVector(0.006246f, 0.394067f, 0.919061f);
		gWrist[1].upLocal = CVector(-0.124347f, -0.911639f, 0.391730f);
		gWrist[1].atLocal = CVector(0.992219f, -0.116729f, 0.043307f);
	}
}

bool
GetSolvedHandWorldBasis(int hand, CVector *position, CVector *right,
	CVector *up, CVector *forward)
{
#if !defined(__ANDROID__) || !defined(GTA_VR_WEAPONS)
	(void)hand;
	(void)position;
	(void)right;
	(void)up;
	(void)forward;
	return false;
#else
	if(hand < 0 || hand > 1 || !position || !right || !up || !forward ||
	   !WeaponOwnershipActive() || !gSolvedHandValid[hand])
		return false;

	const CMatrix &wrist = gSolvedHandWorld[hand];
	*position = wrist.GetPosition();
	*right = wrist.GetRight();
	*up = wrist.GetUp();
	*forward = wrist.GetForward();

	if(right->MagnitudeSqr() <= 1.0e-8f ||
	   up->MagnitudeSqr() <= 1.0e-8f ||
	   forward->MagnitudeSqr() <= 1.0e-8f)
		return false;
	right->Normalise();
	up->Normalise();
	forward->Normalise();
	return true;
#endif
}

bool
GetVehicleNeckWorld(CPed *ped, CVector *position)
{
#if !defined(__ANDROID__) || !defined(GTA_VR_WEAPONS)
	(void)ped;
	(void)position;
	return false;
#else
	if(!position || ped == nil || !BodyIKGameplayContext() || !VehicleEnabled() ||
	   !ped->InVehicle() || ped->m_pMyVehicle == nil ||
	   !ped->m_pMyVehicle->IsBike() ||
	   !gVehicleNeckValid ||
	   gVehicleNeckVehicle != ped->m_pMyVehicle)
		return false;

	*position = gVehicleNeckWorld;
	return true;
#endif
}

bool
GetRigidWeaponHandBasis(int hand, CVector *position, CVector *right,
	CVector *up, CVector *forward)
{
#if !defined(__ANDROID__) || !defined(GTA_VR_WEAPONS)
	(void)hand;
	(void)position;
	(void)right;
	(void)up;
	(void)forward;
	return false;
#else
	if(!WeaponOwnershipActive())
		return false;
	return RebuildRigidWeaponBasis(hand, position, right, up, forward);
#endif
}

#if defined(__ANDROID__) && defined(GTA_VR_WEAPONS)
static void ApplySkinned(CPed *ped, bool onFootIK, bool vehicleIK)
{
	(void)onFootIK;
	RpHAnimHierarchy *hier =
		GetAnimHierarchyFromSkinClump(ped->GetClump());
	if(hier == nil)
		return;
	RwMatrix *mats = RpHAnimHierarchyGetMatrixArray(hier);
	if(mats == nil)
		return;

	UpperBodyBones upperBody;
	if(!ResolveUpperBody(hier, upperBody))
		return;

	ArmBones arms[2];
	if(!ResolveArm(hier, true, arms[0]) ||
	   !ResolveArm(hier, false, arms[1]))
		return;

	if(!CaptureArmSolveAnchor(ped, mats, arms[0], 0) ||
	   !CaptureArmSolveAnchor(ped, mats, arms[1], 1))
		return;

	CMatrix visualBody = ped->GetMatrix();
	if(!vehicleIK){
		if(!BuildGroundedVisualBody(ped, hier, mats, visualBody))
			return;

		if(!ped->bIsDucking){
			float authoredVerticalOffset = 0.0f;
			if(!StabilizeUpperBody(
			   ped, mats, upperBody, visualBody, &authoredVerticalOffset))
				return;

			StabilizeShoulderRoot(
				ped, mats, arms[0], 0, visualBody, authoredVerticalOffset);
			StabilizeShoulderRoot(
				ped, mats, arms[1], 1, visualBody, authoredVerticalOffset);
		}
	}else if(ped->m_pMyVehicle != nil &&
	         ped->m_pMyVehicle->IsBike()){
		const CMatrix nativeBody = ped->GetMatrix();
		const CVector nativeRoot = ped->GetPosition();
		CVehicle *bike = ped->m_pMyVehicle;

		if(gBikeBodyVehicle != bike){
			gBikeBodyVehicle = bike;
			gBikeBodyRootValid = false;
			gBikeBodyLateralValid = false;
			gBikeBodyLateral = 0.0f;
			gBikeBodyLateralLast = 0.0f;
			gBikeBodyLateralAccum = 0.0f;
			gBikeBodyLateralStableFrames = 0;
		}

		if(!gBikeBodyRootValid){
			const CVector rootDelta = nativeRoot-bike->GetPosition();
			gBikeBodyRootLocal = CVector(
				DotProduct(rootDelta, bike->GetRight()),
				DotProduct(rootDelta, bike->GetUp()),
				DotProduct(rootDelta, bike->GetForward()));
			gBikeBodyRootValid = true;
		}

		const CVector currentRootDelta =
			nativeRoot-bike->GetPosition();
		const float currentLateral =
			DotProduct(currentRootDelta, bike->GetRight());

		if(!gBikeBodyLateralValid){
			const float lateralDelta =
				Abs(currentLateral-gBikeBodyLateralLast);

			if(Abs(currentLateral) < 0.60f &&
			   (gBikeBodyLateralStableFrames == 0 ||
			    lateralDelta < 0.015f)){
				if(gBikeBodyLateralStableFrames == 0)
					gBikeBodyLateralAccum = 0.0f;

				gBikeBodyLateralAccum += currentLateral;
				gBikeBodyLateralStableFrames++;
				gBikeBodyLateralLast = currentLateral;

				if(gBikeBodyLateralStableFrames >= 12){
					gBikeBodyLateral =
						gBikeBodyLateralAccum /
						(float)gBikeBodyLateralStableFrames;
					gBikeBodyLateralValid = true;
				}
			}else{
				gBikeBodyLateralStableFrames = 0;
				gBikeBodyLateralAccum = 0.0f;
				gBikeBodyLateralLast = currentLateral;
			}
		}

		const float stableLateral =
			gBikeBodyLateralValid ?
				gBikeBodyLateral : currentLateral;

		const CVector stableRoot =
			bike->GetPosition()+
			bike->GetRight()*stableLateral+
			bike->GetUp()*gBikeBodyRootLocal.y+
			bike->GetForward()*gBikeBodyRootLocal.z;

		visualBody = nativeBody;
		visualBody.GetPosition() = stableRoot;

		if(BuildBikeCarrierBasis(bike, nativeBody, visualBody)){
			for(int i = 0; i < hier->numNodes; i++){
				const CVector localPos =
					ToBasisLocal(CVector(mats[i].pos)-nativeRoot,
					             nativeBody);
				mats[i].pos =
					stableRoot + FromBasisLocal(localPos, visualBody);

				const CVector localRight =
					ToBasisLocal(CVector(mats[i].right), nativeBody);
				const CVector localUp =
					ToBasisLocal(CVector(mats[i].up), nativeBody);
				const CVector localAt =
					ToBasisLocal(CVector(mats[i].at), nativeBody);

				mats[i].right =
					FromBasisLocal(localRight, visualBody);
				mats[i].up =
					FromBasisLocal(localUp, visualBody);
				mats[i].at =
					FromBasisLocal(localAt, visualBody);
				Orthonormalise(mats[i]);
			}

			gVehicleNeckWorld =
				CVector(mats[upperBody.torso].pos);
			gVehicleNeckVehicle = bike;
			gVehicleNeckValid = true;
		}
	}else{
		gVehicleNeckValid = false;
		gVehicleNeckVehicle = nil;
		gBikeBodyVehicle = nil;
		gBikeBodyRootValid = false;
		gBikeBodyLateralValid = false;
		gBikeBodyLateralStableFrames = 0;
		gBikeBodyLateralAccum = 0.0f;
	}

	// Publish the vehicle anchor after the body/neck pose is ready, then
	// convert the controller sample once into that same frame. Carrying this
	// target again by the previous body delta makes free hands jump by speed*dt
	// (and drops that offset abruptly at the old two-metre carry cutoff).
	if(vehicleIK)
		::VrUpdateFirstPersonAnchor(false);
	OculusVR::RefreshQuestTrackedHandWorldPosesForRender();

	CMatrix desiredWorld[2];
	bool desiredValid[2] = { false, false };
	bool weaponSupportLocked[2] = { false, false };
	CVector physicalPitchAxis[2];
	bool physicalPitchAxisValid[2] = { false, false };

	const androidgame::PadInput &ikInput = androidgame::GetPadInput();
	const float handGrip[2] = {
		ClampF(ikInput.leftGrip, 0.0f, 1.0f),
		ClampF(ikInput.rightGrip, 0.0f, 1.0f)
	};

	for(int handIndex = 0; handIndex < 2; handIndex++){
		WristCalibration &cal = gWrist[handIndex];

		CMatrix raw;
		if(vehicleIK){
			if(!GetVehicleHandPresentationPose(handIndex, raw))
				continue;
		}else if(!GetFloatingHandPresentationPose(handIndex, raw))
			continue;

		weaponSupportLocked[handIndex] =
			OculusVR::ApplyTrackedWeaponSupportHandLock(
				handIndex, &raw);

		physicalPitchAxis[handIndex] = raw.GetRight();
		if(physicalPitchAxis[handIndex].MagnitudeSqr() > 1.0e-8f){
			physicalPitchAxis[handIndex].Normalise();
			physicalPitchAxisValid[handIndex] = true;
		}

		if(cal.valid)
			desiredWorld[handIndex] = ApplyCalibration(raw, cal);
		else
			desiredWorld[handIndex] =
				BuildDefaultHandPose(handIndex, raw);

		CaptureRigidWeaponLink(
			handIndex, desiredWorld[handIndex], raw);

		cal.lastPose = desiredWorld[handIndex];
		cal.lastPoseValid = true;
		desiredValid[handIndex] = true;
	}

	if(hier->numNodes <= 0 || hier->numNodes > 96)
		return;
	int parentOf[96];
	BuildParentTable(hier, parentOf);

	int solveOrder[2] = { 0, 1 };
	if(OculusVR::GetHeldWeaponSlot(1) >= 0 &&
	   OculusVR::GetHeldWeaponSlot(0) < 0){
		solveOrder[0] = 1;
		solveOrder[1] = 0;
	}
	for(int solveIndex = 0; solveIndex < 2; solveIndex++){
		const int handIndex = solveOrder[solveIndex];
		if(!desiredValid[handIndex])
			continue;

		if(weaponSupportLocked[handIndex] &&
		   OculusVR::GetHeldWeaponSlot(handIndex) < 0){
			CMatrix currentSupportPose;
			if(GetFloatingHandPresentationPose(handIndex, currentSupportPose) &&
			   OculusVR::ApplyTrackedWeaponSupportHandLock(
				   handIndex, &currentSupportPose, true)){
				WristCalibration &cal = gWrist[handIndex];
				if(cal.valid)
					desiredWorld[handIndex] =
						ApplyCalibration(currentSupportPose, cal);
				else
					desiredWorld[handIndex] =
						BuildDefaultHandPose(handIndex, currentSupportPose);
				CaptureRigidWeaponLink(
					handIndex, desiredWorld[handIndex], currentSupportPose);
			}
		}

		const bool pitchWeaponHand =
			physicalPitchAxisValid[handIndex] &&
			OculusVR::GetHeldWeaponSlot(handIndex) >= 0;

		if(SolveArm(hier, mats, parentOf,
		   arms[handIndex], handIndex,
		   desiredWorld[handIndex], visualBody,
		   physicalPitchAxis[handIndex], pitchWeaponHand,
		   handGrip[handIndex], weaponSupportLocked[handIndex])){
			RwMatrix &finalHand = mats[arms[handIndex].hand];
			gSolvedHandWorld[handIndex].GetPosition() =
				CVector(finalHand.pos);
			gSolvedHandWorld[handIndex].GetRight() =
				CVector(finalHand.right);
			gSolvedHandWorld[handIndex].GetUp() =
				CVector(finalHand.up);
			gSolvedHandWorld[handIndex].GetForward() =
				CVector(finalHand.at);
			OrthonormaliseBasis(gSolvedHandWorld[handIndex]);
			gSolvedHandValid[handIndex] = true;
		}else{
			gSolvedHandValid[handIndex] = false;
		}
	}
}

static void ApplyNonSkinned(CPed *ped, bool onFootIK, bool vehicleIK)
{
	(void)onFootIK;
	// Capture arm solve anchors from the native animated hierarchy before modifying frames
	for(int handIndex = 0; handIndex < 2; handIndex++){
		RwFrame *upperFrame = (handIndex == 0) ?
			(ped->m_pFrames[PED_UPPERARML] ? ped->m_pFrames[PED_UPPERARML]->frame : nil) :
			(ped->m_pFrames[PED_UPPERARMR] ? ped->m_pFrames[PED_UPPERARMR]->frame : nil);
		RwFrame *handFrame = (handIndex == 0) ?
			(ped->m_pFrames[PED_HANDL] ? ped->m_pFrames[PED_HANDL]->frame : nil) :
			(ped->m_pFrames[PED_HANDR] ? ped->m_pFrames[PED_HANDR]->frame : nil);
		if(!upperFrame || !handFrame)
			continue;
		RwFrame *forearmFrame = RwFrameGetParent(handFrame);
		if(!forearmFrame)
			continue;

		CaptureArmSolveAnchorExplicit(
			ped->GetMatrix(),
			*RwFrameGetLTM(upperFrame),
			*RwFrameGetLTM(forearmFrame),
			*RwFrameGetLTM(handFrame),
			handIndex);
	}

	CMatrix visualBody = ped->GetMatrix();
	const CVector nativeRoot = ped->GetPosition();
	CVector baseRoot = nativeRoot;
	if(gVehicleTransitionActive && ped->m_pFrames[PED_MID] != nil &&
	   ped->m_pFrames[PED_MID]->frame != nil && ped->GetClump() != nil){
		RwFrame *waistFrame = RwFrameGetParent(ped->m_pFrames[PED_MID]->frame);
		if(waistFrame != nil && waistFrame != RpClumpGetFrame(ped->GetClump())){
			const RwV3d &waistLocal = RwFrameGetMatrix(waistFrame)->pos;
			baseRoot = ped->GetMatrix() * CVector(waistLocal.x, waistLocal.y, waistLocal.z);
		}
	}
	visualBody.GetPosition() = baseRoot;

	if(!vehicleIK){
		CVector follow(0.0f, 0.0f, 0.0f);
		float eyePosRaw[3];
		float forwardRaw[3];
		float rightRaw[3];
		float upRaw[3];
		if(androidgame::VrGetViewBasis(eyePosRaw, rightRaw, upRaw, forwardRaw)){
			CVector up = visualBody.GetUp();
			up = Unit(up, CVector(0.0f, 0.0f, 1.0f));

			CVector forward(forwardRaw[0], forwardRaw[1], forwardRaw[2]);
			forward -= up * DotProduct(forward, up);
			if(forward.MagnitudeSqr() > 1.0e-8f){
				forward.Normalise();

				CVector right = CrossProduct(forward, up);
				if(right.MagnitudeSqr() > 1.0e-8f){
					right.Normalise();

					const float yawCorrection = DEGTORAD(-6.0f);
					const float cy = cosf(yawCorrection);
					const float sy = sinf(yawCorrection);

					CVector correctedForward = forward * cy + right * sy;
					correctedForward -= up * DotProduct(correctedForward, up);
					correctedForward = Unit(correctedForward, forward);

					CVector correctedRight = CrossProduct(correctedForward, up);
					correctedRight = Unit(correctedRight, right);
					up = CrossProduct(correctedRight, correctedForward);
					up.Normalise();

					visualBody.GetRight() = correctedRight;
					visualBody.GetUp() = up;
					visualBody.GetForward() = correctedForward;
				}
			}

			float anchorRaw[3];
			if(androidgame::VrGetOnFootBodyAnchor(anchorRaw)){
				const CVector eye(eyePosRaw[0], eyePosRaw[1], eyePosRaw[2]);
				const CVector anchor(anchorRaw[0], anchorRaw[1], anchorRaw[2]);
				CVector delta = eye - anchor;
				delta.z = 0.0f;

				const float horizontal = sqrtf(delta.x*delta.x + delta.y*delta.y);
				if(horizontal > 0.0001f){
					const float maxFollow = 2.0f;
					const float scale = (horizontal > maxFollow) ? (maxFollow / horizontal) : 1.0f;
					follow = CVector(delta.x * scale, delta.y * scale, 0.0f);
				}
			}
		}

		visualBody.GetPosition() = baseRoot + follow;

		// Offset root clump frame in roomscale
		RwFrame *clumpFrame = RpClumpGetFrame(ped->GetClump());
		if(clumpFrame){
			if(ped->bIsDucking){
				RwFrame *leftHip = ped->m_pFrames[PED_UPPERLEGL] ?
					ped->m_pFrames[PED_UPPERLEGL]->frame : nil;
				RwFrame *rightHip = ped->m_pFrames[PED_UPPERLEGR] ?
					ped->m_pFrames[PED_UPPERLEGR]->frame : nil;
				CMatrix poseBody;
				if(leftHip && rightHip && BuildCrouchPoseBasis(
				   RwFrameGetLTM(leftHip)->pos, RwFrameGetLTM(rightHip)->pos,
				   visualBody, poseBody)){
					RwMatrix *root = RwFrameGetMatrix(clumpFrame);
					AlignCrouchAxes(*root, poseBody, visualBody);
				}
			}
			RwFrameGetMatrix(clumpFrame)->pos = nativeRoot + follow;
			RwFrameUpdateObjects(clumpFrame);
		}

		// Stabilize upper body (Smid and Storso) upright to visualBody so running/walking
		// animations do not lean the torso or neck forward into the player's view
		if(!ped->bIsDucking){
			const CVector visualRoot = visualBody.GetPosition();
			RwFrame *midFrame = ped->m_pFrames[PED_MID] ? ped->m_pFrames[PED_MID]->frame : nil;
			RwFrame *headFrame = ped->m_pFrames[PED_HEAD] ? ped->m_pFrames[PED_HEAD]->frame : nil;
			RwFrame *torsoFrame = headFrame ? RwFrameGetParent(headFrame) : nil;

			if(midFrame && midFrame != torsoFrame){
				RwMatrix midWorld;
				midWorld.pos = visualRoot + FromBasisLocal(CVector(0.0004f, 0.0348f, -0.068f), visualBody);
				midWorld.right = FromBasisLocal(CVector(0.0f, 1.0f, 0.0f), visualBody);
				midWorld.up = FromBasisLocal(CVector(0.0f, 0.0f, 1.0f), visualBody);
				midWorld.at = FromBasisLocal(CVector(-1.0f, 0.0f, 0.0f), visualBody);
				Orthonormalise(midWorld);

				RwFrame *midParent = RwFrameGetParent(midFrame);
				if(midParent){
					RwMatrix invParent;
					rw::Matrix::invert(&invParent, RwFrameGetLTM(midParent));
					RwMatrix localMid;
					rw::Matrix::mult(&localMid, &midWorld, &invParent);
					*RwFrameGetMatrix(midFrame) = localMid;
					RwFrameUpdateObjects(midFrame);
				}
			}

			if(torsoFrame){
				RwMatrix torsoWorld;
				torsoWorld.pos = visualRoot + FromBasisLocal(CVector(0.0004f, 0.2182f, -0.068f), visualBody);
				torsoWorld.right = FromBasisLocal(CVector(0.0f, 1.0f, 0.0f), visualBody);
				torsoWorld.up = FromBasisLocal(CVector(0.0f, 0.0f, 1.0f), visualBody);
				torsoWorld.at = FromBasisLocal(CVector(-1.0f, 0.0f, 0.0f), visualBody);
				Orthonormalise(torsoWorld);

				RwFrame *torsoParent = RwFrameGetParent(torsoFrame);
				if(torsoParent){
					RwMatrix invParent;
					rw::Matrix::invert(&invParent, RwFrameGetLTM(torsoParent));
					RwMatrix localTorso;
					rw::Matrix::mult(&localTorso, &torsoWorld, &invParent);
					*RwFrameGetMatrix(torsoFrame) = localTorso;
					RwFrameUpdateObjects(torsoFrame);
				}
			}
		}
	}

	// Publish the vehicle anchor after the body/neck pose is ready, then
	// convert the controller sample once into that same frame. Carrying this
	// target again by the previous body delta makes free hands jump by speed*dt
	// (and drops that offset abruptly at the old two-metre carry cutoff).
	if(vehicleIK)
		::VrUpdateFirstPersonAnchor(false);
	OculusVR::RefreshQuestTrackedHandWorldPosesForRender();

	CMatrix desiredWorld[2];
	bool desiredValid[2] = { false, false };
	bool weaponSupportLocked[2] = { false, false };
	CVector physicalPitchAxis[2];
	bool physicalPitchAxisValid[2] = { false, false };

	for(int handIndex = 0; handIndex < 2; handIndex++){
		WristCalibration &cal = gWrist[handIndex];

		CMatrix raw;
		if(vehicleIK){
			if(!GetVehicleHandPresentationPose(handIndex, raw))
				continue;
		}else if(!GetFloatingHandPresentationPose(handIndex, raw))
			continue;

		weaponSupportLocked[handIndex] =
			OculusVR::ApplyTrackedWeaponSupportHandLock(
				handIndex, &raw);

		physicalPitchAxis[handIndex] = raw.GetRight();
		if(physicalPitchAxis[handIndex].MagnitudeSqr() > 1.0e-8f){
			physicalPitchAxis[handIndex].Normalise();
			physicalPitchAxisValid[handIndex] = true;
		}

		if(cal.valid)
			desiredWorld[handIndex] = ApplyCalibration(raw, cal);
		else
			desiredWorld[handIndex] =
				BuildDefaultHandPose(handIndex, raw);

		CaptureRigidWeaponLink(
			handIndex, desiredWorld[handIndex], raw);

		cal.lastPose = desiredWorld[handIndex];
		cal.lastPoseValid = true;
		desiredValid[handIndex] = true;
	}

	int solveOrder[2] = { 0, 1 };
	if(OculusVR::GetHeldWeaponSlot(1) >= 0 &&
	   OculusVR::GetHeldWeaponSlot(0) < 0){
		solveOrder[0] = 1;
		solveOrder[1] = 0;
	}

	for(int solveIndex = 0; solveIndex < 2; solveIndex++){
		const int handIndex = solveOrder[solveIndex];
		if(!desiredValid[handIndex])
			continue;

		RwFrame *upperFrame = (handIndex == 0) ?
			(ped->m_pFrames[PED_UPPERARML] ? ped->m_pFrames[PED_UPPERARML]->frame : nil) :
			(ped->m_pFrames[PED_UPPERARMR] ? ped->m_pFrames[PED_UPPERARMR]->frame : nil);
		RwFrame *handFrame = (handIndex == 0) ?
			(ped->m_pFrames[PED_HANDL] ? ped->m_pFrames[PED_HANDL]->frame : nil) :
			(ped->m_pFrames[PED_HANDR] ? ped->m_pFrames[PED_HANDR]->frame : nil);
		if(!upperFrame || !handFrame)
			continue;
		RwFrame *forearmFrame = RwFrameGetParent(handFrame);
		if(!forearmFrame)
			continue;

		RwMatrix upperMat = *RwFrameGetLTM(upperFrame);
		RwMatrix forearmMat = *RwFrameGetLTM(forearmFrame);
		RwMatrix handMat = *RwFrameGetLTM(handFrame);

		if(weaponSupportLocked[handIndex] &&
		   OculusVR::GetHeldWeaponSlot(handIndex) < 0){
			CMatrix currentSupportPose;
			if(GetFloatingHandPresentationPose(handIndex, currentSupportPose) &&
			   OculusVR::ApplyTrackedWeaponSupportHandLock(
				   handIndex, &currentSupportPose, true)){
				WristCalibration &cal = gWrist[handIndex];
				if(cal.valid)
					desiredWorld[handIndex] =
						ApplyCalibration(currentSupportPose, cal);
				else
					desiredWorld[handIndex] =
						BuildDefaultHandPose(handIndex, currentSupportPose);
				CaptureRigidWeaponLink(
					handIndex, desiredWorld[handIndex], currentSupportPose);
			}
		}

		const bool pitchWeaponHand =
			physicalPitchAxisValid[handIndex] &&
			OculusVR::GetHeldWeaponSlot(handIndex) >= 0;

		if(SolveArmMatrices(upperMat, forearmMat, handMat,
		   handIndex, desiredWorld[handIndex], visualBody,
		   physicalPitchAxis[handIndex], pitchWeaponHand,
		   weaponSupportLocked[handIndex])){

			// Apply solved world transforms back to the RwFrames in parent-to-child order
			RwFrame *upperParent = RwFrameGetParent(upperFrame);
			if(upperParent){
				RwMatrix invParent;
				rw::Matrix::invert(&invParent, RwFrameGetLTM(upperParent));
				RwMatrix localUpper;
				rw::Matrix::mult(&localUpper, &upperMat, &invParent);
				*RwFrameGetMatrix(upperFrame) = localUpper;
				RwFrameUpdateObjects(upperFrame);
			}

			RwMatrix invUpper;
			rw::Matrix::invert(&invUpper, RwFrameGetLTM(upperFrame));
			RwMatrix localForearm;
			rw::Matrix::mult(&localForearm, &forearmMat, &invUpper);
			*RwFrameGetMatrix(forearmFrame) = localForearm;
			RwFrameUpdateObjects(forearmFrame);

			RwMatrix invForearm;
			rw::Matrix::invert(&invForearm, RwFrameGetLTM(forearmFrame));
			RwMatrix localHand;
			rw::Matrix::mult(&localHand, &handMat, &invForearm);
			*RwFrameGetMatrix(handFrame) = localHand;
			RwFrameUpdateObjects(handFrame);

			gSolvedHandWorld[handIndex].GetPosition() = CVector(handMat.pos);
			gSolvedHandWorld[handIndex].GetRight() = CVector(handMat.right);
			gSolvedHandWorld[handIndex].GetUp() = CVector(handMat.up);
			gSolvedHandWorld[handIndex].GetForward() = CVector(handMat.at);
			OrthonormaliseBasis(gSolvedHandWorld[handIndex]);
			gSolvedHandValid[handIndex] = true;
		}else{
			gSolvedHandValid[handIndex] = false;
		}
	}
}
#endif

void Apply(CPed *ped)
{
#if !defined(__ANDROID__) || !defined(GTA_VR_WEAPONS)
	(void)ped;
	return;
#else
	if(ped == nil || !ped->IsPlayer())
		return;

	if(VrRagdoll::Owns(ped)){
		Reset();
		return;
	}

	const bool isSkinned = ped->GetClump() != nil && IsClumpSkinned(ped->GetClump());
	if(ped->GetClump() != nil && !isSkinned){
		RwFrame *clumpFrame = RpClumpGetFrame(ped->GetClump());
		if(clumpFrame != nil){
			RwFrameGetMatrix(clumpFrame)->pos = ped->GetPosition();
			RwFrameUpdateObjects(clumpFrame);
		}
	}

	// The non-skinned clump root above must first lose its previous roomscale
	// offset. Leave native bones untouched and clear caches for recovery.
	if(!BodyIKGameplayContext()){
		Reset();
		return;
	}

	if(gVehicleTransitionActive && isSkinned){
		gSolvedHandValid[0] = false;
		gSolvedHandValid[1] = false;
		gRigidWeaponLink[0] = RigidWeaponHandLink();
		gRigidWeaponLink[1] = RigidWeaponHandLink();
		return;
	}

	const bool onFootIK =
		Enabled() && (OculusVR::IsQuestFirstPersonOnFoot() ||
		              (::gVrFirstPersonActive && gVehicleTransitionActive));
	const bool vehicleIK =
		!onFootIK && VehicleEnabled() && ::gVrFirstPersonActive &&
		ped->InVehicle() && ped->m_pMyVehicle != nil;
	if(!onFootIK && !vehicleIK)
		return;

	if(gLastPlayer != ped){
		Reset();
		gLastPlayer = ped;
	}

	EnsureBakedCalibration();

	// PreRender runs before the final camera setup. On foot, publish the
	// current player anchor before solving either wrist: otherwise the gun
	// inherits the previous camera position while the eyes use this frame.
	// Vehicle anchors are published inside each solver after its body/neck
	// pose is ready. Neither path advances refreshed targets by a body delta.
	if(onFootIK)
		::VrUpdateFirstPersonAnchor(false);

	gSolvedHandValid[0] = false;
	gSolvedHandValid[1] = false;

	if(isSkinned){
		ApplySkinned(ped, onFootIK, vehicleIK);
	}else{
		ApplyNonSkinned(ped, onFootIK, vehicleIK);
	}
#endif
}
}
