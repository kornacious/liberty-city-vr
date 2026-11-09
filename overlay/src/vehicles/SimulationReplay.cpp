#include "common.h"
#include "SimulationReplay.h"
#if defined(__ANDROID__) || defined(LIBERTYVR_WINDOWS)
#include "SimulationReplayCore.h"
#include "BenchmarkIdentity.h"
#include "SourceIdentity.h"
#include "Clock.h"
#include "Weather.h"
#include "platform_android.h"
#include "Automobile.h"
#include "HandlingMgr.h"
#include "Pools.h"
#include "World.h"
#include "Replay.h"
#include "Timer.h"
#include "Ped.h"
#include "ModelSets.h"
#include "QuestProfiler.h"
#include "QuestRenderDoc.h"
#include <cerrno>
#include "vulkan/rwvk.h"
#include "Streaming.h"
#include "Game.h"
#include "Zones.h"
#include "Collision.h"
#include "ModelIndices.h"
#include "CarCtrl.h"
#include <algorithm>
#include <dirent.h>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <time.h>

extern bool SpawnVrVehicleForReplay(int model);

namespace androidgame { namespace {
namespace Core=SimulationReplayCore;
#ifndef LCVR_SIMULATION_REPLAY_ROOT
#define LCVR_SIMULATION_REPLAY_ROOT "/sdcard/Android/data/com.libertyvr.quest/files/simulation-replays"
#endif
const char *Root(){
#ifdef LIBERTYVR_WINDOWS
    static std::string root=std::string(platform::storageRoot())+"/simulation-replays";
    return root.c_str();
#else
    return LCVR_SIMULATION_REPLAY_ROOT;
#endif
}
enum Mode { Idle,Recording,Preparing,Playing,Finishing };
Mode mode=Idle;
int request=0,reference=-1,selectedDemo=0;
unsigned repeatCount=1,runRepeatCount=1,runIteration=1;
bool repeatPending=false;
unsigned model=0,signature=0,duration=120,sampleCount=0;
bool ghost=true,runGhost=false,runAutoProfile=false,profileStarted=false,sampled=false;
bool recordingGhost=true,startFailed=false;
bool routeAssistance=true,runAssisted=false;
bool forwardView=true,runForwardView=false;
bool matchedWorkload=true,runMatched=false;
std::string routeIdentity,assetIdentity;
decltype(CClock::ms_nGameClockHours) oldHours=0,oldMinutes=0;
decltype(CClock::ms_nGameClockSeconds) oldSeconds=0;
decltype(CWeather::OldWeatherType) oldWeather=0,newWeather=0,forcedWeather=0;
float weatherInterpolation=0;
void FixedEnvironment(){
    CClock::ms_nGameClockHours=12; CClock::ms_nGameClockMinutes=0; CClock::ms_nGameClockSeconds=0;
    CWeather::OldWeatherType=CWeather::NewWeatherType=CWeather::ForcedWeatherType=0;
    CWeather::InterpolationValue=0;
}
bool renderdocCapture=false,runRenderdocCapture=false,captureAttempted=false,captureFrameActive=false;
uint64_t captureAtUs=37000000,runCaptureAtUs=0,capturedAtUs=0,captureStartNs=0,captureEndNs=0;
std::string captureResult="off",capturePrefix,captureFile;
#ifndef LCVR_RENDERDOC_CAPTURE_ROOT
#define LCVR_RENDERDOC_CAPTURE_ROOT "/sdcard/Android/data/com.libertyvr.quest/files/renderdoc-captures"
#endif
#ifdef LIBERTYVR_WINDOWS
const char *WindowsRenderDocRoot() { static std::string root=std::string(platform::storageRoot())+"/renderdoc-captures"; return root.c_str(); }
#undef LCVR_RENDERDOC_CAPTURE_ROOT
#define LCVR_RENDERDOC_CAPTURE_ROOT WindowsRenderDocRoot()
#endif
const char *RenderDocRoot(){return LCVR_RENDERDOC_CAPTURE_ROOT;}
unsigned corrections=0;
float correctionMax=0;
std::string deleteArmed;
FILE *output=nullptr;
std::unique_ptr<Core::Tape> tape;
Core::Initial initial;
Core::Sample applied;
Core::Drift drift;
std::vector<std::string> demos;
std::string label,runId,result,profile;
char status[144]="ENTER AN AUTOMOBILE TO RECORD A ROUTE";
uint64_t elapsed=0,lastInput=0,wallStart=0;
uint64_t requestStartNs=0;
uint64_t playbackStartNs=0,playbackEndNs=0,profileStartUs=0;
double gameTime=0,warmTime=0;
float frameDt=0;
uint64_t Now(){ timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return uint64_t(t.tv_sec)*1000000000ULL+t.tv_nsec; }
void Status(const char *s){ std::snprintf(status,sizeof(status),"%s",s); std::fprintf(stderr,"Simulation replay: %s\n",s); }
std::string Unique(){
    const time_t now=time(nullptr); tm date={}; gmtime_r(&now,&date);
    char stamp[32],id[64]; strftime(stamp,sizeof(stamp),"%Y%m%dT%H%M%SZ",&date);
    std::snprintf(id,sizeof(id),"%s-%llu",stamp,(unsigned long long)(Now()/1000000)); return id;
}
std::string Path(const std::string &name){ return std::string(Root())+"/"+name; }
void Refresh(){
    const std::string previous=selectedDemo<int(demos.size())?demos[selectedDemo]:"";
    demos.clear(); mkdir(Root(),0700);
    DIR *dir=opendir(Root()); if(!dir) return;
    while(dirent *e=readdir(dir)) {
        const std::string n=e->d_name;
        if(n.size()>8 && n.size()<80 && n.substr(n.size()-8)==".sim.csv") {
            const std::string stem=n.substr(0,n.size()-8);
            // Generated names have a longer suffix than the legacy command label.
            bool valid=!stem.empty(); for(char c:stem) if(!((c>='0'&&c<='9') || c=='T' || c=='Z' || c=='-')) valid=false;
            struct stat st={}; const std::string path=Path(n);
            if(valid && lstat(path.c_str(),&st)==0 && S_ISREG(st.st_mode)) demos.push_back(stem);
        }
    }
    closedir(dir); std::sort(demos.begin(),demos.end());
    const auto found=std::find(demos.begin(),demos.end(),previous);
    selectedDemo=found==demos.end()?std::max(0,int(demos.size())-1):int(found-demos.begin());
}
CAutomobile *Selected(){
    CVehicle *v=FindPlayerVehicle();
    return v && v->IsCar() && v->GetStatus()==STATUS_PLAYER?static_cast<CAutomobile *>(v):nullptr;
}
CAutomobile *Bound(){
    CVehicle *v=reference>=0?CPools::GetVehicle(reference):nullptr;
    return v && v->IsCar() && unsigned(v->GetModelIndex())==model?static_cast<CAutomobile *>(v):nullptr;
}
CAutomobile *Target(){ CAutomobile *c=Bound(); return c==Selected()?c:nullptr; }
CAutomobile *Ready(){
    CAutomobile *c=Selected();
    if(!c || CWorld::bProcessCutsceneOnly || CReplay::IsPlayingBack()){Status("ENTER AN AUTOMOBILE IN NORMAL GAMEPLAY");return nullptr;}
    if(c->GetUp().z<0.7f || c->m_vecMoveSpeed.MagnitudeSqr()>0.0004f){Status("STOP THE CAR UPRIGHT BEFORE STARTING");return nullptr;}
    return c;
}
std::array<double,Core::HandlingCount> Handling(CAutomobile *c){
    const tHandlingData &h=*c->pHandling;
    // Canonical text, never compiler padding or pointer values. Include transmission.
    const double values[]={c->m_fMass,c->m_fTurnMass,h.fTractionMultiplier,h.fTractionLoss,h.fBrakeDeceleration,h.fBrakeBias,
        h.fSuspensionForceLevel,h.fSuspensionDampingLevel,h.fSuspensionBias,h.fSteeringLock,
        h.fTractionBias,h.fSuspensionUpperLimit,h.fSuspensionLowerLimit,h.fBuoyancy,h.fCollisionDamageMultiplier,
        h.Transmission.fEngineAcceleration,h.Transmission.fMaxVelocity,h.Transmission.fMaxCruiseVelocity,h.Transmission.fMaxReverseVelocity,
        double(h.Transmission.nNumberOfGears),double(h.Transmission.nDriveType),double(h.Transmission.nEngineType),double(h.Transmission.Flags),
        double(h.bABS),double(h.Flags),c->m_vecCentreOfMass.x,c->m_vecCentreOfMass.y,c->m_vecCentreOfMass.z};
    std::array<double,Core::HandlingCount> out{};size_t i=0;
    for(double v:values)out[i++]=v;
    for(const auto &g:h.Transmission.Gears){
        out[i++]=g.fMaxVelocity;out[i++]=g.fShiftUpVelocity;out[i++]=g.fShiftDownVelocity;
    }
    return out;
}
unsigned Signature(CAutomobile *c){
    unsigned hash=2166136261u;
    for(double value:Handling(c)){const float v=float(value);char text[40];std::snprintf(text,sizeof(text),"%.9g;",double(v));for(const char *p=text;*p;++p)hash=(hash^unsigned(*p))*16777619u;}
    return hash;
}
CVector Vec(const float *v){ return CVector(v[0],v[1],v[2]); }
void Repair(CAutomobile *c){ c->Fix(); c->ExtinguishCarFire(); c->m_fHealth=1000; c->m_fFireBlowUpTimer=0; }
Core::Initial Initial(CAutomobile *c){
    Core::Initial s; s.vehicle.model=c->GetModelIndex(); s.vehicle.signature=Signature(c);
    const CVector vectors[]={c->GetPosition(),c->GetRight(),c->GetForward(),c->GetUp(),c->m_vecMoveSpeed,c->m_vecTurnSpeed};
    int i=0; for(const CVector &v:vectors){s.vehicle.pose[i++]=v.x;s.vehicle.pose[i++]=v.y;s.vehicle.pose[i++]=v.z;}
    for(i=0;i<5;++i) s.assets[i]=int(ModelSets::GetActiveForCategory(ModelSets::eModelCategory(i)));
    s.gear=int(c->m_nCurrentGear); s.gearTime=c->m_fChangeGearTime;
    s.portableHandling=true;s.handling=Handling(c);return s;
}
void Restore(CAutomobile *c){
    Repair(c);
    const float *p=initial.vehicle.pose;
    c->Teleport(Vec(p)); CWorld::Remove(c);
    c->GetRight()=Vec(p+3); c->GetForward()=Vec(p+6); c->GetUp()=Vec(p+9);
    c->m_vecMoveSpeed=Vec(p+12); c->m_vecTurnSpeed=Vec(p+15);
    c->m_vecMoveFriction=c->m_vecTurnFriction=CVector(0,0,0);
    c->m_vecMoveSpeedAvg=c->m_vecMoveSpeed; c->m_vecTurnSpeedAvg=c->m_vecTurnSpeed; c->m_nStaticFrames=0;
    c->m_nCurrentGear=uint8(initial.gear); c->m_fChangeGearTime=initial.gearTime;
    // Native Teleport resets current suspension contact. Clear the remaining
    // wheel history identically at record and playback start, too.
    for(int i=0;i<4;++i){
        c->m_aSuspensionSpringRatioPrev[i]=1; c->m_aWheelSpeed[i]=0;
        c->m_aGroundPhysical[i]=nullptr;c->m_aGroundOffset[i]=CVector(0,0,0);
    }
    c->m_fDamageImpulse=0;c->bIsStuck=false;c->bHasContacted=false;c->bHasHitWall=false;
    c->GetMatrix().UpdateRW(); c->UpdateRwFrame(); CWorld::Add(c);
}
bool FollowRoute(CAutomobile *c){
    Core::RouteState state;
    if(!tape->Route(applied,state)) return false;
    const float distance=(c->GetPosition()-Vec(state.position)).Magnitude();
    if(!std::isfinite(distance)) return false;
    correctionMax=std::max(correctionMax,distance);++corrections;
    // Relink sectors without toggling audio or moving-list membership, repairing
    // the car, or resetting suspension every frame. Physics still executes next.
    c->SetPosition(Vec(state.position));c->GetRight()=Vec(state.right);
    c->GetForward()=Vec(state.forward);c->GetUp()=Vec(state.up);
    c->m_vecMoveSpeed=Vec(state.move);c->m_vecTurnSpeed=Vec(state.turn);
    c->m_vecMoveFriction=c->m_vecTurnFriction=CVector(0,0,0);
    c->m_vecMoveSpeedAvg=c->m_vecMoveSpeed;c->m_vecTurnSpeedAvg=c->m_vecTurnSpeed;c->m_nStaticFrames=0;
    if(distance>2){
        c->ResetSuspension();
        for(int i=0;i<4;++i){c->m_aGroundPhysical[i]=nullptr;c->m_aGroundOffset[i]=CVector(0,0,0);c->m_aSuspensionSpringRatioPrev[i]=1;}
        c->m_fDamageImpulse=0;c->bIsStuck=false;c->bHasContacted=false;c->bHasHitWall=false;
    }
    c->GetMatrix().UpdateRW();c->UpdateRwFrame();c->RemoveAndAdd();
    return true;
}
void Finish(const char *reason){
    if(mode==Idle || mode==Finishing) return;
    result=reason;
    playbackEndNs=Now();
    if(output){
        bool valid=sampleCount && elapsed>lastInput && !std::ferror(output);
        if(valid) valid=std::fprintf(output,"END,%llu\n",(unsigned long long)elapsed)>0;
        if(std::fclose(output)!=0) valid=false;
        output=nullptr;
        if(!valid){std::remove(Path(label+".sim.csv").c_str()); result="EMPTY OR FAILED RECORDING";}
    }
    mode=Finishing; // Finish profiling after this rendered app frame, before cleanup.
    Status(result.c_str());
}
bool WriteReport(){
    if(runId.empty()) return true;
    FILE *f=std::fopen(Path(runId+".run.csv").c_str(),"wx");
    if(!f){Status("RUN REPORT WRITE FAILED");return false;}
    std::fprintf(f,"schema,LCVR_SIM_RUN_V1\ndemo,%s\nresult,%s\nghost,%d\nauto_profile,%d\nprofile,%s\nmodel,%u\nhandling,%u\nelapsed_us,%llu\ncomparison_samples,%u\nposition_rms_m,%.9g\nposition_max_m,%.9g\nspeed_max_error_kmh,%.9g\nheading_max_error_deg,%.9g\nbuild,%s %s\n",
        label.c_str(),result.c_str(),runGhost?1:0,runAutoProfile?1:0,profile.c_str(),model,initial.vehicle.signature,(unsigned long long)elapsed,
        drift.count,drift.Rms(),double(drift.maxPosition),double(drift.maxSpeed),double(drift.maxHeading),__DATE__,__TIME__);
    for(int i=0;i<5;++i) std::fprintf(f,"assets_%d,%d\n",i,initial.assets[i]);
    std::fprintf(f,"execution_handling,%u\nhandling_match_policy,%s\n",signature,initial.portableHandling?"numeric_abs1e-6_rel1e-5_flags_exact":"legacy_exact_hash");
    std::fprintf(f,"recording_ghost,%d\n",initial.recordingGhost);
    std::fprintf(f,"view_policy,%s\n",runForwardView?"fixed_forward":"live_head_tracking");
    std::fprintf(f,"renderdoc_requested,%d\nrenderdoc_target_us,%llu\nrenderdoc_result,%s\nrenderdoc_actual_us,%llu\nrenderdoc_start_ns,%llu\nrenderdoc_end_ns,%llu\nrenderdoc_file,%s\n",
        runRenderdocCapture?1:0,(unsigned long long)runCaptureAtUs,captureResult.c_str(),
        (unsigned long long)capturedAtUs,(unsigned long long)captureStartNs,(unsigned long long)captureEndNs,captureFile.c_str());
    std::fprintf(f,"ghost_policy,traffic_props_preserve_roads\n");
    std::fprintf(f,"tracking,%s\nroute_corrections,%u\nroute_correction_max_m,%.9g\nreference_state,%s\n",
        runAssisted?"route_assisted":"input_only",corrections,double(correctionMax),runAssisted?"position_heading_and_estimated_path_velocity":"unmodified_simulation");
    std::fprintf(f,"playback_start_monotonic_ns,%llu\nplayback_end_monotonic_ns,%llu\nprofile_start_simulation_us,%llu\n",
        (unsigned long long)playbackStartNs,(unsigned long long)playbackEndNs,(unsigned long long)profileStartUs);
    std::fprintf(f,"matched_workload,%d\nroute_fingerprint,%s\nasset_fingerprint,%s\nsource_identity,%s\ncompatibility_dirty_at_end,%d\nenvironment,%s\nambient_world,not_restored_rng_traffic_particles_audio_not_deterministic\nfingerprint_policy,fnv1a64_local_workload_not_security_digest\n",
        runMatched?1:0,routeIdentity.c_str(),assetIdentity.c_str(),LCVR_SOURCE_IDENTITY,MatchedProfile::dirty?1:0,
        runMatched?"noon_sunny_each_simulation_step":"live");
    std::fprintf(f,"repeat_index,%u\nrepeat_count,%u\n",runIteration,runRepeatCount);
    const bool written=!std::ferror(f);
    if(std::fclose(f)!=0 || !written){Status("RUN REPORT CLOSE FAILED");return false;}
    return true;
}
bool Start(int operation,bool continuing=false){
    if(mode!=Idle) {Status("STOP THE CURRENT RUN FIRST");return false;}
    if(CWorld::bProcessCutsceneOnly || CReplay::IsPlayingBack() || !FindPlayerPed()){
        Status("RESUME NORMAL GAMEPLAY BEFORE STARTING A RUN");return false;
    }
    CAutomobile *c=nullptr;
    Core::Initial current;
    mkdir(Root(),0700);
    if(operation==2){
        Refresh(); if(demos.empty()){Status("NO RECORDED DEMOS");return false;}
        if(continuing){
            const auto found=std::find(demos.begin(),demos.end(),label);
            if(found==demos.end()){Status("RECORDED DEMO NO LONGER AVAILABLE");return false;}
            selectedDemo=int(found-demos.begin());
        }
        label=demos[selectedDemo];
        FILE *f=std::fopen(Path(label+".sim.csv").c_str(),"r");
        tape.reset(new Core::Tape); const bool loaded=f && tape->Load(f); if(f) std::fclose(f);
        if(!loaded){tape.reset();Status("INCOMPLETE OR INVALID DEMO");return false;}
        if(tape->endUs<1000000){tape.reset();Status("RECORD AT LEAST ONE SECOND BEFORE PLAYBACK");return false;}
        Core::Initial &s=tape->initial;
        c=Selected();
        if(!c || c->GetModelIndex()!=s.vehicle.model){
            if(!SpawnVrVehicleForReplay(int(s.vehicle.model)) || !(c=Selected()) ||
                c->GetModelIndex()!=s.vehicle.model){
                tape.reset();Status("COULD NOT SPAWN AND ENTER RECORDED CAR MODEL");return false;
            }
        }
        current=Initial(c);
        // The current pose/velocity may be moving or overturned. Playback replaces
        // those with the validated tape state; only model/handling/assets must match.
        std::fprintf(stderr,"Simulation replay validation: model=%u/%u handling=%u/%u gear=%d/%d\n",unsigned(current.vehicle.model),unsigned(s.vehicle.model),current.vehicle.signature,s.vehicle.signature,s.gear,c->pHandling->Transmission.nNumberOfGears);
        if(s.vehicle.model!=current.vehicle.model){tape.reset();Status("USE THE RECORDED CAR MODEL");return false;}
        size_t difference=0;
        if(!Core::HandlingMatches(s,current,&difference) || s.gear>c->pHandling->Transmission.nNumberOfGears){
            if(s.portableHandling) std::fprintf(stderr,"Simulation replay handling difference: field=%zu recorded=%.9g current=%.9g\n",difference,double(s.handling[difference]),double(current.handling[difference]));
            tape.reset();Status("RECORDED CAR HANDLING DOES NOT MATCH");return false;
        }
        bool match=true;for(int i=0;i<5;++i) match=match && s.assets[i]==current.assets[i];
        if(!match){tape.reset();Status("RECORDED MODEL SETS DO NOT MATCH");return false;}
        if(!s.portableHandling){
            // Only the original exact-matching setup may upgrade a legacy tape.
            // Keep the original file intact; publish a fully written new tape.
            s.portableHandling=true;s.handling=current.handling;
            const auto upgraded=Unique();const auto path=Path(upgraded+".sim.csv");
            FILE *file=std::fopen(path.c_str(),"wx");bool saved=file && Core::Header(file,s);
            if(file){for(const auto &sample:tape->samples)if(saved)saved=Core::Write(file,sample);
                if(saved)saved=std::fprintf(file,"END,%llu\n",(unsigned long long)tape->endUs)>0;
                if(std::fclose(file)!=0)saved=false;}
            if(!saved){if(file)std::remove(path.c_str());tape.reset();Status("PORTABLE ROUTE UPGRADE FAILED - ORIGINAL KEPT");return false;}
            label=upgraded;Status("PORTABLE ROUTE COPY CREATED - ORIGINAL KEPT");
        }
        if(renderdocCapture){
            if(!QuestRenderDocAvailable()){tape.reset();Status("LAUNCH THE GAME THROUGH RENDERDOC TO USE TIMED CAPTURE");return false;}
            if(captureAtUs>=tape->endUs){tape.reset();Status("RENDERDOC CAPTURE TIME MUST BE BEFORE THE DEMO END");return false;}
            if(mkdir(RenderDocRoot(),0700)!=0 && errno!=EEXIST){tape.reset();Status("CANNOT CREATE LOCAL RENDERDOC CAPTURE DIRECTORY");return false;}
        }
        initial=s;
        runMatched=matchedWorkload || MatchedProfile::active;
        routeIdentity.clear(); assetIdentity.clear();
        if(runMatched){
            if(!ghost || !routeAssistance || !forwardView || renderdocCapture){
                tape.reset();Status("MATCHED RUN NEEDS GHOST, ROUTE, FORWARD VIEW; NO RENDERDOC");return false;
            }
            try {
                routeIdentity=BenchmarkIdentity::Hex(BenchmarkIdentity::File(14695981039346656037ULL,Path(label+".sim.csv")));
                assetIdentity=BenchmarkIdentity::Assets(platform::gameDataRoot());
            } catch(...) { tape.reset();Status("BENCHMARK ASSET FINGERPRINT FAILED");return false; }
            oldHours=CClock::ms_nGameClockHours;oldMinutes=CClock::ms_nGameClockMinutes;oldSeconds=CClock::ms_nGameClockSeconds;
            oldWeather=CWeather::OldWeatherType;newWeather=CWeather::NewWeatherType;forcedWeather=CWeather::ForcedWeatherType;
            weatherInterpolation=CWeather::InterpolationValue;
            FixedEnvironment();
        }
    }else{
        c=Ready();if(!c) return false;
        current=Initial(c);
        if(!Core::Valid(current) || current.gear>c->pHandling->Transmission.nNumberOfGears){Status("INVALID VEHICLE STATE");return false;}
        initial=current;
        runMatched=false;
        initial.recordingGhost=recordingGhost?1:0;
        label=Unique(); output=std::fopen(Path(label+".sim.csv").c_str(),"wx");
        if(!output){Status("CANNOT CREATE DEMO FILE");return false;}
        std::setvbuf(output,nullptr,_IOFBF,65536);
        if(!Core::Header(output,initial)){std::fclose(output);output=nullptr;std::remove(Path(label+".sim.csv").c_str());Status("DEMO HEADER WRITE FAILED");return false;}
    }
    reference=CPools::GetVehicleRef(c);model=unsigned(c->GetModelIndex());signature=current.vehicle.signature;
    sampleCount=0; elapsed=lastInput=0; gameTime=warmTime=0; drift={}; wallStart=Now();
    playbackStartNs=playbackEndNs=profileStartUs=0;
    runGhost=operation==1?recordingGhost:ghost;runAutoProfile=operation==2;profileStarted=false;profile.clear();runId.clear();
    runAssisted=operation==2 && routeAssistance && runGhost;corrections=0;correctionMax=0;
    runForwardView=operation==2 && forwardView;
    runRenderdocCapture=operation==2 && renderdocCapture;runCaptureAtUs=captureAtUs;
    captureAttempted=captureFrameActive=false;capturedAtUs=captureStartNs=captureEndNs=0;
    captureResult=runRenderdocCapture?"armed":"off";capturePrefix.clear();captureFile.clear();
    if(operation==2){
        runId=Unique();
        if(runRenderdocCapture)capturePrefix=std::string(RenderDocRoot())+"/"+runId;
        QuestProfilerSetReplayProfiling(1);
        // Match the game's waypoint teleport: load the island's collision and
        // scene before moving the occupied car. Preparation stays unprofiled.
        const CVector start=Vec(initial.vehicle.pose);
        if((c->GetPosition()-start).MagnitudeSqr()>10000){
            Status("LOADING RECORDED START AREA");
            CTimer::Stop();
            CGame::currLevel=CTheZones::GetLevelFromPosition(&start);
            CCollision::SortOutCollisionAfterLoad();
            CStreaming::LoadScene(start);
            CTimer::Update();
        }
        Restore(c); mode=Preparing; MatchedProfile::benchmark=runMatched;
        Status(runForwardView?"PREPARING PLAYBACK - FIXED FORWARD VIEW":"PREPARING PLAYBACK - LIVE HEAD TRACKING");
    }else{Restore(c);mode=Recording;Status("RECORDING - OPEN THIS MENU TO STOP");}
    return true;
}
bool Protected(CEntity *e){
    if(!e || !runGhost || (mode!=Recording && mode!=Preparing && mode!=Playing && mode!=Finishing)) return false;
    CAutomobile *c=Bound(); if(!c) return false;
    return e==c || e==(CEntity *)FindPlayerPed() || (e->IsPed() && ((const CPed *)e)->m_pMyVehicle==c);
}
} // namespace

bool SimulationReplayProtected(CEntity *e){return Protected(e);}
static bool GhostObstacle(CEntity *e){
    // Buildings/terrain remain solid. Props can be physical objects, unloaded
    // dummy objects, or known street furniture represented as static buildings.
    if(CCarCtrl::ThisRoadObjectCouldMove(e->GetModelIndex())) return false;
    return e->IsPed()||e->IsVehicle()||e->IsObject()||e->IsDummy()||
        IsStreetLight(e->GetModelIndex())||IsTreeModel(e->GetModelIndex())||IsFence(e->GetModelIndex());
}
bool SimulationReplayIgnoreCollision(CEntity *a,CEntity *b){
    if(!a || !b || !runGhost || mode==Idle) return false;
    const bool protectedA=Protected(a),protectedB=Protected(b);
    if(!protectedA&&!protectedB) return false;
    return Core::IgnorePair(true,protectedA,protectedB,GhostObstacle(a),GhostObstacle(b));
}
bool SimulationReplayIgnoreDamage(CEntity *victim,CEntity *attacker){return Protected(victim)||Protected(attacker);}
void SimulationReplayBeginFrame(){
    if(runMatched && (mode==Preparing || mode==Playing)) FixedEnvironment();
    if(request==1 || request==2){
        const int op=request;request=0;
        const bool continuing=repeatPending;repeatPending=false;
        if(!continuing){runIteration=1;runRepeatCount=op==2?repeatCount:1;}
        startFailed=!Start(op,continuing);
        if(startFailed && continuing)QuestProfilerSetReplayProfiling(0);
    }
    if(mode==Idle || mode==Finishing) return;
    CAutomobile *c=Target();
    if(!c || CWorld::bProcessCutsceneOnly || CReplay::IsPlayingBack()){Finish("ABORTED - VEHICLE EXITED OR GAMEPLAY CHANGED");return;}
    if(Signature(c)!=signature){Finish("ABORTED - HANDLING CHANGED");return;}
    if(Now()-wallStart>300000000000ULL){Finish("ABORTED - WALL TIME LIMIT");return;}
    frameDt=CTimer::GetTimeStepInSeconds();sampled=false;
    if(!std::isfinite(frameDt) || frameDt<0 || frameDt>0.25f){Finish("ABORTED - INVALID SIMULATION TIMESTEP");return;}
    if(mode==Preparing && warmTime>=2){Restore(c);mode=Playing;playbackStartNs=Now();Status(runAssisted?"PLAYING - ROUTE ASSISTANCE ACTIVE":"PLAYING - INPUT ONLY, DRIFT LIMIT 50M");}
    if(mode==Playing){
        if(!tape->At(elapsed,applied)){Finish("COMPLETE");return;}
        const CVector p=c->GetPosition(),f=c->GetForward();
        const float position[]={p.x,p.y,p.z},forward[]={f.x,f.y,f.z};
        const bool withinLimit=drift.Add(applied,position,forward,c->m_vecMoveSpeed.Magnitude()*180.0f);
        if(drift.invalid){Finish("ABORTED - INVALID VEHICLE STATE");return;}
        if(runAssisted){
            if(!FollowRoute(c)){Finish("ABORTED - INVALID ROUTE REFERENCE OR VEHICLE STATE");return;}
        }else if(!withinLimit){Finish("ABORTED - ROUTE DRIFT EXCEEDED 50 METERS");return;}
    }
}
void SimulationReplayInputs(CAutomobile *c){
    if(mode==Idle || mode==Finishing || c!=Target() || frameDt<=0) return;
    if(mode==Preparing){c->m_fGasPedal=0;c->m_fBrakePedal=1;c->m_fSteerAngle=0;c->bIsHandbrakeOn=true;sampled=true;return;}
    if(mode==Playing){c->m_fGasPedal=applied.input.gas;c->m_fBrakePedal=applied.input.brake;c->m_fSteerAngle=applied.input.steer;c->bIsHandbrakeOn=applied.input.handbrake!=0;}
    if(sampled) return; sampled=true;
    if(mode==Recording){
        Core::Sample s; s.input={elapsed,c->m_fGasPedal,c->m_fBrakePedal,c->m_fSteerAngle,c->bIsHandbrakeOn?1:0};
        const CVector p=c->GetPosition(),f=c->GetForward();
        s.position[0]=p.x;s.position[1]=p.y;s.position[2]=p.z;s.forward[0]=f.x;s.forward[1]=f.y;s.forward[2]=f.z;
        s.speed=c->m_vecMoveSpeed.Magnitude()*180.0f;
        if(sampleCount>=Core::MaxSamples || !Core::Write(output,s)){Finish("ABORTED - SAMPLE LIMIT OR WRITE ERROR");return;}
        ++sampleCount;lastInput=elapsed;
    }
}
void SimulationReplayEndFrame(){
    if(mode==Idle || mode==Finishing || frameDt<=0) return;
    if(!Target() || !sampled){Finish("ABORTED - VEHICLE CONTROL LOST");return;}
    if(mode==Preparing){warmTime+=frameDt;return;}
    gameTime+=double(frameDt);elapsed=uint64_t(gameTime*1000000.0+0.5);
    if(mode==Recording && elapsed>=uint64_t(duration)*1000000ULL){elapsed=uint64_t(duration)*1000000ULL;Finish("RECORDING SAVED");}
    else if(mode==Playing && elapsed>=tape->endUs){elapsed=tape->endUs;Finish("COMPLETE");}
}
void SimulationReplayAfterAppFrame(){
    if((request==1 || request==2) && Now()-requestStartNs>3000000000ULL){
        request=0;if(repeatPending)QuestProfilerSetReplayProfiling(0);repeatPending=false;startFailed=true;Status("RESUME NORMAL GAMEPLAY BEFORE STARTING A RUN");
    }
    if(request==3){request=0;if(repeatPending)QuestProfilerSetReplayProfiling(0);repeatPending=false;Finish(mode==Recording?"RECORDING SAVED":"ABORTED - STOPPED BY USER");}
    if(mode!=Idle && mode!=Finishing && Now()-wallStart>300000000000ULL) Finish("ABORTED - WALL TIME LIMIT");
    if(mode==Playing && runAutoProfile && !profileStarted){
        const std::string context="demo="+label+";ghost="+(runGhost?"1":"0")+";recording_ghost="+std::to_string(initial.recordingGhost)+";tracking="+(runAssisted?"route_assisted":"input_only")+";view="+(runForwardView?"fixed_forward":"live_head_tracking")+";renderdoc="+(runRenderdocCapture?"1":"0")+";capture_at_us="+std::to_string(runCaptureAtUs)+";repeat_index="+std::to_string(runIteration)+";repeat_count="+std::to_string(runRepeatCount)+";run="+runId;
        QuestProfilerSetCaptureContext(context.c_str());QuestProfilerSetReplayProfiling(2);
        profileStartUs=elapsed;
        profile=QuestProfilerGetCaptureStem();profileStarted=true;
        if(profile.empty()) Finish("ABORTED - PERFORMANCE CAPTURE COULD NOT START");
    }
    if(mode==Playing && runAutoProfile && profileStarted &&
        (!QuestProfilerIsRecording() || profile!=QuestProfilerGetCaptureStem()))
        Finish("ABORTED - PERFORMANCE CAPTURE WAS INTERRUPTED");
    if(mode==Finishing && !captureFrameActive){
        if(runRenderdocCapture && !captureAttempted)captureResult="not_reached";
        // Flush this capture before resetting the vehicle or streaming the next start.
        if(runAutoProfile && !runId.empty()){QuestProfilerSetReplayProfiling(1);QuestProfilerSetCaptureContext("");}
        const bool recorded=runId.empty();
        const bool reportSaved=WriteReport();
        const bool again=!recorded && result=="COMPLETE" && reportSaved && runIteration<runRepeatCount;
        if(runAutoProfile && !recorded && !again)QuestProfilerSetReplayProfiling(0);
        if(runMatched){
            CClock::ms_nGameClockHours=oldHours;CClock::ms_nGameClockMinutes=oldMinutes;CClock::ms_nGameClockSeconds=oldSeconds;
            CWeather::OldWeatherType=oldWeather;CWeather::NewWeatherType=newWeather;CWeather::ForcedWeatherType=forcedWeather;
            CWeather::InterpolationValue=weatherInterpolation;
        }
        MatchedProfile::benchmark=false;
        tape.reset(); reference=-1;mode=Idle;Refresh();
        if(again){
            ++runIteration;repeatPending=true;request=2;requestStartNs=Now();
            Status("PREPARING NEXT PLAYBACK - PERFORMANCE CAPTURE SAVED");
        }
        if(recorded){
            const auto found=std::find(demos.begin(),demos.end(),label);
            if(found!=demos.end()) selectedDemo=int(found-demos.begin());
        }
    }
}
void SimulationReplayBeginCaptureFrame(void *vkInstance){
    if(mode!=Playing || !runRenderdocCapture || captureAttempted || elapsed<runCaptureAtUs || CTimer::GetIsPaused())return;
    captureAttempted=true;capturedAtUs=elapsed;captureStartNs=Now();
    const auto started=QuestRenderDocBegin(vkInstance,capturePrefix.c_str());
    captureFrameActive=started==questcapture::RenderDocFrame::Started;
    captureResult=captureFrameActive?"capturing":started==questcapture::RenderDocFrame::Busy?"busy":
        started==questcapture::RenderDocFrame::Unavailable?"unavailable":"start_failed";
    if(!captureFrameActive)Status("RENDERDOC CAPTURE DID NOT START - CHECK RENDERDOC CONNECTION");
}
void SimulationReplayEndCaptureFrame(bool rendered){
    if(!captureFrameActive)return;
    const auto ended=QuestRenderDocEnd(rendered);captureFrameActive=false;captureEndNs=Now();
    captureResult=ended==questcapture::RenderDocFrame::Saved?"saved":
        ended==questcapture::RenderDocFrame::Discarded?"discarded":"end_failed";
    if(ended==questcapture::RenderDocFrame::Saved){
        captureFile=QuestRenderDocCapturePath();
        // Paths are local diagnostics. Preserve CSV structure even for an unexpected backend name.
        for(char &c:captureFile)if(c==',' || c=='\r' || c=='\n')c='_';
        Status("RENDERDOC FRAME SAVED - CAPTURE RUN IS DIAGNOSTIC");
    }else Status("RENDERDOC FRAME NOT SAVED - CHECK RENDERDOC");
    if(mode==Finishing)SimulationReplayAfterAppFrame();
}
unsigned long long SimulationReplayForwardViewRun(){
    return runForwardView && (mode==Preparing || mode==Playing)?wallStart:0;
}
bool SimulationReplayForwardViewNeutralPose(float position[3],float *yaw){
    return SimulationReplayForwardViewRun() && rw::vulkan::getFirstPersonNeutralPose(position,yaw);
}
const char *SimulationReplayStatus(){return status;}
bool SimulationReplayConsumeStartFailure(){const bool failed=startFailed;startFailed=false;return failed;}
bool SimulationReplayRecordingText(char *text,size_t capacity){
    if(mode!=Recording) return false;
    std::snprintf(text,capacity,"%.1fS RECORDED / %.1fS",double(elapsed)/1000000,double(duration));
    return true;
}
bool SimulationReplayPlaybackText(char *text,size_t capacity){
    if(mode!=Playing || !tape || !tape->endUs) return false;
    if(runRepeatCount>1)
        std::snprintf(text,capacity,"PLAY %.1f/%.1fS RUN %u/%u",
            double(std::min(elapsed,tape->endUs))/1000000,double(tape->endUs)/1000000,runIteration,runRepeatCount);
    else std::snprintf(text,capacity,"PLAYING %.1fS OUT OF %.1fS",
        double(std::min(elapsed,tape->endUs))/1000000,double(tape->endUs)/1000000);
    return true;
}
void SimulationReplayMenuRow(int row,char *text,size_t capacity){
    switch(row){
    case 0:std::snprintf(text,capacity,"RECORD ROUTE  < REPAIRS CAR >");break;
    case 1:std::snprintf(text,capacity,"DEMO < %s >",demos.empty()?"NONE":demos[selectedDemo].c_str());break;
    case 2:std::snprintf(text,capacity,"PLAY SELECTED DEMO  < START >");break;
    case 3:std::snprintf(text,capacity,"STOP  < %s %.1f S >",mode==Recording?"RECORDING":mode==Preparing?"PREPARING":mode==Playing?"PLAYING":"IDLE",double(elapsed)/1000000);break;
    case 4:std::snprintf(text,capacity,"PLAYBACK GHOST  < %s >",ghost?"ON":"OFF");break;
    case 5:std::snprintf(text,capacity,"RECORDING LIMIT  < %u SECONDS >",duration);break;
    case 6:std::snprintf(text,capacity,"RECORDING GHOST  < %s >",recordingGhost?"ON":"OFF");break;
    case 7:std::snprintf(text,capacity,deleteArmed.empty()?"DELETE SELECTED DEMO":"CONFIRM DELETE  < PRESS AGAIN >");break;
    case 8:std::snprintf(text,capacity,"ROUTE ASSISTANCE  < %s >",ghost?(routeAssistance?"ON":"OFF"):"OFF - PLAYBACK GHOST REQUIRED");break;
    case 9:std::snprintf(text,capacity,"BENCHMARK FORWARD VIEW  < %s >",forwardView?"ON":"OFF");break;
    case 10:std::snprintf(text,capacity,"RENDERDOC CAPTURE < %s >",!renderdocCapture?"OFF":!QuestRenderDocAvailable()?"ON - LAUNCH VIA RENDERDOC":captureResult=="saved"?"ON - SAVED":"ON");break;
    case 11:std::snprintf(text,capacity,"CAPTURE AT < %.1f SECONDS >",double(captureAtUs)/1000000);break;
    case 12:std::snprintf(text,capacity,"MATCHED WORKLOAD < %s >",MatchedProfile::active?"PROFILE - ON":matchedWorkload?"ON":"OFF");break;
    case 13:std::snprintf(text,capacity,"PLAYBACK COUNT < %u >",repeatCount);break;
    default:std::snprintf(text,capacity,"BACK");break;
    }
}
bool SimulationReplayMenuAction(int row,int direction){
    if(row!=7) deleteArmed.clear();
    if(row==-1){Refresh();return false;}
    if(row==3){request=3;return false;}
    if(mode!=Idle || request){Status("STOP THE CURRENT RUN FIRST");return false;}
    switch(row){
    case 0:if(!Ready()) return false;request=1;requestStartNs=Now();return true;
    case 1:Refresh();if(!demos.empty()) selectedDemo=(selectedDemo+direction+int(demos.size()))%int(demos.size());break;
    case 2:Refresh();if(demos.empty()){Status("NO RECORDED DEMOS");return false;}request=2;requestStartNs=Now();return true;
    case 4:ghost=!ghost;break;
    case 5:duration=duration==30?(direction>0?60:120):duration==60?(direction>0?120:30):(direction>0?30:60);break;
    case 6:recordingGhost=!recordingGhost;break;
    case 8:routeAssistance=!routeAssistance;break;
    case 9:forwardView=!forwardView;break;
    case 10:renderdocCapture=!renderdocCapture;break;
    case 11:captureAtUs=direction<0?(captureAtUs?captureAtUs-500000:119500000):(captureAtUs+500000)%120000000;break;
    case 12:matchedWorkload=!matchedWorkload;break;
    case 13:repeatCount=direction<0?(repeatCount==1?10:repeatCount-1):(repeatCount==10?1:repeatCount+1);break;
    case 7:{
        Refresh();if(demos.empty()){Status("NO RECORDED DEMOS TO DELETE");break;}
        const std::string name=demos[selectedDemo];
        if(deleteArmed!=name){deleteArmed=name;Status("PRESS DELETE AGAIN TO REMOVE THE SELECTED DEMO");break;}
        deleteArmed.clear();
        if(std::remove(Path(name+".sim.csv").c_str())!=0) Status("COULD NOT DELETE THE SELECTED DEMO");
        else{Refresh();Status("DEMO DELETED - PERFORMANCE REPORTS KEPT");}
        break;
    }
    }
    return false;
}
} // namespace androidgame
#endif
