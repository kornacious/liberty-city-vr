#include "common.h"
#include "VrPlayerTraversal.h"
#ifdef __ANDROID__
#include "VrTraversalRules.h"
#include "PlayerPed.h"
#include "World.h"
#include "Pad.h"
#include "Timer.h"
#include "Camera.h"
#include "Wanted.h"
#include "Streaming.h"
#include "CutsceneMgr.h"
#include "Frontend.h"
#include "Game.h"
#include "skel/android/android.h"

namespace VrPlayerTraversal {
static bool flight, noclip, terrainQuery, terrainModel;
static CPlayerPed *controlled;
static bool previousCollision;
static float fallSpeed;
bool Flight() { return flight; }
bool Noclip() { return noclip; }
bool TerrainQuery() { return terrainQuery; }
bool TerrainModel() { return terrainModel; }
TerrainModelScope::TerrainModelScope(const char *name) : previous(terrainModel)
{ terrainModel=terrainQuery && VrTraversal::TerrainModelName(name); }
TerrainModelScope::~TerrainModelScope() { terrainModel=previous; }
static void Release(CPlayerPed *player)
{
    if(controlled == player){
        player->bUsesCollision=previousCollision;
        player->m_vecMoveSpeed=CVector(0,0,0);
        player->m_vecTurnSpeed=CVector(0,0,0);
        player->m_collPoly.valid=false;
        controlled=nil;
    }
    fallSpeed=0;
}
void Reset(CPlayerPed *player) { Release(player); flight=noclip=false; }

static bool Ground(const CVector &position,float top,float bottom,float &height)
{
    CColPoint hit;
    CEntity *entity=nil;
    const bool previous=terrainQuery;
    terrainQuery=true;
    const bool found=CWorld::ProcessVerticalLine(CVector(position.x,position.y,top),bottom,
        hit,entity,true,false,false,false,false,false,nil);
    terrainQuery=previous;
    if(found) height=hit.point.z+FEET_OFFSET;
    return found;
}
static bool Blocked(CPlayerPed *player,const CVector &position,bool groundOnly=false)
{
    // Three overlapping spheres cover the ped rather than letting head/feet
    // tunnel through thin obstacles. Substeps below bound each swept interval.
    const bool previous=terrainQuery;
    terrainQuery=groundOnly;
    bool blocked=false;
    for(int i=0;i<3;++i)
        if(CWorld::TestSphereAgainstWorld(position+CVector(0,0,-0.55f+i*0.55f),
            0.35f,player,true,!groundOnly,!groundOnly,!groundOnly,!groundOnly,false)){
            blocked=true; break;
        }
    terrainQuery=previous;
    return blocked;
}
static void Place(CPlayerPed *player,const CVector &position)
{
    CWorld::Remove(player);
    player->SetPosition(position);
    player->GetMatrix().UpdateRW();
    player->UpdateRwFrame();
    CWorld::Add(player);
    player->m_vecMoveSpeed=CVector(0,0,0);
    player->m_vecTurnSpeed=CVector(0,0,0);
    player->m_collPoly.valid=false;
    player->m_pCurrentPhysSurface=nil;
    player->bIsInTheAir=false;
    player->bIsLanding=false;
}
static void Recover(CPlayerPed *player)
{
    // Town Hall entrance forecourt, south of the townhall_nite model on
    // Staunton Island. Only the destination coordinate is part of this kit.
    CVector destination(96.8f,-1001.0f,26.0f);
    CStreaming::LoadScene(destination);
    float ground;
    if(Ground(destination,80.0f,-20.0f,ground)) destination.z=ground;
    Release(player);
    flight=noclip=false;
    player->bIsStanding=false;
    player->SetPedState(PED_IDLE);
    Place(player,destination);
    player->m_fRotationCur=player->m_fRotationDest=0;
    player->SetHeading(0);
    TheCamera.RestoreWithJumpCut();
}
static bool CanToggle()
{
    CPlayerPed *player=FindPlayerPed();
    return player && !player->bInVehicle && player->m_fHealth>0 &&
        player->IsPedInControl() && !CCutsceneMgr::IsRunning() &&
        !CCutsceneMgr::IsCutsceneProcessing();
}
bool ToggleFlight()
{
    if(!flight && !CanToggle()) return false;
    flight=!flight;
    fallSpeed=0;
    return true;
}
bool ToggleNoclip()
{
    if(!noclip && !CanToggle()) return false;
    if(noclip && flight && controlled && Blocked(controlled,controlled->GetPosition())){
        Recover(controlled); return true;
    }
    noclip=!noclip;
    return true;
}
bool Process(CPlayerPed *player)
{
    if(player->bInVehicle || player->m_fHealth<=0 ||
       CGame::playingIntro || CCutsceneMgr::IsRunning() || CCutsceneMgr::IsCutsceneProcessing() ||
       TheCamera.m_WideScreenOn || CWorld::Players[CWorld::PlayerInFocus].IsPlayerInRemoteMode()){
        Release(player);
        return false;
    }
    if(VrTraversal::OutOfBounds(player->GetPosition().x,player->GetPosition().y,player->GetPosition().z)){
        Recover(player); return true;
    }
    if(!player->IsPedInControl()){ Release(player); return false; }
    if(!flight && !noclip){
        // Turning collision back on inside a wall would leave the player
        // trapped. Recover before restoring normal physics in that case.
        if(controlled==player && Blocked(player,player->GetPosition())) Recover(player);
        else Release(player);
        return false;
    }
    if(!controlled){ controlled=player; previousCollision=player->bUsesCollision; }
    player->bUsesCollision=false;
    player->m_pWanted->Update();
    const bool paused=androidgame::VrMenuConsumesInput() || FrontEndMenuManager.m_bMenuActive ||
        CPad::GetPad(0)->ArePlayerControlsDisabled();
    const auto &input=androidgame::GetPadInput();
    const float seconds=Min(0.1f,Max(0.0f,CTimer::GetTimeStep()/50.0f));
    const float right=paused ? 0 : CPad::GetPad(0)->GetPedWalkLeftRight()/128.0f;
    const float forward=paused ? 0 : -CPad::GetPad(0)->GetPedWalkUpDown()/128.0f;
    const float up=paused ? 0 : (input.a ? 1.0f : 0.0f)-(input.b ? 1.0f : 0.0f);
    const auto step=VrTraversal::Step(right,forward,up,!paused && input.rightTrigger >= 0.55f,seconds,flight);
    CVector front=TheCamera.Cams[TheCamera.ActiveCam].Front;
    front.z=0;
    if(front.MagnitudeSqr()<0.001f) front=CVector(0,1,0);
    front.Normalise();
    const CVector side(front.y,-front.x,0);
    CVector delta=front*step.forward+side*step.right+CVector(0,0,step.up);
    CVector position=player->GetPosition();
    if(!paused && VrTraversal::NeedsGround(flight,noclip)){
        fallSpeed=Max(-30.0f,fallSpeed-9.81f*seconds);
        delta.z=fallSpeed*seconds;
    }
    const int steps=Max(1,(int)ceilf(delta.Magnitude()/0.15f));
    const CVector increment=delta/(float)steps;
    for(int i=0;i<steps;++i){
        CVector next=position+increment;
        if(VrTraversal::OutOfBounds(next.x,next.y,next.z)){ Recover(player); return true; }
        if(!noclip && Blocked(player,next)) break;
        if(VrTraversal::NeedsGround(flight,noclip) && !paused){
            float height;
            if(Ground(next,position.z+0.6f,next.z-1.5f,height) && next.z<=height){
                if(height>position.z+0.6f) break;
                next.z=height;
                fallSpeed=0;
            }
        }
        if(VrTraversal::NeedsGround(flight,noclip) && Blocked(player,next,true)) break;
        position=next;
    }
    Place(player,position);
    player->bIsStanding=!flight && fallSpeed==0;
    player->m_nMoveState=(Abs(right)+Abs(forward)>0.1f) ? PEDMOVE_WALK : PEDMOVE_STILL;
    player->SetMoveAnim();
    return true;
}
}
#endif
