#pragma once
#include <cmath>

// GTA III has both rigid PC frame trees and skinned Xbox/Mobile peds.
// Keep the solver's virtual joints separate from the model's actual bones.
namespace VrRagdollRig {
enum { MAX_NODES = 40 };
inline bool SameName(const char *a,const char *b) {
	if(!a || !b) return false;
	while(*a && *b) {
		char x=*a++,y=*b++;
		if(x>='A' && x<='Z') x+= 'a'-'A';
		if(y>='A' && y<='Z') y+= 'a'-'A';
		if(x!=y) return false;
	}
	return *a==*b;
}
struct Pose {
	int count = 0;
	int parent[MAX_NODES], tag[MAX_NODES];
	RwMatrix world[MAX_NODES];
	int Find(int bone) const {
		for(int i=0;i<count;i++) if(tag[i]==bone) return i;
		return -1;
	}
};

inline bool Capture(const Pose &rig, int *nodes, RwMatrix *joints) {
	using namespace VrRagdollPhysics;
	const int tags[SIM_BONES] = {
		BONE_waist, BONE_mid, BONE_torso, -1, BONE_head,
		-1, BONE_upperarml, BONE_lowerarml, BONE_Lhand,
		-1, BONE_upperarmr, BONE_lowerarmr, BONE_Rhand,
		BONE_upperlegl, BONE_lowerlegl, BONE_footl,
		BONE_upperlegr, BONE_lowerlegr, BONE_footr
	};
	for(int i=0;i<SIM_BONES;i++) {
		nodes[i]=tags[i]<0 ? -1 : rig.Find(tags[i]);
		if(nodes[i]<0 && tags[i]>=0 && i!=RD_SPINE) return false;
		if(nodes[i]>=0) joints[i]=rig.world[nodes[i]];
	}
	// Some Xbox skins omit Smid. Its solver particle still has real length.
	if(nodes[RD_SPINE]<0) {
		joints[RD_SPINE]=joints[RD_PELVIS];
		joints[RD_SPINE].pos=(CVector(joints[RD_PELVIS].pos)+CVector(joints[RD_SPINE1].pos))*0.5f;
	}
	const int virtualJoints[]={RD_NECK,RD_LCLAV,RD_RCLAV};
	const int ends[]={RD_HEAD,RD_LUARM,RD_RUARM};
	for(int i=0;i<3;i++) {
		joints[virtualJoints[i]]=joints[RD_SPINE1];
		joints[virtualJoints[i]].pos=CVector(joints[RD_SPINE1].pos)+
			(CVector(joints[ends[i]].pos)-CVector(joints[RD_SPINE1].pos))*0.5f;
	}
	// Reject malformed/unsupported poses before publishing an owner.
	for(int i=0;i<SKELETON_STICKS;i++) {
		const CVector delta=CVector(joints[sticks[i].b].pos)-CVector(joints[sticks[i].a].pos);
		const float lengthSq=delta.MagnitudeSqr();
		if(!std::isfinite(lengthSq) || lengthSq<0.000001f || lengthSq>4.0f) return false;
	}
	return true;
}

#ifndef VR_RAGDOLL_RIG_NO_ENGINE
struct Binding {
	Pose pose;
	RpHAnimHierarchy *skin = nil;
	RwFrame *frames[MAX_NODES];
};

inline bool Read(CPed *ped, Binding &out) {
	RpClump *clump=ped->GetClump();
	if(!clump) return false;
	if(IsClumpSkinned(clump)) {
		out.skin=GetAnimHierarchyFromSkinClump(clump);
		if(!out.skin || out.skin->numNodes<=0 || out.skin->numNodes>MAX_NODES) return false;
		out.pose.count=out.skin->numNodes;
		RwMatrix *matrices=RpHAnimHierarchyGetMatrixArray(out.skin);
		if(!matrices) return false;
		int stack[MAX_NODES+1], sp=0, parent=-1;
		stack[sp++]=-1;
		for(int i=0;i<out.pose.count;i++) {
			out.pose.world[i]=matrices[i];
			out.pose.tag[i]=out.skin->nodeInfo[i].id;
			out.pose.parent[i]=parent;
			int flags=out.skin->nodeInfo[i].flags;
			if(flags & rw::HAnimHierarchy::PUSH) stack[sp++]=parent;
			parent=i;
			if(flags & rw::HAnimHierarchy::POP) { if(sp<=0) return false; parent=stack[--sp]; }
		}
	} else {
		if(!RpAnimBlendClumpIsInitialized(clump)) return false;
		CAnimBlendClumpData *data=*RPANIMBLENDCLUMPDATA(clump);
		if(!data || data->numFrames<=0 || data->numFrames>MAX_NODES) return false;
		out.pose.count=data->numFrames;
		for(int i=0;i<out.pose.count;i++) {
			RwFrame *frame=data->frames[i].frame;
			if(!frame) return false;
			out.frames[i]=frame;
			out.pose.world[i]=*RwFrameGetLTM(frame);
			out.pose.tag[i]=-1;
			const char *name=GetFrameNodeName(frame);
			for(int tag=BONE_waist;tag<=BONE_Lhand;tag++)
				if(SameName(name,ConvertBoneTag2BoneName(tag))) out.pose.tag[i]=tag;
		}
		for(int i=0;i<out.pose.count;i++) {
			out.pose.parent[i]=-1;
			for(int j=0;j<out.pose.count;j++)
				if(RwFrameGetParent(out.frames[i])==out.frames[j]) out.pose.parent[i]=j;
		}
	}
	return true;
}

inline void Write(CPed *ped, const Binding &binding, const RwMatrix *world) {
	if(binding.skin) {
		RwMatrix *matrices=RpHAnimHierarchyGetMatrixArray(binding.skin);
		for(int i=0;i<binding.pose.count;i++) matrices[i]=world[i];
	} else {
		// Convert every solved world matrix against the solved parent, never
		// against a partially updated LTM. Keep the entity/clump root untouched.
		for(int i=0;i<binding.pose.count;i++) {
			int parent=binding.pose.parent[i];
			const RwMatrix *parentWorld=parent>=0 ? &world[parent] : RwFrameGetLTM(RwFrameGetParent(binding.frames[i]));
			RwMatrix inverse, local;
			rw::Matrix::invert(&inverse,parentWorld);
			rw::Matrix::mult(&local,&world[i],&inverse);
			*RwFrameGetMatrix(binding.frames[i])=local;
		}
		RwFrameUpdateObjects(RpClumpGetFrame(ped->GetClump()));
	}
}
#endif
}
