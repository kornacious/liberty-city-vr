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
		local.right = Multiply3x3(inverse, CVector(head.right));
		// Authored GTA head matrices use up as facing and at as vertical.
		local.forward = Multiply3x3(inverse, CVector(head.up));
		local.up = Multiply3x3(inverse, CVector(head.at));
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
