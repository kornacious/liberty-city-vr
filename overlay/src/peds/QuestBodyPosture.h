#pragma once

// Presentation-only, stateless posture solve. All positions are world-space;
// headset displacement is relative to the recentered camera anchor. Never write
// a solved pose back to that anchor or to gameplay collision/root transforms.
namespace QuestBodyPosture
{
inline float Bounded(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline bool Finite(const CVector &v) { return isfinite(v.x) && isfinite(v.y) && isfinite(v.z); }
inline CVector Unit(CVector v, const CVector &fallback)
{
	const float n = v.Magnitude();
	return n > 1.0e-6f ? v*(1.0f/n) : fallback;
}
inline CVector Lean(CVector delta, const CVector &up)
{
	delta -= up*DotProduct(delta, up);
	const float n = delta.Magnitude();
	return n > 0.28f ? delta*(0.28f/n) : delta;
}

// Minimal swing preserves each bone's authored twist/axis convention.
inline CVector Swing(const CVector &v, CVector from, CVector to)
{
	from = Unit(from, CVector(0,0,1)); to = Unit(to, from);
	const float c = Bounded(DotProduct(from,to), -1.0f, 1.0f);
	CVector axis = CrossProduct(from,to);
	if(c < -0.9999f){
		axis = Unit(CrossProduct(from, CVector(1,0,0)),
			Unit(CrossProduct(from,CVector(0,1,0)), CVector(0,0,1)));
		return axis*(2.0f*DotProduct(axis,v))-v;
	}
	return v + CrossProduct(axis,v) + CrossProduct(axis,CrossProduct(axis,v))*(1.0f/(1.0f+c));
}

struct Input
{
	CVector spine[4]; // waist, mid, torso, head
	CVector hip[2], knee[2], foot[2];
	CVector delta, up, forward;
	float authoredDrop;
};
struct Output
{
	CVector spine[4], hip[2], knee[2], foot[2];
};

inline bool Solve(const Input &in, Output &out)
{
	if(!Finite(in.delta) || !Finite(in.up) || !Finite(in.forward) ||
	   !isfinite(in.authoredDrop)) return false;
	float lengths[3], upper[2], lower[2];
	float total = 0.0f;
	for(int i=0; i<4; ++i) if(!Finite(in.spine[i])) return false;
	for(int i=0; i<3; ++i){
		lengths[i] = (in.spine[i+1]-in.spine[i]).Magnitude();
		if(lengths[i] < 0.005f) return false;
		total += lengths[i];
	}
	for(int i=0; i<2; ++i){
		if(!Finite(in.hip[i]) || !Finite(in.knee[i]) || !Finite(in.foot[i])) return false;
		upper[i]=(in.knee[i]-in.hip[i]).Magnitude();
		lower[i]=(in.foot[i]-in.knee[i]).Magnitude();
		if(upper[i]<0.02f || lower[i]<0.02f) return false;
	}
	const CVector up=Unit(in.up,CVector(0,0,1));
	const CVector forward=Unit(in.forward-up*DotProduct(in.forward,up),CVector(0,1,0));
	const CVector lean=Lean(in.delta,up);
	// Native button crouch already supplies some lowering. Use the larger of
	// authored and physical crouch, rather than adding both reductions.
	const float legLength = upper[0]+lower[0] < upper[1]+lower[1] ? upper[0]+lower[0] : upper[1]+lower[1];
	const float drop=Bounded(-DotProduct(in.delta,up)-Bounded(in.authoredDrop,0,0.8f),0,
		Bounded(legLength*0.65f,0,0.65f));
	if(lean.MagnitudeSqr()<1.0e-10f && drop<1.0e-5f){
		for(int i=0;i<4;++i) out.spine[i]=in.spine[i];
		for(int i=0;i<2;++i){out.hip[i]=in.hip[i];out.knee[i]=in.knee[i];out.foot[i]=in.foot[i];}
		return true;
	}
	const CVector pelvis=lean*0.18f-up*(drop*0.85f)-forward*(drop*0.15f);
	const CVector root=in.spine[0]+pelvis;
	CVector target=in.spine[3]+lean-up*drop;
	if((target-root).Magnitude() > total*0.9999f)
		target=root+Unit(target-root,up)*(total*0.9999f);
	for(int i=0; i<4; ++i) out.spine[i]=in.spine[i]+pelvis;
	// Seed a forward bend when compressing an otherwise straight spine.
	if((target-root).Magnitude() < total-0.001f){
		out.spine[1]+=forward*0.025f; out.spine[2]+=forward*0.025f;
	}
	for(int iteration=0; iteration<16; ++iteration){
		out.spine[3]=target;
		for(int i=2;i>=0;--i)
			out.spine[i]=out.spine[i+1]+Unit(out.spine[i]-out.spine[i+1],-up)*lengths[i];
		out.spine[0]=root;
		for(int i=0;i<3;++i)
			out.spine[i+1]=out.spine[i]+Unit(out.spine[i+1]-out.spine[i],up)*lengths[i];
	}
	for(int i=0;i<2;++i){
		out.hip[i]=in.hip[i]+pelvis;
		const CVector direction=Unit(in.foot[i]-out.hip[i],-up);
		const float distance=Bounded((in.foot[i]-out.hip[i]).Magnitude(),
			fabsf(upper[i]-lower[i])+0.001f,upper[i]+lower[i]-0.001f);
		out.foot[i]=out.hip[i]+direction*distance;
		CVector pole=in.knee[i]-in.hip[i];
		pole-=direction*DotProduct(pole,direction);
		pole=Unit(pole,Unit(forward-direction*DotProduct(forward,direction),forward));
		const float along=(upper[i]*upper[i]-lower[i]*lower[i]+distance*distance)/(2.0f*distance);
		const float height=sqrtf(Bounded(upper[i]*upper[i]-along*along,0,upper[i]*upper[i]));
		out.knee[i]=out.hip[i]+direction*along+pole*height;
	}
	return true;
}
}
