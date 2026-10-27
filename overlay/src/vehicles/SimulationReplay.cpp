#include "common.h"
#include "SimulationReplay.h"
#ifdef __ANDROID__
#include "SimulationReplayCore.h"
#include "Automobile.h"
#include "HandlingMgr.h"
#include "Pools.h"
#include "World.h"
#include "Replay.h"
#include "Timer.h"
#include "Ped.h"
#include "ModelSets.h"
#include "QuestProfiler.h"
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

namespace androidgame { namespace {
namespace Core=SimulationReplayCore;
#ifndef LCVR_SIMULATION_REPLAY_ROOT
#define LCVR_SIMULATION_REPLAY_ROOT "/sdcard/Android/data/com.libertyvr.quest/files/simulation-replays"
#endif
const char *Root(){ return LCVR_SIMULATION_REPLAY_ROOT; }
enum Mode { Idle,Recording,Preparing,Playing,Finishing };
Mode mode=Idle;
int request=0,reference=-1,selectedDemo=0;
unsigned model=0,signature=0,duration=120,sampleCount=0;
bool ghost=true,autoProfile=true,runGhost=false,runAutoProfile=false,profileStarted=false,sampled=false;
bool recordingGhost=true,startFailed=false;
bool routeAssistance=true,runAssisted=false;
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
void Status(const char *s){ std::snprintf(status,sizeof(status),"%s",s); }
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
unsigned Signature(CAutomobile *c){
    const tHandlingData &h=*c->pHandling;
    // Canonical text, never compiler padding or pointer values. Include transmission.
    const float values[]={c->m_fMass,c->m_fTurnMass,h.fTractionMultiplier,h.fTractionLoss,h.fBrakeDeceleration,h.fBrakeBias,
        h.fSuspensionForceLevel,h.fSuspensionDampingLevel,h.fSuspensionBias,h.fSteeringLock,
        h.fTractionBias,h.fSuspensionUpperLimit,h.fSuspensionLowerLimit,h.fBuoyancy,h.fCollisionDamageMultiplier,
        h.Transmission.fEngineAcceleration,h.Transmission.fMaxVelocity,h.Transmission.fMaxCruiseVelocity,h.Transmission.fMaxReverseVelocity,
        float(h.Transmission.nNumberOfGears),float(h.Transmission.nDriveType),float(h.Transmission.nEngineType),float(h.Transmission.Flags),
        float(h.bABS),float(h.Flags),c->m_vecCentreOfMass.x,c->m_vecCentreOfMass.y,c->m_vecCentreOfMass.z};
    unsigned hash=2166136261u;
    for(float v:values){ char text[40]; std::snprintf(text,sizeof(text),"%.9g;",double(v)); for(const char *p=text;*p;++p) hash=(hash^unsigned(*p))*16777619u; }
    for(const auto &g:h.Transmission.Gears){
        char text[128]; std::snprintf(text,sizeof(text),"%.9g;%.9g;%.9g;",double(g.fMaxVelocity),double(g.fShiftUpVelocity),double(g.fShiftDownVelocity));
        for(const char *p=text;*p;++p) hash=(hash^unsigned(*p))*16777619u;
    }
    return hash;
}
CVector Vec(const float *v){ return CVector(v[0],v[1],v[2]); }
void Repair(CAutomobile *c){ c->Fix(); c->ExtinguishCarFire(); c->m_fHealth=1000; c->m_fFireBlowUpTimer=0; }
Core::Initial Initial(CAutomobile *c){
    Core::Initial s; s.vehicle.model=c->GetModelIndex(); s.vehicle.signature=Signature(c);
    const CVector vectors[]={c->GetPosition(),c->GetRight(),c->GetForward(),c->GetUp(),c->m_vecMoveSpeed,c->m_vecTurnSpeed};
    int i=0; for(const CVector &v:vectors){s.vehicle.pose[i++]=v.x;s.vehicle.pose[i++]=v.y;s.vehicle.pose[i++]=v.z;}
    for(i=0;i<5;++i) s.assets[i]=int(ModelSets::GetActiveForCategory(ModelSets::eModelCategory(i)));
    s.gear=int(c->m_nCurrentGear); s.gearTime=c->m_fChangeGearTime; return s;
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
void WriteReport(){
    if(runId.empty()) return;
    FILE *f=std::fopen(Path(runId+".run.csv").c_str(),"wx");
    if(!f){Status("RUN REPORT WRITE FAILED");return;}
    std::fprintf(f,"schema,LCVR_SIM_RUN_V1\ndemo,%s\nresult,%s\nghost,%d\nauto_profile,%d\nprofile,%s\nmodel,%u\nhandling,%u\nelapsed_us,%llu\ncomparison_samples,%u\nposition_rms_m,%.9g\nposition_max_m,%.9g\nspeed_max_error_kmh,%.9g\nheading_max_error_deg,%.9g\nbuild,%s %s\n",
        label.c_str(),result.c_str(),runGhost?1:0,runAutoProfile?1:0,profile.c_str(),model,signature,(unsigned long long)elapsed,
        drift.count,drift.Rms(),double(drift.maxPosition),double(drift.maxSpeed),double(drift.maxHeading),__DATE__,__TIME__);
    for(int i=0;i<5;++i) std::fprintf(f,"assets_%d,%d\n",i,initial.assets[i]);
    std::fprintf(f,"recording_ghost,%d\n",initial.recordingGhost);
    std::fprintf(f,"ghost_policy,traffic_props_preserve_roads\n");
    std::fprintf(f,"tracking,%s\nroute_corrections,%u\nroute_correction_max_m,%.9g\nreference_state,%s\n",
        runAssisted?"route_assisted":"input_only",corrections,double(correctionMax),runAssisted?"position_heading_and_estimated_path_velocity":"unmodified_simulation");
    std::fprintf(f,"playback_start_monotonic_ns,%llu\nplayback_end_monotonic_ns,%llu\nprofile_start_simulation_us,%llu\n",
        (unsigned long long)playbackStartNs,(unsigned long long)playbackEndNs,(unsigned long long)profileStartUs);
    if(std::fclose(f)!=0) Status("RUN REPORT CLOSE FAILED");
}
bool Start(int operation){
    if(mode!=Idle) {Status("STOP THE CURRENT RUN FIRST");return false;}
    CAutomobile *c=Ready();if(!c) return false;
    mkdir(Root(),0700);
    Core::Initial current=Initial(c);
    if(!Core::Valid(current) || current.gear>c->pHandling->Transmission.nNumberOfGears){Status("INVALID VEHICLE STATE");return false;}
    initial=current;
    if(operation==2){
        Refresh(); if(demos.empty()){Status("NO RECORDED DEMOS");return false;}
        label=demos[selectedDemo];
        FILE *f=std::fopen(Path(label+".sim.csv").c_str(),"r");
        tape.reset(new Core::Tape); const bool loaded=f && tape->Load(f); if(f) std::fclose(f);
        if(!loaded){tape.reset();Status("INCOMPLETE OR INVALID DEMO");return false;}
        if(tape->endUs<1000000){tape.reset();Status("RECORD AT LEAST ONE SECOND BEFORE PLAYBACK");return false;}
        const Core::Initial &s=tape->initial;
        bool match=s.vehicle.model==current.vehicle.model && s.vehicle.signature==current.vehicle.signature && s.gear<=c->pHandling->Transmission.nNumberOfGears;
        for(int i=0;i<5;++i) match=match && s.assets[i]==current.assets[i];
        if(!match){tape.reset();Status("USE THE RECORDED CAR MODEL, ASSETS AND HANDLING");return false;}
        initial=s;
    }else{
        initial.recordingGhost=recordingGhost?1:0;
        label=Unique(); output=std::fopen(Path(label+".sim.csv").c_str(),"wx");
        if(!output){Status("CANNOT CREATE DEMO FILE");return false;}
        std::setvbuf(output,nullptr,_IOFBF,65536);
        if(!Core::Header(output,initial)){std::fclose(output);output=nullptr;std::remove(Path(label+".sim.csv").c_str());Status("DEMO HEADER WRITE FAILED");return false;}
    }
    reference=CPools::GetVehicleRef(c);model=unsigned(c->GetModelIndex());signature=current.vehicle.signature;
    sampleCount=0; elapsed=lastInput=0; gameTime=warmTime=0; drift={}; wallStart=Now();
    playbackStartNs=playbackEndNs=profileStartUs=0;
    runGhost=operation==1?recordingGhost:ghost;runAutoProfile=autoProfile;profileStarted=false;profile.clear();runId.clear();
    runAssisted=operation==2 && routeAssistance && runGhost;corrections=0;correctionMax=0;
    if(operation==2){
        runId=Unique();
        if(runAutoProfile) QuestProfilerSetDisplayMode(0);
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
        Restore(c); mode=Preparing; Status("PREPARING PLAYBACK - LIVE WORLD, LIVE HEAD TRACKING");
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
    if(request==1 || request==2){const int op=request;request=0;startFailed=!Start(op);}
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
    else if(mode==Playing && elapsed>=tape->endUs) Finish("COMPLETE");
}
void SimulationReplayAfterAppFrame(){
    if((request==1 || request==2) && Now()-requestStartNs>3000000000ULL){
        request=0;startFailed=true;Status("RESUME NORMAL GAMEPLAY BEFORE STARTING A RUN");
    }
    if(request==3){request=0;Finish(mode==Recording?"RECORDING SAVED":"ABORTED - STOPPED BY USER");}
    if(mode!=Idle && mode!=Finishing && Now()-wallStart>300000000000ULL) Finish("ABORTED - WALL TIME LIMIT");
    if(mode==Playing && runAutoProfile && !profileStarted){
        const std::string context="demo="+label+";ghost="+(runGhost?"1":"0")+";recording_ghost="+std::to_string(initial.recordingGhost)+";tracking="+(runAssisted?"route_assisted":"input_only")+";run="+runId;
        QuestProfilerSetCaptureContext(context.c_str());QuestProfilerSetDisplayMode(2);
        profileStartUs=elapsed;
        profile=QuestProfilerGetCaptureStem();profileStarted=true;
        if(profile.empty()) Finish("ABORTED - PERFORMANCE CAPTURE COULD NOT START");
    }
    if(mode==Playing && runAutoProfile && profileStarted &&
        (!QuestProfilerIsEnabled() || profile!=QuestProfilerGetCaptureStem()))
        Finish("ABORTED - PERFORMANCE CAPTURE WAS INTERRUPTED");
    if(mode==Finishing){
        if(runAutoProfile && !runId.empty()){QuestProfilerSetDisplayMode(0);QuestProfilerSetCaptureContext("");}
        const bool recorded=runId.empty();
        WriteReport(); tape.reset(); reference=-1;mode=Idle;Refresh();
        if(recorded){
            const auto found=std::find(demos.begin(),demos.end(),label);
            if(found!=demos.end()) selectedDemo=int(found-demos.begin());
        }
    }
}
const char *SimulationReplayStatus(){return status;}
bool SimulationReplayConsumeStartFailure(){const bool failed=startFailed;startFailed=false;return failed;}
bool SimulationReplayRecordingText(char *text,size_t capacity){
    if(mode!=Recording) return false;
    std::snprintf(text,capacity,"%.1fS RECORDED / %.1fS",double(elapsed)/1000000,double(duration));
    return true;
}
void SimulationReplayMenuRow(int row,char *text,size_t capacity){
    switch(row){
    case 0:std::snprintf(text,capacity,"RECORD ROUTE  < REPAIRS CAR >");break;
    case 1:std::snprintf(text,capacity,"DEMO < %s >",demos.empty()?"NONE":demos[selectedDemo].c_str());break;
    case 2:std::snprintf(text,capacity,"PLAY SELECTED DEMO  < START >");break;
    case 3:std::snprintf(text,capacity,"STOP  < %s %.1f S >",mode==Recording?"RECORDING":mode==Preparing?"PREPARING":mode==Playing?"PLAYING":"IDLE",double(elapsed)/1000000);break;
    case 4:std::snprintf(text,capacity,"PLAYBACK GHOST  < %s >",ghost?"ON":"OFF");break;
    case 5:std::snprintf(text,capacity,"AUTO PROFILE PLAYBACK  < %s >",autoProfile?"ON":"OFF");break;
    case 6:std::snprintf(text,capacity,"RECORDING LIMIT  < %u SECONDS >",duration);break;
    case 7:std::snprintf(text,capacity,"RECORDING GHOST  < %s >",recordingGhost?"ON":"OFF");break;
    case 8:std::snprintf(text,capacity,deleteArmed.empty()?"DELETE SELECTED DEMO":"CONFIRM DELETE  < PRESS AGAIN >");break;
    case 9:std::snprintf(text,capacity,"ROUTE ASSISTANCE  < %s >",ghost?(routeAssistance?"ON":"OFF"):"OFF - PLAYBACK GHOST REQUIRED");break;
    default:std::snprintf(text,capacity,"BACK");break;
    }
}
bool SimulationReplayMenuAction(int row,int direction){
    if(row!=8) deleteArmed.clear();
    if(row==-1){Refresh();return false;}
    if(row==3){request=3;return false;}
    if(mode!=Idle || request){Status("STOP THE CURRENT RUN FIRST");return false;}
    switch(row){
    case 0:if(!Ready()) return false;request=1;requestStartNs=Now();return true;
    case 1:Refresh();if(!demos.empty()) selectedDemo=(selectedDemo+direction+int(demos.size()))%int(demos.size());break;
    case 2:if(!Ready()) return false;Refresh();if(demos.empty()){Status("NO RECORDED DEMOS");return false;}request=2;requestStartNs=Now();return true;
    case 4:ghost=!ghost;break;
    case 5:autoProfile=!autoProfile;break;
    case 6:duration=duration==30?(direction>0?60:120):duration==60?(direction>0?120:30):(direction>0?30:60);break;
    case 7:recordingGhost=!recordingGhost;break;
    case 9:routeAssistance=!routeAssistance;break;
    case 8:{
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
