#include "common.h"
#include "DebugWorld.h"
#include "WorldSession.h"
#include "android.h"
#include "QuestProfiler.h"
#include "RendererSettings.h"
#include "VrWorldLod.h"
#include "main.h"
#include "Game.h"
#include "platform.h"
#include "crossplatform.h"
#include "Camera.h"
#include "Draw.h"
#include "Timer.h"
#include "Clock.h"
#include "Weather.h"
#include "Timecycle.h"
#include "Streaming.h"
#include "Zones.h"
#include "Collision.h"
#include "Renderer.h"
#include "VisibilityPlugins.h"
#include "World.h"
#include "Sprite2d.h"
#include "Font.h"
#include "Clouds.h"
#include "Particle.h"
#include "Coronas.h"
#include "Shadows.h"
#include "PointLights.h"
#include "Fluff.h"
#include "SpecialFX.h"
#include "Population.h"
#include "CarCtrl.h"
#include "Frontend.h"
#include "Lights.h"
#include "XrMath.h"
#include "MatchedProfile.h"
#include "RenderDebugProfile.h"
#include "xr_vulkan_session.h"
#include "vulkan/rwvk.h"
#include "vulkan/DebugVisualization.h"
#include <cstring>

extern bool DoRWStuffStartOfFrame_Horizon(int16,int16,int16,int16,int16,int16,int16);
extern void RenderScene();extern void RenderEffects();extern void DefinedState();
namespace RenderDebug {
static bool loaded=false;
static bool ambientPrepared=false;
float LodDistanceMultiplier(){return TheCamera.LODDistMultiplier;}
extern long long sessionTime;
extern xrvk::GraphicsContext sessionGraphics;
void SetView(const Camera &c,const Settings &s){
    const double fov=MatchedProfile::active?ProfileHorizontalFov(MatchedProfile::requested):c.fov;
    double x=c.rotation[0],y=c.rotation[1],z=c.rotation[2],w=c.rotation[3];
    CVector right(float(1-2*(y*y+z*z)),float(2*(x*y+z*w)),float(2*(x*z-y*w)));
    CVector forward(float(2*(x*y-z*w)),float(1-2*(x*x+z*z)),float(2*(y*z+x*w)));
    CVector up(float(2*(x*z+y*w)),float(2*(y*z-x*w)),float(1-2*(x*x+y*y)));
    CVector pos(float(c.position[0]),float(c.position[1]),float(c.position[2]));
    TheCamera.GetMatrix().GetRight()=-right;TheCamera.GetMatrix().GetForward()=forward;TheCamera.GetMatrix().GetUp()=up;TheCamera.GetMatrix().GetPosition()=pos;
    TheCamera.m_vecGameCamPos=pos;TheCamera.Cams[TheCamera.ActiveCam].Source=pos;TheCamera.Cams[TheCamera.ActiveCam].Front=forward;TheCamera.Cams[TheCamera.ActiveCam].Up=up;TheCamera.Cams[TheCamera.ActiveCam].FOV=float(fov);
    const float cameraDistance=70.f/float(fov);
    TheCamera.LODDistMultiplier=WorldLodPolicy::DetailDistanceMultiplier(cameraDistance*CRenderer::ms_lodDistScale);
    TheCamera.GenerationDistMultiplier=WorldLodPolicy::GenerationDistanceMultiplier(cameraDistance);
    WorldSession::Observe(pos.x,pos.y,pos.z);
    CDraw::SetFOV(float(fov));CDraw::SetAspectRatio(float(s.width)/s.height);TheCamera.CalculateDerivedValues();
    auto frame=RwCameraGetFrame(Scene.camera);auto matrix=RwFrameGetMatrix(frame);
    *RwMatrixGetPos(matrix)=pos;*RwMatrixGetAt(matrix)=forward;*RwMatrixGetUp(matrix)=up;*RwMatrixGetRight(matrix)=-right;RwMatrixUpdate(matrix);RwFrameUpdateObjects(frame);
    RwCameraSetNearClipPlane(Scene.camera,float(s.nearClip));RwCameraSetFarClipPlane(Scene.camera,float(s.farClip));RwCameraSetFogDistance(Scene.camera,std::min(float(s.farClip),CTimeCycle::GetFogStart()));
    CDraw::SetNearClipZ(float(s.nearClip));CDraw::SetFarClipZ(float(s.farClip));
    RwV2d window={float(std::tan(c.fov*3.141592653589793/360)),float(std::tan(c.fov*3.141592653589793/360)*s.height/s.width)};RwCameraSetViewWindow(Scene.camera,&window);
    rw::vulkan::setFirstPersonAnchor(nullptr,0,0,0);rw::vulkan::clearFirstPersonEyePoses();
    float origin[3]={},identity[4]={0,0,0,1};rw::vulkan::setHeadPose(origin,0,identity);
    XrMath::Mat4 vp[2];for(int i=0;i<2;++i){XrPosef pose{};pose.orientation.w=1;pose.position.x=s.views==2?float((i?1:-1)*s.ipd*.5):0;
        const float half=float(c.fov*3.141592653589793/360),vertical=std::atan(std::tan(half)*s.height/s.width);XrFovf fov{-half,half,vertical,-vertical};
        if(MatchedProfile::active&&s.views==2){auto get=[&](const char *part){return float(MatchedProfile::requested.at("eye"+std::to_string(i)+"_"+part));};pose.position={get("px"),get("py"),get("pz")};pose.orientation={get("qx"),get("qy"),get("qz"),get("qw")};fov={get("left"),get("right"),get("up"),get("down")};}
        vp[i]=XrMath::multiply(XrMath::projectionFromFov(fov,float(s.nearClip),float(s.farClip)),XrMath::viewFromPose(pose));
    }
    rw::vulkan::setStereoViewProjection(vp[0].m,vp[1].m);rw::vulkan::setSgsrHorizontalFovDegrees(float(fov));
}
void InitializeWorld(){
    Check(WorldSession::IsViewer(),"Observation world session was not selected");
    Check(CGame::InitialiseOnceAfterRW(),"Initialize shared world services");
}
void ShutdownWorld(){if(loaded){CGame::ShutDown();loaded=false;}}
void ResetWorld(const Settings &s,const Camera &c){
    // Reload the shared world for each measured pass to reset particles, streamed
    // entities and population as well as camera/time. Warmup stays unmeasured.
    if(loaded)vkDeviceWaitIdle(sessionGraphics.device);
    ShutdownWorld();mysrand(1);Check(CGame::Initialise("DATA\\GTA3.DAT"),"Reset observation world");loaded=true;
    Check(FindPlayerPed()==nullptr,"Observation session unexpectedly created a player");
    FrontEndMenuManager.m_bGameNotLoaded=false;FrontEndMenuManager.m_bMenuActive=false;CGame::playingIntro=false;gGameState=GS_PLAYING_GAME;
    CTimer::Initialise();CTimer::SetTimeInMilliseconds(0);CTimer::SetPreviousTimeInMilliseconds(0);CTimer::SetFrameCounter(0);
    WorldSession::ambient=s.traffic||s.pedestrians;CWeather::ForceWeatherNow(s.weather);SetView(c,s);
    ambientPrepared=false;
    if(WorldSession::ambient){CStreaming::LoadInitialVehicles();CStreaming::StreamZoneModels(TheCamera.GetPosition());ambientPrepared=true;}
    CStreaming::LoadScene(TheCamera.GetPosition());CStreaming::LoadAllRequestedModels(false);
}
void UpdateWorld(const Settings &s,const Camera &c,double elapsed,double step){
    SetView(c,s);WorldSession::ambient=s.traffic||s.pedestrians;
    if(WorldSession::ambient&&!ambientPrepared){CStreaming::LoadInitialVehicles();CStreaming::StreamZoneModels(TheCamera.GetPosition());CStreaming::LoadAllRequestedModels(false);ambientPrepared=true;}
    const double simulation=elapsed;sessionTime=static_cast<long long>(std::llround(simulation*1e9));
    CTimer::UpdateObservationTime(uint64(sessionTime),s.paused?0:uint64(std::llround(step*1e9)));
    double hour=std::fmod(s.hour+(s.clock?simulation/60:0),24.);CClock::ms_nGameClockHours=uint8(hour);CClock::ms_nGameClockMinutes=uint8((hour-std::floor(hour))*60);CClock::ms_nGameClockSeconds=uint16(std::fmod(hour*3600,60));
    CWeather::OldWeatherType=s.weather;CWeather::NewWeatherType=s.nextWeather;CWeather::InterpolationValue=float(s.weatherBlend);if(!s.paused)CWeather::Update();CTimeCycle::Update();
    CTheZones::Update();CCollision::Update();
    androidgame::QuestProfilerBeginPhase(androidgame::QUEST_PROFILER_PHASE_STREAMING);CStreaming::Update();androidgame::QuestProfilerEndPhase(androidgame::QUEST_PROFILER_PHASE_STREAMING);
    androidgame::VrPushDynamicLights();
    CSprite2d::InitPerFrame();CFont::InitPerFrame();CPointLights::InitPerFrame();
    if(!s.paused){CClouds::Update();CMovingThings::Update();CParticle::Update();CSpecialFX::Update();CCoronas::DoSunAndMoon();CCoronas::Update();CShadows::UpdateStaticShadows();CShadows::UpdatePermanentShadows();
        if(s.pedestrians)CPopulation::Update();if(s.traffic){CCarCtrl::bCarsGeneratedAroundCamera=true;CCarCtrl::GenerateRandomCars();CCarCtrl::RemoveDistantCars();}
        if(WorldSession::ambient)CWorld::Process();
    }
    Check(FindPlayerPed()==nullptr,"Player created during observation update");
}
void RenderWorld(const Settings &s){
    VrWorldLod::BeginFrame(androidgame::VrWorldLodMode());
    auto &v=rw::vulkan::DebugVisualization::Get();v.mesh=decltype(v.mesh)(s.mesh);v.shading=decltype(v.shading)(s.shading);v.bounds=s.bounds;v.collision=s.collision;v.velocity=s.velocity;v.range=s.range;for(int i=0;i<9;++i)v.visible[i]=s.layers[i];
    rw::vulkan::uploadFinishedMips();SetLightsWithTimeOfDayColour(Scene.world);
    androidgame::QuestProfilerBeginPhase(androidgame::QUEST_PROFILER_PHASE_WORLD_LIST);CRenderer::ConstructRenderList();androidgame::QuestProfilerEndPhase(androidgame::QUEST_PROFILER_PHASE_WORLD_LIST);
    CRenderer::PreRender();
    Check(DoRWStuffStartOfFrame_Horizon(CTimeCycle::GetSkyTopRed(),CTimeCycle::GetSkyTopGreen(),CTimeCycle::GetSkyTopBlue(),CTimeCycle::GetSkyBottomRed(),CTimeCycle::GetSkyBottomGreen(),CTimeCycle::GetSkyBottomBlue(),255),"Begin shared world camera");
    DefinedState();CRenderer::SetVrPerViewCulling(true);RenderScene();RenderEffects();CRenderer::SetVrPerViewCulling(false);RwCameraEndUpdate(Scene.camera);
    RendererSettings::ApplyFrame(false);
    rw::vulkan::setFogParams(androidgame::VrDistanceFogEnabled()?CTimeCycle::GetFogStart():0,androidgame::VrDistanceFogEnabled()?float(s.farClip):0);
}
}
