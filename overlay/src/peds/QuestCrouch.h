#pragma once

#if defined(GTA_VR_WEAPONS) && defined(RW_VULKAN) && defined(__ANDROID__)
namespace androidgame { bool VrUsesFixedButtonCrouch(); }
// GTA III's ClearDuck is an NPC shooting transition, not a stand-up action.
// Keep the animation, timer and camera's bIsDucking source in sync here.
static void ProcessQuestCrouch(CPlayerPed *player, CPad *pad)
{
	if(pad == nil)
		return;
	const bool pressed = pad->DuckJustDown();
	const bool fixed = androidgame::VrUsesFixedButtonCrouch();
	const bool moving = pad->GetPedWalkLeftRight() != 0 ||
		pad->GetPedWalkUpDown() != 0;
	const bool jumping = pad->JumpJustDown() || player->bIsInTheAir ||
		player->m_nPedState == PED_JUMP;
	if(player->bIsDucking){
		if(fixed && !pressed){player->m_duckTimer=CTimer::GetTimeInMilliseconds()+60000;return;}
		if(!pressed && !moving && !jumping)
			return;
		const int animations[] = { ANIM_STD_DUCK_DOWN, ANIM_STD_DUCK_LOW };
		for(int animation : animations){
			CAnimBlendAssociation *assoc =
				RpAnimBlendClumpGetAssociation(player->GetClump(), animation);
			if(assoc){
				assoc->blendDelta = -1000.0f;
				assoc->flags |= ASSOC_DELETEFADEDOUT;
			}
		}
		player->bIsDucking = false;
		player->bCrouchWhenShooting = false;
		player->m_duckTimer = 0;
	}else if(pressed && (fixed || !moving) && !jumping &&
	         player->m_nMoveState != PEDMOVE_SPRINT && !player->InVehicle()){
		player->bCrouchWhenShooting = false;
		player->m_duckTimer = 0;
		player->SetDuck(60000);
	}
}
#endif
