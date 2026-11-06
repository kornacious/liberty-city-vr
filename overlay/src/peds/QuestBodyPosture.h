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
	CVector headTarget, footTarget[2];
	bool hasHeadTarget=false, hasFootTargets=false;
};
struct Output
{
	CVector spine[4], hip[2], knee[2], foot[2];
};

inline bool Solve(const Input &in, Output &out)
{
	if(!Finite(in.delta) || !Finite(in.up) || !Finite(in.forward) ||
	   (in.hasHeadTarget && !Finite(in.headTarget)) ||
	   !isfinite(in.authoredDrop)) return false;
	float lengths[3], upper[2], lower[2];
	float total = 0.0f;
	for(int i=0; i<4; ++i) if(!Finite(in.spine[i])) return false;
	if(in.hasHeadTarget && (in.headTarget-in.spine[3]).MagnitudeSqr()>2.25f) return false;
	for(int i=0; i<3; ++i){
		lengths[i] = (in.spine[i+1]-in.spine[i]).Magnitude();
		if(lengths[i] < 0.005f) return false;
		total += lengths[i];
	}
	for(int i=0; i<2; ++i){
		if(!Finite(in.hip[i]) || !Finite(in.knee[i]) || !Finite(in.foot[i])) return false;
		if(in.hasFootTargets && !Finite(in.footTarget[i])) return false;
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
	if(!in.hasHeadTarget && !in.hasFootTargets && lean.MagnitudeSqr()<1.0e-10f && drop<1.0e-5f){
		for(int i=0;i<4;++i) out.spine[i]=in.spine[i];
		for(int i=0;i<2;++i){out.hip[i]=in.hip[i];out.knee[i]=in.knee[i];out.foot[i]=in.foot[i];}
		return true;
	}
	CVector pelvis=lean*0.18f-up*(drop*0.85f)-forward*(drop*0.15f);
	CVector root=in.spine[0]+pelvis;
	const CVector target=in.hasHeadTarget ? in.headTarget : in.spine[3]+lean-up*drop;
	// The headset is authoritative. Move the pelvis to make room for the
	// neck instead of shortening its target and leaving the chest in the eye.
	if((target-root).Magnitude() > total*0.9999f){
		root=target-Unit(target-root,up)*(total*0.9999f);
		pelvis=root-in.spine[0];
	}
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
	// Close the finite-iteration residual at the pelvis, retaining every
	// segment length and exact head placement even when leaning backwards.
	const CVector residual=target-out.spine[3];
	for(int i=0;i<4;++i) out.spine[i]+=residual;
	pelvis=out.spine[0]-in.spine[0];
	for(int i=0;i<2;++i){
		out.hip[i]=in.hip[i]+pelvis;
		const CVector contact=in.hasFootTargets ? in.footTarget[i] : in.foot[i];
		const CVector direction=Unit(contact-out.hip[i],-up);
		const float distance=Bounded((contact-out.hip[i]).Magnitude(),
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

// Stationary contacts are stored in world space, never in the HMD-yawed body
// frame. Walking, loss of ground, a turn/step, teleport or ownership gap releases
// them. The native gait remains the contact source during locomotion.
class Stance
{
	const void *owner=nullptr,*model=nullptr;
	unsigned frame=0;
	CVector root,heading;
	bool active=false,valid[2]={false,false},orientationValid[2]={false,false};
	CVector position[2],normal[2];
	CVector right[2],up[2],at[2];
	bool stepping=false,advanced=false;
	unsigned advancedFrame=0;
	int swing=-1,nextFoot=0;
	float elapsed=0;
	CVector stepStart,stepEnd;
public:
	void Reset(){active=false;valid[0]=valid[1]=false;owner=model=nullptr;swing=-1;advanced=false;}
	void Begin(const void *ped,const void *clump,unsigned tick,
		const CVector &pedRoot,const CVector &forward,bool eligible,bool proceduralSteps=false)
	{
		stepping=proceduralSteps;
		if(!eligible || !Finite(pedRoot) || !Finite(forward)){Reset();return;}
		if(!active || owner!=ped || model!=clump || tick-frame>2 ||
		   (pedRoot-root).MagnitudeSqr()>(stepping ? .36f : .0025f) || DotProduct(heading,forward)<0.82f){
			valid[0]=valid[1]=false;heading=forward;swing=-1;advanced=false;
		}
		active=true;owner=ped;model=clump;frame=tick;root=pedRoot;
	}
	void Advance(const CVector native[2],float seconds)
	{
		if(!active || !stepping || !valid[0] || !valid[1]){swing=-1;return;}
		if(advanced && advancedFrame==frame) return;
		advanced=true;advancedFrame=frame;
		if(swing<0){
			CVector error[2]={native[0]-position[0],native[1]-position[1]};
			error[0].z=error[1].z=0;
			if(error[0].MagnitudeSqr()<.0144f && error[1].MagnitudeSqr()<.0144f) return;
			swing=nextFoot;elapsed=0;stepStart=position[swing];
			stepEnd=native[swing]+Unit(error[swing],CVector(0,0,0))*.04f;
			orientationValid[swing]=false;
		}
		elapsed+=Bounded(seconds,0,.05f);
		const float t=Bounded(elapsed/.22f,0,1),smooth=t*t*(3-2*t);
		position[swing]=stepStart+(stepEnd-stepStart)*smooth+CVector(0,0,sinf(t*3.14159265f)*.065f);
		if(t>=1){nextFoot=1-swing;swing=-1;}
	}
	bool IsSwinging(int side) const {return swing==side;}
	CVector Candidate(int side,const CVector &native) const {return active && valid[side] ? position[side] : native;}
	CVector Contact(int side,const CVector &native,const CVector &ground,const CVector &groundNormal,bool hit)
	{
		if(!active || !hit || !Finite(ground) || !Finite(groundNormal) || groundNormal.z<0.65f ||
		   fabsf(native.z-ground.z)>0.3f){valid[side]=false;return native;}
		if(valid[side] && ((!stepping && (native-position[side]).MagnitudeSqr()>0.09f) ||
		   fabsf(ground.z-position[side].z)>0.3f)) valid[side]=false;
		if(!valid[side]){
			position[side]=CVector(native.x,native.y,ground.z+Bounded(native.z-ground.z,0.02f,0.16f));
			normal[side]=Unit(groundNormal,CVector(0,0,1));valid[side]=true;orientationValid[side]=false;
		}
		return position[side];
	}
	CVector Normal(int side) const {return active && valid[side] ? normal[side] : CVector(0,0,1);}
	void Orient(int side,CVector &r,CVector &u,CVector &a){
		if(!active || !valid[side]) return;
		if(!orientationValid[side]){right[side]=r;up[side]=u;at[side]=a;orientationValid[side]=true;}
		r=right[side];u=up[side];a=at[side];
	}
	void Release(int side){valid[side]=false;}
};
}
