#pragma once

// Native head pose, captured before presentation IK/head hiding. Keep it in
// entity space so post-physics translation/rotation cannot leave it one step
// behind a moving ped or carriage. No easing, eye offset or seat calibration.
namespace QuestHeadAttachment
{
struct Pose
{
	CVector position, right, up, forward;
};

// Walking bob is optional; scripted/posture changes still use the live bone.
// Keep the standing reference across crouch/get-up so recovery cannot latch a
// half-height animation frame. Relatch only for a new model/carrier or toggle.
class ComfortAnchor
{
	const void *owner = nil, *clump = nil, *carrier = nil;
	bool valid = false;
	bool previousBobbing=false;
	CVector local;
public:
	CVector Position(const void *ped, const void *model, const void *vehicle,
		const CMatrix &root, const CVector &nativeHead, bool bobbing,
		bool routineMotion, bool postPhysics, bool buttonCrouch=false, bool jumping=false)
	{
		if(owner != ped || clump != model || carrier != vehicle){
			owner = ped; clump = model; carrier = vehicle; valid = false;
		}
		if(bobbing!=previousBobbing && !buttonCrouch) valid=false;
		previousBobbing=bobbing;
		if(buttonCrouch){
			if(!valid){local=Invert(root)*nativeHead;local.z+=0.45f;valid=true;}
			return root*local-CVector(0,0,0.45f);
		}
		if(jumping && valid){
			// The visible upper body is stabilized during a jump. Retain its
			// horizontal neck reference while following authored vertical motion.
			CVector position = root*local;
			position.z = nativeHead.z;
			return position;
		}
		if(bobbing){
			if(routineMotion && postPhysics){local=Invert(root)*nativeHead;valid=true;}
			return nativeHead;
		}
		if(!routineMotion) return nativeHead;
		if(!valid){
			// Occupancy can change before the native boarding lineup. Commit
			// only after simulation; never retain that early lower pose.
			if(!postPhysics) return nativeHead;
			local = Invert(root) * nativeHead;
			valid = true;
		}
		return root * local;
	}
};

inline void ViewBasis(CVector heading, CVector vertical,
	CVector &right, CVector &up, CVector &forward)
{
	if(vertical.MagnitudeSqr() <= 1.0e-8f) vertical = CVector(0,0,1);
	vertical.Normalise();
	heading -= vertical * DotProduct(heading, vertical);
	if(heading.MagnitudeSqr() <= 1.0e-8f){
		heading = CVector(0,1,0);
		heading -= vertical * DotProduct(heading, vertical);
		if(heading.MagnitudeSqr() <= 1.0e-8f){
			heading = CVector(1,0,0);
			heading -= vertical * DotProduct(heading, vertical);
		}
	}
	heading.Normalise();
	right = CrossProduct(heading, vertical); right.Normalise();
	up = CrossProduct(right, heading); up.Normalise();
	forward = heading;
}

// Presentation IK turns the on-foot body into the physical view heading.
// Carry the authored head displacement in that same frame, including jumps;
// the native snapshot and gameplay root must remain untouched.
inline CVector FacingPosition(const CMatrix &root, const CVector &head,
	const CVector &right, const CVector &up, const CVector &forward,
	float localYaw, CVector &facingRight, CVector &facingForward)
{
	const float c = cosf(localYaw), s = sinf(localYaw);
	facingRight = right*c + forward*s;
	facingForward = forward*c - right*s;
	const CVector local = Invert(root)*head;
	return root*CVector(0,0,0) + facingRight*local.x +
		facingForward*local.y + up*local.z;
}

class Snapshot
{
	const void *owner = nil;
	const void *clump = nil;
	unsigned int frame = 0;
	bool valid = false;
	Pose local;
	static bool Finite(const CVector &v)
	{
		return isfinite(v.x) && isfinite(v.y) && isfinite(v.z);
	}
public:
	bool Matches(const void *ped, const void *model, unsigned int tick) const
	{
		return valid && owner == ped && clump == model && frame == tick;
	}
	void Capture(const void *ped, const void *model, unsigned int tick,
		const CMatrix &root, const RwMatrix &head)
	{
		valid = false;
		owner = ped; clump = model; frame = tick;
		const CMatrix inverse = Invert(root);
		local.position = inverse * CVector(head.pos);
		// Xbox head +X runs up the neck (also the yaw axis used by
		// CPedIK::RotateHead). +Y faces forward and +Z points left.
		// Convert that bone convention to camera right/up/forward; treating
		// bone +Z as camera-up rolls the entire view sideways by 90 degrees.
		local.right = Multiply3x3(inverse, -CVector(head.at));
		local.forward = Multiply3x3(inverse, CVector(head.up));
		local.up = Multiply3x3(inverse, CVector(head.right));
		valid = ped != nil && model != nil &&
			Finite(local.position) && Finite(local.right) &&
			Finite(local.up) && Finite(local.forward) &&
			local.right.MagnitudeSqr() > 1.0e-8f &&
			local.up.MagnitudeSqr() > 1.0e-8f &&
			local.forward.MagnitudeSqr() > 1.0e-8f;
	}
	bool Read(const CMatrix &root, Pose &out) const
	{
		if(!valid) return false;
		out.position = root * local.position;
		out.forward = Multiply3x3(root, local.forward);
		out.up = Multiply3x3(root, local.up);
		out.forward.Normalise();
		out.right = CrossProduct(out.forward, out.up);
		if(out.right.MagnitudeSqr() <= 1.0e-8f) return false;
		out.right.Normalise();
		out.up = CrossProduct(out.right, out.forward);
		out.up.Normalise();
		return true;
	}
};
}
