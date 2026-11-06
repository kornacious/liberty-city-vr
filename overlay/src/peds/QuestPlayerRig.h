#ifndef QUEST_PLAYER_RIG_H
#define QUEST_PLAYER_RIG_H

// Include after Ped/Bones/RwHelper. Classic uses rigid animation frames;
// Xbox uses HAnim matrices and leaves PED_TORSO out of m_pFrames entirely.
namespace QuestPlayerRig
{
inline bool HasCameraPose(CPed *player)
{
	if(player == nil || player->m_rwObject == nil)
		return false;
	RpClump *clump = player->GetClump();
	if(IsClumpSkinned(clump)){
		RpHAnimHierarchy *hier = GetAnimHierarchyFromSkinClump(clump);
		if(hier == nil || RpHAnimHierarchyGetMatrixArray(hier) == nil)
			return false;
		const int head = RpHAnimIDGetIndex(hier, BONE_head);
		const int torso = RpHAnimIDGetIndex(hier, BONE_torso);
		return head >= 0 && head < hier->numNodes &&
			torso >= 0 && torso < hier->numNodes;
	}
	return player->m_pFrames[PED_HEAD] != nil &&
		(player->InVehicle() || player->m_pFrames[PED_TORSO] != nil);
}

inline RwFrame *RigidTransitionWaist(CPed *player)
{
	if(player == nil || player->m_rwObject == nil ||
	   IsClumpSkinned(player->GetClump()))
		return nil;
	// AnimBlendFrameData::frame aliases hanimFrame on a skinned model. Never
	// follow it as an RwFrame, even if the HAnim head pose is temporarily absent.
	if(player->m_pFrames[PED_MID] == nil ||
	   player->m_pFrames[PED_MID]->frame == nil)
		return nil;
	RwFrame *waist = RwFrameGetParent(player->m_pFrames[PED_MID]->frame);
	return waist != RpClumpGetFrame(player->GetClump()) ? waist : nil;
}
}
#endif
